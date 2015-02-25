#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "cube.h"

#define LAN_PORT 28799
#define MASTER_PORT 28800
#define SERVER_PORT 28801

extern int versioning, versionmajor, versionminor, versionpatch, versionplatform, versionarch, versionisserver;
extern char *versionstring, *versionname, *versionuname, *versionrelease, *versionurl, *versionmaster, *versionplatname, *versionplatlongname;
extern uint versioncrc;
#define CUR_VER_MAKE(a,b,c) (((a)<<16) | ((b)<<8) | (c))
#define CUR_VER CUR_VER_MAKE(versionmajor, versionminor, versionpatch)
#define CUR_VERSION (versionmajor*100)+(versionminor*10)+versionpatch

#ifdef WIN32
#define CUR_PLATFORM 0
#define CUR_PLATID
#elif defined(__APPLE__)
#define CUR_PLATFORM 1
#else
#define CUR_PLATFORM 2
#endif
#define CUR_ARCH (int(8*sizeof(void *)))

#define MAX_PLATFORMS 3

#define sup_platform(a) (a >= 0 && a < MAX_PLATFORMS)
#define sup_arch(a) (a == 32 || a == 64)

extern const char *platnames[MAX_PLATFORMS], *platlongnames[MAX_PLATFORMS];
#define plat_name(a) (sup_platform(a) ? platnames[a] : "unk")
#define plat_longname(a) (sup_platform(a) ? platlongnames[a] : "unknown")

#ifdef STANDALONE
extern void setupmaster();
extern void checkmaster();
extern void cleanupmaster();
extern void reloadmaster();
extern int masterserver, masterport;
extern char *masterip;
extern int nextcontrolversion();
#endif

extern void setcrc(const char *bin);

#include "irc.h"
#include "sound.h"

enum { CON_DEBUG = 0, CON_MESG, CON_INFO, CON_SELF, CON_GAMESPECIFIC };

enum { PACKAGEDIR_OCTA = 1<<0 };
extern const char * const disc_reasons[];
struct ipinfo
{
    enum { ALLOW = 0, BAN, MUTE, LIMIT, EXCEPT, MAXTYPES };
    enum { TEMPORARY = 0, LOCAL, GLOBAL };
    enet_uint32 ip, mask;
    int type, flag, time, version;
    char *reason;

    ipinfo() : ip(0), mask(0), type(-1), flag(TEMPORARY), time(-1), version(-1), reason(NULL) {}
    ~ipinfo()
    {
        if(reason) delete[] reason;
    }
};
extern vector<ipinfo> control;
extern const char *ipinfotypes[ipinfo::MAXTYPES];
extern void addipinfo(vector<ipinfo> &info, int type, const char *name, const char *reason = NULL);
extern char *printipinfo(const ipinfo &info, char *buf = NULL);
extern ipinfo *checkipinfo(vector<ipinfo> &info, int type, enet_uint32 ip);
extern void writecfg();
extern void rehash(bool reload = true);

#ifndef STANDALONE
#include "world.h"
#include "glexts.h"
#include "octa.h"
#include "lightmap.h"
#include "bih.h"
#include "texture.h"
#include "model.h"
#include "varray.h"

extern physent *camera1, camera;
extern mapz hdr;
extern int worldscale, octaentsize;
extern vector<ushort> texmru;
extern int xtraverts, xtravertsva;
extern const ivec cubecoords[8];
extern const ivec facecoords[6][4];
extern const uchar fv[6][4];
extern const uchar fvmasks[64];
extern const uchar faceedgesidx[6][4];
extern bool inbetweenframes, renderedframe;

extern SDL_Surface *screen;
extern int zpass, glowpass;

// rendertext
struct font
{
    struct charinfo
    {
        short x, y, w, h, offsetx, offsety, advance, tex;
    };

    char *name;
    vector<Texture *> texs;
    vector<charinfo> chars;
    int charoffset, defaultw, defaulth, maxw, maxh, scale;

