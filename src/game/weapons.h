// weapons.h configuration file
// default weapon variables baked into the game
#define W_ENUM(en, um) \
    en(um, claw, CLAW) en(um, pistol, PISTOL) en(um, sword, SWORD) en(um, shotgun, SHOTGUN) en(um, smg, SMG) \
    en(um, flamer, FLAMER) en(um, plasma, PLASMA) en(um, zapper, ZAPPER) en(um, rifle, RIFLE) en(um, corroder, CORRODER) \
    en(um, grenade, GRENADE) en(um, mine, MINE) en(um, rocket, ROCKET) en(um, minigun, MINIGUN) en(um, jetsaw, JETSAW) en(um, eclipse, ECLIPSE) \
    en(um, melee, MELEE) en(um, maximum, MAX)
ENUM_DLN(W);

ENUM_VAR(W_OFFSET, W_SWORD);
ENUM_VAR(W_ITEM, W_GRENADE);
ENUM_VAR(W_ALL, W_MELEE);
ENUM_VAR(W_LOADOUT, W_ITEM - W_OFFSET);
ENUM_VAR(W_ITEMS, W_MINE - W_GRENADE + 1);
ENUM_VAR(W_REPLACE, W_GRENADE);
ENUM_VAR(W_PRIZE, W_GRENADE);
ENUM_VAR(W_PRIZES, W_ECLIPSE - W_GRENADE + 1);
ENUM_VAR(W_SUPER, W_ROCKET);
ENUM_VAR(W_SUPERS, W_ECLIPSE - W_ROCKET + 1);
ENUM_VAR(W_MASK, (1<<W_CLAW)|(1<<W_PISTOL)|(1<<W_SWORD)|(1<<W_SHOTGUN)|(1<<W_SMG)|(1<<W_FLAMER)|(1<<W_PLASMA)|(1<<W_ZAPPER)|(1<<W_RIFLE)|(1<<W_CORRODER)|(1<<W_GRENADE)|(1<<W_MINE)|(1<<W_ROCKET)|(1<<W_MINIGUN)|(1<<W_JETSAW)|(1<<W_ECLIPSE)|(1<<W_MELEE));
ENUM_VAR(W_ANNOUNCE, (1<<W_ROCKET)|(1<<W_MINIGUN)|(1<<W_JETSAW)|(1<<W_ECLIPSE));

#define WZ(x) (W_MAX+(W_##x))

#define isweap(a)       (a >= 0 && a < W_MAX)
#define itemweap(a)     (a >= W_ITEM && a < W_ALL)
#define w_carry(w1,w2)  (isweap(w1) && w1 != W_CLAW && w1 < W_ALL && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE))) && (w1 >= W_OFFSET && w1 < W_ITEM))

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

#define W_S_ENUM(en, um) \
    en(um, IDLE) en(um, PRIMARY) en(um, SECONDARY) en(um, RELOAD) \
    en(um, POWER) en(um, ZOOM) en(um, SWITCH) en(um, USE) en(um, WAIT) en(um, MAX)
ENUM_DL(W_S);

ENUM_VAR(W_S_ALL, (1<<W_S_IDLE)|(1<<W_S_PRIMARY)|(1<<W_S_SECONDARY)|(1<<W_S_RELOAD)|(1<<W_S_SWITCH)|(1<<W_S_USE)|(1<<W_S_POWER)|(1<<W_S_ZOOM)|(1<<W_S_WAIT));
ENUM_VAR(W_S_INTERRUPT, (1<<W_S_POWER)|(1<<W_S_ZOOM));

enum { W_A_CLIP = 0, W_A_STORE, W_A_MAX };

enum
{
    S_W_PRIMARY = 0, S_W_SECONDARY,
    S_W_PRIMARY_BEGIN, S_W_PRIMARY_END,
    S_W_SECONDARY_BEGIN, S_W_SECONDARY_END,
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
    S_CORRODER  = S_RIFLE+S_W_MAX,
    S_GRENADE   = S_CORRODER+S_W_MAX,
    S_MINE      = S_GRENADE+S_W_MAX,
    S_ROCKET    = S_MINE+S_W_MAX,
    S_MINIGUN   = S_ROCKET+S_W_MAX,
    S_JETSAW    = S_MINIGUN+S_W_MAX,
    S_ECLIPSE   = S_JETSAW+S_W_MAX,
    S_MELEE     = S_ECLIPSE+S_W_MAX,
    S_MAX       = S_MELEE+S_W_OFFSET
};

// weapon shot fx
enum
{
    FX_W_NONE = -1,

    FX_W_MUZZLE1,  // default for pistol1
    FX_W_MUZZLE2,  // default for shotgun1
    FX_W_MUZZLE3,  // default for shotgun2
    FX_W_MUZZLE4,  // default for smg1 and minigun1
    FX_W_MUZZLE5,  // default for smg2 and minigun2
    FX_W_MUZZLE6,  // default for rocket1 and rocket2
    FX_W_FLAME,    // default for flamer1
    FX_W_AIRBLAST, // default for flamer2
    FX_W_PLASMA1,  // default for plasma1 and eclipse1
    FX_W_PLASMA2,  // default for plasma2 and eclipse2
    FX_W_PLASMA_P, // default for plasma power
    FX_W_ENERGY1,  // default for zapper1
    FX_W_ENERGY2,  // default for zapper2
    FX_W_ENERGY_P, // default for zapper power
    FX_W_BEAM1,    // default for rifle1
    FX_W_BEAM2,    // default for rifle2
    FX_W_SPLASH1,  // default for corroder1
    FX_W_SPLASH2,  // default for corroder2

    FX_W_TYPES
};

enum
{
    FX_P_NONE = -1,

    // Weapon projectiles
    FX_P_BULLET,
    FX_P_PELLET,
    FX_P_ACID,
    FX_P_FLAK,
    FX_P_SHRAPNEL,
    FX_P_BLOB,
    FX_P_FLAME,
    FX_P_AIRBLAST,
    FX_P_PLASMA,
    FX_P_VORTEX,
    FX_P_ENERGY,
    FX_P_BEAM,
    FX_P_GRENADE,
    FX_P_MINE,
    FX_P_ROCKET,

    // Misc projectiles
    FX_P_CASING,
    FX_P_GIB,
    FX_P_DEBRIS,

    FX_P_TYPES
};

#define W_FX_POWER_PARAM 0
#define P_FX_LIFETIME_PARAM 0
#define P_FX_BOUNCE_VEL_PARAM 0
#define P_FX_HIT_ALIVE_PARAM 0

enum
{
    COLLIDE_TRACE = 1<<0, COLLIDE_SCAN = 1<<1, COLLIDE_LENGTH = 1<<2, COLLIDE_PROJ = 1<<3, COLLIDE_OWNER = 1<<4,
    IMPACT_GEOM = 1<<5, IMPACT_PLAYER = 1<<6, IMPACT_SHOTS = 1<<7, BOUNCE_GEOM = 1<<8, BOUNCE_PLAYER = 1<<9, BOUNCE_SHOTS = 1<<10,
    DRILL_GEOM = 1<<11, DRILL_PLAYER = 1<<12, DRILL_SHOTS = 1<<13, STICK_GEOM = 1<<14, STICK_PLAYER = 1<<15, ATTRACT_PLAYER = 1<<16,
    COLLIDE_GEOM = IMPACT_GEOM|BOUNCE_GEOM|STICK_GEOM, COLLIDE_PLAYER = IMPACT_PLAYER|BOUNCE_PLAYER|STICK_PLAYER,
    COLLIDE_SHOTS = IMPACT_SHOTS|BOUNCE_SHOTS, COLLIDE_DYNENT = COLLIDE_PLAYER|COLLIDE_SHOTS,
    COLLIDE_FROMTO = COLLIDE_TRACE|COLLIDE_SCAN, COLLIDE_SCLEN = COLLIDE_SCAN|COLLIDE_LENGTH,
    COLLIDE_ALL = COLLIDE_TRACE|COLLIDE_SCAN|COLLIDE_LENGTH|COLLIDE_PROJ|COLLIDE_OWNER|IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|BOUNCE_GEOM|BOUNCE_PLAYER|BOUNCE_SHOTS|DRILL_GEOM|DRILL_PLAYER|DRILL_SHOTS|STICK_GEOM|STICK_PLAYER|ATTRACT_PLAYER
};

#define HIT_ENUM(en, um) \
    en(um, HEAD, 1<<0) en(um, TORSO, 1<<1) en(um, LIMB, 1<<2) en(um, FULL, 1<<3) en(um, WHIPLASH, 1<<4) en(um, ALT, 1<<5) \
    en(um, WAVE, 1<<6) en(um, PROJ, 1<<7) en(um, EXPLODE, 1<<8) en(um, BURN, 1<<9) en(um, BLEED, 1<<10) en(um, SHOCK, 1<<11) en(um, CORRODE, 1<<12) \
    en(um, MATERIAL, 1<<13) en(um, SPAWN, 1<<14) en(um, LOST, 1<<15) en(um, KILL, 1<<16) en(um, FLAK, 1<<17) \
    en(um, SPEC, 1<<18) en(um, TOUCH, 1<<19) en(um, CRUSH, 1<<20) en(um, CHECKPOINT, 1<<21) en(um, JANITOR, 1<<22) en(um, PRIZE, 1<<23)
ENUM_AL(HIT);

