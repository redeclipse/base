// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// ET_*: the only static entity types dictated by the engine... rest are gamecode dependent

enum { ET_EMPTY = 0, ET_LIGHT, ET_MAPMODEL, ET_PLAYERSTART, ET_ENVMAP, ET_PARTICLES, ET_SOUND, ET_LIGHTFX, ET_DECAL, ET_WIND, ET_MAPUI, ET_SOUNDENV, ET_PHYSICS, ET_WORLDCOL, ET_GEOTEMPLATE, ET_GEOINSTANCE, ET_GAMESPECIFIC };
enum { LFX_S_NONE = 0, LFX_S_RAND1 = 1<<0, LFX_S_RAND2 = 1<<1, LFX_S_MAX = 2 };

#define MPV_ENUM(en, um) \
    en(um, Any, ANY) en(um, Default, DEFAULT) en(um, Alternate, ALTERNATE) \
    en(um, Max, MAX)
ENUM_DLN(MPV);
ENUM_VAR(MPV_FIRST, MPV_DEFAULT);
ENUM_VAR(MPV_LAST, MPV_ALTERNATE);
ENUM_VAR(MPV_COUNT, MPV_MAX - 1);

#define LFX_ENUM(en, um) \
    en(um, Spotlight, SPOTLIGHT) en(um, Flicker, FLICKER) \
    en(um, Pulse, PULSE) en(um, Glow, GLOW) \
    en(um, Inverted Pulse, INVPULSE) en(um, Inverted Glow, INVGLOW) \
    en(um, Max, MAX)
ENUM_DLN(LFX);

#define PHYSICS_ENUM(en, um) \
    en(um, Movement, MOVEMENT) en(um, Gravity, GRAVITY) en(um, Coasting, COASTING) \
    en(um, Max, MAX)
ENUM_DLN(PHYSICS);

#define WORLDCOL_ENUM(en, um) \
    en(um, Average, AVERAGE) en(um, Minimum, MINIMUM) en(um, Maximum, MAXIMUM)  \
    en(um, Add, ADD) en(um, Subtract, SUBTRACT) en(um, Multiply, MULTIPLY) \
    en(um, Screen, SCREEN) en(um, Overlay, OVERLAY) en(um, Copy, COPY) \
    en(um, Max, MAX)
ENUM_DLN(WORLDCOL);

#define WORLDCOL_WORLD(en, um) \
    en(um, Ambient, AMBIENT) en(um, Sky Light, SKYLIGHT) en(um, Sunlight, SUNLIGHT) \
    en(um, Fog, FOG) en(um, Sky Box, SKYBOX) en(um, Cloud Box, CLOUDBOX) \
    en(um, Sky Background, SKYBG) en(um, Cloud Layer, CLOUDLAYER) en(um, Env Layer, ENVLAYER) \
    en(um, Cloud Cylinder, CLOUDCYL) en(um, Env Cylinder, ENVCYL) \
    en(um, Atmo Light, ATMOLIGHT) en(um, Atmo Disk, ATMODISK) \
    en(um, Fog Dome, FOGDOME) en(um, Haze, HAZE)
#define WORLDCOL_PALETTES(en, um) \
    en(um, Palette 1, PALETTE1) en(um, Palette 2, PALETTE2) \
    en(um, Palette 3, PALETTE3) en(um, Palette 4, PALETTE4) \
    en(um, Palette 5, PALETTE5) en(um, Palette 6, PALETTE6) \
    en(um, Palette 7, PALETTE7) en(um, Palette 8, PALETTE8) \
    en(um, Palette 9, PALETTE9) en(um, Palette 10, PALETTE10) \
    en(um, Palette 11, PALETTE11) en(um, Palette 12, PALETTE12) \
    en(um, Palette 13, PALETTE13) en(um, Palette 14, PALETTE14) \
    en(um, Palette 15, PALETTE15) en(um, Palette 16, PALETTE16)
