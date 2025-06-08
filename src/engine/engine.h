#ifndef CPP_ENGINE_HEADER
#define CPP_ENGINE_HEADER

#include "version.h"
#include "cube.h"

#define LOG_FILE "log.txt"

extern int version, versioning, versionmajor, versionminor, versionpatch, versionbuild, versionplatform, versionarch, versionisserver, versioncrc, versionsteamid;
extern char *versionstring, *versionname, *versionfname, *versionuname, *versionvname, *versionrelease, *versionurl, *versioncopy, *versiondesc, *versionmaster, *versionplatname, *versionplatlongname, *versionbranch, *versionrevision, *versiondiscordid;
#define CUR_VER_MAKE(a,b,c) (((a)<<16) | ((b)<<8) | (c))
#define CUR_VER CUR_VER_MAKE(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
#define CUR_VERSION (VERSION_MAJOR*100)+(VERSION_MINOR*10)+VERSION_PATCH
extern const char *getverstr();

#ifdef WIN32
#define CUR_PLATFORM 0
#define CUR_PLATID
#else
#define CUR_PLATFORM 1
#endif
#define CUR_ARCH (int(8*sizeof(void *)))

#define MAX_PLATFORMS 2

#define sup_platform(a) (a >= 0 && a < MAX_PLATFORMS)
#define sup_arch(a) (a == 32 || a == 64)
#define sup_var(a) VERSION_VNAME"_" a

extern const char *platnames[MAX_PLATFORMS], *platlongnames[MAX_PLATFORMS];
#define plat_name(a) (sup_platform(a) ? platnames[a] : "unk")
#define plat_longname(a) (sup_platform(a) ? platlongnames[a] : "unknown")

#define MAXBRANCHLEN 16
#define MAXREVISIONLEN 48

extern const char *timestr(int dur, int style = 0);

#ifdef WIN32
#define SI64 "%I64u"
#else
#define SI64 "%llu"
#endif
namespace cdpi
{
    enum { NONE = 0, SWCLIENT = 1<<0, SWSERVER = 1<<1, DISCORD = 1<<2, ALL = SWCLIENT|SWSERVER|DISCORD, STEAM = SWCLIENT|SWSERVER };

    extern int curapis;
    namespace steam
    {
        extern char *steamusername, *steamuserid, *steamserverid;

        extern bool clientready();
        extern bool clientauthticket(char *token, uint *tokenlen, ENetAddress *addr = NULL);

        extern int serverauthmode();
        extern bool serverparseticket(const char *steamid, const uchar *token, uint tokenlen);
        extern void servercancelticket(const char *steamid);
    }

    extern void cleanup();
    extern bool init(); // returning false means restart in other app
    extern void runframe();

    extern int getoverlay();
    extern void clientconnect();
    extern void clientdisconnect();
}

extern vector<char *> gameargs;
extern bool initgame();
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

#include "http.h"
#include "irc.h"

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
extern void writecfg(const char *name = "config.cfg", int flags = IDF_PERSIST);
extern void rehash(bool reload = true);
extern void fatalsignal(int signum);
extern void reloadsignal(int signum);
extern int shutdownwait, maxshutdownwait;
extern void shutdownsignal(int signum);

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
extern int showboundingboxes, xtraverts, xtravertsva;
extern const ivec cubecoords[8];
extern const ivec facecoords[6][4];
extern const uchar fv[6][4];
extern const uchar fvmasks[64];
extern const uchar faceedgesidx[6][4];
extern bool engineready, inbetweenframes, renderedframe;

extern SDL_Window *screen;
extern int screenw, screenh, refreshrate, renderw, renderh, hudw, hudh;

// rendertext
struct font
{
    struct charinfo
    {
        float x, y, w, h, offsetx, offsety, advance;
        int tex;
    };

    char *name;
    vector<Texture *> texs;
    vector<charinfo> chars;
    int charoffset;
    float defaultw, defaulth, maxw, maxh, scale, mw, mh, bordermin, bordermax, outlinemin, outlinemax;

    font() : name(NULL) {}
    ~font() { DELETEA(name); }
};
extern float textscale, curtextscale;

#define FONTH (curfont->scale*curtextscale)
#define FONTW (FONTH*0.5f)
#define FONTTAB (FONTW*4)
#define FONTMH (curfont->mh*curtextscale)
#define FONTMW (curfont->mw*curtextscale)

extern font *curfont;
extern Shader *textshader;
extern const matrix4x3 *textmatrix;
extern void reloadfonts();
extern char *textfontdef, *textfontbold, *textfontlogo, *textfontoutline, *textfonttool;

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
extern bool setfont(const char *name = NULL);
extern bool pushfont(font *id);
extern bool pushfont(const char *name = NULL);
extern bool popfont(int num = 1);
extern float draw_text(const char *str, float rleft, float rtop, int r = -1, int g = -1, int b = -1, int a = 255, int flags = 0, int cursor = -1, float maxwidth = 0, float linespace = 0);
extern float draw_textf(const char *fstr, float left, float top, float xpad = 0, float ypad = 0, int r = -1, int g = -1, int b = -1, int a = 255, int flags = 0, int cursor = -1, float maxwidth = 0, int linespace = 0, ...);
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
extern bool loadedshaders;