    font() : name(NULL) {}
    ~font() { DELETEA(name); }
};

extern float textscale;
#define FONTH int(curfont->scale*textscale)
#define FONTW (FONTH/2)
#define FONTTAB (4*FONTW)

extern font *curfont;

// texture
extern int hwtexsize, hwcubetexsize, hwmaxaniso, maxtexsize, aniso, envmapradius;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern int texalign(void *data, int w, int bpp);
extern void cleanuptexture(Texture *t);
extern void loadalphamask(Texture *t);
extern void loadlayermasks();
extern GLuint cubemapfromsky(int size);
extern Texture *cubemapload(const char *name, bool mipit = true, bool msg = true, bool transient = false);
extern void drawcubemap(int size, int level, const vec &o, float yaw, float pitch, bool flipx, bool flipy, bool swapxy);
extern void loadshaders();
extern void setuptexparameters(int tnum, void *pixels, int clamp, int filter, GLenum format = GL_RGB, GLenum target = GL_TEXTURE_2D);
extern void createtexture(int tnum, int w, int h, void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE);
extern void blurtexture(int n, int bpp, int w, int h, uchar *dst, const uchar *src, int margin = 0);
extern void blurnormals(int n, int w, int h, bvec *dst, const bvec *src, int margin = 0);
extern void renderpostfx();
extern void initenvmaps();
extern void genenvmaps();
extern ushort closestenvmap(const vec &o);
extern ushort closestenvmap(int orient, int x, int y, int z, int size);
extern GLuint lookupenvmap(ushort emid);
extern GLuint lookupenvmap(Slot &slot);
extern bool reloadtexture(Texture *t);
extern bool reloadtexture(const char *name);
extern void setuptexcompress();
extern void clearslots();
extern void compacteditvslots();
extern void compactmruvslots();
extern void compactvslot(int &index);
extern void compactvslots(cube *c, int n = 8);
extern int compactvslots(bool cull = false);

// shader

extern int useshaders, shaderprecision;

// shadowmap

extern int shadowmap, shadowmapcasters;
extern bool shadowmapping;

extern bool isshadowmapcaster(const vec &o, float rad);
extern bool addshadowmapcaster(const vec &o, float xyrad, float zrad);
extern bool isshadowmapreceiver(vtxarray *va);
extern void rendershadowmap();
extern void pushshadowmap();
extern void popshadowmap();
extern void rendershadowmapreceivers();
extern void guessshadowdir();

// pvs
extern void clearpvs();
extern bool pvsoccluded(const ivec &bbmin, const ivec &bbmax);
extern bool pvsoccludedsphere(const vec &center, float radius);
extern bool waterpvsoccluded(int height);
extern void setviewcell(const vec &p);
extern void savepvs(stream *f);
extern void loadpvs(stream *f);
extern int getnumviewcells();

static inline bool pvsoccluded(const ivec &bborigin, int size)
{
    return pvsoccluded(bborigin, ivec(bborigin).add(size));
}

// rendergl
extern bool hasVBO, hasDRE, hasOQ, hasTR, hasFBO, hasDS, hasTF, hasBE, hasBC, hasCM, hasNP2, hasTC, hasS3TC, hasFXT1, hasTE, hasMT, hasD3, hasAF, hasVP2, hasVP3, hasPP, hasMDA, hasTE3, hasTE4, hasVP, hasFP, hasGLSL, hasGM, hasNVFB, hasSGIDT, hasSGISH, hasDT, hasSH, hasNVPCF, hasRN, hasPBO, hasFBB, hasUBO, hasBUE, hasMBR, hasFC, hasTEX;
extern int hasstencil;
extern int glversion, glslversion;
extern char *gfxvendor, *gfxexts, *gfxrenderer, *gfxversion;

