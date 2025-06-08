#define CPP_GAME_MAIN 1
#include "game.h"

namespace game
{
    int nextmode = G_EDITING, nextmuts = 0, gamestate = G_S_WAITING, gamemode = G_EDITING, mutators = 0,
        maptime = 0, mapstart = 0, timeremaining = 0, timeelapsed = 0, timelast = 0, timewait = 0, timesync = 0,
        lastcamera = 0, lasttvcam = 0, lasttvchg = 0, lastzoom = 0, lastcamcn = -1;
    bool zooming = false, inputmouse = false, inputview = false, inputmode = false, wantsloadoutmenu = false, hasspotlights = false;
    float swayfade = 0, swayspeed = 0, swaydist = 0, bobfade = 0, bobdist = 0;
    vec swaydir(0, 0, 0), swaypush(0, 0, 0);

    int attrmap[W_MAX] = {0};
    ICOMMAND(0, getattrmap, "i", (int *i), intret(*i >= 0 && *i < W_MAX ? attrmap[*i] : -1));

    gameent *player1 = new gameent(), *focus = player1, *lastfocus = focus, *previewent = NULL;
    avatarent avatarmodel, bodymodel;
    vector<gameent *> players, waiting;
    vector<cament *> cameras;

    VAR(IDF_PERSIST, playersoundpos, 0, 2, 2);

    vec *getplayersoundpos(physent *d)
    {
        static vec nullvec = vec(0);
        if(d && gameent::is(d) && d == focus && playersoundpos >= (thirdpersonview(true) ? 2 : 1))
            return &camera1->o;
        return d ? &d->o : &nullvec;
    }

    VAR(0, showweapfx, 0, 1, 1);

