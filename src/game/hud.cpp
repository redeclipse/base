#include "game.h"
namespace hud
{
    const int NUMSTATS = 21;
    int uimillis = 0, damageresidue = 0, hudwidth = 0, hudheight = 0, lastteam = 0, laststats = 0, prevstats[NUMSTATS] = {0}, curstats[NUMSTATS] = {0};

    #include "compass.h"
    vector<int> teamkills;

    struct dhloc
    {
        int clientnum, outtime, damage, colour; vec dir;
        dhloc(int a, int t, int d, const vec &p, int c) : clientnum(a), outtime(t), damage(d), colour(c), dir(p) {}
    };
    vector<dhloc> damagelocs, hitlocs;
    VAR(IDF_PERSIST, damageresiduefade, 0, 500, VAR_MAX);

    ICOMMAND(0, conout, "is", (int *n, char *s), conoutft(clamp(*n, 0, CON_MAX-1), "%s", s));

    VAR(IDF_PERSIST, showhud, 0, 1, 1);
    VAR(IDF_PERSIST, hudsize, 0, 2048, VAR_MAX);
    FVAR(IDF_PERSIST, hudblend, 0, 1, 1);

    VAR(IDF_PERSIST, showdemoplayback, 0, 1, 1);
    FVAR(IDF_PERSIST, edgesize, 0, 0.005f, 1000);

    VAR(IDF_PERSIST, shownotices, 0, 3, 4);
    VAR(IDF_PERSIST, showevents, 0, 3, 4);
    VAR(IDF_PERSIST, showeventicons, 0, 1, 7);
    VAR(IDF_PERSIST, showloadingaspect, 0, 2, 3);
    VAR(IDF_PERSIST, showloadingmapbg, 0, 1, 1);
    VAR(IDF_PERSIST, showloadinglogos, 0, 0, 1);

    VAR(IDF_PERSIST, statrate, 1, 100, 1000);

