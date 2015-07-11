// renderva.cpp: handles the occlusion and rendering of vertex arrays

#include "engine.h"

static inline void drawtris(GLsizei numindices, const GLvoid *indices, ushort minvert, ushort maxvert)
{
    glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, numindices, GL_UNSIGNED_SHORT, indices);
    glde++;
}

static inline void drawvatris(vtxarray *va, GLsizei numindices, const GLvoid *indices)
{
    drawtris(numindices, indices, va->minvert, va->maxvert);
}

///////// view frustrum culling ///////////////////////

plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
float vfcDfog;  // far plane culling distance (fog limit).
float vfcDnear[5], vfcDfar[5];

vtxarray *visibleva;

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
    if(dist > rad) return VFC_FOGGED;  //VFC_NOT_VISIBLE;   // culling when fog is closer than size of world results in HOM
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

float vadist(vtxarray *va, const vec &p)
{
    return p.dist_to_bb(va->bbmin, va->bbmax);
}

#define VASORTSIZE 64

static vtxarray *vasort[VASORTSIZE];

void addvisibleva(vtxarray *va)
{
    float dist = vadist(va, camera1->o);
    va->distance = int(dist); /*cv.dist(camera1->o) - va->size*SQRT3/2*/

    int hash = min(int(dist*VASORTSIZE/hdr.worldsize), VASORTSIZE-1);
    vtxarray **prev = &vasort[hash], *cur = vasort[hash];

    while(cur && va->distance >= cur->distance)
    {
        prev = &cur->next;
        cur = cur->next;
    }

    va->next = *prev;
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

void findvisiblevas(vector<vtxarray *> &vas, bool resetocclude = false)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        int prevvfc = resetocclude ? VFC_NOT_VISIBLE : v.curvfc;
        v.curvfc = isvisiblecube(v.o, v.size);
        if(v.curvfc!=VFC_NOT_VISIBLE)
        {
            if(pvsoccluded(v.o, v.size))
            {
                v.curvfc += PVS_FULL_VISIBLE - VFC_FULL_VISIBLE;
                continue;
            }
            addvisibleva(&v);
            if(v.children.length()) findvisiblevas(v.children, prevvfc>=VFC_NOT_VISIBLE);
            if(prevvfc>=VFC_NOT_VISIBLE)
            {
                v.occluded = !v.texs ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
                v.query = NULL;
            }
        }
    }
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

