#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "version.h"
#include "cube.h"

extern int version, versioning, versionmajor, versionminor, versionpatch, versionplatform, versionarch, versionisserver, versioncrc;
extern char *versionstring, *versionname, *versionuname, *versionvname, *versionrelease, *versionurl, *versionmaster, *versionplatname, *versionplatlongname, *versionbranch;
#define CUR_VER_MAKE(a,b,c) (((a)<<16) | ((b)<<8) | (c))
#define CUR_VER CUR_VER_MAKE(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
#define CUR_VERSION (VERSION_MAJOR*100)+(VERSION_MINOR*10)+VERSION_PATCH

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
#define sup_var(a) VERSION_VNAME"_" a

extern const char *platnames[MAX_PLATFORMS], *platlongnames[MAX_PLATFORMS];
#define plat_name(a) (sup_platform(a) ? platnames[a] : "unk")
#define plat_longname(a) (sup_platform(a) ? platlongnames[a] : "unknown")

#define MAXBRANCHLEN 16

extern const char *timestr(int dur, int style = 0);

extern vector<char *> gameargs;
extern void initgame();
extern void changeservertype();
extern void cleanupserver();
extern void serverslice(uint timeout = 0);
extern int updatetimer(bool limit);

extern void setupmaster();
extern void checkmaster();
extern void cleanupmaster();
extern void reloadmaster();
extern int masterserver, masterport;
extern char *masterip;
extern int nextcontrolversion();

extern void setverinfo(const char *bin);

#include "irc.h"
#include "sound.h"

enum { PACKAGEDIR_OCTA = 1<<0 };
extern const char * const disc_reasons[];
struct ipinfo
{
    enum { ALLOW = 0, BAN, MUTE, LIMIT, EXCEPT, SYNCTYPES, TRUST = SYNCTYPES, MAXTYPES };
    enum { TEMPORARY = 0, INTERNAL, LOCAL, GLOBAL };
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
extern void fatalsignal(int signum);
extern void reloadsignal(int signum);
extern int shutdownwait, maxshutdownwait;
extern void shutdownsignal(int signum);

enum { CON_DEBUG = 0, CON_EVENT, CON_GAME, CON_MESG, CON_MAX };

#ifndef STANDALONE
#include "world.h"
#include "octa.h"
#include "light.h"
#include "texture.h"
#include "bih.h"
#include "model.h"

extern physent *camera1, camera;
extern mapz hdr;
extern int octaentsize;
extern vector<ushort> texmru;
extern int xtraverts, xtravertsva;
extern const ivec cubecoords[8];
extern const ivec facecoords[6][4];
extern const uchar fv[6][4];
extern const uchar fvmasks[64];
extern const uchar faceedgesidx[6][4];
extern bool engineready, inbetweenframes, renderedframe;

extern SDL_Window *screen;
extern int screenw, screenh, renderw, renderh, hudw, hudh;

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
extern float textscale, curtextscale;

#define FONTH (curfont->scale*curtextscale)
#define FONTW (FONTH*0.5f)
#define FONTTAB (FONTW*4)

extern font *curfont;
extern Shader *textshader;
extern const matrix4x3 *textmatrix;
extern void reloadfonts();

enum
{
    TEXT_SHADOW         = 1<<0,
    TEXT_NO_INDENT      = 1<<1,
    TEXT_UPWARD         = 1<<2,
    TEXT_BALLOON        = 1<<3,
    TEXT_MODCOL         = 1<<4,

    TEXT_ALIGN          = 3<<8,
    TEXT_LEFT_JUSTIFY   = 0<<8,
    TEXT_CENTERED       = 1<<8,
    TEXT_RIGHT_JUSTIFY  = 2<<8,

    TEXT_LEFT_UP        = TEXT_UPWARD|TEXT_LEFT_JUSTIFY,
    TEXT_CENTER_UP      = TEXT_UPWARD|TEXT_CENTERED,
    TEXT_RIGHT_UP       = TEXT_UPWARD|TEXT_RIGHT_JUSTIFY,

