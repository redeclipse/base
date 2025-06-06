#define G_ENUM(en, um) \
    en(um, Demo, DEMO) en(um, Editing, EDITING) en(um, Deathmatch, DEATHMATCH) en(um, Capture the Flag, CAPTURE) \
    en(um, Defend and Control, DEFEND) en(um, Bomber, BOMBER) en(um, Speedrun, SPEEDRUN) en(um, Maximum, MAX)
ENUM_DLN(G)

ENUM_VAR(G_START, G_EDITING);
ENUM_VAR(G_PLAY, G_DEATHMATCH);
ENUM_VAR(G_RAND, G_BOMBER-G_DEATHMATCH+1);
ENUM_VAR(G_COUNT, G_MAX-G_PLAY);
ENUM_VAR(G_NEVER, (1<<G_DEMO)|(1<<G_EDITING));
ENUM_VAR(G_LIMIT, (1<<G_DEATHMATCH)|(1<<G_CAPTURE)|(1<<G_DEFEND)|(1<<G_BOMBER));
ENUM_VAR(G_ALL, (1<<G_DEMO)|(1<<G_EDITING)|(1<<G_DEATHMATCH)|(1<<G_CAPTURE)|(1<<G_DEFEND)|(1<<G_BOMBER)|(1<<G_SPEEDRUN));
ENUM_VAR(G_SW, (1<<G_SPEEDRUN));

#define G_M_ENUM(en, um) \
    en(um, Free for All, FFA) en(um, Coop, COOP) en(um, Instagib, INSTAGIB) en(um, Medieval, MEDIEVAL) en(um, Kaboom, KABOOM) en(um, Duel, DUEL) en(um, Survivor, SURVIVOR) \
    en(um, Classic, CLASSIC) en(um, Onslaught, ONSLAUGHT) en(um, Vampire, VAMPIRE) en(um, Resize, RESIZE) en(um, HARD, HARD) en(um, Arena, ARENA) en(um, Dark, DARK) \
    en(um, Game Specific 1, GSP1) en(um, Game Specific 2, GSP2) en(um, Game Specific 3, GSP3) en(um, Maximum, MAX)
ENUM_DLN(G_M)

#define G_M_GSN (G_M_MAX-G_M_GSP1)
ENUM_VAR(G_M_GSP, G_M_GSP1);
ENUM_VAR(G_M_ALL, (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3));
ENUM_VAR(G_M_FILTER, (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3));
ENUM_VAR(G_M_ROTATE, (1<<G_M_FFA)|(1<<G_M_ARENA)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3));
ENUM_VAR(G_M_DUKE, (1<<G_M_DUEL)|(1<<G_M_SURVIVOR));

#define G_S_ENUM(en, um) \
    en(um, Waiting, WAITING) en(um, Getting Map, GETMAP) en(um, Sending Map, SENDMAP) en(um, Readying, READYING) en(um, Syncing Info, GAMEINFO) \
    en(um, Playing, PLAYING) en(um, Overtime, OVERTIME) en(um, Intermission, INTERMISSION) en(um, Voting, VOTING) en(um, Maximum, MAX)
ENUM_DLN(G_S);

#define gs_waiting(a) (a >= G_S_WAITING && a <= G_S_GAMEINFO)
#define gs_playing(a) (a >= G_S_PLAYING && a <= G_S_OVERTIME)
#define gs_intermission(a) (a >= G_S_INTERMISSION && a <= G_S_VOTING)

#define G_F_ENUM(en, um) en(um, Game Specific, GSP) en(um, Maximum, MAX)
ENUM_DLN(G_F);