extern Texture *textureloaded(const char *name);
extern Texture *textureload(const char *name, int tclamp = 0, bool mipit = true, bool msg = true, bool gc = false);
extern int texalign(const void *data, int w, int bpp);
extern bool floatformat(GLenum format);
extern void cleanuptexture(Texture *t);
extern uchar *loadalphamask(Texture *t);
extern Texture *cubemapload(const char *name, bool mipit = true, bool msg = true, bool transient = false);
extern void drawcubemap(int size, const vec &o, float yaw, float pitch, bool onlysky = false);
extern void loadshaders();
extern void setuptexparameters(int tnum, const void *pixels, int tclamp, int filter, GLenum format = GL_RGB, GLenum target = GL_TEXTURE_2D, bool swizzle = false);
extern void createtexture(int tnum, int w, int h, const void *pixels, int tclamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE, bool swizzle = false);
extern void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int tclamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_3D, bool swizzle = false);
extern void blurtexture(int n, int bpp, int w, int h, uchar *dst, const uchar *src, int margin = 0);
extern void blurnormals(int n, int w, int h, bvec *dst, const bvec *src, int margin = 0);
extern GLuint setuppostfx(int w, int h, GLuint outfbo = 0);
extern void cleanuppostfx(bool fullclean = false);
extern void renderpostfx(GLuint outfbo = 0);
extern ushort closestenvmap(const vec &o);
extern ushort closestenvmap(int orient, const ivec &o, int size);
extern GLuint closestenvmaptex(const vec &o);
extern GLuint lookupenvmapindex(int index = -1);
extern GLuint closestenvmapbb(const vec &center, const ivec &bbmin, const ivec &bbmax);
extern int closestenvmapindex(const vec &center, const ivec &bbmin, const ivec &bbmax);
extern GLuint lookupenvmap(ushort emid);
extern GLuint lookupenvmap(Slot &slot);
extern GLuint entityenvmap(int id);
extern void initenvtexs();
extern void genenvtexs();
extern bool reloadtexture(Texture *t);
extern bool reloadtexture(const char *name);
extern void setuptexcompress();
extern void resetmaterials();
extern void checkmaterials(const char *name);
extern void resetdecals(int n = 0);
extern void clearslots();
extern void compacteditvslots();
extern void compactmruvslots();
extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern int compactvslots(bool cull = false, int from = 0, int to = -1);
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
extern bool hasVAO, hasTR, hasTSW, hasPBO, hasFBO, hasAFBO, hasDS, hasTF, hasCBF, hasS3TC, hasFXT1, hasLATC, hasRGTC, hasAF, hasFBB, hasFBMS, hasTMS, hasMSS, hasFBMSBS, hasUBO, hasMBR, hasDB2, hasDBB, hasTG, hasTQ, hasPF, hasTRG, hasTI, hasHFV, hasHFP, hasDBT, hasDC, hasDBGO, hasEGPU4, hasGPU4, hasGPU5, hasBFE, hasEAL, hasCR, hasOQ2, hasES2, hasES3, hasCB, hasCI, hasTS;
extern int glversion, glslversion, glcompat;
extern char *gfxvendor, *gfxrenderer, *gfxversion;
extern int maxdrawbufs, maxdualdrawbufs;

enum {
    DRAWTEX_NONE = 0, DRAWTEX_ENVMAP, DRAWTEX_SCENE, DRAWTEX_MAP, DRAWTEX_MINIMAP, DRAWTEX_MODELPREVIEW, DRAWTEX_HALO, DRAWTEX_MAX,
    DRAWTEX_GAME = (1<<DRAWTEX_NONE)|(1<<DRAWTEX_SCENE),
    DRAWTEX_GAMEHALO = (1<<DRAWTEX_NONE)|(1<<DRAWTEX_SCENE)|(1<<DRAWTEX_HALO),
    DRAWTEX_VIEW = (1<<DRAWTEX_NONE)|(1<<DRAWTEX_SCENE)|(1<<DRAWTEX_MAP),
    DRAWTEX_HAZE = (1<<DRAWTEX_NONE)|(1<<DRAWTEX_ENVMAP)|(1<<DRAWTEX_SCENE)|(1<<DRAWTEX_MAP),
    DRAWTEX_DARK = (1<<DRAWTEX_NONE)|(1<<DRAWTEX_ENVMAP)|(1<<DRAWTEX_SCENE)
};

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
extern int wireframe, editinhibit;

extern float cursorx, cursory, cursoryaw, cursorpitch;
extern vec cursordir;

extern int glerr;
extern void glerror(const char *file, int line, GLenum error);

#define GLERROR do { if(glerr) { GLenum error = glGetError(); if(error != GL_NO_ERROR) glerror(__FILE__, __LINE__, error); } } while(0)

extern void gl_checkextensions();
extern void gl_init();
extern void gl_resize();
extern void gl_setupframe(bool force = false);
extern void gl_drawview();
extern void gl_drawnoview();
extern void gl_drawframe();
extern void cleanupgl();

extern void vecfromcursor(float x, float y, float z, vec &dir);
extern bool vectocursor(const vec &v, float &x, float &y, float &z, float clampxy = -1);
extern bool findorientation(vec &o, float yaw, float pitch, vec &pos, float multiplier = 2.0f);
extern void safefindorientation(vec &o, float yaw, float pitch, vec &pos, float multiplier = 2.0f);
extern void setavatarscale(float fov, float zscale);
extern void renderavatar();
extern bool hasnoview();
extern void drawminimap();
extern void enablepolygonoffset(GLenum type, float scale = 1.0f);
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
extern float calcfrustumboundsphere(float nearplane, float farplane, const vec &pos, const vec &view, vec &center);
extern void setfogcolor(const vec &v);
extern void zerofogcolor();
extern void resetfogcolor();
extern float calcfogdensity(float dist);
extern float calcfogcull();

namespace modelpreview
{
    extern void start(int x, int y, int w, int h, float pitch = -15, float roll = 0, float fov = 0, bool background = true, bool scissor = false, vec translate = vec(0, 0, 0));
    extern void end(GLuint outfbo, const vec &skycol = vec(0.1f, 0.1f, 0.1f), const vec &suncol = vec(0.6f, 0.6f, 0.6f), const vec &sundir = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0));
}

struct timer;
extern timer *begintimer(const char *name, bool gpu = true);
extern void endtimer(timer *t);

