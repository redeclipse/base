struct actors
{
    int type,           weap,           health;
    float   xradius,    yradius,    height,     weight,     speed,      scale;
    bool    canmove,    canstrafe,  canjump,    cancrouch,  useweap,    living,     hitbox;
    const char  *name,      *playermodel[4];
};

enum { A_PLAYER = 0, A_BOT, A_TURRET, A_GRUNT, A_DRONE, A_MAX, A_ENEMY = A_TURRET, A_TOTAL = A_MAX-A_ENEMY };
#ifdef GAMESERVER
actors actor[] = {
    {
        A_PLAYER,         -1,             0,
            3,          3,          14,         200,        50,         1,
            true,       true,       true,       true,       true,       true,       true,
                "player",   { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_BOT,         -1,             0,
            3,          3,          14,         200,        50,         1,
            true,       true,       true,       true,       true,       true,       true,
                "bot",      { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_TURRET,      W_SMG,       100,
            4.75,       4.75,       8.75,       150,        1,          1,
            false,      false,      false,      false,      false,      false,      false,
                "turret",   { "actors/player/male/hwep",      "actors/turret",          "actors/player/male/body",      "actors/turret" }
    },
    {
        A_GRUNT,       W_PISTOL,   50,
            3,          3,          14,         200,        50,         1,
            true,       true,       true,       true,       true,       true,       true,
                "grunt",   { "actors/player/male/hwep",      "actors/player/male",      "actors/player/male/body",      "actors/player/male/headless" }
    },
    {
        A_DRONE,       W_MELEE,     50,
            3,          3,          14,         150,        40,         1,
            true,       true,       true,       true,       true,       true,       true,
                "drone",    { "actors/player/male/hwep",      "actors/drone",           "actors/player/male/body",      "actors/drone" }
    },
};
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

TPSVAR(0, name,
    "neutral",  "alpha",    "omega",    "kappa",    "sigma",    "enemy"
);
TPVAR(IDF_HEX, colour, 0, 0xFFFFFF,
    0x90A090,   0x5F66FF,   0xFF4F44,   0xFFD022,   0x22FF22,   0xB0B0B0
);

struct score
{
    int team, total;
    score(int s = -1, int n = 0) : team(s), total(n) {}
    ~score() {}
};

#define numteams(a,b)   (m_fight(a) && m_team(a,b) ? (m_multi(a,b) ? T_TOTAL : T_NUM) : 1)
#define teamcount(a,b)  (m_fight(a) && m_team(a,b) ? (m_multi(a,b) ? T_ALL : T_COUNT) : 1)
#define isteam(a,b,c,d) (m_fight(a) && m_team(a,b) ? (c >= d && c <= numteams(a,b)) : c == T_NEUTRAL)
#define valteam(a,b)    (a >= b && a <= T_TOTAL)

#define PLAYERTYPES 2
#ifdef GAMESERVER
const char *playertypes[PLAYERTYPES][6] = {
    { "actors/player/male/hwep",      "actors/player/male",     "actors/player/male/body",      "actors/player/male/headless",      "player",   "male" },
    { "actors/player/female/hwep",    "actors/player/female",   "actors/player/male/body",      "actors/player/female/headless",    "player",   "female" }
};
float playerdims[PLAYERTYPES][3] = {
    { 3,      3,      14 },
    { 3,      3,      14 },
};
#else
extern const char *playertypes[PLAYERTYPES][6];
extern float playerdims[PLAYERTYPES][3];
#endif

#include "playerdef.h"

PPVAR(0, health, 1, VAR_MAX,
    100
);
PPFVAR(0, weight, 0, FVAR_MAX,
    200
);
PPFVAR(0, scale, FVAR_NONZERO, FVAR_MAX,
    1
);
PPFVAR(0, speed, FVAR_NONZERO, FVAR_MAX,
    50
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
        defformatstring(m)("vanities/%s", r);
        model = newstring(m);
        if(proj)
        {
            delete[] proj;
            formatstring(m)("vanities/%s/proj", r);
            proj = newstring(m);
        }
    }
};
#ifdef GAMEWORLD
vector<vanitys> vanities;
#else
extern vector<vanitys> vanities;
#endif