extern bool envmapping, minimapping, renderedgame, modelpreviewing;
extern const glmatrixf viewmatrix;
extern glmatrixf mvmatrix, projmatrix, mvpmatrix, invmvmatrix, invmvpmatrix, fogmatrix, invfogmatrix, envmatrix;
extern bvec fogcolor;

extern float cursorx, cursory;
extern vec cursordir;

extern GLenum colormask[3];
#define COLORMASK colormask[0], colormask[1], colormask[2]
#define SAVECOLORMASK \
    GLenum oldcolormask[3]; \
    memcpy(oldcolormask, colormask, sizeof(oldcolormask)); \
    setcolormask(); \
    if(memcmp(colormask, oldcolormask, sizeof(oldcolormask))) glColorMask(COLORMASK, GL_TRUE);
#define RESTORECOLORMASK \
    if(memcmp(colormask, oldcolormask, sizeof(oldcolormask))) \
    { \
        memcpy(colormask, oldcolormask, sizeof(oldcolormask)); \
        glColorMask(COLORMASK, GL_TRUE); \
    }

extern void gl_checkextensions();
extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void cleangl();

extern void vecfromcursor(float x, float y, float z, vec &dir);
extern bool vectocursor(const vec &v, float &x, float &y, float &z, float clampxy = -1);
extern void findorientation(vec &o, float yaw, float pitch, vec &pos);
extern void rendergame();
extern void renderavatar(bool early = false, bool project = false);
extern void viewproject(float zscale = 1);
extern void invalidatepostfx();
extern void drawnoview();
extern bool hasnoview();
extern void gl_drawframe(int w, int h);
extern void drawminimap();
extern void drawtextures();
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type);
extern void calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2);
extern int pushscissor(float sx1, float sy1, float sx2, float sy2);
extern void popscissor();
extern void setcolormask(bool r = true, bool g = true, bool b = true);

namespace modelpreview
{
    extern void start(bool background = true);
    extern void end();
}

// renderextras
extern void render3dbox(vec &o, float tofloor, float toceil, float xradius, float yradius = 0);
extern void renderellipse(vec &o, float xradius, float yradius, float yaw);

// octa
extern cube *newcubes(uint face = F_EMPTY, int mat = MAT_AIR);
extern cubeext *growcubeext(cubeext *ext, int maxverts);
extern void setcubeext(cube &c, cubeext *ext);
extern cubeext *newcubeext(cube &c, int maxverts = 0, bool init = true);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, const ivec &p);
extern int familysize(const cube &c);
extern void freeocta(cube *c);
extern void discardchildren(cube &c, bool fixtex = false, int depth = 0);
extern void optiface(uchar *p, cube &c);
extern void validatec(cube *c, int size = 0);
extern bool isvalidcube(const cube &c);
extern ivec lu;
extern int lusize;
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0, ivec &ro = lu, int &rsize = lusize);
extern const cube *neighbourstack[32];
extern int neighbourdepth;
extern const cube &neighbourcube(const cube &c, int orient, int x, int y, int z, int size, ivec &ro = lu, int &rsize = lusize);
extern int lookupmaterial(const vec &o);
extern void resetclipplanes();
extern int getmippedtexture(const cube &p, int orient);
extern void forcemip(cube &c, bool fixtex = true);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern void edgespan2vectorcube(cube &c);
extern int faceconvexity(const ivec v[4]);
extern int faceconvexity(const ivec v[4], int &vis);
extern int faceconvexity(const vertinfo *verts, int numverts, int size);
extern int faceconvexity(const cube &c, int orient);
extern void calcvert(const cube &c, int x, int y, int z, int size, ivec &vert, int i, bool solid = false);
extern void calcvert(const cube &c, int x, int y, int z, int size, vec &vert, int i, bool solid = false);
extern uint faceedges(const cube &c, int orient);
extern bool collapsedface(const cube &c, int orient);
extern bool touchingface(const cube &c, int orient);
extern bool flataxisface(const cube &c, int orient);
extern bool collideface(const cube &c, int orient);
extern int genclipplane(const cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(const cube &c, int x, int y, int z, int size, clipplanes &p, bool collide = true);
extern bool visibleface(const cube &c, int orient, int x, int y, int z, int size, ushort mat = MAT_AIR, ushort nmat = MAT_AIR, ushort matmask = MATF_VOLUME);
extern int classifyface(const cube &c, int orient, int x, int y, int z, int size);
extern int visibletris(const cube &c, int orient, int x, int y, int z, int size, ushort nmat = MAT_ALPHA, ushort matmask = MAT_ALPHA);
extern int visibleorient(const cube &c, int orient);
extern void genfaceverts(const cube &c, int orient, ivec v[4]);
extern int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts);
extern void invalidatemerges(cube &c, const ivec &co, int size, bool msg);
extern void calcmerges();

