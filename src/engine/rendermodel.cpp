#include "engine.h"

VAR(0, oqdynent, 0, 1, 1);
VAR(0, animationinterpolationtime, 0, 200, 1000);

model *loadingmodel = NULL;

#include "ragdoll.h"
#include "animmodel.h"
#include "vertmodel.h"
#include "skelmodel.h"
#include "hitzone.h"

static model *(__cdecl *modeltypes[NUMMODELTYPES])(const char *);

static int addmodeltype(int type, model *(__cdecl *loader)(const char *))
{
    modeltypes[type] = loader;
    return type;
}

#define MODELTYPE(modeltype, modelclass) \
static model *__loadmodel__##modelclass(const char *filename) \
{ \
    return new modelclass(filename); \
} \
UNUSED static int __dummy__##modelclass = addmodeltype((modeltype), __loadmodel__##modelclass);

#include "md2.h"
#include "md3.h"
#include "md5.h"
#include "obj.h"
#include "smd.h"
#include "iqm.h"

MODELTYPE(MDL_MD2, md2);
MODELTYPE(MDL_MD3, md3);
MODELTYPE(MDL_MD5, md5);
MODELTYPE(MDL_OBJ, obj);
MODELTYPE(MDL_SMD, smd);
MODELTYPE(MDL_IQM, iqm);

#define checkmdl if(!loadingmodel) { conoutf("\frNot loading a model"); return; }

void mdlwind(float *wind)
{
    checkmdl;
    loadingmodel->wind = max(*wind, 0.0f);
}
COMMAND(0, mdlwind, "f");

void mdlmaterial(int *material1, int *material2)
{
    checkmdl;
    loadingmodel->setmaterial(clamp(*material1, 0, int(MAXMDLMATERIALS)), clamp(*material2, 0, int(MAXMDLMATERIALS)));
}
COMMAND(0, mdlmaterial, "ii");

void mdlmixer(int *mixer)
{
    checkmdl;
    loadingmodel->setmixer(*mixer != 0);
}
COMMAND(0, mdlmixer, "i");

void mdlpattern(int *pattern)
{
    checkmdl;
    loadingmodel->setpattern(*pattern != 0);
}
COMMAND(0, mdlpattern, "i");

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->setcullface(*cullface);
}
COMMAND(0, mdlcullface, "i");

void mdlcolor(float *r, float *g, float *b)
{
    checkmdl;
    loadingmodel->setcolor(vec(*r, *g, *b));
}
COMMAND(0, mdlcolor, "fff");

void mdlcollide(int *collide)
{
    checkmdl;
    loadingmodel->collide = *collide!=0 ? (loadingmodel->collide ? loadingmodel->collide : COLLIDE_OBB) : COLLIDE_NONE;
}
COMMAND(0, mdlcollide, "i");

void mdlellipsecollide(int *collide)
{
    checkmdl;
    loadingmodel->collide = *collide!=0 ? COLLIDE_ELLIPSE : COLLIDE_NONE;
}
COMMAND(0, mdlellipsecollide, "i");

void mdltricollide(char *collide)
{
    checkmdl;
    DELETEA(loadingmodel->collidemodel);
    char *end = NULL;
    int val = strtol(collide, &end, 0);
    if(*end) { val = 1; loadingmodel->collidemodel = newstring(collide); }
    loadingmodel->collide = val ? COLLIDE_TRI : COLLIDE_NONE;
}
COMMAND(0, mdltricollide, "s");

void mdlspec(float *percent)
{
    checkmdl;
    float spec = *percent > 0 ? *percent/100.0f : 0.0f;
    loadingmodel->setspec(spec);
}
COMMAND(0, mdlspec, "f");

void mdlgloss(int *gloss)
{
    checkmdl;
    loadingmodel->setgloss(clamp(*gloss, 0, 2));
}
COMMAND(0, mdlgloss, "i");

void mdlalphatest(float *cutoff)
{
    checkmdl;
    loadingmodel->setalphatest(max(0.0f, min(1.0f, *cutoff)));
}
COMMAND(0, mdlalphatest, "f");

void mdldepthoffset(int *offset)
{
    checkmdl;
    loadingmodel->depthoffset = *offset!=0;
}
COMMAND(0, mdldepthoffset, "i");

void mdlglow(float *percent, float *delta, float *pulse)
{
    checkmdl;
    float glow = *percent > 0 ? *percent/100.0f : 0.0f, glowdelta = *delta/100.0f, glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
    glowdelta -= glow;
    loadingmodel->setglow(glow, glowdelta, glowpulse);
}
COMMAND(0, mdlglow, "fff");

