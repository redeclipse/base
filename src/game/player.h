// Player and other actor definitions

enum { A_PLAYER = 0, A_BOT, A_TURRET, A_GRUNT, A_DRONE, A_ROLLER, A_MAX, A_ENEMY = A_TURRET, A_TOTAL = A_MAX-A_ENEMY };

struct actor
{
    const char *name;
    int id;
    bool hastags, jetfx, steps, hitboxes, onlyfwd;
    float height, radius;
    const char *mdl;
};
#ifdef CPP_GAME_SERVER
actor actors[] = {
    { "player", A_PLAYER,   true,   true,   true,   true,   false,  27.f,   5.5f,   NULL },
    { "bot",    A_BOT,      true,   true,   true,   true,   false,  27.f,   5.5f,   NULL },
    { "turret", A_TURRET,   true,   true,   true,   true,   false,  27.f,   5.5f,   NULL },
    { "grunt",  A_GRUNT,    true,   true,   true,   true,   false,  27.f,   5.5f,   NULL },
    { "drone",  A_DRONE,    true,   true,   true,   true,   false,  27.f,   5.5f,   NULL },
    { "roller", A_ROLLER,   false,  false,  false,  false,  true,   13.5f,  6.75f,  "actors/roller" },
};
#else
extern actor actors[];
#endif

enum
{
    A_A_MOVE = 0, A_A_JUMP, A_A_CROUCH, A_A_SLIDE, A_A_BOOST, A_A_POUND, A_A_PARKOUR, A_A_MELEE,
    A_A_PRIMARY, A_A_SECONDARY, A_A_PUSHABLE, A_A_AFFINITY, A_A_REGEN, A_A_KAMIKAZE, A_A_MAX,
    A_A_IMFIRST = A_A_SLIDE, A_A_IMLAST = A_A_PARKOUR, A_A_IMPULSE = A_A_IMLAST-A_A_IMFIRST, A_A_IMCOUNT = A_A_IMPULSE+1,
    A_A_IMOFFSET = (1<<(A_A_SLIDE-A_A_IMFIRST))|(1<<(A_A_BOOST-A_A_IMFIRST))|(1<<(A_A_POUND-A_A_IMFIRST))|(1<<(A_A_PARKOUR-A_A_IMFIRST)),
    A_A_IMRELAX = (1<<(A_A_PARKOUR-A_A_IMFIRST)),
    A_A_ATTACK = (1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_KAMIKAZE),
    A_A_ALL = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_SLIDE)|(1<<A_A_BOOST)|(1<<A_A_POUND)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_PLAYER = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_SLIDE)|(1<<A_A_BOOST)|(1<<A_A_POUND)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_MOVINGAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_SLIDE)|(1<<A_A_BOOST)|(1<<A_A_POUND)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_LESSAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE),
    A_A_FIXEDAI = (1<<A_A_PRIMARY)|(1<<A_A_SECONDARY),
    A_A_ROLLER = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_PARKOUR)|(1<<A_A_PUSHABLE)|(1<<A_A_KAMIKAZE),
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

enum
{
    T_NEUTRAL = 0, T_ALPHA, T_OMEGA, T_ENEMY, T_MAX,
    T_FIRST = T_ALPHA, T_LAST = T_OMEGA,
    T_COUNT = T_LAST+1, T_NUM = (T_LAST-T_FIRST)+1
};

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
SVAR(IDF_READONLY, teamnames, "neutral alpha omega enemy");
VAR(IDF_READONLY, teamidxneutral, 1, T_NEUTRAL, -1);
VAR(IDF_READONLY, teamidxalpha, 1, T_ALPHA, -1);
VAR(IDF_READONLY, teamidxomega, 1, T_OMEGA, -1);
VAR(IDF_READONLY, teamidxenemy, 1, T_ENEMY, -1);
VAR(IDF_READONLY, teamidxfirst, 1, T_FIRST, -1);
VAR(IDF_READONLY, teamidxlast, 1, T_LAST, -1);
VAR(IDF_READONLY, teamidxcount, 1, T_COUNT, -1);
VAR(IDF_READONLY, teamidxnum, 1, T_NUM, -1);
#else
extern int mapbals[T_NUM][T_NUM];
#endif

#include "teamdef.h"