// each material colour, in the order of the materials it is for
#define WORLDCOL_MATCOLS(en, um) \
    en(um, Water Colour, WATER) \
    en(um, Water Deep, WATERDEEP) \
    en(um, Water Deep Fade, WATERDEEPFADE) \
    en(um, Water Reflect, WATERREFLECT) \
    en(um, Water Refract, WATERREFRACT) \
    en(um, Water Edge, WATEREDGE) \
    en(um, Water Fall, WATERFALL) \
    en(um, Water Fall Refract, WATERFALLREFRACT) \
    en(um, Lava, LAVA) \
    en(um, Glass, GLASS) \
    en(um, Volfog, VOLFOG) \
    en(um, Volfog Deep, VOLFOGDEEP) \
    en(um, Volfog Deep Fade, VOLFOGDEEPFADE) \
    en(um, Volfog Texture, VOLFOGTEX)
// each material colour for each of the four materials of its kind
#define WORLDCOL_MATSLOTS(en, um) \
    en(um, Water 1 Colour, WATER1) en(um, Water 2 Colour, WATER2) \
    en(um, Water 3 Colour, WATER3) en(um, Water 4 Colour, WATER4) \
    en(um, Water 1 Deep, WATERDEEP1) en(um, Water 2 Deep, WATERDEEP2) \
    en(um, Water 3 Deep, WATERDEEP3) en(um, Water 4 Deep, WATERDEEP4) \
    en(um, Water 1 Deep Fade, WATERDEEPFADE1) en(um, Water 2 Deep Fade, WATERDEEPFADE2) \
    en(um, Water 3 Deep Fade, WATERDEEPFADE3) en(um, Water 4 Deep Fade, WATERDEEPFADE4) \
    en(um, Water 1 Reflect, WATERREFLECT1) en(um, Water 2 Reflect, WATERREFLECT2) \
    en(um, Water 3 Reflect, WATERREFLECT3) en(um, Water 4 Reflect, WATERREFLECT4) \
    en(um, Water 1 Refract, WATERREFRACT1) en(um, Water 2 Refract, WATERREFRACT2) \
    en(um, Water 3 Refract, WATERREFRACT3) en(um, Water 4 Refract, WATERREFRACT4) \
    en(um, Water 1 Edge, WATEREDGE1) en(um, Water 2 Edge, WATEREDGE2) \
    en(um, Water 3 Edge, WATEREDGE3) en(um, Water 4 Edge, WATEREDGE4) \
    en(um, Water 1 Fall, WATERFALL1) en(um, Water 2 Fall, WATERFALL2) \
    en(um, Water 3 Fall, WATERFALL3) en(um, Water 4 Fall, WATERFALL4) \
    en(um, Water 1 Fall Refract, WATERFALLREFRACT1) en(um, Water 2 Fall Refract, WATERFALLREFRACT2) \
    en(um, Water 3 Fall Refract, WATERFALLREFRACT3) en(um, Water 4 Fall Refract, WATERFALLREFRACT4) \
    en(um, Lava 1, LAVA1) en(um, Lava 2, LAVA2) \
    en(um, Lava 3, LAVA3) en(um, Lava 4, LAVA4) \
    en(um, Glass 1, GLASS1) en(um, Glass 2, GLASS2) \
    en(um, Glass 3, GLASS3) en(um, Glass 4, GLASS4) \
    en(um, Volfog 1, VOLFOG1) en(um, Volfog 2, VOLFOG2) \
    en(um, Volfog 3, VOLFOG3) en(um, Volfog 4, VOLFOG4) \
    en(um, Volfog 1 Deep, VOLFOGDEEP1) en(um, Volfog 2 Deep, VOLFOGDEEP2) \
    en(um, Volfog 3 Deep, VOLFOGDEEP3) en(um, Volfog 4 Deep, VOLFOGDEEP4) \
    en(um, Volfog 1 Deep Fade, VOLFOGDEEPFADE1) en(um, Volfog 2 Deep Fade, VOLFOGDEEPFADE2) \
    en(um, Volfog 3 Deep Fade, VOLFOGDEEPFADE3) en(um, Volfog 4 Deep Fade, VOLFOGDEEPFADE4) \
    en(um, Volfog 1 Texture, VOLFOGTEX1) en(um, Volfog 2 Texture, VOLFOGTEX2) \
    en(um, Volfog 3 Texture, VOLFOGTEX3) en(um, Volfog 4 Texture, VOLFOGTEX4)