struct gametypes
{
    int type, flags, implied, mutators[1+G_M_GSN];
    const char *name, *sname, *gsp[G_M_GSN], *desc, *gsd[G_M_GSN];
};
struct mutstypes
{
    int type, implied, mutators;
    const char *name, *desc;
};
#ifdef CPP_GAME_SERVER
gametypes gametype[] = {
    {
        G_DEMO, 0, 0, { 0, 0, 0, 0 },
        "Demo", "Demo", { "", "", "" },
        "Play back previously recorded games", { "", "", "" },
    },
    {
        G_EDITING, 0, (1<<G_M_FFA)|(1<<G_M_ARENA),
        {
            (1<<G_M_FFA)|(1<<G_M_CLASSIC)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA),
            0, 0, 0
        },
        "Editing", "Editing", { "", "", "" },
        "Create and edit existing maps", { "", "", "" },
    },
    {
        G_DEATHMATCH, 0, 0,
        {
            (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            0
        },
        "Deathmatch", "DM", { "Gladiator", "Old School", "" },
        "Shoot to kill and increase score by fragging", { "Fight in a confined area with increased pushback from damage", "Every frag only gives you a single point, like the old days", "" },
    },
    {
        G_CAPTURE, 0, 0,
        {
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP2),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP3)
        },
        "Capture the Flag", "Capture", { "Quick", "Defend", "Protect" },
        "Take the enemy flag and return it to the base to score", { "Dropped flags instantly return to base", "Dropped flags must be defended until they reset", "Protect the flag and hold the enemy flag to score" },
    },
    {
        G_DEFEND, 0, 0,
        {
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2),
            0
        },
        "Defend and Control", "Defend", { "Quick", "King", "" },
        "Defend control points to score", { "Control points secure quicker than normal", "Remain king of the hill to score", ""},
    },
    {
        G_BOMBER, (1<<G_F_GSP), 0,
        {
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "Bomber Ball", "Bomber", { "Hold", "Basket", "Assault" },
        "Carry the bomb into the enemy goal to score", { "Hold the bomb as long as possible to score", "Throw the bomb into the enemy goal to score", "Teams take turns attacking and defending" },
    },
    {
        G_SPEEDRUN, (1<<G_F_GSP), 0,
        {
            (1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_FFA)|(1<<G_M_ONSLAUGHT)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
            (1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3)
        },
        "Speedrun", "Speedrun", { "Lapped", "Endurance", "Gauntlet" },
        "Try to complete the obstacle course in the fastest time", { "Try to complete the obstacle course the most number of times", "Laps must be completed without dying at all", "Teams take turns running the gauntlet" },
    }
};
mutstypes mutstype[] = {
    {
        G_M_FFA, (1<<G_M_FFA),
        (1<<G_M_FFA)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "FFA", "Every player for themselves"
    },
    {
        G_M_COOP, (1<<G_M_COOP),
        (1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Coop", "Players versus drones"
    },
    {
        G_M_INSTAGIB, (1<<G_M_INSTAGIB),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_HARD)|(1<<G_M_RESIZE)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Instagib", "One hit kills instantly"
    },
    {
        G_M_MEDIEVAL, (1<<G_M_MEDIEVAL),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_MEDIEVAL)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Medieval", "Players spawn only with swords"
    },
    {
        G_M_KABOOM,  (1<<G_M_KABOOM),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Kaboom", "Players spawn with explosives only"
    },
    {
        G_M_DUEL, (1<<G_M_DUEL),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_DUEL)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Duel", "One on one battles to determine the winner"
    },
    {
        G_M_SURVIVOR, (1<<G_M_SURVIVOR),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Survivor", "Players battle to determine the winner"
    },
    {
        G_M_CLASSIC, (1<<G_M_CLASSIC),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Classic", "Weapons must be collected from spawns in the arena"
    },
    {
        G_M_ONSLAUGHT, (1<<G_M_ONSLAUGHT),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Onslaught", "Waves of enemies fill the battle arena"
    },
    {
        G_M_VAMPIRE, (1<<G_M_VAMPIRE),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Vampire", "Deal damage to regenerate health"
    },
    {
        G_M_RESIZE, (1<<G_M_RESIZE),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Resize", "Players change size depending on their health"
    },
    {
        G_M_HARD, (1<<G_M_HARD),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Hard", "Radar and health regeneration is disabled"
    },
    {
        G_M_ARENA, (1<<G_M_ARENA),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Arena", "Players are able to carry all weapons at once"
    },
    {
        G_M_DARK, (1<<G_M_DARK),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Dark", "Murder in the dark"
    },
    {
        G_M_GSP1, (1<<G_M_GSP1),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 1", ""
    },
    {
        G_M_GSP2, (1<<G_M_GSP2),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_DARK)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 2", ""
    },
    {
        G_M_GSP3, (1<<G_M_GSP3),
        (1<<G_M_FFA)|(1<<G_M_COOP)|(1<<G_M_INSTAGIB)|(1<<G_M_MEDIEVAL)|(1<<G_M_KABOOM)|(1<<G_M_DUEL)|(1<<G_M_SURVIVOR)|(1<<G_M_CLASSIC)|(1<<G_M_ONSLAUGHT)|(1<<G_M_VAMPIRE)|(1<<G_M_RESIZE)|(1<<G_M_HARD)|(1<<G_M_ARENA)|(1<<G_M_GSP1)|(1<<G_M_GSP2)|(1<<G_M_GSP3),
        "Game Specific 3", ""
    },
};
#else
extern gametypes gametype[];
extern mutstypes mutstype[];
#endif

