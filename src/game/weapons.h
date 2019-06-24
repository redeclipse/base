enum
{
    W_CLAW = 0, W_PISTOL, W_OFFSET, // end of unselectable weapon set
    W_SWORD = W_OFFSET, W_SHOTGUN, W_SMG, W_FLAMER, W_PLASMA, W_ZAPPER, W_RIFLE, W_ITEM,
    W_GRENADE = W_ITEM, W_MINE, W_ROCKET, W_ALL, // end of item weapon set
    W_MELEE = W_ALL, W_MAX, // if you add to this at all, check all arrays with W_MAX
    W_LOADOUT = W_ITEM-W_OFFSET,
    W_ITEMS = W_MINE-W_GRENADE+1,
    W_REPLACE = W_GRENADE
};
#define isweap(a)           (a >= 0 && a < W_MAX)
#define itemweap(a)         (a >= W_ITEM && a < W_ALL)

enum { W_F_NONE = 0, W_F_FORCED = 1<<0 };
enum {
    W_C_SCALE = 1<<0, W_C_SCALEN = 1<<1, W_C_LIFE = 1<<2, W_C_LIFEN = 1<<3,
    W_C_SPEED = 1<<4, W_C_SPEEDN = 1<<5, W_C_RAYS = 1<<6, W_C_ZOOM = 1<<7, W_C_KEEP = 1<<8,
    W_C_ALL = W_C_SCALE|W_C_SCALEN|W_C_LIFE|W_C_LIFEN|W_C_SPEED|W_C_SPEEDN|W_C_RAYS|W_C_ZOOM|W_C_KEEP,
    W_C_SS = W_C_SCALE|W_C_SPEED, W_C_SZ = W_C_SCALE|W_C_ZOOM|W_C_KEEP, W_C_ZK = W_C_ZOOM|W_C_KEEP
};
enum {
    W_N_STADD = 1<<0, W_N_GRADD = 1<<1, W_N_STIMM = 1<<2, W_N_GRIMM = 1<<3, W_N_SLIDE = 1<<4,
    W_N_ADD = W_N_STADD|W_N_GRADD, W_N_IMM = W_N_STIMM|W_N_GRIMM,
    W_N_ST = W_N_STADD|W_N_STIMM, W_N_GR = W_N_GRADD|W_N_GRIMM,
    W_N_AI = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM,
    W_N_ALL = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM|W_N_SLIDE
};
enum {
    W_S_IDLE = 0, W_S_PRIMARY, W_S_SECONDARY, W_S_RELOAD, W_S_POWER, W_S_ZOOM, W_S_SWITCH, W_S_USE, W_S_WAIT, W_S_MAX,
    W_S_ALL = (1<<W_S_IDLE)|(1<<W_S_PRIMARY)|(1<<W_S_SECONDARY)|(1<<W_S_RELOAD)|(1<<W_S_SWITCH)|(1<<W_S_USE)|(1<<W_S_POWER)|(1<<W_S_ZOOM)|(1<<W_S_WAIT),
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
    COLLIDE_GEOM = IMPACT_GEOM|BOUNCE_GEOM|STICK_GEOM,
    COLLIDE_PLAYER = IMPACT_PLAYER|BOUNCE_PLAYER|STICK_PLAYER,
    COLLIDE_SHOTS = IMPACT_SHOTS|BOUNCE_SHOTS,
    COLLIDE_DYNENT = COLLIDE_PLAYER|COLLIDE_SHOTS,
    COLLIDE_ALL = COLLIDE_TRACE|COLLIDE_PROJ|COLLIDE_OWNER|IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|BOUNCE_GEOM|BOUNCE_PLAYER|BOUNCE_SHOTS|DRILL_GEOM|DRILL_PLAYER|DRILL_SHOTS|STICK_GEOM|STICK_PLAYER|COLLIDE_HITSCAN
};

enum
{
    HIT_NONE = 0, HIT_HEAD = 1<<0, HIT_TORSO = 1<<1, HIT_LIMB = 1<<2, HIT_WHIPLASH = 1<<3, HIT_ALT = 1<<4,
    HIT_WAVE = 1<<5, HIT_PROJ = 1<<6, HIT_EXPLODE = 1<<7, HIT_BURN = 1<<8, HIT_BLEED = 1<<9, HIT_SHOCK = 1<<10,
    HIT_MATERIAL = 1<<11, HIT_SPAWN = 1<<12, HIT_LOST = 1<<13, HIT_KILL = 1<<14, HIT_FLAK = 1<<15, HIT_SPEC = 1<<16,
    HIT_CLEAR = HIT_PROJ|HIT_EXPLODE|HIT_BURN|HIT_BLEED|HIT_MATERIAL|HIT_SPAWN|HIT_LOST,
    HIT_SFLAGS = HIT_KILL
};

enum { W_R_BURN = 0, W_R_BLEED, W_R_SHOCK, W_R_MAX, W_R_ALL = (1<<W_R_BURN)|(1<<W_R_BLEED)|(1<<W_R_SHOCK) };

