//weapons.h configuration file
//default weapon variables baked into the game
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
#define WEAPCARRY 2
#define WZ(x) (W_MAX+(W_##x))

#define isweap(a)       (a >= 0 && a < W_MAX)
#define itemweap(a)     (a >= W_ITEM && a < W_ALL)
#define w_carry(w1,w2)  (isweap(w1) && w1 != W_CLAW && w1 < W_ALL && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE))) && (w1 == W_ROCKET || (w1 >= W_OFFSET && w1 < W_ITEM)))

enum { W_F_NONE = 0, W_F_FORCED = 1<<0 };

enum
{
    W_C_SCALE = 1<<0, W_C_SCALEN = 1<<1, W_C_LIFE = 1<<2, W_C_LIFEN = 1<<3,
    W_C_SPEED = 1<<4, W_C_SPEEDN = 1<<5, W_C_RAYS = 1<<6, W_C_ZOOM = 1<<7, W_C_KEEP = 1<<8,
    W_C_ALL = W_C_SCALE|W_C_SCALEN|W_C_LIFE|W_C_LIFEN|W_C_SPEED|W_C_SPEEDN|W_C_RAYS|W_C_ZOOM|W_C_KEEP,
    W_C_SS = W_C_SCALE|W_C_SPEED, W_C_SZ = W_C_SCALE|W_C_ZOOM|W_C_KEEP, W_C_ZK = W_C_ZOOM|W_C_KEEP
};

enum
{
    W_N_STADD = 1<<0, W_N_GRADD = 1<<1, W_N_STIMM = 1<<2, W_N_GRIMM = 1<<3, W_N_SLIDE = 1<<4,
    W_N_ADD = W_N_STADD|W_N_GRADD, W_N_IMM = W_N_STIMM|W_N_GRIMM,
    W_N_ST = W_N_STADD|W_N_STIMM, W_N_GR = W_N_GRADD|W_N_GRIMM,
    W_N_AI = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM,
    W_N_ALL = W_N_STADD|W_N_GRADD|W_N_STIMM|W_N_GRIMM|W_N_SLIDE
};

enum
{
    W_S_IDLE = 0, W_S_PRIMARY, W_S_SECONDARY, W_S_RELOAD, W_S_POWER, W_S_ZOOM, W_S_SWITCH, W_S_USE, W_S_WAIT, W_S_MAX,
    W_S_ALL = (1<<W_S_IDLE)|(1<<W_S_PRIMARY)|(1<<W_S_SECONDARY)|(1<<W_S_RELOAD)|(1<<W_S_SWITCH)|(1<<W_S_USE)|(1<<W_S_POWER)|(1<<W_S_ZOOM)|(1<<W_S_WAIT),
    W_S_INTERRUPT = (1<<W_S_POWER)|(1<<W_S_ZOOM)
};

enum { W_A_CLIP = 0, W_A_STORE, W_A_MAX };

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
    COLLIDE_TRACE = 1<<0, COLLIDE_SCAN = 1<<1, COLLIDE_LENGTH = 1<<2, COLLIDE_PROJ = 1<<3, COLLIDE_OWNER = 1<<4,
    IMPACT_GEOM = 1<<5, IMPACT_PLAYER = 1<<6, IMPACT_SHOTS = 1<<7, BOUNCE_GEOM = 1<<8, BOUNCE_PLAYER = 1<<9, BOUNCE_SHOTS = 1<<10,
    DRILL_GEOM = 1<<11, DRILL_PLAYER = 1<<12, DRILL_SHOTS = 1<<13, STICK_GEOM = 1<<14, STICK_PLAYER = 1<<15,
    COLLIDE_GEOM = IMPACT_GEOM|BOUNCE_GEOM|STICK_GEOM, COLLIDE_PLAYER = IMPACT_PLAYER|BOUNCE_PLAYER|STICK_PLAYER,
    COLLIDE_SHOTS = IMPACT_SHOTS|BOUNCE_SHOTS, COLLIDE_DYNENT = COLLIDE_PLAYER|COLLIDE_SHOTS,
    COLLIDE_FROMTO = COLLIDE_TRACE|COLLIDE_SCAN, COLLIDE_SCLEN = COLLIDE_SCAN|COLLIDE_LENGTH,
    COLLIDE_ALL = COLLIDE_TRACE|COLLIDE_SCAN|COLLIDE_LENGTH|COLLIDE_PROJ|COLLIDE_OWNER|IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|BOUNCE_GEOM|BOUNCE_PLAYER|BOUNCE_SHOTS|DRILL_GEOM|DRILL_PLAYER|DRILL_SHOTS|STICK_GEOM|STICK_PLAYER
};

