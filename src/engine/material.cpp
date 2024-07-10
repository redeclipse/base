#include "engine.h"

struct QuadNode
{
    int x, y, size;
    uint filled;
    QuadNode *child[4];

    QuadNode(int x, int y, int size) : x(x), y(y), size(size), filled(0) { loopi(4) child[i] = 0; }

    void clear()
    {
        loopi(4) DELETEP(child[i]);
    }

    ~QuadNode()
    {
        clear();
    }

    void insert(int mx, int my, int msize)
    {
        if(size == msize)
        {
            filled = 0xF;
            return;
        }
        int csize = size>>1, i = 0;
        if(mx >= x+csize) i |= 1;
        if(my >= y+csize) i |= 2;
        if(csize == msize)
        {
            filled |= (1 << i);
            return;
        }
        if(!child[i]) child[i] = new QuadNode(i&1 ? x+csize : x, i&2 ? y+csize : y, csize);
        child[i]->insert(mx, my, msize);
        loopj(4) if(child[j])
        {
            if(child[j]->filled == 0xF)
            {
                DELETEP(child[j]);
                filled |= (1 << j);
            }
        }
    }

    void genmatsurf(ushort mat, uchar orient, uchar visible, int x, int y, int z, int size, materialsurface *&matbuf)
    {
        materialsurface &m = *matbuf++;
        m.material = mat;
        m.orient = orient;
        m.visible = visible;
        m.csize = size;
        m.rsize = size;
        int dim = dimension(orient);
        m.o[C[dim]] = x;
        m.o[R[dim]] = y;
        m.o[dim] = z;
    }

    void genmatsurfs(ushort mat, uchar orient, uchar visible, int z, materialsurface *&matbuf)
    {
        if(filled == 0xF) genmatsurf(mat, orient, visible, x, y, z, size, matbuf);
        else if(filled)
        {
            int csize = size>>1;
            loopi(4) if(filled & (1 << i))
                genmatsurf(mat, orient, visible, i&1 ? x+csize : x, i&2 ? y+csize : y, z, csize, matbuf);
        }
        loopi(4) if(child[i]) child[i]->genmatsurfs(mat, orient, visible, z, matbuf);
    }
};

static void drawmaterial(const materialsurface &m, float offset, const bvec4 &color = bvec4(0, 0, 0, 0))
{
    if(gle::attribbuf.empty())
    {
        gle::defvertex();
        gle::defcolor(4, GL_UNSIGNED_BYTE);
        gle::begin(GL_QUADS);
    }
    float x = m.o.x, y = m.o.y, z = m.o.z, csize = m.csize, rsize = m.rsize;
    switch(m.orient)
    {
    #define GENFACEORIENT(orient, v0, v1, v2, v3) \
        case orient: v0 v1 v2 v3 break;
    #define GENFACEVERT(orient, vert, mx,my,mz, sx,sy,sz) \
        { \
            gle::attribf(mx sx, my sy, mz sz); \
            gle::attrib(color); \
        }
        GENFACEVERTS(x, x, y, y, z, z, /**/, + csize, /**/, + rsize, + offset, - offset)
    #undef GENFACEORIENT
    #undef GENFACEVERT
    }
}

extern const namemap materials[] =
{
    {"air", MAT_AIR},
    {"water", MAT_WATER}, {"water1", MAT_WATER}, {"water2", MAT_WATER+1}, {"water3", MAT_WATER+2}, {"water4", MAT_WATER+3},
    {"glass", MAT_GLASS}, {"glass1", MAT_GLASS}, {"glass2", MAT_GLASS+1}, {"glass3", MAT_GLASS+2}, {"glass4", MAT_GLASS+3},
    {"lava", MAT_LAVA}, {"lava1", MAT_LAVA}, {"lava2", MAT_LAVA+1}, {"lava3", MAT_LAVA+2}, {"lava4", MAT_LAVA+3},
    {"volfog", MAT_VOLFOG}, {"volfog1", MAT_VOLFOG}, {"volfog2", MAT_VOLFOG+1}, {"volfog3", MAT_VOLFOG+2}, {"volfog4", MAT_VOLFOG+3},
    {"clip", MAT_CLIP},
    {"noclip", MAT_NOCLIP},
    {"aiclip", MAT_AICLIP},
    {"death", MAT_DEATH},
    {"ladder", MAT_LADDER},
    {"alpha", MAT_ALPHA},
    {"hurt", MAT_HURT},
    {"nogi", MAT_NOGI}
};

