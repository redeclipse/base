#include "engine.h"

VAR(IDF_PERSIST, grass, 0, 1, 1);
VAR(0, dbggrass, 0, 0, 1);
VAR(IDF_PERSIST, grassdist, 0, 1024, 10000);
FVAR(IDF_PERSIST, grasstaper, 0, 0.2f, 1);
FVAR(IDF_PERSIST, grassstep, 0.5f, 3, 8);
VAR(IDF_MAP, grassheight, 1, 4, 64);
VAR(IDF_PERSIST, grassmargin, 0, 8, 32);
FVAR(0, grassmarginfade, 0, 1, 1);

struct grasswedge
{
    vec dir, across, edge1, edge2;
    plane bound1, bound2;
    bvec4 vertbounds;

    grasswedge(int i, int n) :
      dir(2*M_PI*(i+0.5f)/float(n), 0),
      across(2*M_PI*((i+0.5f)/float(n) + 0.25f), 0),
      edge1(vec(2*M_PI*i/float(n), 0).div(cos(M_PI/n))),
      edge2(vec(2*M_PI*(i+1)/float(n), 0).div(cos(M_PI/n))),
      bound1(vec(2*M_PI*(i/float(n) - 0.25f), 0), 0),
      bound2(vec(2*M_PI*((i+1)/float(n) + 0.25f), 0), 0)
    {
      across.div(-across.dot(bound1));

      bvec vertbound1(bound1), vertbound2(bound2);
      vertbounds = bvec4(vertbound1.x, vertbound1.y, vertbound2.x, vertbound2.y);
      vertbounds.flip();
    }
};
static vector<grasswedge> grasswedges;
void resetgrasswedges(int n)
{
    grasswedges.setsize(0);
    grasswedges.reserve(n);
    loopi(n) grasswedges.add(grasswedge(i, n));
}
VARFN(IDF_PERSIST, grasswedges, numgrasswedges, 8, 8, 1024, resetgrasswedges(numgrasswedges));

struct grassvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
    bvec4 bounds;
};

static vector<grassvert> grassverts;
static GLuint grassvbo = 0;
static int grassvbosize = 0;

VAR(0, maxgrass, 10, 10000, 10000);

struct grassgroup
{
    const grasstri *tri;
    Texture *tex;
    int offset, numquads, scale, height;
};

static vector<grassgroup> grassgroups;

vector<float> grassoffsets, grassanimoffsets;
void resetgrassoffsets(int n)
{
    grassoffsets.setsize(0);
    grassoffsets.reserve(n);
    loopi(n) grassoffsets.add(rnd(0x1000000)/float(0x1000000));
    grassanimoffsets.setsize(0);
    grassanimoffsets.pad(n);
}
VARFN(IDF_PERSIST, grassoffsets, numgrassoffsets, 8, 32, 1024, resetgrassoffsets(numgrassoffsets));

static int lastgrassanim = -1;

VAR(IDF_MAP, grassanimmillis, 0, 3000, 60000);
FVAR(IDF_MAP, grassanimscale, 0, 0.03f, 1);

static void animategrass()
{
    loopi(numgrassoffsets) grassanimoffsets[i] = grassanimscale*sinf(2*M_PI*(grassoffsets[i] + lastmillis/float(grassanimmillis)));
    lastgrassanim = lastmillis;
}

FVAR(IDF_MAP, grassdensity, 0, GRASSDENSITY, 16);
FVAR(IDF_MAP, grasswidth, 0, GRASSWIDTH, 8);
FVAR(IDF_MAP, grassspec, 0, 0, 1);
FVAR(IDF_MAP, grassnormalblend, 0, 0.75f, 1); // how far blade normals lean towards the ground's, so blades catch light from above
VAR(IDF_MAP, grasscolourmode, 0, 0, 2); // 0 = the texture's colour from root to tip, 1 = its average colour, 2 = root to tip with its detail

