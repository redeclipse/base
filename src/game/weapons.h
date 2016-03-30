enum
{
    W_CLAW = 0, W_PISTOL, W_OFFSET, // end of unselectable weapon set
    W_SWORD = W_OFFSET, W_SHOTGUN, W_SMG, W_FLAMER, W_PLASMA, W_ZAPPER, W_RIFLE, W_ITEM,
    W_GRENADE = W_ITEM, W_MINE, W_ROCKET, W_ALL, // end of item weapon set
    W_MELEE = W_ALL, W_MAX, W_LOADOUT = W_ITEM-W_OFFSET // if you add to this at all, check all arrays with W_MAX
};
#define isweap(a)           (a >= 0 && a < W_MAX)

enum { W_F_NONE = 0, W_F_FORCED = 1<<0 };
enum {
    W_C_SCALE = 1<<0, W_C_SCALEN = 1<<1, W_C_LIFE = 1<<2, W_C_LIFEN = 1<<3,
    W_C_SPEED = 1<<4, W_C_SPEEDN = 1<<5, W_C_RAYS = 1<<6, W_C_ZOOM = 1<<7, W_C_KEEP = 1<<8,
    W_C_ALL = W_C_SCALE|W_C_SCALEN|W_C_LIFE|W_C_LIFEN|W_C_SPEED|W_C_SPEEDN|W_C_RAYS|W_C_ZOOM|W_C_KEEP,
    W_C_SS = W_C_SCALE|W_C_SPEED, W_C_SZ = W_C_SCALE|W_C_ZOOM|W_C_KEEP
};
enum {
    W_N_STADD = 1<<0, W_N_GRADD = 1<<1, W_N_STIMM = 1<<2, W_N_GRIMM = 1<<3, W_N_SLIDE = 1<<4,
    W_N_ADD = W_N_STADD|W_N_GRADD, W_N_IMM = W_N_STIMM|W_N_GRIMM,
    W_N_ST = W_N_STADD|W_N_STIMM, W_N_GR = W_N_GRADD|W_N_GRIMM,
    W_N_AI = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM,
    W_N_ALL = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM|W_N_SLIDE
};
enum {
    W_S_IDLE = 0, W_S_PRIMARY, W_S_SECONDARY, W_S_RELOAD, W_S_SWITCH, W_S_USE, W_S_POWER, W_S_ZOOM, W_S_WAIT, W_S_MAX,
    W_S_EXCLUDE = (1<<W_S_IDLE)|(1<<W_S_POWER)|(1<<W_S_ZOOM)
};

enum
{
    S_W_PRIMARY = 0, S_W_SECONDARY,
    S_W_POWER, S_W_POWER2, S_W_ZOOM,
    S_W_SWITCH, S_W_RELOAD, S_W_NOTIFY,
    S_W_EXPLODE, S_W_EXPLODE2,
    S_W_DESTROY, S_W_DESTROY2,
    S_W_IMPACT, S_W_IMPACT2,
    S_W_EXTINGUISH, S_W_EXTINGUISH2,
    S_W_TRANSIT, S_W_TRANSIT2,
    S_W_BOUNCE, S_W_BOUNCE2,
    S_W_OFFSET, S_W_USE = S_W_OFFSET, S_W_SPAWN,
    S_W_MAX
};

enum
{
    S_WEAPONS   = S_GAME,
    S_CLAW      = S_WEAPONS,
    S_PISTOL    = S_CLAW+S_W_OFFSET,
    S_SWORD     = S_PISTOL+S_W_OFFSET,
    S_SHOTGUN   = S_SWORD+S_W_MAX,
    S_SMG       = S_SHOTGUN+S_W_MAX,
    S_FLAMER    = S_SMG+S_W_MAX,
    S_PLASMA    = S_FLAMER+S_W_MAX,
    S_ZAPPER    = S_PLASMA+S_W_MAX,
    S_RIFLE     = S_ZAPPER+S_W_MAX,
    S_GRENADE   = S_RIFLE+S_W_MAX,
    S_MINE      = S_GRENADE+S_W_MAX,
    S_ROCKET    = S_MINE+S_W_MAX,
    S_MELEE     = S_ROCKET+S_W_MAX,
    S_MAX       = S_MELEE+S_W_OFFSET
};