    void mapweapsounds()
    {
        static const char *nameprefixes[W_MAX] =
        {
            "S_W_CLAW_", "S_W_PISTOL_",
            "S_W_SWORD_", "S_W_SHOTGUN_", "S_W_SMG_", "S_W_FLAMER_", "S_W_PLASMA_", "S_W_ZAPPER_", "S_W_RIFLE_", "S_W_CORRODER_",
            "S_W_GRENADE_", "S_W_MINE_", "S_W_ROCKET_", "S_W_MINIGUN_", "S_W_JETSAW_", "S_W_ECLIPSE_", "S_W_MELEE_"
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

    int getweapsound(int weap, int sound) { return isweap(weap) ? WSND(weap, sound) : -1; }
    int getweapcolor(int weap) { return isweap(weap) ? W(weap, colour) : 0xFFFFFF; }

    void mapgamesounds()
    {
        static const char *names[S_GAME - S_GAMESPECIFIC] =
        {
            "S_JUMP", "S_IMPULSE", "S_IMPULSE_ACTION", "S_IMPULSE_SLIDE", "S_LAND", "S_FOOTSTEP_L", "S_FOOTSTEP_R", "S_SWIMSTEP", "S_PAIN", "S_DEATH",
            "S_SPLASH1", "S_SPLASH2", "S_SPLOSH", "S_DEBRIS", "S_BURNLAVA",
            "S_EXTINGUISH", "S_SHELL", "S_ITEMUSE", "S_ITEMSPAWN",
            "S_REGEN_BEGIN", "S_REGEN_BOOST", "S_REGEN_DECAY", "S_CRITICAL", "S_DAMAGE_TICK", "S_DAMAGE", "S_DAMAGE2", "S_DAMAGE3", "S_DAMAGE4", "S_DAMAGE5", "S_DAMAGE6", "S_DAMAGE7", "S_DAMAGE8",
            "S_BURNED", "S_BLEED", "S_SHOCK", "S_CORRODE", "S_RESPAWN", "S_CHAT", "S_ERROR", "S_ALARM", "S_PRIZELOOP", "S_OPENPRIZE", "S_CATCH", "S_DROP", "S_BOUNCE"
        };

        for(int i = S_GAMESPECIFIC; i < S_GAME; i++)
            mapsoundslot(i, names[i - S_GAMESPECIFIC]);

        mapweapsounds();
    }

    fx::FxHandle getweapfx(int type)
    {
        static fx::FxHandle weapfx[FX_W_TYPES] =
        {
            fx::getfxhandle("FX_W_MUZZLE1"),
            fx::getfxhandle("FX_W_MUZZLE2"),
            fx::getfxhandle("FX_W_MUZZLE3"),
            fx::getfxhandle("FX_W_MUZZLE4"),
            fx::getfxhandle("FX_W_MUZZLE5"),
            fx::getfxhandle("FX_W_MUZZLE6"),
            fx::getfxhandle("FX_W_FLAME"),
            fx::getfxhandle("FX_W_AIRBLAST"),
            fx::getfxhandle("FX_W_PLASMA1"),
            fx::getfxhandle("FX_W_PLASMA2"),
            fx::getfxhandle("FX_W_PLASMA_P"),
            fx::getfxhandle("FX_W_ENERGY1"),
            fx::getfxhandle("FX_W_ENERGY2"),
            fx::getfxhandle("FX_W_ENERGY_P"),
            fx::getfxhandle("FX_W_BEAM1"),
            fx::getfxhandle("FX_W_BEAM2"),
            fx::getfxhandle("FX_W_SPLASH1"),
            fx::getfxhandle("FX_W_SPLASH2")
        };

        return type >= 0 && type < FX_W_TYPES ? weapfx[type] : fx::FxHandle();
    }

    void mapslots() { projs::mapprojfx(); }

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

        loopi(2) if(player1->colours[i] < 0)
        {
            player1->colours[i] = i ? client::playercolour2 : client::playercolour;
            if(player1->colours[i] < 0) setvar(i ? "playercolour2" : "playercolour", rnd(0xFFFFFF), true);
        }
        if(player1->model < 0)
        {
            player1->model = client::playermodel;
            if(player1->model < 0) setvar("playermodel", rnd(PLAYERTYPES), true);
        }
    }

    void cleangl()
    {
        loopv(mixers) mixers[i].cleanup();
    }

    VAR(IDF_PERSIST, flashlightspectator, 0, 0, 1);
    FVAR(IDF_PERSIST, flashlightlevelthird, 0, 0.5f, 1);
    VAR(IDF_PERSIST, flashlightmax, 1, 4, VAR_MAX);
    VAR(IDF_PERSIST, flashlightmaxdark, 1, 16, VAR_MAX);

    #define FLASHLIGHTVARS(name) \
        VAR(IDF_MAP|IDF_HEX, flashlightcolour##name, 0, 0, 0xFFFFFF); \
        FVAR(IDF_MAP, flashlightlevel##name, 0, 1.5f, FVAR_MAX); \
        FVAR(IDF_MAP, flashlightradius##name, 0, 512, FVAR_MAX); \
        VAR(IDF_MAP, flashlightspot##name, 0, 35, 89);

    FLASHLIGHTVARS();
    FLASHLIGHTVARS(alt);

    #define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

    GETMPV(flashlightcolour, int);
    GETMPV(flashlightlevel, float);
    GETMPV(flashlightradius, float);
    GETMPV(flashlightspot, int);

    #define OBITVARS(name) \
        SVAR(IDF_MAP, obit##name, ""); \
        SVAR(IDF_MAP, obit##name##2, ""); \
        SVAR(IDF_MAP, obit##name##3, ""); \
        SVAR(IDF_MAP, obit##name##4, ""); \
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
    OBITVARS(volfog)
    SVAR(IDF_MAP, obitdeath, "");
    SVAR(IDF_MAP, obithurt, "");
    SVAR(IDF_MAP, obitfall, "");
    SVAR(IDF_MAP, obitcheckpoint, "");
    SVAR(IDF_MAP, obittouch, "");
    SVAR(IDF_MAP, obitcrush, "");

    void stopmapmusic()
    {
        if(connected() && maptime > 0) stopmusic();
    }
    VARF(IDF_PERSIST, musictype, 0, 1, 8, stopmapmusic()); // 0 = no in-game music, 1 = map music (or random if none), 2 = always random, 3 = map music (silence if none), 4-5 = same as 1-2 but pick new tracks when done, 6 = theme, 7 = progress, 8 = intermission
    VARF(IDF_PERSIST, musicedit, -1, 0, 8, stopmapmusic()); // same as above for editmode, -1 = use musictype
    SVARF(IDF_PERSIST, musicdir, "sounds/music", stopmapmusic());
    SVARF(IDF_MAP, mapmusic, "", stopmapmusic());

    VAR(IDF_PERSIST, thirdperson, 0, 0, 1);
    VAR(IDF_PERSIST, dynlighteffects, 0, 2, 2);
    VAR(IDF_PERSIST, fullbrightfocus, 0, 0, 3); // bitwise: 0 = don't fullbright focus, 1 = fullbright non-player1, 2 = fullbright player1

    VAR(IDF_PERSIST, thirdpersonmodel, 0, 1, 1);
    VAR(IDF_PERSIST, thirdpersonfov, 90, 120, 150);
    FVAR(IDF_PERSIST, thirdpersonblend, 0, 1, 1);
    VAR(IDF_PERSIST, thirdpersoninterp, 0, 150, VAR_MAX);
    FVAR(IDF_PERSIST, thirdpersondist, FVAR_NONZERO, 14, 20);

    FVAR(IDF_PERSIST, thirdpersonside, -20, 7, 20);
    VAR(IDF_PERSIST, thirdpersoncursor, 0, 1, 2);
    FVAR(IDF_PERSIST, thirdpersoncursorx, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, thirdpersoncursory, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, thirdpersoncursordist, 0, 256, FVAR_MAX);

    VARF(0, follow, -1, -1, VAR_MAX, followswitch(0));

    VAR(IDF_PERSIST, firstpersonmodel, 0, 3, 3);
    VAR(IDF_PERSIST, firstpersoncamera, 0, 0, 1);
    VAR(IDF_PERSIST, firstpersonfov, 90, 100, 150);
    FVAR(IDF_PERSIST, firstpersondepth, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, firstpersonbodydepth, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersonbodydepthkick, 0, 0.75f, 1);
    FVAR(IDF_PERSIST, firstpersondepthfov, 0, 70, 150);
    FVAR(IDF_PERSIST, firstpersonbodydepthfov, 0, 0, 150);

    FVAR(IDF_PERSIST, firstpersonbodydist, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodyside, -10, 0, 10);
    FVAR(IDF_PERSIST, firstpersonbodyoffset, -10, 1, 10);
    FVAR(IDF_PERSIST, firstpersonbodypitch, -1, 1, 1);
    FVAR(IDF_PERSIST, firstpersonbodypitchadjust, -10, 0.75f, 10);

    FVAR(IDF_PERSIST, firstpersonspine, 0, 0.45f, 1);
    FVAR(IDF_PERSIST, firstpersonpitchmin, 0, 90, 90);
    FVAR(IDF_PERSIST, firstpersonpitchmax, 0, 45, 90);
    FVAR(IDF_PERSIST, firstpersonpitchscale, -1, 1, 1);

    VAR(IDF_PERSIST, firstpersonsway, 0, 1, 1);
    FVAR(IDF_PERSIST, firstpersonswayslide, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, firstpersonswaymin, 0, 0.15f, 1);
    FVAR(IDF_PERSIST, firstpersonswaystep, 1, 50.f, 1000);
    FVAR(IDF_PERSIST, firstpersonswayside, 0, 0.05f, 10);
    FVAR(IDF_PERSIST, firstpersonswayup, 0, 0.05f, 10);

    VAR(IDF_PERSIST, firstpersonbob, 0, 0, 1);
    FVAR(IDF_PERSIST, firstpersonbobmin, 0, 0.2f, 1);
    FVAR(IDF_PERSIST, firstpersonbobstep, 1, 50.f, 1000);
    FVAR(IDF_PERSIST, firstpersonbobroll, 0, 0.3f, 10);
    FVAR(IDF_PERSIST, firstpersonbobside, 0, 0.6f, 10);
    FVAR(IDF_PERSIST, firstpersonbobup, 0, 0.6f, 10);
    FVAR(IDF_PERSIST, firstpersonbobtopspeed, 0, 50, 1000);
    FVAR(IDF_PERSIST, firstpersonbobfocusmindist, 0, 64, 10000);
    FVAR(IDF_PERSIST, firstpersonbobfocusmaxdist, 0, 256, 10000);
    FVAR(IDF_PERSIST, firstpersonbobfocus, 0, 0, 1);

    VAR(IDF_PERSIST, firstpersonslidetime, 0, 200, VAR_MAX);
    FVAR(IDF_PERSIST, firstpersonslideroll, -89.9f, -5.f, 89.9f);

    VAR(IDF_PERSIST, editfov, 10, 100, 150);
    VAR(IDF_PERSIST, specfov, 10, 100, 150);

    VAR(IDF_PERSIST, specresetstyle, 0, 1, 1); // 0 = back to player1, 1 = stay at camera

    VARF(IDF_PERSIST, specmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, waitmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv
    VARF(IDF_PERSIST, intermmode, 0, 1, 1, specreset()); // 0 = float, 1 = tv

    VAR(IDF_PERSIST, followdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, followthirdperson, 0, 1, 1);
    VAR(IDF_PERSIST, followaiming, 0, 1, 3); // 0 = don't aim, &1 = aim in thirdperson, &2 = aim in first person
    FVAR(IDF_PERSIST, followdist, FVAR_NONZERO, 14, FVAR_MAX);
    FVAR(IDF_PERSIST, followside, FVAR_MIN, 8, FVAR_MAX);
    FVAR(IDF_PERSIST, followblend, 0, 1, 1);

    VAR(IDF_PERSIST, followtvspeed, 1, 100, VAR_MAX);
    VAR(IDF_PERSIST, followtvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, followtvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, followtvrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, followtvyawscale, FVAR_MIN, 0.5f, 1000);
    FVAR(IDF_PERSIST, followtvpitchscale, FVAR_MIN, 0.25f, 1000);

    VAR(0, spectvcamera, -1, -1, VAR_MAX); // use this specific camera id
    VAR(0, spectvcameraaim, 0, 1, 1); // use this specific camera aiming
    VAR(0, spectvintermthresh, 0, 1000, VAR_MAX); // wait this long before exiting intermission camera
    VAR(IDF_PERSIST, spectviters, 1, 2, 6);
    VAR(IDF_PERSIST, spectvmintime, 1, 5000, VAR_MAX);
    VAR(IDF_PERSIST, spectvtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvspeed, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, spectvyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvyawscale, FVAR_MIN, 1, 1000);
    FVAR(IDF_PERSIST, spectvpitchscale, FVAR_MIN, 1, 1000);
    VAR(IDF_PERSIST, spectvdead, 0, 1, 2); // 0 = never, 1 = in all but duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfirstperson, 0, 2, 3); // bit: 0 = aim in direction followed player is facing, 1 = aim in direction determined by spectv when dead, 2 = aim in direction
    VAR(IDF_PERSIST, spectvthirdperson, 0, 2, 3);

    VAR(IDF_PERSIST, spectvintermiters, 1, 2, 6);
    VAR(IDF_PERSIST, spectvintermmintime, 1000, 5000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvintermpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvintermrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvintermyawscale, FVAR_MIN, 0.5f, 1000);
    FVAR(IDF_PERSIST, spectvintermpitchscale, FVAR_MIN, 0.25f, 1000);

    VAR(0, spectvfollow, -1, -1, VAR_MAX); // attempts to always keep this client in view
    VAR(IDF_PERSIST, spectvfollowself, 0, 1, 2); // if we are not spectating, spectv should show us; 0 = off, 1 = not duel/survivor, 2 = always
    VAR(IDF_PERSIST, spectvfollowiters, 1, 2, 6);
    VAR(IDF_PERSIST, spectvfollowmintime, 1000, 5000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowtime, 1000, 10000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowmaxtime, 0, 20000, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowspeed, 1, 100, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowyawspeed, 1, 500, VAR_MAX);
    VAR(IDF_PERSIST, spectvfollowpitchspeed, 1, 500, VAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowrotate, FVAR_MIN, 0, FVAR_MAX); // rotate style, < 0 = absolute angle, 0 = scaled, > 0 = scaled with max angle
    FVAR(IDF_PERSIST, spectvfollowyawscale, FVAR_MIN, 0.75f, 1000);
    FVAR(IDF_PERSIST, spectvfollowpitchscale, FVAR_MIN, 0.5f, 1000);

    FVAR(IDF_PERSIST, spectvmindist, 0, 4, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvmaxdist, 0, 1024, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowmindist, 0, 4, FVAR_MAX);
    FVAR(IDF_PERSIST, spectvfollowmaxdist, 0, 512, FVAR_MAX);

    VAR(IDF_PERSIST, deathcamstyle, 0, 1, 2); // 0 = no follow, 1 = follow attacker, 2 = follow self
    VAR(IDF_PERSIST, deathcamspeed, 0, 250, VAR_MAX);

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

    FVAR(IDF_PERSIST, aboveheadsmooth, 0, 0.25f, 1);
    VAR(IDF_PERSIST, aboveheadsmoothmillis, 1, 100, 10000);

    VAR(IDF_PERSIST, damageinteger, 0, 1, 1);
    FVAR(IDF_PERSIST, damagedivisor, FVAR_NONZERO, 10, FVAR_MAX);
    FVAR(IDF_PERSIST, damagecritical, 0, 0.25f, 1);
    VAR(IDF_PERSIST, damagecriticalsound, 0, 1, 3);

    VAR(IDF_PERSIST, damagemergedelay, 0, 150, VAR_MAX); // time before being marked as ready
    VAR(IDF_PERSIST, damagemergecombine, 0, 0, VAR_MAX); // time after being ready in which can still merge
    VAR(IDF_PERSIST, damagemergetime, 0, 3000, VAR_MAX); // time that merges last

    VAR(IDF_PERSIST, playdamagetones, 0, 1, 1);
    VAR(IDF_PERSIST, playdamageticks, 0, 0, 1);
    FVAR(IDF_PERSIST, damagetonealarm, 0, 0.25f, FVAR_MAX);
    FVAR(IDF_PERSIST, damagetonegain, 0, 0.5f, FVAR_MAX);
    VAR(IDF_PERSIST, damagetoneself, 0, 0, 1);

    VAR(IDF_PERSIST, prizeeffects, 0, 7, 7); // bit: 1 = sound, 2 = light, 4 = tone
    VAR(IDF_PERSIST, prizeeffectsself, 0, 2, 7); // bit: 1 = sound, 2 = light, 4 = tone
    FVAR(IDF_PERSIST, prizeloopself, 0, 0.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, prizeloopplayer, 0, 0.75f, FVAR_MAX);
    FVAR(IDF_PERSIST, prizeloopjanitor, 0, 1.0f, FVAR_MAX);

    VAR(IDF_PERSIST, playreloadnotify, 0, 3, 15);
    FVAR(IDF_PERSIST, reloadnotifygain, 0, 1, FVAR_MAX);

    VAR(IDF_PERSIST, deathanim, 0, 2, 3); // 0 = hide player when dead, 1 = old death animation, 2 = ragdolls, 3 = ragdolls, but hide in duke
    VAR(IDF_PERSIST, deathfade, 0, 1, 1); // 0 = don't fade out dead players, 1 = fade them out
    VAR(IDF_PERSIST, deathfademin, 0, 1000, VAR_MAX);
    VAR(IDF_PERSIST, deathfademax, 0, 5000, VAR_MAX);
    VAR(IDF_PERSIST, deathfadekamikaze, 0, 750, VAR_MAX);
    VAR(IDF_PERSIST, deathfadeedit, 0, 3000, VAR_MAX);
    VAR(IDF_PERSIST, deathbuttonmash, 0, 1000, VAR_MAX);
    FVAR(IDF_PERSIST, bloodscale, 0, 1, 1000);
    VAR(IDF_PERSIST, bloodfade, 1, 5000, VAR_MAX);
    VAR(IDF_PERSIST, bloodsize, 1, 50, 1000);
    VAR(IDF_PERSIST, bloodsparks, 0, 0, 1);
    VAR(IDF_PERSIST, ragdolleffect, 2, 500, VAR_MAX);

    VAR(IDF_PERSIST, playerhalos, 0, 3, 3); // bitwise: 1 = self, 2 = others
    VAR(IDF_PERSIST, playerhalodamage, 0, 3, 7); // bitwise: 1 = from self, 2 = to self, 4 = others
    VAR(IDF_PERSIST, playerhalodamagetime, 0, 500, VAR_MAX);

    FVAR(IDF_PERSIST, playerblend, 0, 1, 1);
    FVAR(IDF_PERSIST, playereditblend, 0, 1, 1);
    FVAR(IDF_PERSIST, playerghostblend, 0, 0.35f, 1);

    VAR(IDF_PERSIST, playershadow, 0, 2, 2);
    VAR(IDF_PERSIST, playershadowsqdist, 32, 512, VAR_MAX);

    FVAR(IDF_PERSIST, playercombinemix, 0, 0.5f, 1); // when primary and secondary colours are combined

    VAR(IDF_PERSIST, playerovertone, -1, CTONE_SECONDARY_TEAM_MIX, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerovertonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerovertonemix, 0, 0.5f, 1); // when colour and team are combined

    VAR(IDF_PERSIST, playerundertone, -1, CTONE_PRIMARY, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerundertonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerundertonemix, 0, 0.5f, 1); // when colour and team are combined

    VAR(IDF_PERSIST, playerdisplaytone, -1, CTONE_TEAM, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerdisplaytonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerdisplaytonemix, 0, 0.5f, 1); // when colour and team are combined

    VAR(IDF_PERSIST, playerfxtone, -1, CTONE_TEAM, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerfxtonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerfxtonemix, 0, 0.25f, 1); // when colour and team are combined

    VAR(IDF_PERSIST, playerhalotone, -1, CTONE_TEAM, CTONE_MAX-1);
    FVAR(IDF_PERSIST, playerhalotonelevel, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerhalotonemix, 0, 0.5f, 1); // when colour and team are combined

    FVAR(IDF_PERSIST, playerovertoneinterp, 0, 0, 1); // interpolate this much brightness from the opposing tone
    FVAR(IDF_PERSIST, playerovertonebright, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerundertoneinterp, 0, 0, 1); // interpolate this much brightness from the opposing tone
    FVAR(IDF_PERSIST, playerundertonebright, 0.f, 1.f, 10.f);
    FVAR(IDF_PERSIST, playerfxtoneinterp, 0, 0, 1); // interpolate this much brightness from the opposing tone
    FVAR(IDF_PERSIST, playerfxtonebright, 0.f, 1.f, 10.f);

    FVAR(IDF_PERSIST, playerrotdecay, 0, 0.994f, 0.9999f);
    FVAR(IDF_PERSIST, playerrotinertia, 0, 0.2f, 1);
    FVAR(IDF_PERSIST, playerrotmaxinertia, 0, 32.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, playerrotthresh, FVAR_NONZERO, 5, FVAR_MAX);

    VAR(IDF_PERSIST, playerregeneffects, 0, 1, 1);
    FVAR(IDF_PERSIST, playerregentime, 0, 1, 1);
    FVAR(IDF_PERSIST, playerregenfade, 0, 1.0f, 16);
    FVAR(IDF_PERSIST, playerregenslice, 0, 0.125f, 1);
    FVAR(IDF_PERSIST, playerregenblend, 0, 0.25f, 1);
    FVAR(IDF_PERSIST, playerregendecayblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, playerregenbright, -16, 1.0f, 16);
    FVAR(IDF_PERSIST, playerregendecaybright, -16, -1.0f, 16);

    VAR(IDF_PERSIST, playereffect, 0, 1, 1);
    FVAR(IDF_PERSIST, playereffectfade, 0, 1.0f, 16);
    FVAR(IDF_PERSIST, playereffectslice, 0, 0.125f, 1);
    FVAR(IDF_PERSIST, playereffectblend, 0, 1.0f, 1);
    FVAR(IDF_PERSIST, playereffectbright, -16, 0.75f, 16);

    FVAR(IDF_PERSIST, affinityfadeat, 0, 32, FVAR_MAX);
    FVAR(IDF_PERSIST, affinityfadecut, 0, 4, FVAR_MAX);
    FVAR(IDF_PERSIST, affinityfollowblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, affinitythirdblend, 0, 0.5f, 1);

    VAR(IDF_PERSIST, footstepsounds, 0, 3, 3); // 0 = off, &1 = focus, &2 = everyone else
    FVAR(IDF_PERSIST, footstepsoundmin, 0, 0, FVAR_MAX); // minimum velocity magnitude
    FVAR(IDF_PERSIST, footstepsoundmax, 0, 150, FVAR_MAX); // maximum velocity magnitude
    FVAR(IDF_PERSIST, footstepsoundlevel, 0, 1, 10); // a way to scale the volume
    FVAR(IDF_PERSIST, footstepsoundfocus, 0, 0.85f, 10); // focused player version of above
    FVAR(IDF_PERSIST, footstepsoundlight, 0, 0.35f, 10); // crouch/walk version of above
    FVAR(IDF_PERSIST, footstepsoundmedium, 0, 0.75f, 10); // run version of above
    FVAR(IDF_PERSIST, footstepsoundmingain, 0, 0.2f, FVAR_MAX);
    FVAR(IDF_PERSIST, footstepsoundmaxgain, 0, 1, FVAR_MAX);
    FVAR(IDF_PERSIST, footstepsoundrolloff, 0, 0, FVAR_MAX);
    FVAR(IDF_PERSIST, footstepsoundrefdist, 0, 0, FVAR_MAX);

    VAR(IDF_PERSIST, nogore, 0, 0, 1); // turns off all gore, 0 = off, 1 = replace
    VAR(IDF_PERSIST, vanitymodels, 0, 1, 1);
    FVAR(IDF_PERSIST, vanitymaxdist, FVAR_NONZERO, 1024, FVAR_MAX);

    ICOMMANDV(0, gamemode, gamemode)
    ICOMMANDV(0, mutators, mutators)

    int mutscheck(int g, int m, int t)
    {
        int mode = g, muts = m;
        modecheck(mode, muts, t);
        return muts;
    }

    int gettimeoffset()
    {
        return (gs_playing(gamestate) ? lastmillis : totalmillis) - timelast;
    }

    int gettimeremain()
    {
        if(!connected() || !timeremaining) return 0;
        return max(timeremaining - gettimeoffset(), 0);
    }

    int gettimeelapsed(bool force)
    {
        if(!timeelapsed || !timelast || !connected()) return 0;
        return timeelapsed + gettimeoffset();
    }

    int gettimetotal()
    {
        return connected() ? (gettimeremain() + gettimeelapsed()) : 0;
    }

    float gettimeprogress()
    {
        if(!gettimeremain()) return 0.0f;
        int total = gettimetotal();
        return total > 0 ? gettimeelapsed()/float(gettimetotal()) : 0.0f;
    }

    int gettimesync()
    {
        if(!timeelapsed || !timelast || !gs_playing(gamestate) || !connected()) timesync = 0;
        else timesync = max(gettimeelapsed(), timesync);
        return timesync;
    }

    int getprogresswait()
    {
        if(discmillis) return PROGRESS_DISCONNECT;
        if(maploading) return PROGRESS_MAPLOAD;
        if(mapsaving) return PROGRESS_MAPSAVE;
        if(client::needsmap || client::gettingmap) return PROGRESS_MAPDL;
        if(curpeer ? client::waiting() != 0 : connpeer != NULL) return PROGRESS_CONNECT;
        if(!gs_playing(gamestate)) return PROGRESS_GAMESTATE;
        //if(player1->isspectator()) return PROGRESS_GAMEWAIT;
        return PROGRESS_NONE;
    }

    const char *getprogresstitle(bool force)
    {
        if(progressing) return progresstitle;

        int wait = clamp(getprogresswait(), 0, int(PROGRESS_MAX-1));
        if(!force || wait > 0) return PROGRESS_STR[wait];

        return "Loading, please wait";
    }
    ICOMMAND(0, getprogresstitle, "", (), result(getprogresstitle()));

    const char *gamestatename()
    {
        return connected() && gamestate >= 0 && gamestate < G_S_MAX ? G_S_STR[gamestate] : "Main Menu";
    }

    ICOMMAND(0, mutscheck, "iii", (int *g, int *m, int *t), intret(mutscheck(*g, *m, *t)));
    ICOMMAND(0, mutsallowed, "ii", (int *g, int *h), intret(*g >= 0 && *g < G_MAX ? gametype[*g].mutators[*h >= 0 && *h < G_M_GSP+1 ? *h : 0] : 0));
    ICOMMAND(0, mutsimplied, "ii", (int *g, int *m), intret(*g >= 0 && *g < G_MAX ? gametype[*g].implied : 0));
    ICOMMAND(0, gspmutname, "ii", (int *g, int *n), result(*g >= 0 && *g < G_MAX && *n >= 0 && *n < G_M_GSN ? gametype[*g].gsp[*n] : ""));
    ICOMMAND(0, getgameisplay, "b", (int *n), intret(m_play(*n >= 0 ? *n : gamemode) ? 1 :0));
    ICOMMAND(0, getgametimeelapsed, "i", (int *n), intret(gettimeelapsed(*n!=0)));
    ICOMMAND(0, getgametimelimit, "bb", (int *g, int *m), intret(m_mmvar(*g >= 0 ? *g : gamemode, *m >= 0 ? *m : mutators, timelimit)));

    ICOMMANDV(0, intermission, gs_intermission(gamestate) ? 1 : 0);
    ICOMMANDV(0, gamestate, gamestate);
    ICOMMANDV(0, gamemaptime, maptime);
    ICOMMANDV(0, gamemapelapsed, maptime > 0 ? (lastmillis - maptime) : 0);
    ICOMMANDV(0, gametimeremain, gettimeremain());
    ICOMMANDV(0, gametimeelapsed, gettimeelapsed());
    ICOMMANDV(0, gametimetotal, gettimetotal());
    ICOMMANDVF(0, gametimeprogress, gettimeprogress());
    ICOMMANDV(0, gametimewait, timewait);
    ICOMMANDV(0, gametimesync, gettimesync());

    const char *gametitle() { return connected() ? server::gamename(gamemode, mutators) : "Ready"; }
    const char *gametext() { return connected() ? mapname : "Not connected"; }

    enum
    {
        VANITYSTYLE_NONE = 0, VANITYSTYLE_PRIV = 1<<0, VANITYSTYLE_MODEL = 1<<1, VANITYSTYLE_HEAD = 1<<2
    };

    void vanityreset()
    {
        loopvrev(vanities) vanities.remove(i);
        loopv(players) if(players[i]) players[i]->vitems.shrink(0);
        player1->vitems.shrink(0);
        vanitytypetags.deletearrays();
    }
    ICOMMAND(0, resetvanity, "", (), vanityreset());

    ICOMMAND(0, vanitytype, "s", (char *s), vanitytypetags.add(newstring(s)));
    ICOMMAND(0, vanitytypetag, "i", (int *i),
    {
        if(*i < 0) intret(vanitytypetags.length());
        else if(vanitytypetags.inrange(*i)) result(vanitytypetags[*i]);
    });

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

        if(!vanitytypetags.inrange(type)) conoutf(colourorange, "WARNING: Vanity type %d is not declared", type);

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

    int vanitybuild(gameent *d)
    {
        int head = -1;
        if(d->vitems.empty())
        {
            if(*d->vanity)
            {
                vector<char *> vanitylist;
                explodelist(d->vanity, vanitylist);
                loopv(vanitylist)
                {
                    if(!vanitylist[i] || !*vanitylist[i]) continue;
                    loopvk(vanities)
                    {
                        if(strcmp(vanities[k].ref, vanitylist[i])) continue;
                        d->vitems.add(k);
                        if(!actors[d->actortype].hashead || vanities[k].type || head >= 0) continue;
                        head = k;
                    }
                }
                vanitylist.deletearrays();
            }
        }
        else
        {
            loopv(d->vitems)
            {
                if(!vanities.inrange(d->vitems[i]) || vanities[d->vitems[i]].type) continue;
                head = d->vitems[i];
                break;
            }
        }
        if(actors[d->actortype].hashead && !vanities.inrange(head)) loopv(vanities)
        {
            if(vanities[i].type) continue;
            d->vitems.add(i);
            head = i;
            break;
        }
        return head;
    }

    const char *vanitymodel(gameent *d)
    {
        if(d->actortype >= A_ENEMY) return actors[d->actortype%A_MAX].name;
        return playertypes[d->model%PLAYERTYPES][5];
    }

    const char *vanityfname(gameent *d, int n, int head, bool proj)
    {
        const char *file = NULL;
        if(vanities.inrange(n))
        {
            // No need for advanced checks if there is no style.
            if(vanities[n].style == 0) file = proj ? vanities[n].proj : vanities[n].model;
            else
            {
                // Unique ID for this vanity setup.
                defformatstring(id, "%s", vanities[n].model);
                if(vanities[n].style&VANITYSTYLE_HEAD)
                {
                    bool hashead = head >= 0 && vanities.inrange(head);
                    concformatstring(id, "/%s", hashead ? vanities[head].ref : vanitymodel(d));
                    if(hashead && vanities[head].style&VANITYSTYLE_MODEL) concformatstring(id, "/%s", vanitymodel(d));
                }
                else if(vanities[n].style&VANITYSTYLE_MODEL) concformatstring(id, "/%s", vanitymodel(d));

                if(vanities[n].style&VANITYSTYLE_PRIV) concformatstring(id, "/%s", server::privnamex(d->privilege, d->actortype, true));
                if(proj) concatstring(id, "/proj");

                // Check if we've already found the file.
                loopv(vanities[n].files) if(!strcmp(vanities[n].files[i], id)) file = vanities[n].files[i];

                // If not already found, build file name from each style.
                if(!file) file = vanities[n].files.add(newstring(id));
            }
        }
        return file;
    }

    ICOMMAND(0, vanityfname, "i", (int *n), result(vanityfname(player1, *n, vanitybuild(player1), false)));

    bool vanitycheck(gameent *d)
    {
        return vanitymodels && (d == focus || d->o.squaredist(camera1->o) <= vanitymaxdist*vanitymaxdist);
    }

    bool allowspec(gameent *d, int level, int cn = -1)
    {
        if(d->team >= T_ENEMY || !insideworld(d->o)) return false;
        if(d->isspectator() || (d->isnotalive() && !d->lastdeath)) return false;
        if(cn >= 0)
        {
            if(cn == focus->clientnum && !focus->isalive() && d->clientnum == focus->lastattacker) return true;
            return d->clientnum == cn; // override
        }
        switch(level)
        {
            case 0: if(!d->isalive() && !d->isediting()) return false; break;
            case 1: if(m_duke(gamemode, mutators) && !d->isalive() && !d->isediting()) return false; break;
            case 2: break;
        }
        return true;
    }

    bool thirdpersonview(bool viewonly, physent *d)
    {
        if(!gs_playing(gamestate)) return true;
        if(!d) d = focus;
        if(!viewonly && d->isnotalive()) return true;
        if(player1->isediting()) return false;
        if(player1->iswatching() && d == player1) return false;
        if(d == focus && inzoom()) return false;
        if(!(d != player1 ? followthirdperson : thirdperson)) return false;
        return true;
    }
    ICOMMAND(0, isthirdperson, "i", (int *viewonly), intret(thirdpersonview(*viewonly ? true : false) ? 1 : 0));
    ICOMMAND(0, thirdpersonswitch, "", (), int *n = (focus != player1 ? &followthirdperson : &thirdperson); *n = !*n);

    int fov()
    {
        if(player1->isediting()) return editfov;
        if(focus == player1 && player1->isspectator()) return specfov;
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
    ICOMMANDV(0, inzoom, inzoom() ? 1 : 0);

    float zoomscale()
    {
        if(!inzoom()) return 0.f;
        float amt = 1;
        int frame = lastmillis-lastzoom;
        if(frame <= 0) amt = 0;
        else if(frame < W(focus->weapselect, cookzoom))
            amt = clamp(frame/float(W(focus->weapselect, cookzoom)), 0.f, 1.f);
        return zooming ? amt : 1-amt;
    }

    void resetsway()
    {
        swaydir = swaypush = vec(0, 0, 0);
        swayfade = swayspeed = swaydist = bobfade = bobdist = 0;
    }

    void addsway(gameent *d)
    {
        float speed = physics::movevelocity(d), step = firstpersonbob ? firstpersonbobstep : firstpersonswaystep;
        bool bobbed = false, sliding = d->hasslide();
        if(d->isalive() && (d->physstate >= PHYS_SLOPE || physics::sticktospecial(d) || sliding))
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
        if(d->isalive() && speedscale > 0) swaydir.add(vec(inertia).mul((1-k)/(15*speedscale)));
        swaypush.mul(pow(0.5f, curtime/25.0f));
    }

    int errorchan = -1;
    void errorsnd(gameent *d)
    {
        if(d == player1 && !issound(errorchan))
            emitsound(S_ERROR, getplayersoundpos(d), d, &errorchan, SND_PRIORITY|SND_NOENV|SND_NOATTEN);
    }

    void resetfollow()
    {
        follow = spectvfollow = -1;
        if(specresetstyle && (player1->state == CS_WAITING || player1->isspectator()))
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
            if(d->team >= T_ENEMY) return;
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
                                lastcamcn = spectvcamera = -1;
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
            lastcamcn = spectvcamera = -1;
            lastcamera = lasttvcam = lasttvchg = 0;
            resetfollow();
        }
    }

    bool followaim() { return followaiming&(thirdpersonview(true) ? 1 : 2); }

    bool intermthresh()
    {
        if(!gs_playing(gamestate)) return true;
        if(gamestate == G_S_PLAYING && gettimeelapsed() <= spectvintermthresh && player1->iswatching()) return true;
        return false;
    }

    bool tvmode(bool check)
    {
        if(!check || !cameras.empty())
        {
            if(intermthresh() && intermmode) return true;
            else switch(player1->state)
            {
                case CS_SPECTATOR: if(specmode) return true; break;
                case CS_WAITING: if((waitmode && (!player1->lastdeath || lastmillis - player1->lastdeath >= 500))) return true; break;
                default: break;
            }
        }
        return false;
    }

    #define MODESWITCH(name) \
        ICOMMAND(0, name##modeswitch, "", (), \
        { \
            if(tvmode()) name##mode = 0; \
            else name##mode = 1; \
            specreset(); \
        });
    MODESWITCH(spec);
    MODESWITCH(wait);

    void followswitch(int n, bool other)
    {
        if(player1->isspectator() || (player1->state == CS_WAITING && (!player1->lastdeath || !deathbuttonmash || lastmillis-player1->lastdeath > deathbuttonmash)))
        {
            bool istv = tvmode();
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
    }
    ICOMMAND(0, followdelta, "ii", (int *n, int *o), followswitch(*n != 0 ? *n : 1, *o != 0));

    void spectvcamupdate()
    {
        while(spectvcamera < -1) spectvcamera += cameras.length() + 1;
        while(spectvcamera >= cameras.length()) spectvcamera -= cameras.length() + 1;
        if(spectvcamera >= 0 && !cameras.inrange(spectvcamera)) spectvcamera = -1;
    }

    void spectvswitch(int n)
    {
        spectvcamera += n;
        spectvcamupdate();
    }
    ICOMMAND(0, spectvdelta, "i", (int *n), spectvswitch(*n != 0 ? *n : 1));

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
        if(A(d->actortype, abilities)&(1<<A_A_KAMIKAZE)) len = deathfadekamikaze;
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

    float protectfade(gameent *d)
    {
        int prot = m_protect(gamemode, mutators), millis = d->protect(lastmillis, prot); // protect returns time left
        if(millis > 0) return 1.f - min(float(millis) / float(prot), 1.0f);
        return 1;
    }

    float opacity(gameent *d, bool third, bool effect)
    {
        if(d->isnotalive() && !deathanim) return 0;

        float total = d == focus ? (third ? (d != player1 ? followblend : thirdpersonblend) : 1.f) : playerblend;
        if(d->isnotalive())
        {
            if(deathfade)
            {
                float amt = spawnfade(d);
                if(!effect || amt == 0.0f) total *= amt;
            }
        }
        else if(d->isalive())
        {
            if(d == focus && third) total *= min(camera1->o.dist(d->o)/(d != player1 ? followdist : thirdpersondist), 1.0f);
            float amt = protectfade(d);
            if(!effect || amt == 0.0f) total *= amt;
        }
        else if(d->isediting()) total *= playereditblend;

        if(physics::isghost(d, focus)) total *= playerghostblend;

        return total;
    }

    void respawned(gameent *d, bool local, int ent)
    { // remote clients wait until first position update to process this
        d->configure(lastmillis, gamemode, mutators, physics::hasaffinity(d), 0, playerrotdecay, playerrotinertia, playerrotmaxinertia);

        if(local)
        {
            d->state = CS_ALIVE;
            entities::spawnplayer(d, ent, true);
            client::addmsg(N_SPAWN, "ri", d->clientnum);
        }

        if(d == player1) specreset();
        else if(d == focus) resetcamera();
        if(d == focus) resetsway();

        if(d->actortype < A_ENEMY)
        {
            vec center = d->center();
            emitsound(S_RESPAWN, getplayersoundpos(d), d);
            spawneffect(PART_SPARK, center, d->height*0.5f, getcolour(d, playerovertone, playerovertonelevel, playerovertonemix), 0.5f);
            spawneffect(PART_SPARK, center, d->height*0.5f, getcolour(d, playerundertone, playerundertonelevel, playerundertonemix), 0.5f);
            spawneffect(PART_SPARK, center, d->height*0.5f, getcolour(d, playerfxtone, playerfxtonelevel, playerfxtonemix), 1);
            if(dynlighteffects) adddynlight(center, d->height*2, vec::fromcolor(getcolour(d, playerfxtone, playerfxtonelevel, playerfxtonemix)).mul(2.f), 250, 250, L_NOSHADOW|L_NODYNSHADOW);
            if(entities::ents.inrange(ent) && entities::ents[ent]->type == PLAYERSTART) entities::execlink(d, ent, false);
        }
        ai::respawned(d, local, ent);
    }

    // determines if correct gamemode/mutators are present for displaying palette colours
    // for textures, lights, mapmodels, particles which use a palette colour
    VAR(0, forcepalette, 0, 0, 2); // force palette for teams in edit mode: 0 = off, 1 = neutral, 2 = teams

    int hexpalette(int palette, int index)
    {
        if(palette >= 0 && index >= 0) switch(palette)
        {
            case 0: // misc (rainbow palettes)
            {
                if(index <= 0 || index > PULSE_MAX) break;
                return pulsecols[index-1][clamp((lastmillis/100)%PULSE_COLS, 0, PULSE_COLS-1)];
                break;
            }
            case 1: // teams
            {
                int team = index%T_COUNT;
                if(drawtex != DRAWTEX_MAP && index < T_COUNT)
                {
                    if(m_edit(gamemode) && forcepalette)
                    {
                        if(forcepalette == 1 || team > T_LAST) team = T_NEUTRAL;
                    }
                    else if(!m_team(gamemode, mutators) || team > T_LAST) team = T_NEUTRAL; // abstract team coloured levels to neutral
                }
                return TEAM(team, colour); // return color of weapon
                break;
            }
            case 2: // weapons
            {
                int weap = index%W_MAX;
                if(drawtex != DRAWTEX_MAP && !m_edit(gamemode) && index < W_MAX)
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
        if(gver <= 244 && palette == 1 && index%(T_COUNT+2) > T_LAST)
            index = T_NEUTRAL; // remove kappa and sigma

        if(gver <= 273 && palette == 2)
        {
            int weap = index % 13; // old W_MAX
            if(weap >= 9) index++;
        }
    }

    float darkness(int type)
    {
        if(DRAWTEX_DARK&(1<<drawtex) && m_dark(gamemode, mutators))
            switch(type)
            {
                case DARK_ENV: return darknessenv;
                case DARK_GLOW: return darknessglow;
                case DARK_SUN: return darknesssun;
                case DARK_PART: return darknesspart;
                case DARK_HALO: return darknesshalo;
                case DARK_UI: return darknessui;
                default: break;
            }

        return 1;
    }
    ICOMMAND(0, getdarkness, "i", (int *n), floatret(darkness(*n)));

    bool wantflashlight()
    {
        if(m_dark(gamemode, mutators)) return true;
        if(getflashlightcolour() > 0 && getflashlightlevel() > 0) return true;
        return false;
    }

    void flashlighteffect(gameent *d)
    {
        int fcol = getflashlightcolour(), spot = m_dark(gamemode, mutators) ? darknessflashspot : getflashlightspot();
        bvec color = bvec(fcol ? fcol : 0xFFFFFF);

        float radius = getflashlightradius();
        if(m_dark(gamemode, mutators))
        {
            if(darknessflashcolourmin) color.max(darknessflashcolourmin*255);
            if(darknessflashcolourmax) color.min(darknessflashcolourmax*255);
            if(darknessflashradiusmin) radius = max(radius, darknessflashradiusmin);
            if(darknessflashradiusmax) radius = min(radius, darknessflashradiusmax);
        }
        float level = m_dark(gamemode, mutators) ? darknessflashlevel : getflashlightlevel();
        if(d != focus) level *= flashlightlevelthird;

        static fx::FxHandle flashlight_emit = fx::getfxhandle("FX_PLAYER_FLASHLIGHT_EMIT");
        static fx::FxHandle flashlight_beam = fx::getfxhandle("FX_PLAYER_FLASHLIGHT_BEAM");

        if(d->isalive()) color.mul(protectfade(d));

        fx::createfx(d == focus ? flashlight_emit : flashlight_beam, &d->flashlightfx)
            .setentity(d)
            .setcolor(color)
            .setparam(0, radius)
            .setparam(1, spot)
            .setparam(2, level)
            .setparam(3, d != focus ? 1 : 0);
    }

    void adddynlights()
    {
        if(!dynlighteffects || !(DRAWTEX_GAME&(1<<drawtex))) return;

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
            if(d->burntime && d->burnfunc(lastmillis, d->burntime))
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
                adddynlight(d->center(), d->height*intensity*pc, pulsecolour(d, PULSE_BURN, -1).mul(pc), 0, 0, L_NOSHADOW|L_NODYNSHADOW);
            }

            if(d->shocktime && d->shockfunc(lastmillis, d->shocktime))
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
                adddynlight(d->center(), d->height*intensity*pc, pulsecolour(d, PULSE_SHOCK, -1).mul(pc), 0, 0, L_NOSHADOW|L_NODYNSHADOW);
            }

            if((d != focus ? prizeeffects : prizeeffectsself)&2 && d->isprize(focus) > 0)
                adddynlight(d->center(), d->height * 2, pulsecolour(d, PULSE_PRIZE), 0, 0, L_NOSHADOW|L_NODYNSHADOW);
        }
    }

    struct flashent
    {
        gameent *owner;
        float dist;

        flashent(gameent *d) : owner(d), dist(0) { dist = owner->o.squaredist(camera1->o); }
        ~flashent() {}

        static inline bool sort(const flashent *x, const flashent *y)
        {
            if(x->owner == focus) return true;
            if(y->owner == focus) return false;
            if(x->owner->actortype > y->owner->actortype) return true;
            if(x->owner->actortype < y->owner->actortype) return false;
            if(x->dist > y->dist) return true;
            return false;
        }
    };

    void checklights(bool reset = false)
    {
        if(wantflashlight())
        {
            if(!hasspotlights || reset) cleardeferredlightshaders();
            hasspotlights = true;

            gameent *d = NULL;
            int numdyns = numdynents();
            vector<flashent *> list;
            loopi(numdyns)
                if(((d = (gameent *)iterdynents(i)) != NULL) && (d->actortype < A_ENVIRONMENT))
                    if(d->isalive() || (flashlightspectator && d == focus && d->isnophys()))
                        list.add(new flashent(d));
            list.sort(flashent::sort);

            int count = min(list.length(), m_dark(gamemode, mutators) ? flashlightmaxdark : flashlightmax);
            loopi(count) flashlighteffect(list[i]->owner);
            list.deletecontents();
        }
        else hasspotlights = false;
    }

    bool spotlights()
    {
        return hasspotlights;
    }

    bool volumetrics()
    {
        return false;
    }

    void impulseeffect(gameent *d, float gain, int effect)
    {
        static fx::FxHandle impulsesound       = fx::getfxhandle("FX_PLAYER_IMPULSE_SOUND");
        static fx::FxHandle impulseactionsound = fx::getfxhandle("FX_PLAYER_IMPULSE_ACTION_SOUND");
        static fx::FxHandle impulseslidesound  = fx::getfxhandle("FX_PLAYER_IMPULSE_SLIDE_SOUND");
        static fx::FxHandle impulsejet         = fx::getfxhandle("FX_PLAYER_IMPULSE_JET");
        static fx::FxHandle dronefx            = fx::getfxhandle("FX_DRONE");
        static fx::FxHandle janitorfx          = fx::getfxhandle("FX_JANITOR");

        switch(effect)
        {
            case 0:
                if(d->actortype < A_ENEMY)
                {
                    if(d->impulsetimer(IM_T_SLIDE))
                        fx::createfx(impulseslidesound).setparam(0, gain).setentity(d);
                    else if(d->impulsetimer(IM_T_VAULT) || d->impulsetimer(IM_T_WALLRUN))
                        fx::createfx(impulseactionsound).setparam(0, gain).setentity(d);
                    else
                        fx::createfx(impulsesound).setparam(0, gain).setentity(d);
                }
                // fall through
            case 1:
            {
                if(!actors[d->actortype].jetfx || paused) break;

                bool sliding = false;

                fx::FxHandle *curfx = NULL;

                switch(d->actortype)
                {
                    case A_JANITOR: curfx = &janitorfx; break;
                    case A_DRONE: curfx = &dronefx; break;
                    default:
                        sliding = d->impulsetimer(IM_T_SLIDE) && d->physstate >= PHYS_SLOPE;
                        curfx = &impulsejet;
                    break;
                }

                if(curfx) fx::createfx(*curfx, &d->impulsefx)
                    .setentity(d)
                    .setparam(0, effect ? 0.0f : 1.0f)
                    .setparam(1, sliding ? 1.0f : 0.0f)
                    .setcolor(bvec(getcolour(d)));

                break;
            }
            default: break;
        }
    }

    void prizeeffect(gameent *d)
    {
        static fx::FxHandle prizefx = fx::getfxhandle("FX_PRIZE_OPEN");
        fx::createfx(prizefx, &d->prizefx).setentity(d);
    }

    void enveffect(gameent *d)
    {
        if((d->inmaterial&MATF_VOLUME) != MAT_WATER || d->submerged < 0.1f) return;

        static fx::FxHandle underwater = fx::getfxhandle("FX_PLAYER_ENV_WATER");

        fx::createfx(underwater, &d->envfx).setentity(d).setparam(0, d->submerged).setcolor(bvec(getwatercolour(d->inmaterial&MATF_INDEX)));
    }

    void setmode(int nmode, int nmuts) { modecheck(nextmode = nmode, nextmuts = nmuts); }
    ICOMMAND(0, mode, "ii", (int *val, int *mut), setmode(*val, *mut));

    void footstep(gameent *d, int curfoot)
    {
        if(!actors[d->actortype].steps || footstepsoundmaxgain <= 0) return;
        bool moving = d->move || d->strafe, liquid = physics::liquidcheck(d), onfloor = !(A(d->actortype, abilities)&(1<<A_A_FLOAT)) && (d->physstate >= PHYS_SLOPE || physics::sticktospecial(d));
        if(curfoot < 0 || (moving && (liquid || onfloor)))
        {
            float mag = d->vel.magnitude(), m = min(footstepsoundmax, footstepsoundmin), n = max(footstepsoundmax, footstepsoundmin);
            if(n > m && mag > m)
            {
                if(curfoot < 0) curfoot = d->lastfoot;
                if(curfoot < 0 || curfoot > 1) curfoot = 0;
                int sound = -1;

                if(liquid && (!onfloor || rnd(4))) sound = S_SWIMSTEP;
                else sound = curfoot ? S_FOOTSTEP_L : S_FOOTSTEP_R;

                float amt = clamp(mag/n, 0.f, 1.f)*(d != focus ? footstepsoundlevel : footstepsoundfocus);
                if(onfloor && !d->sprinting()) amt *= d->running() ? footstepsoundmedium : footstepsoundlight;
                float gain = clamp(amt*footstepsoundmaxgain, footstepsoundmingain, footstepsoundmaxgain);
                emitsound(sound, d->gettag(TAG_TOE+curfoot), d, &d->sschan[curfoot], d != focus ? 0 : SND_PRIORITY, gain, 1, footstepsoundrolloff > 0 ? footstepsoundrolloff : -1.f, footstepsoundrefdist > 0 ? footstepsoundrefdist : -1.f);;
            }
        }
    }

    void checkoften(gameent *d, bool local)
    {
        adjustscaled(d->quake, quakedelta, quakefade);
        int prevstate = isweap(d->weapselect) ? d->weapstate[d->weapselect] : W_S_IDLE;
        float offset = d->height, minz = d->feetpos().z;
        d->o.z -= d->height;

        entities::physents(d);
        d->configure(lastmillis, gamemode, mutators, physics::hasaffinity(d), curtime, playerrotdecay, playerrotinertia, playerrotmaxinertia);

        if(d->isalive())
        {
            if(!physics::movepitch(d, true))
            {
                bool sliding = d->impulsetimer(IM_T_SLIDE) != 0, crouching = sliding || (d->action[AC_CROUCH] && A(d->actortype, abilities)&(1<<A_A_CROUCH)),
                    moving = d->move || d->strafe || (d->physstate < PHYS_SLOPE && !physics::laddercheck(d)), ishi = moving && !sliding;
                float zradlo = d->zradius*CROUCHLOW, zradhi = d->zradius*CROUCHHIGH, zrad = ishi ? zradhi : zradlo;
                vec old = d->o;

                if(A(d->actortype, abilities)&(1<<A_A_CROUCH) && (!crouching || ishi))
                {
                    short wantcrouch[2] = { 0, 0 };
                    loopj(2)
                    {
                        if(j)
                        {
                            vec dir;
                            vecfromyawpitch(d->yaw, 0.f, d->move, d->strafe, dir);
                            d->o.add(dir);
                        }
                        d->o.z += d->zradius;
                        d->height = d->zradius;
                        if(collide(d, vec(0, 0, 1), 0, false) || collideinside) loopk(2)
                        {
                            zrad = k ? zradlo : zradhi;
                            float zoff = d->zradius - zrad;
                            d->o.z -= zoff;
                            d->height = zrad;
                            if(!collide(d, vec(0, 0, 1), 0, false) && !collideinside)
                            {
                                wantcrouch[j] = k + 1;
                                break;
                            }
                            d->o.z += zoff;
                        }
                        d->o = old;
                        d->height = offset;
                    }
                    loopj(2) if(wantcrouch[j])
                    {
                        if(ishi && wantcrouch[j] == 2) ishi = false;
                        crouching = true;
                    }
                    zrad = ishi ? zradhi : zradlo;
                }
                if(crouching || d->crouching(true))
                {
                    float zamt = (d->zradius - zrad)*curtime/float(PHYSMILLIS);
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

            if(d->running() && d->move > 0 && !d->strafe && (movesprintrotvel == 0.0f || movesprintrotvel > fabs(d->rotvel.x)) && d->vel.magnitude() > 0.0f)
            {
                if((d->physstate >= PHYS_SLOPE || physics::sticktospecial(d, false)) && ((d->sprinttime += curtime) > A(d->actortype, sprinttime)))
                    d->sprinttime = A(d->actortype, sprinttime);
            }
            else if(movesprintdecay == 0.0f || ((d->sprinttime -= int(curtime * movesprintdecay)) < 0)) d->sprinttime = 0;

            if(physics::movepitch(d, true) || d->hasparkour() || d->impulseeffect() || d->sprinting()) impulseeffect(d, 1.f, 1);

            enveffect(d);
        }
        else
        {
            d->height = d->zradius;
            d->actiontime[AC_CROUCH] = d->sprinttime = 0;
        }

        d->o.z += d->airmillis ? offset : d->height;
        float feetz = d->feetpos().z;
        if(feetz < minz) d->o.z += minz - feetz; // ensure player doesn't end up lower than they were

        bool collectcount = false, collectmeter = false;
        if(d->regenimpulse())
        {
            bool onfloor = !(A(d->actortype, abilities)&(1<<A_A_FLOAT)) && (d->physstate >= PHYS_SLOPE || physics::sticktospecial(d, false) || physics::liquidcheck(d)); // collect time until we are able to act upon it

            #define IMPULSEMOD(name, type, test, check, body) \
            { \
                if(impulsecost##name && (test)) \
                { \
                    float skew = d->impulseskew##name(onfloor); \
                    if(skew > 0.0f) \
                    { \
                        int timeslice = int(floorf((curtime + d->impulse[IM_COLLECT_##type]) * skew)); \
                        if(check) { body; } \
                        else \
                        { \
                            d->impulse[IM_COLLECT_##type] += curtime; \
                            if(!d->impulse[IM_LASTCOL_##type]) d->impulse[IM_LASTCOL_##type] = lastmillis; \
                            collect##name = true; \
                        } \
                    } \
                } \
            }

            IMPULSEMOD(count, COUNT, d->impulse[IM_COUNT] > 0, timeslice >= impulsecostcount,
                while(timeslice >= impulsecostcount && d->impulse[IM_COUNT] > 0)
                {
                    timeslice -= impulsecostcount;
                    d->impulse[IM_COUNT]--;
                }
                d->impulse[IM_COLLECT_COUNT] = timeslice;
            );
            IMPULSEMOD(meter, METER, d->impulse[IM_METER] > 0, timeslice > 0,
                d->impulse[IM_METER] = max(d->impulse[IM_METER] - timeslice, 0);
                d->impulse[IM_COLLECT_METER] = 0;
            );
        }
        #define IMPULSECOLLECT(name, type) \
            if(!collect##name) d->impulse[IM_COLLECT_##type] = d->impulse[IM_LASTCOL_##type] = 0;
        IMPULSECOLLECT(count, COUNT);
        IMPULSECOLLECT(meter, METER);

        if(isweap(d->weapselect) && gs_playing(gamestate) && d->isalive())
        {
            bool secondary = physics::secondaryweap(d);
            bool firing = d->weapstate[d->weapselect] == (secondary ? W_S_SECONDARY : W_S_PRIMARY);

            if(d->wasfiring != d->weapselect && firing)
            {
                d->wasfiring = d->weapselect;
                emitsound(WSNDFB(d->weapselect, secondary), weapons::getweapsoundpos(d, d->getmuzzle()), d, &d->wschan[WS_BEGIN_CHAN]);
            }
            else if(d->wasfiring == d->weapselect && !firing)
            {
                d->wasfiring = -1;
                emitsound(WSNDFE(d->weapselect, secondary), weapons::getweapsoundpos(d, d->getmuzzle()), d, &d->wschan[WS_END_CHAN]);
            }

            if(d->weapselect < W_ALL && d->weapstate[d->weapselect] != W_S_RELOAD && prevstate == W_S_RELOAD && playreloadnotify&(d == focus ? 1 : 2) && (d->weapammo[d->weapselect][W_A_CLIP] >= W(d->weapselect, ammoclip) || playreloadnotify&(d == focus ? 4 : 8)) && reloadnotifygain > 0)
                    emitsound(WSND(d->weapselect, S_W_NOTIFY), weapons::getweapsoundpos(d, TAG_ORIGIN), d, &d->wschan[WS_MAIN_CHAN], 0, reloadnotifygain);

            if(d->weapstate[d->weapselect] == W_S_POWER || d->weapstate[d->weapselect] == W_S_ZOOM)
            {
                int millis = lastmillis-d->weaptime[d->weapselect];
                if(millis >= 0 && millis <= d->weapwait[d->weapselect])
                {
                    float amt = millis/float(d->weapwait[d->weapselect]), gain = 1.f;
                    int snd = d->weapstate[d->weapselect] == W_S_POWER ? WSND2(d->weapselect, secondary, S_W_POWER) : WSND(d->weapselect, S_W_ZOOM);
                    if(W2(d->weapselect, cooktime, secondary)) switch(W2(d->weapselect, cooked, secondary))
                    {
                        case 4: case 5: gain = 0.1f+((1.f-amt)*0.9f); break; // longer
                        case 1: case 2: case 3: default: gain = 0.1f+(amt*0.9f); break; // shorter
                    }
                    if(issound(d->wschan[WS_POWER_CHAN])) soundsources[d->wschan[WS_POWER_CHAN]].gain = gain;
                    else emitsound(snd, weapons::getweapsoundpos(d, TAG_ORIGIN), d, &d->wschan[WS_POWER_CHAN], SND_LOOP, gain);
                }
                else if(d->wschan[WS_POWER_CHAN] >= 0)
                {
                    if(issound(d->wschan[WS_POWER_CHAN])) soundsources[d->wschan[WS_POWER_CHAN]].clear();
                    d->wschan[WS_POWER_CHAN] = -1;
                }
            }
            else if(d->wschan[WS_POWER_CHAN] >= 0)
            {
                if(issound(d->wschan[WS_POWER_CHAN])) soundsources[d->wschan[WS_POWER_CHAN]].clear();
                d->wschan[WS_POWER_CHAN] = -1;
            }
            if(W2(d->weapselect, cooked, true)&W_C_KEEP && d->prevstate[d->weapselect] == W_S_ZOOM && !d->action[AC_SECONDARY])
            {
                d->prevstate[d->weapselect] = W_S_IDLE;
                d->prevtime[d->weapselect] = 0;
            }
        }
        else if(d->wschan[WS_POWER_CHAN] >= 0)
        {
            if(issound(d->wschan[WS_POWER_CHAN])) soundsources[d->wschan[WS_POWER_CHAN]].clear();
            d->wschan[WS_POWER_CHAN] = -1;
        }

        if(local)
        {
            if(d->respawned >= 0 && lastmillis-d->respawned >= 2500) d->respawned = -1;
            if(d->suicided >= 0 && lastmillis-d->suicided >= 2500) d->suicided = -1;
        }

        int restime[W_R_MAX] = { d->burntime, d->bleedtime, d->shocktime, d->corrodetime };
        loopi(W_R_MAX) if(d->lastres[i] > 0 && lastmillis-d->lastres[i] >= restime[i]) d->resetresidual(i);
        if(gs_playing(gamestate) && d->isalive() && actors[d->actortype].steps)
        {
            int curfoot = d->curfoot();
            bool hassound = footstepsounds&(d != focus ? 2 : 1);
            if(!hassound) loopi(2) if(d->sschan[i] >= 0)
            {
                if(issound(d->sschan[i])) soundsources[d->sschan[i]].clear();
                d->sschan[i] = -1;
            }
            if(curfoot >= 0)
            {
                if(curfoot != d->lastfoot)
                {
                    if(hassound)
                    {
                        if(d->lastfoot >= 0 && issound(d->sschan[d->lastfoot])) soundsources[d->sschan[d->lastfoot]].pos = d->toetag(d->lastfoot);
                        footstep(d, curfoot);
                        d->lastfoot = curfoot;
                    }
                }
                else if(hassound) loopi(2) if(issound(d->sschan[i])) soundsources[d->sschan[i]].pos = d->toetag(i);
            }
        }

        if(actors[d->actortype].weapfx)
        {
            int millis = lastmillis-d->weaptime[d->weapselect];
            bool last = millis > 0 && millis < d->weapwait[d->weapselect],
                 powering = last && d->weapstate[d->weapselect] == W_S_POWER,
                 secondary = physics::secondaryweap(d);
            float amt = last ? (lastmillis-d->weaptime[d->weapselect])/float(d->weapwait[d->weapselect]) : 1.f;
            int color = WHCOL(d, d->weapselect, fxcol, secondary);

            if(powering && showweapfx)
            {
                int fxtype = W2(d->weapselect, fxtypepower, secondary);
                fx::FxHandle fxhandle = getweapfx(fxtype);
                if(fxhandle.isvalid())
                {
                    vec from = d->muzzletag(d->weapselect);
                    vec targ;
                    safefindorientation(d->o, d->yaw, d->pitch, targ);
                    targ.sub(from).normalize().add(from);
                    fx::emitter &e = fx::createfx(fxhandle, &d->weaponfx)
                        .setfrom(from)
                        .setto(targ)
                        .setcolor(bvec(color))
                        .setentity(d);

                    e.setparam(W_FX_POWER_PARAM, amt);
                }
            }
        }

        if((d != focus ? prizeeffects : prizeeffectsself)&1 && d->isprize(focus))
        {
            float gain = d != focus ? (d->actortype == A_JANITOR ? prizeloopjanitor : prizeloopplayer) : prizeloopself;
            if(!issound(d->plchan[PLCHAN_ALERT]))
                emitsound(S_PRIZELOOP, getplayersoundpos(d), d, &d->plchan[PLCHAN_ALERT], SND_LOOP|SND_PRIORITY, gain);
            else soundsources[d->plchan[PLCHAN_ALERT]].gain = gain;
        }
        else if(issound(d->plchan[PLCHAN_ALERT]) && soundsources[d->plchan[PLCHAN_ALERT]].flags&SND_LOOP)
            soundsources[d->plchan[PLCHAN_ALERT]].clear();
    }

    void checkfloor(gameent *d)
    {
        if(!d->isalive()) return;

        vec pos = d->feetpos();
        if(d->impulsetimer(IM_T_WALLRUN) == 0 && (d->physstate >= PHYS_SLOPE || physics::sticktospecial(d, false) || physics::liquidcheck(d)) && pos.z > 0 && d->floortime(lastmillis))
        {
            int mat = lookupmaterial(pos);
            if(!isclipped(mat&MATF_VOLUME) && !(mat&MAT_DEATH)) d->floorpos = pos;
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

            entities::checkitems(d);

            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime) continue;
            //else if(lagtime > 1000) continue;

            physics::smoothplayer(d, 1, false);
        }
    }

    bool burn(gameent *d, int weap, int flags)
    {
        if(wr_burns(weap, flags) && ((d->inmaterial&MATF_VOLUME) != MAT_WATER || d->submerged < WATERPHYS(extinguish, d->inmaterial)))
        {
            d->lastrestime[W_R_BURN] = lastmillis;
            if(isweap(weap) || flags&HIT_MATERIAL) d->lastres[W_R_BURN] = lastmillis;
            else return true;
        }
        return false;
    }

    bool bleed(gameent *d, int weap, int flags)
    {
        if(wr_bleeds(weap, flags))
        {
            d->lastrestime[W_R_BLEED] = lastmillis;
            if(isweap(weap) || flags&HIT_MATERIAL) d->lastres[W_R_BLEED] = lastmillis;
            else return true;
        }
        return false;
    }

    bool shock(gameent *d, int weap, int flags)
    {
        if(wr_shocks(weap, flags))
        {
            d->lastrestime[W_R_SHOCK] = lastmillis;
            if(isweap(weap) || flags&HIT_MATERIAL) d->lastres[W_R_SHOCK] = lastmillis;
            else return true;
        }
        return false;
    }

    bool corrode(gameent *d, int weap, int flags)
    {
        if(wr_corrodes(weap, flags))
        {
            d->lastrestime[W_R_CORRODE] = lastmillis;
            if(isweap(weap) || flags&HIT_MATERIAL) d->lastres[W_R_CORRODE] = lastmillis;
            else return true;
        }
        return false;
    }

    VAR(IDF_READONLY, lastdamagetick, 0, 0, INT_MAX);
    static int damagemergeseqid = 0;

    struct damagemerge
    {

        enum { HURT = 0, BURN, BLEED, SHOCK, CORRODE, MAX };

        gameent *to, *from;
        int type, weap, flags, fromweap, fromflags, amt,
            millis, ready, delay, length, combine, seqid;

        damagemerge() : to(NULL), from(NULL), type(HURT), weap(-1), flags(0), fromweap(-1), fromflags(0), amt(0), millis(totalmillis ? totalmillis : 1), ready(0), delay(0), length(0), combine(0) { advance(); }
        damagemerge(gameent *d, gameent *v, int t, int w, int f, int fw, int ff, int m, int md = 0, int ml = 0, int mc = 0) :
            to(d), from(v), type(t), weap(w), flags(f), fromweap(fw), fromflags(ff), amt(m), millis(totalmillis ? totalmillis : 1), ready(0), delay(md), length(ml), combine(mc) { advance(); }

        void advance()
        {
            seqid = damagemergeseqid++;
            if(damagemergeseqid < 0) damagemergeseqid = 0;
        }

        bool merge(const damagemerge &m)
        {
            if(to != m.to || from != m.from || type != m.type || delay != m.delay || length != m.length || combine != m.combine)
                return false;

            if(ready && (!combine || totalmillis - ready > combine))
                return false;

            amt += m.amt;
            flags |= m.flags;
            return true;
        }

        void effect()
        {
            if(playdamagetones && from == focus && (damagetoneself || to != focus))
            {
                int snd = -1;
                float gain = damagetonegain;

                if(m_team(gamemode, mutators) && from == focus && to->team == from->team && from != to)
                {
                    snd = S_ALARM;
                    gain = damagetonealarm;
                }
                else if(type == BURN) snd = S_BURNED;
                else if(type == BLEED) snd = S_BLEED;
                else if(type == SHOCK) snd = S_SHOCK;
                else if(type == CORRODE) snd = S_CORRODE;
                else
                {
                    const float dmgsnd[8] = { 0, 0.1f, 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };
                    int hp = to->gethealth(gamemode, mutators);

                    loopirev(8) if(amt >= hp * dmgsnd[i])
                    {
                        snd = S_DAMAGE + i;
                        break;
                    }
                }

                if(gain > 0 && snd >= 0) emitsound(snd, getplayersoundpos(to), to, NULL, SND_CLAMPED, gain);
            }

            if(from == player1 && from != to && type == HURT)
            {
                lastdamagetick = lastmillis;
                if(playdamageticks) emitsound(S_DAMAGE_TICK, getplayersoundpos(from), from);
            }

            ready = totalmillis;
        }
    };

    vector<damagemerge> damagemerges;

    void removedamagemergeall()
    {
        loopvrev(damagemerges) damagemerges.remove(i);
    }

    void removedamagemerges(gameent *d)
    {
        loopvrev(damagemerges)
        {
            damagemerge &m = damagemerges[i];
            if(m.to != d && m.from != d) continue;
            damagemerges.remove(i);
        }
    }

    void flushdamagemerges()
    {
        loopv(damagemerges)
        {
            damagemerge &m = damagemerges[i];
            if(m.ready)
            {
                if(totalmillis - m.ready >= m.length) damagemerges.remove(i--);
                continue;
            }

            if(totalmillis - m.millis >= m.delay) m.effect();
        }
    }

    void checkdamagemerges()
    {
        static int lastcheck = 0; // ensure state is updated
        if(totalmillis == lastcheck) return;
        flushdamagemerges();
        lastcheck = totalmillis;
    }

    void pushdamagemerge(gameent *d, gameent *v, int type, int weap, int flags, int fromweap, int fromflags, int damage, int delay, int length, int combine)
    {
        checkdamagemerges();

        damagemerge dt(d, v, type, weap, flags, fromweap, fromflags, damage, delay, length, combine);
        loopv(damagemerges) if(damagemerges[i].merge(dt)) return;

        damagemerges.add(dt);
    }

    bool candamagemerge(gameent *d, const damagemerge &m)
    {
        if(m.to != d || m.from == d || !m.to->isalive() || m.amt <= 0) return false;
        if(m.to->actortype >= A_ENEMY && m.from->actortype >= A_ENEMY) return false;
        if(m_team(gamemode, mutators) && m.to->team == m.from->team) return false;

        return true;
    }

    gameent *bestdamagemerge(gameent *d)
    {
        static vector<int> totaldamage;
        totaldamage.setsize(0);
        checkdamagemerges();

        gameent *best = NULL;
        int bestamt = 0;
        loopv(damagemerges)
        {
            damagemerge &m = damagemerges[i];

            if(!candamagemerge(d, m)) continue;
            while(m.to->clientnum >= totaldamage.length()) totaldamage.add(0);

            totaldamage[m.to->clientnum] += m.amt;
            if(!best || totaldamage[m.to->clientnum] > bestamt)
            {
                bestamt = totaldamage[m.to->clientnum];
                best = m.to;
            }
        }

        return best;
    }

    bool hasdamagemerge(gameent *d, gameent *e)
    {
        checkdamagemerges();

        loopv(damagemerges)
        {
            damagemerge &m = damagemerges[i];
            if(m.from != e || !candamagemerge(d, m)) continue;
            return true;
        }

        return false;
    }

    int numdamagemerges(int from, int to)
    {
        checkdamagemerges();

        int count = 0;
        loopv(damagemerges)
        {
            damagemerge &m = damagemerges[i];
            if(from >= 0 && m.from->clientnum != from) continue;
            if(to >= 0 && m.to->clientnum != to) continue;
            count++;
        }

        return count;
    }

    float damagescale(dynent *d, int delay)
    {
        if(!delay || !gameent::is(d) || d->isobserver()) return 0.0f;

        checkdamagemerges();

        float amt = 0.0f;
        gameent *e = (gameent *)d;
        int spawnhealth = e->gethealth(gamemode, mutators);
        loopv(damagemerges)
        {
            damagemerge &m = damagemerges[i];
            if(d != m.to || m.amt <= 0) continue;

            int offset = totalmillis - m.millis;
            if(offset >= delay) continue;
            amt += m.amt / float(spawnhealth) * (1.0f - clamp(offset / float(delay), 0.0f, 1.0f));
        }

        return clamp(amt, 0.0f, 1.0f);
    }

    float criticalscale(dynent *d)
    {
        if(!gameent::is(d) || d->isobserver()) return 0.0f;

        if(d->isnotalive()) return 1.0f;

        gameent *e = (gameent *)d;
        int spawnhealth = e->gethealth(gamemode, mutators);
        float crithealth = spawnhealth * damagecritical;
        return crithealth < spawnhealth && e->health <= crithealth ? 1.0f - e->health / crithealth : 0.0f;
    }

    ICOMMAND(0, getdamages, "", (), checkdamagemerges(); intret(damagemerges.length()));
    ICOMMAND(0, getdamagefrom, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].from->clientnum : -1));
    ICOMMAND(0, getdamageclient, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].to->clientnum : -1));
    ICOMMAND(0, getdamagetype, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].type : -1));
    ICOMMAND(0, getdamageweap, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].weap : -1));
    ICOMMAND(0, getdamageflags, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].flags : 0));
    ICOMMAND(0, getdamagefromweap, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].fromweap : -1));
    ICOMMAND(0, getdamagefromflags, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].fromflags : 0));
    ICOMMAND(0, getdamageamt, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].amt : 0));
    ICOMMAND(0, getdamagemillis, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].millis : 0));
    ICOMMAND(0, getdamageready, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].ready : 0));
    ICOMMAND(0, getdamagedelay, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].delay : 0));
    ICOMMAND(0, getdamagecombine, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].combine : 0));
    ICOMMAND(0, getdamagelength, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].length : 0));
    ICOMMAND(0, getdamageseqid, "b", (int *n), checkdamagemerges(); intret(damagemerges.inrange(*n) ? damagemerges[*n].seqid : -1));
    ICOMMAND(0, getdamagemerges, "bb", (int *f, int *t), intret(numdamagemerges(*f, *t)));

    #define LOOPDAMAGE(name,op) \
        ICOMMAND(0, loopdamage##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            checkdamagemerges(); \
            if(damagemerges.empty()) return; \
            loopstart(id, stack); \
            op(damagemerges, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        }); \
        ICOMMAND(0, loopdamage##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            checkdamagemerges(); \
            if(damagemerges.empty()) return; \
            loopstart(id, stack); \
            op(damagemerges, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                if(executebool(cond)) execute(body); \
            }); \
            loopend(id, stack); \
        }); \
        ICOMMAND(0, loopdamage##name##while, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            checkdamagemerges(); \
            if(damagemerges.empty()) return; \
            loopstart(id, stack); \
            op(damagemerges, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                if(!executebool(cond)) break; \
                execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPDAMAGE(,loopcsv);
    LOOPDAMAGE(rev,loopcsvrev);

    void hiteffect(int weap, int flags, int fromweap, int fromflags, int damage, gameent *d, gameent *v, vec &dir, vec &vel, float dist, bool local)
    {
        bool burnfunc = burn(d, weap, flags), bleedfunc = bleed(d, weap, flags), shockfunc = shock(d, weap, flags), corrodefunc = corrode(d, weap, flags), material = flags&HIT_MATERIAL;

        if(!local || burnfunc || bleedfunc || shockfunc || corrodefunc || material)
        {
            float scale = isweap(weap) && WF(WK(flags), weap, damage, WS(flags)) != 0 ? abs(damage)/float(WF(WK(flags), weap, damage, WS(flags))) : 1.f;

            if(hitdealt(flags) && damage > 0)
            {
                vec p = d->headpos(-d->height/4);
                int hp = max(d->gethealth(gamemode, mutators)/5, 1);

                if(!nogore && bloodscale > 0)
                    part_splash(PART_BLOOD, int(clamp(damage/hp, 1, 3)*bloodscale)*(bleedfunc || material ? 2 : 1), bloodfade, p, A(d->actortype, abilities)&(1<<A_A_LIVING) ? 0x209090 : 0xE0E0E0, (rnd((bloodsize+1)/2)+((bloodsize+1)/2))/10.f, 1, 0, 0, 100, 1+STAIN_BLOOD, int(d->radius), 10);
                else if(bloodscale <= 0 || bloodsparks)
                    part_splash(PART_PLASMA, int(clamp(damage/hp, 1, 3))*(bleedfunc || material ? 2: 1), bloodfade, p, A(d->actortype, abilities)&(1<<A_A_LIVING) ? 0x902020 : 0x101010, 1, 0.5f, 0, 0, 50, 1+STAIN_STAIN, int(d->radius));

                int damagetype = damagemerge::HURT;
                if(burnfunc) damagetype = damagemerge::BURN;
                else if(bleedfunc) damagetype = damagemerge::BLEED;
                else if(shockfunc) damagetype = damagemerge::SHOCK;
                else if(corrodefunc) damagetype = damagemerge::CORRODE;
                pushdamagemerge(d, v, damagetype, weap, flags, fromweap, fromflags, damage, damagemergedelay, damagemergetime, damagemergecombine);

                if(!material && damagetype == damagemerge::HURT)
                {
                    if(d == v || (m_team(gamemode, mutators) && d->team == v->team))
                        v->lastteamhit = totalmillis ? totalmillis : 1;
                    else v->lasthit = totalmillis ? totalmillis : 1;
                }

                if(!issound(d->plchan[PLCHAN_VOICE])) emitsound(S_PAIN, getplayersoundpos(d), d, &d->plchan[PLCHAN_VOICE]);
                d->lastpain = lastmillis;

                if(isweap(weap) && !WK(flags)) emitsoundpos(WSND2(weap, WS(flags), S_W_IMPACT), vec(d->center()).add(vec(dir).mul(dist)), NULL, 0, clamp(scale, 0.2f, 1.f));

                if(isweap(weap) && !burnfunc && !bleedfunc && !material && !shockfunc && !corrodefunc && WF(WK(flags), weap, damage, WS(flags)) != 0)
                    projs::updateattract(d, v, weap, flags);
            }

            if(A(d->actortype, abilities)&(1<<A_A_PUSHABLE))
            {
                if(weap == -1 && shockfunc && d->shockstun)
                {
                    float amt = WRS(flags&HIT_WAVE || !hitdealt(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators);
                    if(m_dm_gladiator(gamemode, mutators))
                    {
                        float extra = flags&HIT_WAVE || !hitdealt(flags) ? gladiatorextrawavestunscale : (d->health <= 0 ? gladiatorextradeadstunscale : gladiatorextrahitstunscale);
                        amt *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                    }

                    float s = d->shockstunscale*amt, g = d->shockstunfall*amt;
                    d->addstun(weap, lastmillis, d->shockstuntime, d->shockstun&W_N_STADD ? s : 0.f, d->shockstun&W_N_GRADD ? g : 0.f);

                    if(d->shockstun&W_N_STIMM && s > 0) d->vel.mul(1.f-clamp(s, 0.f, 1.f));
                    if(d->shockstun&W_N_GRIMM && g > 0) d->falling.mul(1.f-clamp(g, 0.f, 1.f));
                    if(d->shockstun&W_N_SLIDE) d->impulse[IM_SLIP] = lastmillis;
                }
                else if(isweap(weap) && !burnfunc && !bleedfunc && !material && !shockfunc && !corrodefunc && WF(WK(flags), weap, damage, WS(flags)) != 0)
                {
                    if(WF(WK(flags), weap, stun, WS(flags)) != 0)
                    {
                        int stun = WF(WK(flags), weap, stun, WS(flags));
                        float amt = scale*WRS(flags&HIT_WAVE || !hitdealt(flags) ? wavestunscale : (d->health <= 0 ? deadstunscale : hitstunscale), stun, gamemode, mutators);
                        if(m_dm_gladiator(gamemode, mutators))
                        {
                            float extra = flags&HIT_WAVE || !hitdealt(flags) ? gladiatorextrawavestunscale : (d->health <= 0 ? gladiatorextradeadstunscale : gladiatorextrahitstunscale);
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
                            float modify = WRS(flags&HIT_WAVE || !hitdealt(flags) ? wavepushscale : (d->health <= 0 ? deadpushscale : hitpushscale), push, gamemode, mutators);
                            if(m_dm_gladiator(gamemode, mutators))
                            {
                                float extra = flags&HIT_WAVE || !hitdealt(flags) ? gladiatorextrawavepushscale : (d->health <= 0 ? gladiatorextradeadpushscale : gladiatorextrahitpushscale);
                                modify *= d->gethealth(gamemode, mutators)/max(d->health, 1)*extra;
                            }
                            d->vel.add(vec(dir).mul(hit*modify));
                            if(doquake) d->quake = min(d->quake+max(int(hit), 1), quakelimit);
                        }

                        hit = WF(WK(flags), weap, hitvel, WS(flags))*amt;

                        if(hit != 0)
                        {
                            float modify = WRS(flags&HIT_WAVE || !hitdealt(flags) ? wavevelscale : (d->health <= 0 ? deadvelscale : hitvelscale), vel, gamemode, mutators);
                            if(m_dm_gladiator(gamemode, mutators))
                            {
                                float extra = flags&HIT_WAVE || !hitdealt(flags) ? gladiatorextrawavevelscale : (d->health <= 0 ? gladiatorextradeadvelscale : gladiatorextrahitvelscale);
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

    void damaged(int weap, int flags, int fromweap, int fromflags, int damage, int health, gameent *d, gameent *v, int millis, vec &dir, vec &vel, float dist)
    {
        if(!gs_playing(gamestate) || !d->isalive()) return;
        if(hitdealt(flags))
        {
            if(!m_insta(gamemode, mutators) && damagecritical > 0 && damagecriticalsound&(d == focus ? 1 : 2))
            {
                int hp = d->gethealth(gamemode, mutators), crit = int(hp*damagecritical);
                if(d->health > crit && health <= crit) emitsound(S_CRITICAL, getplayersoundpos(d), d);
            }
            d->health = health;
            if(damage > 0)
            {
                d->lastregen = d->lastregenamt = 0;
                d->lastpain = lastmillis;
                v->totaldamage += damage;
            }
        }
        hiteffect(weap, flags, fromweap, fromflags, damage, d, v, dir, vel, dist, v == player1 || v->ai);
    }

    vec gibpos(gameent *d, int n)
    {
        if(!d) return vec(0);
        if(n < 0 || n >= actors[d->actortype].parts) return d->center();

        const vec *tag = NULL;
        vec p = vec(0, 0, 0);
        switch(d->actortype)
        {
            case A_PLAYER: case A_BOT: tag = d->gettag(playerparts[n].tag); p = vec(playerparts[n].x, playerparts[n].y, playerparts[n].z); break;
            case A_JANITOR: tag = d->gettag(janitorparts[n].tag); p = vec(janitorparts[n].x, janitorparts[n].y, janitorparts[n].z); break;
            default: break;
        }

        if(!tag) return d->center();

        return vec(*tag).add(vec(d->yaw * RAD, 0.f).mul(vec(p.x, p.y, 0.f).mul(d->radius))).addz(p.z * d->zradius);
    }

    void spawngibs(int weap, int flags, int damage, gameent *d, gameent *v, vector<gameent *> &assist, int style, int material)
    {
        if(giblimit)
        {
            int gibcount = 0, missed = 0;
            if(gibchancevanity && (d->headless || d->obliterated))
            {
                int head = vanitybuild(d), found[VANITYMAX] = {0};
                bool check = vanitycheck(d);
                loopvk(d->vitems)
                {
                    int n = d->vitems[k];
                    if(!vanities.inrange(n)) continue;

                    if(found[vanities[n].type]) continue; // skip ignored vanities
                    found[vanities[n].type]++;

                    if(!(vanities[n].cond&(d->obliterated ? 4 : 2))) continue;
                    if(vanities[n].type ? !check || rnd(101) > gibchancevanity : !actors[d->actortype].hashead)
                    {
                        missed++;
                        continue;
                    }

                    vec pos = d->center();
                    switch(vanities[n].type)
                    {
                        case 0: case 2: case 3: case 4: case 5: case 6: // lots of head stuff
                            pos = d->headtag();
                            break;
                        case 7: // back stuff
                            pos = d->jetbacktag();
                            break;
                        case 8: case 9: // feet stuff
                            pos = d->toetag(vanities[n].type - 8);
                            break;
                        case 10: // left shoulder
                            pos = d->jetlefttag();
                            break;
                        case 11: // right shoulder
                            pos = d->jetrighttag();
                            break;
                        case 12: // tail
                            pos = d->waisttag();
                            break;
                        case 1: default: break; // central stuff
                    }

                    projs::create(pos, vec(pos).addz(rnd(d->obliterated ? 64 : 8)), true, d, PROJ_VANITY, weap, flags, rnd(gibfade) + gibfade, 0, 0, rnd(50) + 10, -1, n, head);

                    if(++gibcount >= giblimit) return;
                }
            }

            if(gibchancepieces && (d->actortype != A_JANITOR || !(flags&HIT_JANITOR)))
            {
                if(d->obliterated && actors[d->actortype].parts)
                {
                    loopi(actors[d->actortype].parts)
                    {
                        if(rnd(101) > gibchancepieces)
                        {
                            missed++;
                            continue;
                        }

                        vec pos = gibpos(d, i);
                        projs::create(pos, vec(pos).addz(rnd(64)), true, d, PROJ_PIECE, weap, flags, rnd(gibfade) + gibfade, 0, 0, rnd(50) + 10, -1, i);

                        if(++gibcount >= giblimit) return;
                    }
                }
                else missed += actors[d->actortype].parts;

                int hp = max(d->gethealth(gamemode, mutators), 1), divisor = int(ceilf(d->obliterated ? hp * gibobliterated : (d->headless ? hp * gibheadless : hp * gibdamage))),
                    count = max(int(ceilf(damage / float(divisor))), 1), amt = clamp(rnd(count) + count + missed, 1, gibpieces);

                loopi(amt)
                {
                    if(rnd(101) > gibchancepieces) continue;

                    vec pos = gibpos(d, -1);
                    projs::create(pos, vec(pos).addz(rnd(d->obliterated ? 64 : 8)), true, d, A(d->actortype, abilities)&(1<<A_A_LIVING) ? PROJ_GIB : PROJ_DEBRIS, weap, flags, rnd(gibfade) + gibfade, 0, rnd(100) + 1, rnd(d->obliterated || d->headless ? 50 : 25) + 10);

                    if(++gibcount >= giblimit) return;
                }
            }
        }

        if(gibchancecollects)
        {
            int count = 0, limit = max(giblimit, 5);

            while(!d->collects.empty())
            {
                int n = rnd(d->collects.length());
                if(rnd(101) > gibchancecollects)
                {
                    d->collects.remove(n);
                    continue; // let's just say it gets destroyed
                }

                projent *p = projs::create(d->o, vec(d->o).add(vec(rnd(21)-10, rnd(21)-10, rnd(61)-10)), true, d, d->collects[n].type, weap, flags, (rnd(gibfade) + gibfade) / 4, 0, rnd(500) + 1, rnd(d->obliterated ? 50 : 25) + 10);
                if(p)
                {
                    p->mdlname = d->collects[n].name;
                    p->lifesize = clamp((d->collects[n].size * 0.25f) + ((rnd(31) - 10) / 100.f), 0.05f, 0.25f);
                }

                d->collects.remove(n);
                if(++count >= limit) return;
            }
        }
    }

    void killed(int weap, int flags, int fromweap, int fromflags, int damage, gameent *d, gameent *v, vector<gameent *> &assist, int style, int material)
    {
        d->lastregen = d->lastregenamt = 0;
        d->lastpain = lastmillis;
        d->state = CS_DEAD;
        d->obliterated = (style&FRAG_OBLITERATE) != 0 || (flags&HIT_PRIZE) != 0;
        d->headless = (style&FRAG_HEADSHOT) != 0;

        bool burnfunc = burn(d, weap, flags), bleedfunc = bleed(d, weap, flags), shockfunc = shock(d, weap, flags), corrodefunc = corrode(d, weap, flags);
        int curmat = material&MATF_VOLUME;

        if(d != player1) d->resetinterp();
        formatstring(d->obit, "%s ", colourname(v));

        if(d != v && v->lastattacker == d->clientnum) v->lastattacker = -1;
        d->lastattacker = v->clientnum;

        const char *obitctx = "";
        if(d == v)
        {
            if(flags&HIT_SPAWN) obitctx = obitspawn;
            else if(flags&HIT_TOUCH) obitctx = *obittouch ? obittouch : obitsplat;
            else if(flags&HIT_CRUSH) obitctx = *obitcrush ? obitcrush : obitsquish;
            else if(flags&HIT_SPEC) obitctx = obitspectator;
            else if(flags&HIT_MATERIAL && curmat&MAT_WATER && getwaterenabled(curmat)) obitctx = getobitwater(material, obitdrowned);
            else if(flags&HIT_MATERIAL && curmat&MAT_LAVA && getlavaenabled(curmat)) obitctx = getobitlava(material, obitmelted);
            else if(flags&HIT_MATERIAL && curmat&MAT_VOLFOG && getvolfogenabled(curmat)) obitctx = getobitvolfog(material, obitchoked);
            else if(flags&HIT_MATERIAL && material&MAT_HURT) obitctx = *obithurt ? obithurt : obithurtmat;
            else if(flags&HIT_MATERIAL) obitctx = *obitdeath ? obitdeath : obitdeathmat;
            else if(flags&HIT_CHECKPOINT) obitctx = *obitcheckpoint ? obitcheckpoint : obitwrongcp;
            else if(flags && isweap(weap) && !burnfunc && !bleedfunc && !shockfunc && !corrodefunc) obitctx = WF(WK(flags), weap, obitsuicide, WS(flags));
            else if(flags&HIT_BURN || burnfunc) obitctx = obitburnself;
            else if(flags&HIT_BLEED || bleedfunc) obitctx = obitbleedself;
            else if(flags&HIT_SHOCK || shockfunc) obitctx = obitshockself;
            else if(flags&HIT_CORRODE || corrodefunc) obitctx = obitcorrodeself;
            else if(d->obliterated) obitctx = obitobliterated;
            else if(d->headless) obitctx = obitheadless;
            else if(flags&HIT_LOST) obitctx = *obitfall ? obitfall : obitlost;
            else if(flags&HIT_JANITOR) obitctx = obitjanitor;
            else obitctx = obitsuicide;

            concatstring(d->obit, obitctx);
        }
        else
        {
            if(damageinteger) concformatstring(d->obit, "(\fs\fc%d\fS) ", int(ceilf(v->health/damagedivisor)));
            else concformatstring(d->obit, "(\fs\fc%.1f\fS) ", v->health/damagedivisor);

            if(!assist.empty())
            {
                loopv(assist) if(assist[i])
                {
                    concformatstring(d->obit, "+ %s ", colourname(assist[i]));

                    if(damageinteger) concformatstring(d->obit, "(\fs\fc%d\fS) ", int(ceilf(assist[i]->health/damagedivisor)));
                    else concformatstring(d->obit, "(\fs\fc%.1f\fS) ", assist[i]->health/damagedivisor);
                }
            }

            if(burnfunc) obitctx = obitburn;
            else if(bleedfunc) obitctx = obitbleed;
            else if(shockfunc) obitctx = obitshock;
            else if(corrodefunc) obitctx = obitcorrode;
            else if(isweap(weap))
            {
                if(d->obliterated) obitctx = WF(WK(flags), weap, obitobliterated, WS(flags));
                else if(d->headless) obitctx = WF(WK(flags), weap, obitheadless, WS(flags));
                else obitctx = WF(WK(flags), weap, obituary, WS(flags));
            }
            else obitctx = obitkilled;

            concatstring(d->obit, obitctx);
            if(m_team(gamemode, mutators) && d->team == v->team) concatstring(d->obit, " \fs\fzawteam-mate\fS");
            concformatstring(d->obit, " %s", colourname(d));

            if(style&FRAG_REVENGE)
            {
                concatstring(d->obit, " \fs\fzoyrevenge\fS");
                v->dominator.removeobj(d);
            }

            if(style&FRAG_DOMINATE)
            {
                concatstring(d->obit, " \fs\fzoydominating\fS");
                if(d->dominator.find(v) < 0) d->dominator.add(v);
            }

            if(style&FRAG_BREAKER)
            {
                concatstring(d->obit, " \fs\fzpwspree-breaking\fS");
            }

            if(style&FRAG_MKILL1)
            {
                if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                concatstring(d->obit, " \fs\fzcwdouble-killing\fS");
            }

            if(style&FRAG_MKILL2)
            {
                if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                concatstring(d->obit, " \fs\fzcwtriple-killing\fS");
            }

            if(style&FRAG_MKILL3)
            {
                if(style&FRAG_BREAKER) concatstring(d->obit, " and");
                concatstring(d->obit, " \fs\fzcwmulti-killing\fS");
            }

            if(style&FRAG_FIRSTBLOOD)
            {
                concatstring(d->obit, " for \fs\fzrwfirst blood\fS");
            }

            if(style&FRAG_SPREE1)
            {
                concatstring(d->obit, " in total \fs\fzywcarnage\fS");
            }

            if(style&FRAG_SPREE2)
            {
                concatstring(d->obit, " on a \fs\fzywslaughter\fS");
            }

            if(style&FRAG_SPREE3)
            {
                concatstring(d->obit, " on a \fs\fzywmassacre\fS");
            }

            if(style&FRAG_SPREE4)
            {
                concatstring(d->obit, " in a \fs\fzyibloodbath\fS");
            }
        }

        if(d != v)
        {
            concformatstring(d->obit, " \fs\fo@\fy%.2f\fom\fS", v->o.dist(d->o)/8.f);
            if(v->isalive() && d->actortype < A_ENEMY)
            {
                copystring(v->obit, d->obit);
                v->lastkill = totalmillis ? totalmillis : 1;
            }
        }

        if(issound(d->plchan[PLCHAN_ALERT])) soundsources[d->plchan[PLCHAN_ALERT]].clear();
        if(flags&HIT_PRIZE) prizeeffect(d);

        // if(gs_playing(gamestate) && (d->actortype < A_ENEMY || d->actortype == A_JANITOR))
        // {
            gamelog *log = new gamelog(GAMELOG_DEATH);
            log->addlist("args", "type", d == v ? "suicide" : "frag");
            log->addlist("args", "flags", GAMELOG_F_CLIENT2);
            log->addlist("args", "actweap", weap);
            log->addlist("args", "actflags", flags);
            log->addlist("args", "fromweap", fromweap);
            log->addlist("args", "fromflags", fromflags);
            if(d->hasprize > 0) log->addlist("args", "prize", d->hasprize);
            log->addlist("args", "damage", damage);
            log->addlist("args", "style", style);
            log->addlist("args", "material", material);
            log->addlist("args", "burnfunc", burnfunc);
            log->addlist("args", "bleedfunc", bleedfunc);
            log->addlist("args", "shockfunc", shockfunc);
            log->addlist("args", "corrodefunc", corrodefunc);
            log->addlist("args", "distance", v->o.dist(d->o)/8.f);
            log->addlist("args", "context", obitctx);
            log->addlist("args", "console", d->obit);
            log->addclient("client", d);
            log->addclient("client", v);
            loopv(assist) if(assist[i] && assist[i] != d && assist[i] != v) log->addclient("client", assist[i]);
            if(!log->push()) DELETEP(log);
        // }

        if(m_bomber(gamemode)) bomber::killed(d, v);
        ai::killed(d, v);

        spawngibs(weap, flags, damage, d, v, assist, style, material);
        emitsound(d->actortype >= A_ENEMY || d->obliterated ? S_SPLOSH : S_DEATH, getplayersoundpos(d), d, &d->plchan[PLCHAN_VOICE]);
    }

    void timeupdate(int state, int remain, int elapsed, int wait)
    {
        int oldstate = gamestate;
        gamestate = state;
        timeremaining = remain;
        timeelapsed = elapsed;
        timelast = gs_playing(gamestate) ? lastmillis : totalmillis;
        timewait = wait;

        if(gs_playing(gamestate) != gs_playing(oldstate)) entities::updaterails();

        if(gs_intermission(gamestate) && gs_playing(oldstate))
        {
            player1->stopmoving(true);
            if(gamestate == G_S_INTERMISSION) hud::showscores(true, true);
        }

        if(gamestate == G_S_VOTING && oldstate != G_S_VOTING)
        {
            hud::showscores(false);
            triggereventcallbacks(CMD_EVENT_GAME_VOTE);
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
            if(client::showpresencehostinfo && client::haspriv(player1, G(iphostlock))) formatstring(ipaddr, " (%s)", d->hostip);
            if(d->actortype == A_PLAYER)
            {
                int amt = client::otherclients(); // not including self to disclude this player
                client::echomsg(colourorange, "%s%s left the game%s (%d %s)", colourname(d), ipaddr, formattedreason, amt, amt != 1 ? "players" : "player");
            }
            else if(d->actortype == A_BOT && ai::showaiinfo)
                client::echomsg(colourorange, "%s was removed from the game%s", colourname(d), formattedreason);
        }
        gameent *e = NULL;
        int numdyns = numdynents();
        loopi(numdyns) if((e = (gameent *)iterdynents(i)))
        {
            e->dominator.removeobj(d);
            if(e->ai) loopvj(e->ai->state)
            {
                if(e->ai->state[j].owner == cn) e->ai->state[j].owner = -1;
                if(e->ai->state[j].targtype == ai::AI_T_ACTOR && e->ai->state[j].target == cn) e->ai->state.remove(j--);
            }
        }
        specreset(d, true);
        waiting.removeobj(d);
        gamelog::removeclient(d);
        client::clearvotes(d);
        hud::removeplayer(d);
        projs::removeplayer(d);
        removedamagemerges(d);
        if(m_capture(gamemode)) capture::removeplayer(d);
        else if(m_defend(gamemode)) defend::removeplayer(d);
        else if(m_bomber(gamemode)) bomber::removeplayer(d);
        DELETEP(players[cn]);
        players[cn] = NULL;
        cleardynentcache();
        fx::clear();
        clearwindemitters();
    }

    void preload()
    {
        ai::preload();
        weapons::preload();
        projs::preload();
        loopv(mixers) loopj(2) mixers[i].loadtex(j!=0);
        if(m_edit(gamemode) || m_capture(gamemode)) capture::preload();
        if(m_edit(gamemode) || m_defend(gamemode)) defend::preload();
        if(m_edit(gamemode) || m_bomber(gamemode)) bomber::preload();
        flushpreloadedmodels();
    }

    void resetmap(bool empty) // called just before a map load
    {
        loopi(SURFACE_ALL) UI::hideui(NULL, i);
    }

    void savemap(bool force, const char *mname)
    {
        ai::savemap(force, mname);
    }

    VAR(IDF_PERSIST, showloadoutmenu, 0, 0, 1); // show the loadout menu at the start of a map

    void startmap(bool empty) // called just after a map load
    {
        if(!*player1->mixer && !mixers.empty())
        {
            int r = rnd(mixers.length());
            if(mixers.inrange(r)) setsvar("playermixer", mixers[r].id, true);
        }

        checklights(true);
        ai::startmap(empty);
        if(!empty)
        {
            gamestate = G_S_WAITING;
            mapstart = maptime = timesync = timelast = timewait = 0;
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
        if(showloadoutmenu && m_loadout(gamemode, mutators)) triggereventcallbacks(CMD_EVENT_GAME_LOADOUT);
        resetcamera();
        resetsway();

        if(!empty) triggereventcallbacks(CMD_EVENT_MAPLOAD);
    }

    gameent *intersectclosest(vec &from, vec &to, gameent *at)
    {
        gameent *best = NULL, *o;
        float bestdist = 1e16f;
        int numdyns = numdynents();
        loopi(numdyns) if((o = (gameent *)iterdynents(i)))
        {
            if(!o || o == at || !o->isalive() || !physics::issolid(o, at)) continue;
            float dist;
            if(intersect(o, from, to, dist, GUARDRADIUS) && dist < bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        return best;
    }

    int numdynents(int level)
    {
        int i = 1+players.length();
        if(level)
        {
            i += level > 1 ? projs::projs.length() : projs::collideprojs.length();
            i += entities::inanimates.length();
        }
        return i;
    }

    dynent *iterdynents(int i, int level)
    {
        if(!i) return player1;
        i--;
        if(i < players.length()) return players[i];
        i -= players.length();
        if(level)
        {
            if(level > 1)
            {
                if(i < projs::projs.length()) return projs::projs[i];
                i -= projs::projs.length();
            }
            else
            {
                if(i < projs::collideprojs.length()) return projs::collideprojs[i];
                i -= projs::collideprojs.length();
            }
            if(i < entities::inanimates.length()) return entities::inanimates[i];
        }
        return NULL;
    }

    dynent *focusedent(bool force)
    {
        if(force) return player1;
        return focus;
    }

    bool duplicatename(const char *name, int clientnum)
    {
        if(!client::demoplayback && clientnum != player1->clientnum && !strcmp(name, player1->name)) return true;
        loopv(players) if(players[i] && clientnum != players[i]->clientnum && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    bool duplicatename(gameent *d, const char *name = NULL)
    {
        if(!name) name = d->name;
        return duplicatename(name, d->clientnum);
    }

    int levelcolour(int colour, float level)
    {
        if(level != 1)
            return (clamp(int((colour>>16) * level), 0, 255)<<16) | (clamp(int(((colour>>8) & 0xFF) * level), 0, 255)<<8) | (clamp(int((colour & 0xFF) * level), 0, 255));
        return colour;
    }

    int findcolour(int team, int colour, int weapselect, bool tone, float level, float mix)
    {
        if(tone)
        {
            if(mix > 0)
            {
                int r1 = (colour>>16), g1 = ((colour>>8) & 0xFF), b1 = (colour & 0xFF),
                    c = TEAM(team, colour), r2 = (c>>16), g2 = ((c>>8) & 0xFF), b2 = (c & 0xFF),
                    r3 = clamp(int((r1 * (1 - mix)) + (r2 * mix)), 0, 255),
                    g3 = clamp(int((g1 * (1 - mix)) + (g2 * mix)), 0, 255),
                    b3 = clamp(int((b1 * (1 - mix)) + (b2 * mix)), 0, 255);

                colour = (r3<<16) | (g3<<8) | b3;
            }

            return levelcolour(colour, level);
        }

        return levelcolour(TEAM(team, colour), level);
    }

    int findcolour(gameent *d, int comb, bool tone, float level, float mix)
    {
        if(prizeeffects&4 && (d != focus || prizeeffects&8) && d != previewent && d->isprize(focus) && (tone || d->actortype == A_JANITOR))
            return pulsehexcol(tone ? PULSE_READY : PULSE_PRIZE);

        int col = d->colours[comb ? 1 : 0];
        switch(comb)
        {
            case 2:
            {
                if(playercombinemix > 0)
                {
                    int r1 = (col>>16), g1 = ((col>>8) & 0xFF), b1 = (col & 0xFF),
                        r2 = (d->colours[0]>>16), g2 = ((d->colours[0]>>8) & 0xFF), b2 = (d->colours[0] & 0xFF),
                        r3 = clamp(int((r1 * (1 - playercombinemix)) + (r2 * playercombinemix)), 0, 255),
                        g3 = clamp(int((g1 * (1 - playercombinemix)) + (g2 * playercombinemix)), 0, 255),
                        b3 = clamp(int((b1 * (1 - playercombinemix)) + (b2 * playercombinemix)), 0, 255);
                    col = (r3<<16) | (g3<<8) | b3;
                    break;
                }

                col = d->colours[0];
                break;
            }
            case 1: col = d->colours[1]; break;
            case 0: default: break;
        }

        return findcolour(d->team, col, d->weapselect, tone, level, mix);
    }

    int getcolour(gameent *d, int type, float level, float mix)
    {
        switch(type)
        {
            case CTONE_TEAM: return findcolour(d, 0, false, level, 0); break;
            case CTONE_PRIMARY: return findcolour(d, 0, true, level, 0); break;
            case CTONE_SECONDARY: return findcolour(d, 1, true, level, 0); break;
            case CTONE_COMBINED: return findcolour(d, 2, true, level, 0); break;
            case CTONE_PRIMARY_TEAM: return findcolour(d, 0, d->team == T_NEUTRAL, level, 0); break;
            case CTONE_PRIMARY_ALONE: return findcolour(d, 0, d->team != T_NEUTRAL, level, 0); break;
            case CTONE_PRIMARY_MIX: return findcolour(d, 0, true, level, mix); break;
            case CTONE_PRIMARY_TEAM_MIX: return findcolour(d, 0, true, level, d->team != T_NEUTRAL ? mix : 0.f); break;
            case CTONE_PRIMARY_ALONE_MIX: return findcolour(d, 0, true, level, d->team == T_NEUTRAL ? mix : 0.f); break;
            case CTONE_SECONDARY_TEAM: return findcolour(d, 1, d->team == T_NEUTRAL, level, 0); break;
            case CTONE_SECONDARY_ALONE: return findcolour(d, 1, d->team != T_NEUTRAL, level, 0); break;
            case CTONE_SECONDARY_MIX: return findcolour(d, 1, true, level, mix); break;
            case CTONE_SECONDARY_TEAM_MIX: return findcolour(d, 1, true, level, d->team != T_NEUTRAL ? mix : 0.f); break;
            case CTONE_SECONDARY_ALONE_MIX: return findcolour(d, 1, true, level, d->team == T_NEUTRAL ? mix : 0.f); break;
            case CTONE_COMBINED_TEAM: return findcolour(d, 2, d->team == T_NEUTRAL, level, 0); break;
            case CTONE_COMBINED_ALONE: return findcolour(d, 2, d->team != T_NEUTRAL, level, 0); break;
            case CTONE_COMBINED_MIX: return findcolour(d, 2, true, level, mix); break;
            case CTONE_COMBINED_TEAM_MIX: return findcolour(d, 2, true, level, d->team != T_NEUTRAL ? mix : 0.f); break;
            case CTONE_COMBINED_ALONE_MIX: return findcolour(d, 2, true, level, d->team == T_NEUTRAL ? mix : 0.f); break;
            case -1: return findcolour(d, 0, true, level, 0); break;
            case -2: default: return levelcolour(d->colours[0], level); break;
        }

        return 0;
    }

    const char *colourname(const char *name, int clientnum, int team, int actortype, int col, int privilege, int weapselect, bool icon, bool dupname, int colour)
    {
        static string colored;
        colored[0] = 0;
        if(!name || !*name) return colored;
        string colortmp;
        if(colour) concatstring(colored, "\fs");
        if(icon)
        {
            if(colour&1)
            {
                formatstring(colortmp, "\f[%d]", findcolour(team, col, weapselect));
                concatstring(colored, colortmp);
            }
            formatstring(colortmp, "\f($priv%stex)", server::privnamex(privilege, actortype, true));
            concatstring(colored, colortmp);
        }
        if(colour&2)
        {
            formatstring(colortmp, "\f[%d]", TEAM(team, colour));
            concatstring(colored, colortmp);
        }
        concatstring(colored, name);
        if(!name[0] || (actortype < A_ENEMY && dupname && duplicatename(name, clientnum)))
        {
            formatstring(colortmp, "%s[%d]", name[0] ? " " : "", clientnum);
            concatstring(colored, colortmp);
        }
        if(colour) concatstring(colored, "\fS");
        return colored;
    }
    ICOMMAND(0, colourname, "siiiiiiibb", (char *name, int *clientnum, int *team, int *actortype, int *col, int *privilege, int *weapselect, int *icon, int *dupname, int *colour),
        result(colourname(name, *clientnum, *team, *actortype, *col, *privilege, *weapselect, *icon != 0, *dupname != 0, *colour >= 0 ? *colour : 3));
    );

    const char *colourname(gameent *d, const char *name, bool icon, bool dupname, int colour)
    {
        if(!name) name = d->name;
        return colourname(name, d->clientnum, d->team, d->actortype, d->colours[0], d->privilege, d->weapselect, icon, dupname, colour);
    }

    const char *teamtexnamex(int team)
    {
        const char *teamtexs[T_MAX] = { "teamneutraltex", "teamalphatex", "teamomegatex", "teamenemytex", "teamenvtex" };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *colourteam(int team, const char *icon)
    {
        if(team < 0 || team > T_MAX) team = T_NEUTRAL;
        static string teamed;
        teamed[0] = '\0';
        concatstring(teamed, "\fs");
        concformatstring(teamed, "\f[%d]", TEAM(team, colour));
        if(icon != NULL) concformatstring(teamed, "\f($%s)", icon && *icon ? icon : teamtexnamex(team));
        concatstring(teamed, TEAM(team, name));
        concatstring(teamed, "\fS");
        return teamed;
    }

    ICOMMAND(0, getteamname, "ibs", (int *team, int *fmt, char *icon), result(*team >= 0 && *team < T_MAX ? (*fmt ? colourteam(*team, icon && *icon ? icon : NULL) : TEAM(*team, name)) : ""));
    ICOMMAND(0, getteamcolour, "i", (int *team), intret(*team >= 0 && *team < T_MAX ? TEAM(*team, colour) : 0));

    void suicide(gameent *d, int flags)
    {
        if((d != player1 && !d->ai) || !d->isalive() || d->suicided >= 0) return;
        burn(d, -1, flags);
        bleed(d, -1, flags);
        shock(d, -1, flags);
        corrode(d, -1, flags);
        client::addmsg(N_SUICIDE, "ri3", d->clientnum, flags, d->inmaterial);
        d->suicided = lastmillis;
    }
    ICOMMAND(0, suicide, "",  (), { suicide(player1); });

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

    void fxtrack(vec &pos, physent *owner, int mode, int tag)
    {
        if(!owner) return;
        physent *posent = owner == focus && focus->isnophys() ? camera1 : owner;

        switch(mode)
        {
            case ENT_POS_ORIGIN: pos = posent->o; break;
            case ENT_POS_BOTTOM: pos = posent->feetpos(); break;
            case ENT_POS_MIDDLE: pos = posent->feetpos(posent->height * 0.5f); break;
            case ENT_POS_TOP:    pos = posent->feetpos(posent->height); break;
            case ENT_POS_DIR:    pos.add(vec(posent->yaw*RAD, posent->pitch*RAD)); break;
            case ENT_POS_MUZZLE:
                if(gameent::is(posent))
                    pos = ((gameent *)posent)->muzzletag(tag);
                else pos = posent->o;
                break;
            case ENT_POS_TAG:
                if(gameent::is(posent) && tag >= 0 && tag < TAG_MAX)
                    pos = *((gameent *)posent)->gettag(tag);
                else pos = posent->o;
                break;
        }
    }

    void dynlighttrack(physent *owner, vec &o, vec &hud)
    {
        if(owner->type != ENT_PLAYER) return;
        physent *posent = owner == focus && focus->isnophys() ? camera1 : owner;
        o = posent->o;
        hud = owner == focus ? vec(posent->o).add(vec(0, 0, 2)) : posent->o;
    }

    void newmap(int size, const char *mname) { client::addmsg(N_NEWMAP, "ris", size, mname); }

    void fixview()
    {
        if(inzoom())
        {
            checkzoom();
            curfov = fov()-(zoomscale()*(fov()-(W(focus->weapselect, cookzoommax)-((W(focus->weapselect, cookzoommax)-W(focus->weapselect, cookzoommin))/float(zoomlevels)*zoomlevel))));
        }
        else curfov = float(fov());
    }

    VAR(0, mouseoverride, 0, 0, 3);
    bool mousemove(int dx, int dy, int x, int y, int w, int h)
    {
        #define mousesens(a,b,c) ((float(a)/float(b))*c)

        if(mouseoverride&2 || (!mouseoverride && hud::hasinput(true)))
        {
            float mousemovex = mousesens(dx, w, mousesensitivity);
            float mousemovey = mousesens(dy, h, mousesensitivity);

            UI::mousetrack(mousemovex, mousemovey);

            if(!UI::cursorlock())
            {
                cursorx = clamp(cursorx+mousemovex, 0.f, 1.f);
                cursory = clamp(cursory+mousemovey, 0.f, 1.f);
            }

            if(!(mouseoverride&1)) return true;
        }

        if(mouseoverride&1 || (!mouseoverride && !tvmode()))
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

    void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
    {
        float dist = from.dist(pos);
        yaw = -atan2(pos.x - from.x, pos.y - from.y) / RAD;
        pitch = dist > 0 ? asin((pos.z - from.z)/dist) / RAD : 0;
        fixrange(yaw, pitch);
    }

    void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float yawspeed, float pitchspeed, float rotate)
    {
        fixrange(yaw, pitch);
        fixrange(targyaw, targpitch);

        if(yaw < targyaw - 180.0f) yaw += 360.0f;
        if(yaw > targyaw + 180.0f) yaw -= 360.0f;

        float offyaw = (rotate < 0 ? fabs(rotate) : (rotate > 0 ? min(float(fabs(targyaw - yaw)), rotate) : fabs(targyaw - yaw))) * max(yawspeed, FVAR_NONZERO),
              offpitch = (rotate < 0 ? fabs(rotate) : (rotate > 0 ? min(float(fabs(targpitch - pitch)), rotate) : fabs(targpitch - pitch))) * max(pitchspeed, FVAR_NONZERO);

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
        if(!d->isalive()) return pos;
        if(firstpersoncamera && gameent::is(d) && d == focus) return ((gameent *)d)->cameratag();

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
        if(firstpersonbob && gs_playing(gamestate) && d->isalive())
        {
            float scale = 1;
            if(gameent::is(d) && d == focus && inzoom()) scale *= 1-zoomscale();
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
        if(level&(d->isnotalive() ? 1 : 2)) return true;
        return false;
    }

    vec camvec(cament *c, float yaw = 0, float pitch = 0)
    {
        if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
        {
            gameentity &e = *(gameentity *)entities::ents[c->id];
            return vec(e.pos()).addz(enttype[e.type].radius);
        }

        if(c->player) return camerapos(c->player, true, true, yaw, pitch);

        return thirdpos(c->o, yaw, pitch, followdist);;
    }

    void getcamyawpitch(cament *c, float &yaw, float &pitch, bool renew = false)
    {
        if(!c) return;

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
            if(dynamic && e.dynamic())
            {
                yaw += e.yaw;
                pitch += e.pitch;
            }
        }
        else if(c->player)
        {
            if(!c->player->isalive()) deathcamyawpitch(c->player, yaw, pitch);
            else
            {
                yaw = c->player->yaw;
                pitch = c->player->pitch;
            }
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
        maxdist = c && c->player ? spectvfollowmaxdist : spectvmaxdist;
        mindist = c && c->player ? spectvfollowmindist : spectvmindist;
        if(c && c->type == cament::ENTITY && entities::ents.inrange(c->id))
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

    bool camhalo(cament *c, cament *v)
    {
        if(c && v) switch(v->type)
        {
            case cament::AFFINITY:
            {
                if(m_capture(gamemode) && capture::haloallow(c->o, v->id, 0, true)) return true;
                if(m_defend(gamemode) && defend::haloallow(c->o, v->id, 0, true)) return true;
                if(m_bomber(gamemode) && bomber::haloallow(c->o, v->id, 0, true)) return true;
                // fall-through as affinities can be carried by players
            }
            case cament::PLAYER:
            {
                if(v->player && haloallow(c->o, v->player, false)) return true; // override and switch to x-ray
                break;
            }
            default: break;
        }

        return false;
    }

    bool camcansee(cament *c, cament *v, vec &from, float &yaw, float &pitch, float maxdist, float mindist, int iter = 0)
    {
        if(!c || !v || v->flagged || (iter != 5 && v->type == cament::ENTITY)) return false;

        if(!v->checked)
        {
            if(!v->player || v->player == c->player || !allowspec(v->player, spectvdead, spectvfollow))
            {
                v->flagged = true;
                return false;
            }

            float dist = from.dist(v->o);
            if(dist < mindist || dist > maxdist)
            {
                v->flagged = true;
                return false;
            }

            v->checked = true;
        }

        if(v->visible) return true; // an earlier round already passed

        vec trg;
        switch(v->type != cament::ENTITY ? iter : 1)
        {
            case 0:
                if(!getsight(from, yaw, pitch, v->o, trg, maxdist, curfov, fovy)) return false; // check if the current view can see this
               break;
            case 1:
                if(!getsight(from, yaw, pitch, v->o, trg, maxdist, curfov + 45.f, fovy + 45.f)) return false; // gives a bit more fov wiggle room in case someone went out of shot
                break;
            case 2:
                if(!camhalo(c, v) || !getvisible(from, yaw, pitch, v->o, curfov, fovy)) return false;
                break;
            case 3:
                if(!camhalo(c, v) || !getvisible(from, yaw, pitch, v->o, curfov + 45.f, fovy + 45.f)) return false;
                break;
            case 4:
                if(!camhalo(c, v)) return false;
                break;
            case 5:
                if(v->type == cament::ENTITY)
                {
                    if(!getvisible(from, yaw, pitch, v->o, curfov, fovy) || !camhalo(c, v)) return false;
                    return true;
                }
            default:
                return false;
                break;
        }

        v->visible = true;

        return true;
    }

    bool camsetup(cament *c, bool full = false)
    {
        if(!c) return false;

        c->reset();

        loopv(cameras)
        {
            cament *v = cameras[i];
            if(!v) continue;

            if(full) v->checked = v->flagged = (c == v);
            v->visible = false;
        }

        return true;
    }

    bool cameraaim(cament *c)
    {
        return spectvcameraaim && c->chase && (!c->player || thirdpersonview(true, c->player));
    }

    bool camupdate(cament *c, int iters, bool renew = false)
    {
        if(!c || (c->player && !allowspec(c->player, spectvdead, spectvfollow))) return false;

        float yaw = 0, pitch = 0, mindist = 0, maxdist = 0;
        getcamyawpitch(c, yaw, pitch, renew);
        getcamdist(c, maxdist, mindist);
        vec from = camvec(c, yaw, pitch);

        loopj(iters)
        {
            if(!cameraaim(c) && j%4 != 0) continue;

            int count = 0;
            vec dir(0, 0, 0);

            if(!camsetup(c, j == 0)) break;

            loopv(cameras)
            {
                cament *v = cameras[i];
                if(!camcansee(c, v, from, yaw, pitch, maxdist, mindist, j)) continue;
                c->inview[v->type]++;
                dir.add(v->o);
                count++;
            }

            if(!count) continue; // can't fix it without a free camera or something visible

            dir.div(count);
            c->dist = from.dist(dir);

            if(!cameraaim(c))
            {
                getcamyawpitch(c, yaw, pitch, true);
                c->dir = vec(yaw*RAD, pitch*RAD);
            }
            else if(!dir.iszero()) c->dir = vec(dir).sub(from).safenormalize();
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
                height = zradius = radius = xradius = yradius = actors[A_PLAYER].height;
            }
        } c;

        c.o = pos;

        static const float sphereyawchecks[8] = { 0, 45, 90, 135, 180, 225, 270, 315 },
                           spherepitchchecks[9] = { 0.0f, 22.5f, -22.5f, 45.0f, -45.0f, 67.5f, -67.5f, 89.9f, -89.9f };
        loopk(8) loopi(csize) loopj(9)
        {
            c.o = vec(pos).add(vec(sphereyawchecks[k] * RAD, spherepitchchecks[j] * RAD).mul(i + 1));
            if(!collide(&c, vec(0, 0, 0), 0, false) && !collideinside)
            {
                pos = c.o;
                return true;
            }
        }
        return false;
    }

    bool buildcams()
    {
        loopk(2)
        {
            int found = 0;
            loopv(entities::ents)
            {
                gameentity &e = *(gameentity *)entities::ents[i];
                if(k ? (e.type != LIGHT && e.type != ENVMAP && e.type != PLAYERSTART && e.type != WEAPON && e.type != CAMERA) : (e.type != CAMERA || e.attrs[0] != CAMERA_NORMAL || !entities::isallowed(e))) continue;
                vec pos = e.pos();
                float radius = e.type == PLAYERSTART || !enttype[e.type].radius ? actors[A_PLAYER].height : enttype[e.type].radius;
                loopj(16) if(camcheck(pos, radius + j)) break;
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
            if(d->team >= T_ENEMY) continue;
            vec pos = d->center();
            cameras.add(new cament(cameras.length(), cament::PLAYER, d->clientnum, pos, d));
        }

        if(m_capture(gamemode)) capture::checkcams(cameras);
        else if(m_defend(gamemode)) defend::checkcams(cameras);
        else if(m_bomber(gamemode)) bomber::checkcams(cameras);

        return !cameras.empty();
    }

    int findcams(cament *cam)
    {
        int count = 0;
        bool found = false;
        loopv(cameras)
        {
            cament *c = cameras[i];

            c->dist = 0.0f;

            if(c->type == cament::ENTITY && entities::ents.inrange(c->id))
            {
                gameentity &e = *(gameentity *)entities::ents[c->id];
                c->o = e.pos();
            }

            if(c->type == cament::PLAYER && (c->player || ((c->player = getclient(c->id)) != NULL)))
            {
                if(!c->player->isspectator())
                {
                    if(!found && c->id == spectvfollow) found = true;
                    if(allowspec(c->player, spectvdead, spectvfollow)) count++;
                }
                c->o = c->player->o;
            }
            else if(c->type != cament::PLAYER && c->player) c->player = NULL;

            if(m_capture(gamemode)) capture::updatecam(c);
            else if(m_defend(gamemode)) defend::updatecam(c);
            else if(m_bomber(gamemode)) bomber::updatecam(c);
        }
        if(!found) spectvfollow = -1;
        return count;
    }

    int getcurrentcam()
    {
        if(tvmode() && cameras.inrange(lastcamcn)) return lastcamcn;
        return -1;
    }
    ICOMMAND(0, currentcam, "iN$", (int *n, int *numargs, ident *id),
    {
        if(*numargs > 0)
        {
            spectvcamera = *n;
            spectvcamupdate();
        }
        else if(*numargs < 0) intret(getcurrentcam());
        else printvar(id, getcurrentcam());
    });

    float cameradist()
    {
        if(tvmode() && cameras.inrange(lastcamcn))
            return cameras[lastcamcn]->dist;
        return 0.0f;
    }

    bool cameratv()
    {
        if(gs_waiting(gamestate)) return true; // override
        if(!tvmode(false) || (cameras.empty() && !buildcams())) return false;
        if(!gs_playing(gamestate)) spectvfollow = -1;
        else if(!player1->isspectator() && spectvfollowself >= (m_duke(gamemode, mutators) ? 2 : 1)) spectvfollow = player1->clientnum;

        bool restart = !lastcamera;
        if(!cameras.inrange(lastcamcn))
        {
            lastcamcn = rnd(cameras.length());
            restart = true;
        }
        cament *cam = cameras[lastcamcn];
        bool reset = false;
        int count = findcams(cam), lastcn = cam->cn;

        #define stvf(z) (intermthresh() ? spectvinterm##z : (spectvfollow >= 0 ? spectvfollow##z : spectv##z))

        if(spectvcamera >= 0)
        {
            while(spectvcamera >= cameras.length()) spectvcamera--;
            if(spectvcamera != lastcamcn) reset = true;
            cam = cameras[spectvcamera];
            lastcamcn = cam->cn;
            camupdate(cam, stvf(iters), restart || !count || !cameraaim(cam));
        }
        else
        {
            camrefresh(cam);

            int millis = lasttvchg ? totalmillis - lasttvchg : 0;
            bool updated = camupdate(cam, stvf(iters), restart || !count || !cameraaim(cam)), override = !lasttvchg || millis >= stvf(mintime),
                timeup = !lasttvcam || totalmillis-lasttvcam >= stvf(time), overtime = stvf(maxtime) && millis >= stvf(maxtime);

            if(restart || overtime || timeup || (!updated && override))
            {
                loopv(cameras) if(cameras[i]->ignore) cameras[i]->ignore = false;
                if(overtime) cam->ignore = true; // so we don't use the last one we just used
                int goodcams = 0;
                loopv(cameras)
                {
                    cament *c = cameras[i];
                    if(!camupdate(c, 1, true)) continue;
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
            if(!reset && cameraaim(cam))
            {
                float speed = curtime / float(cam->player ? followtvspeed : stvf(speed));
                #define SCALEAXIS(x) \
                    float x##scale = 1, adj##x = camera1->x, off##x = x; \
                    if(adj##x < x - 180.0f) adj##x += 360.0f; \
                    if(adj##x > x + 180.0f) adj##x -= 360.0f; \
                    off##x -= adj##x; \
                    if(cam->last##x == 0 || (off##x > 0 && cam->last##x < 0) || (off##x < 0 && cam->last##x > 0)) \
                    { \
                        cam->last##x##time = totalmillis; \
                        x##scale = 0; \
                    } \
                    else if(cam->last##x##time) \
                    { \
                        int offtime = totalmillis - cam->last##x##time, x##speed = cam->player ? followtv##x##speed : stvf(x##speed); \
                        if(offtime <= x##speed) x##scale = smoothinterp(offtime / float(x##speed)); \
                    } \
                    cam->last##x = off##x; \
                    float x##speed = speed * (cam->player ? followtv##x##scale : stvf(x##scale)) * x##scale;
                SCALEAXIS(yaw);
                SCALEAXIS(pitch);
                scaleyawpitch(camera1->yaw, camera1->pitch, yaw, pitch, yawspeed, pitchspeed, cam->player ? followtvrotate : stvf(rotate));
            }
            else
            {
                camera1->yaw = yaw;
                camera1->pitch = pitch;
            }
        }
        else
        {
            if(focus->isnotalive() && focus->lastdeath)
                deathcamyawpitch(focus, camera1->yaw, camera1->pitch);
            else
            {
                camera1->yaw = focus->yaw;
                camera1->pitch = focus->pitch;
                camera1->roll = focus->roll;
            }
        }
        fixrange(camera1->yaw, camera1->pitch);
        if(reset || !cam->player || (cam->player->isalive() || cam->player->isediting()))
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
        if(firstpersonbob && gs_playing(gamestate) && d->isalive() && !thirdpersonview(true))
        {
            float scale = 1;
            if(d == focus && inzoom()) scale *= 1-zoomscale();
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
        if(input && !hud::hasinput(true)) resetcursor();
        checkcamera();

        if(gs_waiting(gamestate) && cameratv() && entities::getcamera(camera1->o, camera1->yaw, camera1->pitch, curfov))
            return;

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
        UI::hideall();
    }

    void resetstate()
    {
        resetworld();
        resetcamera();
        resetsway();
    }

    void updatemusic(int type, bool force)
    {
        if(!engineready || !canplaymusic()) return;

        static int lasttype = 9, nexttype = -1;
        bool playing = playingmusic();

        // type:
        //  0 = no in-game music
        //  1 = map music (or random if none)
        //  2 = always random
        //  3 = map music (silence if none)
        //  4-5 = same as 1-2 but pick new tracks when done
        //  6/9 = theme (9 = game override)
        //  7/10 = progress (10 = game override)
        //  8/11 = intermission (11 = game override)
        if(type < 0)
        {
            if(nexttype >= 0) type = nexttype;
            else if(connected())
            {
                if(maptime <= 0 || client::needsmap) type = 10; // force progress
                else if(gs_intermission(gamestate)) type = 11; // force intermission
                else type = m_edit(gamemode) && musicedit >= 0 ? musicedit : musictype;
            }
            else type = 9; // force theme
        }
        else nexttype = -1;

        if(type ? (playing && type == lasttype) : !playing) return;

        if(playing && !force)
        {
            nexttype = type;
            if(fademusic(-1, type >= 9)) return;
        }
        if(!type) return;

        fademusic(force ? 0 : 1, type >= 9);
        nexttype = -1;

        if(type >= 6) smartmusic((type - 6) % 3);
        else if((type == 2 || type == 5 || (!playmusic(mapmusic, type < 4) && (type == 1 || type == 4))) && *musicdir)
        {
            vector<char *> files;
            listfiles(musicdir, NULL, files);
            while(!files.empty())
            {
                int r = rnd(files.length());
                if(files[r] && files[r][0])
                {
                    bool goodext = false;
                    int flen = strlen(files[r]);
                    loopk(SOUND_EXTS)
                    {
                        if(!soundexts[k] || !*soundexts[k]) continue;
                        int slen = strlen(soundexts[k]), offset = flen - slen;
                        if(offset > 0 && !strcasecmp(&files[r][offset], soundexts[k]))
                        {
                            goodext = true;
                            break;
                        }
                    }
                    if(!goodext) continue;

                    defformatstring(musicfile, "%s/%s", musicdir, files[r]);
                    if(playmusic(musicfile, type < 4)) break;
                }
                files.remove(r);
            }
            files.deletearrays();
        }

        string title, artist, album;
        if(musicinfo(title, artist, album, sizeof(string)) && *title)
        {
            gamelog *log = new gamelog(GAMELOG_EVENT);
            log->addlist("args", "type", "game");
            log->addlist("args", "action", "music");
            log->addlist("args", "title", title);
            defformatstring(constr, "Now playing: \fs\fM%s\fS", title);
            if(*artist)
            {
                log->addlist("args", "artist", artist);
                concformatstring(constr, " by \fs\fM%s\fS", artist);
            }
            if(*album) log->addlist("args", "album", album);
            log->addlist("args", "colour", colourwhite);
            log->addlist("args", "console", constr);
            if(!log->push()) DELETEP(log);
        }

        lasttype = type;
        nexttype = -1;
    }

    void updateworld() // main game update loop
    {
        if(connected())
        {
            if(!curtime || !client::isready)
            {
                gets2c();
                if(player1->clientnum >= 0) client::c2sinfo();
                return;
            }

            if(!maptime)
            {
                maptime = -1;
                timesync = timelast = timewait = 0;
                return; // skip the first loop
            }
            else if(maptime < 0)
            {
                maptime = lastmillis ? lastmillis : 1;
                mapstart = totalmillis ? totalmillis : 1;
                RUNMAP("on_start");
                resetcamera();
                resetsway();
                return;
            }

            checklights();
            player1->conopen = hud::hasinput(true);
            checkoften(player1, true);
            loopv(players) if(players[i]) checkoften(players[i], players[i]->ai != NULL);
            if(!allowmove(player1)) player1->stopmoving(player1->state < CS_SPECTATOR);

            if(focus->isalive() && gs_playing(gamestate) && isweap(focus->weapselect))
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
                if(player1->isalive()) weapons::shoot(player1, worldpos);
            }

            checkplayers();
            flushdamagemerges();
        }

        gets2c();

        if(connected())
        {
            checkcamera();
            if(player1->isnotalive())
            {
                if(player1->ragdoll) moveragdoll(player1);
                else if(lastmillis-player1->lastpain < 5000)
                    physics::move(player1, 10, true);
            }
            else
            {
                if(player1->ragdoll) cleanragdoll(player1);
                if(player1->isediting()) physics::move(player1, 10, true);
                else if(player1->isalive() && gs_playing(gamestate) && !tvmode())
                {
                    physics::move(player1, 10, true);
                    weapons::checkweapons(player1);
                }
            }

            if(gs_playing(gamestate))
            {
                addsway(focus);
                entities::checkitems(player1);

                if(!tvmode() && player1->state >= CS_SPECTATOR)
                {
                    camera1->move = player1->move;
                    camera1->strafe = player1->strafe;
                    physics::move(camera1, 10, true);
                }
            }

            entities::runrails();
            if(hud::canshowscores()) hud::showscores(true);
        }

        if(player1->clientnum >= 0) client::c2sinfo();
        entities::updatepassengers();
    }

    void adjustorientation(vec &pos)
    {
        gameent *best = NULL, *o = NULL;
        float bestdist = 1e16f;
        int numdyns = numdynents();
        loopi(numdyns) if((o = (gameent *)iterdynents(i)))
        {
            if(!o || o == focus || !o->isalive() || !physics::issolid(o, focus)) continue;
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

        bool input = hud::hasinput(true) != 0, view = thirdpersonview(true, focus), mode = tvmode();

        if(input != inputmouse || view != inputview || mode != inputmode || focus != lastfocus)
        {
            if(input != inputmouse) resetcursor();
            else resetcamera();

            inputmouse = input;
            inputview = view;
            inputmode = mode;
            lastfocus = focus;
        }

        bool hasorient = false;
        switch(view && !input ? (focus != player1 ? 1 : thirdpersoncursor) : 0)
        {
            case 1:
            {
                cursorx = thirdpersoncursorx;
                cursory = thirdpersoncursory;

                break;
            }
            case 2:
            {
                findorientation(focus->o, focus->yaw, focus->pitch, worldpos);
                adjustorientation(worldpos);

                vec pos = worldpos, dir = vec(pos).sub(focus->o);
                if(thirdpersoncursordist > 0 && dir.magnitude() > thirdpersoncursordist)
                    pos = dir.safenormalize().mul(thirdpersoncursordist).add(focus->o);

                vec loc(0, 0, 0);
                if(vectocursor(pos, loc.x, loc.y, loc.z))
                {
                    float amt = curtime / float(thirdpersoninterp);
                    cursorx = clamp(cursorx + ((loc.x - cursorx) * amt), 0.0f, 1.0f);
                    cursory = clamp(cursory + ((loc.y - cursory) * amt), 0.0f, 1.0f);
                }

                hasorient = true;

                break;
            }
            default: break;
        }

        vecfromcursor(cursorx, cursory, 1.0f, cursordir);
        vectoyawpitch(cursordir, cursoryaw, cursorpitch);
        if(!hasorient)
        {
            findorientation(camera1->o, cursoryaw, cursorpitch, worldpos);
            adjustorientation(worldpos);
        }

        camera1->inmaterial = lookupmaterial(camera1->o);
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

    void getplayermaterials(gameent *d, modelstate &mdl)
    {
        #define TONEINTERP(name, var) \
            mdl.material[var] = bvec::fromcolor(getcolour(d, player##name##tone, player##name##tonelevel, player##name##tonemix)); \
            mdl.matbright[var] = player##name##tonebright; \
            if(player##name##toneinterp > 0) \
            { \
                float intensity = 1.f-((uint(mdl.material[var].r)+uint(mdl.material[var].g)+uint(mdl.material[var].b))/765.f); \
                mdl.matbright[var] += intensity*player##name##toneinterp; \
            }
        TONEINTERP(over, 0);
        TONEINTERP(under, 1);
        TONEINTERP(fx, 2);
        #undef TONEINTERP

        if(isweap(d->weapselect))
        {
            vec color = vec::fromcolor(W(d->weapselect, colour));
            if(d != previewent)
            {
                bool secondary = physics::secondaryweap(d);
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
            }
            mdl.material[3] = bvec::fromcolor(color);
        }
        else mdl.material[3] = bvec::fromcolor(colourwhite);

        game::haloadjust(d->center(), mdl);
    }

    static void calchwepsway(gameent *d, modelstate &mdl)
    {
        static float speed = 0.0f;
        static int lasthwep = -1;

        if(totalmillis != lasthwep)
        {
            float curspeed = d->vel.magnitude();

            speed += (d->vel.magnitude() - speed) * curtime *
                (curspeed < speed ? 0.01f : 0.001f);

            lasthwep = totalmillis;
        }

        float steplen = firstpersonbob ? firstpersonbobstep : firstpersonswaystep;
        float steps = swaydist/steplen*M_PI;
        float speedfactor = clamp(speed/150.0f, 0.0f, 1.0f);

        // Magic floats to generate the animation cycle
        float f1 = cosf(steps) + 1,
              f2 = sinf(steps*2.0f) + 1,
              f3 = (f1*f1*0.25f)-0.5f,
              f4 = (f2*f2*0.25f)-0.5f,
              f5 = sinf(lastmillis*0.001f); // Low frequency detail

        vec dirforward = vec(mdl.yaw*RAD, 0.0f),
            dirside = vec((mdl.yaw+90)*RAD, 0.0f);

        vec trans = vec(0, 0, 0);
        float rotyaw = 0, rotpitch = 0;

        float up = firstpersonswayup*lerp(1.25f, 0.9f, speedfactor*speedfactor);
        float side = firstpersonswayside*lerp(1.4f, 0.9f, speedfactor*speedfactor);

        // Walk cycle animation
        trans.add(vec(dirforward).mul(side*f4 * 2.0f));
        trans.add(vec(dirside).mul(side*f5 *lerp(2.0f, 4.0f, speedfactor*speedfactor)));
        trans.add(vec(swaydir).mul(-4.0f));
        trans.add(swaypush);
        trans.z += up*f2 * 1.5f;
        trans.z -= speedfactor*0.2f;

        rotyaw += side*f3 * 24.0f;
        rotpitch += up*f2 * -10.0f;

        trans.add(dirside.mul(d->rotvel.x * 0.06f));
        trans.z += d->rotvel.y * 0.045f;
        rotyaw += d->rotvel.x * -0.3f;
        rotpitch += d->rotvel.y * -0.3f;

        mdl.o.add(trans);
        mdl.yaw += rotyaw;
        mdl.pitch += rotpitch;
    }

    VAR(IDF_PERSIST, weapswitchanimtime, 1, 300, INT_MAX);

    const char *getplayerstate(gameent *d, modelstate &mdl, int third, float size, int flags, modelattach *mdlattach, bool vanitypoints)
    {
        int atype = clamp(d->actortype, 0, A_MAX - 1);
        if(!actors[atype].isplayer && third != 1) return NULL; // only the player model supports first person views

        const char *mdlname = actors[atype].isplayer ? playertypes[d->model%PLAYERTYPES][third] : actors[atype].mdl;

        if(!mdlname || !*mdlname) return NULL; // null model, bail out

        int weap = d->weapselect, ai = 0;

        mdl.anim = ANIM_IDLE|ANIM_LOOP;
        mdl.flags = flags;
        mdl.basetime = mdl.basetime2 = 0;
        mdl.size = size;
        mdl.o = third ? d->feetpos() : camerapos(d);

        if(actors[atype].mdlyaw) mdl.yaw = d->yaw;

        if(actors[atype].mdlpitch)
        {
            mdl.pitch = d->pitch;
            mdl.roll = calcroll(d);
        }
        else if(physics::movepitch(d, true))
        {
            mdl.pitch = clamp(-d->vel.y * 22.5f / max(d->speed, 1.0f), -22.5f, 22.5f);
            mdl.roll = clamp(-d->vel.x * 22.5f / max(d->speed, 1.0f), -22.5f, 22.5f);
        }


        if(d->isnotalive())
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
        else if(d->isediting()) mdl.anim = ANIM_EDIT|ANIM_LOOP;
        else
        {
            float weapscale = 1.f;
            bool showweap = actors[d->actortype].weapmdl && (third != 2 || firstpersoncamera) && isweap(weap) && weap < W_ALL;
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

                        // Switch to idle animation after switch/use animation is done
                        if(millis <= weapswitchanimtime)
                            mdl.anim = d->weapstate[weap] == W_S_SWITCH ? ANIM_SWITCH : ANIM_USE;
                        else mdl.anim = weaptype[weap].anim|ANIM_LOOP;
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
                        int vaulttime = d->impulsetimer(IM_T_VAULT);
                        if(vaulttime)
                        {
                            mdl.basetime = vaulttime;
                            mdl.anim = ANIM_VAULT;
                        }
                        else mdl.anim = weaptype[weap].anim|ANIM_LOOP;
                        break;
                    }
                }
            }
            if(third && (mdl.anim&ANIM_IDLE) && lastmillis-d->lastpain <= 300)
            {
                mdl.basetime = d->lastpain;
                mdl.anim = ANIM_PAIN;
            }
            if(mdlattach && showweap)
            {
                const char *weapmdl = third ? weaptype[weap].vwep : weaptype[weap].hwep;
                if(weapmdl && *weapmdl)
                    mdlattach[ai++] = modelattach("tag_weapon", weapmdl, mdl.anim, mdl.basetime, weapscale); // 0
            }
        }
        if(mdlattach)
        {
            // Used for showing vanity slots in the UI
            if(vanitypoints)
            {
                loopvk(vanitytypetags)
                    mdlattach[ai++] = modelattach(vanitytypetags[k], &d->tag[k]);
            }
            else
            {
                if(!(mdl.flags&MDL_ONLYSHADOW) && actors[d->actortype].hastags)
                {
                    if(third != 2 || firstpersoncamera)
                    {
                        mdlattach[ai++] = modelattach("tag_weapon", &d->tag[TAG_ORIGIN]); // 1
                        mdlattach[ai++] = modelattach("tag_muzzle", &d->tag[TAG_MUZZLE1]); // 2
                        mdlattach[ai++] = modelattach("tag_muzzle2", &d->tag[TAG_MUZZLE2]); // 3
                        if(weaptype[weap].eject || weaptype[weap].tape)
                        {
                            mdlattach[ai++] = modelattach("tag_eject", &d->tag[TAG_EJECT1]); // 4
                            mdlattach[ai++] = modelattach("tag_eject2", &d->tag[TAG_EJECT2]); // 5
                        }
                    }
                    if(third && actors[d->actortype].hastags > 1)
                    {
                        mdlattach[ai++] = modelattach("tag_camera", &d->tag[TAG_CAMERA]); // 6
                        mdlattach[ai++] = modelattach("tag_crown", &d->tag[TAG_CROWN]); // 7
                        mdlattach[ai++] = modelattach("tag_torso", &d->tag[TAG_TORSO]); // 8
                        mdlattach[ai++] = modelattach("tag_waist", &d->tag[TAG_WAIST]); // 9
                        mdlattach[ai++] = modelattach("tag_ljet", &d->tag[TAG_JET_LEFT]); // 10
                        mdlattach[ai++] = modelattach("tag_rjet", &d->tag[TAG_JET_RIGHT]); // 11
                        mdlattach[ai++] = modelattach("tag_bjet", &d->tag[TAG_JET_BACK]); // 12
                        mdlattach[ai++] = modelattach("tag_ltoe", &d->tag[TAG_TOE_LEFT]); // 13
                        mdlattach[ai++] = modelattach("tag_rtoe", &d->tag[TAG_TOE_RIGHT]); // 14
                    }
                }
            }

            if(third)
            {
                int count = 0, head = vanitybuild(d), found[VANITYMAX] = {0};
                bool check = vanitycheck(d);
                loopvk(d->vitems) if(vanities.inrange(d->vitems[k]))
                {
                    if(vanities[d->vitems[k]].type ? !check : !actors[d->actortype].hashead) continue;
                    if(found[vanities[d->vitems[k]].type]) continue;
                    if(vanities[d->vitems[k]].cond&1 && third == 2) continue;
                    if(vanities[d->vitems[k]].cond&2 && d->headless) continue;
                    const char *file = vanityfname(d, d->vitems[k], head);
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
                if(!firstpersoncamera && gs_playing(gamestate) && firstpersonsway)
                    calchwepsway(d, mdl);

                int slidetime = d->impulsetimer(IM_T_SLIDE);
                if(slidetime != 0 && firstpersonslidetime && firstpersonslideroll != 0)
                {
                    int dur = min(impulseslidelen/2, firstpersonslidetime), millis = lastmillis - slidetime;
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
                if(firstpersoncamera) break;
                if(firstpersonbodydist) mdl.o.sub(vec(mdl.yaw*RAD, 0.f).mul(firstpersonbodydist));
                if(firstpersonbodyoffset) mdl.o.sub(vec(mdl.yaw*RAD, 0.f).mul(firstpersonspineoffset*firstpersonbodyoffset));
                if(firstpersonbodyside) mdl.o.sub(vec(mdl.yaw*RAD, 0.f).rotate_around_z(90*RAD).mul(firstpersonbodyside));
                if(firstpersonbodypitchadjust && mdl.pitch < 0) mdl.o.sub(vec(mdl.yaw*RAD, 0.f).mul(d->radius*(0-mdl.pitch)/90.f*firstpersonbodypitchadjust));
                if(firstpersonbodypitch) mdl.pitch = mdl.pitch*firstpersonbodypitch;
                if(d->zradius != d->height && d->physstate < PHYS_SLOPE && !physics::laddercheck(d) && !physics::liquidcheck(d) && !physics::sticktospecial(d))
                    mdl.o.z -= d->zradius - d->height;
                break;
            }
        }
        if(animoverride && (m_edit(gamemode) ? (animtargets >= (d == focus ? 0 : 1)) : d == focus))
        {
            mdl.anim = (animoverride < 0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
            mdl.basetime = 0;
        }
        else if(third || firstpersoncamera)
        {
            if(allowmove(d))
            {
                // Test if the player is actually moving at a meaningful speed. This may not be the case if the player is running against a wall or another obstacle.
                bool moving = fabsf(d->vel.x) > 5.0f || fabsf(d->vel.y) > 5.0f, turning = fabsf(d->rotvel.x) >= playerrotthresh * (d->crouching() ? 0.5f : 1.f);
                int vaulttime = d->impulsetimer(IM_T_VAULT), dashtime = d->impulsetimer(IM_T_DASH), slidetime = d->impulsetimer(IM_T_SLIDE), parkourtime = d->impulsetimer(IM_T_WALLRUN),
                    impulsetime = d->impulse[IM_TYPE] >= 0 && d->impulse[IM_TYPE] < IM_MAX ? d->impulsetime[d->impulse[IM_TYPE]] : 0;

                if(physics::liquidcheck(d, 0.1f) && d->physstate <= PHYS_FALL)
                {
                    if(d->crouching())
                    {
                        if((moving && d->strafe) || turning)
                            mdl.anim |= ((d->strafe > 0 || (turning && d->rotvel.x > 0) ? ANIM_CRAWL_LEFT : ANIM_CRAWL_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                        else if(moving && d->move > 0) mdl.anim |= (ANIM_CRAWL_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                        else if(moving && d->move < 0) mdl.anim |= (ANIM_CRAWL_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                        else mdl.anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY;
                    }
                    else mdl.anim |= ((d->move || d->strafe ? int(ANIM_SWIM) : int(ANIM_SINK))|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(vaulttime)
                {
                    mdl.basetime2 = vaulttime;
                    mdl.anim |= ANIM_VAULT<<ANIM_SECONDARY;
                }
                else if(parkourtime)
                {
                    mdl.basetime2 = parkourtime;
                    mdl.anim |= ((d->turnside > 0 ? ANIM_PARKOUR_LEFT : (d->turnside < 0 ? ANIM_PARKOUR_RIGHT : ANIM_PARKOUR_UP))|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->physstate == PHYS_FALL && !physics::laddercheck(d) && impulsetime && lastmillis - impulsetime <= 1000)
                {
                    mdl.basetime2 = impulsetime;
                    if(d->impulse[IM_TYPE] == IM_T_KICK || d->impulse[IM_TYPE] == IM_T_GRAB) mdl.anim |= ANIM_PARKOUR_JUMP<<ANIM_SECONDARY;
                    else if(d->action[AC_SPECIAL] || d->impulse[IM_TYPE] == IM_T_POUND)
                    {
                        mdl.basetime2 = d->weaptime[W_MELEE];
                        mdl.anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                    }
                    else if(moving && d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_BOOST_LEFT : ANIM_BOOST_RIGHT)<<ANIM_SECONDARY;
                    else if(moving && d->move > 0) mdl.anim |= ANIM_BOOST_FORWARD<<ANIM_SECONDARY;
                    else if(moving && d->move < 0) mdl.anim |= ANIM_BOOST_BACKWARD<<ANIM_SECONDARY;
                    else mdl.anim |= ANIM_BOOST_UP<<ANIM_SECONDARY;
                }
                else if(d->physstate == PHYS_FALL && !physics::laddercheck(d) && d->airtime(lastmillis) >= 50)
                {
                    mdl.basetime2 = max(d->airmillis, d->impulsetime[IM_T_JUMP]);
                    if(d->action[AC_SPECIAL] || d->impulse[IM_TYPE] == IM_T_POUND)
                    {
                        mdl.anim |= ANIM_FLYKICK<<ANIM_SECONDARY;
                        mdl.basetime2 = d->weaptime[W_MELEE];
                    }
                    else if(d->crouching())
                    {
                        if((moving && d->strafe) || turning)
                            mdl.anim |= (d->strafe > 0 || (turning && d->rotvel.x > 0) ? ANIM_CROUCH_JUMP_LEFT : ANIM_CROUCH_JUMP_RIGHT)<<ANIM_SECONDARY;
                        else if(moving && d->move > 0) mdl.anim |= ANIM_CROUCH_JUMP_FORWARD<<ANIM_SECONDARY;
                        else if(moving && d->move < 0) mdl.anim |= ANIM_CROUCH_JUMP_BACKWARD<<ANIM_SECONDARY;
                        else mdl.anim |= ANIM_CROUCH_JUMP<<ANIM_SECONDARY;
                    }
                    else if(moving && d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_JUMP_LEFT : ANIM_JUMP_RIGHT)<<ANIM_SECONDARY;
                    else if(moving && d->move > 0) mdl.anim |= ANIM_JUMP_FORWARD<<ANIM_SECONDARY;
                    else if(moving && d->move < 0) mdl.anim |= ANIM_JUMP_BACKWARD<<ANIM_SECONDARY;
                    else mdl.anim |= ANIM_JUMP<<ANIM_SECONDARY;
                    if(!mdl.basetime2) mdl.anim |= ANIM_END<<ANIM_SECONDARY;
                }
                else if(dashtime)
                {
                    mdl.basetime2 = dashtime;
                    if(d->strafe) mdl.anim |= (d->strafe > 0 ? ANIM_BOOST_LEFT : ANIM_BOOST_RIGHT)<<ANIM_SECONDARY;
                    else if(d->move > 0) mdl.anim |= ANIM_BOOST_FORWARD<<ANIM_SECONDARY;
                    else if(d->move < 0) mdl.anim |= ANIM_BOOST_BACKWARD<<ANIM_SECONDARY;
                    else mdl.anim |= ANIM_BOOST_UP<<ANIM_SECONDARY;
                }
                else if(slidetime)
                {
                    mdl.basetime2 = slidetime;
                    mdl.anim |= (ANIM_POWERSLIDE|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->crouching())
                {
                    if((moving && d->strafe) || turning)
                        mdl.anim |= ((d->strafe > 0 || (turning && d->rotvel.x > 0) ? ANIM_CRAWL_LEFT : ANIM_CRAWL_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(moving && d->move > 0) mdl.anim |= (ANIM_CRAWL_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(moving && d->move < 0) mdl.anim |= (ANIM_CRAWL_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else mdl.anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->running())
                {
                    if(moving && d->sprinting(false)) mdl.anim |= (ANIM_SPRINT|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if((moving && d->strafe) || turning)
                        mdl.anim |= ((d->strafe > 0 || (turning && d->rotvel.x > 0) ? ANIM_RUN_LEFT : ANIM_RUN_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(moving && d->move > 0) mdl.anim |= (ANIM_RUN_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                    else if(moving && d->move < 0) mdl.anim |= (ANIM_RUN_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if((moving && d->strafe) || turning)
                    mdl.anim |= ((d->strafe > 0 || (turning && d->rotvel.x > 0) ? ANIM_WALK_LEFT : ANIM_WALK_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(moving && d->move > 0) mdl.anim |= (ANIM_WALK_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(moving && d->move < 0) mdl.anim |= (ANIM_WALK_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;
            }

            if((mdl.anim>>ANIM_SECONDARY)&ANIM_INDEX) switch(mdl.anim&ANIM_INDEX)
            {
                case ANIM_IDLE: case ANIM_CLAW: case ANIM_PISTOL: case ANIM_SWORD:
                case ANIM_SHOTGUN: case ANIM_SMG: case ANIM_FLAMER: case ANIM_PLASMA: case ANIM_ZAPPER:
                case ANIM_RIFLE: case ANIM_CORRODER: case ANIM_GRENADE: case ANIM_MINE: case ANIM_ROCKET:
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

    void mixerreset()
    {
        loopvrev(mixers) mixers.remove(i);
    }
    ICOMMAND(0, resetmixer, "", (), mixerreset());

    int mixerfind(const char *id)
    {
        loopv(mixers) if(!strcmp(mixers[i].id, id)) return i;
        return -1;
    }
    ICOMMAND(0, findmixer, "s", (char *s), intret(mixerfind(s)));

    int mixeritem(const char *id, const char *name, const char *tpname, int tclamp, float tpscale, float fpscale, float split, float blur, bool anytype, const char *fpname)
    {
        if(!id || !*id || !name || !*name || !tpname || !*tpname || mixerfind(id) >= 0) return -1;

        mixer &m = mixers.add();
        m.tclamp = tclamp;
        m.tpscale = tpscale;
        m.fpscale = fpscale;
        m.split = split;
        m.blur = blur;
        m.anytype = anytype;
        m.setnames(id, name, tpname, fpname);

        return mixers.length() - 1;
    }
    
    ICOMMAND(0, addmixer, "sssiggffis", (char *d, char *n, char *tp, int *t, float *ts, float *fs, float *m, float *b, int *a, char *fp),
        intret(mixeritem(d, n, tp, *t, *ts >= 0.0f ? *ts : 1.0f, *fs >= 0.0f ? *fs : 1.0f, *m, *b, *a != 0, fp))
    );

    void mixerinfo(int id, int value)
    {
        if(id < 0) intret(mixers.length());
        else if(value < 0) intret(10);
        else if(mixers.inrange(id)) switch(value)
        {
            case 0: result(mixers[id].id); break;
            case 1: result(mixers[id].name); break;
            case 2: result(mixers[id].loadtex(false) ? mixers[id].tptex->name : ""); break;
            case 3: intret(mixers[id].tclamp); break;
            case 4: floatret(mixers[id].tpscale); break;
            case 5: floatret(mixers[id].split); break;
            case 6: floatret(mixers[id].blur); break;
            case 7: intret(mixers[id].anytype ? 1 : 0); break;
            case 8: floatret(mixers[id].fpscale); break;
            case 9: result(mixers[id].loadtex(true) ? mixers[id].fptex->name : ""); break;
            default: break;
        }
    }
    ICOMMAND(0, getmixer, "bb", (int *t, int *v), mixerinfo(*t, *v));

    void getplayereffects(gameent *d, int third, modelstate &mdl)
    {
        if(regentime && d->lastregen && playerregenslice > 0)
        {
            int regenoffset = lastmillis - d->lastregen, regenscaled = int(ceilf(regentime * playerregentime));
            if(regenoffset <= regenscaled)
            {
                int regenpulse = PULSE_HEALTH;
                bool regendecay = d->lastregenamt < 0;
                float regenamt = regenoffset / float(regenscaled),
                    regenblend = playerregenblend, regenbright = playerregenbright;

                if(regendecay)
                {
                    regenpulse = PULSE_DECAY;
                    regenblend = playerregendecayblend;
                    regenbright = playerregendecaybright;
                    regenamt = 1.0f - regenamt;
                }

                mdl.effecttype = MDLFX_SHIMMER;
                mdl.effectcolor = vec4(pulsehexcol(d, regenpulse, 50), regenblend);
                mdl.effectparams = vec4(regenamt, playerregenslice, playerregenfade / playerregenslice, regenbright);
            }
        }
        else if(playereffect)
        {
            float fade = d->isalive() ? protectfade(d) : spawnfade(d);
            if(fade < 1.0f)
            {
                mdl.effecttype = MDLFX_DISSOLVE;
                mdl.effectcolor = vec4(pulsehexcol(d, d->isalive() ? PULSE_HEALTH : PULSE_DECAY, 50), playereffectblend);
                mdl.effectparams = vec4(fade, playereffectslice, playereffectfade / playereffectslice, playereffectbright);
            }
        }

        int atype = clamp(d->actortype, 0, A_MAX - 1);
        if(actors[atype].isplayer && drawtex != DRAWTEX_HALO)
        {
            int playermix = mixerfind(d->mixer);
            if(mixers.inrange(playermix))
            {
                mixer &m = mixers[playermix];

                mdl.mixer = m.loadtex(third == 0);
                mdl.mixerscale = third == 0 ? m.fpscale : m.tpscale;
                mdl.matsplit = m.split;
            }
        }

        if(drawtex == DRAWTEX_HALO && playerhalodamage && (d != focus || playerhalodamage&2))
        {
            vec accumcolor = mdl.material[2].tocolor();
            int dmgtime = min(playerhalodamagetime, damagemergetime);
            
            loopv(damagemerges)
            {
                damagemerge &m = damagemerges[i];
                if(m.to != d || m.amt <= 0) continue;
                if(m.to != focus && (m.from == focus ? !(playerhalodamage&1) : !(playerhalodamage&4))) continue;

                int offset = totalmillis - m.millis;
                if(offset >= damagemergedelay + dmgtime) continue;

                vec curcolor;
                switch(m.type)
                {
                    case damagemerge::BURN: curcolor = pulsecolour(m.to, PULSE_BURN, 50); break;
                    case damagemerge::BLEED: curcolor = pulsecolour(m.to, PULSE_BLEED, 50); break;
                    case damagemerge::SHOCK: curcolor = pulsecolour(m.to, PULSE_SHOCK, 50); break;
                    case damagemerge::CORRODE: curcolor = pulsecolour(m.to, PULSE_CORRODE, 50); break;
                    default: curcolor = pulsecolour(m.to, PULSE_WARN, 50); break;
                }

                float amt = offset > damagemergedelay ? 1.0f - ((offset - damagemergedelay) / float(dmgtime)) : offset / float(damagemergedelay);
                accumcolor.mul(1.0f - amt).add(curcolor.mul(amt));
            }
            
            mdl.material[2] = bvec::fromcolor(accumcolor);
        }
    }

    void haloadjust(const vec &o, modelstate &mdl)
    {
        if(drawtex != DRAWTEX_HALO) return;

        if(mdl.effecttype == MDLFX_DISSOLVE) mdl.color.a *= mdl.effectparams.x;
        
        loopk(MAXMDLMATERIALS) mdl.material[k].mul(mdl.color.a);
        
        mdl.color.a = hud::radardepth(o, halodist, halotolerance, haloaddz);
    }

    bool haloallow(const vec &o, gameent *d, bool justtest)
    {
        if(d == focus && inzoom()) return false;
        if(drawtex != DRAWTEX_HALO) return true;
        if(!(d == focus ? playerhalos&1 : playerhalos&2) || !halosurf.check()) return false;

        if(d != focus)
        {
            vec dir(0, 0, 0);
            float dist = -1;
            if(!client::radarallow(o, d, dir, dist, justtest)) return false;
            if(dist > halodist) return false;
        }

        return true;
    }

    void renderplayer(gameent *d, int third, float size, int flags = 0, const vec4 &color = vec4(1, 1, 1, 1), bool vanitypoints = false)
    {
        if(d->isspectator() || (!d->isalive() && color.a <= 0) || d->obliterated) return;

        modelstate mdl;
        modelattach mdlattach[VANITYMAX + ATTACHMENTMAX];
        dynent *e = third ? (third != 2 ? (dynent *)d : (dynent *)&bodymodel) : (dynent *)&avatarmodel;
        const char *mdlname = getplayerstate(d, mdl, third, size, flags, mdlattach, vanitypoints);
        if(!mdlname || !*mdlname) return;

        mdl.color = color;
        getplayermaterials(d, mdl);
        getplayereffects(d, third, mdl);

        if(actors[d->actortype].mdlflags > 0) mdl.flags |= actors[d->actortype].mdlflags;

        if(DRAWTEX_GAME&(1<<drawtex))
        {
            if(d != focus && !d->ai)
            {
                if(!(mdl.anim&ANIM_RAGDOLL)) mdl.flags |= MDL_CULL_VFC | MDL_CULL_OCCLUDED | (drawtex ? 0 : MDL_CULL_QUERY);
            }

            if(d != focus || (d != player1 ? fullbrightfocus&1 : fullbrightfocus&2)) mdl.flags |= MDL_FULLBRIGHT;

            if((d != focus && playershadow < 2) || playershadow < 1 || (d == focus && d->isediting()) || (camera1->o.squaredist(d->o) > playershadowsqdist))
                mdl.flags |= MDL_NOSHADOW;
        }
        else if(drawtex == DRAWTEX_HALO)
        {
            if(haloallow(camera1->o, d))
            {
                if(d == focus || d->isprize(focus) || focus->isobserver() || (m_team(gamemode, mutators) && focus->team == d->team))
                    mdl.flags |= MDL_HALO_TOP;
            }
            else
            {
                loopi(MAXMDLMATERIALS) mdl.material[i] = bvec(0, 0, 0);
                mdl.color.a = 0;
            }
        }

        rendermodel(mdlname, mdl, e);
    }

    void rendercheck(gameent *d, bool third)
    {
        if(d->obliterated) return;

        float blend = opacity(d, third, playereffect != 0);

        if(d->burntime && d->burnfunc(lastmillis, d->burntime))
        {
            int millis = lastmillis - d->lastres[W_R_BURN], delay = max(d->burndelay, 1);
            float pc = 1, intensity = 0.5f + (rnd(51) / 100.f), fade = (d != focus || !d->isalive() ? 0.175f : 0.f) + (rnd(26) / 100.f);

            if(d->burntime - millis < delay) pc *= (d->burntime - millis) / float(delay);
            else pc *= 0.75f + ((millis % delay)/float(delay * 4));

            loopi(2)
            {
                vec pos = vec(d->center()).add(vec(rnd(11) - 5, rnd(11) - 5, rnd(11) - 3).mul(pc));
                regular_part_create(PART_FIREBALL_SOFT, i ? 150 : 300, pos, pulsehexcol(d, i ? PULSE_BURN : PULSE_FIRE, 50), d->height * intensity * blend * pc, fade * blend * pc, 0, 0, i ? -50 : -25);
            }
        }

        if(d->shocktime && d->shockfunc(lastmillis, d->shocktime))
        {
            float radius = d->getradius() * (d->ragdoll ? 0.6f : 1.2f),
                  height = d->getheight() * (d->ragdoll ? 0.9f : 0.4f);
            vec origin = d->center(), rad = vec(radius, radius, max(radius * 0.8f, height)).mul(blend);

            loopi(6 + rnd(6))
            {
                float fade = blend * (d != focus || d->state != CS_ALIVE ? 0.35f : 0.2f) + (rnd(26) / 100.f);
                vec dir = vec(rnd(201) - 100, rnd(201) - 100, rnd(201) - 100).div(100.f).safenormalize(),
                    from = vec(dir).mul(rad).mul(rnd(51) / 100.f).add(origin),
                    col = pulsecolour(d, PULSE_SHOCK, (i + 1) * 11), to = from;
                int count = 6 + rnd(6), colour = (int(col.x * 255) << 16)|(int(col.y * 255) << 8)|(int(col.z * 255));
                loopj(count)
                {
                    float q = 1.f - (j / float(count)), qc = clamp(q + 0.01f, 0.01f, 1.0f);
                    dir.add(vec(rnd(101) - 50, rnd(101) - 50, rnd(101) - 50).div(100.f).safenormalize()).mul(0.5f).safenormalize();
                    to = vec(from).add(vec(dir).mul(rad).mul((rnd(101) + 1) / 50.f * q * 0.5f));
                    part_flare(from, to, 1, PART_LIGHTNING_FLARE, colour, q * 0.75f, fade * qc * 0.75f);
                    part_flare(from, to, 1, PART_LIGHTZAP_FLARE, colour, q * 0.25f, fade * qc);
                    from = to;

                    int r = j ? rnd(j * 4) + 1 : 0;
                    loopk(r)
                    {
                        float qr = qc * qc * (1.f - (k / float(r))), qq = clamp(qr + 0.001f, 0.001f, qc);
                        vec rdir = vec(dir).add(vec(rnd(101) - 50, rnd(101) - 50, rnd(101) - 50).div(100.f).safenormalize()).mul(0.5f).safenormalize();
                        to = vec(from).add(vec(rdir).mul(rad).mul((rnd(101) + 1) / 50.f * qr * 0.5f));
                        part_flare(from, to, 1, PART_LIGHTNING_FLARE, colour, qr * 0.6f, fade * qq * 0.75f);
                        part_flare(from, to, 1, PART_LIGHTZAP_FLARE, colour, qr * 0.2f, fade * qq);
                    }
                }
            }
        }

        if(d->corrodetime && d->corrodefunc(lastmillis, d->corrodetime))
        {
            int millis = lastmillis - d->lastres[W_R_CORRODE], delay = max(d->corrodedelay, 1);
            float pc = 1, intensity = 0.35f + (rnd(36) / 100.f), fade = (d != focus || !d->isalive() ? 0.2f : 0.1f) + (rnd(20) / 100.f);

            if(d->corrodetime - millis < delay) pc *= (d->corrodetime - millis) / float(delay);
            else pc *= 0.75f + ((millis % delay)/float(delay * 4));

            loopi(2)
            {
                vec pos = vec(d->center()).add(vec(rnd(17) - 8, rnd(17) - 8, rnd(17) - 8).mul(pc));
                regular_part_create(i ? PART_BUBBLE_SOFT : PART_SPLASH_SOFT, i ? 500 : 250, pos, pulsehexcol(d, PULSE_CORRODE, i ? -1 : 50), intensity * blend * pc, fade * blend * pc, 0, 0, i ? -25 : -50);
            }
        }
    }

    void render(int n)
    {
        if(n != 2)
        {
            gameent *d;
            int numdyns = numdynents();
            bool third = thirdpersonview();
            loopi(numdyns) if((d = (gameent *)iterdynents(i)) != NULL)
            {
                if(drawtex == DRAWTEX_HALO) d->cleartags();
                if(d->actortype == A_HAZARD) continue;
                renderplayer(d, 1, d->curscale, d == focus ? MDL_AVATAR|(third ? MDL_FORCESHADOW : MDL_ONLYSHADOW) : 0, vec4(1, 1, 1, opacity(d, true, playereffect != 0)));
            }
        }

        if(n != 1)
        {
            ai::render();
            entities::render();
            projs::render();

            if(m_capture(gamemode)) capture::render();
            else if(m_defend(gamemode)) defend::render();
            else if(m_bomber(gamemode)) bomber::render();
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

    void calcfirstpersontag(gameent *d, int tag)
    {
        if(d->tag[tag].x >= 0) d->tag[tag] = calcavatarpos(d->tag[tag], firstpersondepthfov);
    }

    void calcfirstpersontags(gameent *d)
    {
        calcfirstpersontag(d, TAG_ORIGIN);
        loopi(TAG_N_MUZZLE) calcfirstpersontag(d, TAG_MUZZLE + i);
        loopi(TAG_N_EJECT) calcfirstpersontag(d, TAG_EJECT + i);
    }

    void renderavatar()
    {
        if(thirdpersonview() || focus->obliterated) return;

        vec4 color = vec4(1, 1, 1, opacity(focus, false, false));
        if(firstpersoncamera) renderplayer(focus, 2, focus->curscale, MDL_NOBATCH, color);
        else if(firstpersonmodel)
        {
            float depthfov = firstpersondepthfov != 0 ? firstpersondepthfov : curfov;
            if(inzoom()) depthfov *= 1-zoomscale();

            setavatarscale(depthfov, firstpersondepth);

            if(focus->isalive() && firstpersonmodel&1) renderplayer(focus, 0, focus->curscale, MDL_NOBATCH, color);

            if(focus->isalive() && firstpersonmodel&2)
            {
                bool onfloor = !(A(focus->actortype, abilities)&(1<<A_A_FLOAT)) && (focus->physstate >= PHYS_SLOPE || physics::sticktospecial(focus, false) || physics::liquidcheck(focus));
                float depth = (!onfloor && focus->action[AC_SPECIAL]) || focus->impulse[IM_TYPE] == IM_T_KICK || focus->hasparkour() ? firstpersonbodydepthkick : firstpersonbodydepth;
                setavatarscale(firstpersonbodydepthfov != 0 ? firstpersonbodydepthfov : curfov, depth);
                renderplayer(focus, 2, focus->curscale, MDL_NOBATCH, color);
            }
        }

        calcfirstpersontags(focus);

        if(drawtex != DRAWTEX_HALO) rendercheck(focus, false);
    }

    void initplayerpreview()
    {
        previewent = new gameent;
        previewent->state = CS_ALIVE;
        previewent->actortype = A_PLAYER;
        previewent->physstate = PHYS_FLOOR;
        previewent->spawnstate(G_DEATHMATCH, 0, -1, previewent->gethealth(G_DEATHMATCH, 0));
        loopi(W_MAX) previewent->weapammo[i][W_A_CLIP] = W(i, ammoclip);
        loopi(W_MAX) previewent->weapammo[i][W_A_STORE] = W(i, ammostore) > 0 ? W(i, ammostore) : 0;
        previewent->xradius = previewent->yradius = actors[previewent->actortype].radius;
        previewent->zradius = actors[previewent->actortype].height;
        previewent->radius = max(previewent->xradius, previewent->yradius);
        previewent->aboveeye = 1;
    }

    void renderplayerpreview(float scale, const vec4 &mcolor, const char *actions, float yaw, float offsetyaw)
    {
        if(!previewent) initplayerpreview();
        previewent->yaw = yaw;
        if(actions && *actions) execute(actions);
        float height = previewent->height + previewent->aboveeye, zrad = height/2;
        vec2 xyrad = vec2(previewent->xradius, previewent->yradius).max(height/4);
        previewent->o = calcmodelpreviewpos(vec(xyrad, zrad), previewent->yaw).addz(previewent->height - zrad);
        previewent->yaw += offsetyaw;
        previewent->cleartags();
        renderplayer(previewent, 1, scale, 0, mcolor, true);
    }

    vec playerpreviewvanitypos(int vanity, bool relative)
    {
        if(!previewent || !vanitytypetags.inrange(vanity)) return vec(0, 0, 0);

        return relative ? vec(previewent->o).sub(previewent->tag[vanity]) : previewent->tag[vanity];
    }

    #define PLAYERPREV(name, arglist, argexpr, body) ICOMMAND(0, uiplayerpreview##name, arglist, argexpr, if(previewent) { body; });
    PLAYERPREV(state, "b", (int *n), previewent->state = *n >= 0 ? clamp(*n, 0, int(CS_MAX-1)) : int(CS_ALIVE));
    PLAYERPREV(yaw, "f", (float *n), previewent->yaw = *n);
    PLAYERPREV(pitch, "f", (float *n), previewent->pitch = *n);
    PLAYERPREV(roll, "f", (float *n), previewent->roll = *n);
    PLAYERPREV(move, "i", (int *n), previewent->move = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(strafe, "i", (int *n), previewent->strafe = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(turnside, "i", (int *n), previewent->turnside = *n != 0 ? (*n > 0 ? 1 : -1) : 0);
    PLAYERPREV(physstate, "b", (int *n), previewent->physstate = *n >= 0 ? clamp(*n, 0, int(PHYS_MAX-1)) : int(PHYS_FLOOR));
    PLAYERPREV(actortype, "b", (int *n), previewent->actortype = *n >= 0 ? clamp(*n, 0, int(A_MAX-1)) : int(A_PLAYER));
    PLAYERPREV(health, "bbb", (int *n, int *m, int *o), previewent->health = *n >= 0 ? *n : previewent->gethealth(*m >= 0 ? *m : G_DEATHMATCH, *o >= 0 ? *o : 0));
    PLAYERPREV(model, "i", (int *n), previewent->model = clamp(*n, 0, PLAYERTYPES-1));
    PLAYERPREV(colour, "ii", (int *n, int *v), previewent->colours[*v%2] = clamp(*n, 0, 0xFFFFFF));
    PLAYERPREV(team, "i", (int *n), previewent->team = clamp(*n, 0, int(T_LAST)));
    PLAYERPREV(privilege, "i", (int *n), previewent->privilege = clamp(*n, 0, int(PRIV_MAX-1)));
    PLAYERPREV(weapselect, "i", (int *n), previewent->weapselect = clamp(*n, 0, W_MAX-1));
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
    PLAYERPREV(vanity, "s", (char *n), previewent->setvanity(n));
    PLAYERPREV(mixer, "s", (char *n), previewent->setmixer(n));
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