// imports the old texture repeat, where a higher scale drew more and thinner blades
void importgrassscale(float scale)
{
    if(scale <= 0) return;
    setfvar("grassdensity", clamp(GRASSDENSITY*scale/2, 0.f, 16.f), true);
    setfvar("grasswidth", GRASSWIDTH*2/scale, true);
}
PCVAR(IDF_MAP, grasscolour, 0xFFFFFF);
FVAR(IDF_MAP, grassblend, 0, 1, 1);
FVAR(IDF_MAP, grasstest, 0, 0.6f, 1);

static void gengrassquads(grassgroup *&group, const grasswedge &w, const grasstri &g, Texture *tex, const vec &col, float blend, int scale, int height)
{
    float t = camera1->o.dot(w.dir);
    int tstep = int(ceil(t/grassstep));
    float tstart = tstep*grassstep,
          t0 = w.dir.dot(g.v[0]), t1 = w.dir.dot(g.v[1]), t2 = w.dir.dot(g.v[2]), t3 = w.dir.dot(g.v[3]),
          tmin = min(min(t0, t1), min(t2, t3)),
          tmax = max(max(t0, t1), max(t2, t3));
    if(tmax < tstart || tmin > t + grassdist) return;

    int minstep = max(int(ceil(tmin/grassstep)) - tstep, 1),
        maxstep = int(floor(min(tmax, t + grassdist)/grassstep)) - tstep,
        numsteps = maxstep - minstep + 1,
        gs = scale > 0 ? scale : 2,
        gh = height > 0 ? height : grassheight;

    float texscale = (gs*tex->ys)/float(gh*tex->xs), animscale = gh*texscale;
    vec tc;
    tc.cross(g.surface, w.dir).mul(texscale);

    int offset = tstep + maxstep;
    if(offset < 0) offset = numgrassoffsets - (-offset)%numgrassoffsets;
    offset += numsteps + numgrassoffsets - numsteps%numgrassoffsets;

    float leftdist = t0;
    const vec *leftv = &g.v[0];
    if(t1 > leftdist) { leftv = &g.v[1]; leftdist = t1; }
    if(t2 > leftdist) { leftv = &g.v[2]; leftdist = t2; }
    if(t3 > leftdist) { leftv = &g.v[3]; leftdist = t3; }
    float rightdist = leftdist;
    const vec *rightv = leftv;

    vec across(w.across.x, w.across.y, g.surface.zdelta(w.across)), leftdir(0, 0, 0), rightdir(0, 0, 0), leftp = *leftv, rightp = *rightv;
    float taperdist = grassdist*grasstaper,
          taperscale = 1.0f / (grassdist - taperdist),
          dist = maxstep*grassstep + tstart,
          leftb = 0, rightb = 0, leftdb = 0, rightdb = 0;
    for(int i = maxstep; i >= minstep; i--, offset--, leftp.add(leftdir), rightp.add(rightdir), leftb += leftdb, rightb += rightdb, dist -= grassstep)
    {
        if(dist <= leftdist)
        {
            const vec *prev = leftv;
            float prevdist = leftdist;
            if(--leftv < g.v) leftv += g.numv;
            leftdist = leftv->dot(w.dir);
            if(dist <= leftdist)
            {
                prev = leftv;
                prevdist = leftdist;
                if(--leftv < g.v) leftv += g.numv;
                leftdist = leftv->dot(w.dir);
            }
            leftdir = vec(*leftv).sub(*prev);
            leftdir.mul(grassstep/-w.dir.dot(leftdir));
            leftp = vec(leftdir).mul((prevdist - dist)/grassstep).add(*prev);
            leftb = w.bound1.dist(leftp);
            leftdb = w.bound1.dot(leftdir);
        }
        if(dist <= rightdist)
        {
            const vec *prev = rightv;
            float prevdist = rightdist;
            if(++rightv >= &g.v[g.numv]) rightv = g.v;
            rightdist = rightv->dot(w.dir);
            if(dist <= rightdist)
            {
                prev = rightv;
                prevdist = rightdist;
                if(++rightv >= &g.v[g.numv]) rightv = g.v;
                rightdist = rightv->dot(w.dir);
            }
            rightdir = vec(*rightv).sub(*prev);
            rightdir.mul(grassstep/-w.dir.dot(rightdir));
            rightp = vec(rightdir).mul((prevdist - dist)/grassstep).add(*prev);
            rightb = w.bound2.dist(rightp);
            rightdb = w.bound2.dot(rightdir);
        }
        vec p1 = leftp, p2 = rightp;
        if(leftb > grassmargin)
        {
            if(w.bound1.dist(p2) >= grassmargin) continue;
            p1.add(vec(across).mul(leftb - grassmargin));
        }
        if(rightb > grassmargin)
        {
            if(w.bound2.dist(p1) >= grassmargin) continue;
            p2.sub(vec(across).mul(rightb - grassmargin));
        }

        if(grassverts.length() >= 4*maxgrass) break;

        if(!group)
        {
            group = &grassgroups.add();
            group->tri = &g;
            group->tex = tex;
            group->offset = grassverts.length()/4;
            group->numquads = 0;
            group->scale = gs;
            group->height = gh;
            if(lastgrassanim!=lastmillis) animategrass();
        }

        group->numquads++;

        float tcoffset = grassoffsets[offset%numgrassoffsets],
              animoffset = animscale*grassanimoffsets[offset%numgrassoffsets],
              tc1 = tc.dot(p1) + tcoffset, tc2 = tc.dot(p2) + tcoffset,
              fade = dist - t > taperdist ? (grassdist - (dist - t))*taperscale : 1,
              height = gh * fade;
        bvec gcol = col.iszero() ? bvec::fromcolor(getpulsehexcol(grasscolour)) : bvec(uchar(col.x*255), uchar(col.y*255), uchar(col.z*255));
        if(blend <= 0) blend = grassblend;
        bvec4 color(gcol, uchar(fade*blend*255));

        #define GRASSVERT(n, tcv, modify) { \
            grassvert &gv = grassverts.add(); \
            gv.pos = p##n; \
            gv.color = color; \
            gv.tc = vec2(tc##n, tcv); \
            gv.bounds = w.vertbounds; \
            modify; \
        }

        GRASSVERT(2, 0, { gv.pos.z += height; gv.tc.x += animoffset; });
        GRASSVERT(1, 0, { gv.pos.z += height; gv.tc.x += animoffset; });
        GRASSVERT(1, 1, );
        GRASSVERT(2, 1, );
    }
}

