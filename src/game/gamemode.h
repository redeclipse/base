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
const char *gamestates[4][G_S_MAX] = {
    { "waiting", "getmap", "sendmap", "readying", "syncing", "playing", "overtime", "intermission", "voting" },
    { "Waiting", "Getmap", "Sendmap", "Readying", "Syncing", "Playing", "Overtime", "Intermission", "Voting" },
    { "waiting to start", "server getting map", "server sending map", "waiting for ready players", "syncing game information", "playing", "overtime", "intermission", "voting in progress" },
    { "Waiting to start", "Server getting map", "Server sending map", "Waiting for ready players", "Syncing game information", "Playing", "Overtime", "Intermission", "Voting in progress" }
};
#else
extern const char *gamestates[4][G_S_MAX];
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
        "Demo", "Demo", { "", "", "" },
        "Play back previously recorded games", { "", "", "" },
    },
    {
        G_EDITMODE, 0, (1<<G_M_FFA)|(1<<G_M_CLASSIC),
        {
            (1<<G_M_FFA)|(1<<G_M_CLASSIC)|(1<<G_M_FREESTYLE),
            0, 0, 0
        },
        "Editing", "Editing", { "", "", "" },
        "Create and edit existing maps", { "", "", "" },
    },
    {
        G_DEATHMATCH, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2),
            0
        },
        "Deathmatch", "DM", { "Gladiator", "Old School", "" },
        "Shoot to kill and increase score by fragging", { "Fight in a confined area with increased pushback from damage", "Secure the most number of frags instead of points", "" },
    },
    {
        G_CAPTURE, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP3)
        },
        "Capture the Flag", "Capture", { "Quick", "Defend", "Protect" },
        "Take the enemy flag and return it to the base to score", { "Dropped flags instantly return to base", "Dropped flags must be defended until they reset", "Protect the flag and hold the enemy flag to score" },
    },
    {
        G_DEFEND, 0, 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            0
        },
        "Defend and Control", "Defend", { "Quick", "King", "" },
        "Defend control points to score", { "Control points secure quicker than normal", "Remain king of the hill to score", ""},
    },
    {
        G_BOMBER, (1<<G_F_GSP), 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "Bomber Ball", "Bomber", { "Hold", "Basket", "Attack" },
        "Carry the bomb into the enemy goal to score", { "Hold the bomb as long as possible to score", "Throw the bomb into the enemy goal to score", "Teams take turns attacking and defending" },
    },
    {
        G_RACE, (1<<G_F_GSP), 0,
        {
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_MULTI)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "Race", "Race", { "Timed", "Endurance", "Gauntlet" },
        "Compete for the most number of laps", { "Compete for the fastest time completing a lap", "Impulse meter does not reset at all", "Teams take turns running the gauntlet" },
    }
};
mutstypes mutstype[] = {
    {
        G_M_MULTI, (1<<G_M_MULTI),
        (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Multi", "Four teams battle to determine the winning side"
    },
    {
        G_M_FFA, (1<<G_M_FFA),
        (1<<G_M_FFA)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "FFA", "Every player for themselves"
    },
    {
        G_M_COOP, (1<<G_M_COOP),
        (1<<G_M_MULTI)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Coop", "Players versus drones"
    },
    {
        G_M_INSTA, (1<<G_M_INSTA),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_FREESTYLE)|(1<<G_M_RESIZE)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Instagib", "One hit kills instantly"
    },
    {
        G_M_MEDIEVAL, (1<<G_M_MEDIEVAL),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Medieval", "Players spawn only with swords"
    },
    {
        G_M_KABOOM,  (1<<G_M_KABOOM),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Kaboom", "Players spawn with explosives only"
    },
    {
        G_M_DUEL, (1<<G_M_DUEL),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_DUEL)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Duel", "One on one battles to determine the winner"
    },
    {
        G_M_SURVIVOR, (1<<G_M_SURVIVOR),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Survivor", "Players battle to determine the winner"
    },
    {
        G_M_CLASSIC,    (1<<G_M_CLASSIC),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Classic", "Weapons must be collected from spawns in the arena"
    },
    {
        G_M_ONSLAUGHT, (1<<G_M_ONSLAUGHT),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Onslaught", "Waves of enemies fill the battle arena"
    },
    {
        G_M_FREESTYLE, (1<<G_M_FREESTYLE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Freestyle", "Players can parkour without limits"
    },
    {
        G_M_VAMPIRE, (1<<G_M_VAMPIRE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Vampire", "Deal damage to regenerate health"
    },
    {
        G_M_RESIZE, (1<<G_M_RESIZE),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Resize", "Players change size depending on their health"
    },
    {
        G_M_HARD, (1<<G_M_HARD),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Hard", "No health regeneration, no radar"
    },
    {
        G_M_BASIC, (1<<G_M_BASIC),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Basic", "Players only have the basic weapons that they spawn with"
    },
    {
        G_M_GSP1, (1<<G_M_GSP1),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 1", ""
    },
    {
        G_M_GSP2, (1<<G_M_GSP2),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 2", ""
    },
    {
        G_M_GSP3, (1<<G_M_GSP3),
        (1<<G_M_MULTI)|(1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTA)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_FREESTYLE)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_BASIC)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 3", ""
    },
};
#else
extern gametypes gametype[];
extern mutstypes mutstype[];
#endif

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

