enum
{
    G_DEMO = 0, G_EDITMODE, G_DEATHMATCH, G_CAPTURE, G_DEFEND, G_BOMBER, G_RACE, G_MAX,
    G_START = G_EDITMODE, G_PLAY = G_DEATHMATCH,
    G_RAND = G_BOMBER-G_DEATHMATCH+1, G_COUNT = G_MAX-G_PLAY,
    G_NEVER = (1<<G_DEMO)|(1<<G_EDITMODE),
    G_LIMIT = (1<<G_DEATHMATCH)|(1<<G_CAPTURE)|(1<<G_DEFEND)|(1<<G_BOMBER),
    G_ALL = (1<<G_DEMO)|(1<<G_EDITMODE)|(1<<G_DEATHMATCH)|(1<<G_CAPTURE)|(1<<G_DEFEND)|(1<<G_BOMBER)|(1<<G_RACE),
    G_SW = (1<<G_RACE),
};
enum
{
    G_M_MULTI = 0, G_M_FFA, G_M_COOP, G_M_INSTA, G_M_MEDIEVAL, G_M_KABOOM, G_M_DUEL, G_M_SURVIVOR,
    G_M_CLASSIC, G_M_ONSLAUGHT, G_M_FREESTYLE, G_M_VAMPIRE, G_M_RESIZE, G_M_HARD, G_M_BASIC,
    G_M_GSP, G_M_GSP1 = G_M_GSP, G_M_GSP2, G_M_GSP3, G_M_NUM,
    G_M_GSN = G_M_NUM-G_M_GSP,
    G_M_ALL = (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
    G_M_FILTER = (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
    G_M_ROTATE = (1<<G_M_FFA)|(1<<G_M_CLASSIC),
    G_M_SW = (1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM),
    G_M_DK = (1<<G_M_DUEL)|(1<<G_M_SURVIVOR),
    G_M_IM = (1<<G_M_INSTA)|(1<<G_M_MEDIEVAL),
};
enum { G_F_GSP = 0, G_F_NUM };

enum { G_S_WAITING = 0, G_S_GETMAP, G_S_SENDMAP, G_S_READYING, G_S_GAMEINFO, G_S_PLAYING, G_S_OVERTIME, G_S_INTERMISSION, G_S_VOTING, G_S_MAX };
#ifdef GAMESERVER
const char *gamestates[3][G_S_MAX] = {
    { "waiting", "getmap", "sendmap", "readying", "gameinfo", "playing", "overtime", "intermission", "voting" },
    { "waiting to start", "server getting map", "server sending map", "waiting for ready players", "waiting for game information", "playing", "overtime", "voting in progress" },
    { "Waiting to start", "Server getting map", "Server sending map", "Waiting for ready players", "Waiting for game information", "Playing", "Overtime", "Voting in progress" }
};
#else
extern const char *gamestates[3][G_S_MAX];
#endif
#define gs_waiting(a) (a >= G_S_WAITING && a <= G_S_GAMEINFO)
#define gs_playing(a) (a >= G_S_PLAYING && a <= G_S_OVERTIME)
#define gs_intermission(a) (a >= G_S_INTERMISSION && a <= G_S_VOTING)

struct gametypes
{
    int type, flags, implied, mutators[G_M_GSN+1];
    const char *name, *sname, *gsp[G_M_GSN], *desc, *gsd[G_M_GSN];
};
struct mutstypes
{
    int type, implied, mutators;
    const char *name, *desc;
};
#ifdef GAMESERVER
gametypes gametype[] = {
    {
        G_DEMO, 0, 0, { 0, 0, 0, 0 },
        "demo", "demo", { "", "", "" },
        "play back previously recorded games", { "", "", "" },
    },
    {
        G_EDITMODE, 0, (1<<G_M_FFA)|(1<<G_M_CLASSIC),
        {
            (1<<G_M_FFA)|(1<<G_M_CLASSIC)|(1<<G_M_FREESTYLE),
            0, 0, 0
        },
        "editing", "editing", { "", "", "" },
        "create and edit existing maps", { "", "", "" },
    },
    {
        G_DEATHMATCH, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC),
            0, 0, 0
        },
        "deathmatch", "dm", { "", "", "" },
        "shoot to kill and increase score by fragging", { "", "", "" },
    },
    {
        G_CAPTURE, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP3)
        },
        "capture-the-flag", "capture", { "quick", "defend", "protect" },
        "take the enemy flag and return it to the base to score", { "dropped flags instantly return to base", "dropped flags must be defended until they reset", "protect the flag and hold the enemy flag to score" },
    },
    {
        G_DEFEND, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            0
        },
        "defend-and-control", "defend", { "quick", "king", "" },
        "defend control points to score", { "control points secure quicker than normal", "remain king of the hill to score", ""},
    },
    {
        G_BOMBER, (1<<G_F_GSP), 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "bomber-ball", "bomber", { "hold", "basket", "attack" },
        "carry the bomb into the enemy goal to score", { "hold the bomb as long as possible to score", "throw the bomb into the enemy goal to score", "teams take turns attacking and defending" },
    },
    {
        G_RACE, (1<<G_F_GSP), 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "race", "race", { "timed", "endurance", "gauntlet" },
        "compete for the most number of laps", { "compete for the fastest time completing a lap", "impulse meter does not reset at all", "teams take turns running the gauntlet" },
    }
};
mutstypes mutstype[] = {
    {
        G_M_MULTI, (1<<G_M_MULTI),
        (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "multi", "four teams battle to determine the winning side"
    },
    {
        G_M_FFA, (1<<G_M_FFA),
        (1<<G_M_FFA)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "ffa", "every player for themselves"
    },
    {
        G_M_COOP, (1<<G_M_COOP),
        (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "coop", "players versus drones"
    },
    {
        G_M_INSTA, (1<<G_M_INSTA),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_FREESTYLE)|(1<<G_M_RESIZE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "instagib", "one hit kills instantly"
    },
    {
        G_M_MEDIEVAL, (1<<G_M_MEDIEVAL),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "medieval", "players spawn only with swords"
    },
    {
        G_M_KABOOM,  (1<<G_M_KABOOM),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "kaboom", "players spawn with explosives only"
    },
    {
        G_M_DUEL, (1<<G_M_DUEL),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "duel", "one on one battles to determine the winner"
    },
    {
        G_M_SURVIVOR, (1<<G_M_SURVIVOR),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "survivor", "players battle to determine the winner"
    },
    {
        G_M_CLASSIC,    (1<<G_M_CLASSIC),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "classic",
        "weapons must be collected from spawns in the arena"
    },
    {
        G_M_ONSLAUGHT, (1<<G_M_ONSLAUGHT),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "onslaught", "waves of enemies fill the battle arena"
    },
    {
        G_M_FREESTYLE, (1<<G_M_FREESTYLE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "freestyle", "players can parkour without limits"
    },
    {
        G_M_VAMPIRE, (1<<G_M_VAMPIRE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "vampire", "deal damage to regenerate health"
    },
    {
        G_M_RESIZE, (1<<G_M_RESIZE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "resize", "players change size depending on their health"
    },
    {
        G_M_HARD, (1<<G_M_HARD),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "hard", "no health regeneration, no radar"
    },
    {
        G_M_BASIC, (1<<G_M_BASIC),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "basic", "players only have the basic weapons that they spawn with"
    },
    {
        G_M_GSP1, (1<<G_M_GSP1),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "gsp1", ""
    },
    {
        G_M_GSP2, (1<<G_M_GSP2),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "gsp2", ""
    },
    {
        G_M_GSP3, (1<<G_M_GSP3),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "gsp3", ""
    },
};
#else
extern gametypes gametype[];
extern mutstypes mutstype[];
#endif

#define DSG(a,b,x)          (m_duel(a, b) ? G(duel##x) : G(survivor##x))

#define m_game(a)           (a > -1 && a < G_MAX)
#define m_check(a,b,c,d)    ((!a || (a < 0 ? !((0-a)&(1<<(c-G_PLAY))) : a&(1<<(c-G_PLAY)))) && (!b || (b < 0 ? !((0-b)&d) : b&d)))
#define m_local(a)          (a == G_DEMO)

#define m_demo(a)           (a == G_DEMO)
#define m_edit(a)           (a == G_EDITMODE)
#define m_dm(a)             (a == G_DEATHMATCH)
#define m_capture(a)        (a == G_CAPTURE)
#define m_defend(a)         (a == G_DEFEND)
#define m_bomber(a)         (a == G_BOMBER)
#define m_race(a)           (a == G_RACE)

#define m_play(a)           (a >= G_PLAY)
#define m_affinity(a)       (m_capture(a) || m_defend(a) || m_bomber(a))

#define m_multi(a,b)        ((b&(1<<G_M_MULTI)) || (gametype[a].implied&(1<<G_M_MULTI)))
#define m_ffa(a,b)          ((b&(1<<G_M_FFA)) || (gametype[a].implied&(1<<G_M_FFA)))
#define m_coop(a,b)         ((b&(1<<G_M_COOP)) || (gametype[a].implied&(1<<G_M_COOP)))
#define m_insta(a,b)        ((b&(1<<G_M_INSTA)) || (gametype[a].implied&(1<<G_M_INSTA)))
#define m_medieval(a,b)     ((b&(1<<G_M_MEDIEVAL)) || (gametype[a].implied&(1<<G_M_MEDIEVAL)))
#define m_kaboom(a,b)       ((b&(1<<G_M_KABOOM)) || (gametype[a].implied&(1<<G_M_KABOOM)))
#define m_duel(a,b)         ((b&(1<<G_M_DUEL)) || (gametype[a].implied&(1<<G_M_DUEL)))
#define m_survivor(a,b)     ((b&(1<<G_M_SURVIVOR)) || (gametype[a].implied&(1<<G_M_SURVIVOR)))
#define m_classic(a,b)      ((b&(1<<G_M_CLASSIC)) || (gametype[a].implied&(1<<G_M_CLASSIC)))
#define m_onslaught(a,b)    ((b&(1<<G_M_ONSLAUGHT)) || (gametype[a].implied&(1<<G_M_ONSLAUGHT)))
#define m_freestyle(a,b)    ((b&(1<<G_M_FREESTYLE)) || (gametype[a].implied&(1<<G_M_FREESTYLE)))
#define m_vampire(a,b)      ((b&(1<<G_M_VAMPIRE)) || (gametype[a].implied&(1<<G_M_VAMPIRE)))
#define m_resize(a,b)       ((b&(1<<G_M_RESIZE)) || (gametype[a].implied&(1<<G_M_RESIZE)))
#define m_hard(a,b)         ((b&(1<<G_M_HARD)) || (gametype[a].implied&(1<<G_M_HARD)))
#define m_basic(a,b)        ((b&(1<<G_M_BASIC)) || (gametype[a].implied&(1<<G_M_BASIC)))

#define m_gsp1(a,b)         ((b&(1<<G_M_GSP1)) || (gametype[a].implied&(1<<G_M_GSP1)))
#define m_gsp2(a,b)         ((b&(1<<G_M_GSP2)) || (gametype[a].implied&(1<<G_M_GSP2)))
#define m_gsp3(a,b)         ((b&(1<<G_M_GSP3)) || (gametype[a].implied&(1<<G_M_GSP3)))
#define m_gsp(a,b)          (m_gsp1(a,b) || m_gsp2(a,b) || m_gsp3(a,b))

#define m_team(a,b)         (m_multi(a, b) || !m_ffa(a, b))
#define m_sweaps(a,b)       ((m_race(a) && !m_gsp3(a, b)) || m_insta(a, b) || m_medieval(a, b) || m_kaboom(a, b))
#define m_loadout(a,b)      (!m_classic(a, b) && !m_sweaps(a, b))
#define m_duke(a,b)         (m_duel(a, b) || m_survivor(a, b))
#define m_regen(a,b)        (!m_hard(a,b) && (!m_duke(a, b) || DSG(a, b, regen)) && !m_insta(a, b))
#define m_ghost(a,b)        (m_race(a) && !m_gsp3(a, b))
#define m_bots(a)           (m_play(a) && !m_race(a))
#define m_botbal(a,b)       (m_duel(a, b) ? G(botbalanceduel) : (m_survivor(a, b) ? G(botbalancesurvivor) : G(botbalance)))
#define m_laptime(a,b)      (m_race(a) && m_gsp1(a, b))
#define m_impulsemeter(a,b) ((m_race(a) && m_gsp2(a, b)) || !m_freestyle(a, b))
#define m_nopoints(a,b)     (m_duke(a, b) || (m_bomber(a) && m_gsp1(a, b)) || m_race(a))
#define m_points(a,b)       (!m_nopoints(a, b))

#define m_weapon(a,b)       (m_medieval(a, b) ? W_SWORD : (m_kaboom(a, b) ? W_GRENADE : (m_insta(a, b) ? G(instaweapon) : (m_race(a) && !m_gsp3(a, b) ? G(raceweapon) : G(spawnweapon)))))
#define m_xdelay(a,b,c)     (m_play(a) ? (m_race(a) ? (!m_gsp3(a, b) || c == T_ALPHA ? G(racedelay) : G(racedelayex)) : (m_bomber(a) ? G(bomberdelay) : (m_insta(a, b) ? G(instadelay) : G(spawndelay)))) : 0)
#define m_delay(a,b,c)      (m_duke(a,b) ? 0 : m_xdelay(a, b, c))
#define m_protect(a,b)      (m_duke(a,b) ? DSG(a, b, protect) : (m_insta(a, b) ? G(instaprotect) : G(spawnprotect)))
#define m_health(a,b,c)     (m_insta(a,b) ? 1 : AA(c, health))
#define m_maxhealth(a,b,c)  (int(m_health(a, b, c)*(m_vampire(a,b) ? G(maxhealthvampire) : G(maxhealth))))
#define m_swapteam(a,b)     (m_team(a, b) && (!m_race(a) || m_gsp3(a, b)) && m_play(a) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= 3 && G(teambalanceswap))
#define m_balteam(a,b,c)    (m_team(a, b) && (!m_race(a) || m_gsp3(a, b)) && m_play(a) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= c)
#define m_forcebal(a,b)     ((m_bomber(a) && m_gsp3(a, b)) || (m_race(a) && m_gsp3(a, b)))
#define m_balance(a,b,c)    (m_team(a, b) && (!m_race(a) || m_gsp3(a, b)) && m_play(a) && (m_forcebal(a, b) || ((G(balanceduke) || !m_duke(a, b)) && ((G(balancemaps) >= 0 ? G(balancemaps) : G(mapbalance)) >= (m_affinity(a) ? 1 : (c ? 2 : 3))))))
#define m_balreset(a,b)     (G(balancereset) && (G(balancereset) == 2 || m_capture(a) || m_bomber(a) || m_race(a) || m_duke(a, b)))

#define w_carry(w1,w2)      (isweap(w1) && w1 != W_MELEE && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE))) && (w1 == W_ROCKET || (w1 >= W_OFFSET && w1 < W_ITEM)))
#define w_reload(w1,w2)     (isweap(w1) && (w1 == W_MELEE || (w1 >= W_OFFSET && w1 < W_ITEM) || (isweap(w2) && (w1 == w2 || (w2 == W_GRENADE && w1 == W_MINE)))))
#define w_item(w1,w2)       (isweap(w1) && (w1 >= W_OFFSET && w1 < W_MAX && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE)))))
#define w_attr(a,b,t,w1,w2) (t != WEAPON || m_edit(a) ? w1 : (w1 != w2 ? (!m_classic(a, b) ? (w1 >= W_ITEM ? w1 : -1) : (w1 >= W_OFFSET && w1 < W_MAX ? w1 : -1)) : (w1 != W_GRENADE ? W_GRENADE : W_MINE)))
#define w_spawn(weap)       int(ceilf(G(itemspawntime)*W(weap, frequency)))

#define mapshrink(a,b,c,d) if((a) && (b) && (c) && *(c)) \
{ \
    char *p = shrinklist(b, c, 1, d); \
    if(p) \
    { \
        DELETEA(b); \
        b = p; \
    } \
}
#define mapcull(a,b,c,d,e,f) \
{ \
    mapshrink(m_multi(b, c) && (m_capture(b) || (m_bomber(b) && !m_gsp1(b, c))), a, G(multimaps), false) \
    mapshrink(m_duel(b, c), a, G(duelmaps), false) \
    if((d) > 0 && (e) >= 2 && m_play(b) && !m_duel(b, c)) \
    { \
        mapshrink(G(smallmapmax) && (d) <= G(smallmapmax), a, G(smallmaps), false) \
        else mapshrink(G(mediummapmax) && (d) <= G(mediummapmax), a, G(mediummaps), false) \
        else mapshrink(G(mediummapmax) && (d) > G(mediummapmax), a, G(largemaps), false) \
    } \
    mapshrink(!(f), a, G(previousmaps), true) \
}
#define maplist(a,b,c,d,e,f) \
{ \
    if(m_capture(b)) a = newstring(G(capturemaps)); \
    else if(m_defend(b)) a = newstring(m_gsp2(b, c) ? G(kingmaps) : G(defendmaps)); \
    else if(m_bomber(b)) a = newstring(m_gsp1(b, c) ? G(holdmaps) : G(bombermaps)); \
    else if(m_race(b)) a = newstring(G(racemaps)); \
    else if(m_play(b)) a = newstring(G(mainmaps)); \
    else a = newstring(G(allowmaps)); \
    if(e) mapcull(a, b, c, d, e, f) \
    else mapshrink(!(f), a, G(previousmaps), true) \
}
#ifdef GAMESERVER
SVAR(0, gamestatename, "waiting getmap sendmap readying gameinfo playing overtime intermission voting");
VAR(0, gamestatewaiting, 1, G_S_WAITING, -1);
VAR(0, gamestategetmap, 1, G_S_GETMAP, -1);
VAR(0, gamestatesendmap, 1, G_S_SENDMAP, -1);
VAR(0, gamestatereadying, 1, G_S_READYING, -1);
VAR(0, gamestategameinfo, 1, G_S_GAMEINFO, -1);
VAR(0, gamestateplaying, 1, G_S_PLAYING, -1);
VAR(0, gamestateovertime, 1, G_S_OVERTIME, -1);
VAR(0, gamestateintermission, 1, G_S_INTERMISSION, -1);
VAR(0, gamestatevoting, 1, G_S_VOTING, -1);
VAR(0, gamestatenum, 1, G_S_MAX, -1);
SVAR(0, modename, "demo editing deathmatch capture-the-flag defend-and-control bomber-ball race");
SVAR(0, modeidxname, "demo editing deathmatch capture defend bomber race");
VAR(0, modeidxdemo, 1, G_DEMO, -1);
VAR(0, modeidxediting, 1, G_EDITMODE, -1);
VAR(0, modeidxdeathmatch, 1, G_DEATHMATCH, -1);
VAR(0, modeidxcapture, 1, G_CAPTURE, -1);
VAR(0, modeidxdefend, 1, G_DEFEND, -1);
VAR(0, modeidxbomber, 1, G_BOMBER, -1);
VAR(0, modeidxrace, 1, G_RACE, -1);
VAR(0, modeidxstart, 1, G_START, -1);
VAR(0, modeidxplay, 1, G_PLAY, -1);
VAR(0, modeidxrand, 1, G_RAND, -1);
VAR(0, modeidxnever, 1, G_NEVER, -1);
VAR(0, modeidxlimit, 1, G_LIMIT, -1);
VAR(0, modeidxnum, 1, G_MAX, -1);
VAR(0, modebitdemo, 1, (1<<G_DEMO), -1);
VAR(0, modebitediting, 1, (1<<G_EDITMODE), -1);
VAR(0, modebitdeathmatch, 1, (1<<G_DEATHMATCH), -1);
VAR(0, modebitcapture, 1, (1<<G_CAPTURE), -1);
VAR(0, modebitdefend, 1, (1<<G_DEFEND), -1);
VAR(0, modebitbomber, 1, (1<<G_BOMBER), -1);
VAR(0, modebitrace, 1, (1<<G_RACE), -1);
VAR(0, modebitall, 1, G_ALL, -1);
SVAR(0, mutsname, "multi ffa coop instagib medieval kaboom duel survivor classic onslaught freestyle vampire resize hard basic");
SVAR(0, mutsidxname, "multi ffa coop instagib medieval kaboom duel survivor classic onslaught freestyle vampire resize hard basic");
VAR(0, mutsidxmulti, 1, G_M_MULTI, -1);
VAR(0, mutsidxffa, 1, G_M_FFA, -1);
VAR(0, mutsidxcoop, 1, G_M_COOP, -1);
VAR(0, mutsidxinstagib, 1, G_M_INSTA, -1);
VAR(0, mutsidxmedieval, 1, G_M_MEDIEVAL, -1);
VAR(0, mutsidxkaboom, 1, G_M_KABOOM, -1);
VAR(0, mutsidxduel, 1, G_M_DUEL, -1);
VAR(0, mutsidxsurvivor, 1, G_M_SURVIVOR, -1);
VAR(0, mutsidxclassic, 1, G_M_CLASSIC, -1);
VAR(0, mutsidxonslaught, 1, G_M_ONSLAUGHT, -1);
VAR(0, mutsidxfreestyle, 1, G_M_FREESTYLE, -1);
VAR(0, mutsidxvampire, 1, G_M_VAMPIRE, -1);
VAR(0, mutsidxresize, 1, G_M_RESIZE, -1);
VAR(0, mutsidxhard, 1, G_M_HARD, -1);
VAR(0, mutsidxbasic, 1, G_M_BASIC, -1);
VAR(0, mutsidxgsp1, 1, G_M_GSP1, -1);
VAR(0, mutsidxgsp2, 1, G_M_GSP2, -1);
VAR(0, mutsidxgsp3, 1, G_M_GSP3, -1);
VAR(0, mutsidxgsn, 1, G_M_GSN, -1);
VAR(0, mutsidxgsp, 1, G_M_GSP, -1);
VAR(0, mutsidxnum, 1, G_M_NUM, -1);
VAR(0, mutsbitmulti, 1, (1<<G_M_MULTI), -1);
VAR(0, mutsbitffa, 1, (1<<G_M_FFA), -1);
VAR(0, mutsbitcoop, 1, (1<<G_M_COOP), -1);
VAR(0, mutsbitinstagib, 1, (1<<G_M_INSTA), -1);
VAR(0, mutsbitmedieval, 1, (1<<G_M_MEDIEVAL), -1);
VAR(0, mutsbitkaboom, 1, (1<<G_M_KABOOM), -1);
VAR(0, mutsbitduel, 1, (1<<G_M_DUEL), -1);
VAR(0, mutsbitsurvivor, 1, (1<<G_M_SURVIVOR), -1);
VAR(0, mutsbitclassic, 1, (1<<G_M_CLASSIC), -1);
VAR(0, mutsbitonslaught, 1, (1<<G_M_ONSLAUGHT), -1);
VAR(0, mutsbitfreestyle, 1, (1<<G_M_FREESTYLE), -1);
VAR(0, mutsbitvampire, 1, (1<<G_M_VAMPIRE), -1);
VAR(0, mutsbitresize, 1, (1<<G_M_RESIZE), -1);
VAR(0, mutsbithard, 1, (1<<G_M_HARD), -1);
VAR(0, mutsbitbasic, 1, (1<<G_M_BASIC), -1);
VAR(0, mutsbitgsp1, 1, (1<<G_M_GSP1), -1);
VAR(0, mutsbitgsp2, 1, (1<<G_M_GSP2), -1);
VAR(0, mutsbitgsp3, 1, (1<<G_M_GSP3), -1);
VAR(0, mutsbitall, 1, G_M_ALL, -1);
#endif