    TEXT_LEFT_BAL       = TEXT_BALLOON|TEXT_LEFT_JUSTIFY,
    TEXT_CENTER_BAL     = TEXT_BALLOON|TEXT_CENTERED,
    TEXT_RIGHT_BAL      = TEXT_BALLOON|TEXT_RIGHT_JUSTIFY
};

extern font *findfont(const char *name);
extern bool setfont(font *id);
extern bool setfont(const char *name);
extern bool pushfont(font *id);
extern bool pushfont(const char *name);
extern bool popfont(int num = 1);
extern float draw_text(const char *str, float rleft, float rtop, int r = -1, int g = -1, int b = -1, int a = 255, int flags = 0, int cursor = -1, float maxwidth = 0, float linespace = 0);
extern float draw_textf(const char *fstr, float left, float top, float xpad = 0, float ypad = 0, int r = -1, int g = -1, int b = -1, int a = 255, int flags = 0, int cursor = -1, float maxwidth = 0, float linespace = 0, ...);
extern float text_widthf(const char *str, float xpad = 0, float ypad = 0, int flags = 0, float linespace = 0);
extern void text_boundsf(const char *str, float &width, float &height, float xpad = 0, float ypad = 0, float maxwidth = 0, int flags = 0, float linespace = 0);
extern int text_visible(const char *str, float hitx, float hity, float maxwidth = 0, int flags = 0, float linespace =1);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, float maxwidth = 0, int flags = 0, float linespace = 0);
extern float key_widthf(const char *str);

static inline void text_bounds(const char *str, int &width, int &height, float xpad = 0, float ypad = 0, float maxwidth = 0, int flags = 0, float linespace = 0)
{
    float widthf, heightf;
    text_boundsf(str, widthf, heightf, xpad, ypad, maxwidth, flags, linespace);
    width = int(ceil(widthf));
    height = int(ceil(heightf));
}

static inline void text_pos(const char *str, int cursor, int &cx, int &cy, float maxwidth = 0, int flags = 0, float linespace = 0)
{
    float cxf, cyf;
    text_posf(str, cursor, cxf, cyf, maxwidth, flags, linespace);
    cx = int(cxf);
    cy = int(cyf);
}

// texture
extern int hwtexsize, hwcubetexsize, hwmaxaniso, maxtexsize, hwtexunits, hwvtexunits;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern int texalign(const void *data, int w, int bpp);
extern bool floatformat(GLenum format);
extern void cleanuptexture(Texture *t);
extern uchar *loadalphamask(Texture *t);
extern Texture *cubemapload(const char *name, bool mipit = true, bool msg = true, bool transient = false);
extern void drawcubemap(int size, const vec &o, float yaw, float pitch, const cubemapside &side, bool onlysky = false);
extern void loadshaders();
extern void setuptexparameters(int tnum, const void *pixels, int clamp, int filter, GLenum format = GL_RGB, GLenum target = GL_TEXTURE_2D, bool swizzle = false);
extern void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE, bool swizzle = false);
extern void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_3D, bool swizzle = false);
extern void blurtexture(int n, int bpp, int w, int h, uchar *dst, const uchar *src, int margin = 0);
extern void blurnormals(int n, int w, int h, bvec *dst, const bvec *src, int margin = 0);
extern GLuint setuppostfx(int w, int h, GLuint outfbo = 0);
extern void cleanuppostfx(bool fullclean = false);
extern void renderpostfx(GLuint outfbo = 0);
extern void initenvmaps();
extern void genenvmaps();
extern ushort closestenvmap(const vec &o);
extern ushort closestenvmap(int orient, const ivec &o, int size);
extern GLuint lookupenvmap(ushort emid);
extern GLuint lookupenvmap(Slot &slot);
extern bool reloadtexture(Texture *t);
extern bool reloadtexture(const char *name);
extern void setuptexcompress();
extern void resetmaterials();
extern void resetdecals(int n = 0);
extern void clearslots();
extern void compacteditvslots();
extern void compactmruvslots();
extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern int compactvslots(bool cull = false);
extern void reloadtextures();
extern void cleanuptextures();
extern int findslottex(char *name, bool tryint = true);
extern const char *findtexturetypename(int type);

// pvs
extern void clearpvs();
extern bool pvsoccluded(const ivec &bbmin, const ivec &bbmax);
extern bool pvsoccludedsphere(const vec &center, float radius);
extern bool waterpvsoccluded(int height);
extern void setviewcell(const vec &p);
extern void savepvs(stream *f);
extern void loadpvs(stream *f, int numpvs);
extern int getnumviewcells();

static inline bool pvsoccluded(const ivec &bborigin, int size)
{
    return pvsoccluded(bborigin, ivec(bborigin).add(size));
}

