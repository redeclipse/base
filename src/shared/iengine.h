// the interface the game uses to access the engine

extern int verbose, curtime, lastmillis, totalmillis, timescale, paused;
extern uint totalsecs;
extern time_t clocktime, currenttime, clockoffset;
extern int servertype, serverport, serverlanport, servermasterport;
extern char *servermaster, *serverip;
#ifdef STANDALONE
#define ifserver(x) if(x)
#define ifnserver(x) if(x)
#else
#define ifserver(x) if(servertype >= 3 && (x))
#define ifnserver(x) if(servertype < 3 && (x))
#endif
extern ENetAddress masteraddress;
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);
extern void console(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutft(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);

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
extern void lightreaching(const vec &target, vec &color, vec &dir, bool fast = false, extentity *e = 0, float ambient = 0.4f);
extern const extentity *brightestlight(const vec &target, const vec &dir);

enum { RAY_BB = 1, RAY_POLY = 3, RAY_ALPHAPOLY = 7, RAY_ENTS = 9, RAY_CLIPMAT = 16, RAY_SKIPFIRST = 32, RAY_EDITMAT = 64, RAY_SHADOW = 128, RAY_PASS = 256, RAY_SKIPSKY = 512 };

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = RAY_CLIPMAT, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = RAY_CLIPMAT, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);

struct Texture;

extern void settexture(const char *name, int clamp = 0);

// world
extern bool emptymap(int factor, bool force = false, char *mname = NULL, bool nocfg = false);
extern bool enlargemap(bool split = false, bool force = false);
extern int findentity(int type, int index, vector<int> &attr);
extern void mpeditent(int i, const vec &o, int type, attrvector &attr, bool local = true);
extern int getworldsize();
extern int getmapversion();

// octaedit

enum { EDIT_FACE = 0, EDIT_TEX, EDIT_MAT, EDIT_FLIP, EDIT_COPY, EDIT_PASTE, EDIT_ROTATE, EDIT_REPLACE, EDIT_DELCUBE, EDIT_REMIP, EDIT_VSLOT, EDIT_UNDO, EDIT_REDO };

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
        int worldsize = getworldsize();
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

struct editinfo;

extern bool editmode;

extern int shouldpacktex(int index);
extern bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen);
extern void freeeditinfo(editinfo *&e);
extern void rendereditcursor();
extern void pruneundos(int maxremain = 0);
extern bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);
extern bool noedit(bool view = false);
extern void toggleedit(bool force = true);
extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
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
extern bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpremip(bool local);

// texture

struct VSlot;

extern void packvslot(vector<uchar> &buf, int index);
extern void packvslot(vector<uchar> &buf, const VSlot *vs);

// console
extern int changedkeys;

extern void keypress(int code, bool isdown, int cooked);
extern char *getcurcommand();
extern void resetcomplete();
extern void complete(char *s, const char *cmdprefix);
extern const char *searchbind(const char *action, int type);
extern void searchbindlist(const char *action, int type, int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names, bool force = true);
extern int textkeybg, textkeyseps;

extern bool capslockon, numlockon;
extern bool capslocked();
extern bool numlocked();

struct bindlist
{
    vector<char> names;
    int lastsearch;

    bindlist() : lastsearch(-1) {}

    const char *search(const char *action, int type = 0, const char *s1 = "\f{", const char *s2 = "}", int limit = 5)
    {
        if(names.empty() || lastsearch != changedkeys)
        {
            names.shrink(0);
            searchbindlist(action, type, limit, s1, s2, textkeyseps ? (textkeybg ? "|" : ", ") : (textkeybg ? "" : " "), textkeyseps ? (textkeybg ? "|" : " or ") : (textkeybg ? "" : " "), names);
            lastsearch = changedkeys;
        }
        return names.getbuf();
    }
};

#define SEARCHBINDCACHE(def) static bindlist __##def; const char *def = __##def.search

// menus
extern void newgui(char *name, char *contents, char *initscript = NULL);
extern void showgui(const char *name, int tab = 0, bool *keep = NULL);

// main
struct igame;
extern void keyrepeat(bool on);

