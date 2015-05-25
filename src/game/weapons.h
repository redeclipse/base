enum
{
    W_MELEE = 0, W_PISTOL, W_OFFSET, // end of unselectable weapon set
    W_SWORD = W_OFFSET, W_SHOTGUN, W_SMG, W_FLAMER, W_PLASMA, W_ZAPPER, W_RIFLE, W_ITEM,
    W_GRENADE = W_ITEM, W_MINE, W_ROCKET, // end of item weapon set
    W_MAX, W_LOADOUT = W_ITEM-W_OFFSET // if you add to this at all, check all arrays with W_MAX
};
#define isweap(a)       (a >= 0 && a < W_MAX)

enum { W_F_NONE = 0, W_F_FORCED = 1<<0 };
enum {
    W_C_SCALE = 1<<0, W_C_SCALEN = 1<<1, W_C_LIFE = 1<<2, W_C_LIFEN = 1<<3,
    W_C_SPEED = 1<<4, W_C_SPEEDN = 1<<5, W_C_RAYS = 1<<6, W_C_ZOOM = 1<<7,
    W_C_ALL = W_C_SCALE|W_C_SCALEN|W_C_LIFE|W_C_LIFEN|W_C_SPEED|W_C_SPEEDN|W_C_RAYS|W_C_ZOOM,
    W_C_SS = W_C_SCALE|W_C_SPEED, W_C_SZ = W_C_SCALE|W_C_ZOOM
};
enum {
    W_N_STADD = 1<<0, W_N_GRADD = 1<<1, W_N_STIMM = 1<<2, W_N_GRIMM = 1<<3,
    W_N_ADD = W_N_STADD|W_N_GRADD, W_N_IMM = W_N_STIMM|W_N_GRIMM,
    W_N_ST = W_N_STADD|W_N_STIMM, W_N_GR = W_N_GRADD|W_N_GRIMM,
    W_N_ALL = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM
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
    S_MELEE     = S_WEAPONS,
    S_PISTOL    = S_MELEE+S_W_OFFSET,
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
    S_MAX       = S_ROCKET+S_W_MAX
};

