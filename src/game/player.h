// Player and other actor definitions

enum {
    A_PLAYER = 0, A_BOT, A_TURRET, A_GRUNT, A_DRONE, A_ROLLER, A_HAZARD, A_JANITOR, A_MAX,
    A_ENEMY = A_TURRET, A_ENVIRONMENT = A_HAZARD, A_TOTAL = A_MAX - A_ENEMY, A_CLAMP = A_ENVIRONMENT - A_ENEMY
};
struct actor
{
    const char *name;
    int id, collidezones;
    bool hastags, jetfx, weapfx, steps, onlyfwd, pieces;
    float height, radius;
    const char *mdl;
};
#ifdef CPP_GAME_SERVER
actor actors[] = {
    { "player",         A_PLAYER,   CLZ_ALL,    true,   true,   true,   true,   false,  true,   20.4f,      4.25f,  NULL },
    { "bot",            A_BOT,      CLZ_ALL,    true,   true,   true,   true,   false,  true,   20.4f,      4.25f,  NULL },
    { "turret",         A_TURRET,   CLZ_ALL,    true,   true,   true,   true,   false,  true,   20.4f,      4.25f,  NULL },
    { "grunt",          A_GRUNT,    CLZ_NOHEAD, true,   true,   true,   true,   false,  true,   18.5f,      4.25f,  NULL },
    { "drone",          A_DRONE,    CLZ_NOHEAD, true,   true,   true,   true,   false,  true,   18.5f,      4.25f,  NULL },
    { "roller",         A_ROLLER,   CLZ_NONE,   false,  false,  false,  false,  true,   false,  11.475f,    5.75f,  "actors/roller" },
    { "hazard",         A_HAZARD,   CLZ_NONE,   false,  false,  false,  false,  false,  false,  1.f,        1.f,    NULL },
    { "clenaer",        A_JANITOR,  CLZ_NONE,   false,  false,  false,  false,  false,  false,  11.475f,    5.75f,  "actors/roller" },
};
#else
extern actor actors[];
#endif

#define AA(type) (1<<A_A_##type)
enum
{
    A_A_MOVE = 0, A_A_JUMP, A_A_CROUCH, A_A_MELEE, A_A_PRIMARY, A_A_SECONDARY, A_A_PUSHABLE, A_A_AFFINITY, A_A_REGEN, A_A_KAMIKAZE, A_A_GIBS, A_A_DAMAGE, A_A_AMMO, A_A_FLOAT, A_A_MAX,
    A_A_ATTACK = (1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_KAMIKAZE),
    A_A_ALL = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_GIBS)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO)|(1<<A_A_FLOAT),
    A_A_PLAYER = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_GIBS)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO),
    A_A_MOVINGAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_GIBS)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO),
    A_A_LESSAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_GIBS)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO),
    A_A_FIXEDAI = (1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO),
    A_A_ROLLER = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_PUSHABLE)|(1<<A_A_KAMIKAZE)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO),
    A_A_HAZARD = (1<<A_A_PRIMARY),
    A_A_JANITOR = (1<<A_A_MOVE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_DAMAGE)|(1<<A_A_REGEN)|(1<<A_A_FLOAT)
};

enum
{
    A_C_PLAYERS = 0, A_C_BOTS, A_C_ENEMIES, A_C_MAX,
    A_C_ALL = (1<<A_C_PLAYERS)|(1<<A_C_BOTS)|(1<<A_C_ENEMIES)
};

enum
{
    A_T_PLAYERS = 0, A_T_BOTS, A_T_ENEMIES, A_T_GHOST, A_T_MAX,
    A_T_ALL = (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES)|(1<<A_T_GHOST),
    A_T_PLAYER = (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES),
    A_T_AI = (1<<A_T_BOTS)|(1<<A_T_ENEMIES)
};

#define T_ENUM(en, um) \
    en(um, neutral, NEUTRAL) en(um, alpha, ALPHA) en(um, omega, OMEGA) \
    en(um, enemy, ENEMY) en(um, environment, ENVIRONMENT) en(um, max, MAX)
ENUM_DLN(T);

ENUM_VAR(T_FIRST, T_ALPHA);
ENUM_VAR(T_LAST, T_OMEGA);
ENUM_VAR(T_COUNT, T_LAST + 1);
ENUM_VAR(T_NUM, T_LAST - T_FIRST + 1);

enum
{
    TT_INFO = 1<<0, TT_RESET = 1<<1, TT_SMODE = 1<<2,
    TT_INFOSM = TT_INFO|TT_SMODE,
    TT_RESETX = TT_INFO|TT_RESET
};

