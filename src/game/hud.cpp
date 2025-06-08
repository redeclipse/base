#include "game.h"
namespace hud
{
    int hudwidth = 0, hudheight = 0, laststats = 0;

    VAR(IDF_PERSIST, showhud, 0, 1, 1);
    VAR(IDF_PERSIST, hudsize, 0, 2048, VAR_MAX);

    VAR(IDF_PERSIST, showdemoplayback, 0, 1, 1);
    FVAR(IDF_PERSIST, edgesize, 0, 0.005f, 1000);
    VAR(IDF_PERSIST, mapstartfadein, 0, 3000, VAR_MAX);

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

    VAR(IDF_PERSIST|IDF_HEX, crosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, hitcrosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, clipstone, -CTONE_MAX, 0, 0xFFFFFF);

    VAR(IDF_PERSIST, showindicator, 0, 4, 4);
    FVAR(IDF_PERSIST, indicatorsize, 0, 0.025f, 1000);
    FVAR(IDF_PERSIST, indicatorblend, 0, 1, 1);
    VAR(IDF_PERSIST, indicatorminattack, 0, 1000, VAR_MAX);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, indicatortex, "<grey>textures/hud/indicator", 3);

    VAR(0, hidecrosshair, 0, 0, 1); // temporarily hides crosshair, needs to be set each frame you want it hidden
    VAR(IDF_PERSIST, showcrosshair, 0, 2, 2); // 0 = off, 1 = on, 2 = blend depending on current accuracy level
    VAR(IDF_PERSIST, crosshairdistance, 0, 0, 1); // 0 = off, 1 = shows distance to crosshair target
    FVAR(IDF_PERSIST, crosshairdistblend, 0, 1, 1);
    VAR(IDF_PERSIST, crosshairdistancex, VAR_MIN, 160, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairdistancey, VAR_MIN, 80, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairweapons, 0, 1, 3); // 0 = off, &1 = crosshair-specific weapons, &2 = also appy colour
    FVAR(IDF_PERSIST, crosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, crosshairscale, 0, 1.0f, 4.0f);
    VAR(IDF_PERSIST, crosshairhitspeed, 0, 250, VAR_MAX);
    FVAR(IDF_PERSIST, crosshairblend, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, crosshairaccamt, 0, 0, 1);
    VAR(IDF_PERSIST, crosshairflash, 0, 1, 1);
    FVAR(IDF_PERSIST, crosshairthrob, 0, 0, 1000);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursortex, "textures/hud/cursor", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursorhovertex, "textures/hud/cursorhover", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, crosshairtex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hithairtex, "crosshairs/cross-01-hit", 3);
    FVAR(IDF_PERSIST, clawcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, clawcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, clawcrosshairtex, "crosshairs/triangle-02", 3);
    TVAR(IDF_PERSIST, clawhithairtex, "crosshairs/triangle-02-hit", 3);
    FVAR(IDF_PERSIST, pistolcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, pistolcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, pistolcrosshairtex, "crosshairs/simple-04", 3);
    TVAR(IDF_PERSIST, pistolhithairtex, "crosshairs/simple-04-hit", 3);
    FVAR(IDF_PERSIST, swordcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, swordcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, swordcrosshairtex, "crosshairs/simple-02", 3);
    TVAR(IDF_PERSIST, swordhithairtex, "crosshairs/simple-02-hit", 3);
    FVAR(IDF_PERSIST, shotguncrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, shotguncrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, shotguncrosshairtex, "crosshairs/cross-02", 3);
    TVAR(IDF_PERSIST, shotgunhithairtex, "crosshairs/cross-02-hit", 3);
    FVAR(IDF_PERSIST, smgcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, smgcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, smgcrosshairtex, "crosshairs/simple-03", 3);
    TVAR(IDF_PERSIST, smghithairtex, "crosshairs/simple-03-hit", 3);
    FVAR(IDF_PERSIST, plasmacrosshairsize, 0, 0.0625f, 1000);
    FVAR(IDF_PERSIST, plasmacrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, plasmacrosshairtex, "crosshairs/circle-05", 3);
    TVAR(IDF_PERSIST, plasmahithairtex, "crosshairs/circle-05-hit", 3);
    FVAR(IDF_PERSIST, zappercrosshairsize, 0, 0.0625f, 1000);
    FVAR(IDF_PERSIST, zappercrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, zappercrosshairtex, "crosshairs/circle-03", 3);
    TVAR(IDF_PERSIST, zapperhithairtex, "crosshairs/circle-03-hit", 3);
    FVAR(IDF_PERSIST, flamercrosshairsize, 0, 0.0625f, 1000);
    FVAR(IDF_PERSIST, flamercrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, flamercrosshairtex, "crosshairs/circle-06", 3);
    TVAR(IDF_PERSIST, flamerhithairtex, "crosshairs/circle-06-hit", 3);
    FVAR(IDF_PERSIST, riflecrosshairsize, 0, 0.075f, 1000);
    FVAR(IDF_PERSIST, riflecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, riflecrosshairtex, "crosshairs/simple-01", 3);
    TVAR(IDF_PERSIST, riflehithairtex, "crosshairs/simple-01-hit", 3);
    FVAR(IDF_PERSIST, corrodercrosshairsize, 0, 0.075f, 1000);
    FVAR(IDF_PERSIST, corrodercrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, corrodercrosshairtex, "crosshairs/circle-01", 3);
    TVAR(IDF_PERSIST, corroderhithairtex, "crosshairs/circle-01-hit", 3);
    FVAR(IDF_PERSIST, grenadecrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, grenadecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, grenadecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, grenadehithairtex, "crosshairs/circle-02-hit", 3);
    FVAR(IDF_PERSIST, minecrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, minecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, minecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, minehithairtex, "crosshairs/circle-02-hit", 3);
    FVAR(IDF_PERSIST, rocketcrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, rocketcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, rocketcrosshairtex, "crosshairs/circle-01", 3);
    TVAR(IDF_PERSIST, rockethithairtex, "crosshairs/circle-01-hit", 3);
    FVAR(IDF_PERSIST, miniguncrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, miniguncrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, miniguncrosshairtex, "crosshairs/simple-03", 3);
    TVAR(IDF_PERSIST, minigunhithairtex, "crosshairs/simple-03-hit", 3);
    FVAR(IDF_PERSIST, jetsawcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, jetsawcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, jetsawcrosshairtex, "crosshairs/simple-02", 3);
    TVAR(IDF_PERSIST, jetsawhithairtex, "crosshairs/simple-02-hit", 3);
    FVAR(IDF_PERSIST, eclipsecrosshairsize, 0, 0.0625f, 1000);
    FVAR(IDF_PERSIST, eclipsecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, eclipsecrosshairtex, "crosshairs/circle-05", 3);
    TVAR(IDF_PERSIST, eclipsehithairtex, "crosshairs/circle-05-hit", 3);
    FVAR(IDF_PERSIST, meleecrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, meleecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, meleecrosshairtex, "crosshairs/triangle-02", 3);
    TVAR(IDF_PERSIST, meleehithairtex, "crosshairs/triangle-02-hit", 3);

    FVAR(IDF_PERSIST, editcursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, editcursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editcursortex, "crosshairs/cross-01", 3);
    FVAR(IDF_PERSIST, speccursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, speccursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, speccursortex, "crosshairs/cross-01", 3);
    FVAR(IDF_PERSIST, tvcursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, tvcursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, tvcursortex, "", 3);
    FVAR(IDF_PERSIST, teamcrosshairsize, 0, 0.06f, 1000);
    FVAR(IDF_PERSIST, teamcrosshairblend, 0, 0.5f, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamcrosshairtex, "<grey>textures/icons/warning", 3);
    VAR(IDF_PERSIST, teamcrosshaircolour, 0, 0xFF0000, 0xFFFFFF);

    VAR(IDF_PERSIST, cursorstyle, 0, 0, 1); // 0 = top left tracking, 1 = center
    FVAR(IDF_PERSIST, cursorsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, cursorblend, 0, 1, 1);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomtex, "textures/hud/zoom", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomcrosshairtex, "crosshairs/simple-01", 3);
    FVAR(IDF_PERSIST, zoomcrosshairsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, zoomcrosshairblend, 0, 1, 1);

    VAR(IDF_PERSIST, showcirclebar, 0, 0, 1);
    VAR(IDF_PERSIST, circlebartype, 0, 7, 7); // 0 = off, &1 = health, &2 = impulse, &4 = ammo
    FVAR(IDF_PERSIST, circlebarsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, circlebarblend, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarhealthtone, -CTONE_MAX, 0x88FF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarimpulsetone, -CTONE_MAX, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammocolour, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammotone, -CTONE_MAX-1, 0xFFAA66, 0xFFFFFF);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, circlebartex, "<grey>textures/hud/circlebar", 3);

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
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadetex, "<grey>textures/weapons/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minetex, "<grey>textures/weapons/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rockettex, "<grey>textures/weapons/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, miniguntex, "<grey>textures/weapons/minigun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, jetsawtex, "<grey>textures/weapons/jetsaw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, eclipsetex, "<grey>textures/weapons/eclipse", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, meleetex, "<grey>textures/weapons/melee", 3);

    VAR(IDF_PERSIST, showclips, 0, 1, 1);
    VAR(IDF_PERSIST, clipanims, 0, 2, 2);
    FVAR(IDF_PERSIST, clipsize, 0, 0.025f, 1000);
    FVAR(IDF_PERSIST, clipoffset, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, clipminscale, 0, 0.25f, 1000);
    FVAR(IDF_PERSIST, clipmaxscale, 0, 1, 1000);
    FVAR(IDF_PERSIST, clipblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, clipcolour, 0, 1, 1);
    VAR(IDF_PERSIST, cliplength, 0, 0, VAR_MAX);
    VAR(IDF_PERSIST, clipstore, 0, 1, 1);
    FVAR(IDF_PERSIST, clipstoreblend, 0, 0.2f, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawcliptex, "<grey>textures/weapons/clips/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistolcliptex, "<grey>textures/weapons/clips/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordcliptex, "<grey>textures/weapons/clips/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguncliptex, "<grey>textures/weapons/clips/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgcliptex, "<grey>textures/weapons/clips/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamercliptex, "<grey>textures/weapons/clips/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmacliptex, "<grey>textures/weapons/clips/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappercliptex, "<grey>textures/weapons/clips/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, riflecliptex, "<grey>textures/weapons/clips/rifle", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, corrodercliptex, "<grey>textures/weapons/clips/corroder", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadecliptex, "<grey>textures/weapons/clips/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minecliptex, "<grey>textures/weapons/clips/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rocketcliptex, "<grey>textures/weapons/clips/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, miniguncliptex, "<grey>textures/weapons/clips/minigun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, jetsawcliptex, "<grey>textures/weapons/clips/jetsaw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, eclipsecliptex, "<grey>textures/weapons/clips/eclipse", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, meleecliptex, "<grey>textures/weapons/clips/melee", 3);

    FVAR(IDF_PERSIST, clawclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, pistolclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, swordclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, shotgunclipoffset, 0, 0.125f, 0.5f);
    FVAR(IDF_PERSIST, smgclipoffset, 0, 0.35f, 0.5f);
    FVAR(IDF_PERSIST, flamerclipoffset, 0, 0.5f, 0.5f);
    FVAR(IDF_PERSIST, plasmaclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, zapperclipoffset, 0, 0.3f, 0.5f);
    FVAR(IDF_PERSIST, rifleclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, corroderclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, grenadeclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, mineclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, rocketclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, minigunclipoffset, 0, 0.35f, 0.5f);
    FVAR(IDF_PERSIST, jetsawclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, eclipseclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, meleeclipoffset, 0, 0.25f, 0.5f);

    FVAR(IDF_PERSIST, clawclipskew, 0, 0.75f, 10);
    FVAR(IDF_PERSIST, pistolclipskew, 0, 0.65f, 10);
    FVAR(IDF_PERSIST, swordclipskew, 0, 1, 10);
    FVAR(IDF_PERSIST, shotgunclipskew, 0, 0.7f, 10);
    FVAR(IDF_PERSIST, smgclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, flamerclipskew, 0, 0.4f, 10);
    FVAR(IDF_PERSIST, plasmaclipskew, 0, 0.5f, 10);
    FVAR(IDF_PERSIST, zapperclipskew, 0, 0.5f, 10);
    FVAR(IDF_PERSIST, rifleclipskew, 0, 0.9f, 10);
    FVAR(IDF_PERSIST, corroderclipskew, 0, 0.7f, 10);
    FVAR(IDF_PERSIST, grenadeclipskew, 0, 1.f, 10);
    FVAR(IDF_PERSIST, mineclipskew, 0, 1.f, 10);
    FVAR(IDF_PERSIST, rocketclipskew, 0, 1.0f, 10);
    FVAR(IDF_PERSIST, minigunclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, jetsawclipskew, 0, 1, 10);
    FVAR(IDF_PERSIST, eclipseclipskew, 0, 0.5f, 10);
    FVAR(IDF_PERSIST, meleeclipskew, 0, 0.75f, 10);

    VAR(IDF_PERSIST, clawcliprotate, 0, 12, 15); // "round-the-clock" rotation of texture, 0 = off, &1 = flip x, &2 = flip y, &4 = angle, &8 = spin
    VAR(IDF_PERSIST, pistolcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, swordcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, shotguncliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, smgcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, flamercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, plasmacliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, zappercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, riflecliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, corrodercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, grenadecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, minecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, rocketcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, miniguncliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, jetsawcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, eclipsecliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, meleecliprotate, 0, 12, 15);

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
            if(m_ra_gauntlet(g, m)) \
            { \
                ADDMODE(speedrungauntlet) \
                if(m_ra_lapped(g, m)) ADDMODE(speedrunlapped) \
                if(m_ra_endurance(g, m)) ADDMODE(speedrunendurance) \
            } \
            else if(m_ra_lapped(g, m)) \
            { \
                ADDMODE(speedrunlapped) \
                if(m_ra_endurance(g, m)) ADDMODE(speedrunendurance) \
            } \
            else if(m_ra_endurance(g, m)) ADDMODE(speedrunendurance) \
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
        hidecrosshair = 0;

        loopi(SURFACE_LOOP) UI::pokeui("hud", i);

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

        if(game::maptime <= 0) return; // wait until map started

        if(playerui >= 0)
        {
            gameent *d = NULL;
            int numdyns = game::numdynents();
            loopi(numdyns) if((d = (gameent *)game::iterdynents(i)) && (d->actortype < A_ENEMY || d->isprize(game::focus)) && d != game::focus && !d->isspectator())
            {
                MAKEUI(player, d->clientnum, (game::focus->isspectator() || (m_team(game::gamemode, game::mutators) && d->team == game::focus->team) || d->isprize(game::focus)), d->abovehead());
                MAKEUI(playeroverlay, d->clientnum, true, d->center());
            }
        }

        entities::checkui();
        if(m_capture(game::gamemode)) capture::checkui();
        if(m_defend(game::gamemode)) defend::checkui();
        if(m_bomber(game::gamemode)) bomber::checkui();
    }

    void removeplayer(gameent *d)
    {
        if(!d) return;
        CLEARUI(player, d->clientnum, -1); // close all
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

    void colourskew(float &r, float &g, float &b, float skew)
    {
        if(skew >= 2.f)
        { // fully overcharged to green
            r = b = 0;
        }
        else if(skew >= 1.f)
        { // overcharge to yellow
            b *= 1.f-(skew-1.f);
        }
        else if(skew >= 0.5f)
        { // fade to orange
            float off = skew-0.5f;
            g *= 0.5f+off;
            b *= off*2.f;
        }
        else
        { // fade to red
            g *= skew;
            b = 0;
        }
    }

    template<class T>
    void skewcolour(T &r, T &g, T &b, int colour = 0, bool faded = false)
    {
        if(colour < 0) colour = game::getcolour(game::focus);
        vec c = vec::fromcolor(colour);
        r = T(r*c.r);
        g = T(g*c.g);
        b = T(b*c.b);
        if(!game::focus->team && faded)
        {
            float f = game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING ? 0.25f : 0.5f;
            r = T(r*f);
            g = T(g*f);
            b = T(b*f);
        }
    }

    enum
    {
        POINTER_NONE = 0, POINTER_UI, POINTER_EDIT, POINTER_SPEC,
        POINTER_HAIR, POINTER_TEAM, POINTER_ZOOM, POINTER_HIT, POINTER_MAX
    };

    const char *getpointer(int index, int weap = -1)
    {
        switch(index)
        {
            case POINTER_UI:
            {
                switch(UI::cursortype())
                {
                    case CURSOR_HIDDEN: return NULL; break;
                    case CURSOR_HOVER: return cursorhovertex; break;
                    case CURSOR_DEFAULT: default: break;
                }
                return cursortex;
            }
            case POINTER_EDIT: return editcursortex;
            case POINTER_SPEC: return game::tvmode() ? tvcursortex : speccursortex;
            case POINTER_HAIR:
            {
                if(crosshairweapons&1 && isweap(weap))
                {
                    const char *crosshairtexs[W_MAX] = {
                        clawcrosshairtex, pistolcrosshairtex, swordcrosshairtex, shotguncrosshairtex, smgcrosshairtex,
                        flamercrosshairtex, plasmacrosshairtex, zappercrosshairtex, riflecrosshairtex, corrodercrosshairtex, grenadecrosshairtex, minecrosshairtex,
                        rocketcrosshairtex, miniguncrosshairtex, jetsawcrosshairtex, eclipsecrosshairtex, meleecrosshairtex // end of regular weapons
                    };
                    if(*crosshairtexs[weap]) return crosshairtexs[weap];
                }
                return crosshairtex;
            }
            case POINTER_TEAM: return teamcrosshairtex;
            case POINTER_ZOOM: return zoomcrosshairtex;
            case POINTER_HIT:
            {
                if(crosshairweapons&1 && isweap(weap))
                {
                    const char *hithairtexs[W_MAX] = {
                        clawhithairtex, pistolhithairtex, swordhithairtex, shotgunhithairtex, smghithairtex,
                        flamerhithairtex, plasmahithairtex, zapperhithairtex, riflehithairtex, corroderhithairtex, grenadehithairtex, minehithairtex,
                        rockethithairtex, minigunhithairtex, jetsawhithairtex, eclipsehithairtex, meleehithairtex // end of regular weapons
                    };
                    if(*hithairtexs[weap]) return hithairtexs[weap];
                }
                return hithairtex;
            }
            default: break;
        }
        return NULL;
    }
    ICOMMAND(0, getpointer, "ii", (int *i, int *j), result(getpointer(*i, *j)));

    void drawindicator(int weap, int x, int y, float s, bool secondary, float blend)
    {
        float fade = indicatorblend * blend;
        if(fade <= 0) return;

        int millis = lastmillis-game::focus->weaptime[weap];
        if(!game::focus->weapwait[weap] || millis > game::focus->weapwait[weap]) return;

        float r = 1, g = 1, b = 1, amt = 0;
        switch(game::focus->weapstate[weap])
        {
            case W_S_POWER: case W_S_ZOOM:
            {
                amt = clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, amt);
                break;
            }
            case W_S_RELOAD:
            {
                if(showindicator < (W(weap, ammoadd) < W(weap, ammoclip) ? 3 : 2)) return;
                amt = 1.f-clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, 1.f-amt);
                break;
            }
            case W_S_PRIMARY: case W_S_SECONDARY:
            {
                if(showindicator < 4 || game::focus->weapwait[weap] < indicatorminattack) return;
                amt = 1.f-clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, 1.f-amt);
                break;
            }
            default: return;
        }

        Texture *t = textureload(indicatortex, 3, true, false);
        settexture(t);

        float val = amt < 0.25f ? amt : (amt > 0.75f ? 1.f-amt : 0.25f);
        gle::colorf(val*4.f, val*4.f, val*4.f, fade*val);
        drawsized(x-s, y-s, s*2);
        gle::colorf(r, g, b, fade);
        drawslice(0, clamp(amt, 0.f, 1.f), x, y, s);
    }

    void drawclipitem(const char *tex, float x, float y, float offset, float size, float blend, float angle, float spin, int rotate, const vec &colour)
    {
        Texture *t = textureload(tex, 3, true, false);
        if(!t || t == notexture) return;
        while(angle < 0.0f) angle += 360.0f;
        while(angle >= 360.0f) angle -= 360.0f;
        bool flipx = false, flipy = false;
        float rot = 0;
        if(rotate&8) rot += spin;
        if(rotate&4) rot += angle;
        if(rotate&2) flipy = angle > 90.f && angle <= 180.f;
        if(rotate&1) flipx = angle >= 180.f;
        while(rot < 0.0f) rot += 360.0f;
        while(rot >= 360.0f) rot -= 360.0f;
        vec2 loc(x+offset*sinf(RAD*angle), y+offset*-cosf(RAD*angle));
        gle::color(colour, blend);
        settexture(t);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        loopk(4)
        {
            vec2 norm, tc;
            switch(k)
            {
                case 0: vecfromyaw(rot, 1, -1, norm);   tc = vec2(0, 1); break;
                case 1: vecfromyaw(rot, 1, 1, norm);    tc = vec2(1, 1); break;
                case 2: vecfromyaw(rot, -1, -1, norm);  tc = vec2(0, 0); break;
                case 3: vecfromyaw(rot, -1, 1, norm);   tc = vec2(1, 0); break;
            }
            norm.mul(size*0.5f).add(loc);
            gle::attrib(norm);
            if(flipx) tc.x = 1 - tc.x;
            if(flipy) tc.y = 1 - tc.y;
            gle::attrib(tc);
        }
        gle::end();
    }

    void drawclip(int weap, int x, int y, float s, bool preview, float blend)
    {
        if(!isweap(weap) || weap >= W_MAX || (!W2(weap, ammosub, false) && !W2(weap, ammosub, true))) return;

        float orig = clipblend * blend, fade = orig;
        if(fade <= 0) return;

        const char *cliptexs[W_MAX] = {
            clawcliptex, pistolcliptex, swordcliptex, shotguncliptex, smgcliptex,
            flamercliptex, plasmacliptex, zappercliptex, riflecliptex, corrodercliptex, grenadecliptex, minecliptex,
            rocketcliptex, miniguncliptex, jetsawcliptex, eclipsecliptex, meleecliptex
        };
        const float clipoffs[W_MAX] = {
            clawclipoffset, pistolclipoffset, swordclipoffset, shotgunclipoffset, smgclipoffset,
            flamerclipoffset, plasmaclipoffset, zapperclipoffset, rifleclipoffset, corroderclipoffset, grenadeclipoffset, mineclipoffset,
            rocketclipoffset, minigunclipoffset, jetsawclipoffset, eclipseclipoffset, meleeclipoffset
        };
        const float clipskew[W_MAX] = {
            clawclipskew, pistolclipskew, swordclipskew, shotgunclipskew, smgclipskew,
            flamerclipskew, plasmaclipskew, zapperclipskew, rifleclipskew, corroderclipskew, grenadeclipskew, mineclipskew,
            rocketclipskew, minigunclipskew, jetsawclipskew, eclipseclipskew, meleeclipskew
        };
        const int cliprots[W_MAX] = {
            clawcliprotate, pistolcliprotate, swordcliprotate, shotguncliprotate, smgcliprotate,
            flamercliprotate, plasmacliprotate, zappercliprotate, riflecliprotate, corrodercliprotate, grenadecliprotate, minecliprotate,
            rocketcliprotate, miniguncliprotate, jetsawcliprotate, eclipsecliprotate, meleecliprotate
        };

        int maxammo = W(weap, ammoclip), ammo = preview ? maxammo : game::focus->weapammo[weap][W_A_CLIP],
            store = game::focus->actortype >= A_ENEMY || W(weap, ammostore) < 0 ? maxammo : game::focus->weapammo[weap][W_A_STORE], interval = lastmillis-game::focus->weaptime[weap];
        float skew = clipskew[weap]*clipsize, size = s*skew, offset = s*clipoffset,
              slice = 360/float(maxammo), angle = (maxammo > (cliprots[weap]&4 ? 4 : 3) || maxammo%2 ? 360.f : 360.f-slice*0.5f)-((maxammo-ammo)*slice),
              area = 1-clamp(clipoffs[weap]*2, 1e-3f, 1.f), need = s*skew*area*maxammo, have = 2*M_PI*s*clipoffset,
              scale = clamp(have/need, clipminscale, clipmaxscale), start = angle, amt = 0, spin = 0;
        vec c(1, 1, 1);

        if(clipstone) skewcolour(c.r, c.g, c.b, clipstone);
        if(clipcolour) skewcolour(c.r, c.g, c.b, W(weap, colour));

        if(!preview && interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
        {
            case W_S_PRIMARY: case W_S_SECONDARY:
            {
                if(!W2(weap, ammosub, game::focus->weapstate[weap] == W_S_SECONDARY)) break;
                amt = 1.f-clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                fade *= amt;
                if(clipanims)
                {
                    size *= amt;
                    offset *= amt;
                    if(clipanims >= 2) spin = 360*amt;
                }
                int shot = game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1;
                float rewind = angle;
                loopi(shot) drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, rewind += slice, spin, cliprots[weap], c);
                fade = orig;
                size = s*skew;
                offset = s*clipoffset;
                spin = 0;
                break;
            }
            case W_S_RELOAD: case W_S_USE:
            {
                if(game::focus->weapload[weap][W_A_CLIP] > 0)
                {
                    int check = game::focus->weapwait[weap]/2;
                    float ss = slice*game::focus->weapload[weap][W_A_CLIP];
                    if(interval >= check)
                    {
                        amt = clamp((interval-check)/float(check), 0.f, 1.f);
                        fade *= amt;
                        if(clipanims)
                        {
                            size *= amt*3/4;
                            offset *= amt*3/4;
                            if(clipanims >= 2) spin = 360*amt;
                        }
                        loopi(game::focus->weapload[weap][W_A_CLIP])
                        {
                            drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
                            angle -= slice;
                        }
                    }
                    else angle -= ss;
                    start -= ss;
                    store += game::focus->weapload[weap][W_A_CLIP];
                    ammo -= game::focus->weapload[weap][W_A_CLIP];
                    fade = orig;
                    size = s*skew;
                    offset = s*clipoffset;
                    spin = 0;
                    break;
                }
                if(game::focus->weapstate[weap] == W_S_USE && game::focus->getlastweap(m_weapon(game::focus->actortype, game::gamemode, game::mutators)) == weap)
                {
                    amt = clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                    if(clipanims)
                    {
                        angle -= 360*amt;
                        start -= 360*amt;
                        if(clipanims >= 2) spin = 360*amt;
                    }
                    break;
                }
                // falls through
            }
            case W_S_SWITCH:
            {
                amt = clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                fade *= amt;
                if(clipanims && game::focus->weapstate[weap] != W_S_RELOAD)
                {
                    size *= amt;
                    offset *= amt;
                    if(clipanims >= 2) spin = 360*amt;
                }
                else spin = 0;
                break;
            }
            default: break;
        }
        loopi(ammo)
        {
            drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
            angle -= slice;
        }
        if(clipstore && ammo < maxammo && store > 0)
        {
            int total = clamp(store, 0, maxammo-ammo);
            loopi(total) drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade*clipstoreblend, start += slice, spin, cliprots[weap], c);
        }
    }

    void drawcirclebar(int x, int y, float s, float blend)
    {
        if(game::focus->state != CS_ALIVE) return;

        float orig = circlebarblend, fade = orig;
        if(fade <= 0) return;

        int num = 0;
        loopi(3) if(circlebartype&(1<<i))
        {
            if(i == 1 && !impulsecostmeter) continue;
            num++;
        }
        if(!num) return;

        Texture *t = circlebartex && *circlebartex ? textureload(circlebartex, 3, true, false) : NULL;
        if(!t || t == notexture) return;
        float slice = 1.f/num, pos = num%2 ? slice*0.5f : 0.f;
        settexture(t);
        loopi(3) if(circlebartype&(1<<i))
        {
            float val = 0;
            vec c(1, 1, 1);
            switch(i)
            {
                case 0:
                    val = min(1.f, game::focus->health/float(max(game::focus->gethealth(game::gamemode, game::mutators), 1)));
                    if(circlebarhealthtone) skewcolour(c.r, c.g, c.b, circlebarhealthtone);
                    break;
                case 1:
                    if(!impulsecostmeter) continue;
                    val = 1-clamp(float(game::focus->impulse[IM_METER])/float(impulsecostmeter), 0.f, 1.f);
                    if(circlebarimpulsetone) skewcolour(c.r, c.g, c.b, circlebarimpulsetone);
                    break;
                case 2:
                {
                    if(!isweap(game::focus->weapselect)) continue;
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaptime[weap];
                    val = game::focus->weapammo[weap][W_A_CLIP]/float(W(weap, ammoclip));
                    if(circlebarammotone || circlebarammocolour) skewcolour(c.r, c.g, c.b, circlebarammocolour ? W(weap, colour) : circlebarammotone);
                    if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
                    {
                        case W_S_RELOAD: case W_S_USE:
                            if(game::focus->weapload[weap][W_A_CLIP] > 0)
                            {
                                val -= game::focus->weapload[weap][W_A_CLIP]/float(W(weap, ammoclip));
                                break;
                            }
                            else if(game::focus->weapstate[weap] == W_S_RELOAD) break;
                        case W_S_SWITCH:
                        {
                            float amt = clamp(float(interval)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                            fade *= amt;
                            val *= amt;
                            break;
                        }
                        default: break;
                    }
                    break;
                }
            }
            gle::color(vec(c).mul(0.25f), circlebarblend * 0.65f);
            drawslice(pos, slice, x, y, s*circlebarsize);
            if(val > 0)
            {
                gle::color(c, fade);
                drawslice(pos, val*slice, x, y, s*circlebarsize);
            }
            float nps = pos+val*slice;
            val = 0;
            fade = orig;
            switch(i)
            {
                case 2:
                {
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaptime[weap];
                    if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
                    {
                        case W_S_PRIMARY: case W_S_SECONDARY:
                        {
                            float amt = 1.f-clamp(float(interval)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                            fade *= amt;
                            val = (game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1)/float(W(weap, ammoclip))*amt;
                            break;
                        }
                        case W_S_RELOAD: case W_S_USE:
                        {
                            if(game::focus->weapload[weap][W_A_CLIP] > 0)
                            {
                                int check = game::focus->weapwait[weap]/2;
                                if(interval >= check)
                                {
                                    float amt = clamp(float(interval-check)/float(check), 0.f, 1.f);
                                    fade *= amt;
                                    val = game::focus->weapload[weap][W_A_CLIP]/float(W(weap, ammoclip))*amt;
                                }
                            }
                            break;
                        }
                        default: break;
                    }
                    break;
                }
                case 0:
                {
                    /*
                    float total = 0;
                    loopv(damagelocs)
                    {
                        dhloc &l = damagelocs[i];
                        gameent *e = game::getclient(l.clientnum);
                        if(!e || l.dir.iszero())
                        {
                            damagelocs.remove(i--);
                            continue;
                        }
                        int millis = totalmillis-l.outtime, delay = min(20, l.damage)*50;
                        if(millis >= delay)
                        {
                            if(millis >= onscreendamagetime+onscreendamagefade) damagelocs.remove(i--);
                            continue;
                        }
                        if(!onscreendamageself && e == game::focus) continue;
                        float dam = l.damage/float(max(game::focus->gethealth(game::gamemode, game::mutators), 1)),
                              amt = millis/float(delay);
                        total += dam;
                        val += dam*(1-amt);
                    }
                    if(total > 0)
                    {
                        float amt = val/total;
                        fade *= amt;
                        flashcolour(c.r, c.g, c.b, 0.3f, 0.6f, 0.1f, amt);
                    }
                    else val = 0;
                    */
                }
                default: case 1: break;
            }
            if(val > 0)
            {
                gle::color(c, fade);
                drawslice(nps, val*slice, x, y, s*circlebarsize);
            }
            pos += slice;
        }
    }

    #define RADARLIMIT (m_edit(game::gamemode) && game::player1->isediting() ? 0.f : radardistlimit)

    float radarlimit(float dist) { return min(dist >= 0 && RADARLIMIT > 0 ? clamp(dist, 0.f, RADARLIMIT) : max(dist, 0.f), float(worldsize)); }
    ICOMMAND(0, getradarlimit, "f", (float *n), floatret(radarlimit(*n)));

    float radardepth(const vec &o, float dist, float tolerance, float addz) { return 1 - ((vec(o).addz(addz).dist(camera1->o) + tolerance) / radarlimit(dist)); }
    ICOMMAND(0, getradardepth, "ffffff", (float *x, float *y, float *z, float *n, float *t, float *a), floatret(radardepth(vec(*x, *y, *z), *n, *t, *a)));

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
                    clawtex, pistoltex, swordtex, shotguntex, smgtex, flamertex, plasmatex, zappertex, rifletex, corrodertex, grenadetex, minetex, rockettex, miniguntex, jetsawtex, eclipsetex, meleetex
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

    bool drawpointertex(const char *tex, int x, int y, int s, float r, float g, float b, float fade)
    {
        if(fade <= 0 || !tex || !*tex) return false;
        Texture *t = textureload(tex, 3, true, false);
        if(!t || t == notexture) return false;
        gle::colorf(r, g, b, fade);
        settexture(t);
        drawsized(x, y, s);
        return true;
    }

    void drawpointer(int w, int h, int s, int index, float x, float y, float blend)
    {
        float csize = crosshairsize * crosshairscale, fade = crosshairblend;
        switch(index)
        {
            case POINTER_EDIT: csize = editcursorsize; fade = editcursorblend; break;
            case POINTER_SPEC: csize = speccursorsize; fade = speccursorblend; break;
            case POINTER_TEAM: csize = teamcrosshairsize; fade = teamcrosshairblend; break;
            case POINTER_ZOOM:
                if(game::inzoom())
                {
                    csize = zoomcrosshairsize * crosshairscale;
                    fade = zoomcrosshairblend;
                    break;
                } // fall through
            case POINTER_HIT: case POINTER_HAIR:
            {
                if(crosshairweapons && isweap(game::focus->weapselect))
                {
                    const float crosshairsizes[W_MAX] = {
                        clawcrosshairsize, pistolcrosshairsize, swordcrosshairsize, shotguncrosshairsize, smgcrosshairsize,
                        flamercrosshairsize, plasmacrosshairsize, zappercrosshairsize, riflecrosshairsize, corrodercrosshairsize, grenadecrosshairsize, minecrosshairsize,
                        rocketcrosshairsize, miniguncrosshairsize, jetsawcrosshairsize, eclipsecrosshairsize, meleecrosshairsize
                    }, crosshairblends[W_MAX] = {
                        clawcrosshairblend, pistolcrosshairblend, swordcrosshairblend, shotguncrosshairblend, smgcrosshairblend,
                        flamercrosshairblend, plasmacrosshairblend, zappercrosshairblend, riflecrosshairblend, corrodercrosshairblend, grenadecrosshairblend, minecrosshairblend,
                        rocketcrosshairblend, miniguncrosshairblend, jetsawcrosshairblend, eclipsecrosshairblend, meleecrosshairblend
                    };
                    csize = crosshairsizes[game::focus->weapselect] * crosshairscale;
                    fade = crosshairblends[game::focus->weapselect];
                }
                break;
            }
            default: csize = cursorsize; fade = cursorblend; break;
        }
        fade *= blend;

        vec c(1, 1, 1);
        int cs = int(csize*s);
        if(game::focus->state == CS_ALIVE && index >= POINTER_HAIR)
        {
            if(index == POINTER_TEAM) c = vec::fromcolor(teamcrosshaircolour);
            else if(crosshairweapons&2) c = vec::fromcolor(W(game::focus->weapselect, colour));
            else if(crosshairtone) skewcolour(c.r, c.g, c.b, crosshairtone);

            int heal = game::focus->gethealth(game::gamemode, game::mutators);
            if(crosshairflash && game::focus->state == CS_ALIVE && game::focus->health < heal)
            {
                int millis = lastmillis%1000;
                float amt = (millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f))*clamp(float(heal-game::focus->health)/float(heal), 0.f, 1.f);
                flashcolour(c.r, c.g, c.b, 1.f, 0.f, 0.f, amt);
            }

            if(crosshairthrob > 0 && regentime && game::focus->lastregen && lastmillis-game::focus->lastregen <= regentime)
            {
                float skew = clamp((lastmillis-game::focus->lastregen)/float(regentime/2), 0.f, 2.f);
                cs += int(cs*(skew > 1.f ? 1.f-skew : skew)*(crosshairthrob*(game::focus->lastregenamt >= 0 ? 1 : -1)));
            }

            if(showcrosshair >= 2)
            {
                bool secondary = physics::secondaryweap(game::focus);
                float accskew = weapons::accmodspread(game::focus, game::focus->weapselect, secondary,  W2(game::focus->weapselect, cooked, true)&W_C_ZOOM && secondary)*crosshairaccamt;
                if(fade > 0 && accskew > 0) fade /= accskew;
            }
        }

        int cx = int(x * w), cy = int(y * h);
        if(index != POINTER_UI)
        {
            drawpointertex(getpointer(index, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c.r, c.g, c.b, fade);
            if(index > POINTER_UI)
            {
                if(game::focus->isalive())
                {
                    if(showcirclebar) drawcirclebar(cx, cy, s, blend);

                    if(game::focus->hasweap(game::focus->weapselect, m_weapon(game::focus->actortype, game::gamemode, game::mutators)))
                    {
                        if(showclips) drawclip(game::focus->weapselect, cx, cy, s, false, blend);
                        if(showindicator) drawindicator(game::focus->weapselect, cx, cy, int(indicatorsize*s), physics::secondaryweap(game::focus), blend);
                    }

                    if(fade > 0 && crosshairhitspeed && totalmillis - game::focus->lasthit <= crosshairhitspeed)
                    {
                        vec c2(1, 1, 1);
                        if(hitcrosshairtone) skewcolour(c2.r, c2.g, c2.b, hitcrosshairtone);
                        else c2 = c;
                        drawpointertex(getpointer(POINTER_HIT, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c2.r, c2.g, c2.b, fade);
                    }
                }

                if(crosshairdistance && game::focus->state == CS_EDITING)
                {
                    draw_textf("\fa%.1f\fwm", cx+crosshairdistancex, cy+crosshairdistancey, 0, 0, -1, -1, -1, int(255*crosshairdistblend), TEXT_RIGHT_JUSTIFY, -1, -1, 1, game::focus->o.dist(worldpos)/8.f);
                    resethudshader();
                }
            }
        }
        else drawpointertex(getpointer(index, game::focus->weapselect), cx, cy, cs, c.r, c.g, c.b, fade);
    }

    void drawpointers(int w, int h, float x, float y, float blend)
    {
        int index = POINTER_NONE;
        if(hasinput(false, true)) index = hasinput(true, true) ? POINTER_UI : POINTER_NONE;
        else if(hidecrosshair || !showhud || !showcrosshair || game::focus->state == CS_DEAD || !gs_playing(game::gamestate) || client::waiting() || (game::thirdpersonview(true) && game::focus != game::player1))
            index = POINTER_NONE;
        else if(game::focus->state == CS_EDITING) index = POINTER_EDIT;
        else if(game::focus->state >= CS_SPECTATOR) index = POINTER_SPEC;
        else if(game::inzoom()) index = POINTER_ZOOM;
        else if(m_team(game::gamemode, game::mutators))
        {
            if(crosshairhitspeed && totalmillis - game::focus->lastteamhit <= crosshairhitspeed) index = POINTER_TEAM;
            else
            {
                vec pos = game::focus->headpos();
                gameent *d = game::intersectclosest(pos, worldpos, game::focus);
                if(d && d->actortype < A_ENEMY && d->team == game::focus->team) index = POINTER_TEAM;
                else index = POINTER_HAIR;
            }
        }
        else index = POINTER_HAIR;

        if(index <= POINTER_NONE) return;

        int s = min(w, h);
        drawpointer(w, h, s, index, x, y, blend);
    }

    FVAR(IDF_PERSIST, visorcamvelx, 0.0f, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorcamvely, 0.0f, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorcamvelscale, 0.0f, 1.0f, FVAR_MAX);

    VAR(IDF_PERSIST, visorfxdelay, 0, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, visorfxdamage, 0, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxcritical, 0, 1.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxoverhealth, 0, 0.25f, FVAR_MAX);

    FVAR(IDF_PERSIST, visorfxchromascale, 0, 0.0025f, 1);
    
    FVAR(IDF_PERSIST, visorfxdesatscale, 0, 0.75f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxdesatamt, 0, 1, FVAR_MAX);

    FVAR(IDF_PERSIST, visorfxsaturatescale, 0, 1, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxsaturateamt, 0, 1, FVAR_MAX);

    FVAR(IDF_PERSIST, visorfxnarrow, 0, 1, 2);
    FVAR(IDF_PERSIST, visorfxnarrowspectv, 0, 0.25f, 2);
    VAR(IDF_PERSIST, visorfxnarrowdelay, 0, 150, VAR_MAX);
    FVAR(IDF_PERSIST, visorfxnarrowrun, 0, 0.9f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxnarrowsprint, 0, 0.7f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxnarrowimpulse, 0, 0.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, visorfxnarrowcrouch, 0, 1.1f, FVAR_MAX);

    void visorinfo(VisorSurface::Config &config)
    {
        static int lastnarrow = 0, laststate = 0;
        bool inactive = progressing || gs_waiting(game::gamestate);

        if(inactive || !game::focus->isactive())
        {
            lastnarrow = laststate = 0;
            config.reset();
            if(inactive)
            {
                config.narrow = game::tvmode(false) ? visorfxnarrowspectv : 1.0f;

                int offmillis = lastmillis - game::maptime;
                if(game::maptime > 0 && mapstartfadein > 0 && offmillis < mapstartfadein)
                    config.narrow *= offmillis / float(mapstartfadein);
            }
            else if(game::tvmode(false))
            {
                config.narrow = visorfxnarrowspectv;
                config.focusdist = game::cameradist();
            }
            return;
        }

        float oldnarrow = config.narrow, newnarrow = game::tvmode() ? visorfxnarrowspectv : visorfxnarrow;
        int curstate = 0;

        config.resetfx();

        float protectscale = 0.0f, spawnscale = 0.0f,
              damagescale = game::damagescale(game::focus, visorfxdelay) * visorfxdamage,
              criticalscale = game::criticalscale(game::focus) * visorfxcritical;

        if(game::focus->isalive())
        {
            if(visorcamvelx > 0.0f) config.offsetx = game::focus->rotvel.x * visorcamvelx * visorcamvelscale;
            if(visorcamvely > 0.0f) config.offsety = game::focus->rotvel.y * visorcamvely * visorcamvelscale;

            if(game::focus->impulseeffect()) newnarrow *= visorfxnarrowimpulse;
            else if(game::focus->sprinting()) newnarrow *= visorfxnarrowsprint;
            else if(game::focus->running()) newnarrow *= visorfxnarrowrun;
            else if(game::focus->crouching()) newnarrow *= visorfxnarrowcrouch;
            
            protectscale = game::protectfade(game::focus);
            newnarrow *= protectscale;
            protectscale = 1.0f - protectscale;
            curstate = 2;
        }
        else if(game::focus->isdead())
        {
            spawnscale = game::spawnfade(game::focus);
            newnarrow *= spawnscale;
            curstate = 1;
        }
        else lastnarrow = 0;

        if(damagescale > 0.0f)
            config.chroma = visorfxchromascale * damagescale;
        
        if(criticalscale > 0.0f)
        {
            config.saturate = visorfxdesatscale;
            config.saturateamt = -visorfxdesatamt * criticalscale;
        }

        if(protectscale > 0.0f)
        {
            config.chroma = max(config.chroma, visorfxchromascale * protectscale);
            config.saturate = visorfxsaturatescale;
            config.saturateamt = visorfxsaturateamt * protectscale;
        }
        else if(game::focus->isalive() && visorfxoverhealth > 0.0f)
        {
            int spawnhp = game::focus->gethealth(game::gamemode, game::mutators);
            if(game::focus->health > spawnhp)
            {
                int maxhp = game::focus->gethealth(game::gamemode, game::mutators, true);
                if(maxhp > spawnhp)
                {
                    float hpscale = clamp((game::focus->health - spawnhp) / float(maxhp - spawnhp), 0.f, 1.f) * visorfxoverhealth;
                    config.saturate = visorfxsaturatescale;
                    config.saturateamt = visorfxsaturateamt * hpscale;
                }
            }
        }
        
        if(lastnarrow && curstate == laststate && visorfxnarrowdelay > 0 && oldnarrow != newnarrow)
        {
            float amt = (lastmillis - lastnarrow) / float(visorfxnarrowdelay);
            if(newnarrow > oldnarrow)
            {
                if((oldnarrow += amt) < newnarrow) newnarrow = oldnarrow;
            }
            else if((oldnarrow -= amt) > newnarrow) newnarrow = oldnarrow;
        }

        config.narrow = newnarrow;
        lastnarrow = lastmillis;
        laststate = curstate;
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

        aspect = forceaspect ? forceaspect : w/float(h);
        fovy = 2*atan2(tan(curfov/2*RAD), aspect)/RAD;

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