extern int mergefaces(int orient, facebounds *m, int sz);
extern void mincubeface(const cube &cu, int orient, const ivec &o, int size, const facebounds &orig, facebounds &cf, ushort nmat = MAT_AIR, ushort matmask = MATF_VOLUME);

static inline bool insideworld(const vec &o, bool zup = true)
{
    return o.x>=0 && o.x<hdr.worldsize && o.y>=0 && o.y<hdr.worldsize && o.z>=0 && (!zup || o.z<hdr.worldsize);
}

static inline bool insideworld(const ivec &o, bool zup = true)
{
    return uint(o.x)<uint(hdr.worldsize) && uint(o.y)<uint(hdr.worldsize) && (!zup || uint(o.z)<uint(hdr.worldsize));
}

static inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

// ents
extern bool haveselent();
extern undoblock *copyundoents(undoblock *u);
extern void pasteundoents(undoblock *u);

// octaedit
extern selinfo sel;
extern int texpaneltimer;
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern void addundo(undoblock *u);
extern void commitchanges(bool force = false);
extern editinfo *localedit;

// octarender
extern vector<tjoint> tjoints;

extern ushort encodenormal(const vec &n);
extern vec decodenormal(ushort norm);
extern void reduceslope(ivec &n);
extern void findtjoints();
extern void octarender();
extern void allchanged(bool load = false);
extern void clearvas(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va, bool reparent = true);
extern bool readva(vtxarray *va, ushort *&edata, uchar *&vdata);
extern void updatevabb(vtxarray *va, bool force = false);
extern void updatevabbs(bool force = false);

// renderva
extern int outline, blankgeom;
extern void visiblecubes(bool cull = true);
extern void setvfcP(float z = -1, const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));
extern void savevfcP();
extern void restorevfcP();
extern void rendergeom(float causticspass = 0, bool fogpass = false);
extern void renderalphageom(bool fogpass = false);
extern void rendermapmodels();
extern void renderreflectedgeom(bool causticspass = false, bool fogpass = false);
extern void renderreflectedmapmodels();
extern void renderoutline();
extern bool rendersky(bool explicitonly = false);

extern bool isfoggedsphere(float rad, const vec &cv);
extern int isvisiblesphere(float rad, const vec &cv);
extern bool bboccluded(const ivec &bo, const ivec &br);
extern occludequery *newquery(void *owner);
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();
extern void drawbb(const ivec &bo, const ivec &br, const vec &camera = camera1->o);

#define startquery(query) { glBeginQuery_(GL_SAMPLES_PASSED_ARB, ((occludequery *)(query))->id); }
#define endquery(query) \
    { \
        glEndQuery_(GL_SAMPLES_PASSED_ARB); \
        extern int ati_oq_bug; \
        if(ati_oq_bug) glFlush(); \
    }

// dynlight

extern void updatedynlights();
extern int finddynlights();
extern void calcdynlightmask(vtxarray *va);
extern int setdynlights(vtxarray *va);
extern bool getdynlight(int n, vec &o, float &radius, vec &color);

// material

extern int showmat;