extern GLuint renderfbo;
extern int fogoverlay;
extern void getcamfogmat(int &fogmat, int &abovemat, float &fogbelow);
extern void setfog(int fogmat, float below = 0, float blend = 1, int abovemat = MAT_AIR);
extern void drawfogoverlay(int fogmat, float fogbelow, float fogblend, int abovemat);

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
extern void genclipbounds(const cube &c, const ivec &co, int size, clipplanes &p);
extern int genclipplane(const cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(const cube &c, const ivec &co, int size, clipplanes &p, bool collide = true, bool noclip = false);
extern bool visibleface(const cube &c, int orient, const ivec &co, int size, ushort mat = MAT_AIR, ushort nmat = MAT_AIR, ushort matmask = MATF_VOLUME);
extern int classifyface(const cube &c, int orient, const ivec &co, int size);
extern int visibletris(const cube &c, int orient, const ivec &co, int size, ushort vmat = MAT_AIR, ushort nmat = MAT_ALPHA, ushort matmask = MAT_ALPHA);
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

extern int gscale, gscalecubic, gscalenearest;

#define DARK_ENUM(en, um) \
    en(um, Environment, ENV) en(um, Glow, GLOW) en(um, Sunlight, SUN) en(um, Particles, PART) \
    en(um, Halo, HALO) en(um, UI, UI) en(um, Maximum, MAX)
ENUM_DLN(DARK);

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
extern float shadowradius, shadowbias, currentdepthscale;
extern int shadowside, shadowspot, shadowtransparent;
extern matrix4 shadowmatrix;

extern void loaddeferredlightshaders();
extern void cleardeferredlightshaders();
extern void clearshadowcache();

extern void rendervolumetric();
extern void cleanupvolumetric();

extern void findshadowvas(bool transparent = false);
extern void findshadowmms();

extern int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias);
extern int dynamicshadowvas();
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

extern matrix4 eyematrix, worldmatrix, linearworldmatrix, screenmatrix;

extern int transparentlayer;

extern GLenum hdrformat, stencilformat;
extern int gw, gh, gdepthformat, ghasstencil;
extern GLuint hdrtex, gdepthtex, gcolortex, gnormaltex, gglowtex, gdepthrb, gstencilrb, earlydepthtex;
extern int msaasamples, msaalight;
extern GLuint mshdrtex, msdepthtex, mscolortex, msnormaltex, msglowtex, msdepthrb, msstencilrb, msearlydepthtex;
extern vector<vec2> msaapositions;
extern GLuint hdrfbo, mshdrfbo;
enum { AA_UNUSED = 0, AA_LUMA, AA_MASKED, AA_SPLIT, AA_SPLIT_LUMA, AA_SPLIT_MASKED };

extern void cleanupgbuffer();
extern void initgbuffer();
extern bool usepacknorm();
extern void maskgbuffer(const char *mask);
extern void bindgdepth();
extern void setupscreenparams();
extern void preparegbuffer(bool depthclear = true);
extern void rendergbuffer(bool depthclear = true);
extern void shadesky();
extern void shadegbuffer();
extern void shademinimap(const vec &color = vec(-1, -1, -1));
extern void shademodelpreview(GLuint outfbo, int x, int y, int w, int h, bool background = true, bool scissor = false, const vec &skycol = vec(0.1f, 0.1f, 0.1f), const vec &suncol = vec(0.6f, 0.6f, 0.6f), const vec &sundir = vec(0, -1, 2), const vec &excol_ = vec(0.f, 0.f, 0.f), const vec &exdir_ = vec(0, 0, 0));
extern void renderearlydepth();
extern void rendertransparent();
extern void renderao();
extern void loadhdrshaders(int aa = AA_UNUSED);
extern void processhdr(GLuint outfbo = 0, int aa = AA_UNUSED);
extern void copyhdr(int sw, int sh, GLuint fbo, int dw = 0, int dh = 0, bool flipx = false, bool flipy = false, bool swapxy = false);
extern void setuplights();
extern void setupgbuffer();
extern vec2 renderdepthscale(int w, int h);
extern GLuint shouldscale();
extern void doscale(GLuint outfbo = 0, int w = 0, int h = 0);
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
extern void addundo(undoblock *u);
extern void commitchanges(bool force = false);
extern void changed(const ivec &bbmin, const ivec &bbmax, bool commit = true);
extern void changed(const block3 &sel, bool commit = true);
extern void tryedit();
extern editinfo *localedit;

extern cube &blockcube(int x, int y, int z, const block3 &b, int rgrid);
extern void copy();
extern void delcube();
extern void paste();
extern void vscale(float *scale);
extern void voffset(int *x, int *y);

#define loopxy(b)           loop(y,(b).s[C[dimension((b).orient)]]) loop(x,(b).s[R[dimension((b).orient)]])
#define loopxyz(b, r, f)    { loop(z,(b).s[D[dimension((b).orient)]]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; } }
#define loopselxyz(f)       { if(local) makeundo(); loopxyz(sel, sel.grid, f); changed(sel); }
#define selcube(x, y, z)    blockcube(x, y, z, sel, sel.grid)

extern void renderprefab(const char *name, const vec &o, float yaw, float pitch, float roll, float size = 1, const vec &color = vec(1, 1, 1), float blend = 1);
extern void previewprefab(const char *name, const vec &color, float blend = 1, float yaw = -1, float offsetyaw = 0);
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
extern int sortentcolor(ushort entid1, ushort entid2);

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
extern void renderalphageom(int side);
extern void renderalphashadow(bool cullside = false);
extern void rendermapmodels();
extern void rendertransparentmapmodels();
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

extern void cleardynlights();
extern void updatedynlights();
extern int finddynlights();
extern bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

// material

extern float matliquidsx1, matliquidsy1, matliquidsx2, matliquidsy2;
extern float matsolidsx1, matsolidsy1, matsolidsx2, matsolidsy2;
extern float matrefractsx1, matrefractsy1, matrefractsx2, matrefractsy2;
extern uint matliquidtiles[LIGHTTILE_MAXH], matsolidtiles[LIGHTTILE_MAXH];
extern vector<materialsurface> editsurfs, glasssurfs[4], watersurfs[4], waterfallsurfs[4], lavasurfs[4], lavafallsurfs[4], volfogsurfs[4];
extern const bvec4 matnormals[6];

extern int showmat, vismatmask;

extern int findmaterial(const char *name, bool tryint = false);
extern const char *findmaterialname(int type);
extern const char *getmaterialdesc(ushort *mat, const char *prefix = "");
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
            default: case 0: \
                if(checkmapvariant(MPV_ALTERNATE)) return name##var##alt; \
                return name##var; \
            case 1: \
                if(checkmapvariant(MPV_ALTERNATE)) return name##2##var##alt; \
                return name##2##var; \
            case 2: \
                if(checkmapvariant(MPV_ALTERNATE)) return name##3##var##alt; \
                return name##3##var; \
            case 3: \
                if(checkmapvariant(MPV_ALTERNATE)) return name##4##var##alt; \
                return name##4##var; \
        } \
    }