// what a worldcol entity's flags pick: world colours by name, the palettes its select attr names, and material colours for the materials it names
#define WORLDCOL_F_ENUM(en, um) \
    WORLDCOL_WORLD(en, um) \
    en(um, Palette, PALETTE) \
    WORLDCOL_MATCOLS(en, um) \
    en(um, Max, MAX)
ENUM_DLN(WORLDCOL_F);
ENUM_VAR(WORLDCOL_F_MATERIAL, WORLDCOL_F_WATER);
ENUM_VAR(WORLDCOL_F_MATERIALS, WORLDCOL_F_MAX - WORLDCOL_F_WATER);
ENUM_VAR(WORLDCOL_F_ALL, (1<<WORLDCOL_F_MAX)-1);

// which palettes and materials a worldcol entity's flags apply to, as bits of its select attr
#define WORLDCOL_SEL_ENUM(en, um) \
    WORLDCOL_PALETTES(en, um) \
    en(um, Material 1, MATERIAL1) en(um, Material 2, MATERIAL2) en(um, Material 3, MATERIAL3) en(um, Material 4, MATERIAL4) \
    en(um, Max, MAX)
ENUM_DLN(WORLDCOL_SEL);
ENUM_VAR(WORLDCOL_SEL_ALL, (1<<WORLDCOL_SEL_MAX)-1);

// every world colour, as palette 3 indexes them
#define WORLDCOL_S_ENUM(en, um) \
    WORLDCOL_WORLD(en, um) \
    WORLDCOL_PALETTES(en, um) \
    WORLDCOL_MATSLOTS(en, um) \
    en(um, Max, MAX)
ENUM_DLN(WORLDCOL_S);
ENUM_VAR(WORLDCOL_S_PALETTE, WORLDCOL_S_PALETTE1);
ENUM_VAR(WORLDCOL_S_PALETTES, WORLDCOL_S_PALETTE16 - WORLDCOL_S_PALETTE1 + 1);
ENUM_VAR(WORLDCOL_S_MATERIAL, WORLDCOL_S_WATER1);
ENUM_VAR(WORLDCOL_MATINDEXES, 4); // each material colour has one for each of the four materials of its kind

extern bvec getworldcol(int slot);
struct entbase          // persistent map entity
{
    vec o;              // position
    uchar type;         // type is one of the above
    uchar reserved[3];  // left-over space due to struct alignment
};

enum
{
    EF_HIDE       = 1<<0,
    EF_NOSHADOW   = 1<<1,
    EF_NOCOLLIDE  = 1<<2,
    EF_NOVIS      = 1<<3,
    EF_SHADOWMESH = 1<<4,
    EF_OCTA       = 1<<5,
    EF_RENDER     = 1<<6,
    EF_SPAWNED    = 1<<7,
    EF_DYNAMIC    = 1<<8,
    EF_BBZONE     = 1<<9,
    EF_NOTRIGCOL  = 1<<10,
    EF_VIRTUAL    = 1<<11,
    EF_FULLBRIGHT = 1<<12,
    EF_NOLOD      = 1<<13,
    EF_ONLYSHADOW = 1<<14,
    EF_STATICSHADOW = 1<<15
};

typedef smallvector<int> attrvector;
typedef smallvector<int> linkvector;

struct entity : entbase
{
    attrvector attrs;
    linkvector links;
};

enum { MAXMDLMATERIALS = 4 };
enum
{
    MDL_CULL_VFC            = 1<<0,
    MDL_CULL_DIST           = 1<<1,
    MDL_CULL_OCCLUDED       = 1<<2,
    MDL_CULL_QUERY          = 1<<3,
    MDL_FULLBRIGHT          = 1<<4,
    MDL_NORENDER            = 1<<5,
    MDL_MAPMODEL            = 1<<6,
    MDL_NOBATCH             = 1<<7,
    MDL_ONLYSHADOW          = 1<<8,
    MDL_NOSHADOW            = 1<<9,
    MDL_FORCESHADOW         = 1<<10,
    MDL_FORCETRANSPARENT    = 1<<11,
    MDL_NOEFFECT            = 1<<12,
    MDL_NOMIXER             = 1<<12,
    MDL_FORCEDYNAMIC        = 1<<13,
    MDL_AURA_TOP            = 1<<14,
    MDL_NOLOD               = 1<<15,
    MDL_AVATAR              = 1<<17,
    MDL_STATICSHADOW        = 1<<18,
    MDL_FORCEFULLBRIGHT     = 1<<19,
};