ENUM_VAR(HIT_CLEAR, HIT_PROJ|HIT_EXPLODE|HIT_BURN|HIT_BLEED|HIT_SHOCK|HIT_CORRODE|HIT_MATERIAL|HIT_SPAWN|HIT_LOST|HIT_TOUCH|HIT_CRUSH);
ENUM_VAR(HIT_SFLAGS, HIT_KILL|HIT_PRIZE);

#define W_R_ENUM(en, um) \
    en(um, BURN) en(um, BLEED) en(um, SHOCK) en(um, CORRODE) en(um, MAX)
ENUM_DL(W_R);

ENUM_VAR(W_R_ALL, (1<<W_R_BURN)|(1<<W_R_BLEED)|(1<<W_R_SHOCK)|(1<<W_R_CORRODE));
#define WR(x) (1<<W_R_##x)

struct shotmsg { int id; ivec pos; };
struct hitmsg { int flags, proj, target, dist; ivec dir, vel; };

#define hitdealt(x)         (x&HIT_BURN || x&HIT_BLEED || x&HIT_SHOCK || x&HIT_CORRODE || x&HIT_EXPLODE || x&HIT_PROJ || x&HIT_MATERIAL)

#define wr_burn(x,y)        (isweap(x) && (WF(WK(y), x, residual, WS(y))&(1<<W_R_BURN)))
#define wr_burns(x,y)       (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT_BURN) || wr_burn(x, y)))
#define wr_burnfunc(x,y)    (G(noweapburn) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BURN) || wr_burn(x, y)))

#define wr_bleed(x,y)       (isweap(x) && (WF(WK(y), x, residual, WS(y))&(1<<W_R_BLEED)))
#define wr_bleeds(x,y)      (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT_BLEED) || wr_bleed(x, y)))
#define wr_bleedfunc(x,y)   (G(noweapbleed) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_BLEED) || wr_bleed(x, y)))

#define wr_shock(x,y)       (isweap(x) && (WF(WK(y), x, residual, WS(y))&(1<<W_R_SHOCK)))
#define wr_shocks(x,y)      (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT_SHOCK) || wr_shock(x, y)))
#define wr_shockfunc(x,y)   (G(noweapshock) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_SHOCK) || wr_shock(x, y)))

#define wr_corrode(x,y)     (isweap(x) && (WF(WK(y), x, residual, WS(y))&(1<<W_R_CORRODE)))
#define wr_corrodes(x,y)    (G(noweapcorrode) && hitdealt(y) && ((x == -1 && y&HIT_CORRODE) || wr_corrode(x, y)))
#define wr_corrodefunc(x,y) (G(noweapcorrode) && hitdealt(y) && ((x == -1 && y&HIT_MATERIAL && y&HIT_CORRODE) || wr_corrode(x, y)))

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
    "Corrosion Cannon",
    "Frag Grenade",
    "Shock Mine",
    "Rocket Launcher",
    "Heavy Minigun",
    "Jet Chainsaw",
    "Eclipse Rod",
    "Melee Attack"
);
// Strings formatted as "The <weapon> <primary/secondary> <string>" implied
// e.g. The sword primary is a horizontally slicing moderately powerful slash, capable of being swung at short intervals
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
    "fires a spray of acid, causing splash damage to the victim, but none from afar",
    "is a thrown explosive with a burn residual, capable of having its time to detonation shortened by being held after being cocked",
    "explodes and discharges electric rays when someone comes too close, slowing them down and causing shock residual damage",
    "is a fast, highly explosive rocket with a large shockwave, causing burn residual damage",
    "is a high rate of fire, high damage, and high recoil minigun capable of shredding through opponents at close range",
    "is a jet-powered chainsaw that can be used to quickly cut through opponents, causing high damage at close range",
    "rapid-fires dark beams that follow your mouse movements and spray small homing projectiles during flight",
    "allows you to use parkour and kick moves as melee attacks",
    // begin secondary
    "is a slower and weaker claw attack that pulls you towards the enemy and allows wall grabs",
    "bludgeons opponents with the weapon itself, capable of being swung quickly but not capable of significant damage",
    "is a slower, charged vertical slice capable of powerful bleed-inducing swings",
    "shoots a slug that disintegrates on contact or delay into shrapnel, causing bleed residual damage",
    "shoots medium-range projectiles that can stick to surfaces and players before exploding and releasing shrapnel for additional damage",
    "releases an air blast, capable of extending one's own airtime or knocking back opponents; capable of extinguishing burning players",
    "is a charged, quickly-expanding ball of plasma that quickly sucks players in, causing great damage",
    "is a scoped, long ranged, medium damage, and fairly quick firing ray weapon capable of accurately hitting opponents at long range",
    "is a charged and focused laser shot with great precision and adjustable zoom, ideal for long range",
    "is a charged ball of acid that leaves puddles on the ground, causing corrosion residual damage",
    "is a thrown explosive that sticks to opponents, capable of having its time to detonation shortened by being held; burns opponents",
    "emits an long distance laser trap that detonates when tripped, damaging and slowing the victim with a stun residual",
    "is a guided rocket that moves significantly slower than the primary fire mode, causing burn residual damage",
    "fires a giant ball of shrapnel that explodes on impact",
    "is a jet-powered chainsaw that can be used to quickly cut through opponents, causing high damage at close range",
    "charges up to fire a superheated antiparticle pool at cursor location that deals heavy damage and ignites players",
    "allows you to use slide moves as melee attacks"
);

WPSVAR(IDF_GAMEMOD, 0, name, // keep this incase longname is too long
    "Claw",     "Pistol",   "Sword",    "Shotgun",  "SMG",      "Flamer",   "Plasma",   "Zapper",   "Rifle",    "Corroder","Grenade",  "Mine",     "Rocket",   "Minigun",  "Jetsaw",    "Eclipse",  "Melee"
);

