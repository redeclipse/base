GVAR(IDF_WORLD, numplayers, 0, 4, MAXCLIENTS); // 0 = determine from number of spawns
GVAR(IDF_WORLD, maxplayers, 0, 0, MAXCLIENTS); // 0 = numplayers*3
GVAR(IDF_WORLD, mapbalance, 0, 0, 3); // switches teams for asymmetrical maps, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced

GFVAR(IDF_WORLD, gravity, 0, 50.f, 1000); // gravity
GFVAR(0, gravityscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, floorcoast, 0, 5.f, 1000);
GFVAR(0, floorcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, aircoast, 0, 25.f, 1000);
GFVAR(0, aircoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, slidecoast, 0, 40.f, 1000);
GFVAR(0, slidecoastscale, 0, 1, FVAR_MAX);

GFVAR(IDF_WORLD, liquidspeed, 0, 0.85f, 1);
GFVAR(0, liquidspeedscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidcoast, 0, 10.f, 1000);
GFVAR(0, liquidcoastscale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidsubmerge, 0, 0.75f, 1);
GFVAR(0, liquidsubmergescale, 0, 1, FVAR_MAX);
GFVAR(IDF_WORLD, liquidextinguish, 0, 0.25f, 1);
GFVAR(0, liquidextinguishscale, 0, 1, FVAR_MAX);

GVAR(IDF_ADMIN, serverstats, 0, 0, 1);
GVAR(IDF_ADMIN, serverdebug, 0, 0, 3);
GVAR(IDF_ADMIN, serverclients, 1, 16, MAXCLIENTS);
GVARF(IDF_ADMIN, serverdupclients, 0, 0, MAXCLIENTS, limitdupclients(), );
GVAR(IDF_ADMIN, serveropen, 0, 3, 3);
GSVAR(IDF_ADMIN, serverdesc, "");
GSVAR(IDF_ADMIN, servermotd, "");

GVAR(IDF_ADMIN, autoadmin, 0, 0, 1);

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
GVAR(IDF_ADMIN, demomaxsize, 1, 16, VAR_MAX);
GVAR(IDF_ADMIN, demoautorec, 0, 1, 1); // 0 = off, 1 = automatically record demos each match
GVAR(IDF_ADMIN, demokeep, 0, 0, 1); // 0 = off, 1 = keep demos that don't run to end of match
GVAR(IDF_ADMIN, demoautoserversave, 0, 0, 1);
GVAR(IDF_ADMIN, demoserverkeeptime, 0, 86400, VAR_MAX);

