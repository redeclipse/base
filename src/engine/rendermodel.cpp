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

#define checkmdl if(!loadingmodel) { conoutf(colourred, "Not loading a model"); return; }

void mdlwind(float *wind)
{
    checkmdl;
    loadingmodel->wind = max(*wind, 0.0f);
}
COMMAND(0, mdlwind, "f");

void mdlwindback(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->windback = vec(*x, *y, *z).safenormalize();
}
COMMAND(0, mdlwindback, "fff");

void mdlmaterial(int *material1, int *material2, int *material3, float *split)
{
    checkmdl;
    loadingmodel->setmaterial(clamp(*material1, 0, int(MAXMDLMATERIALS)), clamp(*material2, 0, int(MAXMDLMATERIALS)), clamp(*material3, 0, int(MAXMDLMATERIALS)), clamp(*split, 0.0f, 1.0f));
}
COMMAND(0, mdlmaterial, "iiif");

void mdlmixer(int *mixer)
{
    checkmdl;
    loadingmodel->setmixer(*mixer);
}
COMMAND(0, mdlmixer, "i");

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->setcullface(*cullface);
}
COMMAND(0, mdlcullface, "i");

void mdlcullaura(int *cullaura)
{
    checkmdl;
    loadingmodel->setcullaura(*cullaura != 0);
}
COMMAND(0, mdlcullaura, "i");

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

void mdldither(int *dither)
{
    checkmdl;
    loadingmodel->setdither(*dither != 0);
}
COMMAND(0, mdldither, "i");

void mdlblend(float *blend)
{
    checkmdl;
    loadingmodel->setblend(max(0.0f, min(1.0f, *blend)));
}
COMMAND(0, mdlblend, "f");

void mdlblendmode(int *blendmode)
{
    checkmdl;
    loadingmodel->setblendmode(max((int)MDL_BLEND_TEST, min((int)MDL_BLEND_ALPHA, *blendmode)));
}
COMMAND(0, mdlblendmode, "f");

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
    loadingmodel->addlod(name, 0, *dist);
}
COMMAND(0, mdllod, "sf");

void mdllodpixels(char *name, float *pixels)
{
    checkmdl;
    loadingmodel->addlod(name, *pixels, 0);
}
COMMAND(0, mdllodpixels, "sf");

void mdlparenttag(char *name, float *tx, float *ty, float *tz, float *rx, float *ry, float *rz)
{
    checkmdl;
    float cx = *rx ? cosf(*rx/2*RAD) : 1, sx = *rx ? sinf(*rx/2*RAD) : 0, cy = *ry ? cosf(*ry/2*RAD) : 1, sy = *ry ? sinf(*ry/2*RAD) : 0, cz = *rz ? cosf(*rz/2*RAD) : 1, sz = *rz ? sinf(*rz/2*RAD) : 0;
    parenttag &m = loadingmodel->parenttags.add();
    m.name = newstring(name);
    m.matrix = matrix4x3(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                vec(*tx, *ty, *tz));
}
COMMAND(0, mdlparenttag, "sffffff");

void mdlname()
{
    checkmdl;
    result(loadingmodel->name);
}
COMMAND(0, mdlname, "");

#define checkragdoll \
    checkmdl; \
    if(!loadingmodel->skeletal()) { conoutf(colourred, "Not loading a skeletal model"); return; } \
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

// a map model whose folder is gone is saved as the folder its redirect leads to, found from the files each model type
// loads first; a folder needs a name/* redirect, so the replacement ends with the same file
void portmapmodels()
{
    static const char *const prefixes[] = { "" };
    static const char *const files[] = { "/md2.cfg", "/md3.cfg", "/md5.cfg", "/obj.cfg", "/smd.cfg", "/iqm.cfg", "/tris.md2", "/tris.md3", "/tris.obj" };
    loopv(mapmodels)
    {
        mapmodelinfo &mmi = mapmodels[i];
        redirectprobed found;
        if(redirectprobe(mmi.name, prefixes, 1, files, sizeof(files)/sizeof(files[0]), found) != REDIRECT_REPLACED) continue;
        const char *file = files[found.suffix];
        int len = strlen(found.replacement) - strlen(file);
        if(len <= 0 || strcmp(&found.replacement[len], file))
        {
            scriptconoutf(colouryellow, "Map model %s is redirected to %s, which isn't a model folder", mmi.name, found.replacement);
            continue;
        }
        string name;
        copystring(name, found.replacement, len + 1);
        conoutf(colourwhite, "Ported map model %s to %s", mmi.name, name);
        copystring(mmi.name, name);
    }
}