static void gengrassquads(vtxarray *va)
{
    loopv(va->grasstris)
    {
        grasstri &g = va->grasstris[i];
        if(isfoggedsphere(g.radius, g.center)) continue;
        float dist = g.center.dist(camera1->o);
        if(dist - g.radius > grassdist) continue;

        Slot &s = *lookupvslot(g.texture, false).slot;
        if(!s.grasstex)
        {
            if(!s.grass) continue;
            s.grasstex = textureload(s.grass, 2);
        }

        grassgroup *group = NULL;
        loopi(numgrasswedges)
        {
            grasswedge &w = grasswedges[i];
            if(w.bound1.dist(g.center) > g.radius + grassmargin || w.bound2.dist(g.center) > g.radius + grassmargin) continue;
            gengrassquads(group, w, g, s.grasstex, s.grasscolor, s.grassblend, 0, s.grassheight);
        }
    }
}

VAR(0, grassblades, 0, 1, 1); // TEMP: 0 draws the old rows for A/B comparison

void generategrass()
{
    if(!grass || !grassdist || grassblades) return;

    grassgroups.setsize(0);
    grassverts.setsize(0);

    if(grasswedges.empty()) resetgrasswedges(numgrasswedges);
    if(grassoffsets.empty()) resetgrassoffsets(numgrassoffsets);

    loopi(numgrasswedges)
    {
        grasswedge &w = grasswedges[i];
        w.bound1.offset = -camera1->o.dot(w.bound1);
        w.bound2.offset = -camera1->o.dot(w.bound2);
    }

    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->grasstris.empty() || va->occluded >= OCCLUDE_GEOM) continue;
        if(va->distance > grassdist) continue;
        gengrassquads(va);
    }

    if(grassgroups.empty()) return;

    if(!grassvbo) glGenBuffers_(1, &grassvbo);
    gle::bindvbo(grassvbo);
    int size = grassverts.length()*sizeof(grassvert);
    grassvbosize = max(grassvbosize, size);
    glBufferData_(GL_ARRAY_BUFFER, grassvbosize, size == grassvbosize ? grassverts.getbuf() : NULL, GL_STREAM_DRAW);
    if(size != grassvbosize) glBufferSubData_(GL_ARRAY_BUFFER, 0, size, grassverts.getbuf());
    gle::clearvbo();
}