// rendergl
extern bool hasVAO, hasTR, hasTSW, hasPBO, hasFBO, hasAFBO, hasDS, hasTF, hasCBF, hasS3TC, hasFXT1, hasLATC, hasRGTC, hasAF, hasFBB, hasFBMS, hasTMS, hasMSS, hasFBMSBS, hasUBO, hasMBR, hasDB2, hasDBB, hasTG, hasTQ, hasPF, hasTRG, hasTI, hasHFV, hasHFP, hasDBT, hasDC, hasDBGO, hasEGPU4, hasGPU4, hasGPU5, hasBFE, hasEAL, hasCR, hasOQ2, hasES3, hasCB, hasCI;
extern int glversion, glslversion, glcompat;
extern char *gfxvendor, *gfxrenderer, *gfxversion;
extern int maxdrawbufs, maxdualdrawbufs;

enum { DRAWTEX_NONE = 0, DRAWTEX_ENVMAP, DRAWTEX_MINIMAP, DRAWTEX_MODELPREVIEW };

extern int vieww, viewh;
extern float curfov, fovy, aspect, forceaspect;
extern float nearplane;
extern int farplane;
extern bool hdrfloat;
extern float ldrscale, ldrscaleb;
extern int drawtex;
extern const matrix4 viewmatrix, invviewmatrix;
extern matrix4 cammatrix, projmatrix, camprojmatrix, invcammatrix, invcamprojmatrix, invprojmatrix;
extern vec curfogcolor;
extern int wireframe;

extern float cursorx, cursory;
extern vec cursordir;

extern int glerr;
extern void glerror(const char *file, int line, GLenum error);

#define GLERROR do { if(glerr) { GLenum error = glGetError(); if(error != GL_NO_ERROR) glerror(__FILE__, __LINE__, error); } } while(0)

extern void gl_checkextensions();
extern void gl_init();
extern void gl_resize();
extern void gl_setupframe(bool force = false);
extern void gl_drawnoview();
extern void gl_drawframe();
extern void cleanupgl();

extern void vecfromcursor(float x, float y, float z, vec &dir);
extern bool vectocursor(const vec &v, float &x, float &y, float &z, float clampxy = -1);
extern void findorientation(vec &o, float yaw, float pitch, vec &pos);
extern void setavatarscale(float fov, float zscale);
extern void renderavatar();
extern bool hasnoview();
extern void drawminimap();
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type);
extern bool calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern bool calcbbscissor(const ivec &bbmin, const ivec &bbmax, float &sx1, float &sy1, float &sx2, float &sy2);
extern bool calcspotscissor(const vec &origin, float radius, const vec &dir, int spot, const vec &spotx, const vec &spoty, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern void screenquad();
extern void screenquad(float sw, float sh);
extern void screenquadflipped(float sw, float sh);
extern void screenquad(float sw, float sh, float sw2, float sh2);
extern void screenquadoffset(float x, float y, float w, float h);
extern void screenquadoffset(float x, float y, float w, float h, float x2, float y2, float w2, float h2);
extern void hudquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern void debugquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern void recomputecamera();
extern float calcfrustumboundsphere(float nearplane, float farplane,  const vec &pos, const vec &view, vec &center);
extern void setfogcolor(const vec &v);
extern void zerofogcolor();
extern void resetfogcolor();
extern float calcfogdensity(float dist);
extern float calcfogcull();
extern void writecrosshairs(stream *f);

namespace modelpreview
{
    extern void start(int x, int y, int w, int h, bool background = true, bool scissor = false);
    extern void end();
}

struct timer;
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

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
extern cube &lookupcube(const ivec &to, int tsize = 0, ivec &ro = lu, int &rsize = lusize);
extern const cube *neighbourstack[32];
extern int neighbourdepth;
extern const cube &neighbourcube(const cube &c, int orient, const ivec &co, int size, ivec &ro = lu, int &rsize = lusize);
extern void resetclipplanes();
extern int getmippedtexture(const cube &p, int orient);
extern void forcemip(cube &c, bool fixtex = true);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceconvexity(const ivec v[4]);
extern int faceconvexity(const ivec v[4], int &vis);
extern int faceconvexity(const vertinfo *verts, int numverts, int size);
extern int faceconvexity(const cube &c, int orient);
extern void calcvert(const cube &c, const ivec &co, int size, ivec &vert, int i, bool solid = false);
extern void calcvert(const cube &c, const ivec &co, int size, vec &vert, int i, bool solid = false);
extern uint faceedges(const cube &c, int orient);
extern bool collapsedface(const cube &c, int orient);
extern bool touchingface(const cube &c, int orient);
extern bool flataxisface(const cube &c, int orient);
extern bool collideface(const cube &c, int orient);
extern int genclipplane(const cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide = true);
extern bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat = MAT_AIR, ushort nmat = MAT_AIR, ushort matmask = MATF_VOLUME);
extern int classifyface(const cube &c, int orient, const ivec &co, int size);
extern int visibletris(const cube &c, int orient, const ivec &co, int size, ushort nmat = MAT_ALPHA, ushort matmask = MAT_ALPHA);
extern int visibleorient(const cube &c, int orient);
extern void genfaceverts(const cube &c, int orient, ivec v[4]);
extern int calcmergedsize(int orient, const ivec &co, int size, const vertinfo *verts, int numverts);
extern void invalidatemerges(cube &c, const ivec &co, int size, bool msg);
extern void calcmerges();

