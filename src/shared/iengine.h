// the interface the game uses to access the engine

extern int verbose, curtime, lastmillis, totalmillis, elapsedtime, timescale, paused;
extern uint totalsecs;
extern time_t clocktime, currenttime, clockoffset;
extern int servertype, serverport, serverlanport, servermasterport;
extern char *servermaster, *serverip;
#ifdef STANDALONE
#define servercheck(x) (x)
#else
#define servercheck(x) (servertype >= 3 && (x))
#endif
extern ENetAddress masteraddress;
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int color, const char *s, ...) PRINTFARGS(2, 3);
extern void eventf(int color, const char *s, ...) PRINTFARGS(2, 3);

extern FILE *logfile;
extern FILE *getlogfile();
extern void setlogfile(const char *fname);
extern void closelogfile();
extern void logoutfv(const char *fmt, va_list args);
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);

#ifdef __GNUC__
#define _dbg_ fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
#else
#define _dbg_ fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
#endif

struct namemap { const char *name; int id; };

extern void lightent(extentity &e, float height = 8);

enum { RAY_BB = 1, RAY_POLY = 3, RAY_ALPHAPOLY = 7, RAY_ENTS = 9, RAY_CLIPMAT = 16, RAY_SKIPFIRST = 32, RAY_EDITMAT = 64, RAY_PASS = 128 };

extern float raycube   (const vec &o, const vec &ray,           float radius = 0, int mode = RAY_CLIPMAT, int size = 0, vector<int> *t = NULL);
extern float rayent    (const vec &o, const vec &ray,           float radius,     int mode,               int size, int &orient, vector<int> &ents, vector<int> *filter);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = RAY_CLIPMAT, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);
extern bool  rayoccluded(const vec &o, const vec &dest);

struct Texture;
extern bool settexture(Texture *t = NULL);
extern bool settexture(const char *name, int tclamp = 0);

// world
extern bool emptymap(int scale, bool force = false, const char *mname = NULL, bool usecfg = true);
extern bool enlargemap(bool split = false, bool force = false);
extern int findentity(int type, int index, vector<int> &attr);
extern void mpeditent(int i, const vec &o, int type, attrvector &attr, bool local = true);

// octa
extern int lookupmaterial(const vec &o);

static inline bool insideworld(const vec &o)
{
    extern int worldsize;
    return o.x>=0 && o.x<worldsize && o.y>=0 && o.y<worldsize && o.z>=0 && o.z<worldsize;
}

static inline bool insideworld(const ivec &o)
{
    extern int worldsize;
    return uint(o.x)<uint(worldsize) && uint(o.y)<uint(worldsize) && uint(o.z)<uint(worldsize);
}

// octaedit

enum { EDIT_FACE = 0, EDIT_TEX, EDIT_MAT, EDIT_FLIP, EDIT_COPY, EDIT_PASTE, EDIT_ROTATE, EDIT_REPLACE, EDIT_DELCUBE, EDIT_CALCLIGHT, EDIT_REMIP, EDIT_VSLOT, EDIT_UNDO, EDIT_REDO };

struct selinfo
{
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s;
    int grid, orient;
    selinfo() : corner(0), cx(0), cxs(0), cy(0), cys(0), o(0, 0, 0), s(0, 0, 0), grid(8), orient(0) {}
    int size() const    { return s.x*s.y*s.z; }
    int us(int d) const { return s[d]*grid; }
    bool operator==(const selinfo &sel) const { return o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient; }
    bool validate()
    {
        extern int worldsize;
        if(grid <= 0 || grid >= worldsize) return false;
        if(o.x >= worldsize || o.y >= worldsize || o.z >= worldsize) return false;
        if(o.x < 0) { s.x -= (grid - 1 - o.x)/grid; o.x = 0; }
        if(o.y < 0) { s.y -= (grid - 1 - o.y)/grid; o.y = 0; }
        if(o.z < 0) { s.z -= (grid - 1 - o.z)/grid; o.z = 0; }
        s.x = clamp(s.x, 0, (worldsize - o.x)/grid);
        s.y = clamp(s.y, 0, (worldsize - o.y)/grid);
        s.z = clamp(s.z, 0, (worldsize - o.z)/grid);
        return s.x > 0 && s.y > 0 && s.z > 0;
    }
};
extern selinfo sel;
extern bool havesel;
struct editinfo;
extern bool editmode;