static Shader *grassshader = NULL;

static Shader *bladeshader = NULL;

Shader *loadgrassshader()
{
    string opts;
    int optslen = 0;

    opts[optslen] = '\0';

    defformatstring(name, "grass%s", opts);
    return generateshader(name, "grassshader \"%s\"", opts);

}

void loadgrassshaders()
{
    grassshader = loadgrassshader();
    bladeshader = generateshader("blade", "bladeshader");
}

void cleargrassshaders()
{
    grassshader = NULL;
    bladeshader = NULL;
}

static void renderblades();

void rendergrass()
{
    if(grassblades) { renderblades(); return; }
    if(!grass || !grassdist || grassgroups.empty() || dbggrass || !grassshader) return;

    glDisable(GL_CULL_FACE);

    gle::bindvbo(grassvbo);

    const grassvert *ptr = 0;
    gle::vertexpointer(sizeof(grassvert), ptr->pos.v);
    gle::colorpointer(sizeof(grassvert), ptr->color.v);
    gle::texcoord0pointer(sizeof(grassvert), ptr->tc.v);
    gle::tangentpointer(sizeof(grassvert), ptr->bounds.v, GL_BYTE);
    gle::enablevertex();
    gle::enablecolor();
    gle::enabletexcoord0();
    gle::enabletangent();
    gle::enablequads();

    GLOBALPARAMF(grasstest, grasstest);
    GLOBALPARAMF(grassmargin, grassmargin, grassmargin ? grassmarginfade / grassmargin : 0.0f, grassmargin ? grassmarginfade : 1.0f);

    Texture *tex = NULL;
    int blend = -1;
    loopv(grassgroups)
    {
        grassgroup &g = grassgroups[i];

        if(tex != g.tex)
        {
            settexture(g.tex);
            tex = g.tex;
        }

        if(blend != g.tri->blend)
        {
            if(g.tri->blend)
            {
                glActiveTexture_(GL_TEXTURE1);
                bindblendtexture();
                glActiveTexture_(GL_TEXTURE0);
                grassshader->setvariant(0, 0);
            }
            else grassshader->set();
            blend = g.tri->blend;
        }
        if(blend)
        {
            // blades read the blendmap where they meet the surface they grow from
            GLOBALPARAMF(grassplane, g.tri->surface.x, g.tri->surface.y, g.tri->surface.z, g.tri->surface.offset);
            GLOBALPARAMF(grassorient, g.tri->orient);
            Shader::lastshader->flushparams();
        }

        gle::drawquads(g.offset, g.numquads);
        xtravertsva += 4*g.numquads;
    }

    gle::disablequads();
    gle::disablevertex();
    gle::disablecolor();
    gle::disabletexcoord0();
    gle::disabletangent();

    gle::clearvbo();

    glEnable(GL_CULL_FACE);
}

static void cleanupblades();

void cleanupgrass()
{
    if(grassvbo) { glDeleteBuffers_(1, &grassvbo); grassvbo = 0; }
    grassvbosize = 0;
    cleanupblades();

    cleargrassshaders();
}