extern int mergefaces(int orient, facebounds *m, int sz);
extern void mincubeface(const cube &cu, int orient, const ivec &o, int size, const facebounds &orig, facebounds &cf, ushort nmat = MAT_AIR, ushort matmask = MATF_VOLUME);
extern void remip();

static inline cubeext &ext(cube &c)
{
    return *(c.ext ? c.ext : newcubeext(c));
}

// renderlights

#define LIGHTTILE_MAXW 16
#define LIGHTTILE_MAXH 16

extern int lighttilealignw, lighttilealignh, lighttilevieww, lighttileviewh, lighttilew, lighttileh;

template<class T>
static inline void calctilebounds(float sx1, float sy1, float sx2, float sy2, T &bx1, T &by1, T &bx2, T &by2)
{
    int tx1 = max(int(floor(((sx1 + 1)*0.5f*vieww)/lighttilealignw)), 0),
        ty1 = max(int(floor(((sy1 + 1)*0.5f*viewh)/lighttilealignh)), 0),
        tx2 = min(int(ceil(((sx2 + 1)*0.5f*vieww)/lighttilealignw)), lighttilevieww),
        ty2 = min(int(ceil(((sy2 + 1)*0.5f*viewh)/lighttilealignh)), lighttileviewh);
    bx1 = T((tx1 * lighttilew) / lighttilevieww);
    by1 = T((ty1 * lighttileh) / lighttileviewh);
    bx2 = T((tx2 * lighttilew + lighttilevieww - 1) / lighttilevieww);
    by2 = T((ty2 * lighttileh + lighttileviewh - 1) / lighttileviewh);
}

static inline void masktiles(uint *tiles, float sx1, float sy1, float sx2, float sy2)
{
    int tx1, ty1, tx2, ty2;
    calctilebounds(sx1, sy1, sx2, sy2, tx1, ty1, tx2, ty2);
    for(int ty = ty1; ty < ty2; ty++) tiles[ty] |= ((1<<(tx2-tx1))-1)<<tx1;
}

enum { SM_NONE = 0, SM_REFLECT, SM_CUBEMAP, SM_CASCADE, SM_SPOT };

extern int shadowmapping;

extern vec shadoworigin, shadowdir;
extern float shadowradius, shadowbias;
extern int shadowside, shadowspot;
extern matrix4 shadowmatrix;

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void rendervolumetric();
extern void cleanupvolumetric();

extern void findshadowvas();
extern void findshadowmms();

extern int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias);
extern int dynamicshadowvabounds(int mask, vec &bbmin, vec &bbmax);
extern void rendershadowmapworld();
extern void batchshadowmapmodels(bool skipmesh = false);
extern void rendershadowatlas();
extern void renderrsmgeom(bool dyntex = false);
extern bool useradiancehints();
extern void renderradiancehints();
extern void clearradiancehintscache();
extern void cleanuplights();
extern void workinoq();

