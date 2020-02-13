// renderva.cpp: handles the occlusion and rendering of vertex arrays

#include "engine.h"

static inline void drawtris(GLsizei numindices, const GLvoid *indices, ushort minvert, ushort maxvert)
{
    glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, numindices, GL_UNSIGNED_SHORT, indices);
    glde++;
}

static inline void drawvatris(vtxarray *va, GLsizei numindices, int offset)
{
    drawtris(numindices, (ushort *)0 + va->eoffset + offset, va->minvert, va->maxvert);
}

static inline void drawvaskytris(vtxarray *va)
{
    drawtris(va->sky, (ushort *)0 + va->skyoffset, va->minvert, va->maxvert);
}

///////// view frustrum culling ///////////////////////

plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
float vfcDfog;  // far plane culling distance (fog limit).
float vfcDnear[5], vfcDfar[5];

vtxarray *visibleva = NULL;

bool isfoggedsphere(float rad, const vec &cv)
{
    loopi(4) if(vfcP[i].dist(cv) < -rad) return true;
    float dist = vfcP[4].dist(cv);
    return dist < -rad || dist > vfcDfog + rad;
}

int isvisiblesphere(float rad, const vec &cv)
{
    int v = VFC_FULL_VISIBLE;
    float dist;

    loopi(5)
    {
        dist = vfcP[i].dist(cv);
        if(dist < -rad) return VFC_NOT_VISIBLE;
        if(dist < rad) v = VFC_PART_VISIBLE;
    }

    dist -= vfcDfog;
    if(dist > rad) return VFC_FOGGED;  //VFC_NOT_VISIBLE;    // culling when fog is closer than size of world results in HOM
    if(dist > -rad) v = VFC_PART_VISIBLE;

    return v;
}

static inline int ishiddencube(const ivec &o, int size)
{
    loopi(5) if(o.dist(vfcP[i]) < -vfcDfar[i]*size) return true;
    return false;
}

static inline int isfoggedcube(const ivec &o, int size)
{
    loopi(4) if(o.dist(vfcP[i]) < -vfcDfar[i]*size) return true;
    float dist = o.dist(vfcP[4]);
    return dist < -vfcDfar[4]*size || dist > vfcDfog - vfcDnear[4]*size;
}

int isvisiblecube(const ivec &o, int size)
{
    int v = VFC_FULL_VISIBLE;
    float dist;

    loopi(5)
    {
        dist = o.dist(vfcP[i]);
        if(dist < -vfcDfar[i]*size) return VFC_NOT_VISIBLE;
        if(dist < -vfcDnear[i]*size) v = VFC_PART_VISIBLE;
    }

    dist -= vfcDfog;
    if(dist > -vfcDnear[4]*size) return VFC_FOGGED;
    if(dist > -vfcDfar[4]*size) v = VFC_PART_VISIBLE;

    return v;
}

int isvisiblebb(const ivec &bo, const ivec &br)
{
    int v = VFC_FULL_VISIBLE;
    float dnear, dfar;

    loopi(5)
    {
        const plane &p = vfcP[i];
        dnear = dfar = bo.dist(p);
        if(p.x > 0) dfar += p.x*br.x; else dnear += p.x*br.x;
        if(p.y > 0) dfar += p.y*br.y; else dnear += p.y*br.y;
        if(p.z > 0) dfar += p.z*br.z; else dnear += p.z*br.z;
        if(dfar < 0) return VFC_NOT_VISIBLE;
        if(dnear < 0) v = VFC_PART_VISIBLE;
    }

    if(dnear > vfcDfog) return VFC_FOGGED;
    if(dfar > vfcDfog) v = VFC_PART_VISIBLE;

    return v;
}

static inline float vadist(vtxarray *va, const vec &p)
{
    return p.dist_to_bb(va->bbmin, va->bbmax);
}

#define VASORTSIZE 64

static vtxarray *vasort[VASORTSIZE];

static inline void addvisibleva(vtxarray *va)
{
    float dist = vadist(va, camera1->o);
    va->distance = int(dist); /*cv.dist(camera1->o) - va->size*SQRT3/2*/

    int hash = clamp(int(dist*VASORTSIZE/worldsize), 0, VASORTSIZE-1);
    vtxarray **prev = &vasort[hash], *cur = vasort[hash];

    while(cur && va->distance >= cur->distance)
    {
        prev = &cur->next;
        cur = cur->next;
    }

    va->next = cur;
    *prev = va;
}

void sortvisiblevas()
{
    visibleva = NULL;
    vtxarray **last = &visibleva;
    loopi(VASORTSIZE) if(vasort[i])
    {
        vtxarray *va = vasort[i];
        *last = va;
        while(va->next) va = va->next;
        last = &va->next;
    }
}

template<bool fullvis, bool resetocclude>
static inline void findvisiblevas(vector<vtxarray *> &vas)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        int prevvfc = v.curvfc;
        v.curvfc = fullvis ? VFC_FULL_VISIBLE : isvisiblecube(v.o, v.size);
        if(v.curvfc != VFC_NOT_VISIBLE)
        {
            if(pvsoccluded(v.o, v.size))
            {
                v.curvfc += PVS_FULL_VISIBLE - VFC_FULL_VISIBLE;
                continue;
            }
            bool resetchildren = prevvfc >= VFC_NOT_VISIBLE || resetocclude;
            if(resetchildren)
            {
                v.occluded = !v.texs ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
                v.query = NULL;
            }
            addvisibleva(&v);
            if(v.children.length())
            {
                if(fullvis || v.curvfc == VFC_FULL_VISIBLE)
                {
                    if(resetchildren) findvisiblevas<true, true>(v.children);
                    else findvisiblevas<true, false>(v.children);
                }
                else if(resetchildren) findvisiblevas<false, true>(v.children);
                else findvisiblevas<false, false>(v.children);
            }
        }
    }
}

void findvisiblevas()
{
    memset(vasort, 0, sizeof(vasort));
    findvisiblevas<false, false>(varoot);
    sortvisiblevas();
}

void calcvfcD()
{
    loopi(5)
    {
        plane &p = vfcP[i];
        vfcDnear[i] = vfcDfar[i] = 0;
        loopk(3) if(p[k] > 0) vfcDfar[i] += p[k];
        else vfcDnear[i] += p[k];
    }
}

void setvfcP(const vec &bbmin, const vec &bbmax)
{
    vec4 px = camprojmatrix.rowx(), py = camprojmatrix.rowy(), pz = camprojmatrix.rowz(), pw = camprojmatrix.roww();
    vfcP[0] = plane(vec4(pw).mul(-bbmin.x).add(px)).normalize(); // left plane
    vfcP[1] = plane(vec4(pw).mul(bbmax.x).sub(px)).normalize(); // right plane
    vfcP[2] = plane(vec4(pw).mul(-bbmin.y).add(py)).normalize(); // bottom plane
    vfcP[3] = plane(vec4(pw).mul(bbmax.y).sub(py)).normalize(); // top plane
    vfcP[4] = plane(vec4(pw).add(pz)).normalize(); // near/far planes

    vfcDfog = min(calcfogcull(), float(farplane));
    calcvfcD();
}

plane oldvfcP[5];

void savevfcP()
{
    memcpy(oldvfcP, vfcP, sizeof(vfcP));
}

void restorevfcP()
{
    memcpy(vfcP, oldvfcP, sizeof(vfcP));
    calcvfcD();
}

void visiblecubes(bool cull)
{
    if(cull)
    {
        setvfcP();
        findvisiblevas();
    }
    else
    {
        memset(vfcP, 0, sizeof(vfcP));
        vfcDfog = farplane;
        memset(vfcDnear, 0, sizeof(vfcDnear));
        memset(vfcDfar, 0, sizeof(vfcDfar));
        visibleva = NULL;
        loopv(valist)
        {
            vtxarray *va = valist[i];
            va->distance = 0;
            va->curvfc = VFC_FULL_VISIBLE;
            va->occluded = !va->texs ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
            va->query = NULL;
            va->next = visibleva;
            visibleva = va;
        }
    }
}

///////// occlusion queries /////////////

#define MAXQUERY 2048
#define MAXQUERYFRAMES 2

int deferquery = 0;

struct queryframe
{
    int cur, max, defer;
    occludequery queries[MAXQUERY];

    queryframe() : cur(0), max(0), defer(0) {}

    void flip()
    {
        loopi(cur) queries[i].owner = NULL;
        for(; defer > 0 && max < MAXQUERY; defer--)
        {
            queries[max].owner = NULL;
            queries[max].fragments = -1;
            glGenQueries_(1, &queries[max++].id);
        }
        cur = defer = 0;
    }

    occludequery *newquery(void *owner)
    {
        if(cur >= max)
        {
            if(max >= MAXQUERY) return NULL;
            if(deferquery)
            {
                if(max + defer < MAXQUERY) defer++;
                return NULL;
            }
            glGenQueries_(1, &queries[max++].id);
        }
        occludequery *query = &queries[cur++];
        query->owner = owner;
        query->fragments = -1;
        return query;
    }

    void reset() { loopi(max) queries[i].owner = NULL; }

    void cleanup()
    {
        loopi(max)
        {
            glDeleteQueries_(1, &queries[i].id);
            queries[i].owner = NULL;
        }
        cur = max = defer = 0;
    }
};

static queryframe queryframes[MAXQUERYFRAMES];
static uint flipquery = 0;

int getnumqueries()
{
    return queryframes[flipquery].cur;
}

void flipqueries()
{
    flipquery = (flipquery + 1) % MAXQUERYFRAMES;
    queryframes[flipquery].flip();
}

occludequery *newquery(void *owner)
{
    return queryframes[flipquery].newquery(owner);
}

void resetqueries()
{
    loopi(MAXQUERYFRAMES) queryframes[i].reset();
}

void clearqueries()
{
    loopi(MAXQUERYFRAMES) queryframes[i].cleanup();
}

VARF(0, oqany, 0, 0, 2, clearqueries());
VAR(0, oqfrags, 0, 8, 64);
VAR(0, oqwait, 0, 1, 1);

static inline GLenum querytarget()
{
    return oqany && hasOQ2 ? (oqany > 1 && hasES3 ? GL_ANY_SAMPLES_PASSED_CONSERVATIVE : GL_ANY_SAMPLES_PASSED) : GL_SAMPLES_PASSED;
}

void startquery(occludequery *query)
{
    glBeginQuery_(querytarget(), query->id);
}

void endquery(occludequery *query)
{
    glEndQuery_(querytarget());
}

bool checkquery(occludequery *query, bool nowait)
{
    if(query->fragments < 0)
    {
        if(nowait || !oqwait)
        {
            GLint avail;
            glGetQueryObjectiv_(query->id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if(!avail) return false;
        }

        GLuint fragments;
        glGetQueryObjectuiv_(query->id, GL_QUERY_RESULT, &fragments);
        query->fragments = querytarget() == GL_SAMPLES_PASSED || !fragments ? int(fragments) : oqfrags;
    }
    return query->fragments < oqfrags;
}

static GLuint bbvbo = 0, bbebo = 0;

static void setupbb()
{
    if(!bbvbo)
    {
        glGenBuffers_(1, &bbvbo);
        gle::bindvbo(bbvbo);
        vec verts[8];
        loopi(8) verts[i] = vec(i&1, (i>>1)&1, (i>>2)&1);
        glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        gle::clearvbo();
    }
    if(!bbebo)
    {
        glGenBuffers_(1, &bbebo);
        gle::bindebo(bbebo);
        GLushort tris[3*2*6];
        #define GENFACEORIENT(orient, v0, v1, v2, v3) do { \
            int offset = orient*3*2; \
            tris[offset + 0] = v0; \
            tris[offset + 1] = v1; \
            tris[offset + 2] = v2; \
            tris[offset + 3] = v0; \
            tris[offset + 4] = v2; \
            tris[offset + 5] = v3; \
        } while(0);
        #define GENFACEVERT(orient, vert, ox,oy,oz, rx,ry,rz) (ox | oy | oz)
        GENFACEVERTS(0, 1, 0, 2, 0, 4, , , , , , )
        #undef GENFACEORIENT
        #undef GENFACEVERT
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(tris), tris, GL_STATIC_DRAW);
        gle::clearebo();
    }
}

