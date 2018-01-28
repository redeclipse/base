GVAR(IDF_WORLD, numplayers, 0, 4, MAXCLIENTS); // 0 = determine from number of spawns
GVAR(IDF_WORLD, maxplayers, 0, 0, MAXCLIENTS); // 0 = numplayers*3
GVAR(IDF_WORLD, mapbalance, 0, 0, 3); // switches teams for asymmetrical maps, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced

GVAR(IDF_WORLD, hurtdelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, hurtdamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, hurtresidual, 0, 0, W_R_ALL);

GVAR(IDF_WORLD, hurtburntime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, hurtburndelay, 1, 1000, VAR_MAX);
GVAR(IDF_WORLD, hurtburndamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, hurtbleedtime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, hurtbleeddelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, hurtbleeddamage, 0, 30, VAR_MAX);
GVAR(IDF_WORLD, hurtshocktime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, hurtshockdelay, 0, 1000, VAR_MAX);
GVAR(IDF_WORLD, hurtshockdamage, 0, 20, VAR_MAX);
GVAR(IDF_WORLD, hurtshockstun, 0, W_N_ST, W_N_ALL);
GFVAR(IDF_WORLD, hurtshockstunscale, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_WORLD, hurtshockstunfall, 0, 0.01f, FVAR_MAX);
GVAR(IDF_WORLD, hurtshockstuntime, 0, 500, VAR_MAX);

GVAR(IDF_WORLD, lavaburntime, 0, 5500, VAR_MAX);
GVAR(IDF_WORLD, lavaburndelay, 1, 1000, VAR_MAX);

GFVAR(IDF_WORLD, gravity, 0, 50.f, 1000); // gravity
GFVAR(IDF_GAMEMOD, gravityscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gravityjump, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gravityjumpcrouch, 0, 1.1f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gravityfall, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gravityfallcrouch, 0, 1.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gravitycutoff, FVAR_MIN, 0, FVAR_MAX);

GFVAR(IDF_WORLD, floorcoast, 0, 5.f, 1000);
GFVAR(IDF_GAMEMOD, floorcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, aircoast, 0, 25.f, 1000);
GFVAR(IDF_GAMEMOD, aircoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, slidecoast, 0, 40.f, 1000);
GFVAR(IDF_GAMEMOD, slidecoastscale, 0, 1, FVAR_MAX);

GFVAR(IDF_WORLD, liquidspeed, 0, 0.85f, 1);
GFVAR(IDF_GAMEMOD, liquidspeedscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidcoast, 0, 10.f, 1000);
GFVAR(IDF_GAMEMOD, liquidcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidsubmerge, 0, 0.75f, 1);
GFVAR(IDF_GAMEMOD, liquidsubmergescale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidextinguish, 0, 0.25f, 1);
GFVAR(IDF_GAMEMOD, liquidextinguishscale, 0, 1, FVAR_MAX);

GVARF(IDF_GAMEMOD, forcemapvariant, 0, 0, MPV_MAX-1, if(sv_forcemapvariant) server::changemapvariant(sv_forcemapvariant), if(forcemapvariant) changemapvariant(forcemapvariant));

GVAR(IDF_ADMIN, serverstats, 0, 1, 1);
GVAR(IDF_ADMIN, serverclients, 1, 16, MAXCLIENTS);
GVAR(IDF_ADMIN, serverspectators, 0, 0, MAXCLIENTS); // 0 = copy serverclients, 1+ = spectator slots
GVARF(IDF_ADMIN, serverdupclients, 0, 0, MAXCLIENTS, limitdupclients(), );
GVAR(IDF_ADMIN, serveropen, 0, 3, 3);
GSVAR(IDF_ADMIN, serverdesc, "");
GSVAR(IDF_ADMIN, servermotd, "");

GVAR(IDF_ADMIN, autoadmin, 0, 0, 1);
GVAR(IDF_ADMIN, adminlock, 0, PRIV_ADMINISTRATOR, PRIV_MAX);
GVAR(IDF_ADMIN, moderatorlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, varslock, 0, PRIV_SUPPORTER, PRIV_MAX);

GVAR(IDF_ADMIN, queryinterval, 0, 5000, VAR_MAX); // rebuild client list for server queries this often
GVAR(IDF_ADMIN, masterinterval, 300000, 300000, VAR_MAX); // keep connection alive every this often
GVAR(IDF_ADMIN, connecttimeout, 5000, 15000, VAR_MAX); // disconnected when attempt exceeds this time
GVAR(IDF_ADMIN, allowtimeout, 0, 3600000, VAR_MAX); // temporary allows last this long
GVAR(IDF_ADMIN, bantimeout, 0, 14400000, VAR_MAX); // temporary bans last this long
GVAR(IDF_ADMIN, mutetimeout, 0, 3600000, VAR_MAX); // temporary mutes last this long
GVAR(IDF_ADMIN, limittimeout, 0, 3600000, VAR_MAX); // temporary limits last this long
GVAR(IDF_ADMIN, excepttimeout, 0, 3600000, VAR_MAX); // temporary allows last this long

GVAR(IDF_ADMIN, connectlock, 0, PRIV_NONE, PRIV_MAX);
GVAR(IDF_ADMIN, messagelock, 0, PRIV_NONE, PRIV_MAX);
GVAR(IDF_ADMIN, messagelength, 32, 128, BIGSTRLEN-1);
GSVAR(IDF_ADMIN, censorwords, "");

GVAR(IDF_ADMIN, setinfolock, 0, PRIV_NONE, PRIV_MAX);
GVAR(IDF_ADMIN, setinfowait, 0, 1000, VAR_MAX);