#define GRASSCHUNKPATCHES 256 // patches per uniform block, four vec4s each fill the guaranteed 16KB
#define GRASSCHUNKSIZE (GRASSCHUNKPATCHES*4*sizeof(vec4))
#define GRASSCHUNKAREA 512 // chunks gather the patches of a cell this wide, so each can be culled and thinned with distance on its own
#define GRASSBLADEVERTS 7
#define GRASSBLADEINDICES 15
#define GRASSBLADELODINDICES 9 // a far blade drops its lowest bend
#define GRASSUBOBINDING 0

#define GRASSCLUMPSIZE 12 // blades in a clump share their height, facing, lean and tint

FVAR(IDF_PERSIST, grassthin, 0, 0.15f, 1); // share of grassdist where blades start to thin out
FVAR(IDF_PERSIST, grassthinmin, 0.0625f, 0.0625f, 1); // share of blades left at grassdist, widened to cover the same ground
FVAR(IDF_PERSIST, grasslodmesh, 0, 0.4f, 1); // share of grassdist beyond which blades use fewer triangles

static GLuint bladevbo = 0, bladeebo = 0;
static int grassuboalign = 0;

static void setupblades()
{
    if(bladevbo) return;
    // three pairs of points up the blade, then the tip: x is the distance up the blade, y the side
    static const float verts[GRASSBLADEVERTS][2] = { { 0, -1 }, { 0, 1 }, { 1/3.0f, -1 }, { 1/3.0f, 1 }, { 2/3.0f, -1 }, { 2/3.0f, 1 }, { 1, 0 } };
    static const ushort indices[GRASSBLADEINDICES + GRASSBLADELODINDICES] = { 0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4, 4, 5, 6, 0, 1, 5, 0, 5, 4, 4, 5, 6 };
    glGenBuffers_(1, &bladevbo);
    gle::bindvbo(bladevbo);
    glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gle::clearvbo();
    glGenBuffers_(1, &bladeebo);
    gle::bindebo(bladeebo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    gle::clearebo();
    GLint align = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    grassuboalign = max(int(align), int(sizeof(vec4)));
}

#define GRASSCOLORBANDS 8

// a grass texture's colour from tip to root, averaging the opaque texels of each band of rows, as its see-through texels can hold any colour
struct grasstexcolors
{
    vec bands[GRASSCOLORBANDS], average;
};
static hashtable<GLuint, grasstexcolors> grasstexgradients;

static const grasstexcolors &grassgradient(Texture *t)
{
    grasstexcolors *cached = grasstexgradients.access(t->id);
    if(cached) return *cached;
    grasstexcolors &g = grasstexgradients[t->id];
    loopi(GRASSCOLORBANDS) g.bands[i] = vec(1, 1, 1);
    g.average = vec(1, 1, 1);
    GLint w = 0, h = 0;
    glBindTexture(GL_TEXTURE_2D, t->id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    if(w <= 0 || h < GRASSCOLORBANDS) return g;
    uchar *pixels = new uchar[w*h*4];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    double sum[GRASSCOLORBANDS][3], total[3] = { 0, 0, 0 };
    int opaque[GRASSCOLORBANDS], totalopaque = 0;
    loopi(GRASSCOLORBANDS) { sum[i][0] = sum[i][1] = sum[i][2] = 0; opaque[i] = 0; }
    loopi(h)
    {
        int band = i*GRASSCOLORBANDS/h;
        loopj(w)
        {
            const uchar *p = &pixels[(i*w + j)*4];
            if(p[3] < 128) continue;
            loopk(3) { sum[band][k] += p[k]; total[k] += p[k]; }
            opaque[band]++;
            totalopaque++;
        }
    }
    delete[] pixels;
    if(totalopaque) g.average = vec(total[0], total[1], total[2]).div(totalopaque*255.0f);
    loopi(GRASSCOLORBANDS) g.bands[i] = opaque[i] ? vec(sum[i][0], sum[i][1], sum[i][2]).div(opaque[i]*255.0f) : g.average;
    return g;
}

static void cleanupblades()
{
    grasstexgradients.clear();
    if(bladevbo) { glDeleteBuffers_(1, &bladevbo); bladevbo = 0; }
    if(bladeebo) { glDeleteBuffers_(1, &bladeebo); bladeebo = 0; }
    loopv(valist) cleargrassva(valist[i]);
}

void cleargrassva(vtxarray *va)
{
    if(va->grassbuf) { glDeleteBuffers_(1, &va->grassbuf); va->grassbuf = 0; }
    va->grasschunks.setsize(0);
}

static float grasspatchdensity(Slot &s)
{
    float density = s.grassdensity > 0 ? s.grassdensity : grassdensity, blend = s.grassblend > 0 ? s.grassblend : grassblend;
    return density*blend;
}

static float grasspatcharea(const grasstri &g, int first)
{
    return vec().cross(vec(g.v[first+1]).sub(g.v[0]), vec(g.v[first+2]).sub(g.v[0])).magnitude()/2;
}

static bool grasschunkstale(vtxarray *va)
{
    if(!va->grassbuf) return true;
    loopv(va->grasschunks)
    {
        grasschunk &c = va->grasschunks[i];
        if(c.density != grasspatchdensity(*lookupvslot(c.texture, false).slot)) return true;
    }
    return false;
}

static inline int grasspatchcell(const grasstri *g)
{
    return (int(floorf(g->center.y/GRASSCHUNKAREA))<<16) + int(floorf(g->center.x/GRASSCHUNKAREA));
}

// grass settings belong to the texture, so the many variants of one texture share chunks
static inline int grasspatchslot(const grasstri *g)
{
    return lookupvslot(g->texture, false).slot->index;
}

static int sortgrasspatches(const grasstri *a, const grasstri *b)
{
    if(a->blend != b->blend) return a->blend < b->blend ? -1 : 1;
    int aslot = grasspatchslot(a), bslot = grasspatchslot(b);
    if(aslot != bslot) return aslot < bslot ? -1 : 1;
    int acell = grasspatchcell(a), bcell = grasspatchcell(b);
    if(acell != bcell) return acell < bcell ? -1 : 1;
    return 0;
}

// packs each patch as four vec4s: the corners, with the first blade, the share of the first triangle, the blend orient and the corner count
static void genbladesva(vtxarray *va)
{
    cleargrassva(va);
    static vector<const grasstri *> patches;
    patches.setsize(0);
    loopv(va->grasstris) patches.add(&va->grasstris[i]);
    patches.sort(sortgrasspatches);

    static vector<vec4> data;
    data.setsize(0);
    grasschunk *c = NULL;
    int cell = 0;
    vec cmin(0, 0, 0), cmax(0, 0, 0);
    // blades stand up to the tallest grass above their patches
    #define ENDCHUNK if(c) { cmax.z += 64; c->bbmin = cmin; c->bbmax = cmax; c->center = vec(cmin).add(cmax).mul(0.5f); c->radius = cmax.dist(cmin)/2; }
    loopv(patches)
    {
        const grasstri &g = *patches[i];
        Slot &s = *lookupvslot(g.texture, false).slot;
        if(!s.grass) continue;
        float density = grasspatchdensity(s), area1 = grasspatcharea(g, 0), area2 = g.numv > 3 ? grasspatcharea(g, 1) : 0;
        int blades = int((area1 + area2)*density + 0.5f);
        if(blades <= 0) continue;
        if(!c || &s != lookupvslot(c->texture, false).slot || c->blend != g.blend || c->numpatches >= GRASSCHUNKPATCHES || grasspatchcell(&g) != cell)
        {
            ENDCHUNK;
            cell = grasspatchcell(&g);
            cmin = vec(1e16f, 1e16f, 1e16f);
            cmax = vec(-1e16f, -1e16f, -1e16f);
            while((data.length()*sizeof(vec4))%grassuboalign) data.add(vec4(0, 0, 0, 0));
            c = &va->grasschunks.add();
            c->texture = g.texture;
            c->blend = g.blend;
            c->offset = data.length()*sizeof(vec4);
            c->numpatches = c->numblades = 0;
            c->density = density;
        }
        data.add(vec4(g.v[0], c->numblades));
        data.add(vec4(g.v[1], area2 > 0 ? area1/(area1 + area2) : 1));
        data.add(vec4(g.v[2], g.orient));
        data.add(vec4(g.v[3], g.numv));
        c->numpatches++;
        c->numblades += blades;
        loopk(g.numv) { cmin.min(g.v[k]); cmax.max(g.v[k]); }
    }
    ENDCHUNK;
    #undef ENDCHUNK
    // every chunk binds a full block, so the buffer must reach past the last one
    if(c) data.pad(c->offset/sizeof(vec4) + GRASSCHUNKPATCHES*4 - data.length());
    if(data.empty()) return;

    glGenBuffers_(1, &va->grassbuf);
    glBindBuffer_(GL_UNIFORM_BUFFER, va->grassbuf);
    glBufferData_(GL_UNIFORM_BUFFER, data.length()*sizeof(vec4), data.getbuf(), GL_STATIC_DRAW);
    glBindBuffer_(GL_UNIFORM_BUFFER, 0);
}

#define GRASSPIXELSFULL 16 // blades this many pixels tall are drawn at full density
#define GRASSPIXELSTHIN 2 // and thin out to grassthinmin by this height, vanishing below a pixel

static inline float grasssmoothstep(float lo, float hi, float x)
{
    float t = clamp((x - lo)/(hi - lo), 0.0f, 1.0f);
    return t*t*(3 - 2*t);
}

// the share of blades kept at a distance, by how far away and how tall on screen they are, matching the shader
static float grasskeep(float dist, float height, float pixelsize)
{
    float pixels = height/max(dist*pixelsize, 1e-6f);
    if(pixels < 1) return 0;
    float keep = 1 + (grassthinmin - 1)*grasssmoothstep(grassdist*grassthin, grassdist, dist);
    return min(keep, grassthinmin + (1 - grassthinmin)*grasssmoothstep(GRASSPIXELSTHIN, GRASSPIXELSFULL, pixels));
}

static inline float grasspixelsize() { return 2*tanf(fovy/2*RAD)/max(viewh, 1); }

static void setbladeshader(bool blend)
{
    if(blend) bladeshader->setvariant(0, 0);
    else bladeshader->set();
    GLuint block = glGetUniformBlockIndex_(Shader::lastshader->program, "grasspatches");
    if(block != GL_INVALID_INDEX) glUniformBlockBinding_(Shader::lastshader->program, block, GRASSUBOBINDING);
    LOCALPARAMF(grassdist, grassdist);
    LOCALPARAMF(grassclump, 1.0f/GRASSCLUMPSIZE);
    // the world size of a pixel at a distance of one unit, for the narrowest blade and the thinning by height on screen
    LOCALPARAMF(bladethin, grassdist*grassthin, grassthinmin, grasspixelsize());
    LOCALPARAMF(bladepixels, GRASSPIXELSTHIN, GRASSPIXELSFULL);
}

static void renderblades()
{
    if(!grass || !grassdist || dbggrass || !bladeshader) return;

    setupblades();

    glDisable(GL_CULL_FACE);

    float winddist = getwindanimdist();

    gle::bindvbo(bladevbo);
    gle::bindebo(bladeebo);
    gle::vertexpointer(2*sizeof(float), NULL, GL_FLOAT, 2);
    gle::enablevertex();

    Texture *tex = NULL;
    vec gradient[GRASSCOLORBANDS];
    int blend = -1, lastcolormode = -1;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->grasstris.empty() || va->occluded >= OCCLUDE_GEOM || va->distance > grassdist) continue;
        if(grasschunkstale(va)) genbladesva(va);
        loopvj(va->grasschunks)
        {
            grasschunk &c = va->grasschunks[j];
            float dist = camera1->o.dist_to_bb(c.bbmin, c.bbmax);
            if(dist > grassdist || isvisiblesphere(c.radius, c.center) == VFC_NOT_VISIBLE) continue;
            Slot &cs = *lookupvslot(c.texture, false).slot;
            // a far chunk skips the blades that are thinned out everywhere in it, keeping every second, fourth, eighth or sixteenth
            float keep = grasskeep(dist, cs.grassheight > 0 ? cs.grassheight : grassheight, grasspixelsize());
            if(keep <= 0) continue;
            int stride = 1;
            while(stride < 16 && keep <= 0.5f/stride) stride *= 2;
            bool lodmesh = dist > grassdist*grasslodmesh;
            Slot &s = *lookupvslot(c.texture, false).slot;
            if(!s.grasstex) s.grasstex = textureload(s.grass, 2);
            int colormode = s.grasscolormode >= 0 ? s.grasscolormode : grasscolourmode;
            if(tex != s.grasstex || colormode != lastcolormode)
            {
                const grasstexcolors &g = grassgradient(s.grasstex);
                loopk(GRASSCOLORBANDS) gradient[k] = colormode == 1 ? g.average : g.bands[k];
                if(tex != s.grasstex) settexture(s.grasstex);
                tex = s.grasstex;
                lastcolormode = colormode;
            }
            if(blend != c.blend)
            {
                if(c.blend)
                {
                    glActiveTexture_(GL_TEXTURE1);
                    bindblendtexture();
                    glActiveTexture_(GL_TEXTURE0);
                }
                setbladeshader(c.blend != 0);
                blend = c.blend;
            }
            vec col = s.grasscolor.iszero() ? vec::fromcolor(getpulsehexcol(grasscolour)) : s.grasscolor;
            LOCALPARAMF(bladecolor, col.x, col.y, col.z);
            LOCALPARAMV(bladegradient, gradient, GRASSCOLORBANDS);
            LOCALPARAMF(bladeparams, s.grassheight > 0 ? s.grassheight : grassheight, s.grasswidth > 0 ? s.grasswidth : grasswidth, c.numpatches, stride);
            LOCALPARAMF(bladeshading, s.grassspec >= 0 ? s.grassspec : grassspec, s.grassnormalblend >= 0 ? s.grassnormalblend : grassnormalblend, colormode == 2 ? 1 : 0);
            // the wind at the chunk, fading out beyond where models stop swaying
            float windfade = windanimfalloff ? 1 - clamp((dist - winddist)/windanimfalloff, 0.0f, 1.0f) : (dist <= winddist ? 1.0f : 0.0f);
            vec wind = windfade > 0 ? getwind(c.center).mul(windfade*grassanimscale) : vec(0, 0, 0);
            LOCALPARAMF(bladewind, wind.x, wind.y, wind.z, 2*M_PI/max(grassanimmillis, 1));
            glBindBufferRange_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, va->grassbuf, c.offset, GRASSCHUNKSIZE);
            int instances = (c.numblades + stride - 1)/stride;
            if(lodmesh) glDrawElementsInstanced_(GL_TRIANGLES, GRASSBLADELODINDICES, GL_UNSIGNED_SHORT, (const void *)(GRASSBLADEINDICES*sizeof(ushort)), instances);
            else glDrawElementsInstanced_(GL_TRIANGLES, GRASSBLADEINDICES, GL_UNSIGNED_SHORT, NULL, instances);
            xtravertsva += (lodmesh ? 5 : GRASSBLADEVERTS)*instances;
        }
    }

    glBindBufferBase_(GL_UNIFORM_BUFFER, GRASSUBOBINDING, 0);
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();

    glEnable(GL_CULL_FACE);
}

