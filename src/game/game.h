#ifndef CPP_GAME_HEADER
#define CPP_GAME_HEADER

#include "engine.h"

#define VERSION_GAMEID "fps"
#define VERSION_GAME 281
#define VERSION_DEMOMAGIC "RED_ECLIPSE_DEMO"

#define MAXAI 256
#define MAXPLAYERS (MAXCLIENTS+MAXAI*2)
#define MAXPARAMS 256

// network quantization scale
#define DMF 16.0f // for world locations
#define DNF 1000.0f // for normalized vectors
#define DVELF 1.0f // for playerspeed based velocity vectors

#define FOOTSTEP_DIST 1.5f // Threshold, below which a footstep will be registered

enum
{
    S_JUMP = S_GAMESPECIFIC, S_IMPULSE, S_IMPULSE_ACTION, S_IMPULSE_SLIDE, S_LAND, S_FOOTSTEP_L, S_FOOTSTEP_R, S_SWIMSTEP, S_PAIN, S_DEATH,
    S_SPLASH1, S_SPLASH2, S_SPLOSH, S_DEBRIS, S_BURNLAVA,
    S_EXTINGUISH, S_SHELL, S_ITEMUSE, S_ITEMSPAWN,
    S_REGEN_BEGIN, S_REGEN_BOOST, S_REGEN_DECAY, S_CRITICAL, S_DAMAGE_TICK, S_DAMAGE, S_DAMAGE2, S_DAMAGE3, S_DAMAGE4, S_DAMAGE5, S_DAMAGE6, S_DAMAGE7, S_DAMAGE8,
    S_BURNED, S_BLEED, S_SHOCK, S_CORRODE, S_RESPAWN, S_CHAT, S_ERROR, S_ALARM, S_PRIZELOOP, S_OPENPRIZE, S_CATCH, S_DROP, S_BOUNCE,
    S_GAME
};

enum                                // entity types
{
    NOTUSED = ET_EMPTY, LIGHT = ET_LIGHT, MAPMODEL = ET_MAPMODEL, PLAYERSTART = ET_PLAYERSTART, ENVMAP = ET_ENVMAP, PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND, LIGHTFX = ET_LIGHTFX, DECAL = ET_DECAL, WIND = ET_WIND, MAPUI = ET_MAPUI, SOUNDENV = ET_SOUNDENV, PHYSICS = ET_PHYSICS,
    WEAPON = ET_GAMESPECIFIC, TELEPORT, ACTOR, TRIGGER, PUSHER, AFFINITY, CHECKPOINT, ROUTE, RAIL, CAMERA,
    MAXENTTYPES
};

#define EU_ENUM(en, um) \
    en(um, None, NONE) en(um, Item, ITEM) en(um, Automatic, AUTO) en(um, Action, ACT) en(um, Max, MAX)
ENUM_DLN(EU);

#define TRIG_ENUM(en, um) \
    en(um, Toggle, TOGGLE) en(um, Linked, LINKED) en(um, Script, SCRIPT) en(um, Once, ONCE) en(um, Exit, EXIT) en(um, Max, MAX)
ENUM_DLN(TRIG);

#define TRIG_A_ENUM(en, um) \
    en(um, Manual, MANUAL) en(um, Automatic, AUTO) en(um, Action, ACTION) en(um, Max, MAX)
ENUM_DLN(TRIG_A);

#define TRIG_S_ENUM(en, um) \
    en(um, Inverted, INVERTED) en(um, Routed, ROUTED) en(um, One Way, ONEWAY) en(um, Persist, PERSIST) en(um, Max, MAX)
ENUM_DLN(TRIG_S);
ENUM_VAR(TRIG_S_ALL, (1<<TRIG_S_INVERTED)|(1<<TRIG_S_ROUTED)|(1<<TRIG_S_ONEWAY)|(1<<TRIG_S_PERSIST));

enum { RAIL_YAW = 0, RAIL_PITCH, RAIL_SEEK, RAIL_SPLINE, RAIL_SPEED, RAIL_PREV, RAIL_NEXT, RAIL_MAX, RAIL_ALL = (1<<RAIL_YAW)|(1<<RAIL_PITCH)|(1<<RAIL_SEEK)|(1<<RAIL_SPLINE)|(1<<RAIL_SPEED)|(1<<RAIL_PREV)|(1<<RAIL_NEXT) };
enum { CAMERA_NORMAL = 0, CAMERA_MAPSHOT, CAMERA_MAX };
enum { CAMERA_F_STATIC = 0, CAMERA_F_MAX, CAMERA_F_ALL = (1<<CAMERA_F_STATIC)};

#define TRIGGERIDS      16
#define TRIGSTATE(a,b)  (b%2 ? !a : a)
#define TRIGGERTIME     1000
#define TRIGGERDELAY    500
#define TRIGGERMULTI    5

enum { CP_RESPAWN = 0, CP_START, CP_FINISH, CP_LAST, CP_MAX, CP_ALL = (1<<CP_RESPAWN)|(1<<CP_START)|(1<<CP_FINISH)|(1<<CP_LAST) };

enum { TELE_NOAFFIN = 0, TELE_MAX };
enum { MDLF_HIDE = 0, MDLF_NOCLIP, MDLF_NOSHADOW, MDLF_NOTRIGCOL, MDLF_MAX };

struct enttypes
{
    int type,           priority, links,    radius, usetype,    numattrs,       palattr,    modesattr,  idattr, mvattr, fxattr,     yawattr,    pitchattr,
            canlink, reclink, canuse;
    bool    noisy,  syncs,  resyncs,    syncpos,    synckin;
    const char *name,           *displayname,       *attrs[MAXENTATTRS];
};
#ifdef CPP_GAME_SERVER
extern const enttypes enttype[] = {
    {
        NOTUSED,        -1,         0,      0,      EU_NONE,    0,              -1,         -1,         -1,     -1,     -1,         -1,         -1,
            0, 0, 0,
            true,   false,  false,      false,      false,
                "none",         "NONE",             { "" }
    },
    {
        LIGHT,          1,          59,     0,      EU_NONE,    13,             7,          9,          -1,     11,      12,        -1,         -1,
            (1<<LIGHTFX), (1<<LIGHTFX), 0,
            false,  false,  false,      false,      false,
                "light",        "Light",            { "radius", "red", "green", "blue", "flare", "fscale", "flags", "palette", "palindex", "modes", "muts", "variant", "fxlevel"  }
    },
    {
        MAPMODEL,       1,          58,     0,      EU_NONE,    22,             8,          13,         -1,     15,     16,         1,          2,
            (1<<TRIGGER)|(1<<RAIL), (1<<TRIGGER), 0,
            false,  false,  false,      false,      false,
                "mapmodel",     "Map Model",        { "type", "yaw", "pitch", "roll", "blend", "scale", "flags", "colour", "palette", "palindex", "spinyaw", "spinpitch", "spinroll", "modes", "muts", "variant", "fxlevel", "lodoff", "anim", "aspeed", "aoffset", "shadowdist" }
    },
    {
        PLAYERSTART,    1,          59,     0,      EU_NONE,    7,              -1,         3,          5,      6,      -1,         1,          2,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            0,
            false,  true,  false,      false,      false,
                "playerstart",  "Player Start",     { "team", "yaw", "pitch", "modes", "muts", "id", "variant" }
    },
    {
        ENVMAP,         1,          260,    0,      EU_NONE,    3,              -1,         -1,         -1,     -1,     -1,         -1,         -1,
            (1<<TELEPORT), (1<<TELEPORT), 0,
            false,  false,  false,      false,      false,
                "envmap",       "Envrionment Map",  { "radius", "size", "blur" }
    },
    {
        PARTICLES,      1,          59,     0,      EU_NONE,    19,             -1,         13,         -1,     15,     16,         -1,         -1,
            (1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT)|(1<<RAIL),
            (1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            0,
            false,  false,  false,      false,      false,
                "particles",    "Particles",        { "type", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "millis", "modes", "muts", "variant", "fxlevel", "hintcolour", "hintblend" }
    },
    {
        MAPSOUND,       1,          58,     0,      EU_NONE,    12,             -1,         7,          -1,     9,      -1,         -1,         -1,
            (1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT)|(1<<WIND)|(1<<RAIL),
            (1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT)|(1<<WIND),
            0,
            false,  false,  false,      false,      false,
                "sound",        "Sound",            { "type", "gain", "pitch", "rolloff", "refdist", "maxdist", "flags", "modes", "muts", "variant", "offset", "groupid" }
    },
    {
        LIGHTFX,        1,          1,      0,      EU_NONE,    9,              -1,         5,          -1,     7,      8,          -1,         -1,
            (1<<LIGHT)|(1<<TELEPORT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT)|(1<<RAIL),
            (1<<LIGHT)|(1<<TRIGGER)|(1<<PUSHER)|(1<<PLAYERSTART)|(1<<CHECKPOINT),
            0,
            false,  false,  false,      false,      false,
                "lightfx",      "Light FX",         { "type", "mod", "min", "max", "flags", "modes", "muts", "variant", "fxlevel" }
    },
    {
        DECAL,          1,          0,      0,      EU_NONE,    13,             7,          9,          -1,     11,      12,            1,          2,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "decal",        "Decal",            { "type", "yaw", "pitch", "roll", "scale", "blend", "colour", "palette", "palindex", "modes", "muts", "variant", "fxlevel" }
    },
    {
        WIND,           1,          239,    0,      EU_NONE,    11,             -1,         7,         -1,     9,      10,              1,          -1,
            (1<<MAPSOUND),
            (1<<MAPSOUND),
            0,
            false,  false,  false,      false,      false,
                "wind",        "Wind",              { "mode", "yaw", "speed", "radius", "atten", "interval", "implen", "modes", "muts", "variant", "fxlevel" }
    },
    {
        MAPUI,          1,          241,    0,      EU_NONE,    14,             -1,         10,         -1,     12,     13,             2,          3,
            0, 0,
            0,
            false,  false,  false,      false,      false,
                "mapui",       "Map UI",            { "type", "flags", "yaw", "pitch", "radius", "scale", "yawdet", "pitchdet", "param", "blend", "modes", "muts", "variant", "fxlevel" }
    },
    {
        SOUNDENV,        1,          0,     0,      EU_NONE,    8,              -1,         -1,        -1,     -1,      -1,             -1,         -1,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "soundenv",     "Sound Env",        { "type", "width", "length", "height", "fadevals1", "fadevals2", "fadevals3", "fadevals4" }
    },
    {
        PHYSICS,         1,          0,     16,      EU_NONE,   9,              -1,          6,         -1,     8,      -1,             -1,         -1,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "physics",      "Physics",          { "type", "value", "width", "length", "height", "falloff", "modes", "muts", "variant" }
    },
    {
        WEAPON,         2,          59,     16,     EU_ITEM,    6,              -1,         2,          4,      5,      -1,             -1,         -1,
            0, 0,
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   true,      false,      false,
                "weapon",       "Weapon",           { "type", "flags", "modes", "muts", "id", "variant" }
    },
    {
        TELEPORT,       1,          50,     16,     EU_AUTO,    17,             6,          9,          -1,     11,      -1,            0,          1,
            (1<<ENVMAP)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX)|(1<<TELEPORT),
            (1<<ENVMAP)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI)|(1<<ENT_PROJ),
            false,  false,  false,      false,      false,
                "teleport",     "Teleport",         { "yaw", "pitch", "push", "radius", "colour", "type", "palette", "palindex", "flags", "modes", "muts", "variant", "blend", "size", "envblend", "hintcolour", "hintblend" }
    },
    {
        ACTOR,          1,          59,     0,      EU_NONE,    11,             -1,         3,          5,      10,     -1,             1,          2,
            (1<<TRIGGER),
            0,
            0,
            false,  true,   false,      true,       false,
                "actor",        "Actor",            { "type", "yaw", "pitch", "modes", "muts", "id", "weap", "health", "speed", "scale", "variant" }
    },
    {
        TRIGGER,        1,          58,     16,     EU_AUTO,    8,              -1,         5,          -1,     7,      -1,             -1,         -1,
            (1<<MAPMODEL)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<MAPMODEL)|(1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   true,       false,      true,
                "trigger",      "Trigger",          { "id", "type", "action", "radius", "state", "modes", "muts", "variant" }
    },
    {
        PUSHER,         1,          58,     16,     EU_AUTO,    11,             -1,         6,          -1,     9,      -1,             0,          1,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX)|(1<<PUSHER),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI)|(1<<ENT_PROJ),
            false,  false,  false,      false,      false,
                "pusher",       "Pusher",           { "yaw", "pitch", "force", "maxrad", "minrad", "type", "modes", "muts", "id", "variant", "sdelay" }
    },
    {
        AFFINITY,       1,          48,     32,     EU_NONE,    7,              -1,         3,          5,      6,      -1,             1,          2,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "affinity",     "Affinity",         { "team", "yaw", "pitch", "modes", "muts", "id", "variant" }
    },
    {
        CHECKPOINT,     1,          48,     16,     EU_AUTO,    8,              -1,         3,          5,      7,      -1,             1,          2,
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX)|(1<<CHECKPOINT),
            (1<<MAPSOUND)|(1<<PARTICLES)|(1<<LIGHTFX),
            (1<<ENT_PLAYER)|(1<<ENT_AI),
            false,  true,   false,      true,       true,
                "checkpoint",   "Check Point",      { "radius", "yaw", "pitch", "modes", "muts", "id", "type", "variant" }
    },
    {
        ROUTE,          1,          224,    16,     EU_NONE,    6,              -1,         -1,         -1,     -1,     -1,              1,         2,
            (1<<ROUTE), 0, 0,
            false,   false,  false,      false,      false,
                "route",         "Route",           { "num", "yaw", "pitch", "move", "strafe", "action" }
    },
    {
        RAIL,           -1,         228,    0,      EU_NONE,    13,             -1,         10,         -1,     12,     -1,              2,         3,
            (1<<LIGHT)|(1<<MAPMODEL)|(1<<PLAYERSTART)|(1<<PARTICLES)|(1<<MAPSOUND)|(1<<PHYSICS)|(1<<MAPUI)|(1<<LIGHTFX)|(1<<WEAPON)|(1<<TELEPORT)|(1<<ACTOR)|(1<<TRIGGER)|(1<<PUSHER)|(1<<RAIL)|(1<<CAMERA), 0, 0,
            false,   false,  false,      false,      false,
                "rail",         "Rail",             { "time", "flags", "yaw", "pitch", "rotlen", "rotwait", "collide", "anim", "aspeed", "aoffset", "modes", "muts", "variant" }
    },
    {
        CAMERA,         -1,         252,    0,      EU_NONE,    12,             -1,         7,          9,      10,      -1,            2,          3,
            0, 0, 0,
            false,   false,  false,      false,      false,
                "camera",       "Camera",           { "type", "flags", "yaw", "pitch", "maxdist", "mindist", "delay", "modes", "muts", "id", "variant", "fov" }
    }
};
#else
extern const enttypes enttype[];
namespace entities
{
    extern vector<extentity *> ents;
    extern void removepassenger(physent *d);
}
#endif
#define isent(a) (a >= NOTUSED && a < MAXENTTYPES)

#define MAXNAMELEN 24

#define SAY_ENUM(en, um) en(um, Message, MESSAGE, 1<<0) en(um, Action, ACTION, 1<<1) en(um, Team, TEAM, 1<<2) en(um, Whisper, WHISPER, 1<<3)
ENUM_ALN(SAY);

#define PRIV_ENUM(en, um) \
    en(um, None, NONE) en(um, Player, PLAYER) en(um, Supporter, SUPPORTER) en(um, Moderator, MODERATOR) \
    en(um, Administrator, ADMINISTRATOR) en(um, Developer, DEVELOPER) en(um, Creator, CREATOR) en(um, Max, MAX)