int findmaterial(const char *name, bool tryint)
{
    loopi(sizeof(materials)/sizeof(materials[0])) if(!strcmp(materials[i].name, name)) { return materials[i].id; }
    return tryint && isnumeric(*name) ? atoi(name) : -1;
}
ICOMMAND(0, findmaterial, "s", (char *mat), intret(findmaterial(mat)));

const char *findmaterialname(int type)
{
    loopi(sizeof(materials)/sizeof(materials[0])) if(materials[i].id == type) { return materials[i].name; }
    return NULL;
}

const char *getmaterialdesc(ushort *mat, const char *prefix)
{
    static const ushort matmasks[] = { MATF_VOLUME|MATF_INDEX, MATF_CLIP, MAT_DEATH, MAT_LADDER, MAT_ALPHA, MAT_HURT, MAT_NOGI };
    static string desc;
    desc[0] = '\0';
    loopj(MATF_NUMVOL) loopi(sizeof(matmasks)/sizeof(matmasks[0])) if(mat[j]&matmasks[i])
    {
        const char *matname = findmaterialname(mat[j]&matmasks[i]);
        if(matname)
        {
            concatstring(desc, desc[0] ? " " : prefix);
            concatstring(desc, matname);
        }
    }
    return desc;
}
#define ORIENTS_ALL (1<<O_LEFT)|(1<<O_RIGHT)|(1<<O_FRONT)|(1<<O_BACK)|(1<<O_TOP)|(1<<O_BOTTOM)
#define ORIENTS_LIQUID (1<<O_LEFT)|(1<<O_RIGHT)|(1<<O_FRONT)|(1<<O_BACK)|(1<<O_TOP)