WPFVARM(IDF_GAMEMOD, 0, aidist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      128.0f,     320.0f,     64.0f,      512.0f,     512.0f,     512.0f,     128.0f,     384.0f,     128.0f,     1024.0f,    320.0f,     48.0f,      512.0f,     16.0f,
    16.0f,      40.0f,      48.0f,      256.0f,     320.0f,     32.0f,      128.0f,     128.0f,     2048.0f,    128.0f,     128.0f,     128.0f,     512.0f,     320.0f,     48.0f,      128.0f,     16.0f
);
WPFVARM(IDF_GAMEMOD, 0, aileaddist, 0, FVAR_MAX,
    16.0f,      512.0f,     48.0f,      128.0f,     320.0f,     64.0f,      512.0f,     512.0f,     512.0f,     128.0f,     384.0f,     128.0f,     1024.0f,    320.0f,     48.0f,      512.0f,     16.0f,
    16.0f,      40.0f,      48.0f,      256.0f,     320.0f,     32.0f,      128.0f,     128.0f,     2048.0f,    128.0f,     128.0f,     128.0f,     512.0f,     320.0f,     48.0f,      128.0f,     16.0f
);
WPFVARM(IDF_GAMEMOD, 0, aileadvel, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPFVARM(IDF_GAMEMOD, 0, aiskew, 0, FVAR_MAX,
    0.1f,       0.5f,       0.1f,       0.3f,       0.5f,       0.3f,       0.5f,       0.5f,       0.5f,       0.3f,       0.1f,       0.1f,       0.5f,       0.5f,       0.1f,       0.4f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.3f,       0.5f,       0.3f,       0.5f,       0.5f,       0.5f,       0.3f,       0.1f,       0.1f,       0.5f,       0.5f,       0.1f,       0.4f,       0.1f
);
WPVAR(IDF_GAMEMOD, 0, ammoadd, 1, VAR_MAX,
    1,          10,         1,          2,          40,         100,        30,         48,         6,          200,        1,          1,          1,          40,         5,          99,         1
);
WPVAR(IDF_GAMEMOD, 0, ammoclip, 1, VAR_MAX,
    1,          10,         1,          8,          40,         100,        30,         48,         6,          200,        2,          2,          1,          40,         5,          99,         1
);
WPVAR(IDF_GAMEMOD, 0, ammoitem, 1, VAR_MAX,
    1,          10,         1,          24,         120,        300,        90,         288,        18,         600,        1,          1,          1,          120,        5,          99,         0
);
WPVAR(IDF_GAMEMOD, 0, ammospawn, 1, VAR_MAX,
    1,          10,         1,          24,         120,        300,        90,         288,        18,         600,        2,          2,          1,          120,        5,          99,         1
);
WPVAR(IDF_GAMEMOD, 0, ammostore, -1, VAR_MAX,
    -1,         -1,         -1,         48,         240,        600,        180,        576,        36,         1200,       0,          0,          0,          0,          0,          198,        -1
);
WPVARM(IDF_GAMEMOD, 0, ammosub, 0, VAR_MAX,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,
    0,          0,          0,          2,          4,          10,         30,         8,          1,          20,         1,          1,          1,          4,          1,          33,         0
);
WPFVARK(IDF_GAMEMOD, 0, attractspeed, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,        0.5f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,        0.5f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,        0.5f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,        0.5f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARK(IDF_GAMEMOD, 0, attractdist, 0, FVAR_MAX,
    128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,
    128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,
    128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,
    128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f,     128.0f
);
WPFVARK(IDF_GAMEMOD, 0, attractminvel, 0, FVAR_MAX,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,
    4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f,       4.0f
);
WPFVARK(IDF_GAMEMOD, 0, attractscaled, 0, 1,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARK(IDF_GAMEMOD, 0, bleedtime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, bleeddelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, bleeddamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPFVARK(IDF_GAMEMOD, 0, buoyancy, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, burntime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, burndelay, 1, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, burndamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPFVARK(IDF_GAMEMOD, 0, blend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.1f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, collide, 0, COLLIDE_ALL,
    // primary
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,                                     // claw
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,                         // pistol
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,                        // sword
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,              // shotgun
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,              // smg
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,                                                    // flamer
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,                                       // plasma
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_SCAN|STICK_GEOM,                             // zapper
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM,              // rifle
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // corroder
    BOUNCE_GEOM|BOUNCE_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // grenade
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,  // mine
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // rocket
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|DRILL_GEOM,              // minigun
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,                        // jetsaw
    DRILL_PLAYER|IMPACT_SHOTS|IMPACT_PLAYER|IMPACT_GEOM|COLLIDE_OWNER,                          // eclipse
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,                                                 // melee
    // secondary
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,                                     // claw
    IMPACT_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,                                     // pistol
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,                        // sword
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // shotgun
    IMPACT_SHOTS|COLLIDE_OWNER|BOUNCE_GEOM|BOUNCE_PLAYER,                                       // smg
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,                                       // flamer
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM,                                          // plasma
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE,                                       // zapper
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE|DRILL_GEOM|DRILL_PLAYER, // rifle
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|ATTRACT_PLAYER|COLLIDE_PROJ,// corroder
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,  // grenade
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,  // mine
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // rocket
    IMPACT_SHOTS|COLLIDE_OWNER|BOUNCE_GEOM|BOUNCE_PLAYER,                                       // minigun
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_LENGTH,                        // jetsaw
    IMPACT_GEOM|STICK_GEOM|IMPACT_PLAYER|COLLIDE_TRACE,                                         // eclipse
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_LENGTH,                                                 // melee
    // primary flak
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,                                                  // claw
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // pistol
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,                         // sword
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // shotgun
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // smg
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_OWNER,                                                    // flamer
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,                                       // plasma
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,                         // zapper
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,                         // rifle
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // corroder
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // grenade
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER,               // mine
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // rocket
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // minigun
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,                         // jetsaw
    IMPACT_GEOM|IMPACT_PLAYER|BOUNCE_GEOM|BOUNCE_PLAYER,                                        // eclipse
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,                                                  // melee
    // secondary flak
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER,                                                  // claw
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // pistol
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,                         // sword
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // shotgun
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // smg
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER,                                       // flamer
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM,                                          // plasma
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_TRACE,                         // zapper
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|DRILL_PLAYER|COLLIDE_OWNER,            // rifle
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER|STICK_GEOM|ATTRACT_PLAYER,// corroder
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER|COLLIDE_PROJ,  // grenade
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM|STICK_PLAYER,               // mine
    IMPACT_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_OWNER|COLLIDE_PROJ,                          // rocket
    BOUNCE_GEOM|IMPACT_PLAYER|IMPACT_SHOTS|COLLIDE_TRACE|COLLIDE_OWNER,                         // minigun
    BOUNCE_GEOM|IMPACT_PLAYER|COLLIDE_TRACE|IMPACT_SHOTS|COLLIDE_OWNER,                         // jetsaw
    IMPACT_GEOM|IMPACT_SHOTS|COLLIDE_OWNER|STICK_GEOM,                                          // eclipse
    IMPACT_PLAYER|COLLIDE_TRACE|COLLIDE_OWNER                                                   // melee
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVAR(IDF_GAMEMOD|IDF_HEX, 0, colour, 0, 0xFFFFFF,
    0xD2691E,   0xFFFFFF,   0x1010FF,   0xFFFF00,   0xFF9C10,   0xFF1010,   0x10FFFF,   0x4B10A6,   0x901090,   0x109010,   0x60F060,   0x42D4CC,   0xFF1493,   0xFF4C10,   0x1040FF,   0x3F0000,   0xFFFFFF
);
WPVARM(IDF_GAMEMOD|IDF_HEX, 0, colourcook, PC(LAST), 0xFFFFFF,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0xFF0000,   0,          0,          0,          PC(BLEED),  0,          0,
    0,          0,          PC(BLEED),  0,          0,          0,          PC(DISCO),  PC(SHOCK),  0,          PC(CORRODE),0xFF0000,   0,          0,          0,          PC(BLEED),  PC(DISCO),  0
);
WPVARM(IDF_GAMEMOD|IDF_HEX, 0, colourproj, PC(LAST),  0xFFFFFF,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0xFF0000,   0,          0,          0,          0,          0x3F0000,   0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0xFF0000,   0,          0,          0,          0,          0x3F0000,   0
);
WPVARM(IDF_GAMEMOD, 0, cooked, 0, W_C_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          W_C_LIFEN,  0,          0,          0,          0,          0,          0,
    0,          0,          W_C_SCALE,  0,          0,          0,          W_C_SS,     W_C_SS,     W_C_SZ,     0,          W_C_LIFEN,  0,          0,          0,          W_C_SCALE,  W_C_SCALE,  0
);
WPVAR(IDF_GAMEMOD, 0, cookinterrupt, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, cooktime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3000,       0,          0,          0,          0,          0,          0,
    0,          0,          500,        0,          0,          0,          2000,       500,        500,        0,          3000,       0,          0,          0,          500,        1500,       0
);
WPVAR(IDF_GAMEMOD, 0, cookzoom, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          250,        0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, cookzoommin, 1, 150,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
WPFVAR(IDF_GAMEMOD, 0, cookzoommax, 1, 150,
    60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60,         60
);
WPVARK(IDF_GAMEMOD, 0, corrodetime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, corrodedelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, corrodedamage, 0, VAR_MAX,
    15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,
    15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,
    15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,
    15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15,         15
);
WPVARK(IDF_GAMEMOD, 0, damage, VAR_MIN, VAR_MAX,
    400,        200,        300,        75,         160,        30,         150,        66,         330,        50,         1000,       1000,       2000,       500,        750,        95,         300,
    400,        300,        650,        75,         160,        100,        35,         33,         1200,       500,        1000,       1000,       1000,       500,        1000,       100,        400,
    400,        200,        300,        75,         160,        30,         150,        200,        1000,       50,         1000,       1000,       2000,       500,        750,        15,         300,
    400,        300,        650,        75,         160,        100,        35,         200,        1000,       50,         1000,       1000,       1000,       500,        1000,       500,        400
);
WPFVARK(IDF_GAMEMOD, 0, damagecollect, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, damagehead, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.85f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, damagelimb, FVAR_MIN, FVAR_MAX,
    0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.55f,      0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,
    0.5f,       0.3f,       0.5f,       0.3f,       0.3f,       1.0f,       0.3f,       0.55f,      0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.3f,       0.5f,       0.3f,       0.5f,
    0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.55f,      0.3f,       0.3f,       0.3f,       0.35f,      0.35f,      0.35f,      0.35f,      0.35f,      0.35f,
    0.5f,       0.35f,      0.5f,       0.35f,      0.35f,      0.35f,      0.35f,      0.55f,      0.35f,      0.3f,       0.3f,       0.35f,      0.35f,      0.35f,      0.5f,       0.35f,      0.5f
);
WPFVARK(IDF_GAMEMOD, 0, damageself, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.4f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, damageteam, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.1f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, damagetorso, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.85f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.6f,       0.5f,       0.6f,       0.5f,       0.5f,       1.5f,       0.5f,       0.85f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.6f,       0.5f,       0.6f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.85f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.6f,       0.5f,       0.6f,       0.5f,       0.5f,       1.5f,       0.5f,       0.85f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.6f,       0.5f,       0.6f
);
WPFVARK(IDF_GAMEMOD, 0, damagepenalty, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,          1,          1,          1,          1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, damagewhiplash, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.8f,       0.6f,       0.6f,       0.6f,       0.8f,       0.8f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.8f,       0.6f,       0.6f,       0.6f,       0.8f,       0.8f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.65f,      0.6f,       0.6f,       0.6f,       0.8f,       0.6f,       0.6f,       0.6f,       0.8f,       0.8f,       0.6f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.75f,      0.6f,       0.6f,       0.6f,       0.8f,       0.6f,       0.6f,       0.6f,       0.8f,       0.8f,       0.6f,       0.8f
);
WPVARM(IDF_GAMEMOD, 0, delayattack, 1, VAR_MAX,
    500,        200,        500,        600,        75,         30,         350,        75,         750,        15,         750,        500,        1500,       75,         500,        55,         500,
    800,        300,        800,        1200,       500,        1000,       1500,       1250,       1250,       625,        750,        500,        1500,       500,        800,        1500,       500
);
WPVAR(IDF_GAMEMOD, 0, delayitem, 0, VAR_MAX,
    500,        500,        500,        750,        750,        750,        750,        750,        750,        750,        500,        500,        1000,       750,        500,        1000,       100
);
WPVAR(IDF_GAMEMOD, 0, delayreload, 0, VAR_MAX,
    50,         1000,       50,         1000,       1250,       2000,       1750,       1500,       2000,       1500,       1000,       1500,       2500,       1250,       50,         2500,       50
);
WPVAR(IDF_GAMEMOD, 0, delayswitch, 0, VAR_MAX,
    100,        200,        200,        400,        300,        400,        400,        300,        400,        400,        300,        300,        500,        300,        200,        650,        100
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVARK(IDF_GAMEMOD, 0, destroyburn, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, destroybleed, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, destroycorrode, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, destroyshock, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          1,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          1,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          1,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          1,          0,          0,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, disabled, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, drill, 0, VAR_MAX,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,          0,          2,          0,          256,        0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          2,          0,          0,          0,          2,          0,          0,          0,          0,          2,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          8,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, elasticity, 0, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.35f,      0.75f,      0.5f,       0.5f,       0.65f,      0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.35f,      0.75f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.65f,      0.35f,      0.5f,       0.5f,       0.5f,       0.35f,      0.75f,      0.5f,       0.5f,       0.65f,      0.5f,       1.01f,      0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.35f,      0.75f,      0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, 0, escapedelay, 0, VAR_MAX,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,
    200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200,        200
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, explcol, PC(LAST), 0xFFFFFF,
    PC(FIRE),   PC(FIRE),   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   PC(SHOCK),  PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x1010F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   PC(SHOCK),  PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   PC(SHOCK),  PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   PC(FIRE),
    PC(FIRE),   PC(FIRE),   0x1010F0,   0xF0F020,   0xF05820,   0x808080,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   PC(SHOCK),  PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   PC(FIRE)
);
WPVARK(IDF_GAMEMOD, 0, extinguish, 0, 7,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          2,          2,          2,          1,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          2,          2,          2,          0,          0,
    0,          2,          2,          2,          2,          3,          1,          1,          2,          2,          2,          2,          2,          2,          2,          1,          0,
    0,          2,          2,          2,          2,          2,          0,          1,          2,          2,          2,          2,          2,          2,          2,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, fade, 0, 3,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, 0, fadeat, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, fadecut, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPVARK(IDF_GAMEMOD, 0, fadein, 0, VAR_MAX,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,          0,          0,          40,         0,
    0,          0,          0,          0,          0,          40,         40,         0,          0,          0,          0,          0,          0,          0,          0,          40,         0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, fadeout, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPFVARM(IDF_GAMEMOD, 0, fragjump, 0, 1,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARM(IDF_GAMEMOD, 0, fragcond, 0, 15,
    14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,
    14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14,         14
);
WPFVARM(IDF_GAMEMOD, 0, fragoffset, FVAR_MIN, FVAR_MAX,
    4.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       1.0f,       1.0f,       2.0f,       2.0f,       1.0f,       2.0f,       2.0f,       2.0f,       100.0f,     2.0f,
    2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       1.0f,       1.0f,       2.0f,       2.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f
);
WPVARM(IDF_GAMEMOD, 0, fragrays, 1, MAXPARAMS,
    5,          5,          5,          5,          5,          5,          5,          5,          5,          25,         25,         25,         50,         5,          5,          2,          5,
    5,          5,          5,          25,         25,         5,          5,          5,          5,          25,         25,         10,         25,         25,         5,          5,          5
);
WPFVARM(IDF_GAMEMOD, 0, fragrel, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.5f,       0.1f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.1f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragscale, FVAR_NONZERO, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.3f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragskew, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, fragspeed, 0, VAR_MAX,
    0,          0,          0,          0,          0,          200,        0,          0,          0,          100,        250,        10000,      500,        0,          0,          100,        0,
    0,          0,          0,          0,          350,        250,        0,          0,          0,          100,        250,        10000,      500,        350,        0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, fragspeedmin, 0, FVAR_MAX,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,
    10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      1.0f,       10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f,      10.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragspeedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, fragspread, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       0.2f,       0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       1.0f,       0.5f,       1.0f,       0.2f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       0.2f,       0.75f,      0.1f,       1.0f,       0.25f,      0.25f,      1.0f,       1.0f,       0.0f,       1.0f,       0.75f,      1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, fragtime, 1, VAR_MAX,
    500,        500,        500,        250,        500,        1000,       500,        500,        500,        5000,       2000,       2000,       2000,       500,        500,        1200,       500,
    500,        500,        500,        2000,       250,        3000,       500,        500,        500,        5000,       2000,       5000,       2000,       250,        500,        500,        500
);
WPVARM(IDF_GAMEMOD, 0, fragtimedelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, fragtimeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          5,          5,          5,          5,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          5,          5,          250,        5,          0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, fragweap, -1, W_MAX*2-1,
    -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         -1,         W_SHOTGUN,  W_ZAPPER,   W_SMG,      -1,         -1,         W_ECLIPSE,  -1,
    -1,         -1,         -1,         WZ(SHOTGUN),WZ(SMG),    -1,         -1,         -1,         -1,         W_CORRODER, WZ(SHOTGUN),WZ(ZAPPER), W_SMG,      WZ(SMG),    -1,         -1,         -1
);
WPVARM(IDF_GAMEMOD, 0, fullauto, 0, 1,
    1,          0,          1,          0,          1,          1,          1,          1,          0,          1,          0,          0,          0,          1,          1,          1,          1,
    1,          0,          0,          0,          1,          0,          0,          1,          0,          1,          0,          0,          0,          1,          0,          0,          1
);
WPFVARK(IDF_GAMEMOD, 0, fxblend, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.5f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, fxchain, 0, 1,
    0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, fxcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   0x00FF00,   PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   0xFFFFFF,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   0x00FF00,   PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0xFF0000,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   0xFFFFFF,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0xFF8080,   0xEEEE22
);
WPFVARK(IDF_GAMEMOD, 0, fxscale, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,      1.0f,       1.0f,       1.0f,       1.1f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,      0.25f,      1.0f,       1.0f,       1.3f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,      1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,      0.25f,      1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, fxtype, FX_W_NONE, FX_W_TYPES-1,
    FX_W_NONE,  FX_W_MUZZLE1,FX_W_NONE, FX_W_MUZZLE2,FX_W_MUZZLE4,FX_W_FLAME,   FX_W_PLASMA1,FX_W_ENERGY1,FX_W_BEAM1,FX_W_SPLASH1,FX_W_NONE,FX_W_NONE,FX_W_MUZZLE6,FX_W_MUZZLE4,FX_W_NONE, FX_W_PLASMA1,FX_W_NONE,
    FX_W_NONE,  FX_W_NONE,   FX_W_NONE, FX_W_MUZZLE3,FX_W_MUZZLE5,FX_W_AIRBLAST,FX_W_PLASMA1,FX_W_ENERGY2,FX_W_BEAM2,FX_W_SPLASH2,FX_W_NONE,FX_W_NONE,FX_W_MUZZLE6,FX_W_MUZZLE5,FX_W_NONE, FX_W_PLASMA1,FX_W_NONE,
    FX_W_NONE,  FX_W_MUZZLE1,FX_W_NONE, FX_W_MUZZLE2,FX_W_MUZZLE4,FX_W_FLAME,   FX_W_PLASMA1,FX_W_ENERGY1,FX_W_BEAM1,FX_W_SPLASH1,FX_W_NONE,FX_W_NONE,FX_W_MUZZLE6,FX_W_MUZZLE4,FX_W_NONE, FX_W_PLASMA1,FX_W_NONE,
    FX_W_NONE,  FX_W_NONE,   FX_W_NONE, FX_W_MUZZLE3,FX_W_MUZZLE5,FX_W_AIRBLAST,FX_W_PLASMA1,FX_W_ENERGY2,FX_W_BEAM2,FX_W_SPLASH2,FX_W_NONE,FX_W_NONE,FX_W_MUZZLE6,FX_W_MUZZLE5,FX_W_NONE, FX_W_PLASMA1,FX_W_NONE
);
WPVARK(IDF_GAMEMOD, 0, fxtypeproj, FX_P_NONE, FX_P_TYPES-1,
    FX_P_NONE,  FX_P_BULLET,FX_P_NONE,  FX_P_PELLET,  FX_P_BULLET,  FX_P_FLAME,   FX_P_PLASMA,FX_P_ENERGY,FX_P_BEAM,FX_P_ACID,FX_P_GRENADE,FX_P_MINE,FX_P_ROCKET,FX_P_BULLET,  FX_P_NONE,  FX_P_PLASMA,FX_P_NONE,
    FX_P_NONE,  FX_P_NONE,  FX_P_NONE,  FX_P_FLAK,    FX_P_FLAK,    FX_P_AIRBLAST,FX_P_VORTEX,FX_P_ENERGY,FX_P_BEAM,FX_P_BLOB,FX_P_GRENADE,FX_P_MINE,FX_P_ROCKET,FX_P_FLAK,    FX_P_NONE,  FX_P_VORTEX,FX_P_NONE,
    FX_P_NONE,  FX_P_BULLET,FX_P_NONE,  FX_P_PELLET,  FX_P_BULLET,  FX_P_FLAME,   FX_P_PLASMA,FX_P_ENERGY,FX_P_BEAM,FX_P_ACID,FX_P_GRENADE,FX_P_MINE,FX_P_ROCKET,FX_P_BULLET,  FX_P_NONE,  FX_P_PLASMA,FX_P_NONE,
    FX_P_NONE,  FX_P_NONE,  FX_P_NONE,  FX_P_SHRAPNEL,FX_P_SHRAPNEL,FX_P_AIRBLAST,FX_P_VORTEX,FX_P_ENERGY,FX_P_BEAM,FX_P_BLOB,FX_P_GRENADE,FX_P_MINE,FX_P_ROCKET,FX_P_SHRAPNEL,FX_P_NONE,  FX_P_VORTEX,FX_P_NONE
);
WPVARM(IDF_GAMEMOD, 0, fxtypepower, FX_W_NONE, FX_W_TYPES-1,
    FX_W_NONE,  FX_W_NONE,  FX_W_NONE, FX_W_NONE,   FX_W_NONE,  FX_W_NONE,  FX_W_PLASMA_P,FX_W_ENERGY_P,FX_W_NONE,FX_W_ENERGY_P,FX_W_NONE,FX_W_NONE,  FX_W_NONE,  FX_W_NONE,  FX_W_NONE, FX_W_PLASMA1, FX_W_NONE,
    FX_W_NONE,  FX_W_NONE,  FX_W_NONE, FX_W_NONE,   FX_W_NONE,  FX_W_NONE,  FX_W_PLASMA_P,FX_W_ENERGY_P,FX_W_NONE,FX_W_ENERGY_P,FX_W_NONE,FX_W_NONE,  FX_W_NONE,  FX_W_NONE,  FX_W_NONE, FX_W_PLASMA_P,FX_W_NONE
);
WPVARK(IDF_GAMEMOD, 0, grab, 0, 3,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    3,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, guided, 0, 6,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          4,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, guideddelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, guidedspeed, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARK(IDF_GAMEMOD, 0, headmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       0.0f,       8.0f,       8.0f,       8.0f,       0.0f,       0.0f,       4.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       0.0f,       8.0f,       8.0f,       8.0f,       0.0f,       0.0f,       4.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       0.0f,       8.0f,       8.0f,       8.0f,       0.0f,       0.0f,       4.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       4.0f,       4.0f,       4.0f,       4.0f,       0.0f,       8.0f,       8.0f,       8.0f,       0.0f,       0.0f,       4.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, hitpush, FVAR_MIN, FVAR_MAX,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       15.0f,      10.0f,      25.0f,      5.0f,       60.0f,      0.0f,       150.0f,     25.0f,      25.0f,      -2.0f,      50.0f,
    100.0f,     20.0f,      50.0f,      15.0f,      10.0f,      75.0f,      -5.5f,      10.0f,      50.0f,      5.0f,       60.0f,      0.0f,       75.0f,      10.0f,      50.0f,      3.0f,       100.0f,
    50.0f,      20.0f,      25.0f,      10.0f,      25.0f,      5.0f,       15.0f,      40.0f,      10.0f,      5.0f,       60.0f,      0.0f,       150.0f,     25.0f,      25.0f,      -1.0f,      50.0f,
    100.0f,     20.0f,      50.0f,      15.0f,      10.0f,      75.0f,      -5.5f,      80.0f,      10.0f,      5.0f,       60.0f,      0.0f,       75.0f,      10.0f,      50.0f,      -5.5f,      100.0f
);
WPFVARK(IDF_GAMEMOD, 0, hitpushself, 0, FVAR_MAX,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          5,          1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          -1,         1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
WPFVARK(IDF_GAMEMOD, 0, hitvel, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, interacts, 0, 3,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          3,          3,          1,          1,          0,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          3,          1,          3,          0,          1,          0,
    0,          1,          0,          1,          1,          1,          1,          1,          1,          1,          3,          3,          1,          1,          0,          1,          0,
    0,          1,          0,          3,          3,          1,          1,          1,          1,          3,          3,          3,          1,          3,          0,          1,          0
);
WPFVARM(IDF_GAMEMOD, 0, kickpush, FVAR_MIN, FVAR_MAX,
    0.0f,       4.0f,       0.0f,       50.0f,      5.0f,       1.0f,       20.0f,      5.0f,       35.0f,      1.0f,       5.0f,       5.0f,       300.0f,     5.0f,       -250.0f,    22.0f,      0.0f,
    0.0f,       6.0f,       -100.0f,    75.0f,      25.0f,      50.0f,      300.0f,     50.0f,      50.0f,      50.0f,      5.0f,       5.0f,       300.0f,     25.0f,      -500.0f,    200.0f,     0.0f
);
WPVAR(IDF_GAMEMOD, 0, laser, 0, 1,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0
);
WPFVARK(IDF_GAMEMOD, 0, length, 0, FVAR_MAX,
    0.0f,       8.0f,       0.0f,       30.0f,      20.0f,      0.0f,       0.0f,       1024.0f,    512.0f,     0.0f,       0.0f,       4.0f,       0.0f,       20.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       10.0f,      15.0f,      0.0f,       0.0f,       1024.0f,    1024.0f,    0.0f,       0.0f,       4.0f,       0.0f,       15.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       8.0f,       0.0f,       10.0f,      15.0f,      0.0f,       0.0f,       1024.0f,    512.0f,     0.0f,       0.0f,       4.0f,       0.0f,       15.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       16.0f,      0.0f,       10.0f,      50.0f,      0.0f,       0.0f,       1024.0f,    1024.0f,    0.0f,       0.0f,       4.0f,       0.0f,       50.0f,      0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD|IDF_HEX, 0, lightcol, PC(LAST), 0xFFFFFF,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0x3F0000,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   PC(FIRE),   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0xFF8080,   0xEEEE22,
    0x907020,   0xD0D0D0,   0x1010F0,   0xF0F020,   0xF05820,   0xF0F0F0,   0x40F0C8,   PC(SHOCK),  0xF020F0,   PC(CORRODE),PC(FIRE),   0x00F068,   PC(FIRE),   0xFF4C10,   0x1040FF,   0xFF8080,   0xEEEE22
);
WPVAR(IDF_GAMEMOD, 0, lightpersist, 0, 15,
    0,          0,          1,          0,          0,          1,          0,          1,          0,          0,          8,          0,          6,          0,          1,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, lightradius, 0, FVAR_MAX,
    0,          16,         16,         16,         16,         16,         32,         8,          32,         32,         32,         32,         32,         16,         16,         32,         0
);
WPFVARK(IDF_GAMEMOD, 0, liquidcoast, 0, FVAR_MAX,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,       1.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,       1.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,       1.0f,       0.0f,
    0.0f,       2.0f,       0.0f,       2.0f,       2.0f,       1.0f,       1.0f,       1.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       2.0f,       0.0f,       1.0f,       0.0f
);
WPVAR(IDF_GAMEMOD, 0, modes, -G_ALL, G_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVAR(IDF_GAMEMOD, 0, muts, -G_M_ALL, G_M_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          -G_M_DUKE
);
WPSVARR(IDF_GAMEMOD, 0, obitsuicide,
    "hit themself",
    "ate a bullet",
    "commited seppuku",
    "tested their own shrapnel",
    "fell to their own crossfire",
    "spontaneously combusted",
    "gave themselves blue balls",
    "became a circuit breaker",
    "got a good shock",
    "melted themselves",
    "kicked it, kamikaze style",
    "found their mine",
    "exploded with style",
    "fell to their own flying flak",
    "commited bloody seppuku",
    "swung their rod wrong",
    "kicked themself"
);
WPSVARR(IDF_GAMEMOD, 0, obitobliterated,
    "clawed and mauled",
    "skewered",
    "cleaved",
    "chunkified",
    "swiss-cheesed",
    "barbequed",
    "ooze-i-fied",
    "zapped",
    "pzapped",
    "melted",
    "disassembled",
    "blew apart",
    "obliterated",
    "hole-i-fied",
    "shredded",
    "annihilated",
    "kung-fu'd"
);
WPSVARK(IDF_GAMEMOD, 0, obitheadless,
    "mauled",
    "capped",
    "diced",
    "scrambled",
    "air-conditioned",
    "char-grilled",
    "plasmafied",
    "electrocuted",
    "expertly sniped",
    "splattered",
    "blew to pieces",
    "blew apart",
    "exploded",
    "air-conditioned",
    "diced",
    "annihilated",
    "kung-fu'd",

    "mauled",
    "clobbered",
    "diced",
    "scrambled",
    "air-conditioned",
    "char-grilled",
    "plasmafied",
    "electrocuted",
    "expertly sniped",
    "splattered",
    "blew to pieces",
    "blew apart",
    "exploded",
    "air-conditioned",
    "diced",
    "annihilated",
    "kung-fu'd",

    "mauled",
    "capped",
    "sliced",
    "scrambled",
    "air-conditioned",
    "char-grilled",
    "plasmafied",
    "electrocuted",
    "expertly sniped",
    "blobbed",
    "blew to pieces",
    "blew apart",
    "exploded",
    "air-conditioned",
    "diced",
    "annihilated",
    "kung-fu'd",

    "mauled",
    "clobbered",
    "sliced",
    "scrambled",
    "air-conditioned",
    "char-grilled",
    "plasmafied",
    "electrocuted",
    "expertly sniped",
    "blobbed",
    "blew to pieces",
    "blew apart",
    "exploded",
    "air-conditioned",
    "diced",
    "annihilated",
    "kung-fu'd"
);
WPSVARK(IDF_GAMEMOD, 0, obituary,
    "clawed",
    "pierced",
    "diced",
    "sprayed",
    "perforated",
    "char-grilled",
    "plasmified",
    "electrocuted",
    "laser-shocked",
    "corroded",
    "blew to pieces",
    "blew apart",
    "exploded",
    "perforated",
    "diced",
    "atomized",
    "kicked",

    "clawed",
    "whipped",
    "sliced",
    "peppered",
    "spliced apart",
    "snuffed out",
    "enlightened",
    "electrocuted",
    "laser-scorched",
    "oxidized",
    "blew to pieces",
    "blew apart",
    "exploded",
    "perforated",
    "diced",
    "atomized",
    "kicked",

    "mauled",
    "shredded",
    "liquefied",
    "perforated",
    "air-conditioned",
    "roasted",
    "melted",
    "thunderstruck",
    "lasercuted",
    "decomposed",
    "shrapnelized",
    "fragmented",
    "obliterated",
    "perforated",
    "diced",
    "emulsified",
    "karate-indoctrinated",

    "mauled",
    "shredded",
    "liquefied",
    "perforated",
    "air-conditioned",
    "roasted",
    "melted",
    "thunderstruck",
    "lasercuted",
    "decomposed",
    "shrapnelized",
    "fragmented",
    "obliterated",
    "perforated",
    "diced",
    "emulsified",
    "karate-indoctrinated"
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVARK(IDF_GAMEMOD, 0, proxdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          5000,       0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          5000,       0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1500000,    0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, proxdist, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       32.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       FVAR_MAX,   0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, proxtime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          100,        0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, proxtype, 0, 2,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          1,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, radial, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      3.0f,       4.0f,       0.0f,       64.0f,      64.0f,      96.0f,      0.0f,       0.0f,       20.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       15.0f,      36.0f,      6.0f,       4.0f,       4.0f,       64.0f,      64.0f,      32.0f,      0.0f,       0.0f,       60.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      3.0f,       4.0f,       0.0f,       64.0f,      64.0f,      96.0f,      0.0f,       0.0f,       15.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       15.0f,      36.0f,      6.0f,       4.0f,       0.0f,       64.0f,      64.0f,      32.0f,      0.0f,       0.0f,       36.0f,      0.0f
);
WPVARK(IDF_GAMEMOD, 0, radialdelay, 0, VAR_MAX,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,          0,          0,          50,         0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          50,         0,          0,          0,          0,          0,          50,         0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,          0,          0,          50,         0,
    0,          0,          0,          0,          0,          50,         50,         0,          0,          0,          0,          0,          0,          0,          0,          50,         0
);
WPFVARK(IDF_GAMEMOD, 0, radius, FVAR_NONZERO, FVAR_MAX,
    24.0f,      1.0f,       32.0f,      1.0f,       1.0f,       0.5f,       0.5f,       1.5f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       1.0f,       32.0f,      0.75f,      16.0f,
    24.0f,      24.0f,      32.0f,      2.0f,       1.0f,       0.5f,       0.5f,       1.5f,       1.0f,       1.5f,       1.0f,       1.5f,       1.5f,       1.0f,       32.0f,      6.0f,       16.0f,
    24.0f,      1.0f,       32.0f,      1.0f,       1.0f,       0.5f,       0.5f,       1.5f,       1.0f,       1.0f,       1.0f,       1.5f,       1.5f,       1.0f,       32.0f,      0.5f,       16.0f,
    24.0f,      24.0f,      32.0f,      1.0f,       1.0f,       0.5f,       0.5f,       1.5f,       1.0f,       1.5f,       1.0f,       1.5f,       1.5f,       1.0f,       32.0f,      0.5f,       16.0f
);
WPVARM(IDF_GAMEMOD, 0, rays, 0, MAXPARAMS,
    1,          1,          1,          30,         1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          1,          30,         1,          1,          1,          1,          4,          1,          1,          1,          1
);
// recoil vars
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVARM(IDF_GAMEMOD, 0, recoilpitchdir, VAR_MIN, VAR_MAX,
    0,          0,          0,          0,          500,        0,          0,          0,          0,          0,          0,          0,          0,          500,        0,          0,          0,
    0,          0,          0,          0,          500,        0,          0,          0,          0,          0,          0,          0,          0,          500,        0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchmax, FVAR_MIN, FVAR_MAX,
    1.0f,       0.5f,       0.5f,       3.0f,       0.5f,       0.05f,      2.0f,       0.6f,       2.0f,       0.05f,      1.0f,       1.0f,       1.0f,       0.5f,       0.5f,       1.5f,       1.0f,
    1.0f,       0.0f,       1.0f,       5.0f,       0.25f,      0.1f,       10.0f,      5.0f,       5.0f,       2.5f,       1.0f,       1.0f,       1.0f,       0.25f,      1.0f,       25.0f,      1.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchmin, FVAR_MIN, FVAR_MAX,
    1.0f,       0.25f,      0.25f,      1.5f,       0.1f,       0.025f,     1.f,        0.3f,       1.0f,       0.025f,     1.0f,       1.0f,       1.0f,       0.1f,       0.25f,      0.8f,       1.0f,
    1.0f,       0.0f,       1.0f,       2.5f,       0.05f,      0.05f,      5.0f,       2.5f,       2.5f,       1.25f,      1.0f,       1.0f,       1.0f,       0.05f,      1.0f,       15.0f,      1.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.4f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.4f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.1f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.2f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchsprinting, FVAR_MIN, FVAR_MAX,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.3f,       1.2f,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f
);
WPFVARM(IDF_GAMEMOD, 0, recoilpitchinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       1.0f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       1.0f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawmax, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.25f,      0.0f,       0.0f,       0.1f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.25f,      0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawmin, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       -0.25f,     0.0f,       0.0f,       -0.1f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -0.25f,     0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawcrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawzoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawstill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawmoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawrunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawsprinting, FVAR_MIN, FVAR_MAX,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f
);
WPFVARM(IDF_GAMEMOD, 0, recoilyawinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPVARM(IDF_GAMEMOD, 0, recoiltime, VAR_MIN, VAR_MAX,
    50,         13,         50,         75,         -2,         20,         50,         50,         75,         20,         75,         50,         150,        -2,         50,         75,         0,
    80,         20,         80,         125,        100,        125,        180,        50,         125,        150,        75,         50,         150,        100,        80,         100,        0
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimecrouch, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimezoom, FVAR_MIN, FVAR_MAX,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,
    0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f,      0.75f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimestill, FVAR_MIN, FVAR_MAX,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,
    0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f,       0.8f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimemoving, FVAR_MIN, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.1f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimerunning, FVAR_MIN, FVAR_MAX,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.2f,       1.1f,
    1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f,       1.1f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimesprinting, FVAR_MIN, FVAR_MAX,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.3f,       1.2f,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f
);
WPFVARM(IDF_GAMEMOD, 0, recoiltimeinair, FVAR_MIN, FVAR_MAX,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,
    0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f,       0.5f
);
WPFVARM(IDF_GAMEMOD, 0, relativity, 0, FVAR_MAX,
    0.0f,       0.05f,      0.0f,       0.15f,      0.15f,      0.95f,      0.15f,      0.0f,       0.5f,       0.25f,      0.85f,      0.85f,      0.0f,       0.15f,      0.0f,       0.1f,       0.0f,
    0.0f,       0.05f,      0.0f,       0.35f,      0.25f,      0.35f,      0.15f,      0.0f,       0.1f,       0.25f,      0.85f,      0.85f,      0.0f,       0.25f,      0.0f,       0.15f,      0.0f
);
WPVARK(IDF_GAMEMOD, 0, residual, 0, W_R_ALL,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          0,          0,          WR(CORRODE),WR(BURN),   WR(SHOCK),  WR(BURN),   0,          WR(BLEED),  0,          0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(CORRODE),WR(BURN),   WR(SHOCK),  WR(BURN),   0,          WR(BLEED),  WR(BURN),   0,
    0,          0,          WR(BLEED),  0,          0,          WR(BURN),   0,          WR(SHOCK),  0,          WR(CORRODE),WR(BURN),   WR(SHOCK),  WR(BURN),   0,          WR(BLEED),  0,          0,
    0,          0,          WR(BLEED),  WR(BLEED),  0,          0,          0,          WR(SHOCK),  0,          WR(CORRODE),WR(BURN),   WR(SHOCK),  WR(BURN),   0,          WR(BLEED),  0,          0
);
WPVARK(IDF_GAMEMOD, 0, residualundo, 0, W_R_ALL,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          WR(BURN),   0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVARK(IDF_GAMEMOD, 0, shocktime, 0, VAR_MAX,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,
    5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500,       5500
);
WPVARK(IDF_GAMEMOD, 0, shockdelay, 0, VAR_MAX,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,
    1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000,       1000
);
WPVARK(IDF_GAMEMOD, 0, shockdamage, 0, VAR_MAX,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,
    30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30,         30
);
WPVARK(IDF_GAMEMOD, 0, shockstun, 0, W_N_ALL,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,
    W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST,     W_N_ST
);
WPFVARK(IDF_GAMEMOD, 0, shockstunscale, 0, FVAR_MAX,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.9f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.9f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPFVARK(IDF_GAMEMOD, 0, shockstunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPVARK(IDF_GAMEMOD, 0, shockstuntime, 0, VAR_MAX,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,
    500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500,        500
);
WPVARM(IDF_GAMEMOD, 0, soundskew, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,
    1,          1,          1,          1,          1,          1,          0,          1,          1,          1,          1,          1,          1,          1,          1,          0,          1
);
WPVAR(IDF_GAMEMOD, 0, spawnstay, 0, VAR_MAX,
    30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000,      30000
);
WPVAR(IDF_GAMEMOD, 0, spawntime, 0, VAR_MAX,
    15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      15000,      30000,      30000,      60000,      15000,      15000,      15000,      15000
);
WPVAR(IDF_GAMEMOD, 0, spawnduke, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          60000,      0,          0,          0,          0
);
WPVARM(IDF_GAMEMOD, 0, speed, 0, VAR_MAX,
    0,          2000,       0,          850,        2500,       400,        1250,       10000,      10000,      400,        250,        100,        850,        2500,       0,          700,        0,
    0,          1500,       0,          250,        500,        750,        85,         1000,       100000,     250,        250,        100,        500,        500,        0,          10000,      0
);
WPFVARK(IDF_GAMEMOD, 0, speeddelta, 0, FVAR_MAX,
    20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      50.0f,      20.0f,
    20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,
    20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,
    20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f,      20.0f
);
WPVARM(IDF_GAMEMOD, 0, speedlimit, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          35,         0,          0,          0,          0,          0,          0,          0,          0,          35,         0
);
WPFVARK(IDF_GAMEMOD, 0, speedmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       10.0f,      0.0f,       0.0f,       0.0f,       10.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       10.0f,      10.0f,      1.0f,       0.0f,       0.0f,       0.0f,       10.0f,      0.0f,       0.0f,       0.0f,       10.0f,      0.0f,       0.0f,       0.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,
    15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      1.0f,       15.0f,      0.0f,       15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f,      15.0f
);
WPFVARK(IDF_GAMEMOD, 0, speedmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,        0.0f,       0.0f,       0.0f,       0.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,        0.0f,       0.0f,       0.0f,       0.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,        0.0f,       0.0f,       0.0f,       0.0f,      0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,        0.0f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,        0.0f,       0.0f,       0.0f,       0.0f,      0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spread, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       35.0f,      0.0f,       5.0f,       0.0f,       0.0f,       0.0f,       5.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       25.0f,      0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      0.0f,       0.0f,       0.0f,       0.0f,       75.0f,      0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmax, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadz, 0, FVAR_MAX,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       0.0f,       0.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       2.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       0.0f,       1.0f,       0.0f,       0.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadcrouch, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadzoom, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadstill, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadmoving, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadrunning, 0, FVAR_MAX,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPFVARM(IDF_GAMEMOD, 0, spreadsprinting, 0, FVAR_MAX,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,
    1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f,       1.2f
);
WPFVARM(IDF_GAMEMOD, 0, spreadinair, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPVARK(IDF_GAMEMOD, 0, stun, 0, W_N_ALL,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ALL,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_AI,     W_N_ADD,
    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ALL,    0,          W_N_AI,     W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    W_N_ADD,    0,          W_N_ADD
);
WPFVARK(IDF_GAMEMOD, 0, stunfall, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.2f,       0.5f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.2f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       1.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
WPFVARK(IDF_GAMEMOD, 0, stunscale, 0, FVAR_MAX,
    0.5f,       0.125f,     1.0f,       0.125f,     0.2f,       0.0f,       0.1f,       0.25f,      0.3f,       1.0f,       1.0f,       4.0f,       8.0f,       0.2f,       1.0f,       0.1f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f ,     0.5f,       0.5f,       0.25f,      0.65f,      2.0f,       0.5f,       1.0f,       8.0f,       8.0f,       0.5f,       2.0f,       0.25f,      0.5f,
    0.5f ,      0.125f,     1.0f,       0.125f,     0.2f,       0.0f,       0.1f,       0.25f,      0.6f,       0.25f,      1.0f,       4.0f,       8.0f,       0.2f,       1.0f,       0.1f,       0.25f,
    0.75f,      0.125f,     2.0f,       0.25f,      0.5f,       0.5f,       0.25f,      0.65f,      4.0f,       0.25f,      1.0f,       8.0f,       8.0f,       0.5f,       2.0f,       0.25f,      0.5f
);
WPVARK(IDF_GAMEMOD, 0, stuntime, 0, VAR_MAX,
    250,        25,         300,        75,         75,         0,          0,          250,        100,        75,         200,        500,        750,        75,         300,        0,          100,
    500,        25,         500,        250,        250,        200,        200,        250,        250,        250,        200,        750,        500,        250,        500,        200,        200,
    250,        25,         300,        75,         75,         0,          0,          250,        500,        75,         200,        500,        750,        75,         300,        0,          100,
    500,        25,         500,        250,        75,         200,        200,        500,        750,        250,        200,        750,        500,        75,         500,        200,        200
);
WPVARK(IDF_GAMEMOD, 0, taper, 0, 6,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          2,          0,          0,          0,          2,          0,          2,          0,
    0,          0,          0,          0,          0,          0,          2,          0,          0,          0,          0,          0,          0,          0,          0,          2,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          2,          0,          0,          0,          2,          0,          2,          0,
    0,          0,          0,          2,          2,          2,          2,          0,          0,          0,          0,          0,          0,          2,          0,          2,          0
);
WPFVARK(IDF_GAMEMOD, 0, taperin, 0, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.05f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.33f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.1f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.33f,      0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.33f,      0.0f
);
WPFVARK(IDF_GAMEMOD, 0, taperout, 0, FVAR_MAX,
    0.01f,      0.01f,      0.01f,      0.25f,      0.75f,      0.01f,      0.01f,      0.01f,      0.01f,      0.5f,       0.01f,      0.01f,      0.01f,      0.75f,      0.01f,      0.01f,      0.01f,
    0.01f,      0.01f,      0.01f,      0.25f,      0.75f,      0.01f,      0.33f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.75f,      0.01f,      0.25f,      0.01f,
    0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.5f,       0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,
    0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.33f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.01f,      0.33f,      0.01f
);
WPFVARK(IDF_GAMEMOD, 0, tapermin, 0, 1,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,
    0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f,       0.1f
);
WPFVARK(IDF_GAMEMOD, 0, tapermax, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARM(IDF_GAMEMOD, 0, time, 1, VAR_MAX,
    200,        1000,       350,        300,        500,        200,        350,        75,         7500,       1000,       3000,       45000,      10000,      500,        350,        1500,       250,
    350,        300,        350,        1000,       500,        250,        3500,       100,        7500,       30000,      3000,       90000,      20000,      500,        350,        1000,       250
);
WPVARM(IDF_GAMEMOD, 0, timedelay, 0, VAR_MAX,
    0,          0,          10,         0,          0,          0,          0,          0,          0,          0,          175,        50,         0,          0,          10,         0,          0,
    0,          0,          10,         0,          0,          0,          75,         0,          0,          150,        175,        50,         0,          0,          10,         0,          0
);
WPVARM(IDF_GAMEMOD, 0, timeiter, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          5,          0,          0,          0,          0,          250,        0,          0,          0,          0
);
WPVARK(IDF_GAMEMOD, 0, timestick, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, visfade, 0, 1,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,
    1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f,       1.0f
);
WPVARK(IDF_GAMEMOD, 0, vistime, 0, VAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARK(IDF_GAMEMOD, 0, wavepush, 0, FVAR_MAX,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       2.5f,       2.5f,       0.5f,       2.0f,       2.0f,       3.0f,       1.5f,       1.5f,       1.6f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.5f,       2.5f,       2.5f,       1.5f,       0.5f,       2.0f,       2.0f,       2.0f,       1.5f,       1.5f,       1.1f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       0.0f,       1.5f,       1.5f,       1.5f,       0.5f,       2.0f,       2.0f,       3.0f,       1.5f,       1.5f,       1.5f,       1.5f,
    1.5f,       1.5f,       1.5f,       1.5f,       1.5f,       3.5f,       2.5f,       2.5f,       2.5f,       0.5f,       2.0f,       2.0f,       2.0f,       1.5f,       1.5f,       2.5f,       1.5f
);
WPFVARK(IDF_GAMEMOD, 0, weight, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       750.0f,     75.0f,      150.0f,     0.0f,       0.0f,       0.0f,       1.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       250.0f,     75.0f,      150.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       0.0f,       1000.0f,    75.0f,      150.0f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       250.0f,     0.0f,       0.0f,       -25.0f,     0.0f,       0.0f,       1000.0f,    75.0f,      150.0f,     0.0f,       0.0f,       0.0f,       -25.0f,     0.0f
);
WPFVARM(IDF_GAMEMOD, 0, znudge, FVAR_MIN, FVAR_MAX,
    0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.0625f,    0.0625f,    0.0625f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,
    0.0f,       0.0f,       0.0f,       0.125f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f,       0.125f,     0.0625f,    0.0625f,     0.0f,       0.0f,       0.0f,       0.0f,       0.0f
);
// these are used in modifying values in gameent::configure() et al
//  Claw        Pistol      Sword       Shotgun     SMG         Flamer      Plasma      Zapper      Rifle       Corroder    Grenade     Mine        Rocket      Minigun     Jetsaw      Eclipse     Melee
WPFVAR(IDF_GAMEMOD, 0, modhealth, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modhealthammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modhealthequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
// IMPULSE
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modimpulsespeedattack, FVAR_MIN, FVAR_MAX,
    0,          -5,         0,          -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         0,          -5,         -5,
    0,          -5,         5,          -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         5,          -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedpower, FVAR_MIN, FVAR_MAX,
    0,          -5,         0,          -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         0,          -5,         -5
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          -1,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -2,         -5,         -2,         -1,         -2,         0
);
WPFVAR(IDF_GAMEMOD, 0, modimpulsespeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5,         -5
);
// SPEED
WPFVAR(IDF_GAMEMOD, 0, modspeed, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          -8,         -5,         -8,         -8,         -8,         -10,        -8,         0,          0,          -15,        -5,         0,          -10,        0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          -2,         -2,         0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modspeedattack, FVAR_MIN, FVAR_MAX,
    0,          -5,         0,          -10,        -15,        -5,         -5,         -10,        -15,        -10,        -5,         -5,         -15,        -15,        0,          -10,        -5,
    0,          -5,         5,          -15,        -15,        -5,         -10,        -15,        -20,        -15,        -5,         -5,         -15,        -15,        5,          -25,        -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeedreload, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -10,        -5,         -10,        -5,         -10,        -5,         -5,         -5,         -5,         -10,        -5,         -10,        -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeedswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedpower, FVAR_MIN, FVAR_MAX,
    0,          -5,         0,          -5,         -5,         -5,         0,          -5,         -5,         -5,         -5,         -5,         -10,        -5,         0,          0,          -5
);
WPFVAR(IDF_GAMEMOD, 0, modspeeduse, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          -2,         -2,         -2,         -2,         -2,         -3,         -2,         -5,         -5,         -10,        -2,         0,          -2,         0
);
WPFVAR(IDF_GAMEMOD, 0, modspeedzoom, FVAR_MIN, FVAR_MAX,
    -5,         -5,         -5,         -5,         -5,         -5,         -5,         -10,        -15,        -5,         -5,         -5,         -10,        -5,         -5,         -5,         -5
);
// WEIGHT
WPFVAR(IDF_GAMEMOD, 0, modweight, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          5,          3,          6,          8,          10,         12,         5,          0,          0,          15,         3,          0,          8,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightammo, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          3,          3,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modweightattack, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightreload, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightpower, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightuse, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modweightzoom, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
// BUOYANCY
WPFVAR(IDF_GAMEMOD, 0, modbuoyancy, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyammo, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVARM(IDF_GAMEMOD, 0, modbuoyancyattack, FVAR_MIN, FVAR_MAX,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
    0,          0,           0,         0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyequip, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyreload, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyswitch, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancypower, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyuse, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);
WPFVAR(IDF_GAMEMOD, 0, modbuoyancyzoom, FVAR_MIN, FVAR_MAX,
    0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
);

#define WRS(a,b,c,d)         ((a)*(m_dm_gladiator(c, d) ? G(gladiator##b##scale) : G(b##scale)*(m_sweaps(c, d) ? G(b##limited) : 1.f)))
#define WX(k,a,b,c,d,e,f)    (!m_insta(d, e) || (a) != W_RIFLE ? WRS(WF(k, a, b, c)*f, radial, d, e) : 0)
#define WSP(a,b,c,d,e)       (!m_insta(c, d) || (a) != W_RIFLE ? clamp(W2(a, spread, b)*(e), W2(a, spreadmin, b), W2(a, spreadmax, b) > 0 ? W2(a, spreadmax, b) : FVAR_MAX) : 0.f)
#define WSND(a,b)            (weaptype[a].sound+(b))
#define WSNDF(a,b)           (weaptype[a].sound+((b) ? S_W_SECONDARY : S_W_PRIMARY))
#define WSNDFB(a,b)          (weaptype[a].sound+((b) ? S_W_SECONDARY_BEGIN : S_W_PRIMARY_BEGIN))
#define WSNDFE(a,b)          (weaptype[a].sound+((b) ? S_W_SECONDARY_END : S_W_PRIMARY_END))
#define WSND2(a,b,c)         (weaptype[a].sound+((b) ? (c)+1 : (c)))
#define WHCOL(d,a,b,c)       (getpulsehexcol(d, W2(a, b, c)))
#define WPCOL(d,a,b,c)       (getpulsecolour(d, W2(a, b, c)))

struct weaptypes
{
    int     anim,               sound,          espeed;
    bool    muzzle,     eject,      tape,       thrown;
    float   esize;
    const char *name, *item, *ammo, *vwep, *hwep;

    struct projtype
    {
        int count;
        const char *name[5];
    } proj[2], eprj[2];
};
#ifdef CPP_GAME_SERVER
weaptypes weaptype[] =
{
    {
            ANIM_CLAW,         S_CLAW,          1,
            true,       false,      true,       false,
            0,
            "claw", "", "", "weapons/claw/vwep", "weapons/claw/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_PISTOL,        S_PISTOL,       10,
            true,       true,       false,      false,
            0.45f,
            "pistol", "weapons/pistol/item", "weapons/pistol/ammo", "weapons/pistol/vwep", "weapons/pistol/hwep",
            { { 1, { "weapons/pistol/proj", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 1, { "projectiles/cartridge", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_SWORD,         S_SWORD,        1,
            true,       false,      true,       false,
            0,
            "sword", "weapons/sword/item", "weapons/sword/ammo", "weapons/sword/vwep", "weapons/sword/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_SHOTGUN,       S_SHOTGUN,      10,
            true,       true,       false,      false,
            0.6f,
            "shotgun", "weapons/shotgun/item", "weapons/shotgun/ammo", "weapons/shotgun/vwep", "weapons/shotgun/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 1, { "projectiles/shell", "", "", "", "" } }, { 1, { "projectiles/shell", "", "", "", "" } } }
    },
    {
            ANIM_SMG,           S_SMG,          20,
            true,       true,       false,      false,
            0.45f,
            "smg", "weapons/smg/item", "weapons/smg/ammo", "weapons/smg/vwep", "weapons/smg/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 1, { "projectiles/cartridge", "", "", "", "" } }, { 1, { "projectiles/cartridge", "", "", "", "" } } }
    },
    {
            ANIM_FLAMER,        S_FLAMER,       1,
            true,       true,       false,      false,
            0,
            "flamer", "weapons/flamer/item", "weapons/flamer/ammo", "weapons/flamer/vwep", "weapons/flamer/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_PLASMA,        S_PLASMA,       1,
            true,       false,      false,      false,
            0,
            "plasma", "weapons/plasma/item", "weapons/plasma/ammo", "weapons/plasma/vwep", "weapons/plasma/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_ZAPPER,        S_ZAPPER,       1,
            true,       false,      true,       false,
            0,
            "zapper", "weapons/zapper/item", "weapons/zapper/ammo", "weapons/zapper/vwep", "weapons/zapper/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_RIFLE,         S_RIFLE,        1,
            true,       false,      false,      false,
            0,
            "rifle", "weapons/rifle/item", "weapons/rifle/ammo", "weapons/rifle/vwep", "weapons/rifle/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_CORRODER,      S_CORRODER,      5,
            true,       false,       false,      false,
            0.25f,
            "corroder", "weapons/corroder/item", "weapons/corroder/ammo", "weapons/corroder/vwep", "weapons/corroder/hwep",
            {
                { 0, { "", "", "", "", "" } },
                { 5, { "weapons/corroder/proj/1", "weapons/corroder/proj/2", "weapons/corroder/proj/3", "weapons/corroder/proj/4", "weapons/corroder/proj/5" } }
            },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_GRENADE,       S_GRENADE,      1,
            false,      false,      false,      true,
            0,
            "grenade", "weapons/grenade/item", "weapons/grenade/ammo", "weapons/grenade/vwep", "weapons/grenade/hwep",
            { { 1, { "weapons/grenade/proj", "", "", "", "" } }, { 1, { "weapons/grenade/proj", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_MINE,          S_MINE,         1,
            false,      false,      false,      true,
            0,
            "mine", "weapons/mine/item", "weapons/mine/ammo", "weapons/mine/vwep", "weapons/mine/hwep",
            { { 1, { "weapons/mine/proj", "", "", "", "" } }, { 1, { "weapons/mine/proj", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_ROCKET,        S_ROCKET,       1,
            true,      false,       false,      false,
            0,
            "rocket", "weapons/rocket/item", "weapons/rocket/ammo", "weapons/rocket/vwep", "weapons/rocket/hwep",
            { { 1, { "weapons/rocket/proj", "", "", "", "" } }, { 1, { "weapons/rocket/proj", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_MINIGUN,       S_MINIGUN,      20,
            true,       true,       false,      false,
            0.45f,
            "minigun", "weapons/minigun/item", "weapons/minigun/ammo", "weapons/minigun/vwep", "weapons/minigun/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 1, { "projectiles/cartridge", "", "", "", "" } }, { 1, { "projectiles/cartridge", "", "", "", "" } } }
    },
    {
            ANIM_JETSAW,        S_JETSAW,        1,
            true,       false,      true,       false,
            0,
            "jetsaw", "weapons/jetsaw/item", "weapons/jetsaw/ammo", "weapons/jetsaw/vwep", "weapons/jetsaw/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_ECLIPSE,       S_ECLIPSE,       1,
            true,       false,      false,      false,
            0,
            "eclipse", "weapons/eclipse/item", "weapons/eclipse/ammo", "weapons/eclipse/vwep", "weapons/eclipse/hwep",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    },
    {
            ANIM_CLAW,          S_MELEE,        1,
            false,      false,      false,      false,
            0,
            "melee",    "", "", "", "",
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } },
            { { 0, { "", "", "", "", "" } }, { 0, { "", "", "", "", "" } } }
    }
};
#else
extern weaptypes weaptype[];
#endif