void setvfcP(float z, const vec &bbmin, const vec &bbmax)
{
    vec4 px = mvpmatrix.getrow(0), py = mvpmatrix.getrow(1), pz = mvpmatrix.getrow(2), pw = mvpmatrix.getrow(3);
    vfcP[0] = plane(vec4(pw).mul(-bbmin.x).add(px)).normalize(); // left plane
    vfcP[1] = plane(vec4(pw).mul(bbmax.x).sub(px)).normalize(); // right plane
    vfcP[2] = plane(vec4(pw).mul(-bbmin.y).add(py)).normalize(); // bottom plane
    vfcP[3] = plane(vec4(pw).mul(bbmax.y).sub(py)).normalize(); // top plane
    vfcP[4] = plane(vec4(pw).add(pz)).normalize(); // near/far planes
    if(z >= 0) loopi(5) vfcP[i].reflectz(z);

    extern int fog;
    vfcDfog = fog;
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

extern vector<vtxarray *> varoot, valist;

void visiblecubes(bool cull)
{
    memset(vasort, 0, sizeof(vasort));

    if(cull)
    {
        setvfcP();
        findvisiblevas(varoot);
        sortvisiblevas();
    }
    else
    {
        memset(vfcP, 0, sizeof(vfcP));
        vfcDfog = 1000000;
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

static inline bool insideva(const vtxarray *va, const vec &v, int margin = 2)
{
    int size = va->size + margin;
    return v.x>=va->o.x-margin && v.y>=va->o.y-margin && v.z>=va->o.z-margin &&
           v.x<=va->o.x+size && v.y<=va->o.y+size && v.z<=va->o.z+size;
}

///////// occlusion queries /////////////

#define MAXQUERY 2048
#define MAXQUERYFRAMES 2

struct queryframe
{
    int cur, max;
    occludequery queries[MAXQUERY];

    queryframe() : cur(0), max(0) {}

    void flip() { loopi(cur) queries[i].owner = NULL; cur = 0; }

    occludequery *newquery(void *owner)
    {
        if(cur >= max)
        {
            if(max >= MAXQUERY) return NULL;
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
        cur = max = 0;
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

VAR(0, oqfrags, 0, 8, 64);
VAR(0, oqwait, 0, 1, 1);

bool checkquery(occludequery *query, bool nowait)
{
    GLuint fragments;
    if(query->fragments >= 0) fragments = query->fragments;
    else
    {
        if(nowait || !oqwait)
        {
            GLint avail;
            glGetQueryObjectiv_(query->id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if(!avail) return false;
        }
        glGetQueryObjectuiv_(query->id, GL_QUERY_RESULT, &fragments);
        query->fragments = fragments;
    }
    return fragments < uint(oqfrags);
}

void drawbb(const ivec &bo, const ivec &br, const vec &camera)
{
    glBegin(GL_QUADS);

    #define GENFACEORIENT(orient, v0, v1, v2, v3) do { \
        int dim = dimension(orient); \
        if(dimcoord(orient)) \
        { \
            if(camera[dim] < bo[dim] + br[dim]) continue; \
        } \
        else if(camera[dim] > bo[dim]) continue; \
        v0 v1 v2 v3 \
        xtraverts += 4; \
    } while(0);
    #define GENFACEVERT(orient, vert, ox,oy,oz, rx,ry,rz) \
        glVertex3f(ox rx, oy ry, oz rz);
    GENFACEVERTS(bo.x, bo.x + br.x, bo.y, bo.y + br.y, bo.z, bo.z + br.z, , , , , , )
    #undef GENFACEORIENT
    #undef GENFACEVERTS

    glEnd();
}

extern int octaentsize;

static octaentities *visiblemms, **lastvisiblemms;

static inline bool insideoe(const octaentities *oe, const vec &v, int margin = 1)
{
    return v.x>=oe->bbmin.x-margin && v.y>=oe->bbmin.y-margin && v.z>=oe->bbmin.z-margin &&
           v.x<=oe->bbmax.x+margin && v.y<=oe->bbmax.y+margin && v.z<=oe->bbmax.z+margin;
}

void findvisiblemms(const vector<extentity *> &ents)
{
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->mapmodels.empty() || va->curvfc >= VFC_FOGGED || va->occluded >= OCCLUDE_BB) continue;
        loopv(va->mapmodels)
        {
            octaentities *oe = va->mapmodels[i];
            if(isfoggedcube(oe->o, oe->size) || pvsoccluded(oe->bbmin, oe->bbmax)) continue;

            bool occluded = oe->query && oe->query->owner == oe && checkquery(oe->query);
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
                    if(e.lastemit && e.spawned() && e.attrs[6]&MMT_HIDE) continue;
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
}

VAR(0, oqmm, 0, 4, 8);

VAR(0, mmanimoverride, -1, 0, ANIM_ALL);

void rendermapmodel(extentity &e)
{
    int anim = ANIM_MAPMODEL|ANIM_LOOP, basetime = 0, flags = MDL_CULL_VFC|MDL_CULL_DIST|MDL_DYNLIGHT;
    if(e.lastemit)
    {
        if(e.attrs[6]&MMT_HIDE && e.spawned()) return;
        anim = e.spawned() ? ANIM_TRIGGER_ON : ANIM_TRIGGER_OFF;
        if(e.lastemit > 0 && lastmillis-e.lastemit < entities::triggertime(e)) basetime = e.lastemit;
        else anim |= ANIM_END;
    }
    if(e.attrs[6]&MMT_NOSHADOW && !(e.attrs[6]&MMT_NODYNSHADOW)) flags |= e.lastemit ? MDL_DYNSHADOW : MDL_SHADOW;
    if(mmanimoverride)
    {
        anim = (mmanimoverride<0 ? ANIM_ALL : mmanimoverride)|ANIM_LOOP;
        basetime = 0;
    }
    mapmodelinfo *mmi = getmminfo(e.attrs[0]);
    if(mmi)
    {
        if(e.attrs[8] || e.attrs[9])
        {
            vec r = game::getpalette(e.attrs[8], e.attrs[9]);
            if(e.attrs[7]) r.mul(vec::hexcolor(e.attrs[7]));
            e.light.material[0] = bvec::fromcolor(r);
        }
        else e.light.material[0] = e.attrs[7] ? bvec(e.attrs[7]) : bvec(255, 255, 255);
        rendermodel(&e.light, mmi->name, anim, e.o, e.attrs[1], e.attrs[2], e.attrs[3], flags, NULL, NULL, basetime, 0, e.attrs[4] ? min(e.attrs[4]/100.f, 1.f) : 1.f, e.attrs[5] ? max(e.attrs[5]/100.f, 1e-3f) : 1.f);
    }
}

extern int reflectdist;

vtxarray *reflectedva;

void renderreflectedmapmodels()
{
    const vector<extentity *> &ents = entities::getents();

    octaentities *mms = visiblemms;
    if(reflecting)
    {
        octaentities **lastmms = &mms;
        for(vtxarray *va = reflectedva; va; va = va->rnext)
        {
            if(va->mapmodels.empty() || va->distance > reflectdist) continue;
            loopv(va->mapmodels)
            {
                octaentities *oe = va->mapmodels[i];
                *lastmms = oe;
                lastmms = &oe->rnext;
            }
        }
        *lastmms = NULL;
    }
    for(octaentities *oe = mms; oe; oe = reflecting ? oe->rnext : oe->next) if(reflecting || oe->distance >= 0)
    {
        if(reflecting || refracting>0 ? oe->bbmax.z <= reflectz : oe->bbmin.z >= reflectz) continue;
        if(isfoggedcube(oe->o, oe->size)) continue;
        loopv(oe->mapmodels)
        {
           extentity &e = *ents[oe->mapmodels[i]];
           if(e.flags&EF_RENDER || (e.lastemit && e.spawned() && e.attrs[6]&MMT_HIDE)) continue;
           e.flags |= EF_RENDER;
        }
    }
    if(mms)
    {
        startmodelbatches();
        for(octaentities *oe = mms; oe; oe = reflecting ? oe->rnext : oe->next)
        {
            loopv(oe->mapmodels)
            {
                extentity &e = *ents[oe->mapmodels[i]];
                if(!(e.flags&EF_RENDER)) continue;
                rendermapmodel(e);
                e.flags &= ~EF_RENDER;
            }
        }
        endmodelbatches();
    }
}

void rendermapmodels()
{
    const vector<extentity *> &ents = entities::getents();

    visiblemms = NULL;
    lastvisiblemms = &visiblemms;
    findvisiblemms(ents);

    static int skipoq = 0;
    bool doquery = oqfrags && oqmm;

    startmodelbatches();
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
            rendermapmodel(e);
            e.flags &= ~EF_RENDER;
        }
        if(rendered && oe->query) endmodelquery();
    }
    endmodelbatches();

    bool queried = false;
    for(octaentities *oe = visiblemms; oe; oe = oe->next) if(oe->distance<0)
    {
        oe->query = doquery && !insideoe(oe, camera1->o) ? newquery(oe) : NULL;
        if(!oe->query) continue;
        if(!queried)
        {
            glDepthMask(GL_FALSE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            nocolorshader->set();
            queried = true;
        }
        startquery(oe->query);
        drawbb(oe->bbmin, ivec(oe->bbmax).sub(oe->bbmin));
        endquery(oe->query);
    }
    if(queried)
    {
        glDepthMask(GL_TRUE);
        glColorMask(COLORMASK, fading ? GL_FALSE : GL_TRUE);
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
        ivec co(i, o.x, o.y, o.z, size);
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
VAR(IDF_HEX|IDF_PERSIST, outlinecolour, 0, 0, 0xFFFFFF);
VAR(0, dtoutline, 0, 1, 1);

void renderoutline()
{
    notextureshader->set();

    glEnableClientState(GL_VERTEX_ARRAY);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3ub((outlinecolour>>16)&0xFF, (outlinecolour>>8)&0xFF, outlinecolour&0xFF);

    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    if(!dtoutline) glDisable(GL_DEPTH_TEST);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(va->occluded >= OCCLUDE_BB) continue;
        if(!va->alphaback && !va->alphafront && (!va->texs || va->occluded >= OCCLUDE_GEOM)) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->ebuf);
            glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);
        }

        if(va->texs && va->occluded < OCCLUDE_GEOM)
        {
            drawvatris(va, 3*va->tris, va->edata);
            xtravertsva += va->verts;
        }
        if(va->alphaback || va->alphafront)
        {
            drawvatris(va, 3*(va->alphabacktris + va->alphafronttris), &va->edata[3*(va->tris + va->blendtris)]);
            xtravertsva += 3*(va->alphabacktris + va->alphafronttris);
        }

        prev = va;
    }

    if(!dtoutline) glEnable(GL_DEPTH_TEST);

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    defaultshader->set();
}

VAR(IDF_HEX, blendbrushcolor, 0, 0x0000C0, 0xFFFFFF);

void renderblendbrush(GLuint tex, float x, float y, float w, float h)
{
    SETSHADER(blendbrush);

    glEnableClientState(GL_VERTEX_ARRAY);

    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub((blendbrushcolor>>16)&0xFF, (blendbrushcolor>>8)&0xFF, blendbrushcolor&0xFF, 0x40);

    LOCALPARAMF(texgenS, 1.0f/w, 0, 0, -x/w);
    LOCALPARAMF(texgenT, 0, 1.0f/h, 0, -y/h);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->texs || va->occluded >= OCCLUDE_GEOM) continue;
        if(va->o.x + va->size <= x || va->o.y + va->size <= y || va->o.x >= x + w || va->o.y >= y + h) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->ebuf);
            glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);
        }

        drawvatris(va, 3*va->tris, va->edata);
        xtravertsva += va->verts;

        prev = va;
    }

    glDisable(GL_BLEND);

    glDepthFunc(GL_LESS);

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    notextureshader->set();
}