extern int calcbbsidemask(const vec &bbmin, const vec &bbmax, const vec &lightpos, float lightradius, float bias);
extern int calcspheresidemask(const vec &p, float radius, float bias);
extern int calctrisidemask(const vec &p1, const vec &p2, const vec &p3, float bias);
extern int cullfrustumsides(const vec &lightpos, float lightradius, float size, float border);
extern int calcbbcsmsplits(const ivec &bbmin, const ivec &bbmax);
extern int calcspherecsmsplits(const vec &center, float radius);
extern int calcbbrsmsplits(const ivec &bbmin, const ivec &bbmax);
extern int calcspherersmsplits(const vec &center, float radius);

static inline bool sphereinsidespot(const vec &dir, int spot, const vec &center, float radius)
{
    const vec2 &sc = sincos360[spot];
    float cdist = dir.dot(center), cradius = radius + sc.y*cdist;
    return sc.x*sc.x*(center.dot(center) - cdist*cdist) <= cradius*cradius;
}
static inline bool bbinsidespot(const vec &origin, const vec &dir, int spot, const ivec &bbmin, const ivec &bbmax)
{
    vec radius = vec(ivec(bbmax).sub(bbmin)).mul(0.5f), center = vec(bbmin).add(radius);
    return sphereinsidespot(dir, spot, center.sub(origin), radius.magnitude());
}

extern matrix4 worldmatrix, screenmatrix;

extern int gw, gh, gdepthformat, ghasstencil;
extern GLuint gdepthtex, gcolortex, gnormaltex, gglowtex, gdepthrb, gstencilrb;
extern int msaasamples, msaalight;
extern GLuint msdepthtex, mscolortex, msnormaltex, msglowtex, msdepthrb, msstencilrb;
extern vector<vec2> msaapositions;
enum { AA_UNUSED = 0, AA_LUMA, AA_MASKED, AA_SPLIT, AA_SPLIT_LUMA, AA_SPLIT_MASKED };

extern void cleanupgbuffer();
extern void initgbuffer();
extern bool usepacknorm();
extern void maskgbuffer(const char *mask);
extern void bindgdepth();
extern void preparegbuffer(bool depthclear = true);
extern void rendergbuffer(bool depthclear = true);
extern void shadesky();
extern void shadegbuffer();
extern void shademinimap(const vec &color = vec(-1, -1, -1));
extern void shademodelpreview(int x, int y, int w, int h, bool background = true, bool scissor = false);
extern void rendertransparent();
extern void renderao();
extern void loadhdrshaders(int aa = AA_UNUSED);
extern void processhdr(GLuint outfbo = 0, int aa = AA_UNUSED);
extern void copyhdr(int sw, int sh, GLuint fbo, int dw = 0, int dh = 0, bool flipx = false, bool flipy = false, bool swapxy = false);
extern void setuplights();
extern void setupgbuffer();
extern GLuint shouldscale();
extern void doscale(GLuint outfbo = 0);
extern bool debuglights();
extern void cleanuplights();

extern int avatarmask;
extern bool useavatarmask();
extern void enableavatarmask();
extern void disableavatarmask();

// aa
extern matrix4 nojittermatrix;

extern void setupaa(int w, int h);
extern void jitteraa();
extern bool maskedaa();
extern bool multisampledaa();
extern void setaavelocityparams(GLenum tmu = GL_TEXTURE0);
extern void setaamask(bool val);
extern void enableaamask(int stencil = 0);
extern void disableaamask();
extern void doaa(GLuint outfbo, void (*resolve)(GLuint, int));
extern bool debugaa();
extern void cleanupaa();

// ents
extern bool haveselent();
extern undoblock *copyundoents(undoblock *u);
extern void pasteundoent(int idx, const entbase &ue, int *attrs, int numattrs);
extern void pasteundoents(undoblock *u);

// octaedit
extern int texpaneltimer;
extern void cancelsel();
extern void rendertexturepanel(int w, int h);
extern void addundo(undoblock *u);
extern void commitchanges(bool force = false);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit = true);
extern void changed(const block3 &sel, bool commit = true);
extern void tryedit();
extern editinfo *localedit;

extern void renderprefab(const char *name, const vec &o, float yaw, float pitch, float roll, float size = 1, const vec &color = vec(1, 1, 1), float blend = 1);
extern void previewprefab(const char *name, const vec &color, float blend = 1);
extern void cleanupprefabs();

// octarender
extern ivec worldmin, worldmax, nogimin, nogimax;
extern vector<tjoint> tjoints;
extern vector<vtxarray *> varoot, valist;