#define MATFACEVARS(name) \
    VARF(IDF_MAP, name##faces, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##2faces, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##3faces, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##4faces, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##facesalt, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##2facesalt, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##3facesalt, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged()); \
    VARF(IDF_MAP, name##4facesalt, 0, ORIENTS_ALL, ORIENTS_ALL, if(!(identflags&IDF_MAP)) allchanged());

MATFACEVARS(water);
MATFACEVARS(lava);
MATFACEVARS(glass);

GETMATIDXVAR(water, faces, int)
GETMATIDXVAR(lava, faces, int)
GETMATIDXVAR(glass, faces, int)

int visiblematerial(const cube &c, int orient, const ivec &co, int size, ushort matmask)
{
    ushort mat = c.material&matmask&vismatmask;
    switch(mat)
    {
    case MAT_AIR:
         break;

    case MAT_LAVA:
        if(visibleface(c, orient, co, size, mat, MAT_AIR, matmask))
            return (getlavaenabled(c.material) && orient != O_BOTTOM && (1<<orient)&getlavafaces(c.material) ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
        break;

    case MAT_WATER:
        if(visibleface(c, orient, co, size, mat, MAT_AIR, matmask))
            return (getwaterenabled(c.material) && orient != O_BOTTOM && (1<<orient)&getwaterfaces(c.material) ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
        break;

    case MAT_GLASS:
        if(visibleface(c, orient, co, size, MAT_GLASS, MAT_AIR, matmask))
            return (getglassenabled(c.material) && (1<<orient)&getglassfaces(c.material) ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
        break;

    case MAT_VOLFOG:
        if(visibleface(c, orient, co, size, MAT_VOLFOG, MAT_AIR, matmask))
            return (getvolfogenabled(c.material) && orient == O_TOP ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
        break;

    default:
        if(visibleface(c, orient, co, size, mat, MAT_AIR, matmask)) return MATSURF_EDIT_ONLY;
        break;
    }
    return MATSURF_NOT_VISIBLE;
}

void genmatsurfs(const cube &c, const ivec &co, int size, vector<materialsurface> &matsurfs)
{
    loopi(6)
    {
        static const ushort matmasks[] = { MATF_VOLUME|MATF_INDEX, MATF_CLIP, MAT_DEATH, MAT_LADDER, MAT_ALPHA, MAT_HURT, MAT_NOGI };
        loopj(sizeof(matmasks)/sizeof(matmasks[0]))
        {
            ushort matmask = matmasks[j];
            int vis = visiblematerial(c, i, co, size, matmask&~MATF_INDEX);
            if(vis != MATSURF_NOT_VISIBLE)
            {
                materialsurface m;
                m.material = c.material&matmask;
                m.orient = i;
                m.visible = vis;
                m.o = co;
                m.csize = m.rsize = size;
                if(dimcoord(i)) m.o[dimension(i)] += size;
                matsurfs.add(m);
                break;
            }
        }
    }
}

static inline void addmatbb(ivec &matmin, ivec &matmax, const materialsurface &m)
{
    int dim = dimension(m.orient);
    ivec mmin(m.o), mmax(m.o);
    if(dimcoord(m.orient)) mmin[dim] -= 2; else mmax[dim] += 2;
    mmax[R[dim]] += m.rsize;
    mmax[C[dim]] += m.csize;
    matmin.min(mmin);
    matmax.max(mmax);
}

void calcmatbb(vtxarray *va, const ivec &co, int size, vector<materialsurface> &matsurfs)
{
    va->lavamax = va->watermax = va->glassmax = va->volfogmax = co;
    va->lavamin = va->watermin = va->glassmin = va->volfogmin = ivec(co).add(size);
    loopv(matsurfs)
    {
        materialsurface &m = matsurfs[i];
        switch(m.material&MATF_VOLUME)
        {
            case MAT_LAVA:
                if(m.visible == MATSURF_EDIT_ONLY) continue;
                addmatbb(va->lavamin, va->lavamax, m);
                break;

            case MAT_WATER:
                if(m.visible == MATSURF_EDIT_ONLY) continue;
                addmatbb(va->watermin, va->watermax, m);
                break;

            case MAT_GLASS:
                addmatbb(va->glassmin, va->glassmax, m);
                break;

            case MAT_VOLFOG:
                if(m.visible == MATSURF_EDIT_ONLY) continue;
                addmatbb(va->volfogmin, va->volfogmax, m);
                break;

            default:
                continue;
        }
    }
}

static inline bool mergematcmp(const materialsurface &x, const materialsurface &y)
{
    int dim = dimension(x.orient), c = C[dim], r = R[dim];
    if(x.o[r] + x.rsize < y.o[r] + y.rsize) return true;
    if(x.o[r] + x.rsize > y.o[r] + y.rsize) return false;
    return x.o[c] < y.o[c];
}

static int mergematr(materialsurface *m, int sz, materialsurface &n)
{
    int dim = dimension(n.orient), c = C[dim], r = R[dim];
    for(int i = sz-1; i >= 0; --i)
    {
        if(m[i].o[r] + m[i].rsize < n.o[r]) break;
        if(m[i].o[r] + m[i].rsize == n.o[r] && m[i].o[c] == n.o[c] && m[i].csize == n.csize)
        {
            n.o[r] = m[i].o[r];
            n.rsize += m[i].rsize;
            memmove(&m[i], &m[i+1], (sz - (i+1)) * sizeof(materialsurface));
            return 1;
        }
    }
    return 0;
}

static int mergematc(materialsurface &m, materialsurface &n)
{
    int dim = dimension(n.orient), c = C[dim], r = R[dim];
    if(m.o[r] == n.o[r] && m.rsize == n.rsize && m.o[c] + m.csize == n.o[c])
    {
        n.o[c] = m.o[c];
        n.csize += m.csize;
        return 1;
    }
    return 0;
}

static int mergemat(materialsurface *m, int sz, materialsurface &n)
{
    for(bool merged = false; sz; merged = true)
    {
        int rmerged = mergematr(m, sz, n);
        sz -= rmerged;
        if(!rmerged && merged) break;
        if(!sz) break;
        int cmerged = mergematc(m[sz-1], n);
        sz -= cmerged;
        if(!cmerged) break;
    }
    m[sz++] = n;
    return sz;
}

static int mergemats(materialsurface *m, int sz)
{
    quicksort(m, sz, mergematcmp);

    int nsz = 0;
    loopi(sz) nsz = mergemat(m, nsz, m[i]);
    return nsz;
}

static inline bool optmatcmp(const materialsurface &x, const materialsurface &y)
{
    if(x.material < y.material) return true;
    if(x.material > y.material) return false;
    if(x.orient > y.orient) return true;
    if(x.orient < y.orient) return false;
    int dim = dimension(x.orient);
    return x.o[dim] < y.o[dim];
}

VARF(0, optmats, 0, 1, 1, allchanged());

int optimizematsurfs(materialsurface *matbuf, int matsurfs)
{
    quicksort(matbuf, matsurfs, optmatcmp);
    if(!optmats) return matsurfs;
    materialsurface *cur = matbuf, *end = matbuf+matsurfs;
    while(cur < end)
    {
         materialsurface *start = cur++;
         int dim = dimension(start->orient);
         while(cur < end &&
               cur->material == start->material &&
               cur->orient == start->orient &&
               cur->visible == start->visible &&
               cur->o[dim] == start->o[dim])
            ++cur;
         if(!isliquid(start->material&MATF_VOLUME) || start->orient != O_TOP || !vertwater)
         {
            if(start!=matbuf) memmove(matbuf, start, (cur-start)*sizeof(materialsurface));
            matbuf += mergemats(matbuf, cur-start);
         }
         else if(cur-start>=4)
         {
            QuadNode vmats(0, 0, worldsize);
            loopi(cur-start) vmats.insert(start[i].o[C[dim]], start[i].o[R[dim]], start[i].csize);
            vmats.genmatsurfs(start->material, start->orient, start->visible, start->o[dim], matbuf);
         }
         else
         {
            if(start!=matbuf) memmove(matbuf, start, (cur-start)*sizeof(materialsurface));
            matbuf += cur-start;
         }
    }
    return matsurfs - (end-matbuf);
}

void preloadglassshaders(bool force = false)
{
    static bool needglass = false;
    if(force) needglass = true;
    if(!needglass) return;

    useshaderbyname("glass");

    extern int glassenv;
    if(glassenv) useshaderbyname("glassenv");
}

void setupmaterials(int start, int len)
{
    int hasmat = 0;
    if(!len) len = valist.length();
    for(int i = start; i < len; i++)
    {
        vtxarray *va = valist[i];
        materialsurface *skip = NULL;
        loopj(va->matsurfs)
        {
            materialsurface &m = va->matbuf[j];
            int matvol = m.material&MATF_VOLUME;
            if(isliquid(matvol) && m.orient!=O_BOTTOM && m.orient!=O_TOP)
            {
                m.ends = 0;
                int dim = dimension(m.orient), coord = dimcoord(m.orient);
                ivec o(m.o);
                o.z -= 1;
                o[dim] += coord ? 1 : -1;
                int minc = o[dim^1], maxc = minc + (C[dim]==2 ? m.rsize : m.csize);
                ivec co;
                int csize;
                while(o[dim^1] < maxc)
                {
                    cube &c = lookupcube(o, 0, co, csize);
                    if(isliquid(c.material&MATF_VOLUME)) { m.ends |= 1; break; }
                    o[dim^1] += csize;
                }
                o[dim^1] = minc;
                o.z += R[dim]==2 ? m.rsize : m.csize;
                o[dim] -= coord ? 2 : -2;
                while(o[dim^1] < maxc)
                {
                    cube &c = lookupcube(o, 0, co, csize);
                    if(visiblematerial(c, O_TOP, co, csize)) { m.ends |= 2; break; }
                    o[dim^1] += csize;
                }
            }
            else if(matvol==MAT_GLASS)
            {
                int dim = dimension(m.orient);
                vec center(m.o);
                center[R[dim]] += m.rsize/2;
                center[C[dim]] += m.csize/2;
                m.envmap = closestenvmap(center);
            }
            if(matvol) hasmat |= 1<<m.material;
            m.skip = 0;
            if(skip && m.material == skip->material && m.orient == skip->orient && skip->skip < 0xFFFF)
                skip->skip++;
            else
                skip = &m;
        }
    }
    if(hasmat&(0xF<<MAT_WATER) || hasmat&(0xF<<MAT_VOLFOG)) useshaderbyname("minimapvol");
    if(hasmat&(0xF<<MAT_WATER) || hasmat&(0xF<<MAT_LAVA) || hasmat&(0xF<<MAT_VOLFOG)) useshaderbyname("depthfog");
    if(hasmat&(0xF<<MAT_WATER))
    {
        preloadwatershaders(true);
        loopi(4) if(hasmat&(1<<(MAT_WATER+i))) lookupmaterialslot(MAT_WATER+i);
    }
    if(hasmat&(0xF<<MAT_LAVA))
    {
        useshaderbyname("lava");
        loopi(4) if(hasmat&(1<<(MAT_LAVA+i))) lookupmaterialslot(MAT_LAVA+i);
    }
    if(hasmat&(0xF<<MAT_GLASS))
    {
        preloadglassshaders(true);
        loopi(4) if(hasmat&(1<<(MAT_GLASS+i))) lookupmaterialslot(MAT_GLASS+i);
    }
    if(hasmat&(0xF<<MAT_VOLFOG))
    {
        useshaderbyname("volfog");
        useshaderbyname("volfogtex");
        useshaderbyname("volfogtexsmp");
        useshaderbyname("undervolfog");
        useshaderbyname("undervolfogtex");
        useshaderbyname("undervolfogtexsmp");
        loopi(4) if(hasmat&(1<<(MAT_VOLFOG+i))) lookupmaterialslot(MAT_VOLFOG+i);
    }
}

VAR(IDF_PERSIST, showmat, 0, 0, 1);
VAR(IDF_PERSIST, editmatoffset, 0, 1, 1);
VARF(0, vismatmask, 0, 0xFFFF, 0xFFFF,
{
    if(!noedit(true)) allchanged(true);
    else vismatmask = 0xFFFF;
});

static int sortdim[3];
static ivec sortorigin;

static inline bool editmatcmp(const materialsurface &x, const materialsurface &y)
{
    int xdim = dimension(x.orient), ydim = dimension(y.orient);
    loopi(3)
    {
        int dim = sortdim[i], xmin, xmax, ymin, ymax;
        xmin = xmax = x.o[dim];
        if(dim==C[xdim]) xmax += x.csize;
        else if(dim==R[xdim]) xmax += x.rsize;
        ymin = ymax = y.o[dim];
        if(dim==C[ydim]) ymax += y.csize;
        else if(dim==R[ydim]) ymax += y.rsize;
        if(xmax > ymin && ymax > xmin) continue;
        int c = sortorigin[dim];
        if(c > xmin && c < xmax) return true;
        if(c > ymin && c < ymax) return false;
        xmin = abs(xmin - c);
        xmax = abs(xmax - c);
        ymin = abs(ymin - c);
        ymax = abs(ymax - c);
        if(max(xmin, xmax) <= min(ymin, ymax)) return true;
        else if(max(ymin, ymax) <= min(xmin, xmax)) return false;
    }
    if(x.material < y.material) return true;
    // if(x.material > y.material) return false;
    return false;
}

void sorteditmaterials()
{
    sortorigin = ivec(camera1->o);
    vec dir = vec(camdir).abs();
    loopi(3) sortdim[i] = i;
    if(dir[sortdim[2]] > dir[sortdim[1]]) swap(sortdim[2], sortdim[1]);
    if(dir[sortdim[1]] > dir[sortdim[0]]) swap(sortdim[1], sortdim[0]);
    if(dir[sortdim[2]] > dir[sortdim[1]]) swap(sortdim[2], sortdim[1]);
    editsurfs.sort(editmatcmp);
}

#define EDITMATCOLOR(value) \
    case MAT_WATER:  value = bvec4::fromcolor(colourblue); break; \
    case MAT_CLIP:   value = bvec4::fromcolor(colourred); break; \
    case MAT_GLASS:  value = bvec4::fromcolor(colourcyan); break; \
    case MAT_VOLFOG: value = bvec4::fromcolor(colourpurple); break; \
    case MAT_NOCLIP: value = bvec4::fromcolor(colourgreen); break; \
    case MAT_LAVA:   value = bvec4::fromcolor(colourorange); break; \
    case MAT_AICLIP: value = bvec4::fromcolor(colouryellow); break; \
    case MAT_DEATH:  value = bvec4::fromcolor(colourdarkgrey); break; \
    case MAT_LADDER: value = bvec4::fromcolor(colourviolet); break; \
    case MAT_ALPHA:  value = bvec4::fromcolor(colourpink); break; \
    case MAT_HURT:   value = bvec4::fromcolor(colourgrey); break; \
    case MAT_NOGI:   value = bvec4::fromcolor(colourbrown); break;

void rendermatgrid()
{
    enablepolygonoffset(GL_POLYGON_OFFSET_LINE, editmatoffset ? 1.0f : 2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    int lastmat = -1;
    bvec4 color(0, 0, 0, 0);
    loopvrev(editsurfs)
    {
        materialsurface &m = editsurfs[i];
        if(m.material != lastmat)
        {
            switch(m.material&~MATF_INDEX)
            {
                EDITMATCOLOR(color)
                default: continue;
            }
            lastmat = m.material;
        }
        drawmaterial(m, editmatoffset ? -VOLUME_INSET : 0.0f, color);
    }
    xtraverts += gle::end();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
}

extern const bvec4 matnormals[6] =
{
    bvec4(0x80, 0, 0),
    bvec4(0x7F, 0, 0),
    bvec4(0, 0x80, 0),
    bvec4(0, 0x7F, 0),
    bvec4(0, 0, 0x80),
    bvec4(0, 0, 0x7F)
};

static void drawglass(const materialsurface &m, float offset)
{
    if(gle::attribbuf.empty())
    {
        gle::defvertex();
        gle::defnormal(4, GL_BYTE);
        gle::begin(GL_QUADS);
    }
    float x = m.o.x, y = m.o.y, z = m.o.z, csize = m.csize, rsize = m.rsize;
    switch(m.orient)
    {
    #define GENFACEORIENT(orient, v0, v1, v2, v3) \
        case orient: v0 v1 v2 v3 break;
    #define GENFACEVERT(orient, vert, mx,my,mz, sx,sy,sz) \
        { \
            gle::attribf(mx sx, my sy, mz sz); \
            gle::attrib(matnormals[orient]); \
        }
        GENFACEVERTS(x, x, y, y, z, z, /**/, + csize, /**/, + rsize, + offset, - offset)
    #undef GENFACEORIENT
    #undef GENFACEVERT
    }
}

vector<materialsurface> editsurfs, glasssurfs[4], watersurfs[4], waterfallsurfs[4], lavasurfs[4], lavafallsurfs[4], volfogsurfs[4];

float matliquidsx1 = -1, matliquidsy1 = -1, matliquidsx2 = 1, matliquidsy2 = 1;
float matsolidsx1 = -1, matsolidsy1 = -1, matsolidsx2 = 1, matsolidsy2 = 1;
float matrefractsx1 = -1, matrefractsy1 = -1, matrefractsx2 = 1, matrefractsy2 = 1;
uint matliquidtiles[LIGHTTILE_MAXH], matsolidtiles[LIGHTTILE_MAXH];

int findmaterials()
{
    editsurfs.setsize(0);
    loopi(4)
    {
        glasssurfs[i].setsize(0);
        watersurfs[i].setsize(0);
        waterfallsurfs[i].setsize(0);
        lavasurfs[i].setsize(0);
        lavafallsurfs[i].setsize(0);
        volfogsurfs[i].setsize(0);
    }
    matliquidsx1 = matliquidsy1 = matsolidsx1 = matsolidsy1 = matrefractsx1 = matrefractsy1 = 1;
    matliquidsx2 = matliquidsy2 = matsolidsx2 = matsolidsy2 = matrefractsx2 = matrefractsy2 = -1;
    memset(matliquidtiles, 0, sizeof(matliquidtiles));
    memset(matsolidtiles, 0, sizeof(matsolidtiles));
    int hasmats = 0;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->matsurfs || va->occluded >= OCCLUDE_BB || va->curvfc >= VFC_FOGGED) continue;
        if(editmode && showmat && !drawtex)
        {
            loopi(va->matsurfs) editsurfs.add(va->matbuf[i]);

            continue;
        }
        float sx1, sy1, sx2, sy2;
        if(va->lavamin.x <= va->lavamax.x && calcbbscissor(va->lavamin, va->lavamax, sx1, sy1, sx2, sy2))
        {
            matliquidsx1 = min(matliquidsx1, sx1);
            matliquidsy1 = min(matliquidsy1, sy1);
            matliquidsx2 = max(matliquidsx2, sx2);
            matliquidsy2 = max(matliquidsy2, sy2);
            masktiles(matliquidtiles, sx1, sy1, sx2, sy2);
            loopi(va->matsurfs)
            {
                materialsurface &m = va->matbuf[i];
                if((m.material&MATF_VOLUME) != MAT_LAVA || m.visible == MATSURF_EDIT_ONLY) { i += m.skip; continue; }
                hasmats |= 1;
                if(m.orient == O_TOP) lavasurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                else lavafallsurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                i += m.skip;
            }
        }
        if(va->watermin.x <= va->watermax.x && calcbbscissor(va->watermin, va->watermax, sx1, sy1, sx2, sy2))
        {
            matliquidsx1 = min(matliquidsx1, sx1);
            matliquidsy1 = min(matliquidsy1, sy1);
            matliquidsx2 = max(matliquidsx2, sx2);
            matliquidsy2 = max(matliquidsy2, sy2);
            masktiles(matliquidtiles, sx1, sy1, sx2, sy2);
            matrefractsx1 = min(matrefractsx1, sx1);
            matrefractsy1 = min(matrefractsy1, sy1);
            matrefractsx2 = max(matrefractsx2, sx2);
            matrefractsy2 = max(matrefractsy2, sy2);
            loopi(va->matsurfs)
            {
                materialsurface &m = va->matbuf[i];
                if((m.material&MATF_VOLUME) != MAT_WATER || m.visible == MATSURF_EDIT_ONLY) { i += m.skip; continue; }
                hasmats |= 4|1;
                if(m.orient == O_TOP) watersurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                else waterfallsurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                i += m.skip;
            }
        }
        if(va->volfogmin.x <= va->volfogmax.x && calcbbscissor(va->volfogmin, va->volfogmax, sx1, sy1, sx2, sy2))
        {
            matliquidsx1 = min(matliquidsx1, sx1);
            matliquidsy1 = min(matliquidsy1, sy1);
            matliquidsx2 = max(matliquidsx2, sx2);
            matliquidsy2 = max(matliquidsy2, sy2);
            masktiles(matliquidtiles, sx1, sy1, sx2, sy2);
            matrefractsx1 = min(matrefractsx1, sx1);
            matrefractsy1 = min(matrefractsy1, sy1);
            matrefractsx2 = max(matrefractsx2, sx2);
            matrefractsy2 = max(matrefractsy2, sy2);
            loopi(va->matsurfs)
            {
                materialsurface &m = va->matbuf[i];
                if((m.material&MATF_VOLUME) != MAT_VOLFOG || m.visible == MATSURF_EDIT_ONLY) { i += m.skip; continue; }
                hasmats |= 4|1;
                if(m.orient == O_TOP) volfogsurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                i += m.skip;
            }
        }
        if(drawtex != DRAWTEX_ENVMAP && va->glassmin.x <= va->glassmax.x && calcbbscissor(va->glassmin, va->glassmax, sx1, sy1, sx2, sy2))
        {
            matsolidsx1 = min(matsolidsx1, sx1);
            matsolidsy1 = min(matsolidsy1, sy1);
            matsolidsx2 = max(matsolidsx2, sx2);
            matsolidsy2 = max(matsolidsy2, sy2);
            masktiles(matsolidtiles, sx1, sy1, sx2, sy2);
            matrefractsx1 = min(matrefractsx1, sx1);
            matrefractsy1 = min(matrefractsy1, sy1);
            matrefractsx2 = max(matrefractsx2, sx2);
            matrefractsy2 = max(matrefractsy2, sy2);
            loopi(va->matsurfs)
            {
                materialsurface &m = va->matbuf[i];
                if((m.material&MATF_VOLUME) != MAT_GLASS || m.visible == MATSURF_EDIT_ONLY) { i += m.skip; continue; }
                hasmats |= 4|2;
                glasssurfs[m.material&MATF_INDEX].put(&m, 1+int(m.skip));
                i += m.skip;
            }
        }
    }
    return hasmats;
}

void rendermaterialmask()
{
    glDisable(GL_CULL_FACE);
    loopk(4) { vector<materialsurface> &surfs = glasssurfs[k]; loopv(surfs) drawmaterial(surfs[i], VOLUME_INSET); }
    loopk(4) { vector<materialsurface> &surfs = watersurfs[k]; loopv(surfs) drawmaterial(surfs[i], VOLUME_OFFSET); }
    loopk(4) { vector<materialsurface> &surfs = waterfallsurfs[k]; loopv(surfs) drawmaterial(surfs[i], VOLUME_INSET); }
    loopk(4) { vector<materialsurface> &surfs = volfogsurfs[k]; loopv(surfs) drawmaterial(surfs[i], 0.f); }
    xtraverts += gle::end();
    glEnable(GL_CULL_FACE);
}

#define GLASSVARS(type, name) \
    VAR(IDF_MAP, name##enabled##type, 0, 1, 1); \
    CVAR(IDF_MAP, name##colour##type, 0xB0D8FF); \
    FVAR(IDF_MAP, name##refract##type, 0, 0.1f, 1e3f); \
    VAR(IDF_MAP, name##spec##type, 0, 150, 200);

GLASSVARS(, glass)
GLASSVARS(, glass2)
GLASSVARS(, glass3)
GLASSVARS(, glass4)
GLASSVARS(alt, glass)
GLASSVARS(alt, glass2)
GLASSVARS(alt, glass3)
GLASSVARS(alt, glass4)

GETMATIDXVAR(glass, enabled, int)
GETMATIDXVARDARK(glass, colour, const bvec &)
GETMATIDXVAR(glass, refract, float)
GETMATIDXVAR(glass, spec, int)

VARF(IDF_PERSIST, glassenv, 0, 1, 1, preloadglassshaders());

void renderglass()
{
    loopk(4)
    {
        vector<materialsurface> &surfs = glasssurfs[k];
        if(surfs.empty()) continue;

        MatSlot &gslot = lookupmaterialslot(MAT_GLASS+k);

        Texture *tex = gslot.sts.inrange(0) ? gslot.sts[0].t : notexture;
        float xscale = TEX_SCALE/(tex->xs*gslot.scale);
        float yscale = TEX_SCALE/(tex->ys*gslot.scale);
        GLOBALPARAMF(glasstexgen, xscale, yscale);

        glActiveTexture_(GL_TEXTURE1);
        settexture(tex);
        glActiveTexture_(GL_TEXTURE0);

        float refractscale = (0.5f/255)/ldrscale;
        const bvec &col = getglasscolour(k);
        float refract = getglassrefract(k);
        int spec = getglassspec(k);
        GLOBALPARAMF(glassrefract, col.x*refractscale, col.y*refractscale, col.z*refractscale, refract*viewh);
        GLOBALPARAMF(glassspec, spec/100.0f);

        short envmap = EMID_NONE;
        if(!glassenv) SETSHADER(glass);
        loopv(surfs)
        {
            materialsurface &m = surfs[i];
            if(m.envmap != envmap && glassenv)
            {
                xtraverts += gle::end();
                if(m.envmap != EMID_NONE && glassenv) SETSHADER(glassenv);
                else SETSHADER(glass);
                glBindTexture(GL_TEXTURE_CUBE_MAP, lookupenvmap(m.envmap));
                envmap = m.envmap;
            }
            drawglass(m, VOLUME_INSET);
        }
        xtraverts += gle::end();
    }
}

void renderliquidmaterials()
{
    glDisable(GL_CULL_FACE);

    renderlava();
    renderwater();
    renderwaterfalls();
    rendervolfog();

    glEnable(GL_CULL_FACE);
}

void rendersolidmaterials()
{
    glDisable(GL_CULL_FACE);

    renderglass();

    glEnable(GL_CULL_FACE);
}

FVAR(IDF_PERSIST, editmatscale, 0, 0.125f, 1);
FVAR(IDF_PERSIST, editmatblend, 0, 0.5f, 1);

void rendereditmaterials()
{
    if(editsurfs.empty()) return;

    sorteditmaterials();

    glDisable(GL_CULL_FACE);

    zerofogcolor();

    foggednotextureshader->set();

    if(!editmatoffset) enablepolygonoffset(GL_POLYGON_OFFSET_FILL);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    int lastmat = -1;
    bvec4 color(0, 0, 0, 0);
    loopv(editsurfs)
    {
        const materialsurface &m = editsurfs[i];
        if(lastmat!=m.material)
        {
            switch(m.material&~MATF_INDEX)
            {
                EDITMATCOLOR(color)
                default: continue;
            }
            color.mul(editmatscale, editmatscale, editmatscale, editmatblend);
            lastmat = m.material;
        }
        drawmaterial(m, editmatoffset ? -VOLUME_INSET : 0.0f, color);
    }

    xtraverts += gle::end();

    glDisable(GL_BLEND);

    if(!editmatoffset) disablepolygonoffset(GL_POLYGON_OFFSET_FILL);

    resetfogcolor();

    rendermatgrid();

    glEnable(GL_CULL_FACE);
}

void renderminimapmaterials()
{
    glDisable(GL_CULL_FACE);

    renderlava();
    renderwater();
    rendervolfog();

    glEnable(GL_CULL_FACE);
}