void rendershadowmapreceivers()
{
    SETSHADER(shadowmapreceiver);

    glEnableClientState(GL_VERTEX_ARRAY);

    glCullFace(GL_FRONT);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_GREATER);

    extern int ati_minmax_bug;
    if(!ati_minmax_bug) glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

    glEnable(GL_BLEND);
    glBlendEquation_(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->texs || va->curvfc >= VFC_FOGGED || !isshadowmapreceiver(va)) continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->ebuf);
            glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);
        }

        drawvatris(va, 3*va->tris, va->edata);
        xtravertsva += va->verts;

        prev = va;
    }

    glDisable(GL_BLEND);
    glBlendEquation_(GL_FUNC_ADD);

    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    if(!ati_minmax_bug) glColorMask(COLORMASK, GL_TRUE);

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderdepthobstacles(const vec &bbmin, const vec &bbmax, float scale, float *ranges, int numranges)
{
    float scales[4] = { 0, 0, 0, 0 }, offsets[4] = { 0, 0, 0, 0 };
    if(numranges < 0)
    {
        SETSHADER(depthfxsplitworld);

        loopi(-numranges)
        {
            if(!i) scales[i] = 1.0f/scale;
            else scales[i] = scales[i-1]*256;
        }
    }
    else
    {
        SETSHADER(depthfxworld);

        if(!numranges) loopi(4) scales[i] = 1.0f/scale;
        else loopi(numranges)
        {
            scales[i] = 1.0f/scale;
            offsets[i] = -ranges[i]/scale;
        }
    }
    LOCALPARAMF(depthscale, scales[0], scales[1], scales[2], scales[3]);
    LOCALPARAMF(depthoffsets, offsets[0], offsets[1], offsets[2], offsets[3]);

    glEnableClientState(GL_VERTEX_ARRAY);

    vtxarray *prev = NULL;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->texs || va->occluded >= OCCLUDE_GEOM ||
           va->o.x > bbmax.x || va->o.y > bbmax.y || va->o.z > bbmax.z ||
           va->o.x + va->size < bbmin.x || va->o.y + va->size < bbmin.y || va->o.z + va->size < bbmin.z)
           continue;

        if(!prev || va->vbuf != prev->vbuf)
        {
            glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->ebuf);
            glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);
        }

        drawvatris(va, 3*va->tris, va->edata);
        xtravertsva += va->verts;
        if(va->alphabacktris + va->alphafronttris > 0)
        {
            drawvatris(va, 3*(va->alphabacktris + va->alphafronttris), va->edata + 3*(va->tris + va->blendtris));
            xtravertsva += 3*(va->alphabacktris + va->alphafronttris);
        }

        prev = va;
    }

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    defaultshader->set();
}

VAR(0, oqdist, 0, 256, 1024);
VAR(0, zpass, 0, 1, 1);
VAR(0, glowpass, 0, 1, 1);
VAR(0, envpass, 0, 1, 1);

struct renderstate
{
    bool colormask, depthmask, blending;
    int alphaing;
    GLuint vbuf;
    GLfloat color[4], fogcolor[4];
    vec colorscale, lightcolor;
    float alphascale;
    GLuint textures[8];
    Slot *slot, *texgenslot;
    VSlot *vslot, *texgenvslot;
    vec2 texgenscroll;
    int texgendim;
    int visibledynlights;
    uint dynlightmask;

    renderstate() : colormask(true), depthmask(true), blending(false), alphaing(0), vbuf(0), colorscale(1, 1, 1), alphascale(0), slot(NULL), texgenslot(NULL), vslot(NULL), texgenvslot(NULL), texgenscroll(0, 0), texgendim(-1), visibledynlights(0), dynlightmask(0)
    {
        loopk(4) color[k] = 1;
        loopk(8) textures[k] = 0;
    }
};

void renderquery(renderstate &cur, occludequery *query, vtxarray *va, bool full = true)
{
    nocolorshader->set();
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }
    if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }

    vec camera(camera1->o);
    if(reflecting) camera.z = reflectz;

    startquery(query);

    if(full) drawbb(ivec(va->bbmin).sub(1), ivec(va->bbmax).sub(va->bbmin).add(2), camera);
    else drawbb(va->geommin, ivec(va->geommax).sub(va->geommin), camera);

    endquery(query);

    extern int intel_immediate_bug;
    if(intel_immediate_bug && cur.vbuf) cur.vbuf = 0;
}

enum
{
    RENDERPASS_LIGHTMAP = 0,
    RENDERPASS_Z,
    RENDERPASS_CAUSTICS,
    RENDERPASS_FOG,
    RENDERPASS_LIGHTMAP_BLEND
};

struct geombatch
{
    const elementset &es;
    VSlot &vslot;
    ushort *edata;
    vtxarray *va;
    int next, batch;

    geombatch(const elementset &es, ushort *edata, vtxarray *va)
      : es(es), vslot(lookupvslot(es.texture)), edata(edata), va(va),
        next(-1), batch(-1)
    {}