void mdlenvmap(float *envmapmax, float *envmapmin, char *envmap)
{
    checkmdl;
    loadingmodel->setenvmap(*envmapmin, *envmapmax, envmap[0] ? cubemapload(envmap) : NULL);
}
COMMAND(0, mdlenvmap, "ffs");

void mdlfullbright(float *fullbright)
{
    checkmdl;
    loadingmodel->setfullbright(*fullbright);
}
COMMAND(0, mdlfullbright, "f");

void mdlshader(char *shader)
{
    checkmdl;
    loadingmodel->setshader(lookupshaderbyname(shader));
}
COMMAND(0, mdlshader, "s");

void mdlspin(float *yaw, float *pitch, float *roll)
{
    checkmdl;
    loadingmodel->spinyaw = *yaw;
    loadingmodel->spinpitch = *pitch;
    loadingmodel->spinroll = *roll;
}
COMMAND(0, mdlspin, "fff");

void mdlscale(float *percent)
{
    checkmdl;
    float scale = *percent > 0 ? *percent/100.0f : 1.0f;
    loadingmodel->scale = scale;
}
COMMAND(0, mdlscale, "f");

void mdlscalef(float *amt)
{
    checkmdl;
    loadingmodel->scale = *amt;
}
COMMAND(0, mdlscalef, "f");

void mdltrans(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->translate = vec(*x, *y, *z);
}
COMMAND(0, mdltrans, "fff");

void mdlyaw(float *angle)
{
    checkmdl;
    loadingmodel->offsetyaw = *angle;
}
COMMAND(0, mdlyaw, "f");

void mdlpitch(float *angle)
{
    checkmdl;
    loadingmodel->offsetpitch = *angle;
}
COMMAND(0, mdlpitch, "f");

void mdlroll(float *angle)
{
    checkmdl;
    loadingmodel->offsetroll = *angle;
}
COMMAND(0, mdlroll, "f");

void mdlshadow(int *shadow)
{
    checkmdl;
    loadingmodel->shadow = *shadow!=0;
}
COMMAND(0, mdlshadow, "i");

void mdlalphashadow(int *alphashadow)
{
    checkmdl;
    loadingmodel->alphashadow = *alphashadow!=0;
}
COMMAND(0, mdlalphashadow, "i");

void mdlbb(float *rad, float *h, float *height)
{
    checkmdl;
    loadingmodel->collidexyradius = *rad;
    loadingmodel->collideheight = *h;
    loadingmodel->height = *height;
}
COMMAND(0, mdlbb, "fff");

void mdlextendbb(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->bbextend = vec(*x, *y, *z);
}
COMMAND(0, mdlextendbb, "fff");

void mdllod(char *name, float *dist)
{
    checkmdl;
    loadingmodel->addlod(name, *dist);
}
COMMAND(0, mdllod, "sf");

void mdlname()
{
    checkmdl;
    result(loadingmodel->name);
}
COMMAND(0, mdlname, "");

#define checkragdoll \
    checkmdl; \
    if(!loadingmodel->skeletal()) { conoutf("\frNot loading a skeletal model"); return; } \
    skelmodel *m = (skelmodel *)loadingmodel; \
    if(m->parts.empty()) return; \
    skelmodel::skelmeshgroup *meshes = (skelmodel::skelmeshgroup *)m->parts.last()->meshes; \
    if(!meshes) return; \
    skelmodel::skeleton *skel = meshes->skel; \
    if(!skel->ragdoll) skel->ragdoll = new ragdollskel; \
    ragdollskel *ragdoll = skel->ragdoll; \
    if(ragdoll->loaded) return;


void rdvert(float *x, float *y, float *z, float *radius)
{
    checkragdoll;
    ragdollskel::vert &v = ragdoll->verts.add();
    v.pos = vec(*x, *y, *z);
    v.radius = *radius > 0 ? *radius : 1;
}
COMMAND(0, rdvert, "ffff");

void rdeye(int *v)
{
    checkragdoll;
    ragdoll->eye = *v;
}
COMMAND(0, rdeye, "i");

void rdtri(int *v1, int *v2, int *v3)
{
    checkragdoll;
    ragdollskel::tri &t = ragdoll->tris.add();
    t.vert[0] = *v1;
    t.vert[1] = *v2;
    t.vert[2] = *v3;
}
COMMAND(0, rdtri, "iii");