GVAR(IDF_ADMIN, demolock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, democount, 1, 10, VAR_MAX);
GVAR(IDF_ADMIN, demomaxsize, 1, 16777216, VAR_MAX - 0x10000); // Variable is in bytes. It should fit in int type. See src/game/server.cpp:adddemo()
GVAR(IDF_ADMIN, demoautorec, 0, 1, 1); // 0 = off, 1 = automatically record demos each match
GVAR(IDF_ADMIN, demokeep, 0, 0, 1); // 0 = off, 1 = keep demos that don't run to end of match
GVAR(IDF_ADMIN, demoautoserversave, 0, 0, 1);
GVAR(IDF_ADMIN, demoserverkeeptime, 0, 86400, VAR_MAX);

GVAR(IDF_MODERATOR, speclock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, teamlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, kicklock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, allowlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, banlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, mutelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, limitlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, exceptlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, vetolock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, editlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, spawnlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked, require this to spawn
GVAR(IDF_MODERATOR, spawneditlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked in editmode, require this to spawn
GVAR(IDF_MODERATOR, masterlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, crclock, 0, PRIV_MAX, PRIV_MAX);
GVAR(IDF_MODERATOR, iphostlock, 0, 0, PRIV_MAX); // require this level to see ip/hosts

GVAR(IDF_ADMIN, overflowlock, 0, PRIV_MODERATOR, PRIV_MAX); // normal message queue override
GVAR(IDF_ADMIN, overflowsize, 0, 255, VAR_MAX); // kick if queued messages >= this

GVAR(IDF_ADMIN, floodlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, floodmute, 0, 3, VAR_MAX); // automatically mute player when warned this many times
GVAR(IDF_ADMIN, floodtime, 250, 10000, VAR_MAX); // time span to check for floody messages
GVAR(IDF_ADMIN, floodlines, 1, 5, VAR_MAX); // number of lines in aforementioned span before too many

GVAR(IDF_ADMIN, teamkilllock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, teamkillwarn, 1, 3, VAR_MAX); // automatically warn player every this many team kills
GVAR(IDF_ADMIN, teamkillkick, 0, 3, VAR_MAX); // automatically kick player at this many warnings
GVAR(IDF_ADMIN, teamkillban, 0, 4, VAR_MAX); // automatically ban player at this many warnings
GVAR(IDF_ADMIN, teamkilltime, 0, 5, VAR_MAX); // time threshold (in minutes) to count
GVAR(IDF_ADMIN, teamkillrestore, 0, 1, VAR_MAX); // restore the team score as if the offender was never there if it was by this much

GVAR(IDF_ADMIN, autospectate, 0, 1, 1); // auto spectate if idle, 1 = auto spectate when remaining dead for autospecdelay
GVAR(IDF_ADMIN, autospecdelay, 0, 60000, VAR_MAX);

GVAR(IDF_ADMIN, resetallowsonend, 0, 0, 2); // reset allows on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetbansonend, 0, 0, 2); // reset bans on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetmutesonend, 0, 0, 2); // reset mutes on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetlimitsonend, 0, 0, 2); // reset limits on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetexceptsonend, 0, 0, 2); // reset excepts on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetvarsonend, 0, 1, 2); // reset variables on end (1: just when empty, 2: when matches end)
GVAR(IDF_ADMIN, resetmmonend, 0, 2, 2); // reset mastermode on end (1: just when empty, 2: when matches end)

GVARF(IDF_GAMEMOD, gamespeed, 1, 100, 10000, timescale = sv_gamespeed, timescale = gamespeed);
GVAR(IDF_ADMIN, gamespeedlock, 0, PRIV_ADMINISTRATOR, PRIV_MAX);
GVARF(IDF_ADMIN, gamepaused, 0, 0, 1, paused = sv_gamepaused, paused = gamepaused);

GSVAR(IDF_MODERATOR, defaultmap, "");
GVAR(IDF_MODERATOR, defaultmode, G_START, G_DEATHMATCH, G_MAX-1);
GVAR(IDF_MODERATOR, defaultmuts, 0, 0, G_M_ALL);

GSVAR(IDF_MODERATOR, allowmaps, "untitled");

GSVAR(IDF_MODERATOR, mainmaps, "untitled");
GSVAR(IDF_MODERATOR, capturemaps, "untitled");
GSVAR(IDF_MODERATOR, defendmaps, "untitled");
GSVAR(IDF_MODERATOR, kingmaps, "untitled");
GSVAR(IDF_MODERATOR, bombermaps, "untitled");
GSVAR(IDF_MODERATOR, holdmaps, "untitled");
GSVAR(IDF_MODERATOR, racemaps, "untitled");

GSVAR(IDF_MODERATOR, multimaps, "untitled"); // applies to modes which *require* multi spawns (ctf/bb)
GSVAR(IDF_MODERATOR, duelmaps, "untitled");
GSVAR(IDF_MODERATOR, gladiatormaps, "untitled");

GSVAR(IDF_MODERATOR, smallmaps, "untitled");
GSVAR(IDF_MODERATOR, mediummaps, "untitled");
GSVAR(IDF_MODERATOR, largemaps, "untitled");

GVAR(IDF_MODERATOR, modelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, modelocktype, 0, 2, 2); // 0 = off, 1 = only lock level can change modes, 2 = lock level can set limited modes
GVAR(IDF_MODERATOR, modelockfilter, 0, G_LIMIT, G_ALL);
GVAR(IDF_MODERATOR, mutslockfilter, 0, G_M_FILTER, G_M_ALL);
GVAR(IDF_MODERATOR, mutslockforce, 0, 0, G_M_ALL);

GVAR(IDF_MODERATOR, mapsfilter, 0, 1, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(IDF_MODERATOR, mapslock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, mapslocktype, 0, 2, 2); // 0 = off, 1 = allow maps, 2 = rotation maps

GSVAR(IDF_READONLY, previousmaps, "");
GVAR(IDF_MODERATOR, maphistory, 0, 5, VAR_MAX);

GVAR(IDF_MODERATOR, rotatemaps, 0, 2, 2); // 0 = off, 1 = sequence, 2 = random
GVAR(IDF_MODERATOR, rotatemode, 0, 1, 1);
GVARF(IDF_MODERATOR, rotatemodefilter, 0, G_LIMIT, G_ALL, sv_rotatemodefilter &= ~G_NEVER, rotatemodefilter &= ~G_NEVER); // modes not in this array are filtered out
GVAR(IDF_MODERATOR, rotatemuts, 0, 3, VAR_MAX); // any more than one decreases the chances of it picking
GVAR(IDF_MODERATOR, rotatemutsfilter, 0, G_M_ROTATE, G_M_ALL); // mutators not in this array are filtered out
GVAR(IDF_MODERATOR, rotatemapsfilter, 0, 2, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(IDF_MODERATOR, rotatecycle, 0, 10, VAR_MAX); // 0 = off, else = minutes between forced re-cycles

GVAR(IDF_MODERATOR, votelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_MODERATOR, votelocktype, 0, 2, 2); // 0 = off, 1 = lock level only, 2 = lock level can select previousmaps
GVAR(IDF_MODERATOR, votefilter, 0, 1, 1); // 0 = off, 1 = skip spectators
GVAR(IDF_MODERATOR, votewait, 0, 2500, VAR_MAX);
GVAR(IDF_MODERATOR, votestyle, 0, 2, 2); // 0 = votes don't pass mid-match, 1 = passes if votethreshold is met, 2 = passes if unanimous
GVAR(IDF_MODERATOR, voteinterm, 0, 2, 3); // 0 = must wait entire time, 1 = passes if votethreshold is met, 2 = passes if unanimous, 3 = passes after waiting then selects a random vote
GFVAR(IDF_MODERATOR, votethreshold, 0, 0.5f, 1); // auto-pass votes when this many agree

GVAR(IDF_MODERATOR, smallmapmax, 0, 6, VAR_MAX); // maximum number of players for a small map
GVAR(IDF_MODERATOR, mediummapmax, 0, 12, VAR_MAX); // maximum number of players for a medium map

GVAR(IDF_MODERATOR, waitforplayers, 0, 2, 2); // wait for players: 0 = off, 1 = to load the map, 2 = to exit spectator
GVAR(IDF_MODERATOR, waitforplayertime, 0, 10000, VAR_MAX); // wait at most this long for players to meet waitforplayers criteria
GVAR(IDF_MODERATOR, waitforplayerload, 0, 20000, VAR_MAX); // wait this long for players to load the map to schedule map requests
GVAR(IDF_MODERATOR, waitforplayermaps, 0, 30000, VAR_MAX); // wait this long for sendmap and getmap requests
GVAR(IDF_MODERATOR, waitforplayerinfo, 0, 10000, VAR_MAX); // wait at least this long for players to send info

namespace server
{
    extern void resetgamevars(bool all);
    extern void savegamevars();
}
GICOMMAND(0, resetvars, "i", (int *n), server::resetgamevars(*n!=0); result("success"), );
GICOMMAND(IDF_ADMIN, savevars, "", (), server::savegamevars(); result("success"), );
GICOMMAND(IDF_MODERATOR, resetconfig, "", (), rehash(true); result("success"), );

GFVAR(IDF_MODERATOR, maxalive, 0, 0, FVAR_MAX); // only allow this*maxplayers to be alive at once, 0 = turn off maxalive
GVAR(IDF_MODERATOR, maxalivequeue, 0, 1, 1); // upon triggering maxalive, use a queue system
GVAR(IDF_MODERATOR, maxaliveminimum, 2, 4, VAR_MAX); // kicks in if alive >= this
GFVAR(IDF_MODERATOR, maxalivethreshold, 0, 0, FVAR_MAX); // .. or this percentage of clients

GVAR(IDF_GAMEMOD, spawnrotate, 0, 2, 2); // 0 = let client decide, 1 = sequence, 2 = random
GVAR(IDF_GAMEMOD, spawnprotect, 0, 3000, VAR_MAX); // delay before damage can be dealt to spawning player
GVAR(IDF_GAMEMOD, duelprotect, 0, 3000, VAR_MAX); // .. in duel/survivor matches
GVAR(IDF_GAMEMOD, survivorprotect, 0, 5000, VAR_MAX); // .. in duel/survivor matches
GVAR(IDF_GAMEMOD, instaprotect, 0, 3000, VAR_MAX); // .. in instagib matches
GVAR(IDF_GAMEMOD, protectbreakshoot, 0, 1, 1); // 0 = off, 1 = protection is broken when player starts firing
GVAR(IDF_GAMEMOD, protectbreakcook, 0, 1, 1); // 0 = off, 1 = protection is broken when player starts cooking
GVAR(IDF_GAMEMOD, protectbreakaffinity, 0, 1, 1); // 0 = off, 1 = protection is broken when player takes an affinity

GVAR(IDF_GAMEMOD, radarhardaffinity, 0, 1, 1); // 0 = do not allow showing affinities with hard mutator, 1 = allow it
GVAR(IDF_GAMEMOD, radardisabled, 0, 0, 1); // forces the radar to be off
GFVAR(IDF_GAMEMOD, radardistlimit, 0, 0, FVAR_MAX); // forces the radar to this distance max, 0 = off

GVAR(IDF_GAMEMOD, balancemaps, -1, -1, 3); // determined if map team balancing is used: -1 = map default, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced
GVAR(IDF_GAMEMOD, balancereset, 0, 2, 2); // reset players when balancing them, 0 = off, 1 = only when necessary, 2 = always
GVAR(IDF_GAMEMOD, balancedelay, 0, 5000, VAR_MAX); // before mapbalance forces
GVAR(IDF_GAMEMOD, balancenospawn, 0, 1, 1); // prevent respawning when waiting to balance
GVAR(IDF_GAMEMOD, balanceduke, 0, 1, 1); // enable in duel/survivor

GFVAR(IDF_GAMEMOD, maxhealth, 0, 1.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, maxhealthvampire, 0, 3.0f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, vampirescale, 0, 1.0f, FVAR_MAX);

GFVAR(IDF_GAMEMOD, maxresizescale, 1, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, minresizescale, FVAR_NONZERO, 0.5f, 1);
GFVAR(IDF_GAMEMOD, instaresizeamt, FVAR_NONZERO, 0.1f, 1); // each kill adds this much size in insta-resize

GVAR(IDF_GAMEMOD, regendelay, 0, 3500, VAR_MAX); // regen after no damage for this long
GVAR(IDF_GAMEMOD, regentime, 0, 1000, VAR_MAX); // regen this often when regenerating normally
GVAR(IDF_GAMEMOD, regenhealth, 0, 50, VAR_MAX); // regen this amount each regen
GVAR(IDF_GAMEMOD, regendecay, 0, 25, VAR_MAX); // if over maxhealth, decay this amount each regen

GVAR(IDF_GAMEMOD, noweapburn, 0, 1, 1);
GVAR(IDF_GAMEMOD, noweapbleed, 0, 1, 1);
GVAR(IDF_GAMEMOD, noweapshock, 0, 1, 1);

GVAR(IDF_GAMEMOD, kamikaze, 0, 1, 3); // 0 = never, 1 = holding grenade, 2 = have grenade, 3 = always
GVAR(IDF_GAMEMOD, itemspawntime, 1, 15000, VAR_MAX); // when items respawn
GVAR(IDF_GAMEMOD, itemspawndelay, 0, 0, VAR_MAX); // after map start items first spawn
GVAR(IDF_GAMEMOD, itemspawnstyle, 0, 0, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both
GVAR(IDF_GAMEMOD, itemcollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, itemextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, iteminteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, itemelasticity, FVAR_MIN, 0.4f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemrelativity, FVAR_MIN, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemliquidcoast, 0, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemweight, FVAR_MIN, 150, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemspeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemspeedmax, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemrepulsion, 0, 8, FVAR_MAX);
GFVAR(IDF_GAMEMOD, itemrepelspeed, 0, 25, FVAR_MAX);

GVAR(IDF_GAMEMOD, triggermillis, 1, 1000, INT_MAX);
GVAR(IDF_GAMEMOD, triggerdelay, 1, 250, INT_MAX);

//  dm          duel        survivor    gladiator   capture     defend      defendking  bomber      bomberhold  race        racetimed   racegauntlet
MMVAR(IDF_GAMEMOD, timelimit, 0, VAR_MAX,
    10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10,         10
);
MMVAR(IDF_GAMEMOD, overtimeallow, 0, 1,
    1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1,          1
);
MMVAR(IDF_GAMEMOD, overtimelimit, 0, VAR_MAX,
    5,          2,          3,          2,          5,          5,          5,          5,          5,          5,          5,          5
);
#ifdef GAMESERVER
SVAR(0, limitidxname, "duel survivor gladiator capture defend defendking bomber bomberhold race racetimed racegauntlet");
#endif

GVAR(IDF_GAMEMOD, intermlimit, 0, 10000, VAR_MAX); // .. before vote menu comes up
GVAR(IDF_GAMEMOD, votelimit, 0, 40000, VAR_MAX); // .. before vote passes by default

GVAR(IDF_GAMEMOD, duelrandom, 0, 1, 1); // randomise queue before first round
GVAR(IDF_GAMEMOD, duelreset, 0, 1, 1); // reset winner in duel
GVAR(IDF_GAMEMOD, duelclear, 0, 1, 1); // clear items in duel
GVAR(IDF_GAMEMOD, duelregen, 0, 0, 1); // allow regen in duel
GVAR(IDF_GAMEMOD, dueldelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, duelcooloff, 0, 3000, VAR_MAX); // cool off period before duel goes to next round
GVAR(IDF_GAMEMOD, duelcycle, 0, 2, 3); // determines if players are force-cycled after a certain number of wins (bit: 0 = off, 1 = non-team games, 2 = team games)
GVAR(IDF_GAMEMOD, duelcycles, 0, 2, VAR_MAX); // maximum wins in a row before force-cycling (0 = num team/total players)
GVAR(IDF_GAMEMOD, duelaffinity, 0, 1, 2); // 0 = off, 1 = on enter can respawn next iter, 2 = on enter can respawn immediately
GVAR(IDF_GAMEMOD, duelbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(IDF_GAMEMOD, duelovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, duelmaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for duel. 0 = any number of players

GVAR(IDF_GAMEMOD, survivorreset, 0, 1, 1); // reset winners in survivor
GVAR(IDF_GAMEMOD, survivorclear, 0, 1, 1); // clear items in survivor
GVAR(IDF_GAMEMOD, survivorregen, 0, 0, 1); // allow regen in survivor
GVAR(IDF_GAMEMOD, survivordelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(IDF_GAMEMOD, survivorcooloff, 0, 3000, VAR_MAX); // cool off period before survivor goes to next round
GVAR(IDF_GAMEMOD, survivoraffinity, 0, 0, 1); // 0 = off, 1 = on enter can spawn immediately
GVAR(IDF_GAMEMOD, survivorbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(IDF_GAMEMOD, survivorovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime
GVAR(IDF_GAMEMOD, survivormaxqueued, 0, 0, MAXCLIENTS); // number of players that can be queued for survivor. 0 = any number of players

GVAR(IDF_GAMEMOD, pointlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, fraglimit, 0, 0, VAR_MAX); // finish when score is this or more in oldschool
GVAR(IDF_GAMEMOD, racelimit, 0, 0, VAR_MAX); // finish when lap count is this or more
GVAR(IDF_GAMEMOD, teampersist, 0, 1, 2); // 0 = off, 1 = only attempt, 2 = forced
GVAR(IDF_GAMEMOD, damageself, 0, 1, 1); // 0 = off, 1 = either hurt self or use damageteam rules
GFVAR(IDF_GAMEMOD, damageselfscale, 0, 1, FVAR_MAX); // 0 = off, anything else = scale for damage
GFVAR(IDF_GAMEMOD, damageteamscale, 0, 1, FVAR_MAX); // 0 = off, anything else = scale for damage

GVAR(IDF_GAMEMOD, teambalance, 0, 6, 6); // 0 = off, 1 = by number then style, 2 = by style then number, 3 = by number and enforce, 4 = number, enforce, reassign, 5 = style, number, enforce, reassign, 6 = style during waiting, revert to 4 otherwise
GVAR(IDF_GAMEMOD, teambalanceduel, 0, 0, 1); // allow reassignments in duel
GVAR(IDF_GAMEMOD, teambalanceplaying, 2, 2, VAR_MAX); // min players before reassignments occur
GVAR(IDF_GAMEMOD, teambalanceamt, 2, 2, VAR_MAX); // max-min offset before reassignments occur
GVAR(IDF_GAMEMOD, teambalancewait, 0, 10000, VAR_MAX); // how long before can happen again
GVAR(IDF_GAMEMOD, teambalancedelay, 0, 3000, VAR_MAX); // how long before reassignments start
GVAR(IDF_GAMEMOD, teambalanceswap, 0, 1, 1); // allow swap requests if unable to change team
GVAR(IDF_GAMEMOD, teambalancelock, 0, PRIV_MODERATOR, PRIV_MAX); // level at which one can override swap
GVAR(IDF_GAMEMOD, teambalancestyle, 0, 7, 7); // when moving players, sort by: 0 = top of list, 1 = time played, 2 = points, 3 = frags, 4 = scoretime, 5 = kdratio, 6 = combined kdratio, 7 = average scoreboard position
GVAR(IDF_GAMEMOD, teambalancehighest, 0, 1, 1); // when moving players, move highest first
GFVAR(IDF_GAMEMOD, teambalanceavgposlocalweight, 0, 0.33, 1.0); // weight of local statistics when balancing players by average scoreboard position
GVAR(IDF_GAMEMOD, teambalanceavgposdelay, 0, 5000, VAR_MAX); // delay betweel calculation of avgpos statistics

GVAR(IDF_GAMEMOD, racegauntletwinner, 0, 1, 1); // declare the winner when the final team exceeds best score

GVAR(IDF_GAMEMOD, capturelimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, captureresetfloor, 0, 1, 2); // if tossed, reset to last floor pos. 1 = team tosses own flag, 2 = any flag
GVAR(IDF_GAMEMOD, captureresetstore, 0, 2, 15);
GVAR(IDF_GAMEMOD, captureresetdelay, 0, 30000, VAR_MAX);
GVAR(IDF_GAMEMOD, capturedefenddelay, 0, 10000, VAR_MAX);
GVAR(IDF_GAMEMOD, captureprotectdelay, 0, 15000, VAR_MAX);
GVAR(IDF_GAMEMOD, capturepickupdelay, 500, 2500, VAR_MAX);
GFVAR(IDF_GAMEMOD, capturecarryspeed, 0, 0.9f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturedropheight, 0, 8, FVAR_MAX);
GVAR(IDF_GAMEMOD, capturepoints, 0, 5, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, capturepickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, capturecollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, captureextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, captureinteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, capturerelativity, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, captureelasticity, FVAR_MIN, 0.65f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, captureliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, captureweight, FVAR_MIN, 400, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturespeedmin, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturespeedmax, 0, 100, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturerepulsion, 0, 16, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturerepelspeed, 0, 25, FVAR_MAX);
GFVAR(IDF_GAMEMOD, capturethreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(IDF_GAMEMOD, capturebuffing, 0, 9, 63); // buffed; 0 = off, &1 = when defending, &2 = when defending dropped, &4 = when secured, &8 = when defending secured, &16 = when secured enemy, &32 = when defending secured enemy
GVAR(IDF_GAMEMOD, capturebuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, capturebuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, capturebuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, capturebuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, captureregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, captureregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, captureregenextra, 0, 20, VAR_MAX); // add this to regen when buffed
GVAR(IDF_GAMEMOD, captureretakedelay, 0, 1000, VAR_MAX); // same person can't take same flag this long after capturing it

GVAR(IDF_GAMEMOD, defendlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(IDF_GAMEMOD, defendpoints, 0, 1, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, defendinterval, 0, 50, VAR_MAX);
GVAR(IDF_GAMEMOD, defendhold, 1, 100, VAR_MAX); // points needed to gain a score point
GVAR(IDF_GAMEMOD, defendoccupy, 1, 100, VAR_MAX); // points needed to occupy in regular games
GVAR(IDF_GAMEMOD, defendking, 1, 100, VAR_MAX); // points needed to occupy in king of the hill
GVAR(IDF_GAMEMOD, defendflags, 0, 3, 3); // 0 = init all (neutral), 1 = init neutral and team only, 2 = init team only, 3 = init all (team + neutral + converted)
GVAR(IDF_GAMEMOD, defendbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = even when enemies are present
GFVAR(IDF_GAMEMOD, defendbuffoccupy, 0, 0.5f, 1); // for defendbuffing&4, must be occupied this much before passing
GVAR(IDF_GAMEMOD, defendbuffdelay, 0, 1000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, defendbuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, defendbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, defendbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, defendregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, defendregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, defendregenextra, 0, 20, VAR_MAX); // add this to regen when buffed

GVAR(IDF_GAMEMOD, bomberlimit, 0, 0, VAR_MAX); // finish when score is this or more (non-hold)
GVAR(IDF_GAMEMOD, bomberholdlimit, 0, 0, VAR_MAX); // finish when score is this or more (hold)
GVAR(IDF_GAMEMOD, bomberbasketonly, 0, 0, 1); // prohibit touchdowns in basket game
GVAR(IDF_GAMEMOD, bomberattackreset, 0, 1, 1); // defenders reset rather than carry the ball
GVAR(IDF_GAMEMOD, bomberattackwinner, 0, 1, 1); // declare the winner when the final team exceeds best score
GFVAR(IDF_GAMEMOD, bomberbasketmindist, 0, 0, FVAR_MAX); // prohibit baskets less than this far away
GVAR(IDF_GAMEMOD, bomberwait, 0, 1000, VAR_MAX); // delay before bomb spawns
GVAR(IDF_GAMEMOD, bomberresetdelay, 0, 15000, VAR_MAX);
GVAR(IDF_GAMEMOD, bomberpickupdelay, 500, 7500, VAR_MAX);
GVAR(IDF_GAMEMOD, bombercarrytime, 0, 15000, VAR_MAX);
GFVAR(IDF_GAMEMOD, bombercarryspeed, 0, 0.9f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberdropheight, 0, 8, FVAR_MAX);
GVAR(IDF_GAMEMOD, bomberpoints, 0, 5, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, bomberpenalty, 0, 5, VAR_MAX); // points taken from score
GVAR(IDF_GAMEMOD, bomberpickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, bomberthrowinpoints, 0, 3, VAR_MAX); // team points for throw in
GVAR(IDF_GAMEMOD, bombertouchdownpoints, 0, 1, VAR_MAX); // team points for touch down
GVAR(IDF_GAMEMOD, bomberholdtime, 0, 15000, VAR_MAX);
GVAR(IDF_GAMEMOD, bomberholdpoints, 0, 1, VAR_MAX); // points added to score
GVAR(IDF_GAMEMOD, bomberholdpenalty, 0, 10, VAR_MAX); // penalty for holding too long
GVAR(IDF_GAMEMOD, bomberholdinterval, 0, 1000, VAR_MAX);
GVAR(IDF_GAMEMOD, bomberlockondelay, 0, 250, VAR_MAX);
GFVAR(IDF_GAMEMOD, bomberspeed, 0, 250, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberspeeddelta, 0, 1000, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberspeedmin, 0, 65, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberspeedmax, 0, 200, FVAR_MAX);
GVAR(IDF_GAMEMOD, bombercollide, 0, BOUNCE_GEOM, COLLIDE_ALL);
GVAR(IDF_GAMEMOD, bomberextinguish, 0, 6, 7);
GVAR(IDF_GAMEMOD, bomberinteracts, 0, 3, 3);
GFVAR(IDF_GAMEMOD, bomberrelativity, 0, 0.25f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberelasticity, FVAR_MIN, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberweight, FVAR_MIN, 350, FVAR_MAX);
GFVAR(IDF_GAMEMOD, bomberthreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(IDF_GAMEMOD, bomberbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = when secured as defender in attack
GVAR(IDF_GAMEMOD, bomberbuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(IDF_GAMEMOD, bomberbuffarea, FVAR_NONZERO, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(IDF_GAMEMOD, bomberbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(IDF_GAMEMOD, bomberbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(IDF_GAMEMOD, bomberregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(IDF_GAMEMOD, bomberregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(IDF_GAMEMOD, bomberregenextra, 0, 20, VAR_MAX); // add this to regen when buffed

GFVAR(IDF_MODERATOR, aihostnum, 0, 50, FVAR_MAX); // Multiply number of bots hosted by this much for the bot hosting decision.
GFVAR(IDF_MODERATOR, aihostping, 0, 1, FVAR_MAX); // Multiply host ping by this much for the bot hosting decision.
GFVAR(IDF_MODERATOR, aihostshift, 0, 75, FVAR_MAX); // Require this much difference before shifting bot hosts.
GVAR(IDF_MODERATOR, airefreshdelay, 0, 1000, VAR_MAX);
GVAR(IDF_MODERATOR, aiweightdrag, 0, 5000, VAR_MAX);
GFVAR(IDF_MODERATOR, aiweightpull, 0, 1, FVAR_MAX);
GVAR(IDF_GAMEMOD, botbalance, -1, -1, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GFVAR(IDF_GAMEMOD, botbalancescale, FVAR_NONZERO, 1, FVAR_MAX); // use balance*this
GVAR(IDF_GAMEMOD, botbalanceduel, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(IDF_GAMEMOD, botbalancesurvivor, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(IDF_GAMEMOD, botskillmin, 1, 60, 101);
GVAR(IDF_GAMEMOD, botskillmax, 1, 80, 101);
GFVAR(IDF_GAMEMOD, botskillfrags, -100, -1, 100);
GFVAR(IDF_GAMEMOD, botskilldeaths, -100, 1, 100);
GVAR(IDF_GAMEMOD, botlimit, 0, 32, MAXAI);
GVAR(IDF_GAMEMOD, botoffset, VAR_MIN, 0, VAR_MAX);
GSVAR(IDF_MODERATOR, botmalenames, "");
GSVAR(IDF_MODERATOR, botfemalenames, "");
GSVAR(IDF_MODERATOR, botmalevanities, "");
GSVAR(IDF_MODERATOR, botfemalevanities, "");
GVAR(IDF_GAMEMOD, botcolourseed, 0, 15, 15); // random bot things will be determined by colour as seed, bitwise: 0 = off, 1 = skill, 2 = name, 4 = model, 8 = loadout
GVAR(IDF_GAMEMOD, botrandomcase, 0, 2, VAR_MAX); // bots will randomise the first letter of their name if rnd(botrandomcase) > 0
GVAR(IDF_ADMIN, botoverridelock, PRIV_NONE, PRIV_ADMINISTRATOR, PRIV_MAX);
GFVAR(IDF_GAMEMOD, coopbalance, FVAR_NONZERO, 1.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, coopmultibalance, FVAR_NONZERO, 2, FVAR_MAX);
GVAR(IDF_GAMEMOD, coopskillmin, 1, 75, 101);
GVAR(IDF_GAMEMOD, coopskillmax, 1, 85, 101);
GFVAR(IDF_GAMEMOD, coopskillfrags, -100, 0, 100);
GFVAR(IDF_GAMEMOD, coopskilldeaths, -100, 1, 100);
GVAR(IDF_GAMEMOD, enemybalance, 1, 1, 3);
GVAR(IDF_GAMEMOD, enemyskillmin, 1, 55, 101);
GVAR(IDF_GAMEMOD, enemyskillmax, 1, 75, 101);
GVAR(IDF_GAMEMOD, enemylimit, 0, 32, MAXAI);
GVAR(IDF_GAMEMOD, enemyspawntime, 1, 60000, VAR_MAX); // when enemies respawn
GVAR(IDF_GAMEMOD, enemyspawndelay, 0, 1000, VAR_MAX); // after map start enemies first spawn
GVAR(IDF_GAMEMOD, enemyspawnstyle, 0, 1, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both

GFVAR(IDF_GAMEMOD, movespeed, FVAR_NONZERO, 1.f, FVAR_MAX); // speed
GFVAR(IDF_GAMEMOD, moveslow, 0, 0.5f, FVAR_MAX); // threshold for running
GFVAR(IDF_GAMEMOD, movecrawl, 0, 0.6f, FVAR_MAX); // crawl modifier
GFVAR(IDF_GAMEMOD, moverun, FVAR_NONZERO, 1.3f, FVAR_MAX); // running modifier
GFVAR(IDF_GAMEMOD, movestraight, FVAR_NONZERO, 1.2f, FVAR_MAX); // non-strafe modifier
GFVAR(IDF_GAMEMOD, movestrafe, FVAR_NONZERO, 1, FVAR_MAX); // strafe modifier
GFVAR(IDF_GAMEMOD, moveinair, FVAR_NONZERO, 0.9f, FVAR_MAX); // in-air modifier
GFVAR(IDF_GAMEMOD, movestepup, FVAR_NONZERO, 0.95f, FVAR_MAX); // step-up modifier
GFVAR(IDF_GAMEMOD, movestepdown, FVAR_NONZERO, 1.15f, FVAR_MAX); // step-down modifier

GFVAR(IDF_GAMEMOD, impulseboost, 0, 1, FVAR_MAX); // boost modifier
GFVAR(IDF_GAMEMOD, impulseboostredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulseboostpitchmin, -89.9f, -89.9f, 89.9f); // boost pitch minimum
GFVAR(IDF_GAMEMOD, impulseboostpitchmax, -89.9f, 89.9f, 89.9f); // boost pitch maximum
GFVAR(IDF_GAMEMOD, impulsepower, 0, 1.5f, FVAR_MAX); // power jump modifier
GFVAR(IDF_GAMEMOD, impulsepowerredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsedash, 0, 1.3f, FVAR_MAX); // dashing/powerslide modifier
GFVAR(IDF_GAMEMOD, impulsedashredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsejump, 0, 1.1f, FVAR_MAX); // jump modifier
GFVAR(IDF_GAMEMOD, impulsejumpredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsemelee, 0, 0.75f, FVAR_MAX); // melee modifier
GFVAR(IDF_GAMEMOD, impulsemeleeredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulseparkour, 0, 1, FVAR_MAX); // parkour modifier
GFVAR(IDF_GAMEMOD, impulseparkourredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulseparkournorm, 0, 0.5f, FVAR_MAX); // minimum parkour surface z normal
GFVAR(IDF_GAMEMOD, impulsekick, 0, 1.5f, FVAR_MAX); // parkour kick modifier
GFVAR(IDF_GAMEMOD, impulsekickredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsekickpitchmin, -89.9f, -89.9f, 89.9f); // kick pitch minimum
GFVAR(IDF_GAMEMOD, impulsekickpitchmax, -89.9f, 89.9f, 89.9f); // kick pitch maximum
GFVAR(IDF_GAMEMOD, impulseclimb, 0, 1.4f, FVAR_MAX); // parkour climb modifier
GFVAR(IDF_GAMEMOD, impulseclimbredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsevault, 0, 1.5f, FVAR_MAX); // parkour vault modifier
GFVAR(IDF_GAMEMOD, impulsevaultredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsevaultmin, FVAR_NONZERO, 0.25f, FVAR_MAX); // minimum percentage of height for vault
GFVAR(IDF_GAMEMOD, impulsevaultmax, FVAR_NONZERO, 1.6f, FVAR_MAX); // maximum percentage of height for vault
GFVAR(IDF_GAMEMOD, impulsegrab, 0, 1.8f, FVAR_MAX); // parkour grab modifier
GFVAR(IDF_GAMEMOD, impulsegrabredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(IDF_GAMEMOD, impulsegrabplayer, 0, 1.1f, FVAR_MAX); // parkour grab player modifier
GFVAR(IDF_GAMEMOD, impulsegrabplayerredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one

GVAR(IDF_GAMEMOD, impulsestyle, 0, 1, 3); // impulse style; 0 = off, 1 = touch and count, 2 = count only, 3 = freestyle
GVAR(IDF_GAMEMOD, impulsecount, 0, 5, VAR_MAX); // number of impulse actions per air transit
GVAR(IDF_GAMEMOD, impulseslip, 0, 500, VAR_MAX); // time before floor friction kicks back in
GVAR(IDF_GAMEMOD, impulseslide, 0, 750, VAR_MAX); // time before powerslides end
GVAR(IDF_GAMEMOD, impulseskate, 0, 1000, VAR_MAX); // length of time a run along a wall can last

GVAR(IDF_GAMEMOD, impulsejumpdelay, 0, 250, VAR_MAX); // minimum time after jump for boost
GVAR(IDF_GAMEMOD, impulseboostdelay, 0, 250, VAR_MAX); // minimum time between boosts
GVAR(IDF_GAMEMOD, impulsedashdelay, 0, 1000, VAR_MAX); // minimum time between dashes/powerslides
GVAR(IDF_GAMEMOD, impulsekickdelay, 0, 250, VAR_MAX); // minimum time between wall kicks/climbs/grabs

GFVAR(IDF_GAMEMOD, spreadcrouch, 0, 0.25f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, spreadzoom, 0, 0.125f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, spreadstill, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, spreadmoving, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, spreadrunning, 0, 1.f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, spreadinair, 0, 1.f, FVAR_MAX);

GVAR(IDF_GAMEMOD, quakefade, 0, 300, VAR_MAX);
GVAR(IDF_GAMEMOD, quakewobble, 1, 20, VAR_MAX);
GVAR(IDF_GAMEMOD, quakelimit, 0, 200, VAR_MAX);

GFVAR(IDF_GAMEMOD, damagescale, 0, 1, FVAR_MAX);
GVAR(IDF_GAMEMOD, weaponswitchdelay, 0, 400, VAR_MAX);

GFVAR(IDF_GAMEMOD, radialscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, pushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, hitpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, deadpushscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, wavepushscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, stunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, hitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, deadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, wavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, velscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, hitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, deadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, wavevelscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorradialscale, 0, 0.5f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorhitpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatordeadpushscale, 0, 5, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorwavepushscale, 0, 5, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorhitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatordeadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorwavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorhitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatordeadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorwavevelscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorextrapushscale, 0, 1, FVAR_MAX);  // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, gladiatorextrahitpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextradeadpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextrawavepushscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorextrastunscale, 0, 1, FVAR_MAX); // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, gladiatorextrahitstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextradeadstunscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextrawavestunscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, gladiatorextravelscale, 0, 1, FVAR_MAX); // these do amt*maxhealth/health*extra
GFVAR(IDF_GAMEMOD, gladiatorextrahitvelscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextradeadvelscale, 0, 2, FVAR_MAX);
GFVAR(IDF_GAMEMOD, gladiatorextrawavevelscale, 0, 1, FVAR_MAX);

GFVAR(IDF_GAMEMOD, radiallimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, pushlimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, stunlimited, 0, 0.75f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, vellimited, 0, 0.75f, FVAR_MAX);

GFVAR(IDF_GAMEMOD, kickpushscale, 0, 1, FVAR_MAX);
GFVAR(IDF_GAMEMOD, kickpushcrouch, 0, 0, FVAR_MAX);
GFVAR(IDF_GAMEMOD, kickpushsway, 0, 0.0125f, FVAR_MAX);
GFVAR(IDF_GAMEMOD, kickpushzoom, 0, 0.125f, FVAR_MAX);

GVAR(IDF_GAMEMOD, fragbonus, 0, 3, VAR_MAX);
GVAR(IDF_GAMEMOD, enemybonus, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, lasthitbonus, 0, 3, VAR_MAX);
GVAR(IDF_GAMEMOD, teamkillpenalty, 0, 2, VAR_MAX);
GVAR(IDF_GAMEMOD, firstbloodpoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, headshotpoints, 0, 1, VAR_MAX);

GVAR(IDF_GAMEMOD, assistkilldelay, 0, 5000, VAR_MAX);
GVAR(IDF_GAMEMOD, lasthitdelay, 0, 10000, VAR_MAX);
GVAR(IDF_GAMEMOD, multikilldelay, 0, 5000, VAR_MAX);
GVAR(IDF_GAMEMOD, multikillpoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, multikillbonus, 0, 0, 1); // if bonus is on, then points are multiplied by the current kill mutliplier (x2, x3, x4)
GVAR(IDF_GAMEMOD, spreecount, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, spreepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, spreebreaker, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, dominatecount, 0, 5, VAR_MAX);
GVAR(IDF_GAMEMOD, dominatepoints, 0, 1, VAR_MAX);
GVAR(IDF_GAMEMOD, revengepoints, 0, 1, VAR_MAX);

GSVAR(0, obitdestroyed, "was destroyed");
GSVAR(0, obitdied, "died");
GSVAR(0, obitfragged, "fragged");
GSVAR(0, obitspawn, "tried to spawn inside solid matter");
GSVAR(0, obitspectator, "gave up their corporeal form");
GSVAR(0, obitdrowned, "drowned");
GSVAR(0, obitmelted, "melted into a ball of fire");
GSVAR(0, obitdeathmat, "met their end");
GSVAR(0, obithurtmat, "stayed under too long");
GSVAR(0, obitlost, "fell to their death");
GSVAR(0, obitburn, "set ablaze");
GSVAR(0, obitburnself, "burned up");
GSVAR(0, obitbleed, "fatally wounded");
GSVAR(0, obitbleedself, "bled out");
GSVAR(0, obitshock, "given a terminal dose of shock therapy");
GSVAR(0, obitshockself, "twitched to death");
GSVAR(0, obitobliterated, "was obliterated");
GSVAR(0, obitheadless, "had their head caved in");
GSVAR(0, obitsuicide, "suicided");
GSVAR(0, obitkilled, "killed");