#define GETMATIDXVARDARK(name, var, type) \
    type get##name##var(int mat) \
    { \
        static bvec res; \
        switch(mat&MATF_INDEX) \
        { \
            default: case 0: \
                res = checkmapvariant(MPV_ALTERNATE) ? name##var##alt : name##var; break; \
            case 1: \
                res = checkmapvariant(MPV_ALTERNATE) ? name##2##var##alt : name##2##var; break; \
            case 2: \
                res = checkmapvariant(MPV_ALTERNATE) ? name##3##var##alt : name##3##var; break; \
            case 3: \
                res = checkmapvariant(MPV_ALTERNATE) ? name##4##var##alt : name##4##var; break; \
        } \
        res.mul(game::darkness(DARK_ENV)); \
        return res; \
    }

extern int getwaterenabled(int mat);
extern const bvec &getwatercolour(int mat);
extern const bvec &getwaterdeepcolour(int mat);
extern const bvec &getwaterdeepfade(int mat);
extern const bvec &getwaterreflectcolour(int mat);
extern const bvec &getwaterrefractcolour(int mat);
extern const bvec &getwaterfallcolour(int mat);
extern const bvec &getwaterfallrefractcolour(int mat);
extern int getwaterfog(int mat);
extern int getwaterdeep(int mat);
extern int getwaterspec(int mat);
extern float getwaterrefract(int mat);
extern int getwaterfallspec(int mat);
extern float getwaterfallrefract(int mat);
extern int getwaterreflectstep(int mat);
extern float getwaterfallscrollx(int mat);
extern float getwaterfallscrolly(int mat);

extern const bvec &getwateredgecolour(int mat);
extern float getwateredgefade(int mat);
extern float getwateredgedist(int mat);
extern float getwateredgebright(int mat);
extern float getwateredgebrightref(int mat);
extern float getwateredgescale(int mat);
extern float getwateredgeaccum(int mat);
extern float getwateredgespeed(int mat);
extern float getwateredgebrightbump(int mat);

extern int getlavaenabled(int mat);
extern const bvec &getlavacolour(int mat);
extern int getlavafog(int mat);
extern float getlavaglowmin(int mat);
extern float getlavaglowmax(int mat);
extern int getlavaspec(int mat);
extern float getlavascrollx(int mat);
extern float getlavascrolly(int mat);
extern float getlavafallscrollx(int mat);
extern float getlavafallscrolly(int mat);

extern int getglassenabled(int mat);
extern const bvec &getglasscolour(int mat);
extern float getglassrefract(int mat);
extern int getglassspec(int mat);

extern int getvolfogenabled(int mat);
extern const bvec &getvolfogcolour(int mat);
extern const bvec &getvolfogdeepcolour(int mat);
extern const bvec &getvolfogdeepfade(int mat);
extern const bvec &getvolfogtexcolour(int mat);
extern int getvolfogdist(int mat);
extern int getvolfogdeep(int mat);
extern int getvolfogtexture(int mat);
extern float getvolfogtexblend(int mat);
extern float getvolfogscrollx(int mat);
extern float getvolfogscrolly(int mat);

extern void renderwater();
extern void renderwaterfalls();
extern void rendervolfog();
extern void renderlava();
extern void renderlava(const materialsurface &m, Texture *tex, float scale);
extern void renderdepthfog(int mat, float blend);
extern void preloadwatershaders(bool force = false);

// server
extern void retrieveservers(vector<char> &data);
extern void localclienttoserver(int chan, ENetPacket *);
extern bool serveroption(char *opt);
extern void localconnect(bool force = true);
extern void localdisconnect();

// serverbrowser
extern void addserver(const char *name, int port, int priority = 0, const char *desc = NULL, const char *handle = NULL, const char *flags = NULL, const char *branch = NULL);
extern void writeservercfg();

// client
extern char *connectname;
extern int connectport, discmillis;
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
#define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
#define MOD_ALTS (KMOD_LALT|KMOD_RALT)
#define SKIP_KEYS (KMOD_LCTRL|KMOD_RCTRL)
#define MOD_SHIFTS (KMOD_LSHIFT|KMOD_RSHIFT)

extern void writehistory();
extern char *pastetext(char *buf = NULL, size_t len = 0);
extern void writebinds(stream *f);
extern const char *addreleaseaction(char *s);
extern tagval *addreleaseaction(ident *id, int numargs);
extern const char *getkeyname(int code);
extern int findkeycode(char *key);

extern bigstring consolebuf;
extern int consolemillis, consolepos, consolestay, consoleecho, consolevars, completeoffset, completesize;
extern char *consoleaction, *consoleprompt;
extern bool consolerun;

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

#define PROGRESS_ENUM(en, um) \
    en(um, None, NONE) en(um, Disconnecting, DISCONNECT) en(um, Connecting, CONNECT) \
    en(um, Loading, MAPLOAD) en(um, Saving, MAPSAVE) en(um, Downloading, MAPDL) \
    en(um, Starting, GAMESTATE) en(um, Waiting, GAMEWAIT) en(um, Maximum, MAX)
ENUM_DLN(PROGRESS);

extern void setfullscreen(bool enable);

extern int initing, fullscreen, fullscreendesktop, numcpus, noconfigfile, firstrun, progsteps, frameloops;
extern bool progressing, pixeling;
extern float loadprogress, progressamt;
extern char *progresstitle;

#define PROGRESS(n) progress(((n) + 1) / float(max(progsteps, 1)), "%s", progresstitle)
extern void progress(float amt, const char *s, ...) PRINTFARGS(2, 3);
extern void limitfps(int &millis, int curmillis);

enum
{
    CHANGE_GFX     = 1<<0,
    CHANGE_SOUND   = 1<<1,
    CHANGE_SHADERS = 1<<2,
    CHANGE_MAX     = 3
};
extern bool initwarning(const char *desc, int level = INIT_RESET, int type = CHANGE_GFX);

extern bool grabinput, minimized;
extern int compresslevel, imageformat, renderunfocused;