#ifdef CPP_GAME_SERVER
int mapbals[T_NUM][T_NUM] = {
    { T_ALPHA, T_OMEGA },
    { T_OMEGA, T_ALPHA },
};
#else
extern int mapbals[T_NUM][T_NUM];
#endif

#include "teamdef.h"

TPSVAR(IDF_GAMEMOD, 0, name,
    "Neutral",  "Alpha",    "Omega",    "Enemy",    "Environment"
);
TPVAR(IDF_GAMEMOD|IDF_HEX, 0, colour, 0, 0xFFFFFF,
    0x707070,   0x1040F8,   0xFF3210,   0xE0E020,   0x1EC850
);

struct score
{
    int team, total;
    score(int s = -1, int n = 0) : team(s), total(n) {}
    ~score() {}
};

#define numteams(a,b)   (m_team(a,b) ? T_NUM : 1)
#define teamcount(a,b)  (m_team(a,b) ? T_COUNT : 1)
#define isteam(a,b,c,d) (m_team(a,b) ? (c >= d && c <= numteams(a,b)) : c == T_NEUTRAL)
#define valteam(a,b)    (a >= b && a <= T_NUM)

enum
{
    TAG_CAMERA, TAG_CROWN, TAG_R_CROWN, TAG_TORSO, TAG_R_TORSO, TAG_LIMBS, TAG_R_LIMBS, TAG_WAIST,
    TAG_MUZZLE, TAG_ORIGIN, TAG_EJECT1, TAG_EJECT2, TAG_JET_LEFT, TAG_JET_RIGHT, TAG_JET_BACK, TAG_TOE_LEFT, TAG_TOE_RIGHT,
    TAG_MAX, // WARNING: ensure this value is more than or equal to VANITYMAX (see bottom of file)
    TAG_EJECT = TAG_EJECT1, TAG_N_EJECT = 2, TAG_JET = TAG_JET_LEFT, TAG_N_JET = 3, TAG_TOE = TAG_TOE_LEFT, TAG_N_TOE = 2
};

#define PLAYERTYPES 2
#define PLAYERPATTERNS 17
#define PLAYERPARTS 14

struct playerpattern
{
    const char *filename;
    const char *id;
    const char *name;
    int clamp;
    float scale;
};

struct playerpart
{
    const char *filename;
    int tag;
    float x, y, z;
};

#ifdef CPP_GAME_SERVER
extern const char * const playertypes[PLAYERTYPES][7] = {
    { "actors/player/male/hwep",        "actors/player/male/headless",      "actors/player/male/body",      "actors/player/male",       "player",   "male",     "Male" },
    { "actors/player/female/hwep",      "actors/player/female/headless",    "actors/player/male/body",      "actors/player/female",     "player",   "female",   "Female" }
};

// final entry is texscale in playerpattern (512x -> 2 , 1024x -> 1)
extern const playerpattern playerpatterns[PLAYERPATTERNS] = {
    { "<grey>textures/patterns/default",    "default",  "Default",  0,      1 },
    { "<grey>textures/patterns/soft",       "soft",     "Soft",     0,      1 },
    { "<grey>textures/patterns/camo",       "camo",     "Camo",     0,      1 },
    { "<grey>textures/patterns/heart",      "heart",    "Heart",    0x300,  2 },
    { "<grey>textures/patterns/crown",      "crown",    "Crown",    0x300,  2 },
    { "<grey>textures/patterns/zebra",      "zebra",    "Zebra",    0x300,  2 },
    { "<grey>textures/patterns/checker",    "checker",  "Checker",  0x300,  2 },
    { "<grey>textures/patterns/star",       "star",     "Star",     0x300,  2 },
    { "<grey>textures/patterns/flower",     "flower",   "Flower",   0x300,  2 },
    { "<grey>textures/patterns/leopard",    "leopard",  "Leopard",  0x300,  2 },
    { "<grey>textures/patterns/zigzag",     "zigzag",   "Zigzag",   0x300,  2 },
    { "<grey>textures/patterns/pixel",      "pixel",    "Pixel",    0x300,  2 },
    { "<grey>textures/patterns/circle",     "circle",   "Circle",   0x300,  2 },
    { "<grey>textures/patterns/mutant",     "mutant",   "Mutant",   0x300,  2 },
    { "<grey>textures/patterns/ninja",      "ninja",    "Ninja",    0,      2 },
    { "<grey>textures/patterns/lines",      "lines",    "Lines",    0,      1 },
    { "<grey>textures/patterns/softhero",   "softhero", "Softhero", 0,      1 },
};

