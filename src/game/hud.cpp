#include "game.h"
namespace hud
{
    int hudwidth = 0, hudheight = 0, laststats = 0;

    VAR(IDF_PERSIST, showhud, 0, 1, 1);
    VAR(IDF_PERSIST, hudsize, 0, 2048, VAR_MAX);

    VAR(IDF_PERSIST, showdemoplayback, 0, 1, 1);
    FVAR(IDF_PERSIST, edgesize, 0, 0.005f, 1000);

    const int NUMSTATS = 42;
    int prevstats[NUMSTATS] = {0}, curstats[NUMSTATS] = {0};
    VAR(IDF_PERSIST, statrate, 1, 100, 1000);

    void enginestatrefresh()
    {
        if(totalmillis-laststats >= statrate)
        {
            memcpy(prevstats, curstats, sizeof(prevstats));
            laststats = totalmillis-(totalmillis%statrate);
        }
        int nextstats[NUMSTATS] = {
            wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024,
            allocnodes*8, allocva, glde, gbatches, getnumqueries(),
            curfps, bestfpsdiff, worstfpsdiff, entities::ents.length(), entgroup.length(), ai::waypoints.length(), getnumviewcells(),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()/8.f),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()*0.45f),
            int(camera1->o.x), int(camera1->o.y), int(camera1->o.z), int(camera1->yaw), int(camera1->pitch), int(camera1->roll),
            sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.cx, sel.cxs, sel.cy, sel.cys,
            selchildcount, selchildmat[0], sel.corner, sel.orient, sel.grid
        };
        loopi(NUMSTATS) if(prevstats[i] == curstats[i]) curstats[i] = nextstats[i];
    }
    ICOMMAND(0, refreshenginestats, "", (), enginestatrefresh());
    ICOMMAND(0, getenginestat, "ii", (int *n, int *prev), intret(*n >= 0 && *n < NUMSTATS ? (*prev!=0 ? prevstats[*n] : curstats[*n]) : -1));
    static const char *enginestats[NUMSTATS] = {
        "wtr", "wtr%", "wvt", "wvt%", "evt", "eva", "ond", "va", "gl" "gb", "oq",
        "fps", "best", "worst", "ents", "entsel", "wp", "pvs", "vel", "mps", "kmh",
        "posx", "posy", "posz", "yaw", "pitch", "roll",
        "selox", "seloy", "seloz", "selsx", "selsy", "selsz", "selcx", "selcxs", "selcy", "selcys",
        "cube", "mat", "corner", "orient", "grid"
    };
    ICOMMAND(0, getenginestatname, "i", (int *n), result(*n >= 0 && *n < NUMSTATS ? enginestats[*n]: ""));
    #define LOOPENGSTATS(name,op) \
        ICOMMAND(0, loopenginestat##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            loopstart(id, stack); \
            op(NUMSTATS, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPENGSTATS(,loopcsi)
    LOOPENGSTATS(rev,loopcsirev)

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamneutraltex, "<grey>textures/icons/teamneutral", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamalphatex, "<grey>textures/icons/teamalpha", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamomegatex, "<grey>textures/icons/teamomega", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamenemytex, "<grey>textures/icons/teamenemy", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamenvtex, "<grey>textures/icons/teamenemy", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playertex, "<grey>textures/icons/player", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playermaletex, "<grey>textures/icons/playermale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playerfemaletex, "<grey>textures/icons/playerfemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, outlinemaletex, "<grey>textures/icons/outlinemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, outlinefemaletex, "<grey>textures/icons/outlinefemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, deadtex, "<grey>textures/icons/dead", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatingtex, "<grey>textures/icons/dominating", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatedtex, "<grey>textures/icons/dominated", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inputtex, "<grey><rotate:1>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, menutex, "<grey>textures/icons/menu", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, waitingtex, "<grey>textures/icons/waiting", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spectatortex, "<grey>textures/icons/spectator", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, cameratex, "<grey>textures/icons/camera", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, chattex, "<grey>textures/icons/chat", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editingtex, "<grey>textures/icons/editing", 3);

    TVAR(IDF_PERSIST|IDF_PRELOAD, progresstex, "<grey>textures/hud/progress", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, progringtex, "<grey>textures/hud/progring", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, warningtex, "<grey>textures/icons/warning", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, questiontex, "<grey>textures/icons/question", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtex, "<grey>textures/icons/flag", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pointtex, "<grey>textures/icons/point", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtex, "<grey>textures/icons/bomb", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowtex, "<grey>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowrighttex, "<grey><rotate:1>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowdowntex, "<grey><rotate:2>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowlefttex, "<grey><rotate:3>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, alerttex, "<grey>textures/icons/alert", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagdroptex, "<grey>textures/icons/flagdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtakentex, "<grey>textures/icons/flagtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombdroptex, "<grey>textures/icons/bombdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtakentex, "<grey>textures/icons/bombtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, attacktex, "<grey>textures/icons/attack", 3);

    VAR(0, hidecrosshair, 0, 0, 1); // temporarily hides crosshair, needs to be set each frame you want it hidden
    FVAR(IDF_PERSIST, cursorsize, 0, 0.03f, 1000); // also keeps tooltips clear of the cursor
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomtex, "textures/hud/zoom", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawtex, "<grey>textures/weapons/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistoltex, "<grey>textures/weapons/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordtex, "<grey>textures/weapons/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguntex, "<grey>textures/weapons/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgtex, "<grey>textures/weapons/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamertex, "<grey>textures/weapons/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmatex, "<grey>textures/weapons/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappertex, "<grey>textures/weapons/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rifletex, "<grey>textures/weapons/rifle", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, corrodertex, "<grey>textures/weapons/corroder", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, tridenttex, "<grey>textures/weapons/trident", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadetex, "<grey>textures/weapons/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minetex, "<grey>textures/weapons/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rockettex, "<grey>textures/weapons/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, miniguntex, "<grey>textures/weapons/minigun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, jetsawtex, "<grey>textures/weapons/jetsaw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, eclipsetex, "<grey>textures/weapons/eclipse", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, meleetex, "<grey>textures/weapons/melee", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree1tex, "textures/rewards/carnage", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree2tex, "textures/rewards/slaughter", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree3tex, "textures/rewards/massacre", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree4tex, "textures/rewards/bloodbath", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi1tex, "textures/rewards/double", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi2tex, "textures/rewards/triple", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi3tex, "textures/rewards/multi", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, headshottex, "textures/rewards/headshot", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatetex, "textures/rewards/dominate", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, revengetex, "textures/rewards/revenge", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, firstbloodtex, "textures/rewards/firstblood", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, breakertex, "textures/rewards/breaker", 3);

    TVAR(IDF_PERSIST, privnonetex, "<grey>textures/privs/none", 3);
    TVAR(IDF_PERSIST, privbottex, "<grey>textures/privs/bot", 3);
    TVAR(IDF_PERSIST, privplayertex, "<grey>textures/privs/player", 3);
    TVAR(IDF_PERSIST, privsupportertex, "<grey>textures/privs/supporter", 3);
    TVAR(IDF_PERSIST, privmoderatortex, "<grey>textures/privs/moderator", 3);
    TVAR(IDF_PERSIST, privadministratortex, "<grey>textures/privs/administrator", 3);
    TVAR(IDF_PERSIST, privdevelopertex, "<grey>textures/privs/developer", 3);
    TVAR(IDF_PERSIST, privfoundertex, "<grey>textures/privs/founder", 3);
    TVAR(IDF_PERSIST, privlocalsupportertex, "<grey>textures/privs/localsupporter", 3);
    TVAR(IDF_PERSIST, privlocalmoderatortex, "<grey>textures/privs/localmoderator", 3);
    TVAR(IDF_PERSIST, privlocaladministratortex, "<grey>textures/privs/localadministrator", 3);

    TVAR(IDF_PERSIST, modedemotex, "<grey>textures/modes/demo", 3);
    TVAR(IDF_PERSIST, modeeditingtex, "<grey>textures/modes/editing", 3);
    TVAR(IDF_PERSIST, modedeathmatchtex, "<grey>textures/modes/deathmatch", 3);
    TVAR(IDF_PERSIST, modegladiatortex, "<grey>textures/modes/gladiator", 3);
    TVAR(IDF_PERSIST, modeoldschooltex, "<grey>textures/modes/oldschool", 3);

    TVAR(IDF_PERSIST, modecapturetex, "<grey>textures/modes/capture", 3);
    TVAR(IDF_PERSIST, modecapturequicktex, "<grey>textures/modes/capturequick", 3);
    TVAR(IDF_PERSIST, modecapturedefendtex, "<grey>textures/modes/capturedefend", 3);
    TVAR(IDF_PERSIST, modecaptureprotecttex, "<grey>textures/modes/captureprotect", 3);

    TVAR(IDF_PERSIST, modedefendtex, "<grey>textures/modes/defend", 3);
    TVAR(IDF_PERSIST, modedefendquicktex, "<grey>textures/modes/defendquick", 3);
    TVAR(IDF_PERSIST, modedefendkingtex, "<grey>textures/modes/defendking", 3);

    TVAR(IDF_PERSIST, modebombertex, "<grey>textures/modes/bomber", 3);
    TVAR(IDF_PERSIST, modebomberholdtex, "<grey>textures/modes/bomberhold", 3);
    TVAR(IDF_PERSIST, modebomberbaskettex, "<grey>textures/modes/bomberbasket", 3);
    TVAR(IDF_PERSIST, modebomberassaulttex, "<grey>textures/modes/bomberassault", 3);

    TVAR(IDF_PERSIST, modespeedruntex, "<grey>textures/modes/speedrun", 3);
    TVAR(IDF_PERSIST, modespeedrunlappedtex, "<grey>textures/modes/speedrunlapped", 3);
    TVAR(IDF_PERSIST, modespeedrunendurancetex, "<grey>textures/modes/speedrunendurance", 3);
    TVAR(IDF_PERSIST, modespeedrungauntlettex, "<grey>textures/modes/speedrungauntlet", 3);

    TVAR(IDF_PERSIST, modeffatex, "<grey>textures/modes/ffa", 3);
    TVAR(IDF_PERSIST, modecooptex, "<grey>textures/modes/coop", 3);
    TVAR(IDF_PERSIST, modeinstatex, "<grey>textures/modes/instagib", 3);
    TVAR(IDF_PERSIST, modemedievaltex, "<grey>textures/modes/medieval", 3);
    TVAR(IDF_PERSIST, modekaboomtex, "<grey>textures/modes/kaboom", 3);
    TVAR(IDF_PERSIST, modedueltex, "<grey>textures/modes/duel", 3);
    TVAR(IDF_PERSIST, modesurvivortex, "<grey>textures/modes/survivor", 3);
    TVAR(IDF_PERSIST, modeclassictex, "<grey>textures/modes/classic", 3);
    TVAR(IDF_PERSIST, modeonslaughttex, "<grey>textures/modes/onslaught", 3);
    TVAR(IDF_PERSIST, modevampiretex, "<grey>textures/modes/vampire", 3);
    TVAR(IDF_PERSIST, moderesizetex, "<grey>textures/modes/resize", 3);
    TVAR(IDF_PERSIST, modehardtex, "<grey>textures/modes/hard", 3);
    TVAR(IDF_PERSIST, modearenatex, "<grey>textures/modes/arena", 3);
    TVAR(IDF_PERSIST, modedarktex, "<grey>textures/modes/dark", 3);

    #define ADDMODEICON(g,m) \
    { \
        if(m_demo(g)) ADDMODE(demo) \
        else if(m_edit(g)) ADDMODE(editing) \
        else if(m_capture(g)) \
        { \
            if(m_ctf_quick(g, m)) ADDMODE(capturequick) \
            else if(m_ctf_defend(g, m)) ADDMODE(capturedefend) \
            else if(m_ctf_protect(g, m)) ADDMODE(captureprotect) \
            else ADDMODE(capture) \
        } \
        else if(m_defend(g)) \
        { \
            if(m_dac_king(g, m)) \
            { \
                ADDMODE(defendking) \
                if(m_dac_quick(g, m)) ADDMODE(defendquick) \
            } \
            else if(m_dac_quick(g, m)) ADDMODE(defendquick) \
            else ADDMODE(defend) \
        } \
        else if(m_bomber(g)) \
        { \
            if(m_bb_hold(g, m)) ADDMODE(bomberhold) \
            else if(m_bb_assault(g, m)) \
            { \
                ADDMODE(bomberassault) \
                if(m_bb_basket(g, m)) ADDMODE(bomberbasket) \
            } \
            else if(m_bb_basket(g, m)) ADDMODE(bomberbasket) \
            else ADDMODE(bomber) \
        } \
        else if(m_speedrun(g)) \
        { \
            if(m_sr_gauntlet(g, m)) \
            { \
                ADDMODE(speedrungauntlet) \
                if(m_sr_lapped(g, m)) ADDMODE(speedrunlapped) \
                if(m_sr_endurance(g, m)) ADDMODE(speedrunendurance) \
            } \
            else if(m_sr_lapped(g, m)) \
            { \
                ADDMODE(speedrunlapped) \
                if(m_sr_endurance(g, m)) ADDMODE(speedrunendurance) \
            } \
            else if(m_sr_endurance(g, m)) ADDMODE(speedrunendurance) \
            else ADDMODE(speedrun) \
        } \
        else \
        { \
            if(m_duel(g, m)) ADDMODE(duel) \
            else if(m_survivor(g, m)) ADDMODE(survivor) \
            else if(m_dm_gladiator(g, m)) ADDMODE(gladiator) \
            else if(m_dm_oldschool(g, m)) ADDMODE(oldschool) \
            else ADDMODE(deathmatch) \
        } \
    }

    #define ADDMODE(s) { if(list.length()) list.add(' '); list.put(mode##s##tex, strlen(mode##s##tex)); }
    void modetex(int g, int m, vector<char> &list)
    {
        modecheck(g, m);
        ADDMODEICON(g, m);
    }

    void modetexs(int g, int m, bool before, bool implied, vector<char> &list)
    {
        modecheck(g, m);
        if(before) modetex(g, m, list);
        if(m_ffa(g, m) && (implied || !(gametype[g].implied&(1<<G_M_FFA)))) ADDMODE(ffa)
        if(m_coop(g, m) && (implied || !(gametype[g].implied&(1<<G_M_COOP)))) ADDMODE(coop)
        if(m_insta(g, m) && (implied || !(gametype[g].implied&(1<<G_M_INSTAGIB)))) ADDMODE(insta)
        if(m_medieval(g, m) && (implied || !(gametype[g].implied&(1<<G_M_MEDIEVAL)))) ADDMODE(medieval)
        if(m_kaboom(g, m) && (implied || !(gametype[g].implied&(1<<G_M_KABOOM)))) ADDMODE(kaboom)
        if(!m_dm(g))
        {
            if(m_duel(g, m) && (implied || !(gametype[g].implied&(1<<G_M_DUEL)))) ADDMODE(duel)
            if(m_survivor(g, m) && (implied || !(gametype[g].implied&(1<<G_M_SURVIVOR)))) ADDMODE(survivor)
        }
        else if(m_duel(g, m) || m_survivor(g, m))
        {
            if(m_dm_gladiator(g, m) && (implied || !(gametype[g].implied&(1<<G_M_GSP1)))) ADDMODE(gladiator)
            if(m_dm_oldschool(g, m) && (implied || !(gametype[g].implied&(1<<G_M_GSP2)))) ADDMODE(oldschool)
        }
        if(m_classic(g, m) && (implied || !(gametype[g].implied&(1<<G_M_CLASSIC)))) ADDMODE(classic)
        if(m_onslaught(g, m) && (implied || !(gametype[g].implied&(1<<G_M_ONSLAUGHT)))) ADDMODE(onslaught)
        if(m_vampire(g, m) && (implied || !(gametype[g].implied&(1<<G_M_VAMPIRE)))) ADDMODE(vampire)
        if(m_resize(g, m) && (implied || !(gametype[g].implied&(1<<G_M_RESIZE)))) ADDMODE(resize)
        if(m_hard(g, m) && (implied || !(gametype[g].implied&(1<<G_M_HARD)))) ADDMODE(hard)
        if(m_arena(g, m) && (implied || !(gametype[g].implied&(1<<G_M_ARENA)))) ADDMODE(arena)
        if(m_dark(g, m) && (implied || !(gametype[g].implied&(1<<G_M_DARK)))) ADDMODE(dark)
        if(!before) modetex(g, m, list);
    }
    #undef ADDMODE

    ICOMMAND(0, modetex, "bb", (int *g, int *m),
    {
        vector<char> list;
        modetex(*g >= 0 ? *g : game::gamemode, *m >= 0 ? *m : game::mutators, list);
        list.add('\0');
        result(list.getbuf());
    });

    ICOMMAND(0, modetexlist, "bbbi", (int *g, int *m, int *b, int *p),
    {
        vector<char> list;
        if(*b >= 0) modetexs(*g >= 0 ? *g : game::gamemode, *m >= 0 ? *m : game::mutators, *b!=0, *p!=0, list);
        else modetex(*g >= 0 ? *g : game::gamemode, *m >= 0 ? *m : game::mutators, list);
        list.add('\0');
        result(list.getbuf());
    });

    const char *modeimage()
    {
        if(!connected()) return "menu";
        #define ADDMODE(s) return #s;
        ADDMODEICON(game::gamemode, game::mutators);
        #undef ADDMODE
    }

    bool needminimap() { return true; }

    int hasinput(bool pass, bool cursor)
    {
        if(cdpi::getoverlay() > 0 || consolemillis > 0) return 1;

        int cur = UI::hasinput(cursor);
        if(!cur && UI::hasmenu(pass)) cur = 1;

        return cur;
    }
    ICOMMANDV(0, hasinput, hasinput())
    ICOMMAND(0, getinput, "ii", (int *pass, int *cursor), intret(hasinput(*pass != 0, *cursor != 0)));

    bool textinput(const char *str, int len)
    {
        return false;
    }

    bool keypress(int code, bool isdown)
    {
        return false;
    }

    DEFUIVARS(player, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);
    DEFUIVARS(playeroverlay, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 4096.f, 0.f, 0.f);

    void checkui()
    {
        hidecrosshair = game::emotecamera() ? 1 : 0; // the orbiting camera isn't looking where the player aims

        UI::pokeui("pointer", SURFACE_FOREGROUND);
        loopi(SURFACE_LOOP) UI::pokeui("hud", i);
        UI::pokeui("menubg", SURFACE_BACKGROUND);
        UI::pokeui("menubglyrics", SURFACE_FOREGROUND); // in front of the menu scene's fog

        if(!UI::hasmenu(true))
        {
            if(connected())
            {
                UI::pressui("scoreboard", scoreson);
                if(game::player1->state == CS_DEAD) { if(scoreson) shownscores = true; }
                else shownscores = false;
            }
            else UI::openui("main");
        }

        if(game::maptime <= 0 || gs_waiting(game::gamestate)) return; // wait until map started

        if(playerui >= 0 || playeroverlayui >= 0)
        {
            gameent *d = NULL;
            int numdyns = game::numdynents();
            loopi(numdyns) if((d = (gameent *)game::iterdynents(i)) && (d->actortype < A_ENEMY || d->ishighlight()) && d != game::focus && !d->isspectator())
            {   // players that can't be seen are always on the world surface, so it is checked here
                if(playerui >= 0) MAKEUI(player, d->clientnum, (game::focus->isspectator() || (m_team(game::gamemode, game::mutators) && d->team == game::focus->team) || d->ishighlight(game::focus)), d->abovehead());
                MAKEUI(playeroverlay, d->clientnum, true, d->center());
            }
        }

        entities::checkui();
        if(m_capture(game::gamemode)) capture::checkui();
        if(m_defend(game::gamemode)) defend::checkui();
        if(m_bomber(game::gamemode)) bomber::checkui();
    }

    void drawquad(float x, float y, float w, float h, float tx1, float ty1, float tx2, float ty2, bool flipx, bool flipy)
    {
        if(flipx) swap(tx1, tx2);
        if(flipy) swap(ty1, ty2);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y); gle::attribf(tx1, ty1);
        gle::attribf(x+w, y); gle::attribf(tx2, ty1);
        gle::attribf(x, y+h); gle::attribf(tx1, ty2);
        gle::attribf(x+w, y+h); gle::attribf(tx2, ty2);
        gle::end();
    }
    void drawcoord(float x, float y, float w, float h, float tx, float ty, float tw, float th, bool flipx, bool flipy) { drawquad(x, y, w, h, tx, ty, tx+tw, ty+th, flipx, flipy); }
    void drawtexture(float x, float y, float w, float h, bool flipx, bool flipy) { drawquad(x, y, w, h, 0, 0, 1, 1, flipx, flipy); }
    void drawsized(float x, float y, float s, bool flipx, bool flipy) { drawquad(x, y, s, s, 0, 0, 1, 1, flipx, flipy); }

    void drawblend(int x, int y, int w, int h, float v)
    {
        gle::colorf(0, 0, 0, v);

        gle::defvertex(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,     y);
        gle::attribf(x + w, y);
        gle::attribf(x,     y + h);
        gle::attribf(x + w, y + h);
        gle::end();
    }

    #define RADARLIMIT (m_edit(game::gamemode) && game::player1->isediting() ? 0.f : radardistlimit)

    float radarlimit(float dist) { return min(dist >= 0 && RADARLIMIT > 0 ? clamp(dist, 0.f, RADARLIMIT) : max(dist, 0.f), float(worldsize)); }
    ICOMMAND(0, getradarlimit, "f", (float *n), floatret(radarlimit(*n)));

    float radardepth(const vec &o, float dist) { return 1 - (o.dist(camera1->o) / radarlimit(dist)); }
    ICOMMAND(0, getradardepth, "ffff", (float *x, float *y, float *z, float *n), floatret(radardepth(vec(*x, *y, *z), *n)));

    bool radarlimited(float dist) { return RADARLIMIT > 0 && dist > RADARLIMIT; }
    ICOMMAND(0, getradarlimited, "f", (float *n), intret(radarlimited(*n) ? 1 : 0));

    const char *teamtexname(int team)
    {
        const char *teamtexs[T_MAX] = { teamneutraltex, teamalphatex, teamomegatex, teamenemytex, teamenvtex };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *privtex(int priv, int actortype)
    {
        if(actortype != A_PLAYER) return privbottex;
        const char *privtexs[2][PRIV_MAX] = {
            { privnonetex, privplayertex, privsupportertex, privmoderatortex, privadministratortex, privdevelopertex, privfoundertex },
            { privnonetex, privplayertex, privlocalsupportertex, privlocalmoderatortex, privlocaladministratortex, privnonetex, privnonetex }
        };
        return privtexs[priv&PRIV_LOCAL ? 1 : 0][clamp(priv&PRIV_TYPE, 0, priv&PRIV_LOCAL ? int(PRIV_ADMINISTRATOR) : int(PRIV_LAST))];
    }

    const char *itemtex(int type, int stype)
    {
        switch(type)
        {
            case PLAYERSTART: return playertex; break;
            case AFFINITY: return flagtex; break;
            case WEAPON:
            {
                const char *weaptexs[W_MAX] = {
                    clawtex, pistoltex, swordtex, shotguntex, smgtex, flamertex, plasmatex, zappertex, rifletex, corrodertex, tridenttex,
                    grenadetex, minetex, rockettex, miniguntex, jetsawtex, eclipsetex, meleetex
                };
                return isweap(stype) && *weaptexs[stype] ? weaptexs[stype] : questiontex;
                break;
            }
            default: break;
        }
        return "";
    }

    void drawzoom(int w, int h)
    {
        if(!gs_playing(game::gamestate) || game::focus->state != CS_ALIVE || !game::inzoom()) return;

        float pc = game::zoomscale();
        int x = 0, y = 0, c = 0;
        if(w > h)
        {
            c = h;
            x += (w - h) / 2;
            usetexturing(false);
            drawblend(0, 0, x, c, pc);
            drawblend(x + c, 0, x + 1, c, pc);
            usetexturing(true);
        }
        else if(h > w)
        {
            c = w;
            y += (h - w) / 2;
            usetexturing(false);
            drawblend(0, 0, c, y, pc);
            drawblend(0, y + c, c, y, pc);
            usetexturing(true);
        }
        else c = h;

        Texture *t = textureload(zoomtex, 3, true, false);
        if(!t || t == notexture) return;
        settexture(t);
        gle::colorf(0, 0, 0, pc);
        drawtexture(x, y, c, c);
    }

    FVAR(IDF_PERSIST, visorsway, 0, 1, 1); // strength of the visor spring, 1 is the comfort cap
    FVAR(IDF_PERSIST, visorkick, 0, 0.018f, 0.05f); // fraction of the view height a full Kick moves the visor
    FVAR(IDF_PERSIST, visorswaymax, 0, 0.015f, 0.05f); // fraction of the view height turning and Kicks together can reach

    FVAR(IDF_PERSIST, visorspringhz, 1, 5, 20);
    FVAR(IDF_PERSIST, visorspringdamp, 0.1f, 0.7f, 1);
    FVAR(IDF_PERSIST, visorspringmax, 0, 0.012f, 0.05f); // fraction of the view height turning can reach
    FVAR(IDF_PERSIST, visorspringrate, 1, 360, FVAR_MAX); // turn rate in degrees per second that reaches visorspringmax

    #define VISORSPRINGSTEPS 100 // longest frame simulated, in one millisecond steps

    // the visor is a damped mass on the head, lagging behind turns and settling back
    struct visorspring
    {
        vec2 pos, vel;
        float lastyaw, lastpitch;
        gameent *last;

        visorspring() { reset(); }

        void reset(gameent *d = NULL)
        {
            pos = vel = vec2(0);
            last = d;
            lastyaw = d ? d->yaw : 0;
            lastpitch = d ? d->pitch : 0;
        }

        void update(gameent *d)
        {
            if(d != last) reset(d);
            if(curtime <= 0) return;

            float yaw = d->yaw - lastyaw, pitch = d->pitch - lastpitch;
            if(yaw > 180.0f) yaw -= 360.0f;
            else if(yaw < -180.0f) yaw += 360.0f;
            lastyaw = d->yaw;
            lastpitch = d->pitch;

            // the world moves against a turn, the visor follows it part of the way
            vec2 rate = vec2(-yaw, pitch).mul(1000.0f / curtime);
            rate.clamp(-visorspringrate, visorspringrate);

            float w = 2 * M_PI * visorspringhz, z = visorspringdamp, h = 0.001f;
            vec2 drive = vec2(rate).mul(visorspringmax * w * w / visorspringrate);
            loopi(min(curtime, VISORSPRINGSTEPS))
            {
                vel.add(vec2(drive).sub(vec2(pos).mul(w * w)).sub(vec2(vel).mul(2 * z * w)).mul(h));
                pos.add(vec2(vel).mul(h));
            }
            pos.clamp(-visorspringmax, visorspringmax);
        }
    } visorspringstate;

    VAR(IDF_PERSIST, visorfxdelay, 0, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, visorfxdamage, 0, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxcritical, 0, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxoverhealth, 0, 0.5f, FVAR_MAX);

    FVAR(IDF_PERSIST, visorfxrun, 0, 0.125f, 1);
    FVAR(IDF_PERSIST, visorfxsprint, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, visorfximpulse, 0, 0.375f, 1);

    FVAR(IDF_PERSIST, visorfxglitch, 0, 1, 1);
    FVAR(IDF_PERSIST, visorfxdesaturate, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, visorfxsaturate, 0, 0.75f, 4);
    FVAR(IDF_PERSIST, visorfxblur, 0, 1, 1);

    FVAR(IDF_PERSIST, visorfxnarrow, 0, 1, 2);
    FVAR(IDF_PERSIST, visorfxnarrowspectv, 0, 0.75f, 2);

    void visorinfo(VisorSurface::Config &config, bool noview)
    {
        bool inactive = noview || gs_waiting(game::gamestate);

        if(!inactive && visorsway > 0 && game::focus->isalive() && !game::thirdpersonview(true))
        {
            visorspringstate.update(game::focus);
            vec2 offset = vec2(game::kickoffset()).mul(visorkick).add(visorspringstate.pos).clamp(-visorswaymax, visorswaymax).mul(visorsway);
            config.offsetx = offset.x;
            config.offsety = offset.y;
        }
        else visorspringstate.reset();

        if(inactive || !game::focus->isactive())
        {
            if(inactive)
            {
                config.narrow = game::tvmode(false) ? visorfxnarrowspectv : 1.0f;

                float reveal = game::mapreveal();
                config.narrow *= reveal;
                config.bluramt = 1.0f - reveal;
                
            }
            else if(game::tvmode(false)) config.narrow = visorfxnarrowspectv;
            return;
        }

        config.narrow = game::tvmode() ? visorfxnarrowspectv : visorfxnarrow;

        if(game::focus->isalive())
        {
            float amt = 1.0f;
            if(game::focus->impulseeffect()) amt -= visorfximpulse;
            else if(game::focus->sprinting()) amt -= visorfxsprint;
            else if(game::focus->running()) amt -= visorfxrun;
            
            float protectamt = game::protectfade(game::focus);
            protectamt = protectamt > 0.5f ? 1.0f : protectamt * 2.0f;

            config.narrow *= min(amt, protectamt);
            protectamt = 1.0f - protectamt;
            amt = 1.0f - amt;
            config.bluramt = visorfxblur * amt;

            if(protectamt > 0.0f)
            {
                config.saturate = visorfxsaturate * protectamt;
                config.bluramt = max(config.bluramt, visorfxblur * protectamt * 0.25f);
            }
            else
            {
                float damageamt = game::damagescale(game::focus, visorfxdelay) * visorfxdamage,
                      criticalamt = game::criticalscale(game::focus) * visorfxcritical;

                if(damageamt > 0.0f) config.saturate = -visorfxdesaturate * damageamt;
                
                if(criticalamt > 0.0f)
                {
                    config.saturate = -visorfxdesaturate * criticalamt;
                    config.glitch = visorfxglitch * criticalamt;
                }
                else if(visorfxoverhealth > 0.0f)
                {
                    int spawnhp = game::focus->gethealth(game::gamemode, game::mutators);
                    if(game::focus->health > spawnhp)
                    {
                        int maxhp = game::focus->gethealth(game::gamemode, game::mutators, true);
                        if(maxhp > spawnhp)
                        {
                            float hpamt = clamp((game::focus->health - spawnhp) / float(maxhp - spawnhp), 0.f, 1.f) * visorfxoverhealth;
                            config.saturate = visorfxsaturate * hpamt;
                        }
                    }
                }
            }
        }
        else if(game::focus->isdead())
        {
            float spawnamt = game::spawnfade(game::focus);
            config.narrow *= spawnamt;
            config.bluramt = 1.0f - spawnamt;
            config.saturate = -visorfxdesaturate;
        }
    }

    void startrender(int w, int h, bool wantvisor, bool noview)
    {
        if(noview || !engineready) return;

        hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        drawzoom(hudwidth, hudheight);
    }

    void visorrender(int w, int h, bool wantvisor, bool noview)
    {
    }

    void endrender(int w, int h, bool wantvisor, bool noview)
    {
    }

    void update(int w, int h)
    {
        vieww = w;
        viewh = h;

        aspect = forceaspect ? forceaspect : w / float(h);
        fovy = 2.0f * atan2(tan(curfov * 0.5f * RAD), aspect) / RAD;

        float reveal = game::mapreveal();
        if(reveal > 0.0f && reveal < 1.0f)
        {
            vec pos;
            float yaw, pitch, fovfx, fovfy;
            entities::getcamera(pos, yaw, pitch, fovfx);

            if(aspect > 1.0f)
                fovfy = 2.0f * atan2(tan(fovfx * 0.5f * RAD), aspect) / RAD;
            else if(aspect < 1.0f)
            {
                yaw = fovfx;
                fovfy = fovfx;
                fovfx = 2.0f * atan2(tan(yaw * 0.5f * RAD), 1.0f / aspect) / RAD;
            }
            else fovfy = fovfx;

            curfov = fovfx + (curfov - fovfx) * reveal;
            fovy = fovfy + (fovy - fovfy) * reveal;
        }

        if(aspect > 1)
        {
            hudheight = hudsize;
            hudwidth = int(ceil(hudsize*aspect));
        }
        else if(aspect < 1)
        {
            hudwidth = hudsize;
            hudheight = int(ceil(hudsize/aspect));
        }
        else hudwidth = hudheight = hudsize;

        checkui();
    }
}
