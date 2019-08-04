// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// ET_*: the only static entity types dictated by the engine... rest are gamecode dependent

enum { ET_EMPTY=0, ET_LIGHT, ET_MAPMODEL, ET_PLAYERSTART, ET_ENVMAP, ET_PARTICLES, ET_SOUND, ET_LIGHTFX, ET_DECAL, ET_WIND, ET_GAMESPECIFIC };
enum { LFX_SPOTLIGHT = 0, LFX_FLICKER, LFX_PULSE, LFX_GLOW, LFX_INVPULSE, LFX_INVGLOW, LFX_MAX };
enum { LFX_S_NONE = 0, LFX_S_RAND1 = 1<<0, LFX_S_RAND2 = 1<<1, LFX_S_MAX = 2 };
enum { MPV_ALL = 0, MPV_DEF, MPV_ALT, MPV_MAX };

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
    EF_SPAWNED    = 1<<7

};

typedef smallvector<int> attrvector;
typedef smallvector<int> linkvector;

struct entity : entbase
{
    attrvector attrs;
    linkvector links;
};

enum { MAXMDLMATERIALS = 3 };
enum { MDL_CULL_VFC = 1<<0, MDL_CULL_DIST = 1<<1, MDL_CULL_OCCLUDED = 1<<2, MDL_CULL_QUERY = 1<<3, MDL_FULLBRIGHT = 1<<4, MDL_NORENDER = 1<<5, MDL_MAPMODEL = 1<<6, MDL_NOBATCH = 1<<7, MDL_ONLYSHADOW = 1<<8, MDL_NOSHADOW = 1<<9, MDL_FORCESHADOW = 1<<10, MDL_FORCETRANSPARENT = 1<<11, MDL_NOMIXER = 1<<12, MDL_NOPATTERN = 1<<13 };

struct model;
struct modelattach
{
    const char *tag, *name;
    int anim, basetime;
    float sizescale;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), anim(-1), basetime(0), sizescale(-1), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name, int anim = -1, int basetime = 0, float sizescale = 1) : tag(tag), name(name), anim(anim), basetime(basetime), sizescale(sizescale), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), anim(-1), basetime(0), sizescale(1), pos(pos), m(NULL) {}
};

struct Texture;

struct entmodelstate
{
    float yaw, pitch, roll, size, radius;
    int anim, flags, basetime, basetime2, lastspin;
    vec o, center;
    vec4 color;
    bvec material[MAXMDLMATERIALS];

    entmodelstate() { reset(); }

    void reset()
    {
        yaw = pitch = roll = radius = 0;
        size = 1;
        anim = flags = basetime = basetime2 = lastspin = 0;
        o = center = vec(0, 0, 0);
        color = vec4(1, 1, 1, 1);
        loopi(MAXMDLMATERIALS) material[i] = bvec(255, 255, 255);
    }
};

struct modelstate : entmodelstate
{
    vec4 mixercolor;
    vec2 matbright, mixerglow, mixerscroll;
    float patternscale;
    Texture *mixer, *pattern;
    modelattach *attached;

    modelstate() { reset(); }

    void reset()
    {
        mixercolor = vec4(1, 1, 1, 1);
        matbright = vec2(1, 1);
        mixerglow = mixerscroll = vec2(0, 0);
        patternscale = 1;
        mixer = pattern = NULL;
        attached = NULL;
    }
};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    int flags;        // the only dynamic state of a map entity
    int lastemit, emit[3];

    extentity() : flags(0), lastemit(0)
    {
        emit[0] = emit[1] = emit[2] = 0;
    }

    bool spawned() const { return (flags&EF_SPAWNED) != 0; }
    void setspawned(bool val) { if(val) flags |= EF_SPAWNED; else flags &= ~EF_SPAWNED; }
    void setspawned() { flags |= EF_SPAWNED; }
    void clearspawned() { flags &= ~EF_SPAWNED; }
};

#define MAXENTS 30000
#define MAXENTATTRS 100
#define MAXENTKIN 100

extern int efocus, enthover, entorient;
#define entfocusv(i, f, v){ int n = efocus = (i); if(n>=0) { extentity &e = *v[n]; f; } }
#define entfocus(i, f)    entfocusv(i, f, entities::getents())

enum { CS_ALIVE = 0, CS_DEAD, CS_EDITING, CS_SPECTATOR, CS_WAITING, CS_MAX }; // beware, some stuff uses >= CS_SPECTATOR
enum { PHYS_FLOAT = 0, PHYS_FALL, PHYS_SLIDE, PHYS_SLOPE, PHYS_FLOOR, PHYS_STEP_UP, PHYS_STEP_DOWN, PHYS_MAX };
enum { ENT_PLAYER = 0, ENT_AI, ENT_INANIMATE, ENT_CAMERA, ENT_PROJ, ENT_RAGDOLL, ENT_DUMMY, ENT_MAX };
enum { COLLIDE_NONE = 0, COLLIDE_ELLIPSE, COLLIDE_OBB, COLLIDE_TRI, COLLIDE_MAX };

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
};

struct physent : baseent                        // can be affected by physics
{
    vec deltapos, newpos;
    float speed, jumpspeed, impulsespeed, weight;
    int airmillis, floormillis;
    float radius, height, aboveeye;             // bounding box size
    float xradius, yradius, zradius, zmargin;
    vec floor;                                  // the normal of floor the dynent is on

    bool blocked, inliquid, onladder, forcepos;
    float curscale;
    char move, strafe;

    uchar physstate;                            // one of PHYS_* above
    uchar type;                                 // one of ENT_* above
    uchar collidetype;                          // one of COLLIDE_* above

    physent() : deltapos(0, 0, 0), newpos(0, 0, 0),
        speed(100), jumpspeed(100), impulsespeed(100), weight(100),
        radius(3.75f), height(17.5f), aboveeye(1.25f),
        xradius(3.75f), yradius(3.75f), zradius(17.5f), zmargin(0),
        curscale(1),
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
        blocked = inliquid = onladder = forcepos = false;
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
    ANIM_IDLE = 0, ANIM_FORWARD, ANIM_BACKWARD, ANIM_LEFT, ANIM_RIGHT, ANIM_DEAD, ANIM_DYING, ANIM_SWIM,
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
#define ANIM_SETSPEED    (1<<29)
#define ANIM_NOPITCH     (1<<30)
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
    int lastswitch;
    void *lastmodel;

    animinterpinfo() : lastswitch(-1), lastmodel(NULL) {}

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

struct dynent : physent                         // animated characters, or characters that can receive input
{
    animinterpinfo animinterp[MAXANIMPARTS];
    ragdolldata *ragdoll;
    occludequery *query;
    int occluded, lastrendered;
    vector<usedent> used;

    dynent() : ragdoll(NULL), query(NULL), occluded(0), lastrendered(0)
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

    static bool is(int t) { return t == ENT_PLAYER || t == ENT_AI || t == ENT_PROJ; }
    static bool is(physent *d) { return d->type == ENT_PLAYER || d->type == ENT_AI || d->type == ENT_PROJ; }

    void reset()
    {
        physent::reset();
        loopi(MAXANIMPARTS) animinterp[i].reset();
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

    int lastused(int n, bool millis = false)
    {
        loopv(used) if(used[i].ent == n) return millis ? used[i].millis : i;
        return millis ? 0 : -1;
    }

    void setused(int n, int millis)
    {
        int p = lastused(n);
        usedent &u = used.inrange(p) ? used[p] : used.add();
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