static void cleanupbb()
{
    if(bbvbo) { glDeleteBuffers_(1, &bbvbo); bbvbo = 0; }
    if(bbebo) { glDeleteBuffers_(1, &bbebo); bbebo = 0; }
}

void startbb(bool mask)
{
    setupbb();
    gle::bindvbo(bbvbo);
    gle::bindebo(bbebo);
    gle::vertexpointer(sizeof(vec), (const vec *)0);
    gle::enablevertex();
    SETSHADER(bbquery);
    if(mask)
    {
        glDepthMask(GL_FALSE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
}

void endbb(bool mask)
{
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
    if(mask)
    {
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
}

void drawbb(const ivec &bo, const ivec &br)
{
    LOCALPARAMF(bborigin, bo.x, bo.y, bo.z);
    LOCALPARAMF(bbsize, br.x, br.y, br.z);
    glDrawRangeElements_(GL_TRIANGLES, 0, 8-1, 3*2*6, GL_UNSIGNED_SHORT, (ushort *)0);
    xtraverts += 8;
}

extern int octaentsize;

static octaentities *visiblemms, **lastvisiblemms;

bool mapmodeltransparent(extentity &e)
{
    if(e.attrs[4] > 0 && e.attrs[4] < 100) return true;
    #if 0
    if(mapmodels.inrange(e.attrs[0]))
    {
        mapmodelinfo &mmi = mapmodels[e.attrs[0]];
        model *m = loadlodmodel(mmi.m ? mmi.m : loadmodel(mmi.name), e.viewpos);
        if(m && m->alphatested(true)) return true;
    }
    #endif
    return false;
}

bool mapmodelvisible(extentity &e, bool colvis)
{
    if(e.flags&EF_NOVIS || !checkmapvariant(e.attrs[13]) || !checkmapeffects(e.attrs[14]) || !mapmodels.inrange(e.attrs[0])) return false;
    if(colvis && e.flags&EF_NOCOLLIDE) return false;
    if(e.lastemit)
    {
        if(e.flags&EF_HIDE)
        {
            if(e.spawned()) return false;
        }
        else if(colvis && e.lastemit > 0)
        {
            int millis = lastmillis-e.lastemit, delay = entities::triggertime(e, true);
            if(e.spawned() ? millis > delay : millis < delay) return false;
        }
        else if((colvis || e.lastemit < 0) && e.spawned()) return false;
    }
    mapmodelinfo &mmi = mapmodels[e.attrs[0]];
    model *m = loadlodmodel(mmi.m ? mmi.m : loadmodel(mmi.name), e.viewpos);
    if(!m) return false;
    return true;
}

void findvisiblemms(const vector<extentity *> &ents, bool doquery)
{
    visiblemms = NULL;
    lastvisiblemms = &visiblemms;
    for(vtxarray *va = visibleva; va; va = va->next) if(va->occluded < OCCLUDE_BB && va->curvfc < VFC_FOGGED) loopv(va->mapmodels)
    {
        octaentities *oe = va->mapmodels[i];
        if(isfoggedcube(oe->o, oe->size) || pvsoccluded(oe->bbmin, oe->bbmax)) continue;

        bool occluded = doquery && oe->query && oe->query->owner == oe && checkquery(oe->query);
        if(occluded)
        {
            oe->distance = -1;

            oe->next = NULL;
            *lastvisiblemms = oe;
            lastvisiblemms = &oe->next;
        }
        else
        {
            int visible = 0;
            loopv(oe->mapmodels)
            {
                extentity &e = *ents[oe->mapmodels[i]];
                if(!mapmodelvisible(e) || mapmodeltransparent(e)) continue;
                e.flags |= EF_RENDER;
                ++visible;
            }
            if(!visible) continue;

            oe->distance = int(camera1->o.dist_to_bb(oe->o, oe->size));

            octaentities **prev = &visiblemms, *cur = visiblemms;
            while(cur && cur->distance >= 0 && oe->distance > cur->distance)
            {
                prev = &cur->next;
                cur = cur->next;
            }

            if(*prev == NULL) lastvisiblemms = &oe->next;
            oe->next = *prev;
            *prev = oe;
        }
    }
}

VAR(0, oqmm, 0, 4, 8);
VAR(0, mmanimoverride, -1, 0, ANIM_ALL);

static inline void rendermapmodelent(extentity &e, bool tpass)
{
    if(!mapmodelvisible(e)) return;
    bool blended = mapmodeltransparent(e);
    if(blended != tpass) return;
    entmodelstate mdl;
    mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
    mdl.flags = MDL_CULL_VFC|MDL_CULL_DIST;
    if(e.lastemit)
    {
        mdl.anim = e.spawned() ? ANIM_TRIGGER_ON : ANIM_TRIGGER_OFF;
        if(e.lastemit > 0 && lastmillis-e.lastemit < entities::triggertime(e)) mdl.basetime = e.lastemit;
        else mdl.anim |= ANIM_END;
    }
    if(mmanimoverride)
    {
        mdl.anim = (mmanimoverride<0 ? ANIM_ALL : mmanimoverride)|ANIM_LOOP;
        mdl.basetime = 0;
    }
    mdl.yaw = e.attrs[1]+e.viewyaw;
    mdl.pitch = e.attrs[2]+e.viewpitch;
    mdl.roll = e.attrs[3];
    mdl.o = e.viewpos;
    mdl.color = vec4(1, 1, 1, blended ? min(e.attrs[4]/100.f, 1.f) : 1.f);
    mdl.size = e.attrs[5] ? max(e.attrs[5]/100.f, 1e-3f) : 1.f;
    if(e.attrs[8] || e.attrs[9])
    {
        vec color = game::getpalette(e.attrs[8], e.attrs[9]);
        if(e.attrs[7]) color.mul(vec::fromcolor(e.attrs[7]));
        mdl.material[0] = bvec::fromcolor(color);
    }
    else if(e.attrs[7]) mdl.material[0] = bvec::fromcolor(e.attrs[7]);
    if(e.attrs[10]) mdl.yaw += e.attrs[10]*lastmillis/1000.0f;
    if(e.attrs[11]) mdl.pitch += e.attrs[11]*lastmillis/1000.0f;
    if(e.attrs[12]) mdl.roll += e.attrs[12]*lastmillis/1000.0f;
    rendermapmodel(e.attrs[0], mdl, tpass);
}

void rendermapmodels()
{
    static int skipoq = 0;
    bool doquery = !drawtex && oqfrags && oqmm;
    const vector<extentity *> &ents = entities::getents();
    findvisiblemms(ents, doquery);

    for(octaentities *oe = visiblemms; oe; oe = oe->next) if(oe->distance>=0)
    {
        bool rendered = false;
        loopv(oe->mapmodels)
        {
            extentity &e = *ents[oe->mapmodels[i]];
            if(!(e.flags&EF_RENDER)) continue;
            if(!rendered)
            {
                rendered = true;
                oe->query = doquery && oe->distance>0 && !(++skipoq%oqmm) ? newquery(oe) : NULL;
                if(oe->query) startmodelquery(oe->query);
            }
            rendermapmodelent(e, false);
            e.flags &= ~EF_RENDER;
        }
        if(rendered && oe->query) endmodelquery();
    }
    rendermapmodelbatches();
    clearbatchedmapmodels();

    bool queried = false;
    for(octaentities *oe = visiblemms; oe; oe = oe->next) if(oe->distance<0)
    {
        oe->query = doquery && !camera1->o.insidebb(oe->bbmin, oe->bbmax, 1) ? newquery(oe) : NULL;
        if(!oe->query) continue;
        if(!queried)
        {
            startbb();
            queried = true;
        }
        startquery(oe->query);
        drawbb(oe->bbmin, ivec(oe->bbmax).sub(oe->bbmin));
        endquery(oe->query);
    }
    if(queried)
    {
        endbb();
    }
}

void rendertransparentmapmodels()
{
    const vector<extentity *> &ents = entities::getents();
    for(vtxarray *va = visibleva; va; va = va->next) if(va->occluded < OCCLUDE_BB && va->curvfc < VFC_FOGGED) loopv(va->mapmodels)
    {
        octaentities *oe = va->mapmodels[i];
        if(isfoggedcube(oe->o, oe->size) || pvsoccluded(oe->bbmin, oe->bbmax)) continue;
        loopv(oe->mapmodels)
        {
            extentity &e = *ents[oe->mapmodels[i]];
            if(!mapmodelvisible(e) || !mapmodeltransparent(e)) continue;
            rendermapmodelent(e, true);
        }
    }
}

static inline bool bbinsideva(const ivec &bo, const ivec &br, vtxarray *va)
{
    return bo.x >= va->bbmin.x && bo.y >= va->bbmin.y && bo.z >= va->bbmin.z &&
        br.x <= va->bbmax.x && br.y <= va->bbmax.y && br.z <= va->bbmax.z;
}

static inline bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size)
{
    loopoctabox(o, size, bo, br)
    {
        ivec co(i, o, size);
        if(c[i].ext && c[i].ext->va)
        {
            vtxarray *va = c[i].ext->va;
            if(va->curvfc >= VFC_FOGGED || (va->occluded >= OCCLUDE_BB && bbinsideva(bo, br, va))) continue;
        }
        if(c[i].children && bboccluded(bo, br, c[i].children, co, size>>1)) continue;
        return false;
    }
    return true;
}

bool bboccluded(const ivec &bo, const ivec &br)
{
    int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z);
    if(diff&~((1<<worldscale)-1)) return false;
    int scale = worldscale-1;
    if(diff&(1<<scale)) return bboccluded(bo, br, worldroot, ivec(0, 0, 0), 1<<scale);
    cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->va)
    {
        vtxarray *va = c->ext->va;
        if(va->curvfc >= VFC_FOGGED || (va->occluded >= OCCLUDE_BB && bbinsideva(bo, br, va))) return true;
    }
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->va)
        {
            vtxarray *va = c->ext->va;
            if(va->curvfc >= VFC_FOGGED || (va->occluded >= OCCLUDE_BB && bbinsideva(bo, br, va))) return true;
        }
        scale--;
    }
    if(c->children) return bboccluded(bo, br, c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale);
    return false;
}

VAR(IDF_PERSIST, outline, 0, 1, 1);
CVAR0(IDF_PERSIST, outlinecolour, 0);
VAR(0, dtoutline, 0, 1, 1);