#define MAPMODELLODSCALEMAX 10000

struct model;
struct modelattach
{
    const char *tag, *name;
    int anim, basetime;
    float sizescale, speed;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), anim(-1), basetime(0), sizescale(-1), speed(1), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name, int anim = -1, int basetime = 0, float sizescale = 1, float speed = 1) : tag(tag), name(name), anim(anim), basetime(basetime), sizescale(sizescale), speed(speed), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), anim(-1), basetime(0), sizescale(1), speed(1), pos(pos), m(NULL) {}
};

struct Texture;

struct entmodelstate
{
    int anim, flags, basetime, basetime2, lastspin;
    float yaw, pitch, roll, size, radius, speed, speed2, lodscale, windscale;
    int blendtime; // blend into a new animation over this long, 0 for animationinterpolationtime
    vec o, center;
    vec4 color;
    bvec material[MAXMDLMATERIALS];

    entmodelstate() { reset(); }

    void reset()
    {
        size = speed = speed2 = lodscale = 1;
        anim = flags = basetime = basetime2 = lastspin = blendtime = 0;
        yaw = pitch = roll = radius = 0;
        windscale = 1;
        o = center = vec(0, 0, 0);
        color = vec4(1, 1, 1, 1);
        loopi(MAXMDLMATERIALS) material[i] = bvec(255, 255, 255);
    }
};

#define MDLFX_ENUM(en, um) \
    en(um, Shimmer, SHIMMER) en(um, Max, MAX)
ENUM_DLN(MDLFX);

struct modelstate : entmodelstate
{
    int effecttype;
    vec4 effectcolor, effectparams, matbright, aura, rimlight;
    Texture *mixer;
    modelattach *attached;

    modelstate() { reset(); }

    void reset()
    {
        entmodelstate::reset();

        effecttype = -1;
        effectcolor = matbright = vec4(1, 1, 1, 1);
        effectparams = aura = rimlight = vec4(0, 0, 0, 0);
        mixer = NULL;
        attached = NULL;
    }
};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    int flags;        // the only dynamic state of a map entity
    int lastemit, emit[3];

    extentity() : flags(0), lastemit(0) { emit[0] = emit[1] = emit[2] = 0; }

    bool spawned() const { return (flags&EF_SPAWNED) != 0 && (flags&EF_VIRTUAL) == 0; }
    void setspawned(bool val) { if(val) flags |= EF_SPAWNED; else flags &= ~EF_SPAWNED; }
    void setspawned() { flags |= EF_SPAWNED; }
    void clearspawned() { flags &= ~EF_SPAWNED; }
    bool dynamic() const { return (flags&EF_DYNAMIC) != 0; }
    bool bbzone() const { return (flags&EF_BBZONE) != 0; }
};

#define MAXENTS 30000
#define MAXENTATTRS 100
#define MAXENTKIN 100

extern int entindex, entorient;
extern vector<int> enthover;
#define entfocusv(i, f, v) { int n = entindex = (i); if((v).inrange(n)) { extentity &e = *(v)[n]; f; } }
#define entfocus(i, f) entfocusv(i, f, entities::getents())


#define CS_ENUM(en, um) \
    en(um, Alive, ALIVE) en(um, Dead, DEAD) \
    en(um, Editing, EDITING) en(um, Spectator, SPECTATOR) \
    en(um, Waiting, WAITING) \
    en(um, Maximum, MAX)
ENUM_DLN(CS); // beware, some stuff uses >= CS_SPECTATOR

#define PHYS_ENUM(en, um) \
    en(um, Float, FLOAT) en(um, Fall, FALL) en(um, Slide, SLIDE) \
    en(um, Slope, SLOPE) en(um, Floor, FLOOR) en(um, Step Up, STEP_UP) en(um, Step Down, STEP_DOWN) \
    en(um, Maximum, MAX)