void rdjoint(int *n, int *t, int *v1, int *v2, int *v3)
{
    checkragdoll;
    if(*n < 0 || *n >= skel->numbones) return;
    ragdollskel::joint &j = ragdoll->joints.add();
    j.bone = *n;
    j.tri = *t;
    j.vert[0] = *v1;
    j.vert[1] = *v2;
    j.vert[2] = *v3;
}
COMMAND(0, rdjoint, "iibbb");

void rdlimitdist(int *v1, int *v2, float *mindist, float *maxdist)
{
    checkragdoll;
    ragdollskel::distlimit &d = ragdoll->distlimits.add();
    d.vert[0] = *v1;
    d.vert[1] = *v2;
    d.mindist = *mindist;
    d.maxdist = max(*maxdist, *mindist);
}
COMMAND(0, rdlimitdist, "iiff");

void rdlimitrot(int *t1, int *t2, float *maxangle, float *qx, float *qy, float *qz, float *qw)
{
    checkragdoll;
    ragdollskel::rotlimit &r = ragdoll->rotlimits.add();
    r.tri[0] = *t1;
    r.tri[1] = *t2;
    r.maxangle = *maxangle * RAD;
    r.maxtrace = 1 + 2*cos(r.maxangle);
    r.middle = matrix3(quat(*qx, *qy, *qz, *qw));
}
COMMAND(0, rdlimitrot, "iifffff");

void rdanimjoints(int *on)
{
    checkragdoll;
    ragdoll->animjoints = *on!=0;
}
COMMAND(0, rdanimjoints, "i");

// mapmodels

vector<mapmodelinfo> mapmodels;

void mapmodel(char *name)
{
    mapmodelinfo &mmi = mapmodels.add();
    copystring(mmi.name, name);
    mmi.m = mmi.collide = NULL;
}

void resetmapmodels(int n)
{
    mapmodels.shrink(clamp(n, 0, mapmodels.length()));
}

const char *mapmodelname(int i) { return mapmodels.inrange(i) ? mapmodels[i].name : NULL; }

COMMAND(0, mapmodel, "s");
ICOMMAND(0, mmodel, "s", (char *name), mapmodel(name));
ICOMMAND(0, mapmodelreset, "i", (int *n), if((identflags&IDF_WORLD) || editmode) resetmapmodels(*n));
ICOMMAND(0, mapmodelindex, "s", (char *a),
{
    if(!*a) intret(mapmodels.length());
    else
    {
        int num = parseint(a);
        if(mapmodels.inrange(num)) result(mapmodels[num].name);
    }
});

// model registry

hashnameset<model *> models;
vector<const char *> preloadmodels;
hashset<char *> failedmodels;

void preloadmodel(const char *name)
{
    if(!name || !name[0] || models.access(name) || preloadmodels.htfind(name) >= 0) return;
    preloadmodels.add(newstring(name));
}

void flushpreloadedmodels(bool msg)
{
    loopv(preloadmodels)
    {
        loadprogress = float(i+1)/preloadmodels.length();
        model *m = loadmodel(preloadmodels[i], -1, msg);
        if(!m) { if(msg) conoutf("\frCould not load model: %s", preloadmodels[i]); }
        else
        {
            m->preloadmeshes();
            m->preloadshaders();
        }
    }
    preloadmodels.deletearrays();
    loadprogress = 0;
}

void preloadusedmapmodels(bool msg, bool bih)
{
    vector<extentity *> &ents = entities::getents();
    vector<int> used;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || e.attrs[0] < 0 || used.find(e.attrs[0]) >= 0 || !checkmapvariant(e.attrs[13]) || !checkmapeffects(e.attrs[14])) continue;
        used.add(e.attrs[0]);
    }

    vector<const char *> col;
    loopv(used)
    {
        loadprogress = float(i+1)/used.length();
        int mmindex = used[i];
        if(!mapmodels.inrange(mmindex)) { if(msg) conoutf("\frCould not find map model: %d", mmindex); continue; }
        mapmodelinfo &mmi = mapmodels[mmindex];
        if(!mmi.name[0]) continue;
        model *m = loadmodel(NULL, mmindex, msg);
        if(!m) { if(msg) conoutf("\frCould not load map model: %s", mmi.name); }
        else
        {
            if(bih) m->preloadBIH();
            else if(m->collide == COLLIDE_TRI && !m->collidemodel && m->bih) m->setBIH();
            m->preloadmeshes();
            m->preloadshaders();
            if(m->collidemodel && col.htfind(m->collidemodel) < 0) col.add(m->collidemodel);
        }
    }

    loopv(col)
    {
        loadprogress = float(i+1)/col.length();
        model *m = loadmodel(col[i], -1, msg);
        if(!m) { if(msg) conoutf("\frCould not load collide model: %s", col[i]); }
        else if(!m->bih) m->setBIH();
    }
    loadprogress = 0;
}

