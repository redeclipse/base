GVAR(IDF_WORLD, 0, numplayers, 0, 4, MAXCLIENTS); // 0 = determine from number of spawns
GVAR(IDF_WORLD, 0, maxplayers, 0, 0, MAXCLIENTS); // 0 = numplayers*3
GVAR(IDF_WORLD, 0, mapbalance, 0, 0, 3); // switches teams for asymmetrical maps, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced

GVAR(IDF_WORLD, 0, hurtdelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtdamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtresidual, 0, 0, W_R_ALL);

GVAR(IDF_WORLD, 0, hurtburntime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtburndelay, 1, 1000, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtburndamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtbleedtime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtbleeddelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtbleeddamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtshocktime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtshockdelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtshockdamage, 0, 20, VAR_MAX);
GVAR(IDF_WORLD, 0, hurtshockstun, 0, W_N_ST, W_N_ALL);
GFVAR(IDF_WORLD, 0, hurtshockstunscale, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_WORLD, 0, hurtshockstunfall, 0, 0.01f, FVAR_MAX);
GVAR(IDF_WORLD, 0, hurtshockstuntime, 0, 500, VAR_MAX);

GVAR(IDF_WORLD, 0, lavaburntime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, 0, lavaburndelay, 1, 1000, VAR_MAX);

GFVAR(IDF_WORLD, 0, gravity, 0, 50.f, 1000); // gravity
GFVAR(IDF_GAMEMOD, 0, gravityscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravitycrouch, 0, 1.125f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravityjump, 0, 0.85f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravityjumpcrouch, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravityfall, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravityfallcrouch, 0, 1.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, gravitycutoff, FVAR_MIN, 0, FVAR_MAX);

GFVAR(IDF_WORLD, 0, floorcoast, 0, 5.f, 1000);
GFVAR(IDF_GAMEMOD, 0, floorcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, 0, aircoast, 0, 25.f, 1000);
GFVAR(IDF_GAMEMOD, 0, aircoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, 0, slidecoast, 0, 40.f, 1000);
GFVAR(IDF_GAMEMOD, 0, slidecoastscale, 0, 1, FVAR_MAX);

GFVAR(IDF_WORLD, 0, liquidspeed, 0, 0.85f, 1);
GFVAR(IDF_GAMEMOD, 0, liquidspeedscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, 0, liquidcoast, 0, 10.f, 1000);
GFVAR(IDF_GAMEMOD, 0, liquidcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, 0, liquidsubmerge, 0, 0.75f, 1);
GFVAR(IDF_GAMEMOD, 0, liquidsubmergescale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, 0, liquidextinguish, 0, 0.25f, 1);
GFVAR(IDF_GAMEMOD, 0, liquidextinguishscale, 0, 1, FVAR_MAX);

GVARF(IDF_GAMEMOD, 0, forcemapvariant, 0, 0, MPV_MAX-1, if(sv_forcemapvariant) server::changemapvariant(sv_forcemapvariant), if(forcemapvariant) changemapvariant(forcemapvariant));

GVAR(0, PRIV_ADMINISTRATOR, serverstats, 0, 1, 1);
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

GSVAR(0, PRIV_ADMINISTRATOR, allowmaps, "untitled");

GSVAR(0, PRIV_ADMINISTRATOR, mainmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, capturemaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, defendmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, kingmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, bombermaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, holdmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, racemaps, "untitled");

GSVAR(0, PRIV_ADMINISTRATOR, duelmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, gladiatormaps, "untitled");

GSVAR(0, PRIV_ADMINISTRATOR, smallmaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, mediummaps, "untitled");
GSVAR(0, PRIV_ADMINISTRATOR, largemaps, "untitled");

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
GVAR(0, PRIV_MODERATOR, waitforplayertime, 0, 10000, VAR_MAX); // wait at most this long for players to meet waitforplayers criteria
GVAR(0, PRIV_MODERATOR, waitforplayerload, 0, 20000, VAR_MAX); // wait this long for players to load the map to schedule map requests
GVAR(0, PRIV_MODERATOR, waitforplayermaps, 0, 30000, VAR_MAX); // wait this long for sendmap and getmap requests
GVAR(0, PRIV_MODERATOR, waitforplayerinfo, 0, 10000, VAR_MAX); // wait at least this long for players to send info