void renderoutline()
{
    ldrnotextureshader->set();

    gle::enablevertex();

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    gle::color(outlinecolour);

    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    if(!dtoutline) glDisable(GL_DEPTH_TEST);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next) if(va->occluded < OCCLUDE_BB)
    {
        if((!va->texs || va->occluded >= OCCLUDE_GEOM) && !va->alphaback && !va->alphafront && !va->refracttris) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }

        if(va->texs && va->occluded < OCCLUDE_GEOM)
        {
            drawvatris(va, 3*va->tris, 0);
            xtravertsva += va->verts;
        }
        if(va->alphaback || va->alphafront || va->refract)
        {
            drawvatris(va, 3*(va->alphabacktris + va->alphafronttris + va->refracttris), 3*(va->tris + va->blendtris));
            xtravertsva += 3*(va->alphabacktris + va->alphafronttris + va->refracttris);
        }

        prev = va;
    }

    if(!dtoutline) glEnable(GL_DEPTH_TEST);

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

CVAR0(IDF_PERSIST, blendbrushcolour, 0x0000C0);

void renderblendbrush(GLuint tex, float x, float y, float w, float h)
{
    SETSHADER(blendbrush);

    gle::enablevertex();

    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, tex);
    gle::color(blendbrushcolour, 0x40);

    LOCALPARAMF(texgenS, 1.0f/w, 0, 0, -x/w);
    LOCALPARAMF(texgenT, 0, 1.0f/h, 0, -y/h);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next) if(va->texs && va->occluded < OCCLUDE_GEOM)
    {
        if(va->o.x + va->size <= x || va->o.y + va->size <= y || va->o.x >= x + w || va->o.y >= y + h) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }

        drawvatris(va, 3*va->tris, 0);
        xtravertsva += va->verts;

        prev = va;
    }

    glDisable(GL_BLEND);

    glDepthFunc(GL_LESS);

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

int calcbbsidemask(const ivec &bbmin, const ivec &bbmax, const vec &lightpos, float lightradius, float bias)
{
    vec pmin = vec(bbmin).sub(lightpos).div(lightradius), pmax = vec(bbmax).sub(lightpos).div(lightradius);
    int mask = 0x3F;
    float dp1 = pmax.x + pmax.y, dn1 = pmax.x - pmin.y, ap1 = fabs(dp1), an1 = fabs(dn1),
          dp2 = pmin.x + pmin.y, dn2 = pmin.x - pmax.y, ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<4)
            | (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<4)
            | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = pmax.y + pmax.z, dn1 = pmax.y - pmin.z, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = pmin.y + pmin.z, dn2 = pmin.y - pmax.z, ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<0)
            | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<0)
            | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = pmax.z + pmax.x, dn1 = pmax.z - pmin.x, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = pmin.z + pmin.x, dn2 = pmin.z - pmax.x, ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<2)
            | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<2)
            | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

    return mask;
}

int calcspheresidemask(const vec &p, float radius, float bias)
{
    // p is in the cubemap's local coordinate system
    // bias = border/(size - border)
    float dxyp = p.x + p.y, dxyn = p.x - p.y, axyp = fabs(dxyp), axyn = fabs(dxyn),
          dyzp = p.y + p.z, dyzn = p.y - p.z, ayzp = fabs(dyzp), ayzn = fabs(dyzn),
          dzxp = p.z + p.x, dzxn = p.z - p.x, azxp = fabs(dzxp), azxn = fabs(dzxn);
    int mask = 0x3F;
    radius *= SQRT2;
    if(axyp > bias*axyn + radius) mask &= dxyp < 0 ? ~((1<<0)|(1<<2)) : ~((2<<0)|(2<<2));
    if(axyn > bias*axyp + radius) mask &= dxyn < 0 ? ~((1<<0)|(2<<2)) : ~((2<<0)|(1<<2));
    if(ayzp > bias*ayzn + radius) mask &= dyzp < 0 ? ~((1<<2)|(1<<4)) : ~((2<<2)|(2<<4));
    if(ayzn > bias*ayzp + radius) mask &= dyzn < 0 ? ~((1<<2)|(2<<4)) : ~((2<<2)|(1<<4));
    if(azxp > bias*azxn + radius) mask &= dzxp < 0 ? ~((1<<4)|(1<<0)) : ~((2<<4)|(2<<0));
    if(azxn > bias*azxp + radius) mask &= dzxn < 0 ? ~((1<<4)|(2<<0)) : ~((2<<4)|(1<<0));
    return mask;
}

int calctrisidemask(const vec &p1, const vec &p2, const vec &p3, float bias)
{
    // p1, p2, p3 are in the cubemap's local coordinate system
    // bias = border/(size - border)
    int mask = 0x3F;
    float dp1 = p1.x + p1.y, dn1 = p1.x - p1.y, ap1 = fabs(dp1), an1 = fabs(dn1),
          dp2 = p2.x + p2.y, dn2 = p2.x - p2.y, ap2 = fabs(dp2), an2 = fabs(dn2),
          dp3 = p3.x + p3.y, dn3 = p3.x - p3.y, ap3 = fabs(dp3), an3 = fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<4)
            | (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp3 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<4)
            | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn3 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = p1.y + p1.z, dn1 = p1.y - p1.z, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.y + p2.z, dn2 = p2.y - p2.z, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.y + p3.z, dn3 = p3.y - p3.z, ap3 = fabs(dp3), an3 = fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<0)
            | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp3 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<0)
            | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn3 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = p1.z + p1.x, dn1 = p1.z - p1.x, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.z + p2.x, dn2 = p2.z - p2.x, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.z + p3.x, dn3 = p3.z - p3.x, ap3 = fabs(dp3), an3 = fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<2)
            | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp3 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<2)
            | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn3 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

    return mask;
}


int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border)
{
    int sides = 0x3F, masks[6] = { 3<<4, 3<<4, 3<<0, 3<<0, 3<<2, 3<<2 };
    float scale = (size - 2*border)/size, bias = border / (float)(size - border);
    // check if cone enclosing side would cross frustum plane
    scale = 2 / (scale*scale + 2);
    loopi(5) if(vfcP[i].dist(lightpos) <= -0.03125f)
    {
        vec n = vec(vfcP[i]).div(lightradius);
        float len = scale*n.squaredlen();
        if(n.x*n.x > len) sides &= n.x < 0 ? ~(1<<0) : ~(2 << 0);
        if(n.y*n.y > len) sides &= n.y < 0 ? ~(1<<2) : ~(2 << 2);
        if(n.z*n.z > len) sides &= n.z < 0 ? ~(1<<4) : ~(2 << 4);
    }
    if (vfcP[4].dist(lightpos) >= vfcDfog + 0.03125f)
    {
        vec n = vec(vfcP[4]).div(lightradius);
        float len = scale*n.squaredlen();
        if(n.x*n.x > len) sides &= n.x >= 0 ? ~(1<<0) : ~(2 << 0);
        if(n.y*n.y > len) sides &= n.y >= 0 ? ~(1<<2) : ~(2 << 2);
        if(n.z*n.z > len) sides &= n.z >= 0 ? ~(1<<4) : ~(2 << 4);
    }
    // this next test usually clips off more sides than the former, but occasionally clips fewer/different ones, so do both and combine results
    // check if frustum corners/origin cross plane sides
    // infinite version, assumes frustum corners merely give direction and extend to infinite distance
    vec p = vec(camera1->o).sub(lightpos).div(lightradius);
    float dp = p.x + p.y, dn = p.x - p.y, ap = fabs(dp), an = fabs(dn);
    masks[0] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    masks[1] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));
    dp = p.y + p.z, dn = p.y - p.z, ap = fabs(dp), an = fabs(dn);
    masks[2] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    masks[3] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));
    dp = p.z + p.x, dn = p.z - p.x, ap = fabs(dp), an = fabs(dn);
    masks[4] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    masks[5] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));
    loopi(4)
    {
        vec n;
        switch(i)
        {
            case 0: n.cross(vfcP[0], vfcP[2]); break;
            case 1: n.cross(vfcP[3], vfcP[0]); break;
            case 2: n.cross(vfcP[2], vfcP[1]); break;
            case 3: n.cross(vfcP[1], vfcP[3]); break;
        }
        dp = n.x + n.y, dn = n.x - n.y, ap = fabs(dp), an = fabs(dn);
        if(ap > 0) masks[0] |= dp >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2);
        if(an > 0) masks[1] |= dn >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2);
        dp = n.y + n.z, dn = n.y - n.z, ap = fabs(dp), an = fabs(dn);
        if(ap > 0) masks[2] |= dp >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4);
        if(an > 0) masks[3] |= dn >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4);
        dp = n.z + n.x, dn = n.z - n.x, ap = fabs(dp), an = fabs(dn);
        if(ap > 0) masks[4] |= dp >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0);
        if(an > 0) masks[5] |= dn >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0);
    }
    return sides & masks[0] & masks[1] & masks[2] & masks[3] & masks[4] & masks[5];
}

VAR(0, smbbcull, 0, 1, 1);
VAR(0, smdistcull, 0, 1, 1);
VAR(0, smnodraw, 0, 0, 1);

vec shadoworigin(0, 0, 0), shadowdir(0, 0, 0);
float shadowradius = 0, shadowbias = 0;
int shadowside = 0, shadowspot = 0;

vtxarray *shadowva = NULL;

static inline void addshadowva(vtxarray *va, float dist)
{
    va->rdistance = int(dist);

    int hash = clamp(int(dist*VASORTSIZE/shadowradius), 0, VASORTSIZE-1);
    vtxarray **prev = &vasort[hash], *cur = vasort[hash];

    while(cur && va->rdistance > cur->rdistance)
    {
        prev = &cur->rnext;
        cur = cur->rnext;
    }

    va->rnext = cur;
    *prev = va;
}

void sortshadowvas()
{
    shadowva = NULL;
    vtxarray **last = &shadowva;
    loopi(VASORTSIZE) if(vasort[i])
    {
        vtxarray *va = vasort[i];
        *last = va;
        while(va->rnext) va = va->rnext;
        last = &va->rnext;
    }
}

void findshadowvas(vector<vtxarray *> &vas)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        float dist = vadist(&v, shadoworigin);
        if(dist < shadowradius || !smdistcull)
        {
            v.shadowmask = !smbbcull ? 0x3F : (v.children.length() || v.mapmodels.length() ?
                                calcbbsidemask(v.bbmin, v.bbmax, shadoworigin, shadowradius, shadowbias) :
                                calcbbsidemask(v.geommin, v.geommax, shadoworigin, shadowradius, shadowbias));
            addshadowva(&v, dist);
            if(v.children.length()) findshadowvas(v.children);
        }
    }
}

void findcsmshadowvas(vector<vtxarray *> &vas)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        ivec bbmin, bbmax;
        if(v.children.length() || v.mapmodels.length()) { bbmin = v.bbmin; bbmax = v.bbmax; }
        else { bbmin = v.geommin; bbmax = v.geommax; }
        v.shadowmask = calcbbcsmsplits(bbmin, bbmax);
        if(v.shadowmask)
        {
            float dist = shadowdir.project_bb(bbmin, bbmax) - shadowbias;
            addshadowva(&v, dist);
            if(v.children.length()) findcsmshadowvas(v.children);
        }
    }
}

void findrsmshadowvas(vector<vtxarray *> &vas)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        ivec bbmin, bbmax;
        if(v.children.length() || v.mapmodels.length()) { bbmin = v.bbmin; bbmax = v.bbmax; }
        else { bbmin = v.geommin; bbmax = v.geommax; }
        v.shadowmask = calcbbrsmsplits(bbmin, bbmax);
        if(v.shadowmask)
        {
            float dist = shadowdir.project_bb(bbmin, bbmax) - shadowbias;
            addshadowva(&v, dist);
            if(v.children.length()) findrsmshadowvas(v.children);
        }
    }
}