ENUM_DLN(PHYS);

#define ENT_ENUM(en, um) \
    en(um, Player, PLAYER) en(um, AI, AI) en(um, Inanimate, INANIMATE) \
    en(um, Camera, CAMERA) en(um, Projectile, PROJ) en(um, Ragdoll, RAGDOLL) \
    en(um, Dummy, DUMMY) en(um, Maximum, MAX)
ENUM_DLN(ENT);

#define COLLIDE_ENUM(en, um) \
    en(um, None, NONE) en(um, Ellipse, ELLIPSE) en(um, OBB, OBB) en(um, Triangle, TRI) \
    en(um, Maximum, MAX)
ENUM_DLN(COLLIDE);

struct baseent
{
    vec o, vel, falling, floorpos;              // origin and velocity
    float yaw, pitch, roll;
    uchar state;                                // one of CS_* above
    int inmaterial;
    float submerged;

    baseent() : o(0, 0, 0), yaw(0), pitch(0), roll(0), state(CS_SPECTATOR)
    {
        reset();
    }

    void reset()
    {
        o = vel = falling = vec(0, 0, 0);
        floorpos = vec(-1, -1, -1);
        yaw = pitch = roll = 0;
        inmaterial = 0;
        submerged = 0;
    }

    bool isobserver() const { return state == CS_SPECTATOR || state == CS_EDITING || state == CS_WAITING; }
    bool iswatching() const { return state == CS_SPECTATOR || state == CS_WAITING; }
    bool isspectator() const { return state == CS_SPECTATOR; }
    bool isactive() const { return state == CS_ALIVE || state == CS_DEAD || state == CS_WAITING; }
    bool isalive() const { return state == CS_ALIVE; }
    bool isnotalive() const { return state == CS_DEAD || state == CS_WAITING; }
    bool isdead() const { return state == CS_DEAD; }
    bool isediting() const { return state == CS_EDITING; }
    bool isnophys() const { return state == CS_EDITING || state == CS_SPECTATOR; }
};

struct physent : baseent                        // can be affected by physics
{
    vec deltapos, newpos;
    float speed, impulsespeed, weight, buoyancy;
    int airmillis, floormillis;
    float radius, height, aboveeye;             // bounding box size
    float xradius, yradius, zradius, zmargin;
    vec floor;                                  // the normal of floor the dynent is on

    bool blocked, forcepos;
    float curscale, movescale, gravityscale, coastscale;
    schar move, strafe;

    uchar physstate;                            // one of PHYS_* above
    uchar type;                                 // one of ENT_* above
    uchar collidetype;                          // one of COLLIDE_* above

    physent() : deltapos(0, 0, 0), newpos(0, 0, 0),
        speed(100), impulsespeed(100), weight(100), buoyancy(100),
        radius(3.75f), height(17.5f), aboveeye(1.25f),
        xradius(3.75f), yradius(3.75f), zradius(17.5f), zmargin(0),
        curscale(1), movescale(1), gravityscale(1), coastscale(1),
        type(ENT_INANIMATE),
        collidetype(COLLIDE_ELLIPSE)
    {
        reset();
    }

    void resetinterp(bool force = false)
    {
        newpos = o;
        newpos.z -= height;
        deltapos = vec(0, 0, 0);
        if(force) forcepos = true; // next message forces new position
    }

    void reset()
    {
        baseent::reset();
        airmillis = floormillis = 0;
        blocked = forcepos = false;
        strafe = move = 0;
        physstate = PHYS_FALL;
        floor = vec(0, 0, 1);
        resetinterp();
    }

    void resetphys(bool fall = true)
    {
        falling = vec(0, 0, 0);
        floor = vec(0, 0, 1);
        if(fall) physstate = PHYS_FALL;
    }

    int airtime(int millis)
    {
        if(airmillis) return millis-airmillis;
        return 0;
    }

    int floortime(int millis)
    {
        if(floormillis) return millis-floormillis;
        return 0;
    }