model *loadmodel(const char *name, int i, bool msg)
{
    if(!name)
    {
        if(!mapmodels.inrange(i)) return NULL;
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m) return mmi.m;
        name = mmi.name;
    }
    model **mm = models.access(name);
    model *m;
    if(mm) m = *mm;
    else
    {
        if(!name[0] || loadingmodel || failedmodels.find(name, NULL)) return NULL;
        if(msg)
        {
            defformatstring(str, "Loading model: %s", name);
            progress(loadprogress, str);
        }
        loopi(NUMMODELTYPES)
        {
            m = modeltypes[i](name);
            if(!m) continue;
            loadingmodel = m;
            if(m->load()) break;
            DELETEP(m);
        }
        loadingmodel = NULL;
        if(!m)
        {
            failedmodels.add(newstring(name));
            return NULL;
        }
        models.access(m->name, m);
    }
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
}

void clear_models()
{
    enumerate(models, model *, m, delete m);
}

void cleanupmodels()
{
    enumerate(models, model *, m, m->cleanup());
}

void clearmodel(char *name)
{
    model *m = models.find(name, NULL);
    if(!m) { conoutf("\frModel %s is not loaded", name); return; }
    loopv(mapmodels)
    {
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m == m) mmi.m = NULL;
        if(mmi.collide == m) mmi.collide = NULL;
    }
    models.remove(name);
    m->cleanup();
    delete m;
    conoutf("\fyCleared model %s", name);
}

COMMAND(0, clearmodel, "s");

bool modeloccluded(const vec &center, float radius)
{
    ivec bbmin(vec(center).sub(radius)), bbmax(vec(center).add(radius+1));
    return pvsoccluded(bbmin, bbmax) || bboccluded(bbmin, bbmax);
}

struct batchedmodel
{
    modelstate state;
    int attached;
    union
    {
        int visible;
        int culled;
    };
    dynent *d;
    int next;
};
struct modelbatch
{
    model *m;
    int flags, batched;
};
static vector<batchedmodel> batchedmodels;
static vector<modelbatch> batches;
static vector<modelattach> modelattached;

void resetmodelbatches()
{
    batchedmodels.setsize(0);
    batches.setsize(0);
    modelattached.setsize(0);
}

void addbatchedmodel(model *m, batchedmodel &bm, int idx)
{
    modelbatch *b = NULL;
    if(batches.inrange(m->batch))
    {
        b = &batches[m->batch];
        if(b->m == m && (b->flags & MDL_MAPMODEL) == (bm.state.flags & MDL_MAPMODEL))
            goto foundbatch;
    }

    m->batch = batches.length();
    b = &batches.add();
    b->m = m;
    b->flags = 0;
    b->batched = -1;

foundbatch:
    b->flags |= bm.state.flags;
    bm.next = b->batched;
    b->batched = idx;
}

static inline void renderbatchedmodel(model *m, batchedmodel &b)
{
    if(b.attached>=0) b.state.attached = &modelattached[b.attached];

    int anim = b.state.anim;
    if(shadowmapping > SM_REFLECT)
    {
        anim |= ANIM_NOSKIN;
    }
    else
    {
        if(b.state.flags&MDL_FULLBRIGHT) anim |= ANIM_FULLBRIGHT;
    }

    m->render(anim, &b.state, b.d);
}

VAR(0, maxmodelradiusdistance, 10, 200, 1000);

static inline void enablecullmodelquery()
{
    startbb();
}

static inline void rendercullmodelquery(model *m, dynent *d, const vec &center, float radius)
{
    if(fabs(camera1->o.x-center.x) < radius+1 &&
       fabs(camera1->o.y-center.y) < radius+1 &&
       fabs(camera1->o.z-center.z) < radius+1)
    {
        d->query = NULL;
        return;
    }
    d->query = newquery(d);
    if(!d->query) return;
    startquery(d->query);
    int br = int(radius*2)+1;
    drawbb(ivec(int(center.x-radius), int(center.y-radius), int(center.z-radius)), ivec(br, br, br));
    endquery(d->query);
}

