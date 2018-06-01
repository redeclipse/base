// octarender.cpp: fill vertex arrays with different cube surfaces.

#include "engine.h"

struct vboinfo
{
    int uses;
    uchar *data;
};

hashtable<GLuint, vboinfo> vbos;

VAR(0, printvbo, 0, 0, 1);
VARFN(0, vbosize, maxvbosize, 0, 1<<14, 1<<16, allchanged());

enum
{
    VBO_VBUF = 0,
    VBO_EBUF,
    VBO_SKYBUF,
    VBO_DECALBUF,
    NUMVBO
};

static vector<uchar> vbodata[NUMVBO];
static vector<vtxarray *> vbovas[NUMVBO];
static int vbosize[NUMVBO];

void destroyvbo(GLuint vbo)
{
    vboinfo *exists = vbos.access(vbo);
    if(!exists) return;
    vboinfo &vbi = *exists;
    if(vbi.uses <= 0) return;
    vbi.uses--;
    if(!vbi.uses)
    {
        glDeleteBuffers_(1, &vbo);
        if(vbi.data) delete[] vbi.data;
        vbos.remove(vbo);
    }
}

void genvbo(int type, void *buf, int len, vtxarray **vas, int numva)
{
    gle::disable();

    GLuint vbo;
    glGenBuffers_(1, &vbo);
    GLenum target = type==VBO_VBUF ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
    glBindBuffer_(target, vbo);
    glBufferData_(target, len, buf, GL_STATIC_DRAW);
    glBindBuffer_(target, 0);

    vboinfo &vbi = vbos[vbo];
    vbi.uses = numva;
    vbi.data = new uchar[len];
    memcpy(vbi.data, buf, len);

    if(printvbo) conoutf("VBO %d: type %d, size %d, %d uses", vbo, type, len, numva);

    loopi(numva)
    {
        vtxarray *va = vas[i];
        switch(type)
        {
            case VBO_VBUF:
                va->vbuf = vbo;
                va->vdata = (vertex *)vbi.data;
                break;
            case VBO_EBUF:
                va->ebuf = vbo;
                va->edata = (ushort *)vbi.data;
                break;
            case VBO_SKYBUF:
                va->skybuf = vbo;
                va->skydata = (ushort *)vbi.data;
                break;
            case VBO_DECALBUF:
                va->decalbuf = vbo;
                va->decaldata = (ushort *)vbi.data;
                break;
        }
    }
}

void flushvbo(int type = -1)
{
    if(type < 0)
    {
        loopi(NUMVBO) flushvbo(i);
        return;
    }

    vector<uchar> &data = vbodata[type];
    if(data.empty()) return;
    vector<vtxarray *> &vas = vbovas[type];
    genvbo(type, data.getbuf(), data.length(), vas.getbuf(), vas.length());
    data.setsize(0);
    vas.setsize(0);
    vbosize[type] = 0;
}

uchar *addvbo(vtxarray *va, int type, int numelems, int elemsize)
{
    switch(type)
    {
        case VBO_VBUF: va->voffset = vbosize[type]; break;
        case VBO_EBUF: va->eoffset = vbosize[type]; break;
        case VBO_SKYBUF: va->skyoffset = vbosize[type]; break;
        case VBO_DECALBUF: va->decaloffset = vbosize[type]; break;
    }

    vbosize[type] += numelems;

    vector<uchar> &data = vbodata[type];
    vector<vtxarray *> &vas = vbovas[type];

    vas.add(va);

    int len = numelems*elemsize;
    uchar *buf = data.reserve(len).buf;
    data.advance(len);
    return buf;
}

struct verthash
{
    static const int SIZE = 1<<13;
    int table[SIZE];
    vector<vertex> verts;
    vector<int> chain;

    verthash() { clearverts(); }

    void clearverts()
    {
        memset(table, -1, sizeof(table));
        chain.setsize(0);
        verts.setsize(0);
    }

    int addvert(const vertex &v)
    {
        uint h = hthash(v.pos)&(SIZE-1);
        for(int i = table[h]; i>=0; i = chain[i])
        {
            const vertex &c = verts[i];
            if(c.pos==v.pos && c.tc==v.tc && c.norm==v.norm && c.tangent==v.tangent)
                 return i;
        }
        if(verts.length() >= USHRT_MAX) return -1;
        verts.add(v);
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    }

    int addvert(const vec &pos, const vec &tc = vec(0, 0, 0), const bvec &norm = bvec(128, 128, 128), const bvec4 &tangent = bvec4(128, 128, 128, 128))
    {
        vertex vtx;
        vtx.pos = pos;
        vtx.tc = tc;
        vtx.norm = norm;
        vtx.tangent = tangent;
        return addvert(vtx);
    }
};

enum
{
    NO_ALPHA = 0,
    ALPHA_BACK,
    ALPHA_FRONT,
    ALPHA_REFRACT
};

struct sortkey
{
    ushort tex, envmap;
    uchar orient, layer, alpha;

    sortkey() {}
    sortkey(ushort tex, uchar orient, uchar layer = LAYER_TOP, ushort envmap = EMID_NONE, uchar alpha = NO_ALPHA)
     : tex(tex), envmap(envmap), orient(orient), layer(layer), alpha(alpha)
    {}

    bool operator==(const sortkey &o) const { return tex==o.tex && envmap==o.envmap && orient==o.orient && layer==o.layer && alpha==o.alpha; }

    static inline bool sort(const sortkey &x, const sortkey &y)
    {
        if(x.alpha < y.alpha) return true;
        if(x.alpha > y.alpha) return false;
        if(x.layer < y.layer) return true;
        if(x.layer > y.layer) return false;
        if(x.tex == y.tex)
        {
            if(x.envmap < y.envmap) return true;
            if(x.envmap > y.envmap) return false;
            if(x.orient < y.orient) return true;
            if(x.orient > y.orient) return false;
            return false;
        }
        VSlot &xs = lookupvslot(x.tex, false), &ys = lookupvslot(y.tex, false);
        if(xs.slot->shader < ys.slot->shader) return true;
        if(xs.slot->shader > ys.slot->shader) return false;
        if(xs.slot->params.length() < ys.slot->params.length()) return true;
        if(xs.slot->params.length() > ys.slot->params.length()) return false;
        if(x.tex < y.tex) return true;
        else return false;
    }
};

static inline bool htcmp(const sortkey &x, const sortkey &y)
{
    return x == y;
}

static inline uint hthash(const sortkey &k)
{
    return k.tex;
}

struct decalkey
{
    ushort tex, envmap, reuse;

    decalkey() {}
    decalkey(ushort tex, ushort envmap = EMID_NONE, ushort reuse = 0)
     : tex(tex), envmap(envmap), reuse(reuse)
    {}

    bool operator==(const decalkey &o) const { return tex==o.tex && envmap==o.envmap && reuse==o.reuse; }

    static inline bool sort(const decalkey &x, const decalkey &y)
    {
        if(x.tex == y.tex)
        {
            if(x.envmap < y.envmap) return true;
            if(x.envmap > y.envmap) return false;
            if(x.reuse < y.reuse) return true;
            else return false;
        }
        DecalSlot &xs = lookupdecalslot(x.tex, false), &ys = lookupdecalslot(y.tex, false);
        if(xs.slot->shader < ys.slot->shader) return true;
        if(xs.slot->shader > ys.slot->shader) return false;
        if(xs.slot->params.length() < ys.slot->params.length()) return true;
        if(xs.slot->params.length() > ys.slot->params.length()) return false;
        if(x.tex < y.tex) return true;
        else return false;
    }
};

static inline bool htcmp(const decalkey &x, const decalkey &y)
{
    return x == y;
}

static inline uint hthash(const decalkey &k)
{
    return k.tex;
}

struct sortval
{
     vector<ushort> tris;

