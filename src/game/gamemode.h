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

#define GM(x) (1<<(G_M_##x))
enum
{
    G_M_FFA = 0, G_M_COOP, G_M_INSTA, G_M_MEDIEVAL, G_M_KABOOM, G_M_DUEL, G_M_SURVIVOR,
    G_M_CLASSIC, G_M_ONSLAUGHT, G_M_VAMPIRE, G_M_RESIZE, G_M_HARD, G_M_ARENA,
    G_M_GSP, G_M_GSP1 = G_M_GSP, G_M_GSP2, G_M_GSP3, G_M_NUM,
    G_M_GSN = G_M_NUM-G_M_GSP,
    G_M_ALL = GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
    G_M_FILTER = GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
    G_M_ROTATE = GM(FFA),
    G_M_DUKE = GM(DUEL)|GM(SURVIVOR),
};

#define GF(x) (1<<(G_F_##x))
enum { G_F_GSP = 0, G_F_NUM };

#define G_S_ENUM(pr, en) \
    en(pr, Waiting, WAITING) en(pr, Getting Map, GETMAP) en(pr, Sending Map, SENDMAP) en(pr, Readying, READYING) en(pr, Syncing Info, GAMEINFO) \
    en(pr, Playing, PLAYING) en(pr, Overtime, OVERTIME) en(pr, Intermission, INTERMISSION) en(pr, Voting, VOTING) en(pr, Maximum, MAX)
ENUMNV(G_S, G_S_ENUM)