static inline void disablecullmodelquery()
{
    endbb();
}

static inline int cullmodel(model *m, const vec &center, float radius, int flags, dynent *d = NULL)
{
    if(flags&MDL_CULL_DIST && center.dist(camera1->o)/radius>maxmodelradiusdistance) return MDL_CULL_DIST;
    if(flags&MDL_CULL_VFC && isfoggedsphere(radius, center)) return MDL_CULL_VFC;
    if(flags&MDL_CULL_OCCLUDED && modeloccluded(center, radius)) return MDL_CULL_OCCLUDED;
    else if(flags&MDL_CULL_QUERY && d->query && d->query->owner==d && checkquery(d->query)) return MDL_CULL_QUERY;
    return 0;
}

static inline int shadowmaskmodel(const vec &center, float radius)
{
    switch(shadowmapping)
    {
        case SM_REFLECT:
            return calcspherersmsplits(center, radius);
        case SM_CUBEMAP:

        {
            vec scenter = vec(center).sub(shadoworigin);
            float sradius = radius + shadowradius;
            if(scenter.squaredlen() >= sradius*sradius) return 0;
            return calcspheresidemask(scenter, radius, shadowbias);
        }
        case SM_CASCADE:
            return calcspherecsmsplits(center, radius);
        case SM_SPOT:
        {
            vec scenter = vec(center).sub(shadoworigin);
            float sradius = radius + shadowradius;
            return scenter.squaredlen() < sradius*sradius && sphereinsidespot(shadowdir, shadowspot, scenter, radius) ? 1 : 0;
        }
    }
    return 0;
}

void shadowmaskbatchedmodels(bool dynshadow)
{
    loopv(batchedmodels)
    {
        batchedmodel &b = batchedmodels[i];
        if(b.state.flags&(MDL_MAPMODEL | MDL_NOSHADOW)) break;
        b.visible = dynshadow && (b.state.color.a >= 1 || b.state.flags & (MDL_ONLYSHADOW | MDL_FORCESHADOW)) ? shadowmaskmodel(b.state.center, b.state.radius) : 0;
    }
}

int batcheddynamicmodels()
{
    int visible = 0;
    loopv(batchedmodels)
    {
        batchedmodel &b = batchedmodels[i];
        if(b.state.flags&MDL_MAPMODEL) break;
        visible |= b.visible;
    }
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!(b.flags&MDL_MAPMODEL) || !b.m->animated()) continue;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            visible |= bm.visible;
        }
    }
    return visible;
}

int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax)
{
    int vis = 0;
    loopv(batchedmodels)
    {
        batchedmodel &b = batchedmodels[i];
        if(b.state.flags&MDL_MAPMODEL) break;
        if(b.visible&mask)
        {
            bbmin.min(vec(b.state.center).sub(b.state.radius));
            bbmax.max(vec(b.state.center).add(b.state.radius));
            ++vis;
        }
    }
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!(b.flags&MDL_MAPMODEL) || !b.m->animated()) continue;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(bm.visible&mask)
            {
                bbmin.min(vec(bm.state.center).sub(bm.state.radius));
                bbmax.max(vec(bm.state.center).add(bm.state.radius));
                ++vis;
            }
        }
    }
    return vis;
}

void rendershadowmodelbatches(bool dynmodel)
{
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!b.m->shadow || (!dynmodel && (!(b.flags&MDL_MAPMODEL) || b.m->animated()))) continue;
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(!(bm.visible&(1<<shadowside))) continue;
            if(!rendered) { b.m->startrender(); rendered = true; }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered) b.m->endrender();
    }
}

void rendermapmodelbatches()
{
    enableaamask();
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!(b.flags&MDL_MAPMODEL)) continue;
        b.m->startrender();
        setaamask(b.m->animated());
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            renderbatchedmodel(b.m, bm);
            j = bm.next;
        }
        b.m->endrender();
    }
    disableaamask();
}

float transmdlsx1 = -1, transmdlsy1 = -1, transmdlsx2 = 1, transmdlsy2 = 1;
uint transmdltiles[LIGHTTILE_MAXH];