ENUM_DLN(PRIV);
ENUM_VAR(PRIV_START, PRIV_PLAYER);
ENUM_VAR(PRIV_ELEVATED, PRIV_MODERATOR);
ENUM_VAR(PRIV_LAST, PRIV_CREATOR);
ENUM_VAR(PRIV_TYPE, 0xFF);
ENUM_VAR(PRIV_LOCAL, 1<<8);

#define MASTERMODE_ENUM(en, um) \
    en(um, Open, OPEN) en(um, Veto, VETO) en(um, Locked, LOCKED) en(um, Private, PRIVATE) en(um, Password, PASSWORD) en(um, Max, MAX)
ENUM_DLN(MASTERMODE);
ENUM_VAR(MASTERMODE_MODE, 0xF);
ENUM_VAR(MASTERMODE_AUTOAPPROVE, 0x1000);
ENUM_VAR(MASTERMODE_FREESERV, (MASTERMODE_AUTOAPPROVE|MASTERMODE_MODE));
ENUM_VAR(MASTERMODE_VETOSERV, ((1<<MASTERMODE_OPEN)|(1<<MASTERMODE_VETO)));
ENUM_VAR(MASTERMODE_COOPSERV, (MASTERMODE_AUTOAPPROVE|MASTERMODE_VETOSERV|(1<<MASTERMODE_LOCKED)));
ENUM_VAR(MASTERMODE_OPENSERV, (MASTERMODE_AUTOAPPROVE|(1<<MASTERMODE_OPEN)));

#define SINFO_ENUM(en, um) \
    en(um, None, NONE) en(um, Status, STATUS) en(um, Name, NAME) en(um, Port, PORT) en(um, Query Port, QPORT) en(um, Description, DESC) \
    en(um, Mode, MODE) en(um, Mutators, MUTS) en(um, Map, MAP) en(um, Time, TIME) en(um, Num Players, NUMPLRS) en(um, Max Players, MAXPLRS) \
    en(um, Ping, PING) en(um, Priority, PRIO) en(um, Max, MAX)
ENUM_DLN(SINFO);

#define SSTAT_ENUM(en, um) \
    en(um, Open, OPEN) en(um, Locked, LOCKED) en(um, Private, PRIVATE) en(um, Full, FULL) en(um, Unknown, UNKNOWN) en(um, Max, MAX)
ENUM_DLN(SSTAT);

#define AC_ENUM(en, um) \
    en(um, Primary, PRIMARY) en(um, Secondary, SECONDARY) en(um, Reload, RELOAD) en(um, Use, USE) en(um, Jump, JUMP) en(um, Walk, WALK) \
    en(um, Crouch, CROUCH) en(um, Special, SPECIAL) en(um, Drop, DROP) en(um, Affinity, AFFINITY) en(um, Dash, DASH) en(um, Max, MAX)
ENUM_DLN(AC);
ENUM_VAR(AC_ALL, (1<<AC_PRIMARY)|(1<<AC_SECONDARY)|(1<<AC_RELOAD)|(1<<AC_USE)|(1<<AC_JUMP)|(1<<AC_WALK)|(1<<AC_CROUCH)|(1<<AC_SPECIAL)|(1<<AC_DROP)|(1<<AC_AFFINITY)|(1<<AC_DASH));

#define IM_ENUM(en, um) \
    en(um, Meter, METER) en(um, Count, COUNT) en(um, Type, TYPE) en(um, Regen, REGEN) en(um, Slip, SLIP) en(um, Pusher, PUSHER) \
    en(um, Collect Meter, COLLECT_METER) en(um, Last Meter, LASTCOL_METER) en(um, Collect Count, COLLECT_COUNT) en(um, Last Count, LASTCOL_COUNT) \
    en(um, Max, MAX)
ENUM_DLN(IM);
ENUM_VAR(IM_ALL, (1<<IM_METER)|(1<<IM_COUNT)|(1<<IM_TYPE)|(1<<IM_REGEN)|(1<<IM_SLIP)|(1<<IM_PUSHER)|(1<<IM_COLLECT_METER)|(1<<IM_LASTCOL_METER)|(1<<IM_COLLECT_COUNT)|(1<<IM_LASTCOL_COUNT));
ENUM_VAR(IM_CHECKPOINT, (1<<IM_COUNT)|(1<<IM_COLLECT_COUNT)|(1<<IM_LASTCOL_COUNT));

#define IM_T_ENUM(en, um) \
    en(um, Jump, JUMP) en(um, Boost, BOOST) en(um, Dash, DASH) en(um, Slide, SLIDE) en(um, Launch, LAUNCH) en(um, Melee, MELEE) en(um, Kick, KICK) en(um, Grab, GRAB) \
    en(um, Wallrun, WALLRUN) en(um, Vault, VAULT) en(um, Pound, POUND) en(um, Pusher, PUSHER) en(um, Max, MAX)
ENUM_DLN(IM_T);
ENUM_VAR(IM_T_ALL, (1<<IM_T_JUMP)|(1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_MELEE)|(1<<IM_T_KICK)|(1<<IM_T_GRAB)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_ACTION, (1<<IM_T_JUMP)|(1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_MELEE)|(1<<IM_T_KICK)|(1<<IM_T_GRAB)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_COAST, (1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_REGEN, (1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_MELEE)|(1<<IM_T_KICK)|(1<<IM_T_GRAB)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND));
ENUM_VAR(IM_T_PUSH, (1<<IM_T_JUMP)|(1<<IM_T_BOOST)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_COUNT, (1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_WALLRUN)|(1<<IM_T_POUND));
ENUM_VAR(IM_T_CHECK, (1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_ISTOUCH, (1<<IM_T_JUMP)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_MELEE)|(1<<IM_T_KICK)|(1<<IM_T_GRAB)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_NEEDTOUCH, (1<<IM_T_BOOST));
ENUM_VAR(IM_T_RELAX, (1<<IM_T_VAULT));
ENUM_VAR(IM_T_MVAI, (1<<IM_T_JUMP)|(1<<IM_T_BOOST)|(1<<IM_T_DASH)|(1<<IM_T_SLIDE)|(1<<IM_T_LAUNCH)|(1<<IM_T_MELEE)|(1<<IM_T_KICK)|(1<<IM_T_GRAB)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT)|(1<<IM_T_POUND)|(1<<IM_T_PUSHER));
ENUM_VAR(IM_T_LSAI, (1<<IM_T_JUMP));
ENUM_VAR(IM_T_ROLLER, (1<<IM_T_JUMP)|(1<<IM_T_WALLRUN)|(1<<IM_T_VAULT));

#define SPHY_ENUM(en, um) \
    en(um, Jump, JUMP) en(um, Boost, BOOST) en(um, Dash, DASH) en(um, Slide, SLIDE) en(um, Launch, LAUNCH) en(um, Melee, MELEE) en(um, Kick, KICK) en(um, Grab, GRAB) \
    en(um, Wallrun, WALLRUN) en(um, Vault, VAULT) en(um, Pound, POUND) en(um, Material, MATERIAL) en(um, Prize, PRIZE) en(um, Switch, SWITCH) en(um, Extinguish, EXTINGUISH) \
    en(um, Buff, BUFF) en(um, Max, MAX)
ENUM_DLN(SPHY);
ENUM_VAR(SPHY_SERVER, (1<<SPHY_EXTINGUISH)|(1<<SPHY_BUFF));

#define CROUCHLOW 0.7f
#define CROUCHHIGH 0.9f
#define GUARDRADIUS 3.f
#define PHYSMILLIS 250
#define DEATHMILLIS 300

enum
{
    ANIM_PAIN = ANIM_GAMESPECIFIC,
    ANIM_JUMP_FORWARD, ANIM_JUMP_BACKWARD, ANIM_JUMP_LEFT, ANIM_JUMP_RIGHT, ANIM_JUMP,
    ANIM_RUN_FORWARD, ANIM_RUN_BACKWARD, ANIM_RUN_LEFT, ANIM_RUN_RIGHT,
    ANIM_BOOST_FORWARD, ANIM_BOOST_BACKWARD, ANIM_BOOST_LEFT, ANIM_BOOST_RIGHT, ANIM_BOOST_UP,
    ANIM_PARKOUR_LEFT, ANIM_PARKOUR_RIGHT, ANIM_PARKOUR_UP, ANIM_PARKOUR_JUMP,
    ANIM_VAULT, ANIM_POWERSLIDE, ANIM_FLYKICK,
    ANIM_SINK, ANIM_EDIT, ANIM_WIN, ANIM_LOSE,
    ANIM_CROUCH, ANIM_CRAWL_FORWARD, ANIM_CRAWL_BACKWARD, ANIM_CRAWL_LEFT, ANIM_CRAWL_RIGHT,
    ANIM_CROUCH_JUMP_FORWARD, ANIM_CROUCH_JUMP_BACKWARD, ANIM_CROUCH_JUMP_LEFT, ANIM_CROUCH_JUMP_RIGHT, ANIM_CROUCH_JUMP,
    ANIM_CLAW, ANIM_CLAW_PRIMARY, ANIM_CLAW_SECONDARY, ANIM_CLAW_RELOAD, ANIM_CLAW_POWER, ANIM_CLAW_ZOOM,
    ANIM_PISTOL, ANIM_PISTOL_PRIMARY, ANIM_PISTOL_SECONDARY, ANIM_PISTOL_RELOAD, ANIM_PISTOL_POWER, ANIM_PISTOL_ZOOM,
    ANIM_SWORD, ANIM_SWORD_PRIMARY, ANIM_SWORD_SECONDARY, ANIM_SWORD_POWER, ANIM_SWORD_ZOOM,
    ANIM_SHOTGUN, ANIM_SHOTGUN_PRIMARY, ANIM_SHOTGUN_SECONDARY, ANIM_SHOTGUN_RELOAD, ANIM_SHOTGUN_POWER, ANIM_SHOTGUN_ZOOM,
    ANIM_SMG, ANIM_SMG_PRIMARY, ANIM_SMG_SECONDARY, ANIM_SMG_RELOAD, ANIM_SMG_POWER, ANIM_SMG_ZOOM,
    ANIM_FLAMER, ANIM_FLAMER_PRIMARY, ANIM_FLAMER_SECONDARY, ANIM_FLAMER_RELOAD, ANIM_FLAMER_POWER, ANIM_FLAMER_ZOOM,
    ANIM_PLASMA, ANIM_PLASMA_PRIMARY, ANIM_PLASMA_SECONDARY, ANIM_PLASMA_RELOAD, ANIM_PLASMA_POWER, ANIM_PLASMA_ZOOM,
    ANIM_ZAPPER, ANIM_ZAPPER_PRIMARY, ANIM_ZAPPER_SECONDARY, ANIM_ZAPPER_RELOAD, ANIM_ZAPPER_POWER, ANIM_ZAPPER_ZOOM,
    ANIM_RIFLE, ANIM_RIFLE_PRIMARY, ANIM_RIFLE_SECONDARY, ANIM_RIFLE_RELOAD, ANIM_RIFLE_POWER, ANIM_RIFLE_ZOOM,
    ANIM_CORRODER, ANIM_CORRODER_PRIMARY, ANIM_CORRODER_SECONDARY, ANIM_CORRODER_RELOAD, ANIM_CORRODER_POWER, ANIM_CORRODER_ZOOM,
    ANIM_GRENADE, ANIM_GRENADE_PRIMARY, ANIM_GRENADE_SECONDARY, ANIM_GRENADE_RELOAD, ANIM_GRENADE_POWER, ANIM_GRENADE_ZOOM,
    ANIM_MINE, ANIM_MINE_PRIMARY, ANIM_MINE_SECONDARY, ANIM_MINE_RELOAD, ANIM_MINE_POWER, ANIM_MINE_ZOOM,
    ANIM_ROCKET, ANIM_ROCKET_PRIMARY, ANIM_ROCKET_SECONDARY, ANIM_ROCKET_RELOAD, ANIM_ROCKET_POWER, ANIM_ROCKET_ZOOM,
    ANIM_MINIGUN, ANIM_MINIGUN_PRIMARY, ANIM_MINIGUN_SECONDARY, ANIM_MINIGUN_RELOAD, ANIM_MINIGUN_POWER, ANIM_MINIGUN_ZOOM,
    ANIM_JETSAW, ANIM_JETSAW_PRIMARY, ANIM_JETSAW_SECONDARY, ANIM_JETSAW_RELOAD, ANIM_JETSAW_POWER, ANIM_JETSAW_ZOOM,
    ANIM_ECLIPSE, ANIM_ECLIPSE_PRIMARY, ANIM_ECLIPSE_SECONDARY, ANIM_ECLIPSE_RELOAD, ANIM_ECLIPSE_POWER, ANIM_ECLIPSE_ZOOM,
    ANIM_SWITCH, ANIM_USE, ANIM_SPRINT,
    ANIM_MAX
};

#define RESIDUALS \
    RESIDUAL(burn, BURN, BURN); \
    RESIDUAL(bleed, BLEED, BLEED); \
    RESIDUAL(shock, SHOCK, SHOCK); \
    RESIDUAL(corrode, CORRODE, CORRODE);