    int compare(const geombatch &b) const
    {
        if(va->vbuf < b.va->vbuf) return -1;
        if(va->vbuf > b.va->vbuf) return 1;
        if(va->dynlightmask < b.va->dynlightmask) return -1;
        if(va->dynlightmask > b.va->dynlightmask) return 1;
        if(vslot.slot->shader < b.vslot.slot->shader) return -1;
        if(vslot.slot->shader > b.vslot.slot->shader) return 1;
        if(vslot.slot->params.length() < b.vslot.slot->params.length()) return -1;
        if(vslot.slot->params.length() > b.vslot.slot->params.length()) return 1;
        if(es.texture < b.es.texture) return -1;
        if(es.texture > b.es.texture) return 1;
        if(es.lmid < b.es.lmid) return -1;
        if(es.lmid > b.es.lmid) return 1;
        if(es.envmap < b.es.envmap) return -1;
        if(es.envmap > b.es.envmap) return 1;
        if(es.dim < b.es.dim) return -1;
        if(es.dim > b.es.dim) return 1;
        return 0;
    }
};

static vector<geombatch> geombatches;
static int firstbatch = -1, numbatches = 0;

static void mergetexs(renderstate &cur, vtxarray *va, elementset *texs = NULL, int numtexs = 0, ushort *edata = NULL)
{
    if(!texs)
    {
        texs = va->eslist;
        numtexs = va->texs;
        edata = va->edata;
        if(cur.alphaing)
        {
            texs += va->texs + va->blends;
            edata += 3*(va->tris + va->blendtris);
            numtexs = va->alphaback;
            if(cur.alphaing > 1) numtexs += va->alphafront;
        }
    }

    if(firstbatch < 0)
    {
        firstbatch = geombatches.length();
        numbatches = numtexs;
        loopi(numtexs-1)
        {
            geombatches.add(geombatch(texs[i], edata, va)).next = i+1;
            edata += texs[i].length[1];
        }
        geombatches.add(geombatch(texs[numtexs-1], edata, va));
        return;
    }

    int prevbatch = -1, curbatch = firstbatch, curtex = 0;
    do
    {
        geombatch &b = geombatches.add(geombatch(texs[curtex], edata, va));
        edata += texs[curtex].length[1];
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

static void changevbuf(renderstate &cur, int pass, vtxarray *va)
{
    glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->ebuf);
    cur.vbuf = va->vbuf;

    glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);

    if(pass==RENDERPASS_LIGHTMAP)
    {
        glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), va->vdata[0].tc.v);
        glClientActiveTexture_(GL_TEXTURE1);
        glTexCoordPointer(2, GL_SHORT, sizeof(vertex), va->vdata[0].lm.v);
        glClientActiveTexture_(GL_TEXTURE0);
        glNormalPointer(GL_BYTE, sizeof(vertex), va->vdata[0].norm.v);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), va->vdata[0].tangent.v);
    }
}

static void changebatchtmus(renderstate &cur, int pass, geombatch &b)
{
    bool changed = false;
    extern bool brightengeom;
    int lmid = brightengeom && (b.es.lmid < LMID_RESERVED || (fullbright && editmode)) ? LMID_BRIGHT : b.es.lmid;
    if(cur.textures[1]!=lightmaptexs[lmid].id)
    {
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cur.textures[1] = lightmaptexs[lmid].id);
        changed = true;
    }
    int tmu = 2;
    if(b.vslot.slot->shader->type&SHADER_NORMALSLMS)
    {
        if(cur.textures[tmu]!=lightmaptexs[lmid+1].id)
        {
            glActiveTexture_(GL_TEXTURE0+tmu);
            glBindTexture(GL_TEXTURE_2D, cur.textures[tmu] = lightmaptexs[lmid+1].id);
            changed = true;
        }
        tmu++;
    }
    if(b.vslot.slot->shader->type&SHADER_ENVMAP && b.es.envmap!=EMID_CUSTOM)
    {
        GLuint emtex = lookupenvmap(b.es.envmap);
        if(cur.textures[tmu]!=emtex)
        {
            glActiveTexture_(GL_TEXTURE0+tmu);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cur.textures[tmu] = emtex);
            changed = true;
        }
    }
    if(changed) glActiveTexture_(GL_TEXTURE0);

    if(cur.dynlightmask != b.va->dynlightmask)
    {
        cur.visibledynlights = setdynlights(b.va);
        cur.dynlightmask = b.va->dynlightmask;
    }
}

VAR(0, blankgeom, 0, 0, 1);
vec lightmapcolor(0, 0, 0);
VARF(IDF_HEX, lightmapcolour, 0, 0, 0xFFFFFF, lightmapcolor = vec::hexcolor(lightmapcolour));
FVAR(0, lightmapintensity, 0, 1, 10);

static void changeslottmus(renderstate &cur, int pass, Slot &slot, VSlot &vslot)
{
    vec colorscale = vslot.getcolorscale();
    if(pass==RENDERPASS_LIGHTMAP)
    {
        GLuint diffusetex = blankgeom ? blanktexture->id : (slot.sts.empty() ? notexture->id : slot.sts[0].t->id);
        if(cur.textures[0]!=diffusetex)
            glBindTexture(GL_TEXTURE_2D, cur.textures[0] = diffusetex);

        if(lightmapcolor != vec(0, 0, 0)) colorscale = lightmapcolor;
        if(lightmapintensity != 1.f) colorscale.mul(lightmapintensity);
    }
    if(cur.alphaing)
    {
        float alpha = cur.alphaing > 1 ? vslot.alphafront : vslot.alphaback;
        if(cur.colorscale != colorscale || cur.alphascale != alpha)
        {
            cur.colorscale = colorscale;
            cur.alphascale = alpha;
            GLOBALPARAMF(colorparams, 2*alpha*colorscale.x, 2*alpha*colorscale.y, 2*alpha*colorscale.z, alpha);
            GLfloat fogc[4] = { alpha*cur.fogcolor[0], alpha*cur.fogcolor[1], alpha*cur.fogcolor[2], cur.fogcolor[3] };
            glFogfv(GL_FOG_COLOR, fogc);
        }
    }
    else if(cur.colorscale != colorscale)
    {
        cur.colorscale = colorscale;
        GLOBALPARAMF(colorparams, 2*colorscale.x, 2*colorscale.y, 2*colorscale.z, 1);
    }
    int tmu = 2, envmaptmu = -1;
    if(slot.shader->type&SHADER_NORMALSLMS) tmu++;
    if(slot.shader->type&SHADER_ENVMAP) envmaptmu = tmu++;
    loopvj(slot.sts)
    {
        Slot::Tex &t = slot.sts[j];
        if(t.type==TEX_DIFFUSE || t.combined>=0) continue;
        if(t.type==TEX_ENVMAP)
        {
            if(envmaptmu>=0 && t.t && cur.textures[envmaptmu]!=t.t->id)
            {
                glActiveTexture_(GL_TEXTURE0+envmaptmu);
                glBindTexture(GL_TEXTURE_CUBE_MAP, cur.textures[envmaptmu] = t.t->id);
            }
        }
        else
        {
            if(cur.textures[tmu]!=t.t->id)
            {
                glActiveTexture_(GL_TEXTURE0+tmu);
                glBindTexture(GL_TEXTURE_2D, cur.textures[tmu] = t.t->id);
            }
            if(++tmu >= 8) break;
        }
    }
    glActiveTexture_(GL_TEXTURE0);

    cur.slot = &slot;
    cur.vslot = &vslot;
}