struct shotmsg { int id; ivec pos; };
struct hitmsg { int flags, proj, target, dist; ivec dir, vel; };

#define hithead(x)       (x&HIT_WHIPLASH || x&HIT_HEAD)
#define hitdealt(x)      (x&HIT_BURN || x&HIT_BLEED || x&HIT_SHOCK || x&HIT_EXPLODE || x&HIT_PROJ || x&HIT_MATERIAL)
#define WR(x)            (1<<(W_R_##x))
#define wr_burn(x,y)     (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BURN)))
#define wr_burns(x,y)    (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT_BURN) || wr_burn(x, y)))
#define wr_burning(x,y)  (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BURN) || wr_burn(x, y)))
#define wr_bleed(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BLEED)))
#define wr_bleeds(x,y)   (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_bleeding(x,y) (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_shock(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(SHOCK)))
#define wr_shocks(x,y)   (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT_SHOCK) || wr_shock(x, y)))
#define wr_shocking(x,y) (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_SHOCK) || wr_shock(x, y)))
#define WZ(x)            (W_MAX+(W_##x))

#include "weapdef.h"

WPSVAR(IDF_GAMEMOD, longname, // shown for weapon pickups and loadout menu
    "Claw Attack",
    "Sidearm Pistol",
    "Energy Sword",
    "Super Shotgun",
    "Submachine Gun",
    "Flame Thrower",
    "Plasma Inductor",
    "Electro Zapper",
    "Laser Rifle",
    "Frag Grenade",
    "Shock Mine",
    "Rocket Launcher",
    "Melee Attack"
);

WPSVARM(IDF_GAMEMOD, desc,
    "is a basic, horizontal claw slash, best used in conjunction with an impulse kick",
    "is a long-range, low-damage bullets with a high rate of fire",
    "is a horizontal slice, low-damage, can be swung at short intervals, causes bleed residual damage",
    "fires a lot of pellets, causes mass destruction at short range, useless at long range",
    "rapid-fires bullets that bounce off walls and knock back the victim",
    "is a short-ranged rapid-fire flamethrower, causes burn residual damage",
    "fires plasma balls at a high rate with high damage, but relatively low accuracy and velocity",
    "is a rapid-fire electric attack that hits almost instantly, great to finish off a wounded foe",
    "fires a non-charged laser shot that deals damage in a small area",
    "is a simple explosion device, can be cooked to control detonation delay, causes burn residual damage",
    "explodes and discharges electricity when someone comes too close, slowing them down greatly and causing shock residual damage",
    "is a fast, highly explosive rocket with a large shockwave, causes burn residual damage",
    "allows you to use parkour and kick moves as melee attacks",
    // begin secondary
    "is a slower and weaker claw attack that pulls you towards the enemy and allows wall grabs",
    "is a slower rate of fire than the primary attack, but does more damage",
    "is a slower vertical slice with much more power, causes bleed residual damage",
    "shoots a slug that explodes and disintegrates into shavings, causes bleed residual damage",
    "shoots long-range projectiles that stick to surfaces and players before exploding into fragments for additional damage",
    "is a versatile air blast, can be used after jumping to travel great distances or to blast foes off their feet. extinguishes flame residual damage",
    "is a charged, quickly-expanding ball of plasma that quickly sucks players in, causing great damage",
    "is a charged and focused beam that will also cause shock residual damage and greatly slow down a foe",
    "is a charged and focused laser shot with great precision and adjustable zoom, ideal for long range",
    "sticks to whatever it hits, but has a smaller area of effect than the primary mode, causes burn residual damage",
    "emits an endless distance laser trap that detonates when tripped, in addition causing shock residual damage which can slow down a foe",
    "is a guided rocket that moves significantly slower than the primary fire mode, causes burn residual damage",
    "allows you to use slide moves as melee attacks"
);

WPSVAR(IDF_GAMEMOD, name, // keep this incase longname is too long
    "Claw",     "Pistol",   "Sword",    "Shotgun",  "SMG",      "Flamer",   "Plasma",   "Zapper",   "Rifle",    "Grenade",  "Mine",     "Rocket",   "Melee"
);