#define FRAG_ENUM(en, um) \
    en(um, NONE, 0) en(um, HEADSHOT, 1<<0) en(um, OBLITERATE, 1<<1) \
    en(um, SPREE1, 1<<2) en(um, SPREE2, 1<<3) en(um, SPREE3, 1<<4) en(um, SPREE4, 1<<5) \
    en(um, MKILL1, 1<<6) en(um, MKILL2, 1<<7) en(um, MKILL3, 1<<8) \
    en(um, REVENGE, 1<<9) en(um, DOMINATE, 1<<10) en(um, FIRSTBLOOD, 1<<11) en(um, BREAKER, 1<<12) \
    en(um, SPREES, 4) en(um, SPREE, 2) en(um, MKILL, 6) \
    en(um, CHECK, um##_SPREE1|um##_SPREE2|um##_SPREE3|um##_SPREE4) \
    en(um, MULTI, um##_MKILL1|um##_MKILL2|um##_MKILL3)
ENUM_AL(FRAG);

enum
{
    SENDMAP_MPZ = 0, SENDMAP_CFG, SENDMAP_PNG, SENDMAP_TXT, SENDMAP_GAME, SENDMAP_WPT = SENDMAP_GAME, SENDMAP_MAX,
    SENDMAP_MIN = SENDMAP_PNG, SENDMAP_HAS = SENDMAP_MIN+1, SENDMAP_EDIT = SENDMAP_CFG+1, SENDMAP_ALL = SENDMAP_MAX-1
};
#ifdef CPP_GAME_SERVER
extern const char * const sendmaptypes[SENDMAP_MAX] = { "mpz", "cfg", "png", "txt", "wpt" };
#else
extern const char * const sendmaptypes[SENDMAP_MAX];
// Client Getters
#define CLCOMMANDK(name, body, fail) \
    ICOMMAND(IDF_NAMECOMPLETE, getclient##name, "s", (char *who), \
    { \
        gameent *d = game::getclient(client::parseplayer(who)); \
        if(!d) { fail; return; } \
        body; \
    });
#define CLCOMMAND(name, body) CLCOMMANDK(name, body,)
#define CLCOMMANDMK(name, fmt, args, body, fail) \
    ICOMMAND(IDF_NAMECOMPLETE, getclient##name, fmt, args, \
    { \
        gameent *d = game::getclient(client::parseplayer(who)); \
        if(!d) { fail; return; } \
        body; \
    });
#define CLCOMMANDM(name, fmt, args, body) CLCOMMANDMK(name, fmt, args, body,)
// Projectile Getters
#define PRCOMMANDK(name, body, fail) \
    ICOMMAND(0, getproj##name, "ib", (int *seqid, int *type), \
    { \
        projent *p = projs::findprojseq(*type, *seqid); \
        if(!p) { fail; return; } \
        body; \
    });
#define PRCOMMAND(name, body) PRCOMMANDK(name, body,)
#define PRCOMMANDMK(name, fmt, args, body, fail) \
    ICOMMAND(0, getproj##name, fmt, args, \
    { \
        projent *p = projs::findprojseq(*type, *seqid); \
        if(!p) { fail; return; } \
        body; \
    });
#define PRCOMMANDM(name, fmt, args, body) PRCOMMANDMK(name, fmt, args, body,)
#endif

#include "gamemode.h"
#include "weapons.h"

// network messages codes, c2s, c2c, s2c
enum
{
    N_CONNECT = 0, N_SERVERINIT, N_WELCOME, N_CLIENTINIT, N_POS, N_SPHY, N_TEXT, N_COMMAND, N_GAMELOG, N_DISCONNECT,
    N_SHOOT, N_DESTROY, N_STICKY, N_SUICIDE, N_DIED, N_POINTS,
    N_DAMAGE, N_BURNRES, N_BLEEDRES, N_SHOCKRES, N_CORRODERES,
    N_SHOTFX, N_LOADOUT, N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_WEAPDROP, N_WEAPSELECT, N_WEAPCOOK,
    N_MAPCHANGE, N_MAPVOTE, N_CLEARVOTE, N_CHECKPOINT, N_ITEMSPAWN, N_ITEMUSE, N_TRIGGER, N_EXECLINK,
    N_PING, N_PONG, N_CLIENTPING, N_TICK, N_ITEMACC, N_SERVMSG, N_GETGAMEINFO, N_GAMEINFO, N_GAMESERVINFO, N_ATTRMAP, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITLINK, N_EDITVAR, N_EDITF, N_EDITT, N_EDITM, N_FLIP,
    N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE,
    N_CALCLIGHT, N_REMIP, N_EDITVSLOT, N_UNDO, N_REDO, N_CLIPBOARD, N_NEWMAP,
    N_GETMAP, N_SENDMAP, N_FAILMAP, N_SENDMAPFILE,
    N_MASTERMODE, N_ADDCONTROL, N_CLRCONTROL, N_CURRENTPRIV, N_SPECTATOR, N_WAITING, N_SETPRIV, N_SETTEAM, N_ADDPRIV,
    N_SETUPAFFIN, N_INFOAFFIN, N_MOVEAFFIN,
    N_TAKEAFFIN, N_RETURNAFFIN, N_RESETAFFIN, N_DROPAFFIN, N_SCOREAFFIN, N_INITAFFIN, N_SCORE,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO, N_DEMOREADY, N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_CLIENT, N_RELOAD, N_REGEN, N_INITAI, N_MAPCRC,
    N_SETPLAYERINFO, N_SWITCHTEAM, N_AUTHTRY, N_AUTHCHAL, N_AUTHANS, N_QUEUEPOS, N_STEAMCHAL, N_STEAMANS, N_STEAMFAIL,
    NUMMSG
};

enum
{
    SS_F_NONE = 0,
    SS_F_STEAMAUTH = 1<<0,
    SS_F_ALL = SS_F_STEAMAUTH
};

#ifdef CPP_GAME_SERVER
char msgsizelookup(int msg)
{
    static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
    {
        N_CONNECT, 0, N_SERVERINIT, 5, N_WELCOME, 2, N_CLIENTINIT, 0, N_POS, 0, N_SPHY, 0, N_TEXT, 0, N_COMMAND, 0, N_GAMELOG, 0, N_DISCONNECT, 3,
        N_SHOOT, 0, N_DESTROY, 0, N_STICKY, 0, N_SUICIDE, 4, N_DIED, 0, N_POINTS, 5,
        N_DAMAGE, 16, N_BURNRES, 0, N_BLEEDRES, 0, N_SHOCKRES, 0, N_CORRODERES, 0,
        N_SHOTFX, 0, N_LOADOUT, 0, N_TRYSPAWN, 2, N_SPAWNSTATE, 0, N_SPAWN, 0, N_WEAPDROP, 0, N_WEAPSELECT, 0, N_WEAPCOOK, 0,
        N_MAPCHANGE, 0, N_MAPVOTE, 0, N_CLEARVOTE, 0, N_CHECKPOINT, 0, N_ITEMSPAWN, 4, N_ITEMUSE, 0, N_TRIGGER, 0, N_EXECLINK, 3,
        N_PING, 2, N_PONG, 2, N_CLIENTPING, 2, N_TICK, 5, N_ITEMACC, 0, N_SERVMSG, 0, N_GETGAMEINFO, 0, N_GAMEINFO, 0, N_GAMESERVINFO, 2, N_ATTRMAP, 0, N_RESUME, 0,
        N_EDITMODE, 2, N_EDITENT, 0, N_EDITLINK, 4, N_EDITVAR, 0, N_EDITF, 16, N_EDITT, 16, N_EDITM, 17, N_FLIP, 14,
        N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14,
        N_CALCLIGHT, 1, N_REMIP, 1, N_EDITVSLOT, 16, N_UNDO, 0, N_REDO, 0, N_CLIPBOARD, 0, N_NEWMAP, 0,
        N_GETMAP, 0, N_SENDMAP, 0, N_FAILMAP, 0, N_SENDMAPFILE, 0,
        N_MASTERMODE, 0, N_ADDCONTROL, 0, N_CLRCONTROL, 2, N_CURRENTPRIV, 3, N_SPECTATOR, 3, N_WAITING, 2, N_SETPRIV, 0, N_SETTEAM, 0, N_ADDPRIV, 0,
        N_SETUPAFFIN, 0, N_INFOAFFIN, 0, N_MOVEAFFIN, 0,
        N_TAKEAFFIN, 0, N_RETURNAFFIN, 0, N_RESETAFFIN, 0, N_DROPAFFIN, 0, N_SCOREAFFIN, 0, N_INITAFFIN, 0, N_SCORE, 0,
        N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 3, N_SENDDEMO, 0, N_DEMOREADY, 0,
        N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
        N_CLIENT, 0, N_RELOAD, 0, N_REGEN, 0, N_INITAI, 0, N_MAPCRC, 0,
        N_SETPLAYERINFO, 0, N_SWITCHTEAM, 0, N_AUTHTRY, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_QUEUEPOS, 0,
        N_STEAMCHAL, 0, N_STEAMANS, 0, N_STEAMFAIL, 0,
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

struct demoheader
{
    char magic[16];
    int gamever, gamemode, mutators, starttime;
    string mapname;
};
#include "player.h"
#include "vars.h"
#ifndef CPP_GAME_SERVER
#include "ai.h"
#endif

template<class T>
static inline void adjustscaled(T &n, T &d, int s)
{
    if(n == (T)0) return;
    else if(n > 0)
    {
        n -= (T)(d*(curtime/float(s)));
        if(n < (T)0) n = (T)0;
    }
    else if(n < 0)
    {
        n += (T)(d*(curtime/float(s)));
        if(n > (T)0) n = (T)0;
    }
}

static inline void modecheck(int &mode, int &muts, int trying = 0)
{
    if(!m_game(mode)) mode = G_DEATHMATCH;
    if(gametype[mode].implied) muts |= gametype[mode].implied;
    int retries = G_M_MAX*G_M_MAX;
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
        loop(s, G_M_MAX)
        {
            if(!(mutsidx&(1<<mutstype[s].type)) && (muts&(1<<mutstype[s].type)))
            {
                muts &= ~(1<<mutstype[s].type);
                trying &= ~(1<<mutstype[s].type);
                changed = true;
                break;
            }
            if(muts&(1<<mutstype[s].type)) loopj(G_M_MAX)
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
        case MASTERMODE_OPEN: return "open";
        case MASTERMODE_VETO: return "veto";
        case MASTERMODE_LOCKED: return "locked";
        case MASTERMODE_PRIVATE: return "private";
        case MASTERMODE_PASSWORD: return "password";
        default: return "unknown";
    }
}

struct verinfo
{
    int type, flag, version, major, minor, patch, game, build, platform, arch, gpuglver, gpuglslver, crc;
    char *branch, *revision, *gpuvendor, *gpurenderer, *gpuversion;

    verinfo() : branch(NULL), revision(NULL), gpuvendor(NULL), gpurenderer(NULL), gpuversion(NULL) { reset(); }
    ~verinfo() { reset(); }

    void reset()
    {
        if(branch) delete[] branch;
        if(revision) delete[] revision;
        if(gpuvendor) delete[] gpuvendor;
        if(gpurenderer) delete[] gpurenderer;
        if(gpuversion) delete[] gpuversion;
        branch = revision = gpuvendor = gpurenderer = gpuversion = NULL;
        type = flag = version = major = minor = patch = game = arch = gpuglver = gpuglslver = crc = build = 0;
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
        build = getint(p);
        platform = getint(p);
        arch = getint(p);
        gpuglver = getint(p);
        gpuglslver = getint(p);
        crc = getint(p);
        if(branch) delete[] branch;
        getstring(text, p); branch = newstring(text, MAXBRANCHLEN);
        if(revision) delete[] revision;
        getstring(text, p); revision = newstring(text, MAXREVISIONLEN);
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
        putint(p, build);
        putint(p, platform);
        putint(p, arch);
        putint(p, gpuglver);
        putint(p, gpuglslver);
        putint(p, crc);
        sendstring(branch ? branch : "", p);
        sendstring(revision ? revision : "", p);
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
        build = v.build;
        platform = v.platform;
        arch = v.arch;
        gpuglver = v.gpuglver;
        gpuglslver = v.gpuglslver;
        crc = v.crc;
        if(branch) delete[] branch;
        branch = newstring(v.branch ? v.branch : "", MAXBRANCHLEN);
        if(revision) delete[] revision;
        revision = newstring(v.revision ? v.revision : "", MAXREVISIONLEN);
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
    int health, colours[2], model, checkpointspawn;
    int weapselect, weapammo[W_MAX][W_A_MAX], weapload[W_MAX][W_A_MAX], weapent[W_MAX], weapshot[W_MAX], weapstate[W_MAX], weapwait[W_MAX], weaptime[W_MAX], prevstate[W_MAX], prevtime[W_MAX];
    int lastdeath, lastspawn, lastpain, lastregen, lastregenamt, lastbuff, lastshoot, lastcook, lastaffinity, lastres[W_R_MAX], lastrestime[W_R_MAX];
    int burntime, burndelay, burndamage, bleedtime, bleeddelay, bleeddamage, shocktime, shockdelay, shockdamage, shockstun, shockstuntime, corrodetime, corrodedelay, corrodedamage;
    float shockstunscale, shockstunfall;
    int actortype, spawnpoint, ownernum, skill, points, frags, deaths, totalpoints, totalfrags, totaldeaths, spree, lasttimeplayed, timeplayed, cpmillis, cptime, queuepos, hasprize;
    bool quarantine;
    string vanity, mixer;
    vector<int> loadweap, lastweap, randweap, cpnodes;
    verinfo version;

    clientstate() : model(-1), checkpointspawn(1), weapselect(W_CLAW), lastdeath(0), lastspawn(0), lastpain(0), lastregen(0), lastregenamt(0), lastbuff(0), lastshoot(0), lastcook(0), lastaffinity(0),
        actortype(A_PLAYER), spawnpoint(-1), ownernum(-1), skill(0), points(0), frags(0), deaths(0), totalpoints(0), totalfrags(0), totaldeaths(0), spree(0), lasttimeplayed(0), timeplayed(0),
        cpmillis(0), cptime(0), queuepos(-1), hasprize(0), quarantine(false)
    {
        loopi(2) colours[i] = -1;
        vanity[0] = mixer[0] = '\0';
        loadweap.shrink(0);
        lastweap.shrink(0);
        randweap.shrink(0);
        cpnodes.shrink(0);
        resetresidual();
    }
    ~clientstate() {}

    bool setvanity(const char *v)
    {
        bool changed = strcmp(v, vanity);
        if(changed) copystring(vanity, v);
        return changed;
    }

    bool setmixer(const char *v)
    {
        bool changed = strcmp(v, mixer);
        if(changed) copystring(mixer, v);
        return changed;
    }

    int getammo(int weap, int millis = 0, bool store = false)
    {
        if(!isweap(weap)) return -1;
        if(weapammo[weap][W_A_CLIP] < 0) return -1;

        if(!(A(actortype, abilities)&(1<<A_A_AMMO))) return W(weap, ammoclip); // always max ammo

        int a = weapammo[weap][W_A_CLIP] > 0 ? weapammo[weap][W_A_CLIP] : 0;
        if(millis && weapstate[weap] == W_S_RELOAD && millis-weaptime[weap] < weapwait[weap] && weapload[weap][W_A_CLIP] > 0)
            a -= weapload[weap][W_A_CLIP];
        if(store) a += weapammo[weap][W_A_STORE];
        return a;
    }

    bool hasweap(int weap, int sweap, int level = 0, int exclude = -1)
    {
        if(isweap(weap) && weap != exclude)
        {
            int ammo = getammo(weap, 0, true);
            if(ammo >= 0) switch(level)
            {
                case 0: default: return true; break; // has weap at all
                case 1: if(w_carry(weap, sweap)) return true; break; // only carriable
                case 2: if(ammo > 0) return true; break; // only with actual ammo
                case 3: if(ammo > 0 || canreload(weap, sweap)) return true; break; // only reloadable or has ammo
                case 4: if(ammo >= (canreload(weap, sweap) ? 0 : W(weap, ammoclip))) return true; break; // only reloadable or those with < clipsize
                case 5: case 6: // special case to determine drop in classic games
                    if(weap == sweap || (level == 6 && weap < W_ITEM && weap >= W_OFFSET) || ammo > 0 || canreload(weap, sweap)) return true;
                    break;
            }
        }
        return false;
    }

    bool holdweap(int weap, int sweap, int millis)
    {
        return weap == weapselect || millis-weaptime[weap] < weapwait[weap] || hasweap(weap, sweap);
    }

    int holdweapcount(int sweap, int millis)
    {
        int n = 0;
        loopi(W_ALL) if(holdweap(i, sweap, millis)) n++;
        return n;
    }

    void addlastweap(int weap)
    {
        loopvrev(lastweap) if(lastweap[i] == weap) lastweap.remove(i);
        lastweap.add(weap);
    }

    int getlastweap(int sweap, int exclude = -1)
    {
        loopvrev(lastweap)
        {
            if(lastweap[i] == exclude) continue;
            if(hasweap(lastweap[i], sweap)) return lastweap[i];
        }
        return -1;
    }

    int bestweapcheck(int sweap, int level, int num = W_ALL, int exclude = -1)
    {
        loopvrev(lastweap) if(lastweap[i] < num && hasweap(lastweap[i], sweap, level, exclude)) return lastweap[i];
        loopirev(num) if(hasweap(i, sweap, level, exclude)) return i;
        return -1;
    }

    int bestweap(int sweap, bool ammo, bool pick = false, int exclude = -1)
    {
        int n = bestweapcheck(sweap, 3, W_ALL, exclude);
        if(isweap(n)) return n;
        if(ammo)
        {
            n = bestweapcheck(sweap, 2, W_ALL, exclude);
            if(isweap(n)) return n;
        }
        if(pick)
        {
            n = bestweapcheck(sweap, 0, W_ITEM, exclude);
            if(isweap(n)) return n;
        }
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
            weapwait[i] = weaptime[i] = weapshot[i] = prevtime[0] = 0;
            loopj(W_A_MAX) weapload[i][j] = 0;
            if(full)
            {
                weapammo[i][W_A_CLIP] = weapent[i] = -1;
                weapammo[i][W_A_STORE] = 0;
            }
        }
        if(full) lastweap.shrink(0);
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
                addlastweap(weapselect);
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
        if(!weapwait[weap] || weapstate[weap] == W_S_IDLE || (skip && skip&(1<<weapstate[weap]))) return true;
        int wait = weapwait[weap];
        if(W_S_INTERRUPT&(1<<weapstate[weap]))
        {
            if(W(weap, cookinterrupt)) return true;
            wait += 50;
        }
        return millis-weaptime[weap] >= wait;
    }

    bool candrop(int weap, int sweap, int millis, bool classic, int skip = 0)
    {
        if(!(A(actortype, abilities)&(1<<A_A_AMMO))) return false;

        if(weap < W_ALL && weap != sweap && (classic ? weap >= W_OFFSET && W2(weap, ammosub, false) && W2(weap, ammosub, true) : weap >= W_ITEM)
            && hasweap(weap, sweap) && weapwaited(weap, millis, skip) && weapwaited(weapselect, millis, skip))
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
        if(!(A(actortype, abilities)&(WS(flags) ? (1<<A_A_SECONDARY) : (1<<A_A_PRIMARY)))) return false;

        if(weap == weapselect || weap == W_MELEE)
            if(hasweap(weap, sweap) && getammo(weap, millis) >= (W2(weap, cooktime, WS(flags)) ? 1 : W2(weap, ammosub, WS(flags))) && weapwaited(weap, millis, W_S_INTERRUPT|skip))
                return true;
        return false;
    }

    bool canreload(int weap, int sweap, bool check = false, int millis = 0, int skip = 0)
    {
        if((W(weap, ammostore) < 0 || weapammo[weap][W_A_STORE] > 0 || !(A(actortype, abilities)&(1<<A_A_AMMO)))
                && (!check || (weap == weapselect && hasweap(weap, sweap) && weapammo[weap][W_A_CLIP] < W(weap, ammoclip) && weapstate[weap] != W_S_ZOOM && weapwaited(weap, millis, skip))))
            return true;
        return false;
    }

    bool canuseweap(int gamemode, int mutators, int attr, int sweap, int millis, int skip = 0, bool full = true)
    {
        if(!(A(actortype, abilities)&(1<<A_A_AMMO))) return false;
        if(!m_classic(gamemode, mutators) && attr < W_ITEM && !hasweap(attr, sweap)) return false;

        if(full)
        {
            int total = W(attr, ammostore) > 0 ? W(attr, ammostore) : W(attr, ammoclip),
                ammo = W(attr, ammostore) > 0 ? weapammo[attr][W_A_STORE] : getammo(attr, millis);
            if(ammo >= total) return false;
        }

        return true;
    }

    bool canuse(int gamemode, int mutators, int type, int attr, attrvector &attrs, int sweap, int millis, int skip = 0, bool full = true)
    {
        switch(enttype[type].usetype)
        {
            case EU_AUTO: case EU_ACT: return true; break;
            case EU_ITEM:
            { // can't use when reloading or firing
                if(type != WEAPON || !isweap(attr) || !m_maxcarry(actortype, gamemode, mutators)) return false;
                if(!weapwaited(weapselect, millis, skip)) return false;
                return canuseweap(gamemode, mutators, attr, sweap, millis, skip, full);
            }
            default: break;
        }
        return false;
    }

    void useitem(int id, int type, int attr, int ammoamt, int sweap, int millis, int delay)
    {
        if(type != WEAPON || !isweap(attr) || !(A(actortype, abilities)&(1<<A_A_AMMO))) return;
        int prevclip = max(weapammo[attr][W_A_CLIP], 0), prevstore = max(weapammo[attr][W_A_STORE], 0);
        weapswitch(attr, millis, delay, W_S_USE);
        weapammo[attr][W_A_CLIP] = W(attr, ammostore) <= 0 || !hasweap(attr, sweap) ? clamp(prevclip+ammoamt, 0, W(attr, ammoclip)) : prevclip;
        int diffclip = max(weapammo[attr][W_A_CLIP], 0)-prevclip, store = ammoamt-diffclip;
        weapammo[attr][W_A_STORE] = W(attr, ammostore) > 0 ? clamp(weapammo[attr][W_A_STORE]+store, 0, W(attr, ammostore)) : 0;
        weapload[attr][W_A_CLIP] = diffclip;
        weapload[attr][W_A_STORE] = max(weapammo[attr][W_A_STORE], 0)-prevstore;
        weapent[attr] = id;
    }

    void resetresidual(int n = -1)
    {
        if(n < 0 || n == W_R_BURN) lastres[W_R_BURN] = lastrestime[W_R_BURN] = burntime = burndelay = burndamage = 0;
        if(n < 0 || n == W_R_BLEED) lastres[W_R_BLEED] = lastrestime[W_R_BLEED] = bleedtime = bleeddelay = bleeddamage = 0;
        if(n < 0 || n == W_R_SHOCK)
        {
            lastres[W_R_SHOCK] = lastrestime[W_R_SHOCK] = shocktime = shockdelay = shockdamage = shockstun = shockstuntime = 0;
            shockstunscale = shockstunfall = 0.f;
        }
        if(n < 0 || n == W_R_CORRODE) lastres[W_R_CORRODE] = lastrestime[W_R_CORRODE] = corrodetime = corrodedelay = corrodedamage = 0;
    }

    void resetcheckpoint()
    {
        cpmillis = 0;
        cpnodes.shrink(0);
    }

    int setcheckpoint(int ent, int millis, int type)
    {
        if(type == CP_START)
        {
            if(cpmillis) return 1;
            cpmillis = millis;
        }
        else if(!cpmillis) return -1;

        if(cpnodes.find(ent) < 0) cpnodes.add(ent);

        return 0;
    }

    void clearstate()
    {
        loopi(2) if(colours[i] < 0) colours[i] = rnd(0xFFFFFF);
        if(model < 0) model = rnd(PLAYERTYPES);
        spree = lastdeath = lastpain = lastregen = lastregenamt = lastbuff = lastshoot = lastcook = lastaffinity = hasprize = 0;
        queuepos = -1;
        resetresidual();
    }

    void mapchange(bool change = false)
    {
        points = frags = deaths = cptime = spree = 0;
        resetcheckpoint();
    }

    void respawn(int millis)
    {
        lastspawn = millis;
        clearstate();
        weapreset(true);
    }

    int updatetimeplayed()
    {
        if(lasttimeplayed)
        {
            int millis = totalmillis-lasttimeplayed, secs = millis/1000;
            timeplayed += secs;
            lasttimeplayed = totalmillis+(secs*1000)-millis;
        }
        else lasttimeplayed = totalmillis ? totalmillis : 1;
        return timeplayed;
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
        return ((totalfrags / float(max(totaldeaths, 1))) + (frags / float(max(deaths, 1)))) / ((frags || deaths) ? 2.0f : 1.0f);
    }

    float balancescore(float none = 0.0f)
    {
        switch(G(teambalancestyle))
        {
            case 1: return timeplayed;
            case 2: return totalpoints;
            case 3: return totalfrags;
            case 4: return scoretime();
            case 5: return kdratio();
            case 6: return combinedkdratio();
            case 0: default: return none;
        }
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
        health = heal;
        int s = sweap;
        if(!isweap(s)) s = m_weapon(actortype, gamemode, mutators);
        if(!isweap(s) || s >= W_ALL) s = W_CLAW;
        if(isweap(s))
        {
            weapammo[s][W_A_CLIP] = W(s, ammospawn);
            weapselect = s;
        }

        if(s != W_CLAW && (m_kaboom(gamemode, mutators) || m_insta(gamemode, mutators) || (m_edit(gamemode) && !W(W_CLAW, disabled))))
            weapammo[W_CLAW][W_A_CLIP] = W(W_CLAW, ammospawn); // give SniperGoth his claw in edit mode

        if(s != W_MELEE && A(actortype, abilities)&(1<<A_A_MELEE) && !W(W_MELEE, disabled)) weapammo[W_MELEE][W_A_CLIP] = W(W_MELEE, ammospawn);

        if(actortype < A_ENEMY)
        {
            if(m_kaboom(gamemode, mutators) && !W(W_MINE, disabled)) weapammo[W_MINE][W_A_CLIP] = W(W_MINE, ammospawn);
            else if(!m_speedrun(gamemode) || m_ra_gauntlet(gamemode, mutators))
            {
                if(s != W_GRENADE && A(actortype, spawngrenades) >= (m_insta(gamemode, mutators) ? 2 : 1) && !W(W_GRENADE, disabled))
                    weapammo[W_GRENADE][W_A_CLIP] = W(W_GRENADE, ammospawn);
                if(s != W_MINE && A(actortype, spawnmines) >= (m_insta(gamemode, mutators) ? 2 : 1) && !W(W_MINE, disabled))
                    weapammo[W_MINE][W_A_CLIP] = W(W_MINE, ammospawn);
            }
        }

        if(m_maxcarry(actortype, gamemode, mutators) && m_loadout(gamemode, mutators))
        {
            vector<int> aweap;
            loopj(W_LOADOUT)
            {
                if(loadweap.inrange(j) && isweap(loadweap[j]) && !hasweap(loadweap[j], sweap) && m_check(W(loadweap[j], modes), W(loadweap[j], muts), gamemode, mutators) && !W(loadweap[j], disabled))
                    aweap.add(loadweap[j]);
                else aweap.add(0);
            }
            vector<int> rand, forcerand;
            for(int t = W_OFFSET; t < W_ITEM; t++)
                if(!hasweap(t, sweap) && m_check(W(t, modes), W(t, muts), gamemode, mutators) && !W(t, disabled) && aweap.find(t) < 0)
                    (canrandweap(t) ? rand : forcerand).add(t);
            int count = 0;
            loopj(W_LOADOUT)
            {
                if(aweap[j] <= 0) // specifically asking for random
                {
                    vector<int> &randsrc = rand.empty() ? forcerand : rand;
                    if(!randsrc.empty())
                    {
                        int n = rnd(randsrc.length());
                        aweap[j] = randsrc.remove(n);
                    }
                    else continue;
                }
                if(!isweap(aweap[j])) continue;
                weapammo[aweap[j]][W_A_CLIP] = W(aweap[j], ammospawn);
                count++;
                if(count >= m_maxcarry(actortype, gamemode, mutators)) break;
            }
            loopj(2) if(isweap(aweap[j])) { weapselect = aweap[j]; break; }
        }
        loopj(W_MAX) if(weapammo[j][W_A_CLIP] > W(j, ammoclip))
        {
            if(W(j, ammostore) > 0) weapammo[j][W_A_STORE] = clamp(weapammo[j][W_A_CLIP]-W(j, ammoclip), 0, W(j, ammostore));
            weapammo[j][W_A_CLIP] = W(j, ammoclip);
        }
    }

    void editspawn(int gamemode, int mutators, int heal)
    {
        clearstate();
        spawnstate(gamemode, mutators, m_weapon(actortype, gamemode, mutators), heal);
    }

    int respawnwait(int millis, int delay)
    {
        return lastdeath ? max(0, delay-(millis-lastdeath)) : 0;
    }

    int protect(int millis, int delay)
    {
        if(actortype >= A_ENEMY || !lastspawn || !delay) return 0;
        if(G(protectbreakshoot) && lastshoot) return 0;
        if(G(protectbreakcook) && lastcook) return 0;
        if(G(protectbreakaffinity) && lastaffinity) return 0;
        int len = millis-lastspawn;
        if(len > delay) return 0;
        return delay-len;
    }

    #define RESIDUAL(name, type, pulse) bool name##func(int millis, int len) { return len && lastres[W_R_##type] && millis-lastres[W_R_##type] <= len; }
    RESIDUALS
    #undef RESIDUAL
};


#define PROJ_ENUM(en, um) \
    en(um, Shot, SHOT) en(um, Giblet, GIB) en(um, Debris, DEBRIS) en(um, Eject, EJECT) \
    en(um, Entity, ENTITY) en(um, Affinity, AFFINITY) en(um, Vanity, VANITY) en(um, Piece, PIECE) \
    en(um, Max, MAX)
ENUM_DLN(PROJ);

namespace server
{
    extern void stopdemo();
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern bool servcmd(int nargs, const char *cmd, const char *arg);
    extern const char *privname(int priv = PRIV_NONE, int actortype = A_PLAYER);
    extern const char *privnamex(int priv = PRIV_NONE, int actortype = A_PLAYER, bool local = false);
#ifdef CPP_GAME_SERVER
    struct clientinfo;
    extern void waiting(clientinfo *ci, int drop = 0, bool doteam = true, bool exclude = false);
    extern void setteam(clientinfo *ci, int team, int flags = TT_RESET, bool swaps = true);
    extern int chooseteam(clientinfo *ci, int suggest = -1, bool wantbal = false);
#endif
    extern int getlavaburntime(int mat); \
    extern int getlavaburndelay(int mat);
    extern float getwaterextinguish(int mat);
    extern float getwaterextinguishscale(int mat);
}

#define PLCHAN_ENUM(en, um) en(um, Announce, ANNOUNCE) en(um, Message, MESSAGE) en(um, Voice, VOICE) en(um, Alert, ALERT) en(um, Maximum, MAX)
ENUM_DLN(PLCHAN);

#define HUDPOS_ENUM(en, um) \
    en(um, lefttop, LEFTTOP) en(um, centertop, CENTERTOP) en(um, righttop, RIGHTTOP) \
    en(um, leftbottom, LEFTBOTTOM) en(um, centerbottom, CENTERBOTTOM) en(um, rightbottom, RIGHTBOTTOM) \
    en(um, centermiddle, CENTERMIDDLE) en(um, Maximum, MAX)
ENUM_DLN(HUDPOS)

#ifdef CPP_GAME_SERVER
#define WATERPHYS(name,mat) (server::getwater##name(mat)*server::getwater##name##scale(mat))
#else
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

namespace game
{
    extern int gamestate, gamemode, mutators;
}
#define AFFINITYPOS(n) \
    namespace n \
    { \
        extern bool getpos(int idx, vec &o); \
    }
AFFINITYPOS(capture);
AFFINITYPOS(defend);
AFFINITYPOS(bomber);
struct gameentity : extentity
{
    int schan, affinity, lastspawn, nextemit, lastthirdpos, spawndelay;
    linkvector kin;
    vec offset, thirdpos, curpos;
    float yaw, pitch;

    gameentity() : schan(-1), affinity(-1), lastspawn(0), nextemit(0), lastthirdpos(0), spawndelay(0), offset(0, 0, 0), yaw(0), pitch(0) {}
    ~gameentity()
    {
        if(issound(schan)) soundsources[schan].clear();
        schan = -1;
    }

    void getcurpos()
    {
        curpos = o;

        if(affinity >= 0)
        {
            if(m_capture(game::gamemode)) capture::getpos(affinity, curpos);
            else if(m_defend(game::gamemode)) defend::getpos(affinity, curpos);
            else if(m_bomber(game::gamemode)) bomber::getpos(affinity, curpos);
        }

        if(dynamic()) curpos.add(offset);
    }

    vec pos() { getcurpos(); return curpos; }
    vec *getpos() { getcurpos(); return &curpos; }
};

static const char * const animnames[] =
{
    "idle", "walk forward", "walk backward", "walk left", "walk right", "dead", "dying", "swim",
    "mapmodel", "trigger on", "trigger off", "pain",
    "jump forward", "jump backward", "jump left", "jump right", "jump",
    "run forward", "run backward", "run left", "run right",
    "boost forward", "boost backward", "boost left", "boost right", "boost up",
    "parkour left", "parkour right", "parkour up", "parkour jump",
    "vault", "power slide", "fly kick",
    "sink", "edit", "win", "lose",
    "crouch", "crawl forward", "crawl backward", "crawl left", "crawl right",
    "crouch jump forward", "crouch jump backward", "crouch jump left", "crouch jump right", "crouch jump",
    "claw", "claw primary", "claw secondary", "claw reload", "claw power", "claw zoom",
    "pistol", "pistol primary", "pistol secondary", "pistol reload", "pistol power", "pistol zoom",
    "sword", "sword primary", "sword secondary", "sword power", "sword zoom",
    "shotgun", "shotgun primary", "shotgun secondary", "shotgun reload", "shotgun power", "shotgun zoom",
    "smg", "smg primary", "smg secondary", "smg reload", "smg power", "smg zoom",
    "flamer", "flamer primary", "flamer secondary", "flamer reload", "flamer power", "flamer zoom",
    "plasma", "plasma primary", "plasma secondary", "plasma reload", "plasma power", "plasma zoom",
    "zapper", "zapper primary", "zapper secondary", "zapper reload", "zapper power", "zapper zoom",
    "rifle", "rifle primary", "rifle secondary", "rifle reload", "rifle power", "rifle zoom",
    "corroder", "corroder primary", "corroder secondary", "corroder reload", "corroder power", "corroder zoom",
    "grenade", "grenade primary", "grenade secondary", "grenade reload", "grenade power", "grenade zoom",
    "mine", "mine primary", "mine secondary", "mine reload", "mine power", "mine zoom",
    "rocket", "rocket primary", "rocket secondary", "rocket reload", "rocket power", "rocket zoom",
    "minigun", "minigun primary", "minigun secondary", "minigun reload", "minigun power", "minigun zoom",
    "jetsaw", "jetsaw primary", "jetsaw secondary", "jetsaw reload", "jetsaw power", "jetsaw zoom",
    "eclipse", "eclipse primary", "eclipse secondary", "eclipse reload", "eclipse power", "eclipse zoom",
    "switch", "use", "sprint"
    ""
};

struct stunevent
{
    int weap, millis, delay;
    float scale, gravity;
};

struct recoilevent
{
    int weap, millis, delay, last;
    float yaw, pitch;
};

enum
{
    WS_BEGIN_CHAN = 0,
    WS_MAIN_CHAN,
    WS_END_CHAN,
    WS_POWER_CHAN,
    WS_OTHER_CHAN,

    WS_CHANS
};

enum
{
    SEQ_PROJ = 0,
    SEQ_SHOT,
    SEQ_MAX
};

struct projent;

struct gameent : dynent, clientstate
{
    editinfo *edit;
    ai::aiinfo *ai;
    int team, clientnum, privilege, seqid[SEQ_MAX], lastnode, cplast, respawned, suicided, lastupdate, lastpredict, plag, ping, lastflag, totaldamage,
        actiontime[AC_MAX], impulse[IM_MAX], impulsetime[IM_T_MAX], smoothmillis, turnside, turnmillis[3], turntime[3], plchan[PLCHAN_MAX], wschan[WS_CHANS], sschan[2],
        lasthit, lastteamhit, lastkill, lastattacker, lastpoints, quake, wasfiring, lastfoot, lastdir, sprinttime;
    float deltayaw, deltapitch, newyaw, newpitch, stunscale, stungravity, turnangle[3], lastyaw, lastpitch;
    bool action[AC_MAX], conopen, k_up, k_down, k_left, k_right, obliterated, headless;
    vec tag[TAG_MAX];
    vec2 rotvel;
    string hostip, name, handle, steamid, info, obit;
    vector<gameent *> dominator;
    vector<stunevent> stuns;
    vector<recoilevent> recoils;
    vector<int> vitems;
    fx::emitter *weaponfx, *impulsefx, *flashlightfx, *prizefx, *envfx;
    projent *projchain;

    struct collectlist
    {
        int type;
        float size;
        const char *name;
    };
    vector<collectlist> collects;

    gameent() : edit(NULL), ai(NULL), team(T_NEUTRAL), clientnum(-1), privilege(PRIV_NONE), cplast(0), lastupdate(0), lastpredict(0), plag(0), ping(0),
        totaldamage(0), smoothmillis(-1), lastattacker(-1), lastpoints(0), quake(0), wasfiring(-1), conopen(false), k_up(false), k_down(false), k_left(false), k_right(false), obliterated(false),
        weaponfx(NULL), impulsefx(NULL), flashlightfx(NULL), prizefx(NULL), envfx(NULL), projchain(NULL)
    {
        type = ENT_PLAYER;
        loopi(SEQ_MAX) seqid[i] = 0;
        copystring(hostip, "0.0.0.0");
        name[0] = handle[0] = steamid[0] = info[0] = obit[0] = '\0';
        removesounds(true);
        respawn(-1, 0, 0);
    }
    ~gameent()
    {
        removefx();
        removesounds();
        freeeditinfo(edit);
        if(ai) delete ai;
        removetrackedparticles(this);
        removetrackedsounds(this);
        fx::removetracked(this);
        entities::removepassenger(this);
        clearimpulse();
    }

    static bool is(int t) { return t == ENT_PLAYER || t == ENT_AI; }
    static bool is(physent *d) { return d && (d->type == ENT_PLAYER || d->type == ENT_AI); }

    int gethealth(int gamemode, int mutators, bool full = false)
    {
        if(m_insta(gamemode, mutators)) return 1;

        int hp = A(actortype, health), sweap = m_weapon(actortype, gamemode, mutators);

        if(actortype >= A_ENEMY && actortype != A_HAZARD && entities::ents.inrange(spawnpoint) && entities::ents[spawnpoint]->type == ACTOR && entities::ents[spawnpoint]->attrs[7] > 0)
            hp = entities::ents[spawnpoint]->attrs[7];

        loopi(W_MAX) if(hasweap(i, sweap))
        {
            hp += W(i, modhealth)+(getammo(i, 0, true)*W(i, modhealthammo));
            if(i == weapselect) hp += W(i, modhealthequip);
        }

        hp = hp*(m_hard(gamemode, mutators) ? G(healthscalehard) : G(healthscale));
        if(full) hp = hp*(m_vampire(gamemode, mutators) ? G(maxhealthvampire) : G(maxhealth));

        return max(hp, 1);
    }

    bool collected(int type, float size = 1, const char *name = NULL)
    {
        collectlist &c = collects.add();
        c.type = type != PROJ_ENTITY ? type : PROJ_DEBRIS;
        c.size = size;
        if(type == PROJ_ENTITY) c.size *= 0.3f;
        c.name = name;

        if(ai != NULL && actortype == A_JANITOR && collects.length() >= janitorready)
            return true; // award a prize

        return false;
    }

    void collectprize(int prize)
    {
        if(actortype == A_JANITOR)
            while(collects.length() < janitorready) // just ensure we have enough in there
                collected(rnd(2) ? PROJ_GIB : PROJ_DEBRIS, 0.5f + rnd(101)/100.f);
        hasprize = prize;
    }

    int isprize(gameent *d = NULL)
    {
        if(!isalive() || d->actortype == A_HAZARD) return false;
        if(hasprize > 0) return 1;
        if(d && revengeprize && d->dominator.find(this) >= 0) return 2;
        loopi(W_SUPERS) if(getammo(i + W_SUPER, lastmillis) > 0) return 3;
        return 0;
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
            if(!s.delay)
            {
                stuns.remove(i);
                continue;
            }
            int etime = s.millis+s.delay, len = etime-clamp(millis, s.millis, etime);
            if(len > 0) stun += (gravity ? s.gravity : s.scale)*(len/float(s.delay));
            if(gravity && millis >= etime) stuns.remove(i);
        }
        return 1.f-clamp(stun, 0.f, 1.f);
    }

    void addrecoil(int weap, int millis, int delay, float yawmin, float yawmax, float pitchmin, float pitchmax, int dir = 0)
    {
        if(delay <= 0) return;
        recoilevent &s = recoils.add();
        s.weap = weap;
        s.millis = s.last = millis;
        s.delay = delay;
        float yaw1 = min(yawmin, yawmax), yaw2 = max(yawmin, yawmax), yawv = yaw1,
              pitch1 = min(pitchmin, pitchmax), pitch2 = max(pitchmin, pitchmax), pitchv = pitch1;
        if(yaw2 > yaw1) yawv += (yaw2-yaw1)*(rnd(10000)+1)/10000.f;
        if(pitch2 > pitch1) pitchv += (pitch2-pitch1)*(rnd(10000)+1)/10000.f;
        if(dir)
        {
            int value = rnd((dir > 0 ? dir : -dir)+1);
            if(dir > 0 ? value == 0 : value != 0) pitchv = -pitchv;
        }
        s.yaw = yawv;
        s.pitch = pitchv;
    }

    void recoil(int millis)
    {
        loopvrev(recoils)
        {
            recoilevent &s = recoils[i];
            if(!s.delay)
            {
                recoils.remove(i);
                continue;
            }
            int etime = s.millis+s.delay, len = clamp(millis, s.millis, etime)-s.last;
            if(len > 0)
            {
                float scale = len/float(s.delay);
                yaw += s.yaw*scale;
                if(yaw < 0.0f) yaw = 360.0f - fmodf(-yaw, 360.0f);
                else if(yaw >= 360.0f) yaw = fmodf(yaw, 360.0f);
                pitch += s.pitch*scale;
                if(pitch > 89.9f) pitch = 89.9f;
                else if(pitch < -89.9f) pitch = -89.9f;
                s.last = millis;
            }
            if(millis >= etime) recoils.remove(i);
        }
    }

    void configure(int millis, int gamemode, int mutators, int affinities = 0, int cur = 0, float decay = 0, float inertia = 0, float maxinertia = 0)
    {
        #define MODPHYSL \
            MODPHYS(speed, float, speedscale); \
            MODPHYS(impulsespeed, float, speedscale); \
            MODPHYS(weight, float, curscale); \
            MODPHYS(buoyancy, float, curscale);

        float scale = A(actortype, scale), speedscale = 1;

        if(actortype >= A_ENEMY && actortype != A_HAZARD && entities::ents.inrange(spawnpoint) && entities::ents[spawnpoint]->type == ACTOR)
        {
            if(entities::ents[spawnpoint]->attrs[8] > 0) speedscale *= entities::ents[spawnpoint]->attrs[8] / 100.f;
            if(entities::ents[spawnpoint]->attrs[9] > 0) scale *= entities::ents[spawnpoint]->attrs[9] / 100.f;
        }
        if(hasprize > 0) speedscale *= A(actortype, speedprize);

        if(m_resize(gamemode, mutators) && cur)
        {
            float amtscale = m_insta(gamemode, mutators) ? 1 + (spree*G(instaresizeamt)) : max(health, 1) / float(max(gethealth(gamemode, mutators), 1));
            if(amtscale < 1) amtscale = (amtscale * (1 - G(minresizescale))) + G(minresizescale);
            scale *= clamp(amtscale, G(minresizescale), G(maxresizescale));
        }

        if(scale != curscale)
        {
            if(cur && state == CS_ALIVE)
                curscale = scale > curscale ? min(curscale + cur / 2000.f, scale) : max(curscale - cur / 2000.f, scale);
            else curscale = scale;
        }

        loopi(W_MAX) if(!cur || (weapstate[i] != W_S_IDLE && (weapselect != i || (weapstate[i] != W_S_POWER && weapstate[i] != W_S_ZOOM)) && millis-weaptime[i] >= weapwait[i]+100))
            setweapstate(i, W_S_IDLE, 0, millis);

        xradius = yradius = actors[actortype].radius*curscale;
        zradius = actors[actortype].height*curscale;
        radius = max(xradius, yradius);
        aboveeye = actors[actortype].aboveeye*curscale;

        #define MODPHYS(a,b,c) a = A(actortype, a)*c;
        MODPHYSL;
        #undef MODPHYS
        if(m_single(gamemode, mutators))
        {
            #define MODPHYS(a,b,c) a += A(actortype, a##extra)*c;
            MODPHYSL;
            #undef MODPHYS
        }

        if(cur)
        {
            if(affinities > 0)
            {
                if(m_capture(gamemode))
                {
                    #define MODPHYS(a,b,c) a += G(capturecarry##a)+(affinities*G(capturecarry##a##each));
                    MODPHYSL;
                    #undef MODPHYS
                }
                else if(m_bomber(gamemode))
                {
                    #define MODPHYS(a,b,c) a += G(bombercarry##a);
                    MODPHYSL;
                    #undef MODPHYS
                }
            }
            int sweap = m_weapon(actortype, gamemode, mutators);
            loopi(W_MAX) if(hasweap(i, sweap))
            {
                int numammo = getammo(i, 0, true);
                #define MODCARRY(a) (m_arena(gamemode, mutators) ? (a)*A(actortype, maxcarry)/W_LOADOUT : a)
                #define MODPHYS(a,b,c) a += MODCARRY(W(i, mod##a)+(numammo*W(i, mod##a##ammo)));
                MODPHYSL;
                #undef MODPHYS
                if(i != weapselect) continue;
                #define MODPHYS(a,b,c) a += W(i, mod##a##equip);
                MODPHYSL;
                #undef MODPHYS
                switch(weapstate[i])
                {
                    case W_S_PRIMARY: case W_S_SECONDARY:
                    {
                        #define MODPHYS(a,b,c) a += W2(i, mod##a##attack, weapstate[i] == W_S_SECONDARY);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                    case W_S_RELOAD:
                    {
                        #define MODPHYS(a,b,c) a += W(i, mod##a##reload);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                    case W_S_SWITCH:
                    {
                        #define MODPHYS(a,b,c) a += W(i, mod##a##switch);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                    case W_S_USE:
                    {
                        #define MODPHYS(a,b,c) a += W(i, mod##a##use);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                    case W_S_POWER:
                    {
                        #define MODPHYS(a,b,c) a += W(i, mod##a##power);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                    case W_S_ZOOM:
                    {
                        #define MODPHYS(a,b,c) a += W(i, mod##a##zoom);
                        MODPHYSL;
                        #undef MODPHYS
                        break;
                    }
                }
            }
            recoil(millis);
            stunscale = stunned(millis, false);
            stungravity = stunned(millis, true);
        }
        else
        {
            stunscale = stungravity = 1;
            height = zradius;
        }

        #define MODPHYS(a,b,c) a = max(a, b(0));
        MODPHYSL;
        #undef MODPHYS
        #undef MODPHYSL

        if(cur && lastdir != millis && lastdir >= 0 && decay > 0 && inertia > 0 && maxinertia > 0)
        {
            vec2 curdir = vec2(yaw, pitch), prevdir = vec2(lastyaw, lastpitch), currot = vec2(prevdir).sub(curdir);

            if(currot.x > 180.0f) currot.x -= 360.0f;
            else if(currot.x < -180.0f) currot.x += 360.0f;

            rotvel.mul(powf(decay, curtime));
            rotvel.add(vec2(currot).mul(inertia));
            rotvel.clamp(-maxinertia, maxinertia);
        }
        else rotvel = vec2(0);

        lastyaw = yaw;
        lastpitch = pitch;
        lastdir = millis;
    }

    int getseqid(int idx = -1)
    {
        if(idx < 0 || idx >= SEQ_MAX) return -1;

        seqid[idx]++;
        if(seqid[idx] < 2) seqid[idx] = 2;
        return seqid[idx];
    }

    void removefx()
    {
        if(weaponfx) fx::stopfx(weaponfx);
        if(impulsefx) fx::stopfx(impulsefx);
        if(flashlightfx) fx::stopfx(flashlightfx);
        if(prizefx) fx::stopfx(prizefx);
        if(envfx) fx::stopfx(envfx);
    }

    void removesounds(bool init = false)
    {
        loopi(PLCHAN_MAX)
        {
            if(!init && issound(plchan[i])) soundsources[plchan[i]].clear();
            plchan[i] = -1;
        }
        loopi(WS_CHANS)
        {
            if(!init && issound(wschan[i])) soundsources[wschan[i]].clear();
            wschan[i] = -1;
        }
        loopi(2)
        {
            if(!init && issound(sschan[i])) soundsources[sschan[i]].clear();
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

    void clearimpulse(int clear = -1, int reset = -1)
    {
        loopi(IM_MAX) if(clear < 0 || (clear&(1<<i))) impulse[i] = 0;
        loopi(IM_T_MAX) if(reset < 0 || (reset&(1<<i))) impulsetime[i] = 0;
    }

    void clearstate(int millis, int gamemode, int mutators)
    {
        inmaterial = lasthit = lastkill = quake = turnside = 0;
        loopi(3)
        {
            turnmillis[i] = turntime[i] = 0;
            turnangle[i] = 0.0f;
        }
        sprinttime = 0;
        lastteamhit = lastflag = respawned = suicided = lastnode = lastfoot = wasfiring = lastdir = -1;
        lastyaw = lastpitch = 0;
        rotvel = vec2(0);
        obit[0] = '\0';
        obliterated = headless = false;
        stuns.shrink(0);
        recoils.shrink(0);
        used.shrink(0);
        collects.shrink(0);
        clearimpulse();
        cleartags();
    }

    void respawn(int millis, int gamemode, int mutators)
    {
        stopmoving(false);
        removefx();
        removesounds();
        physent::reset();
        clearstate(millis, gamemode, mutators);
        clientstate::respawn(millis);
        configure(millis, gamemode, mutators);
    }

    void editspawn(int gamemode, int mutators, int heal)
    {
        stopmoving(false);
        clearstate(lastmillis, gamemode, mutators);
        airmillis = floormillis = 0;
        forcepos = false;
        physstate = PHYS_FALL;
        vel = falling = vec(0, 0, 0);
        floor = vec(0, 0, 1);
        resetinterp();
        clientstate::editspawn(gamemode, mutators, heal);
    }

    void resetstate(int millis, int gamemode, int mutators)
    {
        respawn(millis, gamemode, mutators);
        frags = deaths = totaldamage = cplast = 0;
    }

    void mapchange(int millis, int gamemode, int mutators)
    {
        dominator.shrink(0);
        resetstate(millis, gamemode, mutators);
        clientstate::mapchange();
    }

    void cleartags()
    {
        loopi(TAG_MAX) tag[i] = vec(-1);
    }

    float headsize()
    {
        if(!(actors[actortype].collidezones&CLZ_HEAD)) return 0.f;
        return max(xradius * 0.45f, yradius * 0.45f);
    }

    vec &headtag()
    {
        if(tag[TAG_CROWN] == vec(-1))
        {
            tag[TAG_CROWN] = o;
            tag[TAG_CROWN].z -= headsize() * 0.375f;
        }
        return tag[TAG_CROWN];
    }

    vec &cameratag()
    {
        if(tag[TAG_CAMERA] == vec(-1)) tag[TAG_CAMERA] = o;
        return tag[TAG_CAMERA];
    }

    vec &headbox()
    {
        if(tag[TAG_R_CROWN] == vec(-1)) tag[TAG_R_CROWN] = vec(headsize());
        return tag[TAG_R_CROWN];
    }

    float torsosize()
    {
        if(!(actors[actortype].collidezones&CLZ_TORSO)) return 0.f;
        return (headtag().z - headbox().z) - torsotag().z;
    }

    vec &torsotag()
    {
        if(tag[TAG_TORSO] == vec(-1))
        {
            tag[TAG_TORSO] = o;
            tag[TAG_TORSO].z -= height  *0.45f;
        }
        return tag[TAG_TORSO];
    }

    vec &torsobox()
    {
        if(tag[TAG_R_TORSO] == vec(-1)) tag[TAG_R_TORSO] = vec(torsosize());
        return tag[TAG_R_TORSO];
    }

    float limbsize()
    {
        if(!(actors[actortype].collidezones&CLZ_LIMBS)) return 0.f;
        return ((torsotag().z - torsobox().z) - (o.z - height)) * 0.5f;
    }

    vec &limbstag()
    {
        if(tag[TAG_LIMBS] == vec(-1))
        {
            tag[TAG_LIMBS] = torsotag();
            tag[TAG_LIMBS].z -= torsobox().z + limbsize();
        }
        return tag[TAG_LIMBS];
    }

    vec &limbsbox()
    {
        if(tag[TAG_R_LIMBS] == vec(-1)) tag[TAG_R_LIMBS] = vec(limbsize());
        return tag[TAG_R_LIMBS];
    }

    vec &origintag(int weap = -1)
    {
        if(!actors[actortype].hastags) return tag[TAG_ORIGIN] = headpos();

        if(tag[TAG_ORIGIN] == vec(-1))
        {
            if(!isweap(weap)) weap = weapselect;

            if(weap == W_MELEE) tag[TAG_ORIGIN] = feetpos();
            else
            {
                vec dir, right;
                vecfromyawpitch(yaw, pitch, 1, 0, dir);
                dir.mul(radius * 3);
                vecfromyawpitch(yaw, pitch, 0, -1, right);
                right.mul(radius * 1.5f);
                tag[TAG_ORIGIN] = vec(headpos(-height / 6)).add(right).add(dir);
            }
        }
        return tag[TAG_ORIGIN];
    }

    int getmuzzle()
    {
        return seqid[SEQ_SHOT]%2 != 0 ? TAG_MUZZLE2 : TAG_MUZZLE1;
    }

    vec &muzzletag(int weap = -1, int tagid = -1)
    {
        int curtag = TAG_MUZZLE + (tagid < 0 || tagid >= TAG_N_MUZZLE ? seqid[SEQ_SHOT]%2 : tagid);

        if(!actors[actortype].hastags) return tag[curtag] = headpos();

        if(tag[curtag] == vec(-1))
        {
            if(curtag == TAG_MUZZLE2) return muzzletag(weap, 0);

            if(!isweap(weap)) weap = weapselect;

            if(weap == W_SWORD && ((weapstate[weap] == W_S_PRIMARY) || (weapstate[weap] == W_S_SECONDARY)))
            {
                float frac = (lastmillis - weaptime[weap]) / float(weapwait[weap]), yx = yaw, px = pitch;
                if(weapstate[weap] == W_S_PRIMARY)
                {
                    yx -= 90;
                    yx += frac * 180;
                    if(yx >= 360) yx -= 360;
                    if(yx < 0) yx += 360;
                }
                else
                {
                    px += 90;
                    px -= frac * 180;
                    if(px >= 180) px -= 360;
                    if(px < -180) px += 360;
                }
                tag[curtag] = vec(origintag(weap)).add(vec(yx * RAD, px * RAD).mul(8));
            }
            else
            {
                vec dir(yaw * RAD, pitch * RAD);
                if(weap != W_CLAW)
                {
                    vec right;
                    vecfromyawpitch(yaw, pitch, 0, -1, right);
                    tag[curtag] = vec(origintag(weap)).add(dir.mul(radius * 0.75f)).add(right.mul(radius * 0.6f));
                }
                else tag[curtag] = vec(origintag(weap)).add(dir.mul(radius * 2));
            }
        }
        return tag[curtag];
    }

    vec &ejecttag(int weap = -1, int idx = 0)
    {
        if(!isweap(weap)) weap = weapselect;
        if(idx < 0 || idx >= TAG_N_EJECT) idx = 0;

        if(tag[TAG_EJECT1] == vec(-1))
        {
            vec dir = vec(muzzletag(weap)).sub(origintag(weap));
            float mag = dir.magnitude();
            tag[TAG_EJECT1] = vec(origintag(weap)).add(dir.normalize().mul(mag * 0.5f));
        }

        int tnum = TAG_EJECT + idx;
        if(idx > 0 && tag[tnum] == vec(-1))
        {
            tag[tnum] = tag[TAG_EJECT1];
            tag[tnum].add(vec(yaw * RAD, 0.f).rotate_around_z(90 * RAD).mul(8));
            tag[tnum].z += 8;
        }
        return tag[tnum];
    }

    vec &waisttag()
    {
        if(tag[TAG_WAIST] == vec(-1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 0, dir);
            dir.mul(radius * 1.6f);
            dir.z -= height * 0.5f;
            tag[TAG_WAIST] = vec(o).add(dir);
        }
        return tag[TAG_WAIST];
    }

    vec &jetlefttag()
    {
        if(tag[TAG_JET_LEFT] == vec(-1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, -1, dir);
            dir.mul(radius);
            dir.z -= height;
            tag[TAG_JET_LEFT] = vec(o).add(dir);
        }
        return tag[TAG_JET_LEFT];
    }

    vec &jetrighttag()
    {
        if(tag[TAG_JET_RIGHT] == vec(-1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 1, dir);
            dir.mul(radius);
            dir.z -= height;
            tag[TAG_JET_RIGHT] = vec(o).add(dir);
        }
        return tag[TAG_JET_RIGHT];
    }

    vec &jetbacktag()
    {
        if(tag[TAG_JET_BACK] == vec(-1))
        {
            vec dir;
            vecfromyawpitch(yaw, 0, -1, 0, dir);
            dir.mul(radius * 1.25f);
            dir.z -= height * 0.35f;
            tag[TAG_JET_BACK] = vec(o).add(dir);
        }
        return tag[TAG_JET_BACK];
    }

    vec &jettag(int idx)
    {
        switch(idx)
        {
            case 2: return jetbacktag();
            case 1: return jetrighttag();
            case 0: return jetlefttag();
            default: break;
        }
        return jetbacktag();
    }

    vec &foottag(int idx = 0)
    {
        if(idx < 0 || idx > 1) idx = 0;
        int tnum = TAG_TOE + idx;
        if(tag[tnum] == vec(-1))
        {
            int millis = lastmillis%500;
            float amt = millis > 250 ? (500-millis)/250.f : millis/250.f;
            vec dir, right;
            vecfromyawpitch(yaw, pitch, 1, 0, dir);
            vecfromyawpitch(yaw, pitch, 0, idx ? 1 : -1, right);
            dir.mul(radius * 0.5f);
            right.mul(radius * (!move && strafe ? amt - 0.5f : 0.5f));
            dir.z -= height * 0.6f + (height * 0.4f * (idx ? 1.f - amt : amt));
            tag[tnum] = vec(o).add(dir).add(right);
        }
        return tag[tnum];
    }

    vec &toetag(int idx)
    {
        switch(idx)
        {
            case 1: return foottag(0);
            case 0: return foottag(1);
            default: break;
        }
        return foottag(0);
    }

    vec *gettag(int idx)
    {
        if(idx < 0 || idx >= TAG_MAX) return NULL;
        switch(idx)
        {
            case TAG_CAMERA: cameratag(); break;
            case TAG_CROWN: headtag(); break;
            case TAG_R_CROWN: headbox(); break;
            case TAG_TORSO: torsotag(); break;
            case TAG_R_TORSO: torsobox(); break;
            case TAG_LIMBS: limbstag(); break;
            case TAG_R_LIMBS: limbsbox(); break;
            case TAG_WAIST: waisttag(); break;
            case TAG_MUZZLE1: muzzletag(-1, 0); break;
            case TAG_MUZZLE2: muzzletag(-1, 1); break;
            case TAG_ORIGIN: origintag(); break;
            case TAG_EJECT1: ejecttag(-1, 0); break;
            case TAG_EJECT2: ejecttag(-1, 1); break;
            case TAG_JET_LEFT: jetlefttag(); break;
            case TAG_JET_RIGHT: jetrighttag(); break;
            case TAG_JET_BACK: jetbacktag(); break;
            case TAG_TOE_LEFT: toetag(0); break;
            case TAG_TOE_RIGHT: toetag(1); break;
            default: return NULL;
        }
        return &tag[idx];
    }

    int impulsemask(int type)
    {
        switch(type)
        {
            case IM_T_JUMP: return impulsejumpmask;
            case IM_T_BOOST: return impulseboostmask;
            case IM_T_DASH: return impulsedashmask;
            case IM_T_SLIDE: return impulseslidemask;
            case IM_T_LAUNCH: return impulselaunchmask;
            case IM_T_MELEE: return impulsemeleemask;
            case IM_T_KICK: return impulsekickmask;
            case IM_T_GRAB: return impulsegrabmask;
            case IM_T_WALLRUN: return impulsewallrunmask;;
            case IM_T_VAULT: return impulsevaultmask;
            case IM_T_POUND: return impulsepoundmask;
            default: break;
        }
        return 0;
    }

    int impulsereset(int type)
    {
        switch(type)
        {
            case IM_T_JUMP: return impulsejumpreset;
            case IM_T_BOOST: return impulseboostreset;
            case IM_T_DASH: return impulsedashreset;
            case IM_T_SLIDE: return impulseslidereset;
            case IM_T_LAUNCH: return impulselaunchreset;
            case IM_T_MELEE: return impulsemeleereset;
            case IM_T_KICK: return impulsekickreset;
            case IM_T_GRAB: return impulsegrabreset;
            case IM_T_WALLRUN: return impulsewallrunreset;;
            case IM_T_VAULT: return impulsevaultreset;
            case IM_T_POUND: return impulsepoundreset;
            case IM_T_PUSHER: return impulsepusherreset;
            default: break;
        }
        return 0;
    }

    int impulseclear(int type)
    {
        switch(type)
        {
            case IM_T_JUMP: return impulsejumpclear;
            case IM_T_BOOST: return impulseboostclear;
            case IM_T_DASH: return impulsedashclear;
            case IM_T_SLIDE: return impulseslideclear;
            case IM_T_LAUNCH: return impulselaunchclear;
            case IM_T_MELEE: return impulsemeleeclear;
            case IM_T_KICK: return impulsekickclear;
            case IM_T_GRAB: return impulsegrabclear;
            case IM_T_WALLRUN: return impulsewallrunclear;;
            case IM_T_VAULT: return impulsevaultclear;
            case IM_T_POUND: return impulsepoundclear;
            case IM_T_PUSHER: return impulsepusherclear;
            default: break;
        }
        return 0;
    }

    int impulsedelay(int type)
    {
        switch(type)
        {
            case IM_T_JUMP: return impulsejumpdelay;
            case IM_T_BOOST: return impulseboostdelay;
            case IM_T_DASH: return impulsedashdelay;
            case IM_T_SLIDE: return impulseslidedelay;
            case IM_T_LAUNCH: return impulselaunchdelay;
            case IM_T_MELEE: return impulsemeleedelay;
            case IM_T_KICK: return impulsekickdelay;
            case IM_T_GRAB: return impulsegrabdelay;
            case IM_T_WALLRUN: return impulsewallrundelay;
            case IM_T_VAULT: return impulsevaultdelay;
            case IM_T_POUND: return impulsepounddelay;
            case IM_T_PUSHER: return impulse[IM_PUSHER];
            default: break;
        }
        return 0;
    }

    int impulselen(int type)
    {
        switch(type)
        {
            case IM_T_JUMP: return impulsejumplen;
            case IM_T_BOOST: return impulseboostlen;
            case IM_T_DASH: return impulsedashlen;
            case IM_T_SLIDE: return impulseslidelen;
            case IM_T_LAUNCH: return impulselaunchlen;
            case IM_T_MELEE: return impulsemeleelen;
            case IM_T_KICK: return impulsekicklen;
            case IM_T_GRAB: return impulsegrablen;
            case IM_T_WALLRUN: return impulsewallrunlen;
            case IM_T_VAULT: return impulsevaultlen;
            case IM_T_POUND: return impulsepoundlen;
            case IM_T_PUSHER: return impulse[IM_PUSHER];
            default: break;
        }
        return 0;
    }

    float impulseskewmeter(bool onfloor)
    {
        float skew = 1.0f;

        if(running()) skew *= sprinting(false) ? impulsesprintregenmeter : impulserunregenmeter;
        if(move || strafe) skew *= impulsemoveregenmeter;
        if((!onfloor || hasslide()) && PHYS(gravity) > 0) skew *= impulseinairregenmeter;
        if(onfloor && crouching() && !hasslide()) skew *= impulsecrouchregenmeter;

        loopi(IM_T_MAX) if(impulsetimer(i, false, false) || impulsetimer(i, false, true)) switch(i)
        {
            case IM_T_JUMP: skew *= impulsejumpregenmeter;
            case IM_T_BOOST: skew *= impulseboostregenmeter;
            case IM_T_DASH: skew *= impulsedashregenmeter;
            case IM_T_SLIDE: skew *= impulseslideregenmeter;
            case IM_T_LAUNCH: skew *= impulselaunchregenmeter;
            case IM_T_MELEE: skew *= impulsemeleeregenmeter;
            case IM_T_KICK: skew *= impulsekickregenmeter;
            case IM_T_GRAB: skew *= impulsegrabregenmeter;
            case IM_T_WALLRUN: skew *= impulsewallrunregenmeter;
            case IM_T_VAULT: skew *= impulsevaultregenmeter;
            case IM_T_POUND: skew *= impulsepoundregenmeter;
            default: break;
        }

        return skew;
    }

    float impulseskewcount(bool onfloor)
    {
        float skew = 1.0f;

        if(running()) skew *= sprinting(false) ? impulsesprintregencount : impulserunregencount;
        if(move || strafe) skew *= impulsemoveregencount;
        if((!onfloor || hasslide()) && PHYS(gravity) > 0) skew *= impulseinairregencount;
        if(onfloor && crouching() && !hasslide()) skew *= impulsecrouchregencount;

        loopi(IM_T_MAX) if(impulsetimer(i, false, false) || impulsetimer(i, false, true)) switch(i)
        {
            case IM_T_JUMP: skew *= impulsejumpregencount;
            case IM_T_BOOST: skew *= impulseboostregencount;
            case IM_T_DASH: skew *= impulsedashregencount;
            case IM_T_SLIDE: skew *= impulseslideregencount;
            case IM_T_LAUNCH: skew *= impulselaunchregencount;
            case IM_T_MELEE: skew *= impulsemeleeregencount;
            case IM_T_KICK: skew *= impulsekickregencount;
            case IM_T_GRAB: skew *= impulsegrabregencount;
            case IM_T_WALLRUN: skew *= impulsewallrunregencount;
            case IM_T_VAULT: skew *= impulsevaultregencount;
            case IM_T_POUND: skew *= impulsepoundregencount;
            default: break;
        }

        return skew;
    }

    int impulsetimer(int type = -1, bool check = true, bool delay = false, int retval = 0)
    {
        if(type < 0 || type >= IM_T_MAX) type = impulse[IM_TYPE];

        if((check && impulse[IM_TYPE] != type) || !impulsetime[type]) return 0;

        int millis = lastmillis - impulsetime[type], timer = delay ? impulsedelay(type) : impulselen(type);
        if(!timer || millis > timer) return 0;

        switch(retval)
        {
            case 1: return millis;
            case 2: return timer - millis;
            case 3: return timer;
            default: break;
        }
        return impulsetime[type];
    }

    bool impulseready(int mask, bool check = false, bool delay = true)
    {
        loopi(IM_T_MAX)
        {
            if(!(mask&(1<<i))) continue;
            if(impulsetimer(i, check, delay)) return false;
        }
        return true;
    }

    bool impulseeffect()
    {
        loopi(IM_T_MAX) if(impulsetimer(i, true)) return true;
        return false;
    }

    bool regenimpulse()
    {
        if(!isalive() || !impulseready(impulseregenmask)) return false;
        if(impulseregendelay && lastmillis - impulse[IM_REGEN] < impulseregendelay) return false;
        return (impulsecostcount && impulse[IM_COUNT] > 0) || (impulsecostmeter && impulse[IM_METER] > 0);
    }

    bool canimpulse(int type)
    {
        if(!(A(actortype, impulse)&(1<<type))) return false;
        if(PHYS(gravity) != 0 && impulsetouchmasks&(1<<type) && !(impulsetouchtypes&(1<<impulse[IM_TYPE]))) return false;
        if(impulsecounttypes&(1<<type) && impulse[IM_COUNT] >= impulsecount) return false;
        return impulseready(impulsemask(type));
    }

    bool hasparkour()
    {
        return impulsetimer(IM_T_WALLRUN) != 0 || impulsetimer(IM_T_DASH) != 0;
    }

    void updateturn(int side, float yaw = 0, int yawtime = 0, float pitch = 0, int pitchtime = 0, float roll = 0, int rolltime = 0)
    {
        turnside = side;

        if(yawtime >= 0)
        {
            turnangle[0] = yaw;
            turnmillis[0] = turntime[0] = yawtime;
        }

        if(pitchtime >= 0)
        {
            turnangle[1] = pitch;
            turnmillis[1] = turntime[1] = pitchtime;
        }

        if(rolltime >= 0)
        {
            turnangle[2] = roll;
            turnmillis[2] = turntime[2] = rolltime;
        }
    }

    void resetjump()
    {
        airmillis = turnside = 0;
        if(!impulsecostcount) impulse[IM_COUNT] = 0;
    }

    void resetair()
    {
        resetphys();
        resetjump();
    }

    void doimpulse(int type, int millis, int cost = 0, int reset = 0)
    {
        if(type < 0 || type >= IM_T_MAX) return;

        if(cost)
        {
            if(type == IM_T_PUSHER) impulse[IM_PUSHER] = cost;
            else impulse[IM_METER] += cost;
        }

        impulse[IM_TYPE] = type;
        impulse[IM_SLIP] = impulse[IM_REGEN] = impulsetime[type] = millis;
        if(cost && impulsecounttypes&(1<<type)) impulse[IM_COUNT]++;

        clearimpulse(impulseclear(type), impulsereset(type));
        resetair();

        if(reset&1) action[AC_JUMP] = false;
        if(reset&2) action[AC_SPECIAL] = false;
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

    bool setmixer(const char *v)
    {
        return clientstate::setmixer(v);
    }

    void setinfo(const char *n, int c1, int c2, int m, const char *v, const char *p, vector<int> &w, vector<int> &r)
    {
        setname(n);
        colours[0] = c1;
        colours[1] = c2;
        model = m;
        setvanity(v);
        setmixer(p);
        loadweap.shrink(0);
        loopv(w) loadweap.add(w[i]);
        randweap.shrink(0);
        loopv(r) randweap.add(r[i]);
    }

    bool hasmelee(int millis, bool check = true)
    {
        if(!(A(actortype, abilities)&(1<<A_A_MELEE))) return false;
        if(check && ((weapstate[W_MELEE] != W_S_PRIMARY && weapstate[W_MELEE] != W_S_SECONDARY) || millis - weaptime[W_MELEE] >= weapwait[W_MELEE])) return false;
        return true;
    }

    bool canmelee(int sweap, int millis, bool alt = false)
    {
        if(!hasmelee(millis, false)) return false;
        if(!canshoot(W_MELEE, alt ? HIT_ALT : 0, sweap, millis, (1<<W_S_RELOAD))) return false;
        return true;
    }

    int curfoot()
    {
        int foot = -1;
        vec fp = feetpos();

        float d0 = fabs(foottag(0).z - fp.z),
              d1 = fabs(foottag(1).z - fp.z);

        if(d0 < FOOTSTEP_DIST) foot = 0;
        if(d1 < d0 && d1 < FOOTSTEP_DIST) foot = 1;

        return foot;
    }

    bool crouching(bool check = false)
    {
        if(!(A(actortype, abilities)&(1<<A_A_CROUCH))) return false;
        return action[AC_CROUCH] || actiontime[AC_CROUCH] > 0 || (check && zradius > height);
    }

    bool running()
    {
        return hasslide() || (!action[AC_WALK] && !crouching());
    }

    bool sprinting(bool check = true)
    {
        return (!check || running()) && move > 0 && !strafe && sprinttime >= A(actortype, sprinttime);
    }

    bool hasslip()
    {
        return G(impulsesliplen) && impulse[IM_SLIP] && lastmillis - impulse[IM_SLIP] <= G(impulsesliplen);
    }

    bool hasslide()
    {
        return hasslip() || impulsetimer(IM_T_SLIDE) != 0 || impulsetimer(IM_T_DASH) != 0;
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

enum { INANIMATE_NONE = 0, INANIMATE_RAIL, INANIMATE_MAX };
enum { INANIMATE_C_KILL = 0, INANIMATE_C_NOPASS, INANIMATE_C_MAX, INANIMATE_C_ALL = (1<<INANIMATE_C_KILL)|(1<<INANIMATE_C_NOPASS) };

struct passenger
{
    physent *ent;
    vec offset;
    bool local;

    passenger(physent *d = NULL, const vec &off = vec(0, 0, 0), bool loc = true) : ent(d), offset(off), local(loc) {}
    ~passenger() {}
};

struct inanimate : dynent
{
    int control, ent, coltype, curstep;
    float yawed, pitched;
    vec moved, resized;
    vector<passenger> passengers;
    physent *coltarget;

    inanimate() : control(INANIMATE_NONE), ent(-1), coltype(0), curstep(0), yawed(0), pitched(0), moved(0, 0, 0), resized(0, 0, 0), coltarget(NULL)
    {
        physent::reset();
        type = ENT_INANIMATE;
        state = CS_ALIVE;
    }

    ~inanimate()
    {
        removetrackedparticles(this);
        removetrackedsounds(this);
    }

    static bool is(int t) { return t == ENT_INANIMATE; }
    static bool is(physent *d) { return d && d->type == ENT_INANIMATE; }

    int findpassenger(physent *d)
    {
        loopv(passengers) if(passengers[i].ent == d) return i;
        return -1;
    }

    bool localpassenger(physent *d)
    {
        int n = findpassenger(d);
        if(n >= 0)
        {
            passenger &p = passengers[n];
            p.offset = vec(d->o).sub(o);
            return false;
        }
        passengers.add(passenger(d, vec(d->o).sub(o), true));
        return true;
    }

    bool remotepassenger(physent *d, const vec &offset)
    {
        int n = findpassenger(d);
        if(n >= 0)
        {
            passenger &p = passengers[n];
            p.offset = offset;
            p.local = false;
            return false;
        }
        passengers.add(passenger(d, offset, false));
        return true;
    }
};

struct projent : dynent
{
    projent *prev, *next;

    vec from, dest, norm, inertia, sticknrm, stickpos, effectpos, trailpos, lastgood;
    int addtime, lifetime, lifemillis, waittime, spawntime, fadetime, lastradial, lasteffect, lastbounce, beenused, extinguish, stuck;
    float movement, distance, lifespan, lifesize, speedmin, speedmax;
    bool local, limited, escaped, child, bounced;
    int projtype, projcollide, interacts;
    float elasticity, relativity, liquidcoast;
    int seqid, schan, id, weap, flags, fromweap, fromflags, value, collidezones;
    gameent *owner, *target, *stick;
    physent *hit;
    const char *mdlname;
    bvec material;
    fx::emitter *effect;
    int fxtype;

    projent() : projtype(PROJ_SHOT), seqid(-1), id(-1), collidezones(CLZ_NONE), owner(NULL), target(NULL),
        stick(NULL), hit(NULL), mdlname(NULL), effect(NULL), fxtype(FX_P_NONE) { reset(); }
    ~projent()
    {
        if(owner) listremove(this, owner->projchain, prev, next);

        removetrackedparticles(this);
        removetrackedsounds(this);
        if(issound(schan)) soundsources[schan].clear();
        if(effect) effect->unhook();
        schan = -1;
    }

    static bool is(int t) { return t == ENT_PROJ; }
    static bool is(physent *d) { return d && d->type == ENT_PROJ; }
    static bool shot(int t, int w) { return t == ENT_PROJ && w == PROJ_SHOT; }
    static bool shot(physent *d) { return d && d->type == ENT_PROJ && ((projent*)d)->projtype == PROJ_SHOT; }

    bool isjunk(float scale = -1, bool suck = false) const
    {
        if(scale >= 0 && (state == CS_DEAD || !lifetime || (!suck && beenused) || fromflags&HIT_JANITOR)) return false;

        if((projtype == PROJ_DEBRIS || projtype == PROJ_GIB) && (scale < 0 || lifespan >= janitorjunkdebris * scale)) return true;
        if((projtype == PROJ_VANITY || projtype == PROJ_PIECE || projtype == PROJ_EJECT) && (scale < 0 || lifespan >= janitorjunktime * scale)) return true;
        if(projtype == PROJ_ENTITY && (scale < 0 || lifespan >= janitorjunkitems * scale)) return true;
        return false;
    }

    void reset()
    {
        physent::reset();
        type = ENT_PROJ;
        state = CS_ALIVE;
        norm = vec(0, 0, 1);
        inertia = sticknrm = stickpos = lastgood = vec(0, 0, 0);
        effectpos = vec(-1e16f, -1e16f, -1e16f);
        addtime = lifetime = lifemillis = waittime = spawntime = fadetime = lastradial = lasteffect = lastbounce = beenused = flags = fromflags = 0;
        seqid = schan = id = weap = fromweap = value = -1;
        movement = distance = lifespan = speedmin = speedmax = 0;
        curscale = lifesize = 1;
        extinguish = stuck = interacts = 0;
        limited = escaped = child = bounced = false;
        projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
        material = bvec(255, 255, 255);
        if(effect) effect->unhook();
        fxtype = FX_P_NONE;
    }

    bool ready(bool used = true)
    {
        if(owner && (!used || projtype == PROJ_SHOT || !beenused) && waittime <= 0 && state != CS_DEAD)
            return true;
        return false;
    }

    bool usestuck()
    {
        if(projcollide&COLLIDE_SCAN) return false;
        return stuck != 0;
    }

    float getfade(bool used = true, bool test = true)
    {
        if(!ready(used) | !fadetime || !lifemillis) return 0.f;

        int interval = min(lifemillis, fadetime);
        if(lifetime < interval) return float(lifetime) / float(interval);
        else if((!test || projtype != PROJ_EJECT) && lifemillis > interval)
        {
            interval = min(lifemillis - interval, fadetime);
            if(lifemillis - lifetime < interval)
                return float(lifemillis - lifetime) / float(interval);
        }

        return 0.f;
    }

    int getfadedir(bool used = true, bool test = true)
    {
        if(!ready(used) | !fadetime || !lifemillis) return 0;

        int interval = min(lifemillis, fadetime);
        if(lifetime < interval) return 1;
        else if((!test || projtype != PROJ_EJECT) && lifemillis > interval)
        {
            interval = min(lifemillis - interval, fadetime);
            if(lifemillis - lifetime < interval)
                return -1;
        }

        return 0;
    }
};

struct cament
{
    enum { ENTITY = 0, PLAYER, AFFINITY, MAX };

    int cn, type, id, inview[MAX], lastinview[MAX], lastyawtime, lastpitchtime;
    vec o, dir;
    float dist, lastyaw, lastpitch;
    gameent *player;
    bool ignore, chase, flagged, checked, visible;

    cament(int p, int t) : cn(p), type(t), id(-1), o(0, 0, 0), dir(0, 0, 0), player(NULL), ignore(false), chase(true), flagged(false), checked(false), visible(false)
    {
        reset();
        resetlast();
    }
    cament(int p, int t, int n) : cn(p), type(t), id(n), o(0, 0, 0), dir(0, 0, 0), player(NULL), ignore(false), chase(true), flagged(false), checked(false), visible(false)
    {
        reset();
        resetlast();
    }
    cament(int p, int t, int n, const vec &d) : cn(p), type(t), id(n), o(0, 0, 0), dir(0, 0, 0), player(NULL), ignore(false), chase(true), flagged(false), checked(false), visible(false)
    {
        reset();
        resetlast();
        o = d;
    }
    cament(int p, int t, int n, const vec &c, gameent *d) : cn(p), type(t), id(n), o(0, 0, 0), dir(0, 0, 0), player(d), ignore(false), chase(true), flagged(false), checked(false), visible(false)
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
        lastyawtime = lastpitchtime = totalmillis;
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
        return !rnd(a->player ? 2 : 3);
    }
};

namespace client
{
    extern bool demoplayback, isready, loadedmap;
    extern int showpresence, showpresencehostinfo, showteamchange, needsmap, gettingmap, triggerid, playercolour, playercolour2, playermodel;
    extern vector<uchar> messages;
    extern bool radarallow(const vec &o, gameent *d, vec &dir, float &dist, bool self = false);
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
    extern int smoothmove, smoothdist, physframetime, grabstyle, grabplayerstyle;
    extern float getdeathplane();
    extern bool isghost(gameent *d, gameent *e, bool proj = false);
    extern bool sticktospecial(physent *d, bool parkour = true);
    extern float liquidmerge(physent *d, float from, float to);
    extern int hasaffinity(gameent *d);
    extern bool secondaryweap(gameent *d);
    extern float impulsevelocity(physent *d, float amt, int &cost, int type, float redir, vec &keep);
    extern bool movepitch(physent *d, bool onlyfloat = false);
    extern bool movecamera(physent *d, const vec &dir, float dist, float stepdist);
    extern void smoothplayer(gameent *d, int res, bool local);
    extern void update();
    extern void reset();

#define GETLIQUIDFUNCS(name) \
    extern float get##name##buoyancy(int mat); \
    extern float get##name##buoyancyscale(int mat); \
    extern float get##name##falldist(int mat); \
    extern float get##name##fallspeed(int mat); \
    extern float get##name##fallpush(int mat); \
    extern float get##name##speed(int mat); \
    extern float get##name##speedscale(int mat); \
    extern float get##name##coast(int mat); \
    extern float get##name##coastscale(int mat); \
    extern float get##name##boost(int mat); \
    extern float get##name##boostscale(int mat); \
    extern float get##name##submerge(int mat); \
    extern float get##name##submergescale(int mat); \

    GETLIQUIDFUNCS(water)
    GETLIQUIDFUNCS(lava)

    extern float getwaterextinguish(int mat);
    extern float getwaterextinguishscale(int mat);
}
#define LIQUIDPHYS(name,mat) ((mat&MATF_VOLUME) == MAT_LAVA ? physics::getlava##name(mat)*physics::getlava##name##scale(mat) : physics::getwater##name(mat)*physics::getwater##name##scale(mat))
#define LIQUIDVAR(name,mat) ((mat&MATF_VOLUME) == MAT_LAVA ? physics::getlava##name(mat) : physics::getwater##name(mat))
#define WATERPHYS(name,mat) (physics::getwater##name(mat)*physics::getwater##name##scale(mat))

namespace projs
{
    extern vector<projent *> projs, collideprojs, junkprojs, typeprojs[PROJ_MAX];

    extern void mapprojfx();
    extern void reset();
    extern void update();
    extern projent *findprojseq(int type, int id);
    extern projent *create(const vec &from, const vec &to, bool local, gameent *d, int type, int fromweap, int fromflags, int lifetime, int lifemillis, int waittime, int speed, int id = 0, int weap = -1, int value = -1, int flags = 0, float scale = 1, bool child = false, gameent *target = NULL);
    extern void preload();
    extern void removeplayer(gameent *d);
    extern void updateattract(gameent *d, gameent *v, int weap, int flags);
    extern void destruct(gameent *d, int targ, int id, bool all = false);
    extern void sticky(gameent *d, int id, vec &norm, vec &pos, gameent *f = NULL);
    extern void shootv(int weap, int flags, int sub, int offset, float scale, vec &from, vec &dest, vector<shotmsg> &shots, gameent *d, bool local, gameent *v = NULL);
    extern void drop(gameent *d, int weap, int ent, int ammo = -1, bool local = true, int targ = -1, int index = 0, int count = 1, gameent *target = NULL);
    extern void render();
}

namespace weapons
{
    extern int slot(gameent *d, int n, bool back = false);
    extern vec *getweapsoundpos(gameent *d, int weaptag);
    extern bool weapselect(gameent *d, int weap, int filter, bool local = true);
    extern bool weapreload(gameent *d, int weap, int load = -1, int ammo = -1, int store = 0, bool local = true);
    extern void weapdrop(gameent *d, int w = -1);
    extern void checkweapons(gameent *d);
    extern float accmodspread(gameent *d, int weap, bool secondary, bool zooming);
    extern bool doshot(gameent *d, vec &targ, int weap, bool pressed = false, bool secondary = false, int force = 0, gameent *v = NULL);
    extern void shoot(gameent *d, vec &targ, int force = 0);
    extern void preload();
    extern bool canuse(int weap);
}

#define DEFUIVARS(name, surf, yaw, pitch, scale, world, maxdist, detentyaw, detentpitch) \
    VAR(IDF_PERSIST, name##ui, -1, surf, SURFACE_LAST); \
    FVAR(IDF_PERSIST, name##uiyaw, -1, yaw, 360); \
    FVAR(IDF_PERSIST, name##uipitch, -181, pitch, 181); \
    FVAR(IDF_PERSIST, name##uiscale, FVAR_NONZERO, scale, FVAR_MAX); \
    FVAR(IDF_PERSIST, name##uiworld, FVAR_NONZERO, world, FVAR_MAX); \
    FVAR(IDF_PERSIST, name##uimaxdist, 0, maxdist, FVAR_MAX); \
    FVAR(IDF_PERSIST, name##uidetentyaw, 0, detentyaw, 180); \
    FVAR(IDF_PERSIST, name##uidetentpitch, 0, detentpitch, 90);

#define CLEARUI(name, id, nottype) \
   for(int mui_scount = 0; mui_scount < SURFACE_ALL; ++mui_scount) if(mui_scount != nottype) UI::hideui(#name, mui_scount, id);

#define MAKEUI(name, id, cansee, pos) do { \
    int _makeui_type = (cansee) ? name##ui : SURFACE_WORLD; \
    if(_makeui_type >= 0) \
        UI::pokeui(#name, _makeui_type, id, pos, name##uimaxdist, name##uiyaw, name##uipitch, _makeui_type == SURFACE_WORLD ? name##uiworld : name##uiscale, name##uidetentyaw, name##uidetentpitch); \
} while(0);

namespace hud
{
    extern char *chattex, *playertex, *deadtex, *waitingtex, *spectatortex, *editingtex, *dominatingtex, *dominatedtex, *inputtex,
        *pointtex, *flagtex, *bombtex, *arrowtex, *arrowrighttex, *arrowdowntex, *arrowlefttex, *alerttex, *questiontex, *flagdroptex,
        *flagtakentex, *bombdroptex, *bombtakentex, *attacktex, *warningtex, *indicatortex, *crosshairtex, *hithairtex;
    extern int hudwidth, hudheight, hudsize;
    extern float radaraffinityblend, radarblipblend, radaraffinitysize;
    extern bool scoreson, scoresoff, shownscores;
    extern const char *teamtexname(int team = T_NEUTRAL);
    extern const char *itemtex(int type, int stype);
    extern const char *privtex(int priv = PRIV_NONE, int actortype = A_PLAYER);
    extern bool canshowscores();
    extern void showscores(bool on, bool interm = false, bool onauto = true, bool ispress = false);
    extern score &teamscore(int team);
    extern void resetscores();
    extern void removeplayer(gameent *d);
}

#define CTONE_ENUM(en, um) \
    en(um, Team, TEAM) en(um, Primary, PRIMARY) en(um, Secondary, SECONDARY) en(um, Combined, COMBINED) \
    en(um, Primary Team, PRIMARY_TEAM) en(um, Primary Alone, PRIMARY_ALONE) en(um, Primary Mix, PRIMARY_MIX) en(um, Primary Team Mix, PRIMARY_TEAM_MIX) en(um, Primary Alone Mix, PRIMARY_ALONE_MIX) \
    en(um, Secondary Team, SECONDARY_TEAM) en(um, Secondary Alone, SECONDARY_ALONE) en(um, Secondary Mix, SECONDARY_MIX) en(um, Secondary Team Mix, SECONDARY_TEAM_MIX) en(um, Secondary Alone Mix, SECONDARY_ALONE_MIX) \
    en(um, Combined Team, COMBINED_TEAM) en(um, Combined Alone, COMBINED_ALONE) en(um, Combined Mix, COMBINED_MIX) en(um, Combined Team Mix, COMBINED_TEAM_MIX) en(um, Combined Alone Mix, COMBINED_ALONE_MIX) \
    en(um, Maximum, MAX)
ENUM_DLN(CTONE);

namespace game
{
    extern int nextmode, nextmuts, lastzoom, lasttvcam, lasttvchg, spectvtime, waittvtime,
            maptime, mapstart, timeremaining, timeelapsed, timelast, timesync, bloodfade, bloodsize, bloodsparks, damageinteger,
            announcefilter, dynlighteffects, followthirdperson, nogore, playerovertone, playerundertone, playerdisplaytone, playerhalotone, playerfxtone,
            follow, specmode, spectvfollow, clientcrc;
    extern float bloodscale, aboveitemiconsize, playerovertonelevel, playerundertonelevel, playerdisplaytonelevel, playerhalotonelevel, playerfxtonelevel,
            playerovertonemix, playerundertonemix, playerdisplaytonemix, playerhalotonemix, playerfxtonemix, affinityfadeat, affinityfadecut, affinityfollowblend, affinitythirdblend, damagedivisor, damagecritical,
            playerrotdecay, playerrotinertia, playerrotmaxinertia;
    extern bool zooming, wantsloadoutmenu;
    extern vec swaypush, swaydir;
    extern string clientmap;
    extern int attrmap[W_MAX];
    extern int showweapfx;

    extern gameent *player1, *focus;
    extern vector<gameent *> players, waiting;

    struct avatarent : dynent
    {
        avatarent() { type = ENT_CAMERA; }
    };
    extern avatarent avatarmodel, bodymodel;

    extern fx::FxHandle getweapfx(int type);
    extern bool needname(gameent *d);
    extern const char *vanityfname(gameent *d, int n, int head = -1, bool proj = false);
    extern void followswitch(int n, bool other = false);
    extern vector<cament *> cameras;
    extern gameent *newclient(int cn);
    extern gameent *getclient(int cn);
    extern gameent *intersectclosest(vec &from, vec &to, gameent *at);
    extern void clientdisconnected(int cn, int reason = DISC_NONE);
    extern const char *colourname(const char *name, int clientnum, int team, int actortype, int col, int privilege, int weapselect, bool icon = false, bool dupname = true, int colour = 3);
    extern const char *colourname(gameent *d, const char *name = NULL, bool icon = false, bool dupname = true, int colour = 3);
    extern const char *colourteam(int team, const char *icon = "");
    extern int findcolour(int team, int colour, int weapselect, bool tone = true, float level = 1, float mix = 0);
    extern int findcolour(gameent *d, bool sec = false, bool tone = true, float level = 1, float mix = 0);
    extern int getcolour(gameent *d, int type = 0, float level = 1, float mix = 0);
    extern void errorsnd(gameent *d);
    extern void specreset(gameent *d = NULL, bool clear = false);
    extern float spawnfade(gameent *d);
    extern float protectfade(gameent *d);
    extern float opacity(gameent *d, bool third = true, bool effect = false);
    extern void respawn(gameent *d);
    extern void respawned(gameent *d, bool local, int ent = -1);
    extern void spawneffect(int type, const vec &pos, float radius, int colour, float size);
    extern void impulseeffect(gameent *d, float gain = 1, int effect = 0);
    extern void suicide(gameent *d, int flags = 0);
    extern void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch);
    extern void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float yawspeed = 1, float pitchspeed = 1, float rotate = 0);
    extern bool allowmove(physent *d);
    extern void checkzoom();
    extern bool inzoom();
    extern float zoomscale();
    extern bool camcheck(vec &pos, int csize);
    extern bool tvmode(bool check = true);
    extern float cameradist();
    extern void resetcamera(bool cam = true, bool input = true);
    extern void resetsway();
    extern void resetworld();
    extern void resetstate();
    extern gameent *bestdamagemerge(gameent *d);
    extern bool hasdamagemerge(gameent *d, gameent *e);
    extern void hiteffect(int weap, int flags, int fromweap, int fromflags, int damage, gameent *d, gameent *v, vec &dir, vec &vel, float dist, bool local = false);
    extern void damaged(int weap, int flags, int fromweap, int fromflags, int damage, int health, gameent *d, gameent *v, int millis, vec &dir, vec &vel, float dist);
    extern void killed(int weap, int flags, int fromweap, int fromflags, int damage, gameent *d, gameent *v, vector<gameent*> &log, int style, int material);
    extern void timeupdate(int state, int remain, int elapsed, int wait = 0);
    extern void footstep(gameent *d, int curfoot = -1);
    extern void getplayermaterials(gameent *d, modelstate &mdl);
    extern const char *getplayerstate(gameent *d, modelstate &mdl, int third = 1, float size = 1, int flags = 0, modelattach *mdlattach = NULL, bool vanitypoints = false);
    extern void haloadjust(const vec &o, modelstate &mdl);
    extern bool haloallow(const vec &o, gameent *d, bool justtest = false);
}

namespace entities
{
    extern vector<inanimate *> inanimates;
    extern int showentfull, showentweapons, entityeffect;
    extern float showentavailable, showentunavailable, entityeffecttime, entityeffectblend, entityeffectfade, entityeffectslice, entityeffectbright;
    extern void physents(physent *d);
    extern void runrails();
    extern void updaterails();
    extern void localpassenger(inanimate *m, physent *d);
    extern inanimate *remotepassenger(int ent, physent *d, const vec &offset);
    extern void updatepassengers();
    extern inanimate *currentpassenger(physent *d);
    extern int announce(int idx, gameent *d = NULL, int chan = -1, int flags = 0, float gain = 1);
    extern bool execitem(int n, int cn, dynent *d, float dist, bool local);
    extern bool collateitems(dynent *d, vec &pos, float radius);
    extern void checkitems(dynent *d);
    extern void putitems(packetbuf &p);
    extern void setspawn(int n, int m, int p = 0);
    extern bool tryspawn(dynent *d, const vec &o, float yaw = 0, float pitch = 0);
    extern void spawnplayer(gameent *d, int ent = -1, bool suicide = false);
    extern void useeffects(gameent *d, int cn, int ent, int ammoamt, bool spawn, int weap, int drop, int ammo = -1, int delay = 0);
    extern bool haloallow(const vec &o, int id, bool justtest = false);
    extern void checkui();
    extern void render();
    extern void update();
    extern void reset();
}
#endif
#include "capture.h"
#include "defend.h"
#include "bomber.h"
#endif

#define GAMELOG_ENUM(en, um) en(um, Event, EVENT) en(um, Message, MESSAGE) en(um, Death, DEATH) en(um, Maximum, MAX)
ENUM_DLN(GAMELOG);

#define GAMELOG_F_ENUM(en, um) en(um, NONE, 0) en(um, CLIENT1, 1<<0) en(um, CLIENT2, 1<<1) en(um, CLIENTN, 1<<2) en(um, BROADCAST, 1<<3) \
    en(um, CLIENTS, um##_CLIENT1|um##_CLIENT2|um##_CLIENTN) en(um, ALL, um##_CLIENT1|um##_CLIENT2|um##_CLIENTN|um##_BROADCAST)
ENUM_AL(GAMELOG_F);

#ifndef CPP_GAME_SERVER
#include "gamelog.h"
#ifdef CPP_GAME_MAIN
#define ENUM_DEFINE
#endif
#include "enum.h"
#endif