extern const playerpart playerparts[PLAYERPARTS] = {
    { "projectiles/parts/torso/lower",      TAG_WAIST,          0,      0,      0.1f },
    { "projectiles/parts/torso/uper",       TAG_WAIST,          0,      0,      0.25f },
    { "projectiles/parts/foot/left",        TAG_TOE_LEFT,       0,      0,      0 },
    { "projectiles/parts/foot/right",       TAG_TOE_RIGHT,      0,      0,      0 },
    { "projectiles/parts/calf/left",        TAG_TOE_LEFT,       0,      0,      0.125f },
    { "projectiles/parts/calf/right",       TAG_TOE_RIGHT,      0,      0,      0.125f },
    { "projectiles/parts/thigh/left",       TAG_TOE_LEFT,       0,      0,      0.25f },
    { "projectiles/parts/thigh/right",      TAG_TOE_RIGHT,      0,      0,      0.25f },
    { "projectiles/parts/hand/left",        TAG_JET_LEFT,       0.5f,   0.5f,   -0.125f },
    { "projectiles/parts/hand/right",       TAG_JET_RIGHT,      0.5f,   0.5f,   -0.125f },
    { "projectiles/parts/forearm/left",     TAG_JET_LEFT,       0.25f,  0.25f,  -0.1f },
    { "projectiles/parts/forearm/right",    TAG_JET_RIGHT,      0.25f,  0.25f,  -0.1f },
    { "projectiles/parts/upperarm/left",    TAG_JET_LEFT,       0.125f, 0.125f, -0.075f },
    { "projectiles/parts/upperarm/right",   TAG_JET_RIGHT,      0.125f, 0.125f, -0.075f },
};
#else
extern const char * const playertypes[PLAYERTYPES][7];
extern const playerpattern playerpatterns[PLAYERPATTERNS];
extern const playerpart playerparts[PLAYERPARTS];
#endif

#include "playerdef.h"