void rendermodelbatches()
{
    transmdlsx1 = transmdlsy1 = 1;
    transmdlsx2 = transmdlsy2 = -1;
    memset(transmdltiles, 0, sizeof(transmdltiles));

    enableaamask();
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(b.flags&MDL_MAPMODEL) continue;
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            bm.culled = cullmodel(b.m, bm.state.center, bm.state.radius, bm.state.flags, bm.d);
            if(bm.culled || bm.state.flags&MDL_ONLYSHADOW) continue;
            if(bm.state.color.a < 1 || bm.state.flags&MDL_FORCETRANSPARENT)
            {
                float sx1, sy1, sx2, sy2;
                ivec bbmin(vec(bm.state.center).sub(bm.state.radius)), bbmax(vec(bm.state.center).add(bm.state.radius+1));
                if(calcbbscissor(bbmin, bbmax, sx1, sy1, sx2, sy2))
                {
                    transmdlsx1 = min(transmdlsx1, sx1);
                    transmdlsy1 = min(transmdlsy1, sy1);
                    transmdlsx2 = max(transmdlsx2, sx2);
                    transmdlsy2 = max(transmdlsy2, sy2);
                    masktiles(transmdltiles, sx1, sy1, sx2, sy2);
                }
                continue;
            }
            if(!rendered)
            {
                b.m->startrender();
                rendered = true;
                setaamask(true);
            }
            if(bm.state.flags&MDL_CULL_QUERY)
            {
                bm.d->query = newquery(bm.d);
                if(bm.d->query)
                {
                    startquery(bm.d->query);
                    renderbatchedmodel(b.m, bm);
                    endquery(bm.d->query);
                    continue;
                }
            }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered) b.m->endrender();
        if(b.flags&MDL_CULL_QUERY)
        {
            bool queried = false;
            for(int j = b.batched; j >= 0;)
            {
                batchedmodel &bm = batchedmodels[j];
                j = bm.next;
                if(bm.culled&(MDL_CULL_OCCLUDED|MDL_CULL_QUERY) && bm.state.flags&MDL_CULL_QUERY)
                {
                    if(!queried)
                    {
                        if(rendered) setaamask(false);
                        enablecullmodelquery();
                        queried = true;
                    }
                    rendercullmodelquery(b.m, bm.d, bm.state.center, bm.state.radius);
                }
            }
            if(queried) disablecullmodelquery();
        }
    }
    disableaamask();
}

void rendertransparentmodelbatches(int stencil)
{
    enableaamask(stencil);
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(b.flags&MDL_MAPMODEL) continue;
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            bm.culled = cullmodel(b.m, bm.state.center, bm.state.radius, bm.state.flags, bm.d);
            if(bm.culled || !(bm.state.color.a < 1 || bm.state.flags&MDL_FORCETRANSPARENT) || bm.state.flags&MDL_ONLYSHADOW) continue;
            if(!rendered)
            {
                b.m->startrender();
                rendered = true;
                setaamask(true);
            }
            if(bm.state.flags&MDL_CULL_QUERY)
            {
                bm.d->query = newquery(bm.d);
                if(bm.d->query)
                {
                    startquery(bm.d->query);
                    renderbatchedmodel(b.m, bm);
                    endquery(bm.d->query);
                    continue;
                }
            }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered) b.m->endrender();
    }
    disableaamask();
}

static occludequery *modelquery = NULL;
static int modelquerybatches = -1, modelquerymodels = -1, modelqueryattached = -1;

void startmodelquery(occludequery *query)
{
    modelquery = query;
    modelquerybatches = batches.length();
    modelquerymodels = batchedmodels.length();
    modelqueryattached = modelattached.length();
}

void endmodelquery()
{
    if(batchedmodels.length() == modelquerymodels)
    {
        modelquery->fragments = 0;
        modelquery = NULL;
        return;
    }
    enableaamask();
    startquery(modelquery);
    loopv(batches)
    {
        modelbatch &b = batches[i];
        int j = b.batched;
        if(j < modelquerymodels) continue;
        b.m->startrender();
        setaamask(!(b.flags&MDL_MAPMODEL) || b.m->animated());
        do
        {
            batchedmodel &bm = batchedmodels[j];
            renderbatchedmodel(b.m, bm);
            j = bm.next;
        }
        while(j >= modelquerymodels);
        b.batched = j;
        b.m->endrender();
    }
    endquery(modelquery);
    modelquery = NULL;
    batches.setsize(modelquerybatches);
    batchedmodels.setsize(modelquerymodels);
    modelattached.setsize(modelqueryattached);
    disableaamask();
}