WPFVARM(IDF_GAMEMOD, aidist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      64.0f,      256.0f,     64.0f,      512.0f,     768.0f,     768.0f,     384.0f,     128.0f,     1024.0f,    16.0f,
    16.0f,      256.0f,     48.0f,      128.0f,     128.0f,     64.0f,      64.0f,      768.0f,     2048.0f,    256.0f,     128.0f,     512.0f,     16.0f
);
WPVARM(IDF_GAMEMOD, aiskew, 0, VAR_MAX,
    1,          2,          1,          3,          5,          3,          5,          5,          5,          1,          1,          5,          1,
    1,          2,          1,          3,          5,          3,          5,          5,          5,          1,          1,          5,          1
);
WPVAR(IDF_GAMEMOD, ammoadd, 1, VAR_MAX,
    1,          10,         1,          2,          30,         50,         30,         30,         6,          1,          1,          1,          1
);
WPVAR(IDF_GAMEMOD, ammoclip, 1, VAR_MAX,
    1,          10,         1,          8,          30,         50,         30,         30,         6,          2,          2,          1,          1
);
WPVAR(IDF_GAMEMOD, ammoitem, 1, VAR_MAX,
    1,          10,         1,          20,         60,         100,        60,         60,         18,         1,          1,          1,          0
);
WPVAR(IDF_GAMEMOD, ammospawn, 1, VAR_MAX,
    1,          10,         1,          12,         90,         100,        60,         60,         12,         2,          2,          1,          1
);
WPVAR(IDF_GAMEMOD, ammostore, 0, VAR_MAX,
    0,          0,          0,          40,         150,        200,        120,        120,        30,         0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, ammosub, 0, VAR_MAX,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,
    0,          1,          0,          2,          3,          10,         30,         1,          2,          1,          1,          1,          0
);
WPVARK(IDF_GAMEMOD, bleedtime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, bleeddelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, bleeddamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPVARK(IDF_GAMEMOD, burntime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, burndelay, 1, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, burndamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPFVARK(IDF_GAMEMOD, blend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, collide, 0, COLLIDE_ALL,
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_HITSCAN,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_HITSCAN,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
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
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM|DRILL_PLAYER,
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
WPVARM(IDF_GAMEMOD, cooked, 0, W_C_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          W_C_LIFEN,  0,          0,          0,
    0,          0,          0,          0,          0,          0,          W_C_SS,     W_C_ZK,     W_C_SZ,     W_C_LIFEN,  0,          0,          0
);
WPVARM(IDF_GAMEMOD, cooktime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,          0,
    0,          0,          0,          0,          0,          0,          1000,       0,        1000,       3000,       0,          0,          0
);
WPVAR(IDF_GAMEMOD, cookzoom, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          200,        1000,       0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, cookzoommin, 1, 150,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
WPFVAR(IDF_GAMEMOD, cookzoommax, 1, 150,
    60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60
);
WPVARK(IDF_GAMEMOD, damage, VAR_MIN, VAR_MAX,
    400,        250,        350,        70,         170,        75,         170,        250,        350,        1000,       1000,       2000,       300,
    400,        350,        700,        70,         170,        100,        100,        250,        1500,       1000,       1000,       2000,       400,
    400,        250,        350,        70,         190,        75,         170,        150,        150,        1000,       1000,       2000,       300,
    400,        350,        700,        70,         65,         100,        100,        150,        150,        1000,       1000,       2000,       400
);
WPFVARK(IDF_GAMEMOD, damagehead, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.85f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, damagelimb, FVAR_MIN, FVAR_MAX,
    0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.7f,       0.2f,       0.3f,       0.3f,       0.3f,       0.3f,
    0.5f,       0.3f,       0.5f,       0.3f,       0.3f,       1.0f,       0.3f,       0.7f,       0.2f,       0.3f,       0.3f,       0.3f,       0.5f,
    0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.3f,       0.35f,      0.35f,      0.35f,
    0.5f,       0.35f,      0.5f,       0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.35f,      0.35f,      0.5f
);
WPFVARK(IDF_GAMEMOD, damageself, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, damageteam, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, damagetorso, FVAR_MIN, FVAR_MAX,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.7f,       0.4f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       1.5f,       0.6f,       0.7f,       0.4f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       1.5f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f
);
WPFVARK(IDF_GAMEMOD, damagepenalty, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, damagewhiplash, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f
);
WPVARM(IDF_GAMEMOD, delayattack, 1, VAR_MAX,
    500,        130,        500,        750,        55,         90,         350,        200,        750,        750,        500,        1500,       500,
    800,        200,        800,        1250,       350,        1250,       1800,       200,        1250,       750,        500,        1500,       500
);
WPVAR(IDF_GAMEMOD, delayreload, 0, VAR_MAX,
    50,         1000,       50,         900,        1300,       1800,       1800,       1800,       2000,       1000,       1500,       2500,       50
);
WPVAR(IDF_GAMEMOD, delayitem, 0, VAR_MAX,
    300,        400,        400,        600,        500,        600,        600,        500,        600,        500,        500,        700,        100
);
WPVAR(IDF_GAMEMOD, delayswitch, 0, VAR_MAX,
    100,        200,        200,        400,        300,        400,        400,        300,        400,        300,        300,        500,        100
);
WPVARK(IDF_GAMEMOD, destroyburn, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, destroybleed, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, destroyshock, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, disabled, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, drill, 0, VAR_MAX,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,          0,
    0,          0,          0,          0,          2,          0,          0,          2,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          8,          8,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, elasticity, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.75f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.75f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.75f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.75f,      0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, escapedelay, 0, VAR_MAX,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200
);
WPVARK(IDF_GAMEMOD|IDF_HEX, explcol, PC(LAST), 0xFFFFFF,
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE)
);
WPFVARK(IDF_GAMEMOD, explode, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       10.0f,      0.0f,       0.0f,       64.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      44.0f,      0.0f,       0.0f,       52.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       10.0f,      0.0f,       8.0f,       64.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      44.0f,      8.0f,       8.0f,       52.0f,      32.0f,      96.0f,      0.0f
);
WPVARK(IDF_GAMEMOD, extinguish, 0, 7,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0
);
WPVARK(IDF_GAMEMOD, fade, 0, 3,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, fadeat, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, fadecut, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPVARK(IDF_GAMEMOD, fadetime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, fragjump, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, fragoffset, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f
);
WPVARM(IDF_GAMEMOD, fragrays, 1, MAXPARAMS,
    5,          5,          5,          5,          5,          5,          5,          5,          5,          50,         35,         100,        5,
    5,          5,          5,          24,         40,         5,          5,          5,          5,          50,         10,         100,        5
);
WPFVARM(IDF_GAMEMOD, fragrel, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.5f,       0.1f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, fragscale, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, fragskew, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, fragspeed, 0, VAR_MAX,
    0,          0,          0,          0,          0,          200,        0,          0,          0,          150,        500,        300,        0,
    0,          0,          0,          0,          350,        250,        0,          0,          0,          150,        7500,       300,        0
);
WPFVARM(IDF_GAMEMOD, fragspeedmin, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPFVARM(IDF_GAMEMOD, fragspeedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, fragspread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       0.2f,       0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       0.2f,       0.75f,      0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.1f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, fragtime, 1, VAR_MAX,
    500,        500,        500,        250,        500,        1000,       500,        500,        500,        1000,       2500,       1500,       500,
    500,        500,        500,        5000,       750,        3000,       500,        500,        500,        1000,       5000,       1500,       500
);
WPVARM(IDF_GAMEMOD, fragtimedelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, fragtimeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          1,          1,          0,
    0,          0,          0,          0,          3,          0,          0,          0,          0,          1,          100,        1,          0
);
WPVARM(IDF_GAMEMOD, fragweap, -1, W_MAX*2-1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         W_SHOTGUN,  W_ZAPPER,   W_SMG,      -1,
    -1,         -1,         -1,         WZ(SHOTGUN),WZ(SMG),    -1,         -1,         -1,         -1,         W_SHOTGUN,  WZ(ZAPPER), W_SMG,      -1
);
WPVARM(IDF_GAMEMOD, fullauto, 0, 1,
    1,          0,          1,          0,          1,          1,          1,          1,          0,          0,          0,          0,          1,
    1,          0,          1,          0,          1,          0,          0,          1,          0,          0,          0,          0,          1
);
WPVARK(IDF_GAMEMOD, grab, 0, 3,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    3,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, guided, 0, 6,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0
);
WPVARK(IDF_GAMEMOD, guideddelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0
);
WPFVARK(IDF_GAMEMOD, headmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, hitpush, FVAR_MIN, FVAR_MAX,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       10.0f,      40.0f,      25.0f,      60.0f,      0.0f,       125.0f,     50.0f,
    100.0f,     20.0f,      50.0f,      15.0f,      10.0f,      25.0f,     -50.0f,      40.0f,      50.0f,      60.0f,      0.0f,       125.0f,     100.0f,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       10.0f,      10.0f,      10.0f,      60.0f,      0.0f,       125.0f,     50.0f,
    100.0f,     20.0f,      50.0f,      15.0f,      10.0f,      25.0f,     -50.0f,      10.0f,      10.0f,      60.0f,      0.0f,       125.0f,     100.0f
);
WPFVARK(IDF_GAMEMOD, hitvel, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, interacts, 0, 3,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0
);
WPFVAR(IDF_GAMEMOD, itemhalo, 0, FVAR_MAX,
    6,          6,          12,         10,         8,          10,         6,          8,          10,         4,          4,          8,          1
);
WPFVAR(IDF_GAMEMOD, itemhaloammo, 0, FVAR_MAX,
    4,          4,          4,          4,          4,          4,          4,          4,          4,          4,          4,          4,          1
);
WPFVARM(IDF_GAMEMOD, jitterpitchmax, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       10.0f,      6.0f,       0.2f,       5.0f,       3.0f,       4.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       20.0f,      3.5f,       0.5f,       15.0f,      3.0f,       5.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, jitterpitchmin, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       5.0f,       0.75f,      0.1f,       2.5f,       1.5f,       1.8f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       10.0f,      2.0f,       0.2f,       10.0f,      1.5f,       2.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, jitterpitchcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, jitterpitchzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, jitterpitchstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, jitterpitchmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, jitterpitchrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, jitterpitchinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, jitteryawmax, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, jitteryawmin, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       -1.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, jitteryawcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, jitteryawzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, jitteryawstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, jitteryawmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, jitteryawrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, jitteryawinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, jittertime, 0, MAXPARAMS,
    50,         13,         50,         75,         10,         10,         50,         50,         75,        75,         50,         150,        50,
    80,         20,         80,         125,        50,         125,        180,        50,        125,        75,         50,         150,        50
);
WPFVARM(IDF_GAMEMOD, jittertimecrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, jittertimezoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, jittertimestill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, jittertimemoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, jittertimerunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, jittertimeinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, kickpush, FVAR_MIN, FVAR_MAX,
    0.0f,       4.0f,       -15.0f,     50.0f,      5.0f,       1.0f,       20.0f,      10f,      35.0f,      5.0f,       5.0f,       150.0f,     0.0f,
    0.0f,       6.0f,       -30.0f,     75.0f,      25.0f,      50.0f,      150.0f,     10f,      50.0f,      5.0f,       5.0f,       150.0f,     0.0f
);
WPVAR(IDF_GAMEMOD, laser, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD|IDF_HEX, lightcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVAR(IDF_GAMEMOD, lightpersist, 0, 15,
    0,          0,          1,          0,          0,          1,          0,          1,          0,          8,          0,          6,          0
);
WPFVAR(IDF_GAMEMOD, lightradius, 0, FVAR_MAX,
    0,          16,         16,         16,         16,         16,         32,         8,          32,         32,         32,         32,         0
);
WPFVARK(IDF_GAMEMOD, liquidcoast, 0, FVAR_MAX,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f
);
WPVAR(IDF_GAMEMOD, modes, -G_ALL, G_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, muts, -G_M_ALL, G_M_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPSVARR(IDF_GAMEMOD, obitsuicide,
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
WPSVARR(IDF_GAMEMOD, obitobliterated,
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
WPSVARR(IDF_GAMEMOD, obitheadless,
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
WPSVARK(IDF_GAMEMOD, obituary,
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
    "electrocuted",
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
WPFVARK(IDF_GAMEMOD, partblend, 0, 1,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.6f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD|IDF_HEX, partcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVARK(IDF_GAMEMOD, partfade, 1, VAR_MAX,
    500,        250,        500,        250,        250,        200,        500,        100,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        100,        1000,       500,        500,        750,        500,
    500,        250,        500,        250,        250,        200,        500,        250,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        500,        1000,       500,        500,        750,        500
);
WPFVARK(IDF_GAMEMOD, partlen, 0, FVAR_MAX,
    0.0f,       8.0f,       0.0f,       30.0f,      20.0f,      0.0f,       0.0f,       128.0f,     256.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       15.0f,      15.0f,      0.0f,       0.0f,       128.0f,     1024.0f,    0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       8.0f,       0.0f,       15.0f,      15.0f,      0.0f,       0.0f,       1024.f,     256.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       15.0f,      15.0f,      0.0f,       0.0f,       1024.f,     512.0f,     0.0f,       4.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, partsize, 0, FVAR_MAX,
    1.0f,       0.125f,     1.0f,       0.75f,      0.6f,       6.0f,       8.0f,       2.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.5f,       0.75f,      12.0f,      28.0f,      2.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f,
    1.0f,       0.125f,     1.0f,       0.5f,       0.5f,       6.0f,       8.0f,       3.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.5f,       0.5f,       12.0f,      28.0f,      5.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, parttype, 0, W_MAX-1,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE
);
WPVARK(IDF_GAMEMOD, proxdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0
);
WPFVARK(IDF_GAMEMOD, proxdist, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, proxtime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, proxtype, 0, 2,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0
);
WPVARK(IDF_GAMEMOD, radial, 0, VAR_MAX,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, radius, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, rays, 0, MAXPARAMS,
    1,          1,          1,          25,         1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, reflectivity, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, relativity, 0, FVAR_MAX,
    0.0f,       0.05f,      0.0f,       0.15f,      0.15f,      0.95f,      0.15f,      0.0f,       0.5f,       0.75f,      0.5f,       0.0f,       0.0f,
    0.0f,       0.05f,      0.0f,       0.35f,      0.25f,      0.15f,      0.15f,      0.1f,       0.1f,       0.75f,      0.5f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, residual, 0, W_R_ALL,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0
);
WPVARK(IDF_GAMEMOD, residualundo, 0, W_R_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, shocktime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, shockdelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, shockdamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPVARK(IDF_GAMEMOD, shockstun, 0, W_N_ALL,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST
);
WPFVARK(IDF_GAMEMOD, shockstunscale, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPFVARK(IDF_GAMEMOD, shockstunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, shockstuntime, 0, VAR_MAX,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500
);
WPVAR(IDF_GAMEMOD, spawnstay, 0, VAR_MAX,
    30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000
);
WPVAR(IDF_GAMEMOD, spawntime, 0, VAR_MAX,
    15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      30000,      30000,      60000,      15000
);
WPVAR(IDF_GAMEMOD, spawnduke, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          60000,      0
);
WPVARM(IDF_GAMEMOD, speed, 0, VAR_MAX,
    0,          2500,       0,          1000,       4000,       500,        1250,       10000,      10000,      200,        100,        1000,       0,
    0,          1500,       0,          250,        500,        750,        85,         10000,      100000,     200,        100,        250,        0
);
WPFVARK(IDF_GAMEMOD, speeddelta, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPVARM(IDF_GAMEMOD, speedlimit, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          35,         0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, speedmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f
);
WPFVARK(IDF_GAMEMOD, speedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, spread, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       17.5f,      0.0f,       7.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, spreadmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, spreadmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, spreadz, 0, FVAR_MAX,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       4.0f,       2.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadcrouch, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadzoom, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadstill, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadmoving, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadrunning, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, spreadinair, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, stun, 0, W_N_ALL,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_AI,     W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD
);
WPFVARK(IDF_GAMEMOD, stunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.2f,       0.2f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, stunscale, 0, FVAR_MAX,
    0.5f,       0.125f,     1.0f,       0.125f,     0.25f,      0.0f,       0.1f,       0.1f,       0.3f,       1.0f,       4.0f,       8.0f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f ,     0.5f,       0.5f,       0.25f,      2.0f,       2.0f,       1.0f,       8.0f,       8.0f,       0.5f,
    0.5f ,      0.125f,     1.0f,       0.125f,     0.25f,      0.0f,       0.1f,       0.2f,       0.6f,       1.0f,       4.0f,       8.0f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f,      0.5f,       0.5f,       0.25f,      16.0f,      4.0f,       1.0f,       8.0f,       8.0f,       0.5f
);
WPVARK(IDF_GAMEMOD, stuntime, 0, VAR_MAX,
    250,        25,         300,        75,         75,         0,          0,          250,        100,        200,        500,        750,        100,
    500,        25,         500,        250,        250,        500,        200,        250,        250,        200,        750,        500,        200,
    250,        25,         300,        75,         75,         0,          0,          100,        500,        200,        500,        750,        100,
    500,        25,         500,        250,        75,         500,        200,        750,        750,        200,        750,        500,        200
);
WPVARK(IDF_GAMEMOD, taper, 0, 6,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, taperin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.05f,      0.025f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, taperout, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       0.0f,       0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       0.0f,       0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, tapermin, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, tapermax, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, time, 1, VAR_MAX,
    100,        2000,       350,        400,        800,        200,        350,        1000,        7500,       3000,       60000,      5000,       100,
    350,        1000,       500,        1000,       750,        200,        3500,       1000,        7500,       3000,       60000,      5000,       350
);
WPVARM(IDF_GAMEMOD, timedelay, 0, VAR_MAX,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          175,        50,         0,          0,
    0,          0,          10,         0,          0,          0,          75,         0,          0,          175,        50,         0,          0
);
WPVARM(IDF_GAMEMOD, timeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, timestick, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, trace, 0, FVAR_MAX,
    4.0f,       1.0f,       2.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       4.0f,
    4.0f,       1.0f,       2.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       4.0f
);
WPFVARK(IDF_GAMEMOD, visfade, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, vistime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, wavepush, 0, FVAR_MAX,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       2.0f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f
);
WPFVARK(IDF_GAMEMOD, weight, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -35.0f,     0.0f,       0.0f,       0.0f,       80.0f,      175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       300.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       80.0f,      175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       750.0f,     500.0f,     -35.0f,     0.0f,       0.0f,       0.0f,       80.0f,      175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       300.0f,     300.0f,     0.0f,       0.0f,       0.0f,       0.0f,       80.0f,      175.0f,     0.0f,       0.0f
);
// these are used in modifying values in gameent::configure() et al
//  claw        pistol      sword       shotgun     smg         flamer      plasma      zappper     rifle       grenades    mines       rocket      melee
WPFVAR(IDF_GAMEMOD, modhealth, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modhealthammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modhealthequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
// JUMP
WPFVAR(IDF_GAMEMOD, modjumpspeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modjumpspeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modjumpspeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, modjumpspeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modjumpspeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modjumpspeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modjumpspeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modjumpspeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -5,         0
);
WPFVAR(IDF_GAMEMOD, modjumpspeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
// IMPULSE
WPFVAR(IDF_GAMEMOD, modimpulsespeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, modimpulsespeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modimpulsespeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -5,         0
);
WPFVAR(IDF_GAMEMOD, modimpulsespeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
// SPEED
WPFVAR(IDF_GAMEMOD, modspeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          -10,        -5,         -5,         -15,        -10,        -20,        0,          0,          -25,        0
);
WPFVAR(IDF_GAMEMOD, modspeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          -2,         -2,         0,          0
);
WPFVAR(IDF_GAMEMOD, modspeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, modspeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -10,        -15,        -5,         -5,         -10,        -15,        -5,         -5,         -15,        -5,
    -5,         -5,         -5,         -15,        -15,        -5,         -10,        -15,        -20,        -5,         -5,         -15,        -5
);
WPFVAR(IDF_GAMEMOD, modspeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -10,        -5,         -10,        -5,         -10,        -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, modspeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modspeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -10,        -5
);
WPFVAR(IDF_GAMEMOD, modspeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -3,         -5,         -5,         -10,        0
);
WPFVAR(IDF_GAMEMOD, modspeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -10,        -15,        -5,         -5,         -10,        -5
);
// WEIGHT
WPFVAR(IDF_GAMEMOD, modweight, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          5,          3,          6,          8,          10,         12,         0,          0,          15,         0
);
WPFVAR(IDF_GAMEMOD, modweightammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3,          3,          0,          0
);
WPFVARM(IDF_GAMEMOD, modweightattack, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightequip, FVAR_MIN, FVAR_MAX,
    0,          0,          -10,        0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightreload, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightpower, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightuse, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, modweightzoom, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);

#define WRS(a,b,c,d)         ((a)*(m_dm_gladiator(c, d) ? G(gladiator##b##scale) : G(b##scale)*(m_sweaps(c, d) ? G(b##limited) : 1.f)))
#define WX(k,a,b,c,d,e,f)    (!m_insta(d, e) || (a) != W_RIFLE ? WRS(WF(k, a, b, c)*f, radial, d, e) : 0)
#define WSP(a,b,c,d,e)       (!m_insta(c, d) || (a) != W_RIFLE ? clamp(W2(a, spread, b)*(e), W2(a, spreadmin, b), W2(a, spreadmax, b) > 0 ? W2(a, spreadmax, b) : FVAR_MAX) : 0.f)
#define WSND(a,b)            (weaptype[a].sound+(b))
#define WSNDF(a,b)           (weaptype[a].sound+((b) ? S_W_SECONDARY : S_W_PRIMARY))
#define WSND2(a,b,c)         (weaptype[a].sound+((b) ? (c)+1 : (c)))
#define WHCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? W2(a, b, c) : game::pulsehexcol(d, clamp(INVPULSE(W2(a, b, c)), 0, int(PULSE_LAST)), 50))
#define WPCOL(d,a,b,c)       (W2(a, b, c) >= 0 ? vec::fromcolor(W2(a, b, c)) : game::pulsecolour(d, clamp(INVPULSE(W2(a, b, c)), 0, int(PULSE_LAST)), 50))

struct weaptypes
{
    int     anim,               sound,          espeed;
    bool    muzzle,     eject,      tape;
    float   thrown[2],              esize;
    const char *name, *item, *ammo, *vwep, *hwep, *proj, *eprj;
};
#ifdef GAMESERVER
weaptypes weaptype[] =
{
    {
            ANIM_CLAW,         S_CLAW,          1,
            true,       false,      true,
            { 0, 0 },               0,
            "claw", "", "", "weapons/claw/vwep", "weapons/claw/hwep", "", ""
    },
    {
            ANIM_PISTOL,        S_PISTOL,       10,
            true,       true,       false,
            { 0, 0 },               0.35f,
            "pistol", "weapons/pistol/item", "weapons/pistol/ammo", "weapons/pistol/vwep", "weapons/pistol/hwep", "weapons/pistol/proj", "projectiles/cartridge"
    },
    {
            ANIM_SWORD,         S_SWORD,        1,
            true,       false,      true,
            { 0, 0 },               0,
            "sword", "weapons/sword/item", "weapons/sword/ammo", "weapons/sword/vwep", "weapons/sword/hwep", "", ""
    },
    {
            ANIM_SHOTGUN,       S_SHOTGUN,      10,
            true,       true,       false,
            { 0, 0 },               0.45f,
            "shotgun", "weapons/shotgun/item", "weapons/shotgun/ammo", "weapons/shotgun/vwep", "weapons/shotgun/hwep", "", "projectiles/shell"
    },
    {
            ANIM_SMG,           S_SMG,          20,
            true,       true,       false,
            { 0, 0 },               0.35f,
            "smg", "weapons/smg/item", "weapons/smg/ammo", "weapons/smg/vwep", "weapons/smg/hwep", "", "projectiles/cartridge"
    },
    {
            ANIM_FLAMER,        S_FLAMER,       1,
            true,       true,       false,
            { 0, 0 },               0,
            "flamer", "weapons/flamer/item", "weapons/flamer/ammo", "weapons/flamer/vwep", "weapons/flamer/hwep", "", ""
    },
    {
            ANIM_PLASMA,        S_PLASMA,       1,
            true,       false,      false,
            { 0, 0 },               0,
            "plasma", "weapons/plasma/item", "weapons/plasma/ammo", "weapons/plasma/vwep", "weapons/plasma/hwep", "", ""
    },
    {
            ANIM_ZAPPER,        S_ZAPPER,       1,
            true,       false,      true,
            { 0, 0 },               0,
            "zapper", "weapons/zapper/item", "weapons/zapper/ammo", "weapons/zapper/vwep", "weapons/zapper/hwep", "", ""
    },
    {
            ANIM_RIFLE,         S_RIFLE,        1,
            true,       false,      false,
            { 0, 0 },               0,
            "rifle", "weapons/rifle/item", "weapons/rifle/ammo", "weapons/rifle/vwep", "weapons/rifle/hwep", "", ""
    },
    {
            ANIM_GRENADE,       S_GRENADE,      1,
            false,      false,      false,
            { 0.0625f, 0.0625f },   0,
            "grenade", "weapons/grenade/item", "weapons/grenade/ammo", "weapons/grenade/vwep", "weapons/grenade/hwep", "weapons/grenade/proj", ""
    },
    {
            ANIM_MINE,          S_MINE,         1,
            false,      false,      false,
            { 0.0625f, 0.0625f },   0,
            "mine", "weapons/mine/item", "weapons/mine/ammo", "weapons/mine/vwep", "weapons/mine/hwep", "weapons/mine/proj", ""
    },
    {
            ANIM_ROCKET,        S_ROCKET,       1,
            true,      false,       false,
            { 0, 0 },               0,
            "rocket", "weapons/rocket/item", "weapons/rocket/ammo", "weapons/rocket/vwep", "weapons/rocket/hwep", "weapons/rocket/proj",  ""
    },
    {
            0,                  S_MELEE,        1,
            false,      false,      false,
            { 0, 0 },               0,
            "melee",    "", "", "", "", "", ""
    }
};
SVAR(IDF_READONLY, weapname, "claw pistol sword shotgun smg flamer plasma zapper rifle grenade mine rocket melee");
VAR(IDF_READONLY, weapidxclaw, 1, W_CLAW, -1);
VAR(IDF_READONLY, weapidxpistol, 1, W_PISTOL, -1);
VAR(IDF_READONLY, weapidxsword, 1, W_SWORD, -1);
VAR(IDF_READONLY, weapidxshotgun, 1, W_SHOTGUN, -1);
VAR(IDF_READONLY, weapidxsmg, 1, W_SMG, -1);
VAR(IDF_READONLY, weapidxflamer, 1, W_FLAMER, -1);
VAR(IDF_READONLY, weapidxplasma, 1, W_PLASMA, -1);
VAR(IDF_READONLY, weapidxzapper, 1, W_ZAPPER, -1);
VAR(IDF_READONLY, weapidxrifle, 1, W_RIFLE, -1);
VAR(IDF_READONLY, weapidxgrenade, 1, W_GRENADE, -1);
VAR(IDF_READONLY, weapidxmine, 1, W_MINE, -1);
VAR(IDF_READONLY, weapidxrocket, 1, W_ROCKET, -1);
VAR(IDF_READONLY, weapidxmelee, 1, W_MELEE, -1);
VAR(IDF_READONLY, weapidxoffset, 1, W_OFFSET, -1);
VAR(IDF_READONLY, weapidxitem, 1, W_ITEM, -1);
VAR(IDF_READONLY, weapidxhidden, 1, W_ALL, -1);
VAR(IDF_READONLY, weapidxloadout, 1, W_LOADOUT, -1);
VAR(IDF_READONLY, weapidxnum, 1, W_MAX, -1);
VAR(IDF_READONLY, weapstateidle, 1, W_S_IDLE, -1);
VAR(IDF_READONLY, weapstateprimary, 1, W_S_PRIMARY, -1);
VAR(IDF_READONLY, weapstatesecondary, 1, W_S_SECONDARY, -1);
VAR(IDF_READONLY, weapstatereload, 1, W_S_RELOAD, -1);
VAR(IDF_READONLY, weapstateswitch, 1, W_S_SWITCH, -1);
VAR(IDF_READONLY, weapstateuse, 1, W_S_USE, -1);
VAR(IDF_READONLY, weapstatepower, 1, W_S_POWER, -1);
VAR(IDF_READONLY, weapstatezoom, 1, W_S_ZOOM, -1);
VAR(IDF_READONLY, weapstatewait, 1, W_S_WAIT, -1);
VAR(IDF_READONLY, weapstatemax, 1, W_S_MAX, -1);
VAR(IDF_READONLY, weapstateexclude, 1, W_S_EXCLUDE, -1);
VAR(IDF_READONLY, weapresidualburn, 1, W_R_BURN, -1);
VAR(IDF_READONLY, weapresidualbleed, 1, W_R_BLEED, -1);
VAR(IDF_READONLY, weapresidualshock, 1, W_R_SHOCK, -1);
VAR(IDF_READONLY, weapresidualmax, 1, W_R_MAX, -1);
VAR(IDF_READONLY, weapcarrylimit, 1, W_LOADOUT, -1);
#else
extern weaptypes weaptype[];
#endif