static void changeshader(renderstate &cur, Shader *s, Slot &slot, VSlot &vslot, bool shadowed)
{
    if(glaring)
    {
        static Shader *noglareshader = NULL, *noglareblendshader = NULL, *noglarealphashader = NULL;
        Shader *fallback;
        if(cur.blending) { if(!noglareblendshader) noglareblendshader = lookupshaderbyname("noglareblendworld"); fallback = noglareblendshader; }
        else if(cur.alphaing) { if(!noglarealphashader) noglarealphashader = lookupshaderbyname("noglarealphaworld"); fallback = noglarealphashader; }
        else { if(!noglareshader) noglareshader = lookupshaderbyname("noglareworld"); fallback = noglareshader; }
        if(s->hasoption(4)) s->setvariant(cur.visibledynlights, 4, slot, vslot, fallback);
        else s->setvariant(cur.blending ? 1 : 0, 4, slot, vslot, fallback);
    }
    else if(fading && !cur.blending && !cur.alphaing)
    {
        if(shadowed) s->setvariant(cur.visibledynlights, 3, slot, vslot);
        else s->setvariant(cur.visibledynlights, 2, slot, vslot);
    }
    else if(shadowed) s->setvariant(cur.visibledynlights, 1, slot, vslot);
    else if(!cur.visibledynlights) s->set(slot, vslot);
    else s->setvariant(cur.visibledynlights-1, 0, slot, vslot);
}

static void changetexgen(renderstate &cur, int dim, Slot &slot, VSlot &vslot)
{
    if(cur.texgenslot != &slot || cur.texgenvslot != &vslot)
    {
        Texture *curtex = !cur.texgenslot || cur.texgenslot->sts.empty() ? notexture : cur.texgenslot->sts[0].t,
                *tex = slot.sts.empty() ? notexture : slot.sts[0].t;
        if(!cur.texgenvslot || slot.sts.empty() ||
            (curtex->xs != tex->xs || curtex->ys != tex->ys ||
             cur.texgenvslot->rotation != vslot.rotation || cur.texgenvslot->scale != vslot.scale ||
             cur.texgenvslot->offset != vslot.offset || cur.texgenvslot->scroll != vslot.scroll))
        {
            float xs = vslot.rotation>=2 && vslot.rotation<=4 ? -tex->xs : tex->xs,
                  ys = (vslot.rotation>=1 && vslot.rotation<=2) || vslot.rotation==5 ? -tex->ys : tex->ys;
            vec2 scroll(vslot.scroll);
            if((vslot.rotation&5)==1) swap(scroll.x, scroll.y);
            scroll.x *= lastmillis*tex->xs/xs;
            scroll.y *= lastmillis*tex->ys/ys;
            if(cur.texgenscroll != scroll)
            {
                cur.texgenscroll = scroll;
                cur.texgendim = -1;
            }
        }
        cur.texgenslot = &slot;
        cur.texgenvslot = &vslot;
    }

    if(cur.texgendim == dim) return;
    GLOBALPARAM(texgenscroll, cur.texgenscroll);
    cur.texgendim = dim;
}

static void renderbatch(renderstate &cur, int pass, geombatch &b)
{
    geombatch *shadowed = NULL;
    int rendered = -1;
    for(geombatch *curbatch = &b;; curbatch = &geombatches[curbatch->batch])
    {
        ushort len = curbatch->es.length[curbatch->va->shadowed ? 0 : 1];
        if(len)
        {
            if(rendered < 0)
            {
                changeshader(cur, b.vslot.slot->shader, *b.vslot.slot, b.vslot, false);
                rendered = 0;
                gbatches++;
            }
            ushort minvert = curbatch->es.minvert[0], maxvert = curbatch->es.maxvert[0];
            if(!curbatch->va->shadowed) { minvert = min(minvert, curbatch->es.minvert[1]); maxvert = max(maxvert, curbatch->es.maxvert[1]); }
            drawtris(len, curbatch->edata, minvert, maxvert);
            vtris += len/3;
        }
        if(curbatch->es.length[1] > len && !shadowed) shadowed = curbatch;
        if(curbatch->batch < 0) break;
    }
    if(shadowed) for(geombatch *curbatch = shadowed;; curbatch = &geombatches[curbatch->batch])
    {
        if(curbatch->va->shadowed && curbatch->es.length[1] > curbatch->es.length[0])
        {
            if(rendered < 1)
            {
                changeshader(cur, b.vslot.slot->shader, *b.vslot.slot, b.vslot, true);
                rendered = 1;
                gbatches++;
            }
            ushort len = curbatch->es.length[1] - curbatch->es.length[0];
            drawtris(len, curbatch->edata + curbatch->es.length[0], curbatch->es.minvert[1], curbatch->es.maxvert[1]);
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
        if(cur.alphaing)
        {
            if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }
        }
        else if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
        if(!cur.colormask) { cur.colormask = true; glColorMask(COLORMASK, cur.alphaing ? GL_FALSE : GL_TRUE); }
    }
    while(curbatch >= 0)
    {
        geombatch &b = geombatches[curbatch];
        curbatch = b.next;

        if(cur.vbuf != b.va->vbuf) changevbuf(cur, pass, b.va);
        if(cur.vslot != &b.vslot)
        {
            changeslottmus(cur, pass, *b.vslot.slot, b.vslot);
            if(cur.texgendim != b.es.dim || (cur.texgendim <= 2 && cur.texgenvslot != &b.vslot)) changetexgen(cur, b.es.dim, *b.vslot.slot, b.vslot);
        }
        else if(cur.texgendim != b.es.dim) changetexgen(cur, b.es.dim, *b.vslot.slot, b.vslot);
        if(pass == RENDERPASS_LIGHTMAP) changebatchtmus(cur, pass, b);

        renderbatch(cur, pass, b);
    }

    resetbatches();
}

