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

extern void processtextinput(const char *str, int len);
extern void processkey(int code, bool isdown);
extern char *getcurcommand();
extern void resetcomplete();
extern void complete(char *s, const char *cmdprefix);
extern const char *searchbind(const char *action, int type);
extern void searchbindlist(const char *action, int type, int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names, bool force = true);

extern bool capslockon, numlockon;
extern bool capslocked();
extern bool numlocked();

struct bindlist
{
    vector<char> names;
    int lastsearch;

    bindlist() : lastsearch(-1) {}

    const char *search(const char *action, int type = 0, const char *s1 = "\f{", const char *s2 = "}", const char *sep1 = " ", const char *sep2 = " ", int limit = 5)
    {
        if(names.empty() || lastsearch != changedkeys)
        {
            names.shrink(0);
            searchbindlist(action, type, limit, s1, s2, sep1, sep2, names);
            lastsearch = changedkeys;
        }
        return names.getbuf();
    }
};

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
extern const matrix4x3 *textmatrix;

extern void reloadfonts();

extern char *savecolour, *restorecolour, *green, *blue, *yellow, *red, *gray, *magenta, *orange, *white, *black, *cyan;
extern int textskinsize, textskinpad;

enum
{
    TEXT_SHADOW         = 1<<0,
    TEXT_NO_INDENT      = 1<<1,
    TEXT_UPWARD         = 1<<2,
    TEXT_BALLOON        = 1<<3,
    TEXT_SKIN           = 1<<4,

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

extern int colourblack, colourwhite,
    colourgreen, colourblue, colouryellow, colourred, colourgrey, colourmagenta, colourorange, colourcyan, colourpink, colourviolet, colourpurple, colourbrown,
    colourdarkgreen, colourdarkblue, colourdarkyellow, colourdarkred, colourdarkgrey, colourdarkmagenta, colourdarkorange, colourdarkcyan, colourdarkpink, colourdarkviolet, colourdarkpurple, colourdarkbrown;

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

extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

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
    PT_SHADER   = 1<<20,
    PT_FLIP     = PT_HFLIP | PT_VFLIP | PT_ROT
};