enum
{
    COLLIDE_TRACE = 1<<0, COLLIDE_PROJ = 1<<1, COLLIDE_OWNER = 1<<2,
    IMPACT_GEOM = 1<<3, IMPACT_PLAYER = 1<<4, IMPACT_SHOTS = 1<<5,
    BOUNCE_GEOM = 1<<6, BOUNCE_PLAYER = 1<<7, BOUNCE_SHOTS = 1<<8,
    DRILL_GEOM = 1<<9, DRILL_PLAYER = 1<<10, DRILL_SHOTS = 1<<11,
    STICK_GEOM = 1<<12, STICK_PLAYER = 1<<13, COLLIDE_HITSCAN = 1<<14,
    COLLIDE_GEOM = IMPACT_GEOM|BOUNCE_GEOM,
    COLLIDE_PLAYER = IMPACT_PLAYER|BOUNCE_PLAYER,
    COLLIDE_SHOTS = IMPACT_SHOTS|BOUNCE_SHOTS,
    COLLIDE_DYNENT = COLLIDE_PLAYER|COLLIDE_SHOTS,
    COLLIDE_ALL = COLLIDE_TRACE|COLLIDE_PROJ|COLLIDE_OWNER|IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|BOUNCE_GEOM|BOUNCE_PLAYER|BOUNCE_SHOTS|DRILL_GEOM|DRILL_PLAYER|DRILL_SHOTS|STICK_GEOM|STICK_PLAYER|COLLIDE_HITSCAN
};

enum
{
    HIT_NONE = 0, HIT_ALT = 1<<0, HIT_LEGS = 1<<1, HIT_TORSO = 1<<2, HIT_WHIPLASH = 1<<3, HIT_HEAD = 1<<4,
    HIT_WAVE = 1<<5, HIT_PROJ = 1<<6, HIT_EXPLODE = 1<<7, HIT_BURN = 1<<8, HIT_BLEED = 1<<9, HIT_SHOCK = 1<<10,
    HIT_MATERIAL = 1<<11, HIT_SPAWN = 1<<12, HIT_LOST = 1<<13, HIT_KILL = 1<<14, HIT_FLAK = 1<<15, HIT_SPEC = 1<<16,
    HIT_CLEAR = HIT_PROJ|HIT_EXPLODE|HIT_BURN|HIT_BLEED|HIT_MATERIAL|HIT_SPAWN|HIT_LOST,
    HIT_SFLAGS = HIT_KILL
};

enum { WR_BURN = 0, WR_BLEED, WR_SHOCK, WR_MAX, WR_ALL = (1<<WR_BURN)|(1<<WR_BLEED)|(1<<WR_SHOCK) };

struct shotmsg { int id; ivec pos; };
struct hitmsg { int flags, proj, target, dist; ivec dir, vel; };

#define hithead(x)       (x&HIT_WHIPLASH || x&HIT_HEAD)
#define hitdealt(x)      (x&HIT_BURN || x&HIT_BLEED || x&HIT_SHOCK || x&HIT_EXPLODE || x&HIT_PROJ || x&HIT_MATERIAL)
#define WR(x)            (1<<(WR_##x))
#define wr_burn(x,y)     (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BURN)))
#define wr_burns(x,y)    (G(burntime) && hitdealt(y) && ((x == -1 && y&HIT_BURN) || wr_burn(x, y)))
#define wr_burning(x,y)  (G(burntime) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BURN) || wr_burn(x, y)))
#define wr_bleed(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BLEED)))
#define wr_bleeds(x,y)   (G(bleedtime) && hitdealt(y) && ((x == -1 && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_bleeding(x,y) (G(bleedtime) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_shock(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(SHOCK)))
#define wr_shocks(x,y)   (G(shocktime) && hitdealt(y) && ((x == -1 && y&HIT_SHOCK) || wr_shock(x, y)))
#define wr_shocking(x,y) (G(shocktime) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_SHOCK) || wr_shock(x, y)))
#define WZ(x)            (W_MAX+(W_##x))

#include "weapdef.h"

WPSVAR(IDF_GAMEMOD, longname, // shown for weapon pickups and loadout menu
    "claw-attack",
    "sidearm-pistol",
    "energy-sword",
    "super-shotgun",
    "submachine-gun",
    "flame-thrower",
    "plasma-inductor",
    "electro-zapper",
    "laser-rifle",
    "frag-grenade",
    "shock-mine",
    "rocket-launcher",
    "melee-attack"
);