GVAR(IDF_ADMIN, speclock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, teamlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, kicklock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, allowlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, banlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, mutelock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, limitlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, exceptlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, vetolock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, editlock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, spawnlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked, require this to spawn
GVAR(IDF_ADMIN, spawneditlock, 0, PRIV_MODERATOR, PRIV_MAX); // if locked in editmode, require this to spawn
GVAR(IDF_ADMIN, masterlock, 0, PRIV_MODERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, crclock, 0, PRIV_MAX, PRIV_MAX);

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

GVARF(0, gamespeed, 1, 100, 10000, timescale = sv_gamespeed, timescale = gamespeed);
GVAR(IDF_ADMIN, gamespeedlock, 0, PRIV_ADMINISTRATOR, PRIV_MAX);
GVARF(IDF_ADMIN, gamepaused, 0, 0, 1, paused = sv_gamepaused, paused = gamepaused);

GSVAR(IDF_ADMIN, defaultmap, "");
GVAR(IDF_ADMIN, defaultmode, G_START, G_DEATHMATCH, G_MAX-1);
GVAR(IDF_ADMIN, defaultmuts, 0, 0, G_M_ALL);

GSVAR(IDF_ADMIN, allowmaps, "untitled");

GSVAR(IDF_ADMIN, mainmaps, "untitled");
GSVAR(IDF_ADMIN, capturemaps, "untitled");
GSVAR(IDF_ADMIN, defendmaps, "untitled");
GSVAR(IDF_ADMIN, kingmaps, "untitled");
GSVAR(IDF_ADMIN, bombermaps, "untitled");
GSVAR(IDF_ADMIN, holdmaps, "untitled");
GSVAR(IDF_ADMIN, racemaps, "untitled");

GSVAR(IDF_ADMIN, multimaps, "untitled"); // applies to modes which *require* multi spawns (ctf/bb)
GSVAR(IDF_ADMIN, duelmaps, "untitled");

GSVAR(IDF_ADMIN, smallmaps, "untitled");
GSVAR(IDF_ADMIN, mediummaps, "untitled");
GSVAR(IDF_ADMIN, largemaps, "untitled");

GVAR(IDF_ADMIN, modelock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, modelocktype, 0, 2, 2); // 0 = off, 1 = only lock level can change modes, 2 = lock level can set limited modes
GVAR(IDF_ADMIN, modelockfilter, 0, G_LIMIT, G_ALL);
GVAR(IDF_ADMIN, mutslockfilter, 0, G_M_FILTER, G_M_ALL);
GVAR(IDF_ADMIN, mutslockforce, 0, 0, G_M_ALL);

GVAR(IDF_ADMIN, mapsfilter, 0, 1, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(IDF_ADMIN, mapslock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, mapslocktype, 0, 2, 2); // 0 = off, 1 = allow maps, 2 = rotation maps

GSVAR(IDF_READONLY, previousmaps, "");
GVAR(IDF_ADMIN, maphistory, 0, 5, VAR_MAX);

GVAR(IDF_ADMIN, rotatemaps, 0, 2, 2); // 0 = off, 1 = sequence, 2 = random
GVAR(IDF_ADMIN, rotatemode, 0, 1, 1);
GVARF(IDF_ADMIN, rotatemodefilter, 0, G_LIMIT, G_ALL, sv_rotatemodefilter &= ~G_NEVER, rotatemodefilter &= ~G_NEVER); // modes not in this array are filtered out
GVAR(IDF_ADMIN, rotatemuts, 0, 3, VAR_MAX); // any more than one decreases the chances of it picking
GVAR(IDF_ADMIN, rotatemutsfilter, 0, G_M_ROTATE, G_M_ALL); // mutators not in this array are filtered out
GVAR(IDF_ADMIN, rotatemapsfilter, 0, 2, 2); // 0 = off, 1 = filter based on mutators, 2 = also filter based on players
GVAR(IDF_ADMIN, rotatecycle, 0, 10, VAR_MAX); // 0 = off, else = minutes between forced re-cycles

GVAR(IDF_ADMIN, varslock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, votelock, 0, PRIV_OPERATOR, PRIV_MAX);
GVAR(IDF_ADMIN, votelocktype, 0, 2, 2); // 0 = off, 1 = lock level only, 2 = lock level can select previousmaps
GVAR(IDF_ADMIN, votewait, 0, 2500, VAR_MAX);
GVAR(IDF_ADMIN, votestyle, 0, 2, 2); // 0 = votes don't pass mid-match, 1 = passes if votethreshold is met, 2 = passes if unanimous
GVAR(IDF_ADMIN, voteinterm, 0, 2, 3); // 0 = must wait entire time, 1 = passes if votethreshold is met, 2 = passes if unanimous, 3 = passes after waiting then selects a random vote
GFVAR(IDF_ADMIN, votethreshold, 0, 0.5f, 1); // auto-pass votes when this many agree

GVAR(IDF_ADMIN, smallmapmax, 0, 6, VAR_MAX); // maximum number of players for a small map
GVAR(IDF_ADMIN, mediummapmax, 0, 12, VAR_MAX); // maximum number of players for a medium map

GVAR(IDF_ADMIN, waitforplayers, 0, 2, 2); // wait for players: 0 = off, 1 = to load the map, 2 = to exit spectator
GVAR(IDF_ADMIN, waitforplayermin, 0, 3000, VAR_MAX); // wait at least this long for players to meet waitforplayers criteria
GVAR(IDF_ADMIN, waitforplayertime, 0, 10000, VAR_MAX); // wait at most this long for players to meet waitforplayers criteria
GVAR(IDF_ADMIN, waitforplayerload, 0, 20000, VAR_MAX); // wait this long for players to load the map to schedule map requests
GVAR(IDF_ADMIN, waitforplayermaps, 0, 30000, VAR_MAX); // wait this long for sendmap and getmap requests

namespace server
{
    extern void resetgamevars(bool flush, bool all);
    extern void savegamevars();
}
GICOMMAND(0, resetvars, "", (), server::resetgamevars(true, false); result("success"), );
GICOMMAND(0, savevars, "", (), server::savegamevars(); result("success"), );
GICOMMAND(IDF_ADMIN, resetconfig, "", (), rehash(true); result("success"), );

GFVAR(0, maxalive, 0, 0, FVAR_MAX); // only allow this*maxplayers to be alive at once, 0 = turn off maxalive
GVAR(0, maxalivequeue, 0, 1, 1); // upon triggering maxalive, use a queue system
GVAR(0, maxaliveminimum, 2, 4, VAR_MAX); // kicks in if alive >= this
GFVAR(0, maxalivethreshold, 0, 0, FVAR_MAX); // .. or this percentage of clients

GVAR(0, maxcarry, 1, 2, W_LOADOUT);
GVAR(0, spawnrotate, 0, 2, 2); // 0 = let client decide, 1 = sequence, 2 = random
GVAR(0, spawnweapon, 0, W_PISTOL, W_MAX-1);
GVAR(0, instaweapon, 0, W_RIFLE, W_MAX-1);
GVAR(0, raceweapon, 0, W_MELEE, W_MAX-1);
GVAR(0, spawngrenades, 0, 0, 2); // 0 = never, 1 = all but insta/race, 2 = always
GVAR(0, spawnmines, 0, 0, 2); // 0 = never, 1 = all but insta/race, 2 = always
GVAR(0, spawndelay, 0, 5000, VAR_MAX); // delay before spawning in most modes
GVAR(0, instadelay, 0, 3000, VAR_MAX); // .. in instagib matches
GVAR(0, racedelay, 0, 1000, VAR_MAX); // .. in time race matches
GVAR(0, racedelayex, 0, 3000, VAR_MAX); // .. for defenders in gauntlet time race matches
GVAR(0, bomberdelay, 0, 3000, VAR_MAX); // delay before spawning in bomber
GVAR(0, spawnprotect, 0, 3000, VAR_MAX); // delay before damage can be dealt to spawning player
GVAR(0, duelprotect, 0, 3000, VAR_MAX); // .. in duel/survivor matches
GVAR(0, survivorprotect, 0, 5000, VAR_MAX); // .. in duel/survivor matches
GVAR(0, instaprotect, 0, 3000, VAR_MAX); // .. in instagib matches
GVAR(0, protectbreak, 0, 1, 1); // 0 = off, 1 = protection is broken when player starts firing

GVAR(0, radardisabled, 0, 0, 1); // forces the radar to be off
GVAR(0, radardistlimit, 0, 0, VAR_MAX); // forces the radar to this distance max, 0 = off

GVAR(0, balancemaps, -1, -1, 3); // determined if map team balancing is used: -1 = map default, 0 = off, 1 = ctf/dnc/bb, 2 = with team spawns, 3 = forced
GVAR(0, balancereset, 0, 2, 2); // reset players when balancing them, 0 = off, 1 = only when necessary, 2 = always
GVAR(0, balancedelay, 0, 5000, 30000); // before mapbalance forces
GVAR(0, balancenospawn, 0, 1, 1); // prevent respawning when waiting to balance
GVAR(0, balanceduke, 0, 1, 1); // enable in duel/survivor

GFVAR(0, maxhealth, 0, 1.5f, FVAR_MAX);
GFVAR(0, maxhealthvampire, 0, 3.0f, FVAR_MAX);
GFVAR(0, vampirescale, 0, 1.0f, FVAR_MAX);

GFVAR(0, maxresizescale, 1, 2, FVAR_MAX);
GFVAR(0, minresizescale, FVAR_NONZERO, 0.5f, 1);
GFVAR(0, instaresizeamt, FVAR_NONZERO, 0.1f, 1); // each kill adds this much size in insta-resize

GVAR(0, burntime, 0, 5500, VAR_MAX);
GVAR(0, burndelay, 0, 1000, VAR_MAX);
GVAR(0, burndamage, 0, 3, VAR_MAX);
GVAR(0, bleedtime, 0, 5500, VAR_MAX);
GVAR(0, bleeddelay, 0, 1000, VAR_MAX);
GVAR(0, bleeddamage, 0, 3, VAR_MAX);
GVAR(0, shocktime, 0, 5500, VAR_MAX);
GVAR(0, shockdelay, 0, 1000, VAR_MAX);
GVAR(0, shockdamage, 0, 2, VAR_MAX);
GVAR(0, shockstun, 0, W_N_ST, W_N_ALL);
GFVAR(0, shockstunscale, 0, 0.5f, FVAR_MAX);
GFVAR(0, shockstunfall, 0, 0.01f, FVAR_MAX);
GVAR(0, shockstuntime, 0, 500, VAR_MAX);

GVAR(0, regendelay, 0, 3000, VAR_MAX); // regen after no damage for this long
GVAR(0, regentime, 0, 1000, VAR_MAX); // regen this often when regenerating normally
GVAR(0, regenhealth, 0, 5, VAR_MAX); // regen this amount each regen
GVAR(0, regendecay, 0, 3, VAR_MAX); // if over maxhealth, decay this amount each regen

GVAR(0, kamikaze, 0, 1, 3); // 0 = never, 1 = holding grenade, 2 = have grenade, 3 = always
GVAR(0, itemspawntime, 1, 15000, VAR_MAX); // when items respawn
GVAR(0, itemspawndelay, 0, 0, VAR_MAX); // after map start items first spawn
GVAR(0, itemspawnstyle, 0, 0, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both
GVAR(0, itemcollide, 0, BOUNCE_GEOM, VAR_MAX);
GVAR(0, itemextinguish, 0, 6, 7);
GVAR(0, iteminteracts, 0, 3, 3);
GFVAR(0, itemelasticity, FVAR_MIN, 0.4f, FVAR_MAX);
GFVAR(0, itemrelativity, FVAR_MIN, 1, FVAR_MAX);
GFVAR(0, itemliquidcoast, 0, 1.75f, FVAR_MAX);
GFVAR(0, itemweight, FVAR_MIN, 150, FVAR_MAX);
GFVAR(0, itemspeedmin, 0, 0, FVAR_MAX);
GFVAR(0, itemspeedmax, 0, 0, FVAR_MAX);
GFVAR(0, itemrepulsion, 0, 8, FVAR_MAX);
GFVAR(0, itemrepelspeed, 0, 25, FVAR_MAX);

GVAR(0, timelimit, 0, 10, VAR_MAX); // maximum time a match may last, 0 = forever
GVAR(0, overtimeallow, 0, 1, 1); // if scores are equal, go into overtime
GVAR(0, overtimelimit, 0, 3, VAR_MAX); // maximum time overtime may last, 0 = forever
GVAR(0, intermlimit, 0, 10000, VAR_MAX); // .. before vote menu comes up
GVAR(0, votelimit, 0, 40000, VAR_MAX); // .. before vote passes by default

GVAR(0, duelreset, 0, 1, 1); // reset winner in duel
GVAR(0, duelclear, 0, 1, 1); // clear items in duel
GVAR(0, duelregen, 0, 0, 1); // allow regen in duel
GVAR(0, dueldelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(0, duelcooloff, 0, 3000, VAR_MAX); // cool off period before duel goes to next round
GVAR(0, duelcycle, 0, 2, 3); // determines if players are force-cycled after a certain number of wins (bit: 0 = off, 1 = non-team games, 2 = team games)
GVAR(0, duelcycles, 0, 2, VAR_MAX); // maximum wins in a row before force-cycling (0 = num team/total players)
GVAR(0, duelaffinity, 0, 1, 2); // 0 = off, 1 = on enter can respawn next iter, 2 = on enter can respawn immediately
GVAR(0, duelbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(0, duelovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime

GVAR(0, survivorreset, 0, 1, 1); // reset winners in survivor
GVAR(0, survivorclear, 0, 1, 1); // clear items in survivor
GVAR(0, survivorregen, 0, 0, 1); // allow regen in survivor
GVAR(0, survivordelay, 500, 1000, VAR_MAX); // round continues for this length after winning
GVAR(0, survivorcooloff, 0, 3000, VAR_MAX); // cool off period before survivor goes to next round
GVAR(0, survivoraffinity, 0, 0, 1); // 0 = off, 1 = on enter can spawn immediately
GVAR(0, survivorbotcheck, 0, 1, 1); // 0 = off, 1 = skip bots when checking respawns
GVAR(0, survivorovertime, 0, 1, 1); // 0 = off, 1 = ffa: only spawn leaders in overtime

GVAR(0, pointlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(0, teampersist, 0, 1, 2); // 0 = off, 1 = only attempt, 2 = forced
GVAR(0, damageself, 0, 1, 1); // 0 = off, 1 = either hurt self or use damageteam rules
GFVAR(0, damageselfscale, FVAR_MIN, 1, FVAR_MAX); // 0 = off, anything else = scale for damage
GVAR(0, damageteam, 0, 1, 2); // 0 = off, 1 = non-bots damage team, 2 = all players damage team
GFVAR(0, damageteamscale, FVAR_MIN, 1, FVAR_MAX); // 0 = off, anything else = scale for damage

GVAR(0, teambalance, 0, 5, 6); // 0 = off, 1 = by number then skill, 2 = by skill then number, 3 = by number and enforce, 4 = number, enforce, reassign, 5 = skill, number, enforce, reassign, 6 = skill during waiting, revert to 4 otherwise
GVAR(0, teambalanceduel, 0, 0, 1); // allow reassignments in duel
GVAR(0, teambalanceplaying, 2, 2, VAR_MAX); // min players before reassignments occur
GVAR(0, teambalanceamt, 2, 2, VAR_MAX); // max-min offset before reassignments occur
GVAR(0, teambalancewait, 0, 30000, VAR_MAX); // how long before can happen again
GVAR(0, teambalancedelay, 0, 5000, VAR_MAX); // how long before reassignments start
GVAR(0, teambalanceswap, 0, 1, 1); // allow swap requests if unable to change team
GVAR(0, teambalancelock, 0, PRIV_PLAYER, PRIV_MAX); // level at which one can override swap and automatically reassign a lower player
GVAR(0, teambalancestyle, 0, 4, 4); // when moving players, sort by: 0 = top of list, 1 = lowest time played, 2 = lowest points, 3 = lowest frags, 4 = lowest skill

GVAR(0, racegauntletwinner, 0, 1, 1); // declare the winner when the final team exceeds best score

GVAR(0, capturelimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(0, captureresetstore, 0, 2, 15);
GVAR(0, captureresetdelay, 0, 30000, VAR_MAX);
GVAR(0, capturedefenddelay, 0, 10000, VAR_MAX);
GVAR(0, captureprotectdelay, 0, 15000, VAR_MAX);
GVAR(0, capturepickupdelay, 500, 2500, VAR_MAX);
GVAR(0, captureteampenalty, -1, 30000, VAR_MAX);
GVAR(0, captureresetpenalty, -1, 5000, VAR_MAX);
GFVAR(0, capturecarryspeed, 0, 0.9f, FVAR_MAX);
GFVAR(0, capturedropheight, 0, 8, FVAR_MAX);
GVAR(0, capturepoints, 0, 5, VAR_MAX); // points added to score
GVAR(0, capturepickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(0, capturecollide, 0, BOUNCE_GEOM, VAR_MAX);
GVAR(0, captureextinguish, 0, 6, 7);
GVAR(0, captureinteracts, 0, 3, 3);
GFVAR(0, capturerelativity, 0, 0, FVAR_MAX);
GFVAR(0, captureelasticity, FVAR_MIN, 0.65f, FVAR_MAX);
GFVAR(0, captureliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(0, captureweight, FVAR_MIN, 400, FVAR_MAX);
GFVAR(0, capturespeedmin, 0, 0, FVAR_MAX);
GFVAR(0, capturespeedmax, 0, 100, FVAR_MAX);
GFVAR(0, capturerepulsion, 0, 16, FVAR_MAX);
GFVAR(0, capturerepelspeed, 0, 25, FVAR_MAX);
GFVAR(0, capturethreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(0, capturebuffing, 0, 9, 63); // buffed; 0 = off, &1 = when defending, &2 = when defending dropped, &4 = when secured, &8 = when defending secured, &16 = when secured enemy, &32 = when defending secured enemy
GVAR(0, capturebuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(0, capturebuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(0, capturebuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(0, capturebuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(0, captureregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(0, captureregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(0, captureregenextra, 0, 2, VAR_MAX); // add this to regen when buffed

GVAR(0, defendlimit, 0, 0, VAR_MAX); // finish when score is this or more
GVAR(0, defendpoints, 0, 1, VAR_MAX); // points added to score
GVAR(0, defendinterval, 0, 50, VAR_MAX);
GVAR(0, defendhold, 1, 100, VAR_MAX); // points needed to gain a score point
GVAR(0, defendoccupy, 1, 100, VAR_MAX); // points needed to occupy in regular games
GVAR(0, defendking, 1, 100, VAR_MAX); // points needed to occupy in king of the hill
GVAR(0, defendflags, 0, 3, 3); // 0 = init all (neutral), 1 = init neutral and team only, 2 = init team only, 3 = init all (team + neutral + converted)
GVAR(0, defendbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = even when enemies are present
GFVAR(0, defendbuffoccupy, 0, 0.5f, 1); // for defendbuffing&4, must be occupied this much before passing
GVAR(0, defendbuffdelay, 0, 1000, VAR_MAX); // buffed for this long after leaving
GFVAR(0, defendbuffarea, 0, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(0, defendbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(0, defendbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(0, defendregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(0, defendregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(0, defendregenextra, 0, 2, VAR_MAX); // add this to regen when buffed

GVAR(0, bomberlimit, 0, 0, VAR_MAX); // finish when score is this or more (non-hold)
GVAR(0, bomberholdlimit, 0, 0, VAR_MAX); // finish when score is this or more (hold)
GVAR(0, bomberbasketonly, 0, 1, 1); // prohibit touchdowns in basket game
GVAR(0, bomberattackreset, 0, 1, 1); // defenders reset rather than carry the ball
GVAR(0, bomberattackwinner, 0, 1, 1); // declare the winner when the final team exceeds best score
GFVAR(0, bomberbasketmindist, 0, 48, FVAR_MAX); // prohibit baskets less than this far away
GVAR(0, bomberwait, 0, 1000, VAR_MAX); // delay before bomb spawns
GVAR(0, bomberresetdelay, 0, 15000, VAR_MAX);
GVAR(0, bomberpickupdelay, 500, 15000, VAR_MAX);
GVAR(0, bombercarrytime, 0, 15000, VAR_MAX);
GFVAR(0, bombercarryspeed, 0, 0.9f, FVAR_MAX);
GFVAR(0, bomberdropheight, 0, 8, FVAR_MAX);
GVAR(0, bomberpoints, 0, 5, VAR_MAX); // points added to score
GVAR(0, bomberpenalty, 0, 5, VAR_MAX); // points taken from score
GVAR(0, bomberpickuppoints, 0, 3, VAR_MAX); // points added to score
GVAR(0, bomberholdtime, 0, 15000, VAR_MAX);
GVAR(0, bomberholdpoints, 0, 1, VAR_MAX); // points added to score
GVAR(0, bomberholdpenalty, 0, 10, VAR_MAX); // penalty for holding too long
GVAR(0, bomberholdinterval, 0, 1000, VAR_MAX);
GVAR(0, bomberlockondelay, 0, 250, VAR_MAX);
GFVAR(0, bomberspeed, 0, 250, FVAR_MAX);
GFVAR(0, bomberspeeddelta, 0, 1000, FVAR_MAX);
GFVAR(0, bomberspeedmin, 0, 65, FVAR_MAX);
GFVAR(0, bomberspeedmax, 0, 200, FVAR_MAX);
GVAR(0, bombercollide, 0, BOUNCE_GEOM, VAR_MAX);
GVAR(0, bomberextinguish, 0, 6, 7);
GVAR(0, bomberinteracts, 0, 3, 3);
GFVAR(0, bomberrelativity, 0, 0.25f, FVAR_MAX);
GFVAR(0, bomberelasticity, FVAR_MIN, 0.75f, FVAR_MAX);
GFVAR(0, bomberliquidcoast, FVAR_MIN, 1.75f, FVAR_MAX);
GFVAR(0, bomberweight, FVAR_MIN, 350, FVAR_MAX);
GFVAR(0, bomberthreshold, 0, 0, FVAR_MAX); // if someone 'warps' more than this distance, auto-drop
GVAR(0, bomberbuffing, 0, 1, 7); // buffed; 0 = off, &1 = when guarding, &2 = when securing, &4 = when secured as defender in attack
GVAR(0, bomberbuffdelay, 0, 3000, VAR_MAX); // buffed for this long after leaving
GFVAR(0, bomberbuffarea, FVAR_NONZERO, 160, FVAR_MAX); // radius in which buffing occurs
GFVAR(0, bomberbuffdamage, 1, 1.25f, FVAR_MAX); // multiply outgoing damage by this much when buffed
GFVAR(0, bomberbuffshield, 1, 1.25f, FVAR_MAX); // divide incoming damage by this much when buffed
GVAR(0, bomberregenbuff, 0, 1, 1); // 0 = off, 1 = modify regeneration when buffed
GVAR(0, bomberregendelay, 0, 1000, VAR_MAX); // regen this often when buffed
GVAR(0, bomberregenextra, 0, 2, VAR_MAX); // add this to regen when buffed

GVAR(IDF_ADMIN, airefreshdelay, 0, 1000, VAR_MAX);
GVAR(0, botbalance, -1, -1, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GFVAR(0, botbalancescale, FVAR_NONZERO, 0.75f, FVAR_MAX); // use balance*this
GVAR(0, botbalanceduel, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(0, botbalancesurvivor, -1, 2, VAR_MAX); // -1 = always use numplayers, 0 = don't balance, 1 or more = fill only with this many
GVAR(0, botskillmin, 1, 60, 101);
GVAR(0, botskillmax, 1, 75, 101);
GFVAR(0, botskillfrags, 0, 1, 100);
GFVAR(0, botskilldeaths, 0, 1, 100);
GVAR(0, botlimit, 0, 32, MAXAI);
GVAR(0, botoffset, VAR_MIN, 0, VAR_MAX);
GSVAR(IDF_ADMIN, botmalenames, "");
GSVAR(IDF_ADMIN, botfemalenames, "");
GSVAR(IDF_ADMIN, botmalevanities, "");
GSVAR(IDF_ADMIN, botfemalevanities, "");
GFVAR(0, botspeed, 0, 1, FVAR_MAX);
GFVAR(0, botscale, FVAR_NONZERO, 1, FVAR_MAX);
GFVAR(0, coopbalance, FVAR_NONZERO, 1.5f, FVAR_MAX);
GFVAR(0, coopmultibalance, FVAR_NONZERO, 2, FVAR_MAX);
GVAR(0, coopskillmin, 1, 75, 101);
GVAR(0, coopskillmax, 1, 85, 101);
GFVAR(0, coopskillfrags, 0, 1, 100);
GFVAR(0, coopskilldeaths, 0, 1.5f, 100);
GVAR(0, enemybalance, 1, 1, 3);
GVAR(0, enemyskillmin, 1, 65, 101);
GVAR(0, enemyskillmax, 1, 80, 101);
GVAR(0, enemylimit, 0, 32, MAXAI);
GVAR(0, enemyspawntime, 1, 30000, VAR_MAX); // when enemies respawn
GVAR(0, enemyspawndelay, 0, 1000, VAR_MAX); // after map start enemies first spawn
GVAR(0, enemyspawnstyle, 0, 1, 3); // 0 = all at once, 1 = staggered, 2 = random, 3 = randomise between both
GFVAR(0, enemyspeed, 0, 1, FVAR_MAX);
GFVAR(0, enemyscale, FVAR_NONZERO, 1, FVAR_MAX);
GFVAR(0, enemystrength, FVAR_NONZERO, 1, FVAR_MAX); // scale enemy health values by this much

GFVAR(0, movespeed, FVAR_NONZERO, 125, FVAR_MAX); // speed
GFVAR(0, moveslow, 0, 50, FVAR_MAX); // threshold for moving
GFVAR(0, movecrawl, 0, 0.6f, FVAR_MAX); // crawl modifier
GFVAR(0, moverun, FVAR_NONZERO, 1.3f, FVAR_MAX); // running modifier
GFVAR(0, movestraight, FVAR_NONZERO, 1.2f, FVAR_MAX); // non-strafe modifier
GFVAR(0, movestrafe, FVAR_NONZERO, 1, FVAR_MAX); // strafe modifier
GFVAR(0, moveinair, FVAR_NONZERO, 0.9f, FVAR_MAX); // in-air modifier
GFVAR(0, movestepup, FVAR_NONZERO, 0.95f, FVAR_MAX); // step-up modifier
GFVAR(0, movestepdown, FVAR_NONZERO, 1.15f, FVAR_MAX); // step-down modifier

GFVAR(0, jumpspeed, FVAR_NONZERO, 110, FVAR_MAX); // extra velocity to add when jumping
GFVAR(0, impulsespeed, FVAR_NONZERO, 90, FVAR_MAX); // extra velocity to add when impulsing
GFVAR(0, impulselimit, 0, 0, FVAR_MAX); // maximum impulse speed
GFVAR(0, impulseboost, 0, 1, FVAR_MAX); // thrust modifier
GFVAR(0, impulseboostredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulsepower, 0, 1.5f, FVAR_MAX); // power jump modifier
GFVAR(0, impulsepowerredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulsedash, 0, 1.3f, FVAR_MAX); // dashing/powerslide modifier
GFVAR(0, impulsedashredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulsejump, 0, 1.1f, FVAR_MAX); // jump modifier
GFVAR(0, impulsejumpredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulsemelee, 0, 0.75f, FVAR_MAX); // melee modifier
GFVAR(0, impulsemeleeredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulseparkour, 0, 1, FVAR_MAX); // parkour modifier
GFVAR(0, impulseparkourredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulseparkourkick, 0, 1.5f, FVAR_MAX); // parkour kick modifier
GFVAR(0, impulseparkourkickredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulseparkourclimb, 0, 1.4f, FVAR_MAX); // parkour climb modifier
GFVAR(0, impulseparkourclimbredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulseparkourvault, 0, 1.5f, FVAR_MAX); // parkour vault modifier
GFVAR(0, impulseparkourvaultredir, 0, 1, FVAR_MAX); // how much of the old velocity is redirected into the new one
GFVAR(0, impulseparkournorm, 0, 0.5f, FVAR_MAX); // minimum parkour surface z normal
GVAR(0, impulseallowed, 0, IM_A_ALL, IM_A_ALL); // impulse actions allowed; bit: 0 = off, 1 = dash, 2 = boost, 4 = parkour
GVAR(0, impulsestyle, 0, 1, 3); // impulse style; 0 = off, 1 = touch and count, 2 = count only, 3 = freestyle
GVAR(0, impulsecount, 0, 6, VAR_MAX); // number of impulse actions per air transit
GVAR(0, impulseslip, 0, 500, VAR_MAX); // time before floor friction kicks back in
GVAR(0, impulseslide, 0, 1000, VAR_MAX); // time before powerslides end
GVAR(0, impulsejumpdelay, 0, 100, VAR_MAX); // minimum time after jump for boost
GVAR(0, impulseboostdelay, 0, 250, VAR_MAX); // minimum time between boosts
GVAR(0, impulsedashdelay, 0, 500, VAR_MAX); // minimum time between dashes/powerslides
GVAR(0, impulsekickdelay, 0, 350, VAR_MAX); // minimum time between wall kicks/climbs
GFVAR(0, impulsevaultmin, FVAR_NONZERO, 0.25f, FVAR_MAX); // minimum percentage of height for vault
GFVAR(0, impulsevaultmax, FVAR_NONZERO, 1.f, FVAR_MAX); // maximum percentage of height for vault

GVAR(0, impulsemeter, 1, 30000, VAR_MAX); // impulse dash length; timer
GVAR(0, impulsecost, 1, 5000, VAR_MAX); // cost of impulse move
GVAR(0, impulsecostrelax, 0, IM_A_RELAX, IM_A_ALL); // whether the cost of an impulse move is unimportant; bit: 0 = off, 1 = dash, 2 = boost, 4 = parkour
GVAR(0, impulsecostscale, 0, 0, 1); // whether the cost scales depend on the amount the impulse scales
GVAR(0, impulseskate, 0, 1000, VAR_MAX); // length of time a run along a wall can last
GFVAR(0, impulseregen, 0, 5, FVAR_MAX); // impulse regen multiplier
GFVAR(0, impulseregencrouch, 0, 2.5f, FVAR_MAX); // impulse regen crouch modifier
GFVAR(0, impulseregenrun, 0, 0.75f, FVAR_MAX); // impulse regen running modifier
GFVAR(0, impulseregenmove, 0, 1, FVAR_MAX); // impulse regen moving modifier
GFVAR(0, impulseregeninair, 0, 0.75f, FVAR_MAX); // impulse regen in-air modifier
GFVAR(0, impulseregenslide, 0, 0, FVAR_MAX); // impulse regen sliding modifier
GVAR(0, impulseregendelay, 0, 350, VAR_MAX); // delay before impulse regens

GFVAR(0, spreadstill, 0, 0.25f, FVAR_MAX);
GFVAR(0, spreadmoving, 0, 1.f, FVAR_MAX);
GFVAR(0, spreadrunning, 0, 1.f, FVAR_MAX);
GFVAR(0, spreadinair, 0, 1.f, FVAR_MAX);

GVAR(0, quakefade, 0, 250, VAR_MAX);
GVAR(0, quakewobble, 1, 18, VAR_MAX);
GVAR(0, quakelimit, 0, 200, VAR_MAX);

GVAR(0, zoomlimitmin, 1, 10, 150);
GVAR(0, zoomlimitmax, 1, 60, 150);
GVAR(0, zoomtime, 1, 500, VAR_MAX);

GFVAR(0, radialscale, 0, 1, FVAR_MAX);
GFVAR(0, radiallimited, 0, 0.75f, FVAR_MAX);
GFVAR(0, damagescale, 0, 1, FVAR_MAX);
GVAR(0, weaponswitchdelay, 0, 400, VAR_MAX);

GFVAR(0, pushscale, 0, 1, FVAR_MAX);
GFVAR(0, pushlimited, 0, 0.75f, FVAR_MAX);
GFVAR(0, hitpushscale, 0, 1, FVAR_MAX);
GFVAR(0, hitvelscale, 0, 1, FVAR_MAX);
GFVAR(0, deadpushscale, 0, 2, FVAR_MAX);
GFVAR(0, wavepushscale, 0, 1, FVAR_MAX);

GFVAR(0, stunscale, 0, 1, FVAR_MAX);
GFVAR(0, stunlimited, 0, 0.75f, FVAR_MAX);
GFVAR(0, hitstunscale, 0, 1, FVAR_MAX);
GFVAR(0, deadstunscale, 0, 2, FVAR_MAX);
GFVAR(0, wavestunscale, 0, 1, FVAR_MAX);

GFVAR(0, kickpushscale, 0, 1, FVAR_MAX);
GFVAR(0, kickpushcrouch, 0, 0, FVAR_MAX);
GFVAR(0, kickpushsway, 0, 0.0125f, FVAR_MAX);
GFVAR(0, kickpushzoom, 0, 0.125f, FVAR_MAX);

GVAR(0, fragbonus, 0, 3, VAR_MAX);
GVAR(0, enemybonus, 0, 1, VAR_MAX);
GVAR(0, teamkillpenalty, 0, 2, VAR_MAX);
GVAR(0, firstbloodpoints, 0, 1, VAR_MAX);
GVAR(0, headshotpoints, 0, 1, VAR_MAX);

GVAR(0, assistkilldelay, 0, 5000, VAR_MAX);
GVAR(0, multikilldelay, 0, 5000, VAR_MAX);
GVAR(0, multikillpoints, 0, 1, VAR_MAX);
GVAR(0, multikillbonus, 0, 0, 1); // if bonus is on, then points are multiplied by the current kill mutliplier (x2, x3, x4)
GVAR(0, spreecount, 0, 5, VAR_MAX);
GVAR(0, spreepoints, 0, 1, VAR_MAX);
GVAR(0, spreebreaker, 0, 1, VAR_MAX);
GVAR(0, dominatecount, 0, 5, VAR_MAX);
GVAR(0, dominatepoints, 0, 1, VAR_MAX);
GVAR(0, revengepoints, 0, 1, VAR_MAX);

GSVAR(0, obitdestroyed, "was destroyed");
GSVAR(0, obitdied, "died");
GSVAR(0, obitfragged, "fragged");
GSVAR(0, obitspawn, "tried to spawn inside solid matter");
GSVAR(0, obitspectator, "gave up their corporeal form");
GSVAR(0, obitdrowned, "drowned");
GSVAR(0, obitmelted, "melted into a ball of fire");
GSVAR(0, obitdeathmat, "met their end");
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