// rendertext
extern char *savecolour, *restorecolour, *green, *blue, *yellow, *red, *gray, *magenta, *orange, *white, *black, *cyan;

enum
{
    TEXT_SHADOW         = 1<<0,
    TEXT_NO_INDENT      = 1<<1,
    TEXT_UPWARD         = 1<<2,
    TEXT_BALLOON        = 1<<3,

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

extern bool setfont(const char *name);
extern bool pushfont(const char *name);
extern bool popfont(int num = 1);
extern int draw_text(const char *str, int rleft, int rtop, int r = 255, int g = 255, int b = 255, int a = 255, int flags = TEXT_SHADOW, int cursor = -1, int maxwidth = -1);
extern int draw_textx(const char *fstr, int left, int top, int r = 255, int g = 255, int b = 255, int a = 255, int flags = TEXT_SHADOW, int cursor = -1, int maxwidth = -1, ...);
extern int draw_textf(const char *fstr, int left, int top, ...) PRINTFARGS(1, 4);
extern float text_widthf(const char *str, int flags = 0);
extern void text_boundsf(const char *str, float &width, float &height, int maxwidth = -1, int flags = 0);
extern int text_visible(const char *str, float hitx, float hity, int maxwidth = -1, int flags = 0);
extern void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth, int flags = 0);

static inline int text_width(const char *str, int flags = 0)
{
    return int(ceil(text_widthf(str, flags)));
}

static inline void text_bounds(const char *str, int &width, int &height, int maxwidth = -1, int flags = 0)
{
    float widthf, heightf;
    text_boundsf(str, widthf, heightf, maxwidth, flags);
    width = int(ceil(widthf));
    height = int(ceil(heightf));
}

static inline void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth, int flags = 0)
{
    float cxf, cyf;
    text_posf(str, cursor, cxf, cyf, maxwidth, flags);
    cx = int(cxf);
    cy = int(cyf);
}

// renderva
enum
{
    DL_SHRINK = 1<<0,
    DL_EXPAND = 1<<1,
    DL_FLASH  = 1<<2,
    DL_KEEP   = 1<<3
};

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0));
extern void dynlightreaching(const vec &target, vec &color, vec &dir);
extern void makelightfx(extentity &e, extentity &f);

// rendergl
extern vec worldpos, camdir, camright, camup;
extern void getscreenres(int &w, int &h);
extern void gettextres(int &w, int &h);

extern vec minimapcenter, minimapradius, minimapscale;
extern void bindminimap();

extern matrix4 hudmatrix;
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);

// renderparticles
enum
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_FIREBALL,
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
    PT_HFLIP    = 1<<10,    // uses random horizontal flipping
    PT_VFLIP    = 1<<11,    // uses random vertical flipping
    PT_ROT      = 1<<12,    // uses random rotation
    PT_LERP     = 1<<13,    // use very sparingly - order of blending issues
    PT_GLARE    = 1<<14,    // use glare when available
    PT_SOFT     = 1<<15,    // use soft quad rendering when available
    PT_ONTOP    = 1<<16,    // render on top of everything else, remove depth testing
    PT_FEW      = 1<<17,    // allocate smaller number of particles
    PT_SHRINK   = 1<<18,    // shrink particle as it fades
    PT_GROW     = 1<<19,    // grow particle as it fades
    PT_NOTEX    = 1<<20,
    PT_SHADER   = 1<<21,
    PT_FLIP     = PT_HFLIP | PT_VFLIP | PT_ROT
};