#define m_dm_gladiator(a,b) (m_dm(a) && m_gsp1(a, b))
#define m_dm_oldschool(a,b) (m_dm(a) && m_gsp2(a, b))

#define m_ctf_quick(a,b)    (m_capture(a) && m_gsp1(a, b))
#define m_ctf_defend(a,b)   (m_capture(a) && m_gsp2(a, b))
#define m_ctf_protect(a,b)  (m_capture(a) && m_gsp3(a, b))

#define m_dac_quick(a,b)    (m_defend(a) && m_gsp1(a, b))
#define m_dac_king(a,b)     (m_defend(a) && m_gsp2(a, b))

#define m_bb_hold(a,b)      (m_bomber(a) && m_gsp1(a, b))
#define m_bb_basket(a,b)    (m_bomber(a) && m_gsp2(a, b))
#define m_bb_attack(a,b)    (m_bomber(a) && m_gsp3(a, b))

#define m_ra_timed(a,b)     (m_race(a) && m_gsp1(a, b))
#define m_ra_endurance(a,b) (m_race(a) && m_gsp2(a, b))
#define m_ra_gauntlet(a,b)  (m_race(a) && m_gsp3(a, b))

#define m_team(a,b)         (m_multi(a, b) || !m_ffa(a, b))
#define m_sweaps(a,b)       (m_dm_gladiator(a, b) || (m_race(a) && !m_ra_gauntlet(a, b)) || m_insta(a, b) || m_medieval(a, b) || m_kaboom(a, b))
#define m_loadout(a,b)      (!m_classic(a, b) && !m_sweaps(a, b))
#define m_duke(a,b)         (m_duel(a, b) || m_survivor(a, b))
#define m_regen(a,b)        (!m_hard(a,b) && (!m_duke(a, b) || DSG(a, b, regen)) && !m_insta(a, b))
#define m_ghost(a,b)        (m_race(a) && !m_ra_gauntlet(a, b))
#define m_bots(a)           (m_play(a) && !m_race(a))
#define m_botbal(a,b)       (m_duel(a, b) ? G(botbalanceduel) : (m_survivor(a, b) ? G(botbalancesurvivor) : G(botbalance)))
#define m_laptime(a,b)      (m_ra_timed(a, b))
#define m_impulsemeter(a,b) (m_ra_endurance(a, b) || !m_freestyle(a, b))
#define m_nopoints(a,b)     (m_duke(a, b) || m_bb_hold(a, b) || m_race(a))
#define m_points(a,b)       (!m_nopoints(a, b))
#define m_normalweapons(a,b)    (!m_race(a) && !m_insta(a,b) && !m_medieval(a,b) && !m_kaboom(a,b) && !m_dm_gladiator(a,b))
#define m_lasthit(a,b)      (m_dm_gladiator(a,b) && m_points(a,b))