WPSVARM(IDF_GAMEMOD, desc,
    "a weak claw attack.",
    "long-range, low-damage bullets with a high rate of fire.",
    "horizontal slice, low-damage, can be swung at short intervals, causes bleed residual damage.",
    "fires a lot of pellets, causes mass destruction at short range, useless at long range.",
    "rapid-fire bullets that bounce off walls and knock back the victim.",
    "short-ranged rapid-fire flamethrower, causes burn residual damage.",
    "fires plasma balls at a high rate with high damage, but relatively low accuracy and velocity.",
    "a rapid-fire electric attack that hits almost instantly, great to finish off a wounded foe.",
    "fires a non-charged laser shot that deals damage in a small area.",
    "a simple explosion device, can be cooked to control detonation delay, causes burn residual damage",
    "explodes and discharges electricity when someone comes too close, slowing them down greatly and causing shock residual damage.",
    "a fast, highly explosive rocket with a large shockwave, causes burn residual damage.",
    "use parkour and kick moves as melee attacks.",
    // begin secondary
    "a strong claw attack.",
    "slower rate of fire than the primary attack, but does more damage.",
    "a slower vertical slice with much more power, causes bleed residual damage.",
    "shoots a slug that explodes and disintegrates into shavings, causes bleed residual damage.",
    "long-range projectiles that stick to surfaces and players before exploding into fragments for additional damage.",
    "a versatile air blast, can be used after jumping to travel great distances or to blast foes off their feet. extinguishes flame residual damage.",
    "a charged, quickly-expanding ball of plasma that quickly sucks players in, causing great damage.",
    "a rapid-fire electric discharge with multiple rays that will also cause shock residual damage and greatly slow down a foe.",
    "a charged and focused laser shot with great precision and adjustable zoom, ideal for long range.",
    "sticks to whatever it hits, but has a smaller area of effect than the primary mode, causes burn residual damage",
    "emits an endless distance laser trap that detonates when tripped, in addition causing shock residual damage which can slow down a foe.",
    "a guided rocket that moves significantly slower than the primary fire mode, causes burn residual damage.",
    "use slide moves as melee attacks."
);

WPSVAR(IDF_GAMEMOD,  name, // keep this incase longname is too long
    "claw",     "pistol",   "sword",    "shotgun",  "smg",      "flamer",   "plasma",   "zapper",   "rifle",    "grenade",  "mine",     "rocket",   "melee"
);