void findspotshadowvas(vector<vtxarray *> &vas)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        float dist = vadist(&v, shadoworigin);
        if(dist < shadowradius || !smdistcull)
        {
            v.shadowmask = !smbbcull || (v.children.length() || v.mapmodels.length() ?
                                bbinsidespot(shadoworigin, shadowdir, shadowspot, v.bbmin, v.bbmax) :
                                bbinsidespot(shadoworigin, shadowdir, shadowspot, v.geommin, v.geommax)) ? 1 : 0;
            addshadowva(&v, dist);
            if(v.children.length()) findspotshadowvas(v.children);
        }
    }
}

void findshadowvas()
{
    memset(vasort, 0, sizeof(vasort));
    switch(shadowmapping)
    {
        case SM_REFLECT: findrsmshadowvas(varoot); break;
        case SM_CUBEMAP: findshadowvas(varoot); break;
        case SM_CASCADE: findcsmshadowvas(varoot); break;
        case SM_SPOT: findspotshadowvas(varoot); break;
    }
    sortshadowvas();
}

void rendershadowmapworld()
{
    SETSHADER(shadowmapworld);

    gle::enablevertex();

    vtxarray *prev = NULL;
    for(vtxarray *va = shadowva; va; va = va->rnext) if(va->tris && va->shadowmask&(1<<shadowside))
    {
        if(!prev || va->vbuf != prev->vbuf)
        {
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }

        if(!smnodraw) drawvatris(va, 3*va->tris, 0);
        xtravertsva += va->verts;

        prev = va;
    }

    if(getskyshadow())
    {
        prev = NULL;
        for(vtxarray *va = shadowva; va; va = va->rnext) if(va->sky && va->shadowmask&(1<<shadowside))
        {
            if(!prev || va->vbuf != prev->vbuf)
            {
                gle::bindvbo(va->vbuf);
                gle::bindebo(va->skybuf);
                const vertex *ptr = 0;
                gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            }

            if(!smnodraw) drawvaskytris(va);
            xtravertsva += va->sky/3;

            prev = va;
        }
    }

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

static octaentities *shadowmms = NULL;

void findshadowmms()
{
    shadowmms = NULL;
    octaentities **lastmms = &shadowmms;
    for(vtxarray *va = shadowva; va; va = va->rnext) loopvj(va->mapmodels)
    {
        octaentities *oe = va->mapmodels[j];
        switch(shadowmapping)
        {
            case SM_REFLECT:
                break;
            case SM_CASCADE:
                if(!calcbbcsmsplits(oe->bbmin, oe->bbmax))
                    continue;
                break;
            case SM_CUBEMAP:
                if(smdistcull && shadoworigin.dist_to_bb(oe->bbmin, oe->bbmax) >= shadowradius)
                    continue;
                break;
            case SM_SPOT:
                if(smdistcull && shadoworigin.dist_to_bb(oe->bbmin, oe->bbmax) >= shadowradius)
                    continue;
                if(smbbcull && !bbinsidespot(shadoworigin, shadowdir, shadowspot, oe->bbmin, oe->bbmax))
                    continue;
                break;
        }
        oe->rnext = NULL;
        *lastmms = oe;
        lastmms = &oe->rnext;
    }
}

void batchshadowmapmodels(bool skipmesh)
{
    if(!shadowmms) return;
    int nflags = EF_NOVIS|EF_NOSHADOW;
    if(skipmesh) nflags |= EF_SHADOWMESH;
    const vector<extentity *> &ents = entities::getents();
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext) loopvk(oe->mapmodels)
    {
        extentity &e = *ents[oe->mapmodels[k]];
        if(e.flags&nflags || !mapmodelvisible(e)) continue;
        e.flags |= EF_RENDER;
    }
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext) loopvj(oe->mapmodels)
    {
        extentity &e = *ents[oe->mapmodels[j]];
        if(!(e.flags&EF_RENDER)) continue;
        rendermapmodelent(e, false);
        e.flags &= ~EF_RENDER;
    }
}

VAR(0, oqdist, 0, 256, 1024);

struct renderstate
{
    bool colormask, depthmask;
    int alphaing;
    GLuint vbuf;
    bool vattribs, vquery;
    vec colorscale;
    float alphascale;
    float refractscale;
    vec refractcolor;
    bool blend;
    int blendx, blendy;
    int globals, tmu;
    GLuint textures[7];
    Slot *slot, *texgenslot;
    VSlot *vslot, *texgenvslot;
    vec2 texgenscroll;
    int texgenorient, texgenmillis;

    renderstate() : colormask(true), depthmask(true), alphaing(0), vbuf(0), vattribs(false), vquery(false), colorscale(1, 1, 1), alphascale(0), refractscale(0), refractcolor(1, 1, 1), blend(false), blendx(-1), blendy(-1), globals(-1), tmu(-1), slot(NULL), texgenslot(NULL), vslot(NULL), texgenvslot(NULL), texgenscroll(0, 0), texgenorient(-1), texgenmillis(lastmillis)
    {
        loopk(7) textures[k] = 0;
    }
};

static inline void disablevbuf(renderstate &cur)
{
    gle::clearvbo();
    gle::clearebo();
    cur.vbuf = 0;
}

static inline void enablevquery(renderstate &cur)
{
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }
    if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }
    startbb(false);
    cur.vquery = true;
}

static inline void disablevquery(renderstate &cur)
{
    endbb(false);
    cur.vquery = false;
}

static void renderquery(renderstate &cur, occludequery *query, vtxarray *va, bool full = true)
{
    if(!cur.vquery) enablevquery(cur);

    startquery(query);

    if(full) drawbb(ivec(va->bbmin).sub(1), ivec(va->bbmax).sub(va->bbmin).add(2));
    else drawbb(va->geommin, ivec(va->geommax).sub(va->geommin));

    endquery(query);
}

enum
{
    RENDERPASS_GBUFFER = 0,
    RENDERPASS_Z,
    RENDERPASS_CAUSTICS,
    RENDERPASS_GBUFFER_BLEND,
    RENDERPASS_RSM,
    RENDERPASS_RSM_BLEND
};

struct geombatch
{
    const elementset &es;
    VSlot &vslot;
    int offset;
    vtxarray *va;
    int next, batch;

    geombatch(const elementset &es, int offset, vtxarray *va)
      : es(es), vslot(lookupvslot(es.texture)), offset(offset), va(va),
        next(-1), batch(-1)
    {}

    int compare(const geombatch &b) const
    {
        if(va->vbuf < b.va->vbuf) return -1;
        if(va->vbuf > b.va->vbuf) return 1;
        if(es.layer&LAYER_BOTTOM)
        {
            if(!(b.es.layer&LAYER_BOTTOM)) return 1;
            int x1 = va->o.x&~0xFFF, x2 = b.va->o.x&~0xFFF;
            if(x1 < x2) return -1;
            if(x1 > x2) return 1;
            int y1 = va->o.y&~0xFFF, y2 = b.va->o.y&~0xFFF;
            if(y1 < y2) return -1;
            if(y1 > y2) return 1;
        }
        else if(b.es.layer&LAYER_BOTTOM) return -1;
        if(vslot.slot->shader < b.vslot.slot->shader) return -1;
        if(vslot.slot->shader > b.vslot.slot->shader) return 1;
        if(es.texture < b.es.texture) return -1;
        if(es.texture > b.es.texture) return 1;
        if(es.envmap < b.es.envmap) return -1;
        if(es.envmap > b.es.envmap) return 1;
        if(vslot.slot->params.length() < b.vslot.slot->params.length()) return -1;
        if(vslot.slot->params.length() > b.vslot.slot->params.length()) return 1;
        if(es.orient < b.es.orient) return -1;
        if(es.orient > b.es.orient) return 1;
        return 0;
    }
};

static vector<geombatch> geombatches;
static int firstbatch = -1, numbatches = 0;

static void mergetexs(renderstate &cur, vtxarray *va, elementset *texs = NULL, int numtexs = 0, int offset = 0)
{
    if(!texs)
    {
        texs = va->texelems;
        numtexs = va->texs;
        if(cur.alphaing)
        {
            texs += va->texs + va->blends;
            offset += 3*(va->tris + va->blendtris);
            numtexs = va->alphaback;
            if(cur.alphaing > 1) numtexs += va->alphafront + va->refract;
        }
    }

    if(firstbatch < 0)
    {
        firstbatch = geombatches.length();
        numbatches = numtexs;
        loopi(numtexs-1)
        {
            geombatches.add(geombatch(texs[i], offset, va)).next = i+1;
            offset += texs[i].length;
        }
        geombatches.add(geombatch(texs[numtexs-1], offset, va));
        return;
    }

    int prevbatch = -1, curbatch = firstbatch, curtex = 0;
    do
    {
        geombatch &b = geombatches.add(geombatch(texs[curtex], offset, va));
        offset += texs[curtex].length;
        int dir = -1;
        while(curbatch >= 0)
        {
            dir = b.compare(geombatches[curbatch]);
            if(dir <= 0) break;
            prevbatch = curbatch;
            curbatch = geombatches[curbatch].next;
        }
        if(!dir)
        {
            int last = curbatch, next;
            for(;;)
            {
                next = geombatches[last].batch;
                if(next < 0) break;
                last = next;
            }
            if(last==curbatch)
            {
                b.batch = curbatch;
                b.next = geombatches[curbatch].next;
                if(prevbatch < 0) firstbatch = geombatches.length()-1;
                else geombatches[prevbatch].next = geombatches.length()-1;
                curbatch = geombatches.length()-1;
            }
            else
            {
                b.batch = next;
                geombatches[last].batch = geombatches.length()-1;
            }
        }
        else
        {
            numbatches++;
            b.next = curbatch;
            if(prevbatch < 0) firstbatch = geombatches.length()-1;
            else geombatches[prevbatch].next = geombatches.length()-1;
            prevbatch = geombatches.length()-1;
        }
    }
    while(++curtex < numtexs);
}

static inline void enablevattribs(renderstate &cur, bool all = true)
{
    gle::enablevertex();
    if(all)
    {
        gle::enabletexcoord0();
        gle::enablenormal();
        gle::enabletangent();
    }
    cur.vattribs = true;
}

static inline void disablevattribs(renderstate &cur, bool all = true)
{
    gle::disablevertex();
    if(all)
    {
        gle::disabletexcoord0();
        gle::disablenormal();
        gle::disabletangent();
    }
    cur.vattribs = false;
}

static void changevbuf(renderstate &cur, int pass, vtxarray *va)
{
    gle::bindvbo(va->vbuf);
    gle::bindebo(va->ebuf);
    cur.vbuf = va->vbuf;

    vertex *vdata = (vertex *)0;
    gle::vertexpointer(sizeof(vertex), vdata->pos.v);

    if(pass==RENDERPASS_GBUFFER || pass==RENDERPASS_RSM)
    {
        gle::normalpointer(sizeof(vertex), vdata->norm.v, GL_BYTE);
        gle::texcoord0pointer(sizeof(vertex), vdata->tc.v);
        gle::tangentpointer(sizeof(vertex), vdata->tangent.v, GL_BYTE);
    }
}

