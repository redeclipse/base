#ifndef __GAME_H__
#define __GAME_H__

#include "engine.h"

#define VERSION_GAMEID "fps"
#define VERSION_GAME 230
#define VERSION_DEMOMAGIC "RED_ECLIPSE_DEMO"

#define MAXAI 256
#define MAXPLAYERS (MAXCLIENTS+MAXAI*2)
#define MAXPARAMS 256

// network quantization scale
#define DMF 16.0f // for world locations
#define DNF 1000.0f // for normalized vectors
#define DVELF 1.0f // for playerspeed based velocity vectors

enum
{
    S_JUMP = S_GAMESPECIFIC, S_IMPULSE, S_LAND, S_FOOTSTEP, S_SWIMSTEP, S_PAIN, S_DEATH,
    S_SPLASH1, S_SPLASH2, S_SPLOSH, S_DEBRIS, S_BURNLAVA,
    S_EXTINGUISH, S_SHELL, S_ITEMUSE, S_ITEMSPAWN,
    S_REGEN, S_DAMAGE, S_DAMAGE2, S_DAMAGE3, S_DAMAGE4, S_DAMAGE5, S_DAMAGE6, S_DAMAGE7, S_DAMAGE8,
    S_BURNED, S_BLEED, S_SHOCK, S_RESPAWN, S_CHAT, S_ERROR, S_ALARM, S_CATCH, S_BOUNCE,
    S_V_FLAGSECURED, S_V_FLAGOVERTHROWN, S_V_FLAGPICKUP, S_V_FLAGDROP, S_V_FLAGRETURN, S_V_FLAGSCORE, S_V_FLAGRESET,
    S_V_BOMBSTART, S_V_BOMBDUEL, S_V_BOMBPICKUP, S_V_BOMBSCORE, S_V_BOMBRESET,
    S_V_NOTIFY, S_V_FIGHT, S_V_START, S_V_CHECKPOINT, S_V_COMPLETE, S_V_OVERTIME, S_V_ONEMINUTE, S_V_HEADSHOT,
    S_V_SPREE, S_V_SPREE2, S_V_SPREE3, S_V_SPREE4, S_V_MULTI, S_V_MULTI2, S_V_MULTI3,
    S_V_REVENGE, S_V_DOMINATE, S_V_FIRSTBLOOD, S_V_BREAKER,
    S_V_YOUWIN, S_V_YOULOSE, S_V_DRAW, S_V_FRAGGED, S_V_BALWARN, S_V_BALALERT,
    S_GAME
};

enum                                // entity types
{
    NOTUSED = ET_EMPTY, LIGHT = ET_LIGHT, MAPMODEL = ET_MAPMODEL, PLAYERSTART = ET_PLAYERSTART, ENVMAP = ET_ENVMAP, PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND, LIGHTFX = ET_LIGHTFX, SUNLIGHT = ET_SUNLIGHT, WEAPON = ET_GAMESPECIFIC,
    TELEPORT, ACTOR, TRIGGER, PUSHER, AFFINITY, CHECKPOINT,
    ROUTE, UNUSEDENT,
    MAXENTTYPES
};

enum { EU_NONE = 0, EU_ITEM, EU_AUTO, EU_ACT, EU_MAX };

enum { TR_TOGGLE = 0, TR_LINK, TR_SCRIPT, TR_ONCE, TR_EXIT, TR_MAX };
enum { TA_MANUAL = 0, TA_AUTO, TA_ACTION, TA_MAX };
#define TRIGGERIDS      16
#define TRIGSTATE(a,b)  (b%2 ? !a : a)

enum { CP_RESPAWN = 0, CP_START, CP_FINISH, CP_LAST, CP_MAX, CP_ALL = (1<<CP_RESPAWN)|(1<<CP_START)|(1<<CP_FINISH)|(1<<CP_LAST) };

enum { TELE_NOAFFIN = 0, TELE_MAX };