COMMAND(IDF_SAFE, mapmodel, "s");
ICOMMAND(IDF_SAFE, mmodel, "s", (char *name), mapmodel(name));
ICOMMAND(IDF_SAFE, mapmodelreset, "i", (int *n), if((identflags&IDF_MAP) || editmode) resetmapmodels(*n));
ICOMMAND(IDF_SAFE, mapmodelindex, "s", (char *a),
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
vector<uchar> preloadquiet;
hashset<char *> failedmodels;

// a required model that couldn't load, noted for fileredirectmissing when its folder is gone rather than broken
static void modelmissing(const char *name)
{
    if(fileredirectmissing && !fileexists(findfile(name, "d"), "d")) filemissing(name);
}

// quiet models are optional, so not finding one isn't an error
void preloadmodel(const char *name, bool quiet)
{
    if(!name || !name[0] || models.access(name) || failedmodels.find(name, NULL) || preloadmodels.htfind(name) >= 0) return;
    preloadmodels.add(newstring(name));
    preloadquiet.add(quiet ? 1 : 0);
}

int modelanimlength(const char *name, int anim, dynent *d, int basetime)
{
    if(!name || !name[0]) return 0;
    model **m = models.access(name); // only models already loaded, never load mid-frame
    return m && *m ? (*m)->animlength(anim, d, basetime) : 0;
}

void flushpreloadedmodels(bool msg)
{
    loopv(preloadmodels)
    {
        loadprogress = float(i+1)/preloadmodels.length();
        model *m = loadmodel(preloadmodels[i], -1, msg);
        if(!m)
        {
            if(preloadquiet[i]) continue;
            modelmissing(preloadmodels[i]);
            if(msg) conoutf(colourred, "Could not load model: %s", preloadmodels[i]);
        }
        else
        {
            m->preloadmeshes();
            m->preloadshaders();
        }
    }
    preloadmodels.deletearrays();
    preloadquiet.setsize(0);
    loadprogress = 0;
}

void preloadusedmapmodels(bool msg, bool bih)
{
    vector<extentity *> &ents = entities::getents();
    vector<int> used;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || e.flags&EF_VIRTUAL || e.attrs[0] < 0 || used.find(e.attrs[0]) >= 0 || !entities::isallowed(e)) continue;
        used.add(e.attrs[0]);
    }

    vector<const char *> col;
    loopv(used)
    {
        loadprogress = float(i+1)/used.length();
        int mmindex = used[i];
        if(!mapmodels.inrange(mmindex)) { if(msg) conoutf(colourred, "Could not find map model: %d", mmindex); continue; }
        mapmodelinfo &mmi = mapmodels[mmindex];
        if(!mmi.name[0]) continue;
        model *m = loadmodel(NULL, mmindex, msg);
        if(!m)
        {
            modelmissing(mmi.name);
            if(msg) conoutf(colourred, "Could not load map model: %s", mmi.name);
        }
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
        if(!m)
        {
            modelmissing(col[i]);
            if(msg) conoutf(colourred, "Could not load collide model: %s", col[i]);
        }
        else if(!m->bih) m->setBIH();
    }
    loadprogress = 0;
}

model *loadmodel(const char *name, int i, bool msg, model *parent)
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
        if(!sandboxpath(name))
        {
            conoutf(colourred, "Not loading model %s from outside the game's folders", name);
            failedmodels.add(newstring(name));
            return NULL;
        }
        loopi(NUMMODELTYPES)
        {
            if(msg) progress(loadprogress, "Loading model: %s", name);
            m = modeltypes[i](name);
            if(!m) continue;
            loadingmodel = m;
            bool loaded = false;
            LIFTMAP(loaded = m->load(parent));
            if(loaded) break;
            DELETEP(m);
        }
        loadingmodel = NULL;
        if(!m)
        {
            failedmodels.add(newstring(name));
            return NULL;
        }
        models.access(m->name, m);
        m->loadlods(msg);
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

void clearmodel(const char *name, bool msg)
{
    model *m = models.find(name, NULL);
    if(!m) { if(msg) conoutf(colourred, "Model %s is not loaded", name); return; }
    loopv(mapmodels)
    {
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m == m) mmi.m = NULL;
        if(mmi.collide == m) mmi.collide = NULL;
    }
    enumerate(models, model *, om, om->forgetlod(m));
    models.remove(name);
    m->cleanup();
    delete m;
    if(msg) conoutf(colouryellow, "Cleared model %s", name);
}

ICOMMAND(0, clearmodel, "s", (char *name), clearmodel(name, true));

void enummodels()
{
    vector<char> buf;
    enumerate(models, model *, m,
    {
        if(buf.length()) buf.add(' ');
        buf.put(m->name, strlen(m->name));
    });
    buf.add('\0');
    result(buf.getbuf());
}

COMMAND(0, enummodels, "");