extern int shouldpacktex(int index);
extern bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen);
extern void freeeditinfo(editinfo *&e);
extern void rendereditcursor();
extern void boxs(int orient, vec o, const vec &s, float size);
extern void boxs(int orient, vec o, const vec &s);
extern void boxs3D(const vec &o, vec s, int g);
extern void boxsgrid(int orient, vec o, vec s, int g);
extern void renderboundboxes();
extern void pruneundos(int maxremain = 0);
extern bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);
extern bool noedit(bool view = false, bool msg = true);
extern void toggleedit(bool force = true);
extern int mpeditface(int dir, int mode, selinfo &sel, bool local);
extern void mpedittex(int tex, int allfaces, selinfo &sel, bool local);
extern bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpeditmat(int matid, int filter, int style, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);
extern void mpdelcube(selinfo &sel, bool local);
extern void mpremip(bool local);
extern bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpcalclight(bool local);
extern void filltexlist();
extern void edittex(int i, bool save = true, bool edit = true);

// console
extern int changedkeys;

extern void processtextinput(const char *str, int len);
extern void processkey(int code, bool isdown);
extern void resetcomplete();
extern void complete(char *str, bool reverse);
extern const char *searchbind(const char *action, int type, int modifiers = 0);
extern void searchbindlist(const char *action, int type, int modifiers, int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names, bool force = true);

extern bool capslockon, numlockon;
extern bool capslocked();
extern bool numlocked();

struct bindlist
{
    vector<char> names;
    int lastsearch;

    bindlist() : lastsearch(-1) {}

    const char *search(const char *action, int type = 0, int modifiers = 0, const char *s1 = "\f{", const char *s2 = "}", const char *sep1 = " ", const char *sep2 = " ", int limit = 5)
    {
        if(names.empty() || lastsearch != changedkeys)
        {
            names.shrink(0);
            searchbindlist(action, type, modifiers, limit, s1, s2, sep1, sep2, names);
            lastsearch = changedkeys;
        }
        return names.getbuf();
    }
};

// texture

struct VSlot;

extern void packvslot(vector<uchar> &buf, int index);
extern void packvslot(vector<uchar> &buf, const VSlot *vs);

// renderlights

enum { L_NOSHADOW = 1<<0, L_NODYNSHADOW = 1<<1, L_VOLUMETRIC = 1<<2, L_NOSPEC = 1<<3, L_SMALPHA = 1<<4, L_DYNWORLDCHECK = 1<<5, L_ALL = L_NOSHADOW|L_NODYNSHADOW|L_VOLUMETRIC|L_NOSPEC|L_SMALPHA|L_DYNWORLDCHECK };

// dynlight
enum
{
    DL_SHRINK = 1<<8,
    DL_EXPAND = 1<<9,
    DL_FLASH  = 1<<10
};

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0), physent *owner = NULL, const vec &dir = vec(0, 0, 0), int spot = 0);
extern void removetrackeddynlights(physent *owner = NULL);

// rendergl
extern vec worldpos, camdir, camright, camup;
extern void gettextres(int &w, int &h);

extern vec calcavatarpos(const vec &pos, float fov);
extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

extern int minimapsize;
extern vec minimapcenter, minimapradius, minimapscale;
extern void bindminimap();

extern matrix4 hudmatrix;
extern void setcamprojmatrix(bool init = true, bool flush = false);
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);
extern void resethudshader();

// renderparticles
enum
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_EXPLOSION,
    PT_LIGHTNING,
    PT_FLARE,
    PT_PORTAL,
    PT_ICON,
    PT_LINE,
    PT_TRIANGLE,
    PT_ELLIPSE,
    PT_CONE,
    PT_TYPE     = 0xFF,

    PT_MOD      = 1<<8,
    PT_RND4     = 1<<9,     // uses random image quarters
    PT_LERP     = 1<<10,    // use very sparingly - order of blending issues
    PT_BRIGHT   = 1<<11,
    PT_SOFT     = 1<<12,    // use soft quad rendering when available
    PT_HFLIP    = 1<<13,    // uses random horizontal flipping
    PT_VFLIP    = 1<<14,    // uses random vertical flipping
    PT_ROT      = 1<<15,    // uses random rotation
    PT_CULL     = 1<<16,
    PT_FEW      = 1<<17,    // allocate smaller number of particles
    PT_ONTOP    = 1<<18,    // render on top of everything else, remove depth testing
    PT_NOTEX    = 1<<19,
    PT_SHADER   = 1<<20,
    PT_NOLAYER  = 1<<21,
    PT_RND16    = 1<<22,    // uses random image sixteenths
    PT_MATERIAL = 1<<23,    // can only exist in same material
    PT_WIND     = 1<<24,    // particles affected by the wind
    PT_HAZE     = 1<<25,    // creates haze effect
    PT_ENVMAP   = 1<<26,    // has envmap
    PT_FLIP     = PT_HFLIP | PT_VFLIP | PT_ROT
};