extern ushort encodenormal(const vec &n);
extern vec decodenormal(ushort norm);
extern void guessnormals(const vec *pos, int numverts, vec *normals);
extern void reduceslope(ivec &n);
extern void findtjoints();
extern void octarender();
extern void allchanged(bool load = false);
extern void clearvas(cube *c);
extern void destroyva(vtxarray *va, bool reparent = true);
extern void updatevabb(vtxarray *va, bool force = false);
extern void updatevabbs(bool force = false);

// renderva

extern int oqfrags;
extern float alphafrontsx1, alphafrontsx2, alphafrontsy1, alphafrontsy2, alphabacksx1, alphabacksx2, alphabacksy1, alphabacksy2, alpharefractsx1, alpharefractsx2, alpharefractsy1, alpharefractsy2;
extern uint alphatiles[LIGHTTILE_MAXH];
extern int blankgeom;
extern vtxarray *visibleva;

extern void visiblecubes(bool cull = true);
extern void setvfcP(const vec &bbmin = vec(-1, -1, -1), const vec &bbmax = vec(1, 1, 1));
extern void savevfcP();
extern void restorevfcP();
extern void rendergeom();
extern int findalphavas();
extern void renderrefractmask();
extern void renderalphageom(int side);
extern void rendermapmodels();
extern void renderoutline();
extern void cleanupva();

extern bool isfoggedsphere(float rad, const vec &cv);
extern int isvisiblesphere(float rad, const vec &cv);
extern int isvisiblebb(const ivec &bo, const ivec &br);
extern bool bboccluded(const ivec &bo, const ivec &br);

extern int deferquery;
extern void flipqueries();
extern occludequery *newquery(void *owner);
extern void startquery(occludequery *query);
extern void endquery(occludequery *query);
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();
extern void startbb(bool mask = true);
extern void endbb(bool mask = true);
extern void drawbb(const ivec &bo, const ivec &br);

extern void renderdecals();

struct shadowmesh;
extern void clearshadowmeshes();
extern void genshadowmeshes();
extern shadowmesh *findshadowmesh(int idx, extentity &e);
extern void rendershadowmesh(shadowmesh *m);

// dynlight

extern void updatedynlights();
extern int finddynlights();
extern bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

// material

extern float matliquidsx1, matliquidsy1, matliquidsx2, matliquidsy2;
extern float matsolidsx1, matsolidsy1, matsolidsx2, matsolidsy2;
extern float matrefractsx1, matrefractsy1, matrefractsx2, matrefractsy2;
extern uint matliquidtiles[LIGHTTILE_MAXH], matsolidtiles[LIGHTTILE_MAXH];
extern vector<materialsurface> editsurfs, glasssurfs[4], watersurfs[4], waterfallsurfs[4], lavasurfs[4], lavafallsurfs[4];
extern const vec matnormals[6];

extern int showmat;