enum
{
    PART_TELEPORT = 0, PART_ICON,
    PART_LINE, PART_LINE_ONTOP, PART_TRIANGLE, PART_TRIANGLE_ONTOP,
    PART_ELLIPSE, PART_ELLIPSE_ONTOP, PART_CONE, PART_CONE_ONTOP,
    PART_FIREBALL_LERP, PART_PLASMA_LERP, PART_FLARE_LERP, PART_MUZZLE_FLARE_LERP,
    PART_SMOKE_LERP_SOFT, PART_SMOKE_LERP, PART_HINT_LERP_SOFT, PART_HINT_LERP, PART_HINT_BOLD_LERP_SOFT, PART_HINT_BOLD_LERP,
    PART_SMOKE_SOFT, PART_SMOKE, PART_HINT_SOFT, PART_HINT, PART_HINT_BOLD_SOFT, PART_HINT_BOLD,
    PART_BLOOD,
    PART_EDIT, PART_EDIT_ONTOP,
    PART_SPARK,
    PART_FIREBALL_SOFT, PART_FIREBALL,
    PART_PLASMA_SOFT, PART_PLASMA,
    PART_ELECTRIC_SOFT, PART_ELECTRIC,
    PART_ELECZAP_SOFT, PART_ELECZAP,
    PART_FLAME,
    PART_FLARE, PART_MUZZLE_FLARE, PART_LIGHTNING_FLARE, PART_LIGHTZAP_FLARE,
    PART_MUZZLE_FLASH,
    PART_SNOW,
    PART_TEXT, PART_TEXT_ONTOP,
    PART_EXPLOSION, PART_SHOCKWAVE, PART_SHOCKBALL,
    PART_LIGHTNING, PART_LIGHTZAP,
    PART_LENS_FLARE,
    PART_MAX
};

struct particle
{
    vec o, d, m;
    int collide, fade, grav, millis;
    bvec color;
    uchar flags;
    float size, blend;
    union
    {
        const char *text;         // will call delete[] on this only if it starts with an @
        float val;
        struct
        {
            uchar color2[3];
            uchar progress;
        };
    };
    physent *owner;
};

