#include "game.h"
namespace hud
{
    const int NUMSTATS = 11;
    int damageresidue = 0, hudwidth = 0, hudheight = 0, lastteam = 0, laststats = 0, prevstats[NUMSTATS] = {0}, curstats[NUMSTATS] = {0};

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
    VAR(IDF_PERSIST, hudminimal, 0, 0, 1);

    VAR(IDF_PERSIST, showdemoplayback, 0, 1, 1);
    FVAR(IDF_PERSIST, edgesize, 0, 0.005f, 1000);

    VAR(IDF_PERSIST, showconsole, 0, 2, 2);
    VAR(IDF_PERSIST, shownotices, 0, 3, 4);
    VAR(IDF_PERSIST, showevents, 0, 3, 4);
    VAR(IDF_PERSIST, showloadingaspect, 0, 2, 3);
    VAR(IDF_PERSIST, showloadingmapbg, 0, 1, 1);
    VAR(IDF_PERSIST, showloadinggpu, 0, 1, 1);
    VAR(IDF_PERSIST, showloadingversion, 0, 1, 1);
    VAR(IDF_PERSIST, showloadingurl, 0, 1, 1);

    VAR(IDF_PERSIST, showfps, 0, 0, 3);
    VAR(IDF_PERSIST, showstats, 0, 1, 2);
    VAR(IDF_PERSIST, statrate, 0, 200, 1000);
    FVAR(IDF_PERSIST, statblend, 0, 1, 1);

    bool fullconsole = false;
    void toggleconsole() { fullconsole = !fullconsole; }
    COMMAND(0, toggleconsole, "");

    VAR(IDF_PERSIST, titlefade, 0, 1000, 10000);
    VAR(IDF_PERSIST, tvmodefade, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, spawnfade, 0, 250, VAR_MAX);

    VAR(IDF_PERSIST, commandfade, 0, 250, VAR_MAX);
    FVAR(IDF_PERSIST, commandfadeamt, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, commandfadeskew, 0, 0, 1);
    FVAR(IDF_PERSIST, commandscale, FVAR_NONZERO, 1, FVAR_MAX);
    VAR(IDF_PERSIST, uifade, 0, 250, VAR_MAX);
    FVAR(IDF_PERSIST, uifadeamt, 0, 0.5f, 1);

    int conskip = 0;
    void setconskip(int *n)
    {
        conskip += *n;
        if(conskip < 0) conskip = 0;
        else if(conskip >= conlines.length()) conskip = conlines.length()-1;
    }
    COMMANDN(0, conskip, setconskip, "i");

    VAR(IDF_PERSIST, consize, 1, 5, 100);
    VAR(IDF_PERSIST, contime, 0, 30000, VAR_MAX);
    VAR(IDF_PERSIST, confade, 0, 500, VAR_MAX);
    VAR(IDF_PERSIST, conoverflow, 0, 5, VAR_MAX);
    VAR(IDF_PERSIST, concenter, 0, 0, 1);
    VAR(IDF_PERSIST, confilter, 0, 1, 1);
    VAR(IDF_PERSIST, condate, 0, 0, 1);
    FVAR(IDF_PERSIST, conblend, 0, 0.6f, 1);
    FVAR(IDF_PERSIST, conscale, FVAR_NONZERO, 1, FVAR_MAX);
    SVAR(IDF_PERSIST, condateformat, "%H:%M:%S");
    VAR(IDF_PERSIST, chatconsize, 1, 5, 100);
    VAR(IDF_PERSIST, chatcontime, 0, 30000, VAR_MAX);
    VAR(IDF_PERSIST, chatconfade, 0, 1000, VAR_MAX);
    VAR(IDF_PERSIST, chatconoverflow, 0, 5, VAR_MAX);
    VAR(IDF_PERSIST, chatcondate, 0, 0, 1);
    FVAR(IDF_PERSIST, chatconblend, 0, 1, 1);
    FVAR(IDF_PERSIST, chatconscale, FVAR_NONZERO, 1, FVAR_MAX);
    SVAR(IDF_PERSIST, chatcondateformat, "%H:%M:%S");

    FVAR(IDF_PERSIST, selfconblend, 0, 1, 1);
    FVAR(IDF_PERSIST, fullconblend, 0, 1, 1);

    VAR(IDF_PERSIST, conskipwarn, 0, 1, 1);
    VAR(IDF_PERSIST, capslockwarn, 0, 1, 1);