extern bool interceptkey(int sym);
extern void resetcursor(bool warp = true, bool reset = true);
extern void getframemillis(float &avg, float &best, float &worst);
extern void getfps(int &fps, int &bestdiff, int &worstdiff);
extern void swapbuffers(bool overlay = true);
extern Uint32 getclockticks();
extern Uint32 getclockmillis();

enum { KR_CONSOLE = 1<<0, KR_UI = 1<<1, KR_EDITMODE = 1<<2 };

extern void keyrepeat(bool on, int mask = ~0);

enum { TI_CONSOLE = 1<<0, TI_UI = 1<<1 };

extern void textinput(bool on, int mask = ~0);
#define CON_ENUM(en, um) \
    en(um, Debug, DEBUG) en(um, Event, EVENT) en(um, Maximum, MAX)
ENUM_DLN(CON);

#define MAXCONLINES 1000
struct cline { char *cref; int color, reftime, outtime, realtime; };
extern reversequeue<cline, MAXCONLINES> conlines[CON_MAX];
extern void conline(int color, const char *sf, int type = CON_DEBUG);

// physics
extern int dynentsize;
extern bool overlapsdynent(const vec &o, float radius);
extern void rotatebb(vec &center, vec &radius, int yaw, int pitch = 0, int roll = 0);
extern bool getsight(const vec &o, float yaw, float pitch, const vec &q, vec &v, float mdist, float fovx, float fovy);
extern bool getvisible(const vec &o, float yaw, float pitch, const vec &q, float fovx, float fovy, float radius = 0, int vfc = VFC_FULL_VISIBLE);
extern void fixfullrange(float &yaw, float &pitch, float &roll, bool full = false);
extern void fixrange(float &yaw, float &pitch, bool full = false);

// worldio
extern char *maptitle, *mapauthor, *mapname, *maptext, *mapdesc;
extern int mapcrc, maploading, mapsaving, mapvariant, mapeffects;
extern bool checkmapvariant(int variant);
extern void changemapvariant(int variant);
extern bool checkmapeffects(int fxlevel);

// world
extern vector<int> outsideents;
extern int envmapradius;
extern int worldscale, worldsize;
extern void entcancel();
extern void entitiesinoctanodes();
extern void freeoctaentities(cube &c);
extern bool pointinsel(const selinfo &sel, const vec &o);
extern void clearmapvars(bool msg = false);
extern void resetmap(bool empty, int variant = MPV_DEFAULT);
extern bool entselectionbox(extentity &e, vec &eo, vec &es, bool full = false);

// rendermodel
struct mapmodelinfo { string name; model *m, *collide; };

extern vector<mapmodelinfo> mapmodels;

extern float transmdlsx1, transmdlsy1, transmdlsx2, transmdlsy2;
extern uint transmdltiles[LIGHTTILE_MAXH];

extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks);
extern void resetmodelbatches();
extern void startmodelquery(occludequery *query);
extern void endmodelquery();
extern void rendershadowmodelbatches(bool dynmodel = true);
extern void shadowmaskbatchedmodels(bool dynshadow = true, bool noavatar = false);
extern void rendermapmodelbatches();
extern void rendermodelbatches();
extern void rendertransparentmodelbatches(int stencil = 0);
extern void renderhalomodelbatches(bool ontop);
extern bool mapmodelvisible(extentity &e, int n, int colvis = 0, bool shadowpass = false);
extern void getmapmodelstate(extentity &e, entmodelstate &mdl);
extern void rendermapmodel(int idx, entmodelstate &state, bool tpass = false);
extern void clearbatchedmapmodels();
extern void preloadusedmapmodels(bool msg = false, bool bih = false);
extern int batcheddynamicmodels();
extern int batcheddynamicmodelbounds(int mask, vec &bbmin, vec &bbmax);
extern void cleanupmodels();
extern int showmapmodels;

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
extern bool hazeparticles;

enum { PL_ALL = 0, PL_UNDER, PL_OVER, PL_NOLAYER };

extern void initparticles();
extern void clearparticles();
extern void makeparticle(const vec &o, attrvector &attr);
extern void clearparticleemitters();
extern void seedparticles();
extern void updateparticles();
extern void debugparticles();
extern void renderparticles(int layer = PL_ALL);
extern void initparticlehaze();
extern void renderhazeparticles(GLuint hazertex, bool hazemix);
extern void cleanupparticles();