extern void regular_part_create(int type, int fade, const vec &p, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL, int delay = 0);
extern void part_create(int type, int fade, const vec &p, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regular_part_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0, float radius = 150, float vel = 1, int delay = 0);
extern void part_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0, float radius = 4, float vel = 1);
extern void part_trail(int ptype, int fade, const vec &s, const vec &e, int color = 0xFFFFFF, float size = .8f, float blend = 1, int grav = 0, int collide = 0);
extern void part_text(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = 0xFFFFFF, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_textcopy(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = 0xFFFFFF, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_flare(const vec &p, const vec &dest, int fade, int type, int color = 0xFFFFFF, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regular_part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_spawn(const vec &o, const vec &v, float z, uchar type, int amt = 1, int fade = 1, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_flares(const vec &o, const vec &v, float z1, const vec &d, const vec &w, float z2, uchar type, int amt = 1, int fade = 1, int color = 0xFFFFFF, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_portal(const vec &o, float size, float blend = 1, float yaw = 0, float pitch = 0, int type = PART_TELEPORT, int fade = 1, int color = 0xFFFFFF);
extern void part_icon(const vec &o, Texture *tex, float size = 2, float blend = 1, int grav = 0, int collide = 0, int fade = 1, int color = 0xFFFFFF, float start = 0, float length = 1, physent *pl = NULL);
extern void part_line(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = 0xFFFFFF, int type = PART_LINE);
extern void part_triangle(const vec &o, float yaw, float pitch, float size = 1, float blend = 1, int fade = 1, int color = 0xFFFFFF, bool fill = true, int type = PART_TRIANGLE);
extern void part_dir(const vec &o, float yaw, float pitch, float length = 1, float size = 1, float blend = 1, int fade = 1, int color = 0x0000FF, int interval = 0, bool fill = true);
extern void part_trace(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = 0xFFFFFF, int interval = 0, bool fill = true);
extern void part_ellipse(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = 0xFFFFFF, int axis = 0, bool fill = false, int type = PART_ELLIPSE);
extern void part_radius(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = 0x00FFFF, bool fill = false);
extern void part_cone(const vec &o, const vec &dir, float radius, float angle = 0.f, float size = 1, float blend = 1, int fade = 1, int color = 0x00FFFF, bool fill = false, int type = PART_CONE);

extern void removetrackedparticles(physent *pl = NULL);
extern int particletext, maxparticledistance;

extern particle *newparticle(const vec &o, const vec &d, int fade, int type, int color = 0xFFFFFF, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void create(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regularcreate(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL, int delay = 0);
extern void splash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1);
extern void regularsplash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1, int delay = 0);
extern void regularshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1);
extern void regularflame(int type, const vec &p, float radius, float height, int color, int density = 3, int fade = 500, float size = 2, float blend = 1, int grav = -1, int collide = 0, float vel = 1);

// decal
enum
{
    DECAL_SMOKE = 0,
    DECAL_SCORCH, DECAL_SCORCH_SHORT,
    DECAL_BLOOD,
    DECAL_BULLET,
    DECAL_ENERGY,
    DECAL_STAIN,
    DECAL_MAX
};

extern void adddecal(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

// worldio
extern void setnames(const char *fname, int type);
extern bool load_world(const char *mname, bool temp = false);
extern void save_world(const char *mname, bool nodata = false, bool forcesave = false);
extern uint getmapcrc();
extern void clearmapcrc();

// physics
extern bool ellipsecollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0, bool playercol = true);
extern bool plcollide(physent *d, const vec &dir = vec(0, 0, 0));
extern bool plcollide(physent *d, const vec &dir, physent *o);
extern float pltracecollide(physent *d, const vec &o, const vec &ray, float maxdist);
extern float tracecollide(physent *d, const vec &o, const vec &ray, float maxdist, int mode = RAY_CLIPMAT|RAY_ALPHAPOLY, bool playercol = true);
extern bool intersect(physent *d, const vec &from, const vec &to, float &dist);
extern bool overlapsbox(const vec &d, float h1, float r1, const vec &v, float h2, float r2);
extern const vector<physent *> &checkdynentcache(int x, int y);
extern void updatedynentcache(physent *d);
extern void cleardynentcache();

// rendermodel
enum { MDL_CULL_VFC = 1<<0, MDL_CULL_DIST = 1<<1, MDL_CULL_OCCLUDED = 1<<2, MDL_CULL_QUERY = 1<<3, MDL_SHADOW = 1<<4, MDL_DYNSHADOW = 1<<5, MDL_LIGHT = 1<<6, MDL_DYNLIGHT = 1<<7, MDL_FULLBRIGHT = 1<<8, MDL_NORENDER = 1<<9, MDL_LIGHT_FAST = 1<<10, MDL_LIGHTFX = 1<<11 };

struct model;
struct modelattach
{
    const char *tag, *name;
    int anim, basetime;
    float transparent, sizescale;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), anim(-1), basetime(0), transparent(-1), sizescale(-1), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name, int anim = -1, int basetime = 0, float transparent = -1, float sizescale = -1) : tag(tag), name(name), anim(anim), basetime(basetime), transparent(transparent), sizescale(sizescale), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), anim(-1), basetime(0), transparent(-1), sizescale(-1), pos(pos), m(NULL) {}
};

extern void startmodelbatches();
extern void endmodelbatches();
extern void rendermodel(entitylight *light, const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED | MDL_LIGHT, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float trans = 1, float size = 1);
extern void abovemodel(vec &o, const char *mdl);
extern void rendershadow(dynent *d);
extern void setbbfrommodel(dynent *d, const char *mdl, float size = 1);

// ragdoll

extern bool validragdoll(dynent *d, int millis);
extern void moveragdoll(dynent *d, bool smooth);
extern void cleanragdoll(dynent *d);
extern void warpragdoll(dynent *d, const vec &vel, const vec &offset = vec(0, 0, 0));
extern void twitchragdoll(dynent *d, float vel);

// server
#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                  // max amount of data to swallow in 1 go

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_KICK, DISC_MSGERR, DISC_IPBAN, DISC_PRIVATE, DISC_PASSWORD, DISC_PURE, DISC_MAXCLIENTS, DISC_INCOMPATIBLE, DISC_TIMEOUT, DISC_OVERFLOW, DISC_SHUTDOWN, DISC_HOSTFAIL, DISC_NUM };

extern void *getinfo(int i);
extern const char *gethostname(int i);
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

extern bool findoctadir(const char *name, bool fallback = false);
extern void trytofindocta(bool fallback = true);
extern void setlocations(bool wanthome = true);

// client
struct serverinfo
{
    enum
    {
        MAXPINGS = 3,

        WAITING = INT_MAX
    };
    enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

