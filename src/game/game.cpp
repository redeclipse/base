#define GAMEWORLD 1
#include "game.h"
namespace game
{
    int nextmode = G_EDITMODE, nextmuts = 0, gamestate = G_S_WAITING, gamemode = G_EDITMODE, mutators = 0, maptime = 0, timeremaining = 0, lasttimeremain = 0,
        lastcamera = 0, lasttvcam = 0, lasttvchg = 0, lastzoom = 0, spectvfollowing = -1;
    bool prevzoom = false, zooming = false, inputmouse = false, inputview = false, inputmode = false;
    float swayfade = 0, swayspeed = 0, swaydist = 0, bobfade = 0, bobdist = 0;
    vec swaydir(0, 0, 0), swaypush(0, 0, 0);

    string clientmap = "";

    gameent *player1 = new gameent(), *focus = player1, *lastfocus = focus;
    avatarent avatarmodel, bodymodel;
    vector<gameent *> players, waiting;
    vector<cament *> cameras;

    void start()
    {
        player1->version.major = VERSION_MAJOR;
        player1->version.minor = VERSION_MINOR;
        player1->version.patch = VERSION_PATCH;
        player1->version.game = VERSION_GAME;
        player1->version.platform = versionplatform;
        player1->version.arch = versionarch;
        player1->version.gpuglver = glversion;
        player1->version.gpuglslver = glslversion;
        player1->version.crc = versioncrc;
        if(player1->version.gpuvendor) delete[] player1->version.gpuvendor;
        player1->version.gpuvendor = newstring(gfxvendor);
        if(player1->version.gpurenderer) delete[] player1->version.gpurenderer;
        player1->version.gpurenderer = newstring(gfxrenderer);
        if(player1->version.gpuversion) delete[] player1->version.gpuversion;
        player1->version.gpuversion = newstring(gfxversion);
    }

    FVAR(IDF_WORLD, illumlevel, 0, 0, 2);
    VAR(IDF_WORLD, illumradius, 0, 0, VAR_MAX);
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
    SVAR(IDF_WORLD, obitfall, "");

    void stopmapmusic()
    {
        if(connected() && maptime > 0 && gs_intermission(gamestate)) musicdone(true);
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

    VAR(IDF_PERSIST, firstpersonmodel, 0, 2, 2);
    VAR(IDF_PERSIST, firstpersonfov, 90, 100, 150);
    FVAR(IDF_PERSIST, firstpersonblend, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersondepth, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, firstpersonbodydepth, 0, 0.65f, 1);

    FVAR(IDF_PERSIST, firstpersonbodydist, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodyside, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodypitch, -1, 1, 1);

    FVAR(IDF_PERSIST, firstpersonspine, 0, 0.45f, 1);
    FVAR(IDF_PERSIST, firstpersonpitchmin, 0, 90, 90);
    FVAR(IDF_PERSIST, firstpersonpitchmax, 0, 45, 90);
    FVAR(IDF_PERSIST, firstpersonpitchscale, -1, 1, 1);
    FVAR(IDF_PERSIST, firstpersonbodyfeet, -1, 4.75f, 20);

    VAR(IDF_PERSIST, firstpersonsway, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersonswaymin, 0, 0.15f, 1);
    FVAR(IDF_PERSIST, firstpersonswaystep, 1, 28.f, 1000);
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

    VAR(IDF_PERSIST, editfov, 1, 120, 179);
    VAR(IDF_PERSIST, specfov, 1, 120, 179);

    VAR(IDF_PERSIST, specresetstyle, 0, 1, 1); // 0 = back to player1, 1 = stay at camera

    VAR(IDF_PERSIST, followmode, 0, 1, 1); // 0 = never, 1 = tv
    VARF(IDF_PERSIST, specmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, waitmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, intermmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv

    VAR(IDF_PERSIST, followdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, followthirdperson, 0, 1, 1);
    VAR(IDF_PERSIST, followaiming, 0, 1, 3); // 0 = don't aim, &1 = aim in thirdperson, &2 = aim in first person
    FVAR(IDF_PERSIST, followblend, 0, 1, 1);
    FVAR(IDF_PERSIST, followdist, FVAR_NONZERO, 10, FVAR_MAX);
    FVAR(IDF_PERSIST, followside, FVAR_MIN, 8, FVAR_MAX);

    VAR(IDF_PERSIST, followtvspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, followtvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, followtvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, followtvrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, followtvyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, followtvpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, followtvyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, followtvpitchthresh, 0, 0, 180);

    VAR(IDF_PERSIST, spectvtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvmintime, 1000, 3000, VAR_MAX);
    VAR(IDF_PERSIST, spectvmaxtime, 0, 15000, VAR_MAX);
    VAR(IDF_PERSIST, spectvspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvpitchthresh, 0, 0, 180);
    VAR(IDF_PERSIST, spectvdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfirstperson, 0, 0, 2); // 0 = aim in direction followed player is facing, 1 = aim in direction determined by spectv when dead, 2 = always aim in direction
    VAR(IDF_PERSIST, spectvthirdperson, 0, 2, 2); // 0 = aim in direction followed player is facing, 1 = aim in direction determined by spectv when dead, 2 = always aim in direction

    VAR(IDF_PERSIST, spectvintermtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermmintime, 1000, 6000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvintermrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvintermyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvintermpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvintermyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvintermpitchthresh, 0, 0, 180);

    VARF(0, spectvfollow, -1, -1, VAR_MAX, spectvfollowing = spectvfollow); // attempts to always keep this client in view
    VAR(0, spectvfollowself, 0, 1, 2); // if we are not spectating, spectv should show us; 0 = off, 1 = not duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfollowtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowmintime, 1000, 1000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowpitchspeed, 1, 350, VAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowrotate, FVAR_MIN, 45, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvfollowyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvfollowpitchscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvfollowyawthresh, 0, 0, 360);
    FVAR(IDF_PERSIST, spectvfollowpitchthresh, 0, 0, 180);

    FVAR(IDF_PERSIST, spectvmindist, 0, 0, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvmaxdist, 0, 128, FVAR_MAX);

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

    VAR(IDF_PERSIST, aboveheadnames, 0, 1, 1);
    VAR(IDF_PERSIST, aboveheadinventory, 0, 0, 2); // 0 = off, 1 = weapselect only, 2 = all weapons
    VAR(IDF_PERSIST, aboveheadstatus, 0, 1, 1);
    VAR(IDF_PERSIST, aboveheadteam, 0, 0, 3);
    VAR(IDF_PERSIST, aboveheaddamage, 0, 0, 1);
    VAR(IDF_PERSIST, aboveheadicons, 0, 5, 7);
    FVAR(IDF_PERSIST, aboveheadblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadnamesblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadinventoryblend, 0.f, 0.8f, 1.f);
    FVAR(IDF_PERSIST, aboveheadinventoryfade, 0.f, 0.5f, 1.f);
    FVAR(IDF_PERSIST, aboveheadstatusblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadiconsblend, 0.f, 1, 1.f);
    FVAR(IDF_PERSIST, aboveheadnamessize, 0, 3, 10);
    FVAR(IDF_PERSIST, aboveheadinventorysize, 0, 4, 10);
    FVAR(IDF_PERSIST, aboveheadstatussize, 0, 2.5f, 10);
    FVAR(IDF_PERSIST, aboveheadiconssize, 0, 2.5f, 10);
    FVAR(IDF_PERSIST, aboveheadeventsize, 0, 3, 10);

    FVAR(IDF_PERSIST, aboveitemiconsize, 0, 2.5f, 10);

    FVAR(IDF_PERSIST, aboveheadsmooth, 0, 0.25f, 1);
    VAR(IDF_PERSIST, aboveheadsmoothmillis, 1, 100, 10000);

    VAR(IDF_PERSIST, eventiconfade, 500, 7000, VAR_MAX);
    VAR(IDF_PERSIST, eventiconshort, 500, 3500, VAR_MAX);

    VAR(IDF_PERSIST, showobituaries, 0, 4, 5); // 0 = off, 1 = only me, 2 = 1 + announcements, 3 = 2 + but dying bots, 4 = 3 + but bot vs bot, 5 = all
    VAR(IDF_PERSIST, showobitdists, 0, 2, 2); // 0 = off, 1 = self only, 2 = all
    VAR(IDF_PERSIST, showobithpleft, 0, 1, 2); // 0 = off, 1 = self only, 2 = all
    VAR(IDF_PERSIST, obitannounce, 0, 2, 2); // 0 = off, 1 = only focus, 2 = everyone
    VAR(IDF_PERSIST, obitverbose, 0, 1, 1); // 0 = extremely simple, 1 = regular messages
    VAR(IDF_PERSIST, obitstyles, 0, 1, 1); // 0 = no obituary styles, 1 = show sprees/dominations/etc

    VAR(IDF_PERSIST, damagemergedelay, 0, 75, VAR_MAX);
    VAR(IDF_PERSIST, damagemergeburn, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, damagemergebleed, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, damagemergeshock, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, playdamagetones, 0, 1, 3);
    VAR(IDF_PERSIST, damagetonevol, -1, -1, 255);
    VAR(IDF_PERSIST, playreloadnotify, 0, 3, 15);
    VAR(IDF_PERSIST, reloadnotifyvol, -1, -1, 255);

    VAR(IDF_PERSIST, deathanim, 0, 2, 2); // 0 = hide player when dead, 1 = old death animation, 2 = ragdolls
    VAR(IDF_PERSIST, deathfade, 0, 1, 1); // 0 = don't fade out dead players, 1 = fade them out
    VAR(IDF_PERSIST, deathscale, 0, 0, 1); // 0 = don't scale out dead players, 1 = scale them out
    VAR(IDF_PERSIST, deathbuttonmash, 0, 1000, VAR_MAX);
    FVAR(IDF_PERSIST, bloodscale, 0, 1, 1000);
    VAR(IDF_PERSIST, bloodfade, 1, 3000, VAR_MAX);
    VAR(IDF_PERSIST, bloodsize, 1, 50, 1000);
    VAR(IDF_PERSIST, bloodsparks, 0, 0, 1);
    FVAR(IDF_PERSIST, debrisscale, 0, 1, 1000);
    VAR(IDF_PERSIST, debrisfade, 1, 3500, VAR_MAX);
    FVAR(IDF_PERSIST, gibscale, 0, 1, 1000);
    VAR(IDF_PERSIST, gibfade, 1, 5000, VAR_MAX);
    FVAR(IDF_PERSIST, impulsescale, 0, 1, 1000);
    VAR(IDF_PERSIST, impulsefade, 0, 200, VAR_MAX);
    VAR(IDF_PERSIST, ragdolleffect, 2, 500, VAR_MAX);

    VAR(IDF_PERSIST, playerovertone, -1, CTONE_TEAM, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerundertone, -1, CTONE_TONE, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerdisplaytone, -1, CTONE_TONE, CTONE_MAX-1);
    VAR(IDF_PERSIST, playereffecttone, -1, CTONE_TEAMED, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerlighttone, -1, CTONE_TEAMED, CTONE_MAX-1);
    VAR(IDF_PERSIST, playerteamtone, -1, CTONE_TEAM, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerlightmix, 0, 0.5f, 100);
    FVAR(IDF_PERSIST, playertonemix, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, playerblend, 0, 1, 1);
    FVAR(IDF_PERSIST, playereditblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, playerghostblend, 0, 0.5f, 1);

    VAR(IDF_PERSIST, playerhint, 0, 3, 3);
    VAR(IDF_PERSIST, playerhinthurt, 0, 1, 1);
    VAR(IDF_PERSIST, playerhinthurtthrob, 0, 1, 1);
    VAR(IDF_PERSIST, playerhinttone, -1, CTONE_TEAMED, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerhintblend, 0, 0.1f, 1);
    FVAR(IDF_PERSIST, playerhintscale, 0, 0.7f, 1); // scale blend depending on health
    FVAR(IDF_PERSIST, playerhintsize, 0, 1.2f, 2);
    FVAR(IDF_PERSIST, playerhintmaxsize, 0, 20, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhintfadeat, 0, 64, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhintfadecut, 0, 8, FVAR_MAX);
    FVAR(IDF_PERSIST, playerhinthurtblend, 0, 0.9f, 1);
    FVAR(IDF_PERSIST, playerhinthurtsize, 0, 1.2f, 2);

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

    VAR(IDF_PERSIST, nogore, 0 , 0, 2); // turns off all gore, 0 = off, 1 = replace, 2 = remove
    VAR(IDF_PERSIST, forceplayermodel, -1, -1, PLAYERTYPES-1);
    VAR(IDF_PERSIST, vanitymodels, 0, 1, 1);
    VAR(IDF_PERSIST, headlessmodels, 0, 1, 1);
    FVAR(IDF_PERSIST, twitchspeed, 0, 8, FVAR_MAX);

    bool wantsloadoutmenu = false;
    VAR(IDF_PERSIST, showloadoutmenu, 0, 0, 1); // show the loadout menu at the start of a map

    bool needname(gameent *d)
    {
        if(!d || *d->name) return false; // || client::waiting()) return false;
        return true;
    }
    ICOMMAND(0, needname, "b", (int *cn), intret(needname(*cn >= 0 ? getclient(*cn) : player1) ? 1 : 0));

    ICOMMAND(0, gamemode, "", (), intret(gamemode));
    ICOMMAND(0, mutators, "", (), intret(mutators));

    int mutscheck(int g, int m, int t)
    {
        int mode = g, muts = m;
        modecheck(mode, muts, t);
        return muts;
    }
    ICOMMAND(0, mutscheck, "iii", (int *g, int *m, int *t), intret(mutscheck(*g, *m, *t)));
    ICOMMAND(0, mutsallowed, "ii", (int *g, int *h), intret(*g >= 0 && *g < G_MAX ? gametype[*g].mutators[*h >= 0 && *h < G_M_GSP+1 ? *h : 0] : 0));
    ICOMMAND(0, mutsimplied, "ii", (int *g, int *m), intret(*g >= 0 && *g < G_MAX ? gametype[*g].implied : 0));
    ICOMMAND(0, gspmutname, "ii", (int *g, int *n), result(*g >= 0 && *g < G_MAX && *n >= 0 && *n < G_M_GSN ? gametype[*g].gsp[*n] : ""));
    ICOMMAND(0, getintermission, "", (), intret(gs_intermission(gamestate) ? 1 : 0));
    ICOMMAND(0, getgamestate, "", (), intret(gamestate));

    const char *gametitle() { return connected() ? server::gamename(gamemode, mutators) : "ready"; }
    const char *gametext() { return connected() ? mapname : "not connected"; }

    void vanityreset()
    {
        loopvrev(vanities) vanities.remove(i);
    }
    ICOMMAND(0, resetvanity, "", (), vanityreset());

    int vanityitem(int type, const char *ref, const char *name, const char *tag, int cond, int style)
    {
        if(type < 0 || type >= VANITYMAX || !ref || !name || !tag) return -1;
        int num = vanities.length();
        vanitys &v = vanities.add();
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
        vector<char *> vanitylist;
        explodelist(d->vanity, vanitylist);
        loopv(vanitylist) if(vanitylist[i] && *vanitylist[i])
            loopvk(vanities) if(!strcmp(vanities[k].ref, vanitylist[i]))
                d->vitems.add(k);
        vanitylist.deletearrays();
    }

    const char *vanityfname(gameent *d, int n, bool proj)
    {
        const char *file = NULL;
        if(vanities.inrange(n)) switch(vanities[n].style)
        {
            case 1:
            {
                const char *id = server::privnamex(d->privilege, d->actortype, true);
                loopv(vanities[n].files)
                    if(vanities[n].files[i].proj == proj && !strcmp(vanities[n].files[i].id, id))
                        file = vanities[n].files[i].name;
                if(!file)
                {
                    vanityfile &f = vanities[n].files.add();
                    defformatstring(fn)("%s/%s%s", vanities[n].model, id, proj ? "/proj" : "");
                    f.id = newstring(id);
                    f.name = newstring(fn);
                    f.proj = proj;
                    file = f.name;
                }
                break;
            }
            case 0: default:
            {
                if(proj && !vanities[n].proj)
                {
                    defformatstring(fn)("%s/proj", vanities[n].model);
                    vanities[n].proj = newstring(fn);
                }
                file = proj ? vanities[n].proj : vanities[n].model;
                break;
            }
        }
        return file;
    }

    bool allowspec(gameent *d, int level, int cn = -1)
    {
        if(d->o.magnitude() <= 0 || d->o.z < 0) return false;
        if(d->state == CS_SPECTATOR || ((d->state == CS_DEAD || d->state == CS_WAITING) && !d->lastdeath)) return false;
        if(cn >= 0)
        {
            if(cn == player1->clientnum && player1->state != CS_ALIVE && d->clientnum == player1->lastattacker) return true;
            return d->clientnum == cn; // override
        }
        switch(level)
        {
            case 0: if(d->state != CS_ALIVE) return false; break;
            case 1: if(m_duke(gamemode, mutators) && d->state != CS_ALIVE) return false; break;
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
        int r = curfov;
        if(player1->state == CS_EDITING) r = editfov;
        else if(focus == player1 && player1->state == CS_SPECTATOR) r = specfov;
        else if(thirdpersonview(true)) r = thirdpersonfov;
        else r = firstpersonfov;
        return r;
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
            if(focus == game::player1) resetcursor();
            lastzoom = millis-max(zoomtime-(millis-lastzoom), 0);
            prevzoom = zooming;
            if(zoomdefault >= 0 && on) zoomlevel = zoomdefault;
        }
        checkzoom();
        zooming = on;
    }

    bool inzoom()
    {
        if(lastzoom && (zooming || lastmillis-lastzoom <= zoomtime))
            return true;
        return false;
    }
    ICOMMAND(0, iszooming, "", (), intret(inzoom() ? 1 : 0));

    bool inzoomswitch()
    {
        if(lastzoom && ((zooming && lastmillis-lastzoom >= zoomtime/2) || (!zooming && lastmillis-lastzoom <= zoomtime/2)))
            return true;
        return false;
    }

    void resetsway()
    {
        swaydir = swaypush = vec(0, 0, 0);
        swayfade = swayspeed = swaydist = bobfade = bobdist = 0;
    }

    void addsway(gameent *d)
    {
        float speed = physics::movevelocity(d), step = firstpersonbob ? firstpersonbobstep : firstpersonswaystep;
        if(d->state == CS_ALIVE && (d->physstate >= PHYS_SLOPE || d->onladder || d->turnside))
        {
            swayspeed = max(speed*firstpersonswaymin, min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), speed));
            swaydist += swayspeed*curtime/1000.0f;
            swaydist = fmod(swaydist, 2*step);
            bobdist += swayspeed*curtime/1000.0f;
            bobdist = fmod(bobdist, 2*firstpersonbobstep);
            bobfade = swayfade = 1;
        }
        else
        {
            if(swayfade > 0)
            {
                swaydist += swayspeed*swayfade*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*step);
                swayfade -= 0.5f*(curtime*speed)/(step*1000.0f);
            }
            if(bobfade > 0)
            {
                bobdist += swayspeed*bobfade*curtime/1000.0f;
                bobdist = fmod(bobdist, 2*firstpersonbobstep);
                bobfade -= 0.5f*(curtime*speed)/(firstpersonbobstep*1000.0f);
            }
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
    struct ancbuf
    {
        int idx;
        physent *t;
        int *chan;

        ancbuf() : idx(-1), t(NULL), chan(NULL) {}
        ~ancbuf() {}

        bool play()
        {
            if(!issound(*chan))
            {
                playsound(idx, t->o, t, t != camera1 ? SND_IMPORT : SND_FORCED, -1, -1, -1, chan);
                return true;
            }
            return false;
        }
    };
    vector<ancbuf> anclist;

    VAR(IDF_PERSIST, announcecollect, 1, 1, 3);
    VAR(IDF_PERSIST, announcededupe, 0, 2, 3);
    VAR(IDF_PERSIST, announcebuffer, 1, 64, VAR_MAX);

    void removeannounceall()
    {
        loopvrev(anclist) anclist.remove(i);
        if(issound(announcerchan)) removesound(announcerchan);
        announcerchan = -1;
    }

    void removeannounce(gameent *d)
    {
        loopvrev(anclist) if(anclist[i].chan == &d->aschan) anclist.remove(i);
    }

    void checkannounce()
    {
        loopv(anclist) if(anclist[i].play()) anclist.remove(i--);
        while(anclist.length() > announcebuffer) anclist.pop();
    }

    void announce(int idx, gameent *d, bool forced)
    {
        if(idx < 0) return;
        physent *t = !d || d == player1 || forced ? camera1 : d;
        bool ispl = d && !forced, inuse = false;
        int *chan = ispl ? &d->aschan : &announcerchan;
        if(issound(*chan))
        {
            if(announcededupe&(ispl ? 2 : 1) && sounds[*chan].slotnum == idx) return; // de-dupe
            inuse = true;
        }
        if(announcecollect&(ispl ? 2 : 1))
        {
            loopv(anclist) if(anclist[i].chan == chan)
            {
                if(announcededupe&(ispl ? 2 : 1) && anclist[i].idx == idx) return; // de-dupe
                inuse = true;
            }
            if(inuse)
            {
                ancbuf &a = anclist.add();
                a.idx = idx;
                a.t = t;
                a.chan = chan;
                return;
            }
        }
        playsound(idx, t->o, t, t != camera1 ? SND_IMPORT : SND_FORCED, -1, -1, -1, chan);
    }

    void announcef(int idx, int targ, gameent *d, bool forced, const char *msg, ...)
    {
        if(targ >= 0 && msg && *msg)
        {
            defvformatbigstring(text, msg, msg);
            conoutft(targ == CON_INFO && d == player1 ? CON_SELF : targ, "%s", text);
        }
        announce(idx, d, forced);
    }
    ICOMMAND(0, announce, "iiisN", (int *idx, int *targ, int *cn, int *forced, char *s, int *numargs), (*numargs >= 5 ? announcef(*numargs >= 1 ? *idx : -1, *numargs >= 2 ? *targ : CON_MESG, *numargs >= 3 ? getclient(*cn) : NULL, *numargs >= 4 ? *forced!=0 : false, "\fw%s", s) : announcef(*numargs >= 1 ? *idx : -1, *numargs >= 2 ? *targ : CON_MESG, *numargs >= 3 ? getclient(*cn) : NULL, *numargs >= 4 ? *forced!=0 : false, NULL)));

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
    }