extern void regular_part_create(int type, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, int delay = 0, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_create(int type, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regular_part_splash(int type, int num, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float radius = 150, float vel = 1, int delay = 0, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_splash(int type, int num, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float radius = 4, float vel = 1, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_trail(int type, int fade, const vec &s, const vec &e, int color = colourwhite, float size = .8f, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float sizechange = 0, float vel = 10.0f, float stepscale = 2.0f, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_text(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = colourwhite, float size = 2, float blend = 1, float gravity = 0, int collide = 0, physent *pl = NULL, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_textcopy(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = colourwhite, float size = 2, float blend = 1, float gravity = 0, int collide = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_flare(const vec &p, const vec &dest, int fade, int type, int color = colourwhite, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regular_part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0);
extern void part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL);
extern void part_spawn(const vec &o, const vec &v, float z, uchar type, int amt = 1, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_flares(const vec &o, const vec &v, float z1, const vec &d, const vec &w, float z2, uchar type, int amt = 1, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_portal(const vec &o, float size, float blend = 1, float yaw = 0, float pitch = 0, int type = PART_PORTAL, int fade = 1, int color = colourwhite, GLuint envmap = 0, float envblend = 1, float destyaw = 0, float destpitch = 0, int hintcolor = 0, float hintblend = 0);
extern void part_icons(const vec &o, Texture *tex, int type, float size = 2, float blend = 1, float gravity = 0, int collide = 0, int fade = 1, int color = colourwhite, int hintcolor = 0, float hintblend = 0, float start = 0, float length = 1, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_icon(const vec &o, Texture *tex, float size = 2, float blend = 1, float gravity = 0, int collide = 0, int fade = 1, int color = colourwhite, int hintcolor = 0, float hintblend = 0, float start = 0, float length = 1, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_icon_ontop(const vec &o, Texture *tex, float size = 2, float blend = 1, float gravity = 0, int collide = 0, int fade = 1, int color = colourwhite, int hintcolor = 0, float hintblend = 0, float start = 0, float length = 1, physent *pl = NULL, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void part_line(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int type = PART_LINE);
extern void part_triangle(const vec &o, float yaw, float pitch, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, bool fill = true, int type = PART_TRIANGLE);
extern void part_dir(const vec &o, float yaw, float pitch, float length = 1, float size = 1, float blend = 1, int fade = 1, int color = colourpink, int interval = 0, bool fill = true);
extern void part_trace(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int interval = 0, bool fill = true);
extern void part_ellipse(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int axis = 0, bool fill = false, int type = PART_ELLIPSE);
extern void part_radius(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourcyan, bool fill = false);
extern void part_cone(const vec &o, const vec &dir, float radius, float angle = 0.f, float size = 1, float blend = 1, int fade = 1, int color = colourcyan, bool fill = false, int spokenum = 15, int type = PART_CONE);

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
extern const bvec &getfogcolour(), &getambient(), &getskylight();
extern int getskytexture(), getskyshadow();
extern float getambientscale(), getskylightscale();

extern void drawenvlayer(Texture *tex, float height = 0.f, const bvec &colour = bvec(255, 255 ,255), float blend = 1.f, float subdiv = 4.f, float fade = 0.f, float scale = 1.f, float offsetx = 0.f, float offsety = 0.f, float shadowblend = 0.5f, float zrot = 0.f, bool skyplane = false, bool shadowpass = false, int cylinder = 0, float dist = 1);
extern void drawenvlayers(bool skyplane, bool shadowpass = false);
extern void drawskybox(bool clear = false);
extern bool hasskybox();
extern bool limitsky();
extern bool hasenvshadow();
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
extern int colorpos, curfps, bestfps, worstfps, bestfpsdiff, worstfpsdiff, maxfps;

// editing
extern int fullbright, fullbrightlevel;
extern vector<int> entgroup;

extern int newentity(const vec &v, int type, const attrvector &attrs, bool fix = true, bool alter = true);
extern int newentity(int type, const attrvector &attrs, bool fix = true, bool alter = true);

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

// renderfx
struct RenderBuffer
{
    int tclamp = 3, filter = 1, width = 0, height = 0;
    GLenum format = GL_RGBA, target = GL_TEXTURE_RECTANGLE;
    GLuint tex = 0, fbo = 0;

    RenderBuffer() {}
    RenderBuffer(int _width, int _height, GLenum _format = GL_RGBA, GLenum _target = GL_TEXTURE_RECTANGLE, int _tclamp = 3, int _filter = 1) :
        tclamp(_tclamp), filter(_filter), width(_width), height(_height), format(_format), target(_target) { setup(); }
    ~RenderBuffer() { cleanup(); }

    void cleanup()
    {
        if(tex) { glDeleteTextures(1, &tex); tex = 0; }
        if(fbo) { glDeleteFramebuffers_(1, &fbo); fbo = 0; }
    }

    void setup()
    {
        glGenTextures(1, &tex);
        createtexture(tex, width, height, NULL, tclamp, filter, format, target);

        glGenFramebuffers_(1, &fbo);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, tex, 0);
    }

    bool check(int _width, int _height, GLenum _format = GL_RGBA, GLenum _target = GL_TEXTURE_RECTANGLE, int _tclamp = 3, int _filter = 1)
    {
        if(tex && fbo && width == _width && height == _height && format == _format && target == _target && tclamp == _tclamp && filter == _filter)
            return false;

        cleanup();

        tclamp = _tclamp;
        filter = _filter;
        width = _width;
        height = _height;
        format = _format;
        target = _target;

        setup();

        return true;
    }
};

struct RenderSurface
{
    enum { GENERIC = 0, HALO, HAZE, VISOR, VIEW, MAX };

    int type = GENERIC, origvieww = 0, origviewh = 0;
    GLuint origfbo = 0;
    vector<RenderBuffer *> buffers;

    RenderSurface() {}
    ~RenderSurface() { destroy(); }

    bool cleanup();

    virtual void checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n);
    virtual int setup(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1);
    virtual bool bindtex(int index = 0, int tmu = -1);
    virtual void savefbo();
    virtual bool bindfbo(int index = 0, int w = 0, int h = 0);
    virtual int create(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1);
    virtual bool destroy();
    virtual bool render(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1);
    virtual bool swap(int index = 0);
    virtual bool draw(int x = 0, int y = 0, int w = 0, int h = 0);
    virtual void debug(int w, int h, int index = 0, bool large = false);
    virtual bool save(const char *name, int w, int h, int index = 0);
    virtual void restorefbo();
    virtual bool copy(int index, GLuint fbo, int w, int h, bool linear = true, bool restore = false);
};

extern int debughalo;
extern int halodist;
extern float haloblend, halotolerance, haloaddz;
extern bvec halocolour;

struct HaloSurface : RenderSurface
{
    enum { DEPTH = 0, ONTOP, MAX };

    int halotype = 0;

    HaloSurface() : halotype(-1) { type = RenderSurface::HALO; }
    ~HaloSurface() { destroy(); }

    bool check();

    void checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n) override;
    int create(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1) override;
    bool destroy() override;
    bool render(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1) override;
    bool swap(int index = 0) override;
    bool draw(int x = 0, int y = 0, int w = 0, int h = 0) override;
};
extern HaloSurface halosurf;

extern int debughaze;

extern int gethaze();
extern const bvec &gethazecolour();
extern float gethazecolourmix();
extern float gethazemargin();
extern float gethazemindist();
extern float gethazemaxdist();
extern float gethazeblend();

struct HazeSurface : RenderSurface
{
    Texture *tex = NULL;

    HazeSurface() : tex(NULL) { type = RenderSurface::HAZE; }
    ~HazeSurface() { destroy(); }

    bool check();

    void checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n) override;
    int create(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1) override;
    bool render(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1) override;
};
extern HazeSurface hazesurf;

extern int rendervisor;

extern int debugvisor;
struct VisorSurface : RenderSurface
{
    enum { BACKGROUND = 0, WORLD, VISOR, FOREGROUND, LOOPED, BLIT = LOOPED, BUFFERS, SCALE1 = BUFFERS, GLASS, SCALE2 = GLASS, FOCUS1, FOCUS2, MAX };

    struct Config
    {
        float cursorx, cursory, offsetx, offsety,
              chroma, saturate, saturateamt,
              narrow, focusdist;
        bool enabled;

        Config() { reset(); }
        ~Config() {}
        
        void resetfx()
        {
            offsetx = offsety = 0.0f;
            chroma = saturateamt = 0.0f;
            narrow = saturate = 1.0f;
            enabled = false;
        }
        
        void reset()
        {
            cursorx = cursory = 0.5f;
            focusdist = 0.0f;
            resetfx();
        }
    } config;

    VisorSurface() { type = RenderSurface::VISOR; }
    ~VisorSurface() { destroy(); }

    bool drawnoview();
    void drawprogress();

    float getcursorx(int type = 0);
    float getcursory(int type = 0);
    bool check();
    void coords(float cx, float cy, float &vx, float &vy, bool back = false);

    void checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n) override;
    int create(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = MAX) override;
    bool render(int w = 0, int h = 0, GLenum f = GL_RGBA, GLenum t = GL_TEXTURE_RECTANGLE, int count = MAX) override;
};
extern VisorSurface visorsurf;
namespace hud { void visorinfo(VisorSurface::Config &config); }

struct ViewSurface : RenderSurface
{
    vec worldpos = vec(0, 0, 0);
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f, fov = 90.0f, ratio = 0.0f, nearpoint = 0.54f, farscale = 1.0f;
    int texmode = DRAWTEX_SCENE;

    ViewSurface() { type = RenderSurface::VIEW; }
    ViewSurface(int m) : texmode(m) { type = RenderSurface::VIEW; }
    ~ViewSurface() { destroy(); }

    void checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n) override;
    bool render(int w = 0, int h = 0, GLenum f = GL_RGB, GLenum t = GL_TEXTURE_RECTANGLE, int count = 1) override;
};

#endif // STANDALONE

#include "sound.h"

// command extras
extern const char *getprintable(int arg);

#define PULSE_ENUM(en, um) \
    en(um, Fire, FIRE) en(um, Burn, BURN) en(um, Disco, DISCO) en(um, Shock, SHOCK) en(um, Bleed, BLEED) \
    en(um, Buff, BUFF) en(um, Warn, WARN) en(um, Health, HEALTH) en(um, Flash, FLASH) en(um, Rainbow, RAINBOW) \
    en(um, Corrode, CORRODE) en(um, Power, POWER) en(um, Earth, EARTH) en(um, Gray, GRAY) en(um, Alert, ALERT) \
    en(um, Growth, GROWTH) en(um, Magic, MAGIC) en(um, Divine, DIVINE) en(um, Shadow, SHADOW) \
    en(um, Headshot, HEADSHOT) en(um, Spree, SPREE) en(um, Break, BREAK) en(um, Ready, READY) en(um, Prize, PRIZE) \
    en(um, Multi, MULTI) en(um, Achievement, ACHIEVEMENT) en(um, Bloodbath, BLOODBATH) \
    en(um, Dominate, DOMINATE) en(um, Revenge, REVENGE) en(um, Decay, DECAY) \
    en(um, Maximum, MAX)
ENUM_DLN(PULSE)

ENUM_VAR(PULSE_LAST, PULSE_MAX - 1);
ENUM_VAR(PULSE_COLS, 11);
ENUM_VAR(PULSE_CYCLE, 100);

#define PULSE(x) (PULSE_##x)
#define INVPULSE(x) (-1-(x))
#define PC(x) (INVPULSE(PULSE(x)))

#ifdef CPP_ENGINE_COMMAND
extern const int pulsecols[PULSE_MAX][PULSE_COLS] = {
    { 0xCE8837, 0xCD853F, 0xD2946C, 0xAC5F5F, 0xD2691E, 0x8B4513, 0xA0522D, 0xCD853F, 0xD2B48C, 0xCD853F, 0xAE8857 }, // fire
    { 0xCD5C5C, 0xD2691E, 0xFF8C00, 0xFFA500, 0xCD853F, 0xB22222, 0xA52A2A, 0x8B4513, 0x800000, 0x8B0000, 0xB22222 }, // burning
    { 0xFFB6C1, 0xFFA07A, 0xFFFFE0, 0x90EE90, 0xADD8E6, 0xD8BFD8, 0xFFE4E1, 0xE6E6FA, 0xDDA0DD, 0xDA70D6, 0xEE82EE }, // disco time
    { 0x3D59AB, 0x0000CD, 0x4169E1, 0x0000FF, 0x6495ED, 0x1E90FF, 0x87CEFA, 0x87CEEB, 0x4682B4, 0x5F9EA0, 0x48D1CC }, // electric shock
    { 0xFF0000, 0xDC143C, 0xB22222, 0xFF6347, 0xFF4500, 0xFF0000, 0xCD5C5C, 0x8B0000, 0xA52A2A, 0x8B4513, 0x800000 }, // red bleed
    { 0xFFFFE0, 0xFFFFF0, 0xFFFACD, 0xFFF8DC, 0xFFEFD5, 0xFFE4B5, 0xFFDAB9, 0xFFDEAD, 0xF5DEB3, 0xFFE4C4, 0xFFEBCD }, // shield/buff
    { 0xFF4500, 0xFF6347, 0xFF7F50, 0xFFA500, 0xFF8C00, 0xFFD700, 0xFFA07A, 0xFFDEAD, 0xD2691E, 0xB22222, 0xA52A2A }, // warning
    { 0xFF0000, 0xFF4500, 0xFF8C00, 0xFFA500, 0xFFD700, 0xFFFF00, 0xADFF2F, 0x7CFC00, 0x00FF00, 0x40E0D0, 0x1E90FF }, // health regen
    { 0xFFFFFF, 0xF8F8FF, 0xF5F5F5, 0xDCDCDC, 0xD3D3D3, 0xC0C0C0, 0xA9A9A9, 0x808080, 0x696969, 0x778899, 0x708090 }, // flash
    { 0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3, 0x8A2BE2, 0x9932CC, 0x8A2BE2, 0x9400D3 }, // rainbow
    { 0x7FFF00, 0xADFF2F, 0x32CD32, 0x00FF7F, 0x00FA9A, 0x3CB371, 0x2E8B57, 0x228B22, 0x6B8E23, 0x808000, 0x556B2F }, // acid corrosion
    { 0x0000FF, 0x0000CD, 0x00008B, 0x000080, 0x191970, 0x1E90FF, 0x6495ED, 0x87CEEB, 0x87CEFA, 0x4682B4, 0x4169E1 }, // blue power
    { 0x8B4513, 0xA0522D, 0xD2691E, 0xCD853F, 0xF4A460, 0xDEB887, 0xD2B48C, 0xBC8F8F, 0xDAA520, 0xB8860B, 0xCD5C5C }, // earth tones
    { 0x708090, 0x778899, 0x2F4F4F, 0x696969, 0x808080, 0xA9A9A9, 0xC0C0C0, 0xD3D3D3, 0xDCDCDC, 0xF5F5F5, 0xFFFFFF }, // grayscale
    { 0x800000, 0x8B0000, 0xA52A2A, 0xB22222, 0xDC143C, 0xFF0000, 0xFF4500, 0xFF6347, 0xFF7F50, 0xFA8072, 0xE9967A }, // red alert
    { 0x006400, 0x008000, 0x228B22, 0x2E8B57, 0x32CD32, 0x3CB371, 0x90EE90, 0x98FB98, 0xADFF2F, 0x7FFF00, 0x7CFC00 }, // green growth
    { 0x4B0082, 0x6A5ACD, 0x7B68EE, 0x483D8B, 0x9370DB, 0x8A2BE2, 0x9400D3, 0x9932CC, 0xBA55D3, 0x800080, 0x8B008B }, // purple magic
    { 0xFFD700, 0xFFE4B5, 0xFFEBCD, 0xFFEFD5, 0xFFF0F5, 0xFFF5EE, 0xFFF8DC, 0xFFFAF0, 0xFFFAFA, 0xFFFFF0, 0xFFFFFF }, // divine light
    { 0x000000, 0x2F4F4F, 0x696969, 0x708090, 0x778899, 0x808080, 0xA9A9A9, 0xC0C0C0, 0xD3D3D3, 0xDCDCDC, 0x2F4F4F }, // shadow stealth
    { 0xFF0000, 0xB22222, 0x8B0000, 0x800000, 0xA52A2A, 0xCD5C5C, 0xDC143C, 0xFF4500, 0xFF6347, 0xFF7F50, 0xFF8C00 }, // headshot
    { 0xFFD700, 0xFFA500, 0xFF8C00, 0xFF7F50, 0xFF6347, 0xFF4500, 0xDC143C, 0xB22222, 0xA52A2A, 0x8B4513, 0x800000 }, // killing spree
    { 0xFF0000, 0xFF4500, 0xFF8C00, 0xFFD700, 0xFFFF00, 0x9ACD32, 0x008000, 0x00BFFF, 0x1E90FF, 0x0000CD, 0x800080 }, // spree breaker
    { 0xFF6347, 0x4169E1, 0xFFFF33, 0xFF69B4, 0x40E0D0, 0xFFA07A, 0xBA55D3, 0xFFA07A, 0xEE82EE, 0x87CEFA, 0xADFF2F }, // prize ready
    { 0xFF0000, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFF4500, 0x8A2BE2, 0xD2691E, 0x8B008B, 0x4682B4, 0x6B8E23 }, // prize awarded
    { 0x0000FF, 0x0000CD, 0x00008B, 0x000080, 0x191970, 0x1E90FF, 0x6495ED, 0x87CEEB, 0x87CEFA, 0x4682B4, 0x4169E1 }, // multi kill
    { 0xFF4500, 0xFF8C00, 0xFFD700, 0xADFF2F, 0x32CD32, 0x00FF7F, 0x00FA9A, 0x3CB371, 0x2E8B57, 0x228B22, 0x6B8E23 }, // achievement
    { 0x8B0000, 0xA52A2A, 0xB22222, 0xDC143C, 0xFF0000, 0xFF4500, 0xFF6347, 0xFF7F50, 0xFF8C00, 0xFFA500, 0xFFD700 }, // bloodbath
    { 0x9400D3, 0x9932CC, 0x8A2BE2, 0x800080, 0x9370DB, 0x7B68EE, 0x6A5ACD, 0x4B0082, 0x483D8B, 0x6A5ACD, 0x7B68EE }, // dominating
    { 0x101010, 0x2F4F4F, 0x696969, 0x708090, 0x778899, 0x808080, 0xA9A9A9, 0xC0C0C0, 0xD3D3D3, 0xDCDCDC, 0x2F4F4F }, // revenge
    { 0x7E8F7C, 0x4E6258, 0x5C512F, 0x736D58, 0x837C71, 0x6C705B, 0x5B664E, 0x4E5D37, 0x3D4B27, 0x2C3720, 0x1B2319 }, // decay
};
#else // CPP_ENGINE_COMMAND
extern const int pulsecols[PULSE_MAX][PULSE_COLS];
#endif // CPP_ENGINE_COMMAND

extern vec pulsecolour(int n = 0, int cycle = PULSE_CYCLE);
extern int pulsehexcol(int n = 0, int cycle = PULSE_CYCLE);
extern vec getpulsecolour(int n = 0, int cycle = PULSE_CYCLE);
extern int getpulsehexcol(int n = 0, int cycle = PULSE_CYCLE);
#ifndef STANDALONE
extern vec pulsecolour(physent *d, int n = 0, int cycle = PULSE_CYCLE);
extern int pulsehexcol(physent *d, int n = 0, int cycle = PULSE_CYCLE);
extern vec getpulsecolour(physent *d, int n = 0, int cycle = PULSE_CYCLE);
extern int getpulsehexcol(physent *d, int n = 0, int cycle = PULSE_CYCLE);
#endif // STANDALONE
#define PCVAR(flags, name, cur) VAR(flags|IDF_HEX, name, PC(LAST), cur, 0xFFFFFF)
#endif // CPP_ENGINE_HEADER

#if !defined(CPP_GAME_HEADER) || defined(CPP_GAME_SERVER)
#define ENUM_DEFINE
#include "enum.h"
#endif