extern const char *findmaterialname(int type);
extern int findmaterial(const char *name, bool tryint = false);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, int cx, int cy, int cz, int size, vector<materialsurface> &matsurfs);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void rendermatgrid(materialsurface *matbuf, int matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern void rendermaterials();
extern int visiblematerial(const cube &c, int orient, int x, int y, int z, int size, ushort matmask = MATF_VOLUME);

// water
extern int refracting, refractfog;
extern bvec refractcolor;
extern bool reflecting, fading, fogging;
extern float reflectz;
extern int reflectdist, vertwater, waterrefract, waterreflect, waterfade, caustics, waterfallrefract;

#define GETMATIDXVAR(name, var, type) \
    type get##name##var(int mat) \
    { \
        switch(mat&MATF_INDEX) \
        { \
            default: case 0: return name##var; \
            case 1: return name##2##var; \
            case 2: return name##3##var; \
            case 3: return name##4##var; \
        } \
    }

extern const bvec &getwatercol(int mat);
extern const bvec &getwaterfallcol(int mat);
extern int getwaterfog(int mat);
extern const bvec &getlavacol(int mat);
extern int getlavafog(int mat);
extern const bvec &getglasscol(int mat);

extern void cleanreflections();
extern void queryreflections();
extern void drawreflections();
extern void renderwater();
extern void renderlava(const materialsurface &m, Texture *tex, float scale);
extern void loadcaustics(bool force = false);
extern void preloadwatershaders(bool force = false);

// glare
extern bool glaring;

extern void drawglaretex();
extern void addglare();

// depthfx
extern bool depthfxing;

extern void drawdepthfxtex();

// server
extern const char *timestr(int dur, int style = 0);
extern vector<char *> gameargs;
extern void initgame();
extern void changeservertype();
extern void cleanupserver();
extern void serverslice(uint timeout = 0);
extern int updatetimer(bool limit);

extern void retrieveservers(vector<char> &data);
extern void localclienttoserver(int chan, ENetPacket *);
extern void lanconnect();
extern bool serveroption(char *opt);
extern void localconnect(bool force = true);
extern void localdisconnect();

// serverbrowser
extern void addserver(const char *name, int port, int priority = 0, const char *desc = NULL);

// client
extern char *connectname;
extern int connectport;
extern void localservertoclient(int chan, ENetPacket *packet);
extern bool connected(bool attempt = true, bool local = true);
extern void connectserv(const char *name = NULL, int port = MASTER_PORT, const char *password = NULL);
extern void reconnect(const char *pass);
extern void lanconnect();
extern void abortconnect(bool msg = true);
extern void clientkeepalive();
extern ENetHost *clienthost;
extern ENetPeer *curpeer, *connpeer;

// console
#ifdef __APPLE__
    #define MOD_KEYS (KMOD_LMETA|KMOD_RMETA)
    #define MOD_ALTS MOD_KEYS
#else
    #define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
    #define MOD_ALTS (KMOD_LALT|KMOD_RALT)
#endif

extern void writebinds(stream *f);
extern void writecompletions(stream *f);
extern const char *addreleaseaction(char *s);
extern const char *getkeyname(int code);
extern int findkeycode(char *key);

extern int uimillis, commandmillis,  commandpos, commandcolour, completeoffset, completesize;
extern bigstring commandbuf;
extern char *commandaction, *commandicon;
extern bool fullconsole;
// main
extern void quit();
enum
{
    NOT_INITING = 0,
    INIT_LOAD,
    INIT_RESET,
    INIT_DEFAULTS
};
extern int initing, fullscreen, numcpus;
void setfullscreen(bool enable, bool force = false);
extern bool progressing, pixeling;
extern float loadprogress, progresspart, progressamt;
extern char *progresstitle, *progresstext;
extern void progress(float bar1 = 0, const char *text1 = NULL, float bar2 = 0, const char *text2 = NULL);
extern void limitfps(int &millis, int curmillis);

enum
{
    CHANGE_GFX   = 1<<0,
    CHANGE_SOUND = 1<<1
};
extern bool initwarning(const char *desc, int level = INIT_RESET, int type = CHANGE_GFX, bool force = false);

extern bool minimized;

extern void resetcursor(bool warp = true, bool reset = true);
extern int compresslevel, imageformat;

extern void pushevent(const SDL_Event &e);
extern bool interceptkey(int sym, int mod = 0);
extern void getfps(int &fps, int &bestdiff, int &worstdiff);
extern void swapbuffers(bool overlay = true);
extern int getclockmillis();

// menu
extern float menuscale;

extern void menuprocess();
extern void addchange(const char *desc, int type, bool force = false);
extern void clearchanges(int type);

// physics
extern bool pointincube(const clipplanes &p, const vec &v);
extern bool overlapsdynent(const vec &o, float radius);
extern void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll = 0);
extern float shadowray(const vec &o, const vec &ray, float radius, int mode, extentity *t = NULL);
struct ShadowRayCache;
extern ShadowRayCache *newshadowraycache();
extern void freeshadowraycache(ShadowRayCache *&cache);
extern void resetshadowraycache(ShadowRayCache *cache);
extern float shadowray(ShadowRayCache *cache, const vec &o, const vec &ray, float radius, int mode, extentity *t = NULL);
extern bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy);