     sortval() {}
};
struct vacollect : verthash
{
    ivec origin;
    int size;
    hashtable<sortkey, sortval> indices;
    hashtable<decalkey, sortval> decalindices;
    vector<ushort> skyindices;
    vector<sortkey> texs;
    vector<decalkey> decaltexs;
    vector<grasstri> grasstris;
    vector<materialsurface> matsurfs;
    vector<octaentities *> mapmodels, decals, extdecals;
    int worldtris, skytris, decaltris;
    vec alphamin, alphamax;
    vec refractmin, refractmax;
    vec skymin, skymax;
    ivec nogimin, nogimax;

    void clear()
    {
        clearverts();
        worldtris = skytris = decaltris = 0;
        indices.clear();
        decalindices.clear();
        skyindices.setsize(0);
        matsurfs.setsize(0);
        mapmodels.setsize(0);
        decals.setsize(0);
        extdecals.setsize(0);
        grasstris.setsize(0);
        texs.setsize(0);
        decaltexs.setsize(0);
        alphamin = refractmin = skymin = vec(1e16f, 1e16f, 1e16f);
        alphamax = refractmax = skymax = vec(-1e16f, -1e16f, -1e16f);
        nogimin = ivec(INT_MAX, INT_MAX, INT_MAX);
        nogimax = ivec(INT_MIN, INT_MIN, INT_MIN);
    }

    void optimize()
    {
        enumeratekt(indices, sortkey, k, sortval, t,
        {
            if(t.tris.length()) texs.add(k);
        });
        texs.sort(sortkey::sort);

        matsurfs.shrink(optimizematsurfs(matsurfs.getbuf(), matsurfs.length()));
    }

#define GENVERTS(type, ptr, body) do \
    { \
        type *f = (type *)ptr; \
        loopv(verts) \
        { \
            const vertex &v = verts[i]; \
            body; \
            f++; \
        } \
    } while(0)

    void genverts(void *buf)
    {
        GENVERTS(vertex, buf, { *f = v; f->norm.flip(); f->tangent.flip(); });
    }

    void gendecal(const extentity &e, DecalSlot &s, const decalkey &key)
    {
        matrix3 orient;
        orient.identity();
        if(e.attrs[1]) orient.rotate_around_z(sincosmod360(e.attrs[1]));
        if(e.attrs[2]) orient.rotate_around_x(sincosmod360(e.attrs[2]));
        if(e.attrs[3]) orient.rotate_around_y(sincosmod360(-e.attrs[3]));
        vec size(max(float(e.attrs[4]), 1.0f));
        size.y *= s.depth;
        if(!s.sts.empty())
        {
            Texture *t = s.sts[0].t;
            if(t->xs < t->ys) size.x *= t->xs / float(t->ys);
            else if(t->xs > t->ys) size.z *= t->ys / float(t->xs);
        }
        vec center = orient.transform(vec(0, size.y*0.5f, 0)).add(e.o), radius = orient.abstransform(vec(size).mul(0.5f));
        vec bbmin = vec(center).sub(radius), bbmax = vec(center).add(radius);
        vec clipoffset = orient.transposedtransform(center).msub(size, 0.5f);
        loopv(texs)
        {
            const sortkey &k = texs[i];
            if(k.layer == LAYER_BLEND || k.alpha != NO_ALPHA) continue;
            const sortval &t = indices[k];
            if(t.tris.empty()) continue;
            decalkey tkey(key);
            if(shouldreuseparams(s, lookupvslot(k.tex, false))) tkey.reuse = k.tex;
            for(int j = 0; j < t.tris.length(); j += 3)
            {
                const vertex &t0 = verts[t.tris[j]], &t1 = verts[t.tris[j+1]], &t2 = verts[t.tris[j+2]];
                vec v0 = t0.pos, v1 = t1.pos, v2 = t2.pos;
                vec tmin = vec(v0).min(v1).min(v2), tmax = vec(v0).max(v1).max(v2);
                if(tmin.x >= bbmax.x || tmin.y >= bbmax.y || tmin.z >= bbmax.z ||
                   tmax.x <= bbmin.x || tmax.y <= bbmin.y || tmax.z <= bbmin.z)
                    continue;
                float f0 = t0.norm.tonormal().dot(orient.b), f1 = t1.norm.tonormal().dot(orient.b), f2 = t2.norm.tonormal().dot(orient.b);
                if(f0 >= 0 && f1 >= 0 && f2 >= 0) continue;
                vec p1[9], p2[9];
                p1[0] = v0; p1[1] = v1; p1[2] = v2;
                int nump = polyclip(p1, 3, orient.b, clipoffset.y, clipoffset.y + size.y, p2);
                if(nump < 3) continue;
                nump = polyclip(p2, nump, orient.a, clipoffset.x, clipoffset.x + size.x, p1);
                if(nump < 3) continue;
                nump = polyclip(p1, nump, orient.c, clipoffset.z, clipoffset.z + size.z, p2);
                if(nump < 3) continue;

                bvec4 n0 = t0.norm, n1 = t1.norm, n2 = t2.norm,
                      x0 = t0.tangent, x1 = t1.tangent, x2 = t2.tangent;
                vec e1 = vec(v1).sub(v0), e2 = vec(v2).sub(v0);
                float d11 = e1.dot(e1), d12 = e1.dot(e2), d22 = e2.dot(e2);
                int idx[9];
                loopk(nump)
                {
                    vertex v;
                    v.pos = p2[k];
                    vec ep = vec(v.pos).sub(v0);
                    float dp1 = ep.dot(e1), dp2 = ep.dot(e2), denom = d11*d22 - d12*d12,
                          b1 = (d22*dp1 - d12*dp2) / denom,
                          b2 = (d11*dp2 - d12*dp1) / denom,
                          b0 = 1 - b1 - b2;
                    v.norm.lerp(n0, n1, n2, b0, b1, b2);
                    v.norm.w = uchar(127.5f - 127.5f*(f0*b0 + f1*b1 + f2*b2));
                    vec tc = orient.transposedtransform(vec(center).sub(v.pos)).div(size).add(0.5f);
                    v.tc = vec(tc.x, tc.z, s.fade ? tc.y * s.depth / s.fade : 1.0f);
                    v.tangent.lerp(x0, x1, x2, b0, b1, b2);
                    idx[k] = addvert(v);
                }
                vector<ushort> &tris = decalindices[tkey].tris;
                loopk(nump-2) if(idx[0] != idx[k+1] && idx[k+1] != idx[k+2] && idx[k+2] != idx[0])
                {
                    tris.add(idx[0]);
                    tris.add(idx[k+1]);
                    tris.add(idx[k+2]);
                    decaltris += 3;
                }
            }
        }
    }

    void gendecals()
    {
        if(decals.length()) extdecals.put(decals.getbuf(), decals.length());
        if(extdecals.empty()) return;
        vector<extentity *> &ents = entities::getents();
        loopv(extdecals)
        {
            octaentities *oe = extdecals[i];
            loopvj(oe->decals)
            {
                extentity &e = *ents[oe->decals[j]];
                if(e.flags&EF_RENDER || !checkmapvariant(e.attrs[5]) || e.attrs[6] > mapeffects) continue;
                e.flags |= EF_RENDER;
                DecalSlot &s = lookupdecalslot(e.attrs[0], true);
                if(!s.shader) continue;
                ushort envmap = s.shader->type&SHADER_ENVMAP ? (s.texmask&(1<<TEX_ENVMAP) ? EMID_CUSTOM : closestenvmap(e.o)) : EMID_NONE;
                decalkey k(e.attrs[0], envmap);
                gendecal(e, s, k);
            }
        }
        loopv(extdecals)
        {
            octaentities *oe = extdecals[i];
            loopvj(oe->decals)
            {
                extentity &e = *ents[oe->decals[j]];
                if(e.flags&EF_RENDER) e.flags &= ~EF_RENDER;
            }
        }
        enumeratekt(decalindices, decalkey, k, sortval, t,
        {
            if(t.tris.length()) decaltexs.add(k);
        });
        decaltexs.sort(decalkey::sort);
    }