    void specreset(gameent *d, bool clear)
    {
        if(d)
        {
            if(clear)
            {
                loopvrev(cameras) if(cameras[i]->player == d)
                {
                    if(cameras[i]->type == cament::PLAYER)
                    {
                        cament *p = cameras[i];
                        cameras.remove(i);
                        if(p) delete p;
                        if(!i) lastcamera = lasttvcam = lasttvchg = 0;
                    }
                    else cameras[i]->player = NULL;
                }
                if(d == focus) resetfollow();
            }
            else if(maptime > 0 && gameent::is(d) && d->actortype < A_ENEMY)
            {
                cament *c = cameras.add(new cament);
                c->o = d->headpos();
                c->type = cament::PLAYER;
                c->id = d->clientnum;
                c->player = d;
            }
        }
        else
        {
            cameras.deletecontents();
            lastcamera = lasttvcam = lasttvchg = 0;
            resetfollow();
        }
    }

    bool followaim() { return followaiming&(thirdpersonview(true) ? 1 : 2); }

    bool tvmode(bool check, bool force)
    {
        if(!m_edit(gamemode) && (!check || !cameras.empty()))
        {
            if(!gs_playing(gamestate) && intermmode) return true;
            else switch(player1->state)
            {
                case CS_SPECTATOR: if(specmode || (force && focus != player1 && followmode && followaim())) return true; break;
                case CS_WAITING: if((waitmode && (!player1->lastdeath || lastmillis-player1->lastdeath >= 500)) || (force && focus != player1 && followmode && followaim()))
                    return true; break;
                default: break;
            }
        }
        return false;
    }

    ICOMMAND(0, specmodeswitch, "", (), {
        if(tvmode(true, true))
        {
            if(!tvmode(true, false)) followmode = 0;
            else specmode = 0;
        }
        else if(focus != player1) followmode = 1;
        else specmode = 1;
        specreset();
    });
    ICOMMAND(0, waitmodeswitch, "", (), {
        if(tvmode(true, true))
        {
            if(!tvmode(true, false)) followmode = 0;
            else waitmode = 0;
        }
        else if(focus != player1) followmode = 1;
        else waitmode = 1;
        specreset();
    });