    vec abovehead(float offset = 1) const { return vec(o).add(vec(0, 0, aboveeye+offset)); }
    vec feetpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset-height)); }
    vec headpos(float offset = 0) const { return vec(o).add(vec(0, 0, offset)); }
    vec center() const { return vec(o).sub(vec(0, 0, height*0.5f)); }
    float getradius() const { return radius; }
    float getheight() const { return height; }
};

enum
{
    ANIM_IDLE = 0, ANIM_WALK_FORWARD, ANIM_WALK_BACKWARD, ANIM_WALK_LEFT, ANIM_WALK_RIGHT, ANIM_DEAD, ANIM_DYING, ANIM_SWIM,
    ANIM_MAPMODEL, ANIM_TRIGGER_ON, ANIM_TRIGGER_OFF,
    ANIM_GAMESPECIFIC
};

#define ANIM_ALL         0x1FF
#define ANIM_INDEX       0x1FF
#define ANIM_LOOP        (1<<9)
#define ANIM_CLAMP       (1<<10)
#define ANIM_REVERSE     (1<<11)
#define ANIM_START       (ANIM_LOOP|ANIM_CLAMP)
#define ANIM_END         (ANIM_LOOP|ANIM_CLAMP|ANIM_REVERSE)
#define ANIM_DIR         0xE00
#define ANIM_SECONDARY   12
#define ANIM_REUSE       0xFFFFFF
#define ANIM_NOSKIN      (1<<24)
#define ANIM_SETTIME     (1<<25)
#define ANIM_FULLBRIGHT  (1<<26)
#define ANIM_NORENDER    (1<<27)
#define ANIM_RAGDOLL     (1<<28)
#define ANIM_NOPITCH     (1<<29)
#define ANIM_FLAGS       0xFF000000

struct animinfo // description of a character's animation
{
    int anim, frame, range, basetime;
    float speed;
    uint varseed;

    animinfo() : anim(0), frame(0), range(0), basetime(0), speed(100.0f), varseed(0) { }

    bool operator==(const animinfo &o) const { return frame==o.frame && range==o.range && (anim&(ANIM_SETTIME|ANIM_DIR))==(o.anim&(ANIM_SETTIME|ANIM_DIR)) && (anim&ANIM_SETTIME || basetime==o.basetime) && speed==o.speed; }
    bool operator!=(const animinfo &o) const { return frame!=o.frame || range!=o.range || (anim&(ANIM_SETTIME|ANIM_DIR))!=(o.anim&(ANIM_SETTIME|ANIM_DIR)) || (!(anim&ANIM_SETTIME) && basetime!=o.basetime) || speed!=o.speed; }
};

struct animinterpinfo // used for animation blending of animated characters
{
    animinfo prev, cur;
    int lastswitch, blendtime, setmillis;
    float prevrate, currate; // how fast a held animation's time was moving, so it keeps moving while blended out
    void *lastmodel;

    animinterpinfo() : lastswitch(-1), blendtime(0), setmillis(-1), prevrate(0), currate(0), lastmodel(NULL) {}

    // the animation being blended out, a held one carried on from where it was left at the rate it was moving, wrapping round
    // its frames as held animations are looped with a closing frame matching the first
    animinfo blendprev(int diff) const
    {
        animinfo info = prev;
        if(info.anim&ANIM_SETTIME && prevrate > 0 && info.range > 1)
        {
            int len = int((info.range-1)*info.speed);
            info.basetime += int(diff*prevrate);
            if(len > 0) info.basetime %= len;
        }
        return info;
    }

    void reset() { lastswitch = -1; }
};

#define MAXANIMPARTS 2

struct occludequery;
struct ragdolldata;
struct dynent;

extern vec rdabove(dynent *d, float offset = 1);
extern vec rdbottom(dynent *d, float offset = 0);
extern vec rdtop(dynent *d, float offset = 0);
extern vec rdcenter(dynent *d);
extern float rdradius(dynent *d);
extern float rdheight(dynent *d);

struct usedent
{
    int ent, millis;
};

#define ACTITEM_ENUM(en, um) en(um, Entity, ENT) en(um, Projectile, PROJ) en(um, Max, MAX)
ENUM_DLN(ACTITEM);

struct actitem
{
    int type, ent, id, millis, enter, leave;
    float score;