#define m_game(a)           (a > -1 && a < G_MAX)
#define m_check(a,b,c,d)    ((!a || (a < 0 ? !((0 - a)&(1<<(c - G_PLAY))) : a&(1<<(c - G_PLAY)))) && (!b || (b < 0 ? !((0 - b)&d) : b&d)))
#define m_local(a)          (a == G_DEMO)

#define m_demo(a)           (a == G_DEMO)
#define m_edit(a)           (a == G_EDITING)
#define m_dm(a)             (a == G_DEATHMATCH)
#define m_capture(a)        (a == G_CAPTURE)
#define m_defend(a)         (a == G_DEFEND)
#define m_bomber(a)         (a == G_BOMBER)
#define m_speedrun(a)       (a == G_SPEEDRUN)

#define m_play(a)           (a >= G_PLAY)
#define m_affinity(a)       (m_capture(a) || m_defend(a) || m_bomber(a))

#define m_ffa(a,b)          ((b&(1<<G_M_FFA)) || (gametype[a].implied&(1<<G_M_FFA)))
#define m_coop(a,b)         ((b&(1<<G_M_COOP)) || (gametype[a].implied&(1<<G_M_COOP)))
#define m_insta(a,b)        ((b&(1<<G_M_INSTAGIB)) || (gametype[a].implied&(1<<G_M_INSTAGIB)))
#define m_medieval(a,b)     ((b&(1<<G_M_MEDIEVAL)) || (gametype[a].implied&(1<<G_M_MEDIEVAL)))
#define m_kaboom(a,b)       ((b&(1<<G_M_KABOOM)) || (gametype[a].implied&(1<<G_M_KABOOM)))
#define m_duel(a,b)         ((b&(1<<G_M_DUEL)) || (gametype[a].implied&(1<<G_M_DUEL)))
#define m_survivor(a,b)     ((b&(1<<G_M_SURVIVOR)) || (gametype[a].implied&(1<<G_M_SURVIVOR)))
#define m_classic(a,b)      ((b&(1<<G_M_CLASSIC)) || (gametype[a].implied&(1<<G_M_CLASSIC)))
#define m_onslaught(a,b)    ((b&(1<<G_M_ONSLAUGHT)) || (gametype[a].implied&(1<<G_M_ONSLAUGHT)))
#define m_vampire(a,b)      ((b&(1<<G_M_VAMPIRE)) || (gametype[a].implied&(1<<G_M_VAMPIRE)))
#define m_resize(a,b)       ((b&(1<<G_M_RESIZE)) || (gametype[a].implied&(1<<G_M_RESIZE)))
#define m_hard(a,b)         ((b&(1<<G_M_HARD)) || (gametype[a].implied&(1<<G_M_HARD)))
#define m_arena(a,b)        ((b&(1<<G_M_ARENA)) || (gametype[a].implied&(1<<G_M_ARENA)))
#define m_dark(a,b)         ((b&(1<<G_M_DARK)) || (gametype[a].implied&(1<<G_M_DARK)))

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
#define m_bb_assault(a,b)   (m_bomber(a) && m_gsp3(a, b))

#define m_ra_lapped(a,b)    (m_speedrun(a) && m_gsp1(a, b))
#define m_ra_timed(a,b)     (m_speedrun(a) && !m_gsp1(a, b))
#define m_ra_endurance(a,b) (m_speedrun(a) && m_gsp2(a, b))
#define m_ra_gauntlet(a,b)  (m_speedrun(a) && m_gsp3(a, b))