TPSVAR(IDF_GAMEMOD, 0, name,
    "Neutral",  "Alpha",    "Omega",    "Enemy"
);
TPVAR(IDF_GAMEMOD|IDF_HEX, 0, colour, 0, 0xFFFFFF,
    0x909090,   0x2020FF,   0xFF2020,   0xE020E0
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

#define PLAYERTYPES 2
#define PLAYERPATTERNS 16

struct playerpattern
{
    const char *filename;
    const char *id;
    const char *name;
    int clamp;
    float scale;
};

#ifdef CPP_GAME_SERVER
extern const char * const playertypes[PLAYERTYPES][7] = {
    { "actors/player/male/hwep",        "actors/player/male",       "actors/player/male/body",      "actors/player/male/headless",      "player",   "male",     "Male" },
    { "actors/player/female/hwep",      "actors/player/female",     "actors/player/male/body",      "actors/player/female/headless",    "player",   "female",   "Female" }
};

// final entry is texscale in playerpattern (512x -> 2 , 1024x -> 1)
extern const playerpattern playerpatterns[PLAYERPATTERNS] = {
    { "<grey>textures/patterns/default",  "default",  "Default",  0,     1 },
    { "<grey>textures/patterns/soft",     "soft",     "Soft",     0,     1 },
    { "<grey>textures/patterns/camo",     "camo",     "Camo",     0,     1 },
    { "<grey>textures/patterns/heart",    "heart",    "Heart",    0x300, 2 },
    { "<grey>textures/patterns/crown",    "crown",    "Crown",    0x300, 2 },
    { "<grey>textures/patterns/zebra",    "zebra",    "Zebra",    0x300, 2 },
    { "<grey>textures/patterns/checker",  "checker",  "Checker",  0x300, 2 },
    { "<grey>textures/patterns/star",     "star",     "Star",     0x300, 2 },
    { "<grey>textures/patterns/flower",   "flower",   "Flower",   0x300, 2 },
    { "<grey>textures/patterns/leopard",  "leopard",  "Leopard",  0x300, 2 },
    { "<grey>textures/patterns/zigzag",   "zigzag",   "Zigzag",   0x300, 2 },
    { "<grey>textures/patterns/pixel",    "pixel",    "Pixel",    0x300, 2 },
    { "<grey>textures/patterns/circle",   "circle",   "Circle",   0x300, 2 },
    { "<grey>textures/patterns/ninja",    "ninja",    "Ninja",    0,     2 },
    { "<grey>textures/patterns/lines",    "lines",    "Lines",    0,     1 },
    { "<grey>textures/patterns/softhero", "softhero", "Softhero", 0,     1 },
};
#else
extern const char * const playertypes[PLAYERTYPES][7];
extern const playerpattern playerpatterns[PLAYERPATTERNS];
#endif

#include "playerdef.h"

APSVAR(IDF_GAMEMOD, 0, vname,
    "Player",       "Bot",          "Turret",       "Grunt",        "Drone",        "Roller"
);
APVAR(IDF_GAMEMOD, 0, abilities, 0, A_A_ALL,
    A_A_PLAYER,     A_A_MOVINGAI,   A_A_FIXEDAI,    A_A_MOVINGAI,   A_A_LESSAI,     A_A_ROLLER
);
APVAR(IDF_GAMEMOD, 0, collide, 0, A_C_ALL,
    A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL
);
APVAR(IDF_GAMEMOD, 0, health, 1, VAR_MAX,
    1000,           1000,           1000,           500,            500,            500
);
APVAR(IDF_GAMEMOD, 0, hurtstop, 1, VAR_MAX,
    0,              0,              0,              100,            500,            100
);
APVAR(IDF_GAMEMOD, 0, maxcarry, 0, WEAPCARRY,
    WEAPCARRY,      WEAPCARRY,      0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, scale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              0.5f,           0.85f,          0.7f,           1.f
);
APVAR(IDF_GAMEMOD, 0, spawndelay, 0, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, 0, spawndelayedit, 0, VAR_MAX,
    0,              5000,           30000,          30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, 0, spawndelaybomber, 0, VAR_MAX,
    3000,           3000,           30000,          30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, 0, spawndelaycapture, 0, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, 0, spawndelaydefend, 0, VAR_MAX,
    5000,           5000,           30000,          30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, 0, spawndelaygauntlet, 0, VAR_MAX,
    3000,           3000,           30000,          30000,          30000,          30000
);
APFVAR(IDF_GAMEMOD, 0, spawndelayinstascale, 0, FVAR_MAX,
    0.5f,           0.5f,           0.75f,          0.75f,          0.75f,          0.75f
);
APVAR(IDF_GAMEMOD, 0, spawndelayrace, 0, VAR_MAX,
    1000,           1000,           10000,          10000,          10000,          10000
);
APVAR(IDF_GAMEMOD, 0, spawngrenades, 0, 2,
    0,              0,              0,              0,              0,              0
);
APVAR(IDF_GAMEMOD, 0, spawnmines, 0, 2,
    0,              0,              0,              0,              0,              0
);
APVAR(IDF_GAMEMOD, 0, teamdamage, 0, A_T_ALL,
    A_T_PLAYER,     A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI
);
APVAR(IDF_GAMEMOD, 0, weapongladiator, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW
);
APVAR(IDF_GAMEMOD, 0, weaponinsta, 0, W_ALL-1,
    W_RIFLE,        W_RIFLE,        W_RIFLE,        W_RIFLE,        W_CLAW,         W_CLAW
);
APVAR(IDF_GAMEMOD, 0, weaponkaboom, 0, W_ALL-1,
    W_GRENADE,      W_GRENADE,      W_GRENADE,      W_GRENADE,      W_CLAW,         W_CLAW
);
APVAR(IDF_GAMEMOD, 0, weaponmedieval, 0, W_ALL-1,
    W_SWORD,        W_SWORD,        W_RIFLE,        W_SWORD,        W_CLAW,         W_CLAW
);
APVAR(IDF_GAMEMOD, 0, weaponrace, 0, W_ALL-1,
    W_CLAW,         W_CLAW,         W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW
);
APVAR(IDF_GAMEMOD, 0, weaponspawn, 0, W_ALL-1,
    W_PISTOL,       W_PISTOL,       W_SMG,          W_PISTOL,       W_CLAW,         W_CLAW
);
// these are modified by gameent::configure() et al
APFVAR(IDF_GAMEMOD, 0, speed, 0, FVAR_MAX,
    95,             95,             0,              85,             100,            100
);
APFVAR(IDF_GAMEMOD, 0, speedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, jumpspeed, 0, FVAR_MAX,
    120,            120,            0,              110,            90,             75
);
APFVAR(IDF_GAMEMOD, 0, jumpspeedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, impulsespeed, 0, FVAR_MAX,
    85,             85,             0,              85,             85,             75
);
APFVAR(IDF_GAMEMOD, 0, impulsespeedextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, 0, weight, 0, FVAR_MAX,
    264,            264,            264,            264,            195,            150
);
APFVAR(IDF_GAMEMOD, 0, weightextra, FVAR_MIN, FVAR_MAX,
    0,              0,              0,              0,              0,              0
);

#define VANITYMAX 16
#define ATTACHMENTMAX VANITYMAX+13

struct vanityfile
{
    char *id, *name;
    bool proj;

    vanityfile() : id(NULL), name(NULL), proj(false) {}
    vanityfile(const char *d, const char *n, bool p = false) : id(newstring(d)), name(newstring(n)), proj(p) {}
    ~vanityfile()
    {
        if(id) delete[] id;
        if(name) delete[] name;
    }
};
struct vanity
{
    int type, cond, style;
    char *ref, *model, *proj, *name, *tag;
    vector<vanityfile> files;

    vanity() : type(-1), cond(0), style(0), ref(NULL), model(NULL), proj(NULL), name(NULL), tag(NULL) {}
    vanity(int t, const char *r, const char *n, const char *g, int c, int s) : type(t), cond(c), style(s), ref(newstring(r)), model(NULL), proj(NULL), name(newstring(n)), tag(newstring(g)) { setmodel(r); }
    ~vanity()
    {
        if(ref) delete[] ref;
        if(model) delete[] model;
        if(proj) delete[] proj;
        if(name) delete[] name;
        if(tag) delete[] tag;
        loopvrev(files) files.remove(i);
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
#else
extern vector<vanity> vanities;
#endif