#define PART_ENUM(en, um) \
    en(um, Portal, PORTAL) en(um, Portal Env, PORTAL_ENV) en(um, Icon, ICON) en(um, Icon Ontop, ICON_ONTOP) \
    en(um, Line, LINE) en(um, Line Ontop, LINE_ONTOP) en(um, Triangle, TRIANGLE) en(um, Triangle Ontop, TRIANGLE_ONTOP) \
    en(um, Ellipse, ELLIPSE) en(um, Ellipse Ontop, ELLIPSE_ONTOP) en(um, Cone, CONE) en(um, Cone Ontop, CONE_ONTOP) \
    en(um, Fireball Lerp, FIREBALL_LERP) en(um, Plasma Lerp, PLASMA_LERP) \
    en(um, Flare Lerp, FLARE_LERP) en(um, Muzzle Flare Lerp, MUZZLE_FLARE_LERP) \
    en(um, Smoke Lerp Soft, SMOKE_LERP_SOFT) en(um, Smoke Lerp, SMOKE_LERP) \
    en(um, Smoke Soft, SMOKE_SOFT) en(um, Smoke, SMOKE) \
    en(um, Hint Soft, HINT_SOFT) en(um, Hint, HINT) \
    en(um, Hint Bold Soft, HINT_BOLD_SOFT) en(um, Hint Bold, HINT_BOLD) \
    en(um, Hint Vert Soft, HINT_VERT_SOFT) en(um, Vert Bold, VERT_BOLD) \
    en(um, Hint Horz Soft, HINT_HORZ_SOFT) en(um, Horz Bold, HORZ_BOLD) \
    en(um, Blood, BLOOD) \
    en(um, Entity, ENTITY) en(um, Entity Ontop, ENTITY_ONTOP) \
    en(um, Spark, SPARK) \
    en(um, Fireball Soft, FIREBALL_SOFT) en(um, Fireball, FIREBALL) \
    en(um, Plasma Soft, PLASMA_SOFT) en(um, Plasma, PLASMA) \
    en(um, Electric Soft, ELECTRIC_SOFT) en(um, Electric, ELECTRIC) \
    en(um, Eleczap Soft, ELECZAP_SOFT) en(um, Eleczap, ELECZAP) \
    en(um, Flame, FLAME) \
    en(um, Flare, FLARE) en(um, Clean Flare, CLEAN_FLARE) en(um, Noisy Flare, NOISY_FLARE) en(um, Muzzle Flare, MUZZLE_FLARE) \
    en(um, Lightning Flare, LIGHTNING_FLARE) en(um, Lightzap Flare, LIGHTZAP_FLARE) \
    en(um, Muzzle Flash, MUZZLE_FLASH) en(um, Snow, SNOW) \
    en(um, Haze, HAZE) en(um, Haze Flame, FLAME_HAZE) en(um, Haze Tape, TAPE_HAZE) \
    en(um, Rain, RAIN) \
    en(um, Bubble Soft, BUBBLE_SOFT) en(um, Bubble, BUBBLE) \
    en(um, Splash Soft, SPLASH_SOFT) en(um, Splash, SPLASH) \
    en(um, Bubble Haze, BUBBLE_HAZE) en(um, Bubble Material, BUBBLE_MATERIAL) \
    en(um, Text, TEXT) en(um, Text Ontop, TEXT_ONTOP) \
    en(um, Explosion, EXPLOSION) en(um, Shockwave, SHOCKWAVE) en(um, Shockball, SHOCKBALL) en(um, Glimmery, GLIMMERY) \
    en(um, Lightning, LIGHTNING) en(um, Lightzap, LIGHTZAP) \
    en(um, Lens Flare, LENS_FLARE) \
    en(um, Max, MAX)