struct enttypes
{
    int type,           priority, links,    radius, usetype,    numattrs,   modesattr,  idattr,
            canlink, reclink, canuse;
    bool    noisy,  syncs,  resyncs,    syncpos,    synckin;
    const char *name,           *attrs[13];
};
#ifdef GAMESERVER
enttypes enttype[] = {
    {
        NOTUSED,        -1,         0,      0,      EU_NONE,    0,          -1,         -1,
            0, 0, 0,
            true,   false,  false,      false,      false,
                "none",         { "" }
    },
    {
        LIGHT,          1,          59,     0,      EU_NONE,    6,          -1,         -1,
            (1<<LIGHTFX), (1<<LIGHTFX), 0,
            false,  false,  false,      false,      false,
                "light",        { "radius", "red",      "green",    "blue",     "flare",    "flarescale"  }
    },
    {
        MAPMODEL,       1,          58,     0,      EU_NONE,    13,         -1,         -1,
            (1<<TRIGGER), (1<<TRIGGER), 0,
            false,  false,  false,      false,      false,
                "mapmodel",     { "type",   "yaw",      "pitch",    "roll",     "blend",    "scale",    "flags",    "colour",   "palette",  "palindex", "spinyaw",  "spinpitch", "spinroll" }
    },
    {
        PLAYERSTART,    1,          59,     0,      EU_NONE,    6,          3,          5,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            0,
            false,  true,  false,      false,      false,
                "playerstart",  { "team",   "yaw",      "pitch",    "modes",    "muts",     "id" }
    },
    {
        ENVMAP,         1,          0,      0,      EU_NONE,    3,          -1,         -1,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "envmap",       { "radius", "size", "blur" }
    },
    {
        PARTICLES,      1,          59,     0,      EU_NONE,    12,         -1,         -1,
            (1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            (1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            0,
            false,  false,  false,      false,      false,
                "particles",    { "type",   "a",        "b",        "c",        "d",        "e",        "f",        "g",        "i",        "j",        "k",        "j" }
    },
    {
        MAPSOUND,       1,          58,     0,      EU_NONE,    5,          -1,         -1,
            (1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            (1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            0,
            false,  false,  false,      false,      false,
                "sound",        { "type",   "maxrad",   "minrad",   "volume",   "flags" }
    },
    {
        LIGHTFX,        1,          1,      0,      EU_NONE,    5,          -1,         -1,
            (1<<LIGHT)|(1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            (1<<LIGHT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            0,
            false,  false,  false,      false,      false,
                "lightfx",      { "type",   "mod",      "min",      "max",      "flags" }
    },
    {
        SUNLIGHT,       1,          160,    0,      EU_NONE,    8,          -1,         -1,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "sunlight",     { "yaw",    "pitch",    "red",      "green",    "blue",     "offset",   "flare",    "flarescale" }
    },
    {
        WEAPON,         2,          59,     24,     EU_ITEM,    5,          2,          4,
            0, 0,
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   true,      false,      false,
                "weapon",       { "type",   "flags",    "modes",    "muts",     "id" }
    },
    {
        TELEPORT,       1,          50,     12,     EU_AUTO,    9,          -1,         -1,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX)|(1<<TELEPORT),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI)|(1<<ENT_PROJ),
            false,  false,  false,      false,      false,
                "teleport",     { "yaw",    "pitch",    "push",     "radius",   "colour",   "type",     "palette",  "palindex", "flags" }
    },
    {
        ACTOR,          1,          59,     0,      EU_NONE,    10,         3,          5,
            0, 0, 0,
            false,  true,   false,      true,       false,
                "actor",        { "type",   "yaw",      "pitch",    "modes",    "muts",     "id",       "weap",     "health",   "speed",    "scale" }
    },
    {
        TRIGGER,        1,          58,     16,     EU_AUTO,    7,          5,          -1,
            (1<<MAPMODEL)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPMODEL)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   true,       false,      true,
                "trigger",      { "id",     "type",     "action",   "radius",   "state",    "modes",    "muts" }
    },
    {
        PUSHER,         1,          58,     12,     EU_AUTO,    6,          -1,         -1,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI)|(1<<ENT_PROJ),
            false,  false,  false,      false,      false,
                "pusher",       { "yaw",    "pitch",    "force",    "maxrad",   "minrad",   "type" }
    },
    {
        AFFINITY,       1,          48,     32,     EU_NONE,    6,          3,          5,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "affinity",     { "team",   "yaw",      "pitch",    "modes",    "muts",     "id" }
    },
    {
        CHECKPOINT,     1,          48,     16,     EU_AUTO,    7,          3,          5,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   false,      true,       false,
                "checkpoint",   { "radius", "yaw",      "pitch",    "modes",    "muts",     "id",       "type" }
    },
    {
        ROUTE,          1,         224,      16,    EU_NONE,    6,          -1,         -1,
            (1<<ROUTE), 0, 0,
            false,   false,  false,      false,      false,
                "route",         { "num",   "yaw",      "pitch",    "move",     "strafe",   "action" }
    },
    {
        UNUSEDENT,      -1,          0,      0,     EU_NONE,    0,          -1,         -1,
            0, 0, 0,
            true,   false,  false,      false,      false,
                "unused",         { "" }
    }
};
#else
extern enttypes enttype[];
#endif

#define MAXNAMELEN 24
enum { SAY_NONE = 0, SAY_ACTION = 1<<0, SAY_TEAM = 1<<1, SAY_WHISPER = 1<<2, SAY_NUM = 3 };

enum {
    PRIV_NONE = 0, PRIV_PLAYER, PRIV_SUPPORTER, PRIV_MODERATOR, PRIV_OPERATOR, PRIV_ADMINISTRATOR, PRIV_DEVELOPER, PRIV_CREATOR, PRIV_MAX,
    PRIV_START = PRIV_PLAYER, PRIV_ELEVATED = PRIV_MODERATOR, PRIV_LAST = PRIV_CREATOR, PRIV_TYPE = 0xFF, PRIV_LOCAL = 1<<8
};

#define MM_MODE 0xF
#define MM_AUTOAPPROVE 0x1000
#define MM_FREESERV (MM_AUTOAPPROVE|MM_MODE)
#define MM_VETOSERV ((1<<MM_OPEN)|(1<<MM_VETO))
#define MM_COOPSERV (MM_AUTOAPPROVE|MM_VETOSERV|(1<<MM_LOCKED))
#define MM_OPENSERV (MM_AUTOAPPROVE|(1<<MM_OPEN))

enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD };
enum { SINFO_NONE = 0, SINFO_STATUS, SINFO_NAME, SINFO_PORT, SINFO_QPORT, SINFO_DESC, SINFO_MODE, SINFO_MUTS, SINFO_MAP, SINFO_TIME, SINFO_NUMPLRS, SINFO_MAXPLRS, SINFO_PING, SINFO_PRIO, SINFO_MAX };
enum { SSTAT_OPEN = 0, SSTAT_LOCKED, SSTAT_PRIVATE, SSTAT_FULL, SSTAT_UNKNOWN, SSTAT_MAX };

enum {
    AC_PRIMARY = 0, AC_SECONDARY, AC_RELOAD, AC_USE, AC_JUMP, AC_WALK, AC_CROUCH, AC_SPECIAL, AC_DROP, AC_AFFINITY, AC_TOTAL, AC_DASH = AC_TOTAL, AC_MAX,
    AC_ALL = (1<<AC_PRIMARY)|(1<<AC_SECONDARY)|(1<<AC_RELOAD)|(1<<AC_USE)|(1<<AC_JUMP)|(1<<AC_WALK)|(1<<AC_CROUCH)|(1<<AC_SPECIAL)|(1<<AC_DROP)|(1<<AC_AFFINITY)
};
enum { IM_METER = 0, IM_TYPE, IM_TIME, IM_REGEN, IM_COUNT, IM_COLLECT, IM_SLIP, IM_SLIDE, IM_JUMP, IM_MAX };
enum { IM_T_NONE = 0, IM_T_BOOST, IM_T_DASH, IM_T_MELEE, IM_T_KICK, IM_T_VAULT, IM_T_GRAB, IM_T_SKATE, IM_T_MAX, IM_T_WALL = IM_T_MELEE };
enum { SPHY_NONE = 0, SPHY_JUMP, SPHY_BOOST, SPHY_DASH, SPHY_MELEE, SPHY_KICK, SPHY_VAULT, SPHY_GRAB, SPHY_SKATE, SPHY_COOK, SPHY_MATERIAL, SPHY_EXTINGUISH, SPHY_BUFF, SPHY_MAX, SPHY_SERVER = SPHY_EXTINGUISH };

#define CROUCHHEIGHT 0.7f
#define PHYSMILLIS 250

enum
{
    ANIM_PAIN = ANIM_GAMESPECIFIC,
    ANIM_JUMP_FORWARD, ANIM_JUMP_BACKWARD, ANIM_JUMP_LEFT, ANIM_JUMP_RIGHT, ANIM_JUMP,
    ANIM_IMPULSE_FORWARD, ANIM_IMPULSE_BACKWARD, ANIM_IMPULSE_LEFT, ANIM_IMPULSE_RIGHT,
    ANIM_DASH_FORWARD, ANIM_DASH_BACKWARD, ANIM_DASH_LEFT, ANIM_DASH_RIGHT, ANIM_DASH_UP,
    ANIM_WALL_RUN_LEFT, ANIM_WALL_RUN_RIGHT, ANIM_WALL_JUMP, ANIM_POWERSLIDE, ANIM_FLYKICK,
    ANIM_SINK, ANIM_EDIT, ANIM_WIN, ANIM_LOSE,
    ANIM_CROUCH, ANIM_CRAWL_FORWARD, ANIM_CRAWL_BACKWARD, ANIM_CRAWL_LEFT, ANIM_CRAWL_RIGHT,
    ANIM_CROUCH_JUMP_FORWARD, ANIM_CROUCH_JUMP_BACKWARD, ANIM_CROUCH_JUMP_LEFT, ANIM_CROUCH_JUMP_RIGHT, ANIM_CROUCH_JUMP,
    ANIM_CLAW, ANIM_CLAW_PRIMARY, ANIM_CLAW_SECONDARY,
    ANIM_PISTOL, ANIM_PISTOL_PRIMARY, ANIM_PISTOL_SECONDARY, ANIM_PISTOL_RELOAD,
    ANIM_SWORD, ANIM_SWORD_PRIMARY, ANIM_SWORD_SECONDARY,
    ANIM_SHOTGUN, ANIM_SHOTGUN_PRIMARY, ANIM_SHOTGUN_SECONDARY, ANIM_SHOTGUN_RELOAD,
    ANIM_SMG, ANIM_SMG_PRIMARY, ANIM_SMG_SECONDARY, ANIM_SMG_RELOAD,
    ANIM_FLAMER, ANIM_FLAMER_PRIMARY, ANIM_FLAMER_SECONDARY, ANIM_FLAMER_RELOAD,
    ANIM_PLASMA, ANIM_PLASMA_PRIMARY, ANIM_PLASMA_SECONDARY, ANIM_PLASMA_RELOAD,
    ANIM_ZAPPER, ANIM_ZAPPER_PRIMARY, ANIM_ZAPPER_SECONDARY, ANIM_ZAPPER_RELOAD,
    ANIM_RIFLE, ANIM_RIFLE_PRIMARY, ANIM_RIFLE_SECONDARY, ANIM_RIFLE_RELOAD,
    ANIM_GRENADE, ANIM_GRENADE_PRIMARY, ANIM_GRENADE_SECONDARY, ANIM_GRENADE_RELOAD, ANIM_GRENADE_POWER,
    ANIM_MINE, ANIM_MINE_PRIMARY, ANIM_MINE_SECONDARY, ANIM_MINE_RELOAD,
    ANIM_ROCKET, ANIM_ROCKET_PRIMARY, ANIM_ROCKET_SECONDARY, ANIM_ROCKET_RELOAD,
    ANIM_SWITCH, ANIM_USE,
    ANIM_MAX
};

enum { PULSE_FIRE = 0, PULSE_BURN, PULSE_DISCO, PULSE_SHOCK, PULSE_BLEED, PULSE_MAX, PULSE_LAST = PULSE_MAX-1 };
#define PULSECOLOURS 8
#define PULSE(x) (PULSE_##x)
#define INVPULSE(x) (-1-(x))
#define PC(x) (INVPULSE(PULSE(x)))
#ifdef GAMESERVER
int pulsecols[PULSE_MAX][PULSECOLOURS] = {
    { 0xFF5808, 0x981808, 0x782808, 0x481808, 0x983818, 0x681808, 0xC81808, 0x381808 },
    { 0xFFC848, 0xF86838, 0xA85828, 0xA84838, 0xF8A858, 0xC84828, 0xF86848, 0xA89858 },
    { 0xFF8888, 0xFFAA88, 0xFFFF88, 0x88FF88, 0x88FFFF, 0x8888FF, 0xFF88FF, 0xFFFFFF },
    { 0xAA88FF, 0xAA88FF, 0xAAAAFF, 0x44AAFF, 0x88AAFF, 0x4444FF, 0xAA44FF, 0xFFFFFF },
    { 0xFF0000, 0xFF2222, 0xFF0022, 0xFF2200, 0x880000, 0x882222, 0x880022, 0x882200 }
};
#else
extern int pulsecols[PULSE_MAX][PULSECOLOURS];
#endif

enum
{
    FRAG_NONE = 0, FRAG_HEADSHOT = 1<<1, FRAG_OBLITERATE = 1<<2,
    FRAG_SPREE1 = 1<<3, FRAG_SPREE2 = 1<<4, FRAG_SPREE3 = 1<<5, FRAG_SPREE4 = 1<<6,
    FRAG_MKILL1 = 1<<7, FRAG_MKILL2 = 1<<8, FRAG_MKILL3 = 1<<9,
    FRAG_REVENGE = 1<<10, FRAG_DOMINATE = 1<<11, FRAG_FIRSTBLOOD = 1<<12, FRAG_BREAKER = 1<<13,
    FRAG_SPREES = 4, FRAG_SPREE = 3, FRAG_MKILL = 7,
    FRAG_CHECK = FRAG_SPREE1|FRAG_SPREE2|FRAG_SPREE3|FRAG_SPREE4,
    FRAG_MULTI = FRAG_MKILL1|FRAG_MKILL2|FRAG_MKILL3,
};

enum {
    SENDMAP_MPZ = 0, SENDMAP_CFG, SENDMAP_PNG, SENDMAP_TXT, SENDMAP_GAME, SENDMAP_WPT = SENDMAP_GAME, SENDMAP_MAX,
    SENDMAP_MIN = SENDMAP_PNG, SENDMAP_HAS = SENDMAP_MIN+1, SENDMAP_ALL = SENDMAP_MAX-1
};
#ifdef GAMESERVER
const char *sendmaptypes[SENDMAP_MAX] = { "mpz", "cfg", "png", "txt", "wpt" };
#else
extern const char *sendmaptypes[SENDMAP_MAX];
#endif

#include "gamemode.h"
#include "weapons.h"

// network messages codes, c2s, c2c, s2c
enum
{
    N_CONNECT = 0, N_SERVERINIT, N_WELCOME, N_CLIENTINIT, N_POS, N_SPHY, N_TEXT, N_COMMAND, N_ANNOUNCE, N_DISCONNECT,
    N_SHOOT, N_DESTROY, N_STICKY, N_SUICIDE, N_DIED, N_POINTS, N_TOTALS, N_DAMAGE, N_SHOTFX,
    N_LOADW, N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_DROP, N_WSELECT,
    N_MAPCHANGE, N_MAPVOTE, N_CLEARVOTE, N_CHECKPOINT, N_ITEMSPAWN, N_ITEMUSE, N_TRIGGER, N_EXECLINK,
    N_PING, N_PONG, N_CLIENTPING, N_TICK, N_ITEMACC, N_SERVMSG, N_GETGAMEINFO, N_GAMEINFO, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITLINK, N_EDITVAR, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_REMIP, N_EDITVSLOT, N_UNDO, N_REDO, N_CLIPBOARD, N_NEWMAP,
    N_GETMAP, N_SENDMAP, N_FAILMAP, N_SENDMAPFILE,
    N_MASTERMODE, N_ADDCONTROL, N_CLRCONTROL, N_CURRENTPRIV, N_SPECTATOR, N_WAITING, N_SETPRIV, N_SETTEAM, N_ADDPRIV,
    N_SETUPAFFIN, N_INFOAFFIN, N_MOVEAFFIN,
    N_TAKEAFFIN, N_RETURNAFFIN, N_RESETAFFIN, N_DROPAFFIN, N_SCOREAFFIN, N_INITAFFIN, N_SCORE,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO, N_DEMOREADY,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_CLIENT, N_RELOAD, N_REGEN, N_INITAI, N_MAPCRC,
    N_SETPLAYERINFO, N_SWITCHTEAM, N_AUTHTRY, N_AUTHCHAL, N_AUTHANS, N_QUEUEPOS,
    NUMMSG
};

#ifdef GAMESERVER
char msgsizelookup(int msg)
{
    static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
    {
        N_CONNECT, 0, N_SERVERINIT, 5, N_WELCOME, 2, N_CLIENTINIT, 0, N_POS, 0, N_SPHY, 0, N_TEXT, 0, N_COMMAND, 0, N_ANNOUNCE, 0, N_DISCONNECT, 3,
        N_SHOOT, 0, N_DESTROY, 0, N_STICKY, 0, N_SUICIDE, 4, N_DIED, 0, N_POINTS, 4, N_TOTALS, 0, N_DAMAGE, 14, N_SHOTFX, 0,
        N_LOADW, 0, N_TRYSPAWN, 2, N_SPAWNSTATE, 0, N_SPAWN, 0, N_DROP, 0, N_WSELECT, 0,
        N_MAPCHANGE, 0, N_MAPVOTE, 0, N_CLEARVOTE, 0, N_CHECKPOINT, 0, N_ITEMSPAWN, 3, N_ITEMUSE, 0, N_TRIGGER, 0, N_EXECLINK, 3,
        N_PING, 2, N_PONG, 2, N_CLIENTPING, 2, N_TICK, 3, N_ITEMACC, 0, N_SERVMSG, 0, N_GETGAMEINFO, 0, N_GAMEINFO, 0, N_RESUME, 0,
        N_EDITMODE, 2, N_EDITENT, 0, N_EDITLINK, 4, N_EDITVAR, 0, N_EDITF, 16, N_EDITT, 16, N_EDITM, 17, N_FLIP, 14,
        N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_REMIP, 1, N_EDITVSLOT, 16, N_UNDO, 0, N_REDO, 0, N_NEWMAP, 0,
        N_GETMAP, 0, N_SENDMAP, 0, N_FAILMAP, 0, N_SENDMAPFILE, 0,
        N_MASTERMODE, 0, N_ADDCONTROL, 0, N_CLRCONTROL, 2, N_CURRENTPRIV, 3, N_SPECTATOR, 3, N_WAITING, 2, N_SETPRIV, 0, N_SETTEAM, 0, N_ADDPRIV, 0,
        N_SETUPAFFIN, 0, N_INFOAFFIN, 0, N_MOVEAFFIN, 0,
        N_DROPAFFIN, 0, N_SCOREAFFIN, 0, N_RETURNAFFIN, 0, N_TAKEAFFIN, 0, N_RESETAFFIN, 0, N_INITAFFIN, 0, N_SCORE, 0,
        N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 3, N_SENDDEMO, 0, N_DEMOREADY, 0,
        N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
        N_CLIENT, 0, N_RELOAD, 0, N_REGEN, 0, N_INITAI, 0, N_MAPCRC, 0,
        N_SETPLAYERINFO, 0, N_SWITCHTEAM, 0, N_AUTHTRY, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_QUEUEPOS, 0,
        -1
    };
    static int sizetable[NUMMSG] = { -1 };
    if(sizetable[0] < 0)
    {
        memset(sizetable, -1, sizeof(sizetable));
        for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
    }
    return msg >= 0 && msg < NUMMSG ? sizetable[msg] : -1;
}
#else
extern char msgsizelookup(int msg);
#endif
enum { CON_CHAT = CON_GAMESPECIFIC, CON_EVENT, CON_MAX, CON_LO = CON_MESG, CON_HI = CON_SELF, CON_IMPORTANT = CON_SELF };

struct demoheader
{
    char magic[16];
    int gamever, gamemode, mutators, starttime;
    string mapname;
};
#include "player.h"
#include "vars.h"
#ifndef GAMESERVER
#include "ai.h"
#endif

template<class T>
static inline void adjustscaled(T &n, int s)
{
    if(n > 0)
    {
        n = (T)(n/(1.f+sqrtf((float)curtime)/float(s)));
        if(n <= 0) n = (T)0;
    }
    else if(n < 0)
    {
        n = (T)(n/(1.f+sqrtf((float)curtime)/float(s)));
        if(n >= 0) n = (T)0;
    }
}

static inline void modecheck(int &mode, int &muts, int trying = 0)
{
    if(!m_game(mode)) mode = G_DEATHMATCH;
    if(gametype[mode].implied) muts |= gametype[mode].implied;
    int retries = G_M_NUM*G_M_NUM;
    loop(r, retries)
    {
        if(!muts) break; // nothing to do then
        bool changed = false;
        int mutsidx = gametype[mode].mutators[0];
        loopj(G_M_GSN) if(gametype[mode].mutators[j+1])
        {
            int m = 1<<(j+G_M_GSP);
            if(!(muts&m)) continue;
            if(trying&m) loopi(G_M_GSN) if(i != j && gametype[mode].mutators[i+1])
            {
                int n = 1<<(i+G_M_GSP);
                if(!(muts&n)) continue;
                if(!(gametype[mode].mutators[i+1]&m))
                {
                    muts &= ~n;
                    trying &= ~n;
                    changed = true;
                    break;
                }
            }
            if(changed) break;
            if(gametype[mode].flags&(1<<G_F_GSP))
            {
                //trying |= m; // game specific mutator exclusively provides allowed bits
                mutsidx = gametype[mode].mutators[j+1];
            }
        }
        if(changed) continue;
        loop(s, G_M_NUM)
        {
            if(!(mutsidx&(1<<mutstype[s].type)) && (muts&(1<<mutstype[s].type)))
            {
                muts &= ~(1<<mutstype[s].type);
                trying &= ~(1<<mutstype[s].type);
                changed = true;
                break;
            }
            if(muts&(1<<mutstype[s].type)) loopj(G_M_NUM)
            {
                if(!(mutstype[s].mutators&(1<<mutstype[j].type)) && (muts&(1<<mutstype[j].type)))
                {
                    if(trying && (trying&(1<<mutstype[j].type)) && !(gametype[mode].implied&(1<<mutstype[s].type)))
                    {
                        muts &= ~(1<<mutstype[s].type);
                        trying &= ~(1<<mutstype[s].type);
                    }
                    else
                    {
                        muts &= ~(1<<mutstype[j].type);
                        trying &= ~(1<<mutstype[j].type);
                    }
                    changed = true;
                    break;
                }
                int implying = gametype[mode].implied|mutstype[s].implied;
                if(implying && (implying&(1<<mutstype[j].type)) && !(muts&(1<<mutstype[j].type)))
                {
                    muts |= (1<<mutstype[j].type);
                    changed = true;
                    break;
                }
            }
            if(changed) break;
        }
        if(!changed) break;
    }
}

static inline const char *mastermodename(int type)
{
    switch(type)
    {
        case MM_OPEN: return "open";
        case MM_VETO: return "veto";
        case MM_LOCKED: return "locked";
        case MM_PRIVATE: return "private";
        case MM_PASSWORD: return "password";
        default: return "unknown";
    }
}

struct verinfo
{
    int type, flag, version, major, minor, patch, game, platform, arch, gpuglver, gpuglslver, crc;
    char *branch, *gpuvendor, *gpurenderer, *gpuversion;

    verinfo() : branch(NULL), gpuvendor(NULL), gpurenderer(NULL), gpuversion(NULL) { reset(); }
    ~verinfo() { reset(); }

    void reset()
    {
        if(branch) delete[] branch;
        if(gpuvendor) delete[] gpuvendor;
        if(gpurenderer) delete[] gpurenderer;
        if(gpuversion) delete[] gpuversion;
        branch = gpuvendor = gpurenderer = gpuversion = NULL;
        type = flag = version = major = minor = patch = game = arch = gpuglver = gpuglslver = crc = 0;
        platform = -1;
    }

    template <class T>
    void get(T &p)
    {
        string text;
        major = getint(p);
        minor = getint(p);
        patch = getint(p);
        game = getint(p);
        platform = getint(p);
        arch = getint(p);
        gpuglver = getint(p);
        gpuglslver = getint(p);
        crc = getint(p);
        if(branch) delete[] branch;
        getstring(text, p); branch = newstring(text);
        if(gpuvendor) delete[] gpuvendor;
        getstring(text, p); gpuvendor = newstring(text);
        if(gpurenderer) delete[] gpurenderer;
        getstring(text, p); gpurenderer = newstring(text);
        if(gpuversion) delete[] gpuversion;
        getstring(text, p); gpuversion = newstring(text);
    }

    template <class T>
    void put(T &p)
    {
        putint(p, major);
        putint(p, minor);
        putint(p, patch);
        putint(p, game);
        putint(p, platform);
        putint(p, arch);
        putint(p, gpuglver);
        putint(p, gpuglslver);
        putint(p, crc);
        sendstring(branch ? branch : "", p);
        sendstring(gpuvendor ? gpuvendor : "", p);
        sendstring(gpurenderer ? gpurenderer : "", p);
        sendstring(gpuversion ? gpuversion : "", p);
    }

    void grab(verinfo &v)
    {
        major = v.major;
        minor = v.minor;
        patch = v.patch;
        game = v.game;
        platform = v.platform;
        arch = v.arch;
        gpuglver = v.gpuglver;
        gpuglslver = v.gpuglslver;
        crc = v.crc;
        if(branch) delete[] branch;
        branch = newstring(v.branch ? v.branch : "");
        if(gpuvendor) delete[] gpuvendor;
        gpuvendor = newstring(v.gpuvendor ? v.gpuvendor : "");
        if(gpurenderer) delete[] gpurenderer;
        gpurenderer = newstring(v.gpurenderer ? v.gpurenderer : "");
        if(gpuversion) delete[] gpuversion;
        gpuversion = newstring(v.gpuversion ? v.gpuversion : "");
    }
};

// inherited by gameent and server clients
struct clientstate
{
    int health, ammo[W_MAX], entid[W_MAX], colour, model, checkpointspawn;
    int weapselect, weapload[W_MAX], weapshot[W_MAX], weapstate[W_MAX], weapwait[W_MAX], weaptime[W_MAX], prevstate[W_MAX], prevtime[W_MAX];
    int lastdeath, lastspawn, lastpain, lastregen, lastregenamt, lastbuff, lastshoot, lastres[WR_MAX], lastrestime[WR_MAX];
    int actortype, spawnpoint, ownernum, skill, points, frags, deaths, totalpoints, totalfrags, totaldeaths, spree, lasttimeplayed, timeplayed, cpmillis, cptime, queuepos;
    bool quarantine;
    string vanity;
    vector<int> loadweap, lastweap, randweap;
    verinfo version;

    clientstate() : colour(0), model(0), checkpointspawn(1), weapselect(W_CLAW), lastdeath(0), lastspawn(0), lastpain(0), lastregen(0), lastregenamt(0), lastbuff(0), lastshoot(0),
        actortype(A_PLAYER), spawnpoint(-1), ownernum(-1), skill(0), points(0), frags(0), deaths(0), totalpoints(0), totalfrags(0), totaldeaths(0), spree(0), lasttimeplayed(0), timeplayed(0),
        cpmillis(0), cptime(0), queuepos(-1), quarantine(false)
    {
        vanity[0] = '\0';
        loadweap.shrink(0);
        lastweap.shrink(0);
        randweap.shrink(0);
        resetresidual();
    }
    ~clientstate() {}

    bool setvanity(const char *v)
    {
        bool changed = strcmp(v, vanity);
        if(changed) copystring(vanity, v);
        return changed;
    }

    int getammo(int weap, int millis = 0)
    {
        if(!isweap(weap)) return 0;
        int a = ammo[weap];
        if(millis && weapstate[weap] == W_S_RELOAD && millis-weaptime[weap] < weapwait[weap] && weapload[weap] > 0)
            a -= weapload[weap];
        return a;
    }

    bool hasweap(int weap, int sweap, int level = 0, int exclude = -1)
    {
        if(isweap(weap) && weap != exclude)
        {
            if(ammo[weap] > 0 || (canreload(weap, sweap) && !ammo[weap])) switch(level)
            {
                case 0: default: return true; break; // has weap at all
                case 1: if(w_carry(weap, sweap)) return true; break; // only carriable
                case 2: if(ammo[weap] > 0) return true; break; // only with actual ammo
                case 3: if(ammo[weap] > 0 && canreload(weap, sweap)) return true; break; // only reloadable with actual ammo
                case 4: if(ammo[weap] >= (canreload(weap, sweap) ? 0 : W(weap, ammomax))) return true; break; // only reloadable or those with < max
                case 5: if(weap != sweap && weap >= W_ITEM) return true; break; // special case to determine drop in loadout games
                case 6: if(weap != sweap && weap >= W_OFFSET) return true; break; // special case to determine drop in classic games
            }
        }
        return false;
    }

    bool holdweap(int weap, int sweap, int millis)
    {
        return weap == weapselect || millis-weaptime[weap] < weapwait[weap] || hasweap(weap, sweap);
    }

    void addlastweap(int weap)
    {
        lastweap.add(weap);
        if(lastweap.length() >= W_ALL) lastweap.remove(0);
    }

    int getlastweap(int sweap, int exclude = -1)
    {
        loopvrev(lastweap)
        {
            if(lastweap[i] == exclude) continue;
            else if(hasweap(lastweap[i], sweap)) return lastweap[i];
        }
        return -1;
    }

    int bestweap(int sweap, bool last = false)
    {
        if(last)
        {
            int w = getlastweap(sweap);
            if(hasweap(w, sweap)) return w;
        }
        loopirev(W_ALL) if(hasweap(i, sweap, 3)) return i; // reloadable first
        loopirev(W_ALL) if(hasweap(i, sweap, 1)) return i; // carriable second
        loopirev(W_ALL) if(hasweap(i, sweap, 0)) return i; // any just to bail us out
        return weapselect;
    }

    int carry(int sweap, int level = 1, int exclude = -1)
    {
        int carry = 0;
        loopi(W_ALL) if(hasweap(i, sweap, level, exclude)) carry++;
        return carry;
    }

    int drop(int sweap)
    {
        if(hasweap(weapselect, sweap, 1)) return weapselect;
        int w = getlastweap(sweap, weapselect);
        if(hasweap(w, sweap, 1)) return w;
        loopi(W_ALL) if(hasweap(i, sweap, 1)) return i;
        return -1;
    }

    void weapreset(bool full = false)
    {
        loopi(W_MAX)
        {
            weapstate[i] = prevstate[i] = W_S_IDLE;
            weapwait[i] = weaptime[i] = weapload[i] = weapshot[i] = prevtime[0] = 0;
            if(full) ammo[i] = entid[i] = -1;
        }
        lastweap.shrink(0);
    }

    void setweapstate(int weap, int state, int delay, int millis, int offtime = 0, bool blank = false)
    {
        if(blank || (state >= W_S_RELOAD && state <= W_S_USE))
        {
            prevstate[weap] = W_S_IDLE;
            prevtime[weap] = millis;
        }
        else if(weapstate[weap] == W_S_ZOOM || weapstate[weap] == W_S_POWER)
        {
            prevstate[weap] = weapstate[weap];
            prevtime[weap] = weaptime[weap];
        }
        weapstate[weap] = state;
        weapwait[weap] = delay;
        weaptime[weap] = millis-offtime;
    }

    bool weapswitch(int weap, int millis, int delay = 0, int state = W_S_SWITCH)
    {
        if(isweap(weap) && weap < W_ALL)
        {
            if(isweap(weapselect))
            {
                lastweap.add(weapselect);
                setweapstate(weapselect, W_S_SWITCH, delay, millis);
            }
            weapselect = weap;
            setweapstate(weap, state, delay, millis);
            return true;
        }
        return false;
    }

    bool weapwaited(int weap, int millis, int skip = 0)
    {
        if(weap != weapselect) skip &= ~(1<<W_S_RELOAD);
        if(!weapwait[weap] || W_S_EXCLUDE&(1<<weapstate[weap]) || (skip && skip&(1<<weapstate[weap]))) return true;
        return millis-weaptime[weap] >= weapwait[weap];
    }

    bool candrop(int weap, int sweap, int millis, bool load, int skip = 0)
    {
        if(hasweap(weap, sweap, load ? 5 : 6) && weapwaited(weap, millis, skip) && weapwaited(weapselect, millis, skip))
            return true;
        return false;
    }

    bool canswitch(int weap, int sweap, int millis, int skip = 0)
    {
        if(!isweap(weap) || weap >= W_ALL) return false;
        if(weap != weapselect && weapwaited(weapselect, millis, skip) && hasweap(weap, sweap) && weapwaited(weap, millis, skip))
            return true;
        return false;
    }

    bool canshoot(int weap, int flags, int sweap, int millis, int skip = 0)
    {
        if(!(AA(actortype, abilities)&(1<<(WS(flags) ? A_A_SECONDARY : A_A_PRIMARY)))) return false;
        if(weap == weapselect || weap == W_MELEE)
            if(hasweap(weap, sweap) && getammo(weap, millis) >= (W2(weap, cooktime, WS(flags)) ? 1 : W2(weap, ammosub, WS(flags))) && weapwaited(weap, millis, skip))
                return true;
        return false;
    }

    bool canreload(int weap, int sweap, bool check = true, int millis = 0, int skip = 0)
    {
        if(weapstate[weap] != W_S_ZOOM && (check || (weap == weapselect && hasweap(weap, sweap) && ammo[weap] < W(weap, ammomax) && weapwaited(weap, millis, skip))))
            return w_reload(weap, sweap);
        return false;
    }

    bool canuse(int type, int attr, attrvector &attrs, int sweap, int millis, int skip = 0)
    {
        switch(enttype[type].usetype)
        {
            case EU_AUTO: case EU_ACT: return true; break;
            case EU_ITEM:
            { // can't use when reloading or firing
                if(type != WEAPON || !isweap(attr) || !AA(actortype, maxcarry)) return false;
                if(!hasweap(attr, sweap, 4) && weapwaited(weapselect, millis, skip))
                    return true;
                break;
            }
            default: break;
        }
        return false;
    }

    void useitem(int id, int type, int attr, int ammoamt, int sweap, int millis, int delay)
    {
        if(type != WEAPON || !isweap(attr)) return;
        int prev = max(ammo[attr], 0), ammoval = ammoamt >= 0 ? ammoamt : WUSE(attr);
        weapswitch(attr, millis, delay, W_S_USE);
        ammo[attr] = clamp(prev+ammoval, 0, W(attr, ammomax));
        weapload[attr] = ammo[attr]-prev;
        entid[attr] = id;
    }

    void resetresidual(int n = -1)
    {
        if(n >= 0 && n < WR_MAX) lastres[n] = lastrestime[n] = 0;
        else loopi(WR_MAX) lastres[i] = lastrestime[i] = 0;
    }

    void clearstate()
    {
        spree = lastdeath = lastpain = lastregen = lastregenamt = lastbuff = lastshoot = 0;
        queuepos = -1;
        resetresidual();
    }

    void mapchange(bool change = false)
    {
        points = frags = deaths = cpmillis = cptime = spree = 0;
    }

    void respawn(int millis)
    {
        lastspawn = millis;
        clearstate();
        weapreset(true);
    }

    void updatetimeplayed()
    {
        if(lasttimeplayed)
        {
            int millis = totalmillis-lasttimeplayed, secs = millis/1000;
            timeplayed += secs;
            lasttimeplayed = totalmillis+(secs*1000)-millis;
        }
        else lasttimeplayed = totalmillis ? totalmillis : 1;
    }

    float scoretime(bool update = true)
    {
        if(update) updatetimeplayed();
        return totalpoints/float(max(timeplayed, 1));
    }

    float kdratio(bool total = true)
    {
        if(total) return totalfrags >= totaldeaths ? (totalfrags/float(max(totaldeaths, 1))) : -(totaldeaths/float(max(totalfrags, 1)));
        return frags >= deaths ? (frags/float(max(deaths, 1))) : -(deaths/float(max(frags, 1)));
    }

    float combinedkdratio()
    {
        return ((totalfrags / max(totaldeaths, 1)) + (frags / max(deaths, 1))) / 2;
    }

    bool canrandweap(int weap)
    {
        int cweap = weap-W_OFFSET;
        if(!randweap.inrange(cweap)) return true;
        return randweap[cweap];
    }

    void spawnstate(int gamemode, int mutators, int sweap, int heal)
    {
        weapreset(true);
        health = heal > 0 ? heal : (m_insta(gamemode, mutators) ? 1 : m_health(gamemode, mutators, actortype));
        int s = sweap;
        if(!isweap(s)) s = m_weapon(actortype, gamemode, mutators);
        if(s >= W_ALL) s = W_CLAW;
        if(isweap(s))
        {
            ammo[s] = max(1, W(s, ammomax));
            weapselect = s;
        }
        if(s != W_CLAW && AA(actortype, abilities)&(1<<A_A_CLAW)) ammo[W_CLAW] = max(1, W(W_CLAW, ammomax));
        if(s != W_MELEE && AA(actortype, abilities)&(1<<A_A_MELEE)) ammo[W_MELEE] = max(1, W(W_MELEE, ammomax));
        if(actortype < A_ENEMY)
        {
            if(m_kaboom(gamemode, mutators)) ammo[W_MINE] = max(1, W(W_MINE, ammomax));
            else if(!m_race(gamemode) || m_ra_gauntlet(gamemode, mutators))
            {
                if(s != W_GRENADE && AA(actortype, spawngrenades) >= (m_insta(gamemode, mutators) ? 2 : 1))
                    ammo[W_GRENADE] = max(1, W(W_GRENADE, ammomax));
                if(s != W_MINE && AA(actortype, spawnmines) >= (m_insta(gamemode, mutators) ? 2 : 1))
                    ammo[W_MINE] = max(1, W(W_MINE, ammomax));
            }
        }
        if(AA(actortype, maxcarry) && m_loadout(gamemode, mutators))
        {
            int n = 0, musthave = AA(actortype, maxcarry);
            loopj(W_LOADOUT) if(canrandweap(j + W_OFFSET)) musthave--;
            vector<bool> forced;
            loopj(W_LOADOUT) forced.add(false);
            vector<int> aweap;
            loopj(W_LOADOUT) aweap.add(loadweap.inrange(j) ? loadweap[j] : 0);
            loopvj(aweap)
            {
                if(!aweap[j]) // specifically asking for random
                {
                    for(int t = rnd(W_ITEM-W_OFFSET)+W_OFFSET, r = 0; r < W_LOADOUT; r++)
                    {
                        bool canuse = canrandweap(t);
                        if(!canuse && musthave > 0 && !forced[t - W_OFFSET])
                        {
                            canuse = true;
                            musthave--;
                            forced[t - W_OFFSET] = true;
                        }
                        if(t >= W_OFFSET && t < W_ITEM && !hasweap(t, sweap) && m_check(W(t, modes), W(t, muts), gamemode, mutators) && !W(t, disabled) && canuse)
                        {
                            aweap[j] = t;
                            break;
                        }
                        else if(++t >= W_ITEM) t = W_OFFSET;
                    }
                }
                if(aweap[j] >= W_OFFSET && aweap[j] < W_ITEM && !hasweap(aweap[j], sweap) && m_check(W(aweap[j], modes), W(aweap[j], muts), gamemode, mutators) && !W(aweap[j], disabled))
                {
                    ammo[aweap[j]] = max(1, W(aweap[j], ammomax));
                    if(!n) weapselect = aweap[j];
                    if(++n >= AA(actortype, maxcarry)) break;
                }
            }
        }
    }

    void editspawn(int gamemode, int mutators)
    {
        clearstate();
        spawnstate(gamemode, mutators, m_weapon(actortype, gamemode, mutators), m_health(gamemode, mutators, actortype));
    }

    int respawnwait(int millis, int delay)
    {
        return lastdeath ? max(0, delay-(millis-lastdeath)) : 0;
    }

    int protect(int millis, int delay)
    {
        if(actortype >= A_ENEMY || !lastspawn || !delay) return 0;
        if(G(protectbreak) && lastshoot) return 0;
        int len = millis-lastspawn;
        if(len > delay) return 0;
        return delay-len;
    }

    bool burning(int millis, int len) { return len && lastres[WR_BURN] && millis-lastres[WR_BURN] <= len; }
    bool bleeding(int millis, int len) { return len && lastres[WR_BLEED] && millis-lastres[WR_BLEED] <= len; }
    bool shocking(int millis, int len) { return len && lastres[WR_SHOCK] && millis-lastres[WR_SHOCK] <= len; }
};

namespace server
{
    extern void stopdemo();
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern bool servcmd(int nargs, const char *cmd, const char *arg);
    extern const char *gamename(int mode, int muts, int compact = 0, int limit = 0, const char *separator = " ");
    extern const char *privname(int priv = PRIV_NONE, int actortype = A_PLAYER);
    extern const char *privnamex(int priv = PRIV_NONE, int actortype = A_PLAYER, bool local = false);
#ifdef GAMESERVER
    struct clientinfo;
    extern void waiting(clientinfo *ci, int drop = 0, bool doteam = true, bool exclude = false);
    extern void setteam(clientinfo *ci, int team, int flags = TT_RESET, bool swaps = true);
    extern int chooseteam(clientinfo *ci, int suggest = -1, bool wantbal = false);
#endif
}

#if !defined(GAMESERVER) && !defined(STANDALONE)
template<class T> inline void flashcolour(T &r, T &g, T &b, T br, T bg, T bb, float amt)
{
    r += (br-r)*amt;
    g += (bg-g)*amt;
    b += (bb-b)*amt;
}

template<class T> inline void flashcolourf(T &r, T &g, T &b, T &f, T br, T bg, T bb, T bf, float amt)
{
    r += (br-r)*amt;
    g += (bg-g)*amt;
    b += (bb-b)*amt;
    f += (bf-f)*amt;
}

struct gameentity : extentity
{
    int schan;
    int lastspawn, nextemit;
    linkvector kin;

    gameentity() : schan(-1), lastspawn(0), nextemit(0) {}
    ~gameentity()
    {
        if(issound(schan)) removesound(schan);
        schan = -1;
    }
};

struct actitem
{
    enum { ENT = 0, PROJ };
    int type, target;
    float score;

    actitem() : type(ENT), target(-1), score(0) {}
    ~actitem() {}
};
#ifdef GAMEWORLD
const char * const animnames[] =
{
    "idle", "forward", "backward", "left", "right", "dead", "dying", "swim",
    "mapmodel", "trigger on", "trigger off", "pain",
    "jump forward", "jump backward", "jump left", "jump right", "jump",
    "impulse forward", "impulse backward", "impulse left", "impulse right",
    "dash forward", "dash backward", "dash left", "dash right", "dash up",
    "wall run left", "wall run right", "wall jump", "power slide", "fly kick",
    "sink", "edit", "win", "lose",
    "crouch", "crawl forward", "crawl backward", "crawl left", "crawl right",
    "crouch jump forward", "crouch jump backward", "crouch jump left", "crouch jump right", "crouch jump",
    "claw", "claw primary", "claw secondary",
    "pistol", "pistol primary", "pistol secondary", "pistol reload",
    "sword", "sword primary", "sword secondary",
    "shotgun", "shotgun primary", "shotgun secondary", "shotgun reload",
    "smg", "smg primary", "smg secondary", "smg reload",
    "flamer", "flamer primary", "flamer secondary", "flamer reload",
    "plasma", "plasma primary", "plasma secondary", "plasma reload",
    "zapper", "zapper primary", "zapper secondary", "zapper reload",
    "rifle", "rifle primary", "rifle secondary", "rifle reload",
    "grenade", "grenade primary", "grenade secondary", "grenade reload", "grenade power",
    "mine", "mine primary", "mine secondary", "mine reload",
    "rocket", "rocket primary", "rocket secondary", "rocket reload",
    "switch", "use",
    ""
};
#else
extern const char * const animnames[];
#endif

struct eventicon
{
    enum { SPREE = 0, MULTIKILL, HEADSHOT, DOMINATE, REVENGE, FIRSTBLOOD, BREAKER, WEAPON, AFFINITY, TOTAL, SORTED = WEAPON, VERBOSE = WEAPON };
    int type, millis, fade, length, value;
};

struct stunevent
{
    int weap, millis, delay;
    float scale, gravity;
};

struct gameent : dynent, clientstate
{
    editinfo *edit;
    ai::aiinfo *ai;
    int team, clientnum, privilege, projid, lastnode, checkpoint, cplast, respawned, suicided, lastupdate, lastpredict, plag, ping, lastflag, totaldamage,
        actiontime[AC_MAX], impulse[IM_MAX], smoothmillis, turnmillis, turnside, aschan, cschan, vschan, wschan, pschan, sschan[2],
        lasthit, lastteamhit, lastkill, lastattacker, lastpoints, quake, lastfoot, lastimpulsecollect;
    float deltayaw, deltapitch, newyaw, newpitch, turnyaw, turnroll;
    vec head, torso, muzzle, origin, eject[2], waist, jet[3], legs, hrad, trad, lrad, toe[2];
    bool action[AC_MAX], conopen, k_up, k_down, k_left, k_right, obliterated, headless;
    string hostname, hostip, name, handle, info, obit;
    vector<gameent *> dominating, dominated;
    vector<eventicon> icons;
    vector<stunevent> stuns;
    vector<int> vitems;

    gameent() : edit(NULL), ai(NULL), team(T_NEUTRAL), clientnum(-1), privilege(PRIV_NONE), projid(0), checkpoint(-1), cplast(0), lastupdate(0), lastpredict(0), plag(0), ping(0),
        totaldamage(0), smoothmillis(-1), turnmillis(0), lastattacker(-1), lastpoints(0), quake(0),
        conopen(false), k_up(false), k_down(false), k_left(false), k_right(false), obliterated(false)
    {
        state = CS_DEAD;
        type = ENT_PLAYER;
        copystring(hostname, "0.0.0.0");
        copystring(hostip, "0.0.0.0");
        name[0] = handle[0] = info[0] = obit[0] = '\0';
        removesounds();
        cleartags();
        checktags();
        respawn(-1, 0, 0);
    }
    ~gameent()
    {
        removesounds();
        freeeditinfo(edit);
        if(ai) delete ai;
        removetrackedparticles(this);
        removetrackedsounds(this);
    }

    static bool is(int t) { return t == ENT_PLAYER || t == ENT_AI; }
    static bool is(physent *d) { return d->type == ENT_PLAYER || d->type == ENT_AI; }

    void setparams(bool reset)
    {
        int type = clamp(actortype, 0, int(A_MAX-1));
        xradius = actor[type].xradius*curscale;
        yradius = actor[type].yradius*curscale;
        zradius = actor[type].height*curscale;
        if(reset) height = zradius;
        speed = AA(type, speed);
        weight = AA(type, weight)*curscale;
        radius = max(xradius, yradius);
        aboveeye = curscale;
    }

    void setscale(float scale, int millis, bool reset)
    {
        if(scale != curscale)
        {
            if(!reset && state == CS_ALIVE && millis > 0)
                curscale = scale > curscale ? min(curscale+millis/2000.f, scale) : max(curscale-millis/2000.f, scale);
            else curscale = scale;
        }
        setparams(reset);
    }

    int getprojid()
    {
        if(++projid < 2) projid = 2;
        return projid;
    }

    void removesounds()
    {
        if(issound(aschan)) removesound(aschan);
        if(issound(cschan)) removesound(cschan);
        if(issound(vschan)) removesound(vschan);
        if(issound(wschan)) removesound(wschan);
        if(issound(pschan)) removesound(pschan);
        aschan = cschan = vschan = wschan = pschan = -1;
        loopi(2)
        {
            if(issound(sschan[i])) removesound(sschan[i]);
            sschan[i] = -1;
        }
    }

    void stopmoving(bool full)
    {
        if(full) move = strafe = 0;
        loopi(AC_MAX)
        {
            action[i] = false;
            actiontime[i] = 0;
        }
    }

    void clearstate(int gamemode, int mutators)
    {
        loopi(IM_MAX) if(i != IM_METER || !m_ra_endurance(gamemode, mutators)) impulse[i] = 0;
        lasthit = lastkill = quake = turnmillis = turnside = lastimpulsecollect = 0;
        turnroll = turnyaw = 0;
        lastteamhit = lastflag = respawned = suicided = lastnode = lastfoot = -1;
        obit[0] = '\0';
        obliterated = headless = false;
        setscale(1, 0, true);
        icons.shrink(0);
        stuns.shrink(0);
        used.shrink(0);
    }

    void respawn(int millis, int gamemode, int mutators)
    {
        stopmoving(true);
        removesounds();
        clearstate(gamemode, mutators);
        physent::reset();
        clientstate::respawn(millis);
    }

    void editspawn(int gamemode, int mutators)
    {
        stopmoving(true);
        clearstate(gamemode, mutators);
        inmaterial = airmillis = floormillis = 0;
        inliquid = onladder = forcepos = false;
        strafe = move = 0;
        physstate = PHYS_FALL;
        vel = falling = vec(0, 0, 0);
        floor = vec(0, 0, 1);
        resetinterp();
        clientstate::editspawn(gamemode, mutators);
    }

    void resetstate(int millis, int gamemode, int mutators)
    {
        respawn(millis, gamemode, mutators);
        checkpoint = -1;
        frags = deaths = totaldamage = cplast = 0;
    }

    void mapchange(int millis, int gamemode, int mutators)
    {
        dominating.shrink(0);
        dominated.shrink(0);
        icons.shrink(0);
        resetstate(millis, gamemode, mutators);
        clientstate::mapchange();
    }

    void cleartags() { head = torso = muzzle = origin = eject[0] = eject[1] = waist = jet[0] = jet[1] = jet[2] = toe[0] = toe[1] = vec(-1, -1, -1); }

    vec footpos(int foot)
    {
        if(foot < 0 || foot > 1) return feetpos();
        if(toe[foot] == vec(-1, -1, -1))
        {
            int millis = lastmillis%500;
            float amt = millis > 250 ? (500-millis)/250.f : millis/250.f;
            vec dir, right;
            vecfromyawpitch(yaw, pitch, 1, 0, dir);
            vecfromyawpitch(yaw, pitch, 0, foot ? 1 : -1, right);
            dir.mul(radius*0.5f);
            right.mul(radius*(!move && strafe ? amt-0.5f : 0.5f));
            dir.z -= height*0.6f+(height*0.4f*(foot ? 1-amt : amt));
            toe[foot] = vec(o).add(dir).add(right);
        }
        return toe[foot];
    }

    vec originpos(int weap = -1)
    {
        if(!isweap(weap)) weap = weapselect;
        if(origin == vec(-1, -1, -1))
            origin = vec(weap == W_MELEE ? feetpos() : center()).add(vec(yaw*RAD, pitch*RAD));
        return origin;
    }

    vec muzzlepos(int weap = -1)
    {
        if(!isweap(weap)) weap = weapselect;
        if(muzzle == vec(-1, -1, -1))
        {
            if(weap == W_SWORD && ((weapstate[weap] == W_S_PRIMARY) || (weapstate[weap] == W_S_SECONDARY)))
            {
                float frac = (lastmillis-weaptime[weap])/float(weapwait[weap]), yx = yaw, px = pitch;
                if(weapstate[weap] == W_S_PRIMARY)
                {
                    yx -= 90;
                    yx += frac*180;
                    if(yx >= 360) yx -= 360;
                    if(yx < 0) yx += 360;
                }
                else
                {
                    px += 90;
                    px -= frac*180;
                    if(px >= 180) px -= 360;
                    if(px < -180) px += 360;
                }
                muzzle = vec(originpos(weap)).add(vec(yx*RAD, px*RAD).mul(8));
            }
            else
            {
                vec dir(yaw*RAD, pitch*RAD);
                if(weap != W_CLAW)
                {
                    vec right;
                    vecfromyawpitch(yaw, pitch, 0, -1, right);
                    muzzle = vec(originpos(weap)).add(dir.mul(radius*0.75f)).add(right.mul(radius*0.6f));
                }
                else muzzle = vec(originpos(weap)).add(dir.mul(radius*2));
            }
        }
        return muzzle;
    }

    vec ejectpos(int weap = -1, bool alt = false)
    {
        if(!isweap(weap)) weap = weapselect;
        if(eject[alt ? 1 : 0] == vec(-1, -1, -1)) eject[alt ? 1 : 0] = alt ? originpos(weap) : muzzlepos(weap);
        return eject[alt ? 1 : 0];
    }

    void hitboxes()
    {
        float hsize = max(xradius*0.45f, yradius*0.45f);
        if(head == vec(-1, -1, -1))
        {
            head = o;
            head.z -= hsize*0.375f;
        }
        hrad = vec(xradius*0.5f, yradius*0.5f, hsize);
        if(torso == vec(-1, -1, -1))
        {
            torso = o;
            torso.z -= height*0.45f;
        }
        float tsize = (head.z-hrad.z)-torso.z;
        trad = vec(xradius, yradius, tsize);
        float lsize = ((torso.z-trad.z)-(o.z-height))*0.5f;
        legs = torso;
        legs.z -= trad.z+lsize;
        lrad = vec(xradius*0.85f, yradius*0.85f, lsize);
        if(waist == vec(-1, -1, -1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 0, dir);
            dir.mul(radius*1.5f);
            dir.z -= height*0.5f;
            waist = vec(o).add(dir);
        }
        if(jet[0] == vec(-1, -1, -1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, -1, dir);
            dir.mul(radius);
            dir.z -= height;
            jet[0] = vec(o).add(dir);
        }
        if(jet[1] == vec(-1, -1, -1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 1, dir);
            dir.mul(radius);
            dir.z -= height;
            jet[1] = vec(o).add(dir);
        }
        if(jet[2] == vec(-1, -1, -1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 0, dir);
            dir.mul(radius*1.25f);
            dir.z -= height*0.35f;
            jet[2] = vec(o).add(dir);
        }
    }

    bool wantshitbox() { return type == ENT_PLAYER || (type == ENT_AI && actor[actortype].hitbox); }

    void checktags()
    {
        originpos();
        muzzlepos();
        loopi(2) ejectpos(i!=0);
        if(wantshitbox()) hitboxes();
    }


    void doimpulse(int cost, int type, int millis)
    {
        bool jump = type > IM_T_NONE && type < IM_T_WALL;
        impulse[IM_METER] += cost;
        impulse[IM_TIME] = impulse[IM_REGEN] = millis;
        if(type == IM_T_DASH) impulse[IM_SLIDE] = millis;
        if(type != IM_T_KICK) impulse[IM_SLIP] = millis;
        if(!impulse[IM_JUMP] && jump) impulse[IM_JUMP] = millis;
        impulse[IM_TYPE] = type;
        impulse[IM_COUNT]++;
        resetphys(jump);
    }

    void resetjump()
    {
        airmillis = turnside = impulse[IM_COUNT] = impulse[IM_TYPE] = impulse[IM_JUMP] = 0;
    }

    void resetair()
    {
        resetphys();
        resetjump();
    }

    void addicon(int type, int millis, int fade, int value = 0)
    {
        int pos = -1;
        loopv(icons)
        {
            if(icons[i].type == type)
            {
                switch(icons[i].type)
                {
                    case eventicon::WEAPON:
                    {
                        if(icons[i].value != value) continue;
                        break;
                    }
                    default: break;
                }
                icons[i].length = max(icons[i].fade, fade);
                icons[i].fade = millis-icons[i].millis+fade;
                icons[i].value = value;
                return;
            }
            if(pos < 0 && type >= eventicon::SORTED && icons[i].type > type) pos = i;
        }
        eventicon e;
        e.type = type;
        e.millis = millis;
        e.fade = e.length = fade;
        e.value = value;
        if(pos < 0) icons.add(e);
        else icons.insert(pos, e);
    }

    void setname(const char *n)
    {
        if(n && *n) copystring(name, n, MAXNAMELEN+1);
        else name[0] = '\0';
    }

    bool setvanity(const char *v)
    {
        if(clientstate::setvanity(v))
        {
            vitems.shrink(0);
            return true;
        }
        return false;
    }

    void setinfo(const char *n, int c, int m, const char *v, vector<int> &w, vector<int> &r)
    {
        setname(n);
        colour = c;
        model = m;
        setvanity(v);
        loadweap.shrink(0);
        loopv(w) loadweap.add(w[i]);
        randweap.shrink(0);
        loopv(r) randweap.add(r[i]);
    }

    void addstun(int weap, int millis, int delay, float scale, float gravity)
    {
        if(delay <= 0 || (scale == 0 && gravity == 0)) return;
        stunevent &s = stuns.add();
        s.weap = weap;
        s.millis = millis;
        s.delay = delay;
        s.scale = scale;
        s.gravity = gravity;
    }

    float stunned(int millis, bool gravity = false)
    {
        float stun = 0;
        loopvrev(stuns)
        {
            stunevent &s = stuns[i];
            if(!s.delay || millis-s.millis >= s.delay) stuns.remove(i);
            else stun += (gravity ? s.gravity : s.scale)*(1.f-(float(millis-s.millis)/float(s.delay)));
        }
        return stun;
    }

    bool hasmelee(int millis, bool check = false, bool slide = false, bool onfloor = true, bool can = true)
    {
        if(!(AA(actortype, abilities)&(1<<A_A_MELEE))) return false;
        if(check && (!action[AC_SPECIAL] || onfloor) && !slide) return false;
        if(can && (weapstate[W_MELEE] != (slide ? W_S_SECONDARY : W_S_PRIMARY) || millis-weaptime[W_MELEE] >= weapwait[W_MELEE])) return false;
        return true;
    }

    bool canmelee(int sweap, int millis, bool check = false, bool slide = false, bool onfloor = true)
    {
        if(!hasmelee(millis, check, slide, onfloor, false)) return false;
        if(!canshoot(W_MELEE, slide ? HIT_ALT : HIT_NONE, sweap, millis, (1<<W_S_RELOAD))) return false;
        return true;
    }

    int curfoot()
    {
        vec dir;
        vecfromyawpitch(yaw, 0, move, strafe && !move ? strafe : 0, dir);
        dir.mul(radius).add(o).z -= height; // foot furthest away is one being set down
        return footpos(0).squaredist(dir) > footpos(1).squaredist(dir) ? 0 : 1;
    }

    bool crouching(bool limit = false)
    {
        if(!(AA(actortype, abilities)&(1<<A_A_CROUCH))) return false;
        return action[AC_CROUCH] || (!limit && zradius > height);
    }

    bool running(float minspeed = 0)
    {
        if(minspeed != 0 && vel.magnitude() >= minspeed) return true;
        return sliding() || (!action[AC_WALK] && !crouching());
    }

    bool sliding(bool power = false)
    {
        if((!power && turnside) || (G(impulseslip) && impulse[IM_SLIP] && lastmillis-impulse[IM_SLIP] <= G(impulseslip)) || (G(impulseslide) && impulse[IM_SLIDE] && lastmillis-impulse[IM_SLIDE] <= G(impulseslide)))
        {
            if(!power || crouching())
            {
                if(power && G(impulseslide) && G(impulseslip) && move == 1 && impulse[IM_SLIP] > impulse[IM_SLIDE])
                    impulse[IM_SLIDE] = impulse[IM_SLIP];
                return true;
            }
        }
        return false;
    }

    int zooming()
    {
        if(isweap(weapselect) && W2(weapselect, cooked, true)&W_C_ZOOM)
        {
            if(weapstate[weapselect] == W_S_ZOOM) return weaptime[weapselect];
            if(W2(weapselect, cooked, true)&W_C_KEEP && prevstate[weapselect] == W_S_ZOOM && action[AC_SECONDARY])
                return prevtime[weapselect];
        }
        return 0;
    }
};

enum { PRJ_SHOT = 0, PRJ_GIBS, PRJ_DEBRIS, PRJ_EJECT, PRJ_ENT, PRJ_AFFINITY, PRJ_VANITY, PRJ_MAX };

struct projent : dynent
{
    vec from, to, dest, norm, inertia, sticknrm, stickpos, effectpos, lastgood;
    int addtime, lifetime, lifemillis, waittime, spawntime, fadetime, lastradial, lasteffect, lastbounce, beenused, extinguish, stuck;
    float movement, distance, lifespan, lifesize, speedmin, speedmax;
    bool local, limited, escaped, child;
    int projtype, projcollide, interacts;
    float elasticity, reflectivity, relativity, liquidcoast;
    int schan, id, weap, fromweap, fromflags, value, flags, hitflags;
    entitylight light;
    gameent *owner, *target, *stick;
    physent *hit;
    const char *mdl;

    projent() : projtype(PRJ_SHOT), id(-1), hitflags(HITFLAG_NONE), owner(NULL), target(NULL), stick(NULL), hit(NULL), mdl(NULL) { reset(); }
    ~projent()
    {
        removetrackedparticles(this);
        removetrackedsounds(this);
        if(issound(schan)) removesound(schan);
        schan = -1;
    }

    static bool is(int t) { return t == ENT_PROJ; }
    static bool is(physent *d) { return d->type == ENT_PROJ; }

    void reset()
    {
        physent::reset();
        type = ENT_PROJ;
        state = CS_ALIVE;
        norm = vec(0, 0, 1);
        inertia = sticknrm = stickpos = lastgood = vec(0, 0, 0);
        effectpos = vec(-1e16f, -1e16f, -1e16f);
        addtime = lifetime = lifemillis = waittime = spawntime = fadetime = lastradial = lasteffect = lastbounce = beenused = flags = 0;
        schan = id = weap = fromweap = fromflags = value = -1;
        movement = distance = lifespan = speedmin = speedmax = 0;
        curscale = speedscale = lifesize = 1;
        extinguish = stuck = interacts = 0;
        limited = escaped = child = false;
        projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
    }

    bool ready(bool used = true)
    {
        if(owner && (!used || projtype == PRJ_SHOT || !beenused) && waittime <= 0 && state != CS_DEAD)
            return true;
        return false;
    }
};

struct cament
{
    enum { ENTITY = 0, WAYPOINT, PLAYER, AFFINITY, MAX };

    int cn, type, id, inview[MAX], lastinview[MAX], lastyawtime, lastpitchtime;
    vec o, dir;
    float dist, lastyaw, lastpitch;
    gameent *player;
    bool ignore;
    cament *moveto;

    cament(int p, int t) : cn(p), type(t), id(-1), player(NULL), ignore(false), moveto(NULL)
    {
        reset();
        resetlast();
    }
    cament(int p, int t, int n) : cn(p), type(t), id(n), player(NULL), ignore(false), moveto(NULL)
    {
        reset();
        resetlast();
    }
    cament(int p, int t, int n, vec &d) : cn(p), type(t), id(n), player(NULL), ignore(false), moveto(NULL)
    {
        reset();
        resetlast();
        o = d;
    }
    cament(int p, int t, int n, vec &c, gameent *d) : cn(p), type(t), id(n), player(d), ignore(false), moveto(NULL)
    {
        reset();
        resetlast();
        o = c;
    }
    ~cament() {}

    void reset()
    {
        loopi(MAX) inview[i] = lastinview[i] = 0;
        if(dir.iszero()) dir = vec(float(rnd(360)), float(rnd(91)-45));
    }

    void resetlast()
    {
        lastyawtime = lastpitchtime = lastmillis;
        lastyaw = lastpitch = 0;
    }

    static bool compare(const cament *a, const cament *b)
    {
        if(!a->ignore && b->ignore) return true;
        if(a->ignore && !b->ignore) return false;
        if(a->inview[cament::PLAYER] > b->inview[cament::PLAYER]) return true;
        if(a->inview[cament::PLAYER] < b->inview[cament::PLAYER]) return false;
        if(a->inview[cament::AFFINITY] > b->inview[cament::AFFINITY]) return true;
        if(a->inview[cament::AFFINITY] < b->inview[cament::AFFINITY]) return false;
        return !rnd(2);
    }

    vec pos(float amt = 0)
    {
        if(amt > 0 && moveto) return vec(o).add(vec(moveto->o).sub(o).mul(min(amt, 1.f)));
        return o;
    }
};

namespace client
{
    extern int showpresence, showteamchange, needsmap, gettingmap;
    extern bool demoplayback, isready;
    extern vector<uchar> messages;
    extern void clearvotes(gameent *d, bool msg = false);
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool isignored(int cn);
    extern bool addmsg(int type, const char *fmt = NULL, ...);
    extern void saytext(gameent *f, gameent *t, int flags, char *text);
    extern void c2sinfo(bool force = false);
    extern bool haspriv(gameent *d, int priv = PRIV_NONE);
}

namespace physics
{
    extern int smoothmove, smoothdist, physframetime, physinterp, impulsemethod, impulseaction, jumpstyle, dashstyle, crouchstyle, walkstyle, grabstyle, grabplayerstyle, kickoffstyle, kickupstyle;
    extern float floatspeed, floatcoast, impulsekick, impulserolll, kickoffangle, kickupangle;
    extern bool isghost(gameent *d, gameent *e, bool proj = false);
    extern bool carryaffinity(gameent *d);
    extern bool dropaffinity(gameent *d);
    extern bool secondaryweap(gameent *d);
    extern bool allowimpulse(physent *d, int level = 0);
    extern bool canimpulse(physent *d, int level = 0, bool kick = false);
    extern float impulsevelocity(physent *d, float amt, int &cost, int type, float redir, vec &keep);
    extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
    extern void smoothplayer(gameent *d, int res, bool local);
    extern void update();
    extern void reset();
}

namespace projs
{
    extern vector<projent *> projs, collideprojs;

    extern void reset();
    extern void update();
    extern projent *create(const vec &from, const vec &to, bool local, gameent *d, int type, int fromweap, int fromflags, int lifetime, int lifemillis, int waittime, int speed, int id = 0, int weap = -1, int value = -1, int flags = 0, float scale = 1, bool child = false, projent *parent = NULL);
    extern void preload();
    extern void remove(gameent *owner);
    extern void destruct(gameent *d, int id);
    extern void sticky(gameent *d, int id, vec &norm, vec &pos, gameent *f = NULL);
    extern void shootv(int weap, int flags, int sub, int offset, float scale, vec &from, vector<shotmsg> &shots, gameent *d, bool local);
    extern void drop(gameent *d, int weap, int ent, int ammo = -1, bool local = true, int index = 0, int targ = -1);
    extern void adddynlights();
    extern void render();
}

namespace weapons
{
    extern int autoreloading;
    extern int slot(gameent *d, int n, bool back = false);
    extern bool weapselect(gameent *d, int weap, int filter, bool local = true);
    extern bool weapreload(gameent *d, int weap, int load = -1, int ammo = -1, bool local = true);
    extern void weapdrop(gameent *d, int w = -1);
    extern void checkweapons(gameent *d);
    extern float accmod(gameent *d, bool zooming);
    extern bool doshot(gameent *d, vec &targ, int weap, bool pressed = false, bool secondary = false, int force = 0);
    extern void shoot(gameent *d, vec &targ, int force = 0);
    extern void preload();
    extern bool canuse(int weap);
}

namespace hud
{
    extern char *chattex, *insigniatex, *playertex, *deadtex, *waitingtex, *spectatortex, *editingtex, *dominatingtex, *dominatedtex, *inputtex,
        *bliptex, *pointtex, *flagtex, *bombtex, *arrowtex, *arrowrighttex, *arrowdowntex, *arrowlefttex, *alerttex, *questiontex, *flagdroptex,
        *flagtakentex, *bombdroptex, *bombtakentex, *attacktex, *warningtex, *inventorytex, *indicatortex, *crosshairtex, *hithairtex,
        *spree1tex, *spree2tex, *spree3tex, *spree4tex, *multi1tex, *multi2tex, *multi3tex, *headshottex, *dominatetex, *revengetex,
        *firstbloodtex, *breakertex;
    extern int hudwidth, hudheight, hudsize, lastteam, damageresidue, damageresiduefade, shownotices, showevents, radaraffinitynames,
        inventorygame, inventoryscore, inventoryscorespec, inventoryscorebg, inventoryscoreinfo, inventoryscorename, inventoryscorepos, inventoryscorebreak, inventoryracestyle,
        teamhurthud, teamhurttime, teamhurtdist;
    extern float noticescale, eventscale, inventoryblend, inventoryskew, radaraffinityblend, radarblipblend, radaraffinitysize,
        inventoryglow, inventoryscoresize, inventoryscoreshrink, inventoryscoreshrinkmax, noticepadx, noticepady, eventpadx, eventpady;
    extern vector<int> teamkills;
    extern const char *icontex(int type, int value);
    extern bool chkcond(int val, bool cond);
    extern void drawindicator(int weap, int x, int y, int s);
    extern void drawclip(int weap, int x, int y, float s);
    extern void drawpointertex(const char *tex, int x, int y, int s, float r = 1, float g = 1, float b = 1, float fade = 1);
    extern void drawpointer(int w, int h, int index);
    extern int numteamkills();
    extern int radarrange();
    extern void drawblip(const char *tex, float area, int w, int h, float s, float blend, int style, const vec &pos, const vec &colour = vec(1, 1, 1), const char *font = "reduced", const char *text = NULL, ...);
    extern int drawprogress(int x, int y, float start, float length, float size, bool left, float r = 1, float g = 1, float b = 1, float fade = 1, float skew = 1, const char *font = NULL, const char *text = NULL, ...);
    extern int drawitembar(int x, int y, float size, bool left, float r = 1, float g = 1, float b = 1, float fade = 1, float skew = 1, float amt = 1, int type = 0);
    extern int drawitem(const char *tex, int x, int y, float size, float sub = 0, bool bg = true, bool left = false, float r = 1, float g = 1, float b = 1, float fade = 1, float skew = 1, const char *font = NULL, const char *text = NULL, ...);
    extern int drawitemtextx(int x, int y, float size, int flags = TEXT_LEFT_UP, float skew = 1, const char *font = NULL, float blend = 1, const char *text = NULL, ...);
    extern int drawitemtext(int x, int y, float size, bool left = false, float skew = 1, const char *font = NULL, float blend = 1, const char *text = NULL, ...);
    extern int drawweapons(int x, int y, int s, float blend = 1);
    extern int drawhealth(int x, int y, int s, float blend = 1, bool interm = false);
    extern void drawinventory(int w, int h, int edge, float blend = 1);
    extern void damage(int n, const vec &loc, gameent *v, int weap, int flags);
    extern void hit(int n, const vec &loc, gameent *v, int weap, int flags);
    extern const char *teamtexname(int team = T_NEUTRAL);
    extern const char *itemtex(int type, int stype);
    extern const char *privtex(int priv = PRIV_NONE, int actortype = A_PLAYER);
    extern bool canshowscores();
    extern void showscores(bool on, bool interm = false, bool onauto = true, bool ispress = false);
    extern score &teamscore(int team);
    extern void resetscores();
    extern int raceinventory(int x, int y, int s, float blend);
    extern int drawscore(int x, int y, int s, int m, float blend, int count);
    extern void cleanup();
}

enum { CTONE_TEAM = 0, CTONE_TONE, CTONE_TEAMED, CTONE_ALONE, CTONE_MIXED, CTONE_TMIX, CTONE_AMIX, CTONE_MAX };
namespace game
{
    extern int gamestate, gamemode, mutators, nextmode, nextmuts, timeremaining, lasttimeremain, maptime, lastzoom, lasttvcam, lasttvchg, spectvtime, waittvtime,
            bloodfade, bloodsize, bloodsparks, debrisfade, eventiconfade, eventiconshort,
            announcefilter, dynlighteffects, aboveheadnames, followthirdperson, nogore, forceplayermodel,
            playerovertone, playerundertone, playerdisplaytone, playereffecttone, playerteamtone, follow, specmode, spectvfollow, spectvfollowing, clientcrc;
    extern float bloodscale, debrisscale, aboveitemiconsize, playerovertonelevel, playerundertonelevel, playerdisplaytonelevel, playereffecttonelevel, playerteamtonelevel;
    extern bool zooming;
    extern vec swaypush, swaydir;
    extern string clientmap;

    extern gameent *player1, *focus;
    extern vector<gameent *> players, waiting;

    struct avatarent : dynent
    {
        avatarent() { type = ENT_CAMERA; }
    };
    extern avatarent avatarmodel, bodymodel;

    extern void vanityreset();
    extern void vanitybuild(gameent *d);
    extern const char *vanityfname(gameent *d, int n, bool proj = false);
    extern bool followswitch(int n, bool other = false);
    extern vector<cament *> cameras;
    extern gameent *newclient(int cn);
    extern gameent *getclient(int cn);
    extern gameent *intersectclosest(vec &from, vec &to, gameent *at);
    extern void clientdisconnected(int cn, int reason = DISC_NONE);
    extern const char *colourname(gameent *d, char *name = NULL, bool icon = true, bool dupname = true, int colour = 3);
    extern const char *colourteam(int team, const char *icon = "");
    extern int findcolour(gameent *d, bool tone = true, bool mix = false, float level = 1);
    extern int getcolour(gameent *d, int type = 0, float level = 1.f);
    extern void errorsnd(gameent *d);
    extern void announce(int idx, gameent *d = NULL, bool forced = false);
    extern void announcef(int idx, int targ, gameent *d, bool forced, const char *msg, ...);
    extern void specreset(gameent *d = NULL, bool clear = false);
    extern void respawn(gameent *d);
    extern void respawned(gameent *d, bool local, int ent = -1);
    extern vec pulsecolour(physent *d, int i = 0, int cycle = 50);
    extern int hexpulsecolour(physent *d, int i = 0, int cycle = 50);
    extern void spawneffect(int type, const vec &pos, float radius, int colour, float size);
    extern void impulseeffect(gameent *d, int effect = 0);
    extern void suicide(gameent *d, int flags);
    extern void fixrange(float &yaw, float &pitch);
    extern void fixfullrange(float &yaw, float &pitch, float &roll, bool full = false);
    extern void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch);
    extern void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float yawspeed = 1, float pitchspeed = 1, float rotate = 0);
    extern bool allowmove(physent *d);
    extern int deadzone();
    extern void checkzoom();
    extern bool inzoom();
    extern void zoomview(bool down);
    extern bool tvmode(bool check = true, bool force = true);
    extern void resetcamera(bool cam = true, bool input = true);
    extern void resetworld();
    extern void resetstate();
    extern void hiteffect(int weap, int flags, int damage, gameent *d, gameent *v, vec &dir, vec &vel, float dist, bool local = false);
    extern void damaged(int weap, int flags, int damage, int health, gameent *d, gameent *v, int millis, vec &dir, vec &vel, float dist);
    extern void killed(int weap, int flags, int damage, gameent *d, gameent *v, vector<gameent*> &log, int style, int material);
    extern void timeupdate(int state, int remain);
    extern vec rescolour(dynent *d, int c = PULSE_BURN);
    extern float rescale(gameent *d);
    extern float opacity(gameent *d, bool third = true);
    extern void footstep(gameent *d, int curfoot = -1);
    extern bool canregenimpulse(gameent *d);
}

namespace entities
{
    extern int showentdescs, simpleitems;
    extern vector<extentity *> ents;
    extern int lastuse(int type);
    extern bool execitem(int n, dynent *d, vec &pos, float dist);
    extern bool collateitems(dynent *d, vec &pos, float radius, vector<actitem> &actitems);
    extern void checkitems(dynent *d);
    extern void putitems(packetbuf &p);
    extern void execlink(gameent *d, int index, bool local, int ignore = -1);
    extern void setspawn(int n, int m);
    extern bool tryspawn(dynent *d, const vec &o, float yaw = 0, float pitch = 0);
    extern void spawnplayer(gameent *d, int ent = -1, bool suicide = false);
    extern const char *entinfo(int type, attrvector &attr, bool full = false, bool icon = false);
    extern void useeffects(gameent *d, int ent, int ammoamt, bool spawn, int weap, int drop, int ammo = -1);
    extern const char *entmdlname(int type, attrvector &attr);
    extern const char *findname(int type);
    extern void adddynlights();
    extern void render();
    extern void update();
}
#endif
#include "capture.h"
#include "defend.h"
#include "bomber.h"

#endif