void clearbatchedmapmodels()
{
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(b.flags&MDL_MAPMODEL)
        {
            batchedmodels.setsize(b.batched);
            batches.setsize(i);
            break;
        }
    }
}

VAR(IDF_PERSIST, lodmodelfov, 0, 1, 1);
FVAR(IDF_PERSIST, lodmodelfovmax, 1, 90, 180);
FVAR(IDF_PERSIST, lodmodelfovmin, 1, 10, 180);
FVAR(IDF_PERSIST, lodmodelfovdist, 1, 1024, VAR_MAX);
FVAR(IDF_PERSIST, lodmodelfovscale, 0, 1, 1000);

model *loadlodmodel(model *m, const vec &pos)
{
    if(drawtex || !m) return m;
    float dist = camera1->o.dist(pos);
    if(lodmodelfov && dist <= lodmodelfovdist)
    {
        float fovmin = min(lodmodelfovmin, lodmodelfovmax),
              fovmax = max(lodmodelfovmax, fovmin+1.f),
              fovnow = clamp(curfov, fovmin, fovmax);
        if(fovnow < fovmax)
        {
            float x = fmod(fabs(asin((pos.z-camera1->o.z)/dist)/RAD-camera1->pitch), 360),
                  y = fmod(fabs(-atan2(pos.x-camera1->o.x, pos.y-camera1->o.y)/RAD-camera1->yaw), 360);
            if(min(x, 360-x) <= curfov && min(y, 360-y) <= fovy) dist *= fovnow/fovmax*lodmodelfovscale;
        }
    }
    const char *mdl = m->lodmodel(dist);
    if(!mdl || !*mdl) return m;
    model *lm = loadmodel(mdl);
    return lm ? lm : m;
}

void rendermapmodel(int idx, entmodelstate &state)
{
    if(!mapmodels.inrange(idx)) return;
    mapmodelinfo &mmi = mapmodels[idx];
    model *m = loadlodmodel(mmi.m ? mmi.m : loadmodel(mmi.name), state.o);
    if(!m) return;
    vec bbradius;
    m->boundbox(state.center, bbradius);
    state.radius = bbradius.magnitude();
    state.center.mul(state.size);
    if(state.roll) state.center.rotate_around_y(-state.roll*RAD);
    if(state.pitch && m->pitched()) state.center.rotate_around_x(state.pitch*RAD);
    state.center.rotate_around_z(state.yaw*RAD);
    state.center.add(state.o);
    state.radius *= state.size;

    int visible = 0;
    if(shadowmapping)
    {
        if(!m->shadow) return;
        visible = shadowmaskmodel(state.center, state.radius);
        if(!visible) return;
    }
    else if(state.flags&(MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED) && cullmodel(m, state.center, state.radius, state.flags))
        return;

    batchedmodel &b = batchedmodels.add();
    (entmodelstate &)b.state = state;
    b.state.flags |= MDL_MAPMODEL;
    b.visible = visible;
    b.d = NULL;
    b.attached = -1;
    addbatchedmodel(m, b, batchedmodels.length()-1);
}