ENUM_DLN(PART);
ENUM_VAR(PART_LAST, PART_TEXT - 1);

struct particle
{
    vec o, d, m, prev;
    int collide, fade, gravity, millis, inmaterial;
    bvec color, hintcolor, envcolor;
    uchar flags;
    windprobe wind;
    float size, sizechange, blend, hintblend, envblend;
    bool enviro, precollide;
    union
    {
        const char *text;
        float val;
        struct
        {
            uchar color2[3];
            uchar progress;
        };
    };
    physent *owner;
};

extern void removetrackedparticles(physent *pl = NULL);
extern int particletext, maxparticledistance, flarelights;

extern particle *newparticle(const vec &o, const vec &d, int fade, int type, int color = colourwhite, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float val = 0, physent *pl = NULL, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void create(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regularcreate(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, physent *pl = NULL, int delay = 0, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void splash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float vel = 1, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regularsplash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float vel = 1, int delay = 0, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void createshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float vel = 1, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regularshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = 0, int collide = 0, float vel = 1, float sizechange = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void regularflame(int type, const vec &p, float radius, float height, int color, int density = 3, int fade = 500, float size = 2, float blend = 1, int hintcolor = 0, float hintblend = 0, float gravity = -1, int collide = 0, float vel = 1, int envcolor = 0xFFFFFF, float envblend = 0.5f);
extern void lensflare(const vec &o, const vec &color, bool sun, int sparkle, float scale = 1);

// stain
#define STAIN_ENUM(en, um) \
    en(um, Smoke, SMOKE) en(um, Scorch, SCORCH) en(um, Short Scorch, SCORCH_SHORT) \
    en(um, Blood, BLOOD) en(um, Bullet, BULLET) en(um, Energy, ENERGY) \
    en(um, Splash, SPLASH) en(um, Envmap Splash, ENVSPLASH) en(um, Envmap Splash Glow, ENVSPLASH_GLOW) \
    en(um, Splat, SPLAT) en(um, Envmap Splat, ENVSPLAT) en(um, Envmap Splat Glow, ENVSPLAT_GLOW) \
    en(um, Stain, STAIN)  en(um, Maximum, MAX)
ENUM_DLN(STAIN);

extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0, const bvec4 &envcolor = bvec4(0xFF, 0xFF, 0xFF, 0x80));

static inline void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0, const bvec &envcolor = bvec(0xFF, 0xFF, 0xFF), float envblend = 0.5f)
{
    addstain(type, center, surface, radius, color, info, bvec4(envcolor, uchar(envblend * 255)));
}

static inline void addstain(int type, const vec &center, const vec &surface, float radius, int color, int info = 0, int envcolor = 0xFFFFFF, float envblend = 0.5f)
{
    addstain(type, center, surface, radius, bvec::fromcolor(color), info, bvec4::fromcolor(envcolor, envblend));
}

// worldio
extern void setnames(const char *fname, int crc = 0);
extern bool load_world(const char *mname, int crc = 0, int variant = MPV_DEFAULT);
extern void save_world(const char *mname, bool nodata = false, bool forcesave = false);
extern char *mapctitle(const char *s);
extern char *mapcauthor(const char *s);
extern char *mapcdesc(const char *s);

// physics
extern bool ellipsecollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0, bool playercol = true, bool insideplayercol = false, float guard = 0, bool npcol = true);
extern bool plcollide(physent *d, const vec &dir = vec(0, 0, 0), bool insideplayercol = false, float guard = 0);
extern bool plcollide(physent *d, const vec &dir, physent *o, float guard = 0);
extern float pltracecollide(physent *d, const vec &o, const vec &ray, float maxdist, float guard = 0);
extern float tracecollide(physent *d, const vec &o, const vec &ray, float maxdist, int mode = RAY_CLIPMAT|RAY_ALPHAPOLY, bool playercol = true, float guard = 0);
extern bool intersect(physent *d, const vec &from, const vec &to, float &dist, float guard);
extern bool overlapsbox(const vec &d, float h1, float r1, const vec &v, float h2, float r2);
extern const vector<physent *> &checkdynentcache(int x, int y);
extern void updatedynentcache(physent *d);
extern void cleardynentcache();