extern int findmaterial(const char *name, bool tryint = false);
extern const char *findmaterialname(int type);
extern const char *getmaterialdesc(int mat, const char *prefix = "");
extern void genmatsurfs(const cube &c, const ivec &co, int size, vector<materialsurface> &matsurfs);
extern void calcmatbb(vtxarray *va, const ivec &co, int size, vector<materialsurface> &matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(int start = 0, int len = 0);
extern int findmaterials();
extern void rendermaterialmask();
extern void renderliquidmaterials();
extern void rendersolidmaterials();
extern void rendereditmaterials();
extern void renderminimapmaterials();
extern int visiblematerial(const cube &c, int orient, const ivec &co, int size, ushort matmask = MATF_VOLUME);

// water
extern int vertwater, waterreflect, caustics;
extern float watersx1, watersy1, watersx2, watersy2;

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

extern const bvec &getwatercolour(int mat);
extern const bvec &getwaterdeepcolour(int mat);
extern const bvec &getwaterdeepfade(int mat);
extern const bvec &getwaterrefractcolour(int mat);
extern const bvec &getwaterfallcolour(int mat);
extern const bvec &getwaterfallrefractcolour(int mat);
extern int getwaterfog(int mat);
extern int getwaterdeep(int mat);
extern int getwaterspec(int mat);
extern float getwaterrefract(int mat);
extern int getwaterfallspec(int mat);
extern float getwaterfallrefract(int mat);

extern const bvec &getlavacolour(int mat);
extern int getlavafog(int mat);
extern float getlavaglowmin(int mat);
extern float getlavaglowmax(int mat);
extern int getlavaspec(int mat);

extern const bvec &getglasscolour(int mat);
extern float getglassrefract(int mat);
extern int getglassspec(int mat);

extern void renderwater();
extern void renderwaterfalls();
extern void renderlava();
extern void renderlava(const materialsurface &m, Texture *tex, float scale);
extern void loadcaustics(bool force = false);
extern void renderwaterfog(int mat, float blend);
extern void preloadwatershaders(bool force = false);

// server
extern void retrieveservers(vector<char> &data);
extern void localclienttoserver(int chan, ENetPacket *);
extern bool serveroption(char *opt);
extern void localconnect(bool force = true);
extern void localdisconnect();

// serverbrowser
extern void addserver(const char *name, int port, int priority = 0, const char *desc = NULL, const char *handle = NULL, const char *flags = NULL, const char *branch = NULL);

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
    #define MOD_KEYS (KMOD_LGUI|KMOD_RGUI)
    #define MOD_ALTS MOD_KEYS
#else
    #define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
    #define MOD_ALTS (KMOD_LALT|KMOD_RALT)
#endif

extern char *pastetext(char *buf = NULL, size_t len = 0);
extern void writebinds(stream *f);
extern const char *addreleaseaction(char *s);
extern tagval *addreleaseaction(ident *id, int numargs);
extern const char *getkeyname(int code);
extern int findkeycode(char *key);

extern int commandmillis,  commandpos, commandcolour, completeoffset, completesize;
extern bigstring commandbuf;
extern char *commandaction, *commandprompt, *commandicon;

// main
extern void quit();
enum
{
    NOT_INITING = 0,
    INIT_GAME,
    INIT_LOAD,
    INIT_RESET,
    INIT_DEFAULTS,
    INIT_QUIT
};
extern int initing, fullscreen, numcpus;
void setfullscreen(bool enable);
extern bool progressing, pixeling;
extern float loadprogress, progressamt;
extern char *progresstitle;
extern void progress(float bar1 = 0, const char *text1 = NULL);
extern void limitfps(int &millis, int curmillis);

enum
{
    CHANGE_GFX     = 1<<0,
    CHANGE_SOUND   = 1<<1,
    CHANGE_SHADERS = 1<<2
};
extern bool initwarning(const char *desc, int level = INIT_RESET, int type = CHANGE_GFX);

extern bool grabinput, minimized;
extern int compresslevel, imageformat;

extern void pushevent(const SDL_Event &e);
extern bool interceptkey(int sym, int mod = 0);
extern void resetcursor(bool warp = true, bool reset = true);
extern void getframemillis(float &avg, float &best, float &worst);
extern void getfps(int &fps, int &bestdiff, int &worstdiff);
extern void swapbuffers(bool overlay = true);
extern int getclockmillis();

enum { KR_CONSOLE = 1<<0, KR_UI = 1<<1, KR_EDITMODE = 1<<2 };

extern void keyrepeat(bool on, int mask = ~0);

enum { TI_CONSOLE = 1<<0, TI_UI = 1<<1 };

extern void textinput(bool on, int mask = ~0);

#define MAXCONLINES 1000
struct cline { char *cref; int reftime, outtime, realtime; };
extern reversequeue<cline, MAXCONLINES> conlines[CON_MAX];
extern void conline(int type, const char *sf, int n);

// physics
extern int dynentsize;
extern bool pointincube(const clipplanes &p, const vec &v);
extern bool overlapsdynent(const vec &o, float radius);
extern void rotatebb(vec &center, vec &radius, int yaw, int pitch, int roll = 0);
extern float shadowray(const vec &o, const vec &ray, float radius, int mode, extentity *t = NULL);
extern bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy);

// worldio
extern char *maptitle, *mapauthor, *mapname, *maptext, *mapdesc;
extern int mapcrc, maptype, maploading, mapvariant;
extern const char *mapvariants[MPV_MAX];
extern bool checkmapvariant(int variant);
extern void changemapvariant(int variant);

// world
extern vector<int> outsideents;
extern int envmapradius;
extern int worldscale, worldsize;
extern void entcancel();
extern void entitiesinoctanodes();
extern void freeoctaentities(cube &c);
extern bool pointinsel(const selinfo &sel, const vec &o);
extern void clearworldvars(bool msg = false);
extern void resetmap(bool empty, int variant = MPV_DEFAULT);