#define m_weapon(at,a,b)    (m_medieval(a, b) ? AA(at, weaponmedieval) : (m_kaboom(a, b) ? AA(at, weaponkaboom) : (m_insta(a, b) ? AA(at, weaponinsta) : (m_race(a) && !m_ra_gauntlet(a, b) ? AA(at, weaponrace) : (m_dm_gladiator(a, b) ? AA(at, weapongladiator) : AA(at, weaponspawn))))))
#define m_delay(at,a,b,c)   ((m_play(a) || at >= A_ENEMY) && !m_duke(a,b) ? int((m_race(a) ? (!m_ra_gauntlet(a, b) || c == T_ALPHA ? AA(at, spawndelayrace) : AA(at, spawndelaygauntlet)) : (m_bomber(a) ? AA(at, spawndelaybomber) : (m_defend(a) ? AA(at, spawndelaydefend) : (m_capture(a) ? AA(at, spawndelaycapture) : AA(at, spawndelay)))))*(m_insta(a, b) ? AA(at, spawndelayinstascale) : 1.f)) : 0)
#define m_protect(a,b)      (m_duke(a,b) ? DSG(a, b, protect) : (m_insta(a, b) ? G(instaprotect) : G(spawnprotect)))
#define m_health(a,b,c)     (m_insta(a,b) ? 1 : AA(c, health))
#define m_maxhealth(a,b,c)  (int(m_health(a, b, c)*(m_vampire(a,b) ? G(maxhealthvampire) : G(maxhealth))))
#define m_swapteam(a,b)     (m_team(a, b) && (!m_race(a) || m_ra_gauntlet(a, b)) && m_play(a) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= 3 && G(teambalanceswap))
#define m_balteam(a,b,c)    (m_team(a, b) && (!m_race(a) || m_ra_gauntlet(a, b)) && m_play(a) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= c)
#define m_forcebal(a,b)     (m_bb_attack(a, b) || m_ra_gauntlet(a, b))
#define m_balance(a,b,c)    (m_team(a, b) && (!m_race(a) || m_ra_gauntlet(a, b)) && m_play(a) && (m_forcebal(a, b) || ((G(balanceduke) || !m_duke(a, b)) && ((G(balancemaps) >= 0 ? G(balancemaps) : G(mapbalance)) >= (m_affinity(a) ? 1 : (c ? 2 : 3))))))
#define m_balreset(a,b)     (G(balancereset) && (G(balancereset) == 2 || m_capture(a) || m_bomber(a) || m_race(a) || m_duke(a, b)))

#define w_carry(w1,w2)      (isweap(w1) && w1 != W_CLAW && w1 < W_ALL && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE))) && (w1 == W_ROCKET || (w1 >= W_OFFSET && w1 < W_ITEM)))
#define w_reload(w1,w2)     (isweap(w1) && (w1 >= W_ALL || (w1 >= W_OFFSET && w1 < W_ITEM) || (isweap(w2) && (w1 == w2 || (w2 == W_GRENADE && w1 == W_MINE)))))
#define w_item(w1,w2)       (isweap(w1) && (w1 >= W_OFFSET && w1 < W_ALL && (!isweap(w2) || (w1 != w2 && (w2 != W_GRENADE || w1 != W_MINE)))))
#define w_attr(a,b,t,w1,w2) (t != WEAPON || m_edit(a) ? w1 : (w1 != w2 ? (!m_classic(a, b) ? (w1 >= W_ITEM ? w1 : -1) : (w1 >= W_OFFSET && w1 < W_ALL ? w1 : -1)) : (w1 != W_GRENADE ? W_GRENADE : W_MINE)))
#define w_spawn(weap)       int(ceilf(G(itemspawntime)*W(weap, frequency)))