template<class T> static inline bool overlapsbb(const T &bbmin1, const T &bbmax1, const T &bbmin2, const T &bbmax2)
{
    if (bbmax1.x < bbmin2.x || bbmin1.x > bbmax2.x) return false;
    if (bbmax1.y < bbmin2.y || bbmin1.y > bbmax2.y) return false;
    if (bbmax1.z < bbmin2.z || bbmin1.z > bbmax2.z) return false;
    return true;
}

// rendermodel
extern void rendermodel(const char *mdl, modelstate &state, dynent *d = NULL);
extern int intersectmodel(const char *mdl, modelstate &state, const vec &o, const vec &ray, float &dist, int mode = 0, dynent *d = NULL);
extern void abovemodel(vec &o, const char *mdl);
extern void interpolateorientation(dynent *d, float &interpyaw, float &interppitch);
extern void setbbfrommodel(dynent *d, const char *mdl, float size = 1);
extern const char *mapmodelname(int i);
extern model *loadmodel(const char *name, int i = -1, bool msg = false, model *parent = NULL);
extern model *loadbestlod(model *m, const vec &center, float radius = 1, float offset = 0, bool lodvis = true);
extern void preloadmodel(const char *name);
extern void flushpreloadedmodels(bool msg = true);
extern void resetmapmodels(int n = 0);

// ragdoll
extern bool validragdoll(dynent *d, int millis);
extern void moveragdoll(dynent *d);
extern void cleanragdoll(dynent *d);
extern void warpragdoll(dynent *d, const vec &vel, const vec &offset = vec(0, 0, 0));

// server
#define MAXCLIENTS 256  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000   // max amount of data to swallow in 1 go
#define MAXSDESCLEN 80  // max length of server description field

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_KICK, DISC_MSGERR, DISC_IPBAN, DISC_PRIVATE, DISC_PASSWORD, DISC_PURE, DISC_MAXCLIENTS, DISC_INCOMPATIBLE, DISC_TIMEOUT, DISC_OVERFLOW, DISC_SHUTDOWN, DISC_HOSTFAIL, DISC_AUTH, DISC_NUM };

extern void *getinfo(int i);
extern const char *gethostip(int i);
extern void sendf(int cn, int chan, const char *format, ...);
extern void sendfile(int cn, int chan, stream *file, const char *format = "", ...);
extern void sendpacket(int cn, int chan, ENetPacket *packet, int exclude = -1);
extern void flushserver(bool force);
extern int getservermtu();
extern int getnumclients();
extern uint getclientip(int n);
extern bool filterword(char *src, const char *list);
extern bool filterstring(char *dst, const char *src, bool newline, bool colour, bool whitespace, bool wsstrip, size_t len);
template<size_t N> static inline bool filterstring(char (&dst)[N], const char *src, bool newline = true, bool colour = true, bool whitespace = true, bool wsstrip = false) { return filterstring(dst, src, newline, colour, whitespace, wsstrip, N-1); }
extern void disconnect_client(int n, int reason);
extern void kicknonlocalclients(int reason);
extern bool hasnonlocalclients();
extern bool haslocalclients();
extern void limitdupclients();
extern void sendqueryreply(ucharbuf &p);
extern bool resolverwait(const char *name, ENetAddress *address);
extern int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address);
extern bool connectedmaster();
extern ENetSocket connectmaster(bool reuse = true);
extern void disconnectmaster();
extern bool requestmaster(const char *req);
extern bool requestmasterf(const char *fmt, ...) PRINTFARGS(1, 2);
extern void flushmasteroutput();
extern void flushmasterinput();

extern void setverinfo(const char *bin);
extern void setlocations(const char *bin);

// client
struct serverinfo
{
    enum
    {
        MAXPINGS = 3,
        WAITING = INT_MAX
    };
    enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

    string name, map, sdesc, authhandle, flags, branch;
    int numplayers, lastping, lastinfo, nextping, ping, resolved, port, priority;
    int pings[MAXPINGS];
    vector<int> attr;
    vector<char *> players, handles;
    ENetAddress address;

    serverinfo(uint ip, int port, int priority = 0)
     : numplayers(0), resolved(ip==ENET_HOST_ANY ? UNRESOLVED : RESOLVED), port(port), priority(priority)
    {
        name[0] = map[0] = sdesc[0] = authhandle[0] = flags[0] = branch[0] = '\0';
        address.host = ip;
        address.port = port+1;
        clearpings();
    }
    ~serverinfo() { cleanup(); }