static void changebatchtmus(renderstate &cur, int pass, geombatch &b)
{
    if(b.vslot.slot->shader && b.vslot.slot->shader->type&SHADER_ENVMAP && b.es.envmap!=EMID_CUSTOM)
    {
        GLuint emtex = lookupenvmap(b.es.envmap);
        if(cur.textures[TEX_ENVMAP]!=emtex)
        {
            cur.tmu = TEX_ENVMAP;
            glActiveTexture_(GL_TEXTURE0 + TEX_ENVMAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cur.textures[TEX_ENVMAP] = emtex);
        }
    }

    if(b.es.layer&LAYER_BOTTOM)
    {
        if(!cur.blend)
        {
            cur.blend = true;
            cur.vslot = NULL;

        }
        if((cur.blendx != (b.va->o.x&~0xFFF) || cur.blendy != (b.va->o.y&~0xFFF)))
        {
            cur.tmu = 7;
            glActiveTexture_(GL_TEXTURE7);
            bindblendtexture(b.va->o);
            cur.blendx = b.va->o.x&~0xFFF;
            cur.blendy = b.va->o.y&~0xFFF;
        }
    }
    else if(cur.blend)
    {
        cur.blend = false;
        cur.vslot = NULL;
    }

    if(cur.tmu != 0)
    {
        cur.tmu = 0;
        glActiveTexture_(GL_TEXTURE0);
    }
}

static inline void bindslottex(renderstate &cur, int type, Texture *tex, GLenum target = GL_TEXTURE_2D)
{
    if(cur.textures[type] != tex->id)
    {
        if(cur.tmu != type)
        {
            cur.tmu = type;
            glActiveTexture_(GL_TEXTURE0 + type);
        }
        glBindTexture(target, cur.textures[type] = tex->id);
    }
}

VAR(0, blankgeom, 0, 0, 1);
CVAR(0, lightcolour, 0);
FVAR(0, lightintensity, 0, 1, 10);

static void changeslottmus(renderstate &cur, int pass, Slot &slot, VSlot &vslot)
{
    if(pass==RENDERPASS_GBUFFER || pass==RENDERPASS_RSM)
    {
        Texture *diffuse = blankgeom ? blanktexture : (!slot.sts.empty() ? slot.sts[0].t : notexture);
        bindslottex(cur, TEX_DIFFUSE, diffuse);

        if(pass == RENDERPASS_GBUFFER)
        {
            if(msaasamples) GLOBALPARAMF(hashid, vslot.index);

            if(slot.shader && slot.shader->type&SHADER_TRIPLANAR)
            {
                float scale = TEX_SCALE/vslot.scale;
                GLOBALPARAMF(texgenscale, scale/diffuse->xs, scale/diffuse->ys);
            }
        }
    }

    vec colorscale = vslot.getcolorscale();
    if(!lightcolour.iszero()) colorscale = lightcolour.tocolor();
    if(lightintensity != 1.f) colorscale.mul(lightintensity);

    if(cur.alphaing)
    {
        float alpha = cur.alphaing > 1 ? vslot.alphafront : vslot.alphaback;
        if(cur.alphascale != alpha)
        {
            cur.alphascale = alpha;
            cur.refractscale = 0;
            goto changecolorparams;
        }
        if(cur.colorscale != colorscale)
        {
        changecolorparams:
            cur.colorscale = colorscale;
            GLOBALPARAMF(colorparams, alpha*colorscale.x, alpha*colorscale.y, alpha*colorscale.z, alpha);
        }
        if(cur.alphaing > 1 && vslot.refractscale > 0 && (cur.refractscale != vslot.refractscale || cur.refractcolor != vslot.refractcolor))
        {
            cur.refractscale = vslot.refractscale;
            cur.refractcolor = vslot.refractcolor;
            float refractscale = 0.5f/ldrscale*(1-alpha);
            GLOBALPARAMF(refractparams, vslot.refractcolor.x*refractscale, vslot.refractcolor.y*refractscale, vslot.refractcolor.z*refractscale, vslot.refractscale*viewh);
        }
    }
    else if(cur.colorscale != colorscale)
    {
        cur.colorscale = colorscale;
        GLOBALPARAMF(colorparams, colorscale.x, colorscale.y, colorscale.z, 1);
    }

    loopvj(slot.sts)
    {
        Slot::Tex &t = slot.sts[j];
        switch(t.type)
        {
            case TEX_ENVMAP:
                if(t.t) bindslottex(cur, t.type, t.t, GL_TEXTURE_CUBE_MAP);
                break;
            case TEX_NORMAL:
            case TEX_GLOW:
                bindslottex(cur, t.type, t.t);
                break;
        }
    }

    if(pass == RENDERPASS_GBUFFER && vslot.detail)
    {
        VSlot &detail = lookupvslot(vslot.detail);
        loopvj(detail.slot->sts)
        {
            Slot::Tex &t = detail.slot->sts[j];
            switch(t.type)
            {
                case TEX_DIFFUSE:
                    if(slot.shader && slot.shader->type&SHADER_TRIPLANAR)
                    {
                        float scale = TEX_SCALE/detail.scale;
                        GLOBALPARAMF(detailscale, scale/t.t->xs, scale/t.t->ys);
                    }
                    // fall-through
                case TEX_NORMAL:
                    bindslottex(cur, TEX_DETAIL + t.type, t.t);
                    break;
            }
        }
    }

    GLOBALPARAM(rotate, vec2(vslot.angle.y, vslot.angle.z));

    if(cur.tmu != 0)
    {
        cur.tmu = 0;
        glActiveTexture_(GL_TEXTURE0);
    }

    cur.slot = &slot;
    cur.vslot = &vslot;
}

static void changetexgen(renderstate &cur, int orient, Slot &slot, VSlot &vslot)
{
    if(cur.texgenslot != &slot || cur.texgenvslot != &vslot)
    {
        Texture *curtex = !cur.texgenslot || cur.texgenslot->sts.empty() ? notexture : cur.texgenslot->sts[0].t,
                *tex = slot.sts.empty() ? notexture : slot.sts[0].t;
        if(!cur.texgenvslot || slot.sts.empty() ||
            (curtex->xs != tex->xs || curtex->ys != tex->ys ||
             cur.texgenvslot->rotation != vslot.rotation || cur.texgenvslot->scale != vslot.scale ||
             cur.texgenvslot->offset != vslot.offset || cur.texgenvslot->scroll != vslot.scroll) ||
             cur.texgenvslot->angle != vslot.angle)
        {
            const texrotation &r = texrotations[vslot.rotation];
            float xs = r.flipx ? -tex->xs : tex->xs,
                  ys = r.flipy ? -tex->ys : tex->ys;
            vec2 scroll(vslot.scroll);
            if(r.swapxy) swap(scroll.x, scroll.y);
            scroll.x *= cur.texgenmillis*tex->xs/xs;
            scroll.y *= cur.texgenmillis*tex->ys/ys;
            if(cur.texgenscroll != scroll)
            {
                cur.texgenscroll = scroll;
                cur.texgenorient = -1;
            }
        }
        cur.texgenslot = &slot;
        cur.texgenvslot = &vslot;
    }

    if(cur.texgenorient == orient) return;
    GLOBALPARAM(texgenscroll, cur.texgenscroll);

    cur.texgenorient = orient;
}

static inline void changeshader(renderstate &cur, int pass, geombatch &b)
{
    VSlot &vslot = b.vslot;
    Slot &slot = *vslot.slot;
    if(pass == RENDERPASS_RSM)
    {
        extern Shader *rsmworldshader;
        if(b.es.layer&LAYER_BOTTOM) rsmworldshader->setvariant(0, 0, slot, vslot);
        else rsmworldshader->set(slot, vslot);
    }
    else if(cur.alphaing) slot.shader->setvariant(cur.alphaing > 1 && vslot.refractscale > 0 ? 1 : 0, 1, slot, vslot);
    else if(b.es.layer&LAYER_BOTTOM) slot.shader->setvariant(0, 0, slot, vslot);
    else slot.shader->set(slot, vslot);
    cur.globals = GlobalShaderParamState::nextversion;
}

template<class T>
static inline void updateshader(T &cur)
{
    if(cur.globals != GlobalShaderParamState::nextversion)
    {
        if(Shader::lastshader) Shader::lastshader->flushparams();
        cur.globals = GlobalShaderParamState::nextversion;
    }
}

static void renderbatch(renderstate &cur, int pass, geombatch &b)
{
    gbatches++;
    for(geombatch *curbatch = &b;; curbatch = &geombatches[curbatch->batch])
    {
        ushort len = curbatch->es.length;
        if(len)
        {
            drawtris(len, (ushort *)0 + curbatch->va->eoffset + curbatch->offset, curbatch->es.minvert, curbatch->es.maxvert);
            vtris += len/3;
        }
        if(curbatch->batch < 0) break;
    }
}

static void resetbatches()
{
    geombatches.setsize(0);
    firstbatch = -1;
    numbatches = 0;
}

static void renderbatches(renderstate &cur, int pass)
{
    cur.slot = NULL;
    cur.vslot = NULL;
    int curbatch = firstbatch;
    if(curbatch >= 0)
    {
        if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
        if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
        if(!cur.vattribs)
        {
            if(cur.vquery) disablevquery(cur);
            enablevattribs(cur);
        }
    }
    while(curbatch >= 0)
    {
        geombatch &b = geombatches[curbatch];
        curbatch = b.next;

        if(cur.vbuf != b.va->vbuf) changevbuf(cur, pass, b.va);
        if(pass == RENDERPASS_GBUFFER || pass == RENDERPASS_RSM) changebatchtmus(cur, pass, b);
        if(cur.vslot != &b.vslot)
        {
            changeslottmus(cur, pass, *b.vslot.slot, b.vslot);
            if(cur.texgenorient != b.es.orient || (cur.texgenorient < O_ANY && cur.texgenvslot != &b.vslot)) changetexgen(cur, b.es.orient, *b.vslot.slot, b.vslot);
            changeshader(cur, pass, b);

        }
        else
        {
            if(cur.texgenorient != b.es.orient) changetexgen(cur, b.es.orient, *b.vslot.slot, b.vslot);
            updateshader(cur);
        }

        renderbatch(cur, pass, b);
    }

    resetbatches();
}

void renderzpass(renderstate &cur, vtxarray *va)
{
    if(!cur.vattribs)
    {
        if(cur.vquery) disablevquery(cur);
        enablevattribs(cur, false);
    }
    if(cur.vbuf!=va->vbuf) changevbuf(cur, RENDERPASS_Z, va);
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }

    int firsttex = 0, numtris = va->tris, offset = 0;
    if(cur.alphaing)
    {
        firsttex += va->texs + va->blends;
        offset += 3*(va->tris + va->blendtris);
        numtris = va->alphabacktris + va->alphafronttris + va->refracttris;
        xtravertsva += 3*numtris;
    }
    else xtravertsva += va->verts;
    nocolorshader->set();
    drawvatris(va, 3*numtris, offset);
}

#define startvaquery(va, flush) \
    do { \
        if(va->query) \
        { \
            flush; \
            startquery(va->query); \
        } \
    } while(0)


#define endvaquery(va, flush) \
    do { \
        if(va->query) \
        { \
            flush; \
            endquery(va->query); \
        } \
    } while(0)

VAR(0, batchgeom, 0, 1, 1);