// rendermodel
struct mapmodelinfo { string name; model *m, *collide; };

extern vector<mapmodelinfo> mapmodels;

extern bool matchanim(const char *name, const char *pattern);
extern float transmdlsx1, transmdlsy1, transmdlsx2, transmdlsy2;
extern uint transmdltiles[LIGHTTILE_MAXH];

extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks);
extern void resetmodelbatches();
extern void startmodelquery(occludequery *query);
extern void endmodelquery();
extern void rendershadowmodelbatches(bool dynmodel = true);
extern void shadowmaskbatchedmodels(bool dynshadow = true);
extern void rendermapmodelbatches();
extern void rendermodelbatches();
extern void rendertransparentmodelbatches(int stencil = 0);
extern void rendermapmodel(int idx, entmodelstate &state);
extern void clearbatchedmapmodels();
extern void preloadusedmapmodels(bool msg = false, bool bih = false);
extern int batcheddynamicmodels();
extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
extern void cleanupmodels();

static inline model *loadmapmodel(int n)
{
    if(mapmodels.inrange(n))
    {
        model *m = mapmodels[n].m;
        return m ? m : loadmodel(NULL, n);
    }
    return NULL;
}

static inline mapmodelinfo *getmminfo(int n) { return mapmodels.inrange(n) ? &mapmodels[n] : NULL; }

// renderparticles
extern int particlelayers;

enum { PL_ALL = 0, PL_UNDER, PL_OVER, PL_NOLAYER };

extern void initparticles();
extern void clearparticles();
extern void makeparticle(const vec &o, attrvector &attr);
extern void clearparticleemitters();
extern void seedparticles();
extern void updateparticles();
extern void debugparticles();
extern void renderparticles(int layer = PL_ALL);
extern void cleanupparticles();

// stain
enum { STAINBUF_OPAQUE = 0, STAINBUF_TRANSPARENT, STAINBUF_MAPMODEL, NUMSTAINBUFS };

struct stainrenderer;

extern void initstains();
extern void clearstains();
extern bool renderstains(int sbuf, bool gbuf, int layer = 0);
extern void cleanupstains();
extern void genstainmmtri(stainrenderer *s, const vec v[3]);

// rendersky
extern int explicitsky;

extern int getfog();
extern bvec &getfogcolour(), &getambient(), &getskylight();
extern int getskytexture(), getskyshadow();
extern float getambientscale(), getskylightscale();

extern void drawskybox(bool clear = false);
extern bool hasskybox();
extern bool limitsky();
extern bool renderexplicitsky(bool outline = false);
extern void cleanupsky();
extern void initskybox();

// octaedit
extern int outline, nompedit;
extern bvec outlinecolour;

extern void replacetexcube(cube &c, int oldtex, int newtex);

// skybox
extern void loadsky(char *basename);

// main
extern void setcaption(const char *text = "", const char *text2 = "");
extern bool checkconn();
extern int colorpos, curfps, bestfps, worstfps, bestfpsdiff, worstfpsdiff, maxfps;

// editing
extern int fullbright, fullbrightlevel;
extern vector<int> entgroup;

extern int newentity(const vec &v, int type, const attrvector &attrs, bool fix = true);
extern int newentity(int type, const attrvector &attrs, bool fix = true);

// rendergl
extern void usetexturing(bool on);

extern void drawslice(float start, float length, float x, float y, float size);
extern void drawfadedslice(float start, float length, float x, float y, float size, float alpha, float r = 1.f, float g = 1.f, float b = 1.f, float minsize = 0.25f);

// grass
extern void loadgrassshaders();
extern void generategrass();
extern void rendergrass();
extern void cleanupgrass();

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
extern bool loadblendmap(stream *f, int info);
extern void saveblendmap(stream *f);
extern uchar shouldsaveblendmap();
extern bool usesblendmap(int x1 = 0, int y1 = 0, int x2 = worldsize, int y2 = worldsize);
extern int calcblendlayer(int x1, int y1, int x2, int y2);
extern void updateblendtextures(int x1 = 0, int y1 = 0, int x2 = worldsize, int y2 = worldsize);
extern void bindblendtexture(const ivec &p);
extern void clearblendtextures();
extern void cleanupblendmap();

// recorder
namespace recorder
{
    extern void stop();
    extern void capture(bool overlay = true);
    extern void cleanup();
}
#endif // STANDALONE

#endif