    void clearpings()
    {
        ping = WAITING;
        loopk(MAXPINGS) pings[k] = WAITING;
        nextping = 0;
        lastping = lastinfo = -1;
    }

    void cleanup()
    {
        clearpings();
        attr.setsize(0);
        players.deletearrays();
        handles.deletearrays();
        numplayers = 0;
    }

    void reset()
    {
        lastping = lastinfo = -1;
    }

    void checkdecay(int decay)
    {
        if(lastping >= 0 && totalmillis - lastping >= decay)
            cleanup();
        if(lastping < 0) lastping = totalmillis ? totalmillis : 1;
    }

    void calcping()
    {
        int numpings = 0, totalpings = 0;
        loopk(MAXPINGS) if(pings[k] != WAITING) { totalpings += pings[k]; numpings++; }
        ping = numpings ? totalpings/numpings : WAITING;
    }

    void addping(int rtt, int millis)
    {
        if(millis >= lastping) lastping = -1;
        pings[nextping] = rtt;
        nextping = (nextping+1)%MAXPINGS;
        calcping();
    }
};

extern vector<serverinfo *> servers;

extern void sendclientpacket(ENetPacket *packet, int chan);
extern void flushclient();
extern void disconnect(bool onlyclean = false, bool async = false);
extern bool multiplayer(bool msg = true);
extern void neterr(const char *s);
extern void gets2c();

// crypto
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern bool calcpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern bool answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);

// UI
struct editor;
enum { EDITORFOCUSED = 1, EDITORUSED, EDITORFOREVER, EDITORREADONLY };
enum { CURSOR_DEFAULT = 0, CURSOR_HOVER, CURSOR_HIDDEN, CURSOR_MAX };

#define SURFACE_ENUM(en, um) \
    en(um, Visor, VISOR) en(um, Background, BACKGROUND) en(um, Foreground, FOREGROUND) en(um, World, WORLD) \
    en(um, Progress, PROGRESS) en(um, Composite, COMPOSITE) en(um, Maximum, MAX)
ENUM_DLN(SURFACE);
ENUM_VAR(SURFACE_MAIN, SURFACE_FOREGROUND);
ENUM_VAR(SURFACE_LOOP, SURFACE_MAIN+1);
ENUM_VAR(SURFACE_LAST, SURFACE_WORLD);
ENUM_VAR(SURFACE_ALL, SURFACE_LAST+1);

namespace UI
{
    static const vec nullvec = vec(-FLT_MAX);

    enum
    {
        MAPUI_NONE = 0, MAPUI_SHOWPROX = 1<<0, MAPUI_INPUTPROX = 1<<1, MAPUI_VISOR = 1<<2, MAPUI_BACKGROUND = 1<<3, MAPUI_FOREGROUND = 1<<4,
        MAPUI_ALL = MAPUI_SHOWPROX|MAPUI_INPUTPROX|MAPUI_VISOR|MAPUI_BACKGROUND|MAPUI_FOREGROUND, MAPUI_PROXIMITY = MAPUI_SHOWPROX|MAPUI_INPUTPROX
    };

    extern bool showui(const char *name, int stype = SURFACE_FOREGROUND, int param = -1, const vec &origin = nullvec, float maxdist = 0, float yaw = -1, float pitch = 0, float scale = 1, float detentyaw = 0, float detentpitch = 0);
    extern bool pokeui(const char *name, int stype = SURFACE_FOREGROUND, int param = -1, const vec &origin = nullvec, float maxdist = 0, float yaw = -1, float pitch = 0, float scale = 1, float detentyaw = 0, float detentpitch = 0);
    extern bool hideui(const char *name = NULL, int stype = SURFACE_FOREGROUND, int param = -1);
    extern bool toggleui(const char *name, int stype = SURFACE_FOREGROUND, int param = -1, const vec &origin = nullvec, float maxdist = 0, float yaw = -1, float pitch = 0, float scale = 1, float detentyaw = 0, float detentpitch = 0);
    extern int openui(const char *name, int stype = SURFACE_FOREGROUND);
    extern int closeui(const char *name, int stype = SURFACE_FOREGROUND);
    extern void hideall();