#define HIT(x) (1<<(HIT_##x))
enum
{
    HIT_HEAD = 0, HIT_TORSO, HIT_LIMB, HIT_WHIPLASH, HIT_ALT,
    HIT_WAVE, HIT_PROJ, HIT_EXPLODE, HIT_BURN, HIT_BLEED, HIT_SHOCK,
    HIT_MATERIAL, HIT_SPAWN, HIT_LOST, HIT_KILL, HIT_FLAK, HIT_SPEC,
    HIT_CLEAR = HIT(PROJ)|HIT(EXPLODE)|HIT(BURN)|HIT(BLEED)|HIT(MATERIAL)|HIT(SPAWN)|HIT(LOST),
    HIT_SFLAGS = HIT(KILL)
};

#define WR(x) (1<<(W_R_##x))
enum { W_R_BURN = 0, W_R_BLEED, W_R_SHOCK, W_R_MAX, W_R_ALL = WR(BURN)|WR(BLEED)|WR(SHOCK) };

struct shotmsg { int id; ivec pos; };
struct hitmsg { int flags, proj, target, dist; ivec dir, vel; };

#define hithead(x)       (x&HIT(WHIPLASH) || x&HIT(HEAD))
#define hitdealt(x)      (x&HIT(BURN) || x&HIT(BLEED) || x&HIT(SHOCK) || x&HIT(EXPLODE) || x&HIT(PROJ) || x&HIT(MATERIAL))
#define wr_burn(x,y)     (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BURN)))
#define wr_burns(x,y)    (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT(BURN)) || wr_burn(x, y)))
#define wr_burning(x,y)  (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT(MATERIAL) && y&HIT(BURN)) || wr_burn(x, y)))
#define wr_bleed(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(BLEED)))
#define wr_bleeds(x,y)   (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT(BLEED)) || wr_bleed(x, y)))
#define wr_bleeding(x,y) (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT(MATERIAL) && y&HIT(BLEED)) || wr_bleed(x, y)))
#define wr_shock(x,y)    (isweap(x) && (WF(WK(y), x, residual, WS(y))&WR(SHOCK)))
#define wr_shocks(x,y)   (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT(SHOCK)) || wr_shock(x, y)))
#define wr_shocking(x,y) (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT(MATERIAL) && y&HIT(SHOCK)) || wr_shock(x, y)))

#include "weapdef.h"

