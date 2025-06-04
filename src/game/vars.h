namespace server
{
    extern void resetgamevars(bool all);
    extern void savegamevars();
    extern void setuptriggers(bool update, int id = -1);
}

GVAR(IDF_MAP, 0, numplayers, 0, 4, MAXCLIENTS); // 0 = determine from number of spawns
GVAR(IDF_MAP, 0, maxplayers, 0, 0, MAXCLIENTS); // 0 = numplayers*3
GVAR(IDF_MAP, 0, mapbalance, 0, 0, 3); // switches teams for asymmetrical maps, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced

GVAR(IDF_MAP, 0, hurtdelay, 0, 1000, VAR_MAX);
GVAR(IDF_MAP, 0, hurtdamage, 0, 30, VAR_MAX);
GVAR(IDF_MAP, 0, hurtresidual, 0, 0, W_R_ALL);

GVAR(IDF_MAP, 0, hurtburntime, 0, 5500, VAR_MAX);
GVAR(IDF_MAP, 0, hurtburndelay, 1, 1000, VAR_MAX);
GVAR(IDF_MAP, 0, hurtburndamage, 0, 30, VAR_MAX);
GVAR(IDF_MAP, 0, hurtbleedtime, 0, 5500, VAR_MAX);
GVAR(IDF_MAP, 0, hurtbleeddelay, 0, 1000, VAR_MAX);
GVAR(IDF_MAP, 0, hurtbleeddamage, 0, 30, VAR_MAX);
GVAR(IDF_MAP, 0, hurtshocktime, 0, 5500, VAR_MAX);
GVAR(IDF_MAP, 0, hurtshockdelay, 0, 1000, VAR_MAX);
GVAR(IDF_MAP, 0, hurtshockdamage, 0, 20, VAR_MAX);
GVAR(IDF_MAP, 0, hurtshockstun, 0, W_N_ST, W_N_ALL);
GFVAR(IDF_MAP, 0, hurtshockstunscale, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_MAP, 0, hurtshockstunfall, 0, 0.01f, FVAR_MAX);
GVAR(IDF_MAP, 0, hurtshockstuntime, 0, 500, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, shocktwitchvel, 0, 2, FVAR_MAX);
GVAR(IDF_MAP, 0, hurtcorrodetime, 0, 5500, VAR_MAX);
GVAR(IDF_MAP, 0, hurtcorrodedelay, 0, 1000, VAR_MAX);
GVAR(IDF_MAP, 0, hurtcorrodedamage, 0, 15, VAR_MAX);

GFVAR(IDF_MAP, 0, gravity, 0, 50.f, FVAR_MAX); // gravity
GFVAR(IDF_GAMEMOD, 0, gravityscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravitycrouch, 0, 1.25f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, buoyancycrouch, 0, 0, FVAR_MAX);