    void setupdata(vtxarray *va)
    {
        optimize();
        gendecals();

        va->verts = verts.length();
        va->tris = worldtris/3;
        va->vbuf = 0;
        va->vdata = 0;
        va->minvert = 0;
        va->maxvert = va->verts-1;
        va->voffset = 0;
        if(va->verts)
        {
            if(vbosize[VBO_VBUF] + verts.length() > maxvbosize ||
               vbosize[VBO_EBUF] + worldtris > USHRT_MAX ||
               vbosize[VBO_SKYBUF] + skytris > USHRT_MAX ||
               vbosize[VBO_DECALBUF] + decaltris > USHRT_MAX)
                flushvbo();

            uchar *vdata = addvbo(va, VBO_VBUF, va->verts, sizeof(vertex));
            genverts(vdata);
            va->minvert += va->voffset;
            va->maxvert += va->voffset;
        }

        va->matbuf = NULL;
        va->matsurfs = matsurfs.length();
        va->matmask = 0;
        if(va->matsurfs)
        {
            va->matbuf = new materialsurface[matsurfs.length()];
            memcpy(va->matbuf, matsurfs.getbuf(), matsurfs.length()*sizeof(materialsurface));
            loopv(matsurfs)
            {
                materialsurface &m = matsurfs[i];
                if(m.visible == MATSURF_EDIT_ONLY) continue;
                switch(m.material)
                {
                    case MAT_GLASS: case MAT_LAVA: case MAT_WATER: break;
                    default: continue;
                }
                va->matmask |= 1<<m.material;
            }
        }

        va->skybuf = 0;
        va->skydata = 0;
        va->skyoffset = 0;
        va->sky = skyindices.length();
        if(va->sky)
        {
            ushort *skydata = (ushort *)addvbo(va, VBO_SKYBUF, va->sky, sizeof(ushort));
            memcpy(skydata, skyindices.getbuf(), va->sky*sizeof(ushort));
            if(va->voffset) loopi(va->sky) skydata[i] += va->voffset;
        }

        va->texelems = NULL;
        va->texs = texs.length();
        va->blendtris = 0;
        va->blends = 0;
        va->alphabacktris = 0;
        va->alphaback = 0;
        va->alphafronttris = 0;
        va->alphafront = 0;
        va->refracttris = 0;
        va->refract = 0;
        va->ebuf = 0;
        va->edata = 0;
        va->eoffset = 0;
        if(va->texs)
        {
            va->texelems = new elementset[va->texs];
            ushort *edata = (ushort *)addvbo(va, VBO_EBUF, worldtris, sizeof(ushort)), *curbuf = edata;
            loopv(texs)
            {
                const sortkey &k = texs[i];
                const sortval &t = indices[k];
                elementset &e = va->texelems[i];
                e.texture = k.tex;
                e.orient = k.orient;
                e.layer = k.layer;
                e.envmap = k.envmap;
                ushort *startbuf = curbuf;
                e.minvert = USHRT_MAX;
                e.maxvert = 0;

                if(t.tris.length())
                {
                    memcpy(curbuf, t.tris.getbuf(), t.tris.length() * sizeof(ushort));

                    loopvj(t.tris)
                    {
                        curbuf[j] += va->voffset;
                        e.minvert = min(e.minvert, curbuf[j]);
                        e.maxvert = max(e.maxvert, curbuf[j]);
                    }

                    curbuf += t.tris.length();
                }
                e.length = curbuf-startbuf;

                if(k.layer==LAYER_BLEND) { va->texs--; va->tris -= e.length/3; va->blends++; va->blendtris += e.length/3; }
                else if(k.alpha==ALPHA_BACK) { va->texs--; va->tris -= e.length/3; va->alphaback++; va->alphabacktris += e.length/3; }
                else if(k.alpha==ALPHA_FRONT) { va->texs--; va->tris -= e.length/3; va->alphafront++; va->alphafronttris += e.length/3; }
                else if(k.alpha==ALPHA_REFRACT) { va->texs--; va->tris -= e.length/3; va->refract++; va->refracttris += e.length/3; }
            }
        }

        va->texmask = 0;
        va->dyntexs = 0;
        loopi(va->texs+va->blends+va->alphaback+va->alphafront+va->refract)
        {
            VSlot &vslot = lookupvslot(va->texelems[i].texture, false);
            if(vslot.isdynamic()) va->dyntexs++;
            Slot &slot = *vslot.slot;
            loopvj(slot.sts) va->texmask |= 1<<slot.sts[j].type;
            if(slot.shader->type&SHADER_ENVMAP) va->texmask |= 1<<TEX_ENVMAP;
        }

        va->decalbuf = 0;
        va->decaldata = 0;
        va->decaloffset = 0;
        va->decalelems = NULL;
        va->decaltexs = decaltexs.length();
        va->decaltris = decaltris/3;
        if(va->decaltexs)
        {
            va->decalelems = new elementset[va->decaltexs];
            ushort *edata = (ushort *)addvbo(va, VBO_DECALBUF, decaltris, sizeof(ushort)), *curbuf = edata;
            loopv(decaltexs)
            {
                const decalkey &k = decaltexs[i];
                const sortval &t = decalindices[k];
                elementset &e = va->decalelems[i];
                e.texture = k.tex;
                e.reuse = k.reuse;
                e.envmap = k.envmap;
                ushort *startbuf = curbuf;
                e.minvert = USHRT_MAX;
                e.maxvert = 0;

                if(t.tris.length())
                {
                    memcpy(curbuf, t.tris.getbuf(), t.tris.length() * sizeof(ushort));

                    loopvj(t.tris)
                    {
                        curbuf[j] += va->voffset;
                        e.minvert = min(e.minvert, curbuf[j]);
                        e.maxvert = max(e.maxvert, curbuf[j]);
                    }

                    curbuf += t.tris.length();
                }
                e.length = curbuf-startbuf;
            }
        }

        if(grasstris.length())
        {
            va->grasstris.move(grasstris);
            loadgrassshaders();
        }

        if(mapmodels.length()) va->mapmodels.put(mapmodels.getbuf(), mapmodels.length());
        if(decals.length()) va->decals.put(decals.getbuf(), decals.length());
    }

    bool emptyva()
    {
        return verts.empty() && matsurfs.empty() && skyindices.empty() && grasstris.empty() && mapmodels.empty() && decals.empty();
    }
} vc;

int recalcprogress = 0;
#define rcprogress(s)     if((recalcprogress++&0xFFF)==0) progress(recalcprogress/(float)allocnodes, s);

vector<tjoint> tjoints;

VARF(IDF_PERSIST, filltjoints, 0, 1, 1, allchanged());

void reduceslope(ivec &n)
{
    int mindim = -1, minval = 64;
    loopi(3) if(n[i])
    {
        int val = abs(n[i]);
        if(mindim < 0 || val < minval)
        {
            mindim = i;
            minval = val;
        }
    }
    if(!(n[R[mindim]]%minval) && !(n[C[mindim]]%minval)) n.div(minval);
    while(!((n.x|n.y|n.z)&1)) n.shr(1);
}

