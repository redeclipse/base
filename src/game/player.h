// Player and other actor definitions

#define A_ENUM(en, um) \
    en(um, player, PLAYER) en(um, bot, BOT) en(um, turret, TURRET) en(um, grunt, GRUNT) \
    en(um, drone, DRONE) en(um, roller, ROLLER) en(um, hazard, HAZARD) en(um, janitor, JANITOR) \
    en(um, max, MAX)
ENUM_DLN(A);
ENUM_VAR(A_ENEMY, A_TURRET);
ENUM_VAR(A_ENVIRONMENT, A_HAZARD);
ENUM_VAR(A_TOTAL, A_MAX - A_ENEMY);
ENUM_VAR(A_CLAMP, A_ENVIRONMENT - A_ENEMY);

struct actor
{
    const char *name;
    int id, collidezones, hastags, parts, mdlflags;
    bool hashead, jetfx, weapfx, weapmdl, steps, onlyfwd, isplayer, mdlyaw, mdlpitch;
    float height, radius, aboveeye;
    const char *mdl;
};

#define PLAYERTYPES 2
#define PLAYERPARTS 14
#define JANITORPARTS 7

#ifdef CPP_GAME_SERVER
actor actors[] = {
    { "player",         A_PLAYER,   CLZ_ALL,    2,  PLAYERPARTS,    0,                      true,   true,   true,   true,   true,   false,  true,   true,   true,   20.4f,      4.25f,  1.0f,   NULL },
    { "bot",            A_BOT,      CLZ_ALL,    2,  PLAYERPARTS,    0,                      true,   true,   true,   true,   true,   false,  true,   true,   true,   20.4f,      4.25f,  1.0f,   NULL },
    { "turret",         A_TURRET,   CLZ_NONE,   1,  0,              MDL_CULL_DIST,          false,  false,  true,   true,   false,  false,  false,  true,   true,   12.0f,      6.0f,   1.0f,   "actors/turret/sphere" },
    { "grunt",          A_GRUNT,    CLZ_NOHEAD, 2,  0,              MDL_CULL_DIST,          false,  true,   true,   true,   true,   false,  true,   true,   true,   18.5f,      4.25f,  1.0f,   NULL },
    { "drone",          A_DRONE,    CLZ_NONE,   1,  0,              MDL_CULL_DIST,          false,  false,  true,   false,  false,  false,  false,  true,   true,   5.0f,       2.5f,   1.0f,   "actors/drone" },
    { "roller",         A_ROLLER,   CLZ_NONE,   0,  0,              MDL_CULL_DIST,          false,  false,  false,  false,  false,  true,   false,  true,   true,   11.475f,    5.75f,  1.0f,   "actors/roller" },
    { "hazard",         A_HAZARD,   CLZ_NONE,   0,  0,              MDL_CULL_DIST,          false,  false,  false,  false,  false,  false,  false,  true,   true,   2.0f,       1.0f,   1.0f,   "" },
    { "janitor",        A_JANITOR,  CLZ_NONE,   1,  JANITORPARTS,   MDL_FORCETRANSPARENT,   false,  false,  true,   false,  false,  false,  false,  false,  false,  3.0f,       4.5f,   1.0f,   "actors/janitor" },
};
#else
extern actor actors[];
#endif

#define AA(type) (1<<A_A_##type)

#define A_A_ENUM(en, um) \
    en(um, move, MOVE) en(um, jump, JUMP) en(um, crouch, CROUCH) en(um, melee, MELEE) \
    en(um, primary, PRIMARY) en(um, secondary, SECONDARY) en(um, pushable, PUSHABLE) \
    en(um, affinity, AFFINITY) en(um, regen, REGEN) en(um, kamikaze, KAMIKAZE) \
    en(um, living, LIVING) en(um, damage, DAMAGE) en(um, ammo, AMMO) en(um, float, FLOAT) \
    en(um, max, MAX)