// worldio
extern char *maptitle, *mapauthor, *mapname;
extern int getmapversion();
extern int getmaprevision();


// world
extern vector<int> outsideents;

extern void entcancel();
extern void entitiesinoctanodes();
extern void attachentities();
extern void freeoctaentities(cube &c);
extern bool pointinsel(const selinfo &sel, const vec &o);

extern void clearworldvars(bool msg = false);
extern void resetmap(bool empty);

// rendermodel
struct mapmodelinfo { string name; model *m; };
extern vector<mapmodelinfo> mapmodels;
extern void mmodel(char *name);
extern void resetmapmodels();

extern bool matchanim(const char *name, const char *pattern);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern mapmodelinfo *getmminfo(int i);
extern void startmodelquery(occludequery *query);
extern void endmodelquery();
extern void preloadmodelshaders();
extern void preloadmodel(const char *name);
extern void flushpreloadedmodels(bool msg = true);
extern void preloadusedmapmodels(bool msg = false, bool bih = false);

static inline model *loadmapmodel(int n)
{
    extern vector<mapmodelinfo> mapmodels;
    if(mapmodels.inrange(n))
    {
        model *m = mapmodels[n].m;
        return m ? m : loadmodel(NULL, n);
    }
    return NULL;
}

// renderparticles
extern void particleinit();
extern void clearparticles();
extern void makeparticle(const vec &o, attrvector &attr);
extern void updateparticles();
extern void renderparticles(bool mainpass = false);


// decal
extern void initdecals();
extern void cleardecals();
extern void renderdecals(bool mainpass = false);

// blob

enum
{
    BLOB_STATIC = 0,
    BLOB_DYNAMIC
};

extern int showblobs;

extern void initblobs(int type = -1);
extern void resetblobs();
extern void renderblob(int type, const vec &o, float radius, float fade = 1);
extern void flushblobs();

// rendersky
extern int explicitsky;
extern double skyarea;

extern void drawskybox(int farplane, bool limited);
extern bool limitsky();

// gui
extern int mouseaction[2];
extern bool guiactionon;

extern int guishadow, guiclicktab, guitextblend, guitextfade, guisepsize, guiscaletime,
    guibgcolour, guibordercolour, guihovercolour, guitooltips, guitooltiptime,
    guitooltipcolour, guitooltipbordercolour, guifieldbgcolour, guifieldbordercolour, guifieldactivecolour, guiactivecolour;
extern float guibgblend, guiborderblend, guihoverscale, guihoverblend, guitooltipblend, guitooltipborderblend, guifieldbgblend, guifieldborderblend, guifieldactiveblend, guiactiveblend;