    void enginestatrefresh()
    {
        if(totalmillis-laststats >= statrate)
        {
            memcpy(prevstats, curstats, sizeof(prevstats));
            laststats = totalmillis-(totalmillis%statrate);
        }
        int nextstats[NUMSTATS] = {
            wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024, allocnodes*8, allocva, glde, gbatches, getnumqueries(),
            curfps, bestfpsdiff, worstfpsdiff, entities::ents.length(), entgroup.length(), ai::waypoints.length(), getnumviewcells(),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()), int(vec(game::focus->vel).add(game::focus->falling).magnitude()/8.f), int(vec(game::focus->vel).add(game::focus->falling).magnitude()*0.45f)
        };
        loopi(NUMSTATS) if(prevstats[i] == curstats[i]) curstats[i] = nextstats[i];
    }
    ICOMMAND(0, refreshenginestats, "", (), enginestatrefresh());
    ICOMMAND(0, getenginestat, "ii", (int *n, int *prev), intret(*n >= 0 && *n < NUMSTATS ? (*prev!=0 ? prevstats[*n] : curstats[*n]) : -1));
    static const char *enginestats[NUMSTATS] = { "wtr", "wtr%", "wvt", "wvt%", "evt", "eva", "ond", "va", "gl" "gb", "oq", "fps", "best", "worst", "ents", "entsel", "wp", "pvs", "vel", "mps", "kmh" };
    ICOMMAND(0, getenginestatname, "i", (int *n), result(*n >= 0 && *n < NUMSTATS ? enginestats[*n]: ""));
    #define LOOPENGSTATS(name,op) \
        ICOMMAND(0, loopenginestat##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            loopstart(id, stack); \
            op(NUMSTATS, *count, *skip) \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            } \
            loopend(id, stack); \
        });
    LOOPENGSTATS(,loopcsi)
    LOOPENGSTATS(rev,loopcsirev)

    VAR(IDF_PERSIST, titlefade, 0, 1000, 10000);
    VAR(IDF_PERSIST, tvmodefade, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, spawnfade, 0, 250, VAR_MAX);

    VAR(IDF_PERSIST, commandfade, 0, 250, VAR_MAX);
    FVAR(IDF_PERSIST, commandfadeamt, 0, 0.75f, 1);
    VAR(IDF_PERSIST, uifade, 0, 250, VAR_MAX);
    FVAR(IDF_PERSIST, uifadeamt, 0, 0.5f, 1);

    FVAR(IDF_PERSIST, noticeoffset, -1, 0.35f, 1);
    FVAR(IDF_PERSIST, noticeblend, 0, 1, 1);
    FVAR(IDF_PERSIST, noticescale, 1e-4f, 1, 1000);
    FVAR(IDF_PERSIST, noticepadx, FVAR_MIN, 0, FVAR_MAX);
    FVAR(IDF_PERSIST, noticepady, FVAR_MIN, 0, FVAR_MAX);
    VAR(IDF_PERSIST, noticetitle, 0, 10000, 60000);
    FVAR(IDF_PERSIST, eventoffset, -1, 0.58f, 1);
    FVAR(IDF_PERSIST, eventblend, 0, 1, 1);
    FVAR(IDF_PERSIST, eventscale, 1e-4f, 1, 1000);
    FVAR(IDF_PERSIST, eventiconscale, 1e-4f, 2.5f, 1000);
    FVAR(IDF_PERSIST, eventpadx, FVAR_MIN, 0.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, eventpady, FVAR_MIN, 0.125f, FVAR_MAX);
    VAR(IDF_PERSIST, noticetime, 0, 5000, VAR_MAX);
    VAR(IDF_PERSIST, obitnotices, 0, 2, 2);
    VAR(IDF_PERSIST, teamnotices, 0, 2, 2);
    VAR(IDF_PERSIST, teamnoticedelay, 0, 5000, VAR_MAX);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamtex, "<grey>textures/icons/team", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamalphatex, "<grey>textures/icons/teamalpha", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamomegatex, "<grey>textures/icons/teamomega", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamkappatex, "<grey>textures/icons/teamkappa", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamsigmatex, "<grey>textures/icons/teamsigma", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playertex, "<grey>textures/icons/player", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, deadtex, "<grey>textures/icons/dead", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatingtex, "<grey>textures/icons/dominating", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatedtex, "<grey>textures/icons/dominated", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inputtex, "textures/icons/menu", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, waitingtex, "<grey>textures/icons/waiting", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spectatortex, "<grey>textures/icons/spectator", 3);
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
    VAR(IDF_PERSIST|IDF_HEX, noticetone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, eventtone, -CTONE_MAX, -CTONE_TEAM-1, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, clipstone, -CTONE_MAX, 0, 0xFFFFFF);

    VAR(IDF_PERSIST, teamhurthud, 0, 1, 3); // 0 = off, 1 = full body particle, 2 = fixed position and size
    VAR(IDF_PERSIST, teamhurttime, 0, 2500, VAR_MAX);
    VAR(IDF_PERSIST, teamhurtdist, 0, 0, VAR_MAX);
    FVAR(IDF_PERSIST, teamhurtsize, 0, 0.0175f, 1000);

    VAR(IDF_PERSIST, showdamage, 0, 2, 2); // 1 shows just damage texture, 2 blends as well
    VAR(IDF_PERSIST, damagefade, 0, 0, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damagetex, "<grey>textures/hud/damage", 3);
    FVAR(IDF_PERSIST, damageblend, 0, 1, 1);
    FVAR(IDF_PERSIST, damageskew, 0, 0.25f, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, burntex, "<grey>textures/hud/burn", 3);
    FVAR(IDF_PERSIST, burnblend, 0, 1, 1);

    VAR(IDF_PERSIST, showindicator, 0, 4, 4);
    FVAR(IDF_PERSIST, indicatorsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, indicatorblend, 0, 0.5f, 1);
    VAR(IDF_PERSIST, indicatorminattack, 0, 1000, VAR_MAX);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, indicatortex, "<grey>textures/hud/indicator", 3);

    VAR(IDF_PERSIST, showcrosshair, 0, 2, 2); // 0 = off, 1 = on, 2 = blend depending on current accuracy level
    VAR(IDF_PERSIST, crosshairdistance, 0, 0, 1); // 0 = off, 1 = shows distance to crosshair target
    VAR(IDF_PERSIST, crosshairdistancex, VAR_MIN, 160, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairdistancey, VAR_MIN, 80, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairweapons, 0, 0, 3); // 0 = off, &1 = crosshair-specific weapons, &2 = also appy colour
    FVAR(IDF_PERSIST, crosshairsize, 0, 0.03f, 1000);
    VAR(IDF_PERSIST, crosshairhitspeed, 0, 500, VAR_MAX);
    FVAR(IDF_PERSIST, crosshairblend, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, crosshairaccamt, 0, 0.75f, 1);
    VAR(IDF_PERSIST, crosshairflash, 0, 1, 1);
    FVAR(IDF_PERSIST, crosshairthrob, 1e-4f, 0.1f, 1000);
    TVAR(IDF_PERSIST|IDF_PRELOAD, pointertex, "textures/hud/pointer", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursortex, "textures/hud/cursor", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursorhovertex, "textures/hud/cursorhover", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, crosshairtex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hithairtex, "crosshairs/cross-01-hit", 3);
    TVAR(IDF_PERSIST, clawcrosshairtex, "crosshairs/triangle-02", 3);
    TVAR(IDF_PERSIST, clawhithairtex, "crosshairs/triangle-02-hit", 3);
    TVAR(IDF_PERSIST, pistolcrosshairtex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST, pistolhithairtex, "crosshairs/cross-01-hit", 3);
    TVAR(IDF_PERSIST, swordcrosshairtex, "crosshairs/simple-02", 3);
    TVAR(IDF_PERSIST, swordhithairtex, "crosshairs/simple-02-hit", 3);
    TVAR(IDF_PERSIST, shotguncrosshairtex, "crosshairs/cross-02", 3);
    TVAR(IDF_PERSIST, shotgunhithairtex, "crosshairs/cross-02-hit", 3);
    TVAR(IDF_PERSIST, smgcrosshairtex, "crosshairs/simple-03", 3);
    TVAR(IDF_PERSIST, smghithairtex, "crosshairs/simple-03-hit", 3);
    TVAR(IDF_PERSIST, plasmacrosshairtex, "crosshairs/circle-03", 3);
    TVAR(IDF_PERSIST, plasmahithairtex, "crosshairs/circle-03-hit", 3);
    TVAR(IDF_PERSIST, zappercrosshairtex, "crosshairs/circle-03", 3);
    TVAR(IDF_PERSIST, zapperhithairtex, "crosshairs/circle-03-hit", 3);
    TVAR(IDF_PERSIST, flamercrosshairtex, "crosshairs/circle-04", 3);
    TVAR(IDF_PERSIST, flamerhithairtex, "crosshairs/circle-04-hit", 3);
    TVAR(IDF_PERSIST, riflecrosshairtex, "crosshairs/simple-01", 3);
    TVAR(IDF_PERSIST, riflehithairtex, "crosshairs/simple-01-hit", 3);
    TVAR(IDF_PERSIST, grenadecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, grenadehithairtex, "crosshairs/circle-02-hit", 3);
    TVAR(IDF_PERSIST, minecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, minehithairtex, "crosshairs/circle-02-hit", 3);
    TVAR(IDF_PERSIST, rocketcrosshairtex, "crosshairs/circle-01", 3);
    TVAR(IDF_PERSIST, rockethithairtex, "crosshairs/circle-01-hit", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editcursortex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, speccursortex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, tvcursortex, "", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamcrosshairtex, "", 3);
    VAR(IDF_PERSIST, cursorstyle, 0, 0, 1); // 0 = top left tracking, 1 = center
    FVAR(IDF_PERSIST, cursorsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, cursorblend, 0, 1, 1);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomtex, "textures/hud/zoom", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomcrosshairtex, "crosshairs/cross-01", 3);
    FVAR(IDF_PERSIST, zoomcrosshairsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, zoomcrosshairblend, 0, 0.75f, 1000);

    VAR(IDF_PERSIST, showcirclebar, 0, 0, 1);
    VAR(IDF_PERSIST, circlebartype, 0, 7, 7); // 0 = off, &1 = health, &2 = impulse, &4 = ammo
    FVAR(IDF_PERSIST, circlebarsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, circlebarblend, 0, 0.75f, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarhealthtone, -CTONE_MAX, 0x88FF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarimpulsetone, -CTONE_MAX, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammocolour, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammotone, -CTONE_MAX-1, 0xFFAA66, 0xFFFFFF);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, circlebartex, "textures/hud/circlebar", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawtex, "<grey>textures/weapons/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistoltex, "<grey>textures/weapons/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordtex, "<grey>textures/weapons/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguntex, "<grey>textures/weapons/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgtex, "<grey>textures/weapons/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadetex, "<grey>textures/weapons/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minetex, "<grey>textures/weapons/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rockettex, "<grey>textures/weapons/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamertex, "<grey>textures/weapons/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmatex, "<grey>textures/weapons/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappertex, "<grey>textures/weapons/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rifletex, "<grey>textures/weapons/rifle", 3);

    VAR(IDF_PERSIST, showclips, 0, 2, 2);
    VAR(IDF_PERSIST, clipanims, 0, 2, 2);
    FVAR(IDF_PERSIST, clipsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, clipoffset, 0, 0.035f, 1000);
    FVAR(IDF_PERSIST, clipminscale, 0, 0.3f, 1000);
    FVAR(IDF_PERSIST, clipmaxscale, 0, 1, 1000);
    FVAR(IDF_PERSIST, clipblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, clipcolour, 0, 1, 1);
    VAR(IDF_PERSIST, cliplength, 0, 0, VAR_MAX);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawcliptex, "<grey>textures/weapons/clips/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistolcliptex, "<grey>textures/weapons/clips/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordcliptex, "<grey>textures/weapons/clips/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguncliptex, "<grey>textures/weapons/clips/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgcliptex, "<grey>textures/weapons/clips/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamercliptex, "<grey>textures/weapons/clips/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmacliptex, "<grey>textures/weapons/clips/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappercliptex, "<grey>textures/weapons/clips/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, riflecliptex, "<grey>textures/weapons/clips/rifle", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadecliptex, "<grey>textures/weapons/clips/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minecliptex, "<grey>textures/weapons/clips/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rocketcliptex, "<grey>textures/weapons/clips/rocket", 3);
    FVAR(IDF_PERSIST, clawclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, pistolclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, swordclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, shotgunclipoffset, 0, 0.125f, 0.5f);
    FVAR(IDF_PERSIST, smgclipoffset, 0, 0.35f, 0.5f);
    FVAR(IDF_PERSIST, flamerclipoffset, 0, 0.5f, 0.5f);
    FVAR(IDF_PERSIST, plasmaclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, zapperclipoffset, 0, 0.3f, 0.5f);
    FVAR(IDF_PERSIST, rifleclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, grenadeclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, mineclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, rocketclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, clawclipskew, 0, 0.75f, 10);
    FVAR(IDF_PERSIST, pistolclipskew, 0, 0.65f, 10);
    FVAR(IDF_PERSIST, swordclipskew, 0, 1, 10);
    FVAR(IDF_PERSIST, shotgunclipskew, 0, 0.75f, 10);
    FVAR(IDF_PERSIST, smgclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, flamerclipskew, 0, 0.45f, 10);
    FVAR(IDF_PERSIST, plasmaclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, zapperclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, rifleclipskew, 0, 1, 10);
    FVAR(IDF_PERSIST, grenadeclipskew, 0, 1.25f, 10);
    FVAR(IDF_PERSIST, mineclipskew, 0, 1.25f, 10);
    FVAR(IDF_PERSIST, rocketclipskew, 0, 1.25f, 10);
    VAR(IDF_PERSIST, clawcliprotate, 0, 12, 7); // "round-the-clock" rotation of texture, 0 = off, &1 = flip x, &2 = flip y, &4 = angle, &8 = spin
    VAR(IDF_PERSIST, pistolcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, swordcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, shotguncliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, smgcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, flamercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, plasmacliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, zappercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, riflecliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, grenadecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, minecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, rocketcliprotate, 0, 12, 15);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bliptex, "<grey>textures/hud/blip", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playerbliptex, "<grey>textures/hud/playerblip", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hurttex, "<grey>textures/hud/hurt", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hinttex, "<grey>textures/hud/hint", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, glowtex, "<grey>textures/hud/glow", 3);

    VAR(IDF_PERSIST, onscreendamage, 0, 1, 2); // 0 = off, 1 = basic damage, 2 = verbose
    FVAR(IDF_PERSIST, onscreendamagescale, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, onscreendamageblipsize, 0, 0.1f, 1);
    FVAR(IDF_PERSIST, onscreendamageoffset, 0, 0.4f, 1);
    VAR(IDF_PERSIST, onscreendamageself, 0, 1, 1);
    VAR(IDF_PERSIST, onscreendamagemerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagetime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagefade, 1, 3500, VAR_MAX);
    FVAR(IDF_PERSIST, onscreendamagesize, 0, 20, 1000);
    FVAR(IDF_PERSIST, onscreendamageblend, 0, 0.8f, 1);
    VAR(IDF_PERSIST, onscreendamagemin, 1, 10, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagemax, 1, 100, VAR_MAX);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamagecolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamageburncolour, PC(LAST), PC(BURN), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamagebleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamageshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

    VAR(IDF_PERSIST, onscreenhits, 0, 1, 2);
    VAR(IDF_PERSIST, onscreenhitsheal, 0, 1, 1);
    VAR(IDF_PERSIST, onscreenhitsself, 0, 0, 1);
    VAR(IDF_PERSIST, onscreenhitsfollow, 0, 0, 1);
    VAR(IDF_PERSIST, onscreenhitsmerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreenhitstime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreenhitsfade, 1, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, onscreenhitsswipe, 0, 6, 1000);
    FVAR(IDF_PERSIST, onscreenhitsscale, 0, 1.5f, 1000);
    FVAR(IDF_PERSIST, onscreenhitsblend, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsheight, -1000, 0.25f, 1000);
    FVAR(IDF_PERSIST, onscreenhitsoffset, -1000, 3, 1000);
    VAR(IDF_PERSIST, onscreenhitsglow, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsglowblend, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsglowscale, 0, 2, 1000);
    FVAR(IDF_PERSIST, onscreenhitsglowcolour, 0, 0.75f, 5);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, onscreenhitsglowtex, "<grey>textures/hud/glow", 3);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitscolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsburncolour, PC(LAST), PC(BURN), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsbleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

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
    TVAR(IDF_PERSIST, privoperatortex, "<grey>textures/privs/operator", 3);
    TVAR(IDF_PERSIST, privadministratortex, "<grey>textures/privs/administrator", 3);
    TVAR(IDF_PERSIST, privdevelopertex, "<grey>textures/privs/developer", 3);
    TVAR(IDF_PERSIST, privfoundertex, "<grey>textures/privs/founder", 3);
    TVAR(IDF_PERSIST, privlocalsupportertex, "<grey>textures/privs/localsupporter", 3);
    TVAR(IDF_PERSIST, privlocalmoderatortex, "<grey>textures/privs/localmoderator", 3);
    TVAR(IDF_PERSIST, privlocaloperatortex, "<grey>textures/privs/localoperator", 3);
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
    TVAR(IDF_PERSIST, modebomberattacktex, "<grey>textures/modes/bomberattack", 3);

    TVAR(IDF_PERSIST, moderacetex, "<grey>textures/modes/race", 3);
    TVAR(IDF_PERSIST, moderacetimedtex, "<grey>textures/modes/racetimed", 3);
    TVAR(IDF_PERSIST, moderaceendurancetex, "<grey>textures/modes/raceendurance", 3);
    TVAR(IDF_PERSIST, moderacegauntlettex, "<grey>textures/modes/racegauntlet", 3);

    TVAR(IDF_PERSIST, modemultitex, "<grey>textures/modes/multi", 3);
    TVAR(IDF_PERSIST, modeffatex, "<grey>textures/modes/ffa", 3);
    TVAR(IDF_PERSIST, modecooptex, "<grey>textures/modes/coop", 3);
    TVAR(IDF_PERSIST, modeinstatex, "<grey>textures/modes/instagib", 3);
    TVAR(IDF_PERSIST, modemedievaltex, "<grey>textures/modes/medieval", 3);
    TVAR(IDF_PERSIST, modekaboomtex, "<grey>textures/modes/kaboom", 3);
    TVAR(IDF_PERSIST, modedueltex, "<grey>textures/modes/duel", 3);
    TVAR(IDF_PERSIST, modesurvivortex, "<grey>textures/modes/survivor", 3);
    TVAR(IDF_PERSIST, modeclassictex, "<grey>textures/modes/classic", 3);
    TVAR(IDF_PERSIST, modeonslaughttex, "<grey>textures/modes/onslaught", 3);
    TVAR(IDF_PERSIST, modefreestyletex, "<grey>textures/modes/freestyle", 3);
    TVAR(IDF_PERSIST, modevampiretex, "<grey>textures/modes/vampire", 3);
    TVAR(IDF_PERSIST, moderesizetex, "<grey>textures/modes/resize", 3);
    TVAR(IDF_PERSIST, modehardtex, "<grey>textures/modes/hard", 3);
    TVAR(IDF_PERSIST, modebasictex, "<grey>textures/modes/basic", 3);

    #define ADDMODEICON(g,m) \
    { \
        if(m_demo(g)) ADDMODE(modedemotex) \
        else if(m_edit(g)) ADDMODE(modeeditingtex) \
        else if(m_capture(g)) \
        { \
            if(m_ctf_quick(g, m)) ADDMODE(modecapturequicktex) \
            else if(m_ctf_defend(g, m)) ADDMODE(modecapturedefendtex) \
            else if(m_ctf_protect(g, m)) ADDMODE(modecaptureprotecttex) \
            else ADDMODE(modecapturetex) \
        } \
        else if(m_defend(g)) \
        { \
            if(m_dac_king(g, m)) \
            { \
                ADDMODE(modedefendkingtex) \
                if(m_dac_quick(g, m)) ADDMODE(modedefendquicktex) \
            } \
            else if(m_dac_quick(g, m)) ADDMODE(modedefendquicktex) \
            else ADDMODE(modedefendtex) \
        } \
        else if(m_bomber(g)) \
        { \
            if(m_bb_hold(g, m)) ADDMODE(modebomberholdtex) \
            else if(m_bb_attack(g, m)) \
            { \
                ADDMODE(modebomberattacktex) \
                if(m_bb_basket(g, m)) ADDMODE(modebomberbaskettex) \
            } \
            else if(m_bb_basket(g, m)) ADDMODE(modebomberbaskettex) \
            else ADDMODE(modebombertex) \
        } \
        else if(m_race(g)) \
        { \
            if(m_ra_gauntlet(g, m)) \
            { \
                ADDMODE(moderacegauntlettex) \
                if(m_ra_timed(g, m)) ADDMODE(moderacetimedtex) \
                if(m_ra_endurance(g, m)) ADDMODE(moderaceendurancetex) \
            } \
            else if(m_ra_timed(g, m)) \
            { \
                ADDMODE(moderacetimedtex) \
                if(m_ra_endurance(g, m)) ADDMODE(moderaceendurancetex) \
            } \
            else if(m_ra_endurance(g, m)) ADDMODE(moderaceendurancetex) \
            else ADDMODE(moderacetex) \
        } \
        else \
        { \
            if(m_dm_gladiator(g, m)) ADDMODE(modegladiatortex) \
            else if(m_dm_oldschool(g, m)) ADDMODE(modeoldschooltex) \
            else ADDMODE(modedeathmatchtex) \
        } \
    }

    #define ADDMODE(s) { if(list.length()) list.add(' '); list.put(s, strlen(s)); }
    void modetex(int g, int m, vector<char> &list)
    {
        modecheck(g, m);
        ADDMODEICON(g, m);
    }

    void modetexs(int g, int m, bool before, bool implied, vector<char> &list)
    {
        modecheck(g, m);
        if(before) modetex(g, m, list);
        if(m_multi(g, m) && (implied || !(gametype[g].implied&(1<<G_M_MULTI)))) ADDMODE(modemultitex)
        if(m_ffa(g, m) && (implied || !(gametype[g].implied&(1<<G_M_FFA)))) ADDMODE(modeffatex)
        if(m_coop(g, m) && (implied || !(gametype[g].implied&(1<<G_M_COOP)))) ADDMODE(modecooptex)
        if(m_insta(g, m) && (implied || !(gametype[g].implied&(1<<G_M_INSTA)))) ADDMODE(modeinstatex)
        if(m_medieval(g, m) && (implied || !(gametype[g].implied&(1<<G_M_MEDIEVAL)))) ADDMODE(modemedievaltex)
        if(m_kaboom(g, m) && (implied || !(gametype[g].implied&(1<<G_M_KABOOM)))) ADDMODE(modekaboomtex)
        if(m_duel(g, m) && (implied || !(gametype[g].implied&(1<<G_M_DUEL)))) ADDMODE(modedueltex)
        if(m_survivor(g, m) && (implied || !(gametype[g].implied&(1<<G_M_SURVIVOR)))) ADDMODE(modesurvivortex)
        if(m_classic(g, m) && (implied || !(gametype[g].implied&(1<<G_M_CLASSIC)))) ADDMODE(modeclassictex)
        if(m_onslaught(g, m) && (implied || !(gametype[g].implied&(1<<G_M_ONSLAUGHT)))) ADDMODE(modeonslaughttex)
        if(m_freestyle(g, m) && (implied || !(gametype[g].implied&(1<<G_M_FREESTYLE)))) ADDMODE(modefreestyletex)
        if(m_vampire(g, m) && (implied || !(gametype[g].implied&(1<<G_M_VAMPIRE)))) ADDMODE(modevampiretex)
        if(m_resize(g, m) && (implied || !(gametype[g].implied&(1<<G_M_RESIZE)))) ADDMODE(moderesizetex)
        if(m_hard(g, m) && (implied || !(gametype[g].implied&(1<<G_M_HARD)))) ADDMODE(modehardtex)
        if(m_basic(g, m) && (implied || !(gametype[g].implied&(1<<G_M_BASIC)))) ADDMODE(modebasictex)
        if(!before) modetex(g, m, list);
    }
    #undef ADDMODE

    ICOMMAND(0, modetexlist, "iibi", (int *g, int *m, int *b, int *p),
    {
        vector<char> list;
        if(*b >= 0) modetexs(*g, *m, *b!=0, *p!=0, list);
        else modetex(*g, *m, list);
        list.add('\0');
        result(list.getbuf());
    });

    bool needminimap() { return true; }

    bool hasinput(bool pass, bool focus)
    {
        if(focus && (commandmillis > 0 || curcompass)) return true;
        return UI::hasinput() || UI::hasmenu(pass);
    }

    bool hastkwarn(gameent *d)
    {
        if(!m_play(game::gamemode)) return false;
        return teamkillwarn && m_team(game::gamemode, game::mutators) && numteamkills() >= teamkillwarn;
    }

    bool hasteaminfo(gameent *d)
    {
        if(!m_play(game::gamemode) || game::focus->state != CS_ALIVE) return false;
        if(!lastteam) lastteam = totalmillis ? totalmillis : 1;
        return teamnotices >= 1 && totalmillis-lastteam <= teamnoticedelay;
    }

    bool textinput(const char *str, int len)
    {
        return UI::textinput(str, len);
    }

    bool keypress(int code, bool isdown)
    {
        if(curcompass) return keycmenu(code, isdown);
        return UI::keypress(code, isdown); // ignore UI if compass is open
    }

    void checkui()
    {
        UI::hideui("loading");
        UI::showui("hud");
        if(connected())
        {
            if(!UI::hasmenu() && (game::needname(game::player1) || game::wantsloadoutmenu))
            {
                UI::openui("profile");
                game::wantsloadoutmenu = false;
            }
            else
            {
                UI::pressui("scoreboard", scoreson);
                if(game::player1->state == CS_DEAD) { if(scoreson) shownscores = true; }
                else shownscores = false;
            }
        }
        else if(!UI::hasmenu()) UI::openui(game::needname(game::player1) ? "profile" : "main");
        UI::update();
    }

    void damage(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n) return;
        damageresidue = clamp(damageresidue+(n*(flags&HIT_BLEED ? 3 : 1)), 0, 200);
        int colour = onscreendamagecolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF44FF;
        else if(wr_burns(weap, flags)) colour = onscreendamageburncolour;
        else if(wr_bleeds(weap, flags)) colour = onscreendamagebleedcolour;
        else if(wr_shocks(weap, flags)) colour = onscreendamageshockcolour;
        vec dir = vec(loc).sub(camera1->o).normalize();
        loopv(damagelocs)
        {
            dhloc &d = damagelocs[i];
            if(v->clientnum != d.clientnum) continue;
            if(totalmillis-d.outtime > onscreendamagemerge) continue;
            if(d.colour != colour) continue;
            d.damage += n;
            d.dir = dir;
            return; // accumulate
        }
        damagelocs.add(dhloc(v->clientnum, totalmillis, n, loc, colour));
    }

    void hit(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n) return;
        int colour = onscreenhitscolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF44FF;
        else if(wr_burns(weap, flags)) colour = onscreenhitsburncolour;
        else if(wr_bleeds(weap, flags)) colour = onscreenhitsbleedcolour;
        else if(wr_shocks(weap, flags)) colour = onscreenhitsshockcolour;
        loopv(hitlocs)
        {
            dhloc &d = hitlocs[i];
            if(v->clientnum != d.clientnum) continue;
            if(totalmillis-d.outtime > onscreenhitsmerge) continue;
            if(d.colour != colour) continue;
            d.damage += n;
            d.dir = v->center();
            return; // accumulate
        }
        hitlocs.add(dhloc(v->clientnum, totalmillis, n, v->center(), colour));
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

    void drawblend(int x, int y, int w, int h, float r, float g, float b, bool blend)
    {
        if(!blend) glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        gle::colorf(r, g, b);
        gle::defvertex(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y);
        gle::attribf(x+w, y);
        gle::attribf(x, y+h);
        gle::attribf(x+w, y+h);
        gle::end();
        if(!blend) glDisable(GL_BLEND);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        if(colour < 0) colour = game::getcolour(game::focus, INVPULSE(colour));
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
        POINTER_NONE = 0, POINTER_RELATIVE, POINTER_UI, POINTER_EDIT, POINTER_SPEC,
        POINTER_HAIR, POINTER_TEAM, POINTER_ZOOM, POINTER_HIT, POINTER_MAX
    };

    const char *getpointer(int index, int weap = -1)
    {
        switch(index)
        {
            case POINTER_RELATIVE: return pointertex;
            case POINTER_UI:
            {
                switch(UI::cursortype)
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
                        flamercrosshairtex, plasmacrosshairtex, zappercrosshairtex, riflecrosshairtex, grenadecrosshairtex, minecrosshairtex,
                        rocketcrosshairtex, "" // end of regular weapons
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
                        flamerhithairtex, plasmahithairtex, zapperhithairtex, riflehithairtex, grenadehithairtex, minehithairtex,
                        rockethithairtex, "" // end of regular weapons
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

    void drawindicator(int weap, int x, int y, float s, bool secondary)
    {
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
                if(showindicator < (W(weap, ammoadd) < W(weap, ammomax) ? 3 : 2)) return;
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
        Texture *t = textureload(indicatortex, 3);
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, t->id);
        float val = amt < 0.25f ? amt : (amt > 0.75f ? 1.f-amt : 0.25f);
        gle::colorf(val*4.f, val*4.f, val*4.f, indicatorblend*hudblend*val);
        drawsized(x-s, y-s, s*2);
        gle::colorf(r, g, b, indicatorblend*hudblend);
        drawslice(0, clamp(amt, 0.f, 1.f), x, y, s);
    }

    void drawclipitem(const char *tex, float x, float y, float offset, float size, float blend, float angle, float spin, int rotate, const vec &colour)
    {
        Texture *t = textureload(tex, 3);
        if(t)
        {
            while(angle < 0.0f) angle += 360.0f;
            while(angle >= 360.0f) angle -= 360.0f;
            bool flipx = false, flipy = false;
            float rot = 0;
            if(rotate&8) rot += spin;
            if(rotate&4) rot += angle;
            if(rotate&2) flipy= angle > 90.f && angle <= 180.f;
            if(rotate&1) flipx = angle >= 180.f;
            while(rot < 0.0f) rot += 360.0f;
            while(rot >= 360.0f) rot -= 360.0f;
            vec2 loc(x+offset*sinf(RAD*angle), y+offset*-cosf(RAD*angle));
            gle::color(colour, blend);
            glBindTexture(GL_TEXTURE_2D, t->id);
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
    }

    void drawsimpleclipitem(float x, float y, float start, float length, float size, float blend, const vec &colour)
    {
        Texture *t = textureload(progringtex, 3);
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::color(colour, blend);
        drawslice(start, length, x, y, size);
    }

    void drawclip(int weap, int x, int y, float s)
    {
        if(!isweap(weap) || weap >= W_ALL || (!W2(weap, ammosub, false) && !W2(weap, ammosub, true)) || W(weap, ammomax) < cliplength) return;
        const char *cliptexs[W_ALL] = {
            clawcliptex, pistolcliptex, swordcliptex, shotguncliptex, smgcliptex,
            flamercliptex, plasmacliptex, zappercliptex, riflecliptex, grenadecliptex, minecliptex, rocketcliptex
        };
        const float clipoffs[W_ALL] = {
            clawclipoffset, pistolclipoffset, swordclipoffset, shotgunclipoffset, smgclipoffset,
            flamerclipoffset, plasmaclipoffset, zapperclipoffset, rifleclipoffset, grenadeclipoffset, mineclipoffset, rocketclipoffset
        };
        const float clipskew[W_ALL] = {
            clawclipskew, pistolclipskew, swordclipskew, shotgunclipskew, smgclipskew,
            flamerclipskew, plasmaclipskew, zapperclipskew, rifleclipskew, grenadeclipskew, mineclipskew, rocketclipskew
        };
        const int cliprots[W_ALL] = {
            clawcliprotate, pistolcliprotate, swordcliprotate, shotguncliprotate, smgcliprotate,
            flamercliprotate, plasmacliprotate, zappercliprotate, riflecliprotate, grenadecliprotate, minecliprotate, rocketcliprotate
        };
        int ammo = game::focus->ammo[weap], maxammo = W(weap, ammomax), interval = lastmillis-game::focus->weaptime[weap];
        bool simple = showclips == 1 || maxammo > 360;
        float fade = clipblend*hudblend, size = s*clipsize*(simple ? 1.f : clipskew[weap]), offset = s*clipoffset, amt = 0, spin = 0,
              slice = 360/float(maxammo), angle = (simple || maxammo > (cliprots[weap]&4 ? 4 : 3) || maxammo%2 ? 360.f : 360.f-slice*0.5f)-((maxammo-ammo)*slice),
              area = 1-clamp(clipoffs[weap]*2, 1e-3f, 1.f), need = s*clipsize*clipskew[weap]*area*maxammo, have = 2*M_PI*s*clipoffset,
              scale = clamp(have/need, clipminscale, clipmaxscale);
        vec c(1, 1, 1);
        if(clipstone) skewcolour(c.r, c.g, c.b, clipstone);
        if(clipcolour) skewcolour(c.r, c.g, c.b, W(weap, colour));
        if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
        {
            case W_S_PRIMARY: case W_S_SECONDARY:
            {
                amt = 1.f-clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                fade *= amt;
                if(clipanims)
                {
                    size *= amt;
                    offset *= amt;
                    if(clipanims >= 2) spin = 360*amt;
                }
                int shot = game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1;
                if(simple) drawsimpleclipitem(x, y, angle/360.f, slice*shot/360.f, size, fade, c);
                else
                {
                    float rewind = angle;
                    loopi(shot) drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, rewind += slice, spin, cliprots[weap], c);
                }
                fade = clipblend*hudblend;
                size = s*clipsize*clipskew[weap];
                offset = s*clipoffset;
                spin = 0;
                break;
            }
            case W_S_RELOAD: case W_S_USE:
            {
                if(game::focus->weapload[weap] > 0)
                {
                    int check = game::focus->weapwait[weap]/2;
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
                    }
                    else
                    {
                        fade = spin = 0;
                        if(clipanims) size = offset = 0;
                    }
                    if(simple)
                    {
                        float amt = slice*game::focus->weapload[weap];
                        angle -= amt;
                        drawsimpleclipitem(x, y, angle/360.f, amt/360.f, size, fade, c);
                    }
                    else loopi(game::focus->weapload[weap])
                    {
                        drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
                        angle -= slice;
                    }
                    ammo -= game::focus->weapload[weap];
                    fade = clipblend*hudblend;
                    size = s*clipsize*clipskew[weap];
                    offset = s*clipoffset;
                    spin = 0;
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
        if(simple) drawsimpleclipitem(x, y, 0, slice*ammo/360.f, size, fade, c);
        else loopi(ammo)
        {
            drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
            angle -= slice;
        }
    }

    void drawcirclebar(int x, int y, float s)
    {
        if(game::focus->state != CS_ALIVE) return;
        int num = 0;
        loopi(3) if(circlebartype&(1<<i)) num++;
        if(!num) return;
        Texture *t = circlebartex && *circlebartex ? textureload(circlebartex, 3) : NULL;
        if(!t || t == notexture) return;
        float slice = 1.f/num, pos = num%2 ? slice*0.5f : 0.f;
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, t->id);
        loopi(3) if(circlebartype&(1<<i))
        {
            float val = 0, fade = hudblend*circlebarblend;
            vec c(1, 1, 1);
            switch(i)
            {
                case 0:
                    val = min(1.f, game::focus->health/float(m_health(game::gamemode, game::mutators, game::focus->actortype)));
                    if(circlebarhealthtone) skewcolour(c.r, c.g, c.b, circlebarhealthtone);
                    break;
                case 1:
                    if(!m_impulsemeter(game::gamemode, game::mutators)) continue;
                    val = 1-clamp(float(game::focus->impulse[IM_METER])/float(impulsemeter), 0.f, 1.f);
                    if(circlebarimpulsetone) skewcolour(c.r, c.g, c.b, circlebarimpulsetone);
                    break;
                case 2:
                {
                    if(!isweap(game::focus->weapselect)) continue;
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaptime[weap];
                    val = game::focus->ammo[weap]/float(W(weap, ammomax));
                    if(circlebarammotone || circlebarammocolour) skewcolour(c.r, c.g, c.b, circlebarammocolour ? W(weap, colour) : circlebarammotone);
                    if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
                    {
                        case W_S_RELOAD: case W_S_USE:
                            if(game::focus->weapload[weap] > 0)
                            {
                                val -= game::focus->weapload[weap]/float(W(weap, ammomax));
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
            gle::color(vec(c).mul(0.25f), hudblend*circlebarblend*0.65f);
            drawslice(pos, slice, x, y, s*circlebarsize);
            if(val > 0)
            {
                gle::color(c, fade);
                drawslice(pos, val*slice, x, y, s*circlebarsize);
            }
            float nps = pos+val*slice;
            val = 0;
            fade = hudblend*circlebarblend;
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
                            val = (game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1)/float(W(weap, ammomax))*amt;
                            break;
                        }
                        case W_S_RELOAD: case W_S_USE:
                        {
                            if(game::focus->weapload[weap] > 0)
                            {
                                int check = game::focus->weapwait[weap]/2;
                                if(interval >= check)
                                {
                                    float amt = clamp(float(interval-check)/float(check), 0.f, 1.f);
                                    fade *= amt;
                                    val = game::focus->weapload[weap]/float(W(weap, ammomax))*amt;
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
                    float total = 0;
                    loopv(damagelocs)
                    {
                        dhloc &d = damagelocs[i];
                        int millis = totalmillis-d.outtime, delay = min(20, d.damage)*50;
                        if(millis >= delay || d.dir.iszero()) { if(millis >= onscreendamagetime+onscreendamagefade) damagelocs.remove(i--); continue; }
                        gameent *e = game::getclient(d.clientnum);
                        if(!onscreendamageself && e == game::focus) continue;
                        float dam = d.damage/float(m_health(game::gamemode, game::mutators, game::focus->actortype)),
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

    void drawpointertex(const char *tex, int x, int y, int s, float r, float g, float b, float fade)
    {
        if(!tex || !*tex) return;
        Texture *t = textureload(tex, 3);
        if(t != notexture)
        {
            if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            else glBlendFunc(GL_ONE, GL_ONE);
            gle::colorf(r, g, b, fade);
            glBindTexture(GL_TEXTURE_2D, t->id);
            drawsized(x, y, s);
        }
    }

    void drawpointer(int w, int h, int index)
    {
        int cs = int((index == POINTER_UI ? cursorsize : crosshairsize)*hudsize);
        float fade = index == POINTER_UI ? cursorblend : crosshairblend;
        vec c(1, 1, 1);
        if(game::focus->state == CS_ALIVE && index >= POINTER_HAIR)
        {
            if(crosshairweapons&2) c = vec::fromcolor(W(game::focus->weapselect, colour));
            if(index == POINTER_ZOOM && game::inzoom())
            {
                int frame = lastmillis-game::lastzoom, off = int(zoomcrosshairsize*hudsize)-cs;
                float amt = frame <= W(game::focus->weapselect, cookzoom) ? clamp(float(frame)/float(W(game::focus->weapselect, cookzoom)), 0.f, 1.f) : 1.f;
                if(!game::zooming) amt = 1.f-amt;
                cs += int(off*amt);
                fade += (zoomcrosshairblend-fade)*amt;
            }
            if(crosshairtone) skewcolour(c.r, c.g, c.b, crosshairtone);
            int heal = m_health(game::gamemode, game::mutators, game::focus->actortype);
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
                float accskew = weapons::accmod(game::focus, physics::secondaryweap(game::focus))*crosshairaccamt;
                if(accskew > 0) fade /= accskew;
            }
        }
        int cx = int(hudwidth*cursorx), cy = int(hudheight*cursory);
        if(index != POINTER_UI)
        {
            drawpointertex(getpointer(index, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c.r, c.g, c.b, fade*hudblend);
            if(index > POINTER_UI)
            {
                if(showcirclebar) drawcirclebar(cx, cy, hudsize);
                if(game::focus->state == CS_ALIVE && game::focus->hasweap(game::focus->weapselect, m_weapon(game::focus->actortype, game::gamemode, game::mutators)))
                {
                    if(showclips) drawclip(game::focus->weapselect, cx, cy, hudsize);
                    if(showindicator) drawindicator(game::focus->weapselect, cx, cy, int(indicatorsize*hudsize), physics::secondaryweap(game::focus));
                }
                if(crosshairhitspeed && totalmillis-game::focus->lasthit <= crosshairhitspeed)
                {
                    vec c2(1, 1, 1);
                    if(hitcrosshairtone) skewcolour(c2.r, c2.g, c2.b, hitcrosshairtone);
                    else c2 = c;
                    drawpointertex(getpointer(POINTER_HIT, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c2.r, c2.g, c2.b, crosshairblend*hudblend);
                }
                if(crosshairdistance && game::focus->state == CS_EDITING) draw_textf("\fa%.1f\fwm", cx+crosshairdistancex, cy+crosshairdistancey, 0, 0, -1, -1, -1, int(hudblend*255), TEXT_RIGHT_JUSTIFY, -1, -1, 1, game::focus->o.dist(worldpos)/8.f);
            }
        }
        else drawpointertex(getpointer(index, game::focus->weapselect), cx, cy, cs, c.r, c.g, c.b, fade*hudblend);
    }

    void drawpointers(int w, int h)
    {
        int index = POINTER_NONE;
        if(hasinput()) index = hasinput(true) ? POINTER_UI : POINTER_NONE;
        else if(!showhud || !showcrosshair || game::focus->state == CS_DEAD || !gs_playing(game::gamestate) || client::waiting() || (game::thirdpersonview(true) && game::focus != game::player1))
            index = POINTER_NONE;
        else if(game::focus->state == CS_EDITING) index = POINTER_EDIT;
        else if(game::focus->state >= CS_SPECTATOR) index = POINTER_SPEC;
        else if(game::inzoom()) index = POINTER_ZOOM;
        else if(m_team(game::gamemode, game::mutators))
        {
            vec pos = game::focus->headpos();
            gameent *d = game::intersectclosest(pos, worldpos, game::focus);
            if(d && d->actortype < A_ENEMY && d->team == game::focus->team) index = POINTER_TEAM;
            else index = POINTER_HAIR;
        }
        else index = POINTER_HAIR;
        if(index > POINTER_NONE)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawpointer(w, h, index);
            glDisable(GL_BLEND);
        }
    }

    int numteamkills()
    {
        int numkilled = 0;
        loopvrev(teamkills)
        {
            if(totalmillis-teamkills[i] <= teamkilltime*60000) numkilled++;
            else teamkills.remove(i);
        }
        return numkilled;
    }

    bool showname()
    {
        if(game::focus != game::player1)
        {
            if(game::thirdpersonview(true) && game::aboveheadnames >= 2) return false;
            return true;
        }
        return false;
    }

    const char *specviewname()
    {
        if(showname()) return game::colourname(game::focus);
        if(game::tvmode())
        {
            if(game::spectvfollow >= 0)
            {
                gameent *d = game::getclient(game::spectvfollow);
                if(d) return game::colourname(d);
            }
            return "SpecTV";
        }
        return "Spectating";
    }

    void drawnotices()
    {
        pushhudscale(noticescale);
        int ty = int(((hudheight/2)+(hudheight/2*noticeoffset))/noticescale), tx = int((hudwidth/2)/noticescale),
            tf = int(hudblend*noticeblend*255), tr = 255, tg = 255, tb = 255,
            tw = int((hudwidth-((hudsize*edgesize)*2))/noticescale);
        if(noticetone) skewcolour(tr, tg, tb, noticetone);

        pushfont("emphasis");
        if(!gs_playing(game::gamestate) || totalmillis-game::mapstart <= noticetitle)
        {
            ty += draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), -1, -1, -1, tf, TEXT_CENTERED, -1, tw, 1, *maptitle ? maptitle : mapname);
            pushfont("reduced");
            if(*mapauthor) ty += draw_textf("by %s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), -1, -1, -1, tf, TEXT_CENTERED, -1, tw, 1, mapauthor);
            defformatstring(gname, "%s", server::gamename(game::gamemode, game::mutators, 0, 32));
            ty += draw_textf("[ \fs\fa%s\fS ]", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), -1, -1, -1, tf, TEXT_CENTERED, -1, tw, 1, gname);
            popfont();
            ty += FONTH/3;
        }
        if(client::demoplayback && showdemoplayback)
            ty += draw_textf("Demo playback in progress", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), -1, -1, -1, tf, TEXT_CENTERED, -1, tw, 1)+FONTH/3;
        popfont();

        pushfont("default");
        if(game::player1->quarantine)
        {
            ty += draw_textf("You are \fzoyQUARANTINED", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
            ty += draw_textf("Please await instructions from a moderator", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1)+FONTH/3;
        }
        else if(game::player1->state == CS_SPECTATOR)
            ty += draw_textf("[ %s ]", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, specviewname())+FONTH/3;
        else if(game::player1->state == CS_WAITING && showname())
            ty += draw_textf("[ %s ]", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, game::colourname(game::focus))+FONTH/3;

        if(gs_playing(game::gamestate))
        {
            gameent *target = game::player1->state != CS_SPECTATOR ? game::player1 : game::focus;
            if(target->state == CS_DEAD || target->state == CS_WAITING)
            {
                int delay = target->respawnwait(lastmillis, m_delay(target->actortype, game::gamemode, game::mutators, target->team));
                if(delay || m_duke(game::gamemode, game::mutators) || (m_play(game::gamemode) && maxalive > 0))
                {
                    if(gs_waiting(game::gamestate)) ty += draw_textf("Waiting for game to start", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                    else if(m_survivor(game::gamemode, game::mutators)) ty += draw_textf("Queued for new round", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                    else if(m_duel(game::gamemode, game::mutators))
                    {
                        switch(target->queuepos)
                        {
                            case -1:
                                if(game::gamestate == G_S_OVERTIME) ty += draw_textf("You lost, in sudden death overtime", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                else ty += draw_textf("Queued for new round", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                break;
                            case 0:
                                ty += draw_textf("You are \fs\fzcgNEXT\fS in the duel queue", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                break;
                            default:
                                ty += draw_textf("You are \fs\fy#\fS\fs\fc%d\fS in the duel queue", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target->queuepos);
                                break;
                        }
                    }
                    else if(delay) ty += draw_textf("%s: Down for \fs\fy%s\fS", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target == game::player1 && target->state == CS_WAITING ? "Please Wait" : "Fragged", timestr(delay));
                    else if(target == game::player1 && target->state == CS_WAITING && m_play(game::gamemode) && maxalive > 0 && maxalivequeue)
                    {
                        switch(target->queuepos)
                        {
                            case -1:
                                ty += draw_textf("Queued for new round", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                break;
                            case 0:
                                ty += draw_textf("You are \fs\fzcgNEXT\fS in the spawn queue", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                break;
                            default:
                                ty += draw_textf("You are \fs\fy#\fS\fs\fc%d\fS in the spawn queue", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target->queuepos);
                                break;
                        }
                    }
                    if(target == game::player1 && target->state != CS_WAITING && shownotices >= 2 && lastmillis-target->lastdeath >= 500)
                    {
                        pushfont("little");
                        ty += draw_textf("Press \fs\fw\f{=primary}\fS to enter respawn queue", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                        popfont();
                    }
                }
                else
                {
                    ty += draw_textf("Ready to respawn", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                    if(target == game::player1 && target->state != CS_WAITING && shownotices >= 2)
                    {
                        pushfont("little");
                        ty += draw_textf("Press \fs\fw\f{=primary}\fS to respawn now", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                        popfont();
                    }
                }
                if(obitnotices && target->lastdeath && (target->state == CS_WAITING || target->state == CS_DEAD) && *target->obit)
                {
                    pushfont("reduced");
                    ty += draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target->obit);
                    popfont();
                }
                if(shownotices >= 2)
                {
                    if(target == game::player1 && !client::demoplayback)
                    {
                        if(target->state == CS_WAITING && shownotices >= 2)
                        {
                            pushfont("little");
                            ty += draw_textf("Press \fs\fw\f{=3:waitmodeswitch}\fS to %s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, game::tvmode() ? "interact" : "switch to TV");
                            popfont();
                        }
                        if(m_loadout(game::gamemode, game::mutators))
                        {
                            pushfont("little");
                            ty += draw_textf("Press \fs\fw\f{=%s profile}\fS to \fs%s\fS loadout", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, UI::uiopencmd, target->loadweap.empty() ? "\fzoyselect" : "change");
                            popfont();
                        }
                        if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators))
                        {
                            pushfont("little");
                            ty += draw_textf("Press \fs\fw\f{=%s team}\fS to change teams", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, UI::uiopencmd);
                            popfont();
                        }
                    }
                }
            }
            else if(target->state == CS_ALIVE)
            {
                if(obitnotices && totalmillis-target->lastkill <= noticetime && *target->obit)
                {
                    pushfont("reduced");
                    ty += draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target->obit);
                    popfont();
                }
                if(target == game::player1 && shownotices >= 2 && game::allowmove(target))
                {
                    pushfont("emphasis");
                    static vector<actitem> actitems;
                    actitems.setsize(0);
                    vec pos = target->center();
                    float radius = max(target->height*0.5f, max(target->xradius, target->yradius));
                    if(entities::collateitems(target, pos, radius, actitems))
                    {
                        while(!actitems.empty())
                        {
                            actitem &t = actitems.last();
                            int ent = -1;
                            switch(t.type)
                            {
                                case actitem::ENT:
                                {
                                    if(!entities::ents.inrange(t.target)) break;
                                    ent = t.target;
                                    break;
                                }
                                case actitem::PROJ:
                                {
                                    if(!projs::projs.inrange(t.target)) break;
                                    projent &proj = *projs::projs[t.target];
                                    ent = proj.id;
                                    break;
                                }
                                default: break;
                            }
                            if(entities::ents.inrange(ent))
                            {
                                extentity &e = *entities::ents[ent];
                                if(enttype[e.type].usetype == EU_ITEM && e.type == WEAPON)
                                {
                                    int sweap = m_weapon(target->actortype, game::gamemode, game::mutators), attr = w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], sweap);
                                    if(isweap(attr) && target->canuse(e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD)) && weapons::canuse(attr))
                                    {
                                        int drop = -1;
                                        if(m_classic(game::gamemode, game::mutators) && target->ammo[attr] < 0 && w_carry(attr, sweap) && target->carry(sweap) >= AA(target->actortype, maxcarry))
                                            drop = target->drop(sweap);
                                        if(isweap(drop))
                                        {
                                            static struct dropattrs : attrvector { dropattrs() { add(0, 5); } } attrs;
                                            attrs[0] = drop;
                                            defformatstring(dropweap, "%s", entities::entinfo(WEAPON, attrs, false, true));
                                            ty += draw_textf("Press \fs\fw\f{=use}\fS to swap \fs%s\fS for \fs%s\fS", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, dropweap, entities::entinfo(e.type, e.attrs, false, true));
                                        }
                                        else ty += draw_textf("Press \fs\fw\f{=use}\fS to pickup \fs%s\fS", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, entities::entinfo(e.type, e.attrs, false, true));
                                        break;
                                    }
                                }
                                else if(e.type == TRIGGER && e.attrs[2] == TA_ACTION)
                                {
                                    ty += draw_textf("Press \fs\fw\f{=use}\fS to interact", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                                    break;
                                }
                            }
                            actitems.pop();
                        }
                    }
                    popfont();
                    if(shownotices >= 4)
                    {
                        pushfont("little");
                        if(target->canshoot(target->weapselect, 0, m_weapon(target->actortype, game::gamemode, game::mutators), lastmillis, (1<<W_S_RELOAD)))
                            ty += draw_textf("Press \fs\fw\f{=primary}\fS to attack", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                        if(target->canshoot(target->weapselect, HIT_ALT, m_weapon(target->actortype, game::gamemode, game::mutators), lastmillis, (1<<W_S_RELOAD)))
                            ty += draw_textf("Press \fs\fw\f{=secondary}\fS to %s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, W2(target->weapselect, cooked, true)&W_C_ZOOM ? "zoom" : "alt-attack");
                        if(target->canreload(target->weapselect, m_weapon(target->actortype, game::gamemode, game::mutators), false, lastmillis))
                            ty += draw_textf("Press \fs\fw\f{=reload}\fS to reload ammo", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                        popfont();
                    }
                }
            }

            if(game::player1->state == CS_SPECTATOR)
            {
                pushfont("little");
                if(!client::demoplayback)
                {
                    ty += draw_textf("Press \fs\fw\f{=1:spectate 0}\fS to join the game", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                    if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators) && shownotices >= 2)
                        ty += draw_textf("Press \fs\fw\f{=1:%s team}\fS to join a team", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, UI::uiopencmd);
                }
                if(!m_edit(game::gamemode) && shownotices >= 2)
                    ty += draw_textf("Press \fs\fw\f{=1:specmodeswitch}\fS to %s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, game::tvmode() ? "interact" : "switch to TV");
                popfont();
            }

            if(m_edit(game::gamemode) && (game::focus->state != CS_EDITING || shownotices >= 4) && !client::demoplayback)
            {
                pushfont("little");
                ty += draw_textf("Press \fs\fw\f{=1:edittoggle}\fS to %s editmode", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, game::focus->state != CS_EDITING ? "enter" : "exit");
                popfont();
            }

            if(m_capture(game::gamemode)) capture::drawnotices(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
            else if(m_defend(game::gamemode)) defend::drawnotices(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
            else if(m_bomber(game::gamemode)) bomber::drawnotices(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        }
        popfont();
        pophudmatrix();
    }

    void drawevents(float blend)
    {
        pushhudscale(eventscale);
        int ty = int(((hudheight/2)-(hudheight/2*eventoffset))/eventscale), tx = int((hudwidth/2)/eventscale),
            tf = int(hudblend*eventblend*255), tr = 255, tg = 255, tb = 255,
            tw = int((hudwidth-((hudsize*edgesize)*2))/eventscale);
        if(eventtone) skewcolour(tr, tg, tb, eventtone);
        pushfont("emphasis");
        if(!gs_playing(game::gamestate))
            ty -= draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), -1, -1, -1, tf, TEXT_CENTERED, -1, tw, 1, gamestates[3][game::gamestate])+FONTH/3;
        else
        {
            bool tkwarn = hastkwarn(game::focus), tinfo = hasteaminfo(game::focus);
            if(tkwarn || tinfo)
            {
                const char *col = teamnotices >= 2 ? "\fs\fzyS" : "";
                if(tkwarn) ty -= draw_textf("\fzryDo NOT shoot team-mates", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1)+FONTH/4;
                if(m_race(game::gamemode)) ty -= draw_textf("%sRace", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, col)+FONTH/4;
                else if(!m_team(game::gamemode, game::mutators)) ty -= draw_textf("%sFree-for-all %s", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, col, m_bomber(game::gamemode) ? "Bomber-ball" : "Deathmatch")+FONTH/4;
                else ty -= draw_textf("%sYou are on team %s", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, col, game::colourteam(game::focus->team))+FONTH/4;
            }
        }
        if(m_capture(game::gamemode)) capture::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        else if(m_defend(game::gamemode)) defend::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        else if(m_bomber(game::gamemode)) bomber::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        if(showeventicons && game::focus->state != CS_EDITING && game::focus->state != CS_SPECTATOR) loopv(game::focus->icons)
        {
            if(game::focus->icons[i].type == eventicon::AFFINITY && !(showeventicons&2)) continue;
            if(game::focus->icons[i].type == eventicon::WEAPON && !(showeventicons&4)) continue;
            int millis = totalmillis-game::focus->icons[i].millis;
            if(millis <= game::focus->icons[i].fade)
            {
                Texture *t = textureload(icontex(game::focus->icons[i].type, game::focus->icons[i].value));
                if(t && t != notexture)
                {
                    int olen = min(game::focus->icons[i].length/5, 1000), ilen = olen/2, colour = colourwhite;
                    float skew = millis < ilen ? millis/float(ilen) : (millis > game::focus->icons[i].fade-olen ? (game::focus->icons[i].fade-millis)/float(olen) : 1.f),
                          fade = blend*eventblend*skew;
                    int size = int(FONTH*skew*eventiconscale), width = int((t->w/float(t->h))*size), rsize = game::focus->icons[i].type < eventicon::SORTED ? int(size*2/3) : int(size);
                    switch(game::focus->icons[i].type)
                    {
                        case eventicon::WEAPON: colour = W(game::focus->icons[i].value, colour); break;
                        case eventicon::AFFINITY: colour = m_bomber(game::gamemode) ? pulsecols[PULSE_DISCO][clamp((totalmillis/100)%PULSECOLOURS, 0, PULSECOLOURS-1)] : TEAM(game::focus->icons[i].value, colour); break;
                        default: break;
                    }
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    gle::color(vec::fromcolor(colour), fade);
                    drawtexture(tx-width/2, ty-rsize/2, width, size);
                    ty -= rsize;
                }
            }
        }
        pophudmatrix();
    }

    float radarlimit(float dist) { return dist >= 0 && radardistlimit > 0 ? clamp(dist, 0.f, radardistlimit) : max(dist, 0.f); }
    ICOMMAND(0, getradarlimit, "f", (float *n), floatret(radarlimit(*n)));

    bool radarlimited(float dist) { return radardistlimit > 0 && dist > radardistlimit; }
    ICOMMAND(0, getradarlimited, "f", (float *n), intret(radarlimited(*n) ? 1 : 0));

    const char *teamtexname(int team)
    {
        const char *teamtexs[T_MAX] = { teamtex, teamalphatex, teamomegatex, teamkappatex, teamsigmatex, teamtex };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *privtex(int priv, int actortype)
    {
        if(actortype != A_PLAYER) return privbottex;
        const char *privtexs[2][PRIV_MAX] = {
            { privnonetex, privplayertex, privsupportertex, privmoderatortex, privoperatortex, privadministratortex, privdevelopertex, privfoundertex },
            { privnonetex, privplayertex, privlocalsupportertex, privlocalmoderatortex, privlocaloperatortex, privlocaladministratortex, privnonetex, privnonetex }
        };
        return privtexs[priv&PRIV_LOCAL ? 1 : 0][clamp(priv&PRIV_TYPE, 0, int(priv&PRIV_LOCAL ? PRIV_ADMINISTRATOR : PRIV_LAST))];
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
                    clawtex, pistoltex, swordtex, shotguntex, smgtex, flamertex, plasmatex, zappertex, rifletex, grenadetex, minetex, rockettex, ""
                };
                return isweap(stype) && *weaptexs[stype] ? weaptexs[stype] : questiontex;
                break;
            }
            default: break;
        }
        return "";
    }

    const char *icontex(int type, int value)
    {
        switch(type)
        {
            case eventicon::SPREE:
            {
                switch(value)
                {
                    case 0: return spree1tex; break;
                    case 1: return spree2tex; break;
                    case 2: return spree3tex; break;
                    case 3: default: return spree4tex; break;
                }
                break;
            }
            case eventicon::MULTIKILL:
            {
                switch(value)
                {
                    case 0: return multi1tex; break;
                    case 1: return multi2tex; break;
                    case 2: default: return multi3tex; break;
                }
                break;
            }
            case eventicon::HEADSHOT: return headshottex; break;
            case eventicon::DOMINATE: return dominatetex; break;
            case eventicon::REVENGE: return revengetex; break;
            case eventicon::FIRSTBLOOD: return firstbloodtex; break;
            case eventicon::BREAKER: return breakertex; break;
            case eventicon::WEAPON: return itemtex(WEAPON, value);
            case eventicon::AFFINITY:
            {
                if(m_bomber(game::gamemode)) return bombtex;
                if(m_defend(game::gamemode)) return pointtex;
                return flagtex;
            }
        }
        return "";
    }

    void drawdamage(int w, int h, float blend)
    {
        if(*damagetex)
        {
            float pc = game::focus->state == CS_DEAD ? 0.5f : (game::focus->state == CS_ALIVE ? min(damageresidue, 100)/100.f : 0.f);
            if(pc > 0)
            {
                Texture *t = textureload(damagetex, 3);
                if(t && t != notexture)
                {
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    gle::colorf(0.85f, 0.09f, 0.09f, pc*blend*damageblend);
                    drawtexture(0, 0, w, h);
                }
            }
        }
    }

    void drawfire(int w, int h, float blend)
    {
        if(*burntex && game::focus->burning(lastmillis, burntime))
        {
            Texture *t = textureload(burntex, 3);
            if(t && t != notexture)
            {
                int interval = lastmillis-game::focus->lastres[WR_BURN];
                float pc = interval >= burntime-500 ? 1.f+(interval-(burntime-500))/500.f : (interval%burndelay)/float(burndelay/2); if(pc > 1.f) pc = 2.f-pc;
                glBindTexture(GL_TEXTURE_2D, t->id);
                gle::colorf(0.9f*max(pc,0.5f), 0.3f*pc, 0.0625f*max(pc,0.25f), blend*burnblend*(interval >= burntime-(burndelay/2) ? pc : min(pc+0.5f, 1.f)));
                drawtexture(0, 0, w, h);
            }
        }
    }

    void drawzoom(int w, int h)
    {
        Texture *t = textureload(zoomtex, 3);
        int frame = lastmillis-game::lastzoom;
        float pc = frame <= W(game::focus->weapselect, cookzoom) ? float(frame)/float(W(game::focus->weapselect, cookzoom)) : 1.f;
        if(!game::zooming) pc = 1.f-pc;
        int x = 0, y = 0, c = 0;
        if(w > h)
        {
            float rc = 1.f-pc;
            c = h; x += (w-h)/2;
            usetexturing(false);
            drawblend(0, 0, x, c, rc, rc, rc, true);
            drawblend(x+c, 0, x+1, c, rc, rc, rc, true);
            usetexturing(true);
        }
        else if(h > w)
        {
            float rc = 1.f-pc;
            c = w; y += (h-w)/2;
            usetexturing(false);
            drawblend(0, 0, c, y, rc, rc, rc, true);
            drawblend(0, y+c, c, y, rc, rc, rc, true);
            usetexturing(true);
        }
        else c = h;
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::colorf(1.f, 1.f, 1.f, pc);
        drawtexture(x, y, c, c);
    }

    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundwatertex, "<grey><noswizzle>textures/water", 0x300);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundcausttex, "<grey><noswizzle>caustics/caust00", 0x300);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundcloudtex, "<grey><noswizzle>torley/desat/cloudyformations_z", 0x300);

    void drawbackground(int w, int h)
    {
        gle::colorf(1, 1, 1, 1);

        Texture *t = NULL;
        if(showloadingmapbg && *mapname && strcmp(mapname, "maps/untitled"))
        {
            defformatstring(tex, "<blur:2>%s", mapname);
            t = textureload(tex, 3, true, false);
        }
        if(!t || t == notexture)
        {
            (hudbackgroundshader ? hudbackgroundshader : nullshader)->set();

            pushhudmatrix();
            hudmatrix.ortho(-1, 1, -1, 1, -1, 1);
            flushhudmatrix();

            LOCALPARAMF(time, lastmillis / 1000.0f);
            glActiveTexture_(GL_TEXTURE0);
            settexture(backgroundwatertex, 0x300);
            glActiveTexture_(GL_TEXTURE1);
            settexture(backgroundcausttex, 0x300);
            glActiveTexture_(GL_TEXTURE2);
            settexture(backgroundcloudtex, 0x300);
            glActiveTexture_(GL_TEXTURE0);
            drawquad(-1, -1, 2, 2, 0, 0, 1, 1);
            pophudmatrix();
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, t->id);
            float offsetx = 0, offsety = 0;
            if(showloadingaspect&(1<<showloadingmapbg))
            {
                if(w > h) offsety = ((w-h)/float(w))*0.5f;
                else if(h > w) offsetx = ((h-w)/float(h))*0.5f;
            }
            drawquad(0, 0, w, h, offsetx, offsety, 1-offsetx, 1-offsety);
        }

        if(showloadinglogos)
        {
            gle::colorf(1, 1, 1, 1);

            t = textureload(logotex, 3);
            glBindTexture(GL_TEXTURE_2D, t->id);
            drawtexture(w-1024, 0, 1024, 512);
        }

        if(!engineready)
        {
            pushfont("small");
            draw_textf("%s", FONTH*3/4, h-FONTH/2, 0, 0, 255, 255, 255, 255, TEXT_LEFT_UP, -1, -1, 1, *progresstitle ? progresstitle : "Loading, please wait..");
            popfont();
        }
    }

    ICOMMAND(0, getprogresstitle, "", (),
    {
        if(progressing) result(progresstitle);
        else
        {
            int wait = client::waiting();
            switch(wait)
            {
                case 0: break;
                case 1:
                    if(curpeer || haslocalclients())
                    {
                        if(!client::isready) result("Negotiating with server..");
                        else if(!client::loadedmap) result("Getting game information..");
                        else result("Loading game state..");
                    }
                    else if(connpeer != NULL) result("Connecting to server..");
                    else result("Loading game state..");
                    break;
                case 2:
                    result("Requesting map..");
                    break;
                case 3:
                    result("Downloading map..");
                    break;
                default: break;
            }
        }
    });

    void drawonscreenhits(int w, int h, float blend)
    {
        pushfont("tiny");
        pushhudscale(onscreenhitsscale);
        float maxy = -1.f;
        loopv(hitlocs)
        {
            dhloc &d = hitlocs[i];
            int millis = totalmillis-d.outtime;
            if(millis >= onscreenhitstime+onscreenhitsfade || d.dir.iszero()) { hitlocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            gameent *a = game::getclient(d.clientnum);
            if((!onscreenhitsheal && d.damage < 0) || (!onscreenhitsself && a == game::focus)) continue;
            vec o = onscreenhitsfollow && a ? a->center() : d.dir;
            o.z += actor[a ? a->actortype : A_PLAYER].height*onscreenhitsheight;
            float cx = 0, cy = 0, cz = 0;
            if(!vectocursor(o, cx, cy, cz)) continue;
            float hx = cx*w/onscreenhitsscale, hy = cy*h/onscreenhitsscale, fade = blend*onscreenhitsblend;
            if(onscreenhitsoffset != 0) hx += FONTW*onscreenhitsoffset;
            if(millis <= onscreenhitstime)
            {
                float amt = millis/float(onscreenhitstime), total = FONTW*onscreenhitsswipe*(1-amt);
                if(onscreenhitsoffset < 0) hx -= total;
                else hx += total;
                fade *= amt;
            }
            else
            {
                int offset = millis-onscreenhitstime;
                hy -= FONTH*offset/float(onscreenhitstime);
                fade *= 1-(offset/float(onscreenhitsfade));
            }
            defformatstring(text, "%c%d", d.damage > 0 ? '-' : (d.damage < 0 ? '+' : '~'), d.damage < 0 ? 0-d.damage : d.damage);
            vec colour = d.colour < 0 ? game::rescolour(a, INVPULSE(d.colour)) : vec::fromcolor(d.colour);
            if(maxy >= 0 && hy < maxy) hy = maxy;
            if(onscreenhitsglow && settexture(onscreenhitsglowtex))
            {
                float width = 0, height = 0;
                text_boundsf(text, width, height, 0, 0, -1, TEXT_CENTERED, 1);
                gle::colorf(colour.r*onscreenhitsglowcolour, colour.g*onscreenhitsglowcolour, colour.b*onscreenhitsglowcolour, fade*onscreenhitsglowblend);
                drawtexture(hx-(width*onscreenhitsglowscale*0.5f), hy-(height*onscreenhitsglowscale*0.25f), width*onscreenhitsglowscale, height*onscreenhitsglowscale);
            }
            hy += draw_textf("%s", hx, hy, 0, 0, int(colour.r*255), int(colour.g*255), int(colour.b*255), int(fade*255), TEXT_CENTERED, -1, -1, 1, text)/onscreenhitsscale;
            if(maxy < 0 || hy > maxy) maxy = hy;
        }
        pophudmatrix();
        popfont();
    }

    void drawonscreendamage(int w, int h, float blend)
    {
        loopv(damagelocs)
        {
            dhloc &d = damagelocs[i];
            int millis = totalmillis-d.outtime;
            if(millis >= onscreendamagetime+onscreendamagefade || d.dir.iszero()) { if(millis >= min(20, d.damage)*50) damagelocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            gameent *e = game::getclient(d.clientnum);
            if(!onscreendamageself && e == game::focus) continue;
            float amt = millis >= onscreendamagetime ? 1.f-(float(millis-onscreendamagetime)/float(onscreendamagefade)) : float(millis)/float(onscreendamagetime),
                range = clamp(max(d.damage, onscreendamagemin)/float(max(onscreendamagemax-onscreendamagemin, 1)), onscreendamagemin/100.f, 1.f),
                fade = clamp(onscreendamageblend*blend, min(onscreendamageblend*onscreendamagemin/100.f, 1.f), onscreendamageblend)*amt,
                size = clamp(range*onscreendamagesize, min(onscreendamagesize*onscreendamagemin/100.f, 1.f), onscreendamagesize)*amt;
            vec dir = d.dir, colour = d.colour < 0 ? game::rescolour(game::focus, INVPULSE(d.colour)) : vec::fromcolor(d.colour);
            if(e == game::focus) d.dir = vec(e->yaw*RAD, 0.f).neg();
            dir.rotate_around_z(-camera1->yaw*RAD).normalize();
            float yaw = -atan2(dir.x, dir.y)/RAD, x = sinf(RAD*yaw), y = -cosf(RAD*yaw), sz = max(w, h)/2,
                  ts = sz*onscreendamagescale, tp = ts*size, tq = tp*onscreendamageblipsize, tr = ts*onscreendamageoffset, lx = (tr*x)+w/2, ly = (tr*y)+h/2;
            gle::color(colour, fade);
            Texture *t = textureload(hurttex, 3);
            if(t != notexture)
            {
                glBindTexture(GL_TEXTURE_2D, t->id);
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_TRIANGLE_STRIP);
                vec2 o(lx, ly);
                loopk(4)
                {
                    vec2 norm, tc;
                    switch(k)
                    {
                        case 0: vecfromyaw(yaw, 1, -1, norm);   tc = vec2(0, 1); break;
                        case 1: vecfromyaw(yaw, 1, 1, norm);    tc = vec2(1, 1); break;
                        case 2: vecfromyaw(yaw, -1, -1, norm);  tc = vec2(0, 0); break;
                        case 3: vecfromyaw(yaw, -1, 1, norm);   tc = vec2(1, 0); break;
                    }
                    norm.mul(tq).add(o);
                    gle::attrib(norm);
                    gle::attrib(tc);
                }
                gle::end();
            }
        }
    }

    void render(bool noview)
    {
        hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
        flushhudmatrix();
        float fade = hudblend;
        if(!progressing)
        {
            vec colour = vec(1, 1, 1);
            if(commandfade && (commandmillis > 0 || totalmillis-abs(commandmillis) <= commandfade))
            {
                float a = min(float(totalmillis-abs(commandmillis))/float(commandfade), 1.f)*commandfadeamt;
                if(commandmillis > 0) a = 1.f-a;
                else a += (1.f-commandfadeamt);
                loopi(3) if(a < colour[i]) colour[i] *= a;
            }
            if(compassfade && (compassmillis > 0 || totalmillis-abs(compassmillis) <= compassfade))
            {
                float a = min(float(totalmillis-abs(compassmillis))/float(compassfade), 1.f)*compassfadeamt;
                if(compassmillis > 0) a = 1.f-a;
                else a += (1.f-compassfadeamt);
                loopi(3) if(a < colour[i]) colour[i] *= a;
            }
            if(UI::hasmenu(false) ? uimillis <= 0 : uimillis >= 0) uimillis = UI::hasmenu(false) ? totalmillis : -totalmillis;
            if(uifade && (uimillis > 0 || totalmillis-abs(uimillis) <= uifade))
            {
                float n = min(float(totalmillis-abs(uimillis))/float(uifade), 1.f), a = n*uifadeamt;
                if(uimillis > 0) a = 1.f-a;
                else a += (1.f-uifadeamt);
                loopi(3) if(a < colour[i]) colour[i] *= a;
            }
            if(!noview)
            {
                if(titlefade && (client::waiting() || !game::mapstart || totalmillis-game::mapstart <= titlefade))
                {
                    float a = !client::waiting() || !game::mapstart ? float(totalmillis-game::mapstart)/float(titlefade) : 0.f;
                    loopi(3) if(a < colour[i]) colour[i] *= a;
                }
                if(tvmodefade && game::tvmode())
                {
                    float a = game::lasttvchg ? (totalmillis-game::lasttvchg <= tvmodefade ? float(totalmillis-game::lasttvchg)/float(tvmodefade) : 1.f) : 0.f;
                    loopi(3) if(a < colour[i]) colour[i] *= a;
                }
                if(game::focus == game::player1 || !game::thirdpersonview(true))
                {
                    if(spawnfade && game::focus->state == CS_ALIVE && game::focus->lastspawn && lastmillis-game::focus->lastspawn <= spawnfade)
                    {
                        float a = (lastmillis-game::focus->lastspawn)/float(spawnfade/3);
                        if(a < 3.f)
                        {
                            vec col = vec(1, 1, 1);
                            skewcolour(col.x, col.y, col.z, game::getcolour(game::focus, game::playereffecttone, game::playereffecttonelevel));
                            if(a < 1.f) { loopi(3) col[i] *= a; }
                            else { a = (a-1.f)*0.5f; loopi(3) col[i] += (1.f-col[i])*a; }
                            loopi(3) if(col[i] < colour[i]) colour[i] *= col[i];
                        }
                    }
                    if(showdamage >= 2 && damageresidue > 0)
                    {
                        float pc = min(damageresidue, 100)/100.f*damageskew;
                        loopi(2) if(colour[i+1] > 0) colour[i+1] -= colour[i+1]*pc;
                    }
                }
            }
            if(colour.x < 1 || colour.y < 1 || colour.z < 1)
            {
                usetexturing(false);
                drawblend(0, 0, hudwidth, hudheight, colour.x, colour.y, colour.z);
                usetexturing(true);
                fade *= (colour.x+colour.y+colour.z)/3.f;
            }
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        resethudshader();
        if(noview) drawbackground(hudwidth, hudheight);
        else if(!client::waiting())
        {
            if(showhud)
            {
                if(gs_playing(game::gamestate))
                {
                    bool third = game::thirdpersonview(true) && game::focus != game::player1;
                    if(game::focus->state == CS_ALIVE && game::inzoom()) drawzoom(hudwidth, hudheight);
                    if(showdamage && !third)
                    {
                        if(burntime && game::focus->state == CS_ALIVE) drawfire(hudwidth, hudheight, fade);
                        drawdamage(hudwidth, hudheight, fade);
                    }
                    if(teamhurthud&2 && teamhurttime && m_team(game::gamemode, game::mutators) && game::focus == game::player1 && game::player1->lastteamhit >= 0 && totalmillis-game::player1->lastteamhit <= teamhurttime)
                    {
                        vec targ;
                        bool hasbound = false;
                        int dist = teamhurtdist ? teamhurtdist : worldsize;
                        loopv(game::players) if(game::players[i] && game::players[i]->team == game::player1->team)
                        {
                            if(game::players[i]->lastteamhit < 0 || lastmillis-game::players[i]->lastteamhit > teamhurttime) continue;
                            if(!getsight(camera1->o, camera1->yaw, camera1->pitch, game::players[i]->o, targ, dist, curfov, fovy)) continue;
                            if(!hasbound)
                            {
                                Texture *t = textureload(warningtex, 3);
                                glBindTexture(GL_TEXTURE_2D, t->id);
                                float amt = float(totalmillis%250)/250.f, value = (amt > 0.5f ? 1.f-amt : amt)*2.f;
                                gle::colorf(value, value*0.125f, value*0.125f, value);
                                hasbound = true;
                            }
                            float cx = 0.5f, cy = 0.5f, cz = 1;
                            if(vectocursor(game::players[i]->o, cx, cy, cz))
                            {
                                int s = int(teamhurtsize*hudwidth), sx = int(cx*hudwidth-s), sy = int(cy*hudheight-s);
                                drawsized(sx, sy, s*2);
                            }
                        }
                    }
                    if(!hasinput(true))
                    {
                        if(onscreenhits) drawonscreenhits(hudwidth, hudheight, fade);
                        if(onscreendamage) drawonscreendamage(hudwidth, hudheight, fade);
                        if(m_capture(game::gamemode)) capture::drawonscreen(hudwidth, hudheight, fade);
                        else if(m_defend(game::gamemode)) defend::drawonscreen(hudwidth, hudheight, fade);
                        else if(m_bomber(game::gamemode)) bomber::drawonscreen(hudwidth, hudheight, fade);
                    }
                }
                if(showevents && !texpaneltimer && !game::tvmode() && !client::waiting() && !hasinput(false)) drawevents(fade);
            }
            else if(gs_playing(game::gamestate) && game::focus->state == CS_ALIVE && game::inzoom())
                drawzoom(hudwidth, hudheight);
        }
        if(!progressing && showhud)
        {
            if(commandmillis <= 0 && curcompass) rendercmenu();
            else if(shownotices && !client::waiting() && !hasinput(false) && !texpaneltimer) drawnotices();
        }
        if(progressing || !curcompass) UI::render();
        if(!progressing)
        {
            hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
            flushhudmatrix();
            drawpointers(hudwidth, hudheight);
            rendertexturepanel(hudwidth, hudheight);
        }
        glDisable(GL_BLEND);
    }

    void update(int w, int h)
    {
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
    }

    void cleanup()
    {
        teamkills.shrink(0);
        damagelocs.shrink(0);
        hitlocs.shrink(0);
        damageresidue = lastteam = 0;
    }
}