#define m_mmvar(a,b,c)      (m_dm(a) ? \
                                (m_duel(a, b) ? G(c##duel) : \
                                    (m_survivor(a, b) ? G(c##survivor) : \
                                        (m_dm_gladiator(a, b) ? G(c##gladiator) : G(c)) \
                                    ) \
                                ) : \
                                (m_capture(a) ? G(c##capture) : \
                                    (m_defend(a) ? \
                                        (m_dac_king(a, b) ? G(c##defendking) : G(c##defend)) : \
                                        (m_bomber(a) ? (m_bb_hold(a, b) ? G(c##bomberhold) : G(c##bomber)) : \
                                            (m_race(a) ? \
                                                (m_ra_timed(a, b) ? G(c##racetimed) : \
                                                    (m_ra_gauntlet(a, b) ? G(c##racegauntlet) : G(c##race)) \
                                                ) : 0 \
                                            ) \
                                        ) \
                                    ) \
                                ) \
                            )

#define MMVAR(f,a,b,c,w01,w02,w03,w04,w05,w06,w07,w08,w09,w10,w11,w12) \
    GVAR(f, a, b, w01, c); \
    GVAR(f, a##duel, b, w02, c); \
    GVAR(f, a##survivor, b, w03, c); \
    GVAR(f, a##gladiator, b, w04, c); \
    GVAR(f, a##capture, b, w05, c); \
    GVAR(f, a##defend, b, w06, c); \
    GVAR(f, a##defendking, b, w07, c); \
    GVAR(f, a##bomber, b, w08, c); \
    GVAR(f, a##bomberhold, b, w09, c); \
    GVAR(f, a##race, b, w10, c); \
    GVAR(f, a##racetimed, b, w11, c); \
    GVAR(f, a##racegauntlet, b, w12, c);

#define DSG(a,b,x)          (m_duel(a, b) ? G(duel##x) : G(survivor##x))
#define GL(a,b,x)           (m_dm_gladiator(a, b) ? gladiator##x : x)

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
    mapshrink(m_multi(b, c) && (m_capture(b) || (m_bomber(b) && !m_bb_hold(b, c))), a, G(multimaps), false) \
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
    else if(m_defend(b)) a = newstring(m_dac_king(b, c) ? G(kingmaps) : G(defendmaps)); \
    else if(m_bomber(b)) a = newstring(m_bb_hold(b, c) ? G(holdmaps) : G(bombermaps)); \
    else if(m_race(b)) a = newstring(G(racemaps)); \
    else if(m_dm(b)) a = newstring(m_dm_gladiator(b, c) ? G(gladiatormaps) : G(mainmaps)); \
    else a = newstring(G(allowmaps)); \
    if(e) mapcull(a, b, c, d, e, f) \
    else mapshrink(!(f), a, G(previousmaps), true) \
}
#ifdef GAMESERVER
SVAR(IDF_READONLY, gamestatename, "waiting getmap sendmap readying gameinfo playing overtime intermission voting");
VAR(IDF_READONLY, gamestatewaiting, 1, G_S_WAITING, -1);
VAR(IDF_READONLY, gamestategetmap, 1, G_S_GETMAP, -1);
VAR(IDF_READONLY, gamestatesendmap, 1, G_S_SENDMAP, -1);
VAR(IDF_READONLY, gamestatereadying, 1, G_S_READYING, -1);
VAR(IDF_READONLY, gamestategameinfo, 1, G_S_GAMEINFO, -1);
VAR(IDF_READONLY, gamestateplaying, 1, G_S_PLAYING, -1);
VAR(IDF_READONLY, gamestateovertime, 1, G_S_OVERTIME, -1);
VAR(IDF_READONLY, gamestateintermission, 1, G_S_INTERMISSION, -1);
VAR(IDF_READONLY, gamestatevoting, 1, G_S_VOTING, -1);
VAR(IDF_READONLY, gamestatenum, 1, G_S_MAX, -1);
SVAR(IDF_READONLY, modename, "Demo Editing Deathmatch [Capture the Flag] [Defend and Control] [Bomber Ball] [Race]");
SVAR(IDF_READONLY, modeidxname, "demo editing deathmatch capture defend bomber race");
VAR(IDF_READONLY, modeidxdemo, 1, G_DEMO, -1);
VAR(IDF_READONLY, modeidxediting, 1, G_EDITMODE, -1);
VAR(IDF_READONLY, modeidxdeathmatch, 1, G_DEATHMATCH, -1);
VAR(IDF_READONLY, modeidxcapture, 1, G_CAPTURE, -1);
VAR(IDF_READONLY, modeidxdefend, 1, G_DEFEND, -1);
VAR(IDF_READONLY, modeidxbomber, 1, G_BOMBER, -1);
VAR(IDF_READONLY, modeidxrace, 1, G_RACE, -1);
VAR(IDF_READONLY, modeidxstart, 1, G_START, -1);
VAR(IDF_READONLY, modeidxplay, 1, G_PLAY, -1);
VAR(IDF_READONLY, modeidxrand, 1, G_RAND, -1);
VAR(IDF_READONLY, modeidxnever, 1, G_NEVER, -1);
VAR(IDF_READONLY, modeidxlimit, 1, G_LIMIT, -1);
VAR(IDF_READONLY, modeidxnum, 1, G_MAX, -1);
VAR(IDF_READONLY, modebitdemo, 1, (1<<G_DEMO), -1);
VAR(IDF_READONLY, modebitediting, 1, (1<<G_EDITMODE), -1);
VAR(IDF_READONLY, modebitdeathmatch, 1, (1<<G_DEATHMATCH), -1);
VAR(IDF_READONLY, modebitcapture, 1, (1<<G_CAPTURE), -1);
VAR(IDF_READONLY, modebitdefend, 1, (1<<G_DEFEND), -1);
VAR(IDF_READONLY, modebitbomber, 1, (1<<G_BOMBER), -1);
VAR(IDF_READONLY, modebitrace, 1, (1<<G_RACE), -1);
VAR(IDF_READONLY, modebitall, 1, G_ALL, -1);
SVAR(IDF_READONLY, mutsname, "Multi FFA Coop Instagib Medieval Kaboom Duel Survivor Classic Onslaught Freestyle Vampire Resize Hard Basic");
SVAR(IDF_READONLY, mutsidxname, "multi ffa coop instagib medieval kaboom duel survivor classic onslaught freestyle vampire resize hard basic");
VAR(IDF_READONLY, mutsidxmulti, 1, G_M_MULTI, -1);
VAR(IDF_READONLY, mutsidxffa, 1, G_M_FFA, -1);
VAR(IDF_READONLY, mutsidxcoop, 1, G_M_COOP, -1);
VAR(IDF_READONLY, mutsidxinstagib, 1, G_M_INSTA, -1);
VAR(IDF_READONLY, mutsidxmedieval, 1, G_M_MEDIEVAL, -1);
VAR(IDF_READONLY, mutsidxkaboom, 1, G_M_KABOOM, -1);
VAR(IDF_READONLY, mutsidxduel, 1, G_M_DUEL, -1);
VAR(IDF_READONLY, mutsidxsurvivor, 1, G_M_SURVIVOR, -1);
VAR(IDF_READONLY, mutsidxclassic, 1, G_M_CLASSIC, -1);
VAR(IDF_READONLY, mutsidxonslaught, 1, G_M_ONSLAUGHT, -1);
VAR(IDF_READONLY, mutsidxfreestyle, 1, G_M_FREESTYLE, -1);
VAR(IDF_READONLY, mutsidxvampire, 1, G_M_VAMPIRE, -1);
VAR(IDF_READONLY, mutsidxresize, 1, G_M_RESIZE, -1);
VAR(IDF_READONLY, mutsidxhard, 1, G_M_HARD, -1);
VAR(IDF_READONLY, mutsidxbasic, 1, G_M_BASIC, -1);
VAR(IDF_READONLY, mutsidxgsp1, 1, G_M_GSP1, -1);
VAR(IDF_READONLY, mutsidxgsp2, 1, G_M_GSP2, -1);
VAR(IDF_READONLY, mutsidxgsp3, 1, G_M_GSP3, -1);
VAR(IDF_READONLY, mutsidxgsn, 1, G_M_GSN, -1);
VAR(IDF_READONLY, mutsidxgsp, 1, G_M_GSP, -1);
VAR(IDF_READONLY, mutsidxnum, 1, G_M_NUM, -1);
VAR(IDF_READONLY, mutsidxnormal, 1, G_M_NUM-G_M_GSN, -1);
VAR(IDF_READONLY, mutsbitmulti, 1, (1<<G_M_MULTI), -1);
VAR(IDF_READONLY, mutsbitffa, 1, (1<<G_M_FFA), -1);
VAR(IDF_READONLY, mutsbitcoop, 1, (1<<G_M_COOP), -1);
VAR(IDF_READONLY, mutsbitinstagib, 1, (1<<G_M_INSTA), -1);
VAR(IDF_READONLY, mutsbitmedieval, 1, (1<<G_M_MEDIEVAL), -1);
VAR(IDF_READONLY, mutsbitkaboom, 1, (1<<G_M_KABOOM), -1);
VAR(IDF_READONLY, mutsbitduel, 1, (1<<G_M_DUEL), -1);
VAR(IDF_READONLY, mutsbitsurvivor, 1, (1<<G_M_SURVIVOR), -1);
VAR(IDF_READONLY, mutsbitclassic, 1, (1<<G_M_CLASSIC), -1);
VAR(IDF_READONLY, mutsbitonslaught, 1, (1<<G_M_ONSLAUGHT), -1);
VAR(IDF_READONLY, mutsbitfreestyle, 1, (1<<G_M_FREESTYLE), -1);
VAR(IDF_READONLY, mutsbitvampire, 1, (1<<G_M_VAMPIRE), -1);
VAR(IDF_READONLY, mutsbitresize, 1, (1<<G_M_RESIZE), -1);
VAR(IDF_READONLY, mutsbithard, 1, (1<<G_M_HARD), -1);
VAR(IDF_READONLY, mutsbitbasic, 1, (1<<G_M_BASIC), -1);
VAR(IDF_READONLY, mutsbitgsp1, 1, (1<<G_M_GSP1), -1);
VAR(IDF_READONLY, mutsbitgsp2, 1, (1<<G_M_GSP2), -1);
VAR(IDF_READONLY, mutsbitgsp3, 1, (1<<G_M_GSP3), -1);
VAR(IDF_READONLY, mutsbitall, 1, G_M_ALL, -1);
#endif