void renderzpass(renderstate &cur, vtxarray *va)
{
    if(cur.vbuf!=va->vbuf) changevbuf(cur, RENDERPASS_Z, va);
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); }
    int firsttex = 0, numtris = va->tris;
    ushort *edata = va->edata;
    if(cur.alphaing)
    {
        firsttex += va->texs + va->blends;
        edata += 3*(va->tris + va->blendtris);
        numtris = va->alphabacktris + va->alphafronttris;
        xtravertsva += 3*numtris;
    }
    else xtravertsva += va->verts;
    nocolorshader->set();
    drawvatris(va, 3*numtris, edata);
}

vector<vtxarray *> foggedvas;

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

void renderfoggedvas(renderstate &cur, bool doquery = false)
{
    static Shader *fogshader = NULL;
    if(!fogshader) fogshader = lookupshaderbyname("fogworld");
    if(fading) fogshader->setvariant(0, 2);
    else fogshader->set();

    glColor3ubv(fogging ? refractcolor.v : fogcolor.v);

    loopv(foggedvas)
    {
        vtxarray *va = foggedvas[i];
        if(cur.vbuf!=va->vbuf) changevbuf(cur, RENDERPASS_FOG, va);

        if(doquery) startvaquery(va, );
        drawvatris(va, 3*va->tris, va->edata);
        vtris += va->tris;
        if(doquery) endvaquery(va, );
    }

    foggedvas.setsize(0);
}

VAR(0, batchgeom, 0, 1, 1);

void renderva(renderstate &cur, vtxarray *va, int pass = RENDERPASS_LIGHTMAP, bool fogpass = false, bool doquery = false)
{
    switch(pass)
    {
        case RENDERPASS_LIGHTMAP:
            if(!cur.alphaing) vverts += va->verts;
            va->shadowed = false;
            va->dynlightmask = 0;
            if(fogpass ? va->geommax.z<=reflectz-refractfog || !refractfog : va->curvfc==VFC_FOGGED)
            {
                if(!cur.alphaing && !cur.blending) foggedvas.add(va);
                break;
            }
            if(!envmapping && !glaring && !cur.alphaing)
            {
                va->shadowed = isshadowmapreceiver(va);
                calcdynlightmask(va);
            }
            if(doquery) startvaquery(va, { if(geombatches.length()) renderbatches(cur, pass); });
            mergetexs(cur, va);
            if(doquery) endvaquery(va, { if(geombatches.length()) renderbatches(cur, pass); });
            else if(!batchgeom && geombatches.length()) renderbatches(cur, pass);
            break;

        case RENDERPASS_LIGHTMAP_BLEND:
        {
            if(doquery) startvaquery(va, { if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP); });
            mergetexs(cur, va, &va->eslist[va->texs], va->blends, va->edata + 3*va->tris);
            if(doquery) endvaquery(va, { if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP); });
            else if(!batchgeom && geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);
            break;
        }

        case RENDERPASS_FOG:
            if(cur.vbuf!=va->vbuf) changevbuf(cur, pass, va);
            drawvatris(va, 3*va->tris, va->edata);
            xtravertsva += va->verts;
            break;

        case RENDERPASS_CAUSTICS:
            if(cur.vbuf!=va->vbuf) changevbuf(cur, pass, va);
            drawvatris(va, 3*va->tris, va->edata);
            xtravertsva += va->verts;
            break;

        case RENDERPASS_Z:
            if(doquery) startvaquery(va, );
            renderzpass(cur, va);
            if(doquery) endvaquery(va, );
            break;
    }
}

#define NUMCAUSTICS 32

static Texture *caustictex[NUMCAUSTICS] = { NULL };

void loadcaustics(bool force)
{
    static bool needcaustics = false;
    if(force) needcaustics = true;
    if(!caustics || !needcaustics) return;
    useshaderbyname("caustic");
    if(caustictex[0]) return;
    loopi(NUMCAUSTICS)
    {
        defformatstring(name)("<grey><mad:-0.6,0.6>caustics/caust%.2d.png", i);
        caustictex[i] = textureload(name);
    }
}

void cleanupva()
{
    clearvas(worldroot);
    clearqueries();
    loopi(NUMCAUSTICS) caustictex[i] = NULL;
}

VAR(IDF_WORLD, causticscale, 1, 50, 10000);
VAR(IDF_WORLD, causticmillis, 1, 75, 1000);
VARF(IDF_PERSIST, caustics, 0, 1, 1, loadcaustics());

void setupcaustics(int tmu, float blend, GLfloat *color = NULL)
{
    if(!caustictex[0]) loadcaustics(true);

    vec s = vec(0.011f, 0, 0.0066f).mul(100.0f/causticscale), t = vec(0, 0.011f, 0.0066f).mul(100.0f/causticscale);
    int tex = (lastmillis/causticmillis)%NUMCAUSTICS;
    float frac = float(lastmillis%causticmillis)/causticmillis;
    if(color) color[3] = frac;
    else glColor4f(1, 1, 1, frac);
    loopi(2)
    {
        glActiveTexture_(GL_TEXTURE0+tmu+i);
        glBindTexture(GL_TEXTURE_2D, caustictex[(tex+i)%NUMCAUSTICS]->id);
    }
    glActiveTexture_(GL_TEXTURE0);
    SETSHADER(caustic);
    LOCALPARAM(texgenS, s);
    LOCALPARAM(texgenT, t);
    LOCALPARAMF(frameoffset, blend*(1-frac), blend*frac, blend);
}