#define m_team(a,b)         (!m_ffa(a, b))
#define m_single(a,b)       (m_dm_gladiator(a, b) || (m_speedrun(a) && !m_ra_gauntlet(a, b)) || m_insta(a, b) || m_medieval(a, b)) // games that give you only a single weapon (for *extra values)
#define m_sweaps(a,b)       (m_dm_gladiator(a, b) || (m_speedrun(a) && !m_ra_gauntlet(a, b)) || m_insta(a, b) || m_medieval(a, b) || m_kaboom(a, b)) // games that do not require selecing a spawn loadout but also don't have you pick one up
#define m_rotweaps(a,b)     (m_dm_gladiator(a, b) || (m_speedrun(a) && !m_ra_gauntlet(a, b)) || m_medieval(a, b) || m_kaboom(a, b)) // games that require picking up rotation weapons
#define m_loadout(a,b)      (!m_classic(a, b) && !m_sweaps(a, b)) // games that require selecting a spawn loadout: non-classic, non-([non-gladiator speedrun],gauntlet,insta,medieval,kaboom)
#define m_duke(a,b)         (m_duel(a, b) || m_survivor(a, b))
#define m_regen(a,b)        (!m_hard(a,b) && (!m_dm_gladiator(a, b) || G(gladiatorregen))&& (!m_duke(a, b) || DSG(a, b, regen)) && !m_insta(a, b))
#define m_ghost(a,b)        (m_speedrun(a) && !m_ra_gauntlet(a, b))
#define m_bots(a)           (m_play(a) && !m_speedrun(a))
#define m_botbal(a,b)       (m_duel(a, b) ? G(botbalanceduel) : (m_survivor(a, b) ? G(botbalancesurvivor) : G(botbalance)))
#define m_nopoints(a,b)     (m_duke(a, b) || m_bb_hold(a, b) || m_speedrun(a))
#define m_points(a,b)       (!m_nopoints(a, b))
#define m_normweaps(a,b)    (!m_speedrun(a) && !m_insta(a,b) && !m_medieval(a,b) && !m_kaboom(a,b) && !m_dm_gladiator(a,b))
#define m_lasthit(a,b)      (m_dm_gladiator(a,b) && m_points(a,b))

#define m_weapon(at,a,b)    (m_medieval(a, b) ? A(at, weaponmedieval) : (m_kaboom(a, b) ? A(at, weaponkaboom) : (m_insta(a, b) ? A(at, weaponinsta) : (m_speedrun(a) && !m_ra_gauntlet(a, b) ? A(at, weaponspeedrun) : (m_dm_gladiator(a, b) ? A(at, weapongladiator) : A(at, weaponspawn))))))
#define m_maxcarry(at,a,b)  (at < A_ENEMY && m_arena(a, b) ? W_LOADOUT : A(at, maxcarry))
#define m_delay(at,a,b,c)   (!m_duke(a,b) ? int((m_edit(a) ? A(at, spawndelayedit) : (m_speedrun(a) ? (!m_ra_gauntlet(a, b) || c == T_ALPHA ? A(at, spawndelayspeedrun) : A(at, spawndelaygauntlet)) : (m_bomber(a) ? A(at, spawndelaybomber) : (m_defend(a) ? A(at, spawndelaydefend) : (m_capture(a) ? A(at, spawndelaycapture) : A(at, spawndelay))))))*(m_insta(a, b) ? A(at, spawndelayinstascale) : 1.f)) : 0)
#define m_protect(a,b)      (m_duke(a,b) ? DSG(a, b, protect) : (m_insta(a, b) ? G(instaprotect) : G(spawnprotect)))
#define m_teamspawn(a,b)    (m_team(a, b) && (!m_speedrun(a) || m_ra_gauntlet(a, b)))
#define m_swapteam(a,b)     (m_play(a) && m_teamspawn(a, b) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= 3 && G(teambalanceswap))
#define m_balteam(a,b,c)    (m_play(a) && m_teamspawn(a, b) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= c)
#define m_forcebal(a,b)     (m_bb_assault(a, b) || m_ra_gauntlet(a, b))
#define m_balance(a,b,c)    (m_play(a) && m_teamspawn(a, b) && (m_forcebal(a, b) || ((G(balanceduke) || !m_duke(a, b)) && ((G(balancemaps) >= 0 ? G(balancemaps) : G(mapbalance)) >= (m_affinity(a) ? 1 : (c ? 2 : 3))))))
#define m_balreset(a,b)     (G(balancereset) && (G(balancereset) == 2 || m_capture(a) || m_bomber(a) || m_speedrun(a) || m_duke(a, b)))