enum
{
    PART_TELEPORT = 0, PART_ICON,
    PART_LINE, PART_LINE_ONTOP, PART_TRIANGLE, PART_TRIANGLE_ONTOP,
    PART_ELLIPSE, PART_ELLIPSE_ONTOP, PART_CONE, PART_CONE_ONTOP,
    PART_FIREBALL_LERP, PART_PLASMA_LERP, PART_FLARE_LERP, PART_MUZZLE_FLARE_LERP,
    PART_SMOKE_LERP_SOFT, PART_SMOKE_LERP, PART_HINT_LERP_SOFT, PART_HINT_LERP, PART_HINT_BOLD_LERP_SOFT, PART_HINT_BOLD_LERP,
    PART_HINT_VERT_LERP_SOFT, PART_VERT_BOLD_LERP, PART_HINT_HORZ_LERP_SOFT, PART_HORZ_BOLD_LERP,
    PART_SMOKE_SOFT, PART_SMOKE, PART_HINT_SOFT, PART_HINT, PART_HINT_BOLD_SOFT, PART_HINT_BOLD,
    PART_HINT_VERT_SOFT, PART_VERT_BOLD, PART_HINT_HORZ_SOFT, PART_HORZ_BOLD,
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

extern void regular_part_create(int type, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL, int delay = 0);
extern void part_create(int type, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regular_part_splash(int type, int num, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0, float radius = 150, float vel = 1, int delay = 0);
extern void part_splash(int type, int num, int fade, const vec &p, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0, float radius = 4, float vel = 1);
extern void part_trail(int ptype, int fade, const vec &s, const vec &e, int color = colourwhite, float size = .8f, float blend = 1, int grav = 0, int collide = 0);
extern void part_text(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = colourwhite, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_textcopy(const vec &s, const char *t, int type = PART_TEXT, int fade = 1, int color = colourwhite, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_flare(const vec &p, const vec &dest, int fade, int type, int color = colourwhite, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regular_part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_explosion(const vec &dest, float maxsize, int type, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_spawn(const vec &o, const vec &v, float z, uchar type, int amt = 1, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0);
extern void part_flares(const vec &o, const vec &v, float z1, const vec &d, const vec &w, float z2, uchar type, int amt = 1, int fade = 1, int color = colourwhite, float size = 4, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void part_portal(const vec &o, float size, float blend = 1, float yaw = 0, float pitch = 0, int type = PART_TELEPORT, int fade = 1, int color = colourwhite);
extern void part_icon(const vec &o, Texture *tex, float size = 2, float blend = 1, int grav = 0, int collide = 0, int fade = 1, int color = colourwhite, float start = 0, float length = 1, physent *pl = NULL);
extern void part_line(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int type = PART_LINE);
extern void part_triangle(const vec &o, float yaw, float pitch, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, bool fill = true, int type = PART_TRIANGLE);
extern void part_dir(const vec &o, float yaw, float pitch, float length = 1, float size = 1, float blend = 1, int fade = 1, int color = colourpink, int interval = 0, bool fill = true);
extern void part_trace(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int interval = 0, bool fill = true);
extern void part_ellipse(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourwhite, int axis = 0, bool fill = false, int type = PART_ELLIPSE);
extern void part_radius(const vec &o, const vec &v, float size = 1, float blend = 1, int fade = 1, int color = colourcyan, bool fill = false);
extern void part_cone(const vec &o, const vec &dir, float radius, float angle = 0.f, float size = 1, float blend = 1, int fade = 1, int color = colourcyan, bool fill = false, int type = PART_CONE);

extern void removetrackedparticles(physent *pl = NULL);
extern int particletext, maxparticledistance;

extern particle *newparticle(const vec &o, const vec &d, int fade, int type, int color = colourwhite, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void create(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL);
extern void regularcreate(int type, int color, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL, int delay = 0);
extern void splash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1);
extern void regularsplash(int type, int color, float radius, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1, int delay = 0);
extern void createshape(int type, float radius, int color, int dir, int num, int fade, const vec &p, float size = 2, float blend = 1, int grav = 0, int collide = 0, float vel = 1);
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
extern void setnames(const char *fname, int type, int crc = 0);
extern bool load_world(const char *mname, int crc = 0);
extern void save_world(const char *mname, bool nodata = false, bool forcesave = false);
extern char *mapctitle(const char *s);
extern char *mapcauthor(const char *s);
extern char *mapcdesc(const char *s);

// physics
extern bool ellipsecollide(physent *d, const vec &dir, const vec &o, const vec &center, float yaw, float xr, float yr, float hi, float lo);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0, bool playercol = true, bool insideplayercol = false);
extern bool plcollide(physent *d, const vec &dir = vec(0, 0, 0), bool insideplayercol = false);
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

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_KICK, DISC_MSGERR, DISC_IPBAN, DISC_PRIVATE, DISC_PASSWORD, DISC_PURE, DISC_MAXCLIENTS, DISC_INCOMPATIBLE, DISC_TIMEOUT, DISC_OVERFLOW, DISC_SHUTDOWN, DISC_HOSTFAIL, DISC_AUTH, DISC_NUM };

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
extern void genpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);

// ui
enum { EDITORFOCUSED = 1, EDITORUSED, EDITORFOREVER, EDITORREADONLY };
struct editor;

namespace UI
{
    extern char *uiopencmd, *uiclosecmd;
    extern bool showui(const char *name);
    extern bool hideui(const char *name);
    extern bool toggleui(const char *name);

    extern int openui(const char *name);
    extern int closeui(const char *name);

    extern void holdui(const char *name, bool on);
    extern void pressui(const char *name, bool on);
    extern bool uivisible(const char *name);
    extern bool hasinput();
    extern bool hasmenu(bool pass = true);
    extern bool keypress(int code, bool isdown);
    extern bool textinput(const char *str, int len);
    extern float abovehud();

    extern void setup();
    extern void update();
    extern void render();
    extern void cleanup();

    extern editor *geteditor(const char *name, int mode, const char *init = NULL, const char *parent = NULL);
    extern void editorline(editor *e, const char *str, int limit = -1);
    extern void editorclear(editor *e, const char *init = "");
    extern void editoredit(editor *e, const char *init = "");
}

// menus
extern void addchange(const char *desc, int type);
extern void clearchanges(int type);
extern void menuprocess();

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

extern int collideinside;
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
    MAT_AICLIP = 3 << MATF_CLIP_SHIFT,  // clip waypoints etc

    MAT_DEATH  = 1 << MATF_FLAG_SHIFT,  // force player suicide
    MAT_LADDER = 2 << MATF_FLAG_SHIFT,  // acts as ladder (move up/down)
    MAT_ALPHA  = 4 << MATF_FLAG_SHIFT,  // alpha blended
    MAT_HURT   = 8 << MATF_FLAG_SHIFT   // hurt at intervals
};