WPFVARM(IDF_GAMEMOD,  aidist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      64.0f,      512.0f,     64.0f,      512.0f,     512.f,      768.0f,     384.0f,     128.0f,     1024.0f,    16.0f,
    16.0f,      256.0f,     48.0f,      128.0f,     128.0f,     64.0f,      64.0f,      512.f,      2048.0f,    256.0f,     128.0f,     512.0f,     16.0f
);
WPVARM(IDF_GAMEMOD,  aiskew, 0, VAR_MAX,
    1,          2,          1,          3,          5,          3,          5,          3,          5,          1,          1,          5,          1,
    1,          2,          1,          3,          5,          3,          5,          3,          5,          1,          1,          5,          1
);
WPVAR(IDF_GAMEMOD,  ammoadd, 1, VAR_MAX,
    1,          10,         1,          2,          40,         50,         25,         48,         6,          1,          1,          1,          1
);
WPVAR(IDF_GAMEMOD,  ammomax, 1, VAR_MAX,
    1,          10,          1,         8,          40,         50,         25,         48,         6,          2,          2,          1,          1
);
WPVARM(IDF_GAMEMOD,  ammosub, 0, VAR_MAX,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,
    0,          1,          0,          2,          4,          10,         25,         6,          1,          1,          1,          1,          0
);
WPFVARK(IDF_GAMEMOD,  blend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD,  collide, 0, COLLIDE_ALL,
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_HITSCAN,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_HITSCAN,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM,
    BOUNCE_GEOM|BOUNCE_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_HITSCAN,

    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_HITSCAN,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_HITSCAN,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_HITSCAN,

    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,

    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER
);
WPVAR(IDF_GAMEMOD|IDF_HEX, colour, 0, 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF02020,   0x40F0C8,   0xC090F0,   0xA020F0,   0x40F000,   0x00F068,   0x803000,   0x606060
);
WPVARM(IDF_GAMEMOD,  cooked, 0, W_C_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          W_C_LIFEN,  0,          0,          0,
    0,          0,          0,          0,          0,          0,          W_C_SS,     0,          W_C_SZ,     W_C_LIFEN,  0,          0,          0
);
WPVARM(IDF_GAMEMOD,  cooktime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,          0,
    0,          0,          0,          0,          0,          0,          2000,       0,          650,        3000,       0,          0,          0
);
WPVAR(IDF_GAMEMOD,  cookzoom, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          650,        0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD,  cookzoommin, 1, 150,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
WPFVAR(IDF_GAMEMOD,  cookzoommax, 1, 150,
    60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60
);
WPVARK(IDF_GAMEMOD,  damage, VAR_MIN, VAR_MAX,
    25,         25,         30,         6,          16,         6,          18,         5,          33,         100,        100,        200,        30,
    10,         35,         65,         6,          16,         10,         10,         12,         120,        100,        100,        200,        40,
    25,         25,         30,         6,          16,         6,          18,         5,          10,         100,        100,        200,        30,
    10,         35,         65,         6,          6,          10,         10,         12,         10,         100,        100,        200,        40
);
WPFVARK(IDF_GAMEMOD,  damagehead, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.85f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD,  damagelegs, FVAR_MIN, FVAR_MAX,
    0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,
    0.5f,       0.3f,       0.5f,       0.3f,       0.3f,       1.0f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.5f,
    0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.3f,       0.35f,      0.35f,      0.35f,
    0.5f,       0.35f,      0.5f,       0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.35f,      0.35f,      0.5f
);
WPFVARK(IDF_GAMEMOD,  damageself, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD,  damageteam, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD,  damagetorso, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.6f,       0.5f,       0.6f,       0.5f,       0.5f,       1.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.6f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.6f,       0.5f,       0.6f,       0.5f,       0.5f,       1.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.6f
);
WPFVARK(IDF_GAMEMOD,  damagepenalty, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  damagewhiplash, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f
);
WPVARM(IDF_GAMEMOD,  delayattack, 1, VAR_MAX,
    500,        130,        500,        750,        90,         90,         350,        90,         750,        750,        500,        1500,       500,
    800,        200,        800,        1250,       350,        1250,       1800,       1250,       1250,       750,        500,        1500,       500
);
WPVAR(IDF_GAMEMOD,  delayreload, 0, VAR_MAX,
    50,         1000,       50,         800,        1200,       1700,       1700,       2000,       2500,       1000,       1500,       2500,       50
);
WPVAR(IDF_GAMEMOD,  disabled, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD,  drill, 0, VAR_MAX,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,          0,
    0,          0,          0,          0,          2,          0,          0,          2,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          8,          8,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  elasticity, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD,  escapedelay, 0, VAR_MAX,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200
);
WPVARK(IDF_GAMEMOD|IDF_HEX, explcol, PC(LAST), 0xFFFFFF,
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE)
);
WPFVARK(IDF_GAMEMOD,  explode, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       8.0f,       3.0f,       4.0f,       64.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      44.0f,      6.0f,       0.0f,       52.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       8.0f,       3.0f,       4.0f,       64.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      44.0f,      6.0f,       4.0f,       52.0f,      32.0f,      96.0f,      0.0f
);
WPVARK(IDF_GAMEMOD,  extinguish, 0, 7,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0
);
WPVARK(IDF_GAMEMOD,  fade, 0, 3,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD,  fadeat, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f
);
WPFVARK(IDF_GAMEMOD,  fadecut, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD,  fadetime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD,  fragjump, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  fragoffset, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f
);
WPVARM(IDF_GAMEMOD,  fragrays, 1, MAXPARAMS,
    5,          5,          5,          5,          5,          5,          5,          5,          5,          25,         30,         35,         5,
    5,          5,          5,          22,         30,         5,          5,          5,          5,          25,         10,         35,         5
);
WPFVARM(IDF_GAMEMOD,  fragrel, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.5f,       0.1f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD,  fragscale, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD,  fragskew, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD,  fragspeed, 0, VAR_MAX,
    0,          0,          0,          0,          0,          200,        0,          0,          0,          250,        750,        400,        0,
    0,          0,          0,          0,          350,        250,        0,          0,          0,          250,        7500,       400,        0
);
WPFVARM(IDF_GAMEMOD,  fragspeedmin, 0, FVAR_MAX,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f
);
WPFVARM(IDF_GAMEMOD,  fragspeedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  fragspread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       0.2f,       0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       0.2f,       0.75f,      0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.1f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD,  fragtime, 1, VAR_MAX,
    500,        500,        500,        250,        500,        1000,       500,        500,        500,        1000,       1000,       1000,       500,
    500,        500,        500,        2000,       1000,       3000,       500,        500,        500,        1000,       5000,       1000,       500
);
WPVARM(IDF_GAMEMOD,  fragtimedelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD,  fragtimeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3,          5,          3,          0,
    0,          0,          0,          0,          3,          0,          0,          0,          0,          3,          100,        3,          0
);
WPVARM(IDF_GAMEMOD,  fragweap, -1, W_MAX*2-1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         W_SHOTGUN,  W_ZAPPER,   W_SMG,      -1,
    -1,         -1,         -1,         WZ(SHOTGUN),WZ(SMG),    -1,         -1,         -1,         -1,         W_SHOTGUN,  WZ(ZAPPER), W_SMG,      -1
);
WPFVAR(IDF_GAMEMOD,  frequency, 0, FVAR_MAX,
    0.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       4.0f,       0.0f
);
WPVARM(IDF_GAMEMOD,  fullauto, 0, 1,
    1,          0,          1,          0,          1,          1,          1,          1,          0,          0,          0,          0,          1,
    1,          0,          1,          0,          1,          0,          0,          0,          0,          0,          0,          0,          1
);
WPVARK(IDF_GAMEMOD,  grab, 0, 3,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    3,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD,  guided, 0, 6,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0
);
WPVARK(IDF_GAMEMOD,  guideddelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0
);
WPFVARK(IDF_GAMEMOD,  headmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  hitpush, FVAR_MIN, FVAR_MAX,
    100.0f,     35.0f,      50.0f,      20.0f,      50.0f,      5.0f,       20.0f,      20.0f,      50.0f,      125.0f,     0.0f,       250.0f,     100.0f,
    200.0f,     35.0f,      100.0f,     25.0f,      15.0f,      50.0f,     -75.0f,      20.0f ,     100.0f,     125.0f,     0.0f,       250.0f,     200.0f,
    100.0f,     35.0f,      50.0f,      20.0f,      50.0f,      5.0f,       20.0f,      20.0f,      20.0f,      125.0f,     0.0f,       250.0f,     100.0f,
    200.0f,     35.0f,      100.0f,     25.0f,      15.0f,      50.0f,     -75.0f,      20.0f,      20.0f,      125.0f,     0.0f,       250.0f,     200.0f
);
WPFVARK(IDF_GAMEMOD,  hitvel, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD,  interacts, 0, 3,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0
);
WPFVARM(IDF_GAMEMOD,  kickpush, FVAR_MIN, FVAR_MAX,
    0.0f,       4.0f,       -15.0f,     50.0f,      5.0f,       1.0f,       20.0f,      5.0f,       35.0f,      5.0f,       5.0f,       150.0f,     0.0f,
    0.0f,       6.0f,       -30.0f,     75.0f,      25.0f,      50.0f,      150.0f,     50.0f,      50.0f,      5.0f,       5.0f,       150.0f,     0.0f
);
WPVAR(IDF_GAMEMOD,  laser, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD|IDF_HEX, lightcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVAR(IDF_GAMEMOD,  lightpersist, 0, 15,
    0,          0,          1,          0,          0,          1,          0,          1,          0,          8,          0,          6,          0
);
WPFVAR(IDF_GAMEMOD,  lightradius, 0, FVAR_MAX,
    0,          8,          8,          8,          8,          8,          16,         4,          16,         16,         16,         16,         0
);
WPFVARK(IDF_GAMEMOD,  liquidcoast, 0, FVAR_MAX,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f
);
WPVAR(IDF_GAMEMOD,  modes, -G_ALL, G_ALL,
    0,          -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      0
);
WPVAR(IDF_GAMEMOD,  muts, -G_M_ALL, G_M_ALL,
    0,          -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    0,          -G_M_IM,    -G_M_DK,    0
);
WPSVARR(IDF_GAMEMOD,  obitsuicide,
    "hit themself",
    "ate a bullet",
    "created too much torsional stress",
    "tested the effectiveness of their own shrapnel",
    "fell victim to their own crossfire",
    "spontaneously combusted",
    "was caught up in their own plasma-filled mayhem",
    "tried to use themself as a circuit breaker",
    "got a good shock",
    "kicked it, kamikaze style",
    "kicked it, kamikaze style",
    "exploded with style",
    "kicked themself"
);
WPSVARR(IDF_GAMEMOD,  obitobliterated,
    "clawed and mauled",
    "skewered",
    "sliced in half",
    "turned into little chunks",
    "swiss-cheesed",
    "barbequed",
    "reduced to ooze",
    "shocked relentlessly",
    "given laser shock treatment",
    "turned into shrapnel",
    "turned into shrapnel",
    "obliterated",
    "given kung-fu lessons"
);
WPSVARR(IDF_GAMEMOD,  obitheadless,
    "clawed and mauled",
    "capped",
    "sliced in half",
    "scrambled",
    "air conditioned",
    "char-grilled",
    "plasmafied",
    "electrocuted",
    "expertly sniped",
    "blown to pieces",
    "blown to pieces",
    "exploded",
    "given kung-fu lessons"
);
WPSVARK(IDF_GAMEMOD,  obituary,
    "clawed",
    "pierced",
    "impaled",
    "sprayed with buckshot",
    "riddled with holes",
    "char-grilled",
    "plasmified",
    "electrocuted",
    "laser shocked",
    "blown to pieces",
    "blown to pieces",
    "exploded",
    "kicked",

    "clawed",
    "pierced",
    "impaled",
    "filled with lead",
    "spliced apart",
    "snuffed out",
    "shown the light",
    "shocked into submission",
    "given laser burn",
    "blown to pieces",
    "blown to pieces",
    "exploded",
    "kicked",

    "clawed and mauled",
    "picked to pieces",
    "melted in half",
    "filled with shrapnel",
    "air-conditioned",
    "cooked alive",
    "melted alive",
    "turned into a lightning rod",
    "shocked to pieces",
    "turned into shrapnel",
    "turned into shrapnel",
    "obliterated",
    "given kung-fu lessons",

    "clawed and mauled",
    "picked to pieces",
    "melted in half",
    "filled with shrapnel",
    "air-conditioned",
    "cooked alive",
    "melted alive",
    "turned into a lightning rod",
    "shocked to pieces",
    "turned into shrapnel",
    "turned into shrapnel",
    "obliterated",
    "given kung-fu lessons"
);
WPFVARK(IDF_GAMEMOD,  partblend, 0, 1,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.15f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.15f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD|IDF_HEX, partcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVARK(IDF_GAMEMOD,  partfade, 1, VAR_MAX,
    500,        250,        500,        250,        250,        200,        500,        100,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        200,        950,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        200,        500,        250,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        500,        750,        500,        500,        750,        500
);
WPFVARK(IDF_GAMEMOD,  partlen, 0, FVAR_MAX,
    0.0f,       8.0f,       0.0f,       30.0f,      20.0f,      0.0f,       0.0f,       1024.f,     256.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       15.0f,      15.0f,      0.0f,       0.0f,       1024.f,     1024.0f,    0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       8.0f,       0.0f,       7.5f,       7.5f,       0.0f,       0.0f,       1024.f,     256.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       7.5f,       7.5f,       0.0f,       0.0f,       1024.f,     512.0f,     0.0f,       4.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  partsize, 0, FVAR_MAX,
    1.0f,       0.125f,     1.0f,       0.75f,      0.6f,       6.0f,       8.0f,       2.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.45f,      0.4f,       12.0f,      28.0f,      3.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f,
    1.0f,       0.125f,     1.0f,       0.45f,      0.4f,       6.0f,       8.0f,       3.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.45f,      0.4f,       12.0f,      28.0f,      5.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f
);
WPVARK(IDF_GAMEMOD,  parttype, 0, W_MAX-1,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE
);
WPVARK(IDF_GAMEMOD,  proxdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0
);
WPFVARK(IDF_GAMEMOD,  proxdist, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD,  proxtime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD,  proxtype, 0, 2,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0
);
WPVARK(IDF_GAMEMOD,  radial, 0, VAR_MAX,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  radius, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD,  rays, 0, MAXPARAMS,
    1,          1,          1,          20,         1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          5,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD,  reflectivity, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  relativity, 0, FVAR_MAX,
    0.0f,       0.05f,      0.0f,       0.15f,      0.15f,      0.95f,      0.15f,      0.0f,       0.5f,       0.75f,      0.5f,       0.0f,       0.0f,
    0.0f,       0.05f,      0.0f,       0.35f,      0.25f,      0.15f,      0.15f,      0.0f,       0.1f,       0.75f,      0.5f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD,  residual, 0, WR_ALL,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0
);
WPVARK(IDF_GAMEMOD, residualundo, 0, WR_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD,  speed, 0, VAR_MAX,
    0,          1500,       0,          1000,       2000,       400,        1250,       10000,      10000,      200,        100,        1000,       0,
    0,          1000,       0,          250,        1500,       750,        85,         50000,      100000,     200,        100,        250,        0
);
WPFVARK(IDF_GAMEMOD,  speeddelta, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      1000.0f,    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      1000.0f,    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPVARM(IDF_GAMEMOD,  speedlimit, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          35,         0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  speedmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       25.0f,      25.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       25.0f,      25.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      0.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      0.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f
);
WPFVARK(IDF_GAMEMOD,  speedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  spread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       12.5f,      2.5f,       5.0f,       2.0f,       2.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       2.0f,       2.0f,       1.0f,       1.0f,       3.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD,  spreadmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  spreadmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD,  spreadz, 0, FVAR_MAX,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       2.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       4.0f,       2.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPVARK(IDF_GAMEMOD,  stun, 0, W_N_ALL,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_AI,     W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD
);
WPFVARK(IDF_GAMEMOD,  stunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.1f,       0.01f,      0.1f,       0.0f,       8.0f,       0.2f,       0.0f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       1.5f,       0.5f,       0.01f,      1.0f,       0.0f,       16.0f,      0.1f,       0.0f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.1f,       8.0f,       8.0f,       0.0f,       8.0f,       0.2f,       0.0f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       1.5f,       0.5f,       16.0f,      16.0f,      0.0f,       16.0f,      0.1f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  stunscale, 0, FVAR_MAX,
    0.5f,       0.25f,      2.0f,       0.25f,      0.5f,       0.0f,       0.2f,       0.5f,       0.75f,      2.0f,       8.0f,       16.0f,      0.5f,
    1.0f,       0.25f,      4.0f,       0.35f,      1.0f,       0.15f,      0.5f,       0.5f,       2.0f,       2.0f,       16.0f,      8.0f,       1.0f,
    0.5f,       0.25f,      2.0f,       0.25f,      0.5f,       0.0f,       0.2f,       8.0f,       8.0f,       2.0f,       8.0f,       16.0f,      0.5f,
    1.0f,       0.25f,      4.0f,       0.35f,      1.0f,       0.15f,      0.5f,       16.0f,      16.0f,      2.0f,       16.0f,      8.0f,       1.0f
);
WPVARK(IDF_GAMEMOD,  stuntime, 0, VAR_MAX,
    100,        25,         300,        75,         75,         0,          0,          500,        100,        200,        500,        750,        100,
    200,        25,         500,        150,        150,        500,        200,        250,        250,        200,        750,        500,        200,
    100,        25,         300,        75,         75,         0,          0,          500,        500,        200,        500,        750,        100,
    200,        25,         500,        150,        150,        500,        200,        750,        750,        200,        750,        500,        200
);
WPVARK(IDF_GAMEMOD,  taper, 0, 6,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  taperin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  taperout, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  tapermin, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD,  tapermax, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD,  time, 1, VAR_MAX,
    100,        2000,       350,        200,        510,        200,        350,        100,        5000,       3000,       60000,      5000,       100,
    350,        1000,       500,        700,        1000,       100,        3500,       100,        5000,       3000,       30000,      5000,       350
);
WPVARM(IDF_GAMEMOD,  timedelay, 0, VAR_MAX,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          75,         75,         0,          0,
    0,          0,          10,         0,          0,          0,          75,         0,          0,          75,         75,         0,          0
);
WPVARM(IDF_GAMEMOD,  timeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          80,         0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD,  timestick, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD,  trace, 0, FVAR_MAX,
    4.0f,       1.0f,       1.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       4.0f,
    4.0f,       1.0f,       1.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       4.0f
);
WPFVARK(IDF_GAMEMOD,  visfade, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD,  vistime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD,  wavepush, 0, FVAR_MAX,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       2.5f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       2.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       2.5f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f
);
WPFVARK(IDF_GAMEMOD,  weight, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,       0.0f
);

#define WRS(a,b,c,d)         ((a)*(m_dm_gladiator(c, d) ? G(gladiator##b##scale) : G(b##scale)*(m_sweaps(c, d) ? G(b##limited) : 1.f)))
#define WX(k,a,b,c,d,e,f)    (!m_insta(d, e) || (a) != W_RIFLE ? WRS(WF(k, a, b, c)*f, radial, d, e) : 0)
#define WSP(a,b,c,d,e)       (!m_insta(c, d) || (a) != W_RIFLE ? clamp(W2(a, spread, b)*(e), W2(a, spreadmin, b), W2(a, spreadmax, b) > 0 ? W2(a, spreadmax, b) : FVAR_MAX) : 0.f)
#define WSND(a,b)            (weaptype[a].sound+(b))
#define WSNDF(a,b)           (weaptype[a].sound+((b) ? S_W_SECONDARY : S_W_PRIMARY))
#define WSND2(a,b,c)         (weaptype[a].sound+((b) ? (c)+1 : (c)))
#define WUSE(a)              (a < W_ITEM ? W(a, ammomax) : W(a, ammoadd))
#define WHCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? W2(a, b, c) : game::hexpulsecolour(d, clamp(INVPULSE(W2(a, b, c)), 0, int(PULSE_LAST)), 50))
#define WPCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? vec::hexcolor(W2(a, b, c)) : game::pulsecolour(d, clamp(INVPULSE(W2(a, b, c)), 0, int(PULSE_LAST)), 50))

struct weaptypes
{
    int     anim,               sound,      espeed;
    bool    muzzle,     eject,      tape;
    float   thrown[2],              halo,       esize;
    const char *name,   *item,                      *vwep, *hwep,                     *proj,                  *eprj;
};
#ifdef GAMESERVER
weaptypes weaptype[] =
{
    {
            ANIM_CLAW,         S_CLAW,    1,
            true,       false,      true,
            { 0, 0 },               14,         0,
            "claw", "weapons/claw/item", "weapons/claw/vwep", "weapons/claw/hwep", "", ""
    },
    {
            ANIM_PISTOL,        S_PISTOL,   10,
            true,       true,       false,
            { 0, 0 },               8,          0.35f,
            "pistol", "weapons/pistol/item", "weapons/pistol/vwep", "weapons/pistol/hwep", "weapons/pistol/proj", "projectiles/cartridge"
    },
    {
            ANIM_SWORD,         S_SWORD,    1,
            true,       false,      true,
            { 0, 0 },               14,         0,
            "sword", "weapons/sword/item", "weapons/sword/vwep", "weapons/sword/hwep", "", ""
    },
    {
            ANIM_SHOTGUN,       S_SHOTGUN,  10,
            true,       true,       false,
            { 0, 0 },               12,         0.45f,
            "shotgun", "weapons/shotgun/item", "weapons/shotgun/vwep", "weapons/shotgun/hwep", "", "projectiles/shell"
    },
    {
            ANIM_SMG,           S_SMG,      20,
            true,       true,       false,
            { 0, 0 },               10,         0.35f,
            "smg", "weapons/smg/item", "weapons/smg/vwep", "weapons/smg/hwep", "", "projectiles/cartridge"
    },
    {
            ANIM_FLAMER,        S_FLAMER,   1,
            true,       true,       false,
            { 0, 0 },               12,         0,
            "flamer", "weapons/flamer/item", "weapons/flamer/vwep", "weapons/flamer/hwep", "", ""
    },
    {
            ANIM_PLASMA,        S_PLASMA,   1,
            true,       false,      false,
            { 0, 0 },               8,         0,
            "plasma", "weapons/plasma/item", "weapons/plasma/vwep", "weapons/plasma/hwep", "", ""
    },
    {
            ANIM_ZAPPER,        S_ZAPPER,   1,
            true,       false,      true,
            { 0, 0 },               10,         0,
            "zapper", "weapons/zapper/item", "weapons/zapper/vwep", "weapons/zapper/hwep", "", ""
    },
    {
            ANIM_RIFLE,         S_RIFLE,    1,
            true,       false,      false,
            { 0, 0 },               12,         0,
            "rifle", "weapons/rifle/item", "weapons/rifle/vwep", "weapons/rifle/hwep", "", ""
    },
    {
            ANIM_GRENADE,       S_GRENADE,  1,
            false,      false,      false,
            { 0.0625f, 0.0625f },   6,          0,
            "grenade", "weapons/grenade/item", "weapons/grenade/vwep", "weapons/grenade/hwep", "weapons/grenade/proj", ""
    },
    {
            ANIM_MINE,          S_MINE,     1,
            false,      false,      false,
            { 0.0625f, 0.0625f },   6,          0,
            "mine", "weapons/mine/item", "weapons/mine/vwep", "weapons/mine/hwep", "weapons/mine/proj", ""
    },
    {
            ANIM_ROCKET,        S_ROCKET,   1,
            true,      false,       false,
            { 0, 0 },               10,          0,
            "rocket", "weapons/rocket/item", "weapons/rocket/vwep", "weapons/rocket/hwep", "weapons/rocket/proj",  ""
    },
    {
            0,                  S_MELEE,    1,
            false,      false,      false,
            { 0, 0 },               1,          0,
            "melee",    "", "", "", "", ""
    }
};
SVAR(0, weapname, "claw pistol sword shotgun smg flamer plasma zapper rifle grenade mine rocket melee");
VAR(0, weapidxclaw, 1, W_CLAW, -1);
VAR(0, weapidxpistol, 1, W_PISTOL, -1);
VAR(0, weapidxsword, 1, W_SWORD, -1);
VAR(0, weapidxshotgun, 1, W_SHOTGUN, -1);
VAR(0, weapidxsmg, 1, W_SMG, -1);
VAR(0, weapidxflamer, 1, W_FLAMER, -1);
VAR(0, weapidxplasma, 1, W_PLASMA, -1);
VAR(0, weapidxzapper, 1, W_ZAPPER, -1);
VAR(0, weapidxrifle, 1, W_RIFLE, -1);
VAR(0, weapidxgrenade, 1, W_GRENADE, -1);
VAR(0, weapidxmine, 1, W_MINE, -1);
VAR(0, weapidxrocket, 1, W_ROCKET, -1);
VAR(0, weapidxmelee, 1, W_MELEE, -1);
VAR(0, weapidxoffset, 1, W_OFFSET, -1);
VAR(0, weapidxitem, 1, W_ITEM, -1);
VAR(0, weapidxhidden, 1, W_ALL, -1);
VAR(0, weapidxloadout, 1, W_LOADOUT, -1);
VAR(0, weapidxnum, 1, W_MAX, -1);
#else
extern weaptypes weaptype[];
#endif