void rendermodel(const char *mdl, modelstate &state, dynent *d)
{
    model *m = loadlodmodel(loadmodel(mdl), state.o);
    if(!m) return;
    vec bbradius;
    m->boundbox(state.center, bbradius);
    state.radius = bbradius.magnitude();
    if(d)
    {
        if(d->ragdoll)
        {
            if(state.anim&ANIM_RAGDOLL && d->ragdoll->millis >= state.basetime)
            {
                state.radius = max(state.radius, d->ragdoll->radius);
                state.center = d->ragdoll->center;
                goto hasboundbox;
            }
            DELETEP(d->ragdoll);
        }
        if(state.anim&ANIM_RAGDOLL) state.flags &= ~(MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY);
    }
    state.center.mul(state.size);
    if(state.roll) state.center.rotate_around_y(-state.roll*RAD);
    if(state.pitch && m->pitched()) state.center.rotate_around_x(state.pitch*RAD);
    state.center.rotate_around_z(state.yaw*RAD);
    state.center.add(state.o);
hasboundbox:
    state.radius *= state.size;

    if(state.flags&MDL_NORENDER) state.anim |= ANIM_NORENDER;

    if(state.attached) for(int i = 0; state.attached[i].tag; i++)
        if(state.attached[i].name) state.attached[i].m = loadlodmodel(loadmodel(state.attached[i].name), state.o);

    if(state.flags&MDL_CULL_QUERY)
    {
        if(!oqfrags || !oqdynent || !d) state.flags &= ~MDL_CULL_QUERY;
    }

    if(state.flags&MDL_NOBATCH)
    {
        int culled = cullmodel(m, state.center, state.radius, state.flags, d);
        if(culled)
        {
            if(culled&(MDL_CULL_OCCLUDED|MDL_CULL_QUERY) && state.flags&MDL_CULL_QUERY)
            {
                enablecullmodelquery();
                rendercullmodelquery(m, d, state.center, state.radius);
                disablecullmodelquery();
            }
            return;
        }
        enableaamask();
        if(state.flags&MDL_CULL_QUERY)
        {
            d->query = newquery(d);
            if(d->query) startquery(d->query);
        }
        m->startrender();
        setaamask(true);
        if(state.flags&MDL_FULLBRIGHT) state.anim |= ANIM_FULLBRIGHT;
        m->render(state.anim, &state, d);
        m->endrender();
        if(state.flags&MDL_CULL_QUERY && d->query) endquery(d->query);
        disableaamask();
        return;
    }

    batchedmodel &b = batchedmodels.add();
    b.state = state;
    b.visible = 0;
    b.d = d;
    b.attached = state.attached ? modelattached.length() : -1;
    if(state.attached) for(int i = 0;; i++) { modelattached.add(state.attached[i]); if(!state.attached[i].tag) break; }
    addbatchedmodel(m, b, batchedmodels.length()-1);
}

int intersectmodel(const char *mdl, modelstate &state, const vec &o, const vec &ray, float &dist, int mode, dynent *d)
{
    model *m = loadmodel(mdl);
    if(!m) return -1;
    if(d && d->ragdoll && (!(state.anim&ANIM_RAGDOLL) || d->ragdoll->millis < state.basetime)) DELETEP(d->ragdoll);
    if(state.attached) for(int i = 0; state.attached[i].tag; i++)
    {
        if(state.attached[i].name) state.attached[i].m = loadmodel(state.attached[i].name);
    }
    return m->intersect(state.anim, &state, d, o, ray, dist, mode);
}

void abovemodel(vec &o, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    o.z += m->above();
}

bool matchanim(const char *name, const char *pattern)
{
    for(;; pattern++)
    {
        const char *s = name;
        char c;
        for(;; pattern++)
        {
            c = *pattern;
            if(!c || c=='|') break;
            else if(c=='*')
            {
                if(!*s || iscubespace(*s)) break;
                do s++; while(*s && !iscubespace(*s));
            }
            else if(c!=*s) break;
            else s++;
        }
        if(!*s && (!c || c=='|')) return true;
        pattern = strchr(pattern, '|');
        if(!pattern) break;
    }
    return false;
}

ICOMMAND(0, findanims, "s", (char *name),
{
    vector<int> anims;
    game::findanims(name, anims);
    vector<char> buf;
    string num;
    loopv(anims)
    {
        formatstring(num, "%d", anims[i]);
        if(i > 0) buf.add(' ');
        buf.put(num, strlen(num));
    }
    buf.add('\0');
    result(buf.getbuf());
});

void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks) // model skin sharing
{
    string dirs[3];
    formatstring(dirs[0], "%s/", dir);
    formatstring(dirs[1], "%s/", altdir);
    formatstring(dirs[2], "textures/");
    masks = notexture;

    #define tryload(tex, prefix, cmd, path) loopi(4) { if((tex = textureload(makerelpath(i < 3 ? dirs[i] : "", path, prefix, cmd), 0, true, false)) != notexture) break; }
    tryload(skin, NULL, NULL, "skin");
    tryload(masks, NULL, NULL, "masks");
}

void setbbfrommodel(dynent *d, const char *mdl, float size)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    vec center, radius;
    m->collisionbox(center, radius);
    center.mul(size);
    radius.mul(size);
    if(m->collide != COLLIDE_ELLIPSE) d->collidetype = COLLIDE_OBB;
    d->xradius  = radius.x + fabs(center.x);
    d->yradius  = radius.y + fabs(center.y);
    d->radius   = d->collidetype==COLLIDE_OBB ? sqrtf(d->xradius*d->xradius + d->yradius*d->yradius) : max(d->xradius, d->yradius);
    d->height   = d->zradius = (center.z-radius.z) + radius.z*2*m->height;
    d->aboveeye = radius.z*2*(1.0f-m->height);
}