void renderva(renderstate &cur, vtxarray *va, int pass = RENDERPASS_GBUFFER, bool doquery = false)
{
    switch(pass)
    {
        case RENDERPASS_GBUFFER:
            if(!cur.alphaing) vverts += va->verts;
            if(doquery) startvaquery(va, { if(geombatches.length()) renderbatches(cur, pass); });
            mergetexs(cur, va);
            if(doquery) endvaquery(va, { if(geombatches.length()) renderbatches(cur, pass); });
            else if(!batchgeom && geombatches.length()) renderbatches(cur, pass);
            break;

        case RENDERPASS_GBUFFER_BLEND:
            if(doquery) startvaquery(va, { if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER); });
            mergetexs(cur, va, &va->texelems[va->texs], va->blends, 3*va->tris);
            if(doquery) endvaquery(va, { if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER); });
            else if(!batchgeom && geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);
            break;

        case RENDERPASS_CAUSTICS:
            if(!cur.vattribs) enablevattribs(cur, false);
            if(cur.vbuf!=va->vbuf) changevbuf(cur, pass, va);
            drawvatris(va, 3*va->tris, 0);
            xtravertsva += va->verts;
            break;

        case RENDERPASS_Z:
            if(doquery) startvaquery(va, );
            renderzpass(cur, va);
            if(doquery) endvaquery(va, );
            break;

        case RENDERPASS_RSM:
            mergetexs(cur, va);
            if(!batchgeom && geombatches.length()) renderbatches(cur, pass);
            break;

        case RENDERPASS_RSM_BLEND:
            mergetexs(cur, va, &va->texelems[va->texs], va->blends, 3*va->tris);
            if(!batchgeom && geombatches.length()) renderbatches(cur, RENDERPASS_RSM);
            break;
    }
}

void cleanupva()
{
    clearvas(worldroot);
    clearqueries();
    cleanupbb();
    cleanupgrass();
}

void setupgeom(renderstate &cur)
{
    glActiveTexture_(GL_TEXTURE0);
    GLOBALPARAMF(colorparams, 1, 1, 1, 1);
    GLOBALPARAMF(blendlayer, 1.0f);
}

void cleanupgeom(renderstate &cur)
{
    if(cur.vattribs) disablevattribs(cur);
    if(cur.vbuf) disablevbuf(cur);
}

VAR(0, oqgeom, 0, 1, 1);

void rendergeom()
{
    bool doOQ = oqfrags && oqgeom && !drawtex, multipassing = false;
    renderstate cur;

    int blends = 0;
    if(doOQ)
    {
        for(vtxarray *va = visibleva; va; va = va->next) if(va->texs)
        {
            if(!camera1->o.insidebb(va->o, va->size, 2))
            {
                if(va->parent && va->parent->occluded >= OCCLUDE_BB)
                {
                    va->query = NULL;
                    va->occluded = OCCLUDE_PARENT;
                    continue;
                }
                va->occluded = va->query && va->query->owner == va && checkquery(va->query) ? min(va->occluded+1, int(OCCLUDE_BB)) : OCCLUDE_NOTHING;
                va->query = newquery(va);
                if(!va->query || !va->occluded)
                    va->occluded = pvsoccluded(va->geommin, va->geommax) ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
                if(va->occluded >= OCCLUDE_GEOM)
                {
                    if(va->query)
                    {
                        if(cur.vattribs) disablevattribs(cur, false);
                        if(cur.vbuf) disablevbuf(cur);
                        renderquery(cur, va->query, va);
                    }
                    continue;
                }
            }
            else
            {
                va->query = NULL;
                va->occluded = pvsoccluded(va->geommin, va->geommax) ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
                if(va->occluded >= OCCLUDE_GEOM) continue;
            }

            renderva(cur, va, RENDERPASS_Z, true);
        }

        if(cur.vquery) disablevquery(cur);
        if(cur.vattribs) disablevattribs(cur, false);
        if(cur.vbuf) disablevbuf(cur);

        glFlush();
        if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }
        if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }
        workinoq();
        if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
        if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }

        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        cur.texgenorient = -1;
        setupgeom(cur);
        resetbatches();

        for(vtxarray *va = visibleva; va; va = va->next) if(va->texs && va->occluded < OCCLUDE_GEOM)
        {
            blends += va->blends;
            renderva(cur, va, RENDERPASS_GBUFFER);
        }
        if(geombatches.length()) { renderbatches(cur, RENDERPASS_GBUFFER); glFlush(); }
        for(vtxarray *va = visibleva; va; va = va->next) if(va->texs && va->occluded >= OCCLUDE_GEOM)
        {
            if((va->parent && va->parent->occluded >= OCCLUDE_BB) || (va->query && checkquery(va->query)))
            {
                va->occluded = OCCLUDE_BB;
                continue;
            }
            else
            {
                va->occluded = pvsoccluded(va->geommin, va->geommax) ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
                if(va->occluded >= OCCLUDE_GEOM) continue;
            }

            blends += va->blends;
            renderva(cur, va, RENDERPASS_GBUFFER);

        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);
    }
    else
    {
        setupgeom(cur);
        resetbatches();
        for(vtxarray *va = visibleva; va; va = va->next) if(va->texs)
        {
            va->query = NULL;
            va->occluded = pvsoccluded(va->geommin, va->geommax) ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
            if(va->occluded >= OCCLUDE_GEOM) continue;
            blends += va->blends;
            renderva(cur, va, RENDERPASS_GBUFFER);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);
    }

    if(blends)
    {
        if(cur.vbuf) disablevbuf(cur);

        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        maskgbuffer("cn");

        GLOBALPARAMF(blendlayer, 0.0f);
        cur.texgenorient = -1;
        for(vtxarray *va = visibleva; va; va = va->next) if(va->blends && va->occluded < OCCLUDE_GEOM && va->curvfc != VFC_FOGGED)
        {
            renderva(cur, va, RENDERPASS_GBUFFER_BLEND);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);

        maskgbuffer("cnd");
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    if(multipassing) glDepthFunc(GL_LESS);

    cleanupgeom(cur);

    if(!doOQ)
    {
        glFlush();
        if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }
        if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }
        workinoq();
        if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
        if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
    }
}

int dynamicshadowvabounds(int mask, vec &bbmin, vec &bbmax)
{
    int vis = 0;
    for(vtxarray *va = shadowva; va; va = va->rnext) if(va->shadowmask&mask && va->dyntexs)
    {
        bbmin.min(vec(va->geommin));
        bbmax.max(vec(va->geommax));
        vis++;
    }
    return vis;
}

void renderrsmgeom(bool dyntex)
{
    renderstate cur;
    if(!dyntex) cur.texgenmillis = 0;

    setupgeom(cur);

    if(getskyshadow())
    {
        enablevattribs(cur, false);
        SETSHADER(rsmsky);
        vtxarray *prev = NULL;
        for(vtxarray *va = shadowva; va; va = va->rnext) if(va->sky)
        {
            if(!prev || va->vbuf != prev->vbuf)
            {
                gle::bindvbo(va->vbuf);
                gle::bindebo(va->skybuf);
                const vertex *ptr = 0;
                gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            }

            drawvaskytris(va);
            xtravertsva += va->sky/3;

            prev = va;
        }
        if(cur.vattribs) disablevattribs(cur, false);
    }

    resetbatches();

    int blends = 0;
    for(vtxarray *va = shadowva; va; va = va->rnext) if(va->texs)
    {
        blends += va->blends;
        renderva(cur, va, RENDERPASS_RSM);
    }

    if(geombatches.length()) renderbatches(cur, RENDERPASS_RSM);

    bool multipassing = false;

    if(blends)
    {
        if(cur.vbuf) disablevbuf(cur);

        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        GLOBALPARAMF(blendlayer, 0.0f);
        cur.texgenorient = -1;
        for(vtxarray *va = shadowva; va; va = va->rnext) if(va->blends)
        {
            renderva(cur, va, RENDERPASS_RSM_BLEND);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_RSM);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    if(multipassing) glDepthFunc(GL_LESS);

    cleanupgeom(cur);
}

static vector<vtxarray *> alphavas;
static int alphabackvas = 0, alpharefractvas = 0;
float alphafrontsx1 = -1, alphafrontsx2 = 1, alphafrontsy1 = -1, alphafrontsy2 = -1,
      alphabacksx1 = -1, alphabacksx2 = 1, alphabacksy1 = -1, alphabacksy2 = -1,
      alpharefractsx1 = -1, alpharefractsx2 = 1, alpharefractsy1 = -1, alpharefractsy2 = 1;
uint alphatiles[LIGHTTILE_MAXH];

int findalphavas()
{
    alphavas.setsize(0);
    alphafrontsx1 = alphafrontsy1 = alphabacksx1 = alphabacksy1 = alpharefractsx1 = alpharefractsy1 = 1;
    alphafrontsx2 = alphafrontsy2 = alphabacksx2 = alphabacksy2 = alpharefractsx2 = alpharefractsy2 = -1;
    alphabackvas = alpharefractvas = 0;
    memset(alphatiles, 0, sizeof(alphatiles));
    for(vtxarray *va = visibleva; va; va = va->next) if(va->alphabacktris || va->alphafronttris || va->refracttris)
    {
        if(va->occluded >= OCCLUDE_BB) continue;
        if(va->occluded >= OCCLUDE_GEOM && pvsoccluded(va->alphamin, va->alphamax)) continue;
        if(va->curvfc==VFC_FOGGED) continue;
        float sx1 = -1, sx2 = 1, sy1 = -1, sy2 = 1;
        if(!calcbbscissor(va->alphamin, va->alphamax, sx1, sy1, sx2, sy2)) continue;
        alphavas.add(va);
        masktiles(alphatiles, sx1, sy1, sx2, sy2);
        alphafrontsx1 = min(alphafrontsx1, sx1);
        alphafrontsy1 = min(alphafrontsy1, sy1);
        alphafrontsx2 = max(alphafrontsx2, sx2);
        alphafrontsy2 = max(alphafrontsy2, sy2);
        if(va->alphabacktris)
        {
            alphabackvas++;
            alphabacksx1 = min(alphabacksx1, sx1);
            alphabacksy1 = min(alphabacksy1, sy1);
            alphabacksx2 = max(alphabacksx2, sx2);
            alphabacksy2 = max(alphabacksy2, sy2);
        }
        if(va->refracttris)
        {
            if(!calcbbscissor(va->refractmin, va->refractmax, sx1, sy1, sx2, sy2)) continue;
            alpharefractvas++;
            alpharefractsx1 = min(alpharefractsx1, sx1);
            alpharefractsy1 = min(alpharefractsy1, sy1);
            alpharefractsx2 = max(alpharefractsx2, sx2);
            alpharefractsy2 = max(alpharefractsy2, sy2);
        }
    }
    return (alpharefractvas ? 4 : 0) | (alphavas.length() ? 2 : 0) | (alphabackvas ? 1 : 0);
}

void renderrefractmask()
{
    gle::enablevertex();

    vtxarray *prev = NULL;
    loopv(alphavas)
    {
        vtxarray *va = alphavas[i];
        if(!va->refracttris) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }

        drawvatris(va, 3*va->refracttris, 3*(va->tris + va->blendtris + va->alphabacktris + va->alphafronttris));
        xtravertsva += 3*va->refracttris;

        prev = va;
    }

    gle::clearvbo();
    gle::clearebo();
    gle::disablevertex();
}

void renderalphageom(int side)
{
    resetbatches();

    renderstate cur;
    cur.alphaing = side;
    cur.alphascale = -1;

    setupgeom(cur);

    if(side == 2)
    {
        loopv(alphavas) renderva(cur, alphavas[i], RENDERPASS_GBUFFER);
        if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);
    }
    else
    {
        glCullFace(GL_FRONT);
        loopv(alphavas) if(alphavas[i]->alphabacktris) renderva(cur, alphavas[i], RENDERPASS_GBUFFER);
        if(geombatches.length()) renderbatches(cur, RENDERPASS_GBUFFER);
        glCullFace(GL_BACK);
    }

    cleanupgeom(cur);
}

CVAR(IDF_PERSIST, explicitskycolour, 0x800080);

