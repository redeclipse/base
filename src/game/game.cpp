#define CPP_GAME_MAIN 1
#include "game.h"

namespace game
{
    int nextmode = G_EDITMODE, nextmuts = 0, gamestate = G_S_WAITING, gamemode = G_EDITMODE, mutators = 0, maptime = 0, mapstart = 0, timeremaining = 0, timeelapsed = 0, timelast = 0,
        lastcamera = 0, lasttvcam = 0, lasttvchg = 0, lastzoom = 0, spectvfollowing = -1, lastcamcn = -1;
    bool zooming = false, inputmouse = false, inputview = false, inputmode = false, wantsloadoutmenu = false;
    float swayfade = 0, swayspeed = 0, swaydist = 0, bobfade = 0, bobdist = 0;
    vec swaydir(0, 0, 0), swaypush(0, 0, 0);
    int attrmap[W_MAX] = {0};

    gameent *player1 = new gameent(), *focus = player1, *lastfocus = focus;
    avatarent avatarmodel, bodymodel;
    vector<gameent *> players, waiting;
    vector<cament *> cameras;

    void mapweapsounds()
    {
        static const char *nameprefixes[W_MAX] =
        {
            "S_W_CLAW_", "S_W_PISTOL_",
            "S_W_SWORD_", "S_W_SHOTGUN_", "S_W_SMG_", "S_W_FLAMER_", "S_W_PLASMA_", "S_W_ZAPPER_", "S_W_RIFLE_",
            "S_W_GRENADE_", "S_W_MINE_", "S_W_ROCKET_"
        };

        static const char *names[S_W_MAX] =
        {
            "PRIMARY", "SECONDARY",
            "BEGIN", "END",
            "BEGIN2", "END2",
            "POWER", "POWER2", "ZOOM",
            "SWITCH", "RELOAD", "NOTIFY",
            "EXPLODE", "EXPLODE2",
            "DESTROY", "DESTROY2",
            "IMPACT", "IMPACT2",
            "EXTINGUISH", "EXTINGUISH2",
            "TRANSIT", "TRANSIT2",
            "BOUNCE", "BOUNCE2",
            "USE", "SPAWN"
        };

        loopi(W_MAX) loopj(S_W_MAX)
        {
            if(i < W_OFFSET && j >= S_W_OFFSET) continue;

            string slotname;
            formatstring(slotname, "%s%s", nameprefixes[i], names[j]);
            mapsoundslot(WSND(i, j), slotname);
        }
    }

    void mapgamesounds()
    {
        static const char *names[S_GAME - S_GAMESPECIFIC] =
        {
            "S_JUMP", "S_IMPULSE", "S_LAND", "S_FOOTSTEP", "S_SWIMSTEP", "S_PAIN", "S_DEATH",
            "S_SPLASH1", "S_SPLASH2", "S_SPLOSH", "S_DEBRIS", "S_BURNLAVA",
            "S_EXTINGUISH", "S_SHELL", "S_ITEMUSE", "S_ITEMSPAWN",
            "S_REGEN_BEGIN", "S_REGEN", "S_CRITICAL", "S_DAMAGE", "S_DAMAGE2", "S_DAMAGE3", "S_DAMAGE4", "S_DAMAGE5", "S_DAMAGE6", "S_DAMAGE7", "S_DAMAGE8",
            "S_BURNED", "S_BLEED", "S_SHOCK", "S_RESPAWN", "S_CHAT", "S_ERROR", "S_ALARM", "S_CATCH", "S_BOUNCE",
            "S_V_FLAGSECURED", "S_V_FLAGOVERTHROWN", "S_V_FLAGPICKUP", "S_V_FLAGDROP", "S_V_FLAGRETURN", "S_V_FLAGSCORE", "S_V_FLAGRESET",
            "S_V_BOMBSTART", "S_V_BOMBDUEL", "S_V_BOMBPICKUP", "S_V_BOMBSCORE", "S_V_BOMBRESET",
            "S_V_NOTIFY", "S_V_FIGHT", "S_V_START", "S_V_CHECKPOINT", "S_V_COMPLETE", "S_V_OVERTIME", "S_V_ONEMINUTE", "S_V_HEADSHOT",
            "S_V_SPREE", "S_V_SPREE2", "S_V_SPREE3", "S_V_SPREE4", "S_V_MULTI", "S_V_MULTI2", "S_V_MULTI3",
            "S_V_REVENGE", "S_V_DOMINATE", "S_V_FIRSTBLOOD", "S_V_BREAKER",
            "S_V_YOUWIN", "S_V_YOULOSE", "S_V_DRAW", "S_V_FRAGGED", "S_V_BALWARN", "S_V_BALALERT"
        };

        for(int i = S_GAMESPECIFIC; i < S_GAME; i++)
            mapsoundslot(i, names[i - S_GAMESPECIFIC]);

        mapweapsounds();
    }

    void start()
    {
        player1->version.major = VERSION_MAJOR;
        player1->version.minor = VERSION_MINOR;
        player1->version.patch = VERSION_PATCH;
        player1->version.game = VERSION_GAME;
        player1->version.build = versionbuild;
        player1->version.platform = versionplatform;
        player1->version.arch = versionarch;
        player1->version.gpuglver = glversion;
        player1->version.gpuglslver = glslversion;
        player1->version.crc = versioncrc;
        if(player1->version.branch) delete[] player1->version.branch;
        player1->version.branch = newstring(versionbranch, MAXBRANCHLEN);
        if(player1->version.revision) delete[] player1->version.branch;
        player1->version.revision = newstring(versionrevision, MAXREVISIONLEN);
        if(player1->version.gpuvendor) delete[] player1->version.gpuvendor;
        player1->version.gpuvendor = newstring(gfxvendor);
        if(player1->version.gpurenderer) delete[] player1->version.gpurenderer;
        player1->version.gpurenderer = newstring(gfxrenderer);
        if(player1->version.gpuversion) delete[] player1->version.gpuversion;
        player1->version.gpuversion = newstring(gfxversion);
    }