#define m_messy(a,b)        ((m_insta(a,b) ? G(janitorjunkmessy) : 1.f) * (m_kaboom(a,b) ? G(janitorjunkmessy) : 1.f))

#ifdef CPP_GAME_SERVER
#define m_attr(a,b)         (a == WEAPON ? attrmap[isweap(b) ? b : W_REPLACE] : b)
#else
#define m_attr(a,b)         (a == WEAPON ? game::attrmap[isweap(b) ? b : W_REPLACE] : b)
#endif

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
                                            (m_speedrun(a) ? \
                                                (m_ra_lapped(a, b) ? G(c##speedrunlapped) : \
                                                    (m_ra_gauntlet(a, b) ? G(c##speedrungauntlet) : G(c##speedrun)) \
                                                ) : 0 \
                                            ) \
                                        ) \
                                    ) \
                                ) \
                            )

#define MMVAR(flags, level, varname, minval, maxval, defaultval, duelval, survivorval, gladiatorval, captureval, defendval, defendkingval, bomberval, bomberholdval, speedrunval, speedrunlappedval, speedrungauntletval) \
    GVAR(flags, level, varname, minval, defaultval, maxval); \
    GVAR(flags, level, varname##duel, minval, duelval, maxval); \
    GVAR(flags, level, varname##survivor, minval, survivorval, maxval); \
    GVAR(flags, level, varname##gladiator, minval, gladiatorval, maxval); \
    GVAR(flags, level, varname##capture, minval, captureval, maxval); \
    GVAR(flags, level, varname##defend, minval, defendval, maxval); \
    GVAR(flags, level, varname##defendking, minval, defendkingval, maxval); \
    GVAR(flags, level, varname##bomber, minval, bomberval, maxval); \
    GVAR(flags, level, varname##bomberhold, minval, bomberholdval, maxval); \
    GVAR(flags, level, varname##speedrun, minval, speedrunval, maxval); \
    GVAR(flags, level, varname##speedrunlapped, minval, speedrunlappedval, maxval); \
    GVAR(flags, level, varname##speedrungauntlet, minval, speedrungauntletval, maxval);

#define DSG(a,b,x) (m_duel(a, b) ? G(duel##x) : G(survivor##x))

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
    mapshrink(m_duel(b, c), a, G(duelmaps), false) \
    if((d) > 0 && (e) >= 2 && m_play(b) && !m_duel(b, c)) \
    { \
        mapshrink(G(smallmapmax) && (d) <= G(smallmapmax), a, G(smallmaps), false) \
        else mapshrink(G(mediummapmax) && (d) <= G(mediummapmax), a, G(mediummaps), false) \
        else mapshrink(G(mediummapmax) && (d) > G(mediummapmax), a, G(largemaps), false) \
    } \
    mapshrink(!(f), a, G(previousmaps), true) \
}
// note that maplist removes entries in previousmaps
#define maplist(a,b,c,d,e,f) \
{ \
    if(m_capture(b)) a = newstring(G(capturemaps)); \
    else if(m_defend(b)) a = newstring(m_dac_king(b, c) ? G(kingmaps) : G(defendmaps)); \
    else if(m_bomber(b)) a = newstring(m_bb_hold(b, c) ? G(holdmaps) : G(bombermaps)); \
    else if(m_speedrun(b)) a = newstring(G(speedrunmaps)); \
    else if(m_dm(b)) a = newstring(m_dm_gladiator(b, c) ? G(gladiatormaps) : G(mainmaps)); \
    else a = newstring(G(allowmaps)); \
    if(e) mapcull(a, b, c, d, e, f) \
    else mapshrink(!(f), a, G(previousmaps), true) \
}
// allowmaplist doesn't cut previous maps
#define allowmaplist(a,b,c) \
{ \
    if(m_capture(b)) a = newstring(G(capturemaps)); \
    else if(m_defend(b)) a = newstring(m_dac_king(b, c) ? G(kingmaps) : G(defendmaps)); \
    else if(m_bomber(b)) a = newstring(m_bb_hold(b, c) ? G(holdmaps) : G(bombermaps)); \
    else if(m_speedrun(b)) a = newstring(G(speedrunmaps)); \
    else if(m_dm(b)) a = newstring(m_dm_gladiator(b, c) ? G(gladiatormaps) : G(mainmaps)); \
    else a = newstring(G(allowmaps)); \
}