bool modeloccluded(const vec &center, float radius)
{
    ivec bbmin(vec(center).sub(radius)), bbmax(vec(center).add(radius+1));
    if(!insideworld(bbmin) || !insideworld(bbmax)) return false;
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
    model *shadowm; // the LOD drawn in the shadow pass under way, when it isn't the batch's
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
        if(b->m == m && (b->flags&MDL_MAPMODEL) == (bm.state.flags&MDL_MAPMODEL))
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

static inline bool isbatchdynamic(modelbatch &b)
{
    return b.m->animated() || b.flags&MDL_FORCEDYNAMIC;
}

static inline void renderbatchedmodel(model *m, batchedmodel &b)
{
    if(b.attached>=0) b.state.attached = &modelattached[b.attached];

    int anim = b.state.anim;
    if(shadowmapping > SM_REFLECT || drawtex == DRAWTEX_AURA || transdepthmodels) anim |= ANIM_NOSKIN;
    else if(b.state.flags&MDL_FULLBRIGHT) anim |= ANIM_FULLBRIGHT;

    m->render(anim, &b.state, b.d);
}

VAR(0, maxmodelradiusdistance, 10, 256, 1000);

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
    if(flags&MDL_CULL_DIST && center.dist(camera1->o)/max(radius, 1.0f) > maxmodelradiusdistance) return MDL_CULL_DIST;
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

model *loadbestlod(model *m, const vec &center, float sqradius, float scale, int shadowmask = 0);

FVAR(IDF_PERSIST, smcullsize, 0, 1, 16); // how many texels wide a model has to be in a shadow to cast one, 0 for any size

// whether a model is too small in the finest side of the shadow being drawn to show in it
bool shadowculled(const vec &center, float radius, int shadowmask)
{
    if(smcullsize <= 0) return false;
    float texels = shadowtexelscale(center, shadowmask);
    return texels > 0 && 2*radius*texels < smcullsize;
}

// models are batched once with their LOD on screen, so each shadow pass picks theirs from their width in its texels
static model *shadowbatchedlod(model *m, const batchedmodel &b)
{
    if(b.state.flags&MDL_NOLOD) return NULL;
    model *lm = loadbestlod(m->parentlod ? m->parentlod : m, b.state.center, b.state.radius*b.state.radius, b.state.lodscale, b.visible);
    if(lm == m || (b.d && lm->animkey() != m->animkey())) return NULL; // one with its own skeleton would restart the dynent's animation blending every frame
    return lm;
}

void shadowmaskbatchedmodels(bool dynshadow, bool noavatar)
{
    loopv(batches)
    {
        modelbatch &mb = batches[i];
        if(mb.flags&MDL_MAPMODEL) continue;
        for(int j = mb.batched; j >= 0;)
        {
            batchedmodel &b = batchedmodels[j];
            j = b.next;
            if(b.state.flags&(MDL_MAPMODEL|MDL_NOSHADOW)) continue;
            bool isavatar = (b.state.flags&MDL_AVATAR) != 0;
            b.visible = dynshadow && (!noavatar || !isavatar) && (b.state.color.a >= 1 || isavatar) ? shadowmaskmodel(b.state.center, b.state.radius) : 0;
            if(b.visible && shadowculled(b.state.center, b.state.radius, b.visible)) b.visible = 0;
            b.shadowm = b.visible ? shadowbatchedlod(mb.m, b) : NULL;
        }
    }
}

VAR(IDF_PERSIST, smstaticdist, 0, 256, VAR_MAX); // how far from the camera models that stay in place can have their spinning left still in lights' cached shadows

static inline bool isbatchedstaticshadow(const batchedmodel &b)
{
    return b.state.flags&MDL_STATICSHADOW && camera1->o.dist(b.state.o) >= smstaticdist;
}

// whether a map model is moving now, where the wind leaves one out of its range exactly at rest, so lights can keep the shadows they've cached of it
static inline bool isbatchedmodelmoving(modelbatch &b, batchedmodel &bm)
{
    if(isbatchedstaticshadow(bm)) return false;
    if(b.flags&MDL_FORCEDYNAMIC || !b.m->windonly()) return true;
    if(!windmodels || bm.state.windscale <= 0) return false;
    return camera1->o.dist(bm.state.o) < getwindanimdist() + windanimfalloff + bm.state.radius;
}

// which still models are in each side of a light's shadow, for it to redo a cached side when they change, like a pickup being taken
void batchedstaticshadows(uint *sides)
{
    loopi(6) sides[i] = 0;
    loopv(batchedmodels)
    {
        batchedmodel &b = batchedmodels[i];
        if(!b.visible || b.state.flags&MDL_MAPMODEL || !isbatchedstaticshadow(b)) continue;
        uint hash = hthash(b.state.o);
        loopj(6) if(b.visible&(1<<j)) sides[j] += hash;
    }
}

int batcheddynamicmodels()
{
    int visible = 0;
    loopv(batchedmodels)
    {
        batchedmodel &b = batchedmodels[i];
        if(b.state.flags&MDL_MAPMODEL || isbatchedstaticshadow(b)) continue;
        visible |= b.visible;
    }
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!(b.flags&MDL_MAPMODEL) || !isbatchdynamic(b)) continue;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(isbatchedmodelmoving(b, bm)) visible |= bm.visible;
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
        if(b.state.flags&MDL_MAPMODEL) continue;
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
        if(!(b.flags&MDL_MAPMODEL) || !isbatchdynamic(b)) continue;
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
    static vector<model *> lods;
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(!dynmodel && (!(b.flags&MDL_MAPMODEL) || isbatchdynamic(b))) continue;
        lods.setsize(0);
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(!(bm.visible&(1<<shadowside)) || bm.state.flags&MDL_NOSHADOW) continue;
            model *m = bm.shadowm ? bm.shadowm : b.m;
            if(lods.find(m) < 0) lods.add(m);
        }
        loopvk(lods) // one run per LOD the batch's models have in this shadow
        {
            model *m = lods[k];
            if(!m->shadow || m->alphablended()) continue;
            m->startrender();
            for(int j = b.batched; j >= 0;)
            {
                batchedmodel &bm = batchedmodels[j];
                j = bm.next;
                if(bm.visible&(1<<shadowside) && !(bm.state.flags&MDL_NOSHADOW) && (bm.shadowm ? bm.shadowm : b.m) == m) renderbatchedmodel(m, bm);
            }
            m->endrender();
        }
    }
}

// models drawn straight away instead of batched, like the first person avatar, with the projection they used
struct auraimmediate
{
    model *m;
    modelstate state;
    dynent *d;
    int attached;
    matrix4 proj;
};
static vector<auraimmediate> auraimmediates;
static vector<modelattach> auraattached;

void clearauramodels()
{
    auraimmediates.setsize(0);
    auraattached.setsize(0);
}

static void addauraimmediate(model *m, const modelstate &state, dynent *d)
{
    auraimmediate &a = auraimmediates.add();
    a.m = m;
    a.state = state;
    a.d = d;
    a.proj = projmatrix;
    a.attached = state.attached ? auraattached.length() : -1;
    if(state.attached) for(int i = 0;; i++) { auraattached.add(state.attached[i]); if(!state.attached[i].tag) break; }
}