    string name, map, sdesc, authhandle, flags;
    int numplayers, lastping, lastinfo, nextping, ping, resolved, port, priority;
    int pings[MAXPINGS];
    vector<int> attr;
    vector<char *> players, handles;
    ENetAddress address;

    serverinfo(uint ip, int port, int priority = 0)
     : numplayers(0), resolved(ip==ENET_HOST_ANY ? UNRESOLVED : RESOLVED), port(port), priority(priority)
    {
        name[0] = map[0] = sdesc[0] = authhandle[0] = flags[0] = 0;
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
        if(lastping < 0) lastping = totalmillis;
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
extern void disconnect(int onlyclean = 0, int async = 0);
extern bool multiplayer(bool msg = true);
extern void neterr(const char *s);
extern void gets2c();

// crypto
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern void genpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);

// gui
enum { GUI_DOWN = 1<<0, GUI_UP = 1<<1, GUI_ALT = 1<<2, GUI_PRESSED = 1<<3, GUI_ROLLOVER = 1<<4, GUI_DRAGGED = 1<<5 };
enum { EDITORFOCUSED = 1, EDITORUSED, EDITORFOREVER, EDITORREADONLY };

struct Texture;
struct VSlot;
struct guient
{
    virtual ~guient() {}

    virtual void start(int starttime, int *tab = NULL, bool allowinput = true, bool wantstitle = true, bool wantsbgfx = true) = 0;
    virtual void end() = 0;

    virtual int text(const char *text, int color = 0xFFFFFF, const char *icon = NULL, int icolor = 0xFFFFFF, int wrap = -1) = 0;
    int textf(const char *fmt, int color = 0xFFFFFF, const char *icon = NULL, int icolor = 0xFFFFFF, int wrap = -1, ...) PRINTFARGS(2, 7)
    {
        defvformatstring(str, wrap, fmt);
        return text(str, color, icon, icolor, wrap);
    }
    virtual int button(const char *text, int color = 0xFFFFFF, const char *icon = NULL, int icolor = 0xFFFFFF, int wrap = -1, bool faded = true) = 0;
    int buttonf(const char *fmt, int color = 0xFFFFFF, const char *icon = NULL, int icolor = 0xFFFFFF, int wrap = -1, bool faded = true, ...) PRINTFARGS(2, 8)
    {
        defvformatstring(str, faded, fmt);
        return button(str, color, icon, icolor, wrap, faded);
    }
    virtual void fill(int color, int parentw = 0, int parenth = 0) = 0;
    virtual void outline(int color, int parentw = 0, int parenth = 0, int offsetx = 0, int offsety = 0) = 0;
    virtual void background(int colour1 = -1, float blend1 = -1, int colour2 = -1, float blend2 = -1, bool skinborder = false, int parentw = 0, int parenth = 0) = 0;

    virtual void pushlist(bool merge = false) {}
    virtual int poplist() { return 0; }