    bool followswitch(int n, bool other)
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
                *f += clamp(n, -1, 1); \
                checkfollow; \
                if(*f == -1) \
                { \
                    if(other) *f += clamp(n, -1, 1); \
                    else \
                    { \
                        specreset(); \
                        return true; \
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
                    if(!d || d->actortype >= A_ENEMY || !allowspec(d, istv ? spectvdead : followdead)) addfollow
                    else
                    {
                        focus = d;
                        return true;
                    }
                }
            }
            specreset();
        }
        return false;
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

    void respawn(gameent *d)
    {
        if(d == game::player1 && (maptime <= 0 || needname(d) || wantsloadoutmenu)) return; // prevent spawning
        if(d->state == CS_DEAD && d->respawned < 0 && (!d->lastdeath || lastmillis-d->lastdeath >= 500))
        {
            client::addmsg(N_TRYSPAWN, "ri", d->clientnum);
            d->respawned = lastmillis;
        }
    }

    void respawned(gameent *d, bool local, int ent)
    { // remote clients wait until first position update to process this
        if(local)
        {
            d->state = CS_ALIVE;
            entities::spawnplayer(d, ent, true);
            client::addmsg(N_SPAWN, "ri", d->clientnum);
        }
        d->setscale(rescale(d), 0, true);

        if(d == player1) specreset();
        else if(d == focus) resetcamera();

        if(d->actortype < A_ENEMY)
        {
            playsound(S_RESPAWN, d->o, d);
            if(dynlighteffects)
            {
                adddynlight(d->headpos(), d->height*2, vec::hexcolor(getcolour(d, playereffecttone)).mul(2.f), 250, 250);
                regularshape(PART_SPARK, d->height*2, getcolour(d, playerundertone), 53, 50, 350, d->center(), 1.5f, 1, 1, 0, 35);
                regularshape(PART_SPARK, d->height*2, getcolour(d, playerovertone), 53, 50, 350, d->center(), 1.5f, 1, 1, 0, 35);
            }
            if(entities::ents.inrange(ent) && entities::ents[ent]->type == PLAYERSTART) entities::execlink(d, ent, false);
        }
        ai::respawned(d, local, ent);
    }

    vec pulsecolour(physent *d, int i, int cycle)
    {
        size_t seed = size_t(d) + (lastmillis/cycle);
        int n = detrnd(seed, PULSECOLOURS), n2 = detrnd(seed + 1, PULSECOLOURS), q = clamp(i, 0, int(PULSE_LAST));
        return vec::hexcolor(pulsecols[q][n]).lerp(vec::hexcolor(pulsecols[q][n2]), (lastmillis%cycle)/float(cycle));
    }

    int hexpulsecolour(physent *d, int i, int cycle)
    {
        bvec h = bvec::fromcolor(pulsecolour(d, i, cycle));
        return (h.r<<16)|(h.g<<8)|h.b;
    }

    vec getpalette(int palette, int index)
    { // colour palette abstractions for textures, etc.
        if(palette >= 0 && index >= 0) switch(palette)
        {
            case 0: // misc
            {
                switch(index)
                {
                    case 0: break; // off
                    case 1: case 2: case 3: case 4:
                        return vec::hexcolor(pulsecols[index-1][clamp((lastmillis/100)%PULSECOLOURS, 0, PULSECOLOURS-1)]);
                        break;
                    default: break;
                }
                break;
            }
            case 1: // teams
            {
                int team = index%T_ALL;
                if(!m_edit(gamemode) && index < T_ALL)
                {
                    if(!m_team(gamemode, mutators) || team > (m_multi(gamemode, mutators) ? T_MULTI : T_LAST))
                        team = T_NEUTRAL; // abstract team coloured levels to neutral
                }
                return vec::hexcolor(TEAM(team, colour));
                break;
            }
            case 2: // weapons
            {
                int weap = index%W_MAX;
                if(!m_edit(gamemode) && index < W_MAX)
                {
                    weap = w_attr(gamemode, mutators, WEAPON, weap, m_weapon(gamemode, mutators));
                    if(!isweap(weap) || W(weap, disabled) || (m_loadout(gamemode, mutators) && weap < W_ITEM) || !m_check(W(weap, modes), W(weap, muts), gamemode, mutators))
                        weap = -1;
                }
                if(isweap(weap)) return vec::hexcolor(W(weap, colour));
                break;
            }
            default: break;
        }
        return vec(1, 1, 1);
    }

    void adddynlights()
    {
        if(dynlighteffects)
        {
            projs::adddynlights();
            entities::adddynlights();
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
                    bool last = lastmillis-d->weaplast[d->weapselect] > 0,
                         powering = last && d->weapstate[d->weapselect] == W_S_POWER,
                         reloading = last && d->weapstate[d->weapselect] == W_S_RELOAD,
                         secondary = physics::secondaryweap(d);
                    float amt = last ? clamp(float(lastmillis-d->weaplast[d->weapselect])/d->weapwait[d->weapselect], 0.f, 1.f) : 0.f;
                    vec col = WPCOL(d, d->weapselect, lightcol, secondary);
                    if(d->weapselect == W_FLAMER && (!reloading || amt > 0.5f) && !physics::liquidcheck(d))
                    {
                        float scale = powering ? 1.f+(amt*1.5f) : (d->weapstate[d->weapselect] == W_S_IDLE ? 1.f : (reloading ? (amt-0.5f)*2 : amt));
                        adddynlight(d->ejectpos(d->weapselect), 16*scale, col, 0, 0, DL_KEEP);
                    }
                    if((W(d->weapselect, lightpersist) || powering) && W(d->weapselect, lightradius) > 0)
                    {
                        float thresh = max(amt, 0.25f), size = W(d->weapselect, lightradius)*thresh;
                        int span = max(W2(d->weapselect, cooktime, physics::secondaryweap(d))/4, 500), interval = lastmillis%span, part = span/2;
                        if(interval) size += size*0.5f*(interval <= part ? interval/float(part) : (span-interval)/float(part));
                        adddynlight(d->muzzlepos(d->weapselect), size, vec(col).mul(thresh), 0, 0, DL_KEEP);
                    }
                }
                if(burntime && d->burning(lastmillis, burntime))
                {
                    int millis = lastmillis-d->lastres[WR_BURN];
                    size_t seed = size_t(d) + (millis/50);
                    float pc = 1, amt = (millis%50)/50.0f, intensity = 0.75f+(detrnd(seed, 25)*(1-amt) + detrnd(seed + 1, 25)*amt)/100.f;
                    if(burntime-millis < burndelay) pc *= float(burntime-millis)/float(burndelay);
                    else
                    {
                        float fluc = float(millis%burndelay)*(0.25f+0.03f)/burndelay;
                        if(fluc >= 0.25f) fluc = (0.25f+0.03f-fluc)*(0.25f/0.03f);
                        pc *= 0.75f+fluc;
                    }
                    adddynlight(d->center(), d->height*intensity*pc, pulsecolour(d).mul(pc), 0, 0, DL_KEEP);
                }
                if(shocktime && d->shocking(lastmillis, shocktime))
                {
                    int millis = lastmillis-d->lastres[WR_SHOCK];
                    size_t seed = size_t(d) + (millis/50);
                    float pc = 1, amt = (millis%50)/50.0f, intensity = 0.75f+(detrnd(seed, 25)*(1-amt) + detrnd(seed + 1, 25)*amt)/100.f;
                    if(shocktime-millis < shockdelay) pc *= float(shocktime-millis)/float(shockdelay);
                    else
                    {
                        float fluc = float(millis%shockdelay)*(0.25f+0.03f)/shockdelay;
                        if(fluc >= 0.25f) fluc = (0.25f+0.03f-fluc)*(0.25f/0.03f);
                        pc *= 0.75f+fluc;
                    }
                    adddynlight(d->center(), d->height*intensity*pc, rescolour(d, PULSE_SHOCK).mul(pc), 0, 0, DL_KEEP);
                }
                if(d->actortype < A_ENEMY && illumlevel > 0 && illumradius > 0)
                {
                    vec col = vec::hexcolor(getcolour(d, playereffecttone)).mul(illumlevel);
                    adddynlight(d->center(), illumradius, col, 0, 0, DL_KEEP);
                }
            }
        }
    }

    void boosteffect(gameent *d, const vec &pos, int num, int len, bool shape = false)
    {
        float scale = 0.4f+(rnd(40)/100.f);
        part_create(PART_HINT, shape ? len/2 : len/10, pos, getcolour(d, playereffecttone), scale*1.5f, scale*0.75f, 0, 0);
        part_create(PART_FIREBALL, shape ? len/2 : len/10, pos, pulsecols[PULSE_FIRE][rnd(PULSECOLOURS)], scale*1.25f, scale*0.75f, 0, 0);
        if(shape) loopi(num) regularshape(PART_FIREBALL, int(d->radius)*2, pulsecols[PULSE_FIRE][rnd(PULSECOLOURS)], 21, 1, len, pos, scale*1.25f, 0.75f, -5, 0, 10);
    }

    void impulseeffect(gameent *d, int effect)
    {
        int num = int((effect ? 3 : 10)*impulsescale);
        switch(effect)
        {
            case 0: playsound(S_IMPULSE, d->o, d); // fail through
            case 1: if(num > 0 && impulsefade > 0) loopi(3) boosteffect(d, d->jet[i], num, impulsefade, effect==0);
                    break;
        }
    }

    void setmode(int nmode, int nmuts) { modecheck(nextmode = nmode, nextmuts = nmuts); }
    ICOMMAND(0, mode, "ii", (int *val, int *mut), setmode(*val, *mut));

    float spawnfade(gameent *d)
    {
        int len = d->actortype >= A_ENEMY ? (actor[d->actortype].living ? min(ai::aideadfade, enemyspawntime) : 500) : m_delay(gamemode, mutators, d->team);
        if(len > 0)
        {
            int interval = min(len/3, ragdolleffect), over = max(len-interval, 1), millis = lastmillis-d->lastdeath;
            if(millis <= len) { if(millis >= over) return 1.f-((millis-over)/float(interval)); }
            else return 0;
        }
        return 1;
    }

    float rescale(gameent *d)
    {
        float total = d->actortype != A_PLAYER ? (d->actortype >= A_ENEMY ? enemyscale : botscale) : PLAYER(d->model, scale);
        if(d->actortype >= A_ENEMY)
        {
            bool hasent = entities::ents.inrange(d->spawnpoint) && entities::ents[d->spawnpoint]->type == ACTOR;
            if(hasent && entities::ents[d->spawnpoint]->attrs[9] > 0) total *= (entities::ents[d->spawnpoint]->attrs[9]/100.f);
            else total *= actor[clamp(d->actortype, int(A_PLAYER), int(A_MAX-1))].scale;
        }
        if(d->state != CS_SPECTATOR && d->state != CS_EDITING)
        {
            if(m_resize(gamemode, mutators))
            {
                float minscale = 1, amtscale = m_insta(gamemode, mutators) ? 1+(d->spree*instaresizeamt) : max(d->health, 1)/float(d->actortype >= A_ENEMY ? actor[d->actortype].health*enemystrength : m_health(gamemode, mutators, d->model));
                if(m_resize(gamemode, mutators))
                {
                    minscale = minresizescale;
                    if(amtscale < 1) amtscale = (amtscale*(1-minscale))+minscale;
                }
                total *= clamp(amtscale, minscale, maxresizescale);
            }
            if(deathscale && (d->state == CS_DEAD || d->state == CS_WAITING)) total *= spawnfade(d);
        }
        return total;
    }

    float opacity(gameent *d, bool third)
    {
        float total = d == focus ? (third ? (d != player1 ? followblend : thirdpersonblend) : firstpersonblend) : playerblend;
        if(physics::isghost(d, focus)) total *= playerghostblend;
        if(deathfade && (d->state == CS_DEAD || d->state == CS_WAITING)) total *= spawnfade(d);
        else if(d->state == CS_ALIVE)
        {
            if(d == focus && third) total *= camera1->o.dist(d->o)/(d != player1 ? followdist : thirdpersondist);
            int prot = m_protect(gamemode, mutators), millis = d->protect(lastmillis, prot); // protect returns time left
            if(millis > 0) total *= 1.f-(float(millis)/float(prot));
            if(d == focus && inzoom())
            {
                int frame = lastmillis-lastzoom;
                float pc = frame <= zoomtime ? (frame)/float(zoomtime) : 1.f;
                total *= zooming ? 1.f-pc : pc;
            }
        }
        else if(d->state == CS_EDITING) total *= playereditblend;
        return total;
    }

    void footstep(gameent *d, int curfoot)
    {
        bool moving = d->move || d->strafe, liquid = physics::liquidcheck(d), onfloor = d->physstate >= PHYS_SLOPE || d->onladder || d->turnside;
        if(curfoot < 0 || (moving && (liquid || onfloor)))
        {
            float mag = d->vel.magnitude(), m = min(footstepsoundmax, footstepsoundmin), n = max(footstepsoundmax, footstepsoundmin);
            if(n > m && mag > m)
            {
                if(curfoot < 0) curfoot = d->lastfoot;
                vec pos = d->footpos(curfoot);
                float amt = clamp(mag/n, 0.f, 1.f)*(d != focus ? footstepsoundlevel : footstepsoundfocus);
                if(onfloor && (!d->running(moveslow) || d->crouching())) amt *= footstepsoundlight;
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

        d->setscale(rescale(d), curtime, false);
        d->speedscale = d->curscale;
        if(d->actortype > A_PLAYER)
        {
            bool hasent = d->actortype >= A_ENEMY && entities::ents.inrange(d->spawnpoint) && entities::ents[d->spawnpoint]->type == ACTOR;
            if(hasent && entities::ents[d->spawnpoint]->attrs[8] > 0) d->speedscale *= entities::ents[d->spawnpoint]->attrs[8]*enemyspeed;
            else d->speedscale *= d->actortype >= A_ENEMY ? enemyspeed : botspeed;
        }

        float offset = d->height;
        d->o.z -= d->height;
        if(d->state == CS_ALIVE && actor[clamp(d->actortype, int(A_PLAYER), int(A_MAX-1))].cancrouch)
        {
            bool crouching = d->action[AC_CROUCH];
            float crouchoff = 1.f-CROUCHHEIGHT, zrad = d->zradius-(d->zradius*crouchoff);
            vec old = d->o;
            if(!crouching) loopk(2)
            {
                if(k)
                {
                    vec dir;
                    vecfromyawpitch(d->yaw, 0, d->move, d->strafe, dir);
                    d->o.add(dir.mul(2));
                }
                d->o.z += d->zradius;
                d->height = d->zradius;
                if(collide(d, vec(0, 0, 1), 0, false) || collideinside)
                {
                    d->o.z -= d->zradius-zrad;
                    d->height = zrad;
                    if(!collide(d, vec(0, 0, 1), 0, false) && !collideinside) crouching = true;
                }
                d->o = old;
                d->height = offset;
                if(crouching)
                {
                    if(d->actiontime[AC_CROUCH] >= 0)
                        d->actiontime[AC_CROUCH] = max(PHYSMILLIS-(lastmillis-d->actiontime[AC_CROUCH]), 0)-lastmillis;
                    break;
                }
                else if(k && d->actiontime[AC_CROUCH] < 0)
                {
                    d->actiontime[AC_CROUCH] = lastmillis-max(PHYSMILLIS-(lastmillis+d->actiontime[AC_CROUCH]), 0);
                    break;
                }
            }
            if(d->crouching())
            {
                int crouchtime = abs(d->actiontime[AC_CROUCH]);
                float amt = lastmillis-crouchtime <= PHYSMILLIS ? clamp(float(lastmillis-crouchtime)/PHYSMILLIS, 0.f, 1.f) : 1.f;
                if(!crouching) amt = 1.f-amt;
                crouchoff *= amt;
                d->height = d->zradius-(d->zradius*crouchoff);
            }
            else d->height = d->zradius;
        }
        else d->height = d->zradius;
        d->o.z += d->airmillis ? offset : d->height;

        d->checktags();

        if(m_impulsemeter(gamemode, mutators) && canregenimpulse(d) && d->impulse[IM_METER] > 0)
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
            impulsemod(d->sliding(), impulseregenslide);
            if(collect)
            {
                if(timeslice > 0)
                {
                    if((d->impulse[IM_METER] -= timeslice) < 0) d->impulse[IM_METER] = 0;
                    d->impulse[IM_COLLECT] = 0;
                }
                else d->impulse[IM_COLLECT] += curtime;
            }
        }

        loopi(W_MAX) if(d->weapstate[i] != W_S_IDLE && (!gs_playing(gamestate) || d->weapselect != i || d->weapstate[i] != W_S_ZOOM))
        {
            bool timeexpired = lastmillis-d->weaplast[i] >= d->weapwait[i]+(d->weapselect != i || d->weapstate[i] != W_S_POWER ? 0 : PHYSMILLIS);
            if(gs_playing(gamestate) && d->state == CS_ALIVE && i == d->weapselect && d->weapstate[i] == W_S_RELOAD && timeexpired && playreloadnotify&(d == focus ? 1 : 2) && (d->ammo[i] >= W(i, ammomax) || playreloadnotify&(d == focus ? 4 : 8)))
                playsound(WSND(i, S_W_NOTIFY), d->o, d, 0, reloadnotifyvol, -1, -1, &d->wschan);
            if(!gs_playing(gamestate) || d->state != CS_ALIVE || timeexpired) d->setweapstate(i, W_S_IDLE, 0, lastmillis);
        }
        if(gs_playing(gamestate) && d->state == CS_ALIVE && isweap(d->weapselect) && (d->weapstate[d->weapselect] == W_S_POWER || d->weapstate[d->weapselect] == W_S_ZOOM))
        {
            int millis = lastmillis-d->weaplast[d->weapselect];
            if(millis >= 0 && millis <= d->weapwait[d->weapselect])
            {
                bool secondary = physics::secondaryweap(d);
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
        if(local)
        {
            if(d->respawned > 0 && lastmillis-d->respawned >= 2500) d->respawned = -1;
            if(d->suicided > 0 && lastmillis-d->suicided >= 2500) d->suicided = -1;
        }
        int restime[WR_MAX] = { burntime, bleedtime, shocktime };
        loopi(WR_MAX) if(d->lastres[i] > 0 && lastmillis-d->lastres[i] >= restime[i]) d->resetresidual(i);
        if(gs_playing(gamestate) && d->state == CS_ALIVE)
        {
            int curfoot = d->curfoot();
            bool hassound = footstepsounds&(d != focus ? 2 : 1);
            if(!hassound) loopi(2) if(d->sschan[i] >= 0)
            {
                if(issound(d->sschan[i])) removesound(d->sschan[i]);
                d->sschan[i] = -1;
            }
            if(curfoot != d->lastfoot)
            {
                if(hassound)
                {
                    if(d->lastfoot >= 0 && issound(d->sschan[d->lastfoot])) sounds[d->sschan[d->lastfoot]].pos = d->footpos(d->lastfoot);
                    footstep(d, curfoot);
                    d->lastfoot = curfoot;
                }
            }
            else if(hassound) loopi(2) if(issound(d->sschan[i])) sounds[d->sschan[i]].pos = d->footpos(i);
        }
        loopv(d->icons) if(lastmillis-d->icons[i].millis > d->icons[i].fade) d->icons.remove(i--);
    }


    void otherplayers()
    {
        loopv(players) if(players[i])
        {
            gameent *d = players[i];
            const int lagtime = totalmillis-d->lastupdate;
            if(d->ai || !lagtime || !gs_playing(gamestate)) continue;
            //else if(lagtime > 1000) continue;
            physics::smoothplayer(d, 1, false);
            if(gs_playing(gamestate) && (d->state == CS_DEAD || d->state == CS_WAITING)) entities::checkitems(d);
        }
    }

    bool burn(gameent *d, int weap, int flags)
    {
        if(wr_burns(weap, flags))
        {
            d->lastrestime[WR_BURN] = lastmillis;
            if(isweap(weap)) d->lastres[WR_BURN] = lastmillis;
            else return true;
        }
        return false;
    }

    bool bleed(gameent *d, int weap, int flags)
    {
        if(wr_bleeds(weap, flags))
        {
            d->lastrestime[WR_BLEED] = lastmillis;
            if(isweap(weap)) d->lastres[WR_BLEED] = lastmillis;
            else return true;
        }
        return false;
    }

    bool shock(gameent *d, int weap, int flags)
    {
        if(wr_shocks(weap, flags))
        {
            d->lastrestime[WR_SHOCK] = lastmillis;
            if(isweap(weap)) d->lastres[WR_SHOCK] = lastmillis;
            else return true;
        }
        return false;
    }

    struct damagemerge
    {
        enum { BURN = 1<<0, BLEED = 1<<1, SHOCK = 1<<2 };

        gameent *d, *v;
        int weap, damage, flags, millis;

        damagemerge() { millis = totalmillis; }
        damagemerge(gameent *d, gameent *v, int weap, int damage, int flags) : d(d), v(v), weap(weap), damage(damage), flags(flags) { millis = totalmillis; }

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
                const int dmgsnd[8] = { 0, 10, 25, 50, 75, 100, 150, 200 };
                int snd = -1;
                if(flags&BURN) snd = S_BURNED;
                else if(flags&BLEED) snd = S_BLEED;
                else if(flags&SHOCK) snd = S_SHOCK;
                else loopirev(8) if(damage >= dmgsnd[i]) { snd = S_DAMAGE+i; break; }
                if(snd >= 0) playsound(snd, d->o, d, SND_IMPORT|(v == focus ? SND_NODIST : SND_CLAMPED), damagetonevol);
            }
            if(aboveheaddamage)
            {
                defformatstring(ds)("<sub>\fo%c%d", damage > 0 ? '-' : (damage < 0 ? '+' : '~'), damage < 0 ? 0-damage : damage);
                part_textcopy(d->abovehead(), ds, d != focus ? PART_TEXT : PART_TEXT_ONTOP, eventiconfade, 0xFFFFFF, 4, 1, -10, 0, d);
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
        bool burning = burn(d, weap, flags), bleeding = bleed(d, weap, flags), shocking = shock(d, weap, flags);
        if(!local || burning || bleeding || shocking)
        {
            float scale = isweap(weap) && WF(WK(flags), weap, damage, WS(flags)) ? abs(damage)/float(WF(WK(flags), weap, damage, WS(flags))) : 1.f;
            if(hithurts(flags) && damage > 0)
            {
                if(d == focus) hud::damage(damage, v->o, v, weap, flags);
                if(actor[d->actortype].living)
                {
                    vec p = d->headpos(-d->height/4);
                    if(!nogore && bloodscale > 0)
                        part_splash(PART_BLOOD, int(clamp(damage/20, 1, 5)*bloodscale)*(bleeding ? 2 : 1), bloodfade, p, 0x229999, (rnd(bloodsize/2)+(bloodsize/2))/10.f, 1, 100, DECAL_BLOOD, int(d->radius), 10);
                    if(nogore != 2 && (bloodscale <= 0 || bloodsparks))
                        part_splash(PART_PLASMA, int(clamp(damage/20, 1, 5))*(bleeding ? 2: 1), bloodfade, p, 0x882222, 1, 0.5f, 50, DECAL_STAIN, int(d->radius));
                }
                if(d != v)
                {
                    bool sameteam = m_team(gamemode, mutators) && d->team == v->team;
                    if(!sameteam) pushdamagemerge(d, v, weap, damage, (burning ? damagemerge::BURN : 0)|(bleeding ? damagemerge::BLEED : 0)|(shocking ? damagemerge::SHOCK : 0));
                    else if(v == player1 && !burning && !bleeding && !shocking)
                    {
                        player1->lastteamhit = d->lastteamhit = lastmillis;
                        if(!issound(alarmchan)) playsound(S_ALARM, v->o, v, 0, -1, -1, -1, &alarmchan);
                    }
                    if(!burning && !bleeding && !shocking && !sameteam) v->lasthit = totalmillis;
                }
                if(d->actortype < A_ENEMY && !issound(d->vschan)) playsound(S_PAIN, d->o, d, 0, -1, -1, -1, &d->vschan);
                d->lastpain = lastmillis;
                if(!WK(flags)) playsound(WSND2(weap, WS(flags), S_W_IMPACT), vec(d->center()).add(vec(dir).mul(dist)), NULL, 0, clamp(int(255*scale), 64, 255));
            }
            if(actor[d->actortype].canmove)
            {
                if(weap == -1 && shocking && shockstun)
                {
                    float amt = WRS(flags&HIT_WAVE || !hithurts(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators),
                          s = G(shockstunscale)*amt, g = G(shockstunfall)*amt;
                    d->addstun(weap, lastmillis, G(shockstuntime), shockstun&W_N_STADD ? s : 0.f, shockstun&W_N_GRADD ? g : 0.f);
                    if(shockstun&W_N_STIMM && s > 0) d->vel.mul(1.f-clamp(s, 0.f, 1.f));
                    if(shockstun&W_N_GRIMM && g > 0) d->falling.mul(1.f-clamp(g, 0.f, 1.f));
                    if(shockstun&W_N_SLIDE) d->impulse[IM_SLIP] = lastmillis;
                }
                else if(isweap(weap) && !burning && !bleeding && !shocking && WF(WK(flags), weap, damage, WS(flags)))
                {
                    if(WF(WK(flags), weap, stun, WS(flags)))
                    {
                        int stun = WF(WK(flags), weap, stun, WS(flags));
                        float amt = scale*WRS(flags&HIT_WAVE || !hithurts(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators),
                              s = WF(WK(flags), weap, stunscale, WS(flags))*amt, g = WF(WK(flags), weap, stunfall, WS(flags))*amt;
                        d->addstun(weap, lastmillis, int(scale*WF(WK(flags), weap, stuntime, WS(flags))), stun&W_N_STADD ? s : 0.f, stun&W_N_GRADD ? g : 0.f);
                        if(stun&W_N_STIMM && s > 0) d->vel.mul(1.f-clamp(s, 0.f, 1.f));
                        if(stun&W_N_GRIMM && g > 0) d->falling.mul(1.f-clamp(g, 0.f, 1.f));
                        if(stun&W_N_SLIDE) d->impulse[IM_SLIP] = lastmillis;
                    }
                    if(WF(WK(flags), weap, hitpush, WS(flags)) != 0 || WF(WK(flags), weap, hitvel, WS(flags)) != 0)
                    {
                        float amt = scale*WRS(flags&HIT_WAVE || !hithurts(flags) ? wavepushscale : (d->health <= 0 ? deadpushscale : hitpushscale), push, gamemode, mutators);
                        bool doquake = hithurts(flags) && damage > 0;
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
                        float hit = WF(WK(flags), weap, hitpush, WS(flags))*amt;
                        if(hit != 0)
                        {
                            d->vel.add(vec(dir).mul(hit));
                            if(doquake) d->quake = min(d->quake+max(int(hit), 1), quakelimit);
                        }
                        hit = WF(WK(flags), weap, hitvel, WS(flags))*amt;
                        if(hit != 0) d->vel.add(vec(vel).mul(hit));
                    }
                }
            }
            ai::damaged(d, v, weap, flags, damage);
        }
    }

    void damaged(int weap, int flags, int damage, int health, gameent *d, gameent *v, int millis, vec &dir, vec &vel, float dist)
    {
        if(d->state != CS_ALIVE || !gs_playing(gamestate)) return;
        if(hithurts(flags))
        {
            d->health = health;
            if(damage > 0)
            {
                d->lastregen = 0;
                d->lastpain = lastmillis;
                v->totaldamage += damage;
            }
        }
        hiteffect(weap, flags, damage, d, v, dir, vel, dist, v == player1 || v->ai);
    }

    void killed(int weap, int flags, int damage, gameent *d, gameent *v, vector<gameent *> &log, int style, int material)
    {
        d->lastregen = 0;
        d->lastpain = lastmillis;
        d->state = CS_DEAD;
        d->obliterated = (style&FRAG_OBLITERATE)!=0;
        d->headless = (style&FRAG_HEADSHOT)!=0;
        bool burning = burn(d, weap, flags), bleeding = bleed(d, weap, flags), shocking = shock(d, weap, flags),
             isfocus = d == focus || v == focus, isme = d == player1 || v == player1,
             allowanc = obitannounce && (obitannounce >= 2 || isfocus) && (m_fight(gamemode) || isme) && v->actortype < A_ENEMY;
        int anc = d == focus && allowanc ? S_V_FRAGGED : -1, dth = d->actortype >= A_ENEMY || d->obliterated ? S_SPLOSH : S_DEATH,
            curmat = material&MATF_VOLUME;
        if(d != player1) d->resetinterp();
        if(!isme) loopv(log) if(log[i] == player1)
        {
            isme = true;
            break;
        }
        formatstring(d->obit)("%s ", colourname(d));
        if(d != v && v->lastattacker == d->clientnum) v->lastattacker = -1;
        d->lastattacker = v->clientnum;
        if(d == v)
        {
            if(!actor[d->actortype].living) concatstring(d->obit, obitdestroyed);
            else if(!obitverbose) concatstring(d->obit, obitdied);
            else if(flags&HIT_SPAWN) concatstring(d->obit, obitspawn);
            else if(flags&HIT_SPEC) concatstring(d->obit, obitspectator);
            else if(flags&HIT_MATERIAL && curmat&MAT_WATER) concatstring(d->obit, getobitwater(material, obitdrowned));
            else if(flags&HIT_MATERIAL && curmat&MAT_LAVA) concatstring(d->obit, getobitlava(material, obitmelted));
            else if(flags&HIT_MATERIAL) concatstring(d->obit, *obitdeath ? obitdeath : obitdeathmat);
            else if(flags&HIT_LOST) concatstring(d->obit, *obitfall ? obitfall : obitlost);
            else if(flags && isweap(weap) && !burning && !bleeding && !shocking) concatstring(d->obit, WF(WK(flags), weap, obitsuicide, WS(flags)));
            else if(flags&HIT_BURN || burning) concatstring(d->obit, obitburnself);
            else if(flags&HIT_BLEED || bleeding) concatstring(d->obit, obitbleedself);
            else if(flags&HIT_SHOCK || shocking) concatstring(d->obit, obitshockself);
            else if(d->obliterated) concatstring(d->obit, obitobliterated);
            else if(d->headless) concatstring(d->obit, obitheadless);
            else concatstring(d->obit, obitsuicide);
        }
        else
        {
            concatstring(d->obit, "was ");
            if(!actor[d->actortype].living) concatstring(d->obit, obitdestroyed);
            else if(!obitverbose) concatstring(d->obit, obitfragged);
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
            concatstring(d->obit, " by");
            bool override = false;
            if(d->headless)
            {
                v->addicon(eventicon::HEADSHOT, lastmillis, eventiconfade, 0);
                if(!override && allowanc) anc = S_V_HEADSHOT;
            }
            if(!m_fight(gamemode) || v->actortype >= A_ENEMY)
            {
                concatstring(d->obit, v->actortype >= A_ENEMY ? " a " : " ");
                concatstring(d->obit, colourname(v));
            }
            else if(m_team(gamemode, mutators) && d->team == v->team)
            {
                concatstring(d->obit, " \fs\fzawteam-mate\fS ");
                concatstring(d->obit, colourname(v));
                if(v == focus) { anc = S_ALARM; override = true; }
            }
            else if(obitstyles)
            {
                if(style&FRAG_REVENGE)
                {
                    concatstring(d->obit, " \fs\fzoyvengeful\fS");
                    v->addicon(eventicon::REVENGE, lastmillis, eventiconfade); // revenge
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
                    v->addicon(eventicon::DOMINATE, lastmillis, eventiconfade); // dominating
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
                    concatstring(d->obit, " \fs\fzPwspree-breaking\fS");
                    v->addicon(eventicon::BREAKER, lastmillis, eventiconfade);
                    if(!override && allowanc) anc = S_V_BREAKER;
                }

                if(style&FRAG_MKILL1)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwdouble-killing\fS");
                    v->addicon(eventicon::MULTIKILL, lastmillis, eventiconfade, 0);
                    if(!override && allowanc) anc = S_V_MULTI;
                }
                else if(style&FRAG_MKILL2)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwtriple-killing\fS");
                    v->addicon(eventicon::MULTIKILL, lastmillis, eventiconfade, 1);
                    if(!override && allowanc) anc = S_V_MULTI2;
                }
                else if(style&FRAG_MKILL3)
                {
                    if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                    concatstring(d->obit, " \fs\fzcwmulti-killing\fS");
                    v->addicon(eventicon::MULTIKILL, lastmillis, eventiconfade, 2);
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
                    v->addicon(eventicon::FIRSTBLOOD, lastmillis, eventiconfade, 0);
                    if(allowanc)
                    {
                        anc = S_V_FIRSTBLOOD;
                        override = true;
                    }
                }

                if(style&FRAG_SPREE1)
                {
                    concatstring(d->obit, " in total \fs\fzYwcarnage\fS");
                    v->addicon(eventicon::SPREE, lastmillis, eventiconfade, 0);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE2)
                {
                    concatstring(d->obit, " on a \fs\fzYwslaughter\fS");
                    v->addicon(eventicon::SPREE, lastmillis, eventiconfade, 1);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE2;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE3)
                {
                    concatstring(d->obit, " on a \fs\fzYwmassacre\fS");
                    v->addicon(eventicon::SPREE, lastmillis, eventiconfade, 2);
                    if(!override && allowanc)
                    {
                        anc = S_V_SPREE3;
                        override = true;
                    }
                }
                else if(style&FRAG_SPREE4)
                {
                    concatstring(d->obit, " in a \fs\fzYPbloodbath\fS");
                    v->addicon(eventicon::SPREE, lastmillis, eventiconfade, 3);
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
                defformatstring(obitx)(" \fs\fo@\fy%.2f\fom\fS", v->o.dist(d->o)/8.f);
                concatstring(d->obit, obitx);
            }
            if(showobithpleft >= (d != player1 ? 2 : 1))
            {
                defformatstring(obitx)(" (\fs\fc%d\fS)", v->health);
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
                    defformatstring(obitx)(" (\fs\fc%d\fS)", log[i]->health);
                    concatstring(d->obit, obitx);
                }
            }
        }
        if(d != v)
        {
            if(v->state == CS_ALIVE && d->actortype < A_ENEMY)
            {
                copystring(v->obit, d->obit);
                v->lastkill = totalmillis;
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
                int target = show ? (isme ? CON_SELF : CON_INFO) : -1;
                announcef(anc, target, d, false, "\fw%s", d->obit);
            }
            else if(anc >= 0) announce(anc, d);
            if(anc >= 0 && d != v) announce(anc, v);
        }
        vec pos = d->wantshitbox() ? d->head : d->headpos();
        pos.z -= d->zradius*0.125f;

        if(vanitymodels && d->headless && !nogore && headlessmodels && *d->vanity)
        {
            if(d->vitems.empty()) vanitybuild(d);
            int found[VANITYMAX] = {0};
            loopvk(d->vitems) if(vanities.inrange(d->vitems[k]))
            {
                if(found[vanities[d->vitems[k]].type]) continue;
                if(!(vanities[d->vitems[k]].cond&2)) continue;
                projs::create(pos, pos, true, d, PRJ_VANITY, (rnd(gibfade)+gibfade)*2, 0, 0, rnd(50)+10, -1, d->vitems[k], 0, 0);
                found[vanities[d->vitems[k]].type]++;
            }
        }

        if(actor[d->actortype].living && nogore != 2 && gibscale > 0 && !(flags&HIT_LOST))
        {
            int gib = clamp(max(damage, 10)/20, 1, 10), amt = int((rnd(gib)+gib)*gibscale);
            if(d->obliterated) amt *= 2;
            loopi(amt) projs::create(pos, pos, true, d, nogore ? PRJ_DEBRIS : PRJ_GIBS, rnd(gibfade)+gibfade, 0, rnd(500)+1, rnd(50)+10);
        }
        if(m_team(gamemode, mutators) && d->team == v->team && d != v && v == player1 && isweap(weap) && WF(WK(flags), weap, damagepenalty, WS(flags)))
        {
            hud::teamkills.add(totalmillis);
            if(hud::numteamkills() >= teamkillwarn) hud::lastteam = totalmillis;
        }
        if(m_bomber(gamemode)) bomber::killed(d, v);
        ai::killed(d, v);
    }

    void timeupdate(int state, int remain)
    {
        int oldstate = gamestate;
        gamestate = state;
        timeremaining = remain;
        lasttimeremain = lastmillis;
        if(gs_intermission(gamestate) && gs_playing(oldstate))
        {
            player1->stopmoving(true);
            if(gamestate == G_S_INTERMISSION) hud::showscores(true, true);
            smartmusic(true);
        }
        if(gamestate == G_S_VOTING && oldstate != G_S_VOTING)
        {
            hud::showscores(false);
            showgui("maps", 1);
        }
    }

    gameent *newclient(int cn)
    {
        if(cn < 0 || cn >= MAXPLAYERS)
        {
            defformatstring(cnmsg)("clientnum [%d]", cn);
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

    int numwaiting()
    {
        int n = 0;
        loopv(waiting) if(waiting[i]->state == CS_WAITING && (!m_team(gamemode, mutators) || waiting[i]->team == player1->team)) n++;
        return n;
    }

    void clientdisconnected(int cn, int reason)
    {
        if(!players.inrange(cn)) return;
        gameent *d = players[cn];
        if(!d) return;
        if(d->name[0] && client::showpresence >= (client::waiting(false) ? 2 : 1))
        {
            if(d->actortype == A_PLAYER)
            {
                int amt = client::otherclients(); // not including self to disclude this player
                conoutft(CON_EVENT, "\fo%s (%s) left the game (%s) (%d %s)", colourname(d), d->hostname, reason >= 0 ? disc_reasons[reason] : "normal", amt, amt != 1 ? "players" : "player");
            }
            else if(d->actortype == A_BOT && ai::showaiinfo)
                conoutft(CON_EVENT, "\fo%s was removed from the game (%s)", colourname(d), reason >= 0 ? disc_reasons[reason] : "normal");
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
        projs::remove(d);
        removedamagemerges(d);
        removeannounce(d);
        if(m_capture(gamemode)) capture::removeplayer(d);
        else if(m_defend(gamemode)) defend::removeplayer(d);
        else if(m_bomber(gamemode)) bomber::removeplayer(d);
        DELETEP(players[cn]);
        players[cn] = NULL;
        cleardynentcache();
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
        if(!empty) smartmusic(true);
    }

    void startmap(const char *name, const char *reqname, bool empty)    // called just after a map load
    {
        ai::startmap(name, reqname, empty);
        gamestate = m_fight(gamemode) ? G_S_WAITING : G_S_PLAYING;
        maptime = 0;
        removedamagemergeall();
        removeannounceall();
        projs::reset();
        physics::reset();
        resetworld();
        resetcursor();
        resetsway();
        if(!empty) preload();
        // reset perma-state
        gameent *d;
        int numdyns = numdynents();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) && gameent::is(d)) d->mapchange(lastmillis, gamemode, mutators);
        entities::spawnplayer(player1); // prevent the player from being in the middle of nowhere
        resetcamera();
        if(!empty) client::sendgameinfo = client::sendcrcinfo = true;
        copystring(clientmap, reqname ? reqname : (name ? name : ""));
        if(showloadoutmenu && m_loadout(gamemode, mutators)) wantsloadoutmenu = true;
    }

    gameent *intersectclosest(vec &from, vec &to, gameent *at)
    {
        gameent *best = NULL, *o;
        float bestdist = 1e16f;
        int numdyns = numdynents();
        loopi(numdyns) if((o = (gameent *)iterdynents(i)))
        {
            if(!o || o==at || o->state!=CS_ALIVE || !physics::issolid(o, at)) continue;
            float dist;
            if(intersect(o, from, to, dist) && dist < bestdist)
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
        return i;
    }
    dynent *iterdynents(int i, bool all)
    {
        if(!i) return player1;
        i--;
        if(i<players.length()) return players[i];
        i -= players.length();
        if(all)
        {
            if(i<projs::collideprojs.length()) return projs::collideprojs[i];
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

    int findcolour(gameent *d, bool tone, bool mix)
    {
        if(tone)
        {
            int col = d->actortype >= A_ENEMY ? 0x060606 : d->colour;
            if(!col && isweap(d->weapselect))
            {
                col = W(d->weapselect, colour);
                int lastweap = d->getlastweap(m_weapon(gamemode, mutators));
                if(isweap(lastweap) && d->weapselect != lastweap && (d->weapstate[d->weapselect] == W_S_USE || d->weapstate[d->weapselect] == W_S_SWITCH))
                {
                    float amt = (lastmillis-d->weaplast[d->weapselect])/float(d->weapwait[d->weapselect]);
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
                if(mix)
                {
                    int r1 = (col>>16), g1 = ((col>>8)&0xFF), b1 = (col&0xFF),
                        c = TEAM(d->team, colour), r2 = (c>>16), g2 = ((c>>8)&0xFF), b2 = (c&0xFF),
                        r3 = clamp(int((r1*(1-playertonemix))+(r2*playertonemix)), 0, 255),
                        g3 = clamp(int((g1*(1-playertonemix))+(g2*playertonemix)), 0, 255),
                        b3 = clamp(int((b1*(1-playertonemix))+(b2*playertonemix)), 0, 255);
                    col = (r3<<16)|(g3<<8)|b3;
                }
                return col;
            }
        }
        return TEAM(d->team, colour);
    }

    int getcolour(gameent *d, int level)
    {
        switch(level)
        {
            case -1: return d->colour;
            case CTONE_TMIX: return findcolour(d, true, d->team != T_NEUTRAL); break;
            case CTONE_AMIX: return findcolour(d, true, d->team == T_NEUTRAL); break;
            case CTONE_MIXED: return findcolour(d, true, true); break;
            case CTONE_ALONE: return findcolour(d, d->team != T_NEUTRAL); break;
            case CTONE_TEAMED: return findcolour(d, d->team == T_NEUTRAL); break;
            case CTONE_TONE: return findcolour(d, true); break;
            case CTONE_TEAM: default: return findcolour(d, false); break;
        }
    }

    const char *colourname(gameent *d, char *name, bool icon, bool dupname, int colour)
    {
        if(!name) name = d->name;
        static string colored; colored[0] = 0; string colortmp;
        if(colour) concatstring(colored, "\fs");
        if(icon)
        {
            if(colour&1)
            {
                formatstring(colortmp)("\f[%d]", findcolour(d));
                concatstring(colored, colortmp);
            }
            formatstring(colortmp)("\f($priv%stex)", server::privnamex(d->privilege, d->actortype, true));
            concatstring(colored, colortmp);
        }
        if(colour&2)
        {
            formatstring(colortmp)("\f[%d]", TEAM(d->team, colour));
            concatstring(colored, colortmp);
        }
        concatstring(colored, name);
        if(!name[0] || (d->actortype < A_ENEMY && dupname && duplicatename(d, name)))
        {
            formatstring(colortmp)("%s[%d]", name[0] ? " " : "", d->clientnum);
            concatstring(colored, colortmp);
        }
        if(colour) concatstring(colored, "\fS");
        return colored;
    }

    const char *teamtexnamex(int team)
    {
        const char *teamtexs[T_MAX] = { "teamtex", "teamalphatex", "teamomegatex", "teamkappatex", "teamsigmatex", "teamtex" };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *colourteam(int team, const char *icon)
    {
        if(team < 0 || team > T_MAX) team = T_NEUTRAL;
        static string teamed; teamed[0] = 0; string teamtmp;
        concatstring(teamed, "\fs");
        formatstring(teamtmp)("\f[%d]", TEAM(team, colour));
        concatstring(teamed, teamtmp);
        if(icon != NULL)
        {
            formatstring(teamtmp)("\f($%s)", *icon ? icon : teamtexnamex(team));
            concatstring(teamed, teamtmp);
        }
        concatstring(teamed, TEAM(team, name));
        concatstring(teamed, "\fS");
        return teamed;
    }

    void suicide(gameent *d, int flags)
    {
        if((d == player1 || d->ai) && d->state == CS_ALIVE && d->suicided < 0)
        {
            burn(d, -1, flags);
            client::addmsg(N_SUICIDE, "ri3", d->clientnum, flags, d->inmaterial);
            d->suicided = lastmillis;
        }
    }
    ICOMMAND(0, suicide, "",  (), { suicide(player1, 0); });

    vec rescolour(dynent *d, int c)
    {
        size_t seed = size_t(d) + (lastmillis/50);
        int n = detrnd(seed, 2*PULSECOLOURS), n2 = detrnd(seed + 1, 2*PULSECOLOURS);
        return vec::hexcolor(pulsecols[c][n%PULSECOLOURS]).lerp(vec::hexcolor(pulsecols[c][n2%PULSECOLOURS]), (lastmillis%50)/50.0f);
    }

    void particletrack(particle *p, uint type, int &ts,  bool lastpass)
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
                p->d = p->o = d->muzzlepos(d->weapselect);
                p->d.add(vec(d->yaw*RAD, d->pitch*RAD).mul(dist));
                break;
            }
            case PT_PART: case PT_FIREBALL: case PT_FLARE:
            {
                p->o = d->muzzlepos(d->weapselect);
                break;
            }
            default: break;
        }
    }

    void newmap(int size) { client::addmsg(N_NEWMAP, "ri", size); }

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

    void fixview(int w, int h)
    {
        if(inzoom())
        {
            checkzoom();
            int frame = lastmillis-lastzoom;
            float zoom = zoomlimitmax-((zoomlimitmax-zoomlimitmin)/float(zoomlevels)*zoomlevel),
                  diff = float(fov()-zoom), amt = frame < zoomtime ? clamp(frame/float(zoomtime), 0.f, 1.f) : 1.f;
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

    void project(int w, int h)
    {
        bool input = hud::hasinput(true), view = thirdpersonview(true, focus), mode = tvmode();
        if(input != inputmouse || (view != inputview || mode != inputmode || focus != lastfocus))
        {
            if(input != inputmouse) resetcursor();
            else resetcamera(true);
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
        if(!dist && !side) return c.o = pos;
        vec dir[3];
        if(dist) vecfromyawpitch(yaw, pitch, -1, 0, dir[0]);
        if(side) vecfromyawpitch(yaw, pitch, 0, -1, dir[1]);
        dir[2] = dir[0].mul(dist).add(dir[1].mul(side)).normalize();
        c.o = pos;
        physics::movecamera(&c, dir[2], dist, 0.1f);
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
            if(d == focus && inzoom())
            {
                int frame = lastmillis-lastzoom;
                float pc = frame <= zoomtime ? (frame)/float(zoomtime) : 1.f;
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
        if(dir.iszero()) return c.o;
        float dist = dir.magnitude();
        dir.normalize();
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
        if(deathcamstyle)
        {
            gameent *a = deathcamstyle > 1 ? d : getclient(d->lastattacker);
            if(a)
            {
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
        }
    }

    bool camrefresh(cament *cam, bool renew = false)
    {
        bool rejigger = false;
        if(cam->player && cam->type != cament::PLAYER)
        {
            loopv(cameras) if(cameras[i]->type == cament::PLAYER && cameras[i]->player == cam->player)
            {
                cam = cameras[i];
                rejigger = true;
                break;
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

    vec camvec(cament *c, float amt, float yaw, float pitch)
    {
        if(c->type == cament::AFFINITY) return thirdpos(c->o, yaw, pitch, followdist);
        else if(c->player) return camerapos(c->player, true, true, yaw, pitch);
        else return c->pos(amt);
    }

    bool spectvaiming(gameent *d)
    {
        bool third = d != player1 ? followthirdperson : thirdperson;
        int level = third ? spectvthirdperson : spectvfirstperson;
        if(level >= (d->state == CS_DEAD || d->state == CS_WAITING ? 1 : 2)) return true;
        return false;
    }

    bool camupdate(cament *c, float amt, bool renew = false, bool force = false)
    {
        float foglevel = float(fog*2/3);
        c->reset();
        if(!force && c->player && !allowspec(c->player, spectvdead, spectvfollowing)) return false;
        bool aim = !c->player || spectvaiming(c->player);
        float yaw = c->player ? c->player->yaw : camera1->yaw, pitch = c->player ? c->player->pitch : camera1->pitch;
        fixrange(yaw, pitch);
        vec from = c->pos(amt), dir(0, 0, 0), trg;
        loopj(c->player ? 3 : 2)
        {
            int count = 0;
            loopv(cameras) if(c != cameras[i])
            {
                cament *cam = cameras[i];
                switch(cam->type)
                {
                    case cament::ENTITY: continue;
                    case cament::AFFINITY: if(cam->player == c->player) continue;
                    case cament::PLAYER:
                    {
                        if(!cam->player || c->player == cam->player || !allowspec(cam->player, spectvdead, spectvfollowing)) continue;
                        break;
                    }
                    default: break;
                }
                if(aim && j == (c->player ? 2 : 1) && !count)
                {
                    vectoyawpitch(vec(cam->o).sub(from).normalize(), yaw, pitch);
                    fixrange(yaw, pitch);
                }
                float dist = from.dist(cam->o), fogdist = min(spectvmaxdist, foglevel);
                if(dist >= spectvmindist && getsight(from, yaw, pitch, cam->o, trg, fogdist, curfov, fovy))
                {
                    c->inview[cam->type]++;
                    dir.add(cam->o);
                    count++;
                }
            }
            if(count && !dir.iszero())
            {
                if(c->player) c->inview[cament::PLAYER]++;
                if(c->inview[cament::PLAYER])
                {
                    c->dir = vec(vec(dir).div(count)).sub(from).normalize();
                    return true;
                }
            }
            if(c->player && !j)
            {
                yaw = camera1->yaw;
                pitch = camera1->pitch;
                fixrange(yaw, pitch);
            }
        }
        if(renew || force || c->player)
        {
            yaw = c->player ? c->player->yaw : float(rnd(360));
            pitch = c->player ? c->player->pitch : float(rnd(91)-45);
            fixrange(yaw, pitch);
            c->dir = vec(yaw*RAD, pitch*RAD);
            if(force) return true;
        }
        return false;
    }

    bool cameratv()
    {
        if(!tvmode(false)) return false;
        if(!gs_playing(gamestate)) spectvfollowing = -1;
        else if(player1->state != CS_SPECTATOR && spectvfollowself >= (m_duke(gamemode, mutators) ? 2 : 1))
            spectvfollowing = player1->clientnum;
        else spectvfollowing = spectvfollow;
        if(cameras.empty())
        {
            loopv(entities::ents)
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                if(e.type != PLAYERSTART && e.type != LIGHT && e.type < MAPSOUND) continue;
                cament *c = cameras.add(new cament);
                c->o = e.o;
                c->o.z += max(enttype[e.type].radius, 2);
                c->type = cament::ENTITY;
                c->id = i;
            }
            loopv(players) if(players[i])
            {
                gameent *d = players[i];
                if(d->actortype >= A_ENEMY) continue;
                cament *c = cameras.add(new cament);
                c->o = d->headpos();
                c->type = cament::PLAYER;
                c->id = d->clientnum;
                c->player = d;
            }
            cament *c = cameras.add(new cament);
            c->o = player1->headpos();
            c->type = cament::PLAYER;
            c->id = player1->clientnum;
            c->player = player1;
            if(m_capture(gamemode)) capture::checkcams(cameras);
            else if(m_defend(gamemode)) defend::checkcams(cameras);
            else if(m_bomber(gamemode)) bomber::checkcams(cameras);
        }
        if(cameras.empty()) return false;
        cament *cam = cameras[0];
        bool forced = !tvmode(false, false), renew = !lastcamera, found = spectvfollowing < 0;
        float amt = 0;
        loopv(cameras)
        {
            if(cameras[i]->type == cament::PLAYER && (cameras[i]->player || ((cameras[i]->player = getclient(cameras[i]->id)) != NULL)))
            {
                if(!found && cameras[i]->id == spectvfollowing && cameras[i]->player->state != CS_SPECTATOR) found = true;
                cameras[i]->o = cameras[i]->player->headpos();
                if(forced && cameras[i]->player == focus) cam = cameras[i];
            }
            else if(cameras[i]->type != cament::PLAYER && cameras[i]->player) cameras[i]->player = NULL;
            if(m_capture(gamemode)) capture::updatecam(cameras[i]);
            else if(m_defend(gamemode)) defend::updatecam(cameras[i]);
            else if(m_bomber(gamemode)) bomber::updatecam(cameras[i]);
        }
        if(!found) spectvfollow = spectvfollowing = -1;
        camrefresh(cam);
        #define stvf(z) (!gs_playing(gamestate) ? spectvinterm##z : (spectvfollowing >= 0 ? spectvfollow##z : spectv##z))
        if(forced)
        {
            camupdate(cam, amt, renew, true);
            if(renew)
            {
                lasttvchg = lasttvcam = lastmillis;
                cam->resetlast();
            }
        }
        else loopk(spectvfollowing >= 0 ? 2 : 1)
        {
            int lasttype = cam->type, lastid = cam->id, millis = lasttvchg ? lastmillis-lasttvchg : 0;
            if(millis) amt = float(millis)/float(stvf(maxtime));
            bool updated = camupdate(cam, amt, renew), override = !lasttvchg || millis >= stvf(mintime),
                 reset = (stvf(maxtime) && millis >= stvf(maxtime)) || !lasttvcam || lastmillis-lasttvcam >= stvf(time);
            if(spectvfollowing >= 0 && !reset && !updated && !override)
            {
                spectvfollowing = -1;
                continue;
            }
            if(reset || (!updated && override))
            {
                cam->ignore = true;
                loopv(cameras) if((cameras[i]->player && (cameras[i]->player == cam->player || ((cameras[i]->player->state == CS_DEAD || cameras[i]->player->state == CS_WAITING) && !cameras[i]->player->lastdeath))) || !camupdate(cameras[i], 0, true))
                {
                    cameras[i]->reset();
                    cameras[i]->ignore = true;
                }
                reset = true;
            }
            if(reset)
            {
                cameras.sort(cament::compare);
                loopv(cameras) if(cameras[i]->ignore) cameras[i]->ignore = false;
                cam = cameras[0];
                lasttvcam = lastmillis;
            }
            camrefresh(cam, reset);
            if(!lasttvchg || cam->type != lasttype || cam->id != lastid)
            {
                amt = 0;
                lasttvchg = lastmillis;
                renew = true;
                cam->moveto = NULL;
                cam->resetlast();
                if(cam->type == cament::ENTITY)
                {
                    vector<cament *> mcams;
                    mcams.reserve(cameras.length());
                    mcams.put(cameras.getbuf(), cameras.length());
                    while(mcams.length())
                    {
                        cament *mcam = mcams.removeunordered(rnd(mcams.length()));
                        if(mcam->type == cament::ENTITY)
                        {
                            vec ray = vec(mcam->o).sub(cam->o);
                            float mag = ray.magnitude();
                            ray.mul(1.0f/mag);
                            if(raycube(cam->o, ray, mag, RAY_CLIPMAT|RAY_POLY) >= mag)
                            {
                                cam->moveto = mcam;
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
        bool chase = cam->player && (forced || spectvaiming(cam->player));
        if(!cam->player || chase)
        {
            float yaw = camera1->yaw, pitch = camera1->pitch;
            if(!cam->dir.iszero()) vectoyawpitch(cam->dir, yaw, pitch);
            fixrange(yaw, pitch);
            if(!renew)
            {
                float speed = curtime/float(chase ? followtvspeed : stvf(speed));
                #define SCALEAXIS(x) \
                    float x##scale = 1, adj##x = camera1->x, off##x = x, x##thresh = chase ? followtv##x##thresh : stvf(x##thresh); \
                    if(adj##x < x - 180.0f) adj##x += 360.0f; \
                    if(adj##x > x + 180.0f) adj##x -= 360.0f; \
                    off##x -= adj##x; \
                    if(cam->last##x == 0 || (off##x > 0 && cam->last##x < 0) || (off##x < 0 && cam->last##x > 0) || (x##thresh > 0 && (fabs(cam->last##x - off##x) >= x##thresh))) \
                    { \
                        cam->last##x##time = lastmillis; \
                        x##scale = 0; \
                    } \
                    else if(cam->last##x##time) \
                    { \
                        int offtime = lastmillis-cam->last##x##time, x##speed = chase ? followtv##x##speed : stvf(x##speed); \
                        if(offtime <= x##speed) x##scale = offtime/float(x##speed); \
                    } \
                    cam->last##x = off##x; \
                    float x##speed = speed*(chase ? followtv##x##scale : stvf(x##scale))*x##scale;
                SCALEAXIS(yaw);
                SCALEAXIS(pitch);
                scaleyawpitch(camera1->yaw, camera1->pitch, yaw, pitch, yawspeed, pitchspeed, (chase ? followtvrotate : stvf(rotate)));
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
        camera1->o = camvec(cam, amt, camera1->yaw, camera1->pitch);
        camera1->resetinterp(); // because this just sets position directly
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
                float pc = frame <= zoomtime ? (frame)/float(zoomtime) : 1.f;
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
        cleargui();
    }

    void resetstate()
    {
        resetworld();
        resetcamera();
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
            if(!maptime) { maptime = -1; return; } // skip the first loop
            else if(maptime < 0)
            {
                maptime = lastmillis ? lastmillis : 1;
                if(type != 6) musicdone(false);
                RUNWORLD("on_start");
                resetcamera();
                return;
            }
            else if(!nosound && mastervol && musicvol && type && !playingmusic())
            {
                if(type == 6) smartmusic(true);
                else
                {
                    defformatstring(musicfile)("%s", mapmusic);
                    if(*musicdir && (type == 2 || type == 5 || ((type == 1 || type == 4) && (!*musicfile || !fileexists(findfile(musicfile, "r"), "r")))))
                    {
                        vector<char *> files;
                        listfiles(musicdir, NULL, files);
                        while(!files.empty())
                        {
                            int r = rnd(files.length());
                            formatstring(musicfile)("%s/%s", musicdir, files[r]);
                            if(files[r][0] != '.' && strcmp(files[r], "readme.txt") && playmusic(musicfile, type >= 4 ? "music" : NULL)) break;
                            else files.remove(r);
                        }
                    }
                    else if(*musicfile) playmusic(musicfile, type >= 4 ? "music" : NULL);
                }
            }
            player1->conopen = commandmillis > 0 || hud::hasinput(true);
            checkoften(player1, true);
            loopv(players) if(players[i]) checkoften(players[i], players[i]->ai != NULL);
            if(!allowmove(player1)) player1->stopmoving(player1->state < CS_SPECTATOR);
            if(focus->state == CS_ALIVE && gs_playing(gamestate)) zoomset(focus->zooming(), lastmillis);
            else if(zooming) zoomset(false, 0);

            physics::update();
            ai::navigate();
            projs::update();
            ai::update();
            if(gs_playing(gamestate))
            {
                entities::update();
                if(m_capture(gamemode)) capture::update();
                else if(m_bomber(gamemode)) bomber::update();
                if(player1->state == CS_ALIVE) weapons::shoot(player1, worldpos);
            }
            otherplayers();
            checkannounce();
            flushdamagemerges();
            if(!menuactive())
            {
                if(needname(player1)) showgui("profile", 1);
                else if(wantsloadoutmenu) showgui("profile", 2, &wantsloadoutmenu);
            }
        }
        else if(!menuactive()) showgui(needname(player1) ? "profile" : "main", 1);
        gets2c();
        adjustscaled(hud::damageresidue, hud::damageresiduefade);
        if(connected())
        {
            checkcamera();
            if(player1->state == CS_DEAD || player1->state == CS_WAITING)
            {
                if(player1->ragdoll) moveragdoll(player1, true);
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

        if(player1->clientnum >= 0) client::c2sinfo();
    }

    void recomputecamera(int w, int h)
    {
        fixview(w, h);
        if(!client::waiting())
        {
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
            bool pthird = focus == player1 && thirdpersonview(true, focus);
            if(thirdpersoncursor != 1 && pthird)
            {
                float yaw = camera1->yaw, pitch = camera1->pitch;
                vectoyawpitch(cursordir, yaw, pitch);
                findorientation(camera1->o, yaw, pitch, worldpos);
            }
            else findorientation(focus->o, focus->yaw, focus->pitch, worldpos);
            if(pthird)
            {
                gameent *best = NULL, *o;
                float bestdist = 1e16f;
                int numdyns = numdynents();
                loopi(numdyns) if((o = (gameent *)iterdynents(i)))
                {
                    if(!o || o==focus || o->state!=CS_ALIVE || !physics::issolid(o, focus)) continue;
                    float dist;
                    if(intersect(o, camera1->o, worldpos, dist) && dist < bestdist)
                    {
                        best = o;
                        bestdist = dist;
                    }
                }
                if(best) worldpos = vec(worldpos).sub(camera1->o).normalize().mul(bestdist+best->radius).add(camera1->o);
            }

            camera1->inmaterial = lookupmaterial(camera1->o);
            camera1->inliquid = isliquid(camera1->inmaterial&MATF_VOLUME);
            lastcamera = lastmillis;
        }
    }

    VAR(0, animoverride, -1, 0, ANIM_MAX-1);
    VAR(0, testanims, 0, 0, 1);

    int numanims() { return ANIM_MAX; }

    void findanims(const char *pattern, vector<int> &anims)
    {
        loopi(sizeof(animnames)/sizeof(animnames[0]))
            if(*animnames[i] && matchanim(animnames[i], pattern))
                anims.add(i);
    }

    void renderclient(gameent *d, int third, float trans, float size, int team, modelattach *attachments, bool secondary, int animflags, int animdelay, int lastaction, bool early)
    {
        int idx = third == 1 && d->headless && !nogore && headlessmodels ? 3 : third;
        const char *mdl = playertypes[forceplayermodel >= 0 ? forceplayermodel : 0][idx];
        if(d->actortype >= A_ENEMY && d->actortype != A_GRUNT) mdl = actor[d->actortype%A_MAX].playermodel[idx];
        else if(forceplayermodel < 0) mdl = playertypes[d->model%PLAYERTYPES][idx];

        bool onfloor = d->physstate >= PHYS_SLOPE || d->onladder || physics::liquidcheck(d);
        float yaw = d->yaw, pitch = d->pitch, roll = calcroll(focus);
        vec o = third ? d->feetpos() : camerapos(d);
        if(third == 2)
        {
            o.sub(vec(yaw*RAD, 0.f).mul(firstpersonbodydist+firstpersonspineoffset));
            o.sub(vec(yaw*RAD, 0.f).rotate_around_z(90*RAD).mul(firstpersonbodyside));
            if(firstpersonbodyfeet >= 0 && d->wantshitbox())
            {
                float minz = max(d->toe[0].z, d->toe[1].z)+firstpersonbodyfeet;
                if(minz > camera1->o.z) o.z -= minz-camera1->o.z;
            }
        }
        else if(gs_playing(gamestate))
        {
            if(third == 1 && d == focus && d == player1 && thirdpersonview(true, d))
                vectoyawpitch(vec(worldpos).sub(d->headpos()).normalize(), yaw, pitch);
            else if(!third && firstpersonsway)
            {
                float steps = swaydist/(firstpersonbob ? firstpersonbobstep : firstpersonswaystep)*M_PI;
                vec dir = vec(d->yaw*RAD, 0.f).mul(firstpersonswayside*cosf(steps));
                dir.z = firstpersonswayup*(fabs(sinf(steps)) - 1);
                o.add(dir).add(swaydir).add(swaypush);
            }
        }

        int anim = animflags, basetime = lastaction, basetime2 = 0;
        if(animoverride)
        {
            anim = (animoverride<0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
            basetime = 0;
        }
        else
        {
            bool melee = d->hasmelee(lastmillis, true, d->sliding(true), onfloor);
            if(secondary && allowmove(d) && actor[d->actortype].canmove)
            {
                if(physics::liquidcheck(d) && d->physstate <= PHYS_FALL)
                    anim |= ((d->move || d->strafe || d->vel.z+d->falling.z>0 ? int(ANIM_SWIM) : int(ANIM_SINK))|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->turnside) anim |= ((d->turnside>0 ? ANIM_WALL_RUN_LEFT : ANIM_WALL_RUN_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->physstate == PHYS_FALL && !d->onladder && d->impulse[IM_TYPE] != IM_T_NONE && lastmillis-d->impulse[IM_TIME] <= 1000)
                {
                    basetime2 = d->impulse[IM_TIME];
                    if(d->impulse[IM_TYPE] == IM_T_KICK || d->impulse[IM_TYPE] == IM_T_VAULT) anim |= ANIM_WALL_JUMP<<ANIM_SECONDARY;
                    else if(melee)
                    {
                        anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                        basetime2 = d->weaplast[W_MELEE];
                    }
                    else if(d->move>0) anim |= ANIM_DASH_FORWARD<<ANIM_SECONDARY;
                    else if(d->strafe) anim |= (d->strafe>0 ? ANIM_DASH_LEFT : ANIM_DASH_RIGHT)<<ANIM_SECONDARY;
                    else if(d->move<0) anim |= ANIM_DASH_BACKWARD<<ANIM_SECONDARY;
                    else anim |= ANIM_DASH_UP<<ANIM_SECONDARY;
                }
                else if(d->physstate == PHYS_FALL && !d->onladder && d->airtime(lastmillis) >= 50)
                {
                    basetime2 = max(d->airmillis, d->impulse[IM_JUMP]);
                    if(melee)
                    {
                        anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                        basetime2 = d->weaplast[W_MELEE];
                    }
                    else if(d->action[AC_CROUCH] || d->actiontime[AC_CROUCH]<0)
                    {
                        if(d->move>0) anim |= ANIM_CROUCH_JUMP_FORWARD<<ANIM_SECONDARY;
                        else if(d->strafe) anim |= (d->strafe>0 ? ANIM_CROUCH_JUMP_LEFT : ANIM_CROUCH_JUMP_RIGHT)<<ANIM_SECONDARY;
                        else if(d->move<0) anim |= ANIM_CROUCH_JUMP_BACKWARD<<ANIM_SECONDARY;
                        else anim |= ANIM_CROUCH_JUMP<<ANIM_SECONDARY;
                    }
                    else if(d->move>0) anim |= ANIM_JUMP_FORWARD<<ANIM_SECONDARY;
                    else if(d->strafe) anim |= (d->strafe>0 ? ANIM_JUMP_LEFT : ANIM_JUMP_RIGHT)<<ANIM_SECONDARY;
                    else if(d->move<0) anim |= ANIM_JUMP_BACKWARD<<ANIM_SECONDARY;
                    else anim |= ANIM_JUMP<<ANIM_SECONDARY;
                    if(!basetime2) anim |= ANIM_END<<ANIM_SECONDARY;
                }
                else if(d->sliding(true)) anim |= (ANIM_POWERSLIDE|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->crouching(true))
                {
                    if(d->move>0) anim |= (ANIM_CRAWL_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->strafe) anim |= ((d->strafe>0 ? ANIM_CRAWL_LEFT : ANIM_CRAWL_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move<0) anim |= (ANIM_CRAWL_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->running(moveslow))
                {
                    if(d->move>0) anim |= (ANIM_IMPULSE_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->strafe) anim |= ((d->strafe>0 ? ANIM_IMPULSE_LEFT : ANIM_IMPULSE_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(d->move<0) anim |= (ANIM_IMPULSE_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->move>0) anim |= (ANIM_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->strafe) anim |= ((d->strafe>0 ? ANIM_LEFT : ANIM_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->move<0) anim |= (ANIM_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
            }

            if((anim>>ANIM_SECONDARY)&ANIM_INDEX) switch(anim&ANIM_INDEX)
            {
                case ANIM_IDLE: case ANIM_MELEE: case ANIM_PISTOL: case ANIM_SWORD:
                case ANIM_SHOTGUN: case ANIM_SMG: case ANIM_FLAMER: case ANIM_PLASMA: case ANIM_ZAPPER:
                case ANIM_RIFLE: case ANIM_GRENADE: case ANIM_MINE: case ANIM_ROCKET:
                {
                    anim = (anim>>ANIM_SECONDARY) | ((anim&((1<<ANIM_SECONDARY)-1))<<ANIM_SECONDARY);
                    swap(basetime, basetime2);
                    break;
                }
                default: break;
            }
        }

        if(third == 1 && testanims && d == focus) yaw = 0; else yaw += 90;
        if(d->ragdoll && (deathanim!=2 || (anim&ANIM_INDEX)!=ANIM_DYING)) cleanragdoll(d);
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;

        int flags = MDL_LIGHT|MDL_LIGHTFX;
        if(d->actortype >= A_ENEMY) flags |= MDL_CULL_DIST;
        if(d != focus || (d != player1 ? fullbrightfocus&1 : fullbrightfocus&2)) flags |= MDL_FULLBRIGHT;
        if(d != focus && !(anim&ANIM_RAGDOLL)) flags |= MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_CULL_QUERY;
        if(early) flags |= MDL_NORENDER;
        else if(third && (anim&ANIM_INDEX)!=ANIM_DEAD) flags |= MDL_DYNSHADOW;
        if(modelpreviewing) flags &= ~(MDL_LIGHT|MDL_FULLBRIGHT|MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_CULL_QUERY|MDL_CULL_DIST|MDL_DYNSHADOW);
        dynent *e = third ? (third != 2 ? (dynent *)d : (dynent *)&bodymodel) : (dynent *)&avatarmodel;
        if(e->light.millis != lastmillis)
        {
            e->light.effect = vec::hexcolor(getcolour(d, playerlighttone)).mul(playerlightmix);
            e->light.material[0] = bvec(getcolour(d, playerovertone));
            e->light.material[1] = bvec(getcolour(d, playerundertone));
            if(renderpath != R_FIXEDFUNCTION && isweap(d->weapselect) && (W2(d->weapselect, ammosub, false) || W2(d->weapselect, ammosub, true)) && W(d->weapselect, ammomax) > 1)
            {
                int ammo = d->ammo[d->weapselect], maxammo = W(d->weapselect, ammomax);
                float scale = 1;
                switch(d->weapstate[d->weapselect])
                {
                    case W_S_RELOAD:
                    {
                        int millis = lastmillis-d->weaplast[d->weapselect], check = d->weapwait[d->weapselect]/2;
                        scale = millis >= check ? float(millis-check)/check : 0.f;
                        if(d->weapload[d->weapselect] > 0)
                            scale = max(scale, float(ammo - d->weapload[d->weapselect])/maxammo);
                        break;
                    }
                    default: scale = float(ammo)/maxammo; break;
                }
                uchar wepmat = uchar(255*scale);
                e->light.material[2] = bvec(wepmat, wepmat, wepmat);
            }
            else e->light.material[2] = bvec(255, 255, 255);
            if(burntime && d->burning(lastmillis, burntime))
            {
                vec col = rescolour(d, PULSE_BURN);
                e->light.material[1] = bvec::fromcolor(e->light.material[1].tocolor().max(col));
                e->light.effect.max(col);
            }
            if(shocktime && d->shocking(lastmillis, shocktime))
            {
                vec col = rescolour(d, PULSE_SHOCK);
                e->light.material[1] = bvec::fromcolor(e->light.material[1].tocolor().max(col));
                e->light.effect.max(col);
            }
            if(d->state == CS_ALIVE && bleedtime && d->bleeding(lastmillis, bleedtime))
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f);
                flashcolour(e->light.material[1].r, e->light.material[1].g, e->light.material[1].b, uchar(255), uchar(52), uchar(52), amt);
                flashcolour(e->light.effect.r, e->light.effect.g, e->light.effect.b, 1.f, 0.2f, 0.2f, amt);
            }
            if(d->state == CS_ALIVE && d->lastbuff)
            {
                int millis = lastmillis%1000;
                float amt = millis <= 500 ? 1.f-(millis/500.f) : (millis-500)/500.f;
                flashcolour(e->light.material[1].r, e->light.material[1].g, e->light.material[1].b, uchar(192), uchar(192), uchar(192), amt);
                flashcolour(e->light.effect.r, e->light.effect.g, e->light.effect.b, 1.f, 1.f, 1.f, amt);
            }
            if(m_bomber(gamemode) && bomber::carryaffinity(d))
            {
                vec col = rescolour(d, PULSE_DISCO);
                e->light.material[1] = bvec::fromcolor(e->light.material[1].tocolor().max(col));
                e->light.effect.max(col);
            }
        }
        rendermodel(NULL, mdl, anim, o, yaw, third == 2 && firstpersonbodypitch >= 0 ? pitch*firstpersonbodypitch : pitch, third == 2 ? 0.f : roll, flags, e, attachments, basetime, basetime2, trans, size);
    }

    void renderabovehead(gameent *d, float trans)
    {
        vec pos = d->abovehead(d->state != CS_DEAD && d->state != CS_WAITING ? 1 : 0);
        float blend = aboveheadblend*trans;
        if(aboveheadnames && d != player1)
        {
            pos.z += aboveheadnamessize/2;
            part_textcopy(pos, colourname(d), PART_TEXT, 1, 0xFFFFFF, aboveheadnamessize, blend*aboveheadnamesblend);
        }
        if(aboveheadinventory && d != player1)
        {
            string weapons = "";
            #define printweapon(q) \
                if((q != W_MELEE || q == d->weapselect) && d->hasweap(q, m_weapon(gamemode, mutators))) \
                { \
                    vec colour = vec::hexcolor(W(q, colour)); \
                    if(q != d->weapselect) colour.mul(aboveheadinventoryfade); \
                    defformatstring(str)("\fs\f[%d]%s\f(%s)\fS", colour.tohexcolor(), q != d->weapselect ? "\fE" : "", hud::itemtex(WEAPON, q)); \
                    concatstring(weapons, str); \
                }
            if(aboveheadinventory >= 2) { loopi(W_MAX) printweapon(i); }
            else { printweapon(d->weapselect); }
            pos.z += aboveheadinventorysize/2;
            part_textcopy(pos, weapons, PART_TEXT, 1, 0xFFFFFF, aboveheadinventorysize, blend*aboveheadinventoryblend);
        }
        if(aboveheadstatus)
        {
            Texture *t = NULL;
            int colour = getcolour(d, playerteamtone);
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
                        colour = pulsecols[PULSE_DISCO][clamp((lastmillis/100)%PULSECOLOURS, 0, PULSECOLOURS-1)];
                    }
                    else if(d->dominated.find(focus) >= 0)
                    {
                        t = textureload(hud::dominatedtex, 3);
                        colour = pulsecols[PULSE_DISCO][clamp((lastmillis/100)%PULSECOLOURS, 0, PULSECOLOURS-1)];
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
            int millis = lastmillis-d->icons[i].millis;
            if(millis <= d->icons[i].fade)
            {
                Texture *t = textureload(hud::icontex(d->icons[i].type, d->icons[i].value));
                if(t && t != notexture)
                {
                    int olen = min(d->icons[i].length/5, 1000), ilen = olen/2, colour = 0xFFFFFF;
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

    void renderplayer(gameent *d, int third, float trans, float size, bool early = false)
    {
        if(d->state == CS_SPECTATOR) return;
        if(trans <= 0.f || (d == focus && !(third == 1 ? thirdpersonmodel : firstpersonmodel)))
        {
            if(d->state == CS_ALIVE && rendernormally && (early || d != focus))
                trans = 1e-16f; // we need tag_muzzle/tag_waist
            else return; // screw it, don't render them
        }
        int team = m_fight(gamemode) && m_team(gamemode, mutators) ? d->team : T_NEUTRAL,
            weap = d->weapselect, lastaction = 0, animflags = ANIM_IDLE|ANIM_LOOP, weapflags = animflags, weapaction = 0, animdelay = 0;
        bool secondary = false, showweap = third != 2 && isweap(weap) && actor[d->actortype].useweap;
        float weapscale = 1.f;
        if(d->state == CS_DEAD || d->state == CS_WAITING)
        {
            showweap = false;
            animflags = ANIM_DYING|ANIM_NOPITCH;
            lastaction = d->lastpain;
            switch(deathanim)
            {
                case 0: return;
                case 1:
                {
                    int t = lastmillis-lastaction;
                    if(t < 0) return;
                    if(t > 1000) animflags = ANIM_DEAD|ANIM_LOOP|ANIM_NOPITCH;
                    break;
                }
                case 2:
                {
                    if(!validragdoll(d, lastaction)) animflags |= ANIM_RAGDOLL;
                    break;
                }
            }
        }
        else if(d->state == CS_EDITING)
        {
            animflags = ANIM_EDIT|ANIM_LOOP;
            showweap = false;
        }
        else
        {
            secondary = third;
            if(showweap && isweap(weap))
            {
                weapaction = lastaction = d->weaplast[weap];
                animdelay = d->weapwait[weap];
                switch(d->weapstate[weap])
                {
                    case W_S_SWITCH: case W_S_USE:
                    {
                        int millis = lastmillis-d->weaplast[weap], off = d->weapwait[weap]/3, lastweap = d->getlastweap(m_weapon(gamemode, mutators));
                        if(isweap(lastweap) && millis <= off)
                        {
                            weap = lastweap;
                            weapscale = 1-(millis/float(off));
                        }
                        else if(!d->hasweap(weap, m_weapon(gamemode, mutators))) showweap = false;
                        else if(millis <= off*2) weapscale = (millis-off)/float(off);
                        weapflags = animflags = d->weapstate[weap] == W_S_SWITCH ? ANIM_SWITCH : ANIM_USE;
                        break;
                    }
                    case W_S_POWER: case W_S_ZOOM:
                    {
                        switch(weaptype[weap].anim)
                        {
                            case ANIM_GRENADE: weapflags = animflags = ANIM_GRENADE_POWER; break;
                            default: weapflags = animflags = weaptype[weap].anim|ANIM_LOOP; break;
                        }
                        break;
                    }
                    case W_S_PRIMARY: case W_S_SECONDARY:
                    {
                        if(weaptype[weap].thrown[d->weapstate[weap]-W_S_PRIMARY] > 0)
                        {
                            int millis = lastmillis-d->weaplast[weap], off = d->weapwait[weap]/2;
                            if(millis <= off || !d->hasweap(weap, m_weapon(gamemode, mutators)))
                                showweap = false;
                            else weapscale = (millis-off)/float(off);
                        }
                        weapflags = animflags = (weaptype[weap].anim+d->weapstate[weap])|ANIM_CLAMP;
                        break;
                    }
                    case W_S_RELOAD:
                    {
                        if(!d->hasweap(weap, m_weapon(gamemode, mutators))) showweap = false;
                        weapflags = animflags = weaptype[weap].anim+d->weapstate[weap];
                        break;
                    }
                    case W_S_IDLE: case W_S_WAIT: default:
                    {
                        if(!d->hasweap(weap, m_weapon(gamemode, mutators))) showweap = false;
                        weapflags = animflags = weaptype[weap].anim|ANIM_LOOP;
                        break;
                    }
                }
            }
            if(third && (animflags&ANIM_IDLE) && lastmillis-d->lastpain <= 300)
            {
                secondary = true;
                lastaction = d->lastpain;
                animflags = ANIM_PAIN;
                animdelay = 300;
            }
        }
        if(!early && third == 1 && d->actortype < A_ENEMY && !shadowmapping && !envmapping) renderabovehead(d, trans);
        const char *weapmdl = showweap && isweap(weap) ? (third ? weaptype[weap].vwep : weaptype[weap].hwep) : "";
        int ai = 0;
        modelattach a[1+VANITYMAX+12];
        if(vanitymodels && third && *d->vanity)
        {
            int idx = third == 1 && (d->state == CS_DEAD || d->state == CS_WAITING) && d->headless && !nogore && headlessmodels ? 3 : third;
            if(d->vitems.empty())
            {
                vector<char *> vanitylist;
                explodelist(d->vanity, vanitylist);
                loopv(vanitylist) if(vanitylist[i] && *vanitylist[i])
                    loopvk(vanities) if(!strcmp(vanities[k].ref, vanitylist[i]))
                        d->vitems.add(k);
                vanitylist.deletearrays();
            }
            int found[VANITYMAX] = {0};
            loopvk(d->vitems) if(vanities.inrange(d->vitems[k]))
            {
                if(found[vanities[d->vitems[k]].type]) continue;
                if(vanities[d->vitems[k]].cond&1 && idx == 2) continue;
                if(vanities[d->vitems[k]].cond&2 && idx == 3) continue;
                const char *file = vanityfname(d, d->vitems[k]);
                if(file)
                {
                    a[ai++] = modelattach(vanities[d->vitems[k]].tag, file);
                    found[vanities[d->vitems[k]].type]++;
                }
            }
        }
        bool hasweapon = showweap && *weapmdl;
        if(hasweapon) a[ai++] = modelattach("tag_weapon", weapmdl, weapflags, weapaction, trans, weapscale*size);
        if(rendernormally && (early || d != focus))
        {
            if(third != 2)
            {
                a[ai++] = modelattach(hasweapon ? "tag_muzzle" : "tag_weapon", &d->muzzle);
                a[ai++] = modelattach("tag_weapon", &d->origin);
                if(weaptype[weap].eject || weaptype[weap].tape)
                {
                    a[ai++] = modelattach("tag_eject", &d->eject[0]);
                    a[ai++] = modelattach("tag_eject2", &d->eject[1]);
                }
            }
            if(third && d->wantshitbox())
            {
                a[ai++] = modelattach("tag_head", &d->head);
                a[ai++] = modelattach("tag_torso", &d->torso);
                a[ai++] = modelattach("tag_waist", &d->waist);
                a[ai++] = modelattach("tag_ljet", &d->jet[0]);
                a[ai++] = modelattach("tag_rjet", &d->jet[1]);
                a[ai++] = modelattach("tag_bjet", &d->jet[2]);
                a[ai++] = modelattach("tag_ltoe", &d->toe[0]);
                a[ai++] = modelattach("tag_rtoe", &d->toe[1]);
            }
        }
        renderclient(d, third, trans, size, team, a[0].tag ? a : NULL, secondary, animflags, animdelay, lastaction, early);
    }

    void rendercheck(gameent *d, bool third = false)
    {
        d->checktags();
        float blend = opacity(d, third);
        if(d->state == CS_ALIVE)
        {
            bool useth = hud::teamhurthud&1 && hud::teamhurttime && m_team(gamemode, mutators) && focus == player1 &&
                 d->team == player1->team && d->lastteamhit >= 0 && lastmillis-d->lastteamhit <= hud::teamhurttime,
                 hashint = playerhint&(d->team != focus->team ? 2 : 1);
            if(d->actortype < A_ENEMY && d != focus && (useth || hashint))
            {
                if(hashint)
                {
                    vec c = vec::hexcolor(getcolour(d, playerhinttone));
                    float height = d->height, fade = blend;
                    if(playerhintscale > 0)
                    {
                        float per = d->health/float(m_health(gamemode, mutators, d->model));
                        fade = (fade*(1.f-playerhintscale))+(fade*per*playerhintscale);
                        if(fade > 1)
                        {
                            height *= 1.f+(fade-1.f);
                            fade = 1;
                        }
                    }
                    if(d->state == CS_ALIVE && d->lastbuff)
                    {
                        int millis = lastmillis%1000;
                        float amt = millis <= 500 ? 1.f-(millis/500.f) : (millis-500)/500.f;
                        flashcolour(c.r, c.g, c.b, 1.f, 1.f, 1.f, amt);
                        height += height*amt*0.1f;
                    }
                    vec o = d->center(), offset = vec(o).sub(camera1->o).rescale(d->radius/2);
                    offset.z = max(offset.z, -1.0f);
                    offset.add(o);
                    part_create(PART_HINT_BOLD_SOFT, 1, offset, c.tohexcolor(), clamp(height*playerhintsize, 1.f, playerhintmaxsize), fade*playerhintblend*camera1->o.distrange(o, playerhintfadeat, playerhintfadecut));
                }
                if(useth)
                {
                    vec c(0.25f, 0.25f, 0.25f);
                    int millis = lastmillis%500;
                    float amt = millis <= 250 ? 1.f-(millis/250.f) : (millis-250)/250.f, height = d->height*0.5f;
                    flashcolour(c.r, c.g, c.b, 1.f, 0.f, 0.f, amt);
                    if(playerhinthurtthrob) height += height*amt*0.1f;
                    vec o = d->center(), offset = vec(o).sub(camera1->o).rescale(-d->radius);
                    offset.z = max(offset.z, -1.0f);
                    offset.add(o);
                    part_icon(offset, textureload(hud::warningtex, 3, true, false), height*playerhinthurtsize, amt*blend*playerhinthurtblend, 0, 0, 1, c.tohexcolor());
                }
            }
            float minz = d == focus && !third && firstpersonbodyfeet >= 0 && d->wantshitbox() ? camera1->o.z-firstpersonbodyfeet : 0.f;
            if(d->hasmelee(lastmillis, true, d->sliding(true), d->physstate >= PHYS_SLOPE || d->onladder || physics::liquidcheck(d))) loopi(2)
            {
                vec pos = d->footpos(i);
                if(minz > 0 && pos.z > minz) pos.z -= pos.z-minz;
                float amt = 1-((lastmillis-d->weaplast[W_MELEE])/float(d->weapwait[W_MELEE]));
                part_create(PART_HINT, 1, pos, TEAM(d->team, colour), 2.f, amt*blend, 0, 0);
            }
            int millis = lastmillis-d->weaplast[d->weapselect];
            bool last = millis > 0 && millis < d->weapwait[d->weapselect],
                 powering = last && d->weapstate[d->weapselect] == W_S_POWER,
                 reloading = last && d->weapstate[d->weapselect] == W_S_RELOAD,
                 secondary = physics::secondaryweap(d);
            float amt = last ? (lastmillis-d->weaplast[d->weapselect])/float(d->weapwait[d->weapselect]) : 1.f;
            int colour = WHCOL(d, d->weapselect, partcol, secondary);
            if(d->weapselect == W_FLAMER && (!reloading || amt > 0.5f) && !physics::liquidcheck(d))
            {
                float scale = powering ? 1.f+(amt*1.5f) : (d->weapstate[d->weapselect] == W_S_IDLE ? 1.f : (reloading ? (amt-0.5f)*2 : amt));
                part_create(PART_HINT, 1, d->ejectpos(d->weapselect), 0x1818A8, 0.75f*scale, min(0.65f*scale, 0.8f)*blend, 0, 0);
                part_create(PART_FIREBALL, 1, d->ejectpos(d->weapselect), colour, 0.5f*scale, min(0.75f*scale, 0.95f)*blend, 0, 0);
                regular_part_create(PART_FIREBALL, d->vel.magnitude() > 10 ? 30 : 75, d->ejectpos(d->weapselect), colour, 0.5f*scale, min(0.75f*scale, 0.95f)*blend, d->vel.magnitude() > 10 ? -40 : -10, 0);
            }
            if(W(d->weapselect, laser) && !reloading)
            {
                vec v, origin = d->originpos(), muzzle = d->muzzlepos(d->weapselect);
                origin.z += 0.25f; muzzle.z += 0.25f;
                float yaw, pitch;
                vectoyawpitch(vec(muzzle).sub(origin).normalize(), yaw, pitch);
                findorientation(d->o, d->yaw, d->pitch, v);
                part_flare(origin, v, 1, PART_FLARE, colour, 0.5f*amt, amt*blend);
            }
            if(d->weapselect == W_SWORD || d->weapselect == W_ZAPPER || powering)
            {
                static const struct powerfxs {
                    int type, parttype;
                    float size, radius;
                } powerfx[W_MAX] = {
                    { 0, 0, 0, 0 },
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
                };
                switch(powerfx[d->weapselect].type)
                {
                    case 1: case 2:
                    {
                        regularshape(powerfx[d->weapselect].parttype, 1+(amt*powerfx[d->weapselect].radius), colour, powerfx[d->weapselect].type == 2 ? 21 : 53, 5, 60+int(30*amt), d->muzzlepos(d->weapselect), powerfx[d->weapselect].size*max(amt, 0.25f), max(amt, 0.1f)*blend, 1, 0, 5+(amt*5));
                        break;
                    }
                    case 3:
                    {
                        int interval = lastmillis%1000;
                        float fluc = powerfx[d->weapselect].size+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(powerfx[d->weapselect].parttype, 1, d->originpos(), colour, (powerfx[d->weapselect].radius*max(amt, 0.25f))+fluc, max(amt, 0.1f)*blend);
                        break;
                    }
                    case 4:
                    {
                        part_flare(d->ejectpos(d->weapselect), d->ejectpos(d->weapselect, true), 1, powerfx[d->weapselect].parttype, colour, powerfx[d->weapselect].size*max(amt, 0.25f), max(amt, 0.1f)*blend);
                        break;
                    }
                    case 0: default: break;
                }
            }
            if(d->turnside || d->impulse[IM_JUMP] || d->sliding(true)) impulseeffect(d, 1);
        }
        if(burntime && d->burning(lastmillis, burntime))
        {
            int millis = lastmillis-d->lastres[WR_BURN];
            float pc = 1, intensity = 0.5f+(rnd(50)/100.f), fade = (d != focus ? 0.5f : 0.f)+(rnd(50)/100.f);
            if(burntime-millis < burndelay) pc *= float(burntime-millis)/float(burndelay);
            else pc *= 0.75f+(float(millis%burndelay)/float(burndelay*4));
            vec pos = vec(d->center()).sub(vec(rnd(11)-5, rnd(11)-5, rnd(5)-2).mul(pc));
            regular_part_create(PART_FIREBALL, 50, pos, pulsecols[PULSE_FIRE][rnd(PULSECOLOURS)], d->height*0.75f*intensity*blend*pc, fade*blend*pc, -10, 0);
        }
        if(shocktime && d->shocking(lastmillis, shocktime))
        {
            vec origin = d->center(), col = rescolour(d, PULSE_SHOCK), rad = vec(d->xradius, d->yradius, d->height/(d->state == CS_ALIVE ? 2 : 3)).mul(blend);
            int colour = (int(col.x*255)<<16)|(int(col.y*255)<<8)|(int(col.z*255));
            float fade = blend*(d != focus ? 1.f : 0.5f);
            loopi(3+rnd(10))
            {
                float q = 0.75f;
                vec from = vec(origin).add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).div(100.f).normalize().mul(rad).mul(rnd(200)/100.f)), to = from;
                loopj(1+rnd(3))
                {
                    to = vec(from).add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).div(100.f).normalize().mul(rad).mul(rnd(200)/100.f*q));
                    part_flare(from, to, 1, PART_LIGHTNING_FLARE, colour, q, fade*q);
                    from = to;
                    q = q*0.75f;
                }
            }
            if(rendernormally && d->ragdoll && twitchspeed > 0) twitchragdoll(d, twitchspeed*blend*rnd(100)/80.f);
        }
    }

    void render()
    {
        startmodelbatches();
        gameent *d;
        int numdyns = numdynents();
        loopi(numdyns) if((d = (gameent *)iterdynents(i)) && d != focus) renderplayer(d, 1, opacity(d, true), d->curscale);
        entities::render();
        projs::render();
        if(m_capture(gamemode)) capture::render();
        else if(m_defend(gamemode)) defend::render();
        else if(m_bomber(gamemode)) bomber::render();
        ai::render();
        if(rendernormally) loopi(numdyns) if((d = (gameent *)iterdynents(i)) && d != focus) d->cleartags();
        endmodelbatches();
        if(rendernormally) loopi(numdyns) if((d = (gameent *)iterdynents(i)) && d != focus) rendercheck(d);
    }

    void renderavatar(bool early, bool project)
    {
        bool third = thirdpersonview();
        if(rendernormally && early) focus->cleartags();
        if(project && !third) viewproject(firstpersondepth);
        if(third || !rendernormally) renderplayer(focus, 1, opacity(focus, thirdpersonview(true)), focus->curscale, early);
        else if(!third && focus->state == CS_ALIVE) renderplayer(focus, 0, opacity(focus, false), focus->curscale, early);
        if(project && !third) viewproject();
        if(!third && focus->state == CS_ALIVE && firstpersonmodel == 2)
        {
            if(project) viewproject(firstpersonbodydepth);
            renderplayer(focus, 2, opacity(focus, false), focus->curscale, early);
            if(project) viewproject();
        }
        if(rendernormally && early) rendercheck(focus, third);
    }

    void renderplayerpreview(int model, int color, int team, int weap, const char *vanity, float scale, float blend)
    {
        static gameent *previewent = NULL;
        if(!previewent)
        {
            previewent = new gameent;
            previewent->state = CS_ALIVE;
            previewent->physstate = PHYS_FLOOR;
            previewent->o = vec(0, 0.75f*(previewent->height + previewent->aboveeye), previewent->height - (previewent->height + previewent->aboveeye)/2);
            previewent->spawnstate(G_DEATHMATCH, 0, -1, m_health(G_DEATHMATCH, 0, 0));
            previewent->light.color = vec(1, 1, 1);
            previewent->light.dir = vec(0, -1, 2).normalize();
            loopi(W_MAX) previewent->ammo[i] = W(i, ammomax);
        }
        previewent->colour = color;
        previewent->model = model;
        previewent->team = clamp(team, 0, int(T_MULTI));
        previewent->weapselect = clamp(weap, 0, W_MAX-1);
        previewent->setvanity(vanity);
        previewent->yaw = fmod(lastmillis/10000.0f*360.0f, 360.0f);
        previewent->light.millis = -1;
        renderplayer(previewent, 1, blend, scale);
    }

    bool clientoption(char *arg) { return false; }
}
#undef GAMEWORLD