ENUM_DLN(A_A);
ENUM_VAR(A_A_ATTACK, (1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_KAMIKAZE));
ENUM_VAR(A_A_ALL, (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_LIVING)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO)|(1<<A_A_FLOAT));
ENUM_VAR(A_A_PLAYER, (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_LIVING)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO));
ENUM_VAR(A_A_BOT, (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_LIVING)|(1<<A_A_DAMAGE)|(1<<A_A_AMMO));
ENUM_VAR(A_A_GRUNT, (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_LIVING)|(1<<A_A_DAMAGE));
ENUM_VAR(A_A_DRONE, (1<<A_A_MOVE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN)|(1<<A_A_DAMAGE)|(1<<A_A_FLOAT));
ENUM_VAR(A_A_TURRET, (1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_DAMAGE)|(1<<A_A_REGEN)|(1<<A_A_FLOAT));
ENUM_VAR(A_A_ROLLER, (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_PUSHABLE)|(1<<A_A_KAMIKAZE)|(1<<A_A_DAMAGE));
ENUM_VAR(A_A_HAZARD, (1<<A_A_PRIMARY));
ENUM_VAR(A_A_JANITOR, (1<<A_A_MOVE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_DAMAGE)|(1<<A_A_REGEN)|(1<<A_A_FLOAT));

#define A_C_ENUM(en, um) \
    en(um, players, PLAYERS) en(um, bots, BOTS) en(um, enemies, ENEMIES) en(um, max, MAX)
ENUM_DLN(A_C);
ENUM_VAR(A_C_ALL, (1<<A_C_PLAYERS)|(1<<A_C_BOTS)|(1<<A_C_ENEMIES));

#define A_T_ENUM(en, um) \
    en(um, players, PLAYERS) en(um, bots, BOTS) en(um, enemies, ENEMIES) en(um, ghost, GHOST) en(um, max, MAX)
ENUM_DLN(A_T);
ENUM_VAR(A_T_ALL, (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES)|(1<<A_T_GHOST));
ENUM_VAR(A_T_PLAYER, (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES));
ENUM_VAR(A_T_AI, (1<<A_T_BOTS)|(1<<A_T_ENEMIES));

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
    0x808080,   0x3030F0,   0xF03030,   0xF0F030,   0x30F030
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

#define TAG_ENUM(en, um) \
    en(um, Camera, CAMERA) en(um, Crown, CROWN) en(um, Crown Radius, R_CROWN) en(um, Torso, TORSO) \
    en(um, Torso Radius, R_TORSO) en(um, Limbs, LIMBS) en(um, Limbs Radius, R_LIMBS) en(um, Waist, WAIST) \
    en(um, Origin, ORIGIN) en(um, Muzzle 1, MUZZLE1) en(um, Muzzle 2, MUZZLE2) en(um, Eject 1, EJECT1) en(um, Eject 2, EJECT2) \
    en(um, Left Jet, JET_LEFT) en(um, Right Jet, JET_RIGHT) en(um, Back Jet, JET_BACK) \
    en(um, Left Toe, TOE_LEFT) en(um, Right Toe, TOE_RIGHT) en(um, Max, MAX)
ENUM_DLN(TAG);
ENUM_VAR(TAG_MUZZLE, TAG_MUZZLE1);
ENUM_VAR(TAG_N_MUZZLE, 2);
ENUM_VAR(TAG_EJECT, TAG_EJECT1);
ENUM_VAR(TAG_N_EJECT, 2);
ENUM_VAR(TAG_JET, TAG_JET_LEFT);
ENUM_VAR(TAG_N_JET, 3);
ENUM_VAR(TAG_TOE, TAG_TOE_LEFT);
ENUM_VAR(TAG_N_TOE, 2);

struct actorpart
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