    FVAR(IDF_PERSIST, noticeoffset, -1, 0.35f, 1);
    FVAR(IDF_PERSIST, noticeblend, 0, 1, 1);
    FVAR(IDF_PERSIST, noticescale, 1e-4f, 1, 1000);
    FVAR(IDF_PERSIST, noticepadx, FVAR_MIN, 0, FVAR_MAX);
    FVAR(IDF_PERSIST, noticepady, FVAR_MIN, 0, FVAR_MAX);
    VAR(IDF_PERSIST, noticetitle, 0, 10000, 60000);
    FVAR(IDF_PERSIST, eventoffset, -1, 0.61f, 1);
    FVAR(IDF_PERSIST, eventblend, 0, 1, 1);
    FVAR(IDF_PERSIST, eventscale, 1e-4f, 1, 1000);
    FVAR(IDF_PERSIST, eventpadx, FVAR_MIN, 0.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, eventpady, FVAR_MIN, 0.125f, FVAR_MAX);
    VAR(IDF_PERSIST, noticetime, 0, 5000, VAR_MAX);
    VAR(IDF_PERSIST, obitnotices, 0, 2, 2);
    VAR(IDF_PERSIST, teamnotices, 0, 2, 2);
    VAR(IDF_PERSIST, teamnoticedelay, 0, 5000, VAR_MAX);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamtex, "<grey>textures/team", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamalphatex, "<grey>textures/teamalpha", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamomegatex, "<grey>textures/teamomega", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamkappatex, "<grey>textures/teamkappa", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamsigmatex, "<grey>textures/teamsigma", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, insigniatex, "<grey>textures/action", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playertex, "<grey>textures/player", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, deadtex, "<grey>textures/dead", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatingtex, "<grey>textures/dominating", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatedtex, "<grey>textures/dominated", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inputtex, "textures/menu", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, waitingtex, "<grey>textures/waiting", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spectatortex, "<grey>textures/spectator", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, chattex, "<grey>textures/chat", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editingtex, "<grey>textures/editing", 3);

    TVAR(IDF_PERSIST|IDF_PRELOAD, progresstex, "<grey>textures/progress", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, progringtex, "<grey>textures/progring", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, warningtex, "<grey>textures/warning", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, questiontex, "<grey>textures/question", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, healthtex, "<grey>textures/hud/health", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, healthbgtex, "<grey>textures/hud/healthbg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, impulsetex, "<grey>textures/hud/impulse", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, impulsebgtex, "<grey>textures/hud/impulsebg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inventorytex, "<grey>textures/hud/inventory", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inventorybigtex, "<grey>textures/hud/inventorybig", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inventorybartex, "<grey>textures/hud/inventorybar", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtex, "<grey>textures/hud/flag", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pointtex, "<grey>textures/hud/point", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtex, "<grey>textures/hud/bomb", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowtex, "<grey>textures/hud/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowrighttex, "<grey><rotate:1>textures/hud/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowdowntex, "<grey><rotate:2>textures/hud/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowlefttex, "<grey><rotate:3>textures/hud/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, alerttex, "<grey>textures/hud/alert", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagdroptex, "<grey>textures/hud/flagdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtakentex, "<grey>textures/hud/flagtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombdroptex, "<grey>textures/hud/bombdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtakentex, "<grey>textures/hud/bombtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, attacktex, "<grey>textures/hud/attack", 3);

    VAR(IDF_PERSIST|IDF_HEX, inventorytone, -CTONE_MAX, -CTONE_TEAM-1, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, crosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, hitcrosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, noticetone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, eventtone, -CTONE_MAX, -CTONE_TEAM-1, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, clipstone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radartone, -CTONE_MAX, -CTONE_TEAM-1, 0xFFFFFF);

    VAR(IDF_PERSIST, teamhurthud, 0, 1, 3); // 0 = off, 1 = full body particle, 2 = fixed position and size
    VAR(IDF_PERSIST, teamhurttime, 0, 2500, VAR_MAX);
    VAR(IDF_PERSIST, teamhurtdist, 0, 0, VAR_MAX);
    FVAR(IDF_PERSIST, teamhurtsize, 0, 0.0175f, 1000);

    enum { BORDER_PLAY, BORDER_EDIT, BORDER_SPEC, BORDER_WAIT, BORDER_BG, BORDER_MAX };
    enum { BORDERP_TOP, BORDERP_BOTTOM, BORDERP_MAX, BORDERP_ALL = (1<<BORDERP_TOP)|(1<<BORDERP_BOTTOM) };

    VAR(IDF_PERSIST, playborder, 0, 0, BORDERP_ALL);
    VAR(IDF_PERSIST|IDF_HEX, playbordertone, -CTONE_MAX, -CTONE_TEAM-1, 0xFFFFFF);
    FVAR(IDF_PERSIST, playbordersize, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, playborderblend, 0, 0.5f, 1);

    VAR(IDF_PERSIST, editborder, 0, 0, BORDERP_ALL);
    VAR(IDF_PERSIST|IDF_HEX, editbordertone, -CTONE_MAX, 0, 0xFFFFFF);
    FVAR(IDF_PERSIST, editbordersize, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, editborderblend, 0, 0.9f, 1);

    VAR(IDF_PERSIST, specborder, 0, BORDERP_ALL, BORDERP_ALL);
    VAR(IDF_PERSIST|IDF_HEX, specbordertone, -CTONE_MAX, 0, 0xFFFFFF);
    FVAR(IDF_PERSIST, specbordersize, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, specborderblend, 0, 0.9f, 1);

    VAR(IDF_PERSIST, waitborder, 0, BORDERP_ALL, BORDERP_ALL);
    VAR(IDF_PERSIST|IDF_HEX, waitbordertone, -CTONE_MAX, 0, 0xFFFFFF);
    FVAR(IDF_PERSIST, waitbordersize, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, waitborderblend, 0, 0.9f, 1);

    VAR(IDF_PERSIST, backgroundborder, 0, 0, BORDERP_ALL);
    VAR(IDF_PERSIST|IDF_HEX, backgroundbordertone, -CTONE_MAX, 0, 0xFFFFFF);
    FVAR(IDF_PERSIST, backgroundbordersize, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, backgroundborderblend, 0, 0.5f, 1);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bordertoptex, "<grey>textures/hud/border", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, borderbottomtex, "<grey><rotate:2>textures/hud/border", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, underlaytex, "", 3);
    VAR(IDF_PERSIST, underlaydisplay, 0, 0, 2); // 0 = only firstperson and alive, 1 = only when alive, 2 = always
    VAR(IDF_PERSIST, underlayblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, overlaytex, "", 3);
    VAR(IDF_PERSIST, overlaydisplay, 0, 0, 2); // 0 = only firstperson and alive, 1 = only when alive, 2 = always
    FVAR(IDF_PERSIST, overlayblend, 0, 1, 1);

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
    FVAR(IDF_PERSIST, crosshairsize, 0, 0.06f, 1000);
    VAR(IDF_PERSIST, crosshairhitspeed, 0, 500, VAR_MAX);
    FVAR(IDF_PERSIST, crosshairblend, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, crosshairaccamt, 0, 0.75f, 1);
    VAR(IDF_PERSIST, crosshairflash, 0, 1, 1);
    FVAR(IDF_PERSIST, crosshairthrob, 1e-4f, 0.1f, 1000);
    TVAR(IDF_PERSIST|IDF_PRELOAD, pointertex, "textures/hud/pointer", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, guicursortex, "textures/hud/cursor", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, guicursorhovertex, "textures/hud/cursorhover", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, guicursorinputtex, "textures/hud/cursorinput", 3);

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

    VAR(IDF_PERSIST, showinventory, 0, 1, 1);
    VAR(IDF_PERSIST, inventoryammo, 0, 3, 3);
    VAR(IDF_PERSIST, inventoryammobar, 0, 2, 2);
    VAR(IDF_PERSIST, inventoryammostyle, 0, 1, 1);
    VAR(IDF_PERSIST, inventorygame, 0, 2, 2);
    VAR(IDF_PERSIST, inventorydate, 0, 0, 1);
    SVAR(IDF_PERSIST, inventorydateformat, "%H:%M:%S");
    VAR(IDF_PERSIST, inventorytime, 0, 1, 1);
    VAR(IDF_PERSIST, inventorytimeflash, 0, 1, 1);
    VAR(IDF_PERSIST, inventorytimestyle, 0, 3, 4);
    VAR(IDF_PERSIST, inventoryscore, 0, 1, VAR_MAX);
    VAR(IDF_PERSIST, inventoryscorespec, 0, 3, VAR_MAX);
    VAR(IDF_PERSIST, inventoryscorebg, 0, 1, 1);
    VAR(IDF_PERSIST, inventoryscoreinfo, 0, 1, 1); // shows offset
    VAR(IDF_PERSIST, inventoryscorepos, 0, 1, 1); // shows position
    VAR(IDF_PERSIST, inventoryscorename, 0, 2, 2); // 0 = off, 1 = only ffa, 2 = teams as well
    VAR(IDF_PERSIST, inventoryscorebreak, 0, 0, 2); // breaks up over multiple lines, 2 = center as well
    VAR(IDF_PERSIST, inventoryweapids, 0, 2, 2);
    VAR(IDF_PERSIST, inventorycolour, 0, 2, 2);
    VAR(IDF_PERSIST, inventoryflash, 0, 0, 1);
    FVAR(IDF_PERSIST, inventoryleft, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, inventoryright, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, inventoryskew, 1e-4f, 0.65f, 1000);
    FVAR(IDF_PERSIST, inventorydateskew, 1e-4f, 0.85f, 1000);
    FVAR(IDF_PERSIST, inventorytimeskew, 1e-4f, 1, 1000);
    FVAR(IDF_PERSIST, inventoryscoresize, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryscoreshrink, 0, 0, 1);
    FVAR(IDF_PERSIST, inventoryscoreshrinkmax, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, inventoryblend, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryglow, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, inventorytextoffsetx, -1, -0.3f, 1);
    FVAR(IDF_PERSIST, inventorytextoffsety, -1, -0.125f, 1);

    VAR(IDF_PERSIST, inventorybg, 0, 1, 2);
    FVAR(IDF_PERSIST, inventorybgskew, 0, 0.05f, 1); // skew items inside by this much
    FVAR(IDF_PERSIST, inventorybgblend, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, inventorybgskin, 0, 1, 1);
    FVAR(IDF_PERSIST, inventorybgspace, 0, 0.05f, 1); // for aligning diagonals

    FVAR(IDF_PERSIST, inventorybartop, 0, 0.046875f, 1); // starts from this offset
    FVAR(IDF_PERSIST, inventorybarbottom, 0, 0.28125f, 1); // ends at this offset
    FVAR(IDF_PERSIST, inventorybaroffset, -1, -0.25f, 1); // starts from this offset

    VAR(IDF_PERSIST, inventoryedit, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryeditblend, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryeditskew, 1e-4f, 0.5f, 1000);

    VAR(IDF_PERSIST, inventoryhealth, 0, 3, 3); // 0 = off, 1 = text, 2 = bar, 3 = bar + text
    VAR(IDF_PERSIST, inventoryhealthflash, 0, 2, 2);
    FVAR(IDF_PERSIST, inventoryhealthblend, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryhealththrob, 0, 0.035f, 1);
    FVAR(IDF_PERSIST, inventoryhealthbartop, 0, 0.09375f, 1); // starts from this offset
    FVAR(IDF_PERSIST, inventoryhealthbarbottom, 0, 0.0859375f, 1); // ends at this offset
    FVAR(IDF_PERSIST, inventoryhealthbgglow, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, inventoryhealthbgblend, 0, 0.5f, 1);

    VAR(IDF_PERSIST, inventoryimpulse, 0, 2, 3); // 0 = off, 1 = text, 2 = bar, 3 = both
    VAR(IDF_PERSIST, inventoryimpulseflash, 0, 2, 2);
    FVAR(IDF_PERSIST, inventoryimpulseblend, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryimpulsethrob, 0, 0.035f, 1);
    FVAR(IDF_PERSIST, inventoryimpulsebartop, 0, 0.171875f, 1); // starts from this offset
    FVAR(IDF_PERSIST, inventoryimpulsebarbottom, 0, 0.0859375f, 1); // ends at this offset
    FVAR(IDF_PERSIST, inventoryimpulsebgglow, 0, 0.05f, 1);
    FVAR(IDF_PERSIST, inventoryimpulsebgblend, 0, 0.5f, 1);

    VAR(IDF_PERSIST, inventoryvelocity, 0, 0, 2);
    FVAR(IDF_PERSIST, inventoryvelocityblend, 0, 1, 1);
    VAR(IDF_PERSIST, inventoryrace, 0, 2, 2);
    VAR(IDF_PERSIST, inventoryracestyle, 0, 1, 4);
    FVAR(IDF_PERSIST, inventoryraceblend, 0, 1, 1);
    VAR(IDF_PERSIST, inventorystatus, 0, 2, 3); // 0 = off, 1 = text, 2 = icon, 3 = icon + tex
    FVAR(IDF_PERSIST, inventorystatusblend, 0, 1, 1);
    FVAR(IDF_PERSIST, inventorystatusiconblend, 0, 0.65f, 1);

    VAR(IDF_PERSIST, inventoryalert, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryalertblend, 0, 0.5f, 1);
    VAR(IDF_PERSIST, inventoryalertflash, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, buffedtex, "<grey>textures/alerts/buff", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, burningtex, "<grey>textures/alerts/burn", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bleedingtex, "<grey>textures/alerts/bleed", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shockingtex, "<grey>textures/alerts/shock", 3);

    VAR(IDF_PERSIST, inventorygameinfo, 0, 15, 31); // &1 = mode, &2 = important, &4 = duel/survivor, &8 = non-alive, &16 = force all
    FVAR(IDF_PERSIST, inventorygameinfoblend, 0, 0.6f, 1);
    VAR(IDF_PERSIST, inventorygameinfoflash, 0, 3000, VAR_MAX);

    VAR(IDF_PERSIST, inventoryconopen, 0, 1, 1);
    FVAR(IDF_PERSIST, inventoryconopenblend, 0, 0.5f, 1);
    VAR(IDF_PERSIST, inventoryconopenflash, 0, 0, 1);
    VAR(IDF_PERSIST, inventoryinput, 0, 0, 3); // bitwise, 1 = focus=player1, 2 = focus!=player1
    FVAR(IDF_PERSIST, inventoryinputblend, 0, 0.75f, 1);
    VAR(IDF_PERSIST, inventoryinputfilter, 0, AC_ALL, AC_ALL);
    VAR(IDF_PERSIST, inventoryinputlinger, 0, AC_ALL, AC_ALL);
    VAR(IDF_PERSIST, inventoryinputdelay, 0, 125, VAR_MAX);
    VAR(IDF_PERSIST, inventoryinputactive, 0, 0x22FF22, 0xFFFFFF);
    VAR(IDF_PERSIST, inventoryinputcolour, 0, 0x888888, 0xFFFFFF);

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
    FVAR(IDF_PERSIST, clipsize, 0, 0.035f, 1000);
    FVAR(IDF_PERSIST, clipoffset, 0, 0.04f, 1000);
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
    FVAR(IDF_PERSIST, grenadeclipskew, 0, 1.5f, 10);
    FVAR(IDF_PERSIST, mineclipskew, 0, 2.f, 10);
    FVAR(IDF_PERSIST, rocketclipskew, 0, 1.75f, 10);
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

    VAR(IDF_PERSIST, showradar, 0, 1, 2);
    VAR(IDF_PERSIST, radarstyle, 0, 1, 3); // 0 = compass-sectional, 1 = compass-distance, 2 = screen-space, 3 = right-corner-positional
    FVAR(IDF_PERSIST, radaraspect, 0, 1, 2); // 0 = off, else = (for radarstyle 0/1) radar forms an ellipse
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, radarcornertex, "<grey>textures/hud/radar", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bliptex, "<grey>textures/hud/blip", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playerbliptex, "<grey>textures/hud/blip", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hurttex, "<grey>textures/hud/hurt", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hinttex, "<grey>textures/hint", 3);
    FVAR(IDF_PERSIST, radarblend, 0, 0.85f, 1);
    FVAR(IDF_PERSIST, radarplayerblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radarplayerhintblend, 0, 0.65f, 1);
    FVAR(IDF_PERSIST, radarplayersize, 0, 0.35f, 1000);
    FVAR(IDF_PERSIST, radarplayerhintsize, 0, 0.8f, 1);
    FVAR(IDF_PERSIST, radarblipblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radarblipsize, 0, 0.35f, 1000);
    FVAR(IDF_PERSIST, radarbliprotate, 0, 1, 1);
    FVAR(IDF_PERSIST, radaraffinityblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radaraffinitysize, 0, 0.8f, 1000);
    FVAR(IDF_PERSIST, radaritemblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radaritemsize, 0, 0.7f, 1000);
    FVAR(IDF_PERSIST, radarsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, radaroffset, 0, 0.08f, 1000);
    FVAR(IDF_PERSIST, radarcorner, 0, 0.15f, 1000);
    FVAR(IDF_PERSIST, radarcornersize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, radarcorneroffset, 0, 0.045f, 1);
    FVAR(IDF_PERSIST, radarcornerblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radartexblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radartexbright, 0, 0.65f, 1);
    FVAR(IDF_PERSIST, radarcornerbright, 0, 0.8f, 1);
    VAR(IDF_PERSIST, radardist, 0, 750, VAR_MAX); // 0 = use world size
    VAR(IDF_PERSIST, radarcornerdist, 0, 750, VAR_MAX); // 0 = use world size
    VAR(IDF_PERSIST, radaritems, 0, 2, 2);
    VAR(IDF_PERSIST, radaritemspawn, 0, 1, 1);
    VAR(IDF_PERSIST, radaritemtime, 0, 5000, VAR_MAX);
    VAR(IDF_PERSIST, radaritemnames, 0, 0, 2);

    VAR(IDF_PERSIST, radarplayers, 0, 2, 2);
    VAR(IDF_PERSIST, radarplayerfilter, 0, 0, 3); // 0 = off, 1 = non-team, 2 = team, 3 = only in duel/survivor/edit
    VAR(IDF_PERSIST, radarplayernames, 0, 0, 2);
    VAR(IDF_PERSIST, radarplayereffects, 0, 1, 1);
    VAR(IDF_PERSIST, radarplayerdominated, 0, 1, 1); // 0 = off, 1 = always track dominating players
    VAR(IDF_PERSIST, radarplayerduke, 0, 1, 1); // 0 = off, 1 = track when your side has one player left
    VAR(IDF_PERSIST, radarplayerkill, 0, 2, 2); // 0 = off, 1 = track killers, 2 = bots too

    VAR(IDF_PERSIST, radaraffinity, 0, 2, 2);
    VAR(IDF_PERSIST, radaraffinitynames, 0, 1, 2);

    VAR(IDF_PERSIST, radardamage, 0, 1, 2); // 0 = off, 1 = basic damage, 2 = verbose
    VAR(IDF_PERSIST, radardamageself, 0, 1, 1);
    VAR(IDF_PERSIST, radardamagemerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, radardamagetime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, radardamagefade, 1, 3500, VAR_MAX);
    FVAR(IDF_PERSIST, radardamagesize, 0, 20, 1000);
    FVAR(IDF_PERSIST, radardamageblend, 0, 0.85f, 1);
    VAR(IDF_PERSIST, radardamagemin, 1, 10, VAR_MAX);
    VAR(IDF_PERSIST, radardamagemax, 1, 100, VAR_MAX);
    VAR(IDF_PERSIST|IDF_HEX, radardamagecolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radardamageburncolour, PC(LAST), PC(FIRE), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radardamagebleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radardamageshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

    VAR(IDF_PERSIST, radarhits, 0, 1, 2);
    VAR(IDF_PERSIST, radarhitsheal, 0, 1, 1);
    VAR(IDF_PERSIST, radarhitsself, 0, 0, 1);
    VAR(IDF_PERSIST, radarhitsfollow, 0, 0, 1);
    VAR(IDF_PERSIST, radarhitsmerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, radarhitstime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, radarhitsfade, 1, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, radarhitsswipe, 0, 6, 1000);
    FVAR(IDF_PERSIST, radarhitsscale, 0, 1.8f, 1000);
    FVAR(IDF_PERSIST, radarhitsblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radarhitsheight, -1000, 0.25f, 1000);
    FVAR(IDF_PERSIST, radarhitsoffset, -1000, 3, 1000);
    VAR(IDF_PERSIST, radarhitsglow, 0, 1, 1);
    FVAR(IDF_PERSIST, radarhitsglowblend, 0, 1, 1);
    FVAR(IDF_PERSIST, radarhitsglowscale, 0, 2, 1000);
    FVAR(IDF_PERSIST, radarhitsglowcolour, 0, 0.75f, 5);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, radarhitsglowtex, "textures/guihover", 3);
    VAR(IDF_PERSIST|IDF_HEX, radarhitscolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radarhitsburncolour, PC(LAST), PC(FIRE), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radarhitsbleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, radarhitsshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

    VAR(IDF_PERSIST, showeditradar, 0, 1, 1);
    VAR(IDF_PERSIST, editradarstyle, 0, 2, 3); // 0 = compass-sectional, 1 = compass-distance, 2 = screen-space, 3 = right-corner-positional
    VAR(IDF_PERSIST, editradardist, 0, 0, VAR_MAX); // 0 = use world size

    VAR(IDF_PERSIST, motionblurfx, 0, 1, 2); // 0 = off, 1 = on, 2 = override
    FVAR(IDF_PERSIST, motionblurmin, 0, 0.0f, 1); // minimum
    FVAR(IDF_PERSIST, motionblurmax, 0, 0.75f, 1); // maximum
    FVAR(IDF_PERSIST, motionbluramt, 0, 0.5f, 1); // used for override

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

    TVAR(IDF_PERSIST, modeeditingtex, "<grey>textures/modes/editing", 3);
    TVAR(IDF_PERSIST, modedeathmatchtex, "<grey>textures/modes/deathmatch", 3);

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
        if(m_edit(g)) ADDMODE(modeeditingtex) \
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
        else ADDMODE(modedeathmatchtex) \
    }

    void modetexs(int g, int m, bool before, bool implied, vector<char> &list)
    {
        modecheck(g, m);
        #define ADDMODE(s) { list.put(s, strlen(s)); list.add(' '); }
        if(before) ADDMODEICON(g, m)
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
        if(!before) ADDMODEICON(g, m)
        #undef ADDMODE
    }

    ICOMMAND(0, modetexlist, "iiii", (int *g, int *m, int *b, int *p),
    {
        vector<char> list;
        modetexs(*g, *m, *b!=0, *p!=0, list);
        list.add('\0');
        result(list.getbuf());
    });

    bool needminimap() { return true; }

    bool chkcond(int val, bool cond)
    {
        if(val == 2 || (val && cond)) return true;
        return false;
    }

    bool minimal(int val, bool cond = false)
    {
        if(cond) return val != 0 && !hudminimal; // is not activated in minimal
        return val != 0 || hudminimal; // is activated in minimal
    }

    bool hasinput(bool pass, bool focus)
    {
        if(focus && (commandmillis > 0 || curcompass)) return true;
        return UI::active(pass);
    }

    bool hastkwarn(gameent *d)
    {
        if(!m_play(game::gamemode)) return false;
        return teamkillwarn && m_team(game::gamemode, game::mutators) && numteamkills() >= teamkillwarn;
    }

    bool hasteaminfo(gameent *d)
    {
        if(!m_play(game::gamemode) || game::focus->state != CS_ALIVE) return false;
        if(!lastteam) lastteam = totalmillis;
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

    float motionblur(float scale)
    {
        float amt = 0;
        switch(motionblurfx)
        {
            case 1:
            {
                if(game::focus->state >= CS_SPECTATOR || game::focus->state == CS_EDITING) break;
                float damage = game::focus->state == CS_ALIVE ? min(damageresidue, 100)/100.f : 1.f,
                      healthscale = float(m_health(game::gamemode, game::mutators, game::focus->actortype));
                if(healthscale > 0) damage = max(damage, 1.f-max(game::focus->health, 0)/healthscale);
                amt += damage*0.65f;
                if(burntime && game::focus->burning(lastmillis, burntime))
                    amt += 0.25f+(float((lastmillis-game::focus->lastres[WR_BURN])%burndelay)/float(burndelay))*0.35f;
                if(bleedtime && game::focus->bleeding(lastmillis, bleedtime))
                    amt += 0.25f+(float((lastmillis-game::focus->lastres[WR_BLEED])%bleeddelay)/float(bleeddelay))*0.35f;
                if(shocktime && game::focus->shocking(lastmillis, shocktime))
                    amt += 0.25f+(float((lastmillis-game::focus->lastres[WR_SHOCK])%shockdelay)/float(shockdelay))*0.35f;
                if(game::focus->turnside || game::focus->impulse[IM_JUMP])
                    amt += game::focus->turnside ? 0.125f : 0.25f;
                break;
            }
            case 2: amt += motionbluramt; break;
            default: break;
        }
        return clamp(amt, motionblurmin, motionblurmax)*scale;
    }

    void damage(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n) return;
        damageresidue = clamp(damageresidue+(n*(flags&HIT_BLEED ? 3 : 1)), 0, 200);
        int colour = radardamagecolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF44FF;
        else if(wr_burns(weap, flags)) colour = radardamageburncolour;
        else if(wr_bleeds(weap, flags)) colour = radardamagebleedcolour;
        else if(wr_shocks(weap, flags)) colour = radardamageshockcolour;
        vec dir = vec(loc).sub(camera1->o).normalize();
        loopv(damagelocs)
        {
            dhloc &d = damagelocs[i];
            if(v->clientnum != d.clientnum) continue;
            if(lastmillis-d.outtime > radardamagemerge) continue;
            if(d.colour != colour) continue;
            d.damage += n;
            d.dir = dir;
            return; // accumulate
        }
        damagelocs.add(dhloc(v->clientnum, lastmillis, n, loc, colour));
    }

    void hit(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n) return;
        int colour = radarhitscolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF44FF;
        else if(wr_burns(weap, flags)) colour = radarhitsburncolour;
        else if(wr_bleeds(weap, flags)) colour = radarhitsbleedcolour;
        else if(wr_shocks(weap, flags)) colour = radarhitsshockcolour;
        loopv(hitlocs)
        {
            dhloc &d = hitlocs[i];
            if(v->clientnum != d.clientnum) continue;
            if(lastmillis-d.outtime > radarhitsmerge) continue;
            if(d.colour != colour) continue;
            d.damage += n;
            d.dir = v->center();
            return; // accumulate
        }
        hitlocs.add(dhloc(v->clientnum, lastmillis, n, v->center(), colour));
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
        vec c = vec::hexcolor(colour);
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
        POINTER_NONE = 0, POINTER_RELATIVE, POINTER_GUI, POINTER_EDIT, POINTER_SPEC,
        POINTER_HAIR, POINTER_TEAM, POINTER_ZOOM, POINTER_HIT, POINTER_MAX
    };

    const char *getpointer(int index, int weap = -1)
    {
        switch(index)
        {
            case POINTER_RELATIVE: return pointertex;
            case POINTER_GUI:
            {
                switch(guicursortype)
                {
                    case 2: return guicursorinputtex; break;
                    case 1: return guicursorhovertex; break;
                    case 0: default: break;
                }
                return guicursortex;
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
        int millis = lastmillis-game::focus->weaplast[weap];
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

    void vecfromyaw(float yaw, int move, int strafe, vec2 &m)
    {
        if(move)
        {
            m.x = move*-sinf(RAD*yaw);
            m.y = move*cosf(RAD*yaw);
        }
        else m.x = m.y = 0;
        if(strafe)
        {
            m.x += strafe*cosf(RAD*yaw);
            m.y += strafe*sinf(RAD*yaw);
        }
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
        int ammo = game::focus->ammo[weap], maxammo = W(weap, ammomax), interval = lastmillis-game::focus->weaplast[weap];
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
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaplast[weap];
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
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaplast[weap];
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
                        int millis = lastmillis-d.outtime, delay = min(20, d.damage)*50;
                        if(millis >= delay || d.dir.iszero()) { if(millis >= radardamagetime+radardamagefade) damagelocs.remove(i--); continue; }
                        gameent *e = game::getclient(d.clientnum);
                        if(!radardamageself && e == game::focus) continue;
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
        Texture *t = tex && *tex ? textureload(tex, 3) : NULL;
        if(t && t != notexture)
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
        int cs = int((index == POINTER_GUI ? cursorsize : crosshairsize)*hudsize);
        float fade = index == POINTER_GUI ? cursorblend : crosshairblend;
        vec c(1, 1, 1);
        if(game::focus->state == CS_ALIVE && index >= POINTER_HAIR)
        {
            if(crosshairweapons&2) c = vec::hexcolor(W(game::focus->weapselect, colour));
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
        if(index != POINTER_GUI)
        {
            drawpointertex(getpointer(index, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c.r, c.g, c.b, fade*hudblend);
            if(index > POINTER_GUI)
            {
                if(minimal(showcirclebar)) drawcirclebar(cx, cy, hudsize);
                if(game::focus->state == CS_ALIVE && game::focus->hasweap(game::focus->weapselect, m_weapon(game::focus->actortype, game::gamemode, game::mutators)))
                {
                    if(minimal(showclips, true)) drawclip(game::focus->weapselect, cx, cy, hudsize);
                    if(showindicator) drawindicator(game::focus->weapselect, cx, cy, int(indicatorsize*hudsize), physics::secondaryweap(game::focus));
                }
                if(crosshairhitspeed && totalmillis-game::focus->lasthit <= crosshairhitspeed)
                {
                    vec c2(1, 1, 1);
                    if(hitcrosshairtone) skewcolour(c2.r, c2.g, c2.b, hitcrosshairtone);
                    else c2 = c;
                    drawpointertex(getpointer(POINTER_HIT, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c2.r, c2.g, c2.b, crosshairblend*hudblend);
                }
                if(crosshairdistance && (game::focus->state == CS_ALIVE || game::focus->state == CS_EDITING)) draw_textf("\fa%.1f\fwm", cx+crosshairdistancex, cy+crosshairdistancey, 0, 0, 255, 255, 255, int(hudblend*255), TEXT_RIGHT_JUSTIFY, -1, -1, 1, game::focus->o.dist(worldpos)/8.f);
            }
        }
        else
        {
            if(guicursortype == 2)
            {
                cy -= cs/2;
                cx -= cs/2;
            }
            drawpointertex(getpointer(index, game::focus->weapselect), cx, cy, cs, c.r, c.g, c.b, fade*hudblend);
        }
    }

    void drawpointers(int w, int h)
    {
        int index = POINTER_NONE;
        if(hasinput()) index = !hasinput(true) || commandmillis > 0 ? POINTER_NONE : POINTER_GUI;
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
            hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
            flushhudmatrix();
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
            tw = int((hudwidth-((hudsize*edgesize)*2+(hudsize*inventoryleft)+(hudsize*inventoryright)))/noticescale);
        if(noticetone) skewcolour(tr, tg, tb, noticetone);

        pushfont("emphasis");
        if(!gs_playing(game::gamestate) || lastmillis-game::maptime <= noticetitle)
        {
            ty += draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), 255, 255, 255, tf, TEXT_CENTERED, -1, tw, 1, *maptitle ? maptitle : mapname);
            pushfont("reduced");
            if(*mapauthor) ty += draw_textf("by %s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), 255, 255, 255, tf, TEXT_CENTERED, -1, tw, 1, mapauthor);
            defformatstring(gname, "%s", server::gamename(game::gamemode, game::mutators, 0, 32));
            ty += draw_textf("[ \fs\fa%s\fS ]", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), 255, 255, 255, tf, TEXT_CENTERED, -1, tw, 1, gname);
            popfont();
            ty += FONTH/3;
        }
        if(client::demoplayback && showdemoplayback)
            ty += draw_textf("Demo playback in progress", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), 255, 255, 255, tf, TEXT_CENTERED, -1, tw, 1)+FONTH/3;
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
                            ty += draw_textf("Press \fs\fw\f{=showgui profile 2}\fS to \fs%s\fS loadout", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1, target->loadweap.empty() ? "\fzoyselect" : "change");
                            popfont();
                        }
                        if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators))
                        {
                            pushfont("little");
                            ty += draw_textf("Press \fs\fw\f{=showgui team}\fS to change teams", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
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
                    ty += draw_textf("Press \fs\fw\f{=1:spectator 0}\fS to join the game", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
                    if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators) && shownotices >= 2)
                        ty += draw_textf("Press \fs\fw\f{=1:showgui team}\fS to join a team", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), tr, tg, tb, tf, TEXT_CENTERED, -1, tw, 1);
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
            tw = int((hudwidth-((hudsize*edgesize)*2+(hudsize*inventoryleft)+(hudsize*inventoryright)))/eventscale);
        if(eventtone) skewcolour(tr, tg, tb, eventtone);
        pushfont("huge");
        if(!gs_playing(game::gamestate))
            ty -= draw_textf("%s", tx, ty, int(FONTW*noticepadx), int(FONTH*noticepady), 255, 255, 255, tf, TEXT_CENTERED, -1, tw, 1, gamestates[2][game::gamestate])+FONTH/3;
        else
        {
            bool tkwarn = hastkwarn(game::focus), tinfo = hasteaminfo(game::focus);
            if(tkwarn || tinfo)
            {
                const char *col = teamnotices >= 2 ? "\fs\fzyS" : "";
                if(tkwarn) ty -= draw_textf("\fzryDo NOT shoot team-mates", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_SKIN|TEXT_CENTERED, -1, tw, 1)+FONTH/4;
                if(m_race(game::gamemode)) ty -= draw_textf("%sRace", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_SKIN|TEXT_CENTERED, -1, tw, 1, col)+FONTH/4;
                else if(!m_team(game::gamemode, game::mutators)) ty -= draw_textf("%sFree-for-all %s", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_SKIN|TEXT_CENTERED, -1, tw, 1, col, m_bomber(game::gamemode) ? "Bomber-ball" : "Deathmatch")+FONTH/4;
                else ty -= draw_textf("%sYou are on team %s", tx, ty, int(FONTW*eventpadx)+FONTW/4, int(FONTH*eventpady), tr, tg, tb, tf, TEXT_SKIN|TEXT_CENTERED, -1, tw, 1, col, game::colourteam(game::focus->team))+FONTH/4;
            }
        }
        if(m_capture(game::gamemode)) capture::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        else if(m_defend(game::gamemode)) defend::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        else if(m_bomber(game::gamemode)) bomber::drawevents(hudwidth, hudheight, tx, ty, tr, tg, tb, tf/255.f);
        popfont();
        pophudmatrix();
    }

    void drawlast()
    {
        if(!progressing && showhud)
        {
            hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
            flushhudmatrix();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if(commandmillis <= 0 && curcompass) rendercmenu();
            else if(shownotices && !client::waiting() && !hasinput(false) && !texpaneltimer) drawnotices();
            if(overlaydisplay >= 2 || (game::focus->state == CS_ALIVE && (overlaydisplay || !game::thirdpersonview(true))))
            {
                Texture *t = *overlaytex ? textureload(overlaytex, 3) : notexture;
                if(t != notexture)
                {
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    gle::colorf(1.f, 1.f, 1.f, overlayblend*hudblend);
                    drawtexture(0, 0, hudwidth, hudheight);
                }
            }
            glDisable(GL_BLEND);
        }
        if(progressing || (commandmillis <= 0 && !curcompass)) UI::render();
        if(!progressing) drawpointers(hudwidth, hudheight);
    }

    void drawconsole(int type, int w, int h, int x, int y, int s, float fade)
    {
        static vector<int> refs; refs.setsize(0);
        bool full = fullconsole || commandmillis > 0;
        int tz = 0;
        pushfont("console");
        if(type >= 2)
        {
            int numl = chatconsize, numo = chatconsize+chatconoverflow;
            loopvj(conlines) if(conlines[j].type >= CON_CHAT)
            {
                int len = !full && conlines[j].type > CON_CHAT ? chatcontime/2 : chatcontime;
                if(full || totalmillis-conlines[j].reftime <= len+chatconfade)
                {
                    if(refs.length() >= numl)
                    {
                        if(refs.length() >= numo)
                        {
                            if(full) break;
                            bool found = false;
                            loopvrev(refs) if(conlines[refs[i]].reftime+(conlines[refs[i]].type > CON_CHAT ? chatcontime/2 : chatcontime) < conlines[j].reftime+len)
                            {
                                refs.remove(i);
                                found = true;
                                break;
                            }
                            if(!found) continue;
                        }
                        conlines[j].reftime = min(conlines[j].reftime, totalmillis-len);
                    }
                    refs.add(j);
                }
            }
            pushhudscale(chatconscale);
            int tx = int(x/chatconscale), ty = int(y/chatconscale),
                ts = int(s/chatconscale), tr = tx+FONTW;
            tz = int(tz/chatconscale);
            loopvj(refs)
            {
                int len = !full && conlines[refs[j]].type > CON_CHAT ? chatcontime/2 : chatcontime;
                float f = full || !chatconfade ? 1.f : clamp(((len+chatconfade)-(totalmillis-conlines[refs[j]].reftime))/float(chatconfade), 0.f, 1.f),
                    g = conlines[refs[j]].type > CON_CHAT ? conblend : chatconblend;
                if(chatcondate && *chatcondateformat)
                    tz += draw_textf("%s %s", tr, ty-tz, 0, 0, 255, 255, 255, int(fade*f*g*255), TEXT_LEFT_UP, -1, ts, 1, gettime(conlines[refs[j]].realtime, chatcondateformat), conlines[refs[j]].cref)*f;
                else tz += draw_textf("%s", tr, ty-tz, 0, 0, 255, 255, 255, int(fade*f*g*255), TEXT_LEFT_UP, -1, ts, 1, conlines[refs[j]].cref)*f;
            }
            pophudmatrix();
            tz = int(tz*chatconscale);
        }
        else
        {
            if((showconsole && showhud) || commandmillis > 0)
            {
                int numl = consize, numo = consize+conoverflow;
                loopvj(conlines) if(type ? conlines[j].type >= (confilter && !full ? CON_LO : 0) && conlines[j].type <= CON_HI : conlines[j].type >= (confilter && !full ? CON_LO : 0))
                {
                    int len = conlines[j].type >= CON_CHAT ? (!full && conlines[j].type > CON_CHAT ? chatcontime/2 : chatcontime) : (!full && conlines[j].type < CON_IMPORTANT ? contime/2 : contime),
                        fadelen = conlines[j].type >= CON_CHAT ? chatconfade : confade;
                    if(conskip ? j>=conskip-1 || j>=conlines.length()-numl : full || totalmillis-conlines[j].reftime <= len+fadelen)
                    {
                        if(refs.length() >= numl)
                        {
                            if(refs.length() >= numo)
                            {
                                if(full) break;
                                bool found = false;
                                loopvrev(refs)
                                {
                                    int check = conlines[refs[i]].type >= CON_CHAT ? (!full && conlines[refs[i]].type > CON_CHAT ? chatcontime/2 : chatcontime) : (!full && conlines[refs[i]].type < CON_IMPORTANT ? contime/2 : contime);
                                    if(conlines[refs[i]].reftime+check < conlines[j].reftime+len)
                                    {
                                        refs.remove(i);
                                        found = true;
                                        break;
                                    }
                                }
                                if(!found) continue;
                            }
                            conlines[j].reftime = min(conlines[j].reftime, totalmillis-len);
                        }
                        refs.add(j);
                    }
                }
                pushhudscale(conscale);
                int tx = int(x/conscale), ty = int(y/conscale),
                    ts = int(s/conscale), tr = concenter ? tx+ts/2 : tx;
                tz = int(tz/conscale);
                loopvrev(refs)
                {
                    int len = !full && conlines[refs[i]].type < CON_IMPORTANT ? contime/2 : contime;
                    float f = full || !confade ? 1.f : clamp(((len+confade)-(totalmillis-conlines[refs[i]].reftime))/float(confade), 0.f, 1.f),
                        g = full ? fullconblend  : (conlines[refs[i]].type >= CON_IMPORTANT ? selfconblend : conblend);
                    if(condate && *condateformat)
                        tz += draw_textf("%s %s", tr, ty+tz, 0, 0, 255, 255, 255, int(fade*f*g*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, ts, 1, gettime(conlines[refs[i]].realtime, condateformat), conlines[refs[i]].cref)*f;
                    else tz += draw_textf("%s", tr, ty+tz, 0, 0, 255, 255, 255, int(fade*f*g*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, ts, 1, conlines[refs[i]].cref)*f;
                }
                if(conskipwarn && conskip)
                    tz += draw_textf("\fs\fzwy^^^\fS IN BACKLOG: \fs\fy%d\fS \fs\fzwy^^^\fS", tr, ty+tz, 0, 0, 255, 255, 255, int(fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, ts, 1, conskip);
                pophudmatrix();
                tz = int(tz*conscale);
            }
            if(commandmillis > 0)
            {
                pushfont("command");
                Texture *t = textureload(commandicon ? commandicon : inputtex, 3);
                vec c(1, 1, 1);
                if(commandcolour) c = vec::hexcolor(commandcolour);
                float f = float(totalmillis%1000)/1000.f;
                if(f < 0.5f) f = 1.f-f;
                pushhudscale(commandscale);
                float th = FONTH, tw = float(t->w)/float(t->h)*th;
                int tx = int(x/commandscale), ty = int(y/commandscale),
                    ts = int(s/commandscale), tq = (concenter ? tx+ts/2-FONTW*3 : tx), tr = int(tw+FONTW), tt = ts-(FONTH+FONTW);
                tz = int(tz/commandscale);
                glBindTexture(GL_TEXTURE_2D, t->id);
                gle::color(c, fullconblend*fade*f);
                drawtexture(tx, ty+tz, th, tw);
                int cp = commandpos >= 0 ? commandpos : strlen(commandbuf);//, fp = completesize && completeoffset >= 0 ? min(pos, completeoffset+completesize) : -1;
                tz += draw_textf("%s", tq+tr, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, cp, tt, 1, commandbuf);
                if(capslockwarn && capslockon)
                    tz += draw_textf("\fs\fzoy^\fS \fs\fw\f{CAPSLOCK}\fS is \fs\fcON\fS", tq+tr, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1);
                popfont();
                if(commandbuf[0] == '/' && commandbuf[1])
                {
                    char *start = &commandbuf[1];
                    const char chrlist[7] = { ';', '(', ')', '[', ']', '\"', '$', };
                    loopi(7)
                    {
                        char *semi = strrchr(start, chrlist[i]);
                        if(semi) start = semi+1;
                    }
                    while(*start == ' ') start++;
                    if(*start)
                    {
                        char *end = start;
                        end += strcspn(start, " \t\0");
                        if(end)
                        {
                            string idname;
                            copystring(idname, start, min(size_t(end-start+1), sizeof(idname)));
                            ident *id = idents.access(idname);
                            if(id)
                            {
                                string idtype = "";
                                if(id->flags&IDF_CLIENT || id->flags&IDF_SERVER)
                                {
                                    if(id->flags&IDF_ADMIN) concatstring(idtype, "admin-only ");
                                    else if(id->flags&IDF_MODERATOR) concatstring(idtype, "moderator-only ");
                                    concatstring(idtype, id->flags&IDF_CLIENT ? "game " : "server ");
                                }
                                if(id->type != ID_COMMAND)
                                {
                                    if(id->flags&IDF_READONLY) concatstring(idtype, "read-only ");
                                    if(id->flags&IDF_PERSIST) concatstring(idtype, "persistent ");
                                    if(id->flags&IDF_WORLD) concatstring(idtype, "world ");
                                }
                                switch(id->type)
                                {
                                    case ID_ALIAS:
                                    {
                                        tz += draw_textf("%salias", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, idtype);
                                        break;
                                    }
                                    case ID_COMMAND:
                                    {
                                        tz += draw_textf("%scommand", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, idtype);
                                        if(strlen(id->args)) tz += draw_textf("\faargs: \fw%d \fa(\fw%s\fa)", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, strlen(id->args), id->args);
                                        else tz += draw_textf("\faargs: \fwnone", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1);
                                        break;
                                    }
                                    case ID_VAR:
                                    {
                                        tz += draw_textf("%sinteger", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, idtype);
                                        if(id->flags&IDF_HEX)
                                        {
                                            if(id->maxval == 0xFFFFFF)
                                                tz += draw_textf("\famin: \fw0x%.6X\fa (\fw%d\fa,\fw%d\fa,\fw%d\fa), max: \fw0x%.6X\fa (\fw%d\fa,\fw%d\fa,\fw%d\fa), default: \fw0x%.6X\fa (\fw%d\fa,\fw%d\fa,\fw%d\fa), current: \fw0x%.6X (\fw%d\fa,\fw%d\fa,\fw%d\fa) [\fs\f[%d]#\fS]", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1,
                                                        id->minval, (id->minval>>16)&0xFF, (id->minval>>8)&0xFF, id->minval&0xFF,
                                                        id->maxval, (id->maxval>>16)&0xFF, (id->maxval>>8)&0xFF, id->maxval&0xFF,
                                                        id->def.i, (id->def.i>>16)&0xFF, (id->def.i>>8)&0xFF, id->def.i&0xFF,
                                                        *id->storage.i, (*id->storage.i>>16)&0xFF, (*id->storage.i>>8)&0xFF, *id->storage.i&0xFF, *id->storage.i);
                                            else tz += draw_textf("\famin: \fw0x%X\fa, max: \fw0x%X\fa, default: \fw0x%X\fa, current: \fw0x%X", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->minval, id->maxval, id->def.i, *id->storage.i);
                                        }
                                        else tz += draw_textf("\famin: \fw%d\fa, max: \fw%d\fa, default: \fw%d\fa, current: \fw%d", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->minval, id->maxval, id->def.i, *id->storage.i);
                                        break;
                                    }
                                    case ID_FVAR:
                                    {
                                        tz += draw_textf("%sfloat", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, idtype);
                                        tz += draw_textf("\famin: \fw%f\fa, max: \fw%f\fa, default: \fw%f\fa, current: \fw%f", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->minvalf, id->maxvalf, id->def.f, *id->storage.f);
                                        break;
                                    }
                                    case ID_SVAR:
                                    {
                                        tz += draw_textf("%s%s", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, idtype, id->flags&IDF_TEXTURE ? "texture" : "string");
                                        tz += draw_textf("\fadefault: \fw%s\fa, current: \fw%s", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->def.s, *id->storage.s);
                                        break;
                                    }
                                }

                                string fields = "";
                                if(id->type == ID_VAR || id->type == ID_COMMAND)
                                {
                                    if(id->type == ID_VAR && id->fields.length() > 1)
                                    {
                                        concatstring(fields, "<bitfield>");
                                        loopvj(id->fields) if(id->fields[j])
                                            concformatstring(fields, "\n%d [0x%x] = %s", 1<<j, 1<<j, id->fields[j]);
                                    }
                                    else loopvj(id->fields) if(id->fields[j])
                                        concformatstring(fields, "%s<%s>", j ? " " : "", id->fields[j]);
                                }
                                if(!*fields) switch(id->type)
                                {
                                    case ID_ALIAS: concatstring(fields, "<value>"); break;
                                    case ID_VAR: concatstring(fields, "<integer>"); break;
                                    case ID_FVAR: concatstring(fields, "<float>"); break;
                                    case ID_SVAR: concatstring(fields, "<string>"); break;
                                    case ID_COMMAND:
                                    {
                                        loopj(strlen(id->args)) switch(id->args[j])
                                        {
                                            case 's': concformatstring(fields, "%s<string>", j ? " " : ""); break;
                                            case 'i': case 'b': case 'N': concformatstring(fields, "%s<%s>", j ? " " : "", id->flags&IDF_HEX ? "bitfield" : "integer"); break;
                                            case 'f': case 'g': concformatstring(fields, "%s<float>", j ? " " : ""); break;
                                            case 't': concformatstring(fields, "%s<null>", j ? " " : ""); break;
                                            case 'e': concformatstring(fields, "%s<commands>", j ? " " : ""); break;
                                            case 'r': case '$': concformatstring(fields, "%s<ident>", j ? " " : ""); break;
                                            default: concformatstring(fields, "%s<?>", j ? " " : ""); break;
                                        }
                                        break;
                                    }
                                    default: break;
                                }
                                tz += draw_textf("usage: \fa/%s %s", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->name, fields);

                                if(id->desc)
                                    tz += draw_textf("\fa%s", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->desc);

                                if(id->type == ID_ALIAS)
                                {
                                    pushfont("consub");
                                    tz += draw_textf("\facontents: \fw%s", tq, ty+tz, 0, 0, 255, 255, 255, int(fullconblend*fade*255), concenter ? TEXT_CENTERED : TEXT_LEFT_JUSTIFY, -1, tt, 1, id->getstr());
                                    popfont();
                                }
                            }
                        }
                    }
                }
                pophudmatrix();
                //tz = int(tz*commandscale);
            }
        }
        popfont();
    }

    int radartype()
    {
        if(game::focus->state == CS_EDITING) return editradarstyle;
        return radarstyle;
    }

    int radarrange()
    {
        if(game::focus->state == CS_EDITING) return editradardist ? editradardist : getworldsize();
        int dist = getworldsize();
        switch(radartype())
        {
            case 3: dist = radarcornerdist ? radarcornerdist : getworldsize(); break;
            case 2: case 1: case 0: case -1: default: dist = radardist ? radardist : getworldsize(); break;
        }
        if(radardistlimit) dist = min(radardistlimit, dist);
        return dist;
    }

    void drawblip(const char *tex, float area, int w, int h, float s, float blend, int style, const vec &pos, const vec &colour, const char *font, const char *text, ...)
    {
        if(style < 0) style = radartype();
        vec dir = vec(pos).sub(camera1->o);
        float dist = clamp(dir.magnitude()/float(radarrange()), 0.f, 1.f);
        dir.rotate_around_z(-camera1->yaw*RAD).normalize();
        vec loc(0, 0, 0);
        if(style == 2)
        {
            if(vectocursor(pos, loc.x, loc.y, loc.z))
            {
                loc.x *= hudwidth;
                loc.y *= hudsize;
            }
            else return; // can't render things we can't point at
        }
        float yaw = -atan2(dir.x, dir.y)/RAD, x = sinf(RAD*yaw), y = -cosf(RAD*yaw), size = max(w, h)/2,
              ts = size*(style != 3 ? radarsize : radarcornersize), tp = ts*s, tq = tp*0.5f;
        if(style != 2)
        {
            float tx = 0, ty = 0, tr = 0;
            switch(style)
            {
                case 3:
                {
                    ts = size*radarcorner;
                    tx = w-ts;
                    ty = ts;
                    ts -= ts*radarcorneroffset;
                    tr = ts*dist;
                    break;
                }
                case 1:
                {
                    tx = w/2;
                    ty = h/2;
                    tr = (size*radaroffset)+(ts*4*dist);
                    break;
                }
                case 0: default:
                {
                    tx = w/2;
                    ty = h/2;
                    tr = (size*radaroffset)+(ts*area);
                    break;
                }
            }
            loc.x = tr*x;
            loc.y = tr*y;
            if(style != 3 && radaraspect != 0 && w != h)
            {
                if(w > h) loc.x *= (float(w)/float(h))*radaraspect;
                else loc.y *= (float(h)/float(w))*radaraspect;
            }
            loc.x += tx;
            loc.y += ty;
        }
        gle::color(colour, blend);
        Texture *t = textureload(tex, 3);
        if(t)
        {
            glBindTexture(GL_TEXTURE_2D, t->id);
            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_STRIP);
            if(style != 2 && radarbliprotate)
            {
                vec2 o(loc.x, loc.y);
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
            }
            else
            {
                gle::attribf(loc.x - tq, loc.y + tq); gle::attribf(0, 1);
                gle::attribf(loc.x + tq, loc.y + tq); gle::attribf(1, 1);
                gle::attribf(loc.x - tq, loc.y - tq); gle::attribf(0, 0);
                gle::attribf(loc.x + tq, loc.y - tq); gle::attribf(1, 0);
            }
            gle::end();
        }
        if(text && *text)
        {
            if(font && *font) pushfont(font);
            defvformatstring(str, text, text);
            loc.x -= x*tq; loc.y -= y*tq*0.75f;
            if(y > 0)
            {
                int width, height;
                text_bounds(str, width, height, 0, 0, -1, TEXT_CENTERED|TEXT_NO_INDENT, 1);
                loc.y -= y*height;
            }
            draw_textf("%s", int(loc.x), int(loc.y), 0, 0, 255, 255, 255, int(blend*255.f), TEXT_CENTERED|TEXT_NO_INDENT, -1, -1, 1, str);
            if(font && *font) popfont();
        }
    }

    void drawplayerblip(gameent *d, int w, int h, int style, float blend, bool force)
    {
        bool killer = false, self = false;
        if(radarplayerkill && (game::focus->state == CS_DEAD || game::focus->state == CS_WAITING) && game::focus->lastdeath)
        {
            if(d->clientnum == game::focus->lastattacker)
                killer = (radarplayerkill >= 2 || d->actortype == A_PLAYER) && (d->state == CS_ALIVE || d->state == CS_DEAD || d->state == CS_WAITING);
            if(d == game::focus) self = lastmillis-game::focus->lastdeath <= m_delay(game::focus->actortype, game::gamemode, game::mutators, d->team);
        }
        if(d == game::focus && !self) return;
        vec dir = vec(d->o).sub(camera1->o);
        float dist = dir.magnitude();
        bool isdominated = radarplayereffects && (!m_team(game::gamemode, game::mutators) || d->team != game::focus->team) && d->dominated.find(game::focus) >= 0,
            dominated = radarplayerdominated && isdominated;
        if(!force && !killer && !self && !dominated) switch(radarplayerfilter)
        {
            case 1: if(m_team(game::gamemode, game::mutators) && d->team == game::focus->team) return; break;
            case 2: if(m_team(game::gamemode, game::mutators) && d->team != game::focus->team) return; break;
            default: break;
        }
        if(force || killer || self || dominated || dist <= radarrange())
        {
            bool burning = radarplayereffects && burntime && lastmillis%150 < 50 && d->burning(lastmillis, burntime),
                 bleeding = radarplayereffects && bleedtime && lastmillis%150 < 50 && d->bleeding(lastmillis, bleedtime),
                 shocking = radarplayereffects && shocktime && lastmillis%150 < 50 && d->shocking(lastmillis, shocktime);
            vec colour[2];
            if(isdominated) colour[0] = game::rescolour(d, PULSE_DISCO);
            else colour[0] = vec::hexcolor(game::getcolour(d, game::playerundertone));
            if(d->lastbuff)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? 1.f-(millis/500.f) : (millis-500)/500.f;
                flashcolour(colour[0].r, colour[0].g, colour[0].b, 1.f, 1.f, 1.f, amt);
            }
            if(burning)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                vec c = game::rescolour(d, PULSE_BURN);
                flashcolour(colour[0].r, colour[0].g, colour[0].b, c.r, c.g, c.b, amt);
            }
            if(bleeding)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                vec c = game::rescolour(d, PULSE_SHOCK);
                flashcolour(colour[0].r, colour[0].g, colour[0].b, c.r, c.g, c.b, amt);
            }
            if(shocking)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                vec c = game::rescolour(d, PULSE_SHOCK);
                flashcolour(colour[0].r, colour[0].g, colour[0].b, c.r, c.g, c.b, amt);
            }
            colour[1] = vec::hexcolor(game::getcolour(d, game::playerovertone));
            const char *tex = isdominated ? dominatedtex : (killer || self ? arrowtex : playerbliptex);
            float fade = (force || killer || self || dominated ? 1.f : clamp(1.f-(dist/float(radarrange())), isdominated ? 0.25f : 0.f, 1.f))*blend, size = killer || self ? 1.5f : (isdominated ? 1.25f : 1.f);
            if(!self && (d->state == CS_DEAD || d->state == CS_WAITING))
            {
                int millis = d->lastdeath ? lastmillis-d->lastdeath : 0;
                if(millis > 0)
                {
                    int len = min(m_delay(d->actortype, game::gamemode, game::mutators, d->team), 2500);
                    if(len > 0) fade *= clamp(float(len-millis)/float(len), 0.f, 1.f);
                    else return;
                }
                else return;
                tex = deadtex;
            }
            else if(d->state == CS_ALIVE)
            {
                int len = m_protect(game::gamemode, game::mutators), millis = d->protect(lastmillis, len);
                if(millis > 0) fade *= clamp(float(len-millis)/float(len), 0.f, 1.f);
                if(!force && !killer && !self && !dominated)
                    fade *= clamp(vec(d->vel).add(d->falling).magnitude()/movespeed, 0.f, 1.f);
            }
            loopi(2)
            {
                if(i)
                {
                    if(killer && d->state == CS_ALIVE)
                    {
                        drawblip(tex, 1, w, h, size*(i ? radarplayersize : radarplayerhintsize), fade*(i ? radarplayerblend : radarplayerhintblend), style, d->o, colour[i], "tiny", "%s (\fs\fc%d\fS)", game::colourname(d), d->health);
                        continue;
                    }
                    if(force || self || chkcond(radarplayernames, !game::tvmode()))
                    {
                        drawblip(tex, 1, w, h, size*(i ? radarplayersize : radarplayerhintsize), fade*(i ? radarplayerblend : radarplayerhintblend), style, d->o, colour[i], "tiny", "%s", d != game::player1 ? game::colourname(d) : "you");
                        continue;
                    }
                }
                drawblip(i ? tex : hinttex, 1, w, h, size*(i ? radarplayersize : radarplayerhintsize), fade*(i ? radarplayerblend : radarplayerhintblend), style, d->o, colour[i]);
            }
        }
    }

    void drawentblip(int w, int h, float blend, int n, vec &o, int type, attrvector &attr, bool spawned, int lastspawn, bool insel)
    {
        if(type > NOTUSED && type < MAXENTTYPES && ((enttype[type].usetype == EU_ITEM && spawned) || game::focus->state == CS_EDITING))
        {
            float inspawn = radaritemtime && spawned && lastspawn && lastmillis-lastspawn <= radaritemtime ? float(lastmillis-lastspawn)/float(radaritemtime) : 0.f;
            if(game::focus->state != CS_EDITING && radaritemspawn && (enttype[type].usetype != EU_ITEM || inspawn <= 0.f)) return;
            vec dir = vec(o).sub(camera1->o);
            float dist = dir.magnitude();
            if(dist >= radarrange())
            {
                if(insel || inspawn > 0.f) dir.mul(radarrange()/dist);
                else return;
            }
            const char *tex = bliptex;
            vec colour(1, 1, 1);
            float fade = insel ? 1.f : clamp(1.f-(dist/float(radarrange())), 0.1f, 1.f), size = radarblipsize;
            if(type == WEAPON)
            {
                int attr1 = w_attr(game::gamemode, game::mutators, type, attr[0], m_weapon(game::focus->actortype, game::gamemode, game::mutators));
                if(isweap(attr1))
                {
                    tex = itemtex(WEAPON, attr1);
                    colour = vec::hexcolor(W(attr1, colour));
                    fade *= radaritemblend;
                    size = radaritemsize;
                }
                else return;
            }
            else
            {
                tex = itemtex(type, attr[0]);
                if(!tex || !*tex) tex = bliptex;
                else size = radaritemsize;
                fade *= radarblipblend;
            }
            if(game::focus->state != CS_EDITING && !insel && inspawn > 0.f)
                fade = radaritemspawn ? 1.f-inspawn : fade+((1.f-fade)*(1.f-inspawn));
            if(insel) drawblip(tex, 0, w, h, size, fade*blend, -1, o, colour, "tiny", "%s %s", enttype[type].name, entities::entinfo(type, attr, insel));
            else if(chkcond(radaritemnames, !game::tvmode())) drawblip(tex, 0, w, h, size, fade*blend, -1, o, colour, "tiny", "%s", entities::entinfo(type, attr, false));
            else drawblip(tex, 0, w, h, size, fade*blend, -1, o, colour);
        }
    }

    void drawentblips(int w, int h, float blend)
    {
        if(m_edit(game::gamemode) && game::focus->state == CS_EDITING)
        {
            int hover = !entities::ents.inrange(enthover) && !entgroup.empty() ? entgroup[0] : -1;
            loopv(entgroup) if(entities::ents.inrange(entgroup[i]) && entgroup[i] != hover)
            {
                gameentity &e = *(gameentity *)entities::ents[entgroup[i]];
                drawentblip(w, h, blend, entgroup[i], e.o, e.type, e.attrs, e.spawned(), e.lastspawn, true);
            }
            if(entities::ents.inrange(hover))
            {
                gameentity &e = *(gameentity *)entities::ents[hover];
                drawentblip(w, h, blend, hover, e.o, e.type, e.attrs, e.spawned(), e.lastspawn, true);
            }
        }
        else
        {
            loopi(entities::lastuse(EU_ITEM))
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                drawentblip(w, h, blend, i, e.o, e.type, e.attrs, e.spawned(), e.lastspawn, false);
            }
            loopv(projs::projs) if(projs::projs[i]->projtype == PRJ_ENT && projs::projs[i]->ready())
            {
                projent &proj = *projs::projs[i];
                if(entities::ents.inrange(proj.id))
                    drawentblip(w, h, blend, -1, proj.o, entities::ents[proj.id]->type, entities::ents[proj.id]->attrs, true, proj.spawntime, false);
            }
        }
    }

    void drawdamageblips(int w, int h, float blend)
    {
        loopv(damagelocs)
        {
            dhloc &d = damagelocs[i];
            int millis = lastmillis-d.outtime;
            if(millis >= radardamagetime+radardamagefade || d.dir.iszero()) { if(millis >= min(20, d.damage)*50) damagelocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            gameent *e = game::getclient(d.clientnum);
            if(!radardamageself && e == game::focus) continue;
            float amt = millis >= radardamagetime ? 1.f-(float(millis-radardamagetime)/float(radardamagefade)) : float(millis)/float(radardamagetime),
                range = clamp(max(d.damage, radardamagemin)/float(max(radardamagemax-radardamagemin, 1)), radardamagemin/100.f, 1.f),
                fade = clamp(radardamageblend*blend, min(radardamageblend*radardamagemin/100.f, 1.f), radardamageblend)*amt,
                size = clamp(range*radardamagesize, min(radardamagesize*radardamagemin/100.f, 1.f), radardamagesize)*amt;
            vec dir = d.dir, colour = d.colour < 0 ? game::rescolour(game::focus, INVPULSE(d.colour)) : vec::hexcolor(d.colour);
            if(e == game::focus) d.dir = vec(e->yaw*RAD, 0.f).neg();
            vec o = vec(camera1->o).add(vec(dir).mul(radarrange()));
            if(radardamage >= 5) drawblip(hurttex, 2+size/3, w, h, size, fade, 0, o, colour, "tiny", "%s +%d", e ? game::colourname(e) : "?", d.damage);
            else drawblip(hurttex, 2+size/3, w, h, size, fade, 0, o, colour);
        }
    }

    void drawhits(int w, int h, float blend)
    {
        pushfont("tiny");
        pushhudscale(radarhitsscale);
        float maxy = -1.f;
        loopv(hitlocs)
        {
            dhloc &d = hitlocs[i];
            int millis = lastmillis-d.outtime;
            if(millis >= radarhitstime+radarhitsfade || d.dir.iszero()) { hitlocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            gameent *a = game::getclient(d.clientnum);
            if((!radarhitsheal && d.damage < 0) || (!radarhitsself && a == game::focus)) continue;
            vec o = radarhitsfollow && a ? a->center() : d.dir;
            o.z += actor[a ? a->actortype : A_PLAYER].height*radarhitsheight;
            float cx = 0, cy = 0, cz = 0;
            if(!vectocursor(o, cx, cy, cz)) continue;
            float hx = cx*w/radarhitsscale, hy = cy*h/radarhitsscale, fade = blend*radarhitsblend;
            if(radarhitsoffset != 0) hx += FONTW*radarhitsoffset;
            if(millis <= radarhitstime)
            {
                float amt = millis/float(radarhitstime), total = FONTW*radarhitsswipe*(1-amt);
                if(radarhitsoffset < 0) hx -= total;
                else hx += total;
                fade *= amt;
            }
            else
            {
                int offset = millis-radarhitstime;
                hy -= FONTH*offset/float(radarhitstime);
                fade *= 1-(offset/float(radarhitsfade));
            }
            defformatstring(text, "%c%d", d.damage > 0 ? '-' : (d.damage < 0 ? '+' : '~'), d.damage < 0 ? 0-d.damage : d.damage);
            vec colour = d.colour < 0 ? game::rescolour(a, INVPULSE(d.colour)) : vec::hexcolor(d.colour);
            if(maxy >= 0 && hy < maxy) hy = maxy;
            if(radarhitsglow)
            {
                float width = 0, height = 0;
                text_boundsf(text, width, height, 0, 0, -1, TEXT_CENTERED, 1);
                gle::colorf(colour.r*radarhitsglowcolour, colour.g*radarhitsglowcolour, colour.b*radarhitsglowcolour, fade*radarhitsglowblend);
                settexture(radarhitsglowtex);
                drawtexture(hx-(width*radarhitsglowscale*0.5f), hy-(height*radarhitsglowscale*0.25f), width*radarhitsglowscale, height*radarhitsglowscale);
            }
            hy += draw_textf("%s", hx, hy, 0, 0, int(colour.r*255), int(colour.g*255), int(colour.b*255), int(fade*255), TEXT_CENTERED, -1, -1, 1, text)/radarhitsscale;
            if(maxy < 0 || hy > maxy) maxy = hy;
        }
        pophudmatrix();
        popfont();
    }

    void drawradar(int w, int h, float blend)
    {
        if(chkcond(radarhits, !game::tvmode())) drawhits(w, h, blend);
        if(radartype() == 3)
        {
            vec pos = vec(camera1->o).sub(minimapcenter).mul(minimapscale).add(0.5f), dir(camera1->yaw*RAD, 0.f);
            float scale = radarrange(), size = max(w, h)/2, s = size*radarcorner, x = w-s*2, y = 0, q = s*2*radarcorneroffset, r = s-q;
            bindminimap();
            gle::colorf(radarcornerbright, radarcornerbright, radarcornerbright, radarcornerblend);
            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_FAN);
            loopi(16)
            {
                vec v = vec(0, -1, 0).rotate_around_z(i/16.0f*2*M_PI);
                gle::attribf(x + q + r*(1.0f + v.x), y + q + r*(1.0f + v.y));
                vec tc = vec(dir).rotate_around_z(i/16.0f*2*M_PI);
                gle::attribf(pos.x + tc.x*scale*minimapscale.x, pos.y + tc.y*scale*minimapscale.y);
            }
            gle::end();
            float gr = 1, gg = 1, gb = 1;
            if(radartone) skewcolour(gr, gg, gb, radartone);
            settexture(radarcornertex, 3);
            gle::colorf(gr*radartexbright, gg*radartexbright, gb*radartexbright, radartexblend);
            drawsized(w-s*2, 0, s*2);
        }
        if(chkcond(radaritems, !game::tvmode()) || m_edit(game::gamemode)) drawentblips(w, h, blend*radarblend); // 2
        if(chkcond(radaraffinity, !game::tvmode())) // 3
        {
            if(m_capture(game::gamemode)) capture::drawblips(w, h, blend*radarblend);
            else if(m_defend(game::gamemode)) defend::drawblips(w, h, blend*radarblend);
            else if(m_bomber(game::gamemode)) bomber::drawblips(w, h, blend*radarblend);
        }
        if(chkcond(radarplayers, radarplayerfilter != 3 || m_duke(game::gamemode, game::mutators) || m_edit(game::gamemode))) // 4
        {
            gameent *d = NULL;
            int numdyns = game::numdynents(), style = radartype() != 2 ? radartype() : 1, others[T_MAX] = {0};
            if(radarplayerduke && game::focus->state == CS_ALIVE && m_survivor(game::gamemode, game::mutators))
            {
                loopi(numdyns) if((d = (gameent *)game::iterdynents(i)) && d->state == CS_ALIVE && d->actortype < A_ENEMY)
                    others[d->team]++;
            }
            loopi(numdyns) if((d = (gameent *)game::iterdynents(i)) && d->state != CS_SPECTATOR && d->actortype < A_ENEMY)
            {
                bool force = false;
                if(radarplayerduke && game::focus->state == CS_ALIVE)
                {
                    if(m_duel(game::gamemode, game::mutators)) force = true;
                    else if(m_survivor(game::gamemode, game::mutators))
                        force = (m_team(game::gamemode, game::mutators) ? (d->team != game::focus->team && others[game::focus->team] == 1) : (others[T_NEUTRAL] == 2));
                }
                drawplayerblip(d, w, h, style, blend*radarblend, force);
            }
        }
        if(radardamage) drawdamageblips(w, h, blend*radarblend); // 5+
    }

    int drawprogress(int x, int y, float start, float length, float size, bool left, float r, float g, float b, float fade, float skew, const char *font, const char *text, ...)
    {
        if(skew <= 0.f) return 0;
        float q = clamp(skew, 0.f, 1.f), cr = r*q, cg = g*q, cb = b*q, s = size*skew, cs = s/2, cx = left ? x+cs : x-cs, cy = y-cs;
        gle::colorf(cr, cg, cb, fade);
        settexture(progringtex, 3);
        drawslice((SDL_GetTicks()%1000)/1000.f, 0.1f, cx, cy, cs);
        settexture(progresstex, 3);
        gle::colorf(cr, cg, cb, fade*0.25f);
        drawslice(0, 1, cx, cy, cs);
        gle::colorf(cr, cg, cb, fade);
        drawslice(start, length, cx, cy, cs);
        if(text && *text)
        {
            pushhudscale(skew);
            if(font && *font) pushfont(font);
            int tx = int(cx/skew), ty = int((cy-FONTH/2*skew)/skew), ti = int(255.f*fade);
            defvformatstring(str, text, text);
            draw_textf("%s", tx, ty, 0, 0, 255, 255, 255, ti, TEXT_CENTERED, -1, -1, 1, str);
            if(font && *font) popfont();
            pophudmatrix();
        }
        return int(s);
    }

    const struct barstep
    {
        float amt, r, g, b;
    } barsteps[4][4] = {
        { { 0, 1, 1, 1 }, { 0.35f, 0.75f, 0.75f, 0.75f }, { 0.65f, 0.65f, 0.65f, 0.65f }, { 1, 1, 1, 1 } },
        { { 0, 0.75f, 0, 0 }, { 0.35f, 1, 0.5f, 0 }, { 0.65f, 1, 1, 0 }, { 1, 0, 1, 0 } },
        { { 0, 1, 0.25f, 0.25f }, { 0.35f, 1, 0, 1 }, { 0.65f, 0.25f, 0.25f, 1 }, { 1, 0, 1, 1 } },
        { { 0, 0.5f, 0, 0 }, { 0.35f, 0.25f, 0.f, 0.5f }, { 0.65f, 0.25f, 0.25f, 0.75f }, { 1, 0.75f, 0.75f, 1 } }
    };

    int drawbar(int x, int y, int w, int h, int type, float top, float bottom, float fade, float amt, const char *tex, const char *bgtex, int tone, float bgglow, float blend, float pulse, float throb, float throbscale, int throbcolour = -1, int throbreverse = false)
    {
        int offset = int(w*(throb >= 0 ? throb*throbscale : 0.f)), id = clamp(type, 0, 3);
        if(bgtex && *bgtex)
        {
            int glow = 0;
            float gr = 1, gg = 1, gb = 1, gf = fade*blend;
            if(tone) skewcolour(gr, gg, gb, tone);
            if(pulse > 0)
            {
                int millis = lastmillis%1000;
                float skew = (millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f))*pulse;
                flashcolourf(gr, gg, gb, gf, id != 1 && (id != 2 || throb < 0) ? 0.5f : 1.f, 0.f, id != 1 && (id != 2 || throb < 0) ? 0.5f : 0.f, 1.f, skew);
                glow += int(w*bgglow*skew);
            }
            settexture(bgtex, 3);
            gle::colorf(gr, gg, gb, fade*gf);
            drawtexture(x-offset-glow, y-h-offset-glow, w+glow*2+offset*2, h+glow*2+offset*2);
        }
        if(amt <= 0.f) return h;
        settexture(tex, 3);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::defcolor(4);
        gle::begin(GL_TRIANGLE_STRIP);
        float btoff = 1-bottom, middle = btoff-top;
        int cx = x-offset, cy = y-h+int(h*top)-offset, cw = w+offset*2, ch = int(h*middle)+offset*2, throbstep = int(roundf(3*(throbreverse ? 1-throb : throb)*amt));
        const float margin = 0.1f;
        vec lastbarcolour(0, 0, 0);
        loopi(4)
        {
            const barstep &step = barsteps[id][i];
            vec colour = throb >= 0 && throbcolour >= 0 && i == throbstep ? vec::hexcolor(throbcolour) : vec(step.r, step.g, step.b);
            if(i > 0)
            {
                if(step.amt > amt && barsteps[id][i-1].amt <= amt)
                {
                    float hoff = 1 - amt, hlerp = (amt - barsteps[id][i-1].amt) / (step.amt - barsteps[id][i-1].amt),
                          r = colour.r*hlerp + lastbarcolour.r*(1-hlerp),
                          g = colour.g*hlerp + lastbarcolour.g*(1-hlerp),
                          b = colour.b*hlerp + lastbarcolour.b*(1-hlerp);
                    gle::attribf(cx, cy + hoff*ch); gle::attribf(0, hoff*middle+top); gle::attribf(r, g, b, fade);
                    gle::attribf(cx + cw, cy + hoff*ch); gle::attribf(1, hoff*middle+top); gle::attribf(r, g, b, fade);
                }
                if(step.amt > amt + margin)
                {
                    float hoff = 1 - (amt + margin), hlerp = (amt + margin - barsteps[id][i-1].amt) / (step.amt - barsteps[id][i-1].amt),
                          r = colour.r*hlerp + lastbarcolour.r*(1-hlerp),
                          g = colour.g*hlerp + lastbarcolour.g*(1-hlerp),
                          b = colour.b*hlerp + lastbarcolour.b*(1-hlerp);
                    gle::attribf(cx, cy + hoff*ch); gle::attribf(0, hoff*middle+top); gle::attribf(r, g, b, 0);
                    gle::attribf(cx + cw, cy + hoff*ch); gle::attribf(1, hoff*middle+top); gle::attribf(r, g, b, 0);
                    lastbarcolour = colour;
                    break;
                }
            }
            float off = 1 - step.amt, hfade = fade, r = colour.r, g = colour.g, b = colour.b;
            if(step.amt > amt) hfade *= 1 - (step.amt - amt)/margin;
            gle::attribf(cx, cy + off*ch); gle::attribf(0, off*middle+top); gle::attribf(r, g, b, hfade);
            gle::attribf(cx + cw, cy + off*ch); gle::attribf(1, off*middle+top); gle::attribf(r, g, b, hfade);
            lastbarcolour = colour;
        }
        gle::end();
        return h;
    }

    int drawitembar(int x, int y, float size, bool left, float r, float g, float b, float fade, float skew, float amt, int type)
    {
        if(skew <= 0.f || amt <= 0.f) return 0;
        Texture *t = textureload(inventorybartex, 3);
        float q = clamp(skew, 0.f, 1.f), cr = left ? r : r*q, cg = left ? g : g*q, cb = left ? b : b*q, s = size*skew,
              w = float(t->w)/float(t->h)*s, btoff = 1-inventorybarbottom, middle = btoff-inventorybartop;
        int sx = int(w), sy = int(s), so = int(sx*inventorybaroffset), cx = left ? x-so : x-sx+so, cy = y-sy+int(sy*inventorybartop), cw = sx, ch = int(sy*middle), id = clamp(type, 0, 3);
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::defcolor(4);
        gle::begin(GL_TRIANGLE_STRIP);
        const float margin = 0.1f;
        loopi(4)
        {
            const barstep &step = barsteps[id][i];
            if(i > 0)
            {
                if(step.amt > amt && barsteps[id][i-1].amt <= amt)
                {
                    float hoff = 1 - amt, hlerp = (amt - barsteps[id][i-1].amt) / (step.amt - barsteps[id][i-1].amt),
                          hr = cr*step.r*hlerp + cr*barsteps[id][i-1].r*(1-hlerp),
                          hg = cg*step.g*hlerp + cg*barsteps[id][i-1].g*(1-hlerp),
                          hb = cb*step.b*hlerp + cb*barsteps[id][i-1].b*(1-hlerp);
                    gle::attribf(cx, cy + hoff*ch); gle::attribf(0, hoff*middle+inventorybartop); gle::attribf(hr, hg, hb, fade);
                    gle::attribf(cx + cw, cy + hoff*ch); gle::attribf(1, hoff*middle+inventorybartop); gle::attribf(hr, hg, hb, fade);
                }
                if(step.amt > amt + margin)
                {
                    float hoff = 1 - (amt + margin), hlerp = (amt + margin - barsteps[id][i-1].amt) / (step.amt - barsteps[id][i-1].amt),
                          hr = cr*step.r*hlerp + cr*barsteps[id][i-1].r*(1-hlerp),
                          hg = cg*step.g*hlerp + cg*barsteps[id][i-1].g*(1-hlerp),
                          hb = cb*step.b*hlerp + cb*barsteps[id][i-1].b*(1-hlerp);
                    gle::attribf(cx, cy + hoff*ch); gle::attribf(0, hoff*middle+inventorybartop); gle::attribf(hr, hg, hb, 0);
                    gle::attribf(cx + cw, cy + hoff*ch); gle::attribf(1, hoff*middle+inventorybartop); gle::attribf(hr, hg, hb, 0);
                    break;
                }
            }
            float off = 1 - step.amt, hfade = fade, hr = cr*step.r, hg = cg*step.g, hb = cb*step.b;
            if(step.amt > amt) hfade *= 1 - (step.amt - amt)/margin;
            gle::attribf(cx, cy + off*ch); gle::attribf(0, off*middle+inventorybartop); gle::attribf(hr, hg, hb, hfade);
            gle::attribf(cx + cw, cy + off*ch); gle::attribf(1, off*middle+inventorybartop); gle::attribf(hr, hg, hb, hfade);
        }
        gle::end();
        return sy;
    }

    int drawitem(const char *tex, int x, int y, float size, float sub, bool bg, bool left, float r, float g, float b, float fade, float skew, const char *font, const char *text, ...)
    {
        if(skew <= 0.f) return 0;
        Texture *t = textureload(tex, 3);
        float q = clamp(skew, 0.f, 1.f), cr = left ? r : r*q, cg = left ? g : g*q, cb = left ? b : b*q, s = size*skew, w = float(t->w)/float(t->h)*s;
        int heal = m_health(game::gamemode, game::mutators, game::focus->actortype), sy = int(s), cx = x, cy = y, cs = int(s), cw = int(w);
        bool pulse = inventoryflash && game::focus->state == CS_ALIVE && game::focus->health < heal;
        if(bg && sub == 0 && inventorybg)
        {
            Texture *u = textureload(inventorybg == 2 ? inventorybigtex : inventorytex, 3);
            float gr = 1, gb = 1, gg = 1, gf = fade*inventorybgblend;
            int glow = 0, bw = int(float(u->w)/float(u->h)*s);
            if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
            if(pulse)
            {
                int millis = lastmillis%1000;
                float amt = (millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f))*clamp(float(heal-game::focus->health)/float(heal), 0.f, 1.f);
                flashcolourf(gr, gg, gb, gf, 1.f, 0.f, 0.f, 1.f, amt);
                glow += int(s*inventoryglow*amt);
            }
            glBindTexture(GL_TEXTURE_2D, u->id);
            gle::colorf(gr, gg, gb, fade*gf);
            drawtexture(left ? cx-glow : cx-bw-glow, cy-cs-glow, bw+glow*2, cs+glow*2, left);
        }
        if(bg && inventorybg)
        {
            int co = int(cs*inventorybgskew), cp = int(cw*inventorybgskew);
            if(sub == 0) sy -= int(cs*inventorybgspace*skew);
            cx += left ? cp/2 : -cp/2;
            cy -= co/2;
            cs -= co;
            cw -= cp;
        }
        if(sub > 0)
        {
            int co = int(cs*sub);
            sy -= cs-co;
            cs = co;
            cw = int(cw*sub);
        }
        gle::colorf(cr, cg, cb, fade);
        glBindTexture(GL_TEXTURE_2D, t->id);
        drawtexture(left ? cx : cx-cw, cy-cs, cw, cs);
        if(text && *text)
        {
            pushhudscale(skew);
            if(font && *font) pushfont(font);
            int ox = int(cw*inventorytextoffsetx), oy = int(cs*inventorytextoffsety),
                tx = int((left ? (cx+cw-ox) : (cx-cw+ox))/skew),
                ty = int((cy-cs/2+oy)/skew), tj = left ? TEXT_LEFT_BAL : TEXT_RIGHT_BAL;
            defvformatstring(str, text, text);
            draw_textf("%s", tx, ty, 0, 0, 255, 255, 255, int(fade*255), tj|TEXT_NO_INDENT, -1, -1, 1, str);
            if(font && *font) popfont();
            pophudmatrix();
        }
        return sy;
    }

    int drawitemtextx(int x, int y, float size, int flags, float skew, const char *font, float blend, const char *text, ...)
    {
        if(skew <= 0.f) return 0;
        pushhudscale(skew);
        if(font && *font) pushfont(font);
        int curflags = flags|TEXT_NO_INDENT, cx = x, cy = y, xpad = 0, ypad = 0;
        if(inventorybg && size > 0)
        {
            int cs = int(size*skew), co = int(cs*inventorybgskew);
            cx += (flags&TEXT_ALIGN) == TEXT_LEFT_JUSTIFY ? co/2 : -co/2;
            cy -= co/2;
        }
        float gr = 1, gb = 1, gg = 1, gf = blend;
        if(curflags&TEXT_SKIN)
        {
            if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
            gf *= inventorybgskin;
            xpad = FONTW/4;
        }
        defvformatstring(str, text, text);
        int tx = int(cx/skew), ty = int(cy/skew);
        switch(curflags&TEXT_ALIGN)
        {
            case TEXT_LEFT_JUSTIFY: tx += FONTW*0.5f; break;
            case TEXT_RIGHT_JUSTIFY: tx -= FONTW*0.5f; break;
            case TEXT_CENTERED:
            {
                int width, height;
                text_bounds(str, width, height, xpad, ypad, -1, curflags, 1);
                tx -= (width/skew*0.5f)+FONTW;
                break;
            }
        }
        int sy = draw_textf("%s", tx, ty, xpad, ypad, int(gr*255), int(gg*255), int(gb*255), int(gf*255), curflags, -1, -1, 1, str)*skew;
        if(font && *font) popfont();
        pophudmatrix();
        return sy;
    }

    int drawitemtext(int x, int y, float size, bool left, float skew, const char *font, float blend, const char *text, ...)
    {
        defvformatstring(str, text, text);
        return drawitemtextx(x, y, size, left ? TEXT_LEFT_UP : TEXT_RIGHT_UP, skew, font, blend, "%s", str);
    }

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

    int drawentitem(int n, int x, int y, int s, float skew, float fade)
    {
        if(entities::ents.inrange(n))
        {
            gameentity &e = *(gameentity *)entities::ents[n];
            string attrstr; attrstr[0] = '\0';
            loopi(enttype[e.type].numattrs)
            {
                defformatstring(s, "%s%d", i ? " " : "", e.attrs[i]);
                concatstring(attrstr, s);
            }
            const char *itext = itemtex(e.type, e.attrs[0]);
            int ty = drawitem(itext && *itext ? itext : "textures/blank", x, y, s, 0, true, false, 1.f, 1.f, 1.f, fade, skew),
                qy = drawitemtext(x, y, s, false, skew, "reduced", fade, "%s", attrstr);
            qy += drawitemtext(x, y-qy, s, false, skew, "reduced", fade, "%s", entities::entinfo(e.type, e.attrs, true));
            qy += drawitemtext(x, y-qy, s, false, skew, "default", fade, "%s (%d)", enttype[e.type].name, n);
            return ty;
        }
        return 0;
    }

    int drawselection(int x, int y, int s, int m, float blend)
    {
        int sy = 0;
        if(game::focus->state == CS_ALIVE && inventoryammo)
        {
            const char *hudtexs[W_ALL] = {
                clawtex, pistoltex, swordtex, shotguntex, smgtex, flamertex, plasmatex, zappertex, rifletex, grenadetex, minetex, rockettex
            };
            int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators);//, lastweap = game::focus->getlastweap(sweap);
            loopi(W_ALL) if(game::focus->holdweap(i, sweap, lastmillis))
            {
                if(y-sy-s < m) break;
                float size = s, skew = 0.f;
                if(game::focus->weapstate[i] == W_S_SWITCH || game::focus->weapstate[i] == W_S_USE)// && (i != game::focus->weapselect || i != lastweap))
                {
                    float amt = clamp(float(lastmillis-game::focus->weaplast[i])/float(game::focus->weapwait[i]), 0.f, 1.f);
                    if(i != game::focus->weapselect) skew = game::focus->hasweap(i, sweap) ? 1.f-(amt*(1.f-inventoryskew)) : 1.f-amt;
                    else skew = game::focus->weapstate[i] == W_S_USE ? amt : inventoryskew+(amt*(1.f-inventoryskew));
                }
                else if(game::focus->hasweap(i, sweap) || i == game::focus->weapselect) skew = i != game::focus->weapselect ? inventoryskew : 1.f;
                else continue;
                vec c(1, 1, 1);
                if(inventorytone || inventorycolour) skewcolour(c.r, c.g, c.b, inventorycolour ? W(i, colour) : inventorytone);
                int oldy = y-sy, curammo = game::focus->ammo[i];
                if(inventoryammostyle && (game::focus->weapstate[i] == W_S_RELOAD || game::focus->weapstate[i] == W_S_USE) && game::focus->weapload[i] > 0)
                {
                    int reloaded = int(curammo*clamp(float(lastmillis-game::focus->weaplast[i])/float(game::focus->weapwait[i]), 0.f, 1.f));
                    curammo = max(curammo-game::focus->weapload[i], 0);
                    if(reloaded > curammo) curammo = reloaded;
                }

                if(inventoryammo >= 2 && (i == game::focus->weapselect || inventoryammo >= 3) && W(i, ammomax) > 1 && game::focus->hasweap(i, sweap))
                    sy += drawitem(hudtexs[i], x, y-sy, size, 0, true, false, c.r, c.g, c.b, blend, skew, "super", "%d", curammo);
                else sy += drawitem(hudtexs[i], x, y-sy, size, 0, true, false, c.r, c.g, c.b, blend, skew);
                if(inventoryammobar && (i == game::focus->weapselect || inventoryammobar >= 2) && W(i, ammomax) > 1 && game::focus->hasweap(i, sweap))
                    drawitembar(x, oldy, size, false, c.r, c.g, c.b, blend, skew, curammo/float(W(i, ammomax)));
                if(inventoryweapids && (i == game::focus->weapselect || inventoryweapids >= 2))
                {
                    static string weapids[W_ALL];
                    static int lastweapids = -1;
                    int n = weapons::slot(game::focus, i);
                    if(lastweapids != changedkeys)
                    {
                        loopj(W_ALL)
                        {
                            defformatstring(action, "weapon %d", j);
                            const char *actkey = searchbind(action, 0);
                            if(actkey && *actkey) copystring(weapids[j], actkey);
                            else formatstring(weapids[j], "%d", j);
                        }
                        lastweapids = changedkeys;
                    }
                    drawitemtext(x, oldy, size, false, skew, "default", blend, "\f[%d]%s", inventorycolour >= 2 ? W(i, colour) : 0xAAAAAA, isweap(n) ? weapids[n] : "?");
                }
            }
        }
        else if(game::focus->state == CS_EDITING && inventoryedit)
        {
            int hover = entities::ents.inrange(enthover) ? enthover : (!entgroup.empty() ? entgroup[0] : -1);
            sy += FONTH*4;
            if(y-sy-s >= m) sy += drawentitem(hover, x, y-sy, s, 1.f, blend*inventoryeditblend);
            loopv(entgroup) if(entgroup[i] != hover)
            {
                if(y-sy-s < m) break;
                sy += drawentitem(entgroup[i], x, y-sy, s, inventoryeditskew, blend*inventoryeditblend);
            }
        }
        return sy;
    }

    int drawhealth(int x, int y, int s, float blend, bool interm)
    {
        int size = s*2, sy = 0;
        bool alive = !interm && game::focus->state == CS_ALIVE;
        if(alive)
        {
            if(inventoryhealth)
            {
                float fade = blend*inventoryhealthblend;
                int heal = m_health(game::gamemode, game::mutators, game::focus->actortype);
                float pulse = inventoryhealthflash ? clamp((heal-game::focus->health)/float(heal), 0.f, 1.f) : 0.f,
                      throb = inventoryhealththrob > 0 && regentime && game::focus->lastregen && lastmillis-game::focus->lastregen <= regentime ? clamp((lastmillis-game::focus->lastregen)/float(regentime), 0.f, 1.f) : -1.f;
                if(inventoryhealth&2)
                    sy += drawbar(x, y, s, size, 1, inventoryhealthbartop, inventoryhealthbarbottom, fade, clamp(game::focus->health/float(heal), 0.f, 1.f), healthtex, healthbgtex, inventorytone, inventoryhealthbgglow, inventoryhealthbgblend, pulse, throb, inventoryhealththrob, inventoryhealthflash >= 2 ? (game::focus->lastregenamt <= 0 ? 0xFF0000 : 0x00FF00) : -1, game::focus->lastregenamt <= 0);
                if(inventoryhealth&1)
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(pulse > 0)
                    {
                        int millis = lastmillis%1000;
                        float amt = (millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f))*pulse;
                        flashcolour(gr, gg, gb, 1.f, 0.f, 0.f, amt);
                    }
                    pushfont("super");
                    int ty = inventoryhealth&2 ? 0-size/2 : 0;
                    ty += draw_textf("%d", x+s/2, y-sy-ty, 0, 0, int(gr*255), int(gg*255), int(gb*255), int(fade*255), TEXT_CENTER_UP, -1, -1, 1, max(game::focus->health, 0));
                    popfont();
                    if(!(inventoryhealth&2))
                    {
                        pushfont("reduced");
                        ty += draw_textf("health", x+s/2, y-sy-ty, 0, 0, 255, 255, 255, int(fade*255), TEXT_CENTER_UP, -1, -1, 1);
                        popfont();
                        sy += ty;
                    }
                }
            }
            if(game::focus->actortype < A_ENEMY && physics::allowimpulse(game::focus) && m_impulsemeter(game::gamemode, game::mutators) && inventoryimpulse)
            {
                float fade = blend*inventoryimpulseblend, span = 1-clamp(float(game::focus->impulse[IM_METER])/float(impulsemeter), 0.f, 1.f),
                      pulse = inventoryimpulseflash && game::focus->impulse[IM_METER] ? 1-span : 0.f,
                      throb = game::canregenimpulse(game::focus) && game::focus->impulse[IM_METER] > 0 && game::focus->lastimpulsecollect ? clamp(((lastmillis-game::focus->lastimpulsecollect)%1000)/1000.f, 0.f, 1.f) : -1.f,
                      gr = 1, gg = 1, gb = 1;
                flashcolour(gr, gg, gb, 0.25f, 0.25f, 0.25f, 1-span);
                if(pulse > 0 && impulsemeter-game::focus->impulse[IM_METER] < impulsecost) flashcolour(gr, gg, gb, 1.f, 0.f, 0.f, clamp(lastmillis%1000/1000.f, 0.f, 1.f));
                if(inventoryimpulse&2)
                    sy += drawbar(x, y-sy, s, size, 2, inventoryimpulsebartop, inventoryimpulsebarbottom, fade, span, impulsetex, impulsebgtex, inventorytone, inventoryimpulsebgglow, inventoryimpulsebgblend, pulse, throb, inventoryimpulsethrob, inventoryimpulseflash >= 2 ? 0xFFFFFF : -1);
                if(inventoryimpulse&1)
                {
                    if(!(inventoryimpulse&2))
                    {
                        pushfont("super");
                        int ty = draw_textf("%d%%", x+s/2, y-sy+(inventoryimpulse&2 ? size/2 : 0), 0, 0, int(gr*255), int(gg*255), int(gb*255), int(fade*255), TEXT_CENTER_UP, -1, -1, 1, int(span*100));
                        popfont();
                        pushfont("reduced");
                        ty += draw_textf("impulse", x+s/2, y-sy-ty, 0, 0, 255, 255, 255, int(fade*255), TEXT_CENTER_UP, -1, -1, 1);
                        popfont();
                        sy += ty;
                    }
                    else
                    {
                        pushfont("super");
                        draw_textf("%d", x+s/2, y-sy+(inventoryimpulse&2 ? size/2 : 0), 0, 0, int(gr*255), int(gg*255), int(gb*255), int(fade*255), TEXT_CENTER_UP, -1, -1, 1, int(span*100));
                        popfont();
                    }
                }
            }
            if(inventoryvelocity >= (m_race(game::gamemode) ? 1 : 2))
            {
                int flags = TEXT_CENTER_UP;
                float gr = 1, gg = 1, gb = 1, gf = blend;
                if(inventorybg)
                {
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    gf *= inventorybgskin;
                    flags |= TEXT_SKIN;
                }
                else gf *= inventoryvelocityblend;
                pushfont("emphasis");
                sy += draw_textf("%d", x+s/2, y-sy, 0, 0, int(gr*255), int(gg*255), int(gb*255), int(gf*255), flags, -1, -1, 1, int(vec(game::focus->vel).add(game::focus->falling).magnitude()));
                popfont();
                pushfont("reduced");
                sy += draw_textf("speed", x+s/2, y-sy, 0, 0, int(gr*255), int(gg*255), int(gb*255), int(gf*255), flags, -1, -1, 1);
                popfont();
            }
            if(inventoryalert)
            {
                float fade = blend*inventoryalertblend;
                if(game::focus->lastbuff)
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    if(inventoryalertflash)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                        flashcolour(gr, gg, gb, 1.f, 1.f, 1.f, amt);
                    }
                    sy += drawitem(buffedtex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
                }
                if(burntime && game::focus->burning(lastmillis, burntime))
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    if(inventoryalertflash)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                        vec c = game::rescolour(game::focus, PULSE_BURN);
                        flashcolour(gr, gg, gb, c.r, c.g, c.b, amt);
                    }
                    sy += drawitem(burningtex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
                }
                if(bleedtime && game::focus->bleeding(lastmillis, bleedtime))
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    if(inventoryalertflash)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                        vec c = game::rescolour(game::focus, PULSE_BLEED);
                        flashcolour(gr, gg, gb, c.r, c.g, c.b, amt);
                    }
                    sy += drawitem(bleedingtex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
                }
                if(shocktime && game::focus->shocking(lastmillis, shocktime))
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    if(inventoryalertflash)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                        vec c = game::rescolour(game::focus, PULSE_SHOCK);
                        flashcolour(gr, gg, gb, c.r, c.g, c.b, amt);
                    }
                    sy += drawitem(shockingtex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
                }
            }
            if(inventoryconopen)
            {
                float fade = blend*inventoryconopenblend;
                if(game::focus->conopen)
                {
                    float gr = 1, gg = 1, gb = 1;
                    if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                    if(inventoryconopenflash)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                        flashcolour(gr, gg, gb, 1.f, 1.f, 1.f, amt);
                    }
                    sy += drawitem(chattex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
                }
            }
            if(inventoryinput&(game::focus != game::player1 ? 2 : 1))
            {
                static const char *actionnames[AC_TOTAL] = {
                    "shoot1", "shoot2", "reload", "use", "jump", "walk", "crouch", "special", "drop", "affinity"
                };
                pushfont("little");

                sy += draw_textf("\f[%d]\f(%s)", x+s/2, y-sy, 0, 0, 255, 255, 255, int(blend*inventoryinputblend*255), TEXT_CENTER_UP, -1, -1, 1,
                        game::focus->move == -1 ? inventoryinputactive : inventoryinputcolour, arrowdowntex);

                sy += draw_textf("\f[%d]\f(%s)  \f[%d]\f(%s)", x+s/2, y-sy, 0, 0, 255, 255, 255, int(blend*inventoryinputblend*255), TEXT_CENTER_UP, -1, -1, 1,
                        game::focus->strafe == 1 ? inventoryinputactive : inventoryinputcolour, arrowlefttex,
                        game::focus->strafe == -1 ? inventoryinputactive : inventoryinputcolour, arrowrighttex);

                sy += draw_textf("\f[%d]\f(%s)", x+s/2, y-sy, 0, 0, 255, 255, 255, int(blend*inventoryinputblend*255), TEXT_CENTER_UP, -1, -1, 1,
                        game::focus->move == 1 ? inventoryinputactive : inventoryinputcolour, arrowtex);

                loopi(AC_TOTAL) if(inventoryinputfilter&(1<<i))
                {
                    bool active = game::focus->action[i] || (inventoryinputlinger&(1<<i) && game::focus->actiontime[i] && lastmillis-abs(game::focus->actiontime[i]) <= inventoryinputdelay);
                    sy += draw_textf("\fs\fw\f{\f[%d]%s}\fS", x+s/2, y-sy, 0, 0, 255, 255, 255, int(blend*inventoryinputblend*255), TEXT_CENTER_UP, -1, -1, 1,
                            active ? inventoryinputactive : inventoryinputcolour, actionnames[i]);
                }

                popfont();
            }
        }
        else
        {
            int st = interm ? CS_WAITING : game::player1->state;
            const char *state = "", *tex = "";
            switch(st)
            {
                case CS_EDITING: state = "EDIT"; tex = editingtex; break;
                case CS_SPECTATOR: state = "SPEC"; tex = spectatortex; break;
                case CS_WAITING: state = "WAIT"; tex = waitingtex; break;
                case CS_DEAD: state = "DEAD"; tex = deadtex; break;
            }
            if(inventorystatus&1 && *state)
            {
                sy -= x/2;
                pushfont("emphasis");
                sy += draw_textf("%s", x+s/2, y-sy, 0, 0, 255, 255, 255, int(blend*inventorystatusblend*255), TEXT_CENTER_UP, -1, -1, 1, state);
                popfont();
            }
            if(inventorystatus&2 && *tex)
            {
                float gr = 1, gg = 1, gb = 1, fade = blend*inventorystatusiconblend;
                if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
                sy += drawitem(tex, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
            }
        }
        if(inventorygameinfo)
        {
            bool over = (!alive && inventorygameinfo&8) || inventorygameinfo&16;
            float gr = 1, gg = 1, gb = 1, fade = blend*inventorygameinfoblend;
            if(inventorytone) skewcolour(gr, gg, gb, inventorytone);
            if(alive && inventorygameinfoflash && lastmillis-game::focus->lastspawn <= inventorygameinfoflash)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                flashcolour(gr, gg, gb, 0.f, 1.f, 1.f, amt);
            }
            #define ADDMODE(a) sy += drawitem(a, x, y-sy, s, 0, false, true, gr, gg, gb, fade);
            if(game::focus->state != CS_EDITING && !m_dm(game::gamemode) && ((alive && inventorygameinfo&1) || over)) ADDMODEICON(game::gamemode, game::mutators)
            if(over && m_multi(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_MULTI))) ADDMODE(modemultitex)
            if(over && m_ffa(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_FFA))) ADDMODE(modeffatex)
            if(over && m_coop(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_COOP))) ADDMODE(modecooptex)
            if(over && m_insta(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_INSTA))) ADDMODE(modeinstatex)
            if(over && m_medieval(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_MEDIEVAL))) ADDMODE(modemedievaltex)
            if(over && m_kaboom(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_KABOOM))) ADDMODE(modekaboomtex)
            if(((alive && inventorygameinfo&4) || over) && m_duel(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_DUEL))) ADDMODE(modedueltex)
            if(((alive && inventorygameinfo&4) || over) && m_survivor(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_SURVIVOR))) ADDMODE(modesurvivortex)
            if(over && m_classic(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_CLASSIC))) ADDMODE(modeclassictex)
            if(over && m_onslaught(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_ONSLAUGHT))) ADDMODE(modeonslaughttex)
            if(((alive && inventorygameinfo&2) || over) && m_freestyle(game::gamemode, game::mutators)) ADDMODE(modefreestyletex)
            if(((alive && inventorygameinfo&2) || over) && m_vampire(game::gamemode, game::mutators)) ADDMODE(modevampiretex)
            if((alive && inventorygameinfo&4) && m_resize(game::gamemode, game::mutators) && !(gametype[game::gamemode].implied&(1<<G_M_RESIZE))) ADDMODE(moderesizetex)
            if(((alive && inventorygameinfo&2) || over) && m_hard(game::gamemode, game::mutators)) ADDMODE(modehardtex)
            if(((alive && inventorygameinfo&2) || over) && m_basic(game::gamemode, game::mutators)) ADDMODE(modebasictex)
            #undef ADDMODE
        }
        return sy;
    }

    int drawtimer(int x, int y, int s, float blend)
    {
        if(game::focus->state == CS_EDITING || game::focus->state == CS_SPECTATOR) return 0;
        int sy = 0;
        if(inventoryrace && m_race(game::gamemode))
        {
            float fade = blend*inventoryraceblend;
            pushfont("default");
            if((!m_ra_gauntlet(game::gamemode, game::mutators) || game::focus->team == T_ALPHA) && (game::focus->cpmillis || game::focus->cptime) && (game::focus->state == CS_ALIVE || game::focus->state == CS_DEAD || game::focus->state == CS_WAITING))
            {
                sy += draw_textf("\falap: \fw%d", x, y-sy, 0, 0, 255, 255, 255, int(fade*255), TEXT_LEFT_UP, -1, -1, 1, game::focus->points+1);
                if(game::focus->cptime)
                    sy += draw_textf("\fy%s", x, y-sy, 0, 0, 255, 255, 255, int(fade*255), TEXT_LEFT_UP, -1, -1, 1, timestr(game::focus->cptime, inventoryracestyle));
                if(game::focus->cpmillis)
                    sy += draw_textf("%s", x, y-sy, 0, 0, 255, 255, 255, int(fade*255), TEXT_LEFT_UP, -1, -1, 1, timestr(lastmillis-game::focus->cpmillis, inventoryracestyle));
                else if(game::focus->cplast)
                    sy += draw_textf("\fzwe%s", x, y-sy, 0, 0, 255, 255, 255, int(fade*255), TEXT_LEFT_UP, -1, -1, 1, timestr(game::focus->cplast, inventoryracestyle));
            }
            sy += raceinventory(x, y-sy, s, fade);
            popfont();
        }
        return sy;
    }

    int drawinventory(int w, int h, int edge, int top, int bottom, float blend)
    {
        int cx[2] = { edge, w-edge }, cy[2] = { h-edge-bottom, h-edge-bottom }, left = edge,
            csl = int(inventoryleft*w), csr = int(inventoryright*w), cr = edge/2, cc = 0, bf = blend*255, bs = (w-edge*2)/2;
        if(!texpaneltimer)
        {
            if(totalmillis-laststats >= statrate)
            {
                memcpy(prevstats, curstats, sizeof(prevstats));
                laststats = totalmillis-(totalmillis%statrate);
            }
            int nextstats[NUMSTATS] = {
                vtris*100/max(wtris, 1), vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024, glde, gbatches, getnumqueries(), rplanes, curfps, bestfpsdiff, worstfpsdiff
            };
            loopi(NUMSTATS) if(prevstats[i] == curstats[i]) curstats[i] = nextstats[i];
            pushfont("consub");
            if(showfps)
            {
                pushfont("console");
                cy[1] -= draw_textf("%d fps", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, curstats[8]);
                popfont();
                switch(showfps)
                {
                    case 3:
                        cy[1] -= draw_textf("+%d-%d range", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, maxfps, curstats[9], curstats[10]);
                    case 2:
                        cy[1] -= draw_textf("%d max", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, maxfps);
                    default: break;
                }
            }
            if(showstats >= (m_edit(game::gamemode) ? 1 : 2))
            {
                cy[1] -= draw_textf("ond:%d va:%d gl:%d(%d) oq:%d", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, allocnodes*8, allocva, curstats[4], curstats[5], curstats[6]);
                cy[1] -= draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, wtris/1024, curstats[0], wverts/1024, curstats[1], curstats[2], curstats[3]);
                cy[1] -= draw_textf("ents:%d(%d) wp:%d lm:%d rp:%d pvs:%d", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1, entities::ents.length(), entgroup.length(), ai::waypoints.length(), lightmaps.length(), curstats[7], getnumviewcells());
                if(game::player1->state == CS_EDITING)
                {
                    cy[1] -= draw_textf("cube:%s%d corner:%d orient:%d grid:%d%s", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1,
                            selchildcount<0 ? "1/" : "", abs(selchildcount), sel.corner, sel.orient, sel.grid, showmat && selchildmat > 0 ? getmaterialdesc(selchildmat, " mat:") : "");
                    cy[1] -= draw_textf("sel:%d,%d,%d %d,%d,%d (%d,%d,%d,%d)", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1,
                            sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z,
                                sel.cx, sel.cxs, sel.cy, sel.cys);
                }
                cy[1] -= draw_textf("pos:%.2f,%.2f,%.2f yaw:%.2f pitch:%.2f", cx[1], cy[1], 0, 0, 255, 255, 255, bf, TEXT_RIGHT_UP, -1, bs, 1,
                        camera1->o.x, camera1->o.y, camera1->o.z, camera1->yaw, camera1->pitch);
            }
            popfont();
        }
        if(!minimal(showinventory, true)) return left;
        float fade = blend*inventoryblend;
        bool interm = !gs_playing(game::gamestate) && game::tvmode() && game::focus == game::player1;
        loopi(2) switch(i)
        {
            case 0:
            {
                bool found = false;
                if((cc = drawhealth(cx[i], cy[i], csl, fade, interm)) > 0) { cy[i] -= cc+cr; found = true; }
                if(!interm && (cc = drawtimer(cx[i], cy[i], csl, fade)) > 0) { cy[i] -= cc+cr; found = true; }
                if(found) left += csl;
                break;
            }
            case 1:
            {
                int cm = top;
                if(!radardisabled && !m_hard(game::gamemode, game::mutators) && radartype() == 3 && !hasinput(true) && (game::focus->state == CS_EDITING ? showeditradar >= 1 : chkcond(showradar, !game::tvmode() || (game::focus != game::player1 && radartype() == 3))))
                    cm += int(max(w, h)/2*radarcorner*2)+cr;
                if(inventorydate)
                    cm += drawitemtextx(cx[i], cm, 0, (inventorybg ? TEXT_SKIN : 0)|TEXT_RIGHT_JUSTIFY, inventorydateskew, "huge", fade, "%s", gettime(currenttime, inventorydateformat))+cr;
                if(inventorytime)
                {
                    if(paused) cm += drawitemtextx(cx[i], cm, 0, (inventorybg ? TEXT_SKIN : 0)|TEXT_RIGHT_JUSTIFY, inventorytimeskew, "huge", fade, "\fs\fopaused\fS", 0xFFFFFF)+cr;
                    else if(m_edit(game::gamemode)) cm += drawitemtextx(cx[i], cm, 0, (inventorybg ? TEXT_SKIN : 0)|TEXT_RIGHT_JUSTIFY, inventorytimeskew, "huge", fade, "\fs\fgediting\fS")+cr;
                    else if(m_play(game::gamemode) || client::demoplayback)
                    {
                        int timecorrected = max(game::timeremaining*1000-((gs_playing(game::gamestate) ? lastmillis : totalmillis)-game::lasttimeremain), 0);
                        if(game::gamestate != G_S_PLAYING)
                            cm += drawitemtextx(cx[i], cm, 0, (inventorybg ? TEXT_SKIN : 0)|TEXT_RIGHT_JUSTIFY, inventorytimeskew, "huge", fade, "%s \fs%s%s\fS", gamestates[0][game::gamestate], gs_waiting(game::gamestate) ? "\fr" : (game::gamestate == G_S_OVERTIME ? (inventorytimeflash ? "\fzoy" : "\fo") : "\fg"), timestr(timecorrected, inventorytimestyle))+cr;
                        else if(timelimit) cm += drawitemtextx(cx[i], cm, 0, (inventorybg ? TEXT_SKIN : 0)|TEXT_RIGHT_JUSTIFY, inventorytimeskew, "huge", fade, "\fs%s%s\fS", timecorrected > 60000 ? "\fg" : (inventorytimeflash ? "\fzgy" : "\fy"), timestr(timecorrected, inventorytimestyle))+cr;
                    }
                }
                if(texpaneltimer) break;
                if(m_play(game::gamemode))
                {
                    int count = game::player1->state == CS_SPECTATOR ? inventoryscorespec : inventoryscore;
                    if(count && ((cc = drawscore(cx[i], cm, csr, (h-edge*2)/2, fade, count)) > 0)) cm += cc+cr;
                }
                if((cc = drawselection(cx[i], cy[i], csr, cm, fade)) > 0) cy[i] -= cc+cr;
                if(inventorygame)
                {
                    if(m_capture(game::gamemode) && ((cc = capture::drawinventory(cx[i], cy[i], csr, cm, fade)) > 0)) cy[i] -= cc+cr;
                    else if(m_defend(game::gamemode) && ((cc = defend::drawinventory(cx[i], cy[i], csr, cm, fade)) > 0)) cy[i] -= cc+cr;
                    else if(m_bomber(game::gamemode) && ((cc = bomber::drawinventory(cx[i], cy[i], csr, cm, fade)) > 0)) cy[i] -= cc+cr;
                }
                break;
            }
            default: break;
        }
        return left;
    }

    void drawdamage(int w, int h, int top, int bottom, float blend)
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
                    drawtexture(0, top, w, h-top-bottom);
                }
            }
        }
    }

    void drawfire(int w, int h, int top, int bottom, float blend)
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
                drawtexture(0, top, w, h-top-bottom);
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

    void drawspecborder(int w, int h, int type, int &top, int &bottom)
    {
        if(type < 0 || type >= BORDER_MAX) return;
        int btype[BORDER_MAX] = { playborder, editborder, specborder, waitborder, backgroundborder };
        if(!btype[type]) return;
        int bcolour[BORDER_MAX] = { playbordertone, editbordertone, specbordertone, waitbordertone, backgroundbordertone };
        float bsize[BORDER_MAX] = { playbordersize, editbordersize, specbordersize, waitbordersize, backgroundbordersize },
              bfade[BORDER_MAX] = { playborderblend, editborderblend, specborderblend, waitborderblend, backgroundborderblend };
        int s = int(h*0.5f*bsize[type]);
        if(!s) return;
        vec col = vec(0, 0, 0);
        if(bcolour[type]) skewcolour(col.x, col.y, col.z, bcolour[type]);
        gle::color(col, bfade[type]);
        loopi(BORDERP_MAX) if(btype[type]&(1<<i))
        {
            const char *bptex = i ? borderbottomtex : bordertoptex;
            if(*bptex)
            {
                Texture *t = textureload(bptex, 3);
                glBindTexture(GL_TEXTURE_2D, t->id);
                float tw = t->w*(s/float(t->h));
                int cw = int(ceilf(w/tw));
                loopk(cw) switch(i)
                {
                    case BORDERP_TOP: drawtexture(k*tw, 0, tw, s); break;
                    case BORDERP_BOTTOM: drawtexture(k*tw, h-s, tw, s); break;
                    default: break;
                }
            }
            else
            {
                usetexturing(false);
                switch(i)
                {
                    case BORDERP_TOP: drawblend(0, 0, w, s, col.r, col.g, col.b, true); break;
                    case BORDERP_BOTTOM: drawblend(0, h-s, w, s, col.r, col.g, col.b, true); break;
                    default: break;
                }
                usetexturing(true);
            }
            switch(i)
            {
                case BORDERP_TOP: top += s; break;
                case BORDERP_BOTTOM: bottom += s; break;
                default: break;
            }
        }
    }

    void drawbackground(int w, int h, int &top, int &bottom)
    {
        gle::colorf(1, 1, 1, 1);

        Texture *t = NULL;
        int mapbg = 0;
        if(showloadingmapbg && *mapname && strcmp(mapname, "maps/untitled"))
        {
            defformatstring(tex, "<blur:2>%s", mapname);
            t = textureload(tex, 3, true, false);
            mapbg = showloadingmapbg;
        }
        if(!t || t == notexture)
        {
            t = textureload(backgroundtex, 3);
            mapbg = 0;
        }
        glBindTexture(GL_TEXTURE_2D, t->id);
        float offsetx = 0, offsety = 0;
        if(showloadingaspect&(1<<mapbg))
        {
            if(w > h) offsety = ((w-h)/float(w))*0.5f;
            else if(h > w) offsetx = ((h-w)/float(h))*0.5f;
        }
        drawquad(0, 0, w, h, offsetx, offsety, 1-offsetx, 1-offsety);

        drawspecborder(w, h, BORDER_BG, top, bottom);

        gle::colorf(1, 1, 1, 1);

        t = textureload(logotex, 3);
        glBindTexture(GL_TEXTURE_2D, t->id);
        drawtexture(w-1024, top, 1024, 256);

        t = textureload(badgetex, 3);
        glBindTexture(GL_TEXTURE_2D, t->id);
        drawtexture(w-336, top, 256, 128);

        pushfont("console");
        int y = h-bottom-FONTH/2;
        bool p = progressing;
        const char *ptitle = progresstitle, *ptext = progresstext;
        float pamt = progressamt, ppart = progresspart;
        if(!p)
        {
            int wait = client::waiting();
            if(wait > 1)
            {
                p = true;
                ptitle = wait == 2 ? "requesting map.." : "downloading map..";
                pamt = ppart = 0;
                ptext = "";
            }
        }
        if(p)
        {
            if(pamt > 0) drawprogress(FONTH, y, 0, pamt, FONTH*2, true, 1, 1, 1, 1, 1, "consub", "\fy%d%%", int(pamt*100));
            else drawprogress(FONTH, y, 0, pamt, FONTH*2, true, 1, 1, 1, 1, 1, "consub", "\fg...");
            y -= FONTH/2;
            if(*ptext) y -= draw_textf("%s %s [\fs\fa%d%%\fS]", FONTH*7/2, y, 0, 0, 255, 255, 255, 255, TEXT_LEFT_UP, -1, -1, 1, *ptitle ? ptitle : "please wait...", ptext, int(ppart*100));
            else y -= draw_textf("%s", FONTH*7/2, y, 0, 0, 255, 255, 255, 255, TEXT_LEFT_UP, -1, -1, 1, *ptitle ? ptitle : "please wait...");
        }
        y = h-bottom-FONTH/2;
        if(showloadinggpu) y -= draw_textf("%s (%s v%s)", w-FONTH, y, 0, 0, 255, 255, 255, 255, TEXT_RIGHT_UP, -1, -1, 1, gfxrenderer, gfxvendor, gfxversion);
        if(showloadingversion) y -= draw_textf("%s v%s-%s%d-%s (%s)", w-FONTH, y, 0, 0, 255, 255, 255, 255, TEXT_RIGHT_UP, -1, -1, 1, VERSION_NAME, VERSION_STRING, versionplatname, versionarch, versionbranch, VERSION_RELEASE);
        if(showloadingurl && *VERSION_URL) y -= draw_textf("%s", w-FONTH, y, 0, 0, 255, 255, 255, 255, TEXT_RIGHT_UP, -1, -1, 1, VERSION_URL);
        popfont();
    }

    int drawheadsup(int w, int h, int edge, int &top, int &bottom, float fade)
    {
        if(underlaydisplay >= 2 || (game::focus->state == CS_ALIVE && (underlaydisplay || !game::thirdpersonview(true))))
        {
            Texture *t = *underlaytex ? textureload(underlaytex, 3) : notexture;
            if(t != notexture)
            {
                glBindTexture(GL_TEXTURE_2D, t->id);
                gle::colorf(1.f, 1.f, 1.f, underlayblend*hudblend);
                drawtexture(0, top, w, h-top-bottom);
            }
        }
        if(gs_playing(game::gamestate))
        {
            bool third = game::thirdpersonview(true) && game::focus != game::player1;
            if(game::focus->state == CS_ALIVE && game::inzoom()) drawzoom(w, h);
            if(showdamage && !third)
            {
                if(burntime && game::focus->state == CS_ALIVE) drawfire(w, h, top, bottom, fade);
                drawdamage(w, h, top, bottom, fade);
            }
            if(teamhurthud&2 && teamhurttime && m_team(game::gamemode, game::mutators) && game::focus == game::player1 && game::player1->lastteamhit >= 0 && lastmillis-game::player1->lastteamhit <= teamhurttime)
            {
                vec targ;
                bool hasbound = false;
                int dist = teamhurtdist ? teamhurtdist : getworldsize();
                loopv(game::players) if(game::players[i] && game::players[i]->team == game::player1->team)
                {
                    if(game::players[i]->lastteamhit < 0 || lastmillis-game::players[i]->lastteamhit > teamhurttime) continue;
                    if(!getsight(camera1->o, camera1->yaw, camera1->pitch, game::players[i]->o, targ, dist, curfov, fovy)) continue;
                    if(!hasbound)
                    {
                        Texture *t = textureload(warningtex, 3);
                        glBindTexture(GL_TEXTURE_2D, t->id);
                        float amt = float(lastmillis%250)/250.f, value = (amt > 0.5f ? 1.f-amt : amt)*2.f;
                        gle::colorf(value, value*0.125f, value*0.125f, value);
                        hasbound = true;
                    }
                    float cx = 0.5f, cy = 0.5f, cz = 1;
                    if(vectocursor(game::players[i]->o, cx, cy, cz))
                    {
                        int s = int(teamhurtsize*w), sx = int(cx*w-s), sy = int(cy*h-s);
                        drawsized(sx, sy, s*2);
                    }
                }
            }
            if(!radardisabled && !m_hard(game::gamemode, game::mutators) && !hasinput(true) && (game::focus->state == CS_EDITING ? showeditradar >= 1 : chkcond(showradar, !game::tvmode() || (game::focus != game::player1 && radartype() == 3))))
                drawradar(w, h, fade);
        }
        drawspecborder(w, h, !gs_playing(game::gamestate) || game::player1->state == CS_SPECTATOR ? BORDER_SPEC : (game::player1->state == CS_WAITING ? BORDER_WAIT : (game::player1->state == CS_WAITING ? BORDER_EDIT : BORDER_PLAY)), top, bottom);
        return drawinventory(w, h, edge, top, bottom, fade);
    }

    void drawhud(bool noview)
    {
        hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
        flushhudmatrix();

        float fade = hudblend, consolefade = hudblend;
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
            if(uifade && (uimillis > 0 || totalmillis-abs(uimillis) <= uifade))
            {
                float n = min(float(totalmillis-abs(uimillis))/float(uifade), 1.f), a = n*uifadeamt;
                if(uimillis > 0) a = 1.f-a;
                else a += (1.f-uifadeamt);
                loopi(3) if(a < colour[i]) colour[i] *= a;
            }
            if(!noview)
            {
                if(titlefade && (client::waiting() || lastmillis-game::maptime <= titlefade))
                {
                    float a = !client::waiting() ? float(lastmillis-game::maptime)/float(titlefade) : 0.f;
                    loopi(3) if(a < colour[i]) colour[i] *= a;
                }
                if(tvmodefade && game::tvmode())
                {
                    float a = game::lasttvchg ? (lastmillis-game::lasttvchg <= tvmodefade ? float(lastmillis-game::lasttvchg)/float(tvmodefade) : 1.f) : 0.f;
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
                            skewcolour(col.x, col.y, col.z, game::getcolour(game::focus, game::playereffecttone));
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
                float amt = (colour.x+colour.y+colour.z)/3.f;
                if(commandfadeskew < 1 && (!commandmillis || (commandmillis < 0 && totalmillis-abs(commandmillis) > commandfade)))
                    consolefade *= amt+((1.f-amt)*commandfadeskew);
                fade *= amt;
            }
        }

        int edge = int(hudsize*edgesize), left = 0, top = 0, bottom = 0;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gle::colorf(1, 1, 1);

        if(noview) drawbackground(hudwidth, hudheight, top, bottom);
        else if(!client::waiting())
        {
            if(showhud)
            {
                left += drawheadsup(hudwidth, hudheight, edge, top, bottom, fade);
                if(showevents && !texpaneltimer && !game::tvmode() && !client::waiting() && !hasinput(false)) drawevents(fade);
            }
            else if(gs_playing(game::gamestate) && game::focus == game::player1 && game::focus->state == CS_ALIVE && game::inzoom())
                drawzoom(hudwidth, hudheight);
        }
        drawconsole(showconsole < 2 || noview ? 0 : 1, hudwidth, hudheight, edge*2, edge+top, hudwidth-edge*2, consolefade);
        if(showconsole >= 2 && !noview && showconsole && showhud)
            drawconsole(2, hudwidth, hudheight, left, hudheight-edge-bottom, showfps >= 2 || showstats >= (m_edit(game::gamemode) ? 1 : 2) ? (hudwidth-left*2)/2-edge*4 : ((hudwidth-left*2)/2-edge*4)*2, consolefade);
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