WPSVAR(IDF_GAMEMOD, 0, longname, // shown for weapon pickups and loadout menu
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
//Strings formatted as "The <weapon> <primary/secondary> <string>" implied
//e.g. The sword primary is a horizontally slicing moderately powerful slash, capable of being swung at short intervals
WPSVARM(IDF_GAMEMOD, 0, desc,
    "is a basic, horizontal claw slash, best used in conjunction with an impulse kick",
    "is an accurate, low-damage, and moderately fast projectile attack with a high rate of fire",
    "is a horizontally slicing, moderately powerful slash, capable of being swung at short intervals",
    "fires a conical spray of projectiles, causing high damage at short range, but none from afar",
    "rapid-fires bullets that bounce off walls and knock back the victim, but at the cost of high muzzle climb",
    "fires a short-ranged rapid-fire flame stream, causing burn residual damage",
    "fires plasma balls at a high rate with high damage, but relatively low accuracy and velocity",
    "is a long ranged, medium damage, and fairly quick firing ray weapon capable of holding off opponents at medium to long range",
    "fires an uncharged laser shot that deals a reasonably large amount of damage at a fairly slow rate",
    "is a thrown explosive with a burn residual, capable of having its time to detination shortened by being held after being cocked",
    "explodes and discharges electric rays when someone comes too close, slowing them down and causing shock residual damage",
    "is a fast, highly explosive rocket with a large shockwave, causing burn residual damage",
    "allows you to use parkour and kick moves as melee attacks",
    // begin secondary
    "is a slower and weaker claw attack that pulls you towards the enemy and allows wall grabs",
    "bludgeons opponents with the weapon itself, capable of being swung quickly but not capable of significant damage",
    "is a slower, charged vertical slice capable of powerful bleed-inducing swings",
    "shoots a slug that disintegrates on contact or delay into shrapnel, causing bleed residual damage",
    "shoots medium-range projectiles that can stick to surfaces and players before exploding and releasing shrapnel for additional damage",
    "releases an air blast, capable of extending one's own airtime or knocking back opponents; capable of extinguishing burning players",
    "is a charged, slow moving spray of plasma chunks that splits on impact with surfaces and renders surfaces untraversible for a time",
    "is a scoped, long ranged, medium damage, and fairly quick firing ray weapon capable of accurately hitting opponents at long range",
    "is a charged and focused laser shot with great precision and adjustable zoom, ideal for long range",
    "is a thrown explosive that sticks to opponents, capable of having its time to detination shortened by being held; burns opponents",
    "emits an long distance laser trap that detonates when tripped, damaging and slowing the victim with a stun residual",
    "is a guided rocket that moves significantly slower than the primary fire mode, causing burn residual damage",
    "allows you to use slide moves as melee attacks"
);

WPSVAR(IDF_GAMEMOD, 0, name, // keep this incase longname is too long
    "Claw",     "Pistol",   "Sword",    "Shotgun",  "SMG",      "Flamer",   "Plasma",   "Zapper",   "Rifle",    "Grenade",  "Mine",     "Rocket",   "Melee"
);

WPFVARM(IDF_GAMEMOD, 0, aidist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      128.0f,     320.0f,     64.0f,      512.0f,     512.0f,     512.0f,     384.0f,     128.0f,     1024.0f,    16.0f,
    16.0f,      40.0f,      48.0f,      256.0f,     320.0f,     32.0f,      128.0f,     1024.0f,    2048.0f,    128.0f,     128.0f,     512.0f,     16.0f
);
WPVARM(IDF_GAMEMOD, 0, aiskew, 0, VAR_MAX,
    1,          5,          1,          3,          5,          3,          5,          5,          5,          1,          1,          5,          1,
    1,          1,          1,          3,          5,          3,          5,          5,          5,          1,          1,          5,          1
);
WPVAR(IDF_GAMEMOD, 0, ammoadd, 1, VAR_MAX,
    1,          10,         1,          2,          30,         50,         16,         24,         6,          1,          1,          1,          1
);
WPVAR(IDF_GAMEMOD, 0, ammoclip, 1, VAR_MAX,
    1,          10,         1,          8,          30,         50,         16,         24,         6,          2,          2,          1,          1
);
WPVAR(IDF_GAMEMOD, 0, ammoitem, 1, VAR_MAX,
    1,          10,         1,          12,         60,         100,        32,         48,         18,         1,          1,          1,          0
);
WPVAR(IDF_GAMEMOD, 0, ammospawn, 1, VAR_MAX,
    1,          10,         1,          12,         90,         100,        32,         48,         12,         2,          2,          1,          1
);
WPVAR(IDF_GAMEMOD, 0, ammostore, -1, VAR_MAX,
    -1,         -1,         -1,         40,         150,        200,        64,         96,         30,         0,          0,          0,          -1
);
WPVARM(IDF_GAMEMOD, 0, ammosub, 0, VAR_MAX,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,
    0,          0,          0,          2,          3,          10,         8,          1,          2,          1,          1,          1,          0
);
WPVARK(IDF_GAMEMOD, 0, bleedtime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, bleeddelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, bleeddamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPVARK(IDF_GAMEMOD, 0, burntime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, burndelay, 1, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, burndamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPFVARK(IDF_GAMEMOD, 0, blend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, collide, 0, COLLIDE_ALL,
    //primary
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_SCAN|STICK_GEOM,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM,
    BOUNCE_GEOM|BOUNCE_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,
    //secondary
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_SHOTS|COLLIDE_OWNER|BOUNCE_GEOM|BOUNCE_PLAYER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_SCAN|STICK_GEOM|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM|DRILL_PLAYER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,
    //primary flak
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_SCAN|STICK_GEOM,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    //secondary flak
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_SCAN|STICK_GEOM|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPVAR(IDF_GAMEMOD|IDF_HEX, 0, colour, 0, 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF02020,   0x40F0C8,   0xC090F0,   0xA020F0,   0x40F000,   0x00F068,   0x803000,   0x606060
);
WPVARM(IDF_GAMEMOD|IDF_HEX, 0, colourcook, -1, 0xFFFFFF,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         0xFF0000,   -1,         -1,         -1,
    -1,         -1,         0xFF00FF,   -1,         -1,         -1,         0x00FFFF,   -1,         -1,         0xFF0000,   -1,         -1,         -1
);
WPVARM(IDF_GAMEMOD|IDF_HEX, 0, colourproj, -1, 0xFFFFFF,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         0xFF0000,   -1,         -1,         -1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         0xFF0000,   -1,         -1,         -1
);
WPVARM(IDF_GAMEMOD, 0, cooked, 0, W_C_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          W_C_LIFEN,  0,          0,          0,
    0,          0,          W_C_SCALE,  0,          0,          0,          W_C_RAYS,   W_C_ZK,     W_C_SZ,     W_C_LIFEN,  0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, cookinterrupt, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, cooktime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          2500,       0,          0,          0,
    0,          0,          800,        0,          0,          0,          1000,       5,          1000,       2500,       0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, cookzoom, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          200,        450,        0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, cookzoommin, 1, 150,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
WPFVAR(IDF_GAMEMOD, 0, cookzoommax, 1, 150,
    60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60
);
WPVARK(IDF_GAMEMOD, 0, damage, VAR_MIN, VAR_MAX,
    400,        200,        500,        60,         120,        45,         220,        250,        400,        1600,       1300,       2000,       300,
    400,        300,        1200,        0,         130,        120,        45,         250,        1500,       1100,       1300,       2000,       400,
    400,        200,        500,        70,         190,        45,         220,        150,        150,        1600,       1300,       2000,       300,
    400,        300,        1200,       50,         95,         120,        75,         150,        150,        1100,       1300,       2000,       400
);
WPFVARK(IDF_GAMEMOD, 0, damagehead, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.85f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, damagelimb, FVAR_MIN, FVAR_MAX,
    0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.6f,       0.5f,       0.3f,       0.3f,       0.3f,       0.3f,
    0.5f,       0.3f,       0.5f,       0.3f,       0.3f,       1.0f,       0.3f,       0.6f,       0.2f,       0.3f,       0.3f,       0.3f,       0.5f,
    0.35f,      0.35f,      0.35f,      0.5f,       0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.3f,       0.35f,      0.35f,      0.35f,
    0.5f,       0.35f,      0.5f,       0.5f,       0.5f,       0.35f,      0.35f,      0.35f,      0.35f,      0.3f,       0.35f,      0.35f,      0.5f
);
WPFVARK(IDF_GAMEMOD, 0, damageself, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, damageteam, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, damagetorso, FVAR_MIN, FVAR_MAX,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       1.5f,       0.6f,       0.6f,       0.48f,      0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.5f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,
    0.6f,       0.6f,       0.6f,       0.5f,       0.6f,       1.5f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f
);
WPFVARK(IDF_GAMEMOD, 0, damagepenalty, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, damagewhiplash, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.6f,       0.8f
);
WPVARM(IDF_GAMEMOD, 0, delayattack, 1, VAR_MAX,
    500,        200,        500,        900,        55,         20,         350,        250,        750,        750,        500,        1500,       1000,
    800,        300,        400,        1350,       350,        750,        1000,       245,        1350,       750,        500,        1500,       1000
);
WPVAR(IDF_GAMEMOD, 0, delayreload, 0, VAR_MAX,
    50,         1000,       50,         1000,       1300,       2000,       1800,       1800,       2000,       1000,       1500,       2500,       50
);
WPVAR(IDF_GAMEMOD, 0, delayitem, 0, VAR_MAX,
    300,        400,        400,        600,        500,        600,        600,        500,        600,        500,        500,        700,        100
);
WPVAR(IDF_GAMEMOD, 0, delayswitch, 0, VAR_MAX,
    100,        200,        200,        400,        300,        400,        400,        300,        400,        300,        300,        500,        100
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPVARK(IDF_GAMEMOD, 0, destroyburn, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, destroybleed, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, destroyshock, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, disabled, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, drill, 0, VAR_MAX,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,          0,
    0,          0,          0,          0,          2,          0,          0,          2,          2,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          8,          8,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, elasticity, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.15f,      0.5f,       0.5f,       0.5f,       0.5f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.3f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.35f,      0.5f,       0.5f,       0.5f,       0.5f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.65f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,      0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, 0, escapedelay, 0, VAR_MAX,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, explcol, PC(LAST), 0xFFFFFF,
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x4040F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   PC(SHOCK),  PC(FIRE),   PC(FIRE)
);
WPFVARK(IDF_GAMEMOD, 0, explode, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       10.0f,      0.0f,       0.0f,       72.0f,      54.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      8.0f,       0.0f,       0.0f,       60.0f,      32.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       6.0f,       10.0f,      0.0f,       8.0f,       72.0f,      54.0f,      96.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       12.0f,      8.0f,       8.0f,       8.0f,       60.0f,      32.0f,      96.0f,      0.0f
);
WPVARK(IDF_GAMEMOD, 0, extinguish, 0, 7,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          0
);
WPVARK(IDF_GAMEMOD, 0, fade, 0, 3,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, 0, fadeat, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, fadecut, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPVARK(IDF_GAMEMOD, 0, fadein, 0, VAR_MAX,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, fadeout, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          75,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          75,         0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          150,        0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPFVARM(IDF_GAMEMOD, 0, fragjump, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragoffset, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       1.0f,       1.0f,       4.0f,       1.0f,       2.0f,       4.0f
);
WPVARM(IDF_GAMEMOD, 0, fragrays, 1, MAXPARAMS,
    5,          5,          5,          5,          5,          5,          5,          5,          5,          50,         35,         100,        5,
    5,          5,          5,          28,         40,         5,          2,          5,          5,          50,         1,          100,        5
);
WPFVARM(IDF_GAMEMOD, 0, fragrel, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       0.1f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragscale, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragskew, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, fragspeed, 0, VAR_MAX,
    0,          0,          0,          0,          0,          200,        0,          0,          0,          150,        500,        300,        0,
    0,          0,          0,          0,          350,        250,        50,         0,          0,          150,        7500,       300,        0
);
WPFVARM(IDF_GAMEMOD, 0, fragspeedmin, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragspeedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragspread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       0.2f,       0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.5f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       0.2f,       0.75f,      0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       0.1f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, fragtime, 1, VAR_MAX,
    500,        500,        500,        250,        500,        1000,       500,        500,        500,        1000,       2000,       1500,       500,
    500,        500,        500,        1800,       250,        3000,       3400,       500,        500,        1000,       2000,       1500,       500
);
WPVARM(IDF_GAMEMOD, 0, fragtimedelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, fragtimeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, fragweap, -1, W_MAX*2-1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         W_SHOTGUN,  W_ZAPPER,   W_SMG,      -1,
    -1,         -1,         -1,         WZ(SHOTGUN),WZ(SMG),    -1,         WZ(PLASMA), -1,         -1,         W_SHOTGUN,  WZ(ZAPPER), W_SMG,      -1
);
WPVARM(IDF_GAMEMOD, 0, fullauto, 0, 1,
    1,          0,          1,          0,          1,          1,          1,          1,          0,          0,          0,          0,          1,
    1,          0,          0,          0,          1,          0,          0,          1,          0,          0,          0,          0,          1
);
WPVARK(IDF_GAMEMOD, 0, grab, 0, 3,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    3,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, guided, 0, 6,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0
);
WPVARK(IDF_GAMEMOD, 0, guideddelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0
);
WPFVARK(IDF_GAMEMOD, 0, headmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       8.0f,       8.0f,       8.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, hitpush, FVAR_MIN, FVAR_MAX,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       10.0f,      40.0f,      0.0f,       60.0f,      0.0f,       125.0f,     50.0f,
    100.0f,     40.0f,      50.0f,      15.0f,      150.0f,     25.0f,      0.0f,       40.0f,      0.0f,       60.0f,      0.0f,       125.0f,     100.0f,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       10.0f,      10.0f,      10.0f,      60.0f,      0.0f,       125.0f,     50.0f,
    100.0f,     40.0f,      50.0f,      15.0f,      50.0f,      25.0f,      0.0f,       10.0f,      10.0f,      60.0f,      0.0f,       125.0f,     100.0f
);
WPFVARK(IDF_GAMEMOD, 0, hitpushself, 0, FVAR_MAX,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          2,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          2,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, 0, hitvel, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, interacts, 0, 3,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          3,          3,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          1,          0
);
WPFVAR(IDF_GAMEMOD, 0, itemhalo, 0, FVAR_MAX,
    9,          9,          18,         15,         12,         15,         9,          12,         15,         6,          6,          12,         1.5f
);
WPFVAR(IDF_GAMEMOD, 0, itemhaloammo, 0, FVAR_MAX,
    12,         12,         12,         12,         12,         12,         12,         12,         12,         6,          6,          12,         1.5f
);
//jitter vars
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPVARM(IDF_GAMEMOD, 0, jitterpitchdir, VAR_MIN, VAR_MAX,
    0,          500,        0,          0,          500,        0,          0,          0,          0,          0,          0,          0,          0,
    0,          500,        0,          0,          500,        0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchmax, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       5.0f,       3.0f,       0.2f,       2.5f,       3.0f,       4.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       0.0f,       1.0f,       10.0f,      1.75f,      0.5f,       7.5f,       3.0f,       5.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchmin, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       2.5f,       0.25f,      0.1f,       1.25f,      1.5f,       1.8f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       0.0f,       1.0f,       5.0f,       1.0f,       0.2f,       5.0f,       1.5f,       2.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.4f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, jitterpitchinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       1.0f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawmax, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawmin, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       -1.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, jitteryawinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, 0, jittertime, VAR_MIN, VAR_MAX,
    50,         13,         50,         75,         -2,         20,         50,         50,         75,         75,         50,         150,        0,
    80,         20,         80,         125,        100,        125,        180,        50,         125,        75,         50,         150,        0
);
WPFVARM(IDF_GAMEMOD, 0, jittertimecrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, jittertimezoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, jittertimestill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, jittertimemoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, jittertimerunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, jittertimeinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, kickpush, FVAR_MIN, FVAR_MAX,
    0.0f,       4.0f,       -15.0f,     50.0f,      5.0f,       1.0f,       20.0f,      10.0f,      35.0f,      5.0f,       5.0f,       150.0f,     0.0f,
    0.0f,       -15.0f,     -30.0f,     75.0f,      25.0f,      100.0f,     50.0f,      10.0f,      50.0f,      5.0f,       5.0f,       150.0f,     0.0f
);
WPVAR(IDF_GAMEMOD, 0, laser, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, lightcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVAR(IDF_GAMEMOD, 0, lightpersist, 0, 15,
    0,          0,          1,          0,          0,          1,          0,          1,          0,          8,          0,          6,          0
);
WPFVAR(IDF_GAMEMOD, 0, lightradius, 0, FVAR_MAX,
    0,          16,         16,         16,         16,         16,         32,         8,          32,         32,         32,         32,         0
);
WPFVARK(IDF_GAMEMOD, 0, liquidcoast, 0, FVAR_MAX,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f
);
WPVAR(IDF_GAMEMOD, 0, modes, -G_ALL, G_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, muts, -G_M_ALL, G_M_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPSVARR(IDF_GAMEMOD, 0, obitsuicide,
    "hit themself",
    "ate a bullet",
    "commited sudoku",
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
WPSVARR(IDF_GAMEMOD, 0, obitobliterated,
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
WPSVARK(IDF_GAMEMOD, 0, obitheadless,
    //primary
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
    "given kung-fu lessons",
    //secondary
    "clawed and mauled",
    "clobbered",
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
    "given kung-fu lessons",
    //primary flak
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
    "given kung-fu lessons",
    //secondary flak
    "clawed and mauled",
    "clobbered",
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
WPSVARK(IDF_GAMEMOD, 0, obituary,
    "clawed",
    "pierced",
    "sliced and diced",
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
    "pistol whipped",
    "sliced and diced",
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
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPFVARK(IDF_GAMEMOD, 0, partblend, 0, 1,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.3f,       1.0f,       1.0f,       1.0f,       0.8f,       0.8f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f,
    1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f,       0.75f,      1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, partcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22,
    0x907020,   0xD0D0D0,   0x4040F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xA020F0,   PC(FIRE),   0x00F068,   PC(FIRE),   0xEEEE22
);
WPVARK(IDF_GAMEMOD, 0, partfade, 1, VAR_MAX,
    500,        250,        500,        250,        250,        200,        500,        100,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        100,        1000,       500,        500,        750,        500,
    500,        250,        500,        250,        250,        200,        500,        250,        500,        500,        500,        750,        500,
    500,        250,        500,        250,        250,        500,        500,        500,        1000,       500,        500,        750,        500
);
WPFVARK(IDF_GAMEMOD, 0, partlen, 0, FVAR_MAX,
    0.0f,       8.0f,       0.0f,       30.0f,      20.0f,      0.0f,       0.0f,       512.0f,     512.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       10.0f,      15.0f,      0.0f,       0.0f,       512.0f,     1024.0f,    0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       8.0f,       0.0f,       10.0f,      15.0f,      0.0f,       0.0f,       1024.f,     512.0f,     0.0f,       4.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       10.0f,      50.0f,      0.0f,       0.0f,       1024.f,     1024.0f,    0.0f,       4.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, partsize, 0, FVAR_MAX,
    1.0f,       0.125f,     1.0f,       0.75f,      0.6f,       6.0f,       8.0f,       2.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.5f,       0.75f,      12.0f,      5.0f,       2.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f,
    1.0f,       0.125f,     1.0f,       0.4f,       0.4f,       6.0f,       8.0f,       3.0f,       1.5f,       1.0f,       2.0f,       2.0f,       0.5f,
    2.0f,       0.25f,      1.25f,      0.4f,       0.4f,       12.0f,      10.0f,      5.0f,       3.0f,       1.0f,       2.0f,       2.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, parttype, 0, W_MAX-1,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_FLAMER,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_PLASMA,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE,
    W_CLAW,     W_PISTOL,   W_SWORD,    W_SHOTGUN,  W_SMG,      W_FLAMER,   W_FLAMER,   W_ZAPPER,   W_RIFLE,    W_GRENADE,  W_MINE,     W_ROCKET,   W_MELEE
);
WPVARK(IDF_GAMEMOD, 0, proxdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0
);
WPFVARK(IDF_GAMEMOD, 0, proxdist, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, proxtime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, proxtype, 0, 2,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, radial, 0, VAR_MAX,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          50,         100,        0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, radius, FVAR_NONZERO, FVAR_MAX,
    10.0f,      1.0f,       17.0f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       10.0f,
    10.0f,      14.0f,      17.0f,      2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       10.0f,
    10.0f,      1.0f,       17.0f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       10.0f,
    10.0f,      14.0f,      17.0f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       10.0f
);
WPVARM(IDF_GAMEMOD, 0, rays, 0, MAXPARAMS,
    1,          1,          1,          25,         1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          15,         1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, 0, reflectivity, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, relativity, 0, FVAR_MAX,
    0.0f,       0.05f,      0.0f,       0.15f,      0.15f,      0.95f,      0.15f,      0.0f,       0.5f,       0.75f,      0.5f,       0.0f,       0.0f,
    0.0f,       0.05f,      0.0f,       0.35f,      0.25f,      0.15f,      0.15f,      0.1f,       0.1f,       0.75f,      0.5f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, residual, 0, W_R_ALL,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          0,          0,          WR(BURN),   WR(SHOCK),  WR(BURN),   0
);
WPVARK(IDF_GAMEMOD, 0, residualundo, 0, W_R_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine        Rocket      Melee
WPVARK(IDF_GAMEMOD, 0, shocktime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, shockdelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, shockdamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPVARK(IDF_GAMEMOD, 0, shockstun, 0, W_N_ALL,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST
);
WPFVARK(IDF_GAMEMOD, 0, shockstunscale, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPFVARK(IDF_GAMEMOD, 0, shockstunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, shockstuntime, 0, VAR_MAX,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500
);
WPVAR(IDF_GAMEMOD, 0, spawnstay, 0, VAR_MAX,
    30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000
);
WPVAR(IDF_GAMEMOD, 0, spawntime, 0, VAR_MAX,
    15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      45000,      45000,      60000,      15000
);
WPVAR(IDF_GAMEMOD, 0, spawnduke, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          60000,      0
);
WPVARM(IDF_GAMEMOD, 0, speed, 0, VAR_MAX,
    0,          2000,       0,          1000,       4000,       200,        1250,       10000,      10000,      275,        100,        1000,       0,
    0,          1500,       0,          270,        500,        750,        150,        10000,      100000,     275,        100,        250,        0
);
WPFVARK(IDF_GAMEMOD, 0, speeddelta, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPVARM(IDF_GAMEMOD, 0, speedlimit, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          35,         0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, speedmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f
);
WPFVARK(IDF_GAMEMOD, 0, speedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spread, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       24.0f,      0.0f,       7.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       30.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadz, 0, FVAR_MAX,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.5f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       4.0f,       2.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadcrouch, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadzoom, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadstill, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmoving, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadrunning, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadinair, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Grenade     Mine       Rocket      Melee
WPVARK(IDF_GAMEMOD, 0, stun, 0, W_N_ALL,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ALL,    0,          W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD
);
WPFVARK(IDF_GAMEMOD, 0, stunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.2f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, stunscale, 0, FVAR_MAX,
    0.5f,       0.125f,     1.0f,       0.125f,     0.25f,      0.0f,       0.1f,       0.1f,       0.3f,       1.0f,       4.0f,       8.0f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f ,     0.5f,       0.5f,       0.25f,      2.0f,       2.0f,       1.0f,       8.0f,       8.0f,       0.5f,
    0.5f ,      0.125f,     1.0f,       0.125f,     0.25f,      0.0f,       0.1f,       0.2f,       0.6f,       1.0f,       4.0f,       8.0f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f,      0.5f,       0.5f,       0.25f,      16.0f,      4.0f,       1.0f,       8.0f,       8.0f,       0.5f
);
WPVARK(IDF_GAMEMOD, 0, stuntime, 0, VAR_MAX,
    250,        25,         300,        75,         75,         0,          0,          250,        100,        200,        500,        750,        100,
    500,        25,         500,        250,        250,        500,        200,        250,        250,        200,        750,        500,        200,
    250,        25,         300,        75,         75,         0,          0,          100,        500,        200,        500,        750,        100,
    500,        25,         500,        250,        75,         500,        200,        750,        750,        200,        750,        500,        200
);
WPVARK(IDF_GAMEMOD, 0, taper, 0, 6,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, taperin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.05f,      0.025f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, taperout, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       0.0f,       0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       0.0f,       0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.01f,      0.01f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, tapermin, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, tapermax, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, time, 1, VAR_MAX,
    200,        1000,       350,        400,        800,        650,        350,        250,        7500,       2500,       60000,      5000,       250,
    350,        300,        250,        1000,       750,        200,        2000,       250,        7500,       2500,       60000,      5000,       250
);
WPVARM(IDF_GAMEMOD, 0, timedelay, 0, VAR_MAX,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          175,        50,         0,          0,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          175,        50,         0,          0
);
WPVARM(IDF_GAMEMOD, 0, timeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, timestick, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, visfade, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, vistime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, wavepush, 0, FVAR_MAX,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       1.5f,       2.0f,       2.0f,       3.0f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.0f,       2.5f,       2.0f,       2.5f,       2.0f,       2.0f,       3.0f,       1.5f
);
WPFVARK(IDF_GAMEMOD, 0, weight, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       200.0f,     0.0f,       0.0f,       0.0f,       200.0f,     175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       300.0f,     300.0f,     0.0f,       100.0f,     0.0f,       0.0f,       200.0f,     175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       750.0f,     500.0f,     200.0f,     0.0f,       0.0f,       0.0f,       200.0f,     175.0f,     0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       500.0f,     0.0f,       0.0f,       100.0f,     0.0f,       0.0f,       200.0f,     175.0f,     0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, znudge, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.1f,       0.0625f,    0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.1f,       0.1f,       0.0f,       0.15f,      0.0f,       0.0f,       0.1f,       0.0625f,    0.0f,       0.0f
);
// these are used in modifying values in gameent::configure() et al
//  claw        pistol      sword       shotgun     smg         flamer      plasma      zappper     rifle       grenades    mines       rocket      melee
WPFVAR(IDF_GAMEMOD, 0, modhealth, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modhealthammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modhealthequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
// JUMP
WPFVAR(IDF_GAMEMOD, 0, modjumpspeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modjumpspeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -5,         0
);
WPFVAR(IDF_GAMEMOD, 0, modjumpspeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
// IMPULSE
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modimpulsespeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -5,         0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
// SPEED
WPFVAR(IDF_GAMEMOD, 0, modspeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          -8,         -5,         -8,         -8,         -8,         -10,        0,          0,          -15,        0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          -2,         -2,         0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modspeedattack, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -10,        -15,        -5,         -5,         -10,        -15,        -5,         -5,         -15,        -5,
    -5,         -5,         -5,         -15,        -15,        -5,         -10,        -15,        -20,        -5,         -5,         -15,        -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -10,        -5,         -10,        -5,         -10,        -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedpower, FVAR_MIN, FVAR_MAX,
    -5,         -5,         0,          -5,         -5,         -5,         0,          -5,         -5,         -5,         -5,         -10,        -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -3,         -5,         -5,         -10,        0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -10,        -15,        -5,         -5,         -10,        -5
);
// WEIGHT
WPFVAR(IDF_GAMEMOD, 0, modweight, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          5,          3,          6,          8,          10,         12,         0,          0,          15,         0
);
WPFVAR(IDF_GAMEMOD, 0, modweightammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          3,          3,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modweightattack, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightreload, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightpower, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightuse, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightzoom, FVAR_MIN, FVAR_MAX,
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
    bool    muzzle,     eject,      tape,       thrown;
    float   esize;
    const char *name, *item, *ammo, *vwep, *hwep, *proj, *eprj;
};
#ifdef CPP_GAME_SERVER
weaptypes weaptype[] =
{
    {
            ANIM_CLAW,         S_CLAW,          1,
            true,       false,      true,       false,
            0,
            "claw", "", "", "weapons/claw/vwep", "weapons/claw/hwep", "", ""
    },
    {
            ANIM_PISTOL,        S_PISTOL,       10,
            true,       true,       false,       false,
            0.45f,
            "pistol", "weapons/pistol/item", "weapons/pistol/ammo", "weapons/pistol/vwep", "weapons/pistol/hwep", "weapons/pistol/proj", "projectiles/cartridge"
    },
    {
            ANIM_SWORD,         S_SWORD,        1,
            true,       false,      true,       false,
            0,
            "sword", "weapons/sword/item", "weapons/sword/ammo", "weapons/sword/vwep", "weapons/sword/hwep", "", ""
    },
    {
            ANIM_SHOTGUN,       S_SHOTGUN,      10,
            true,       true,       false,       false,
            0.6f,
            "shotgun", "weapons/shotgun/item", "weapons/shotgun/ammo", "weapons/shotgun/vwep", "weapons/shotgun/hwep", "", "projectiles/shell"
    },
    {
            ANIM_SMG,           S_SMG,          20,
            true,       true,       false,       false,
            0.45f,
            "smg", "weapons/smg/item", "weapons/smg/ammo", "weapons/smg/vwep", "weapons/smg/hwep", "", "projectiles/cartridge"
    },
    {
            ANIM_FLAMER,        S_FLAMER,       1,
            true,       true,       false,       false,
            0,
            "flamer", "weapons/flamer/item", "weapons/flamer/ammo", "weapons/flamer/vwep", "weapons/flamer/hwep", "", ""
    },
    {
            ANIM_PLASMA,        S_PLASMA,       1,
            true,       false,      false,       false,
            0,
            "plasma", "weapons/plasma/item", "weapons/plasma/ammo", "weapons/plasma/vwep", "weapons/plasma/hwep", "", ""
    },
    {
            ANIM_ZAPPER,        S_ZAPPER,       1,
            true,       false,      true,       false,
            0,
            "zapper", "weapons/zapper/item", "weapons/zapper/ammo", "weapons/zapper/vwep", "weapons/zapper/hwep", "", ""
    },
    {
            ANIM_RIFLE,         S_RIFLE,        1,
            true,       false,      false,       false,
            0,
            "rifle", "weapons/rifle/item", "weapons/rifle/ammo", "weapons/rifle/vwep", "weapons/rifle/hwep", "", ""
    },
    {
            ANIM_GRENADE,       S_GRENADE,      1,
            false,      false,      false,      true,
            0,
            "grenade", "weapons/grenade/item", "weapons/grenade/ammo", "weapons/grenade/vwep", "weapons/grenade/hwep", "weapons/grenade/proj", ""
    },
    {
            ANIM_MINE,          S_MINE,         1,
            false,      false,      false,      true,
            0,
            "mine", "weapons/mine/item", "weapons/mine/ammo", "weapons/mine/vwep", "weapons/mine/hwep", "weapons/mine/proj", ""
    },
    {
            ANIM_ROCKET,        S_ROCKET,       1,
            true,      false,       false,       false,
            0,
            "rocket", "weapons/rocket/item", "weapons/rocket/ammo", "weapons/rocket/vwep", "weapons/rocket/hwep", "weapons/rocket/proj",  ""
    },
    {
            0,                  S_MELEE,        1,
            false,      false,      false,       false,
            0,
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
VAR(IDF_READONLY, weapstateinterrupt, 1, W_S_INTERRUPT, -1);
VAR(IDF_READONLY, weapresidualburn, 1, W_R_BURN, -1);
VAR(IDF_READONLY, weapresidualbleed, 1, W_R_BLEED, -1);
VAR(IDF_READONLY, weapresidualshock, 1, W_R_SHOCK, -1);
VAR(IDF_READONLY, weapresidualmax, 1, W_R_MAX, -1);
VAR(IDF_READONLY, weapcarrydefault, 1, WEAPCARRY, -1);
VAR(IDF_READONLY, weapcarrylimit, 1, W_LOADOUT, -1);
#else
extern weaptypes weaptype[];
#endif