// the mask shader takes the aura colour from the third material and the on-top flag from the alpha
static void setauramask(modelstate &state, bool ontop)
{
    bvec colour = bvec::fromcolor(vec(state.aura.r, state.aura.g, state.aura.b).mul(state.aura.a).clamp(0.f, 1.f));
    loopk(MAXMDLMATERIALS) state.material[k] = colour;
    state.matbright = vec4(1, 1, 1, 1);
    state.color = vec4(1, 1, 1, ontop ? 1 : 0);
    state.effecttype = -1;
}

void renderauramaskbatches()
{
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(b.flags&MDL_MAPMODEL) continue;
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(bm.state.aura.a <= 0 || bm.state.flags&MDL_ONLYSHADOW) continue;
            // culling results stored in the batch are overwritten by shadow passes, and queries may not be ready yet
            bool ontop = (bm.state.flags&MDL_AURA_TOP) != 0;
            int cullflags = bm.state.flags&~(MDL_CULL_QUERY|(ontop ? MDL_CULL_OCCLUDED : 0));
            if(cullmodel(b.m, bm.state.center, bm.state.radius, cullflags, bm.d)) continue;
            batchedmodel mask = bm;
            setauramask(mask.state, ontop);
            if(!rendered) { b.m->startrender(); rendered = true; }
            renderbatchedmodel(b.m, mask);
        }
        if(rendered) b.m->endrender();
    }

    if(auraimmediates.empty()) return;

    loopv(auraimmediates)
    {
        auraimmediate &a = auraimmediates[i];
        modelstate mask = a.state;
        if(a.attached >= 0) mask.attached = &auraattached[a.attached];
        setauramask(mask, (mask.flags&MDL_AURA_TOP) != 0);
        projmatrix = a.proj;
        setcamprojmatrix(false);
        a.m->startrender();
        a.m->render(mask.anim|ANIM_NOSKIN, &mask, a.d);
        a.m->endrender();
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
        setaamask(isbatchdynamic(b));
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            if(!(bm.state.flags&MDL_ONLYSHADOW)) renderbatchedmodel(b.m, bm);
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

bool transdepthmodels = false;

// the depth of the models rendertransparentmodelbatches draws, for the transparent depth, whose blending keeps the nearest
void rendertransdepthmodelbatches()
{
    transdepthmodels = true;
    loopv(batches)
    {
        modelbatch &b = batches[i];
        if(b.flags&MDL_MAPMODEL) continue;
        bool rendered = false;
        for(int j = b.batched; j >= 0;)
        {
            batchedmodel &bm = batchedmodels[j];
            j = bm.next;
            if(!(bm.state.color.a < 1 || bm.state.flags&MDL_FORCETRANSPARENT) || bm.state.flags&MDL_ONLYSHADOW) continue;
            // culling results stored in the batch are overwritten by shadow passes, and queries are left to the transparent pass
            if(cullmodel(b.m, bm.state.center, bm.state.radius, bm.state.flags&~MDL_CULL_QUERY, bm.d)) continue;
            if(!rendered) { b.m->startrender(); rendered = true; }
            renderbatchedmodel(b.m, bm);
        }
        if(rendered) b.m->endrender();
    }
    transdepthmodels = false;
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
        setaamask(!(b.flags&MDL_MAPMODEL) || isbatchdynamic(b));
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

VARF(IDF_PERSIST, lodmodels, 0, 1, 1, changedshadowmeshes());
VARF(IDF_PERSIST, lodmodelshadow, 0, 1, 2, changedshadowmeshes()); // how many LODs simpler models are in shadows than their size in its texels or on screen gives
FVAR(IDF_PERSIST, lodmodelscale, 0.01f, 1, 100); // how large models count as on screen when choosing their LOD, higher keeps detail further away

// the squared pixels a unit covers at a distance of one unit, only worked out again when the view changes
static inline float lodsqpixelscale()
{
    static struct { float fovy, scale, sqpixels; int viewh; } last = { -1, -1, 0, -1 };
    if(fovy != last.fovy || viewh != last.viewh || lodmodelscale != last.scale)
    {
        last.fovy = fovy;
        last.viewh = viewh;
        last.scale = lodmodelscale;
        float pixels = lodmodelscale*max(viewh, 1)/(2*tanf(fovy/2*RAD));
        last.sqpixels = pixels*pixels;
    }
    return last.sqpixels;
}

// in a shadow pass, shadowmask has the sides the model is in, and a shadow shows no more than the texels of the finest one or the pixels it lands on
model *loadbestlod(model *m, const vec &center, float sqradius, float scale, int shadowmask)
{
    if(!lodmodels || !m || !m->haslod() || drawtex == DRAWTEX_ENVMAP) return m; // environment maps are small and kept, so their pixels don't say what's seen

    float sqdist = camera1->o.squaredist(center);
    if(sqdist <= 0) return m;

    float sqpixels = 4*sqradius*scale*scale*lodsqpixelscale()/sqdist, // the model's squared width on screen
          texels = shadowmapping ? shadowtexelscale(center, shadowmask) : 0;
    if(texels > 0) sqpixels = min(sqpixels, 4*sqradius*scale*scale*texels*texels); // and in shadow texels

    model *lm = m->bestlod(sqpixels, shadowmapping ? lodmodelshadow : 0);
    return lm ? lm : m;
}

VAR(0, showmapmodels, 0, 1, 1);

void rendermapmodel(int idx, entmodelstate &state, bool tpass)
{
    if(!mapmodels.inrange(idx) || (editmode && !showmapmodels)) return;

    mapmodelinfo &mmi = mapmodels[idx];
    model *m = mmi.m ? mmi.m : loadmodel(mmi.name);
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
        visible = shadowmaskmodel(state.center, state.radius);
        if(!visible || shadowculled(state.center, state.radius, visible)) return;
    }

    if(!(state.flags&MDL_NOLOD))
        m = loadbestlod(m, state.center, state.radius*state.radius, state.lodscale, visible);

    if(shadowmapping)
    {
        if(!m->shadow) return;
    }
    else if(state.flags&(MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED) && cullmodel(m, state.center, state.radius, state.flags))
        return;

    batchedmodel &b = batchedmodels.add();
    (entmodelstate &)b.state = state;
    b.state.flags |= tpass ? MDL_FORCETRANSPARENT : MDL_MAPMODEL;
    if(!m->shadow || m->alphablended()) b.state.flags |= MDL_NOSHADOW; // rendershadowmodelbatches skips it, so it must not redo the cached shadows of lights
    b.visible = visible;
    b.d = NULL;
    b.attached = -1;
    b.shadowm = NULL;
    addbatchedmodel(m, b, batchedmodels.length()-1);
}

void rendermodel(const char *mdl, modelstate &state, dynent *d)
{
    model *m = loadmodel(mdl);
    if(!m) return;

    if(drawtex == DRAWTEX_MODELPREVIEW)
    {
        state.flags |= MDL_NOLOD;
        state.flags &= ~(MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY);
    }

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

    if(drawtex == DRAWTEX_NONE && state.aura.a > 0 && !(state.flags&(MDL_NORENDER|MDL_ONLYSHADOW)))
        aurasurf.add(state.center, state.radius);

    if(!(state.flags&MDL_NOLOD))
        m = loadbestlod(m, state.center, state.radius*state.radius, state.lodscale);

    if(state.flags&MDL_NORENDER) state.anim |= ANIM_NORENDER;

    if(state.attached) for(int i = 0; state.attached[i].tag; i++) if(state.attached[i].name)
    {
        model *am = state.attached[i].m = loadmodel(state.attached[i].name);
        if(am && am->haslod() && !(state.flags&MDL_NOLOD))
        {
            vec acenter, aradius;
            am->boundbox(acenter, aradius);
            float asize = state.size*max(state.attached[i].sizescale, 0.f);
            state.attached[i].m = loadbestlod(am, state.o, aradius.squaredlen()*asize*asize, state.lodscale);
        }
    }

    if(state.flags&MDL_CULL_QUERY)
    {
        if(!oqfrags || !oqdynent || !d) state.flags &= ~MDL_CULL_QUERY;
    }

    if(state.flags&MDL_NOBATCH)
    {
        if(state.flags&MDL_NORENDER)
        {   // only update animation state and tags, nothing is drawn
            m->startrender();
            m->render(state.anim, &state, d);
            m->endrender();
            return;
        }
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
        if(drawtex == DRAWTEX_NONE && state.aura.a > 0) addauraimmediate(m, state, d);
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
    if(!(m->parentlod ? m->parentlod : m)->shadow || m->alphablended()) b.state.flags |= MDL_NOSHADOW; // shadows pick their own LOD, which may cast one
    b.visible = 0;
    b.shadowm = NULL;
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
    if (d->aboveeye + d->height <= 0.5f)
    {
        float zrad = (0.5f - (d->aboveeye + d->height)) / 2;
        d->aboveeye += zrad;
        d->height += zrad;
    }
}


namespace UI { extern int uihidden; }

// The model editor: a loaded model's placement and its skeleton's tags changed live, put back by clearing the model so it reloads

namespace modeledit
{
    enum { SCALE = 0, X, Y, Z, YAW, PITCH, ROLL, NUMFIELDS };
    static const char * const fieldnames[NUMFIELDS] = { "scale", "x", "y", "z", "yaw", "pitch", "roll" };
    static const char * const formatnames[NUMMODELTYPES] = { "md2", "md3", "md5", "obj", "smd", "iqm" };

    // the editor's setup, kept for next time: the host model, its attachments as model and tag pairs, its animation, and where to save
    SVAR(IDF_PERSIST, modeledithost, "actors/player/male/hwep");
    SVAR(IDF_PERSIST, modeleditattached, "");
    SVAR(IDF_PERSIST, modeleditanim, "");
    SVAR(IDF_PERSIST, modeleditfile, "mdledit.cfg");

    struct savedtag
    {
        char *name;
        int bone;
        matrix4x3 matrix;

        savedtag() : name(NULL) {}
        ~savedtag() { DELETEA(name); }
    };

    struct savedskel
    {
        skelmodel::skeleton *skel;
        bool shared; // a shared skeleton outlives its models, so is always there to put back
        vector<savedtag> tags;
    };

    struct tagangles
    {
        skelmodel::skeleton *skel;
        char *name;
        vec rot;

        tagangles() : skel(NULL), name(NULL), rot(0, 0, 0) {}
        ~tagangles() { DELETEA(name); }
    };

    struct state
    {
        vector<char *> touched; // models changed, cleared when the editor lets go of them
        vector<savedskel *> skels; // tags as they were before the editor first changed a skeleton
        vector<tagangles> angles; // the angles a tag was last given, which its matrix can't give back exactly
        bool kept, release; // whether the editor's Panel was in the interface last built, and if not, to let go of its changes

        state() : kept(false), release(false) {}
    } st;

    static int findfield(const char *name)
    {
        loopi(NUMFIELDS) if(!strcmp(fieldnames[i], name)) return i;
        return -1;
    }

    static bool canedit()
    {
        if(!curpeer) return true;
        conoutf(colourred, "The model editor can't change models while connected to a server");
        return false;
    }

    static model *getmodel(const char *name)
    {
        return name && *name ? loadmodel(name) : NULL;
    }

    static bool touched(const char *name)
    {
        loopv(st.touched) if(!strcmp(st.touched[i], name)) return true;
        return false;
    }

    static void touch(model *m)
    {
        if(!touched(m->name)) st.touched.add(newstring(m->name));
    }

    static void getskels(model *m, vector<skelmodel::skeleton *> &skels)
    {
        if(!m || !m->skeletal()) return;
        skelmodel *sm = (skelmodel *)m;
        loopv(sm->parts)
        {
            skelmodel::skelmeshgroup *g = (skelmodel::skelmeshgroup *)sm->parts[i]->meshes;
            if(g && g->skel) skels.addunique(g->skel);
        }
    }

    // the skeleton holding a tag, and the tag's index in it
    static skelmodel::skeleton *findtag(model *m, const char *name, int &idx)
    {
        vector<skelmodel::skeleton *> skels;
        getskels(m, skels);
        loopv(skels) loopvj(skels[i]->tags) if(!strcmp(skels[i]->tags[j].name, name))
        {
            idx = j;
            return skels[i];
        }
        idx = -1;
        return NULL;
    }

    static void saveskel(skelmodel::skeleton *skel)
    {
        loopv(st.skels) if(st.skels[i]->skel == skel) return;
        savedskel *s = st.skels.add(new savedskel);
        s->skel = skel;
        s->shared = skel->shared > 0;
        loopv(skel->tags)
        {
            savedtag &t = s->tags.add();
            t.name = newstring(skel->tags[i].name);
            t.bone = skel->tags[i].bone;
            t.matrix = skel->tags[i].matrix;
        }
    }

    static void restoreskel(savedskel *s)
    {
        vector<skelmodel::tag> &tags = s->skel->tags;
        loopvrev(tags)
        {
            savedtag *t = NULL;
            loopvj(s->tags) if(!strcmp(s->tags[j].name, tags[i].name)) { t = &s->tags[j]; break; }
            if(t)
            {
                tags[i].bone = t->bone;
                tags[i].matrix = t->matrix;
                continue;
            }
            // added by the editor, so close the gap; nothing keeps a tag's index between draws
            DELETEA(tags[i].name);
            for(int j = i; j < tags.length()-1; j++)
            {
                tags[j].name = tags[j+1].name;
                tags[j].bone = tags[j+1].bone;
                tags[j].matrix = tags[j+1].matrix;
            }
            tags.last().name = NULL;
            tags.drop();
        }
    }

    // whether a skeleton saved before is still there, as one a model cleared some other way took with it isn't
    static bool liveskel(savedskel *s)
    {
        if(s->shared) return true;
        bool live = false;
        enumerate(models, model *, m,
        {
            vector<skelmodel::skeleton *> skels;
            getskels(m, skels);
            if(skels.find(s->skel) >= 0) live = true;
        });
        return live;
    }

    static void clearangles(skelmodel::skeleton *skel)
    {
        loopvrev(st.angles) if(!skel || st.angles[i].skel == skel) st.angles.remove(i);
    }

    // puts back a model, or every model the editor changed
    static void reset(const char *name = NULL)
    {
        loopvrev(st.touched)
        {
            if(name && strcmp(st.touched[i], name)) continue;
            model *m = models.find(st.touched[i], NULL);
            if(m)
            {
                vector<skelmodel::skeleton *> skels;
                getskels(m, skels);
                loopvj(skels) loopvkrev(st.skels) if(st.skels[k]->skel == skels[j])
                {
                    restoreskel(st.skels[k]);
                    clearangles(skels[j]);
                    delete st.skels.remove(k);
                }
            }
            clearmodel(st.touched[i], false);
            delete[] st.touched.remove(i);
        }
        if(!name)
        {
            loopv(st.skels) if(liveskel(st.skels[i])) restoreskel(st.skels[i]);
            st.skels.deletecontents();
            clearangles(NULL);
        }
    }

    static float getvalue(model *m, int field)
    {
        bool flip = ((animmodel *)m)->flipy(); // these formats' loading turned mdltrans's y around
        switch(field)
        {
            case SCALE: return m->scale*100;
            case X: return m->translate.x;
            case Y: return flip ? -m->translate.y : m->translate.y;
            case Z: return m->translate.z;
            case YAW: return m->offsetyaw;
            case PITCH: return m->offsetpitch;
            case ROLL: return m->offsetroll;
        }
        return 0;
    }

    static void setvalue(model *m, int field, float val)
    {
        bool flip = ((animmodel *)m)->flipy();
        switch(field)
        {
            case SCALE: m->scale = val > 0 ? val/100 : 1; break;
            case X: m->translate.x = val; break;
            case Y: m->translate.y = flip ? -val : val; break;
            case Z: m->translate.z = val; break;
            case YAW: m->offsetyaw = val; break;
            case PITCH: m->offsetpitch = val; break;
            case ROLL: m->offsetroll = val; break;
            default: return;
        }
        m->bbradius = m->collideradius = vec(-1, -1, -1); // worked out again from the new placement
    }

    // the angles iqmtag takes, the inverse of how it builds the tag's rotation
    static vec tagrot(skelmodel::skeleton *skel, const skelmodel::tag &t)
    {
        loopv(st.angles) if(st.angles[i].skel == skel && !strcmp(st.angles[i].name, t.name)) return st.angles[i].rot;
        quat q(t.matrix);
        float rx = atan2f(2*(q.w*q.x + q.y*q.z), 1 - 2*(q.x*q.x + q.y*q.y)),
              ry = asinf(clamp(2*(q.w*q.y - q.z*q.x), -1.0f, 1.0f)),
              rz = atan2f(2*(q.w*q.z + q.x*q.y), 1 - 2*(q.y*q.y + q.z*q.z));
        vec rot(rx/RAD, ry/RAD, rz/RAD);
        loopk(3) rot[k] = roundf(rot[k]*1000)/1000 + 0.0f; // no float noise or negative zero in what's saved
        return rot;
    }

    static void settagrot(skelmodel::skeleton *skel, skelmodel::tag &t, const vec &rot)
    {
        tagangles *a = NULL;
        loopv(st.angles) if(st.angles[i].skel == skel && !strcmp(st.angles[i].name, t.name)) { a = &st.angles[i]; break; }
        if(!a)
        {
            a = &st.angles.add();
            a->skel = skel;
            a->name = newstring(t.name);
        }
        a->rot = rot;
        float cx = cosf(rot.x/2*RAD), sx = sinf(rot.x/2*RAD), cy = cosf(rot.y/2*RAD), sy = sinf(rot.y/2*RAD), cz = cosf(rot.z/2*RAD), sz = sinf(rot.z/2*RAD);
        t.matrix = matrix4x3(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)), t.matrix.d);
    }

    // the lines to paste into the model's configuration for what's on screen
    static void writemodel(model *m, vector<char> &buf)
    {
        defformatstring(line, "// %s\n", m->name);
        buf.put(line, strlen(line));
        formatstring(line, "mdlscale %s\n", floatstr(getvalue(m, SCALE)));
        buf.put(line, strlen(line));
        formatstring(line, "mdltrans %s", floatstr(getvalue(m, X)));
        concformatstring(line, " %s", floatstr(getvalue(m, Y)));
        concformatstring(line, " %s\n", floatstr(getvalue(m, Z)));
        buf.put(line, strlen(line));
        formatstring(line, "mdlyaw %s\nmdlpitch %s\nmdlroll %s\n", floatstr(getvalue(m, YAW)), floatstr(getvalue(m, PITCH)), floatstr(getvalue(m, ROLL)));
        buf.put(line, strlen(line));
        vector<skelmodel::skeleton *> skels;
        getskels(m, skels);
        loopv(skels) loopvj(skels[i]->tags)
        {
            skelmodel::tag &t = skels[i]->tags[j];
            vec rot = tagrot(skels[i], t);
            const char *bone = skels[i]->bones[t.bone].name;
            formatstring(line, "%stag %s", formatnames[m->type()], escapestring(bone ? bone : ""));
            concformatstring(line, " %s", escapestring(t.name));
            concformatstring(line, " %s", floatstr(t.matrix.d.x));
            concformatstring(line, " %s", floatstr(t.matrix.d.y));
            concformatstring(line, " %s", floatstr(t.matrix.d.z));
            concformatstring(line, " %s", floatstr(rot.x));
            concformatstring(line, " %s", floatstr(rot.y));
            concformatstring(line, " %s\n", floatstr(rot.z));
            buf.put(line, strlen(line));
        }
    }

    // after the interface is built: an interface without the editor's Panel lets go of its changes
    void built()
    {
        if(UI::uihidden) return; // hiding the interface for a screenshot isn't leaving the editor
        if(!st.kept && (st.touched.length() || st.skels.length())) st.release = true;
        st.kept = false;
    }

    // once a frame, outside drawing, so nothing being drawn has its model cleared
    void update()
    {
        if(st.touched.empty() && st.skels.empty()) st.release = false;
        else if(curpeer || st.release)
        {
            reset();
            st.release = false;
        }
    }

    ICOMMAND(0, modeleditkeep, "", (), st.kept = true);
    ICOMMAND(0, modeleditreset, "", (), reset());
    ICOMMAND(0, modeleditrevert, "s", (char *name), reset(name));
    ICOMMAND(0, modeleditchanged, "s", (char *name), intret(touched(name) ? 1 : 0));

    ICOMMAND(0, modeleditformat, "s", (char *name),
    {
        model *m = getmodel(name);
        result(m ? formatnames[m->type()] : "");
    });

    ICOMMAND(0, modeleditget, "ss", (char *name, char *field),
    {
        model *m = getmodel(name);
        int f = findfield(field);
        floatret(m && f >= 0 ? getvalue(m, f) : 0);
    });

    ICOMMAND(0, modeleditset, "ssf", (char *name, char *field, float *val),
    {
        model *m = getmodel(name);
        int f = findfield(field);
        if(!m || f < 0 || !canedit()) return;
        touch(m);
        setvalue(m, f, *val);
    });

    ICOMMAND(0, modeledittags, "s", (char *name),
    {
        model *m = getmodel(name);
        vector<char> buf;
        if(m)
        {
            animmodel *am = (animmodel *)m;
            vector<const char *> names;
            loopv(am->parts) if(am->parts[i]->meshes)
            {
                const char *tag;
                for(int j = 0; (tag = am->parts[i]->meshes->tagname(j)); j++) names.addunique(tag);
            }
            loopv(names)
            {
                if(buf.length()) buf.add(' ');
                const char *s = escapestring(names[i]);
                buf.put(s, strlen(s));
            }
        }
        buf.add('\0');
        result(buf.getbuf());
    });

    ICOMMAND(0, modeleditbones, "s", (char *name),
    {
        vector<skelmodel::skeleton *> skels;
        getskels(getmodel(name), skels);
        vector<char> buf;
        loopv(skels) loopj(skels[i]->numbones) if(skels[i]->bones[j].name)
        {
            if(buf.length()) buf.add(' ');
            const char *s = escapestring(skels[i]->bones[j].name);
            buf.put(s, strlen(s));
        }
        buf.add('\0');
        result(buf.getbuf());
    });

    // bone, x, y, z, rx, ry, rz, or editable for whether the editor can change it
    ICOMMAND(0, modeledittagget, "sss", (char *name, char *tag, char *field),
    {
        int idx = -1;
        skelmodel::skeleton *skel = findtag(getmodel(name), tag, idx);
        if(!strcmp(field, "editable")) { intret(skel ? 1 : 0); return; }
        if(!skel) { if(!strcmp(field, "bone")) result(""); else floatret(0); return; }
        skelmodel::tag &t = skel->tags[idx];
        if(!strcmp(field, "bone")) { result(skel->bones[t.bone].name ? skel->bones[t.bone].name : ""); return; }
        vec rot = tagrot(skel, t);
        if(!strcmp(field, "x")) floatret(t.matrix.d.x);
        else if(!strcmp(field, "y")) floatret(t.matrix.d.y);
        else if(!strcmp(field, "z")) floatret(t.matrix.d.z);
        else if(!strcmp(field, "rx")) floatret(rot.x);
        else if(!strcmp(field, "ry")) floatret(rot.y);
        else if(!strcmp(field, "rz")) floatret(rot.z);
        else floatret(0);
    });

    ICOMMAND(0, modeledittagset, "sssf", (char *name, char *tag, char *field, float *val),
    {
        model *m = getmodel(name);
        int idx = -1;
        skelmodel::skeleton *skel = findtag(m, tag, idx);
        if(!skel || !canedit()) return;
        saveskel(skel);
        touch(m);
        skelmodel::tag &t = skel->tags[idx];
        vec rot = tagrot(skel, t);
        if(!strcmp(field, "x")) t.matrix.d.x = *val;
        else if(!strcmp(field, "y")) t.matrix.d.y = *val;
        else if(!strcmp(field, "z")) t.matrix.d.z = *val;
        else if(!strcmp(field, "rx")) { rot.x = *val; settagrot(skel, t, rot); }
        else if(!strcmp(field, "ry")) { rot.y = *val; settagrot(skel, t, rot); }
        else if(!strcmp(field, "rz")) { rot.z = *val; settagrot(skel, t, rot); }
    });

    ICOMMAND(0, modeledittagadd, "sss", (char *name, char *bone, char *tag),
    {
        model *m = getmodel(name);
        vector<skelmodel::skeleton *> skels;
        getskels(m, skels);
        if(skels.empty()) { conoutf(colourred, "Only a skeletal model can have tags added"); intret(0); return; }
        if(!*tag) { conoutf(colourred, "A tag needs a name"); intret(0); return; }
        if(!canedit()) { intret(0); return; }
        loopv(skels) if(skels[i]->findtag(tag) >= 0) { conoutf(colourred, "Model %s already has a tag called %s", m->name, tag); intret(0); return; }
        loopv(skels)
        {
            int b = skels[i]->findbone(bone);
            if(b < 0) continue;
            saveskel(skels[i]);
            touch(m);
            matrix4x3 ident;
            ident.identity();
            skelmodel::tag &t = skels[i]->tags.add();
            t.name = newstring(tag);
            t.bone = b;
            t.matrix = ident;
            intret(1);
            return;
        }
        conoutf(colourred, "Model %s has no bone called %s", m->name, bone);
        intret(0);
    });

    ICOMMAND(0, modeleditanims, "s", (char *name),
    {
        model *m = getmodel(name);
        vector<char> buf;
        if(m)
        {
            animmodel *am = (animmodel *)m;
            loopi(game::numanims())
            {
                bool has = false;
                loopvj(am->parts) loopk(am->parts[j]->numanimparts) if(am->parts[j]->anims[k] && am->parts[j]->anims[k][i].length()) has = true;
                const char *anim = has ? game::animname(i) : NULL;
                if(!anim || !*anim) continue;
                if(buf.length()) buf.add(' ');
                const char *s = escapestring(anim);
                buf.put(s, strlen(s));
            }
        }
        buf.add('\0');
        result(buf.getbuf());
    });

    ICOMMAND(0, modeleditanimlength, "ss", (char *name, char *anim),
    {
        model *m = getmodel(name);
        vector<int> anims;
        if(m && *anim) game::findanims(anim, anims);
        intret(anims.length() ? m->animlength(anims[0]) : 0);
    });

    // the lines for each model on screen, to paste into its configuration
    ICOMMAND(0, modeleditcopy, "s", (char *name),
    {
        model *m = getmodel(name);
        if(!m) return;
        vector<char> buf;
        writemodel(m, buf);
        buf.add('\0');
        SDL_SetClipboardText(buf.getbuf());
    });

    ICOMMAND(0, modeleditsave, "ss", (char *file, char *names),
    {
        if(!*file) file = *modeleditfile ? modeleditfile : (char *)"mdledit.cfg";
        vector<char *> list;
        explodelist(names, list);
        vector<char> buf;
        loopv(list)
        {
            model *m = getmodel(list[i]);
            if(!m) continue;
            if(buf.length()) buf.add('\n');
            writemodel(m, buf);
        }
        list.deletearrays();
        stream *f = openutf8file(file, "w");
        if(!f) { conoutf(colourred, "Could not write %s", file); intret(0); return; }
        f->write(buf.getbuf(), buf.length());
        delete f;
        conoutf(colourwhite, "Saved model placement to %s", file);
        intret(1);
    });
}
