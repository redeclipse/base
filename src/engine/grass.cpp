#include "engine.h"

VAR(IDF_PERSIST, grass, 0, 1, 1);
VAR(0, dbggrass, 0, 0, 1);
VAR(IDF_PERSIST, grassdist, 0, 256, 10000);
FVAR(IDF_PERSIST, grasstaper, 0, 0.2f, 1);
FVAR(IDF_PERSIST, grassstep, 0.5f, 2, 8);
VAR(IDF_WORLD, grassheight, 1, 4, 64);

struct grasswedge
{
    vec dir, across, edge1, edge2;
    plane bound1, bound2;

    void init(int i, int n)
    {
      dir = vec(2*M_PI*(i+0.5f)/float(n), 0);
      across = vec(2*M_PI*((i+0.5f)/float(n) + 0.25f), 0);
      edge1 = vec(2*M_PI*i/float(n), 0).div(cos(M_PI/n));
      edge2 = vec(2*M_PI*(i+1)/float(n), 0).div(cos(M_PI/n));
      bound1 = plane(vec(2*M_PI*(i/float(n) - 0.25f), 0), 0);
      bound2 = plane(vec(2*M_PI*((i+1)/float(n) + 0.25f), 0), 0);
      across.div(-across.dot(bound1));
    }
};
grasswedge *grasswedges = NULL;
void resetgrasswedges(int n)
{
    DELETEA(grasswedges);
    grasswedges = new grasswedge[n];
    loopi(n) grasswedges[i].init(i, n);
}
VARFN(IDF_PERSIST, grasswedges, numgrasswedges, 8, 8, 1024, resetgrasswedges(numgrasswedges));

struct grassvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
};

static vector<grassvert> grassverts;
static GLuint grassvbo = 0;
static int grassvbosize = 0;

VAR(0, maxgrass, 10, 10000, 10000);

struct grassgroup
{
    const grasstri *tri;
    int tex, offset, numquads, scale, height;
};

static vector<grassgroup> grassgroups;

float *grassoffsets = NULL, *grassanimoffsets = NULL;
void resetgrassoffsets(int n)
{
    DELETEA(grassoffsets);
    DELETEA(grassanimoffsets);
    grassoffsets = new float[n];
    grassanimoffsets = new float[n];
    loopi(n) grassoffsets[i] = rnd(0x1000000)/float(0x1000000);
}
VARFN(IDF_PERSIST, grassoffsets, numgrassoffsets, 8, 32, 1024, resetgrassoffsets(numgrassoffsets));

void initgrass()
{
    if(!grasswedges) resetgrasswedges(numgrasswedges);
    if(!grassoffsets) resetgrassoffsets(numgrassoffsets);
}

static int lastgrassanim = -1;

VAR(IDF_WORLD, grassanimmillis, 0, 3000, 60000);
FVAR(IDF_WORLD, grassanimscale, 0, 0.03f, 1);

static void animategrass()
{
    loopi(numgrassoffsets) grassanimoffsets[i] = grassanimscale*sinf(2*M_PI*(grassoffsets[i] + lastmillis/float(grassanimmillis)));
    lastgrassanim = lastmillis;
}

VAR(IDF_WORLD, grassscale, 1, 2, 64);
bvec grasscolor(255, 255, 255);
VARF(IDF_HEX|IDF_WORLD, grasscolour, 0, 0xFFFFFF, 0xFFFFFF,
{
    if(!grasscolour) grasscolour = 0xFFFFFF;
    grasscolor = bvec((grasscolour>>16)&0xFF, (grasscolour>>8)&0xFF, grasscolour&0xFF);
});
FVAR(IDF_WORLD, grassblend, 0, 1, 1);
FVAR(IDF_WORLD, grasstest, 0, 0.6f, 1);

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
        gs = scale > 0 ? scale : grassscale,
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
    bvec gcol = col.iszero() ? grasscolor : bvec(uchar(col.x*255), uchar(col.y*255), uchar(col.z*255));
    if(blend <= 0) blend = grassblend;
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
        if(leftb > 0)
        {
            if(w.bound1.dist(p2) >= 0) continue;
            p1.add(vec(across).mul(leftb));
        }
        if(rightb > 0)
        {
            if(w.bound2.dist(p1) >= 0) continue;
            p2.sub(vec(across).mul(rightb));
        }

        if(grassverts.length() >= 4*maxgrass) break;

        if(!group)
        {
            group = &grassgroups.add();
            group->tri = &g;
            group->tex = tex->id;
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
        bvec4 color(gcol, uchar(fade*blend*255));

        #define GRASSVERT(n, tcv, modify) { \
            grassvert &gv = grassverts.add(); \
            gv.pos = p##n; \
            gv.color = color; \
            gv.tc = vec2(tc##n, tcv); \
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
            if(w.bound1.dist(g.center) > g.radius || w.bound2.dist(g.center) > g.radius) continue;
            gengrassquads(group, w, g, s.grasstex, s.grasscolor, s.grassblend, s.grassscale, s.grassheight);
        }
    }
}

void generategrass()
{
    if(!grass || !grassdist) return;

    grassgroups.setsize(0);
    grassverts.setsize(0);

    resetgrasswedges(numgrasswedges);
    resetgrassoffsets(numgrassoffsets);

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
}

void cleargrassshaders()
{
    grassshader = NULL;
}

void rendergrass()
{
    if(!grass || !grassdist || grassgroups.empty() || dbggrass || !grassshader) return;

    glDisable(GL_CULL_FACE);

    gle::bindvbo(grassvbo);

    const grassvert *ptr = 0;
    gle::vertexpointer(sizeof(grassvert), ptr->pos.v);
    gle::colorpointer(sizeof(grassvert), ptr->color.v);
    gle::texcoord0pointer(sizeof(grassvert), ptr->tc.v);
    gle::enablevertex();
    gle::enablecolor();
    gle::enabletexcoord0();
    gle::enablequads();

    GLOBALPARAMF(grasstest, grasstest);

    int texid = -1, blend = -1;
    loopv(grassgroups)
    {
        grassgroup &g = grassgroups[i];

        if(texid != g.tex)
        {
            glBindTexture(GL_TEXTURE_2D, g.tex);
            texid = g.tex;
        }

        if(blend != g.tri->blend)
        {
            if(g.tri->blend)
            {
                glActiveTexture_(GL_TEXTURE1);
                bindblendtexture(ivec(g.tri->center));
                glActiveTexture_(GL_TEXTURE0);
                grassshader->setvariant(0, 0);
            }
            else grassshader->set();
            blend = g.tri->blend;
        }

        gle::drawquads(g.offset, g.numquads);
        xtravertsva += 4*g.numquads;
    }

    gle::disablequads();
    gle::disablevertex();
    gle::disablecolor();
    gle::disabletexcoord0();

    gle::clearvbo();

    glEnable(GL_CULL_FACE);
}

void cleanupgrass()
{
    if(grassvbo) { glDeleteBuffers_(1, &grassvbo); grassvbo = 0; }
    grassvbosize = 0;

    cleargrassshaders();
}