bool renderexplicitsky(bool outline)
{
    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next) if(va->sky && va->occluded < OCCLUDE_BB &&
        ((va->skymax.x >= 0 && isvisiblebb(va->skymin, ivec(va->skymax).sub(va->skymin)) != VFC_NOT_VISIBLE) ||
         !insideworld(camera1->o)))
    {
        if(!prev || va->vbuf != prev->vbuf)
        {
            if(!prev)
            {
                gle::enablevertex();
                if(outline)
                {
                    ldrnotextureshader->set();
                    gle::color(explicitskycolour);
                    glDepthMask(GL_FALSE);
                    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                }
                else if(editmode)
                {
                    maskgbuffer("d");
                    SETSHADER(depth);
                }
                else
                {
                    nocolorshader->set();
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                }
            }
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->skybuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
        }
        drawvaskytris(va);
        xtraverts += va->sky;
        prev = va;
    }
    if(!prev) return false;
    if(outline)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
        glDepthMask(GL_TRUE);
    }
    else if(editmode)
    {
        maskgbuffer("cnd");
    }
    else
    {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    gle::disablevertex();
    gle::clearvbo();
    gle::clearebo();
    return true;
}

struct decalrenderer
{
    GLuint vbuf;
    vec4 colorscale;
    int globals, tmu;
    GLuint textures[7];
    DecalSlot *slot;

    decalrenderer() : vbuf(0), colorscale(1, 1, 1, 1), globals(-1), tmu(-1), slot(NULL)
    {
        loopi(7) textures[i] = 0;
    }
};

struct decalbatch
{
    const elementset &es;
    DecalSlot &slot;
    int offset;
    vtxarray *va;
    int next, batch;

    decalbatch(const elementset &es, int offset, vtxarray *va)
      : es(es), slot(lookupdecalslot(es.texture)), offset(offset), va(va),
        next(-1), batch(-1)
    {}

    int compare(const decalbatch &b) const
    {
        if(va->vbuf < b.va->vbuf) return -1;
        if(va->vbuf > b.va->vbuf) return 1;
        if(slot.shader < b.slot.shader) return -1;
        if(slot.shader > b.slot.shader) return 1;
        if(es.texture < b.es.texture) return -1;
        if(es.texture > b.es.texture) return 1;
        if(es.envmap < b.es.envmap) return -1;
        if(es.envmap > b.es.envmap) return 1;
        if(slot.Slot::params.length() < b.slot.Slot::params.length()) return -1;
        if(slot.Slot::params.length() > b.slot.Slot::params.length()) return 1;
        if(es.reuse < b.es.reuse) return -1;
        if(es.reuse > b.es.reuse) return 1;
        return sortentcolor(es.entid, b.es.entid);
    }
};

static vector<decalbatch> decalbatches;

static void mergedecals(decalrenderer &cur, vtxarray *va)
{
    elementset *texs = va->decalelems;
    int numtexs = va->decaltexs, offset = 0;

    if(firstbatch < 0)
    {
        firstbatch = decalbatches.length();
        numbatches = numtexs;
        loopi(numtexs-1)
        {
            decalbatches.add(decalbatch(texs[i], offset, va)).next = i+1;
            offset += texs[i].length;
        }
        decalbatches.add(decalbatch(texs[numtexs-1], offset, va));
        return;
    }

    int prevbatch = -1, curbatch = firstbatch, curtex = 0;
    do
    {
        decalbatch &b = decalbatches.add(decalbatch(texs[curtex], offset, va));
        offset += texs[curtex].length;
        int dir = -1;
        while(curbatch >= 0)
        {
            dir = b.compare(decalbatches[curbatch]);
            if(dir <= 0) break;
            prevbatch = curbatch;
            curbatch = decalbatches[curbatch].next;
        }
        if(!dir)
        {
            int last = curbatch, next;
            for(;;)
            {
                next = decalbatches[last].batch;
                if(next < 0) break;
                last = next;
            }
            if(last==curbatch)
            {
                b.batch = curbatch;
                b.next = decalbatches[curbatch].next;
                if(prevbatch < 0) firstbatch = decalbatches.length()-1;
                else decalbatches[prevbatch].next = decalbatches.length()-1;
                curbatch = decalbatches.length()-1;
            }
            else
            {
                b.batch = next;
                decalbatches[last].batch = decalbatches.length()-1;
            }
        }
        else
        {
            numbatches++;
            b.next = curbatch;
            if(prevbatch < 0) firstbatch = decalbatches.length()-1;
            else decalbatches[prevbatch].next = decalbatches.length()-1;
            prevbatch = decalbatches.length()-1;
        }
    }
    while(++curtex < numtexs);
}

static void resetdecalbatches()
{
    decalbatches.setsize(0);
    firstbatch = -1;
    numbatches = 0;
}

static void changevbuf(decalrenderer &cur, int pass, vtxarray *va)
{
    gle::bindvbo(va->vbuf);
    gle::bindebo(va->decalbuf);
    cur.vbuf = va->vbuf;

    vertex *vdata = (vertex *)0;
    gle::vertexpointer(sizeof(vertex), vdata->pos.v);
    gle::normalpointer(sizeof(vertex), vdata->norm.v, GL_BYTE, 4);
    gle::texcoord0pointer(sizeof(vertex), vdata->tc.v, GL_FLOAT, 3);
    gle::tangentpointer(sizeof(vertex), vdata->tangent.v, GL_BYTE);
}

static void changebatchtmus(decalrenderer &cur, int pass, decalbatch &b)
{
    if(b.slot.shader && b.slot.shader->type&SHADER_ENVMAP && b.es.envmap!=EMID_CUSTOM)
    {
        GLuint emtex = lookupenvmap(b.es.envmap);
        if(cur.textures[TEX_ENVMAP]!=emtex)
        {
            cur.tmu = TEX_ENVMAP;
            glActiveTexture_(GL_TEXTURE0 + TEX_ENVMAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cur.textures[TEX_ENVMAP] = emtex);
        }
    }
    if(cur.tmu != 0)
    {
        cur.tmu = 0;
        glActiveTexture_(GL_TEXTURE0);
    }
}

static inline void bindslottex(decalrenderer &cur, int type, Texture *tex, GLenum target = GL_TEXTURE_2D)
{
    if(cur.textures[type] != tex->id)
    {
        if(cur.tmu != type)
        {
            cur.tmu = type;
            glActiveTexture_(GL_TEXTURE0 + type);
        }
        glBindTexture(target, cur.textures[type] = tex->id);
    }
}

static void updateslotcolor(decalrenderer &cur, int pass, DecalSlot &slot, ushort entid)
{
    vec4 colorscale = vec4(slot.getcolorscale(), 1);
    if(entid != USHRT_MAX)
    {
        const vector<extentity *> &ents = entities::getents();
        if(ents.inrange(entid) && ents[entid]->type == ET_DECAL)
        {
            if(ents[entid]->attrs[5] > 0 && ents[entid]->attrs[5] < 100) colorscale.w *= ents[entid]->attrs[5]/100.f;
            if(ents[entid]->attrs[6] > 0) colorscale.mul(vec::fromcolor(ents[entid]->attrs[6]));
            if(ents[entid]->attrs[7] || ents[entid]->attrs[8]) colorscale.mul(game::getpalette(ents[entid]->attrs[7], ents[entid]->attrs[8]));
        }
    }
    if(cur.colorscale != colorscale)
    {
        cur.colorscale = colorscale;
        GLOBALPARAMF(colorparams, colorscale.x, colorscale.y, colorscale.z, colorscale.w);
    }
}

static void changeslottmus(decalrenderer &cur, int pass, DecalSlot &slot)
{
    Texture *diffuse = slot.sts.empty() ? notexture : slot.sts[0].t;
    bindslottex(cur, TEX_DIFFUSE, diffuse);

    loopv(slot.sts)
    {
        Slot::Tex &t = slot.sts[i];
        switch(t.type)
        {
            case TEX_ENVMAP:
                if(t.t) bindslottex(cur, t.type, t.t, GL_TEXTURE_CUBE_MAP);
                break;
            case TEX_NORMAL:
            case TEX_GLOW:
                bindslottex(cur, t.type, t.t);
                break;
            case TEX_SPEC:
                if(t.combined < 0) bindslottex(cur, TEX_GLOW, t.t);
                break;
        }
    }
    if(cur.tmu != 0)
    {
        cur.tmu = 0;
        glActiveTexture_(GL_TEXTURE0);
    }

    cur.slot = &slot;
}

static inline void changeshader(decalrenderer &cur, int pass, decalbatch &b)
{
    DecalSlot &slot = b.slot;
    if(b.es.reuse)
    {
        VSlot &reuse = lookupvslot(b.es.reuse);
        if(pass) slot.shader->setvariant(0, 0, slot, reuse);
        else slot.shader->set(slot, reuse);
    }
    else if(pass) slot.shader->setvariant(0, 0, slot);
    else slot.shader->set(slot);
    cur.globals = GlobalShaderParamState::nextversion;
}

static void renderdecalbatch(decalrenderer &cur, int pass, decalbatch &b)
{
    gbatches++;
    for(decalbatch *curbatch = &b;; curbatch = &decalbatches[curbatch->batch])
    {
        ushort len = curbatch->es.length;
        if(len)
        {
            drawtris(len, (ushort *)0 + curbatch->va->decaloffset + curbatch->offset, curbatch->es.minvert, curbatch->es.maxvert);
            vtris += len/3;
        }
        if(curbatch->batch < 0) break;
    }
}

static void renderdecalbatches(decalrenderer &cur, int pass)
{
    cur.slot = NULL;
    int curbatch = firstbatch;
    while(curbatch >= 0)
    {
        decalbatch &b = decalbatches[curbatch];
        curbatch = b.next;

        if(pass && !b.slot.shader->numvariants(0)) continue;

        if(cur.vbuf != b.va->vbuf) changevbuf(cur, pass, b.va);
        changebatchtmus(cur, pass, b);
        if(cur.slot != &b.slot)
        {
            changeslottmus(cur, pass, b.slot);
            updateslotcolor(cur, pass, b.slot, b.es.entid);
            changeshader(cur, pass, b);
        }
        else
        {
            updateslotcolor(cur, pass, b.slot, b.es.entid);
            updateshader(cur);
        }

        renderdecalbatch(cur, pass, b);
    }

    resetdecalbatches();
}

void setupdecals(decalrenderer &cur)
{
    gle::enablevertex();
    gle::enablenormal();
    gle::enabletexcoord0();
    gle::enabletangent();

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    enablepolygonoffset(GL_POLYGON_OFFSET_FILL);

    GLOBALPARAMF(colorparams, 1, 1, 1, 1);
}

void cleanupdecals(decalrenderer &cur)
{
    disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    maskgbuffer("cnd");

    gle::disablevertex();
    gle::disablenormal();
    gle::disabletexcoord0();
    gle::disabletangent();

    gle::clearvbo();
    gle::clearebo();
}

VAR(0, batchdecals, 0, 1, 1);