enum
{
    COLLIDE_TRACE = 1<<0, COLLIDE_PROJ = 1<<1, COLLIDE_OWNER = 1<<2,
    IMPACT_GEOM = 1<<3, IMPACT_PLAYER = 1<<4, IMPACT_SHOTS = 1<<5,
    BOUNCE_GEOM = 1<<6, BOUNCE_PLAYER = 1<<7, BOUNCE_SHOTS = 1<<8,
    DRILL_GEOM = 1<<9, DRILL_PLAYER = 1<<10, DRILL_SHOTS = 1<<11,
    STICK_GEOM = 1<<12, STICK_PLAYER = 1<<13,
    COLLIDE_GEOM = IMPACT_GEOM|BOUNCE_GEOM,
    COLLIDE_PLAYER = IMPACT_PLAYER|BOUNCE_PLAYER,
    COLLIDE_SHOTS = IMPACT_SHOTS|BOUNCE_SHOTS,
    COLLIDE_DYNENT = COLLIDE_PLAYER|COLLIDE_SHOTS,
    COLLIDE_ALL = COLLIDE_TRACE|COLLIDE_PROJ|COLLIDE_OWNER|IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|BOUNCE_GEOM|BOUNCE_PLAYER|BOUNCE_SHOTS|DRILL_GEOM|DRILL_PLAYER|DRILL_SHOTS|STICK_GEOM|STICK_PLAYER
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
#define hithurts(x)      (x&HIT_BURN || x&HIT_BLEED || x&HIT_SHOCK || x&HIT_EXPLODE || x&HIT_PROJ || x&HIT_MATERIAL)
#define WR(x)            (1<<(WR_##x))
#define wr_burn(x,y)     (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BURN)))
#define wr_burns(x,y)    (G(burntime) && hithurts(y) && ((x == -1 && y&HIT_BURN) || wr_burn(x, y)))
#define wr_burning(x,y)  (G(burntime) && hithurts(y) && wr_burn(x, y))
#define wr_bleed(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BLEED)))
#define wr_bleeds(x,y)   (G(bleedtime) && hithurts(y) && ((x == -1 && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_bleeding(x,y) (G(bleedtime) && hithurts(y) && wr_bleed(x, y))
#define wr_shock(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(SHOCK)))
#define wr_shocks(x,y)   (G(shocktime) && hithurts(y) && ((x == -1 && y&HIT_SHOCK) || wr_shock(x, y)))
#define wr_shocking(x,y) (G(shocktime) && hithurts(y) && wr_shock(x, y))
#define WZ(x)            (W_MAX+(W_##x))

#include "weapdef.h"

WPSVAR(0, name,
    "melee",    "pistol",   "sword",    "shotgun",  "smg",      "flamer",   "plasma",   "zapper",   "rifle",    "grenade",  "mine",     "rocket"
);
WPFVARM(0, aidist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      64.0f,      512.0f,     64.0f,      512.0f,     256.f,      768.0f,     384.0f,     128.0f,     1024.0f,
    16.0f,      256.0f,     48.0f,      128.0f,     128.0f,     64.0f,      64.0f,      256.f,      2048.0f,    256.0f,     128.0f,     512.0f
);
WPVARM(0, aiskew, 0, VAR_MAX,
    1,          2,          1,          3,          4,          3,          4,          2,          5,          1,          1,          5,
    1,          2,          1,          3,          4,          3,          4,          2,          5,          1,          1,          5
);
WPVAR(0, ammoadd, 1, VAR_MAX,
    1,          10,         1,          2,          40,         60,         25,         48,         6,          1,          1,          1
);
WPVAR(0, ammomax, 1, VAR_MAX,
    1,          10,          1,         8,          40,         60,         25,         48,         6,          2,          2,          1
);
WPVARM(0, ammosub, 0, VAR_MAX,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    0,          1,          0,          2,          4,          12,         25,         6,          1,          1,          1,          1
);
WPFVARK(0, blend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(0, collide, 0, COLLIDE_ALL,
    IMPACT_PLAYER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|DRILL_PLAYER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM,
    BOUNCE_GEOM|BOUNCE_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,

    IMPACT_PLAYER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|STICK_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,

    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|DRILL_PLAYER|COLLIDE_OWNER,
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
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|DRILL_PLAYER|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ
);
WPVAR(IDF_HEX, colour, 0, 0xFFFFFF,
    0x606060,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   0xF02020,   0x40F0C8,   0xC090F0,   0xA020F0,   0x40F000,   0x00F068,   0x803000
);
WPVARM(0, cooked, 0, W_C_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          W_C_LIFEN,  0,          0,
    0,          0,          0,          0,          0,          0,          W_C_SS,     0,          W_C_SZ,     W_C_LIFEN,  0,          0
);
WPVARM(0, cooktime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          2000,       0,          350,        3000,       0,          0
);
WPVARK(0, damage, VAR_MIN, VAR_MAX,
    30,         25,         30,         6,          12,         6,          18,         6,          33,         100,        100,        200,
    40,         35,         65,         6,          12,         10,         10,         12,         100,        100,        100,        200,
    30,         25,         30,         6,          12,         6,          18,         6,          10,         100,        100,        200,
    40,         35,         65,         6,          6,          10,         10,         12,         10,         100,        100,        200
);
WPFVARK(0, damagehead, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(0, damagelegs, FVAR_MIN, FVAR_MAX,
    0.3f,       0.325f,     0.3f,       0.3f,       0.25f,      0.25f,      0.25f,      0.25f,      0.3f,       0.2f,       0.2f,       0.2f,
    0.6f,       0.325f,     0.3f,       0.3f,       0.25f,      1.0f,       0.25f,      0.25f,      0.3f,       0.2f,       0.2f,       0.2f,
    0.3f,       0.325f,     0.3f,       0.3f,       0.25f,      0.25f,      0.25f,      0.25f,      0.3f,       0.2f,       0.2f,       0.2f,
    0.6f,       0.325f,     0.3f,       0.3f,       0.25f,      0.25f,      0.25f,      0.25f,      0.3f,       0.2f,       0.2f,       0.2f
);
WPFVARK(0, damageself, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(0, damageteam, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(0, damagetorso, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.55f,      0.6f,       0.5f,       0.5f,       0.4f,       0.4f,       0.5f,       0.4f,       0.4f,       0.4f,
    0.75f,      0.6f,       0.55f,      0.6f,       0.5f,       1.5f,       0.4f,       0.4f,       0.5f,       0.4f,       0.4f,       0.4f,
    0.5f,       0.5f,       0.55f,      0.6f,       0.5f,       0.5f,       0.4f,       0.4f,       0.5f,       0.4f,       0.4f,       0.4f,
    0.75f,      0.6f,       0.55f,      0.6f,       0.5f,       1.5f,       0.4f,       0.4f,       0.5f,       0.4f,       0.4f,       0.4f
);
WPFVARK(0, damagepenalty, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(0, damagewhiplash, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f
);
WPVARM(0, delayattack, 1, VAR_MAX,
    500,        130,        500,        800,        90,         100,        275,        80,         850,        1000,       1000,       1500,
    1000,       200,        800,        1200,       400,        1250,       2000,       1200,       1000,       1000,       1000,       1500
);
WPVAR(0, delayreload, 0, VAR_MAX,
    50,         1000,       50,         850,        1250,       1850,       1750,       1500,       1500,       800,        1400,       2200
);
WPVAR(0, disabled, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(0, drill, 0, VAR_MAX,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,
    0,          0,          0,          0,          2,          0,          0,          2,          2,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          8,          8,          0,          0,          0
);
WPFVARK(0, elasticity, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(0, escapedelay, 0, VAR_MAX,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200
);
WPVARK(IDF_HEX, explcol, -PULSE_MAX, 0xFFFFFF,
    -1,         -1,         0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         -4,         -1,
    -1,         -1,         0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   -4,         0xA020F0,   -1,         -4,         -1,
    -1,         -1,         0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         -4,         -1,
    -1,         -1,         0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   -4,         0xA020F0,   -1,         -4,         -1
);
WPFVARK(0, explode, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       6.0f,       8.0f,       8.0f,       8.0f,       64.0f,      32.0f,      96.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      48.0f,      0.0f,       0.0f,       52.0f,      32.0f,      96.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       8.0f,       4.0f,       8.0f,       64.0f,      32.0f,      96.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      48.0f,      8.0f,       8.0f,       52.0f,      32.0f,      96.0f
);
WPVARK(0, extinguish, 0, 7,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2
);
WPVARK(0, fade, 0, 3,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(0, fadeat, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f
);
WPFVARK(0, fadecut, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(0, fadetime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(0, fragjump, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, fragoffset, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f
);
WPVARM(0, fragrays, 1, MAXPARAMS,
    5,          5,          5,          5,          5,          5,          5,          5,          5,          25,         30,         35,
    5,          5,          5,          22,         30,         5,          5,          5,          5,          25,         10,         35
);
WPFVARM(0, fragrel, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.5f,       0.1f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, fragscale, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(0, fragskew, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f
);
WPVARM(0, fragspeed, 0, VAR_MAX,
    0,          0,          0,          0,          0,          200,        0,          0,          0,          250,        750,        400,
    0,          0,          0,          0,          350,        250,        0,          0,          0,          250,        7500,       400
);
WPFVARM(0, fragspeedmin, 0, FVAR_MAX,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      50.0f
);
WPFVARM(0, fragspeedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, fragspread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       0.2f,       0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.5f,       1.0f,
    1.0f,       1.0f,       1.0f,       0.2f,       0.75f,      0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.1f,       1.0f
);
WPVARM(0, fragtime, 1, VAR_MAX,
    500,        500,        500,        250,        500,        1000,       500,        500,        500,        1000,       1000,       1000,
    500,        500,        500,        2000,       1000,       3000,       500,        500,        500,        1000,       5000,       1000
);
WPVARM(0, fragtimedelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(0, fragtimeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3,          5,          3,
    0,          0,          0,          0,          3,          0,          0,          0,          0,          3,          100,        3
);
WPVARM(0, fragweap, -1, W_MAX*2-1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         W_SHOTGUN,  W_ZAPPER,   W_SMG,
    -1,         -1,         -1,         WZ(SHOTGUN),WZ(SMG),    -1,         -1,         -1,         -1,         W_SHOTGUN,  WZ(ZAPPER), W_SMG
);
WPFVAR(0, frequency, 0, FVAR_MAX,
    0.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       4.0f
);
WPVARM(0, fullauto, 0, 1,
    1,          0,          1,          0,          1,          1,          1,          1,          0,          0,          0,          0,
    1,          0,          1,          0,          1,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(0, guided, 0, 6,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1
);
WPVARK(0, guideddelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100
);
WPFVARK(0, headmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f
);
WPFVARK(0, hitpush, FVAR_MIN, FVAR_MAX,
    100.0f,     35.0f,      50.0f,      20.0f,      50.0f,      5.0f,       20.0f,      20.0f,      50.0f,      125.0f,     0.0f,       250.0f,
    200.0f,     35.0f,      100.0f,     25.0f,      50.0f,      100.0f,     -75.0f,     20.0f ,     100.0f,     125.0f,     0.0f,       250.0f,
    100.0f,     35.0f,      50.0f,      20.0f,      50.0f,      5.0f,       20.0f,      20.0f,      20.0f,      125.0f,     0.0f,       250.0f,
    200.0f,     35.0f,      100.0f,     25.0f,      50.0f,      100.0f,     -75.0f,     20.0f,      20.0f,      125.0f,     0.0f,       250.0f
);
WPFVARK(0, hitvel, 0, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(0, interacts, 0, 3,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1
);
WPFVARM(0, kickpush, FVAR_MIN, FVAR_MAX,
    0.0f,       4.0f,       -15.0f,     50.0f,      5.0f,       1.0f,       20.0f,      5.0f,       35.0f,      5.0f,       5.0f,       150.0f,
    0.0f,       6.0f,       -30.0f,     75.0f,      25.0f,      5.0f,       150.0f,     50.0f,      50.0f,      5.0f,       5.0f,       150.0f
);
WPVAR(0, laser, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_HEX, lightcol, -PULSE_MAX, 0xFFFFFF,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1
);
WPVAR(0, lightpersist, 0, 1,
    0,          0,          1,          0,          0,          1,          0,          1,          0,          0,          0,          0
);
WPFVAR(0, lightradius, 0, FVAR_MAX,
    0,          8,          8,          8,          8,          8,          16,         4,          16,         16,         16,         16
);
WPFVARK(0, liquidcoast, 0, FVAR_MAX,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f
);
WPVAR(0, modes, -G_ALL, G_ALL,
    0,          -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW,      -G_SW
);
WPVAR(0, muts, -G_M_ALL, G_M_ALL,
    0,          -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    -G_M_SW,    0,          -G_M_IM,    -G_M_DK
);
WPSVARR(0, obitsuicide,
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
    "exploded with style"
);
WPSVARR(0, obitobliterated,
    "given kung-fu lessons",
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
    "obliterated"
);
WPSVARR(0, obitheadless,
    "given kung-fu lessons",
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
    "exploded"
);
WPSVARK(0, obituary,
    "punched",
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

    "given kung-fu lessons",
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
    "obliterated"
);
WPFVARK(0, partblend, 0, 1,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.15f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.15f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f
);
WPVARK(IDF_HEX, partcol, -PULSE_MAX, 0xFFFFFF,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   -1,         0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1,
    0xEEEE22,   0xC0C0C0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   -4,         0xA020F0,   -1,         0x00F068,   -1
);
WPVARK(0, partfade, 1, VAR_MAX,
    500,        250,        500,        250,        250,        200,        500,        100,        500,        500,        500,        750,
    500,        250,        500,        250,        250,        500,        500,        200,        950,        500,        500,        750,
    500,        250,        500,        250,        250,        200,        500,        250,        500,        500,        500,        750,
    500,        250,        500,        250,        250,        500,        500,        500,        750,        500,        500,        750
);
WPFVARK(0, partlen, 0, FVAR_MAX,
    0.0f,       8.0f,       0.0f,       30.0f,      20.0f,      0.0f,       0.0f,       1024.f,     256.0f,     0.0f,       4.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       15.0f,      15.0f,      0.0f,       0.0f,       1024.f,     1024.0f,    0.0f,       4.0f,       0.0f,
    0.0f,       8.0f,       0.0f,       7.5f,       7.5f,       0.0f,       0.0f,       1024.f,     256.0f,     0.0f,       4.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       7.5f,       7.5f,       0.0f,       0.0f,       1024.f,     512.0f,     0.0f,       4.0f,       0.0f
);
WPFVARK(0, partsize, 0, FVAR_MAX,
    1.0f,       0.125f,     1.0f,       0.75f,      0.6f,       6.0f,       8.0f,       2.0f,       1.5f,       1.0f,       2.0f,       1.0f,
    2.0f,       0.25f,      1.25f,      0.45f,      0.4f,       12.0f,      28.0f,      3.0f,       3.0f,       1.0f,       2.0f,       1.0f,
    1.0f,       0.125f,     1.0f,       0.45f,      0.4f,       6.0f,       8.0f,       3.0f,       1.5f,       1.0f,       2.0f,       1.0f,
    2.0f,       0.25f,      1.25f,      0.45f,      0.4f,       12.0f,      28.0f,      5.0f,       3.0f,       1.0f,       2.0f,       1.0f
);
WPVARK(0, parttype, 0, W_MAX-1,
    W_MELEE,    W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,
    W_MELEE,    W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,
    W_MELEE,    W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,
    W_MELEE,    W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET
);
WPVARK(0, proxdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0
);
WPFVARK(0, proxdist, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f
);
WPVARK(0, proxtime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(0, proxtype, 0, 2,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0
);
WPVARK(0, radial, 0, VAR_MAX,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0
);
WPFVARK(0, radius, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(0, rays, 0, MAXPARAMS,
    1,          1,          1,          22,         1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          5,          1,          1,          1,          1
);
WPFVARK(0, reflectivity, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, relativity, 0, FVAR_MAX,
    0.0f,       0.05f,      0.0f,       0.15f,      0.15f,      0.95f,      0.15f,      0.0f,       0.5f,       0.75f,      0.5f,       0.0f,
    0.0f,       0.05f,      0.0f,       0.35f,      0.25f,      0.15f,      0.15f,      0.0f,       0.1f,       0.75f,      0.5f,       0.0f
);
WPVARK(0, residual, 0, WR_ALL,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(BURN),   WR(SHOCK),  WR(BURN),
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(BURN),   WR(SHOCK),  WR(BURN)
);
WPVARK(0, residualundo, 0, WR_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0
);
WPVARM(0, speed, 0, VAR_MAX,
    0,          1500,       0,          1000,       2000,       400,        1200,       15000,       10000,      200,        100,        1000,
    0,          1000,       0,          250,        750,        750,        85,         100000,       100000,     200,        100,        250
);
WPFVARK(0, speeddelta, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      1000.0f,    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      1000.0f,    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPVARM(0, speedlimit, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          35,         0,          0,          0,          0,          0
);
WPFVARK(0, speedmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       25.0f,      25.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       25.0f,      25.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      0.0f,       50.0f,      50.0f,      50.0f,      50.0f,
    50.0f,      50.0f,      50.0f,      50.0f,      50.0f,      1.0f,       50.0f,      0.0f,       50.0f,      50.0f,      50.0f,      50.0f
);
WPFVARK(0, speedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, spread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       12.0f,      2.5f,       7.5f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       2.0f,       2.0f,       1.0f,       1.0f,       3.0f,       0.25f,      1.0f,       1.0f,       1.0f
);
WPFVARM(0, spreadmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, spreadmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(0, spreadz, 0, FVAR_MAX,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       2.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    1.0f,       2.0f,       1.0f,       4.0f,       2.0f,       4.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(0, stun, 0, W_N_ALL,
    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ALL,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD
);
WPFVARK(0, stunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.1f,       0.01f,      0.1f,       0.0f,       8.0f,       0.2f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       1.5f,       0.5f,       0.01f,      1.0f,       0.0f,       16.0f,      0.1f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.1f,       8.0f,       8.0f,       0.0f,       8.0f,       0.2f,
    0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       1.5f,       0.5f,       16.0f,      16.0f,      0.0f,       16.0f,      0.1f
);
WPFVARK(0, stunscale, 0, FVAR_MAX,
    0.5f,       0.25f,      2.0f,       0.25f,      0.5f,       0.0f,       0.2f,       0.5f,       0.5f,       2.0f,       8.0f,       16.0f,
    1.0f,       0.25f,      4.0f,       0.35f,      1.0f,       0.15f,      0.5f,       0.5f,       1.0f,       2.0f,       16.0f,      8.0f,
    0.5f,       0.25f,      2.0f,       0.25f,      0.5f,       0.0f,       0.2f,       8.0f,       8.0f,       2.0f,       8.0f,       16.0f,
    1.0f,       0.25f,      4.0f,       0.35f,      1.0f,       0.15f,      0.5f,       16.0f,      16.0f,      2.0f,       16.0f,      8.0f
);
WPVARK(0, stuntime, 0, VAR_MAX,
    100,        25,         300,        75,         75,         0,          0,          250,        100,        200,        500,        750,
    200,        25,         500,        150,        150,        500,        200,        250,        250,        200,        750,        500,
    100,        25,         300,        75,         75,         0,          0,          500,        500,        200,        500,        750,
    200,        25,         500,        150,        150,        500,        200,        750,        750,        200,        750,        500
);
WPVARK(0, taper, 0, 6,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0
);
WPFVARK(0, taperin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(0, taperout, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.01f,      0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARM(0, time, 1, VAR_MAX,
    100,        2000,       350,        260,        510,        200,        350,        80,         5000,       3000,       60000,      5000,
    350,        1000,       500,        2500,       1000,       100,        3500,       80,         5000,       3000,       30000,      5000
);
WPVARM(0, timedelay, 0, VAR_MAX,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          75,         75,         0,
    0,          0,          10,         0,          0,          0,          75,         0,          0,          75,         75,         0
);
WPVARM(0, timeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          80,         0,          0,          0,          0
);
WPFVARM(0, trace, 0, FVAR_MAX,
    2.0f,       1.0f,       1.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    2.0f,       1.0f,       1.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(0, visfade, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(0, vistime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(0, wavepush, 0, FVAR_MAX,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       2.5f,       2.5f,       2.0f,       2.0f,       3.0f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       2.5f,       2.5f,       2.5f,       1.5f,       2.0f,       2.0f,       3.0f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       2.5f,       2.5f,       2.5f,       2.5f,       2.0f,       2.0f,       3.0f
);
WPFVARK(0, weight, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      150.0f,     0.0f
);

#define WRS(a,b,c,d)         ((a)*(m_sweaps(c, d) ? G(b##limited) : G(b##scale)))
#define WX(k,a,b,c,d,e,f)    (!m_insta(d, e) || (a) != W_RIFLE ? WRS(WF(k, a, b, c)*f, radial, d, e) : 0)
#define WSP(a,b,c,d,e)       (!m_insta(c, d) || (a) != W_RIFLE ? clamp(W2(a, spread, b)*(e), W2(a, spreadmin, b), W2(a, spreadmax, b) > 0 ? W2(a, spreadmax, b) : FVAR_MAX) : 0.f)
#define WSND(a,b)            (weaptype[a].sound+(b))
#define WSNDF(a,b)           (weaptype[a].sound+((b) ? S_W_SECONDARY : S_W_PRIMARY))
#define WSND2(a,b,c)         (weaptype[a].sound+((b) ? (c)+1 : (c)))
#define WUSE(a)              (a < W_ITEM ? W(a, ammomax) : W(a, ammoadd))
#define WHCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? W2(a, b, c) : game::hexpulsecolour(d, clamp(-1-W2(a, b, c), 0, int(PULSE_LAST)), 50))
#define WPCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? vec::hexcolor(W2(a, b, c)) : game::pulsecolour(d, clamp(-1-W2(a, b, c), 0, int(PULSE_LAST)), 50))

struct weaptypes
{
    int     anim,               sound,      espeed;
    bool    melee,      traced,     muzzle,     eject,      tape;
    float   thrown[2],              halo,       esize;
    const char *name,   *item,                      *vwep, *hwep,                     *proj,                  *eprj;
};
#ifdef GAMESERVER
weaptypes weaptype[] =
{
    {
            ANIM_MELEE,         S_MELEE,    1,
            true,       true,       false,      false,      false,
            { 0, 0 },               1,          0,
            "melee",    "", "", "", "", ""
    },
    {
            ANIM_PISTOL,        S_PISTOL,   10,
            false,      false,      true,       true,       false,
            { 0, 0 },               8,          0.35f,
            "pistol", "weapons/pistol/item", "weapons/pistol/vwep", "weapons/pistol/hwep", "weapons/pistol/proj", "projectiles/cartridge"
    },
    {
            ANIM_SWORD,         S_SWORD,    1,
            true,       true,       true,       false,      true,
            { 0, 0 },               14,         0,
            "sword", "weapons/sword/item", "weapons/sword/vwep", "weapons/sword/hwep", "", ""
    },
    {
            ANIM_SHOTGUN,       S_SHOTGUN,  10,
            false,      false,      true,       true,       false,
            { 0, 0 },               12,         0.45f,
            "shotgun", "weapons/shotgun/item", "weapons/shotgun/vwep", "weapons/shotgun/hwep", "", "projectiles/shell"
    },
    {
            ANIM_SMG,           S_SMG,      20,
            false,      false,      true,       true,       false,
            { 0, 0 },               10,         0.35f,
            "smg", "weapons/smg/item", "weapons/smg/vwep", "weapons/smg/hwep", "", "projectiles/cartridge"
    },
    {
            ANIM_FLAMER,        S_FLAMER,   1,
            false,      false,      true,       true,       false,
            { 0, 0 },               12,         0,
            "flamer", "weapons/flamer/item", "weapons/flamer/vwep", "weapons/flamer/hwep", "", ""
    },
    {
            ANIM_PLASMA,        S_PLASMA,   1,
            false,      false,      true,       false,      false,
            { 0, 0 },               8,         0,
            "plasma", "weapons/plasma/item", "weapons/plasma/vwep", "weapons/plasma/hwep", "", ""
    },
    {
            ANIM_ZAPPER,        S_ZAPPER,   1,
            false,      false,      true,       false,      true,
            { 0, 0 },               10,         0,
            "zapper", "weapons/zapper/item", "weapons/zapper/vwep", "weapons/zapper/hwep", "", ""
    },
    {
            ANIM_RIFLE,         S_RIFLE,    1,
            false,      false,      true,       false,      false,
            { 0, 0 },               12,         0,
            "rifle", "weapons/rifle/item", "weapons/rifle/vwep", "weapons/rifle/hwep", "", ""
    },
    {
            ANIM_GRENADE,       S_GRENADE,  1,
            false,      false,      false,      false,      false,
            { 0.0625f, 0.0625f },   6,          0,
            "grenade", "weapons/grenade/item", "weapons/grenade/vwep", "weapons/grenade/hwep", "weapons/grenade/proj", ""
    },
    {
            ANIM_MINE,          S_MINE,     1,
            false,      false,      false,      false,      false,
            { 0.0625f, 0.0625f },   6,          0,
            "mine", "weapons/mine/item", "weapons/mine/vwep", "weapons/mine/hwep", "weapons/mine/proj", ""
    },
    {
            ANIM_ROCKET,        S_ROCKET,   1,
            false,      false,      true,      false,       false,
            { 0, 0 },               10,          0,
            "rocket", "weapons/rocket/item", "weapons/rocket/vwep", "weapons/rocket/hwep", "weapons/rocket/proj",  ""
    }
};
SVAR(0, weapname, "melee pistol sword shotgun smg flamer plasma zapper rifle grenade mine rocket");
VAR(0, weapidxmelee, 1, W_MELEE, -1);
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
VAR(0, weapidxoffset, 1, W_OFFSET, -1);
VAR(0, weapidxitem, 1, W_ITEM, -1);
VAR(0, weapidxloadout, 1, W_LOADOUT, -1);
VAR(0, weapidxnum, 1, W_MAX, -1);
#else
extern weaptypes weaptype[];
#endif