extern const actorpart playerparts[PLAYERPARTS] = {
    { "projectiles/parts/player/torso/lower",      TAG_WAIST,          0,      0,      0.1f },
    { "projectiles/parts/player/torso/upper",      TAG_WAIST,          0,      0,      0.25f },
    { "projectiles/parts/player/foot/left",        TAG_TOE_LEFT,       0,      0,      0 },
    { "projectiles/parts/player/foot/right",       TAG_TOE_RIGHT,      0,      0,      0 },
    { "projectiles/parts/player/calf/left",        TAG_TOE_LEFT,       0,      0,      0.125f },
    { "projectiles/parts/player/calf/right",       TAG_TOE_RIGHT,      0,      0,      0.125f },
    { "projectiles/parts/player/thigh/left",       TAG_TOE_LEFT,       0,      0,      0.25f },
    { "projectiles/parts/player/thigh/right",      TAG_TOE_RIGHT,      0,      0,      0.25f },
    { "projectiles/parts/player/hand/left",        TAG_JET_LEFT,       0.5f,   0.5f,   -0.125f },
    { "projectiles/parts/player/hand/right",       TAG_JET_RIGHT,      0.5f,   0.5f,   -0.125f },
    { "projectiles/parts/player/forearm/left",     TAG_JET_LEFT,       0.25f,  0.25f,  -0.1f },
    { "projectiles/parts/player/forearm/right",    TAG_JET_RIGHT,      0.25f,  0.25f,  -0.1f },
    { "projectiles/parts/player/upperarm/left",    TAG_JET_LEFT,       0.125f, 0.125f, -0.075f },
    { "projectiles/parts/player/upperarm/right",   TAG_JET_RIGHT,      0.125f, 0.125f, -0.075f },
};
extern const actorpart janitorparts[JANITORPARTS] = {
    { "projectiles/parts/janitor/armlower",         TAG_TORSO,          0,      0,      -0.5f },
    { "projectiles/parts/janitor/armupper",         TAG_TORSO,          0,      0,      -0.3f },
    { "projectiles/parts/janitor/bladeslower",      TAG_TORSO,          0,      0,      -0.1f },
    { "projectiles/parts/janitor/bladesupper",      TAG_TORSO,          0,      0,      0.1f },
    { "projectiles/parts/janitor/centre",           TAG_TORSO,          0,      0,      0 },
    { "projectiles/parts/janitor/flap",             TAG_TORSO,          0,      0,      0 },
    { "projectiles/parts/janitor/ring",             TAG_TORSO,          0,      0,      0 },
};
#else
extern const char * const playertypes[PLAYERTYPES][7];
extern const actorpart playerparts[PLAYERPARTS];
extern const actorpart janitorparts[JANITORPARTS];
#endif

#include "playerdef.h"