void setupgeom(renderstate &cur)
{
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    GLOBALPARAMF(colorparams, 2, 2, 2, 1);
    GLOBALPARAM(camera, camera1->o);
    GLOBALPARAMF(ambient, ambientcolor.x/255.0f, ambientcolor.y/255.0f, ambientcolor.z/255.0f);
    GLOBALPARAMF(millis, lastmillis/1000.0f);

    glColor4fv(cur.color);

    glActiveTexture_(GL_TEXTURE1);
    glClientActiveTexture_(GL_TEXTURE1);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glActiveTexture_(GL_TEXTURE0);
    glClientActiveTexture_(GL_TEXTURE0);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

void cleanupgeom(renderstate &cur)
{
    glActiveTexture_(GL_TEXTURE1);
    glClientActiveTexture_(GL_TEXTURE1);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glActiveTexture_(GL_TEXTURE0);
    glClientActiveTexture_(GL_TEXTURE0);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

#define FIRSTVA (reflecting ? reflectedva : visibleva)
#define NEXTVA (reflecting ? va->rnext : va->next)

static void rendergeommultipass(renderstate &cur, int pass, bool fogpass)
{
    cur.vbuf = 0;
    cur.texgendim = -1;
    for(vtxarray *va = FIRSTVA; va; va = NEXTVA)
    {
        if(!va->texs) continue;
        if(refracting)
        {
            if((refracting < 0 ? va->geommin.z > reflectz : va->geommax.z <= reflectz) || va->occluded >= OCCLUDE_GEOM) continue;
            if(ishiddencube(va->o, va->size)) continue;
        }
        else if(reflecting)
        {
            if(va->geommax.z <= reflectz) continue;
        }
        else if(va->occluded >= OCCLUDE_GEOM) continue;
        if(fogpass ? va->geommax.z <= reflectz-refractfog || !refractfog : va->curvfc==VFC_FOGGED) continue;
        renderva(cur, va, pass, fogpass);
    }
    if(geombatches.length()) renderbatches(cur, pass);
}

VAR(0, oqgeom, 0, 1, 1);

void rendergeom(float causticspass, bool fogpass)
{
    if(causticspass && !causticscale) causticspass = 0;

    bool mainpass = !reflecting && !refracting && !envmapping && !glaring,
         doOQ = oqfrags && oqgeom && mainpass,
         doZP = doOQ && zpass,
         doSM = shadowmap && !envmapping && !glaring;
    renderstate cur;
    if(mainpass)
    {
        flipqueries();
        vtris = vverts = 0;
    }
    if(!doZP)
    {
        if(shadowmap && hasFBO && mainpass) rendershadowmap();
        setupgeom(cur);
        if(doSM) pushshadowmap();
    }

    finddynlights();

    resetbatches();

    glEnableClientState(GL_VERTEX_ARRAY);

    int blends = 0;
    for(vtxarray *va = FIRSTVA; va; va = NEXTVA)
    {
        if(!va->texs) continue;
        if(refracting)
        {
            if((refracting < 0 ? va->geommin.z > reflectz : va->geommax.z <= reflectz) || va->occluded >= OCCLUDE_GEOM) continue;
            if(ishiddencube(va->o, va->size)) continue;
        }
        else if(reflecting)
        {
            if(va->geommax.z <= reflectz) continue;
        }
        else if(doOQ && (zpass || va->distance > oqdist) && !insideva(va, camera1->o))
        {
            if(va->parent && va->parent->occluded >= OCCLUDE_BB)
            {
                va->query = NULL;
                va->occluded = OCCLUDE_PARENT;
                continue;
            }
            va->occluded = va->query && va->query->owner == va && checkquery(va->query) ? min(va->occluded+1, int(OCCLUDE_BB)) : OCCLUDE_NOTHING;
            va->query = newquery(va);
            if((!va->query && zpass) || !va->occluded)
                va->occluded = pvsoccluded(va->geommin, va->geommax) ? OCCLUDE_GEOM : OCCLUDE_NOTHING;
            if(va->occluded >= OCCLUDE_GEOM)
            {
                if(va->query)
                {
                    if(!zpass && geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);
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

        if(!doZP) blends += va->blends;
        renderva(cur, va, doZP ? RENDERPASS_Z : RENDERPASS_LIGHTMAP, fogpass, doOQ);
    }

    if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);

    if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }

    bool multipassing = false;

    if(doZP)
    {
        glFlush();
        if(shadowmap && hasFBO && mainpass)
        {
			glDisableClientState(GL_VERTEX_ARRAY);
            glBindBuffer_(GL_ARRAY_BUFFER, 0);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
            rendershadowmap();
            glEnableClientState(GL_VERTEX_ARRAY);
        }
        setupgeom(cur);
        if(doSM) pushshadowmap();
        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        cur.vbuf = 0;
        cur.texgendim = -1;

        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(!va->texs || va->occluded >= OCCLUDE_GEOM) continue;
            blends += va->blends;
            renderva(cur, va, RENDERPASS_LIGHTMAP, fogpass);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(!va->texs || va->occluded < OCCLUDE_GEOM) continue;
            else if((va->parent && va->parent->occluded >= OCCLUDE_BB) ||
                    (va->query && checkquery(va->query)))
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
            renderva(cur, va, RENDERPASS_LIGHTMAP, fogpass);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);
    }

    if(blends)
    {
        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColorMask(COLORMASK, GL_FALSE);

        cur.vbuf = 0;
        cur.texgendim = -1;
        cur.blending = true;
        for(vtxarray *va = FIRSTVA; va; va = NEXTVA)
        {
            if(!va->blends) continue;
            if(refracting)
            {
                if(refracting < 0 ? va->geommin.z > reflectz : va->geommax.z <= reflectz) continue;
                if(ishiddencube(va->o, va->size)) continue;
                if(va->occluded >= OCCLUDE_GEOM) continue;
            }
            else if(reflecting)
            {
                if(va->geommax.z <= reflectz) continue;
            }
            else if(va->occluded >= OCCLUDE_GEOM) continue;
            if(fogpass ? va->geommax.z <= reflectz-refractfog || !refractfog : va->curvfc==VFC_FOGGED) continue;
            renderva(cur, va, RENDERPASS_LIGHTMAP_BLEND, fogpass);
        }
        if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);
        cur.blending = false;

        glColorMask(COLORMASK, GL_TRUE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    if(doSM) popshadowmap();

    cleanupgeom(cur);

    if(foggedvas.length()) renderfoggedvas(cur, doOQ && !zpass);

    if(causticspass)
    {
        if(!multipassing) { multipassing = true; glDepthFunc(GL_LEQUAL); }
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        static GLfloat zerofog[4] = { 0, 0, 0, 1 };
        glGetFloatv(GL_FOG_COLOR, cur.fogcolor);

        setupcaustics(0, causticspass);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glFogfv(GL_FOG_COLOR, zerofog);
        if(fading) glColorMask(COLORMASK, GL_FALSE);
        rendergeommultipass(cur, RENDERPASS_CAUSTICS, fogpass);
        if(fading) glColorMask(COLORMASK, GL_TRUE);

        glFogfv(GL_FOG_COLOR, cur.fogcolor);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    if(multipassing) glDepthFunc(GL_LESS);

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderalphageom(bool fogpass)
{
    static vector<vtxarray *> alphavas;
    alphavas.setsize(0);
    bool hasback = false;
    for(vtxarray *va = FIRSTVA; va; va = NEXTVA)
    {
        if(!va->alphabacktris && !va->alphafronttris) continue;
        if(refracting)
        {
            if((refracting < 0 ? va->geommin.z > reflectz : va->geommax.z <= reflectz) || va->occluded >= OCCLUDE_BB) continue;
            if(ishiddencube(va->o, va->size)) continue;
            if(va->occluded >= OCCLUDE_GEOM && pvsoccluded(va->geommin, va->geommax)) continue;
        }
        else if(reflecting)
        {
            if(va->geommax.z <= reflectz) continue;
        }
        else
        {
            if(va->occluded >= OCCLUDE_BB) continue;
            if(va->occluded >= OCCLUDE_GEOM && pvsoccluded(va->geommin, va->geommax)) continue;
        }
        if(fogpass ? va->geommax.z <= reflectz-refractfog || !refractfog : va->curvfc==VFC_FOGGED) continue;
        alphavas.add(va);
        if(va->alphabacktris) hasback = true;
    }
    if(alphavas.empty()) return;

    resetbatches();

    renderstate cur;
    cur.alphaing = 1;

    glEnableClientState(GL_VERTEX_ARRAY);

    glGetFloatv(GL_FOG_COLOR, cur.fogcolor);

    loop(front, 2) if(front || hasback)
    {
        cur.alphaing = front+1;
        if(!front) glCullFace(GL_FRONT);
        cur.vbuf = 0;
        cur.texgendim = -1;
        loopv(alphavas) renderva(cur, alphavas[i], RENDERPASS_Z);
        if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); }
        cur.colormask = true;
        glColorMask(COLORMASK, GL_FALSE);

        setupgeom(cur);

        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        cur.vbuf = 0;
        cur.texgendim = -1;
        cur.colorscale = vec(1, 1, 1);
        loopk(3) cur.color[k] = 1;
        cur.alphascale = -1;
        loopv(alphavas) if(front || alphavas[i]->alphabacktris) renderva(cur, alphavas[i], RENDERPASS_LIGHTMAP, fogpass);
        if(geombatches.length()) renderbatches(cur, RENDERPASS_LIGHTMAP);

        cleanupgeom(cur);

        glFogfv(GL_FOG_COLOR, cur.fogcolor);
        if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); }
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
        if(!front) glCullFace(GL_BACK);
    }

    glColorMask(COLORMASK, fading ? GL_FALSE : GL_TRUE);

    glBindBuffer_(GL_ARRAY_BUFFER, 0);
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void findreflectedvas(vector<vtxarray *> &vas, int prevvfc = VFC_PART_VISIBLE)
{
    loopv(vas)
    {
        vtxarray *va = vas[i];
        if(prevvfc >= VFC_NOT_VISIBLE) va->curvfc = prevvfc;
        if(va->curvfc == VFC_FOGGED || va->curvfc == PVS_FOGGED || va->o.z+va->size <= reflectz || isfoggedcube(va->o, va->size)) continue;
        bool render = true;
        if(va->curvfc == VFC_FULL_VISIBLE)
        {
            if(va->occluded >= OCCLUDE_BB) continue;
            if(va->occluded >= OCCLUDE_GEOM) render = false;
        }
        else if(va->curvfc == PVS_FULL_VISIBLE) continue;
        if(render)
        {
            if(va->curvfc >= VFC_NOT_VISIBLE) va->distance = (int)vadist(va, camera1->o);
            vtxarray **vprev = &reflectedva, *vcur = reflectedva;
            while(vcur && va->distance > vcur->distance)
            {
                vprev = &vcur->rnext;
                vcur = vcur->rnext;
            }
            va->rnext = *vprev;
            *vprev = va;
        }
        if(va->children.length()) findreflectedvas(va->children, va->curvfc);
    }
}

void renderreflectedgeom(bool causticspass, bool fogpass)
{
    if(reflecting)
    {
        reflectedva = NULL;
        findreflectedvas(varoot);
        rendergeom(causticspass ? 1 : 0, fogpass);
    }
    else rendergeom(causticspass ? 1 : 0, fogpass);
}

static vtxarray *prevskyva = NULL;

void renderskyva(vtxarray *va, bool explicitonly = false)
{
    if(!prevskyva || va->vbuf != prevskyva->vbuf)
    {
        if(!prevskyva) glEnableClientState(GL_VERTEX_ARRAY);
        glBindBuffer_(GL_ARRAY_BUFFER, va->vbuf);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, va->skybuf);
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), va->vdata[0].pos.v);
    }

    drawvatris(va, explicitonly ? va->explicitsky : va->sky+va->explicitsky, explicitonly ? va->skydata+va->sky : va->skydata);

    if(!explicitonly) xtraverts += va->sky/3;
    xtraverts += va->explicitsky/3;

    prevskyva = va;
}