    virtual void allowhitfx(bool on) = 0;
    virtual bool visibletab() = 0;
    virtual bool visible() = 0;
    virtual bool shouldtab() { return false; }
    virtual void tab(const char *name = NULL, int color = 0xFFFFFF, bool front = false) = 0;
    virtual void setstatus(const char *fmt, int width, ...) = 0;
    virtual void settooltip(const char *fmt, int width, ...) = 0;
    virtual int image(Texture *t, float scale, bool overlaid = false, int icolor = 0xFFFFFF) = 0;
    virtual int texture(VSlot &vslot, float scale, bool overlaid = true) = 0;
    virtual int slice(Texture *t, float scale, float start = 0, float end = 1, const char *text = NULL) = 0;
    virtual int slider(int &val, int vmin, int vmax, int colour, const char *label = NULL, bool reverse = false, bool scroll = false, int style = 0, int scolour = -1) = 0;
    virtual void separator(int size = 0, int space = 0, int colour = -1, int border = -1) = 0;
    virtual void progress(float percent, float scale) = 0;
    virtual void pushfont(const char *font) = 0;
    virtual void popfont() = 0;
    virtual void strut(float size = 1) = 0;
    virtual void space(float size = 1) = 0;
    virtual void spring(int weight = 1) = 0;
    virtual char *field(const char *name, int color, int length, int height = 0, const char *initval = NULL, int initmode = EDITORFOCUSED, bool focus = false, const char *parent = NULL, const char *prompt = NULL, bool immediate = false) = 0;
    virtual char *keyfield(const char *name, int color, int length, int height = 0, const char *initval = NULL, int initmode = EDITORFOCUSED, bool focus = false, const char *parent = NULL, const char *prompt = NULL, bool immediate = false) = 0;
    virtual int playerpreview(int model, int color, int team, int weap, const char *vanity, float sizescale, bool overlaid = false, float scale = 1, float blend = 1) { return 0; }
    virtual int modelpreview(const char *name, int anim, float sizescale, bool overlaid = false, float scale = 1, float blend = 1) { return 0; }

};

struct guicb
{
    virtual ~guicb() {}
    static int starttime() { extern int totalmillis; return totalmillis; }
    virtual void gui(guient &g, bool firstpass) = 0;
};

extern char *guiskintex, *guiskinbordertex, *guioverlaytex, *guiexittex, *guihovertex;

struct editor;

namespace UI
{
    extern bool isopen;
    extern bool keypress(int code, bool isdown, int cooked);
    extern void update();
    extern void render();
    extern bool active(bool pass = true);
    extern bool hit(bool on, bool act);
    extern void addcb(guicb *cb);
    extern void limitscale(float scale);
    extern editor *geteditor(const char *name, int mode, const char *init = NULL, const char *parent = NULL);
    extern void editorline(editor *e, const char *str, int limit = -1);
    extern void editorclear(editor *e, const char *init = "");
    extern void editoredit(editor *e);
}

// client
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP, ST_REMOTE };

struct clientdata
{
    int type;
    int num;
    ENetPeer *peer;
    string hostname, hostip;
    void *info;
};

extern void process(ENetPacket *packet, int sender, int chan);
extern void send_welcome(int n);
extern void delclient(int n);
extern int addclient(int type = ST_EMPTY);
extern ENetHost *serverhost;

// world

extern bool collideinside;
extern physent *hitplayer;
extern int hitflags;
extern vec collidewall, hitsurface;

enum { HITFLAG_NONE = 0, HITFLAG_LEGS = 1<<0, HITFLAG_TORSO = 1<<1, HITFLAG_HEAD = 1<<2, HITFLAG_FULL = 1<<4 };

enum
{
    MAP_MAPZ = 0,
    MAP_OCTA,
    MAP_MAX
};

enum
{
    MATF_INDEX_SHIFT  = 0,
    MATF_VOLUME_SHIFT = 2,
    MATF_CLIP_SHIFT   = 5,
    MATF_FLAG_SHIFT   = 8,

    MATF_INDEX  = 3 << MATF_INDEX_SHIFT,
    MATF_VOLUME = 7 << MATF_VOLUME_SHIFT,
    MATF_CLIP   = 7 << MATF_CLIP_SHIFT,
    MATF_FLAGS  = 0xFF << MATF_FLAG_SHIFT
};

enum // cube empty-space materials
{
    MAT_AIR   = 0,                      // the default, fill the empty space with air
    MAT_WATER = 1 << MATF_VOLUME_SHIFT, // fill with water, showing waves at the surface
    MAT_LAVA  = 2 << MATF_VOLUME_SHIFT, // fill with lava
    MAT_GLASS = 3 << MATF_VOLUME_SHIFT, // behaves like clip but is blended blueish

    MAT_NOCLIP = 1 << MATF_CLIP_SHIFT,  // collisions always treat cube as empty
    MAT_CLIP   = 2 << MATF_CLIP_SHIFT,  // collisions always treat cube as solid
    MAT_AICLIP = 3 << MATF_CLIP_SHIFT,  // clip monsters only

    MAT_DEATH  = 1 << MATF_FLAG_SHIFT,  // force player suicide
    MAT_LADDER = 2 << MATF_FLAG_SHIFT,  // acts as ladder (move up/down)
    MAT_ALPHA  = 4 << MATF_FLAG_SHIFT   // alpha blended
};