    #define ILLUMVARS(name) \
        FVAR(IDF_WORLD, illumlevel##name, 0, 0, 2); \
        VAR(IDF_WORLD, illumradius##name, 0, 0, VAR_MAX);

    ILLUMVARS();
    ILLUMVARS(alt);

    #define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALT)) return name##alt; \
        return name; \
    }

    GETMPV(illumlevel, float);
    GETMPV(illumradius, int);

    #define OBITVARS(name) \
        SVAR(IDF_WORLD, obit##name, ""); \
        SVAR(IDF_WORLD, obit##name##2, ""); \
        SVAR(IDF_WORLD, obit##name##3, ""); \
        SVAR(IDF_WORLD, obit##name##4, ""); \
        const char *getobit##name(int mat, const char *def = NULL) \
        { \
            loopi(2) switch(i ? 0 : mat&MATF_INDEX) \
            { \
                default: case 0: if(!def || *obit##name) return obit##name; break; \
                case 1: if(*obit##name##2) return obit##name##2; break; \
                case 2: if(*obit##name##3) return obit##name##3; break; \
                case 3: if(*obit##name##4) return obit##name##4; break; \
            } \
            return def ? def : ""; \
        }
    OBITVARS(lava)
    OBITVARS(water)
    SVAR(IDF_WORLD, obitdeath, "");
    SVAR(IDF_WORLD, obithurt, "");
    SVAR(IDF_WORLD, obitfall, "");
    SVAR(IDF_WORLD, obittouch, "");
    SVAR(IDF_WORLD, obitcrush, "");

    void stopmapmusic()
    {
        if(connected() && maptime > 0) musicdone(true);
    }
    VARF(IDF_PERSIST, musictype, 0, 1, 6, stopmapmusic()); // 0 = no in-game music, 1 = map music (or random if none), 2 = always random, 3 = map music (silence if none), 4-5 = same as 1-2 but pick new tracks when done, 6 = always use theme song
    VARF(IDF_PERSIST, musicedit, -1, 0, 6, stopmapmusic()); // same as above for editmode, -1 = use musictype
    SVARF(IDF_PERSIST, musicdir, "sounds/music", stopmapmusic());
    SVARF(IDF_WORLD, mapmusic, "", stopmapmusic());

    VAR(IDF_PERSIST, thirdperson, 0, 0, 1);
    VAR(IDF_PERSIST, dynlighteffects, 0, 2, 2);
    VAR(IDF_PERSIST, fullbrightfocus, 0, 0, 3); // bitwise: 0 = don't fullbright focus, 1 = fullbright non-player1, 2 = fullbright player1

    VAR(IDF_PERSIST, thirdpersonmodel, 0, 1, 1);
    VAR(IDF_PERSIST, thirdpersonfov, 90, 120, 150);
    FVAR(IDF_PERSIST, thirdpersonblend, 0, 1, 1);
    VAR(IDF_PERSIST, thirdpersoninterp, 0, 100, VAR_MAX);
    FVAR(IDF_PERSIST, thirdpersondist, FVAR_NONZERO, 14, 20);

    FVAR(IDF_PERSIST, thirdpersonside, -20, 7, 20);
    VAR(IDF_PERSIST, thirdpersoncursor, 0, 1, 2);
    FVAR(IDF_PERSIST, thirdpersoncursorx, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, thirdpersoncursory, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, thirdpersoncursordist, 0, 256, FVAR_MAX);

    VARF(0, follow, -1, -1, VAR_MAX, followswitch(0));

    VAR(IDF_PERSIST, firstpersonmodel, 0, 3, 3);
    VAR(IDF_PERSIST, firstpersonfov, 90, 100, 150);
    FVAR(IDF_PERSIST, firstpersondepth, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, firstpersonbodydepth, 0, 0.65f, 1);
    FVAR(IDF_PERSIST, firstpersondepthfov, 0, 70, 150);
    FVAR(IDF_PERSIST, firstpersonbodydepthfov, 0, 0, 150);

    FVAR(IDF_PERSIST, firstpersonbodydist, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodyside, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodypitch, -1, 1, 1);
    FVAR(IDF_PERSIST, firstpersonbodyzoffset, 0, 1, 10);
    FVAR(IDF_PERSIST, firstpersonbodypitchadjust, 0, 0.75f, 10);

    FVAR(IDF_PERSIST, firstpersonspine, 0, 0.45f, 1);
    FVAR(IDF_PERSIST, firstpersonpitchmin, 0, 90, 90);
    FVAR(IDF_PERSIST, firstpersonpitchmax, 0, 45, 90);
    FVAR(IDF_PERSIST, firstpersonpitchscale, -1, 1, 1);

    VAR(IDF_PERSIST, firstpersonsway, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersonswayslide, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, firstpersonswaymin, 0, 0.15f, 1);
    FVAR(IDF_PERSIST, firstpersonswaystep, 1, 35.f, 1000);
    FVAR(IDF_PERSIST, firstpersonswayside, 0, 0.05f, 10);
    FVAR(IDF_PERSIST, firstpersonswayup, 0, 0.06f, 10);

    VAR(IDF_PERSIST, firstpersonbob, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersonbobmin, 0, 0.2f, 1);
    FVAR(IDF_PERSIST, firstpersonbobstep, 1, 28.f, 1000);
    FVAR(IDF_PERSIST, firstpersonbobroll, 0, 0.3f, 10);
    FVAR(IDF_PERSIST, firstpersonbobside, 0, 0.6f, 10);
    FVAR(IDF_PERSIST, firstpersonbobup, 0, 0.6f, 10);
    FVAR(IDF_PERSIST, firstpersonbobtopspeed, 0, 50, 1000);
    FVAR(IDF_PERSIST, firstpersonbobfocusmindist, 0, 64, 10000);
    FVAR(IDF_PERSIST, firstpersonbobfocusmaxdist, 0, 256, 10000);
    FVAR(IDF_PERSIST, firstpersonbobfocus, 0, 0.5f, 1);

    VAR(IDF_PERSIST, firstpersonslidetime, 0, 200, VAR_MAX);
    FVAR(IDF_PERSIST, firstpersonslideroll, -89.9f, -5.f, 89.9f);

    VAR(IDF_PERSIST, editfov, 90, 120, 150);
    VAR(IDF_PERSIST, specfov, 90, 120, 150);

    VAR(IDF_PERSIST, specresetstyle, 0, 1, 1); // 0 = back to player1, 1 = stay at camera

    VARF(IDF_PERSIST, specmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, waitmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, intermmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv

    VAR(IDF_PERSIST, followdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, followthirdperson, 0, 1, 1);
    VAR(IDF_PERSIST, followaiming, 0, 0, 3); // 0 = don't aim, &1 = aim in thirdperson, &2 = aim in first person
    FVAR(IDF_PERSIST, followdist, FVAR_NONZERO, 10, FVAR_MAX);
    FVAR(IDF_PERSIST, followside, FVAR_MIN, 8, FVAR_MAX);
    FVAR(IDF_PERSIST, followblend, 0, 1, 1);

    VAR(IDF_PERSIST, followtvspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, followtvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, followtvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, followtvrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, followtvyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, followtvpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, followtvyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, followtvpitchthresh, 0, 0, 180);

    VAR(IDF_PERSIST, spectvmintime, 1000, 3000, VAR_MAX);
    VAR(IDF_PERSIST, spectvtime, 1000, 7500, VAR_MAX);
    VAR(IDF_PERSIST, spectvmaxtime, 0, 15000, VAR_MAX);
    VAR(IDF_PERSIST, spectvspeed, 1, 350, VAR_MAX);
    VAR(IDF_PERSIST, spectvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvpitchthresh, 0, 0, 180);
    VAR(IDF_PERSIST, spectvdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfirstperson, 0, 0, 2); // 0 = aim in direction followed player is facing, 1 = aim in direction determined by spectv when dead, 2 = always aim in direction
    VAR(IDF_PERSIST, spectvthirdperson, 0, 2, 2); // 0 = aim in direction followed player is facing, 1 = aim in direction determined by spectv when dead, 2 = always aim in direction

    VAR(IDF_PERSIST, spectvintermmintime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermtime, 1000, 7500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvintermrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvintermyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvintermpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvintermyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvintermpitchthresh, 0, 0, 180);

    VARF(0, spectvfollow, -1, -1, VAR_MAX, spectvfollowing = spectvfollow); // attempts to always keep this client in view
    VAR(0, spectvfollowself, 0, 1, 2); // if we are not spectating, spectv should show us; 0 = off, 1 = not duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfollowmintime, 1000, 3000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowtime, 1000, 7500, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowmaxtime, 0, 15000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowspeed, 1, 350, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowyawspeed, 1, 350, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowpitchspeed, 1, 350, VAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvfollowyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvfollowpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvfollowyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvfollowpitchthresh, 0, 0, 180);

    FVAR(IDF_PERSIST, spectvmindist, 0, 32, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvmaxdist, 0, 512, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvmovedist, 0, 64, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowmindist, 0, 16, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowmaxdist, 0, 256, FVAR_MAX);

    VAR(IDF_PERSIST, deathcamstyle, 0, 2, 2); // 0 = no follow, 1 = follow attacker, 2 = follow self
    VAR(IDF_PERSIST, deathcamspeed, 0, 500, VAR_MAX);

    VAR(IDF_PERSIST, mouseinvert, 0, 0, 1);
    FVAR(IDF_PERSIST, sensitivity, 1e-4f, 10, 10000);
    FVAR(IDF_PERSIST, sensitivityscale, 1e-4f, 100, 10000);
    FVAR(IDF_PERSIST, yawsensitivity, 1e-4f, 1, 10000);
    FVAR(IDF_PERSIST, pitchsensitivity, 1e-4f, 1, 10000);
    FVAR(IDF_PERSIST, mousesensitivity, 1e-4f, 1, 10000);
    FVAR(IDF_PERSIST, zoomsensitivity, 0, 0.65f, 1000);

    VARF(IDF_PERSIST, zoomlevel, 0, 4, 10, checkzoom());
    VAR(IDF_PERSIST, zoomlevels, 1, 5, 10);
    VAR(IDF_PERSIST, zoomdefault, -1, -1, 10); // -1 = last used, else defines default level
    VAR(IDF_PERSIST, zoomoffset, 0, 2, 10); // if zoomdefault = -1, then offset from zoomlevels this much for initial default
    VAR(IDF_PERSIST, zoomscroll, 0, 0, 1); // 0 = stop at min/max, 1 = go to opposite end

    VAR(IDF_PERSIST, aboveheadaffinity, 0, 0, 1);
    VAR(IDF_PERSIST, aboveheaddead, 0, 1, 1);
    VAR(IDF_PERSIST, aboveheadnames, 0, 1, 1);
    VAR(IDF_PERSIST, aboveheadinventory, 0, 0, 2); // 0 = off, 1 = weapselect only, 2 = all weapons
    VAR(IDF_PERSIST, aboveheadstatus, 0, 1, 1);
    VAR(IDF_PERSIST, aboveheadteam, 0, 1, 3);
    VAR(IDF_PERSIST, aboveheaddamage, 0, 0, 1);
    VAR(IDF_PERSIST, aboveheadicons, 0, 5, 7);
    FVAR(IDF_PERSIST, aboveheadblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadnamesblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadinventoryblend, 0.f, 0.8f, 1.f);
    FVAR(IDF_PERSIST, aboveheadinventoryfade, 0.f, 0.5f, 1.f);
    FVAR(IDF_PERSIST, aboveheadstatusblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadiconsblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadnamessize, 0, 4, 10);
    FVAR(IDF_PERSIST, aboveheadinventorysize, 0, 4, 10);
    FVAR(IDF_PERSIST, aboveheadstatussize, 0, 3.f, 10);
    FVAR(IDF_PERSIST, aboveheadiconssize, 0, 2.5f, 10);
    FVAR(IDF_PERSIST, aboveheadeventsize, 0, 3, 10);

    FVAR(IDF_PERSIST, aboveitemiconsize, 0, 3.f, 10);
    FVAR(IDF_PERSIST, aboveheadsmooth, 0, 0.25f, 1);
    VAR(IDF_PERSIST, aboveheadsmoothmillis, 1, 100, 10000);

    VAR(IDF_PERSIST, eventiconfade, 500, 8000, VAR_MAX);
    VAR(IDF_PERSIST, eventiconshort, 500, 3500, VAR_MAX);

    VAR(IDF_PERSIST, showobituaries, 0, 4, 5); // 0 = off, 1 = only me, 2 = 1 + announcements, 3 = 2 + but dying bots, 4 = 3 + but bot vs bot, 5 = all
    VAR(IDF_PERSIST, showobitdists, 0, 2, 2); // 0 = off, 1 = self only, 2 = all
    VAR(IDF_PERSIST, showobithpleft, 0, 2, 2); // 0 = off, 1 = self only, 2 = all
    VAR(IDF_PERSIST, obitannounce, 0, 2, 2); // 0 = off, 1 = only focus, 2 = everyone
    VAR(IDF_PERSIST, obitverbose, 0, 1, 1); // 0 = extremely simple, 1 = regular messages
    VAR(IDF_PERSIST, obitstyles, 0, 1, 1); // 0 = no obituary styles, 1 = show sprees/dominations/etc

    VAR(IDF_PERSIST, damageinteger, 0, 1, 1);
    FVAR(IDF_PERSIST, damagedivisor, FVAR_NONZERO, 10, FVAR_MAX);
    FVAR(IDF_PERSIST, damagecritical, 0, 0.25f, 1);
    VAR(IDF_PERSIST, damagecriticalsound, 0, 1, 3);
    VAR(IDF_PERSIST, damagemergedelay, 0, 75, VAR_MAX);
    VAR(IDF_PERSIST, damagemergeburn, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, damagemergebleed, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, damagemergeshock, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, playdamagetones, 0, 1, 3);
    VAR(IDF_PERSIST, damagetonevol, -1, -1, 255);
    VAR(IDF_PERSIST, playreloadnotify, 0, 3, 15);
    VAR(IDF_PERSIST, reloadnotifyvol, -1, -1, 255);

    VAR(IDF_PERSIST, deathanim, 0, 2, 3); // 0 = hide player when dead, 1 = old death animation, 2 = ragdolls, 3 = ragdolls, but hide in duke
    VAR(IDF_PERSIST, deathfade, 0, 1, 1); // 0 = don't fade out dead players, 1 = fade them out
    VAR(IDF_PERSIST, deathfademin, 0, 500, VAR_MAX);
    VAR(IDF_PERSIST, deathfademax, 0, 5000, VAR_MAX);
    VAR(IDF_PERSIST, deathfadekamikaze, 0, 750, VAR_MAX);
    VAR(IDF_PERSIST, deathfadeedit, 0, 3000, VAR_MAX);
    VAR(IDF_PERSIST, deathbuttonmash, 0, 1000, VAR_MAX);
    FVAR(IDF_PERSIST, bloodscale, 0, 1, 1000);
    VAR(IDF_PERSIST, bloodfade, 1, 5000, VAR_MAX);
    VAR(IDF_PERSIST, bloodsize, 1, 50, 1000);
    VAR(IDF_PERSIST, bloodsparks, 0, 0, 1);
    FVAR(IDF_PERSIST, debrisscale, 0, 1, 1000);
    VAR(IDF_PERSIST, debrisfade, 1, 5000, VAR_MAX);
    FVAR(IDF_PERSIST, gibscale, 0, 1, 1000);
    VAR(IDF_PERSIST, gibfade, 1, 5000, VAR_MAX);
    FVAR(IDF_PERSIST, impulsescale, 0, 1, 1000);
    VAR(IDF_PERSIST, impulsefade, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, ragdolleffect, 2, 500, VAR_MAX);

    FVAR(IDF_PERSIST, playerblend, 0, 1, 1);
    FVAR(IDF_PERSIST, playereditblend, 0, 1, 1);
    FVAR(IDF_PERSIST, playerghostblend, 0, 0.35f, 1);

    VAR(IDF_PERSIST, playerovertone, -1, CTONE_TEAM, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerundertone, -1, CTONE_TMIX, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerdisplaytone, -1, CTONE_TONE, CTONE_MAX-1);
    VAR(IDF_PERSIST, playereffecttone, -1, CTONE_TEAMED, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerteamtone, -1, CTONE_TEAM, CTONE_MAX-1);

    FVAR(IDF_PERSIST, playerovertonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerundertonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerdisplaytonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playereffecttonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerteamtonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playertonemix, 0, 0.5f, 1);

    FVAR(IDF_PERSIST, playerovertoneinterp, 0, 0, 1); // interpolate this much brightness from the opposing tone
    FVAR(IDF_PERSIST, playerovertonebright, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerundertoneinterp, 0, 0, 1); // interpolate this much brightness from the opposing tone
    FVAR(IDF_PERSIST, playerundertonebright, 0.f, 1.f, 10.f);

    VAR(IDF_PERSIST, playerhint, 0, 15, 15);
    VAR(IDF_PERSIST, playerhinthurt, 0, 1, 1);
    VAR(IDF_PERSIST, playerhinthurtthrob, 0, 1, 1);
    VAR(IDF_PERSIST, playerhinttone, -1, CTONE_TEAMED, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerhinttonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerhintblend, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, playerhintscale, 0, 0.7f, 1); // scale blend depending on health
    FVAR(IDF_PERSIST, playerhintlight, 0, 0.35f, 1); // override for light effect
    FVAR(IDF_PERSIST, playerhintdom, 0, 0.35f, 1); // override for dominate effect
    FVAR(IDF_PERSIST, playerhintsize, 0, 1.25f, 2);
    FVAR(IDF_PERSIST, playerhintmaxsize, 0, 20, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhintfadeat, 0, 64, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhintfadecut, 0, 8, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhinthurtblend, 0, 0.9f, 1);
    FVAR(IDF_PERSIST, playerhinthurtsize, 0, 1.3f, 2);

    VAR(IDF_PERSIST, affinityhint, 0, 1, 1);
    FVAR(IDF_PERSIST, affinityhintblend, 0, 0.15f, 1);
    FVAR(IDF_PERSIST, affinityhintsize, 0, 1.1f, 2);
    FVAR(IDF_PERSIST, affinityhintfadeat, 0, 32, FVAR_MAX);
    FVAR(IDF_PERSIST, affinityhintfadecut, 0, 4, FVAR_MAX);
    FVAR(IDF_PERSIST, affinityfollowblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, affinitythirdblend, 0, 0.5f, 1);

    FVAR(IDF_PERSIST, bombertargetnamefadeat, 0, 20, FVAR_MAX);
    FVAR(IDF_PERSIST, bombertargetnamefadecut, 0, 15, FVAR_MAX);

    VAR(IDF_PERSIST, footstepsounds, 0, 3, 3); // 0 = off, &1 = focus, &2 = everyone else
    FVAR(IDF_PERSIST, footstepsoundmin, 0, 0, FVAR_MAX); // minimum velocity magnitude
    FVAR(IDF_PERSIST, footstepsoundmax, 0, 150, FVAR_MAX); // maximum velocity magnitude
    FVAR(IDF_PERSIST, footstepsoundlevel, 0, 1, 10); // a way to scale the volume
    FVAR(IDF_PERSIST, footstepsoundfocus, 0, 0.85f, 10); // focused player version of above
    FVAR(IDF_PERSIST, footstepsoundlight, 0, 0.5f, 10); // crouch/walk player version of above
    VAR(IDF_PERSIST, footstepsoundminvol, 0, 64, 255);
    VAR(IDF_PERSIST, footstepsoundmaxvol, 0, 255, 255);
    VAR(IDF_PERSIST, footstepsoundminrad, -1, -1, 255);
    VAR(IDF_PERSIST, footstepsoundmaxrad, -1, -1, 255);

    VAR(IDF_PERSIST, nogore, 0, 0, 2); // turns off all gore, 0 = off, 1 = replace, 2 = remove
    VAR(IDF_PERSIST, forceplayermodel, -1, -1, PLAYERTYPES-1);
    VAR(IDF_PERSIST, forceplayerpattern, -1, -1, PLAYERPATTERNS-1);
    VAR(IDF_PERSIST, vanitymodels, 0, 1, 1);
    FVAR(IDF_PERSIST, vanitymaxdist, FVAR_NONZERO, 1024, FVAR_MAX);
    VAR(IDF_PERSIST, headlessmodels, 0, 1, 1);
    FVAR(IDF_PERSIST, twitchspeed, 0, 2.5f, FVAR_MAX);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, mixerburntex, "textures/residuals/burn", 0);
    FVAR(IDF_PERSIST, mixerburnblend, 0.f, 1.f, 1.f);
    FVAR(IDF_PERSIST, mixerburnintensity, 0.f, 1.f, 1.f);
    FVAR(IDF_PERSIST, mixerburnglowblend, 0.f, 0.25f, 1.f);
    FVAR(IDF_PERSIST, mixerburnglowintensity, 0.f, 1.f, 20);
    FVAR(IDF_PERSIST, mixerburnscroll1, -16, 0.15f, 16);
    FVAR(IDF_PERSIST, mixerburnscroll2, -16, 0.25f, 16);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, mixerbleedtex, "textures/residuals/bleed", 0);
    FVAR(IDF_PERSIST, mixerbleedblend, 0.f, 0.8f, 1.f);
    FVAR(IDF_PERSIST, mixerbleedintensity, 0.f, 0.8f, 1.f);
    FVAR(IDF_PERSIST, mixerbleedglowblend, 0.f, 0.25f, 1.f);
    FVAR(IDF_PERSIST, mixerbleedglowintensity, 0.f, 1.f, 20);
    FVAR(IDF_PERSIST, mixerbleedscroll1, -16, 0.065f, 16);
    FVAR(IDF_PERSIST, mixerbleedscroll2, -16, -0.125f, 16);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, mixershocktex, "textures/residuals/shock", 0);
    FVAR(IDF_PERSIST, mixershockblend, 0.f, 0.9f, 1.f);
    FVAR(IDF_PERSIST, mixershockintensity, 0.f, 0.9f, 1.f);
    FVAR(IDF_PERSIST, mixershockglowblend, 0.f, 0.125f, 1.f);
    FVAR(IDF_PERSIST, mixershockglowintensity, 0.f, 1.f, 20);
    FVAR(IDF_PERSIST, mixershockscroll1, -16, 0.25f, 16);
    FVAR(IDF_PERSIST, mixershockscroll2, -16, 0.45f, 16);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, mixerbufftex, "textures/residuals/buff", 0);
    FVAR(IDF_PERSIST, mixerbuffblend, 0.f, 0.4f, 1.f);
    FVAR(IDF_PERSIST, mixerbuffintensity, 0.f, 0.9f, 1.f);
    FVAR(IDF_PERSIST, mixerbuffglowblend, 0.f, 0.25f, 1.f);
    FVAR(IDF_PERSIST, mixerbuffglowintensity, 0.f, 1.f, 20);
    FVAR(IDF_PERSIST, mixerbuffscroll1, -16, 0.125f, 16);
    FVAR(IDF_PERSIST, mixerbuffscroll2, -16, -0.125f, 16);
    VAR(IDF_PERSIST, mixerbuffpulse, 0, 0, VAR_MAX);
    FVAR(IDF_PERSIST, mixerbuffpulsemin, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, mixerbuffpulsemax, 0, 1, 1);
    VAR(IDF_PERSIST, mixerbufftone, -1, -1, CTONE_MAX-1);

    ICOMMAND(0, gamemode, "", (), intret(gamemode));
    ICOMMAND(0, mutators, "", (), intret(mutators));

    int mutscheck(int g, int m, int t)
    {
        int mode = g, muts = m;
        modecheck(mode, muts, t);
        return muts;
    }

    int gametimeremain()
    {
        return connected() ? max(timeremaining*1000-((gs_timeupdate(gamestate) ? lastmillis : totalmillis)-timelast), 0) : 0;
    }

    int gametimeelapsed(bool force)
    {
        return connected() && gs_timeupdate(gamestate) ? max(timeelapsed+(lastmillis-timelast), 0) : (force && maptime > 0 ? lastmillis-maptime : 0);
    }

    const char *gamestatename(int type)
    {
        return connected() && gamestate >= 0 && gamestate < G_S_MAX ? gamestates[clamp(type, 0, 3)][gamestate] : "Main Menu";
    }

    ICOMMAND(0, mutscheck, "iii", (int *g, int *m, int *t), intret(mutscheck(*g, *m, *t)));
    ICOMMAND(0, mutsallowed, "ii", (int *g, int *h), intret(*g >= 0 && *g < G_MAX ? gametype[*g].mutators[*h >= 0 && *h < G_M_GSP+1 ? *h : 0] : 0));
    ICOMMAND(0, mutsimplied, "ii", (int *g, int *m), intret(*g >= 0 && *g < G_MAX ? gametype[*g].implied : 0));
    ICOMMAND(0, gspmutname, "ii", (int *g, int *n), result(*g >= 0 && *g < G_MAX && *n >= 0 && *n < G_M_GSN ? gametype[*g].gsp[*n] : ""));
    ICOMMAND(0, getintermission, "", (), intret(gs_intermission(gamestate) ? 1 : 0));
    ICOMMAND(0, getgameisplay, "b", (int *n), intret(m_play(*n >= 0 ? *n : gamemode) ? 1 :0));
    ICOMMAND(0, getgamestate, "", (), intret(gamestate));
    ICOMMAND(0, getgamestatestr, "ib", (int *n, int *b), result(gamestates[clamp(*n, 0, 3)][clamp(*b >= 0 ? *b : gamestate, 0, int(G_S_MAX))]));
    ICOMMAND(0, getgametimeremain, "", (), intret(gametimeremain()));
    ICOMMAND(0, getgametimeelapsed, "i", (int *n), intret(gametimeelapsed(*n!=0)));
    ICOMMAND(0, getgametimelimit, "bb", (int *g, int *m), intret(m_mmvar(*g >= 0 ? *g : gamemode, *m >= 0 ? *m : mutators, timelimit)));

    const char *gametitle() { return connected() ? server::gamename(gamemode, mutators) : "Ready"; }
    const char *gametext() { return connected() ? mapname : "Not connected"; }

    enum
    {
        VANITYSTYLE_NONE = 0, VANITYSTYLE_PRIV = 1<<0, VANITYSTYLE_ACTORMODEL = 1<<1,
    };

    void vanityreset()
    {
        loopvrev(vanities) vanities.remove(i);
        loopv(players) if(players[i]) players[i]->vitems.shrink(0);
        player1->vitems.shrink(0);
    }
    ICOMMAND(0, resetvanity, "", (), vanityreset());

    int vanityitem(int type, const char *ref, const char *name, const char *tag, int cond, int style)
    {
        if(type < 0 || type >= VANITYMAX || !ref || !name || !tag) return -1;
        int num = vanities.length();
        vanity &v = vanities.add();
        v.type = type;
        v.ref = newstring(ref);
        v.setmodel(ref);
        v.name = newstring(name);
        v.tag = newstring(tag);
        v.cond = cond;
        v.style = style;
        return num;
    }
    ICOMMAND(0, addvanity, "isssii", (int *t, char *r, char *n, char *g, int *c, int *s), intret(vanityitem(*t, r, n, g, *c, *s)));

    void vanityinfo(int id, int value)
    {
        if(id < 0) intret(vanities.length());
        else if(value < 0) intret(7);
        else if(vanities.inrange(id)) switch(value)
        {
            case 0: intret(vanities[id].type); break;
            case 1: result(vanities[id].ref); break;
            case 2: result(vanities[id].name); break;
            case 3: result(vanities[id].tag); break;
            case 4: intret(vanities[id].cond); break;
            case 5: intret(vanities[id].style); break;
            case 6: result(vanities[id].model); break;
            default: break;
        }
    }
    ICOMMAND(0, getvanity, "bb", (int *t, int *v), vanityinfo(*t, *v));

    int vanityfind(const char *name)
    {
        loopv(vanities) if(!strcmp(vanities[i].ref, name)) return i;
        return -1;
    }
    ICOMMAND(0, findvanity, "s", (char *s), intret(vanityfind(s)));

    void vanitybuild(gameent *d)
    {
        if(!*d->vanity) return; // not needed
        vector<char *> vanitylist;
        explodelist(d->vanity, vanitylist);
        loopv(vanitylist) if(vanitylist[i] && *vanitylist[i])
            loopvk(vanities) if(!strcmp(vanities[k].ref, vanitylist[i]))
                d->vitems.add(k);
        vanitylist.deletearrays();
    }

    const char *vanitymodel(gameent *d)
    {
        if(d->actortype >= A_ENEMY) return actors[d->actortype%A_MAX].name;
        return playertypes[d->model%PLAYERTYPES][5];
    }

    const char *vanityfname(gameent *d, int n, bool proj)
    {
        const char *file = NULL;
        if(vanities.inrange(n))
        {
            // No need for advanced checks if there is no style.
            if(vanities[n].style == 0) file = proj ? vanities[n].proj : vanities[n].model;
            else
            {
                // Unique ID for this vanity setup.
                defformatstring(id, "%s:%s",
                                (vanities[n].style & VANITYSTYLE_PRIV) ? server::privnamex(d->privilege, d->actortype, true) : "",
                                (vanities[n].style & VANITYSTYLE_ACTORMODEL) ? vanitymodel(d) : "");

                // Check if we've already found the file.
                loopv(vanities[n].files)
                    if(vanities[n].files[i].proj == proj && !strcmp(vanities[n].files[i].id, id))
                        file = vanities[n].files[i].name;

                // If not already found, build file name from each style.
                if(!file)
                {
                    defformatstring(fn, "%s", vanities[n].model);
                    if(vanities[n].style & VANITYSTYLE_PRIV) concformatstring(fn, "/%s", server::privnamex(d->privilege, d->actortype, true));
                    if(vanities[n].style & VANITYSTYLE_ACTORMODEL) concformatstring(fn, "/%s", vanitymodel(d));
                    if(proj) concatstring(fn, "/proj");

                    // Add to the list.
                    vanityfile &f = vanities[n].files.add();
                    f.id = newstring(id);
                    f.name = newstring(fn);
                    f.proj = proj;
                    file = f.name;
                }
            }
        }
        return file;
    }

    bool vanitycheck(gameent *d)
    {
        if(!vanitymodels || !d || !*d->vanity || (d != focus && d->o.dist(camera1->o) > vanitymaxdist)) return false;
        return true;
    }

    bool allowspec(gameent *d, int level, int cn = -1)
    {
        if(d->actortype >= A_ENEMY || !insideworld(d->o)) return false;
        if(d->state == CS_SPECTATOR || ((d->state == CS_DEAD || d->state == CS_WAITING) && !d->lastdeath)) return false;
        if(cn >= 0)
        {
            if(cn == focus->clientnum && focus->state != CS_ALIVE && d->clientnum == focus->lastattacker) return true;
            return d->clientnum == cn; // override
        }
        switch(level)
        {
            case 0: if(d->state != CS_ALIVE && d->state != CS_EDITING) return false; break;
            case 1: if(m_duke(gamemode, mutators) && d->state != CS_ALIVE && d->state != CS_EDITING) return false; break;
            case 2: break;
        }
        return true;
    }

    bool thirdpersonview(bool viewonly, physent *d)
    {
        if(!gs_playing(gamestate)) return true;
        if(!d) d = focus;
        if(!viewonly && (d->state == CS_DEAD || d->state == CS_WAITING)) return true;
        if(player1->state == CS_EDITING) return false;
        if(player1->state >= CS_SPECTATOR && d == player1) return false;
        if(d == focus && inzoom()) return false;
        if(!(d != player1 ? followthirdperson : thirdperson)) return false;
        return true;
    }
    ICOMMAND(0, isthirdperson, "i", (int *viewonly), intret(thirdpersonview(*viewonly ? true : false) ? 1 : 0));
    ICOMMAND(0, thirdpersonswitch, "", (), int *n = (focus != player1 ? &followthirdperson : &thirdperson); *n = !*n);

    int fov()
    {
        if(player1->state == CS_EDITING) return editfov;
        if(focus == player1 && player1->state == CS_SPECTATOR) return specfov;
        if(thirdpersonview(true)) return thirdpersonfov;
        return firstpersonfov;
    }

    void checkzoom()
    {
        if(zoomdefault > zoomlevels) zoomdefault = zoomlevels;
        if(zoomlevel < 0) zoomlevel = zoomdefault >= 0 ? zoomdefault : max(zoomlevels-zoomoffset, 0);
        if(zoomlevel > zoomlevels) zoomlevel = zoomlevels;
    }

    void setzoomlevel(int level)
    {
        checkzoom();
        zoomlevel += level;
        if(zoomlevel > zoomlevels) zoomlevel = zoomscroll ? 0 : zoomlevels;
        else if(zoomlevel < 0) zoomlevel = zoomscroll ? zoomlevels : 0;
    }
    ICOMMAND(0, setzoom, "i", (int *level), setzoomlevel(*level));

    void zoomset(bool on, int millis)
    {
        if(on != zooming)
        {
            lastzoom = millis - max(W(focus->weapselect, cookzoom) - (millis - lastzoom), 0);
            if(zoomdefault >= 0 && on) zoomlevel = zoomdefault;
        }
        checkzoom();
        zooming = on;
    }

    bool inzoom()
    {
        if(focus != player1 && followthirdperson) return false;
        if(lastzoom && (zooming || lastmillis-lastzoom <= W(focus->weapselect, cookzoom)))
            return true;
        return false;
    }
    ICOMMAND(0, iszooming, "", (), intret(inzoom() ? 1 : 0));

    void resetsway()
    {
        swaydir = swaypush = vec(0, 0, 0);
        swayfade = swayspeed = swaydist = bobfade = bobdist = 0;
    }

    void addsway(gameent *d)
    {
        float speed = physics::movevelocity(d), step = firstpersonbob ? firstpersonbobstep : firstpersonswaystep;
        bool bobbed = false, sliding = d->sliding(true);
        if(d->state == CS_ALIVE && (d->physstate >= PHYS_SLOPE || d->onladder || d->impulse[IM_TYPE] == IM_T_PARKOUR || sliding))
        {
            float mag = d->vel.magnitude();
            if(sliding) mag *= firstpersonswayslide;
            swayspeed = clamp(mag, speed*firstpersonswaymin, speed);
            swaydist += swayspeed*curtime/1000.0f;
            swaydist = fmod(swaydist, 2*step);
            if(!sliding)
            {
                bobdist += swayspeed*curtime/1000.0f;
                bobdist = fmod(bobdist, 2*firstpersonbobstep);
                bobfade = swayfade = 1;
                bobbed = true;
            }
        }
        else if(swayfade > 0)
        {
            swaydist += swayspeed*swayfade*curtime/1000.0f;
            swaydist = fmod(swaydist, 2*step);
            swayfade -= 0.5f*(curtime*speed)/(step*1000.0f);
        }
        if(!bobbed && bobfade > 0)
        {
            bobdist += swayspeed*bobfade*curtime/1000.0f;
            bobdist = fmod(bobdist, 2*firstpersonbobstep);
            bobfade -= 0.5f*(curtime*speed)/(firstpersonbobstep*1000.0f);
        }

        float k = pow(0.7f, curtime/25.0f);
        swaydir.mul(k);
        vec inertia = vec(d->vel).add(d->falling);
        float speedscale = max(inertia.magnitude(), speed);
        if(d->state == CS_ALIVE && speedscale > 0) swaydir.add(vec(inertia).mul((1-k)/(15*speedscale)));
        swaypush.mul(pow(0.5f, curtime/25.0f));
    }

    int errorchan = -1;
    void errorsnd(gameent *d)
    {
        if(d == player1 && !issound(errorchan))
            playsound(S_ERROR, d->o, d, SND_FORCED, -1, -1, -1, &errorchan);
    }

    int announcerchan = -1;
    void announce(int idx, gameent *d, bool forced, bool unmapped)
    {
        if(idx < 0) return;
        physent *t = !d || d == player1 || forced ? camera1 : d;
        int *chan = d && !forced ? &d->aschan : &announcerchan;
        playsound(idx, t->o, t, (unmapped ? SND_UNMAPPED : 0)|(t != camera1 ? SND_IMPORT : SND_FORCED)|SND_BUFFER, -1, -1, -1, chan);
    }

    void announcef(int idx, int targ, gameent *d, bool forced, const char *msg, ...)
    {
        if(targ >= 0 && msg && *msg)
        {
            defvformatbigstring(text, msg, msg);
            conoutft(targ, "%s", text);
        }
        announce(idx, d, forced);
    }

    ICOMMAND(0, announce, "iiisN", (int *idx, int *targ, int *cn, int *forced, char *s, int *numargs),
    {
        if(*numargs >= 5) announcef(*numargs >= 1 ? *idx : -1, *numargs >= 2 ? *targ : CON_EVENT, *numargs >= 3 ? getclient(*cn) : NULL, *numargs >= 4 ? *forced!=0 : false, "\fw%s", s);
        else announcef(*numargs >= 1 ? *idx : -1, *numargs >= 2 ? *targ : CON_DEBUG, *numargs >= 3 ? getclient(*cn) : NULL, *numargs >= 4 ? *forced!=0 : false, NULL);
    });

    void resetfollow()
    {
        follow = spectvfollow = -1;
        if(specresetstyle && (player1->state == CS_WAITING || player1->state == CS_SPECTATOR))
        {
            player1->o = camera1->o;
            player1->yaw = camera1->yaw;
            player1->pitch = camera1->pitch;
            player1->resetinterp();
        }
        focus = player1;
        resetcamera();
        resetsway();
    }

    void specreset(gameent *d, bool clear)
    {
        if(d)
        {
            if(d->actortype >= A_ENEMY) return;
            if(clear)
            {
                loopv(cameras)
                {
                    cament *c = cameras[i];
                    if(c->player == d)
                    {
                        if(c->type == cament::PLAYER)
                        {
                            cameras.remove(i);
                            delete c;
                            for(int j = i+1; j < cameras.length(); j++) cameras[j]->cn--;
                            if(i == lastcamcn)
                            {
                                lastcamcn = -1;
                                lastcamera = lasttvcam = lasttvchg = 0;
                            }
                        }
                        else c->player = NULL;
                    }
                }
                if(d == focus) resetfollow();
            }
            else if(!cameras.empty()) // otherwise cameras will be built by main loop
                cameras.add(new cament(cameras.length(), cament::PLAYER, d->clientnum, d->center(), d));
        }
        else
        {
            cameras.deletecontents();
            lastcamcn = -1;
            lastcamera = lasttvcam = lasttvchg = 0;
            resetfollow();
        }
    }

    bool followaim() { return followaiming&(thirdpersonview(true) ? 1 : 2); }

    bool tvmode(bool check, bool force)
    {
        if(!check || !cameras.empty())
        {
            if(!gs_playing(gamestate) && intermmode) return true;
            else switch(player1->state)
            {
                case CS_SPECTATOR: if(specmode) return true; break;
                case CS_WAITING: if((waitmode && (!player1->lastdeath || lastmillis-player1->lastdeath >= 500))) return true; break;
                default: break;
            }
        }
        return false;
    }

    #define MODESWITCH(name) \
        ICOMMAND(0, name##modeswitch, "", (), \
        { \
            if(tvmode(true, true)) name##mode = 0; \
            else name##mode = 1; \
            specreset(); \
        });
    MODESWITCH(spec);
    MODESWITCH(wait);

    void followswitch(int n, bool other)
    {
        if(player1->state == CS_SPECTATOR || (player1->state == CS_WAITING && (!player1->lastdeath || !deathbuttonmash || lastmillis-player1->lastdeath > deathbuttonmash)))
        {
            bool istv = tvmode(true, false);
            int *f = istv ? &spectvfollow : &follow;
            #define checkfollow \
                if(*f >= players.length()) *f = -1; \
                else if(*f < -1) *f = players.length()-1;
            #define addfollow \
            { \
                *f += n; \
                checkfollow; \
                if(*f == -1) \
                { \
                    if(other) *f += n; \
                    else \
                    { \
                        specreset(); \
                        return; \
                    } \
                    checkfollow; \
                } \
            }
            addfollow;
            if(!n) n = 1;
            loopi(players.length())
            {
                if(!players.inrange(*f)) addfollow
                else
                {
                    gameent *d = players[*f];
                    if(!d || !allowspec(d, istv ? spectvdead : followdead)) addfollow
                    else
                    {
                        if(!istv)
                        {
                            focus = d;
                            resetcamera();
                            resetsway();
                        }
                        return;
                    }
                }
            }
            specreset();
        }
        return;
    }
    ICOMMAND(0, followdelta, "ii", (int *n, int *o), followswitch(*n!=0 ? *n : 1, *o!=0));

    bool allowmove(physent *d)
    {
        if(gameent::is(d))
        {
            if((d == player1 && tvmode()) || d->state == CS_DEAD || d->state >= CS_SPECTATOR || !gs_playing(gamestate))
                return false;
        }
        return true;
    }

    bool needname(gameent *d)
    {
        if(!d || *d->name) return false; // || client::waiting()) return false;
        return true;
    }
    ICOMMAND(0, needname, "b", (int *cn), intret(needname(*cn >= 0 ? getclient(*cn) : player1) ? 1 : 0));

    void respawn(gameent *d)
    {
        if(d == player1 && (maptime <= 0 || needname(d) || wantsloadoutmenu)) return; // prevent spawning
        if(d->state == CS_DEAD && d->respawned < 0 && (!d->lastdeath || lastmillis-d->lastdeath >= DEATHMILLIS*2))
        {
            client::addmsg(N_TRYSPAWN, "ri", d->clientnum);
            d->respawned = lastmillis;
        }
    }

    void spawneffect(int type, const vec &pos, float radius, int colour, float size)
    {
        vec o = pos;
        o.z -= radius*2.6f;
        loopi(9)
        {
            o.z += radius*0.4f;
            createshape(type, radius*0.25f, colour, 3, 50, 350, o, size, 0.35f, 0, 1, 8);
        }
    }

    float spawnfade(gameent *d)
    {
        int len = m_delay(d->actortype, gamemode, mutators, d->team);
        if(AA(d->actortype, abilities)&(1<<A_A_KAMIKAZE)) len = deathfadekamikaze;
        else
        {
            if(m_edit(gamemode) && !len && deathfadeedit) len = deathfadeedit;
            len = clamp(len, min(deathfademin, deathfademax), max(deathfademin, deathfademax));
        }
        if(len > 0)
        {
            int interval = min(len/4, ragdolleffect), over = max(len-interval, 1), millis = lastmillis-d->lastdeath;
            if(millis <= len) { if(millis >= over) return 1.f-((millis-over)/float(interval)); }
            else return 0;
        }
        return 1;
    }

    float opacity(gameent *d, bool third)
    {
        float total = d == focus ? (third ? (d != player1 ? followblend : thirdpersonblend) : 1.f) : playerblend;
        if(physics::isghost(d, focus)) total *= playerghostblend;
        if(d->state == CS_DEAD || d->state == CS_WAITING)
        {
            if(deathfade) total *= spawnfade(d);
        }
        else if(d->state == CS_ALIVE)
        {
            if(d == focus && third) total *= min(camera1->o.dist(d->o)/(d != player1 ? followdist : thirdpersondist), 1.0f);
            int prot = m_protect(gamemode, mutators), millis = d->protect(lastmillis, prot); // protect returns time left
            if(millis > 0) total *= 1.f-min(float(millis)/float(prot), 1.0f);
        }
        else if(d->state == CS_EDITING) total *= playereditblend;
        return total;
    }

    void respawned(gameent *d, bool local, int ent)
    { // remote clients wait until first position update to process this
        if(local)
        {
            d->state = CS_ALIVE;
            entities::spawnplayer(d, ent, true);
            client::addmsg(N_SPAWN, "ri", d->clientnum);
        }
        d->configure(lastmillis, gamemode, mutators, physics::carryaffinity(d));

        if(d == player1) specreset();
        else if(d == focus) resetcamera();
        if(d == focus) resetsway();

        if(d->actortype < A_ENEMY)
        {
            vec center = d->center();
            playsound(S_RESPAWN, d->o, d);
            spawneffect(PART_SPARK, center, d->height*0.5f, getcolour(d, playerovertone, playerovertonelevel), 1);
            spawneffect(PART_SPARK, center, d->height*0.5f, getcolour(d, playerundertone, playerundertonelevel), 1);
            if(dynlighteffects) adddynlight(center, d->height*2, vec::fromcolor(getcolour(d, playereffecttone, playereffecttonelevel)).mul(2.f), 250, 250);
            if(entities::ents.inrange(ent) && entities::ents[ent]->type == PLAYERSTART) entities::execlink(d, ent, false);
        }
        ai::respawned(d, local, ent);
    }

    // determines if correct gamemode/mutators are present for displaying palette colours
    // for textures, lights, mapmodels, particles which use a palette colour
    int hexpalette(int palette, int index)
    {
        if(palette >= 0 && index >= 0) switch(palette)
        {
            case 0: // misc (rainbow palettes)
            {
                if(index <= 0 || index > PULSE_MAX) break;
                return pulsecols[index-1][clamp((lastmillis/100)%PULSECOLOURS, 0, PULSECOLOURS-1)];
                break;
            }
            case 1: // teams
            {
                int team = index%T_COUNT;
                if(drawtex != DRAWTEX_MAPSHOT && !m_edit(gamemode) && index < T_COUNT)
                {
                    if(!m_team(gamemode, mutators) || team > T_LAST) team = T_NEUTRAL; // abstract team coloured levels to neutral
                }
                return TEAM(team, colour); // return color of weapon
                break;
            }
            case 2: // weapons
            {
                if(drawtex) break;
                int weap = index%W_MAX;
                if(!m_edit(gamemode) && index < W_MAX)
                {
                    weap = m_attr(WEAPON, weap);
                    if(!isweap(weap) || W(weap, disabled) || (m_rotweaps(gamemode, mutators) && weap < W_ITEM) || !m_check(W(weap, modes), W(weap, muts), gamemode, mutators))
                        weap = -1; // blank palette (because weapon not present in mode)
                }
                if(isweap(weap)) return W(weap, colour); // return color of weapon
                break;
            }
            default: break;
        }
        return 0xFFFFFF;
    }

    vec getpalette(int palette, int index)
    {
        return vec::fromcolor(hexpalette(palette, index));
    }

    void fixpalette(int &palette, int &index, int gver)
    {
        if(gver <= 244 && palette == 1 && index%(T_COUNT+2) > T_LAST) index = T_NEUTRAL; // remove kappa and sigma
    }

    void adddynlights()
    {
        entities::adddynlights();
        if(!dynlighteffects || drawtex) return;
        projs::adddynlights();
        if(dynlighteffects >= 2)
        {
            if(m_capture(gamemode)) capture::adddynlights();
            else if(m_defend(gamemode)) defend::adddynlights();
            else if(m_bomber(gamemode)) bomber::adddynlights();
        }
        gameent *d = NULL;
        int numdyns = numdynents();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) != NULL)
        {
            if(d->state == CS_ALIVE && isweap(d->weapselect))
            {
                bool last = lastmillis-d->weaptime[d->weapselect] > 0,
                     powering = last && d->weapstate[d->weapselect] == W_S_POWER,
                     reloading = last && d->weapstate[d->weapselect] == W_S_RELOAD,
                     secondary = physics::secondaryweap(d);
                float amt = last ? clamp(float(lastmillis-d->weaptime[d->weapselect])/d->weapwait[d->weapselect], 0.f, 1.f) : 0.f;
                vec col = WPCOL(d, d->weapselect, lightcol, secondary);
                if(d->weapselect == W_FLAMER && (!reloading || amt > 0.5f) && !physics::liquidcheck(d))
                {
                    float scale = powering ? 1.f+(amt*1.5f) : (d->weapstate[d->weapselect] == W_S_IDLE ? 1.f : (reloading ? (amt-0.5f)*2 : amt));
                    adddynlight(d->ejecttag(d->weapselect), 16*scale, col, 0, 0);
                }
                if((W(d->weapselect, lightpersist)&1 || powering) && W(d->weapselect, lightradius) > 0)
                {
                    float thresh = max(amt, 0.25f), size = W(d->weapselect, lightradius)*thresh;
                    int span = max(W2(d->weapselect, cooktime, physics::secondaryweap(d))/4, 500), interval = lastmillis%span, part = span/2;
                    if(interval) size += size*0.5f*(interval <= part ? interval/float(part) : (span-interval)/float(part));
                    adddynlight(d->muzzletag(d->weapselect), size, vec(col).mul(thresh), 0, 0);
                }
            }
            if(d->burntime && d->burning(lastmillis, d->burntime))
            {
                int millis = lastmillis-d->lastres[W_R_BURN], delay = max(d->burndelay, 1);
                size_t seed = size_t(d) + (millis/50);
                float pc = 1, amt = (millis%50)/50.0f, intensity = 0.75f+(detrnd(seed, 25)*(1-amt) + detrnd(seed + 1, 25)*amt)/100.f;
                if(d->burntime-millis < delay) pc *= float(d->burntime-millis)/float(delay);
                else
                {
                    float fluc = float(millis%delay)*(0.25f+0.03f)/delay;
                    if(fluc >= 0.25f) fluc = (0.25f+0.03f-fluc)*(0.25f/0.03f);
                    pc *= 0.75f+fluc;
                }
                adddynlight(d->center(), d->height*intensity*pc, pulsecolour(d).mul(pc), 0, 0);
            }
            if(d->shocktime && d->shocking(lastmillis, d->shocktime))
            {
                int millis = lastmillis-d->lastres[W_R_SHOCK], delay = max(d->shockdelay, 1);
                size_t seed = size_t(d) + (millis/50);
                float pc = 1, amt = (millis%50)/50.0f, intensity = 0.75f+(detrnd(seed, 25)*(1-amt) + detrnd(seed + 1, 25)*amt)/100.f;
                if(d->shocktime-millis < delay) pc *= float(d->shocktime-millis)/float(delay);
                else
                {
                    float fluc = float(millis%delay)*(0.25f+0.03f)/delay;
                    if(fluc >= 0.25f) fluc = (0.25f+0.03f-fluc)*(0.25f/0.03f);
                    pc *= 0.75f+fluc;
                }
                adddynlight(d->center(), d->height*intensity*pc, pulsecolour(d, PULSE_SHOCK).mul(pc), 0, 0);
            }
            if(d->actortype < A_ENEMY && getillumlevel() > 0 && getillumradius() > 0)
                adddynlight(d->center(), getillumradius(), vec::fromcolor(getcolour(d, playereffecttone, getillumlevel())), 0, 0);
        }
    }

    void boosteffect(gameent *d, const vec &pos, int num, int len, bool shape = false)
    {
        // Prevent spawning particles while the game is paused, as it doesn't clear particles in this state.
        // Otherwise, the number of particles could become very large, causing the renderer to slow down to a crawl.
        if(paused) return;

        float scale = 0.4f+(rnd(40)/100.f);
        part_create(PART_HINT_BOLD_SOFT, shape ? len/2 : len/10, pos, getcolour(d, playereffecttone, playereffecttonelevel), scale*1.5f, scale*0.75f, 0, 0);
        part_create(PART_FIREBALL_SOFT, shape ? len/2 : len/10, pos, pulsehexcol(d, PULSE_FIRE), scale*1.25f, scale*0.75f, 0, 0);
        if(shape) loopi(num) regularshape(PART_FIREBALL, int(d->radius)*2, pulsehexcol(d, PULSE_FIRE), 21, 1, len, pos, scale*1.25f, 0.75f, -5, 0, 10);
    }

    void impulseeffect(gameent *d, int effect)
    {
        int num = int((effect ? 3 : 10)*impulsescale);
        switch(effect)
        {
            case 0: playsound(S_IMPULSE, d->o, d); // fail through
            case 1:
            {
                if(!actors[d->actortype].jetfx) break;
                if(num > 0 && impulsefade > 0) loopi(TAG_N_JET) boosteffect(d, d->jettag(i), num, impulsefade, effect == 0);
                break;
            }
            default: break;
        }
    }

    void setmode(int nmode, int nmuts) { modecheck(nextmode = nmode, nextmuts = nmuts); }
    ICOMMAND(0, mode, "ii", (int *val, int *mut), setmode(*val, *mut));

    void footstep(gameent *d, int curfoot)
    {
        if(!actors[d->actortype].steps) return;
        bool moving = d->move || d->strafe, liquid = physics::liquidcheck(d), onfloor = d->physstate >= PHYS_SLOPE || d->onladder || d->impulse[IM_TYPE] == IM_T_PARKOUR;
        if(curfoot < 0 || (moving && (liquid || onfloor)))
        {
            float mag = d->vel.magnitude(), m = min(footstepsoundmax, footstepsoundmin), n = max(footstepsoundmax, footstepsoundmin);
            if(n > m && mag > m)
            {
                if(curfoot < 0) curfoot = d->lastfoot;
                vec pos = d->toetag(curfoot);
                float amt = clamp(mag/n, 0.f, 1.f)*(d != focus ? footstepsoundlevel : footstepsoundfocus);
                if(onfloor && !d->running(moveslow)) amt *= footstepsoundlight;
                int vol = clamp(int(amt*footstepsoundmaxvol), footstepsoundminvol, footstepsoundmaxvol);
                playsound(liquid && (!onfloor || rnd(4)) ? S_SWIMSTEP : S_FOOTSTEP, pos, NULL, d != focus ? 0 : SND_NOCULL, vol, footstepsoundmaxrad, footstepsoundminrad, &d->sschan[curfoot]);
            }
        }
    }

    bool canregenimpulse(gameent *d)
    {
        if(d->state == CS_ALIVE && impulseregen > 0 && (!impulseregendelay || lastmillis-d->impulse[IM_REGEN] >= impulseregendelay))
            return true;
        return false;
    }

    void checkoften(gameent *d, bool local)
    {
        adjustscaled(d->quake, quakefade);
        int prevstate = isweap(d->weapselect) ? d->weapstate[d->weapselect] : W_S_IDLE;
        float offset = d->height;

        d->configure(lastmillis, gamemode, mutators, physics::carryaffinity(d), curtime);

        d->o.z -= d->height;
        if(d->state == CS_ALIVE && AA(d->actortype, abilities)&(1<<A_A_CROUCH))
        {
            bool sliding = d->sliding(true), crouching = sliding || d->crouching(true),
                 moving = d->move || d->strafe || (d->physstate < PHYS_SLOPE && !d->onladder);
            float zrad = d->zradius*(moving && !sliding ? CROUCHHIGH : CROUCHLOW), zoff = d->zradius-zrad;
            vec old = d->o;
            if(!crouching)
            {
                d->o.z += d->zradius;
                d->height = d->zradius;
                if(collide(d, vec(0, 0, 1), 0, false) || collideinside)
                {
                    d->o.z -= zoff;
                    d->height = zrad;
                    if(!collide(d, vec(0, 0, 1), 0, false) && !collideinside) crouching = true;
                }
                d->o = old;
                d->height = offset;
            }
            if(crouching || d->crouching())
            {
                float zamt = zoff*curtime/float(PHYSMILLIS);
                if(crouching)
                {
                    if(d->actiontime[AC_CROUCH] <= 0) d->actiontime[AC_CROUCH] = lastmillis;
                    if(d->height > zrad && ((d->height -= zamt) < zrad)) d->height = zrad;
                    else if(d->height < zrad && ((d->height += zamt) > zrad)) d->height = zrad;
                }
                else
                {
                    if(d->actiontime[AC_CROUCH] >= 0) d->actiontime[AC_CROUCH] = -lastmillis;
                    if(d->height < d->zradius && ((d->height += zamt) > d->zradius)) d->height = d->zradius;
                    else if(d->height > d->zradius && ((d->height -= zamt) < d->zradius)) d->height = d->zradius;
                }
            }
            else
            {
                d->height = d->zradius;
                d->actiontime[AC_CROUCH] = 0;
            }
        }
        else
        {
            d->height = d->zradius;
            d->actiontime[AC_CROUCH] = 0;
        }
        d->o.z += d->airmillis ? offset : d->height;

        if(impulsemeter && canregenimpulse(d) && d->impulse[IM_METER] > 0)
        {
            bool onfloor = d->physstate >= PHYS_SLOPE || d->onladder || physics::liquidcheck(d),
                 collect = true; // collect time until we are able to act upon it
            int timeslice = int((curtime+d->impulse[IM_COLLECT])*impulseregen);
            #define impulsemod(x,y) \
                if(collect && (x)) \
                { \
                    if(y > 0) { if(timeslice > 0) timeslice = int(timeslice*y); } \
                    else collect = false; \
                }
            impulsemod(d->running(), impulseregenrun);
            impulsemod(d->move || d->strafe, impulseregenmove);
            impulsemod((!onfloor && PHYS(gravity) > 0) || d->sliding(), impulseregeninair);
            impulsemod(onfloor && d->crouching() && !d->sliding(), impulseregencrouch);
            impulsemod(d->sliding(true), impulseregenslide);
            if(collect)
            {
                if(timeslice > 0)
                {
                    if((d->impulse[IM_METER] -= timeslice) < 0) d->impulse[IM_METER] = 0;
                    d->impulse[IM_COLLECT] = 0;
                }
                else d->impulse[IM_COLLECT] += curtime;
                if(!d->lastimpulsecollect) d->lastimpulsecollect = lastmillis;
            }
            else d->lastimpulsecollect = 0;
        }
        else d->lastimpulsecollect = 0;

        if(isweap(d->weapselect) && gs_playing(gamestate) && d->state == CS_ALIVE)
        {
            bool secondary = physics::secondaryweap(d);
            bool firing = d->weapstate[d->weapselect] == (secondary ? W_S_SECONDARY : W_S_PRIMARY);

            if(d->wasfiring != d->weapselect && firing)
            {
                d->wasfiring = d->weapselect;
                playsound(WSNDFB(d->weapselect, secondary), d->o, d, 0, 255, -1, -1, &d->wschan[WS_BEGIN_CHAN]);
            }
            else if(d->wasfiring == d->weapselect && !firing)
            {
                d->wasfiring = -1;
                playsound(WSNDFE(d->weapselect, secondary), d->o, d, 0, 255, -1, -1, &d->wschan[WS_END_CHAN]);
            }

            if(d->weapselect < W_ALL && d->weapstate[d->weapselect] != W_S_RELOAD && prevstate == W_S_RELOAD && playreloadnotify&(d == focus ? 1 : 2) && (d->weapammo[d->weapselect][W_A_CLIP] >= W(d->weapselect, ammoclip) || playreloadnotify&(d == focus ? 4 : 8)))
                    playsound(WSND(d->weapselect, S_W_NOTIFY), d->o, d, 0, reloadnotifyvol, -1, -1, &d->wschan[WS_MAIN_CHAN]);
            if(d->weapstate[d->weapselect] == W_S_POWER || d->weapstate[d->weapselect] == W_S_ZOOM)
            {
                int millis = lastmillis-d->weaptime[d->weapselect];
                if(millis >= 0 && millis <= d->weapwait[d->weapselect])
                {
                    float amt = millis/float(d->weapwait[d->weapselect]);
                    int vol = 255, snd = d->weapstate[d->weapselect] == W_S_POWER ? WSND2(d->weapselect, secondary, S_W_POWER) : WSND(d->weapselect, S_W_ZOOM);
                    if(W2(d->weapselect, cooktime, secondary)) switch(W2(d->weapselect, cooked, secondary))
                    {
                        case 4: case 5: vol = 10+int(245*(1.f-amt)); break; // longer
                        case 1: case 2: case 3: default: vol = 10+int(245*amt); break; // shorter
                    }
                    if(issound(d->pschan)) sounds[d->pschan].vol = vol;
                    else playsound(snd, d->o, d, SND_LOOP, vol, -1, -1, &d->pschan);
                }
                else if(d->pschan >= 0)
                {
                    if(issound(d->pschan)) removesound(d->pschan);
                    d->pschan = -1;
                }
            }
            else if(d->pschan >= 0)
            {
                if(issound(d->pschan)) removesound(d->pschan);
                d->pschan = -1;
            }
            if(W2(d->weapselect, cooked, true)&W_C_KEEP && d->prevstate[d->weapselect] == W_S_ZOOM && !d->action[AC_SECONDARY])
            {
                d->prevstate[d->weapselect] = W_S_IDLE;
                d->prevtime[d->weapselect] = 0;
            }
        }
        else if(d->pschan >= 0)
        {
            if(issound(d->pschan)) removesound(d->pschan);
            d->pschan = -1;
        }

        if(local)
        {
            if(d->respawned >= 0 && lastmillis-d->respawned >= 2500) d->respawned = -1;
            if(d->suicided >= 0 && lastmillis-d->suicided >= 2500) d->suicided = -1;
        }

        int restime[W_R_MAX] = { d->burntime, d->bleedtime, d->shocktime };
        loopi(W_R_MAX) if(d->lastres[i] > 0 && lastmillis-d->lastres[i] >= restime[i]) d->resetresidual(i);
        if(gs_playing(gamestate) && d->state == CS_ALIVE && actors[d->actortype].steps)
        {
            int curfoot = d->curfoot();
            bool hassound = footstepsounds&(d != focus ? 2 : 1);
            if(!hassound) loopi(2) if(d->sschan[i] >= 0)
            {
                if(issound(d->sschan[i])) removesound(d->sschan[i]);
                d->sschan[i] = -1;
            }
            if(curfoot >= 0)
            {
                if(curfoot != d->lastfoot)
                {
                    if(hassound)
                    {
                        if(d->lastfoot >= 0 && issound(d->sschan[d->lastfoot])) sounds[d->sschan[d->lastfoot]].pos = d->toetag(d->lastfoot);
                        footstep(d, curfoot);
                        d->lastfoot = curfoot;
                    }
                }
                else if(hassound) loopi(2) if(issound(d->sschan[i])) sounds[d->sschan[i]].pos = d->toetag(i);
            }
        }
        loopv(d->icons) if(totalmillis-d->icons[i].millis > d->icons[i].fade) d->icons.remove(i--);
    }

    void checkfloor(gameent *d)
    {
        if(d->state != CS_ALIVE) return;
        vec pos = d->feetpos();
        if(d->impulse[IM_TYPE] != IM_T_PARKOUR && (d->physstate >= PHYS_SLOPE || d->onladder || physics::liquidcheck(d)) && pos.z > 0 && d->floortime(lastmillis))
        {
            int mat = lookupmaterial(pos);
            if(!isclipped(mat&MATF_VOLUME) && !((mat&MATF_FLAGS)&MAT_DEATH)) d->floorpos = pos;
        }
    }

    void checkplayers()
    {
        checkfloor(player1);
        loopv(players) if(players[i])
        {
            gameent *d = players[i];
            if(d->ai)
            {
                checkfloor(d);
                continue;
            }
            if(!gs_playing(gamestate)) continue;
            if(d->state == CS_DEAD || d->state == CS_WAITING) entities::checkitems(d);
            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime) continue;
            //else if(lagtime > 1000) continue;
            physics::smoothplayer(d, 1, false);
        }
    }

    bool burn(gameent *d, int weap, int flags)
    {
        if(wr_burns(weap, flags) && (d->submerged < G(liquidextinguish) || (d->inmaterial&MATF_VOLUME) != MAT_WATER))
        {
            d->lastrestime[W_R_BURN] = lastmillis;
            if(isweap(weap) || flags&HIT(MATERIAL)) d->lastres[W_R_BURN] = lastmillis;
            else return true;
        }
        return false;
    }

    bool bleed(gameent *d, int weap, int flags)
    {
        if(wr_bleeds(weap, flags))
        {
            d->lastrestime[W_R_BLEED] = lastmillis;
            if(isweap(weap) || flags&HIT(MATERIAL)) d->lastres[W_R_BLEED] = lastmillis;
            else return true;
        }
        return false;
    }

    bool shock(gameent *d, int weap, int flags)
    {
        if(wr_shocks(weap, flags))
        {
            d->lastrestime[W_R_SHOCK] = lastmillis;
            if(isweap(weap) || flags&HIT(MATERIAL)) d->lastres[W_R_SHOCK] = lastmillis;
            else return true;
        }
        return false;
    }

    struct damagemerge
    {
        enum { BURN = 1<<0, BLEED = 1<<1, SHOCK = 1<<2 };

        gameent *d, *v;
        int weap, damage, flags, millis;

        damagemerge() { millis = totalmillis ? totalmillis : 1; }
        damagemerge(gameent *d, gameent *v, int weap, int damage, int flags) : d(d), v(v), weap(weap), damage(damage), flags(flags) { millis = totalmillis ? totalmillis : 1; }

        bool merge(const damagemerge &m)
        {
            if(d != m.d || v != m.v || flags != m.flags) return false;
            damage += m.damage;
            return true;
        }

        void play()
        {
            if(playdamagetones >= (v == focus ? 1 : (d == focus ? 2 : 3)))
            {
                const float dmgsnd[8] = { 0, 0.1f, 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
                int hp = d->gethealth(gamemode, mutators), snd = -1;
                if(flags&BURN) snd = S_BURNED;
                else if(flags&BLEED) snd = S_BLEED;
                else if(flags&SHOCK) snd = S_SHOCK;
                else loopirev(8) if(damage >= hp*dmgsnd[i]) { snd = S_DAMAGE+i; break; }
                if(snd >= 0) playsound(snd, d->o, d, SND_IMPORT|(v == focus ? SND_NODIST : SND_CLAMPED), damagetonevol);
            }
            if(aboveheaddamage)
            {
                string text;
                if(damageinteger) formatstring(text, "<sub>\fo%c%d", damage > 0 ? '-' : (damage < 0 ? '+' : '~'), int(ceilf((damage < 0 ? 0-damage : damage)/damagedivisor)));
                else formatstring(text, "<sub>\fo%c%.1f", damage > 0 ? '-' : (damage < 0 ? '+' : '~'), (damage < 0 ? 0-damage : damage)/damagedivisor);
                part_textcopy(d->abovehead(), text, d != focus ? PART_TEXT : PART_TEXT_ONTOP, eventiconfade, colourwhite, 4, 1, -10, 0, d);
            }
        }
    };
    vector<damagemerge> damagemerges;

    void removedamagemergeall()
    {
        loopvrev(damagemerges) damagemerges.remove(i);
    }

    void removedamagemerges(gameent *d)
    {
        loopvrev(damagemerges) if(damagemerges[i].d == d || damagemerges[i].v == d) damagemerges.remove(i);
    }

    void pushdamagemerge(gameent *d, gameent *v, int weap, int damage, int flags)
    {
        damagemerge dt(d, v, weap, damage, flags);
        loopv(damagemerges) if(damagemerges[i].merge(dt)) return;
        damagemerges.add(dt);
    }

    void flushdamagemerges()
    {
        loopv(damagemerges)
        {
            int delay = damagemergedelay;
            if(damagemerges[i].flags&damagemerge::BURN) delay = damagemergeburn;
            else if(damagemerges[i].flags&damagemerge::BLEED) delay = damagemergebleed;
            else if(damagemerges[i].flags&damagemerge::SHOCK) delay = damagemergeshock;
            if(totalmillis-damagemerges[i].millis >= delay)
            {
                damagemerges[i].play();
                damagemerges.remove(i--);
            }
        }
    }

    static int alarmchan = -1;
    void hiteffect(int weap, int flags, int damage, gameent *d, gameent *v, vec &dir, vec &vel, float dist, bool local)
    {
        bool burning = burn(d, weap, flags), bleeding = bleed(d, weap, flags), shocking = shock(d, weap, flags), material = flags&HIT(MATERIAL);
        if(!local || burning || bleeding || shocking || material)
        {
            float scale = isweap(weap) && WF(WK(flags), weap, damage, WS(flags)) != 0 ? abs(damage)/float(WF(WK(flags), weap, damage, WS(flags))) : 1.f;
            if(hitdealt(flags) && damage != 0 && v == focus) hud::hit(damage, d->o, d, weap, flags);
            if(hitdealt(flags) && damage > 0)
            {
                if(d == focus) hud::damage(damage, v->o, v, weap, flags);
                vec p = d->headpos(-d->height/4);
                int hp = max(d->gethealth(gamemode, mutators)/5, 1);
                if(!nogore && bloodscale > 0)
                    part_splash(PART_BLOOD, int(clamp(damage/hp, 1, 5)*bloodscale)*(bleeding || material ? 2 : 1), bloodfade, p, 0x229999, (rnd(bloodsize/2)+(bloodsize/2))/10.f, 1, 100, STAIN_BLOOD, int(d->radius), 10);
                if(nogore != 2 && (bloodscale <= 0 || bloodsparks))
                    part_splash(PART_PLASMA, int(clamp(damage/hp, 1, 5))*(bleeding || material ? 2: 1), bloodfade, p, 0x882222, 1, 0.5f, 50, STAIN_STAIN, int(d->radius));
                if(d != v)
                {
                    bool sameteam = m_team(gamemode, mutators) && d->team == v->team;
                    if(!sameteam) pushdamagemerge(d, v, weap, damage, (burning ? damagemerge::BURN : 0)|(bleeding ? damagemerge::BLEED : 0)|(shocking ? damagemerge::SHOCK : 0));
                    else if(v == player1 && !burning && !bleeding && !shocking && !material)
                    {
                        player1->lastteamhit = d->lastteamhit = totalmillis;
                        if(!issound(alarmchan)) playsound(S_ALARM, v->o, v, 0, -1, -1, -1, &alarmchan);
                    }
                    if(!burning && !bleeding && !shocking && !material && !sameteam) v->lasthit = totalmillis ? totalmillis : 1;
                }
                if(d->actortype < A_ENEMY && !issound(d->vschan)) playsound(S_PAIN, d->o, d, 0, -1, -1, -1, &d->vschan);
                d->lastpain = lastmillis;
                if(!WK(flags)) playsound(WSND2(weap, WS(flags), S_W_IMPACT), vec(d->center()).add(vec(dir).mul(dist)), NULL, 0, clamp(int(255*scale), 64, 255));
            }
            if(AA(d->actortype, abilities)&(1<<A_A_PUSHABLE))
            {
                if(weap == -1 && shocking && d->shockstun)
                {
                    float amt = WRS(flags&HIT(WAVE) || !hitdealt(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators);
                    if(m_dm_gladiator(gamemode, mutators))
                    {
                        float extra = flags&HIT(WAVE) || !hitdealt(flags) ? gladiatorextrawavestunscale : (d->health <= 0 ? gladiatorextradeadstunscale : gladiatorextrahitstunscale);
                        amt *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                    }
                    float s = d->shockstunscale*amt, g = d->shockstunfall*amt;
                    d->addstun(weap, lastmillis, d->shockstuntime, d->shockstun&W_N_STADD ? s : 0.f, d->shockstun&W_N_GRADD ? g : 0.f);
                    if(d->shockstun&W_N_STIMM && s > 0) d->vel.mul(1.f-clamp(s, 0.f, 1.f));
                    if(d->shockstun&W_N_GRIMM && g > 0) d->falling.mul(1.f-clamp(g, 0.f, 1.f));
                    if(d->shockstun&W_N_SLIDE) d->impulse[IM_SLIP] = lastmillis;
                }
                else if(isweap(weap) && !burning && !bleeding && !material && !shocking && WF(WK(flags), weap, damage, WS(flags)) != 0)
                {
                    if(WF(WK(flags), weap, stun, WS(flags)) != 0)
                    {
                        int stun = WF(WK(flags), weap, stun, WS(flags));
                        float amt = scale*WRS(flags&HIT(WAVE) || !hitdealt(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators);
                        if(m_dm_gladiator(gamemode, mutators))
                        {
                            float extra = flags&HIT(WAVE) || !hitdealt(flags) ? gladiatorextrawavestunscale : (d->health <= 0 ? gladiatorextradeadstunscale : gladiatorextrahitstunscale);
                            amt *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                        }
                        float s = WF(WK(flags), weap, stunscale, WS(flags))*amt, g = WF(WK(flags), weap, stunfall, WS(flags))*amt;
                        d->addstun(weap, lastmillis, int(scale*WF(WK(flags), weap, stuntime, WS(flags))), stun&W_N_STADD ? s : 0.f, stun&W_N_GRADD ? g : 0.f);
                        if(stun&W_N_STIMM && s > 0) d->vel.mul(1.f-clamp(s, 0.f, 1.f));
                        if(stun&W_N_GRIMM && g > 0) d->falling.mul(1.f-clamp(g, 0.f, 1.f));
                        if(stun&W_N_SLIDE) d->impulse[IM_SLIP] = lastmillis;
                    }
                    if(WF(WK(flags), weap, hitpush, WS(flags)) != 0 || WF(WK(flags), weap, hitvel, WS(flags)) != 0)
                    {
                        float amt = scale;
                        bool doquake = hitdealt(flags) && damage > 0;
                        if(d == v)
                        {
                            float modify = WF(WK(flags), weap, damageself, WS(flags))*G(damageselfscale);
                            if(modify != 0) amt *= 1/modify;
                            else doquake = false;
                        }
                        else if(m_team(gamemode, mutators) && d->team == v->team)
                        {
                            float modify = WF(WK(flags), weap, damageteam, WS(flags))*G(damageteamscale);
                            if(modify != 0) amt *= 1/modify;
                            else doquake = false;
                        }
                        float hit = WF(WK(flags), weap, hitpush, WS(flags));
                        if(d == v) hit *= WF(WK(flags), weap, hitpushself, WS(flags));
                        if(hit != 0)
                        {
                            float modify = WRS(flags&HIT(WAVE) || !hitdealt(flags) ? wavepushscale : (d->health <= 0 ? deadpushscale : hitpushscale), push, gamemode, mutators);
                            if(m_dm_gladiator(gamemode, mutators))
                            {
                                float extra = flags&HIT(WAVE) || !hitdealt(flags) ? gladiatorextrawavepushscale : (d->health <= 0 ? gladiatorextradeadpushscale : gladiatorextrahitpushscale);
                                modify *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                            }
                            d->vel.add(vec(dir).mul(hit*modify));
                            if(doquake) d->quake = min(d->quake+max(int(hit), 1), quakelimit);
                        }
                        hit = WF(WK(flags), weap, hitvel, WS(flags))*amt;
                        if(hit != 0)
                        {
                            float modify = WRS(flags&HIT(WAVE) || !hitdealt(flags) ? wavevelscale : (d->health <= 0 ? deadvelscale : hitvelscale), vel, gamemode, mutators);
                            if(m_dm_gladiator(gamemode, mutators))
                            {
                                float extra = flags&HIT(WAVE) || !hitdealt(flags) ? gladiatorextrawavevelscale : (d->health <= 0 ? gladiatorextradeadvelscale : gladiatorextrahitvelscale);
                                modify *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                            }
                            d->vel.add(vec(vel).mul(hit*modify));
                            if(doquake) d->quake = min(d->quake+max(int(hit), 1), quakelimit);
                        }
                    }
                }
            }
            ai::damaged(d, v, weap, flags, damage);
        }
    }

    void damaged(int weap, int flags, int damage, int health, gameent *d, gameent *v, int millis, vec &dir, vec &vel, float dist)
    {
        if(d->state != CS_ALIVE || !gs_playing(gamestate)) return;
        if(hitdealt(flags))
        {
            if(!m_insta(gamemode, mutators) && damagecritical > 0 && damagecriticalsound&(d == focus ? 1 : 2))
            {
                int hp = d->gethealth(gamemode, mutators), crit = int(hp*damagecritical);
                if(d->health > crit && health <= crit) playsound(S_CRITICAL, d->o, d);
            }
            d->health = health;
            if(damage > 0)
            {
                d->lastregen = d->lastregenamt = 0;
                d->lastpain = lastmillis;
                v->totaldamage += damage;
            }
        }
        hiteffect(weap, flags, damage, d, v, dir, vel, dist, v == player1 || v->ai);
    }

    void killed(int weap, int flags, int damage, gameent *d, gameent *v, vector<gameent *> &log, int style, int material)
    {
        d->lastregen = d->lastregenamt = 0;
        d->lastpain = lastmillis;
        d->state = CS_DEAD;
        d->obliterated = (style&FRAG_OBLITERATE)!=0;
        d->headless = (style&FRAG_HEADSHOT)!=0;
        bool burning = burn(d, weap, flags), bleeding = bleed(d, weap, flags), shocking = shock(d, weap, flags),
             isfocus = d == focus || v == focus, isme = d == player1 || v == player1,
             allowanc = obitannounce && (obitannounce >= 2 || isfocus) && isme && v->actortype < A_ENEMY;
        int anc = d == focus && allowanc ? S_V_FRAGGED : -1, dth = d->actortype >= A_ENEMY || d->obliterated ? S_SPLOSH : S_DEATH,
            curmat = material&MATF_VOLUME;
        if(d != player1) d->resetinterp();
        if(!isme) loopv(log) if(log[i] == player1)
        {
            isme = true;
            break;
        }
        formatstring(d->obit, "%s ", colourname(d));
        if(d != v && v->lastattacker == d->clientnum) v->lastattacker = -1;
        d->lastattacker = v->clientnum;
        if(d == v)
        {
            concatstring(d->obit, "\fs");
            if(!obitverbose) concatstring(d->obit, obitdied);
            else if(flags&HIT(SPAWN)) concatstring(d->obit, obitspawn);
            else if(flags&HIT(TOUCH)) concatstring(d->obit, *obittouch ? obittouch : obitsplat);
            else if(flags&HIT(CRUSH)) concatstring(d->obit, *obitcrush ? obitcrush : obitsquish);
            else if(flags&HIT(SPEC)) concatstring(d->obit, obitspectator);
            else if(flags&HIT(MATERIAL) && curmat&MAT_WATER) concatstring(d->obit, getobitwater(material, obitdrowned));
            else if(flags&HIT(MATERIAL) && curmat&MAT_LAVA) concatstring(d->obit, getobitlava(material, obitmelted));
            else if(flags&HIT(MATERIAL) && (material&MATF_FLAGS)&MAT_HURT) concatstring(d->obit, *obithurt ? obithurt : obithurtmat);
            else if(flags&HIT(MATERIAL)) concatstring(d->obit, *obitdeath ? obitdeath : obitdeathmat);
            else if(flags&HIT(LOST)) concatstring(d->obit, *obitfall ? obitfall : obitlost);
            else if(flags && isweap(weap) && !burning && !bleeding && !shocking) concatstring(d->obit, WF(WK(flags), weap, obitsuicide, WS(flags)));
            else if(flags&HIT(BURN) || burning) concatstring(d->obit, obitburnself);
            else if(flags&HIT(BLEED) || bleeding) concatstring(d->obit, obitbleedself);
            else if(flags&HIT(SHOCK) || shocking) concatstring(d->obit, obitshockself);
            else if(d->obliterated) concatstring(d->obit, obitobliterated);
            else if(d->headless) concatstring(d->obit, obitheadless);
            else concatstring(d->obit, obitsuicide);
            concatstring(d->obit, "\fS");
        }
        else
        {
            concatstring(d->obit, "was \fs");
            if(!obitverbose) concatstring(d->obit, obitfragged);
            else if(burning) concatstring(d->obit, obitburn);
            else if(bleeding) concatstring(d->obit, obitbleed);
            else if(shocking) concatstring(d->obit, obitshock);
            else if(isweap(weap))
            {
                if(d->obliterated) concatstring(d->obit, WF(WK(flags), weap, obitobliterated, WS(flags)));
                else if(d->headless) concatstring(d->obit, WF(WK(flags), weap, obitheadless, WS(flags)));
                else concatstring(d->obit, WF(WK(flags), weap, obituary, WS(flags)));
            }
            else concatstring(d->obit, obitkilled);
            concatstring(d->obit, "\fS by");
            bool override = false;
            if(d->headless)
            {
                v->addicon(eventicon::HEADSHOT, totalmillis, eventiconfade, 0);
                if(!override && allowanc) anc = S_V_HEADSHOT;
            }
            if(v->actortype >= A_ENEMY)
            {
                concatstring(d->obit, " a ");
                concatstring(d->obit, colourname(v));
            }
            else if(m_team(gamemode, mutators) && d->team == v->team)
            {
                concatstring(d->obit, " \fs\fzawteam-mate\fS ");
                concatstring(d->obit, colourname(v));
                if(v == focus)
                {
                    anc = S_ALARM;
                    override = true;
                }
            }
            else if(obitstyles)
            {
                if(style&FRAG_REVENGE)
                {
                    concatstring(d->obit, " \fs\fzoyvengeful\fS");
                    v->addicon(eventicon::REVENGE, totalmillis, eventiconfade); // revenge
                    v->dominating.removeobj(d);
                    d->dominated.removeobj(v);
                    if(allowanc)
                    {
                        anc = S_V_REVENGE;
                        override = true;
                    }
                }
                else if(style&FRAG_DOMINATE)
                {
                    concatstring(d->obit, " \fs\fzoydominating\fS");
                    v->addicon(eventicon::DOMINATE, totalmillis, eventiconfade); // dominating
                    if(v->dominated.find(d) < 0) v->dominated.add(d);
                    if(d->dominating.find(v) < 0) d->dominating.add(v);
                    if(allowanc)
                    {
                        anc = S_V_DOMINATE;
                        override = true;
                    }
                }
                concatstring(d->obit, " ");
                concatstring(d->obit, colourname(v));

                if(style&FRAG_BREAKER)
                {
                    concatstring(d->obit, " \fs\fzpwspree-breaking\fS");
                    v->addicon(eventicon::BREAKER, totalmillis, eventiconfade);
                    if(!override && allowanc) anc = S_V_BREAKER;
                }

                if(style&FRAG_MKILL1)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwdouble-killing\fS");
                    v->addicon(eventicon::MULTIKILL, totalmillis, eventiconfade, 0);
                    if(!override && allowanc) anc = S_V_MULTI;
                }
                else if(style&FRAG_MKILL2)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwtriple-killing\fS");
                    v->addicon(eventicon::MULTIKILL, totalmillis, eventiconfade, 1);
                    if(!override && allowanc) anc = S_V_MULTI2;
                }
                else if(style&FRAG_MKILL3)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwmulti-killing\fS");
                    v->addicon(eventicon::MULTIKILL, totalmillis, eventiconfade, 2);
                    if(!override && allowanc) anc = S_V_MULTI3;
                }
            }
            else
            {
                concatstring(d->obit, " ");
                concatstring(d->obit, colourname(v));
            }
            if(obitstyles)
            {
                if(style&FRAG_FIRSTBLOOD)
                {
                    concatstring(d->obit, " for \fs\fzrwfirst blood\fS");
                    v->addicon(eventicon::FIRSTBLOOD, totalmillis, eventiconfade, 0);
                    if(allowanc)
                    {
                        anc = S_V_FIRSTBLOOD;
                        override = true;
                    }
                }

                if(style&FRAG_SPREE1)
                {
                    concatstring(d->obit, " in total \fs\fzywcarnage\fS");
                    v->addicon(eventicon::SPREE, totalmillis, eventiconfade, 0);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE2)
                {
                    concatstring(d->obit, " on a \fs\fzywslaughter\fS");
                    v->addicon(eventicon::SPREE, totalmillis, eventiconfade, 1);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE2;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE3)
                {
                    concatstring(d->obit, " on a \fs\fzywmassacre\fS");
                    v->addicon(eventicon::SPREE, totalmillis, eventiconfade, 2);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE3;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE4)
                {
                    concatstring(d->obit, " in a \fs\fzyibloodbath\fS");
                    v->addicon(eventicon::SPREE, totalmillis, eventiconfade, 3);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE4;
                        override = true;
                    }
                }
            }
        }
        if(d != v)
        {
            if(showobitdists >= (d != player1 ? 2 : 1))
            {
                defformatstring(obitx, " \fs\fo@\fy%.2f\fom\fS", v->o.dist(d->o)/8.f);
                concatstring(d->obit, obitx);
            }
            if(showobithpleft >= (d != player1 ? 2 : 1))
            {
                string obitx;
                if(damageinteger) formatstring(obitx, " (\fs\fc%d\fS)", int(ceilf(v->health/damagedivisor)));
                else formatstring(obitx, " (\fs\fc%.1f\fS)", v->health/damagedivisor);
                concatstring(d->obit, obitx);
            }
        }
        if(!log.empty())
        {
            if(obitverbose || obitstyles) concatstring(d->obit, rnd(2) ? ", assisted by" : ", helped by");
            else concatstring(d->obit, " +");
            loopv(log) if(log[i])
            {
                if(obitverbose || obitstyles)
                    concatstring(d->obit, log.length() > 1 && i == log.length()-1 ? " and " : (i ? ", " : " "));
                else concatstring(d->obit, log.length() > 1 && i == log.length()-1 ? " + " : (i ? " + " : " "));
                if(log[i]->actortype >= A_ENEMY) concatstring(d->obit, "a ");
                concatstring(d->obit, colourname(log[i]));
                if(showobithpleft >= (d != player1 ? 2 : 1))
                {
                    string obitx;
                    if(damageinteger) formatstring(obitx, " (\fs\fc%d\fS)", int(ceilf(log[i]->health/damagedivisor)));
                    else formatstring(obitx, " (\fs\fc%.1f\fS)", log[i]->health/damagedivisor);
                    concatstring(d->obit, obitx);
                }
            }
        }
        if(d != v)
        {
            if(v->state == CS_ALIVE && d->actortype < A_ENEMY)
            {
                copystring(v->obit, d->obit);
                v->lastkill = totalmillis ? totalmillis : 1;
            }
        }
        if(dth >= 0) playsound(dth, d->o, d, 0, -1, -1, -1, &d->vschan);
        if(d->actortype < A_ENEMY)
        {
            if(showobituaries)
            {
                bool show = false;
                switch(showobituaries)
                {
                    case 1: if(isme || m_duke(gamemode, mutators)) show = true; break;
                    case 2: if(isme || anc >= 0 || m_duke(gamemode, mutators)) show = true; break;
                    case 3: if(isme || d->actortype == A_PLAYER || anc >= 0 || m_duke(gamemode, mutators)) show = true; break;
                    case 4: if(isme || d->actortype == A_PLAYER || v->actortype == A_PLAYER || anc >= 0 || m_duke(gamemode, mutators)) show = true; break;
                    case 5: default: show = true; break;
                }
                if(show) announcef(anc, CON_GAME, d, false, "\fw%s", d->obit);
            }
            else if(anc >= 0) announce(anc, d);
            if(anc >= 0 && d != v) announce(anc, v);
        }
        vec pos = d->headtag();
        pos.z -= d->zradius*0.125f;

        if(d->headless && !nogore && headlessmodels && vanitycheck(d))
        {
            if(d->vitems.empty()) vanitybuild(d);
            int found[VANITYMAX] = {0};
            loopvk(d->vitems) if(vanities.inrange(d->vitems[k]))
            {
                if(found[vanities[d->vitems[k]].type]) continue;
                if(!(vanities[d->vitems[k]].cond&2)) continue;
                projs::create(pos, pos, true, d, PRJ_VANITY, -1, 0, (rnd(gibfade)+gibfade)*2, 0, 0, rnd(50)+10, -1, d->vitems[k], 0, 0);
                found[vanities[d->vitems[k]].type]++;
            }
        }

        if(nogore != 2 && gibscale > 0 && !(flags&HIT(LOST)))
        {
            int hp = max(d->gethealth(gamemode, mutators)/10, 1), gib = clamp(max(damage, hp)/(d->obliterated ? 10 : 20), 2, 10), amt = int((rnd(gib)+gib)*(1+gibscale));
            loopi(amt) projs::create(pos, pos, true, d, nogore ? PRJ_DEBRIS : PRJ_GIBS, -1, 0, rnd(gibfade)+gibfade, 0, rnd(250)+1, rnd(d->obliterated ? 80 : 40)+10);
        }
        if(m_team(gamemode, mutators) && d->team == v->team && d != v && v == player1 && isweap(weap) && WF(WK(flags), weap, damagepenalty, WS(flags)) != 0)
        {
            hud::teamkills.add(totalmillis);
            if(hud::numteamkills() >= teamkillwarn) hud::lastteam = totalmillis ? totalmillis : 1;
        }
        if(m_bomber(gamemode)) bomber::killed(d, v);
        ai::killed(d, v);
    }

    void timeupdate(int state, int remain, int elapsed)
    {
        int oldstate = gamestate;
        gamestate = state;
        timeremaining = remain;
        timeelapsed = elapsed;
        timelast = gs_timeupdate(gamestate) ? lastmillis : totalmillis;
        if(gs_timeupdate(gamestate) != gs_timeupdate(oldstate)) entities::updaterails();
        if(gs_intermission(gamestate) && gs_playing(oldstate))
        {
            player1->stopmoving(true);
            if(gamestate == G_S_INTERMISSION) hud::showscores(true, true);
            smartmusic(true);
        }
        if(gamestate == G_S_VOTING && oldstate != G_S_VOTING)
        {
            hud::showscores(false);
            UI::openui("maps");
        }
    }

    gameent *newclient(int cn)
    {
        if(cn < 0 || cn >= MAXPLAYERS)
        {
            defformatstring(cnmsg, "clientnum [%d]", cn);
            neterr(cnmsg);
            return NULL;
        }

        if(cn == player1->clientnum) return player1;

        while(cn >= players.length()) players.add(NULL);

        if(!players[cn])
        {
            gameent *d = new gameent();
            d->clientnum = cn;
            players[cn] = d;
        }

        return players[cn];
    }

    gameent *getclient(int cn)
    {
        if(cn == player1->clientnum) return player1;
        if(players.inrange(cn)) return players[cn];
        return NULL;
    }

    void clientdisconnected(int cn, int reason)
    {
        if(!players.inrange(cn)) return;
        gameent *d = players[cn];
        if(!d) return;
        if(d->name[0] && client::showpresence >= (client::waiting(false) ? 2 : 1))
        {
            stringz(formattedreason);
            stringz(ipaddr);
            if(reason >= 0) formatstring(formattedreason, " (%s)", disc_reasons[reason]);
            if(client::showpresencehostinfo && client::haspriv(game::player1, G(iphostlock))) formatstring(ipaddr, " (%s)", d->hostip);
            if(d->actortype == A_PLAYER)
            {
                int amt = client::otherclients(); // not including self to disclude this player
                conoutft(CON_EVENT, "\fo%s%s left the game%s (%d %s)", colourname(d), ipaddr, formattedreason, amt, amt != 1 ? "players" : "player");
            }
            else if(d->actortype == A_BOT && ai::showaiinfo)
                conoutft(CON_EVENT, "\fo%s was removed from the game%s", colourname(d), formattedreason);
        }
        gameent *e = NULL;
        int numdyns = numdynents();
        loopi(numdyns) if((e = (gameent *)iterdynents(i)))
        {
            e->dominating.removeobj(d);
            e->dominated.removeobj(d);
            if(e->ai) loopvj(e->ai->state)
            {
                if(e->ai->state[j].owner == cn) e->ai->state[j].owner = -1;
                if(e->ai->state[j].targtype == ai::AI_T_ACTOR && e->ai->state[j].target == cn) e->ai->state.remove(j--);
            }
        }
        specreset(d, true);
        waiting.removeobj(d);
        client::clearvotes(d);
        projs::removeplayer(d);
        hud::removeplayer(d);
        removedamagemerges(d);
        if(m_capture(gamemode)) capture::removeplayer(d);
        else if(m_defend(gamemode)) defend::removeplayer(d);
        else if(m_bomber(gamemode)) bomber::removeplayer(d);
        DELETEP(players[cn]);
        players[cn] = NULL;
        cleardynentcache();
        clearwindemitters();
    }

    void preload()
    {
        ai::preload();
        weapons::preload();
        projs::preload();
        if(m_edit(gamemode) || m_capture(gamemode)) capture::preload();
        if(m_edit(gamemode) || m_defend(gamemode)) defend::preload();
        if(m_edit(gamemode) || m_bomber(gamemode)) bomber::preload();
        flushpreloadedmodels();
    }

    void resetmap(bool empty) // called just before a map load
    {
    }

    void savemap(bool force, const char *mname)
    {
        ai::savemap(force, mname);
    }

    VAR(IDF_PERSIST, showloadoutmenu, 0, 0, 1); // show the loadout menu at the start of a map

    void startmap(bool empty) // called just after a map load
    {
        ai::startmap(empty);
        if(!empty)
        {
            gamestate = G_S_WAITING;
            mapstart = maptime = 0;
        }
        specreset();
        removedamagemergeall();
        projs::reset();
        physics::reset();
        resetworld();
        resetcursor();
        if(!empty) preload();
        gameent *d;
        int numdyns = numdynents();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) && gameent::is(d)) d->mapchange(lastmillis, gamemode, mutators);
        entities::spawnplayer(player1); // prevent the player from being in the middle of nowhere
        if(showloadoutmenu && m_loadout(gamemode, mutators)) wantsloadoutmenu = true;
        resetcamera();
        resetsway();
    }