APSVAR(IDF_GAMEMOD, 0, vname,
    "Player",       "Bot",          "Turret",       "Grunt",        "Drone",        "Roller",       "Hazard",       "Janitor"
);
APVAR(IDF_GAMEMOD, 0, abilities, 0, A_A_ALL,
    A_A_PLAYER,     A_A_MOVINGAI,   A_A_FIXEDAI,    A_A_MOVINGAI,   A_A_LESSAI,     A_A_ROLLER,     A_A_HAZARD,     A_A_JANITOR
);
APFVAR(IDF_GAMEMOD, 0, aipitchscale, FVAR_NONZERO, FVAR_MAX,
    0.5f,           0.5f,           0.5f,           0.5f,           0.5f,           0.5f,           1,              4
);
APFVAR(IDF_GAMEMOD, 0, aiyawscale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              1,              1,              1,              1,              1,              2
);
APVAR(IDF_GAMEMOD, 0, collide, 0, A_C_ALL,
    A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL
);
APVAR(IDF_GAMEMOD, 0, health, 1, VAR_MAX,
    1000,           1000,           1000,           500,            500,            500,            1000,           2000
);
APVAR(IDF_GAMEMOD, 0, hurtstop, 0, VAR_MAX,
    0,              0,              0,              250,            500,            250,            0,              0
);
APVAR(IDF_GAMEMOD, 0, impulse, 0, IM_T_ALL,
    IM_T_ALL,       IM_T_MVAI,      0,              IM_T_MVAI,      IM_T_LSAI,      IM_T_ROLLER,    0,              0
);
APFVAR(IDF_GAMEMOD, 0, liquidboost, 0, FVAR_MAX,
    0,              1,              1,              1,              1,              1,              0,              0
);
APVAR(IDF_GAMEMOD, 0, magboots, 0, 1,
    1,              1,              1,              1,              1,              1,              0,              0
);
APVAR(IDF_GAMEMOD, 0, maxcarry, 0, W_LOADOUT,
    2,              2,              0,              0,              0,              0,              W_LOADOUT,      0
);
APFVAR(IDF_GAMEMOD, 0, scale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              0.5f,           1,              0.85f,          1,              1,              0.5f
);
APVAR(IDF_GAMEMOD, 0, spawndelay, DEATHMILLIS, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawndelayedit, DEATHMILLIS, VAR_MAX,
    DEATHMILLIS,    5000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawndelaybomber, DEATHMILLIS, VAR_MAX,
    3000,           3000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawndelaycapture, DEATHMILLIS, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawndelaydefend, DEATHMILLIS, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawndelaygauntlet, DEATHMILLIS, VAR_MAX,
    3000,           3000,           30000,          30000,          30000,          30000,          DEATHMILLIS,    DEATHMILLIS
);
APFVAR(IDF_GAMEMOD, 0, spawndelayinstascale, 0, FVAR_MAX,
    0.5f,           0.5f,           0.75f,          0.75f,          0.75f,          0.75f,          1,              1
);
APVAR(IDF_GAMEMOD, 0, spawndelayrace, DEATHMILLIS, VAR_MAX,
    1000,           1000,           10000,          10000,          10000,          10000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawngrenades, 0, 2,
    0,              0,              0,              0,              0,              0,              0,              2
);
APVAR(IDF_GAMEMOD, 0, spawnmines, 0, 2,
    0,              0,              0,              0,              0,              0,              0,              2
);
APVAR(IDF_GAMEMOD, 0, teamdamage, 0, A_T_ALL,
    A_T_PLAYER,     A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI,         A_T_PLAYER,     A_T_PLAYER
);
APVAR(IDF_GAMEMOD, 0, weapongladiator, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponinsta, 0, W_ALL-1,
    W_RIFLE,        W_RIFLE,        W_RIFLE,        W_RIFLE,        W_CLAW,         W_CLAW,         W_RIFLE,        W_RIFLE
);
APVAR(IDF_GAMEMOD, 0, weaponkaboom, 0, W_ALL-1,
    W_GRENADE,      W_GRENADE,      W_GRENADE,      W_GRENADE,      W_CLAW,         W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponmedieval, 0, W_ALL-1,
    W_SWORD,        W_SWORD,        W_RIFLE,        W_SWORD,        W_CLAW,         W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponrace, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponspawn, 0, W_ALL-1,
    W_PISTOL,       W_PISTOL,       W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW,         W_PISTOL,       W_ZAPPER
);
// these are modified by gameent::configure() et al
APFVAR(IDF_GAMEMOD, 0, speed, 0, FVAR_MAX,
    100,            100,            0,              90,             100,            50,             100,            175
);
APFVAR(IDF_GAMEMOD, 0, speedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, jumpspeed, 0, FVAR_MAX,
    140,            140,            0,              140,            140,            70,             100,            100
);
APFVAR(IDF_GAMEMOD, 0, jumpspeedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, jumptolerance, 0, FVAR_MAX,
    100,            100,            1000,           100,            100,            100,            100,            100
);
APFVAR(IDF_GAMEMOD, 0, impulsespeed, 0, FVAR_MAX,
    100,            100,            0,              100,            100,            50,             100,            100
);
APFVAR(IDF_GAMEMOD, 0, impulsespeedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, weight, 0, FVAR_MAX,
    250,            250,            250,            250,            200,            200,            0,              0
);
APFVAR(IDF_GAMEMOD, 0, weightextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, buoyancy, 0, FVAR_MAX,
    500,            500,            0,              500,            400,            400,            0,              0
);
APFVAR(IDF_GAMEMOD, 0, buoyancyextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);

// WARNING: ensure this value is less than or equal to TAG_MAX (see above)
#define VANITYMAX 16
// WARNING: ensure this value is at least equal to VANITYMAX (currently only needs 14 of them)
#define ATTACHMENTMAX 16

struct vanity
{
    int type, cond, style;
    char *ref, *model, *proj, *name, *tag;
    vector<char *> files;

    vanity() : type(-1), cond(0), style(0), ref(NULL), model(NULL), proj(NULL), name(NULL), tag(NULL) {}
    vanity(int t, const char *r, const char *n, const char *g, int c, int s) : type(t), cond(c), style(s), ref(newstring(r)), model(NULL), proj(NULL), name(newstring(n)), tag(newstring(g)) { setmodel(r); }
    ~vanity()
    {
        if(ref) delete[] ref;
        if(model) delete[] model;
        if(proj) delete[] proj;
        if(name) delete[] name;
        if(tag) delete[] tag;
        files.deletearrays();
    }

    void setmodel(const char *r)
    {
        if(model) delete[] model;
        defformatstring(m, "vanities/%s", r);
        model = newstring(m);
        if(proj) delete[] proj;
        formatstring(m, "%s/proj", model);
        proj = newstring(m);
    }
};
#ifdef CPP_GAME_MAIN
vector<vanity> vanities;
vector<char *> vanitytypetags;
#else
extern vector<vanity> vanities;
extern vector<char *> vanitytypetags;
#endif