extern void progressmenu();
extern void mainmenu();
extern void texturemenu();
extern bool menuactive();
extern int cleargui(int n = 0, bool skip = true);

#define uipad(parent,count,body) { (parent).space(count); body; (parent).space(count); }
#define uifont(parent,font,body) { (parent).pushfont(font); body; (parent).popfont(); }
#define uicenter(parent, body) { (parent).spring(); body; (parent).spring(); }
#define uilistv(parent,count,body) \
{ \
    loop(uilistv##__LINE__, count) (parent).pushlist(); \
    body; \
    loop(uilistv##__LINE__, count) (parent).poplist(); \
}
#define uilist(parent,body) uilistv(parent, 1, body)
#define uicenterlistv(parent,count,body) uilistv(parent, count, uicenter(parent, body))
#define uicenterlist(parent,body) uicenterlistv(parent, 1, body)

// octaedit
extern void replacetexcube(cube &c, int oldtex, int newtex);

// skybox
extern void loadsky(char *basename);

// main
extern void setcaption(const char *text = "", const char *text2 = "");
extern int grabinput, colorpos, curfps, bestfps, worstfps, bestfpsdiff, worstfpsdiff, maxfps;

// editing
extern int getmatvec(vec v);
extern int fullbright, fullbrightlevel;
extern vector<int> entgroup;

extern int newentity(int type, const attrvector &attrs);
extern int newentity(const vec &v, int type, const attrvector &attrs);

// menu
enum { MN_BACK = 0, MN_INPUT, MN_MAX };

// console
#define MAXCONLINES 1000
struct cline { char *cref; int type, reftime, outtime, realtime; };
extern reversequeue<cline, MAXCONLINES> conlines;
extern void conline(int type, const char *sf, int n);

// rendergl
extern int dynentsize, watercolour, lavacolour, fog, fogcolour;
extern bvec ambientcolor, skylightcolor;
extern float curfov, fovy, aspect, forceaspect;

extern void project(float fovy, float aspect, int farplane, bool flipx = false, bool flipy = false, bool swapxy = false, float zscale = 1);
extern void transplayer();

extern void usetexturing(bool on);

#define rendermainview (!shadowmapping && !envmapping && !reflecting && !refracting)
#define renderatopview (glaring)
#define rendernormally (rendermainview || renderatopview)

extern void renderplayerpreview(int model, int color, int team, int weap, const char *vanity, bool background, float scale, float blend);
extern void drawslice(float start, float length, float x, float y, float size);
extern void drawfadedslice(float start, float length, float x, float y, float size, float alpha, float r = 1.f, float g = 1.f, float b = 1.f, float minsize = 0.25f);
extern void polyhue(dynent *d, vec &colour, int flags = 0);
extern void polybox(vec o, float tofloor, float toceil, float xradius, float yradius);

// grass
extern void generategrass();
extern void rendergrass();

// blendmap
extern int blendpaintmode;

struct BlendMapCache;
extern BlendMapCache *newblendmapcache();
extern void freeblendmapcache(BlendMapCache *&cache);
extern bool setblendmaporigin(BlendMapCache *cache, const ivec &o, int size);
extern bool hasblendmap(BlendMapCache *cache);
extern uchar lookupblendmap(BlendMapCache *cache, const vec &pos);
extern void resetblendmap();
extern void enlargeblendmap();
extern void shrinkblendmap(int octant);
extern void optimizeblendmap();
extern void stoppaintblendmap();
extern void trypaintblendmap();
extern void renderblendbrush(GLuint tex, float x, float y, float w, float h);
extern void renderblendbrush();
extern bool loadblendmap(stream *f);
extern void saveblendmap(stream *f);
extern uchar shouldsaveblendmap();

// recorder
namespace recorder
{
    extern void stop();
    extern void capture(bool overlay = true);
    extern void cleanup();
}
#endif // STANDALONE

#endif