#define LIQUIDVARS(type, name) \
    GFVAR(IDF_MAP, 0, name##buoyancy##type, FVAR_MIN, 50.f, FVAR_MAX); \
    GFVAR(IDF_GAMEMOD, 0, name##buoyancyscale##type, 0, 1, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##falldist##type, 0, 12.f, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##fallspeed##type, FVAR_MIN, 250.f, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##fallpush##type, 0, 0.35f, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##speed##type, 0, 0.85f, 1); \
    GFVAR(IDF_GAMEMOD, 0, name##speedscale##type, 0, 1, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##coast##type, FVAR_NONZERO, 10.f, 1000); \
    GFVAR(IDF_GAMEMOD, 0, name##coastscale##type, FVAR_NONZERO, 1, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##boost##type, 0, 0.5f, 1); \
    GFVAR(IDF_GAMEMOD, 0, name##boostscale##type, 0, 1, FVAR_MAX); \
    GFVAR(IDF_MAP, 0, name##submerge##type, 0, 0.75f, 1); \
    GFVAR(IDF_GAMEMOD, 0, name##submergescale##type, 0, 1, FVAR_MAX);

LIQUIDVARS(, water)
LIQUIDVARS(, water2)
LIQUIDVARS(, water3)
LIQUIDVARS(, water4)
LIQUIDVARS(alt, water)
LIQUIDVARS(alt, water2)
LIQUIDVARS(alt, water3)
LIQUIDVARS(alt, water4)
LIQUIDVARS(, lava)
LIQUIDVARS(, lava2)
LIQUIDVARS(, lava3)
LIQUIDVARS(, lava4)
LIQUIDVARS(alt, lava)
LIQUIDVARS(alt, lava2)
LIQUIDVARS(alt, lava3)
LIQUIDVARS(alt, lava4)

#define WATERVARS(type, name) \
    GFVAR(IDF_MAP, 0, name##extinguish##type, 0, 0.25f, 1); \
    GFVAR(IDF_GAMEMOD, 0, name##extinguishscale##type, 0, 1, FVAR_MAX);

WATERVARS(, water)
WATERVARS(, water2)
WATERVARS(, water3)
WATERVARS(, water4)
WATERVARS(alt, water)
WATERVARS(alt, water2)
WATERVARS(alt, water3)
WATERVARS(alt, water4)

#define LAVAVARS(type, name) \
    GVAR(IDF_MAP, 0, name##burntime##type, 0, 5500, VAR_MAX); \
    GVAR(IDF_MAP, 0, name##burndelay##type, 1, 1000, VAR_MAX);

LAVAVARS(, lava)
LAVAVARS(, lava2)
LAVAVARS(, lava3)
LAVAVARS(, lava4)
LAVAVARS(alt, lava)
LAVAVARS(alt, lava2)
LAVAVARS(alt, lava3)
LAVAVARS(alt, lava4)

GFVAR(IDF_MAP, 0, floorcoast, FVAR_NONZERO, 5.f, 1000);
GFVAR(IDF_GAMEMOD, 0, floorcoastscale, FVAR_NONZERO, 1, FVAR_MAX);
GFVAR(IDF_MAP, 0, aircoast, FVAR_NONZERO, 25.f, 1000);
GFVAR(IDF_GAMEMOD, 0, aircoastscale, FVAR_NONZERO, 1, FVAR_MAX);
GFVAR(IDF_MAP, 0, slidecoast, FVAR_NONZERO, 50.f, 1000);
GFVAR(IDF_GAMEMOD, 0, slidecoastscale, FVAR_NONZERO, 1, FVAR_MAX);

GVARF(IDF_GAMEMOD, 0, forcemapvariant, 0, 0, MPV_MAX-1, if(sv_forcemapvariant) server::changemapvariant(sv_forcemapvariant), if(forcemapvariant) changemapvariant(forcemapvariant));
GVARF(IDF_GAMEMOD, 0, forcetriggerid, -1, -1, TRIGGERIDS, server::setuptriggers(true, sv_forcetriggerid), );

GVAR(0, PRIV_ADMINISTRATOR, serverclients, 1, 16, MAXCLIENTS);
GVAR(0, PRIV_ADMINISTRATOR, serverspectators, -1, 0, MAXCLIENTS); // -1 = copy serverclients, 0+ = spectator slots
GVARF(0, PRIV_ADMINISTRATOR, serverdupclients, 0, 0, MAXCLIENTS, limitdupclients(), );
GVAR(0, PRIV_ADMINISTRATOR, serveropen, 0, 3, 3);
GSVAR(0, PRIV_ADMINISTRATOR, serverdesc, "");
GSVAR(0, PRIV_ADMINISTRATOR, servermotd, "");

GVAR(0, PRIV_ADMINISTRATOR, autoadmin, 0, 0, 1);
GVAR(0, PRIV_MODERATOR, varslock, 0, PRIV_MODERATOR, PRIV_MAX);

GVAR(0, PRIV_ADMINISTRATOR, queryinterval, 0, 5000, VAR_MAX); // rebuild client list for server queries this often
GVAR(0, PRIV_ADMINISTRATOR, masterinterval, 300000, 300000, VAR_MAX); // keep connection alive every this often
GVAR(0, PRIV_ADMINISTRATOR, connecttimeout, 5000, 30000, VAR_MAX); // disconnected when attempt exceeds this time
GVAR(0, PRIV_ADMINISTRATOR, allowtimeout, 0, 3600000, VAR_MAX); // temporary allows last this long
GVAR(0, PRIV_ADMINISTRATOR, bantimeout, 0, 14400000, VAR_MAX); // temporary bans last this long
GVAR(0, PRIV_ADMINISTRATOR, mutetimeout, 0, 3600000, VAR_MAX); // temporary mutes last this long
GVAR(0, PRIV_ADMINISTRATOR, limittimeout, 0, 3600000, VAR_MAX); // temporary limits last this long
GVAR(0, PRIV_ADMINISTRATOR, excepttimeout, 0, 3600000, VAR_MAX); // temporary excepts last this long

GVAR(0, PRIV_ADMINISTRATOR, connectlock, 0, PRIV_NONE, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, messagelock, 0, PRIV_NONE, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, messagelength, 32, 128, BIGSTRLEN-1);
GSVAR(0, PRIV_ADMINISTRATOR, censorwords, "");

GVAR(0, PRIV_ADMINISTRATOR, setinfolock, 0, PRIV_NONE, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, setinfowait, 0, 1000, VAR_MAX);

GVAR(0, PRIV_ADMINISTRATOR, demolock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, democount, 1, 10, VAR_MAX);
GVAR(0, PRIV_ADMINISTRATOR, demomaxsize, 1, 16777216, VAR_MAX - 0x10000); // Variable is in bytes. It should fit in int type. See src/game/server.cpp:adddemo()
GVAR(0, PRIV_ADMINISTRATOR, demoautorec, 0, 1, 1); // 0 = off, 1 = automatically record demos each match
GVAR(0, PRIV_ADMINISTRATOR, demokeep, 0, 0, 1); // 0 = off, 1 = keep demos that don't run to end of match
GVAR(0, PRIV_ADMINISTRATOR, demoautoserversave, 0, 0, 1);
GVAR(0, PRIV_ADMINISTRATOR, demoserverkeeptime, 0, 86400, VAR_MAX);

GVAR(0, PRIV_ADMINISTRATOR, speclock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, teamlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, kicklock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, allowlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, banlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, mutelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, limitlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, exceptlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, vetolock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, editlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, spawnlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked, require this to spawn
GVAR(0, PRIV_ADMINISTRATOR, spawneditlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked in editmode, require this to spawn
GVAR(0, PRIV_ADMINISTRATOR, masterlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, crclock, 0, PRIV_MAX, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, iphostlock, 0, 0, PRIV_MAX); // require this level to see ip/hosts

GVAR(0, PRIV_ADMINISTRATOR, overflowlock, 0, PRIV_MODERATOR, PRIV_MAX); // normal message queue override
GVAR(0, PRIV_ADMINISTRATOR, overflowsize, 0, 255, VAR_MAX); // kick if queued messages >= this

GVAR(0, PRIV_ADMINISTRATOR, floodlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, floodmute, 0, 3, VAR_MAX); // automatically mute player when warned this many times
GVAR(0, PRIV_ADMINISTRATOR, floodtime, 250, 10000, VAR_MAX); // time span to check for floody messages
GVAR(0, PRIV_ADMINISTRATOR, floodlines, 1, 5, VAR_MAX); // number of lines in aforementioned span before too many

GVAR(0, PRIV_ADMINISTRATOR, teamkilllock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_ADMINISTRATOR, teamkillwarn, 1, 3, VAR_MAX); // automatically warn player every this many team kills
GVAR(0, PRIV_ADMINISTRATOR, teamkillkick, 0, 3, VAR_MAX); // automatically kick player at this many warnings
GVAR(0, PRIV_ADMINISTRATOR, teamkillban, 0, 4, VAR_MAX); // automatically ban player at this many warnings
GVAR(0, PRIV_ADMINISTRATOR, teamkilltime, 0, 5, VAR_MAX); // time threshold (in minutes) to count
GVAR(0, PRIV_ADMINISTRATOR, teamkillrestore, 0, 1, VAR_MAX); // restore the team score as if the offender was never there if it was by this much

GVAR(0, PRIV_ADMINISTRATOR, autospectate, 0, 1, 1); // auto spectate if idle, 1 = auto spectate when remaining dead for autospecdelay
GVAR(0, PRIV_ADMINISTRATOR, autospecdelay, 0, 60000, VAR_MAX);

GVAR(0, PRIV_ADMINISTRATOR, resetallowsonend, 0, 0, 2); // reset allows on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetbansonend, 0, 0, 2); // reset bans on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetmutesonend, 0, 0, 2); // reset mutes on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetlimitsonend, 0, 0, 2); // reset limits on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetexceptsonend, 0, 0, 2); // reset excepts on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetvarsonend, 0, 1, 2); // reset variables on end (1: just when empty, 2: when matches end)
GVAR(0, PRIV_ADMINISTRATOR, resetmmonend, 0, 2, 2); // reset mastermode on end (1: just when empty, 2: when matches end)

GVARF(IDF_GAMEMOD, PRIV_ADMINISTRATOR, gamespeed, 1, 100, 10000, timescale = sv_gamespeed, timescale = gamespeed);
GVARF(0, PRIV_ADMINISTRATOR, gamepaused, 0, 0, 1, paused = sv_gamepaused, paused = gamepaused);

GSVAR(0, PRIV_ADMINISTRATOR, defaultmap, "");
GVAR(0, PRIV_ADMINISTRATOR, defaultmode, G_START, G_DEATHMATCH, G_MAX-1);
GVAR(0, PRIV_ADMINISTRATOR, defaultmuts, 0, 0, G_M_ALL);

GSVAR(0, PRIV_ADMINISTRATOR, allowmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, wipmaps, "");

GSVAR(0, PRIV_ADMINISTRATOR, mainmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, capturemaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, defendmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, kingmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, bombermaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, holdmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, speedrunmaps, "");

GSVAR(0, PRIV_ADMINISTRATOR, duelmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, gladiatormaps, "");

GSVAR(0, PRIV_ADMINISTRATOR, smallmaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, mediummaps, "");
GSVAR(0, PRIV_ADMINISTRATOR, largemaps, "");

GVAR(0, PRIV_MODERATOR, modelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_MODERATOR, modelocktype, 0, 2, 2); // 0 = off, 1 = only lock level can change modes, 2 = lock level can set limited modes
GVAR(0, PRIV_MODERATOR, modelockfilter, 0, G_LIMIT, G_ALL);
GVAR(0, PRIV_MODERATOR, mutslockfilter, 0, G_M_FILTER, G_M_ALL);
GVAR(0, PRIV_MODERATOR, mutslockforce, 0, 0, G_M_ALL);

GVAR(0, PRIV_MODERATOR, mapsfilter, 0, 1, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(0, PRIV_MODERATOR, mapslock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_MODERATOR, mapslocktype, 0, 2, 2); // 0 = off, 1 = allow maps, 2 = rotation maps

GSVAR(IDF_READONLY, 0, previousmaps, "");
GVAR(0, PRIV_MODERATOR, maphistory, 0, 1, VAR_MAX);

GVAR(0, PRIV_MODERATOR, rotatemaps, 0, 2, 2); // 0 = off, 1 = sequence, 2 = random
GVAR(0, PRIV_MODERATOR, rotatemode, 0, 1, 1);
GVARF(0, PRIV_MODERATOR, rotatemodefilter, 0, G_LIMIT, G_ALL, sv_rotatemodefilter &= ~G_NEVER, rotatemodefilter &= ~G_NEVER); // modes not in this array are filtered out
GVAR(0, PRIV_MODERATOR, rotatemuts, 0, 3, VAR_MAX); // any more than one decreases the chances of it picking
GVAR(0, PRIV_MODERATOR, rotatemutsfilter, 0, G_M_ROTATE, G_M_ALL); // mutators not in this array are filtered out
GVAR(0, PRIV_MODERATOR, rotatemapsfilter, 0, 2, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(0, PRIV_MODERATOR, rotatecycle, 0, 10, VAR_MAX); // 0 = off, else = minutes between forced re-cycles

GVAR(0, PRIV_MODERATOR, votelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(0, PRIV_MODERATOR, votelocktype, 0, 2, 2); // 0 = off, 1 = lock level only, 2 = lock level can select previousmaps
GVAR(0, PRIV_MODERATOR, votefilter, 0, 1, 1); // 0 = off, 1 = skip spectators
GVAR(0, PRIV_MODERATOR, votewait, 0, 2500, VAR_MAX);
GVAR(0, PRIV_MODERATOR, votestyle, 0, 2, 2); // 0 = votes don't pass mid-match, 1 = passes if votethreshold is met, 2 = passes if unanimous
GVAR(0, PRIV_MODERATOR, voteinterm, 0, 2, 3); // 0 = must wait entire time, 1 = passes if votethreshold is met, 2 = passes if unanimous, 3 = passes after waiting then selects a random vote
GFVAR(0, PRIV_MODERATOR, votethreshold, 0, 0.5f, 1); // auto-pass votes when this many agree

GVAR(0, PRIV_MODERATOR, smallmapmax, 0, 6, VAR_MAX); // maximum number of players for a small map
GVAR(0, PRIV_MODERATOR, mediummapmax, 0, 12, VAR_MAX); // maximum number of players for a medium map

GVAR(0, PRIV_MODERATOR, waitforplayers, 0, 2, 2); // wait for players: 0 = off, 1 = to load the map, 2 = to exit spectator
GVAR(0, PRIV_MODERATOR, waitforplayertime, 0, 5000, VAR_MAX); // wait at most this long for players to meet waitforplayers criteria
GVAR(0, PRIV_MODERATOR, waitforplayerload, 0, 15000, VAR_MAX); // wait this long for players to load the map to schedule map requests
GVAR(0, PRIV_MODERATOR, waitforplayermaps, 0, 30000, VAR_MAX); // wait this long for sendmap and getmap requests
GVAR(0, PRIV_MODERATOR, waitforplayerinfo, 0, 15000, VAR_MAX); // wait at least this long for players to send info

GICOMMAND(0, 0, resetvars, "i", (int *n), server::resetgamevars(*n!=0); result("success"), );
GICOMMAND(0, PRIV_ADMINISTRATOR, savevars, "", (), server::savegamevars(); result("success"), );
GICOMMAND(0, PRIV_MODERATOR, resetconfig, "", (), rehash(true); result("success"), );

GFVAR(0, PRIV_MODERATOR, maxalive, 0, 0, FVAR_MAX); // only allow this*maxplayers to be alive at once, 0 = turn off maxalive
GVAR(0, PRIV_MODERATOR, maxalivequeue, 0, 1, 1); // upon triggering maxalive, use a queue system
GVAR(0, PRIV_MODERATOR, maxaliveminimum, 2, 4, VAR_MAX); // kicks in if alive >= this
GFVAR(0, PRIV_MODERATOR, maxalivethreshold, 0, 0, FVAR_MAX); // .. or this percentage of clients

GVAR(IDF_GAMEMOD, 0, spawnrotate, 0, 2, 2); // 0 = let client decide, 1 = sequence, 2 = random
GVAR(IDF_GAMEMOD, 0, spawnprotect, 0, 3000, VAR_MAX); // delay before damage can be dealt to spawning player
GVAR(IDF_GAMEMOD, 0, duelprotect, 0, 3000, VAR_MAX); // .. in duel/survivor matches
GVAR(IDF_GAMEMOD, 0, survivorprotect, 0, 5000, VAR_MAX); // .. in duel/survivor matches
GVAR(IDF_GAMEMOD, 0, instaprotect, 0, 3000, VAR_MAX); // .. in instagib matches
GVAR(IDF_GAMEMOD, 0, protectbreakshoot, 0, 1, 1); // 0 = off, 1 = protection is broken when player starts firing
GVAR(IDF_GAMEMOD, 0, protectbreakcook, 0, 1, 1); // 0 = off, 1 = protection is broken when player starts cooking
GVAR(IDF_GAMEMOD, 0, protectbreakaffinity, 0, 1, 1); // 0 = off, 1 = protection is broken when player takes an affinity

GVAR(IDF_GAMEMOD, 0, radarhardaffinity, 0, 1, 1); // 0 = do not allow showing affinities with hard mutator, 1 = allow it
GVAR(IDF_GAMEMOD, 0, radardisabled, 0, 0, 1); // forces the radar to be off
GFVAR(IDF_GAMEMOD, 0, radardistlimit, 0, 0, FVAR_MAX); // forces the radar to this distance max, 0 = off

GVAR(IDF_GAMEMOD, 0, balancemaps, -1, -1, 3); // determined if map team balancing is used: -1 = map default, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced
GVAR(IDF_GAMEMOD, 0, balancereset, 0, 2, 2); // reset players when balancing them, 0 = off, 1 = only when necessary, 2 = always
GVAR(IDF_GAMEMOD, 0, balancedelay, 0, 5000, VAR_MAX); // before mapbalance forces
GVAR(IDF_GAMEMOD, 0, balancenospawn, 0, 1, 1); // prevent respawning when waiting to balance
GVAR(IDF_GAMEMOD, 0, balanceduke, 0, 1, 1); // enable in duel/survivor

GFVAR(IDF_GAMEMOD, 0, healthscale, 0, 1.0f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, healthscalehard, 0, 1.0f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, maxhealth, 0, 1.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, maxhealthvampire, 0, 2.0f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, vampirescale, 0, 1.0f, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, maxresizescale, 1, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, minresizescale, FVAR_NONZERO, 0.5f, 1);
GFVAR(IDF_GAMEMOD, 0, instaresizeamt, FVAR_NONZERO, 0.1f, 1); // each kill adds this much size in insta-resize

GVAR(IDF_GAMEMOD, 0, regendelay, 0, 5000, VAR_MAX); // regen after no damage for this long
GVAR(IDF_GAMEMOD, 0, regentime, 0, 1000, VAR_MAX); // regen this often when regenerating normally
GVAR(IDF_GAMEMOD, 0, regenhealth, 0, 50, VAR_MAX); // regen this amount each regen
GVAR(IDF_GAMEMOD, 0, regendecay, 0, 50, VAR_MAX); // if over maxhealth, decay this amount each regen

GVAR(IDF_GAMEMOD, 0, noweapburn, 0, 1, 1);
GVAR(IDF_GAMEMOD, 0, noweapbleed, 0, 1, 1);
GVAR(IDF_GAMEMOD, 0, noweapshock, 0, 1, 1);
GVAR(IDF_GAMEMOD, 0, noweapcorrode, 0, 1, 1);

GVAR(IDF_GAMEMOD, 0, kamikaze, 0, 1, 3); // 0 = never, 1 = holding grenade, 2 = have grenade, 3 = always
GVAR(IDF_GAMEMOD, 0, prizegrenade, 0, 10, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, prizemine, 0, 10, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, prizerocket, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, prizeminigun, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, prizejetsaw, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, prizeeclipse, 0, 5, VAR_MAX);

//  dm          duel        survivor    gladiator   capture     defend      defendking  bomber      bomberhold  speedrun    speedrunlapped  speedrungauntlet
MMVAR(IDF_GAMEMOD, 0, timelimit, 0, VAR_MAX,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,             10
);
MMVAR(IDF_GAMEMOD, 0, overtimeallow, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,              1
);
MMVAR(IDF_GAMEMOD, 0, overtimelimit, 0, VAR_MAX,
    5,          2,          3,          2,          5,          5,          5,          5,          5,          5,          5,              5
);
#ifdef CPP_GAME_SERVER
SVARR(limitidxname, "duel survivor gladiator capture defend defendking bomber bomberhold speedrun speedrunlapped speedrungauntlet");
#endif

GVAR(IDF_GAMEMOD, 0, intermlimit, 0, 10000, VAR_MAX); // .. before vote menu comes up
GVAR(IDF_GAMEMOD, 0, votelimit, 0, 50000, VAR_MAX); // .. before vote passes by default

GVAR(IDF_GAMEMOD, 0, duelrandom, 0, 1, 1); // randomise queue before first round
GVAR(IDF_GAMEMOD, 0, duelreset, 0, 1, 1); // reset winner in duel
GVAR(IDF_GAMEMOD, 0, duelclear, 0, 1, 1); // clear items in duel
GVAR(IDF_GAMEMOD, 0, duelregen, 0, 0, 1); // allow regen in duel
GVAR(IDF_GAMEMOD, 0, dueldelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, 0, duelcooloff, 0, 3000, VAR_MAX); // cool off period before duel goes to next round
GVAR(IDF_GAMEMOD, 0, duelcycle, 0, 2, 3); // determines if players are force-cycled after a certain number of wins (bit: 0 = off, 1 = non-team games, 2 = team games)
GVAR(IDF_GAMEMOD, 0, duelcycles, 0, 2, VAR_MAX); // maximum wins in a row before force-cycling (0 = num team/total players)
GVAR(IDF_GAMEMOD, 0, duelaffinity, 0, 1, 2); // 0 = off, 1 = on enter can respawn next iter, 2 = on enter can respawn immediately
GVAR(IDF_GAMEMOD, 0, duelovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, 0, duelmaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for duel. 0 = any number of players

GVAR(IDF_GAMEMOD, 0, survivorreset, 0, 1, 1); // reset winners in survivor
GVAR(IDF_GAMEMOD, 0, survivorclear, 0, 1, 1); // clear items in survivor
GVAR(IDF_GAMEMOD, 0, survivorregen, 0, 0, 1); // allow regen in survivor
GVAR(IDF_GAMEMOD, 0, survivordelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, 0, survivorcooloff, 0, 3000, VAR_MAX); // cool off period before survivor goes to next round
GVAR(IDF_GAMEMOD, 0, survivoraffinity, 0, 0, 1); // 0 = off, 1 = on enter can spawn immediately
GVAR(IDF_GAMEMOD, 0, survivorovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, 0, survivormaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for survivor. 0 = any number of players

GVAR(IDF_GAMEMOD, 0, pointlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, 0, fraglimit, 0, 0, VAR_MAX); // finish when score is this or more in oldschool
GVAR(IDF_GAMEMOD, 0, speedrunlimit, 0, 0, VAR_MAX); // finish when lap count is this or more
GVAR(IDF_GAMEMOD, 0, teampersist, 0, 1, 2); // 0 = off, 1 = only attempt, 2 = forced
GVAR(IDF_GAMEMOD, 0, damageself, 0, 1, 1); // 0 = off, 1 = either hurt self or use damageteam rules
GFVAR(IDF_GAMEMOD, 0, damageselfscale, 0, 1, FVAR_MAX); // 0 = off, anything else = scale for damage
GFVAR(IDF_GAMEMOD, 0, damageteamscale, 0, 0.5f, FVAR_MAX); // 0 = off, anything else = scale for damage

GVAR(IDF_GAMEMOD, 0, teambalance, 0, 6, 6); // 0 = off, 1 = by number then style, 2 = by style then number, 3 = by number and enforce, 4 = number, enforce, reassign, 5 = style, number, enforce, reassign, 6 = style during waiting, revert to 4 otherwise
GVAR(IDF_GAMEMOD, 0, teambalanceduel, 0, 0, 1); // allow reassignments in duel
GVAR(IDF_GAMEMOD, 0, teambalanceplaying, 2, 2, VAR_MAX); // min players before reassignments occur
GVAR(IDF_GAMEMOD, 0, teambalanceamt, 2, 2, VAR_MAX); // max-min offset before reassignments occur
GVAR(IDF_GAMEMOD, 0, teambalancewait, 0, 10000, VAR_MAX); // how long before can happen again
GVAR(IDF_GAMEMOD, 0, teambalancedelay, 0, 3000, VAR_MAX); // how long before reassignments start
GVAR(IDF_GAMEMOD, 0, teambalanceswap, 0, 1, 1); // allow swap requests if unable to change team
GVAR(IDF_GAMEMOD, 0, teambalancelock, 0, PRIV_MODERATOR, PRIV_MAX); // level at which one can override swap
GVAR(IDF_GAMEMOD, 0, teambalancestyle, 0, 7, 7); // when moving players, sort by: 0 = top of list, 1 = time played, 2 = points, 3 = frags, 4 = scoretime, 5 = kdratio, 6 = combined kdratio, 7 = average scoreboard position
GVAR(IDF_GAMEMOD, 0, teambalancehighest, 0, 0, 1); // when moving players, move highest first

GVAR(IDF_GAMEMOD, 0, speedrungauntletwinner, 0, 1, 1); // declare the winner when the final team exceeds best score
GVAR(IDF_GAMEMOD, 0, speedruncheckpointstrict, 0, 1, 1); // if checkpoints are linked, only allow next link

GVAR(IDF_GAMEMOD, 0, capturelimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, 0, captureresetfloor, 0, 1, 2); // if tossed, reset to last floor pos. 1 = team tosses own flag, 2 = any flag
GVAR(IDF_GAMEMOD, 0, captureresetstore, 0, 2, 15);
GVAR(IDF_GAMEMOD, 0, captureresetdelay, 0, 30000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, capturedefenddelay, 0, 10000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, captureprotectdelay, 0, 15000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, capturepickupdelay, 500, 2500, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturedropheight, 0, 8, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, capturepoints, 0, 5, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, capturepickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, capturecollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, 0, captureextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, 0, captureinteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, 0, capturerelativity, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, captureelasticity, FVAR_MIN, 0.65f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, captureliquidcoast, FVAR_NONZERO, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, captureweight, FVAR_MIN, 400, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturebuoyancy, FVAR_MIN, 300, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturespeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturespeedmax, 0, 100, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturerepulsion, 0, 36, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturerepelspeed, 0, 50, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, capturerepeldelay, 0, 100, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturethreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(IDF_GAMEMOD, 0, capturebuffing, 0, 15, 63); // buffed; 0 = off, &1 = when defending, &2 = when defending dropped, &4 = when secured, &8 = when defending secured, &16 = when secured enemy, &32 = when defending secured enemy, &64 = when defending own base
GVAR(IDF_GAMEMOD, 0, capturebuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, 0, capturebuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, 0, capturebuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, 0, capturebuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, 0, captureregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, 0, captureregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, 0, captureregenextra, 0, 20, VAR_MAX); // add this to regen when buffed
GVAR(IDF_GAMEMOD, 0, captureretakedelay, 0, 1000, VAR_MAX); // same person can't take same flag this long after capturing it
GFVAR(IDF_GAMEMOD, 0, capturecarryimpulsespeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryimpulsespeedeach, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryspeedeach, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryweight, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryweighteach, FVAR_MIN, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarrybuoyancy, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarrybuoyancyeach, FVAR_MIN, 0, FVAR_MAX);

GVAR(IDF_GAMEMOD, 0, defendlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, 0, defendpoints, 0, 1, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, defendinterval, 1, 50, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, defendhold, 1, 100, VAR_MAX); // points needed to gain a score point
GVAR(IDF_GAMEMOD, 0, defendoccupy, 1, 100, VAR_MAX); // points needed to occupy in regular games
GVAR(IDF_GAMEMOD, 0, defendking, 1, 100, VAR_MAX); // points needed to occupy in king of the hill
GVAR(IDF_GAMEMOD, 0, defendflags, 0, 3, 3); // 0 = init all (neutral), 1 = init neutral and team only, 2 = init team only, 3 = init all (team + neutral + converted)
GVAR(IDF_GAMEMOD, 0, defendbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = even when enemies are present
GFVAR(IDF_GAMEMOD, 0, defendbuffoccupy, 0, 0.5f, 1); // for defendbuffing&4, must be occupied this much before passing
GVAR(IDF_GAMEMOD, 0, defendbuffdelay, 0, 1000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, 0, defendbuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, 0, defendbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, 0, defendbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, 0, defendregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, 0, defendregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, 0, defendregenextra, 0, 20, VAR_MAX); // add this to regen when buffed

GVAR(IDF_GAMEMOD, 0, bomberlimit, 0, 0, VAR_MAX); // finish when score is this or more (non-hold)
GVAR(IDF_GAMEMOD, 0, bomberholdlimit, 0, 0, VAR_MAX); // finish when score is this or more (hold)
GVAR(IDF_GAMEMOD, 0, bomberbasketonly, 0, 0, 1); // prohibit touchdowns in basket game
GVAR(IDF_GAMEMOD, 0, bomberassaultreset, 0, 1, 1); // defenders reset rather than carry the ball
GVAR(IDF_GAMEMOD, 0, bomberassaultwinner, 0, 1, 1); // declare the winner when the final team exceeds best score
GFVAR(IDF_GAMEMOD, 0, bomberbasketmindist, 0, 0, FVAR_MAX); // prohibit baskets less than this far away
GVAR(IDF_GAMEMOD, 0, bomberwait, 0, 1000, VAR_MAX); // delay before bomb spawns
GVAR(IDF_GAMEMOD, 0, bomberresetdelay, 0, 15000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, bomberpickupdelay, 500, 7500, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberdropheight, 0, 8, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, bomberpoints, 0, 5, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, bomberpenalty, 0, 5, VAR_MAX); // points taken from score
GVAR(IDF_GAMEMOD, 0, bomberpickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, bomberthrowinpoints, 0, 3, VAR_MAX); // team points for throw in
GVAR(IDF_GAMEMOD, 0, bombertouchdownpoints, 0, 1, VAR_MAX); // team points for touch down
GVAR(IDF_GAMEMOD, 0, bomberholdtime, 0, 0, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, bomberholdpoints, 0, 1, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, bomberholdpenalty, 0, 10, VAR_MAX); // penalty for holding too long
GVAR(IDF_GAMEMOD, 0, bomberholdinterval, 0, 1000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, bomberlockondelay, 0, 250, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberspeed, 0, 250, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberspeeddelta, 0, 1000, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberspeedmin, 0, 65, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberspeedmax, 0, 200, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, bombercollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, 0, bomberextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, 0, bomberinteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, 0, bomberrelativity, 0, 0.25f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberelasticity, FVAR_MIN, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberliquidcoast, FVAR_NONZERO, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberweight, FVAR_MIN, 350, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberbuoyancy, FVAR_MIN, 400, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberthreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(IDF_GAMEMOD, 0, bomberbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = when secured as defender in attack
GVAR(IDF_GAMEMOD, 0, bomberbuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, 0, bomberbuffarea, FVAR_NONZERO, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, 0, bomberbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, 0, bomberbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, 0, bomberregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, 0, bomberregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, 0, bomberregenextra, 0, 20, VAR_MAX); // add this to regen when buffed
GVAR(IDF_GAMEMOD, 0, bombercarrytime, 0, 15000, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryimpulsespeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryweight, FVAR_MIN, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarrybuoyancy, FVAR_MIN, 5, FVAR_MAX);

GFVAR(0, PRIV_MODERATOR, aihostnum, 0, 50, FVAR_MAX); // Multiply number of bots hosted by this much for the bot hosting decision.
GFVAR(0, PRIV_MODERATOR, aihostping, 0, 1, FVAR_MAX); // Multiply host ping by this much for the bot hosting decision.
GFVAR(0, PRIV_MODERATOR, aihostshift, 0, 75, FVAR_MAX); // Require this much difference before shifting bot hosts.
GVAR(0, PRIV_MODERATOR, airefreshdelay, 0, 1000, VAR_MAX);
GVAR(0, PRIV_MODERATOR, aiweightdrag, 0, 5000, VAR_MAX);
GFVAR(0, PRIV_MODERATOR, aiweightpull, 0, 1, FVAR_MAX);

GVAR(IDF_GAMEMOD, 0, botbalance, -1, -1, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GFVAR(IDF_GAMEMOD, 0, botbalancescale, FVAR_NONZERO, 1, FVAR_MAX); // use balance*this
GVAR(IDF_GAMEMOD, 0, botbalanceduel, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(IDF_GAMEMOD, 0, botbalancesurvivor, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(IDF_GAMEMOD, 0, botskillmin, 1, 65, 101);
GVAR(IDF_GAMEMOD, 0, botskillmax, 1, 90, 101);
GFVAR(IDF_GAMEMOD, 0, botskillfrags, -100, -0.1f, 100);
GFVAR(IDF_GAMEMOD, 0, botskilldeaths, -100, 0.1f, 100);
GVAR(IDF_GAMEMOD, 0, botlimit, 0, 32, MAXAI);
GVAR(IDF_GAMEMOD, 0, botoffset, VAR_MIN, 0, VAR_MAX);
GSVAR(0, PRIV_MODERATOR, botmalenames, "");
GSVAR(0, PRIV_MODERATOR, botfemalenames, "");
GSVAR(0, PRIV_MODERATOR, botmalevanities, "");
GSVAR(0, PRIV_MODERATOR, botfemalevanities, "");
GSVAR(0, PRIV_MODERATOR, botmalemixers, "");
GSVAR(0, PRIV_MODERATOR, botfemalemixers, "");
GVAR(IDF_GAMEMOD, 0, botcolourseed, 0, 15, 15); // random bot things will be determined by colour as seed, bitwise: 0 = off, 1 = skill, 2 = name, 4 = model, 8 = loadout
GVAR(IDF_GAMEMOD, 0, botrandomcase, 0, 2, VAR_MAX); // bots will randomise the first letter of their name if rnd(botrandomcase) > 0
GVAR(0, PRIV_ADMINISTRATOR, botoverridelock, PRIV_NONE, PRIV_NONE, PRIV_MAX);

GFVAR(IDF_GAMEMOD, 0, coopbalance, FVAR_NONZERO, 1.5f, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, coopskillmin, 1, 80, 101);
GVAR(IDF_GAMEMOD, 0, coopskillmax, 1, 90, 101);
GFVAR(IDF_GAMEMOD, 0, coopskillfrags, -100, 0, 100);
GFVAR(IDF_GAMEMOD, 0, coopskilldeaths, -100, 1, 100);

GVAR(IDF_GAMEMOD, 0, enemybalance, 1, 1, 3);
GVAR(IDF_GAMEMOD, 0, enemyskillmin, 1, 55, 101);
GVAR(IDF_GAMEMOD, 0, enemyskillmax, 1, 75, 101);
GVAR(IDF_GAMEMOD, 0, enemylimit, 0, 64, MAXAI); // maximum number of enemies
GVAR(IDF_GAMEMOD, 0, enemyspawntime, 1, 60000, VAR_MAX); // when enemies respawn
GVAR(IDF_GAMEMOD, 0, enemyspawndelay, 0, 1000, VAR_MAX); // after map start enemies first spawn
GVAR(IDF_GAMEMOD, 0, enemyspawnstyle, 0, 1, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both

GVAR(IDF_GAMEMOD, 0, ejectfade, 0, 2500, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, itemspawntime, 1, 10000, VAR_MAX); // when items respawn
GVAR(IDF_GAMEMOD, 0, itemspawndelay, 0, 1000, VAR_MAX); // after map start items first spawn
GVAR(IDF_GAMEMOD, 0, itemspawnstyle, 0, 0, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both
GVAR(IDF_GAMEMOD, 0, itemcollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, 0, itemextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, 0, iteminteracts, 0, 3, 3);
GVAR(IDF_GAMEMOD, 0, itemfadetime, 0, 2000, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemelasticity, FVAR_MIN, 0.4f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrelativity, FVAR_MIN, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemliquidcoast, FVAR_NONZERO, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemweight, FVAR_MIN, 150, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itembuoyancy, FVAR_MIN, 200, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemspeeddelta, 0, 1000, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemspeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemspeedmax, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrepulsion, 0, 16, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrepelspeed, 0, 50, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, itemrepeldelay, 0, 100, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemdropminspeed, 0, 15, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemdropspreadxy, 0, 10, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemdropspreadz, 0, 2, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, itemannouncespawn, 0, W_ANNOUNCE, W_MASK); // announce these item spawns
GVAR(IDF_GAMEMOD, 0, itemannounceuse, 0, W_ANNOUNCE, W_MASK); // announce these item uses
GVAR(IDF_GAMEMOD, 0, itemannouncedrop, 0, W_ANNOUNCE, W_MASK); // announce these item drops

GFVAR(IDF_GAMEMOD, 0, gibelasticity, -10000, 0.35f, 10000);
GFVAR(IDF_GAMEMOD, 0, gibrelativity, -10000, 0.95f, 10000);
GFVAR(IDF_GAMEMOD, 0, gibliquidcoast, 0, 2, 10000);
GFVAR(IDF_GAMEMOD, 0, gibweight, -10000, 150, 10000);
GFVAR(IDF_GAMEMOD, 0, gibbuoyancymax, -10000, 200, 10000);
GFVAR(IDF_GAMEMOD, 0, gibbuoyancymin, -10000, 0, 10000);
GVAR(IDF_GAMEMOD, 0, giblimit, 0, 25, VAR_MAX); // max in one burst
GVAR(IDF_GAMEMOD, 0, gibpieces, 1, 10, VAR_MAX); // max pieces
GVAR(IDF_GAMEMOD, 0, gibfade, 1, 60000, VAR_MAX); // rnd(this) + this
GVAR(IDF_GAMEMOD, 0, gibchancevanity, 0, 75, 100); // percentage chance
GVAR(IDF_GAMEMOD, 0, gibchancepieces, 0, 75, 100); // percentage chance
GVAR(IDF_GAMEMOD, 0, gibchancecollects, 0, 75, 100); // percentage chance
GFVAR(IDF_GAMEMOD, 0, gibdamage, 0, 0.1f, 1); // gibs = (damage / (hp * this))
GFVAR(IDF_GAMEMOD, 0, gibheadless, 0, 0.05f, 1);
GFVAR(IDF_GAMEMOD, 0, gibobliterated, 0, 0.025f, 1);

GFVAR(IDF_GAMEMOD, 0, vanityelasticity, -10000, 0.5f, 10000);
GFVAR(IDF_GAMEMOD, 0, vanityrelativity, -10000, 0.95f, 10000);
GFVAR(IDF_GAMEMOD, 0, vanityliquidcoast, 0, 2, 10000);
GFVAR(IDF_GAMEMOD, 0, vanityweight, -10000, 100, 10000);
GFVAR(IDF_GAMEMOD, 0, vanitybuoyancy, -10000, 50, 10000);

GFVAR(IDF_GAMEMOD, 0, debriselasticity, -10000, 0.6f, 10000);
GFVAR(IDF_GAMEMOD, 0, debrisliquidcoast, 0, 1.7f, 10000);
GFVAR(IDF_GAMEMOD, 0, debrisweight, -10000, 165, 10000);
GFVAR(IDF_GAMEMOD, 0, debrisbuoyancy, -10000, 0, 10000);

GFVAR(IDF_GAMEMOD, 0, ejectelasticity, -10000, 0.35f, 10000);
GFVAR(IDF_GAMEMOD, 0, ejectrelativity, -10000, 1, 10000);
GFVAR(IDF_GAMEMOD, 0, ejectliquidcoast, 0, 1.75f, 10000);
GFVAR(IDF_GAMEMOD, 0, ejectweight, -10000, 180, 10000);
GFVAR(IDF_GAMEMOD, 0, ejectbuoyancy, -10000, 0, 10000);

GVAR(IDF_GAMEMOD, 0, janitorlimit, 0, 8, MAXAI); // maximum number of janitors
GVAR(IDF_GAMEMOD, 0, janitorprize, 0, -1, W_PRIZES); // -1 = random, 0 = none, 1 = grenade, 2 = mine, 3 = rocket, 4 = minigun, 5 = jetsaw, 6 = eclipse
GVAR(IDF_GAMEMOD, 0, janitorghost, 0, 0, 1); // 0 = off, 1 = ghost until prize is ready
GVAR(IDF_GAMEMOD, 0, janitorready, 0, 1000, VAR_MAX); // ready to award a prize at this much stuff
GVAR(IDF_GAMEMOD, 0, janitorcollect, 0, 2000, VAR_MAX); // collect this much before dumping
GFVAR(IDF_GAMEMOD, 0, janitorbalance, 0, 0.33f, FVAR_MAX); // this * current balance = number of janitors
GFVAR(IDF_GAMEMOD, 0, janitorjunktime, 0, 0.25f, 1); // when shell casings and vanities/parts become "junk"
GFVAR(IDF_GAMEMOD, 0, janitorjunkitems, 0, 0.75f, 1); // when items become "junk"
GFVAR(IDF_GAMEMOD, 0, janitorjunkdebris, 0, 0.125f, 1); // when gibs and debris become "junk"
GFVAR(IDF_GAMEMOD, 0, janitorjunkmessy, 0, 0.25f, 1); // scale all junk delays by this much in messy modes
GFVAR(IDF_GAMEMOD, 0, janitorsuckdist, 0, 64.f, FVAR_MAX); // suck distance
GFVAR(IDF_GAMEMOD, 0, janitorsuckspeed, 0, 150.f, FVAR_MAX); // suck speed
GFVAR(IDF_GAMEMOD, 0, janitorreject, 0, 128.f, FVAR_MAX); // reject distance
GSVAR(0, PRIV_MODERATOR, janitorvanities, "");

GFVAR(IDF_GAMEMOD, 0, movespeed, FVAR_NONZERO, 1.0f, FVAR_MAX); // speed
GFVAR(IDF_GAMEMOD, 0, movecrawl, FVAR_NONZERO, 0.6f, FVAR_MAX); // crawl modifier
GFVAR(IDF_GAMEMOD, 0, moverun, FVAR_NONZERO, 1.25f, FVAR_MAX); // running modifier
GFVAR(IDF_GAMEMOD, 0, movestraight, FVAR_NONZERO, 1.2f, FVAR_MAX); // non-strafe modifier
GFVAR(IDF_GAMEMOD, 0, movestrafe, FVAR_NONZERO, 1.1f, FVAR_MAX); // strafe modifier
GFVAR(IDF_GAMEMOD, 0, moveinair, FVAR_NONZERO, 0.75f, FVAR_MAX); // in-air modifier
GFVAR(IDF_GAMEMOD, 0, movestepup, FVAR_NONZERO, 0.95f, FVAR_MAX); // step-up modifier
GFVAR(IDF_GAMEMOD, 0, movestepdown, FVAR_NONZERO, 1.15f, FVAR_MAX); // step-down modifier

GFVAR(IDF_GAMEMOD, 0, movesprint, FVAR_NONZERO, 1.5f, FVAR_MAX); // sprinting modifier
GFVAR(IDF_GAMEMOD, 0, movesprintrotvel, 0.0f, 8.0f, FVAR_MAX); // rotatation velocity inhibitor
GFVAR(IDF_GAMEMOD, 0, movesprintdecay, 0.0f, 3.0f, FVAR_MAX); // sprinting decay factor

GFVAR(IDF_GAMEMOD, 0, impulsejump, 0.0f, 1.5f, FVAR_MAX); // jump modifier
GFVAR(IDF_GAMEMOD, 0, impulsejumpredir, 0.0f, 0.25f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseboost, 0.0f, 2.0f, FVAR_MAX); // boost modifier
GFVAR(IDF_GAMEMOD, 0, impulseboostredir, 0.0f, 0.5f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseboostpitchmin, -89.9f, -89.9f, 89.9f); // boost pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulseboostpitchmax, -89.9f, 89.9f, 89.9f); // boost pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulseboostup, 0.0f, 3.0f, FVAR_MAX); // jump upward modifier
GFVAR(IDF_GAMEMOD, 0, impulseboostupredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsedash, 0.0f, 1.4f, FVAR_MAX); // dash modifier
GFVAR(IDF_GAMEMOD, 0, impulsedashback, 0.0f, 1.0f, FVAR_MAX); // dash modifier
GFVAR(IDF_GAMEMOD, 0, impulsedashredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsepound, 0.0f, 2.5f, FVAR_MAX); // pound modifier
GFVAR(IDF_GAMEMOD, 0, impulsepoundredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulselaunch, 0.0f, 1.0f, FVAR_MAX); // launch modifier
GFVAR(IDF_GAMEMOD, 0, impulselaunchredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulselaunchpitchmin, -89.9f, 22.5f, 89.9f); // launch pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulselaunchpitchmax, -89.9f, 89.9f, 89.9f); // launch pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulselaunchextra, 0.0f, 0.5f, FVAR_MAX); // launch extra modifier
GFVAR(IDF_GAMEMOD, 0, impulseslide, 0.0f, 1.1f, FVAR_MAX); // slide modifier
GFVAR(IDF_GAMEMOD, 0, impulseslideredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsemelee, 0.0f, 1.3f, FVAR_MAX); // melee modifier
GFVAR(IDF_GAMEMOD, 0, impulsemeleeredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsewallrun, 0.0f, 1.5f, FVAR_MAX); // wallrun modifier
GFVAR(IDF_GAMEMOD, 0, impulsewallrunredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsewallrunnorm, 0.0f, 0.5f, FVAR_MAX); // minimum wallrun surface z normal
GFVAR(IDF_GAMEMOD, 0, impulsewallrunstep, 0.0f, 4.0f, FVAR_MAX); // minimum step size in which to do climb
GFVAR(IDF_GAMEMOD, 0, impulsewallrunhead, 0.0f, 2.0f, FVAR_MAX); // minimum space above head for climbing
GFVAR(IDF_GAMEMOD, 0, impulsekick, 0.0f, 1.3f, FVAR_MAX); // wallrun kick modifier
GFVAR(IDF_GAMEMOD, 0, impulsekickredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsekickpitchmin, -89.9f, -89.9f, 89.9f); // kick pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulsekickpitchmax, -89.9f, 89.9f, 89.9f); // kick pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulseclimb, 0.0f, 1.5f, FVAR_MAX); // wallrun climb modifier
GFVAR(IDF_GAMEMOD, 0, impulseclimbredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsegrab, 0.0f, 2.0f, FVAR_MAX); // wallrun grab modifier
GFVAR(IDF_GAMEMOD, 0, impulsegrabredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsegrabplayer, 0.0f, 2.0f, FVAR_MAX); // wallrun grab player modifier
GFVAR(IDF_GAMEMOD, 0, impulsegrabplayerredir, 0.0f, 1.0f, FVAR_MAX); // how much of the old velocity is redirected into the new one

GVAR(IDF_GAMEMOD, 0, impulsevaultstyle, 0, 2, 3); // when to do vault transitions, bit: 1 = wallrun, 2 = climb
GVAR(IDF_GAMEMOD, 0, impulsevaultpitchclamp, 0, 1, 1); // clamp vault pitch to player pitch
GFVAR(IDF_GAMEMOD, 0, impulsevaultpitch, 0.0f, 0.0f, 89.9f); // z direction to funnel in when vaulting
GFVAR(IDF_GAMEMOD, 0, impulsevaultminspeed, 0.0f, 0.05f, FVAR_MAX); // minimum percentage of player speed

GFVAR(IDF_GAMEMOD, 0, impulsejumpmove, 0.0f, 1.0f, FVAR_MAX); // move during jump
GFVAR(IDF_GAMEMOD, 0, impulseboostmove, 0.0f, 0.5f, FVAR_MAX); // move during boosts
GFVAR(IDF_GAMEMOD, 0, impulsedashmove, 0.0f, 0.5f, FVAR_MAX); // move during dashes
GFVAR(IDF_GAMEMOD, 0, impulseslidemove, 0.0f, 0.5f, FVAR_MAX); // move during slides
GFVAR(IDF_GAMEMOD, 0, impulselaunchmove, 0.0f, 1.0f, FVAR_MAX); // move during launches
GFVAR(IDF_GAMEMOD, 0, impulsemeleemove, 0.0f, 1.0f, FVAR_MAX); // move during melee
GFVAR(IDF_GAMEMOD, 0, impulsekickmove, 0.0f, 1.0f, FVAR_MAX); // move during kicks
GFVAR(IDF_GAMEMOD, 0, impulsegrabmove, 0.0f, 1.0f, FVAR_MAX); // move during grabs
GFVAR(IDF_GAMEMOD, 0, impulsewallrunmove, 0.0f, 1.2f, FVAR_MAX); // move during wallrun moves
GFVAR(IDF_GAMEMOD, 0, impulseclimbmove, 0.0f, 1.2f, FVAR_MAX); // move during climb moves
GFVAR(IDF_GAMEMOD, 0, impulsevaultmove, 0.0f, 0.5f, FVAR_MAX); // move during vault moves
GFVAR(IDF_GAMEMOD, 0, impulsepoundmove, 0.0f, 0.5f, FVAR_MAX); // move during pounds

GFVAR(IDF_GAMEMOD, 0, impulsejumpgravity, 0.0f, 1.0f, FVAR_MAX); // gravity during jump
GFVAR(IDF_GAMEMOD, 0, impulseboostgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during boosts
GFVAR(IDF_GAMEMOD, 0, impulsedashgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during dashes
GFVAR(IDF_GAMEMOD, 0, impulseslidegravity, 0.0f, 0.5f, FVAR_MAX); // gravity during slides
GFVAR(IDF_GAMEMOD, 0, impulselaunchgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during launches
GFVAR(IDF_GAMEMOD, 0, impulsemeleegravity, 0.0f, 0.5f, FVAR_MAX); // gravity during melee
GFVAR(IDF_GAMEMOD, 0, impulsekickgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during kicks
GFVAR(IDF_GAMEMOD, 0, impulsegrabgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during grabs
GFVAR(IDF_GAMEMOD, 0, impulsewallrungravity, 0.0f, 0.5f, FVAR_MAX); // gravity during wallrun moves
GFVAR(IDF_GAMEMOD, 0, impulseclimbgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during climb moves
GFVAR(IDF_GAMEMOD, 0, impulsevaultgravity, 0.0f, 0.5f, FVAR_MAX); // gravity during vault moves
GFVAR(IDF_GAMEMOD, 0, impulsepoundgravity, 0.0f, 2.0f, FVAR_MAX); // gravity during pounds

GVAR(IDF_GAMEMOD, 0, impulsecount, 0, 6, VAR_MAX); // number of impulse actions per air transit
GVAR(IDF_GAMEMOD, 0, impulsecounttypes, 0, IM_T_COUNT, IM_T_ACTION); // what actions get counted
GVAR(IDF_GAMEMOD, 0, impulsepoundstyle, 0, 0, 1); // pound style: 0 = stop moving first, 1 = allow moving

GVAR(IDF_GAMEMOD, 0, impulseyawtime, 0, 250, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, impulseyawscale, 0.0f, 0.0f, 1.0f);
GVAR(IDF_GAMEMOD, 0, impulsepitchtime, 0, 250, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, impulsepitchscale, 0.0f, 0.0f, 1.0f);
GVAR(IDF_GAMEMOD, 0, impulserolltime, 0, 250, VAR_MAX);
GFVAR(IDF_GAMEMOD, 0, impulserollscale, 0.0f, 1.0f, 1.0f);

GVAR(IDF_GAMEMOD, 0, impulsetouchtypes, 0, IM_T_ISTOUCH, IM_T_ACTION); // what actions count as touch
GVAR(IDF_GAMEMOD, 0, impulsetouchmasks, 0, IM_T_NEEDTOUCH, IM_T_ACTION); // what actions need a touch
GVAR(IDF_GAMEMOD, 0, impulsecoastmask, 0, IM_T_COAST, IM_T_ACTION); // check actions for slide coast
GVAR(IDF_GAMEMOD, 0, impulseregenmask, 0, IM_T_REGEN, IM_T_ACTION); // check actions for delay to regen

GVAR(IDF_GAMEMOD, 0, impulsejumpmask, 0, IM_T_PUSH, IM_T_ACTION); // check actions for delay to jump
GVAR(IDF_GAMEMOD, 0, impulseboostmask, 0, IM_T_PUSH, IM_T_ACTION); // check actions for delay to boosts
GVAR(IDF_GAMEMOD, 0, impulsedashmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to dashes
GVAR(IDF_GAMEMOD, 0, impulseslidemask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to slides
GVAR(IDF_GAMEMOD, 0, impulselaunchmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to launches
GVAR(IDF_GAMEMOD, 0, impulsemeleemask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to melee
GVAR(IDF_GAMEMOD, 0, impulsekickmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to kicks
GVAR(IDF_GAMEMOD, 0, impulsegrabmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to grabs
GVAR(IDF_GAMEMOD, 0, impulsewallrunmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to wallrun moves
GVAR(IDF_GAMEMOD, 0, impulsevaultmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to vault moves
GVAR(IDF_GAMEMOD, 0, impulsepoundmask, 0, IM_T_ACTION, IM_T_ACTION); // check actions for delay to pounds

GVAR(IDF_GAMEMOD, 0, impulsejumpreset, 0, 0, IM_T_ACTION); // reset these upon jump
GVAR(IDF_GAMEMOD, 0, impulseboostreset, 0, 0, IM_T_ACTION); // reset these upon boosts
GVAR(IDF_GAMEMOD, 0, impulsedashreset, 0, 0, IM_T_ACTION); // reset these upon dashes
GVAR(IDF_GAMEMOD, 0, impulseslidereset, 0, 0, IM_T_ACTION); // reset these upon slides
GVAR(IDF_GAMEMOD, 0, impulselaunchreset, 0, 0, IM_T_ACTION); // reset these upon launches
GVAR(IDF_GAMEMOD, 0, impulsemeleereset, 0, 0, IM_T_ACTION); // reset these upon melee
GVAR(IDF_GAMEMOD, 0, impulsekickreset, 0, 0, IM_T_ACTION); // reset these upon kicks
GVAR(IDF_GAMEMOD, 0, impulsegrabreset, 0, 0, IM_T_ACTION); // reset these upon grabs
GVAR(IDF_GAMEMOD, 0, impulsewallrunreset, 0, 0, IM_T_ACTION); // reset these upon wallrun moves
GVAR(IDF_GAMEMOD, 0, impulsevaultreset, 0, 0, IM_T_ACTION); // reset these upon vault moves
GVAR(IDF_GAMEMOD, 0, impulsepoundreset, 0, 0, IM_T_ACTION); // reset these upon pounds
GVAR(IDF_GAMEMOD, 0, impulsepusherreset, 0, IM_T_ACTION, IM_T_ACTION); // reset these upon pusher
GVAR(IDF_GAMEMOD, 0, impulsecheckpointreset, 0, IM_T_ACTION, IM_T_ACTION); // reset these upon checkpoint

GVAR(IDF_GAMEMOD, 0, impulsejumpclear, 0, 0, IM_ALL); // reset these upon jump
GVAR(IDF_GAMEMOD, 0, impulseboostclear, 0, 0, IM_ALL); // reset these upon boost
GVAR(IDF_GAMEMOD, 0, impulsedashclear, 0, 0, IM_ALL); // reset these upon dash
GVAR(IDF_GAMEMOD, 0, impulseslideclear, 0, 0, IM_ALL); // reset these upon slide
GVAR(IDF_GAMEMOD, 0, impulselaunchclear, 0, 0, IM_ALL); // reset these upon launch
GVAR(IDF_GAMEMOD, 0, impulsemeleeclear, 0, 0, IM_ALL); // reset these upon melee
GVAR(IDF_GAMEMOD, 0, impulsekickclear, 0, 0, IM_ALL); // reset these upon kick
GVAR(IDF_GAMEMOD, 0, impulsegrabclear, 0, 0, IM_ALL); // reset these upon grab
GVAR(IDF_GAMEMOD, 0, impulsewallrunclear, 0, 0, IM_ALL); // reset these upon wallrun
GVAR(IDF_GAMEMOD, 0, impulsevaultclear, 0, 0, IM_ALL); // reset these upon vault
GVAR(IDF_GAMEMOD, 0, impulsepoundclear, 0, 0, IM_ALL); // reset these upon pound
GVAR(IDF_GAMEMOD, 0, impulsepusherclear, 0, 0, IM_ALL); // reset these upon pusher
GVAR(IDF_GAMEMOD, 0, impulsecheckpointclear, 0, IM_CHECKPOINT, IM_ALL); // reset these upon checkpoint

GVAR(IDF_GAMEMOD, 0, impulsejumpdelay, 0, 125, VAR_MAX); // minimum time after jump
GVAR(IDF_GAMEMOD, 0, impulseboostdelay, 0, 250, VAR_MAX); // minimum time after boosts
GVAR(IDF_GAMEMOD, 0, impulsedashdelay, 0, 750, VAR_MAX); // minimum time after dashes
GVAR(IDF_GAMEMOD, 0, impulseslidedelay, 0, 250, VAR_MAX); // minimum time after slides
GVAR(IDF_GAMEMOD, 0, impulselaunchdelay, 0, 250, VAR_MAX); // minimum time after launches
GVAR(IDF_GAMEMOD, 0, impulsemeleedelay, 0, 250, VAR_MAX); // minimum time after melee
GVAR(IDF_GAMEMOD, 0, impulsekickdelay, 0, 250, VAR_MAX); // minimum time after kicks
GVAR(IDF_GAMEMOD, 0, impulsegrabdelay, 0, 250, VAR_MAX); // minimum time after grabs
GVAR(IDF_GAMEMOD, 0, impulsewallrundelay, 0, 250, VAR_MAX); // minimum time after wallrun moves
GVAR(IDF_GAMEMOD, 0, impulsevaultdelay, 0, 250, VAR_MAX); // minimum time after vault moves
GVAR(IDF_GAMEMOD, 0, impulsepounddelay, 0, 500, VAR_MAX); // minimum time after pounds

GVAR(IDF_GAMEMOD, 0, impulsesliplen, 0, 125, VAR_MAX); // time before floor friction kicks back in
GVAR(IDF_GAMEMOD, 0, impulseslideinair, 0, 250, VAR_MAX); // minimum time in air for slide

GVAR(IDF_GAMEMOD, 0, impulsejumplen, 0, 125, VAR_MAX); // length of time for jump
GVAR(IDF_GAMEMOD, 0, impulseboostlen, 0, 250, VAR_MAX); // length of time for boosts
GVAR(IDF_GAMEMOD, 0, impulsedashlen, 0, 500, VAR_MAX); // length of time for dashes
GVAR(IDF_GAMEMOD, 0, impulseslidelen, 0, 1000, VAR_MAX); // length of time for slides
GVAR(IDF_GAMEMOD, 0, impulselaunchlen, 0, 500, VAR_MAX); // length of time for launches
GVAR(IDF_GAMEMOD, 0, impulsemeleelen, 0, 250, VAR_MAX); // length of time for melee
GVAR(IDF_GAMEMOD, 0, impulsekicklen, 0, 250, VAR_MAX); // length of time for kicks
GVAR(IDF_GAMEMOD, 0, impulsegrablen, 0, 250, VAR_MAX); // length of time for grabs
GVAR(IDF_GAMEMOD, 0, impulsewallrunlen, 0, 1000, VAR_MAX); // length of time for wallrun moves
GVAR(IDF_GAMEMOD, 0, impulsevaultlen, 0, 500, VAR_MAX); // length of time for vault moves
GVAR(IDF_GAMEMOD, 0, impulsepoundlen, 0, 500, VAR_MAX); // length of time for pounds

GVAR(IDF_GAMEMOD, 0, impulsedashtolerate, 0, 2, 2); // tolerate dashes for jumps, 1 = length, 2 = delay
GVAR(IDF_GAMEMOD, 0, impulsevaulttolerate, 0, 2, 2); // tolerate vaults for jumps, 1 = length, 2 = delay

GVAR(IDF_GAMEMOD, 0, impulsecostmeter, 0, 30000, VAR_MAX); // impulse cost meter
GVAR(IDF_GAMEMOD, 0, impulsecostcount, 0, 250, VAR_MAX); // cost of count regeneration

GVAR(IDF_GAMEMOD, 0, impulsecostjump, 0, 250, VAR_MAX); // cost of jump move
GVAR(IDF_GAMEMOD, 0, impulsecostboost, 0, 750, VAR_MAX); // cost of boost move
GVAR(IDF_GAMEMOD, 0, impulsecostwallrun, 0, 1000, VAR_MAX); // cost of wallrun move
GVAR(IDF_GAMEMOD, 0, impulsecostdash, 0, 1500, VAR_MAX); // cost of dash move
GVAR(IDF_GAMEMOD, 0, impulsecostvault, 0, 500, VAR_MAX); // cost of vault move
GVAR(IDF_GAMEMOD, 0, impulsecostslide, 0, 1000, VAR_MAX); // cost of slide move
GVAR(IDF_GAMEMOD, 0, impulsecostlaunch, 0, 1000, VAR_MAX); // cost of launch move
GVAR(IDF_GAMEMOD, 0, impulsecostpound, 0, 1000, VAR_MAX); // cost of pound move
GVAR(IDF_GAMEMOD, 0, impulsecostkick, 0, 750, VAR_MAX); // cost of kick move
GVAR(IDF_GAMEMOD, 0, impulsecostmelee, 0, 500, VAR_MAX); // cost of melee move
GVAR(IDF_GAMEMOD, 0, impulsecostclimb, 0, 1000, VAR_MAX); // cost of climb move
GVAR(IDF_GAMEMOD, 0, impulsecostgrab, 0, 500, VAR_MAX); // cost of of grab move
GVAR(IDF_GAMEMOD, 0, impulsecostgrabplayer, 0, 250, VAR_MAX); // cost of of grab move
GVAR(IDF_GAMEMOD, 0, impulsecostrelax, 0, IM_T_RELAX, IM_T_ACTION); // whether the cost of an impulse move is unimportant
GVAR(IDF_GAMEMOD, 0, impulsecostscale, 0, 0, 1); // whether the cost scales depending on the amount the impulse scales

GVAR(IDF_GAMEMOD, 0, impulseregendelay, 0, 250, VAR_MAX); // delay before impulse regens

GFVAR(IDF_GAMEMOD, 0, impulsecrouchregenmeter, 0, 3.0f, FVAR_MAX); // impulse regen crouch modifier
GFVAR(IDF_GAMEMOD, 0, impulserunregenmeter, 0, 0.75f, FVAR_MAX); // impulse regen running modifier
GFVAR(IDF_GAMEMOD, 0, impulsesprintregenmeter, 0, 0.5f, FVAR_MAX); // impulse regen sprinting modifier
GFVAR(IDF_GAMEMOD, 0, impulsemoveregenmeter, 0, 1.0f, FVAR_MAX); // impulse regen moving modifier
GFVAR(IDF_GAMEMOD, 0, impulseinairregenmeter, 0, 0.25f, FVAR_MAX); // impulse regen in-air modifier

GFVAR(IDF_GAMEMOD, 0, impulsejumpregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for jump
GFVAR(IDF_GAMEMOD, 0, impulseboostregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for boost
GFVAR(IDF_GAMEMOD, 0, impulsedashregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for dash
GFVAR(IDF_GAMEMOD, 0, impulseslideregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for slide
GFVAR(IDF_GAMEMOD, 0, impulselaunchregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for launch
GFVAR(IDF_GAMEMOD, 0, impulsemeleeregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for melee
GFVAR(IDF_GAMEMOD, 0, impulsekickregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for kick
GFVAR(IDF_GAMEMOD, 0, impulsegrabregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for grab
GFVAR(IDF_GAMEMOD, 0, impulsewallrunregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for wallrun
GFVAR(IDF_GAMEMOD, 0, impulsevaultregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for vault
GFVAR(IDF_GAMEMOD, 0, impulsepoundregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for pound
GFVAR(IDF_GAMEMOD, 0, impulsepusherregenmeter, 0, 0, FVAR_MAX); // impulse regen modifier for pusher

GFVAR(IDF_GAMEMOD, 0, impulsecrouchregencount, 0, 1.5f, FVAR_MAX); // impulse regen crouch modifier
GFVAR(IDF_GAMEMOD, 0, impulserunregencount, 0, 0.75f, FVAR_MAX); // impulse regen running modifier
GFVAR(IDF_GAMEMOD, 0, impulsesprintregencount, 0, 0.5f, FVAR_MAX); // impulse regen sprinting modifier
GFVAR(IDF_GAMEMOD, 0, impulsemoveregencount, 0, 1.0f, FVAR_MAX); // impulse regen moving modifier
GFVAR(IDF_GAMEMOD, 0, impulseinairregencount, 0, 0.25f, FVAR_MAX); // impulse regen in-air modifier

GFVAR(IDF_GAMEMOD, 0, impulsejumpregencount, 0, 0, FVAR_MAX); // impulse regen modifier for jump
GFVAR(IDF_GAMEMOD, 0, impulseboostregencount, 0, 0, FVAR_MAX); // impulse regen modifier for boost
GFVAR(IDF_GAMEMOD, 0, impulsedashregencount, 0, 0, FVAR_MAX); // impulse regen modifier for dash
GFVAR(IDF_GAMEMOD, 0, impulseslideregencount, 0, 0, FVAR_MAX); // impulse regen modifier for slide
GFVAR(IDF_GAMEMOD, 0, impulselaunchregencount, 0, 0, FVAR_MAX); // impulse regen modifier for launch
GFVAR(IDF_GAMEMOD, 0, impulsemeleeregencount, 0, 0, FVAR_MAX); // impulse regen modifier for melee
GFVAR(IDF_GAMEMOD, 0, impulsekickregencount, 0, 0, FVAR_MAX); // impulse regen modifier for kick
GFVAR(IDF_GAMEMOD, 0, impulsegrabregencount, 0, 0, FVAR_MAX); // impulse regen modifier for grab
GFVAR(IDF_GAMEMOD, 0, impulsewallrunregencount, 0, 0, FVAR_MAX); // impulse regen modifier for wallrun
GFVAR(IDF_GAMEMOD, 0, impulsevaultregencount, 0, 0, FVAR_MAX); // impulse regen modifier for vault
GFVAR(IDF_GAMEMOD, 0, impulsepoundregencount, 0, 0, FVAR_MAX); // impulse regen modifier for pound
GFVAR(IDF_GAMEMOD, 0, impulsepusherregencount, 0, 0, FVAR_MAX); // impulse regen modifier for pusher

GVAR(IDF_GAMEMOD, 0, quakedelta, 0, 100, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, quakefade, 0, 300, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, quakewobble, 1, 20, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, quakelimit, 0, 200, VAR_MAX);

GFVAR(IDF_GAMEMOD, 0, damagescale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, radialscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, pushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, hitpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, deadpushscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, wavepushscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, stunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, hitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, deadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, wavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, velscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, hitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, deadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, wavevelscale, 0, 1, FVAR_MAX);

GVAR(IDF_GAMEMOD, 0, gladiatorregen, 0, 0, 1); // allow regen in gladiator

GFVAR(IDF_GAMEMOD, 0, gladiatorradialscale, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorhitpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatordeadpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorwavepushscale, 0, 5, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, gladiatorstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorhitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatordeadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorwavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, gladiatorvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorhitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatordeadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorwavevelscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, gladiatorextrapushscale, 0, 1, FVAR_MAX);  // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, 0, gladiatorextrahitpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextradeadpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextrawavepushscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, gladiatorextrastunscale, 0, 1, FVAR_MAX); // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, 0, gladiatorextrahitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextradeadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextrawavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, gladiatorextravelscale, 0, 1, FVAR_MAX); // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, 0, gladiatorextrahitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextradeadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gladiatorextrawavevelscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, darknessenv, 0, 0.15f, 1);
GFVAR(IDF_GAMEMOD, 0, darknessglow, 0, 0.25f, 1);
GFVAR(IDF_GAMEMOD, 0, darknesshalo, 0, 0.75f, 1);
GFVAR(IDF_GAMEMOD, 0, darknessui, 0, 0.15f, 1);
GFVAR(IDF_GAMEMOD, 0, darknesssun, 0, 0, 1);
GFVAR(IDF_GAMEMOD, 0, darknesspart, 0, 0.5f, 1);

GFVAR(IDF_GAMEMOD, 0, darknessflashcolourmin, 0, 0.5f, 1);
GFVAR(IDF_GAMEMOD, 0, darknessflashcolourmax, 0, 1, 1);
GFVAR(IDF_GAMEMOD, 0, darknessflashradiusmin, 0, 512, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, darknessflashradiusmax, 0, 1024, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, darknessflashlevel, 0, 1, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, darknessflashspot, 0, 30, 89);

GFVAR(IDF_GAMEMOD, 0, radiallimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, pushlimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, stunlimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, vellimited, 0, 0.75f, FVAR_MAX);

GFVAR(IDF_GAMEMOD, 0, kickpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, kickpushcrouch, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, kickpushsway, 0, 0.0125f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, kickpushzoom, 0, 0.125f, FVAR_MAX);

GVAR(IDF_GAMEMOD, 0, fragbonus, 0, 3, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, enemybonus, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, lasthitbonus, 0, 3, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, teamkillpenalty, 0, 2, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, firstbloodpoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, headshotpoints, 0, 1, VAR_MAX);

GVAR(IDF_GAMEMOD, 0, assistkilldelay, 0, 5000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, lasthitdelay, 0, 10000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, multikilldelay, 0, 5000, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, multikillpoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, multikillbonus, 0, 0, 1); // if bonus is on, then points are multiplied by the current kill mutliplier (x2, x3, x4)
GVAR(IDF_GAMEMOD, 0, spreecount, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, spreepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, spreeprize, -1, -1, W_PRIZES); // -1 = random, 0 = none, 1 = grenade, 2 = mine, 3 = rocket, 4 = minigun, 5 = jetsaw, 6 = eclipse
GVAR(IDF_GAMEMOD, 0, spreemaxprize, -1, -1, W_PRIZES); // -1 = random, 0 = none, 1 = grenade, 2 = mine, 3 = rocket, 4 = minigun, 5 = jetsaw, 6 = eclipse
GVAR(IDF_GAMEMOD, 0, spreebreaker, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, spreebreakprize, -1, -1, W_PRIZES); // -1 = random, 0 = none, 1 = grenade, 2 = mine, 3 = rocket, 4 = minigun, 5 = jetsaw, 6 = eclipse
GVAR(IDF_GAMEMOD, 0, dominatecount, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, dominatepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, revengepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, revengeprize, -1, -1, W_PRIZES); // -1 = random, 0 = none, 1 = grenade, 2 = mine, 3 = rocket, 4 = minigun, 5 = jetsaw, 6 = eclipse

GSVAR(0, 0, obitspawn, "tried to spawn inside solid matter");
GSVAR(0, 0, obitspectator, "gave up their corporeal form");
GSVAR(0, 0, obitdrowned, "drowned");
GSVAR(0, 0, obitmelted, "melted into a ball of fire");
GSVAR(0, 0, obitchoked, "choked to death");
GSVAR(0, 0, obitdeathmat, "met their end");
GSVAR(0, 0, obithurtmat, "stayed under too long");
GSVAR(0, 0, obitlost, "fell to their death");
GSVAR(0, 0, obitwrongcp, "hit the wrong checkpoint");
GSVAR(0, 0, obitburn, "immolated");
GSVAR(0, 0, obitburnself, "burned up");
GSVAR(0, 0, obitbleed, "fatally wounded");
GSVAR(0, 0, obitbleedself, "bled out");
GSVAR(0, 0, obitshock, "shocked");
GSVAR(0, 0, obitshockself, "twitched to death");
GSVAR(0, 0, obitcorrode, "melted");
GSVAR(0, 0, obitcorrodeself, "self-oxidized");
GSVAR(0, 0, obitobliterated, "obliterated");
GSVAR(0, 0, obitheadless, "decapitated");
GSVAR(0, 0, obitsuicide, "suicided");
GSVAR(0, 0, obitkilled, "killed");
GSVAR(0, 0, obitsplat, "got splatted");
GSVAR(0, 0, obitsquish, "was crushed");
GSVAR(0, 0, obitjanitor, "disposed of their trash");
