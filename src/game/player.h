struct actors
{
    int type;
    float xradius, yradius, height;
    bool useweap, living, hitbox;
    const char *name, *playermodel[4];
};

enum { A_PLAYER = 0, A_BOT, A_TURRET, A_GRUNT, A_DRONE, A_MAX, A_ENEMY = A_TURRET, A_TOTAL = A_MAX-A_ENEMY };
enum {
    A_A_MOVE = 0, A_A_JUMP, A_A_CROUCH, A_A_DASH, A_A_BOOST, A_A_PARKOUR, A_A_MELEE, A_A_PRIMARY, A_A_SECONDARY, A_A_PUSHABLE, A_A_AFFINITY, A_A_REGEN, A_A_MAX,
    A_A_IMFIRST = A_A_DASH, A_A_IMLAST = A_A_PARKOUR, A_A_IMPULSE = A_A_IMLAST-A_A_IMFIRST, A_A_IMCOUNT = A_A_IMPULSE+1,
    A_A_IMOFFSET = (1<<(A_A_DASH-A_A_IMFIRST))|(1<<(A_A_BOOST-A_A_IMFIRST))|(1<<(A_A_PARKOUR-A_A_IMFIRST)), A_A_IMRELAX = (1<<(A_A_PARKOUR-A_A_IMFIRST)),
    A_A_ALL = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_DASH)|(1<<A_A_BOOST)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_PLAYER = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_DASH)|(1<<A_A_BOOST)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_MOVINGAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_CROUCH)|(1<<A_A_DASH)|(1<<A_A_BOOST)|(1<<A_A_PARKOUR)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE)|(1<<A_A_AFFINITY)|(1<<A_A_REGEN),
    A_A_LESSAI = (1<<A_A_MOVE)|(1<<A_A_JUMP)|(1<<A_A_MELEE)|(1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)|(1<<A_A_PUSHABLE),
    A_A_FIXEDAI = (1<<A_A_PRIMARY)|(1<<A_A_SECONDARY)
};
enum {
    A_C_PLAYERS = 0, A_C_BOTS, A_C_ENEMIES, A_C_MAX,
    A_C_ALL = (1<<A_C_PLAYERS)|(1<<A_C_BOTS)|(1<<A_C_ENEMIES)
};
enum {
    A_T_PLAYERS = 0, A_T_BOTS, A_T_ENEMIES, A_T_GHOST, A_T_MAX,
    A_T_ALL = (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES)|(1<<A_T_GHOST),
    A_T_PLAYER = (1<<A_T_PLAYERS)|(1<<A_T_BOTS)|(1<<A_T_ENEMIES),
    A_T_AI = (1<<A_T_BOTS)|(1<<A_T_ENEMIES)
};
#ifdef GAMESERVER
actors actor[] = {
    {
        A_PLAYER, 3, 3, 14, true, true, true,
        "player",   { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_BOT, 3, 3, 14, true, true, true,
        "bot",      { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_TURRET, 4.75f, 4.75f, 8.75f, false, false, false,
        "turret",   { "actors/player/male/hwep",      "actors/turret",          "actors/player/male/body",      "actors/turret" }
    },
    {
        A_GRUNT, 3, 3, 14, true, true, true,
        "grunt",   { "actors/player/male/hwep",      "actors/player/male",      "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_DRONE, 3, 3, 14, true, true, true,
        "drone",    { "actors/player/male/hwep",      "actors/drone",           "actors/player/male/body",      "actors/drone" }
    },
};
#else
extern actors actor[];
#endif

enum
{
    T_NEUTRAL = 0, T_ALPHA, T_OMEGA, T_KAPPA, T_SIGMA, T_ENEMY, T_MAX,
    T_FIRST = T_ALPHA, T_LAST = T_OMEGA, T_MULTI = T_SIGMA,
    T_COUNT = T_LAST+1, T_ALL = T_MULTI+1,
    T_NUM = (T_LAST-T_FIRST)+1,
    T_TOTAL = (T_MULTI-T_FIRST)+1
};

enum
{
    TT_INFO = 1<<0, TT_RESET = 1<<1, TT_SMODE = 1<<2,
    TT_INFOSM = TT_INFO|TT_SMODE,
    TT_RESETX = TT_INFO|TT_RESET
};

#ifdef GAMESERVER
const char *teamnames[T_MAX] = { "neutral", "alpha", "omega", "kappa", "sigma", "enemy" };
int mapbals[T_TOTAL][T_TOTAL] = {
    { T_ALPHA, T_OMEGA, T_KAPPA, T_SIGMA },
    { T_OMEGA, T_ALPHA, T_SIGMA, T_KAPPA },
    { T_KAPPA, T_SIGMA, T_ALPHA, T_OMEGA },
    { T_SIGMA, T_KAPPA, T_OMEGA, T_ALPHA }
};
#else
extern const char *teamnames[T_MAX];
extern int mapbals[T_TOTAL][T_TOTAL];
#endif

#include "teamdef.h"

TPSVAR(IDF_GAMEMOD,  name,
    "neutral",  "alpha",    "omega",    "kappa",    "sigma",    "enemy"
);
TPVAR(IDF_GAMEMOD|IDF_HEX, colour, 0, 0xFFFFFF,
    0x90A090,   0x5F66FF,   0xFF4F44,   0xFFD022,   0x22FF22,   0xB0B0B0
);

struct score
{
    int team, total;
    score(int s = -1, int n = 0) : team(s), total(n) {}
    ~score() {}
};

#define numteams(a,b)   (m_play(a) && m_team(a,b) ? (m_multi(a,b) ? T_TOTAL : T_NUM) : 1)
#define teamcount(a,b)  (m_play(a) && m_team(a,b) ? (m_multi(a,b) ? T_ALL : T_COUNT) : 1)
#define isteam(a,b,c,d) (m_play(a) && m_team(a,b) ? (c >= d && c <= numteams(a,b)) : c == T_NEUTRAL)
#define valteam(a,b)    (a >= b && a <= T_TOTAL)

#define PLAYERTYPES 2
#ifdef GAMESERVER
const char *playertypes[PLAYERTYPES][6] = {
    { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless",      "player",   "male" },
    { "actors/player/female/hwep",    "actors/player/female",   "actors/player/male/body",      "actors/player/female/headless",    "player",   "female" }
};
#else
extern const char *playertypes[PLAYERTYPES][6];
#endif

#include "playerdef.h"

APSVAR(IDF_GAMEMOD, vname,
    "player",       "bot",          "turret",       "grunt",        "drone"
);
APVAR(IDF_GAMEMOD, abilities, 0, A_A_ALL,
    A_A_PLAYER,     A_A_MOVINGAI,   A_A_FIXEDAI,    A_A_MOVINGAI,   A_A_LESSAI
);
APVAR(IDF_GAMEMOD, collide, 0, A_C_ALL,
    A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL,        A_C_ALL
);
APVAR(IDF_GAMEMOD, health, 1, VAR_MAX,
    100,            100,            100,            50,             50
);
APVAR(IDF_GAMEMOD, maxcarry, 1, W_LOADOUT,
    2,              2,              0,              0,              0
);
APVAR(IDF_GAMEMOD, teamdamage, 0, A_T_ALL,
    A_T_PLAYER,     A_T_AI,         A_T_AI,         A_T_AI,         A_T_AI
);
APVAR(IDF_GAMEMOD, weapongladiator, 0, W_MAX-1,
    W_MELEE,       W_MELEE,         W_SMG,          W_PISTOL,       W_MELEE
);
APVAR(IDF_GAMEMOD, weaponinsta, 0, W_MAX-1,
    W_RIFLE,        W_RIFLE,        W_RIFLE,        W_RIFLE,        W_MELEE
);
APVAR(IDF_GAMEMOD, weaponkaboom, 0, W_MAX-1,
    W_GRENADE,      W_GRENADE,      W_GRENADE,      W_GRENADE,      W_MELEE
);
APVAR(IDF_GAMEMOD, weaponmedieval, 0, W_MAX-1,
    W_SWORD,        W_SWORD,        W_RIFLE,        W_SWORD,        W_MELEE
);
APVAR(IDF_GAMEMOD, weaponrace, 0, W_MAX-1,
    W_MELEE,       W_MELEE,         W_SMG,          W_PISTOL,       W_MELEE
);
APVAR(IDF_GAMEMOD, weaponspawn, 0, W_MAX-1,
    W_PISTOL,       W_PISTOL,       W_SMG,          W_PISTOL,       W_MELEE
);
APFVAR(IDF_GAMEMOD, weight, 0, FVAR_MAX,
    200,            200,            150,            200,            150
);
APFVAR(IDF_GAMEMOD, scale, FVAR_NONZERO, FVAR_MAX,
    1,              1,              1,              1,              1
);
APVAR(IDF_GAMEMOD, spawndelay, 0, VAR_MAX,
    5000,          5000,            30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, spawndelaybomber, 0, VAR_MAX,
    3000,          3000,            30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, spawndelaycapture, 0, VAR_MAX,
    5000,          5000,            30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, spawndelaydefend, 0, VAR_MAX,
    5000,          5000,            30000,          30000,          30000
);
APVAR(IDF_GAMEMOD, spawndelaygauntlet, 0, VAR_MAX,
    3000,          3000,            30000,          30000,          30000
);
APFVAR(IDF_GAMEMOD, spawndelayinstascale, 0, FVAR_MAX,
    0.5f,          0.5f,            0.75f,          0.75f,          0.75f
);
APVAR(IDF_GAMEMOD, spawndelayrace, 0, VAR_MAX,
    1000,          1000,            10000,          10000,          10000
);
APVAR(IDF_GAMEMOD, spawngrenades, 0, 2,
    0,              0,              0,              0,              0
);
APVAR(IDF_GAMEMOD, spawnmines, 0, 2,
    0,              0,              0,              0,              0
);
APFVAR(IDF_GAMEMOD, speed, 0, FVAR_MAX,
    50,             50,             0,              50,             40
);

#define VANITYMAX 16
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
struct vanitys
{
    int type, cond, style, priv;
    char *ref, *model, *proj, *name, *tag;
    vector<vanityfile> files;

    vanitys() : type(-1), cond(0), style(0), priv(0), ref(NULL), model(NULL), proj(NULL), name(NULL), tag(NULL) {}
    vanitys(int t, const char *r, const char *n, const char *g, int c, int s, int p) : type(t), cond(c), style(s), priv(p), ref(newstring(r)), model(NULL), proj(NULL), name(newstring(n)), tag(newstring(g)) { setmodel(r); }
    ~vanitys()
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
        if(proj)
        {
            delete[] proj;
            formatstring(m, "vanities/%s/proj", r);
            proj = newstring(m);
        }
    }
};
#ifdef GAMEWORLD
vector<vanitys> vanities;
#else
extern vector<vanitys> vanities;
#endif