APSVAR(IDF_GAMEMOD, 0, vname,
    "Player",       "Bot",          "Turret",       "Grunt",        "Drone",        "Roller",       "Hazard",       "Janitor"
);
APVAR(IDF_GAMEMOD, 0, abilities, 0, A_A_ALL,
    A_A_PLAYER,     A_A_BOT,        A_A_TURRET,     A_A_GRUNT,      A_A_DRONE,      A_A_ROLLER,     A_A_HAZARD,     A_A_JANITOR
);
APVAR(IDF_GAMEMOD, 0, aihackchance, 0, VAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              100
);
APVAR(IDF_GAMEMOD, 0, aihackweap, 0, W_MAX-1,
    0,              0,              0,              0,              0,              0,              0,              W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, aihacktype, 0, 15,
    0,              0,              0,              0,              0,              0,              0,              15
);
APFVAR(IDF_GAMEMOD, 0, aifloatheight, 0, FVAR_MAX,
    8.0f,           8.0f,           8.0f,           8.0f,           24.0f,          8.0f,           0.0f,           48.0f
);
APFVAR(IDF_GAMEMOD, 0, aifloatduck, 0, FVAR_MAX,
    4.0f,           4.0f,           4.0f,           4.0f,           12.0f,          4.0f,           0.0f,           16.0f
);
APFVAR(IDF_GAMEMOD, 0, aipitchangle, 0, 89.9f,
    22.5f,          22.5f,          22.5f,          22.5f,          22.5f,          22.5f,          22.5f,          22.5f
);
APFVAR(IDF_GAMEMOD, 0, aipitchscale, FVAR_NONZERO, FVAR_MAX,
    0.5f,           0.5f,           0.5f,           0.5f,           1,              0.5f,           1,              2
);
APFVAR(IDF_GAMEMOD, 0, aiyawscale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              1,              1,              1,              1,              1,              2
);
APVAR(IDF_GAMEMOD, 0, collide, 0, A_C_ALL,
    A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL
);
APFVAR(IDF_GAMEMOD, 0, damage, 1, FVAR_MAX,
    1.0f,           1.0f,           1.0f,           1.0f,           1.0f,           1.0f,           1.0f,           0.25f
);
APVAR(IDF_GAMEMOD, 0, health, 1, VAR_MAX,
    1000,           1000,           1000,           500,            500,            500,            1000,           1000
);
APVAR(IDF_GAMEMOD, 0, hurtstop, 0, VAR_MAX,
    0,              0,              0,              250,            250,            250,            0,              50
);
APVAR(IDF_GAMEMOD, 0, impulse, 0, IM_T_ALL,
    IM_T_ALL,       IM_T_MVAI,      0,              IM_T_MVAI,      0,              IM_T_ROLLER,    0,              0
);
APFVAR(IDF_GAMEMOD, 0, liquidboost, 0, FVAR_MAX,
    8,              8,              8,              8,              8,              8,              8,              8
);
APVAR(IDF_GAMEMOD, 0, maxcarry, 0, W_LOADOUT,
    2,              2,              0,              0,              0,              0,              W_LOADOUT,      0
);
APFVAR(IDF_GAMEMOD, 0, scale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              1,              1,              1,              1,              1,              1
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
APVAR(IDF_GAMEMOD, 0, spawndelayspeedrun, DEATHMILLIS, VAR_MAX,
    1000,           1000,           10000,          10000,          10000,          10000,          DEATHMILLIS,    DEATHMILLIS
);
APVAR(IDF_GAMEMOD, 0, spawngrenades, 0, 2,
    0,              0,              0,              0,              0,              0,              0,              0
);
APVAR(IDF_GAMEMOD, 0, spawnmines, 0, 2,
    0,              0,              0,              0,              0,              0,              0,              0
);
APVAR(IDF_GAMEMOD, 0, teamdamage, 0, A_T_ALL,
    A_T_PLAYER,     A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI,         A_T_PLAYER,     A_T_PLAYER
);
APVAR(IDF_GAMEMOD, 0, weapongladiator, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_RIFLE,        W_SHOTGUN,      W_SMG,          W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponinsta, 0, W_ALL-1,
    W_RIFLE,        W_RIFLE,        W_RIFLE,        W_RIFLE,        W_RIFLE,        W_CLAW,         W_RIFLE,        W_RIFLE
);
APVAR(IDF_GAMEMOD, 0, weaponkaboom, 0, W_ALL-1,
    W_GRENADE,      W_GRENADE,      W_RIFLE,        W_SHOTGUN,      W_SMG,          W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponmedieval, 0, W_ALL-1,
    W_SWORD,        W_SWORD,        W_RIFLE,        W_SHOTGUN,      W_SMG,          W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponspeedrun, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_RIFLE,        W_SHOTGUN,      W_SMG,          W_CLAW,         W_PISTOL,       W_ZAPPER
);
APVAR(IDF_GAMEMOD, 0, weaponspawn, 0, W_ALL-1,
    W_PISTOL,       W_PISTOL,       W_RIFLE,        W_SHOTGUN,      W_SMG,          W_CLAW,         W_PISTOL,       W_ZAPPER
);
// these are modified by gameent::configure() et al
APFVAR(IDF_GAMEMOD, 0, airtolerance, 0, FVAR_MAX,
    125,            125,            1000,           100,            125,            125,            125,            125
);
APFVAR(IDF_GAMEMOD, 0, speed, 0, FVAR_MAX,
    100,            100,            0,              100,            150,            50,             150,            150
);
APFVAR(IDF_GAMEMOD, 0, speedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, speedprize, 0, FVAR_MAX,
    1,              1,              1,              1,              1,              1,              1,              1.25f
);
APVAR(IDF_GAMEMOD, 0, sprinttime, 0, VAR_MAX,
    1000,           1000,           1000,           1000,           1000,           1000,           1000,           1000
);
APFVAR(IDF_GAMEMOD, 0, impulsespeed, 0, FVAR_MAX,
    75,             75,             0,              75,             75,             50,             75,             75
);
APFVAR(IDF_GAMEMOD, 0, impulsespeedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, weight, 0, FVAR_MAX,
    250,            250,            250,            250,            100,            200,            0,              100
);
APFVAR(IDF_GAMEMOD, 0, weightextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, buoyancy, 0, FVAR_MAX,
    500,            500,            0,              500,            0,              400,            0,              0
);
APFVAR(IDF_GAMEMOD, 0, buoyancyextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0,              0,              0
);

#ifndef STANDALONE
struct mixer
{
    char *id, *name, *tpname, *fpname;
    int tclamp;
    float tpscale, fpscale, split, blur;
    bool anytype, tpload, fpload;
    Texture *tptex, *fptex;

    mixer() : id(NULL), name(NULL), tpname(NULL), fpname(NULL), tclamp(0), tpscale(1), fpscale(1), split(0), blur(0), anytype(false), tpload(false), fpload(false), tptex(NULL), fptex(NULL) {}
    ~mixer()
    {
        if(id) delete[] id;
        if(name) delete[] name;
        if(tpname) delete[] tpname;
        if(fpname) delete[] fpname;
    }

    void cleanup()
    {
        tptex = fptex = NULL;
        tpload = fpload = false;
    }

    void processtex(bool fp, const char *name)
    {
        Texture *t = textureload(name, tclamp, true, false);

        if(t && t != notexture && (blur > 0.0f || t->bpp <= 2))
        {
            defformatstring(m, "<comp:0,-%d>", max(t->w, t->h));

            if(t->bpp > 2) concatstring(m, "blur [");
            else
            {
                concatstring(m, "mixerconv [");
                if(split > 0.0f) concformatstring(m, "split = %.6g; ", clamp(split, 0.0f, 0.5f));
            }

            if(blur > 0.0f) concformatstring(m, "blur = %.6g; ", clamp(blur, 0.0f, 0.5f));

            concformatstring(m, "tex = [%s]]", name);

            Texture *u = textureload(m, tclamp, true, false);
            if(u && u != notexture) t = u;
        }

        (fp ? fptex : tptex) = t;
    }
    
    Texture *loadtex(bool fp)
    {
        if((fp ? fpload : tpload) || !engineready)
            return fp ? fptex : tptex;

        if(fp)
        {
            if(!fpname || !*fpname)
            {
                if(!tptex)
                {
                    processtex(false, tpname);
                    tpload = true;
                }
                fptex = tptex;
            }
            else processtex(true, fpname);
            
            fpload = true;
        }
        else
        {
            processtex(false, tpname);
            tpload = true;
        }

        return fp ? fptex : tptex;
    }

    void setnames(const char *d, const char *n, const char *t, const char *f)
    {
        id = newstring(d);
        name = newstring(n);
        tpname = newstring(t);
        fpname = fpname && *fpname ? newstring(f) : NULL;
    }
};
#ifdef CPP_GAME_MAIN
vector<mixer> mixers;
#else
extern vector<mixer> mixers;
#endif
#endif

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