void renderdecals()
{
    vtxarray *decalva;
    for(decalva = visibleva; decalva; decalva = decalva->next) if(decalva->decaltris && decalva->occluded < OCCLUDE_BB) break;
    if(!decalva) return;

    decalrenderer cur;

    setupdecals(cur);
    resetdecalbatches();

    if(maxdualdrawbufs)
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC1_ALPHA);
        maskgbuffer("c");
        for(vtxarray *va = decalva; va; va = va->next) if(va->decaltris && va->occluded < OCCLUDE_BB)
        {
            mergedecals(cur, va);
            if(!batchdecals && decalbatches.length()) renderdecalbatches(cur, 0);
        }
        if(decalbatches.length()) renderdecalbatches(cur, 0);

        if(usepacknorm())
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        }
        else glBlendFunc(GL_SRC1_ALPHA, GL_ONE_MINUS_SRC1_ALPHA);
        maskgbuffer("n");
        cur.vbuf = 0;
        for(vtxarray *va = decalva; va; va = va->next) if(va->decaltris && va->occluded < OCCLUDE_BB)
        {
            mergedecals(cur, va);
            if(!batchdecals && decalbatches.length()) renderdecalbatches(cur, 1);
        }
        if(decalbatches.length()) renderdecalbatches(cur, 1);
    }
    else
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        maskgbuffer("cn");
        for(vtxarray *va = decalva; va; va = va->next) if(va->decaltris && va->occluded < OCCLUDE_BB)
        {
            mergedecals(cur, va);
            if(!batchdecals && decalbatches.length()) renderdecalbatches(cur, 0);
        }
        if(decalbatches.length()) renderdecalbatches(cur, 0);
    }

    cleanupdecals(cur);
}

struct shadowmesh
{
    vec origin;
    float radius;
    vec spotloc;
    int spotangle;
    int type;
    int draws[6];
};

struct shadowdraw
{
    GLuint ebuf, vbuf;
    int offset, tris, next;
    ushort minvert, maxvert;
};

struct shadowverts
{
    static const int SIZE = 1<<13;
    int table[SIZE];
    vector<vec> verts;
    vector<int> chain;

    shadowverts() { clear(); }

    void clear()
    {
        memset(table, -1, sizeof(table));
        chain.setsize(0);
        verts.setsize(0);
    }

    int add(const vec &v)
    {
        uint h = hthash(v)&(SIZE-1);
        for(int i = table[h]; i>=0; i = chain[i]) if(verts[i] == v) return i;
        if(verts.length() >= USHRT_MAX) return -1;
        verts.add(v);
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    }
} shadowverts;
vector<ushort> shadowtris[6];
vector<GLuint> shadowvbos;
hashtable<int, shadowmesh> shadowmeshes;
vector<shadowdraw> shadowdraws;

struct shadowdrawinfo
{
    int last;
    ushort minvert, maxvert;

    shadowdrawinfo() : last(-1) { reset(); }

    void reset() { minvert = USHRT_MAX; maxvert = 0; }
};

static void flushshadowmeshdraws(shadowmesh &m, int sides, shadowdrawinfo draws[6])
{
    int numindexes = 0;
    loopi(sides) numindexes += shadowtris[i].length();
    if(!numindexes) return;

    GLuint ebuf = 0, vbuf = 0;
    glGenBuffers_(1, &ebuf);
    glGenBuffers_(1, &vbuf);
    ushort *indexes = new ushort[numindexes];
    int offset = 0;
    loopi(sides) if(shadowtris[i].length())
    {
        if(draws[i].last < 0) m.draws[i] = shadowdraws.length();
        else shadowdraws[draws[i].last].next = shadowdraws.length();
        draws[i].last = shadowdraws.length();

        shadowdraw &d = shadowdraws.add();
        d.ebuf = ebuf;
        d.vbuf = vbuf;
        d.offset = offset;
        d.tris = shadowtris[i].length()/3;
        d.minvert = draws[i].minvert;
        d.maxvert = draws[i].maxvert;
        d.next = -1;

        memcpy(indexes + offset, shadowtris[i].getbuf(), shadowtris[i].length()*sizeof(ushort));
        offset += shadowtris[i].length();

        shadowtris[i].setsize(0);
        draws[i].reset();
    }

    gle::bindebo(ebuf);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, numindexes*sizeof(ushort), indexes, GL_STATIC_DRAW);
    gle::clearebo();
    delete[] indexes;

    gle::bindvbo(vbuf);
    glBufferData_(GL_ARRAY_BUFFER, shadowverts.verts.length()*sizeof(vec), shadowverts.verts.getbuf(), GL_STATIC_DRAW);
    gle::clearvbo();
    shadowverts.clear();

    shadowvbos.add(ebuf);
    shadowvbos.add(vbuf);
}

static inline void addshadowmeshtri(shadowmesh &m, int sides, shadowdrawinfo draws[6], const vec &v0, const vec &v1, const vec &v2)
{
    extern int smcullside;
    vec l0 = vec(v0).sub(shadoworigin);
    float side = l0.scalartriple(vec(v1).sub(v0), vec(v2).sub(v0));
    if(smcullside ? side > 0 : side < 0) return;
    vec l1 = vec(v1).sub(shadoworigin), l2 = vec(v2).sub(shadoworigin);
    if(l0.squaredlen() > shadowradius*shadowradius && l1.squaredlen() > shadowradius*shadowradius && l2.squaredlen() > shadowradius*shadowradius)
        return;
    int sidemask = 0;
    switch(m.type)
    {
        case SM_SPOT: sidemask = bbinsidespot(shadoworigin, shadowdir, shadowspot, ivec(vec(v0).min(v1).min(v2)), ivec(vec(v0).max(v1).max(v2).add(1))) ? 1 : 0; break;
        case SM_CUBEMAP: sidemask = calctrisidemask(l0.div(shadowradius), l1.div(shadowradius), l2.div(shadowradius), shadowbias); break;
    }
    if(!sidemask) return;
    if(shadowverts.verts.length() + 3 >= USHRT_MAX) flushshadowmeshdraws(m, sides, draws);
    int i0 = shadowverts.add(v0), i1 = shadowverts.add(v1), i2 = shadowverts.add(v2);
    ushort minvert = min(i0, min(i1, i2)), maxvert = max(i0, max(i1, i2));
    loopk(sides) if(sidemask&(1<<k))
    {
        shadowdrawinfo &d = draws[k];
        d.minvert = min(d.minvert, minvert);
        d.maxvert = max(d.maxvert, maxvert);
        shadowtris[k].add(i0);
        shadowtris[k].add(i1);
        shadowtris[k].add(i2);
    }
}

static void genshadowmeshtris(shadowmesh &m, int sides, shadowdrawinfo draws[6], ushort *edata, int numtris, vertex *vdata)
{
    for(int j = 0; j < 3*numtris; j += 3) addshadowmeshtri(m, sides, draws, vdata[edata[j]].pos, vdata[edata[j+1]].pos, vdata[edata[j+2]].pos);
}

static void genshadowmeshmapmodels(shadowmesh &m, int sides, shadowdrawinfo draws[6])
{
    const vector<extentity *> &ents = entities::getents();
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext) loopvk(oe->mapmodels)
    {
        extentity &e = *ents[oe->mapmodels[k]];
        if(e.flags&(EF_NOVIS|EF_NOSHADOW) || !mapmodelvisible(e)) continue;
        e.flags |= EF_RENDER;
    }
    vector<triangle> tris;
    for(octaentities *oe = shadowmms; oe; oe = oe->rnext) loopvj(oe->mapmodels)
    {
        extentity &e = *ents[oe->mapmodels[j]];
        if(!(e.flags&EF_RENDER)) continue;
        e.flags &= ~EF_RENDER;


        model *mm = loadmapmodel(e.attrs[0]);
        if(!mm || !mm->shadow || mm->animated() || (mm->alphashadow && mm->alphatested())) continue;

        matrix4x3 orient;
        orient.identity();
        if(e.attrs[1]) orient.rotate_around_z(sincosmod360(e.attrs[1]));
        if(e.attrs[2]) orient.rotate_around_x(sincosmod360(e.attrs[2]));
        if(e.attrs[3]) orient.rotate_around_y(sincosmod360(-e.attrs[3]));
        if(e.attrs[5] > 0) orient.scale(e.attrs[5]/100.0f);
        orient.settranslation(e.viewpos);
        tris.setsize(0);
        mm->genshadowmesh(tris, orient);

        loopv(tris)
        {
            triangle &t = tris[i];
            addshadowmeshtri(m, sides, draws, t.a, t.b, t.c);
        }

        e.flags |= EF_SHADOWMESH;
    }
}

static void genshadowmesh(int idx, extentity &e)
{
    shadowmesh m;
    m.type = calcshadowinfo(e, m.origin, m.radius, m.spotloc, m.spotangle, shadowbias);
    if(!m.type) return;
    memset(m.draws, -1, sizeof(m.draws));

    shadowmapping = m.type;
    shadoworigin = m.origin;
    shadowradius = m.radius;
    shadowdir = m.type == SM_SPOT ? vec(m.spotloc).sub(m.origin).normalize() : vec(0, 0, 0);
    shadowspot = m.spotangle;

    findshadowvas();
    findshadowmms();

    int sides = m.type == SM_SPOT ? 1 : 6;
    shadowdrawinfo draws[6];
    for(vtxarray *va = shadowva; va; va = va->rnext) if(va->shadowmask)
    {
        if(va->tris) genshadowmeshtris(m, sides, draws, va->edata + va->eoffset, va->tris, va->vdata);
        if(getskyshadow() && va->sky) genshadowmeshtris(m, sides, draws, va->skydata + va->skyoffset, va->sky/3, va->vdata);
    }
    if(shadowmms) genshadowmeshmapmodels(m, sides, draws);
    flushshadowmeshdraws(m, sides, draws);

    shadowmeshes[idx] = m;

    shadowmapping = 0;
}

void clearshadowmeshes()
{
    if(shadowvbos.length()) { glDeleteBuffers_(shadowvbos.length(), shadowvbos.getbuf()); shadowvbos.setsize(0); }
    if(shadowmeshes.numelems)
    {
        vector<extentity *> &ents = entities::getents();
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.flags&EF_SHADOWMESH) e.flags &= ~EF_SHADOWMESH;
        }
    }
    shadowmeshes.clear();
    shadowdraws.setsize(0);
}

VARF(0, smmesh, 0, 1, 1, { if(!smmesh) clearshadowmeshes(); });

void genshadowmeshes()
{
    clearshadowmeshes();
    if(!smmesh) return;

    progress(0, "Generating shadow meshes...");

    vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_LIGHT) continue;
        genshadowmesh(i, e);
    }
}

shadowmesh *findshadowmesh(int idx, extentity &e)
{
    shadowmesh *m = shadowmeshes.access(idx);
    if(!m || m->type != shadowmapping || m->origin != shadoworigin || m->radius < shadowradius) return NULL;
    switch(m->type)
    {
        case SM_SPOT:
        {
            int slight = -1;
            if(!getlightfx(e, NULL, &slight)) return NULL;
            const vector<extentity *> &ents = entities::getents();
            if(!ents.inrange(slight) || m->spotloc != ents[slight]->o || m->spotangle < clamp(int(ents[slight]->attrs[1]), 1, 89)) return NULL;
            break;
        }
    }
    return m;
}

void rendershadowmesh(shadowmesh *m)
{
    int draw = m->draws[shadowside];
    if(draw < 0) return;

    SETSHADER(shadowmapworld);

    gle::enablevertex();

    GLuint ebuf = 0, vbuf = 0;
    while(draw >= 0)
    {
        shadowdraw &d = shadowdraws[draw];
        if(ebuf != d.ebuf) { gle::bindebo(d.ebuf); ebuf = d.ebuf; }
        if(vbuf != d.vbuf) { gle::bindvbo(d.vbuf); vbuf = d.vbuf; gle::vertexpointer(sizeof(vec), 0); }
        drawtris(3*d.tris, (ushort *)0 + d.offset, d.minvert, d.maxvert);
        xtravertsva += 3*d.tris;
        draw = d.next;
    }

    gle::disablevertex();
    gle::clearebo();
    gle::clearvbo();
}