    gameent *intersectclosest(vec &from, vec &to, gameent *at)
    {
        gameent *best = NULL, *o;
        float bestdist = 1e16f;
        int numdyns = numdynents();
        loopi(numdyns) if((o = (gameent *)iterdynents(i)))
        {
            if(!o || o == at || o->state != CS_ALIVE || !physics::issolid(o, at)) continue;
            float dist;
            if(intersect(o, from, to, dist, GUARDRADIUS) && dist < bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        return best;
    }

    int numdynents(bool all)
    {
        int i = 1+players.length();
        if(all) i += projs::collideprojs.length();
        if(all) i += entities::inanimates.length();
        return i;
    }

    dynent *iterdynents(int i, bool all)
    {
        if(!i) return player1;
        i--;
        if(i < players.length()) return players[i];
        i -= players.length();
        if(all)
        {
            if(i < projs::collideprojs.length()) return projs::collideprojs[i];
            i -= projs::collideprojs.length();
            if(i < entities::inanimates.length()) return entities::inanimates[i];
        }
        return NULL;
    }

    dynent *focusedent(bool force)
    {
        if(force) return player1;
        return focus;
    }

    bool duplicatename(gameent *d, char *name = NULL)
    {
        if(!name) name = d->name;
        if(!client::demoplayback && d != player1 && !strcmp(name, player1->name)) return true;
        loopv(players) if(players[i] && d != players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    int levelcolour(int colour, float level)
    {
        if(level != 1)
            return (clamp(int((colour>>16)*level), 0, 255)<<16)|(clamp(int(((colour>>8)&0xFF)*level), 0, 255)<<8)|(clamp(int((colour&0xFF)*level), 0, 255));
        return colour;
    }

    int findcolour(gameent *d, bool tone, bool mix, float level)
    {
        if(tone)
        {
            int col = d->colour;
            if(!col && isweap(d->weapselect))
            {
                col = W(d->weapselect, colour);
                int lastweap = d->getlastweap(m_weapon(d->actortype, gamemode, mutators));
                if(isweap(lastweap) && d->weapselect != lastweap && (d->weapstate[d->weapselect] == W_S_USE || d->weapstate[d->weapselect] == W_S_SWITCH))
                {
                    float amt = (lastmillis-d->weaptime[d->weapselect])/float(d->weapwait[d->weapselect]);
                    int r2 = (col>>16), g2 = ((col>>8)&0xFF), b2 = (col&0xFF),
                        c = W(lastweap, colour), r1 = (c>>16), g1 = ((c>>8)&0xFF), b1 = (c&0xFF),
                        r3 = clamp(int((r1*(1-amt))+(r2*amt)), 0, 255),
                        g3 = clamp(int((g1*(1-amt))+(g2*amt)), 0, 255),
                        b3 = clamp(int((b1*(1-amt))+(b2*amt)), 0, 255);
                    col = (r3<<16)|(g3<<8)|b3;
                }
            }
            if(col)
            {
                if(mix && playertonemix > 0)
                {
                    int r1 = (col>>16), g1 = ((col>>8)&0xFF), b1 = (col&0xFF),
                        c = TEAM(d->team, colour), r2 = (c>>16), g2 = ((c>>8)&0xFF), b2 = (c&0xFF),
                        r3 = clamp(int((r1*(1-playertonemix))+(r2*playertonemix)), 0, 255),
                        g3 = clamp(int((g1*(1-playertonemix))+(g2*playertonemix)), 0, 255),
                        b3 = clamp(int((b1*(1-playertonemix))+(b2*playertonemix)), 0, 255);
                    col = (r3<<16)|(g3<<8)|b3;
                }
                return levelcolour(col, level);
            }
        }
        return levelcolour(TEAM(d->team, colour), level);
    }

    int getcolour(gameent *d, int type, float level)
    {
        switch(type)
        {
            case -1: return findcolour(d, true, false, level); break;
            case CTONE_TMIX: return findcolour(d, true, d->team != T_NEUTRAL, level); break;
            case CTONE_AMIX: return findcolour(d, true, d->team == T_NEUTRAL, level); break;
            case CTONE_MIXED: return findcolour(d, true, true, level); break;
            case CTONE_ALONE: return findcolour(d, d->team != T_NEUTRAL, false, level); break;
            case CTONE_TEAMED: return findcolour(d, d->team == T_NEUTRAL, false, level); break;
            case CTONE_TONE: return findcolour(d, true, false, level); break;
            case CTONE_TEAM: return findcolour(d, false, false, level); break;
            case -2: default: return levelcolour(d->colour, level); break;
        }
        return 0;
    }

    const char *colourname(gameent *d, char *name, bool icon, bool dupname, int colour)
    {
        if(!name) name = d->name;
        static string colored;
        string colortmp;
        colored[0] = '\0';
        if(colour) concatstring(colored, "\fs");
        if(icon)
        {
            if(colour&1)
            {
                formatstring(colortmp, "\f[%d]", findcolour(d));
                concatstring(colored, colortmp);
            }
            formatstring(colortmp, "\f($priv%stex)", server::privnamex(d->privilege, d->actortype, true));
            concatstring(colored, colortmp);
        }
        if(colour&2)
        {
            formatstring(colortmp, "\f[%d]", TEAM(d->team, colour));
            concatstring(colored, colortmp);
        }
        concatstring(colored, name);
        if(!name[0] || (d->actortype < A_ENEMY && dupname && duplicatename(d, name)))
        {
            formatstring(colortmp, "%s[%d]", name[0] ? " " : "", d->clientnum);
            concatstring(colored, colortmp);
        }
        if(colour) concatstring(colored, "\fS");
        return colored;
    }

    const char *teamtexnamex(int team)
    {
        const char *teamtexs[T_MAX] = { "teamneutraltex", "teamalphatex", "teamomegatex", "teamenemytex" };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *colourteam(int team, const char *icon)
    {
        if(team < 0 || team > T_MAX) team = T_NEUTRAL;
        static string teamed;
        teamed[0] = '\0';
        concatstring(teamed, "\fs");
        concformatstring(teamed, "\f[%d]", TEAM(team, colour));
        if(icon != NULL) concformatstring(teamed, "\f($%s)", *icon ? icon : teamtexnamex(team));
        concatstring(teamed, TEAM(team, name));
        concatstring(teamed, "\fS");
        return teamed;
    }

    ICOMMAND(0, getteamname, "i", (int *team), result(*team >= 0 && *team < T_MAX ? TEAM(*team, name) : ""));
    ICOMMAND(0, getteamcolour, "i", (int *team), intret(*team >= 0 && *team < T_MAX ? TEAM(*team, colour) : 0));

    void suicide(gameent *d, int flags)
    {
        if((d != player1 && !d->ai) || d->state != CS_ALIVE || d->suicided >= 0) return;
        burn(d, -1, flags);
        bleed(d, -1, flags);
        shock(d, -1, flags);
        client::addmsg(N_SUICIDE, "ri3", d->clientnum, flags, d->inmaterial);
        d->suicided = lastmillis;
    }
    ICOMMAND(0, suicide, "",  (), { suicide(player1); });

    vec pulsecolour(physent *d, int i, int cycle)
    {
        size_t seed = size_t(d) + (lastmillis/cycle);
        int n = detrnd(seed, 2*PULSECOLOURS), n2 = detrnd(seed + 1, 2*PULSECOLOURS), q = clamp(i, 0, int(PULSE_LAST));
        return vec::fromcolor(pulsecols[q][n%PULSECOLOURS]).lerp(vec::fromcolor(pulsecols[q][n2%PULSECOLOURS]), (lastmillis%cycle)/float(cycle));
    }

    int pulsehexcol(physent *d, int i, int cycle)
    {
        bvec h = bvec::fromcolor(pulsecolour(d, i, cycle));
        return (h.r<<16)|(h.g<<8)|h.b;
    }

    void particletrack(particle *p, uint type, int &ts, bool step)
    {
        if(!p || !p->owner || !gameent::is(p->owner)) return;
        gameent *d = (gameent *)p->owner;
        switch(type&0xFF)
        {
            case PT_TEXT: case PT_ICON:
            {
                vec q = p->owner->abovehead(p->m.z+4);
                float k = pow(aboveheadsmooth, float(curtime)/float(aboveheadsmoothmillis));
                p->o.mul(k).add(q.mul(1-k));
                break;
            }
            case PT_TAPE: case PT_LIGHTNING:
            {
                float dist = p->o.dist(p->d);
                p->d = p->o = d->muzzletag(d->weapselect);
                p->d.add(vec(d->yaw*RAD, d->pitch*RAD).mul(dist));
                break;
            }
            case PT_PART: case PT_EXPLOSION: case PT_FLARE:
            {
                p->o = d->muzzletag(d->weapselect);
                break;
            }
            default: break;
        }
    }

    void dynlighttrack(physent *owner, vec &o, vec &hud)
    {
        if(owner->type != ENT_PLAYER) return;
        o = owner->o;
        hud = owner == focus ? vec(owner->o).add(vec(0, 0, 2)) : owner->o;
    }

    void newmap(int size, const char *mname) { client::addmsg(N_NEWMAP, "ris", size, mname); }

    void fixfullrange(float &yaw, float &pitch, float &roll, bool full)
    {
        if(full)
        {
            while(pitch < -180.0f) pitch += 360.0f;
            while(pitch >= 180.0f) pitch -= 360.0f;
            while(roll < -180.0f) roll += 360.0f;
            while(roll >= 180.0f) roll -= 360.0f;
        }
        else
        {
            if(pitch > 89.9f) pitch = 89.9f;
            if(pitch < -89.9f) pitch = -89.9f;
            if(roll > 89.9f) roll = 89.9f;
            if(roll < -89.9f) roll = -89.9f;
        }
        while(yaw < 0.0f) yaw += 360.0f;
        while(yaw >= 360.0f) yaw -= 360.0f;
    }

    void fixrange(float &yaw, float &pitch)
    {
        float r = 0.f;
        fixfullrange(yaw, pitch, r, false);
    }

    void fixview()
    {
        if(inzoom())
        {
            checkzoom();
            int frame = lastmillis-lastzoom;
            float zoom = W(focus->weapselect, cookzoommax)-((W(focus->weapselect, cookzoommax)-W(focus->weapselect, cookzoommin))/float(zoomlevels)*zoomlevel),
                  diff = float(fov()-zoom), amt = frame < W(focus->weapselect, cookzoom) ? clamp(frame/float(W(focus->weapselect, cookzoom)), 0.f, 1.f) : 1.f;
            if(!zooming) amt = 1.f-amt;
            curfov = fov()-(amt*diff);
        }
        else curfov = float(fov());
    }

    bool mousemove(int dx, int dy, int x, int y, int w, int h)
    {
        #define mousesens(a,b,c) ((float(a)/float(b))*c)
        if(hud::hasinput(true))
        {
            cursorx = clamp(cursorx+mousesens(dx, w, mousesensitivity), 0.f, 1.f);
            cursory = clamp(cursory+mousesens(dy, h, mousesensitivity), 0.f, 1.f);
            return true;
        }
        else if(!tvmode())
        {
            physent *d = (!gs_playing(gamestate) || player1->state >= CS_SPECTATOR) && (focus == player1 || followaim()) ? camera1 : (allowmove(player1) ? player1 : NULL);
            if(d)
            {
                float scale = (focus == player1 && inzoom() && zoomsensitivity > 0 ? (1.f-((zoomlevel+1)/float(zoomlevels+2)))*zoomsensitivity : 1.f)*sensitivity;
                d->yaw += mousesens(dx, sensitivityscale, yawsensitivity*scale);
                d->pitch -= mousesens(dy, sensitivityscale, pitchsensitivity*scale*(mouseinvert ? -1.f : 1.f));
                fixrange(d->yaw, d->pitch);
            }
            return true;
        }
        return false;
    }

    void project()
    {
        bool input = hud::hasinput(true), view = thirdpersonview(true, focus), mode = tvmode();
        if(input != inputmouse || (view != inputview || mode != inputmode || focus != lastfocus))
        {
            if(input != inputmouse) resetcursor();
            else resetcamera();
            inputmouse = input;
            inputview = view;
            inputmode = mode;
            lastfocus = focus;
        }
        if(!input)
        {
            int tpc = focus != player1 ? 1 : thirdpersoncursor;
            if(tpc && view) switch(tpc)
            {
                case 1:
                {
                    vec loc(0, 0, 0), pos = worldpos, dir = vec(worldpos).sub(focus->o);
                    if(thirdpersoncursordist > 0 && dir.magnitude() > thirdpersoncursordist) pos = dir.normalize().mul(thirdpersoncursordist).add(focus->o);
                    if(vectocursor(pos, loc.x, loc.y, loc.z))
                    {
                        float amt = curtime/float(thirdpersoninterp);
                        cursorx = clamp(cursorx+((loc.x-cursorx)*amt), 0.f, 1.f);
                        cursory = clamp(cursory+((loc.y-cursory)*amt), 0.f, 1.f);
                    }
                    break;
                }
                case 2:
                {
                    cursorx = thirdpersoncursorx;
                    cursory = thirdpersoncursory;
                    break;
                }
            }
            vecfromcursor(cursorx, cursory, 1.f, cursordir);
        }
    }

    void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
    {
        float dist = from.dist(pos);
        yaw = -atan2(pos.x-from.x, pos.y-from.y)/RAD;
        pitch = asin((pos.z-from.z)/dist)/RAD;
        fixrange(yaw, pitch);
    }

    void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float yawspeed, float pitchspeed, float rotate)
    {
        if(yaw < targyaw-180.0f) yaw += 360.0f;
        if(yaw > targyaw+180.0f) yaw -= 360.0f;
        float offyaw = (rotate < 0 ? fabs(rotate) : (rotate > 0 ? min(float(fabs(targyaw-yaw)), rotate) : fabs(targyaw-yaw)))*yawspeed,
              offpitch = (rotate < 0 ? fabs(rotate) : (rotate > 0 ? min(float(fabs(targpitch-pitch)), rotate) : fabs(targpitch-pitch)))*pitchspeed;
        if(targyaw > yaw)
        {
            yaw += offyaw;
            if(targyaw < yaw) yaw = targyaw;
        }
        else if(targyaw < yaw)
        {
            yaw -= offyaw;
            if(targyaw > yaw) yaw = targyaw;
        }
        if(targpitch > pitch)
        {
            pitch += offpitch;
            if(targpitch < pitch) pitch = targpitch;
        }
        else if(targpitch < pitch)
        {
            pitch -= offpitch;
            if(targpitch > pitch) pitch = targpitch;
        }
        fixrange(yaw, pitch);
    }

    vec thirdpos(const vec &pos, float yaw, float pitch, float dist, float side)
    {
        static struct tpcam : physent
        {
            tpcam()
            {
                physent::reset();
                type = ENT_CAMERA;
                state = CS_ALIVE;
                height = zradius = radius = xradius = yradius = 2;
            }
        } c;
        c.o = pos;
        if(dist || side)
        {
            vec dir[3] = { vec(0, 0, 0), vec(0, 0, 0), vec(0, 0, 0) };
            if(dist) vecfromyawpitch(yaw, pitch, -1, 0, dir[0]);
            if(side) vecfromyawpitch(yaw, pitch, 0, -1, dir[1]);
            dir[2] = dir[0].mul(dist).add(dir[1].mul(side));
            float mag = dir[2].magnitude();
            if(mag > 0)
            {
                dir[2].mul(1/mag);
                physics::movecamera(&c, dir[2], mag, 0.1f);
            }
        }
        return c.o;
    }

    float firstpersonspineoffset = 0;
    vec firstpos(physent *d, const vec &pos, float yaw, float pitch)
    {
        if(d->state != CS_ALIVE) return pos;
        static struct fpcam : physent
        {
            fpcam()
            {
                physent::reset();
                type = ENT_CAMERA;
                state = CS_ALIVE;
                height = zradius = radius = xradius = yradius = 2;
            }
        } c;

        vec to = pos;
        c.o = pos;
        if(firstpersonspine > 0)
        {
            float spineoff = firstpersonspine*d->zradius*0.5f;
            to.z -= spineoff;
            float lean = clamp(pitch, -firstpersonpitchmin, firstpersonpitchmax);
            if(firstpersonpitchscale >= 0) lean *= firstpersonpitchscale;
            to.add(vec(yaw*RAD, (lean+90)*RAD).mul(spineoff));
        }
        if(firstpersonbob && gs_playing(gamestate) && d->state == CS_ALIVE)
        {
            float scale = 1;
            if(gameent::is(d) && d == focus && inzoom())
            {
                gameent *e = (gameent *)d;
                int frame = lastmillis-lastzoom;
                float pc = frame <= W(e->weapselect, cookzoom) ? frame/float(W(e->weapselect, cookzoom)) : 1.f;
                scale *= zooming ? 1.f-pc : pc;
            }
            if(firstpersonbobtopspeed) scale *= clamp(d->vel.magnitude()/firstpersonbobtopspeed, firstpersonbobmin, 1.f);
            if(scale > 0)
            {
                float steps = bobdist/firstpersonbobstep*M_PI;
                vec dir = vec(yaw*RAD, 0.f).mul(firstpersonbobside*cosf(steps)*scale);
                dir.z = firstpersonbobup*(fabs(sinf(steps)) - 1)*scale;
                to.add(dir);
            }
        }
        c.o.z = to.z; // assume inside ourselves is safe
        vec dir = vec(to).sub(c.o), old = c.o;
        float dist = dir.magnitude();
        if(dist <= 0) return c.o;
        dir.mul(1/dist);
        physics::movecamera(&c, dir, dist, 0.1f);
        firstpersonspineoffset = max(dist-old.dist(c.o), 0.f);
        return c.o;
    }

    vec camerapos(physent *d, bool hasfoc, bool hasyp, float yaw, float pitch)
    {
        vec pos = d->headpos();
        if(d == focus || hasfoc)
        {
            if(!hasyp)
            {
                yaw = d->yaw;
                pitch = d->pitch;
            }
            if(thirdpersonview(true, hasfoc ? d : focus))
                pos = thirdpos(pos, yaw, pitch, d != player1 ? followdist : thirdpersondist, d != player1 ? followside : thirdpersonside);
            else pos = firstpos(d, pos, yaw, pitch);
        }
        return pos;
    }

    void deathcamyawpitch(gameent *d, float &yaw, float &pitch)
    {
        if(!deathcamstyle) return;
        gameent *a = deathcamstyle > 1 ? d : getclient(d->lastattacker);
        if(!a) return;
        float y = 0, p = 0;
        vec dir = vec(a->center()).sub(camera1->o).normalize();
        vectoyawpitch(dir, y, p);
        fixrange(y, p);
        if(deathcamspeed > 0)
        {
            float speed = curtime/float(deathcamspeed);
            scaleyawpitch(yaw, pitch, y, p, speed, speed*4.f);
        }
        else
        {
            yaw = y;
            pitch = p;
        }
    }

    bool camrefresh(cament *cam, bool renew = false)
    {
        bool rejigger = false;
        if(cam->player && cam->type != cament::PLAYER)
        {
            loopv(cameras)
            {
                cament *c = cameras[i];
                if(c->type == cament::PLAYER && c->player == cam->player)
                {
                    cam = c;
                    rejigger = true;
                    break;
                }
            }
            if(!rejigger) cam->player = NULL;
        }
        if(renew || rejigger || (cam->player && focus != cam->player) || (!cam->player && focus != player1))
        {
            if(cam->player) focus = cam->player;
            else focus = player1;
            return true;
        }
        return false;
    }

    bool spectvaiming(gameent *d)
    {
        bool third = d != player1 ? followthirdperson : thirdperson;
        int level = third ? spectvthirdperson : spectvfirstperson;
        if(level >= (d->state == CS_DEAD || d->state == CS_WAITING ? 1 : 2)) return true;
        return false;
    }

    vec camvec(cament *c, float yaw = 0, float pitch = 0)
    {
        if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
        {
            gameentity &e = *(gameentity *)entities::ents[c->id];
            return vec(e.pos()).addz(enttype[e.type].radius);
        }
        else if(c->type == cament::AFFINITY) return thirdpos(c->o, yaw, pitch, followdist);
        else if(c->player) return camerapos(c->player, true, true, yaw, pitch);
        return c->o;
    }

    void getcamyawpitch(cament *c, float &yaw, float &pitch, bool renew = false)
    {
        c->chase = true;
        if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
        {
            gameentity &e = *(gameentity *)entities::ents[c->id];
            bool dynamic = true;
            switch(e.type)
            {
                case PLAYERSTART:
                    yaw = e.attrs[1];
                    pitch = e.attrs[2];
                    break;
                case CAMERA:
                    if(e.attrs[1]&(1<<CAMERA_F_STATIC)) c->chase = false;
                    else if(!renew)
                    {
                        yaw = camera1->yaw;
                        pitch = camera1->pitch;
                        dynamic = false;
                        break;
                    }
                    yaw = e.attrs[2];
                    pitch = e.attrs[3];
                    break;
                default:
                    yaw = 0;
                    pitch = 0;
                    break;
            }
            if(dynamic && e.flags&EF_DYNAMIC)
            {
                yaw += e.yaw;
                pitch += e.pitch;
            }
        }
        else if(c->player)
        {
            yaw = c->player->yaw;
            pitch = c->player->pitch;
            if(!spectvaiming(c->player)) c->chase = false;
        }
        else
        {
            yaw = camera1->yaw;
            pitch = camera1->pitch;
        }
        fixrange(yaw, pitch);
    }

    void getcamdist(cament *c, float &maxdist, float &mindist)
    {
        maxdist = c->player ? spectvfollowmaxdist : spectvmaxdist;
        mindist = c->player ? spectvfollowmindist : spectvmindist;
        if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
        {
            gameentity &e = *(gameentity *)entities::ents[c->id];
            switch(e.type)
            {
                case CAMERA:
                    if(e.attrs[4] > 0) maxdist = e.attrs[4];
                    if(e.attrs[5] > 0) mindist = e.attrs[5];
                    break;
                default: break;
            }
        }
        maxdist = min(maxdist, float(getfog()*2/3));
        mindist = min(mindist, maxdist);
    }

    bool camvisible(cament *c, cament *v, vec &from, float &yaw, float &pitch, float maxdist, float mindist, int iter = 0)
    {
        switch(v->type)
        {
            case cament::AFFINITY:
            {
                if(v->player && (v->player == c->player || !allowspec(v->player, spectvdead, spectvfollowing))) return false;
                break;
            }
            case cament::PLAYER:
            {
                if(!v->player || v->player == c->player || !allowspec(v->player, spectvdead, spectvfollowing)) return false;
                break;
            }
            default: return false;
        }

        #if 0
        if(iter == (v->player ? 2 : 1) && !count)
        {
            vectoyawpitch(vec(v->o).sub(from).safenormalize(), yaw, pitch);
            fixrange(yaw, pitch);
        }
        #endif

        float dist = from.dist(v->o);
        if(dist < mindist || dist > maxdist) return false;
        vec trg;
        switch(iter)
        {
            case 0: default:
                if(getsight(from, yaw, pitch, v->o, trg, maxdist, curfov, fovy)) break; // check if the current view can see this
                return false;
            case 1:
                if(getsight(from, yaw, pitch, v->o, trg, maxdist, curfov + 45.f, fovy + 45.f)) break; // gives a bit more fov wiggle room in case someone went out of shot
                return false;
            case 2: break; // always true
        }

        return true;
    }

    bool camupdate(cament *c, bool renew = false)
    {
        if(c->player && !allowspec(c->player, spectvdead, spectvfollowing)) return false;

        float yaw = 0, pitch = 0, mindist = 0, maxdist = 0;
        getcamyawpitch(c, yaw, pitch, renew);
        getcamdist(c, maxdist, mindist);
        vec from = camvec(c, yaw, pitch);

        loopj(c->chase ? 3 : 1)
        {
            int count = 0;
            vec dir(0, 0, 0);

            c->reset();

            loopv(cameras)
            {
                cament *v = cameras[i];
                if(v == c || !camvisible(c, v, from, yaw, pitch, maxdist, mindist, j)) continue;
                c->inview[v->type]++;
                dir.add(v->o);
                count++;
            }

            if(!count) continue; // can't fix it without a free camera or something visible

            if(j && !dir.iszero()) // extra passes may give a better direction, but may not contain visible players..
            {
                vec(dir).div(count).sub(from).safenormalize();
                vectoyawpitch(dir, yaw, pitch);

                dir = vec(0, 0, 0);
                count = 0;
                c->reset();

                loopv(cameras)
                {
                    cament *v = cameras[i];
                    if(v == c || !camvisible(c, v, from, yaw, pitch, maxdist, mindist)) continue;
                    c->inview[v->type]++;
                    dir.add(v->o);
                    count++;
                }
                if(!count) continue; // nope, that didn't work..
            }

            if(!c->chase)
            {
                getcamyawpitch(c, yaw, pitch, true);
                c->dir = vec(yaw*RAD, pitch*RAD);
            }
            else if(!dir.iszero()) c->dir = vec(dir).div(count).sub(from).safenormalize();
            else continue;
            return true;
        }

        getcamyawpitch(c, yaw, pitch, true);
        c->dir = vec(yaw*RAD, pitch*RAD);

        return false;
    }

    bool camcheck(vec &pos, int csize)
    {
        if(!insideworld(pos)) return false;
        static struct cecam : physent
        {
            cecam()
            {
                physent::reset();
                type = ENT_CAMERA;
                state = CS_ALIVE;
                height = zradius = radius = xradius = yradius = 2;
            }
        } c;
        c.o = pos;
        if(csize) c.o.z += csize;
        if(!collide(&c, vec(0, 0, 0), 0, false) && !collideinside)
        {
            pos = c.o;
            return true;
        }
        if(csize) loopi(csize)
        {
            c.o.z -= 1;
            if(!collide(&c, vec(0, 0, 0), 0, false) && !collideinside)
            {
                pos = c.o;
                return true;
            }
        }
        static const int sphereyawchecks[8] = { 180, 135, 225, 90, 270, 45, 315 }, spherepitchchecks[5] = { 0, 45, -45, 89, -89 };
        loopi(5) loopj(5) loopk(8)
        {
            c.o = vec(pos).add(vec(sphereyawchecks[k]*RAD, spherepitchchecks[j]*RAD).mul((i+1)*2));
            if(!collide(&c, vec(0, 0, 0), 0, false) && !collideinside)
            {
                pos = c.o;
                return true;
            }
        }
        return false;
    }

    void buildcams()
    {
        loopk(2)
        {
            int found = 0;
            loopv(entities::ents)
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                if(k ? (e.type != PLAYERSTART && e.type != WEAPON && e.type != CAMERA) : (e.type != CAMERA || e.attrs[0] != CAMERA_NORMAL)) continue;
                if(enttype[e.type].modesattr >= 0 && !m_check(e.attrs[enttype[e.type].modesattr], e.attrs[enttype[e.type].modesattr+1], game::gamemode, game::mutators)) continue;
                if(enttype[e.type].mvattr >= 0 && !checkmapvariant(e.attrs[enttype[e.type].mvattr])) continue;
                if(enttype[e.type].fxattr >= 0 && !checkmapeffects(e.attrs[enttype[e.type].fxattr])) continue;
                vec pos = e.o;
                float radius = e.type == PLAYERSTART ? actors[A_PLAYER].height+2 : enttype[e.type].radius;
                if(!camcheck(pos, radius)) continue;
                cameras.add(new cament(cameras.length(), cament::ENTITY, i, pos));
                found++;
            }
            if(found) break;
        }

        vec pos = player1->center();
        cameras.add(new cament(cameras.length(), cament::PLAYER, player1->clientnum, pos, player1));

        loopv(players) if(players[i])
        {
            gameent *d = players[i];
            if(d->actortype >= A_ENEMY) continue;
            vec pos = d->center();
            cameras.add(new cament(cameras.length(), cament::PLAYER, d->clientnum, pos, d));
        }

        if(m_capture(gamemode)) capture::checkcams(cameras);
        else if(m_defend(gamemode)) defend::checkcams(cameras);
        else if(m_bomber(gamemode)) bomber::checkcams(cameras);
    }

    bool findcams(cament *cam)
    {
        bool found = false;
        loopv(cameras)
        {
            cament *c = cameras[i];
            if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
            {
                gameentity &e = *(gameentity *)entities::ents[c->id];
                c->o = e.pos();
            }

            if(c->type == cament::PLAYER && (c->player || ((c->player = getclient(c->id)) != NULL)))
            {
                if(!found && c->id == spectvfollowing && c->player->state != CS_SPECTATOR) found = true;
                c->o = c->player->center();
            }
            else if(c->type != cament::PLAYER && c->player) c->player = NULL;

            if(m_capture(gamemode)) capture::updatecam(c);
            else if(m_defend(gamemode)) defend::updatecam(c);
            else if(m_bomber(gamemode)) bomber::updatecam(c);
        }
        return found;
    }

    bool cameratv()
    {
        if(!tvmode(false)) return false;

        if(!gs_playing(gamestate)) spectvfollowing = -1;
        else if(player1->state != CS_SPECTATOR && spectvfollowself >= (m_duke(gamemode, mutators) ? 2 : 1)) spectvfollowing = player1->clientnum;
        else spectvfollowing = spectvfollow;

        if(cameras.empty()) buildcams();

        bool restart = !lastcamera;
        if(!cameras.inrange(lastcamcn))
        {
            lastcamcn = rnd(cameras.length());
            restart = true;
        }

        cament *cam = cameras[lastcamcn];
        bool found = findcams(cam);
        if(!found) spectvfollowing = -1;
        camrefresh(cam);

        #define stvf(z) (!gs_playing(gamestate) ? spectvinterm##z : (spectvfollowing >= 0 ? spectvfollow##z : spectv##z))
        int lastcn = cam->cn, millis = lasttvchg ? totalmillis-lasttvchg : 0;
        bool reset = false, updated = camupdate(cam, restart), override = !lasttvchg || millis >= stvf(mintime),
             timeup = !lasttvcam || totalmillis-lasttvcam >= stvf(time), overtime = stvf(maxtime) && millis >= stvf(maxtime);

        if(restart || overtime || timeup || (!updated && override))
        {
            loopv(cameras) if(cameras[i]->ignore) cameras[i]->ignore = false;
            if(overtime) cam->ignore = true; // so we don't use the last one we just used
            int goodcams = 0;
            loopv(cameras)
            {
                cament *c = cameras[i];
                if(!camupdate(c, true)) continue;
                loopj(cament::MAX) c->lastinview[j] = c->inview[j];
                if(!c->ignore) goodcams++;
            }
            if(!goodcams) cam->ignore = false; // in case there's only one good camera
            else reset = true;
        }

        if(restart || reset || overtime)
        {
            vector<cament *> scams;
            scams.reserve(cameras.length());
            scams.put(cameras.getbuf(), cameras.length());
            scams.sort(cament::compare);
            lastcamcn = scams[0]->cn;
            if(restart || !lasttvcam || cam != cameras[lastcamcn])
            {
                cam = cameras[lastcamcn];
                lasttvcam = totalmillis;
                reset = true;
            }
            else reset = false;
        }

        camrefresh(cam, reset);
        if(!lasttvchg || cam->cn != lastcn)
        {
            cam->resetlast();
            lasttvchg = totalmillis;
            reset = true;
        }

        float yaw = camera1->yaw, pitch = camera1->pitch;
        if(!cam->dir.iszero()) vectoyawpitch(cam->dir, yaw, pitch);

        if(!cam->player || cam->chase)
        {
            if(cam->chase && !reset)
            {
                float speed = curtime/float(cam->player ? followtvspeed : stvf(speed));
                #define SCALEAXIS(x) \
                    float x##scale = 1, adj##x = camera1->x, off##x = x, x##thresh = cam->player ? followtv##x##thresh : stvf(x##thresh); \
                    if(adj##x < x - 180.0f) adj##x += 360.0f; \
                    if(adj##x > x + 180.0f) adj##x -= 360.0f; \
                    off##x -= adj##x; \
                    if(cam->last##x == 0 || (off##x > 0 && cam->last##x < 0) || (off##x < 0 && cam->last##x > 0) || (x##thresh > 0 && (fabs(cam->last##x - off##x) >= x##thresh))) \
                    { \
                        cam->last##x##time = totalmillis; \
                        x##scale = 0; \
                    } \
                    else if(cam->last##x##time) \
                    { \
                        int offtime = totalmillis-cam->last##x##time, x##speed = cam->player ? followtv##x##speed : stvf(x##speed); \
                        if(offtime <= x##speed) x##scale = offtime/float(x##speed); \
                    } \
                    cam->last##x = off##x; \
                    float x##speed = speed*(cam->player ? followtv##x##scale : stvf(x##scale))*x##scale;
                SCALEAXIS(yaw);
                SCALEAXIS(pitch);
                scaleyawpitch(camera1->yaw, camera1->pitch, yaw, pitch, yawspeed, pitchspeed, (cam->player ? followtvrotate : stvf(rotate)));
            }
            else
            {
                camera1->yaw = yaw;
                camera1->pitch = pitch;
            }
        }
        else
        {
            if((focus->state == CS_DEAD || focus->state == CS_WAITING) && focus->lastdeath)
                deathcamyawpitch(focus, camera1->yaw, camera1->pitch);
            else
            {
                camera1->yaw = focus->yaw;
                camera1->pitch = focus->pitch;
                camera1->roll = focus->roll;
            }
        }
        fixrange(camera1->yaw, camera1->pitch);
        if(reset || !cam->player || (cam->player->state == CS_ALIVE || cam->player->state == CS_EDITING))
        {
            camera1->o = camvec(cam, camera1->yaw, camera1->pitch);
            camera1->resetinterp(); // because this just sets position directly
        }
        return true;
    }

    void checkcamera()
    {
        camera1 = &camera;
        if(camera1->type != ENT_CAMERA)
        {
            camera1->reset();
            camera1->type = ENT_CAMERA;
            camera1->state = CS_ALIVE;
            camera1->height = camera1->zradius = camera1->radius = camera1->xradius = camera1->yradius = 2;
        }
        if(player1->state < CS_SPECTATOR && focus != player1 && gs_playing(gamestate)) specreset();
        if(tvmode() || player1->state < CS_SPECTATOR)
        {
            camera1->vel = vec(0, 0, 0);
            camera1->move = camera1->strafe = 0;
        }
    }

    float calcroll(gameent *d)
    {
        bool thirdperson = d != focus || thirdpersonview(true);
        float r = thirdperson ? 0 : d->roll, wobble = float(rnd(quakewobble+1)-quakewobble/2)*(float(min(d->quake, quakelimit))/1000.f);
        switch(d->state)
        {
            case CS_SPECTATOR: case CS_WAITING: r = wobble*0.5f; break;
            case CS_ALIVE: if(d->crouching()) wobble *= 0.5f; r += wobble; break;
            case CS_DEAD: r += wobble; break;
            default: break;
        }
        return r;
    }

    void calcangles(physent *c, gameent *d)
    {
        c->roll = calcroll(d);
        if(firstpersonbob && gs_playing(gamestate) && d->state == CS_ALIVE && !thirdpersonview(true))
        {
            float scale = 1;
            if(d == focus && inzoom())
            {
                int frame = lastmillis-lastzoom;
                float pc = frame <= W(d->weapselect, cookzoom) ? frame/float(W(d->weapselect, cookzoom)) : 1.f;
                scale *= zooming ? 1.f-pc : pc;
            }
            if(firstpersonbobtopspeed) scale *= clamp(d->vel.magnitude()/firstpersonbobtopspeed, firstpersonbobmin, 1.f);
            if(scale > 0)
            {
                vec dir(c->yaw, c->pitch);
                float steps = bobdist/firstpersonbobstep*M_PI, dist = raycube(c->o, dir, firstpersonbobfocusmaxdist, RAY_CLIPMAT|RAY_POLY), yaw, pitch;
                if(dist < 0 || dist > firstpersonbobfocusmaxdist) dist = firstpersonbobfocusmaxdist;
                else if(dist < firstpersonbobfocusmindist) dist = firstpersonbobfocusmindist;
                vectoyawpitch(vec(firstpersonbobside*cosf(steps), dist, firstpersonbobup*(fabs(sinf(steps)) - 1)), yaw, pitch);
                c->yaw -= yaw*firstpersonbobfocus*scale;
                c->pitch -= pitch*firstpersonbobfocus*scale;
                c->roll += (1 - firstpersonbobfocus)*firstpersonbobroll*cosf(steps)*scale;
                fixfullrange(c->yaw, c->pitch, c->roll, false);
            }
        }
    }

    void resetcamera(bool cam, bool input)
    {
        lastcamera = 0;
        zoomset(false, 0);
        if(input && !hud::hasinput(true)) resetcursor();
        checkcamera();
        if(cam || !focus)
        {
            if(!focus) focus = player1;
            camera1->o = camerapos(focus);
            camera1->yaw = focus->yaw;
            camera1->pitch = focus->pitch;
            camera1->roll = calcroll(focus);
            camera1->resetinterp();
            focus->resetinterp();
        }
    }

    void resetworld()
    {
        specreset();
        hud::showscores(false);
        UI::closeui(NULL);
    }

    void resetstate()
    {
        resetworld();
        resetcamera();
        resetsway();
    }

    void updateworld()      // main game update loop
    {
        if(connected())
        {
            if(!curtime || !client::isready)
            {
                gets2c();
                if(player1->clientnum >= 0) client::c2sinfo();
                return;
            }
            int type = client::needsmap ? 6 : (m_edit(gamemode) && musicedit >= 0 ? musicedit : musictype);
            if(!maptime)
            {
                maptime = -1;
                return; // skip the first loop
            }
            else if(maptime < 0)
            {
                maptime = lastmillis ? lastmillis : 1;
                mapstart = totalmillis ? totalmillis : 1;
                if(type != 6) musicdone(false);
                RUNWORLD("on_start");
                resetcamera();
                resetsway();
                return;
            }
            else if(!nosound && mastervol && musicvol && type && !playingmusic())
            {
                if(type == 6) smartmusic(true);
                else
                {
                    defformatstring(musicfile, "%s", mapmusic);
                    if(musicdir[0] && (type == 2 || type == 5 || ((type == 1 || type == 4) && (!musicfile[0] || !fileexists(findfile(musicfile, "r"), "r")))))
                    {
                        vector<char *> files;
                        listfiles(musicdir, NULL, files);
                        while(!files.empty())
                        {
                            int r = rnd(files.length());
                            formatstring(musicfile, "%s/%s", musicdir, files[r]);
                            if(files[r][0] != '.' && strcmp(files[r], "readme.txt") && playmusic(musicfile, type >= 4 ? "music" : NULL)) break;
                            else files.remove(r);
                        }
                    }
                    else if(musicfile[0]) playmusic(musicfile, type >= 4 ? "music" : NULL);
                }
            }
            player1->conopen = commandmillis > 0 || hud::hasinput(true);
            checkoften(player1, true);
            loopv(players) if(players[i]) checkoften(players[i], players[i]->ai != NULL);
            if(!allowmove(player1)) player1->stopmoving(player1->state < CS_SPECTATOR);
            if(focus->state == CS_ALIVE && gs_playing(gamestate) && isweap(focus->weapselect))
            {
                int zoomtime = focus->zooming();
                zoomset(zoomtime ? true : false, zoomtime ? zoomtime : lastmillis);
            }
            else if(zooming) zoomset(false, 0);

            physics::update();
            ai::navigate();
            projs::update();
            ai::update();
            entities::update();
            if(gs_playing(gamestate))
            {
                if(m_capture(gamemode)) capture::update();
                else if(m_bomber(gamemode)) bomber::update();
                if(player1->state == CS_ALIVE) weapons::shoot(player1, worldpos);
            }
            checkplayers();
            flushdamagemerges();
        }
        gets2c();
        adjustscaled(hud::damageresidue, hud::damageresiduefade);
        if(connected())
        {
            checkcamera();
            if(player1->state == CS_DEAD || player1->state == CS_WAITING)
            {
                if(player1->ragdoll) moveragdoll(player1);
                else if(lastmillis-player1->lastpain < 5000)
                    physics::move(player1, 10, true);
            }
            else
            {
                if(player1->ragdoll) cleanragdoll(player1);
                if(player1->state == CS_EDITING) physics::move(player1, 10, true);
                else if(player1->state == CS_ALIVE && gs_playing(gamestate) && !tvmode())
                {
                    physics::move(player1, 10, true);
                    weapons::checkweapons(player1);
                }
            }
            if(gs_playing(gamestate))
            {
                addsway(focus);
                if(player1->state == CS_ALIVE || player1->state == CS_DEAD || player1->state == CS_WAITING)
                    entities::checkitems(player1);
                if(!tvmode() && player1->state >= CS_SPECTATOR)
                {
                    camera1->move = player1->move;
                    camera1->strafe = player1->strafe;
                    physics::move(camera1, 10, true);
                }
            }
            if(hud::canshowscores()) hud::showscores(true);
        }
        updatewind();

        if(player1->clientnum >= 0) client::c2sinfo();
    }

    void adjustorientation(vec &pos)
    {
        gameent *best = NULL, *o = NULL;
        float bestdist = 1e16f;
        int numdyns = numdynents();
        loopi(numdyns) if((o = (gameent *)iterdynents(i)))
        {
            if(!o || o == focus || o->state != CS_ALIVE || !physics::issolid(o, focus)) continue;
            float dist = 1e16f;
            if(intersect(o, camera1->o, pos, dist, GUARDRADIUS) && dist < bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        if(best) pos = vec(pos).sub(camera1->o).normalize().mul(bestdist+best->radius).add(camera1->o);
    }

    void recomputecamera()
    {
        fixview();
        if(client::waiting()) return;
        checkcamera();
        if(!cameratv())
        {
            lasttvchg = lasttvcam = 0;
            if((focus->state == CS_DEAD || (focus != player1 && focus->state == CS_WAITING)) && focus->lastdeath)
                deathcamyawpitch(focus, camera1->yaw, camera1->pitch);
            else
            {
                physent *d = player1->state >= CS_SPECTATOR || (!gs_playing(gamestate) && focus == player1) ? camera1 : focus;
                if(d != camera1 || focus != player1 || !gs_playing(gamestate))
                    camera1->o = camerapos(focus, true, true, d->yaw, d->pitch);
                if(d != camera1 || (!gs_playing(gamestate) && focus == player1) || (focus != player1 && !followaim()))
                {
                    camera1->yaw = (d != camera1 ? d : focus)->yaw;
                    camera1->pitch = (d != camera1 ? d : focus)->pitch;
                }
            }
            if(player1->state >= CS_SPECTATOR && focus != player1) camera1->resetinterp();
        }
        calcangles(camera1, focus);
        if(thirdpersoncursor != 1 && focus == player1 && thirdpersonview(true, focus))
        {
            float yaw = camera1->yaw, pitch = camera1->pitch;
            vectoyawpitch(cursordir, yaw, pitch);
            findorientation(camera1->o, yaw, pitch, worldpos);
        }
        else findorientation(focus->o, focus->yaw, focus->pitch, worldpos);
        adjustorientation(worldpos);
        camera1->inmaterial = lookupmaterial(camera1->o);
        camera1->inliquid = isliquid(camera1->inmaterial&MATF_VOLUME);
        lastcamera = totalmillis;
    }

    VAR(0, animoverride, -1, 0, ANIM_MAX-1);
    VAR(0, animtargets, 0, 1, 1);
    VAR(0, testanims, 0, 0, 1);

    int numanims() { return ANIM_MAX; }

    void findanims(const char *pattern, vector<int> &anims)
    {
        loopi(sizeof(animnames)/sizeof(animnames[0]))
            if(*animnames[i] && cubepattern(animnames[i], pattern) >= 0)
                anims.add(i);
    }

    void renderabovehead(gameent *d)
    {
        vec pos = d->abovehead(d->state != CS_DEAD && d->state != CS_WAITING ? 1 : 0);
        float blend = aboveheadblend*opacity(d, true);
        if(aboveheadnames && d != player1)
        {
            pos.z += aboveheadnamessize/2;
            part_textcopy(pos, colourname(d), PART_TEXT, 1, colourwhite, aboveheadnamessize, blend*aboveheadnamesblend);
        }
        if(aboveheadinventory && d != player1)
        {
            stringz(weapons);
            #define printweapon(q) \
                if(d->hasweap(q, m_weapon(d->actortype, gamemode, mutators))) \
                { \
                    vec colour = vec::fromcolor(W(q, colour)); \
                    if(q != d->weapselect) colour.mul(aboveheadinventoryfade); \
                    defformatstring(str, "\fs\f[%d]%s\f(%s)\fS", colour.tohexcolor(), q != d->weapselect ? "\fE" : "", hud::itemtex(WEAPON, q)); \
                    concatstring(weapons, str); \
                }
            if(aboveheadinventory >= 2) { loopi(W_ALL) printweapon(i); }
            else { printweapon(d->weapselect); }
            pos.z += aboveheadinventorysize/2;
            part_textcopy(pos, weapons, PART_TEXT, 1, colourwhite, aboveheadinventorysize, blend*aboveheadinventoryblend);
        }
        if(aboveheadstatus)
        {
            Texture *t = NULL;
            int colour = getcolour(d, playerteamtone, playerteamtonelevel);
            if(d->state == CS_DEAD || d->state == CS_WAITING) t = textureload(hud::deadtex, 3);
            else if(d->state == CS_ALIVE)
            {
                if(d->conopen) t = textureload(hud::chattex, 3);
                else if(m_team(gamemode, mutators) && (hud::numteamkills() >= teamkillwarn || aboveheadteam&(d->team != focus->team ? 2 : 1)))
                    t = textureload(hud::teamtexname(d->team), 3);
                if(!m_team(gamemode, mutators) || d->team != focus->team)
                {
                    if(d->dominating.find(focus) >= 0)
                    {
                        t = textureload(hud::dominatingtex, 3);
                        colour = pulsehexcol(d, PULSE_DISCO);
                    }
                    else if(d->dominated.find(focus) >= 0)
                    {
                        t = textureload(hud::dominatedtex, 3);
                        colour = pulsehexcol(d, PULSE_DISCO);
                    }
                }
            }
            if(t && t != notexture)
            {
                pos.z += aboveheadstatussize;
                part_icon(pos, t, aboveheadstatussize, blend*aboveheadstatusblend, 0, 0, 1, colour);
            }
        }
        if(aboveheadicons && d->state != CS_EDITING && d->state != CS_SPECTATOR) loopv(d->icons)
        {
            if(d->icons[i].type == eventicon::AFFINITY && !(aboveheadicons&2)) continue;
            if(d->icons[i].type == eventicon::WEAPON && !(aboveheadicons&4)) continue;
            int millis = totalmillis-d->icons[i].millis;
            if(millis <= d->icons[i].fade)
            {
                Texture *t = textureload(hud::icontex(d->icons[i].type, d->icons[i].value));
                if(t && t != notexture)
                {
                    int olen = min(d->icons[i].length/5, 1000), ilen = olen/2, colour = colourwhite;
                    float skew = millis < ilen ? millis/float(ilen) : (millis > d->icons[i].fade-olen ? (d->icons[i].fade-millis)/float(olen) : 1.f),
                          size = skew, fade = blend*skew;
                    if(d->icons[i].type >= eventicon::SORTED)
                    {
                        size *= aboveheadiconssize;
                        switch(d->icons[i].type)
                        {
                            case eventicon::WEAPON: colour = W(d->icons[i].value, colour); break;
                            case eventicon::AFFINITY:
                                if(m_bomber(gamemode))
                                {
                                    bvec pcol = bvec::fromcolor(bomber::pulsecolour());
                                    colour = (pcol.x<<16)|(pcol.y<<8)|pcol.z;
                                }
                                else colour = TEAM(d->icons[i].value, colour);
                                break;
                            default: break;
                        }
                    }
                    else size *= aboveheadeventsize;
                    pos.z += size;
                    part_icon(pos, t, size, fade*aboveheadiconsblend, 0, 0, 1, colour);
                    //if(d->icons[i].type >= eventicon::SORTED) pos.z += 1.5f;
                }
            }
        }
    }

    void getplayermaterials(gameent *d, modelstate &mdl)
    {
        mdl.material[0] = bvec::fromcolor(getcolour(d, playerovertone, playerovertonelevel));
        mdl.material[1] = bvec::fromcolor(getcolour(d, playerundertone, playerundertonelevel));
        #define TONEINTERP(name, mat, var) \
            mdl.matbright[var] = player##name##tonebright; \
            if(player##name##toneinterp > 0) \
            { \
                float intensity = 1.f-((mdl.material[mat].r+mdl.material[mat].g+mdl.material[mat].b)/765.f); \
                mdl.matbright[var] += intensity*player##name##toneinterp; \
            }
        TONEINTERP(over, 1, 0);
        TONEINTERP(under, 0, 1);
        #undef TONEINTERP
        if(isweap(d->weapselect))
        {
            bool secondary = physics::secondaryweap(d);
            vec color = vec::fromcolor(W(d->weapselect, colour));
            if((d->weapstate[d->weapselect] == W_S_POWER || d->weapstate[d->weapselect] == W_S_ZOOM) && W2(d->weapselect, colourcook, secondary) != 0)
            {
                float amt = clamp(float(lastmillis-d->weaptime[d->weapselect])/max(d->weapwait[d->weapselect], 1), 0.f, 1.f);
                color.mul(1-amt).add(vec(WPCOL(d, d->weapselect, colourcook, secondary)).mul(amt)).clamp(0.f, 1.f);
            }
            else if(d->weapselect >= W_OFFSET && d->weapselect < W_ITEM && (W2(d->weapselect, ammosub, false) || W2(d->weapselect, ammosub, true)))
            {
                int ammo = d->weapammo[d->weapselect][W_A_CLIP], maxammo = max(W(d->weapselect, ammoclip), 1);
                float scale = 1;
                switch(d->weapstate[d->weapselect])
                {
                    case W_S_RELOAD:
                    {
                        int millis = lastmillis-d->weaptime[d->weapselect], check = d->weapwait[d->weapselect]/2;
                        scale = millis >= check ? float(millis-check)/check : 0.f;
                        if(d->weapload[d->weapselect][W_A_CLIP] > 0)
                            scale = max(scale, float(ammo - d->weapload[d->weapselect][W_A_CLIP])/maxammo);
                        break;
                    }
                    default: scale = float(ammo)/maxammo; break;
                }
                if(scale < 1) color.mul(scale);
            }
            if(W(d->weapselect, lightpersist)&2) color.max(WPCOL(d, d->weapselect, lightcol, physics::secondaryweap(d)));
            mdl.material[2] = bvec::fromcolor(color);
        }
    }

    const char *getplayerstate(gameent *d, modelstate &mdl, int third, float size, int flags, modelattach *mdlattach, int *lastoffset)
    {
        int weap = d->weapselect, ai = 0,
            mdltype = forceplayermodel >= 0 ? forceplayermodel : d->model%PLAYERTYPES,
            mdlidx = third == 1 && d->headless && !nogore && headlessmodels ? 3 : third;
        const char *mdlname = playertypes[mdltype][mdlidx];
        if(d->actortype > A_PLAYER && d->actortype < A_MAX && actors[d->actortype].mdl && *actors[d->actortype].mdl)
            mdlname = actors[d->actortype].mdl;
        bool hasweapon = false, secondary = false,
             onfloor = d->physstate >= PHYS_SLOPE || d->onladder || physics::liquidcheck(d);

        mdl.anim = ANIM_IDLE|ANIM_LOOP;
        mdl.flags = flags;
        mdl.basetime = mdl.basetime2 = 0;
        mdl.size = size;
        mdl.yaw = d->yaw;
        mdl.pitch = d->pitch;
        mdl.roll = calcroll(d);
        mdl.o = third ? d->feetpos() : camerapos(d);

        if(d->state == CS_DEAD || d->state == CS_WAITING)
        {
            mdl.anim = ANIM_DYING|ANIM_NOPITCH;
            mdl.basetime = d->lastpain;
            switch(deathanim)
            {
                case 0:
                    if(d->ragdoll) cleanragdoll(d);
                    return mdlname;
                case 1:
                {
                    if(d->ragdoll) cleanragdoll(d);
                    int t = lastmillis-mdl.basetime;
                    if(t < 0) return mdlname;
                    if(t > 1000) mdl.anim = ANIM_DEAD|ANIM_LOOP|ANIM_NOPITCH;
                    break;
                }
                case 3: if(m_duke(gamemode, mutators))
                {
                    if(d->ragdoll) cleanragdoll(d);
                    return mdlname;
                }
                case 2: mdl.anim |= ANIM_RAGDOLL; break;
            }
        }
        else if(d->state == CS_EDITING) mdl.anim = ANIM_EDIT|ANIM_LOOP;
        else
        {
            float weapscale = 1.f;
            bool showweap = third != 2 && isweap(weap) && weap < W_ALL;
            secondary = third != 0;
            if(showweap)
            {
                mdl.basetime = d->weaptime[weap];
                switch(d->weapstate[weap])
                {
                    case W_S_SWITCH: case W_S_USE:
                    {
                        int millis = lastmillis-d->weaptime[weap], off = min(d->weapwait[weap]/4, 250),
                            lastweap = d->getlastweap(m_weapon(d->actortype, gamemode, mutators));
                        if(!isweap(lastweap) || lastweap != weap)
                        {
                            if(isweap(lastweap) && millis <= off)
                            {
                                weap = lastweap;
                                weapscale = 1.f-(millis/float(off));
                            }
                            else if(!d->hasweap(weap, m_weapon(d->actortype, gamemode, mutators))) showweap = false;
                            else if(millis <= off*2) weapscale = (millis-off)/float(off);
                        }
                        mdl.anim = d->weapstate[weap] == W_S_SWITCH ? ANIM_SWITCH : ANIM_USE;
                        break;
                    }
                    case W_S_POWER: case W_S_ZOOM:
                    {
                        mdl.anim = (weaptype[weap].anim+d->weapstate[weap])|ANIM_CLAMP;
                        break;
                    }
                    case W_S_PRIMARY: case W_S_SECONDARY:
                    {
                        if(weaptype[weap].thrown)
                        {
                            int millis = lastmillis-d->weaptime[weap], off = d->weapwait[weap]/4;
                            if(millis <= off || !d->hasweap(weap, m_weapon(d->actortype, gamemode, mutators)))
                                showweap = false;
                            else if(millis <= off*2) weapscale = (millis-off)/float(off);
                        }
                        mdl.anim = (weaptype[weap].anim+d->weapstate[weap])|ANIM_CLAMP;
                        break;
                    }
                    case W_S_RELOAD:
                    {
                        if(!d->hasweap(weap, m_weapon(d->actortype, gamemode, mutators))) showweap = false;
                        mdl.anim = weaptype[weap].anim+d->weapstate[weap];
                        break;
                    }
                    case W_S_IDLE: case W_S_WAIT: default:
                    {
                        if(!d->hasweap(weap, m_weapon(d->actortype, gamemode, mutators))) showweap = false;
                        mdl.anim = weaptype[weap].anim|ANIM_LOOP;
                        break;
                    }
                }
            }
            if(third && (mdl.anim&ANIM_IDLE) && lastmillis-d->lastpain <= 300)
            {
                secondary = true;
                mdl.basetime = d->lastpain;
                mdl.anim = ANIM_PAIN;
            }
            if(mdlattach && showweap)
            {
                const char *weapmdl = third ? weaptype[weap].vwep : weaptype[weap].hwep;
                if(weapmdl && *weapmdl)
                {
                    mdlattach[ai++] = modelattach("tag_weapon", weapmdl, mdl.anim, mdl.basetime, weapscale); // 0
                    hasweapon = true;
                }
            }
        }
        if(mdlattach)
        {
            if(!(mdl.flags&MDL_ONLYSHADOW) && actors[d->actortype].hastags)
            {
                if(third != 2)
                {
                    mdlattach[ai++] = modelattach(hasweapon ? "tag_muzzle" : "tag_weapon", &d->tag[TAG_MUZZLE]); // 1
                    mdlattach[ai++] = modelattach("tag_weapon", &d->tag[TAG_ORIGIN]); // 2
                    if(weaptype[weap].eject || weaptype[weap].tape)
                    {
                        mdlattach[ai++] = modelattach("tag_eject", &d->tag[TAG_EJECT1]); // 3
                        mdlattach[ai++] = modelattach("tag_eject2", &d->tag[TAG_EJECT2]); // 4
                    }
                }
                if(third)
                {
                    mdlattach[ai++] = modelattach("tag_head", &d->tag[TAG_HEAD]); // 5
                    mdlattach[ai++] = modelattach("tag_torso", &d->tag[TAG_TORSO]); // 6
                    mdlattach[ai++] = modelattach("tag_waist", &d->tag[TAG_WAIST]); // 7
                    mdlattach[ai++] = modelattach("tag_ljet", &d->tag[TAG_JET_LEFT]); // 8
                    mdlattach[ai++] = modelattach("tag_rjet", &d->tag[TAG_JET_RIGHT]); // 9
                    mdlattach[ai++] = modelattach("tag_bjet", &d->tag[TAG_JET_BACK]); // 10
                    mdlattach[ai++] = modelattach("tag_ltoe", &d->tag[TAG_TOE_LEFT]); // 11
                    mdlattach[ai++] = modelattach("tag_rtoe", &d->tag[TAG_TOE_RIGHT]); // 12
                }
            }
            if(third && vanitycheck(d))
            {
                if(d->vitems.empty()) vanitybuild(d);
                int idx = third == 1 && (d->state == CS_DEAD || d->state == CS_WAITING) && d->headless && !nogore && headlessmodels ? 3 : third,
                    count = 0, found[VANITYMAX] = {0};
                loopvk(d->vitems) if(vanities.inrange(d->vitems[k]))
                {
                    if(found[vanities[d->vitems[k]].type]) continue;
                    if(vanities[d->vitems[k]].cond&1 && idx == 2) continue;
                    if(vanities[d->vitems[k]].cond&2 && idx == 3) continue;
                    const char *file = vanityfname(d, d->vitems[k]);
                    if(file)
                    {
                        mdlattach[ai++] = modelattach(vanities[d->vitems[k]].tag, file);
                        found[vanities[d->vitems[k]].type]++;
                        if(++count >= VANITYMAX) break;
                    }
                }
            }
        }
        switch(third)
        {
            case 0:
            {
                if(gs_playing(gamestate) && firstpersonsway)
                {
                    float steps = swaydist/(firstpersonbob ? firstpersonbobstep : firstpersonswaystep)*M_PI;
                    vec dir = vec(mdl.yaw*RAD, 0.f).mul(firstpersonswayside*sinf(steps) * 2.0f);
                    mdl.o.add(dir).add(swaydir).add(swaypush);
                    mdl.yaw += firstpersonswayside*cosf(steps) * 24.0f;
                    mdl.pitch += firstpersonswayside*sinf(steps*2.0f) * 8.0f;
                }
                if(d->sliding(true) && firstpersonslidetime && firstpersonslideroll != 0)
                {
                    int dur = min(impulseslidelen/2, firstpersonslidetime), millis = lastmillis-d->impulsetime[IM_T_SLIDE];
                    float amt = 1;
                    if(millis > dur)
                    {
                        int off = impulseslidelen-millis;
                        if(off < dur) amt = off/float(dur);
                    }
                    else amt = millis/float(dur);
                    mdl.roll = firstpersonslideroll*amt;
                }
                break;
            }
            case 1:
            {
                if(d == focus && d == player1 && thirdpersonview(true, d))
                    vectoyawpitch(vec(worldpos).sub(d->headpos()).normalize(), mdl.yaw, mdl.pitch);
                break;
            }
            case 2:
            {
                mdl.o.sub(vec(mdl.yaw*RAD, 0.f).mul(firstpersonbodydist+firstpersonspineoffset));
                mdl.o.sub(vec(mdl.yaw*RAD, 0.f).rotate_around_z(90*RAD).mul(firstpersonbodyside));
                if(lastoffset)
                {
                    float zoffset = (max(d->zradius-d->height, 0.f)+(d->radius*0.5f))*firstpersonbodyzoffset;
                    if(!onfloor && (d->action[AC_SPECIAL] || d->impulse[IM_TYPE] == IM_T_POUND || d->sliding(true) || d->impulse[IM_TYPE] == IM_T_KICK || d->impulse[IM_TYPE] == IM_T_GRAB))
                    {
                        int lmillis = d->airtime(lastmillis);
                        if(lmillis < 100) zoffset *= lmillis/100.f;
                        mdl.o.z -= zoffset;
                        *lastoffset = lastmillis;
                    }
                    else if(*lastoffset)
                    {
                        int lmillis = lastmillis-(*lastoffset);
                        if(lmillis < 100) mdl.o.z -= zoffset*((100-lmillis)/100.f);
                    }
                }
                if(firstpersonbodypitchadjust > 0 && mdl.pitch < 0) mdl.o.sub(vec(mdl.yaw*RAD, 0.f).mul(d->radius*(0-mdl.pitch)/90.f*firstpersonbodypitchadjust));
                mdl.pitch = firstpersonbodypitch >= 0 ? mdl.pitch*firstpersonbodypitch : mdl.pitch;
                mdl.roll = 0.f;
                break;
            }
        }
        if(animoverride && (m_edit(gamemode) ? (animtargets >= (d == focus ? 0 : 1)) : d == focus))
        {
            mdl.anim = (animoverride < 0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
            mdl.basetime = 0;
        }
        else
        {
            if(secondary && allowmove(d) && AA(d->actortype, abilities)&(1<<A_A_MOVE))
            {
                if(physics::liquidcheck(d) && d->physstate <= PHYS_FALL)
                    mdl.anim |= ((d->move || d->strafe || d->vel.z+d->falling.z > 0 ? int(ANIM_SWIM) : int(ANIM_SINK))|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->impulse[IM_TYPE] == IM_T_PARKOUR)
                    mdl.anim |= ((d->turnside > 0 ? ANIM_PARKOUR_LEFT : (d->turnside < 0 ? ANIM_PARKOUR_RIGHT : ANIM_PARKOUR_UP))|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->physstate == PHYS_FALL && !d->onladder && d->impulsetime[d->impulse[IM_TYPE]] && lastmillis-d->impulsetime[d->impulse[IM_TYPE]] <= 1000)
                {
                    mdl.basetime2 = d->impulsetime[d->impulse[IM_TYPE]];
                    if(d->impulse[IM_TYPE] == IM_T_KICK || d->impulse[IM_TYPE] == IM_T_GRAB) mdl.anim |= ANIM_PARKOUR_JUMP<<ANIM_SECONDARY;
                    else if(d->action[AC_SPECIAL] || d->impulse[IM_TYPE] == IM_T_POUND)
                    {
                        mdl.anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                        mdl.basetime2 = d->weaptime[W_MELEE];
                    }
                    else if(d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_BOOST_LEFT : ANIM_BOOST_RIGHT)<<ANIM_SECONDARY;
                    else if(d->move > 0) mdl.anim |= ANIM_BOOST_FORWARD<<ANIM_SECONDARY;
                    else if(d->move < 0) mdl.anim |= ANIM_BOOST_BACKWARD<<ANIM_SECONDARY;
                    else mdl.anim |= ANIM_BOOST_UP<<ANIM_SECONDARY;
                }
                else if(d->physstate == PHYS_FALL && !d->onladder && d->airtime(lastmillis) >= 50)
                {
                    mdl.basetime2 = max(d->airmillis, d->impulsetime[IM_T_JUMP]);
                    if(d->action[AC_SPECIAL] || d->impulse[IM_TYPE] == IM_T_POUND)
                    {
                        mdl.anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                        mdl.basetime2 = d->weaptime[W_MELEE];
                    }
                    else if(d->crouching(true))
                    {
                        if(d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_CROUCH_JUMP_LEFT : ANIM_CROUCH_JUMP_RIGHT)<<ANIM_SECONDARY;
                        else if(d->move > 0) mdl.anim |= ANIM_CROUCH_JUMP_FORWARD<<ANIM_SECONDARY;
                        else if(d->move < 0) mdl.anim |= ANIM_CROUCH_JUMP_BACKWARD<<ANIM_SECONDARY;
                        else mdl.anim |= ANIM_CROUCH_JUMP<<ANIM_SECONDARY;
                    }
                    else if(d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_JUMP_LEFT : ANIM_JUMP_RIGHT)<<ANIM_SECONDARY;
                    else if(d->move > 0) mdl.anim |= ANIM_JUMP_FORWARD<<ANIM_SECONDARY;
                    else if(d->move < 0) mdl.anim |= ANIM_JUMP_BACKWARD<<ANIM_SECONDARY;
                    else mdl.anim |= ANIM_JUMP<<ANIM_SECONDARY;
                    if(!mdl.basetime2) mdl.anim |= ANIM_END<<ANIM_SECONDARY;
                }
                else if(d->sliding(true)) mdl.anim |= (ANIM_POWERSLIDE|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->crouching(true))
                {
                    if(d->strafe) mdl.anim |= ((d->strafe > 0 ? ANIM_CRAWL_LEFT : ANIM_CRAWL_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move > 0) mdl.anim |= (ANIM_CRAWL_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move < 0) mdl.anim |= (ANIM_CRAWL_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else mdl.anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->running(moveslow))
                {
                    if(d->strafe) mdl.anim |= ((d->strafe > 0 ? ANIM_RUN_LEFT : ANIM_RUN_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move > 0) mdl.anim |= (ANIM_RUN_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move < 0) mdl.anim |= (ANIM_RUN_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->strafe) mdl.anim |= ((d->strafe > 0 ? ANIM_WALK_LEFT : ANIM_WALK_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->move > 0) mdl.anim |= (ANIM_WALK_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->move < 0) mdl.anim |= (ANIM_WALK_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
            }

            if((mdl.anim>>ANIM_SECONDARY)&ANIM_INDEX) switch(mdl.anim&ANIM_INDEX)
            {
                case ANIM_IDLE: case ANIM_CLAW: case ANIM_PISTOL: case ANIM_SWORD:
                case ANIM_SHOTGUN: case ANIM_SMG: case ANIM_FLAMER: case ANIM_PLASMA: case ANIM_ZAPPER:
                case ANIM_RIFLE: case ANIM_GRENADE: case ANIM_MINE: case ANIM_ROCKET:
                {
                    mdl.anim = (mdl.anim>>ANIM_SECONDARY) | ((mdl.anim&((1<<ANIM_SECONDARY)-1))<<ANIM_SECONDARY);
                    swap(mdl.basetime, mdl.basetime2);
                    break;
                }
                default: break;
            }
        }
        if(third == 1 && testanims && d == focus) mdl.yaw = 0;
        if(!((mdl.anim>>ANIM_SECONDARY)&ANIM_INDEX)) mdl.anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;
        if(mdlattach && mdlattach[0].tag) mdl.attached = mdlattach;
        return mdlname;
    }

    #define RESIDUAL(name, type, pulse) \
        void get##name##effect(physent *d, modelstate &mdl, int length, int millis, int delay) \
        { \
            int offset = length-millis; \
            float pc = offset >= delay ? 1.f : float(offset)/float(delay); \
            vec4 mixercolor = vec4(vec(pulsecolour(d, PULSE_##pulse)).mul(mixer##name##intensity), pc*mixer##name##blend); \
            vec2 mixerglow = vec2((mdl.mixercolor.r+mdl.mixercolor.g+mdl.mixercolor.b)/3.f*mixer##name##glowintensity, pc*mixer##name##glowblend); \
            if(mdl.mixer && mdl.mixer != notexture) \
            { \
                mdl.mixercolor.add(mixercolor).mul(0.5f); \
                mdl.mixerglow.add(mixerglow).mul(0.5f); \
            } \
            else \
            { \
                mdl.mixer = textureload(mixer##name##tex, 0, true); \
                mdl.mixercolor = mixercolor; \
                mdl.mixerglow = mixerglow; \
            } \
            mdl.mixerscroll = vec2(mixer##name##scroll1, mixer##name##scroll2); \
        }
    RESIDUALSF
    #undef RESIDUAL

    void getplayereffects(gameent *d, modelstate &mdl)
    {
        #define RESIDUAL(name, type, pulse) \
            if(d->name##time && d->name##ing(lastmillis, d->name##time)) \
                get##name##effect(d, mdl, d->name##time, lastmillis-d->lastres[W_R_##type], max(d->name##delay, 1));
        RESIDUALS
        #undef RESIDUAL
        if((!mdl.mixer || mdl.mixer == notexture) && d->state == CS_ALIVE && d->lastbuff)
        {
            float pc = 1;
            if(mixerbuffpulse > 0)
            {
                int millis = lastmillis%mixerbuffpulse, part = max(mixerbuffpulse/2, 1);
                pc *= clamp(millis <= part ? 1.f-(millis/float(part)) : (millis-part)/float(part), min(mixerbuffpulsemin, mixerbuffpulsemax), max(mixerbuffpulsemax, mixerbuffpulsemin));
            }
            vec4 mixercolor = vec4(vec(mixerbufftone >= 0 ? vec::fromcolor(getcolour(d, mixerbufftone)) : pulsecolour(d, PULSE_BUFF)).mul(mixerbuffintensity), pc*mixerbuffblend);
            vec2 mixerglow = vec2((mdl.mixercolor.r+mdl.mixercolor.g+mdl.mixercolor.b)/3.f*mixerbuffglowintensity, pc*mixerbuffglowblend);
            mdl.mixer = textureload(mixerbufftex, 0, true);
            mdl.mixercolor = mixercolor;
            mdl.mixerglow = mixerglow;
            mdl.mixerscroll = vec2(mixerbuffscroll1, mixerbuffscroll2);
        }
        int pattern = forceplayerpattern >= 0 ? forceplayerpattern : d->pattern;
        if(pattern >= 0)
        {
            const playerpattern &p = playerpatterns[pattern%PLAYERPATTERNS];
            mdl.pattern = textureload(p.filename, p.clamp, true);
            mdl.patternscale = p.scale;
            if(pattern < 2) mdl.flags |= MDL_NOPATTERN; // first two shouldn't recurse
        }
    }

    void renderplayer(gameent *d, int third, float size, int flags = 0, const vec4 &color = vec4(1, 1, 1, 1), int *lastoffset = NULL)
    {
        if(d->state == CS_SPECTATOR || (d->state != CS_ALIVE && color.a <= 0)) return;
        modelstate mdl;
        modelattach mdlattach[ATTACHMENTMAX];
        dynent *e = third ? (third != 2 ? (dynent *)d : (dynent *)&bodymodel) : (dynent *)&avatarmodel;
        const char *mdlname = getplayerstate(d, mdl, third, size, flags, mdlattach, lastoffset);
        mdl.color = color;
        getplayermaterials(d, mdl);
        getplayereffects(d, mdl);
        if(!drawtex)
        {
            if(d != focus && !d->ai)
            {
                if(!(mdl.anim&ANIM_RAGDOLL)) mdl.flags |= MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY;
                if(d->actortype >= A_ENEMY) mdl.flags |= MDL_CULL_DIST;
            }
            if(d != focus || (d != player1 ? fullbrightfocus&1 : fullbrightfocus&2)) mdl.flags |= MDL_FULLBRIGHT;
        }
        rendermodel(mdlname, mdl, e);
        if((d != focus || d->state == CS_DEAD || d->state == CS_WAITING) && !(mdl.flags&MDL_ONLYSHADOW) && third == 1 && d->actortype < A_ENEMY && !shadowmapping && !drawtex && (aboveheaddead || d->state == CS_ALIVE))
            renderabovehead(d);
    }

    void rendercheck(gameent *d, bool third)
    {
        float blend = opacity(d, third);
        if(d->state == CS_ALIVE)
        {
            bool useth = hud::teamhurthud&1 && hud::teamhurttime && m_team(gamemode, mutators) && focus == player1 &&
                 d->team == player1->team && d->lastteamhit >= 0 && totalmillis-d->lastteamhit <= hud::teamhurttime,
            hashint = playerhint&(d->team != focus->team ? 2 : 1), haslight = false, haspower = false, hasdom = false;
            if(isweap(d->weapselect) && playerhint&4)
            {
                if(W(d->weapselect, lightpersist)&4) haslight = true;
                if(W(d->weapselect, lightpersist)&8 && lastmillis-d->weaptime[d->weapselect] > 0 && d->weapstate[d->weapselect] == W_S_POWER)
                    haspower = true;
            }
            if((!m_team(gamemode, mutators) || d->team != focus->team) && playerhint&8 && d->dominated.find(focus) >= 0) hasdom = true;
            if(d->actortype < A_ENEMY && d != focus && (useth || hashint || haslight || haspower || hasdom))
            {
                if(hashint || haslight || haspower || hasdom)
                {
                    vec c = vec::fromcolor(hasdom ? pulsehexcol(d, PULSE_DISCO) : (haslight ? WHCOL(d, d->weapselect, lightcol, physics::secondaryweap(d)) : getcolour(d, playerhinttone, playerhinttonelevel)));
                    float height = d->height, fade = blend;
                    if(hasdom) fade *= playerhintdom;
                    else if(haslight || haspower)
                    {
                        int millis = lastmillis-d->weaptime[d->weapselect];
                        if(haslight)
                        {
                            if(d->weapstate[d->weapselect] == W_S_SWITCH || d->weapstate[d->weapselect] == W_S_USE)
                                fade *= millis/float(max(d->weapwait[d->weapselect], 1));
                        }
                        else fade *= millis/float(max(d->weapwait[d->weapselect], 1));
                        fade *= playerhintlight;
                    }
                    else if(playerhintscale > 0)
                    {
                        float per = d->health/float(max(d->gethealth(gamemode, mutators), 1));
                        fade = (fade*(1.f-playerhintscale))+(fade*per*playerhintscale);
                        if(fade > 1)
                        {
                            height *= 1.f+(fade-1.f);
                            fade = 1;
                        }
                        fade *= playerhintblend;
                    }
                    if(d->state == CS_ALIVE && d->lastbuff)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? 1.f-(millis/500.f) : (millis-500)/500.f;
                        vec pc = game::pulsecolour(d, PULSE_BUFF);
                        flashcolour(c.r, c.g, c.b, pc.r, pc.g, pc.b, amt);
                        height += height*amt*0.1f;
                    }
                    vec o = d->center(), offset = vec(o).sub(camera1->o).rescale(d->radius/2);
                    offset.z = max(offset.z, -1.0f);
                    offset.add(o);
                    part_create(PART_HINT_VERT_SOFT, 1, offset, c.tohexcolor(), clamp(height*playerhintsize, 1.f, playerhintmaxsize), fade*camera1->o.distrange(o, playerhintfadeat, playerhintfadecut));
                }
                if(useth)
                {
                    int millis = lastmillis%500;
                    float amt = millis <= 250 ? 1.f-(millis/250.f) : (millis-250)/250.f, height = d->height*0.5f;
                    if(playerhinthurtthrob) height += height*amt*0.1f;
                    vec c = pulsecolour(d, PULSE_WARN),
                        o = d->center(), offset = vec(o).sub(camera1->o).rescale(-d->radius);
                    offset.z = max(offset.z, -1.0f);
                    offset.add(o);
                    part_icon(offset, textureload(hud::warningtex, 3, true, false), height*playerhinthurtsize, amt*blend*playerhinthurtblend, 0, 0, 1, c.tohexcolor());
                }
            }
            if(actors[d->actortype].weapfx)
            {
                if(d->hasmelee(lastmillis))
                {
                    float amt = 1-((lastmillis-d->weaptime[W_MELEE])/float(d->weapwait[W_MELEE]));
                    loopi(2) part_create(PART_HINT_SOFT, 1, d->toetag(i), TEAM(d->team, colour), 2.f, amt*blend, 0, 0);
                }
                int millis = lastmillis-d->weaptime[d->weapselect];
                bool last = millis > 0 && millis < d->weapwait[d->weapselect],
                     powering = last && d->weapstate[d->weapselect] == W_S_POWER,
                     reloading = last && d->weapstate[d->weapselect] == W_S_RELOAD,
                     secondary = physics::secondaryweap(d);
                float amt = last ? (lastmillis-d->weaptime[d->weapselect])/float(d->weapwait[d->weapselect]) : 1.f;
                int colour = WHCOL(d, d->weapselect, partcol, secondary);
                if(d->weapselect == W_FLAMER && (!reloading || amt > 0.5f) && !physics::liquidcheck(d))
                {
                    float scale = powering ? 1.f+(amt*1.5f) : (d->weapstate[d->weapselect] == W_S_IDLE ? 1.f : (reloading ? (amt-0.5f)*2 : amt));
                    part_create(PART_HINT_SOFT, 1, d->ejecttag(d->weapselect), 0x1818A8, 0.75f*scale, min(0.65f*scale, 0.8f)*blend, 0, 0);
                    part_create(PART_FIREBALL_SOFT, 1, d->ejecttag(d->weapselect), colour, 0.5f*scale, min(0.75f*scale, 0.95f)*blend, 0, 0);
                    regular_part_create(PART_FIREBALL, d->vel.magnitude() > 10 ? 40 : 80, d->ejecttag(d->weapselect), colour, 0.5f*scale, min(0.75f*scale, 0.95f)*blend, d->vel.magnitude() > 10 ? -40 : -10, 0);
                }
                if(W(d->weapselect, laser) && !reloading)
                {
                    vec v, muzzle = d->muzzletag(d->weapselect);
                    muzzle.z += 0.25f;
                    findorientation(d->o, d->yaw, d->pitch, v);
                    part_flare(muzzle, v, 1, PART_FLARE, colour, 0.5f*amt, amt*blend);
                }
                if(d->weapselect == W_SWORD || d->weapselect == W_ZAPPER || powering)
                {
                    static const struct powerfxs {
                        int type, parttype;
                        float size, radius;
                    } powerfx[W_MAX] = {
                        { 4, PART_LIGHTNING, 1.f, 1 },
                        { 2, PART_SPARK, 0.1f, 1.5f },
                        { 4, PART_LIGHTNING, 2.f, 1 },
                        { 2, PART_SPARK, 0.15f, 2 },
                        { 2, PART_SPARK, 0.1f, 2 },
                        { 2, PART_FIREBALL, 0.1f, 6 },
                        { 1, PART_PLASMA, 0.05f, 2 },
                        { 4, PART_LIGHTZAP, 0.75f, 1 },
                        { 2, PART_PLASMA, 0.05f, 2.5f },
                        { 3, PART_PLASMA, 0.1f, 0.125f },
                        { 0, 0, 0, 0 },
                        { 0, 0, 0, 0 },
                        { 0, 0, 0, 0 },
                    };
                    switch(powerfx[d->weapselect].type)
                    {
                        case 1: case 2:
                        {
                            regularshape(powerfx[d->weapselect].parttype, 1+(amt*powerfx[d->weapselect].radius), colour, powerfx[d->weapselect].type == 2 ? 21 : 53, 5, 60+int(30*amt), d->muzzletag(d->weapselect), powerfx[d->weapselect].size*max(amt, 0.25f), max(amt, 0.1f)*blend, 1, 0, 5+(amt*5));
                            break;
                        }
                        case 3:
                        {
                            int interval = lastmillis%1000;
                            float fluc = powerfx[d->weapselect].size+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                            part_create(powerfx[d->weapselect].parttype, 1, d->muzzletag(d->weapselect), colour, (powerfx[d->weapselect].radius*max(amt, 0.25f))+fluc, max(amt, 0.1f)*blend);
                            break;
                        }
                        case 4:
                        {
                            part_flare(d->ejecttag(d->weapselect), d->ejecttag(d->weapselect, 1), 1, powerfx[d->weapselect].parttype, colour, powerfx[d->weapselect].size*max(amt, 0.25f), max(amt, 0.1f)*blend);
                            break;
                        }
                        case 0: default: break;
                    }
                }
            }
            if(d->impulse[IM_TYPE] == IM_T_PARKOUR || d->impulsetime[IM_T_JUMP] || d->sliding(true)) impulseeffect(d, 1);
        }
        if(d->burntime && d->burning(lastmillis, d->burntime))
        {
            int millis = lastmillis-d->lastres[W_R_BURN], delay = max(d->burndelay, 1);
            float pc = 1, intensity = 0.5f+(rnd(50)/100.f), fade = (d != focus ? 0.75f : 0.f)+(rnd(25)/100.f);
            if(d->burntime-millis < delay) pc *= float(d->burntime-millis)/float(delay);
            else pc *= 0.75f+(float(millis%delay)/float(delay*4));
            vec pos = vec(d->center()).sub(vec(rnd(11)-5, rnd(11)-5, rnd(5)-2).mul(pc));
            regular_part_create(PART_FIREBALL, 200, pos, pulsehexcol(d, PULSE_FIRE), d->height*0.75f*intensity*blend*pc, fade*blend*pc*0.65f, -20, 0);
        }
        if(d->shocktime && d->shocking(lastmillis, d->shocktime))
        {
            float radius = d->getradius(), height = d->getheight();
            if(d->ragdoll)
            {
                radius *= 0.5f;
                height *= 1.35f;
            }
            vec origin = d->center(), col = pulsecolour(d, PULSE_SHOCK), rad = vec(radius, radius, height*0.5f).mul(blend);
            int colour = (int(col.x*255)<<16)|(int(col.y*255)<<8)|(int(col.z*255));
            float fade = blend*(d != focus || d->state != CS_ALIVE ? 1.f : 0.65f);
            loopi(4+rnd(8))
            {
                float q = 1.f;
                vec from = vec(origin).add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).div(100.f).normalize().mul(rad).mul(rnd(200)/100.f)), to = from;
                loopj(3+rnd(3))
                {
                    to = vec(from).add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).div(100.f).normalize().mul(rad).mul(rnd(200)/100.f*q*0.5f));
                    part_flare(from, to, 1, PART_LIGHTNING_FLARE, colour, q*0.5f, fade*q);
                    from = to;
                    q *= 0.8f;
                }
            }
            if(d->ragdoll && twitchspeed > 0) twitchragdoll(d, twitchspeed*blend*rnd(100)/80.f);
        }
    }

    void render()
    {
        entities::render();
        if(drawtex) return;
        ai::render();
        projs::render();
        if(m_capture(gamemode)) capture::render();
        else if(m_defend(gamemode)) defend::render();
        else if(m_bomber(gamemode)) bomber::render();

        gameent *d;
        int numdyns = numdynents();
        bool third = thirdpersonview();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) != NULL)
        {
            if(d != focus || third) d->cleartags();
            renderplayer(d, 1, d->curscale, d == focus ? (third ? MDL_FORCESHADOW : MDL_ONLYSHADOW) : 0, vec4(1, 1, 1, opacity(d, true)));
        }
    }

    void renderpost()
    {
        gameent *d;
        int numdyns = numdynents();
        bool third = thirdpersonview();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) != NULL && (d != focus || third))
            rendercheck(d, true);
    }

    void renderavatar()
    {
        if(thirdpersonview()) return;
        focus->cleartags();
        if(!firstpersonmodel) return;
        float depthfov = firstpersondepthfov != 0 ? firstpersondepthfov : curfov;
        if(inzoom())
        {
            int frame = lastmillis-lastzoom;
            float pc = frame <= W(focus->weapselect, cookzoom) ? frame/float(W(focus->weapselect, cookzoom)) : 1.f;
            depthfov *= zooming ? 1.f-pc : pc;
        }
        setavatarscale(depthfov, firstpersondepth);
        vec4 color = vec4(1, 1, 1, opacity(focus, false));
        if(focus->state == CS_ALIVE && firstpersonmodel&1) renderplayer(focus, 0, focus->curscale, MDL_NOBATCH, color);
        if(focus->state == CS_ALIVE && firstpersonmodel&2)
        {
            setavatarscale(firstpersonbodydepthfov != 0 ? firstpersonbodydepthfov : curfov, firstpersonbodydepth);
            static int lastoffset = 0;
            renderplayer(focus, 2, focus->curscale, MDL_NOBATCH, color, &lastoffset);
        }
        rendercheck(focus, false);
    }

    static gameent *previewent = NULL;
    void initplayerpreview()
    {
        previewent = new gameent;
        previewent->state = CS_ALIVE;
        previewent->actortype = A_PLAYER;
        previewent->physstate = PHYS_FLOOR;
        previewent->spawnstate(G_DEATHMATCH, 0, -1, previewent->gethealth(G_DEATHMATCH, 0));
        loopi(W_MAX) previewent->weapammo[i][W_A_CLIP] = W(i, ammoclip);
        loopi(W_MAX) previewent->weapammo[i][W_A_STORE] = W(i, ammostore) > 0 ? W(i, ammostore) : 0;
    }

    void renderplayerpreview(float scale, const vec4 &mcolor, const char *actions)
    {
        if(!previewent) initplayerpreview();
        previewent->configure(lastmillis, gamemode, mutators);
        float height = previewent->height + previewent->aboveeye, zrad = height/2;
        vec2 xyrad = vec2(previewent->xradius, previewent->yradius).max(height/4);
        previewent->o = calcmodelpreviewpos(vec(xyrad, zrad), previewent->yaw).addz(previewent->height - zrad);
        if(actions && *actions) execute(actions);
        previewent->cleartags();
        renderplayer(previewent, 1, scale, 0, mcolor);
    }

    #define PLAYERPREV(name, arglist, argexpr, body) ICOMMAND(0, playerpreview##name, arglist, argexpr, if(previewent) { body; });
    PLAYERPREV(state, "b", (int *n), previewent->state = *n >= 0 ? clamp(*n, 0, int(CS_MAX-1)) : int(CS_ALIVE));
    PLAYERPREV(move, "i", (int *n), previewent->move = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(strafe, "i", (int *n), previewent->strafe = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(turnside, "i", (int *n), previewent->turnside = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(physstate, "b", (int *n), previewent->physstate = *n >= 0 ? clamp(*n, 0, int(PHYS_MAX-1)) : int(PHYS_FLOOR));
    PLAYERPREV(actortype, "b", (int *n), previewent->actortype = *n >= 0 ? clamp(*n, 0, int(A_MAX-1)) : int(A_PLAYER));
    PLAYERPREV(health, "bbb", (int *n, int *m, int *o), previewent->health = *n >= 0 ? *n : previewent->gethealth(*m >= 0 ? *m : G_DEATHMATCH, *o >= 0 ? *o : 0));
    PLAYERPREV(model, "i", (int *n), previewent->model = clamp(*n, 0, PLAYERTYPES-1));
    PLAYERPREV(colour, "i", (int *n), previewent->colour = clamp(*n, 0, 0xFFFFFF));
    PLAYERPREV(pattern, "i", (int *n), previewent->pattern = clamp(*n, 0, PLAYERPATTERNS-1));
    PLAYERPREV(team, "i", (int *n), previewent->team = clamp(*n, 0, int(T_LAST)));
    PLAYERPREV(privilege, "i", (int *n), previewent->privilege = clamp(*n, 0, int(PRIV_MAX-1)));
    PLAYERPREV(weapselect, "i", (int *n), previewent->weapselect = clamp(*n, 0, W_ALL-1));
    PLAYERPREV(weapammo, "iii", (int *n, int *o, int *p), previewent->weapammo[clamp(*n, 0, W_ALL-1)][clamp(*p, 0, W_A_MAX-1)] = *o);
    PLAYERPREV(weapclip, "ii", (int *n, int *o), previewent->weapammo[clamp(*n, 0, W_ALL-1)][W_A_CLIP] = *o);
    PLAYERPREV(weapstore, "ii", (int *n, int *o), previewent->weapammo[clamp(*n, 0, W_ALL-1)][W_A_STORE] = *o);
    PLAYERPREV(weapstate, "ii", (int *n, int *o), previewent->weapstate[clamp(*n, 0, W_ALL-1)] = clamp(*o, 0, W_S_MAX-1));
    PLAYERPREV(weapwait, "ii", (int *n, int *o), previewent->weapwait[clamp(*n, 0, W_ALL-1)] = *o);
    PLAYERPREV(weaptime, "ii", (int *n, int *o), previewent->weaptime[clamp(*n, 0, W_ALL-1)] = *o);
    PLAYERPREV(lastdeath, "i", (int *n), previewent->lastdeath = *n);
    PLAYERPREV(lastspawn, "i", (int *n), previewent->lastspawn = *n);
    PLAYERPREV(lastbuff, "i", (int *n), previewent->lastbuff = *n);
    PLAYERPREV(lastshoot, "i", (int *n), previewent->lastshoot = *n);
    PLAYERPREV(airmillis, "i", (int *n), previewent->airmillis = *n);
    PLAYERPREV(floormillis, "i", (int *n), previewent->floormillis = *n);
    PLAYERPREV(action, "ii", (int *n, int *o), previewent->action[clamp(*n, 0, int(AC_MAX-1))] = *o != 0);
    PLAYERPREV(actiontime, "ii", (int *n, int *o), previewent->actiontime[clamp(*n, 0, int(AC_MAX-1))] = *o);
    PLAYERPREV(impulse, "ii", (int *n, int *o), previewent->impulse[clamp(*n, 0, int(IM_MAX-1))] = *o);
    PLAYERPREV(impulsetime, "ii", (int *n, int *o), previewent->impulse[clamp(*n, 0, int(IM_T_MAX-1))] = *o);
    PLAYERPREV(headless, "i", (int *n), previewent->headless = *n != 0);
    PLAYERPREV(inliquid, "i", (int *n),
    {
        if((previewent->inliquid = *n != 0) != false)
        {
            previewent->physstate = PHYS_FALL;
            previewent->airmillis = *n;
            previewent->floormillis = 0;
            previewent->submerged = 1;
        }
        else
        {
            previewent->physstate = PHYS_FLOOR;
            previewent->floormillis = *n;
            previewent->airmillis = 0;
            previewent->submerged = 0;
        }
    });
    PLAYERPREV(vanity, "s", (char *n), previewent->setvanity(n));
    void setplayerprevresidual(int n, int q, int r, int s)
    {
        if(n < 0 || n >= W_R_MAX) return;
        previewent->lastres[n] = previewent->lastrestime[n] = q >= 0 ? q : lastmillis;
        #define RESIDUAL(name, type, pulse) \
            case W_R_##type: \
            { \
                previewent->name##time = r >= 0 ? r : lavaburntime; \
                previewent->name##delay = s >= 0 ? s : lavaburndelay; \
                break; \
            }
        switch(n)
        {
            RESIDUALS
            default: break;
        }
        #undef RESIDUAL
    }
    PLAYERPREV(residual, "ibbb", (int *n, int *q, int *r, int *s), setplayerprevresidual(*n, *q, *r, *s));
    #undef PLAYERPREV

    bool clientoption(char *arg) { return false; }
}
#undef CPP_GAME_MAIN