    actitem() : type(ACTITEM_ENT), ent(-1), id(-1), millis(0), enter(-1), leave(-1), score(0) {}
    ~actitem() {}

    static bool sortitems(const actitem &a, const actitem &b)
    {
        if(a.millis > b.millis) return true; // most recently poked first
        if(a.millis < b.millis) return false;

        return a.score < b.score; // closest items first
    }
};

struct dynent : physent                         // animated characters, or characters that can receive input
{
    animinterpinfo animinterp[MAXANIMPARTS];
    ragdolldata *ragdoll;
    occludequery *query;
    int lastrendered, lastactitem;
    vector<usedent> used;
    vector<actitem> actitems;

    dynent() : ragdoll(NULL), query(NULL), lastrendered(0), lastactitem(0)
    {
        reset();
    }

    ~dynent()
    {
#ifndef STANDALONE
        extern void cleanragdoll(dynent *d);
        if(ragdoll) cleanragdoll(this);
#endif
    }

    static bool is(int t) { return t == ENT_PLAYER || t == ENT_AI || t == ENT_PROJ || t == ENT_INANIMATE; }
    static bool is(physent *d) { return d && (d->type == ENT_PLAYER || d->type == ENT_AI || d->type == ENT_PROJ || d->type == ENT_INANIMATE); }

    void reset()
    {
        physent::reset();
        loopi(MAXANIMPARTS) animinterp[i].reset();
        used.shrink(0);
        actitems.shrink(0);
        lastactitem = 0;
    }

    int logitem(int type, int ent, float score, int id = -1)
    {
        loopv(actitems)
        {
            if(actitems[i].type != type || actitems[i].ent != ent) continue;
            if(id >= 0 && actitems[i].id >= 0 && actitems[i].id != id) continue;

            actitems[i].score = score;
            actitems[i].millis = lastactitem;
            return i;
        }

        actitem &n = actitems.add();
        n.type = type;
        n.ent = ent;
        n.id = id;
        n.score = score;
        n.millis = lastactitem;

        return actitems.length() - 1;
    }

    bool hasitems()
    {
        loopv(actitems) if(actitems[i].millis == lastactitem) return true;
        return false;
    }

    bool updateitems()
    {
        if(actitems.empty()) return false;

        actitems.sort(actitem::sortitems);

        bool found = false;
        loopv(actitems)
        {
            actitem &n = actitems[i];

            if(n.millis == lastactitem)
            {
                found = true;

                if(n.enter < 0 || n.enter < n.leave) n.enter = lastactitem;
            }
            else
            {
                if(n.type == ACTITEM_PROJ)
                {
                    actitems.remove(i--);
                    continue;
                }

                if(n.leave < 0 || n.leave < n.enter) n.leave = lastactitem;
            }
        }

        return found;
    }

    void normalize_yaw(float angle)
    {
        while(yaw<angle-180.0f) yaw += 360.0f;
        while(yaw>angle+180.0f) yaw -= 360.0f;
    }

    vec abovehead(float offset = 1) { return rdabove(this, offset); }
    vec feetpos(float offset = 0) { return rdbottom(this, offset); }
    vec headpos(float offset = 0) { return rdtop(this, offset); }
    vec center() { return rdcenter(this); }
    float getradius() { return rdradius(this); }
    float getheight() { return rdheight(this); }

    int lastused(int n)
    {
        loopv(used) if(used[i].ent == n) return used[i].millis;
        return 0;
    }

    void setused(int n, int millis)
    {
        loopvrev(used) if(used[i].ent == n) used.remove(i);
        usedent &u = used.add();
        u.ent = n;
        u.millis = millis ? millis : 1;
    }
};

#define loopent(v,m) for(int v = entities::firstent(m); v < entities::lastent(m); ++v)
#define loopenti(m) loopent(i,m)
#define loopentj(m) loopent(j,m)
#define loopentk(m) loopent(k,m)
#define loopuse(v,m) for(int v = entities::firstuse(m); v < entities::lastuse(m); ++v)
#define loopusei(m) loopuse(i,m)
#define loopusej(m) loopuse(j,m)
#define loopusek(m) loopuse(k,m)