    extern void holdui(const char *name, bool on, int stype = SURFACE_FOREGROUND, int param = -1, const vec &origin = nullvec, float maxdist = 0, float yaw = -1, float pitch = 0, float scale = 1, float detentyaw = 0, float detentpitch = 0);
    extern void pressui(const char *name, bool on, int stype = SURFACE_FOREGROUND, int param = -1, const vec &origin = nullvec, float maxdist = 0, float yaw = -1, float pitch = 0, float scale = 1, float detentyaw = 0, float detentpitch = 0);
    extern bool uitest(const char *name, int stype = SURFACE_FOREGROUND, int param = -1);
    extern int hasinput(bool cursor = false, int stype = -1);
    extern bool hasmenu(bool pass = true, int stype = -1);
    extern bool keypress(int code, bool isdown);
    extern bool textinput(const char *str, int len);

    extern void closedynui(const char *name, int stype = -1, bool mapdef = false);
    extern void cleardynui(const char *name, int stype = -1, bool mapdef = false);
    extern void closemapuis(int n, int stype = -1);

    extern Texture *composite(const char *name, int tclamp = 0, bool mipit = true, bool msg = true, bool gc = false, Texture *tex = NULL, bool reload = false);

    extern void mousetrack(float dx, float dy);
    extern bool cursorlock();
    extern int cursortype();

    extern int savemap(stream *h);
    extern void resetmap();

    extern int processviewports();
    extern void updatetextures();

    extern void setup();
    extern void cleanup();
    extern void cleangl();
    extern void poke(bool ticks = true);
    extern void update();
    extern void build(bool noview);
    extern void render(int stype);
}

// menus
extern void addchange(const char *desc, int type);
extern void clearchanges(int type);

// client
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP, ST_REMOTE };

struct clientdata
{
    int type;
    int num;
    ENetPeer *peer;
    string hostip;
    void *info;
};

extern void process(ENetPacket *packet, int sender, int chan);
extern void delclient(int n);
extern int addclient(int type = ST_EMPTY);
extern ENetHost *serverhost;

// world

extern int collideinside, collidematerial;
extern physent *collideplayer;
extern int collidezones;
extern vec collidewall, hitsurface;

enum { CLZ_NONE = 0, CLZ_LIMBS = 1<<0, CLZ_TORSO = 1<<1, CLZ_HEAD = 1<<2, CLZ_FULL = 1<<3, CLZ_NOHEAD = CLZ_LIMBS|CLZ_TORSO, CLZ_ALL = CLZ_LIMBS|CLZ_TORSO|CLZ_HEAD };

enum
{
    MATF_INDEX_SHIFT  = 0,
    MATF_VOLUME_SHIFT = 2,
    MATF_CLIP_SHIFT   = 5,
    MATF_FLAG_SHIFT   = 8,

    MATF_INDEX  = 3 << MATF_INDEX_SHIFT,
    MATF_VOLUME = 7 << MATF_VOLUME_SHIFT,
    MATF_CLIP   = 7 << MATF_CLIP_SHIFT,
    MATF_FLAGS  = 0xFF << MATF_FLAG_SHIFT,

    MATF_NUMVOL = MATF_VOLUME >> MATF_VOLUME_SHIFT
};

enum // cube empty-space materials
{
    MAT_AIR    = 0,                      // the default, fill the empty space with air
    MAT_WATER  = 1 << MATF_VOLUME_SHIFT, // fill with water, showing waves at the surface
    MAT_LAVA   = 2 << MATF_VOLUME_SHIFT, // fill with lava
    MAT_GLASS  = 3 << MATF_VOLUME_SHIFT, // behaves like clip but is textured, etc
    MAT_VOLFOG = 4 << MATF_VOLUME_SHIFT, // volumetric fog

    MAT_NOCLIP = 1 << MATF_CLIP_SHIFT,   // collisions always treat cube as empty
    MAT_CLIP   = 2 << MATF_CLIP_SHIFT,   // collisions always treat cube as solid
    MAT_AICLIP = 3 << MATF_CLIP_SHIFT,   // clip waypoints etc

    MAT_DEATH  = 1 << MATF_FLAG_SHIFT,   // force player suicide
    MAT_LADDER = 2 << MATF_FLAG_SHIFT,   // acts as ladder (move up/down)
    MAT_ALPHA  = 4 << MATF_FLAG_SHIFT,   // alpha blended
    MAT_HURT   = 8 << MATF_FLAG_SHIFT,   // hurt at intervals
    MAT_NOGI   = 16 << MATF_FLAG_SHIFT   // disable global illumination
};