// [rotation][orient]
extern const vec orientation_tangent[6][6] =
{
    { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
    { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
    { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
    { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
    { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
    { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) }
};
extern const vec orientation_bitangent[6][6] =
{
    { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
    { vec( 0, -1,  0), vec( 0,  1,  0), vec( 1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0), vec(-1,  0,  0) },
    { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) },
    { vec( 0,  1,  0), vec( 0, -1,  0), vec(-1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0), vec( 1,  0,  0) },
    { vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0,  0, -1), vec( 0, -1,  0), vec( 0,  1,  0) },
    { vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  0,  1), vec( 0,  1,  0), vec( 0, -1,  0) }
};

void addtris(VSlot &vslot, int orient, const sortkey &key, vertex *verts, int *index, int numverts, int convex, int tj)
{
    int &total = key.tex==DEFAULT_SKY ? vc.skytris : vc.worldtris;
    int edge = orient*(MAXFACEVERTS+1);
    loopi(numverts-2) if(index[0]!=index[i+1] && index[i+1]!=index[i+2] && index[i+2]!=index[0])
    {
        vector<ushort> &idxs = key.tex==DEFAULT_SKY ? vc.skyindices : vc.indices[key].tris;
        int left = index[0], mid = index[i+1], right = index[i+2], start = left, i0 = left, i1 = -1;
        loopk(4)
        {
            int i2 = -1, ctj = -1, cedge = -1;
            switch(k)
            {
            case 1: i1 = i2 = mid; cedge = edge+i+1; break;
            case 2: if(i1 != mid || i0 == left) { i0 = i1; i1 = right; } i2 = right; if(i+1 == numverts-2) cedge = edge+i+2; break;
            case 3: if(i0 == start) { i0 = i1; i1 = left; } i2 = left; // fall-through
            default: if(!i) cedge = edge; break;
            }
            if(i1 != i2)
            {
                if(total + 3 > USHRT_MAX) return;
                total += 3;
                idxs.add(i0);
                idxs.add(i1);
                idxs.add(i2);
                i1 = i2;
            }
            if(cedge >= 0)
            {
                for(ctj = tj;;)
                {
                    if(ctj < 0) break;
                    if(tjoints[ctj].edge < cedge) { ctj = tjoints[ctj].next; continue; }
                    if(tjoints[ctj].edge != cedge) ctj = -1;
                    break;
                }
            }
            if(ctj >= 0)
            {
                int e1 = cedge%(MAXFACEVERTS+1), e2 = (e1+1)%numverts;
                vertex &v1 = verts[e1], &v2 = verts[e2];
                ivec d(vec(v2.pos).sub(v1.pos).mul(8));
                int axis = abs(d.x) > abs(d.y) ? (abs(d.x) > abs(d.z) ? 0 : 2) : (abs(d.y) > abs(d.z) ? 1 : 2);
                if(d[axis] < 0) d.neg();
                reduceslope(d);
                int origin = int(min(v1.pos[axis], v2.pos[axis])*8)&~0x7FFF,
                    offset1 = (int(v1.pos[axis]*8) - origin) / d[axis],
                    offset2 = (int(v2.pos[axis]*8) - origin) / d[axis];
                vec o = vec(v1.pos).sub(vec(d).mul(offset1/8.0f));
                float doffset = 1.0f / (offset2 - offset1);

                if(i1 < 0) for(;;)
                {
                    tjoint &t = tjoints[ctj];
                    if(t.next < 0 || tjoints[t.next].edge != cedge) break;
                    ctj = t.next;
                }
                while(ctj >= 0)
                {
                    tjoint &t = tjoints[ctj];
                    if(t.edge != cedge) break;
                    float offset = (t.offset - offset1) * doffset;
                    vertex vt;
                    vt.pos = vec(d).mul(t.offset/8.0f).add(o);
                    vt.tc.lerp(v1.tc, v2.tc, offset);
                    vt.norm.lerp(v1.norm, v2.norm, offset);
                    vt.tangent.lerp(v1.tangent, v2.tangent, offset);
                    if(v1.tangent.w != v2.tangent.w)
                        vt.tangent.w = orientation_bitangent[vslot.rotation][orient].scalartriple(vt.norm.tonormal(), vt.tangent.tonormal()) < 0 ? 0 : 255;
                    int i2 = vc.addvert(vt);
                    if(i2 < 0) return;
                    if(i1 >= 0)
                    {
                        if(total + 3 > USHRT_MAX) return;
                        total += 3;
                        idxs.add(i0);
                        idxs.add(i1);
                        idxs.add(i2);
                        i1 = i2;
                    }
                    else start = i0 = i2;
                    ctj = t.next;
                }
            }
        }
    }
}

void addgrasstri(int face, vertex *verts, int numv, ushort texture, int layer)
{
    grasstri &g = vc.grasstris.add();
    int i1, i2, i3, i4;
    if(numv <= 3 && face%2) { i1 = face+1; i2 = face+2; i3 = i4 = 0; }
    else { i1 = 0; i2 = face+1; i3 = face+2; i4 = numv > 3 ? face+3 : i3; }
    g.v[0] = verts[i1].pos;
    g.v[1] = verts[i2].pos;
    g.v[2] = verts[i3].pos;
    g.v[3] = verts[i4].pos;
    g.numv = numv;

    g.surface.toplane(g.v[0], g.v[1], g.v[2]);
    if(g.surface.z <= 0) { vc.grasstris.pop(); return; }

    g.minz = min(min(g.v[0].z, g.v[1].z), min(g.v[2].z, g.v[3].z));
    g.maxz = max(max(g.v[0].z, g.v[1].z), max(g.v[2].z, g.v[3].z));

    g.center = vec(0, 0, 0);
    loopk(numv) g.center.add(g.v[k]);
    g.center.div(numv);
    g.radius = 0;
    loopk(numv) g.radius = max(g.radius, g.v[k].dist(g.center));

    g.texture = texture;
    g.blend = layer == LAYER_BLEND ? ((int(g.center.x)>>12)+1) | (((int(g.center.y)>>12)+1)<<8) : 0;
}

static inline void calctexgen(VSlot &vslot, int orient, vec4 &sgen, vec4 &tgen)
{
    Texture *tex = vslot.slot->sts.empty() ? notexture : vslot.slot->sts[0].t;
    float k = TEX_SCALE/vslot.scale,
          xs = vslot.rotation>=2 && vslot.rotation<=4 ? -tex->xs : tex->xs,
          ys = (vslot.rotation>=1 && vslot.rotation<=2) || vslot.rotation==5 ? -tex->ys : tex->ys,
          sk = k/xs, tk = k/ys,
          soff = -((vslot.rotation&5)==1 ? vslot.offset.y : vslot.offset.x)/xs,
          toff = -((vslot.rotation&5)==1 ? vslot.offset.x : vslot.offset.y)/ys;
    sgen = vec4(0, 0, 0, soff);
    tgen = vec4(0, 0, 0, toff);
    if((vslot.rotation&5)==1) switch(orient)
    {
        case 0: sgen.z = -sk; tgen.y = tk;  break;
        case 1: sgen.z = -sk; tgen.y = -tk; break;
        case 2: sgen.z = -sk; tgen.x = -tk; break;
        case 3: sgen.z = -sk; tgen.x = tk;  break;
        case 4: sgen.y = -sk; tgen.x = tk;  break;
        case 5: sgen.y = sk;  tgen.x = tk;  break;
    }
    else switch(orient)
    {
        case 0: sgen.y = sk;  tgen.z = -tk; break;
        case 1: sgen.y = -sk; tgen.z = -tk; break;
        case 2: sgen.x = -sk; tgen.z = -tk; break;
        case 3: sgen.x = sk;  tgen.z = -tk; break;
        case 4: sgen.x = sk;  tgen.y = -tk; break;
        case 5: sgen.x = sk;  tgen.y = tk;  break;
    }
}

ushort encodenormal(const vec &n)
{
    if(n.iszero()) return 0;
    int yaw = int(-atan2(n.x, n.y)/RAD), pitch = int(asin(n.z)/RAD);
    return ushort(clamp(pitch + 90, 0, 180)*360 + (yaw < 0 ? yaw%360 + 360 : yaw%360) + 1);
}

vec decodenormal(ushort norm)
{
    if(!norm) return vec(0, 0, 1);
    norm--;
    const vec2 &yaw = sincos360[norm%360], &pitch = sincos360[norm/360+270];
    return vec(-yaw.y*pitch.x, yaw.x*pitch.x, pitch.y);
}

void guessnormals(const vec *pos, int numverts, vec *normals)
{
    vec n1, n2;
    n1.cross(pos[0], pos[1], pos[2]);
    if(numverts != 4)
    {
        n1.normalize();
        loopk(numverts) normals[k] = n1;
        return;
    }
    n2.cross(pos[0], pos[2], pos[3]);
    if(n1.iszero())
    {
        n2.normalize();
        loopk(4) normals[k] = n2;
        return;
    }
    else n1.normalize();
    if(n2.iszero())
    {
        loopk(4) normals[k] = n1;
        return;
    }
    else n2.normalize();
    vec avg = vec(n1).add(n2).normalize();
    normals[0] = avg;
    normals[1] = n1;
    normals[2] = avg;
    normals[3] = n2;
}

void addcubeverts(VSlot &vslot, int orient, int size, vec *pos, int convex, ushort texture, vertinfo *vinfo, int numverts, int tj = -1, ushort envmap = EMID_NONE, int grassy = 0, bool alpha = false, int layer = LAYER_TOP)
{
    vec4 sgen, tgen;
    calctexgen(vslot, orient, sgen, tgen);
    vertex verts[MAXFACEVERTS];
    int index[MAXFACEVERTS];
    vec normals[MAXFACEVERTS];
    loopk(numverts)
    {
        vertex &v = verts[k];
        v.pos = pos[k];
        v.tc = vec(sgen.dot(v.pos), tgen.dot(v.pos), 0);
        if(vinfo && vinfo[k].norm)
        {
            vec n = decodenormal(vinfo[k].norm), t = orientation_tangent[vslot.rotation][orient];
            t.project(n).normalize();
            v.norm = bvec(n);
            v.tangent = bvec4(bvec(t), orientation_bitangent[vslot.rotation][orient].scalartriple(n, t) < 0 ? 0 : 255);
        }
        else if(texture != DEFAULT_SKY)
        {
            if(!k) guessnormals(pos, numverts, normals);
            const vec &n = normals[k];
            vec t = orientation_tangent[vslot.rotation][orient];
            t.project(n).normalize();
            v.norm = bvec(n);
            v.tangent = bvec4(bvec(t), orientation_bitangent[vslot.rotation][orient].scalartriple(n, t) < 0 ? 0 : 255);
        }
        else
        {
            v.norm = bvec(128, 128, 255);
            v.tangent = bvec4(255, 128, 128, 255);
        }
        index[k] = vc.addvert(v);
        if(index[k] < 0) return;
    }

    if(alpha)
    {
        loopk(numverts) { vc.alphamin.min(pos[k]); vc.alphamax.max(pos[k]); }
        if(vslot.refractscale > 0) loopk(numverts) { vc.refractmin.min(pos[k]); vc.refractmax.max(pos[k]); }
    }
    if(texture == DEFAULT_SKY) loopi(numverts) if(pos[i][orient>>1] != ((orient&1)<<worldscale))
    {
        loopk(numverts) { vc.skymin.min(pos[k]); vc.skymax.max(pos[k]); }
        break;
    }

    sortkey key(texture, vslot.scroll.iszero() ? O_ANY : orient, layer&LAYER_BOTTOM ? layer : LAYER_TOP, envmap, alpha ? (vslot.refractscale > 0 ? ALPHA_REFRACT : (vslot.alphaback ? ALPHA_BACK : ALPHA_FRONT)) : NO_ALPHA);
    addtris(vslot, orient, key, verts, index, numverts, convex, tj);

    if(grassy)
    {
        for(int i = 0; i < numverts-2; i += 2)
        {
            int faces = 0;
            if(index[0]!=index[i+1] && index[i+1]!=index[i+2] && index[i+2]!=index[0]) faces |= 1;
            if(i+3 < numverts && index[0]!=index[i+2] && index[i+2]!=index[i+3] && index[i+3]!=index[0]) faces |= 2;
            if(grassy > 1 && faces==3) addgrasstri(i, verts, 4, texture, layer);
            else
            {
                if(faces&1) addgrasstri(i, verts, 3, texture, layer);
                if(faces&2) addgrasstri(i+1, verts, 3, texture, layer);
            }
        }
    }
}

struct edgegroup
{
    ivec slope, origin;
    int axis;
};

static inline uint hthash(const edgegroup &g)
{
    return g.slope.x^g.slope.y^g.slope.z^g.origin.x^g.origin.y^g.origin.z;
}

static inline bool htcmp(const edgegroup &x, const edgegroup &y)
{
    return x.slope==y.slope && x.origin==y.origin;
}

enum
{
    CE_START = 1<<0,
    CE_END   = 1<<1,
    CE_FLIP  = 1<<2,
    CE_DUP   = 1<<3
};

struct cubeedge
{
    cube *c;
    int next, offset;
    ushort size;
    uchar index, flags;
};

vector<cubeedge> cubeedges;
hashtable<edgegroup, int> edgegroups(1<<13);

void gencubeedges(cube &c, const ivec &co, int size)
{
    ivec pos[MAXFACEVERTS];
    int vis;
    loopi(6) if((vis = visibletris(c, i, co, size)))
    {
        int numverts = c.ext ? c.ext->surfaces[i].numverts&MAXFACEVERTS : 0;
        if(numverts)
        {
            vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
            ivec vo = ivec(co).mask(~0xFFF).shl(3);
            loopj(numverts)
            {
                vertinfo &v = verts[j];
                pos[j] = ivec(v.x, v.y, v.z).add(vo);
            }
        }
        else if(c.merged&(1<<i)) continue;
        else
        {
            ivec v[4];
            genfaceverts(c, i, v);
            int order = vis&4 || (!flataxisface(c, i) && faceconvexity(v) < 0) ? 1 : 0;
            ivec vo = ivec(co).shl(3);
            pos[numverts++] = v[order].mul(size).add(vo);
            if(vis&1) pos[numverts++] = v[order+1].mul(size).add(vo);
            pos[numverts++] = v[order+2].mul(size).add(vo);
            if(vis&2) pos[numverts++] = v[(order+3)&3].mul(size).add(vo);
        }
        loopj(numverts)
        {
            int e1 = j, e2 = j+1 < numverts ? j+1 : 0;
            ivec d = pos[e2];
            d.sub(pos[e1]);
            if(d.iszero()) continue;
            int axis = abs(d.x) > abs(d.y) ? (abs(d.x) > abs(d.z) ? 0 : 2) : (abs(d.y) > abs(d.z) ? 1 : 2);
            if(d[axis] < 0)
            {
                d.neg();
                swap(e1, e2);
            }
            reduceslope(d);

            int t1 = pos[e1][axis]/d[axis],
                t2 = pos[e2][axis]/d[axis];
            edgegroup g;
            g.origin = ivec(pos[e1]).sub(ivec(d).mul(t1));
            g.slope = d;
            g.axis = axis;
            cubeedge ce;
            ce.c = &c;
            ce.offset = t1;
            ce.size = t2 - t1;
            ce.index = i*(MAXFACEVERTS+1)+j;
            ce.flags = CE_START | CE_END | (e1!=j ? CE_FLIP : 0);
            ce.next = -1;

            bool insert = true;
            int *exists = edgegroups.access(g);
            if(exists)
            {
                int prev = -1, cur = *exists;
                while(cur >= 0)
                {
                    cubeedge &p = cubeedges[cur];
                    if(p.flags&CE_DUP ?
                        ce.offset>=p.offset && ce.offset+ce.size<=p.offset+p.size :
                        ce.offset==p.offset && ce.size==p.size)
                    {
                        p.flags |= CE_DUP;
                        insert = false;
                        break;
                    }
                    else if(ce.offset >= p.offset)
                    {
                        if(ce.offset == p.offset+p.size) ce.flags &= ~CE_START;
                        prev = cur;
                        cur = p.next;
                    }
                    else break;
                }
                if(insert)
                {
                    ce.next = cur;
                    while(cur >= 0)
                    {
                        cubeedge &p = cubeedges[cur];
                        if(ce.offset+ce.size==p.offset) { ce.flags &= ~CE_END; break; }
                        cur = p.next;
                    }
                    if(prev>=0) cubeedges[prev].next = cubeedges.length();
                    else *exists = cubeedges.length();
                }
            }
            else edgegroups[g] = cubeedges.length();

            if(insert) cubeedges.add(ce);
        }
    }
}

void gencubeedges(cube *c = worldroot, const ivec &co = ivec(0, 0, 0), int size = worldsize>>1)
{
    rcprogress("Fixing t-joints...");
    neighbourstack[++neighbourdepth] = c;
    loopi(8)
    {
        ivec o(i, co, size);
        if(c[i].ext) c[i].ext->tjoints = -1;
        if(c[i].children) gencubeedges(c[i].children, o, size>>1);
        else if(!isempty(c[i])) gencubeedges(c[i], o, size);
    }
    --neighbourdepth;
}

void gencubeverts(cube &c, const ivec &co, int size, int csi)
{
    if(!(c.visible&0xC0)) return;

    int vismask = ~c.merged & 0x3F;
    if(!(c.visible&0x80)) vismask &= c.visible;
    if(!vismask) return;

    int tj = filltjoints && c.ext ? c.ext->tjoints : -1, vis;
    loopi(6) if(vismask&(1<<i) && (vis = visibletris(c, i, co, size)))
    {
        vec pos[MAXFACEVERTS];
        vertinfo *verts = NULL;
        int numverts = c.ext ? c.ext->surfaces[i].numverts&MAXFACEVERTS : 0, convex = 0;
        if(numverts)
        {
            verts = c.ext->verts() + c.ext->surfaces[i].verts;
            vec vo(ivec(co).mask(~0xFFF));
            loopj(numverts) pos[j] = vec(verts[j].getxyz()).mul(1.0f/8).add(vo);
            if(!flataxisface(c, i)) convex = faceconvexity(verts, numverts, size);
        }
        else
        {
            ivec v[4];
            genfaceverts(c, i, v);
            if(!flataxisface(c, i)) convex = faceconvexity(v);
            int order = vis&4 || convex < 0 ? 1 : 0;
            vec vo(co);
            pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
            if(vis&1) pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
            pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
            if(vis&2) pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
        }

        VSlot &vslot = lookupvslot(c.texture[i], true),
              *layer = vslot.layer && !(c.material&MAT_ALPHA) ? &lookupvslot(vslot.layer, true) : NULL;
        ushort envmap = vslot.slot->shader->type&SHADER_ENVMAP ? (vslot.slot->texmask&(1<<TEX_ENVMAP) ? EMID_CUSTOM : closestenvmap(i, co, size)) : EMID_NONE,
               envmap2 = layer && layer->slot->shader->type&SHADER_ENVMAP ? (layer->slot->texmask&(1<<TEX_ENVMAP) ? EMID_CUSTOM : closestenvmap(i, co, size)) : EMID_NONE;
        while(tj >= 0 && tjoints[tj].edge < i*(MAXFACEVERTS+1)) tj = tjoints[tj].next;
        int hastj = tj >= 0 && tjoints[tj].edge < (i+1)*(MAXFACEVERTS+1) ? tj : -1;
        int grassy = vslot.slot->grass && i!=O_BOTTOM ? (vis!=3 || convex ? 1 : 2) : 0;
        if(!c.ext)
            addcubeverts(vslot, i, size, pos, convex, c.texture[i], NULL, numverts, hastj, envmap, grassy, (c.material&MAT_ALPHA)!=0);
        else
        {
            const surfaceinfo &surf = c.ext->surfaces[i];
            if(!surf.numverts || surf.numverts&LAYER_TOP)
                addcubeverts(vslot, i, size, pos, convex, c.texture[i], verts, numverts, hastj, envmap, grassy, (c.material&MAT_ALPHA)!=0, surf.numverts&LAYER_BLEND);
            if(surf.numverts&LAYER_BOTTOM)
                addcubeverts(layer ? *layer : vslot, i, size, pos, convex, vslot.layer, verts, numverts, hastj, envmap2, 0, false, surf.numverts&LAYER_TOP ? LAYER_BOTTOM : LAYER_TOP);
        }
    }
}

////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0, gbatches = 0;
vector<vtxarray *> valist, varoot;

vtxarray *newva(const ivec &o, int size)
{
    vtxarray *va = new vtxarray;
    va->parent = NULL;
    va->o = o;
    va->size = size;
    va->curvfc = VFC_NOT_VISIBLE;
    va->occluded = OCCLUDE_NOTHING;
    va->query = NULL;
    va->bbmin = va->alphamin = va->refractmin = va->skymin = ivec(-1, -1, -1);
    va->bbmax = va->alphamax = va->refractmax = va->skymax = ivec(-1, -1, -1);
    va->hasmerges = 0;
    va->mergelevel = -1;

    vc.setupdata(va);

    if(va->alphafronttris || va->alphabacktris || va->refracttris)
    {
        va->alphamin = ivec(vec(vc.alphamin).mul(8)).shr(3);
        va->alphamax = ivec(vec(vc.alphamax).mul(8)).add(7).shr(3);
    }

    if(va->refracttris)
    {
        va->refractmin = ivec(vec(vc.refractmin).mul(8)).shr(3);
        va->refractmax = ivec(vec(vc.refractmax).mul(8)).add(7).shr(3);
    }

    if(va->sky && vc.skymax.x >= 0)
    {
        va->skymin = ivec(vec(vc.skymin).mul(8)).shr(3);
        va->skymax = ivec(vec(vc.skymax).mul(8)).add(7).shr(3);
    }

    va->nogimin = vc.nogimin;
    va->nogimax = vc.nogimax;

    wverts += va->verts;
    wtris  += va->tris + va->blends + va->alphabacktris + va->alphafronttris + va->refracttris + va->decaltris;
    allocva++;
    valist.add(va);

    return va;
}

void destroyva(vtxarray *va, bool reparent)
{
    wverts -= va->verts;
    wtris -= va->tris + va->blends + va->alphabacktris + va->alphafronttris + va->refracttris + va->decaltris;
    allocva--;
    valist.removeobj(va);
    if(!va->parent) varoot.removeobj(va);
    if(reparent)
    {
        if(va->parent) va->parent->children.removeobj(va);
        loopv(va->children)
        {
            vtxarray *child = va->children[i];
            child->parent = va->parent;
            if(child->parent) child->parent->children.add(child);
        }
    }
    if(va->vbuf) destroyvbo(va->vbuf);
    if(va->ebuf) destroyvbo(va->ebuf);
    if(va->skybuf) destroyvbo(va->skybuf);
    if(va->decalbuf) destroyvbo(va->decalbuf);
    if(va->texelems) delete[] va->texelems;
    if(va->decalelems) delete[] va->decalelems;
    if(va->matbuf) delete[] va->matbuf;
    delete va;
}

void clearvas(cube *c)
{
    loopi(8)
    {
        if(c[i].ext)
        {
            if(c[i].ext->va) destroyva(c[i].ext->va, false);
            c[i].ext->va = NULL;
            c[i].ext->tjoints = -1;
        }
        if(c[i].children) clearvas(c[i].children);
    }
}

ivec worldmin(0, 0, 0), worldmax(0, 0, 0), nogimin(0, 0, 0), nogimax(0, 0, 0);

void updatevabb(vtxarray *va, bool force)
{
    if(!force && va->bbmin.x >= 0) return;

    va->bbmin = va->geommin;
    va->bbmax = va->geommax;
    va->bbmin.min(va->lavamin);
    va->bbmax.max(va->lavamax);
    va->bbmin.min(va->watermin);
    va->bbmax.max(va->watermax);
    va->bbmin.min(va->glassmin);
    va->bbmax.max(va->glassmax);
    loopv(va->children)
    {
        vtxarray *child = va->children[i];
        updatevabb(child, force);
        va->bbmin.min(child->bbmin);
        va->bbmax.max(child->bbmax);
    }
    loopv(va->mapmodels)
    {
        octaentities *oe = va->mapmodels[i];
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    loopv(va->decals)
    {
        octaentities *oe = va->decals[i];
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
    va->bbmin.max(va->o);
    va->bbmax.min(ivec(va->o).add(va->size));
    worldmin.min(va->bbmin);
    worldmax.max(va->bbmax);
    nogimin.min(va->nogimin);
    nogimax.max(va->nogimax);
}

void updatevabbs(bool force)
{
    if(force)
    {
        worldmin = nogimin = ivec(worldsize, worldsize, worldsize);
        worldmax = nogimax = ivec(0, 0, 0);
        loopv(varoot) updatevabb(varoot[i], true);
        if(worldmin.x >= worldmax.x)
        {
            worldmin = ivec(0, 0, 0);
            worldmax = ivec(worldsize, worldsize, worldsize);
        }
    }
    else loopv(varoot) updatevabb(varoot[i]);
}

struct mergedface
{
    uchar orient, numverts;
    ushort mat, tex, envmap;
    vertinfo *verts;
    int tjoints;
};

#define MAXMERGELEVEL 12
static int vahasmerges = 0, vamergemax = 0;
static vector<mergedface> vamerges[MAXMERGELEVEL+1];

int genmergedfaces(cube &c, const ivec &co, int size, int minlevel = -1)
{
    if(!c.ext || isempty(c)) return -1;
    int tj = c.ext->tjoints, maxlevel = -1;
    loopi(6) if(c.merged&(1<<i))
    {
        surfaceinfo &surf = c.ext->surfaces[i];
        int numverts = surf.numverts&MAXFACEVERTS;
        if(!numverts)
        {
            if(minlevel < 0) vahasmerges |= MERGE_PART;
            continue;
        }
        mergedface mf;
        mf.orient = i;
        mf.mat = c.material;
        mf.tex = c.texture[i];
        mf.envmap = EMID_NONE;
        mf.numverts = surf.numverts;
        mf.verts = c.ext->verts() + surf.verts;
        mf.tjoints = -1;
        int level = calcmergedsize(i, co, size, mf.verts, mf.numverts&MAXFACEVERTS);
        if(level > minlevel)
        {
            maxlevel = max(maxlevel, level);

            while(tj >= 0 && tjoints[tj].edge < i*(MAXFACEVERTS+1)) tj = tjoints[tj].next;
            if(tj >= 0 && tjoints[tj].edge < (i+1)*(MAXFACEVERTS+1)) mf.tjoints = tj;

            VSlot &vslot = lookupvslot(mf.tex, true),
                  *layer = vslot.layer && !(c.material&MAT_ALPHA) ? &lookupvslot(vslot.layer, true) : NULL;
            if(vslot.slot->shader->type&SHADER_ENVMAP)
                mf.envmap = vslot.slot->texmask&(1<<TEX_ENVMAP) ? EMID_CUSTOM : closestenvmap(i, co, size);
            ushort envmap2 = layer && layer->slot->shader->type&SHADER_ENVMAP ? (layer->slot->texmask&(1<<TEX_ENVMAP) ? EMID_CUSTOM : closestenvmap(i, co, size)) : EMID_NONE;

            if(surf.numverts&LAYER_TOP) vamerges[level].add(mf);
            if(surf.numverts&LAYER_BOTTOM)
            {
                mf.tex = vslot.layer;
                mf.envmap = envmap2;
                mf.numverts &= ~LAYER_BLEND;
                mf.numverts |= surf.numverts&LAYER_TOP ? LAYER_BOTTOM : LAYER_TOP;
                vamerges[level].add(mf);
            }
        }
    }
    if(maxlevel >= 0)
    {
        vamergemax = max(vamergemax, maxlevel);
        vahasmerges |= MERGE_ORIGIN;
    }
    return maxlevel;
}

int findmergedfaces(cube &c, const ivec &co, int size, int csi, int minlevel)
{
    if(c.ext && c.ext->va && !(c.ext->va->hasmerges&MERGE_ORIGIN)) return c.ext->va->mergelevel;
    else if(c.children)
    {
        int maxlevel = -1;
        loopi(8)
        {
            ivec o(i, co, size/2);
            int level = findmergedfaces(c.children[i], o, size/2, csi-1, minlevel);
            maxlevel = max(maxlevel, level);
        }
        return maxlevel;
    }
    else if(c.ext && c.merged) return genmergedfaces(c, co, size, minlevel);
    else return -1;
}

void addmergedverts(int level, const ivec &o)
{
    vector<mergedface> &mfl = vamerges[level];
    if(mfl.empty()) return;
    vec vo(ivec(o).mask(~0xFFF));
    vec pos[MAXFACEVERTS];
    loopv(mfl)
    {
        mergedface &mf = mfl[i];
        int numverts = mf.numverts&MAXFACEVERTS;
        loopi(numverts)
        {
            vertinfo &v = mf.verts[i];
            pos[i] = vec(v.x, v.y, v.z).mul(1.0f/8).add(vo);
        }
        VSlot &vslot = lookupvslot(mf.tex, true);
        int grassy = vslot.slot->grass && mf.orient!=O_BOTTOM && mf.numverts&LAYER_TOP ? 2 : 0;
        addcubeverts(vslot, mf.orient, 1<<level, pos, 0, mf.tex, mf.verts, numverts, mf.tjoints, mf.envmap, grassy, (mf.mat&MAT_ALPHA)!=0, mf.numverts&LAYER_BLEND);
        vahasmerges |= MERGE_USE;
    }
    mfl.setsize(0);
}

static inline void finddecals(vtxarray *va)
{
    if(va->hasmerges&(MERGE_ORIGIN|MERGE_PART))
    {
        loopv(va->decals) vc.extdecals.add(va->decals[i]);
        loopv(va->children) finddecals(va->children[i]);
    }
}

void rendercube(cube &c, const ivec &co, int size, int csi, int &maxlevel) // creates vertices and indices ready to be put into a va
{
    //if(size<=16) return;
    if(c.ext && c.ext->va)
    {
        maxlevel = max(maxlevel, c.ext->va->mergelevel);
        finddecals(c.ext->va);
        return; // don't re-render
    }

    if(c.children)
    {
        neighbourstack[++neighbourdepth] = c.children;
        c.escaped = 0;
        loopi(8)
        {
            ivec o(i, co, size/2);
            int level = -1;
            rendercube(c.children[i], o, size/2, csi-1, level);
            if(level >= csi)
                c.escaped |= 1<<i;
            maxlevel = max(maxlevel, level);
        }
        --neighbourdepth;

        if(csi <= MAXMERGELEVEL && vamerges[csi].length()) addmergedverts(csi, co);

        if(c.ext && c.ext->ents)
        {
            if(c.ext->ents->mapmodels.length()) vc.mapmodels.add(c.ext->ents);
            if(c.ext->ents->decals.length()) vc.decals.add(c.ext->ents);
        }
        return;
    }

    if(!isempty(c))
    {
        gencubeverts(c, co, size, csi);
        if(c.merged) maxlevel = max(maxlevel, genmergedfaces(c, co, size));
    }
    if(c.material != MAT_AIR)
    {
        genmatsurfs(c, co, size, vc.matsurfs);
        if(c.material&MAT_NOGI)
        {
            vc.nogimin.min(co);
            vc.nogimax.max(ivec(co).add(size));
        }
    }

    if(c.ext && c.ext->ents)
    {
        if(c.ext->ents->mapmodels.length()) vc.mapmodels.add(c.ext->ents);
        if(c.ext->ents->decals.length()) vc.decals.add(c.ext->ents);
    }

    if(csi <= MAXMERGELEVEL && vamerges[csi].length()) addmergedverts(csi, co);
}

void calcgeombb(const ivec &co, int size, ivec &bbmin, ivec &bbmax)
{
    vec vmin(co), vmax = vmin;
    vmin.add(size);

    loopv(vc.verts)
    {
        const vec &v = vc.verts[i].pos;
        vmin.min(v);
        vmax.max(v);
    }

    bbmin = ivec(vmin.mul(8)).shr(3);
    bbmax = ivec(vmax.mul(8)).add(7).shr(3);
}

static int entdepth = -1;
static octaentities *entstack[32];

void setva(cube &c, const ivec &co, int size, int csi)
{
    ASSERT(size <= 0x1000);

    int vamergeoffset[MAXMERGELEVEL+1];
    loopi(MAXMERGELEVEL+1) vamergeoffset[i] = vamerges[i].length();

    vc.origin = co;
    vc.size = size;

    loopi(entdepth+1)
    {
        octaentities *oe = entstack[i];
        if(oe->decals.length()) vc.extdecals.add(oe);
    }

    int maxlevel = -1;
    rendercube(c, co, size, csi, maxlevel);

    if(size == min(0x1000, worldsize/2) || !vc.emptyva())
    {
        vtxarray *va = newva(co, size);
        ext(c).va = va;
        calcgeombb(co, size, va->geommin, va->geommax);
        calcmatbb(va, co, size, vc.matsurfs);
        va->hasmerges = vahasmerges;
        va->mergelevel = vamergemax;
    }
    else
    {
        loopi(MAXMERGELEVEL+1) vamerges[i].setsize(vamergeoffset[i]);
    }

    vc.clear();
}

static inline int setcubevisibility(cube &c, const ivec &co, int size)
{
    if(isempty(c) && (c.material&MATF_CLIP) != MAT_CLIP) return 0;
    int numvis = 0, vismask = 0, collidemask = 0, checkmask = 0;
    loopi(6)
    {
        int facemask = classifyface(c, i, co, size);
        if(facemask&1)
        {
            vismask |= 1<<i;
            if(c.merged&(1<<i))
            {
                if(c.ext && c.ext->surfaces[i].numverts&MAXFACEVERTS) numvis++;
            }
            else
            {
                numvis++;
                if(c.texture[i] != DEFAULT_SKY && !(c.ext && c.ext->surfaces[i].numverts&MAXFACEVERTS)) checkmask |= 1<<i;
            }
        }
        if(facemask&2) collidemask |= 1<<i;
    }
    c.visible = collidemask | (vismask ? (vismask != collidemask ? (checkmask ? 0x80|0x40 : 0x80) : 0x40) : 0);
    return numvis;
}

VARF(0, vafacemax, 64, 384, 256*256, allchanged());
VARF(0, vafacemin, 0, 96, 256*256, allchanged());
VARF(0, vacubesize, 32, 128, 0x1000, allchanged());

int updateva(cube *c, const ivec &co, int size, int csi)
{
    rcprogress("Recalculating geometry...");
    int ccount = 0, cmergemax = vamergemax, chasmerges = vahasmerges;
    neighbourstack[++neighbourdepth] = c;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0, childpos = varoot.length();
        ivec o(i, co, size);
        vamergemax = 0;
        vahasmerges = 0;
        if(c[i].ext && c[i].ext->va)
        {
            varoot.add(c[i].ext->va);
            if(c[i].ext->va->hasmerges&MERGE_ORIGIN) findmergedfaces(c[i], o, size, csi, csi);
        }
        else
        {
            if(c[i].children)
            {
                if(c[i].ext && c[i].ext->ents) entstack[++entdepth] = c[i].ext->ents;
                count += updateva(c[i].children, o, size/2, csi-1);
                if(c[i].ext && c[i].ext->ents) --entdepth;
            }
            else count += setcubevisibility(c[i], o, size);
            int tcount = count + (csi <= MAXMERGELEVEL ? vamerges[csi].length() : 0);
            if(tcount > vafacemax || (tcount >= vafacemin && size >= vacubesize) || size == min(0x1000, worldsize/2))
            {
                loadprogress = clamp(recalcprogress/float(allocnodes), 0.0f, 1.0f);
                setva(c[i], o, size, csi);
                if(c[i].ext && c[i].ext->va)
                {
                    while(varoot.length() > childpos)
                    {
                        vtxarray *child = varoot.pop();
                        c[i].ext->va->children.add(child);
                        child->parent = c[i].ext->va;
                    }
                    varoot.add(c[i].ext->va);
                    if(vamergemax > size)
                    {
                        cmergemax = max(cmergemax, vamergemax);
                        chasmerges |= vahasmerges&~MERGE_USE;
                    }
                    continue;
                }
                else count = 0;
            }
        }
        if(csi+1 <= MAXMERGELEVEL && vamerges[csi].length()) vamerges[csi+1].move(vamerges[csi]);
        cmergemax = max(cmergemax, vamergemax);
        chasmerges |= vahasmerges;
        ccount += count;
    }
    --neighbourdepth;
    vamergemax = cmergemax;
    vahasmerges = chasmerges;

    return ccount;
}

void addtjoint(const edgegroup &g, const cubeedge &e, int offset)
{
    int vcoord = (g.slope[g.axis]*offset + g.origin[g.axis]) & 0x7FFF;
    tjoint &tj = tjoints.add();
    tj.offset = vcoord / g.slope[g.axis];
    tj.edge = e.index;

    int prev = -1, cur = ext(*e.c).tjoints;
    while(cur >= 0)
    {
        tjoint &o = tjoints[cur];
        if(tj.edge < o.edge || (tj.edge==o.edge && (e.flags&CE_FLIP ? tj.offset > o.offset : tj.offset < o.offset))) break;
        prev = cur;
        cur = o.next;
    }

    tj.next = cur;
    if(prev < 0) e.c->ext->tjoints = tjoints.length()-1;
    else tjoints[prev].next = tjoints.length()-1;
}

void findtjoints(int cur, const edgegroup &g)
{
    int active = -1;
    while(cur >= 0)
    {
        cubeedge &e = cubeedges[cur];
        int prevactive = -1, curactive = active;
        while(curactive >= 0)
        {
            cubeedge &a = cubeedges[curactive];
            if(a.offset+a.size <= e.offset)
            {
                if(prevactive >= 0) cubeedges[prevactive].next = a.next;
                else active = a.next;
            }
            else
            {
                prevactive = curactive;
                if(!(a.flags&CE_DUP))
                {
                    if(e.flags&CE_START && e.offset > a.offset && e.offset < a.offset+a.size)
                        addtjoint(g, a, e.offset);
                    if(e.flags&CE_END && e.offset+e.size > a.offset && e.offset+e.size < a.offset+a.size)
                        addtjoint(g, a, e.offset+e.size);
                }
                if(!(e.flags&CE_DUP))
                {
                    if(a.flags&CE_START && a.offset > e.offset && a.offset < e.offset+e.size)
                        addtjoint(g, e, a.offset);
                    if(a.flags&CE_END && a.offset+a.size > e.offset && a.offset+a.size < e.offset+e.size)
                        addtjoint(g, e, a.offset+a.size);
                }
            }
            curactive = a.next;
        }
        int next = e.next;
        e.next = active;
        active = cur;
        cur = next;
    }
}

void findtjoints()
{
    recalcprogress = 0;
    gencubeedges();
    tjoints.setsize(0);
    enumeratekt(edgegroups, edgegroup, g, int, e, findtjoints(e, g));
    cubeedges.setsize(0);
    edgegroups.clear();
}

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    int csi = 0;
    while(1<<csi < worldsize) csi++;

    recalcprogress = 0;
    varoot.setsize(0);
    updateva(worldroot, ivec(0, 0, 0), worldsize/2, csi-1);
    loadprogress = 0;
    flushvbo();

    explicitsky = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->sky;
    }

    visibleva = NULL;
}

void precachetextures()
{
    vector<int> texs;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        loopj(va->texs + va->blends)
        {
            int tex = va->texelems[j].texture;
            if(texs.find(tex) < 0)
            {
                texs.add(tex);

                VSlot &vslot = lookupvslot(tex, false);
                if(vslot.layer && texs.find(vslot.layer) < 0) texs.add(vslot.layer);
                if(vslot.detail && texs.find(vslot.detail) < 0) texs.add(vslot.detail);
            }
        }
    }
    loopv(texs)
    {
        loadprogress = float(i+1)/texs.length();
        lookupvslot(texs[i]);
    }
    loadprogress = 0;
}

void allchanged(bool load)
{
    if(!connected()) load = false;
    if(load) initlights();
    progress(-1, "Clearing vertex arrays...");
    clearvas(worldroot);
    resetqueries();
    resetclipplanes();
    if(load) initenvmaps();
    entitiesinoctanodes();
    tjoints.setsize(0);
    if(filltjoints) findtjoints();
    octarender();
    if(load) precachetextures();
    setupmaterials();
    clearshadowcache();
    updatevabbs(true);
    if(load)
    {
        genshadowmeshes();
        updateblendtextures();
        seedparticles();
        genenvmaps();
        drawminimap();
    }
}

void recalc()
{
    allchanged(true);
}

COMMAND(0, recalc, "");