#define gs_waiting(a) (a >= G_S_WAITING && a <= G_S_GAMEINFO)
#define gs_playing(a) (a >= G_S_PLAYING && a <= G_S_OVERTIME)
#define gs_intermission(a) (a >= G_S_INTERMISSION && a <= G_S_VOTING)
#define gs_timeupdate(a) (gs_playing(a) || gs_intermission(a))

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
        G_EDITMODE, 0, GM(FFA),
        {
            GM(FFA)|GM(CLASSIC)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA),
            0, 0, 0
        },
        "Editing", "Editing", { "", "", "" },
        "Create and edit existing maps", { "", "", "" },
    },
    {
        G_DEATHMATCH, 0, 0,
        {
            GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2),
            GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(GSP1),
            GM(FFA)|GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP2),
            0
        },
        "Deathmatch", "DM", { "Gladiator", "Old School", "" },
        "Shoot to kill and increase score by fragging", { "Fight in a confined area with increased pushback from damage", "Every frag only gives you a single point, like the old days", "" },
    },
    {
        G_CAPTURE, 0, 0,
        {
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1),
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP2),
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP3)
        },
        "Capture the Flag", "Capture", { "Quick", "Defend", "Protect" },
        "Take the enemy flag and return it to the base to score", { "Dropped flags instantly return to base", "Dropped flags must be defended until they reset", "Protect the flag and hold the enemy flag to score" },
    },
    {
        G_DEFEND, 0, 0,
        {
            GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2),
            GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2),
            GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2),
            0
        },
        "Defend and Control", "Defend", { "Quick", "King", "" },
        "Defend control points to score", { "Control points secure quicker than normal", "Remain king of the hill to score", ""},
    },
    {
        G_BOMBER, GF(GSP), 0,
        {
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
            GM(FFA)|GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1),
            GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP2)|GM(GSP3),
            GM(COOP)|GM(INSTA)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP2)|GM(GSP3)
        },
        "Bomber Ball", "Bomber", { "Hold", "Basket", "Assault" },
        "Carry the bomb into the enemy goal to score", { "Hold the bomb as long as possible to score", "Throw the bomb into the enemy goal to score", "Teams take turns attacking and defending" },
    },
    {
        G_RACE, GF(GSP), 0,
        {
            GM(FFA)|GM(ONSLAUGHT)|GM(GSP1)|GM(GSP2)|GM(GSP3),
            GM(FFA)|GM(ONSLAUGHT)|GM(GSP1)|GM(GSP2)|GM(GSP3),
            GM(FFA)|GM(ONSLAUGHT)|GM(GSP1)|GM(GSP2)|GM(GSP3),
            GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3)
        },
        "Race", "Race", { "Lapped", "Endurance", "Gauntlet" },
        "Compete for the fastest time completing a lap", { "Compete for the most number of laps", "Laps must be completed without dying at all", "Teams take turns running the gauntlet" },
    }
};
mutstypes mutstype[] = {
    {
        G_M_FFA, GM(FFA),
        GM(FFA)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "FFA", "Every player for themselves"
    },
    {
        G_M_COOP, GM(COOP),
        GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Coop", "Players versus drones"
    },
    {
        G_M_INSTA, GM(INSTA),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(HARD)|GM(RESIZE)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Instagib", "One hit kills instantly"
    },
    {
        G_M_MEDIEVAL, GM(MEDIEVAL),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(MEDIEVAL)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Medieval", "Players spawn only with swords"
    },
    {
        G_M_KABOOM,  GM(KABOOM),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(SURVIVOR)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Kaboom", "Players spawn with explosives only"
    },
    {
        G_M_DUEL, GM(DUEL),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(DUEL)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Duel", "One on one battles to determine the winner"
    },
    {
        G_M_SURVIVOR, GM(SURVIVOR),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(SURVIVOR)|GM(CLASSIC)|GM(MEDIEVAL)|GM(KABOOM)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Survivor", "Players battle to determine the winner"
    },
    {
        G_M_CLASSIC,    GM(CLASSIC),
        GM(FFA)|GM(COOP)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Classic", "Weapons must be collected from spawns in the arena"
    },
    {
        G_M_ONSLAUGHT, GM(ONSLAUGHT),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Onslaught", "Waves of enemies fill the battle arena"
    },
    {
        G_M_VAMPIRE, GM(VAMPIRE),
        GM(FFA)|GM(COOP)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Vampire", "Deal damage to regenerate health"
    },
    {
        G_M_RESIZE, GM(RESIZE),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Resize", "Players change size depending on their health"
    },
    {
        G_M_HARD, GM(HARD),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Hard", "Radar and health regeneration is disabled"
    },
    {
        G_M_ARENA, GM(ARENA),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Arena", "Players are able to carry all weapons at once"
    },
    {
        G_M_GSP1, GM(GSP1),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Game Specific 1", ""
    },
    {
        G_M_GSP2, GM(GSP2),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
        "Game Specific 2", ""
    },
    {
        G_M_GSP3, GM(GSP3),
        GM(FFA)|GM(COOP)|GM(INSTA)|GM(MEDIEVAL)|GM(KABOOM)|GM(DUEL)|GM(SURVIVOR)|GM(CLASSIC)|GM(ONSLAUGHT)|GM(VAMPIRE)|GM(RESIZE)|GM(HARD)|GM(ARENA)|GM(GSP1)|GM(GSP2)|GM(GSP3),
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

#define m_ffa(a,b)          ((b&GM(FFA)) || (gametype[a].implied&GM(FFA)))
#define m_coop(a,b)         ((b&GM(COOP)) || (gametype[a].implied&GM(COOP)))
#define m_insta(a,b)        ((b&GM(INSTA)) || (gametype[a].implied&GM(INSTA)))
#define m_medieval(a,b)     ((b&GM(MEDIEVAL)) || (gametype[a].implied&GM(MEDIEVAL)))
#define m_kaboom(a,b)       ((b&GM(KABOOM)) || (gametype[a].implied&GM(KABOOM)))
#define m_duel(a,b)         ((b&GM(DUEL)) || (gametype[a].implied&GM(DUEL)))
#define m_survivor(a,b)     ((b&GM(SURVIVOR)) || (gametype[a].implied&GM(SURVIVOR)))
#define m_classic(a,b)      ((b&GM(CLASSIC)) || (gametype[a].implied&GM(CLASSIC)))
#define m_onslaught(a,b)    ((b&GM(ONSLAUGHT)) || (gametype[a].implied&GM(ONSLAUGHT)))
#define m_vampire(a,b)      ((b&GM(VAMPIRE)) || (gametype[a].implied&GM(VAMPIRE)))
#define m_resize(a,b)       ((b&GM(RESIZE)) || (gametype[a].implied&GM(RESIZE)))
#define m_hard(a,b)         ((b&GM(HARD)) || (gametype[a].implied&GM(HARD)))
#define m_arena(a,b)        ((b&GM(ARENA)) || (gametype[a].implied&GM(ARENA)))

#define m_gsp1(a,b)         ((b&GM(GSP1)) || (gametype[a].implied&GM(GSP1)))
#define m_gsp2(a,b)         ((b&GM(GSP2)) || (gametype[a].implied&GM(GSP2)))
#define m_gsp3(a,b)         ((b&GM(GSP3)) || (gametype[a].implied&GM(GSP3)))
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

#define m_ra_lapped(a,b)    (m_race(a) && m_gsp1(a, b))
#define m_ra_timed(a,b)     (m_race(a) && !m_gsp1(a, b))
#define m_ra_endurance(a,b) (m_race(a) && m_gsp2(a, b))
#define m_ra_gauntlet(a,b)  (m_race(a) && m_gsp3(a, b))

#define m_team(a,b)         (!m_ffa(a, b))
#define m_single(a,b)       (m_dm_gladiator(a, b) || (m_race(a) && !m_ra_gauntlet(a, b)) || m_insta(a, b) || m_medieval(a, b)) // games that give you only a single weapon (for *extra values)
#define m_sweaps(a,b)       (m_dm_gladiator(a, b) || (m_race(a) && !m_ra_gauntlet(a, b)) || m_insta(a, b) || m_medieval(a, b) || m_kaboom(a, b)) // games that do not require selecing a spawn loadout but also don't have you pick one up
#define m_rotweaps(a,b)     (m_dm_gladiator(a, b) || (m_race(a) && !m_ra_gauntlet(a, b)) || m_medieval(a, b) || m_kaboom(a, b)) // games that require picking up rotation weapons
#define m_loadout(a,b)      (!m_classic(a, b) && !m_sweaps(a, b)) // games that require selecting a spawn loadout: non-classic, non-([non-gladiator race],gauntlet,insta,medieval,kaboom)
#define m_duke(a,b)         (m_duel(a, b) || m_survivor(a, b))
#define m_regen(a,b)        (!m_hard(a,b) && (!m_duke(a, b) || DSG(a, b, regen)) && !m_insta(a, b))
#define m_ghost(a,b)        (m_race(a) && !m_ra_gauntlet(a, b))
#define m_bots(a)           (m_play(a) && !m_race(a))
#define m_botbal(a,b)       (m_duel(a, b) ? G(botbalanceduel) : (m_survivor(a, b) ? G(botbalancesurvivor) : G(botbalance)))
#define m_nopoints(a,b)     (m_duke(a, b) || m_bb_hold(a, b) || m_race(a))
#define m_points(a,b)       (!m_nopoints(a, b))
#define m_normweaps(a,b)    (!m_race(a) && !m_insta(a,b) && !m_medieval(a,b) && !m_kaboom(a,b) && !m_dm_gladiator(a,b))
#define m_lasthit(a,b)      (m_dm_gladiator(a,b) && m_points(a,b))

#define m_weapon(at,a,b)    (m_medieval(a, b) ? A(at, weaponmedieval) : (m_kaboom(a, b) ? A(at, weaponkaboom) : (m_insta(a, b) ? A(at, weaponinsta) : (m_race(a) && !m_ra_gauntlet(a, b) ? A(at, weaponrace) : (m_dm_gladiator(a, b) ? A(at, weapongladiator) : A(at, weaponspawn))))))
#define m_maxcarry(at,a,b)  (at < A_ENEMY && m_arena(a, b) ? W_LOADOUT : A(at, maxcarry))
#define m_delay(at,a,b,c)   (!m_duke(a,b) ? int((m_edit(a) ? A(at, spawndelayedit) : (m_race(a) ? (!m_ra_gauntlet(a, b) || c == T_ALPHA ? A(at, spawndelayrace) : A(at, spawndelaygauntlet)) : (m_bomber(a) ? A(at, spawndelaybomber) : (m_defend(a) ? A(at, spawndelaydefend) : (m_capture(a) ? A(at, spawndelaycapture) : A(at, spawndelay))))))*(m_insta(a, b) ? A(at, spawndelayinstascale) : 1.f)) : 0)
#define m_protect(a,b)      (m_duke(a,b) ? DSG(a, b, protect) : (m_insta(a, b) ? G(instaprotect) : G(spawnprotect)))
#define m_teamspawn(a,b)    (m_team(a, b) && (!m_race(a) || m_ra_gauntlet(a, b)))
#define m_swapteam(a,b)     (m_play(a) && m_teamspawn(a, b) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= 3 && G(teambalanceswap))
#define m_balteam(a,b,c)    (m_play(a) && m_teamspawn(a, b) && (G(teambalanceduel) || !m_duel(a, b)) && !m_coop(gamemode, mutators) && G(teambalance) >= c)
#define m_forcebal(a,b)     (m_bb_assault(a, b) || m_ra_gauntlet(a, b))
#define m_balance(a,b,c)    (m_play(a) && m_teamspawn(a, b) && (m_forcebal(a, b) || ((G(balanceduke) || !m_duke(a, b)) && ((G(balancemaps) >= 0 ? G(balancemaps) : G(mapbalance)) >= (m_affinity(a) ? 1 : (c ? 2 : 3))))))
#define m_balreset(a,b)     (G(balancereset) && (G(balancereset) == 2 || m_capture(a) || m_bomber(a) || m_race(a) || m_duke(a, b)))

#ifdef CPP_GAME_SERVER
#define m_attr(a,b)         (a == WEAPON ? attrmap[isweap(b) ? b : W_GRENADE] : b)
#else
#define m_attr(a,b)         (a == WEAPON ? game::attrmap[isweap(b) ? b : W_GRENADE] : b)
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
                                            (m_race(a) ? \
                                                (m_ra_lapped(a, b) ? G(c##racelapped) : \
                                                    (m_ra_gauntlet(a, b) ? G(c##racegauntlet) : G(c##race)) \
                                                ) : 0 \
                                            ) \
                                        ) \
                                    ) \
                                ) \
                            )

#define MMVAR(f,l,a,b,c,w01,w02,w03,w04,w05,w06,w07,w08,w09,w10,w11,w12) \
    GVAR(f, l, a, b, w01, c); \
    GVAR(f, l, a##duel, b, w02, c); \
    GVAR(f, l, a##survivor, b, w03, c); \
    GVAR(f, l, a##gladiator, b, w04, c); \
    GVAR(f, l, a##capture, b, w05, c); \
    GVAR(f, l, a##defend, b, w06, c); \
    GVAR(f, l, a##defendking, b, w07, c); \
    GVAR(f, l, a##bomber, b, w08, c); \
    GVAR(f, l, a##bomberhold, b, w09, c); \
    GVAR(f, l, a##race, b, w10, c); \
    GVAR(f, l, a##racelapped, b, w11, c); \
    GVAR(f, l, a##racegauntlet, b, w12, c);

#define DSG(a,b,x)          (m_duel(a, b) ? G(duel##x) : G(survivor##x))

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
    else if(m_race(b)) a = newstring(G(racemaps)); \
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
    else if(m_race(b)) a = newstring(G(racemaps)); \
    else if(m_dm(b)) a = newstring(m_dm_gladiator(b, c) ? G(gladiatormaps) : G(mainmaps)); \
    else a = newstring(G(allowmaps)); \
}

#ifdef CPP_GAME_SERVER
SVARR(modename, "Demo Editing Deathmatch [Capture the Flag] [Defend and Control] [Bomber Ball] [Race]");
SVARR(modeidxname, "demo editing deathmatch capture defend bomber race");
VARR(modeidxdemo, G_DEMO);
VARR(modeidxediting, G_EDITMODE);
VARR(modeidxdeathmatch, G_DEATHMATCH);
VARR(modeidxcapture, G_CAPTURE);
VARR(modeidxdefend, G_DEFEND);
VARR(modeidxbomber, G_BOMBER);
VARR(modeidxrace, G_RACE);
VARR(modeidxstart, G_START);
VARR(modeidxplay, G_PLAY);
VARR(modeidxrand, G_RAND);
VARR(modeidxnever, G_NEVER);
VARR(modeidxlimit, G_LIMIT);
VARR(modeidxnum, G_MAX);
VARR(modebitdemo, (1<<G_DEMO));
VARR(modebitediting, (1<<G_EDITMODE));
VARR(modebitdeathmatch, (1<<G_DEATHMATCH));
VARR(modebitcapture, (1<<G_CAPTURE));
VARR(modebitdefend, (1<<G_DEFEND));
VARR(modebitbomber, (1<<G_BOMBER));
VARR(modebitrace, (1<<G_RACE));
VARR(modebitall, G_ALL);
SVARR(mutsname, "FFA Coop Instagib Medieval Kaboom Duel Survivor Classic Onslaught Vampire Resize Hard Arena");
SVARR(mutsidxname, "ffa coop instagib medieval kaboom duel survivor classic onslaught vampire resize hard arena");
VARR(mutsidxffa, G_M_FFA);
VARR(mutsidxcoop, G_M_COOP);
VARR(mutsidxinstagib, G_M_INSTA);
VARR(mutsidxmedieval, G_M_MEDIEVAL);
VARR(mutsidxkaboom, G_M_KABOOM);
VARR(mutsidxduel, G_M_DUEL);
VARR(mutsidxsurvivor, G_M_SURVIVOR);
VARR(mutsidxclassic, G_M_CLASSIC);
VARR(mutsidxonslaught, G_M_ONSLAUGHT);
VARR(mutsidxvampire, G_M_VAMPIRE);
VARR(mutsidxresize, G_M_RESIZE);
VARR(mutsidxhard, G_M_HARD);
VARR(mutsidxarena, G_M_ARENA);
VARR(mutsidxgsp1, G_M_GSP1);
VARR(mutsidxgsp2, G_M_GSP2);
VARR(mutsidxgsp3, G_M_GSP3);
VARR(mutsidxgsn, G_M_GSN);
VARR(mutsidxgsp, G_M_GSP);
VARR(mutsidxnum, G_M_NUM);
VARR(mutsidxnormal, G_M_NUM-G_M_GSN);
VARR(mutsbitffa, GM(FFA));
VARR(mutsbitcoop, GM(COOP));
VARR(mutsbitinstagib, GM(INSTA));
VARR(mutsbitmedieval, GM(MEDIEVAL));
VARR(mutsbitkaboom, GM(KABOOM));
VARR(mutsbitduel, GM(DUEL));
VARR(mutsbitsurvivor, GM(SURVIVOR));
VARR(mutsbitclassic, GM(CLASSIC));
VARR(mutsbitonslaught, GM(ONSLAUGHT));
VARR(mutsbitvampire, GM(VAMPIRE));
VARR(mutsbitresize, GM(RESIZE));
VARR(mutsbithard, GM(HARD));
VARR(mutsbitarena, GM(ARENA));
VARR(mutsbitgsp1, GM(GSP1));
VARR(mutsbitgsp2, GM(GSP2));
VARR(mutsbitgsp3, GM(GSP3));
VARR(mutsbitall, G_M_ALL);
#endif