int renderedsky = 0, renderedexplicitsky = 0, renderedskyfaces = 0, renderedskyclip = INT_MAX;

static inline void updateskystats(vtxarray *va)
{
    renderedsky += va->sky;
    renderedexplicitsky += va->explicitsky;
    renderedskyfaces |= va->skyfaces&0x3F;
    if(!(va->skyfaces&0x1F) || camera1->o.z < va->skyclip) renderedskyclip = min(renderedskyclip, va->skyclip);
    else renderedskyclip = 0;
}

void renderreflectedskyvas(vector<vtxarray *> &vas, int prevvfc = VFC_PART_VISIBLE)
{
    loopv(vas)
    {
        vtxarray *va = vas[i];
        if(prevvfc >= VFC_NOT_VISIBLE) va->curvfc = prevvfc;
        if((va->curvfc == VFC_FULL_VISIBLE && va->occluded >= OCCLUDE_BB) || va->curvfc==PVS_FULL_VISIBLE) continue;
        if(va->o.z+va->size <= reflectz || ishiddencube(va->o, va->size)) continue;
        if(va->sky+va->explicitsky)
        {
            updateskystats(va);
            renderskyva(va);
        }
        if(va->children.length()) renderreflectedskyvas(va->children, va->curvfc);
    }
}

bool rendersky(bool explicitonly)
{
    prevskyva = NULL;
    renderedsky = renderedexplicitsky = renderedskyfaces = 0;
    renderedskyclip = INT_MAX;

    if(reflecting)
    {
        renderreflectedskyvas(varoot);
    }
    else for(vtxarray *va = visibleva; va; va = va->next)
    {
        if((va->occluded >= OCCLUDE_BB && va->skyfaces&0x80) || !(va->sky+va->explicitsky)) continue;

        // count possibly visible sky even if not actually rendered
        updateskystats(va);
        if(explicitonly && !va->explicitsky) continue;
        renderskyva(va, explicitonly);
    }

    if(prevskyva)
    {
        glDisableClientState(GL_VERTEX_ARRAY);
        glBindBuffer_(GL_ARRAY_BUFFER, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return renderedsky+renderedexplicitsky > 0;
}