namespace server
{
    extern void resetgamevars(bool all);
    extern void savegamevars();
}
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
GVAR(IDF_GAMEMOD, 0, regentime, 0, 2000, VAR_MAX); // regen this often when regenerating normally
GVAR(IDF_GAMEMOD, 0, regenhealth, 0, 100, VAR_MAX); // regen this amount each regen
GVAR(IDF_GAMEMOD, 0, regendecay, 0, 50, VAR_MAX); // if over maxhealth, decay this amount each regen

GVAR(IDF_GAMEMOD, 0, noweapburn, 0, 1, 1);
GVAR(IDF_GAMEMOD, 0, noweapbleed, 0, 1, 1);
GVAR(IDF_GAMEMOD, 0, noweapshock, 0, 1, 1);

GVAR(IDF_GAMEMOD, 0, kamikaze, 0, 1, 3); // 0 = never, 1 = holding grenade, 2 = have grenade, 3 = always
GVAR(IDF_GAMEMOD, 0, itemspawntime, 1, 15000, VAR_MAX); // when items respawn
GVAR(IDF_GAMEMOD, 0, itemspawndelay, 0, 0, VAR_MAX); // after map start items first spawn
GVAR(IDF_GAMEMOD, 0, itemspawnstyle, 0, 0, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both
GVAR(IDF_GAMEMOD, 0, itemcollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, 0, itemextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, 0, iteminteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, 0, itemelasticity, FVAR_MIN, 0.4f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrelativity, FVAR_MIN, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemliquidcoast, 0, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemweight, FVAR_MIN, 150, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemspeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemspeedmax, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrepulsion, 0, 16, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, itemrepelspeed, 0, 35, FVAR_MAX);

//  dm          duel        survivor    gladiator   capture     defend      defendking  bomber      bomberhold  race        racelapped  racegauntlet
MMVAR(IDF_GAMEMOD, 0, timelimit, 0, VAR_MAX,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
MMVAR(IDF_GAMEMOD, 0, overtimeallow, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
MMVAR(IDF_GAMEMOD, 0, overtimelimit, 0, VAR_MAX,
    5,          2,          3,          2,          5,          5,          5,          5,          5,          5,          5,          5
);
#ifdef CPP_GAME_SERVER
SVAR(0, limitidxname, "duel survivor gladiator capture defend defendking bomber bomberhold race racetimed racegauntlet");
#endif

GVAR(IDF_GAMEMOD, 0, intermlimit, 0, 10000, VAR_MAX); // .. before vote menu comes up
GVAR(IDF_GAMEMOD, 0, votelimit, 0, 40000, VAR_MAX); // .. before vote passes by default

GVAR(IDF_GAMEMOD, 0, duelrandom, 0, 1, 1); // randomise queue before first round
GVAR(IDF_GAMEMOD, 0, duelreset, 0, 1, 1); // reset winner in duel
GVAR(IDF_GAMEMOD, 0, duelclear, 0, 1, 1); // clear items in duel
GVAR(IDF_GAMEMOD, 0, duelregen, 0, 1, 1); // allow regen in duel
GVAR(IDF_GAMEMOD, 0, dueldelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, 0, duelcooloff, 0, 3000, VAR_MAX); // cool off period before duel goes to next round
GVAR(IDF_GAMEMOD, 0, duelcycle, 0, 2, 3); // determines if players are force-cycled after a certain number of wins (bit: 0 = off, 1 = non-team games, 2 = team games)
GVAR(IDF_GAMEMOD, 0, duelcycles, 0, 2, VAR_MAX); // maximum wins in a row before force-cycling (0 = num team/total players)
GVAR(IDF_GAMEMOD, 0, duelaffinity, 0, 1, 2); // 0 = off, 1 = on enter can respawn next iter, 2 = on enter can respawn immediately
GVAR(IDF_GAMEMOD, 0, duelbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(IDF_GAMEMOD, 0, duelovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, 0, duelmaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for duel. 0 = any number of players

GVAR(IDF_GAMEMOD, 0, survivorreset, 0, 1, 1); // reset winners in survivor
GVAR(IDF_GAMEMOD, 0, survivorclear, 0, 1, 1); // clear items in survivor
GVAR(IDF_GAMEMOD, 0, survivorregen, 0, 1, 1); // allow regen in survivor
GVAR(IDF_GAMEMOD, 0, survivordelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, 0, survivorcooloff, 0, 3000, VAR_MAX); // cool off period before survivor goes to next round
GVAR(IDF_GAMEMOD, 0, survivoraffinity, 0, 0, 1); // 0 = off, 1 = on enter can spawn immediately
GVAR(IDF_GAMEMOD, 0, survivorbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(IDF_GAMEMOD, 0, survivorovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, 0, survivormaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for survivor. 0 = any number of players

GVAR(IDF_GAMEMOD, 0, pointlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, 0, fraglimit, 0, 0, VAR_MAX); // finish when score is this or more in oldschool
GVAR(IDF_GAMEMOD, 0, racelimit, 0, 0, VAR_MAX); // finish when lap count is this or more
GVAR(IDF_GAMEMOD, 0, teampersist, 0, 1, 2); // 0 = off, 1 = only attempt, 2 = forced
GVAR(IDF_GAMEMOD, 0, damageself, 0, 1, 1); // 0 = off, 1 = either hurt self or use damageteam rules
GFVAR(IDF_GAMEMOD, 0, damageselfscale, 0, 1, FVAR_MAX); // 0 = off, anything else = scale for damage
GFVAR(IDF_GAMEMOD, 0, damageteamscale, 0, 1, FVAR_MAX); // 0 = off, anything else = scale for damage

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
GFVAR(IDF_GAMEMOD, 0, teambalanceavgposlocalweight, 0, 0.33, 1.0); // weight of local statistics when balancing players by average scoreboard position
GVAR(IDF_GAMEMOD, 0, teambalanceavgposdelay, 0, 5000, VAR_MAX); // delay betweel calculation of avgpos statistics

GVAR(IDF_GAMEMOD, 0, racegauntletwinner, 0, 1, 1); // declare the winner when the final team exceeds best score

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
GFVAR(IDF_GAMEMOD, 0, captureliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, captureweight, FVAR_MIN, 400, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturespeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturespeedmax, 0, 100, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturerepulsion, 0, 16, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturerepelspeed, 0, 35, FVAR_MAX);
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
GFVAR(IDF_GAMEMOD, 0, capturecarryjumpspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryjumpspeedeach, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryimpulsespeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryimpulsespeedeach, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryspeedeach, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryweight, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, capturecarryweighteach, FVAR_MIN, 5, FVAR_MAX);

GVAR(IDF_GAMEMOD, 0, defendlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, 0, defendpoints, 0, 1, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, 0, defendinterval, 0, 50, VAR_MAX);
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
GVAR(IDF_GAMEMOD, 0, bomberholdtime, 0, 15000, VAR_MAX);
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
GFVAR(IDF_GAMEMOD, 0, bomberliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bomberweight, FVAR_MIN, 350, FVAR_MAX);
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
GFVAR(IDF_GAMEMOD, 0, bombercarryjumpspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryimpulsespeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryspeed, FVAR_MIN, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, 0, bombercarryweight, FVAR_MIN, 5, FVAR_MAX);

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
GVAR(IDF_GAMEMOD, 0, botskillmin, 1, 75, 101);
GVAR(IDF_GAMEMOD, 0, botskillmax, 1, 85, 101);
GFVAR(IDF_GAMEMOD, 0, botskillfrags, -100, -1, 100);
GFVAR(IDF_GAMEMOD, 0, botskilldeaths, -100, 1, 100);
GVAR(IDF_GAMEMOD, 0, botlimit, 0, 32, MAXAI);
GVAR(IDF_GAMEMOD, 0, botoffset, VAR_MIN, 0, VAR_MAX);
GSVAR(0, PRIV_MODERATOR, botmalenames, "");
GSVAR(0, PRIV_MODERATOR, botfemalenames, "");
GSVAR(0, PRIV_MODERATOR, botmalevanities, "");
GSVAR(0, PRIV_MODERATOR, botfemalevanities, "");
GVAR(IDF_GAMEMOD, 0, botcolourseed, 0, 15, 15); // random bot things will be determined by colour as seed, bitwise: 0 = off, 1 = skill, 2 = name, 4 = model, 8 = loadout
GVAR(IDF_GAMEMOD, 0, botrandomcase, 0, 2, VAR_MAX); // bots will randomise the first letter of their name if rnd(botrandomcase) > 0
GVAR(0, PRIV_ADMINISTRATOR, botoverridelock, PRIV_NONE, PRIV_ADMINISTRATOR, PRIV_MAX);
GFVAR(IDF_GAMEMOD, 0, coopbalance, FVAR_NONZERO, 1.5f, FVAR_MAX);
GVAR(IDF_GAMEMOD, 0, coopskillmin, 1, 80, 101);
GVAR(IDF_GAMEMOD, 0, coopskillmax, 1, 90, 101);
GFVAR(IDF_GAMEMOD, 0, coopskillfrags, -100, 0, 100);
GFVAR(IDF_GAMEMOD, 0, coopskilldeaths, -100, 1, 100);
GVAR(IDF_GAMEMOD, 0, enemybalance, 1, 1, 3);
GVAR(IDF_GAMEMOD, 0, enemyskillmin, 1, 55, 101);
GVAR(IDF_GAMEMOD, 0, enemyskillmax, 1, 75, 101);
GVAR(IDF_GAMEMOD, 0, enemylimit, 0, 32, MAXAI);
GVAR(IDF_GAMEMOD, 0, enemyspawntime, 1, 60000, VAR_MAX); // when enemies respawn
GVAR(IDF_GAMEMOD, 0, enemyspawndelay, 0, 1000, VAR_MAX); // after map start enemies first spawn
GVAR(IDF_GAMEMOD, 0, enemyspawnstyle, 0, 1, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both

GFVAR(IDF_GAMEMOD, 0, movespeed, FVAR_NONZERO, 1.f, FVAR_MAX); // speed
GFVAR(IDF_GAMEMOD, 0, moveslow, 0, 0.5f, FVAR_MAX); // threshold for running
GFVAR(IDF_GAMEMOD, 0, movecrawl, 0, 0.6f, FVAR_MAX); // crawl modifier
GFVAR(IDF_GAMEMOD, 0, moverun, FVAR_NONZERO, 1.3f, FVAR_MAX); // running modifier
GFVAR(IDF_GAMEMOD, 0, movestraight, FVAR_NONZERO, 1.2f, FVAR_MAX); // non-strafe modifier
GFVAR(IDF_GAMEMOD, 0, movestrafe, FVAR_NONZERO, 1, FVAR_MAX); // strafe modifier
GFVAR(IDF_GAMEMOD, 0, moveinair, FVAR_NONZERO, 0.75f, FVAR_MAX); // in-air modifier
GFVAR(IDF_GAMEMOD, 0, movestepup, FVAR_NONZERO, 0.95f, FVAR_MAX); // step-up modifier
GFVAR(IDF_GAMEMOD, 0, movestepdown, FVAR_NONZERO, 1.15f, FVAR_MAX); // step-down modifier

GFVAR(IDF_GAMEMOD, 0, impulseboost, 0, 1, FVAR_MAX); // boost modifier
GFVAR(IDF_GAMEMOD, 0, impulseboostredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseboostpitchmin, -89.9f, -89.9f, 89.9f); // boost pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulseboostpitchmax, -89.9f, 89.9f, 89.9f); // boost pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulsepound, 0, 1.1f, FVAR_MAX); // pound modifier
GFVAR(IDF_GAMEMOD, 0, impulsepoundredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulselaunch, 0, 0.9f, FVAR_MAX); // launch modifier
GFVAR(IDF_GAMEMOD, 0, impulselaunchredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulselaunchpitchmin, -89.9f, 15.f, 89.9f); // launch pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulselaunchpitchmax, -89.9f, 89.9f, 89.9f); // launch pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulseslide, 0, 0.5f, FVAR_MAX); // slide modifier
GFVAR(IDF_GAMEMOD, 0, impulseslideredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsejump, 0, 1.1f, FVAR_MAX); // jump upward modifier
GFVAR(IDF_GAMEMOD, 0, impulsejumpredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsemelee, 0, 0.75f, FVAR_MAX); // melee modifier
GFVAR(IDF_GAMEMOD, 0, impulsemeleeredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseparkour, 0, 1.1f, FVAR_MAX); // parkour modifier
GFVAR(IDF_GAMEMOD, 0, impulseparkourredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseparkournorm, 0, 0.5f, FVAR_MAX); // minimum parkour surface z normal
GFVAR(IDF_GAMEMOD, 0, impulsekick, 0, 1.5f, FVAR_MAX); // parkour kick modifier
GFVAR(IDF_GAMEMOD, 0, impulsekickredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsekickpitchmin, -89.9f, -89.9f, 89.9f); // kick pitch minimum
GFVAR(IDF_GAMEMOD, 0, impulsekickpitchmax, -89.9f, 89.9f, 89.9f); // kick pitch maximum
GFVAR(IDF_GAMEMOD, 0, impulseclimb, 0, 1, FVAR_MAX); // parkour climb modifier
GFVAR(IDF_GAMEMOD, 0, impulseclimbredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulseclimbendmin, 0, 5, 89.9f); // parkour climb end minimum pitch
GFVAR(IDF_GAMEMOD, 0, impulseclimbvaultmin, FVAR_NONZERO, 0.25f, FVAR_MAX); // minimum percentage of height for vault
GFVAR(IDF_GAMEMOD, 0, impulseclimbvaultmax, FVAR_NONZERO, 1.6f, FVAR_MAX); // maximum percentage of height for vault
GFVAR(IDF_GAMEMOD, 0, impulsegrab, 0, 1.8f, FVAR_MAX); // parkour grab modifier
GFVAR(IDF_GAMEMOD, 0, impulsegrabredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, 0, impulsegrabplayer, 0, 2, FVAR_MAX); // parkour grab player modifier
GFVAR(IDF_GAMEMOD, 0, impulsegrabplayerredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one

GVAR(IDF_GAMEMOD, 0, impulsestyle, 0, 1, 3); // impulse style; 0 = off, 1 = touch and count, 2 = count only, 3 = freestyle
GVAR(IDF_GAMEMOD, 0, impulseclimbstyle, 0, 1, 1); // climb style; 0 = normal, 1 = with vault from ground
GVAR(IDF_GAMEMOD, 0, impulseclimbendstyle, 0, 2, 2); // climb end style: 0 = free move, 1 = redirect forward, 2 = redirect pitch
GVAR(IDF_GAMEMOD, 0, impulsepoundstyle, 0, 0, 1); // pound style: 0 = stop moving first, 1 = allow moving

GVAR(IDF_GAMEMOD, 0, impulsecount, 0, 6, VAR_MAX); // number of impulse actions per air transit
GVAR(IDF_GAMEMOD, 0, impulsesliplen, 0, 750, VAR_MAX); // time before floor friction kicks back in
GVAR(IDF_GAMEMOD, 0, impulseslidelen, 0, 1000, VAR_MAX); // time before slides end
GVAR(IDF_GAMEMOD, 0, impulseparkourlen, 0, 1250, VAR_MAX); // length of time a run along a wall can last

GVAR(IDF_GAMEMOD, 0, impulsejumpdelay, 0, 200, VAR_MAX); // minimum time after jump for boost
GVAR(IDF_GAMEMOD, 0, impulseboostdelay, 0, 300, VAR_MAX); // minimum time between boosts
GVAR(IDF_GAMEMOD, 0, impulsepounddelay, 0, 500, VAR_MAX); // minimum time between pounds
GVAR(IDF_GAMEMOD, 0, impulseslidedelay, 0, 1000, VAR_MAX); // minimum time between slides
GVAR(IDF_GAMEMOD, 0, impulseslideinair, 0, 100, VAR_MAX); // minimum time in air for slide
GVAR(IDF_GAMEMOD, 0, impulseparkourdelay, 0, 500, VAR_MAX); // minimum time between parkour moves

GVAR(IDF_GAMEMOD, 0, impulsemeter, 0, 0, VAR_MAX); // impulse limiter
GVAR(IDF_GAMEMOD, 0, impulsecost, 1, 5000, VAR_MAX); // cost of impulse move
GFVAR(IDF_GAMEMOD, 0, impulsecostparkour, 0, 1, FVAR_MAX); // scale cost parkour move
GFVAR(IDF_GAMEMOD, 0, impulsecostboost, 0, 1, FVAR_MAX); // scale cost boost move
GFVAR(IDF_GAMEMOD, 0, impulsecostpound, 0, 1, FVAR_MAX); // scale cost pound move
GFVAR(IDF_GAMEMOD, 0, impulsecostkick, 0, 1, FVAR_MAX); // scale cost kick move
GFVAR(IDF_GAMEMOD, 0, impulsecostmelee, 0, 1, FVAR_MAX); // scale cost melee move
GFVAR(IDF_GAMEMOD, 0, impulsecostclimb, 0, 1, FVAR_MAX); // scale cost climb move
GFVAR(IDF_GAMEMOD, 0, impulsecostgrab, 0, 0.5f, FVAR_MAX); // scale cost of grab move
GFVAR(IDF_GAMEMOD, 0, impulsecostgrabplayer, 0, 0.25f, FVAR_MAX); // scale cost of grab move
GVAR(IDF_GAMEMOD, 0, impulsecostrelax, 0, A_A_IMRELAX, A_A_IMOFFSET); // whether the cost of an impulse move is unimportant
GVAR(IDF_GAMEMOD, 0, impulsecostscale, 0, 0, 1); // whether the cost scales depending on the amount the impulse scales
GFVAR(IDF_GAMEMOD, 0, impulseregen, 0, 5, FVAR_MAX); // impulse regen multiplier
GFVAR(IDF_GAMEMOD, 0, impulseregencrouch, 0, 2.5f, FVAR_MAX); // impulse regen crouch modifier
GFVAR(IDF_GAMEMOD, 0, impulseregenrun, 0, 0.75f, FVAR_MAX); // impulse regen running modifier
GFVAR(IDF_GAMEMOD, 0, impulseregenmove, 0, 1, FVAR_MAX); // impulse regen moving modifier
GFVAR(IDF_GAMEMOD, 0, impulseregeninair, 0, 0.75f, FVAR_MAX); // impulse regen in-air modifier
GFVAR(IDF_GAMEMOD, 0, impulseregenslide, 0, 0, FVAR_MAX); // impulse regen sliding modifier
GVAR(IDF_GAMEMOD, 0, impulseregendelay, 0, 350, VAR_MAX); // delay before impulse regens

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
GVAR(IDF_GAMEMOD, 0, spreebreaker, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, dominatecount, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, dominatepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, 0, revengepoints, 0, 1, VAR_MAX);

GSVAR(0, 0, obitdestroyed, "was destroyed");
GSVAR(0, 0, obitdied, "died");
GSVAR(0, 0, obitfragged, "fragged");
GSVAR(0, 0, obitspawn, "tried to spawn inside solid matter");
GSVAR(0, 0, obitspectator, "gave up their corporeal form");
GSVAR(0, 0, obitdrowned, "drowned");
GSVAR(0, 0, obitmelted, "melted into a ball of fire");
GSVAR(0, 0, obitdeathmat, "met their end");
GSVAR(0, 0, obithurtmat, "stayed under too long");
GSVAR(0, 0, obitlost, "fell to their death");
GSVAR(0, 0, obitburn, "set ablaze");
GSVAR(0, 0, obitburnself, "burned up");
GSVAR(0, 0, obitbleed, "fatally wounded");
GSVAR(0, 0, obitbleedself, "bled out");
GSVAR(0, 0, obitshock, "given a terminal dose of shock therapy");
GSVAR(0, 0, obitshockself, "twitched to death");
GSVAR(0, 0, obitobliterated, "was obliterated");
GSVAR(0, 0, obitheadless, "had their head caved in");
GSVAR(0, 0, obitsuicide, "suicided");
GSVAR(0, 0, obitkilled, "killed");
