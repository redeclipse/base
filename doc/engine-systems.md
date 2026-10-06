# Red Eclipse Engine Systems Reference

This document provides detailed technical information about Red Eclipse's engine systems and APIs.

## Variables and Commands System

### Variable Definitions (`src/shared/command.h`)
```cpp
VAR/FVAR/SVAR(flags, name, min, def, max)        // Integer/Float/String variables
VARF/FVARF/SVARF(flags, name, min, def, max, body) // Variables with callbacks
COMMAND/ICOMMAND(flags, name, args, code)        // Basic/Inline commands

// Common flags
IDF_PERSIST    // Save to config file
IDF_READONLY   // Cannot be modified at runtime
IDF_CLIENT     // Client-side variable
IDF_SERVER     // Server-side variable
IDF_GAMEMOD    // Game modification variable
IDF_MAP        // Map-specific variable
IDF_HEX        // Display as hexadecimal

// Examples
VARF(IDF_PERSIST, playerhealth, 1, 100, 1000, setplayerhealth(playerhealth));
FVAR(IDF_PERSIST, footstepsoundmin, 0, 0, FVAR_MAX);
SVAR(IDF_PERSIST, textfontdef, "titillium/clear");
```

### Weapon Definition Patterns (`src/game/weapdef.h`, `src/game/weapons.h`)
```cpp
// Macros are defined in weapdef.h, the weapon variables themselves in weapons.h
// WPVAR: one value per weapon (claw, pistol, sword, shotgun, smg, flamer, plasma, zapper,
//        rifle, corroder, grenade, mine, rocket, minigun, jetsaw, eclipse, melee)
WPVAR(IDF_GAMEMOD, 0, ammoclip, 1, VAR_MAX,
    1, 10, 1, 8, 40, 100, 30, 48, 6, 200, 2, 2, 1, 500, 5, 99, 1
);
// WPVARM: two rows (primary, secondary fire)
// WPVARK: four rows (primary, secondary, then flak primary, flak secondary as flak<name>), e.g. damage

// Access weapon stats with the accessor macros rather than the raw weap_stat_ arrays
int clip = W(weap, ammoclip);                // single value
int damage = W2(weap, damage, secondary);    // primary/secondary value
int dmg = WF(WK(flags), weap, damage, WS(flags)); // primary/secondary, or flak when HIT_FLAK is set
```

### Game Variables (`src/game/vars.h`)
```cpp
// Game modification variables using GVAR/GFVAR macros
GVAR(IDF_GAMEMOD, 0, itemspawnstyle, 0, 0, 3);
GFVAR(IDF_GAMEMOD, 0, movespeed, FVAR_NONZERO, 1.0f, FVAR_MAX);
GSVAR(0, PRIV_MODERATOR, janitorvanities, "");
```

### Enums and Entities
```cpp
// CubeScript-accessible enums using ENUM_DLN macro (weapons are in src/game/weapons.h)
#define W_ENUM(en, um) en(um, claw, CLAW) en(um, pistol, PISTOL) en(um, sword, SWORD) /* ... */ en(um, maximum, MAX)
ENUM_DLN(W);  // Creates W_CLAW, W_PISTOL, W_SWORD, ... W_MAX, plus W_LIST/W_NAMES lists for CubeScript

// Entity type definitions with full metadata
extern const enttypes enttype[];
// Access: enttype[WEAPON].name, enttype[WEAPON].attrs[0]

// Entity/physics patterns
gameent *d = game::getclient(clientnum);
if(d && d->isalive()) physics::move(d, 10, true); // physent, move resolution, local
```

## Network Protocol System

### Basic Network Communication
```cpp
// Always validate network input, use bounds checking
packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
putint(p, N_SERVMSG);
sendstring(text, p);
sendpacket(ci->clientnum, 1, p.finalize()); // client, channel, packet

// Network message handling
void parsemessages(int cn, gameent *d, ucharbuf &p)
{
    while(p.remaining())
    {
        int type = getint(p);
        switch(type)
        {
            case N_SERVMSG:
            {
                string text;
                getstring(text, p);
                // Handle server message
                break;
            }
        }
    }
}
```

## Map Variant Variables (MPV)

### Pattern for Map Variant Support
```cpp
#define MPVVARS(name, type) \
    VARF(IDF_MAP, haze##name, 0, 0, 1, hazesurf.create()); \
    CVAR(IDF_MAP, hazecolour##name, 0); \
    FVAR(IDF_MAP, hazecolourmix##name, 0, 0.5f, 1);

MPVVARS(, MPV_DEFAULT);      // Standard variables
MPVVARS(alt, MPV_ALTERNATE); // Alternate map variant

// Access via getter functions (see src/engine/renderfx.cpp, src/engine/rendersky.cpp)
#define GETMPV(name, type) \
    type get##name() { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

GETMPV(hazecolourmix, float);
GETMPV(hazecolour, const bvec &);
```

## Namespace Organization Patterns

### HUD System (`src/game/hud.cpp`)
```cpp
namespace hud
{
    VAR(IDF_PERSIST, visorfxdelay, 0, 3000, VAR_MAX);
    
    void drawpointer(int w, int h, int s, int index, float x, float y, float blend)
    {
        // HUD rendering logic
    }
    
    void drawindicator(int weap, int x, int y, float s, bool secondary, float blend)
    {
        // Indicator drawing
    }
}
```

### Entity Management (`src/game/entities.cpp`)
```cpp
namespace entities
{
    DEFUIVARS(entityitem, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);
    FVAR(IDF_PERSIST, entitymaxdist, 0, 1024, FVAR_MAX);
    VAR(IDF_PERSIST, entityicons, 0, 1, 1);
    
    void renderentities()
    {
        // Entity rendering logic
    }
}
```

### Game State Management (`src/game/game.cpp`)
```cpp
namespace game
{
    ICOMMAND(0, needname, "b", (int *cn), intret(needname(*cn >= 0 ? getclient(*cn) : player1) ? 1 : 0));
    
    bool allowmove(physent *d)
    {
        if(gameent::is(d))
        {
            if((d == player1 && tvmode()) || d->state == CS_DEAD || d->state >= CS_SPECTATOR || !gs_playing(gamestate))
                return false;
        }
        return true;
    }
}

// Connection handling lives in the client namespace (src/game/client.cpp)
namespace client
{
    void gameconnect(bool _remote)
    {
        remote = _remote;
        if(editmode) toggleedit(true);
        loopi(SURFACE_ALL) UI::hideui(NULL, i);
        game::updatemusic(10, true);
    }
}
```

### AI Behavior (`src/game/ai.cpp`)
```cpp
namespace ai
{
    bool wantsweap(gameent *d, int weap, bool noitems = true)
    {
        if(!isweap(weap) || !m_maxcarry(d->actortype, game::gamemode, game::mutators)) return false;
        // checks item weapons and the actor's loadout against how many weapons it may carry
    }
    
    void think(gameent *d, bool run)
    {
        // AI decision making
    }
}
```

## Memory Management Patterns

### Safe String Operations
```cpp
// Use engine string utilities for safety
string result;
copystring(result, source);                    // Safe string copy
formatstring(result, "format %d", value);      // Safe string formatting
concatstring(result, " suffix");               // Safe concatenation

// Buffer management
bigstring longtext;
formatstring(longtext, "long format string %s %d", str, num);
```

### Container Usage
```cpp
// Engine containers (NOT std::)
vector<gameent*> players;
hashtable<const char*, int> lookup;
string filename;

// Preferred iteration patterns
loopi(players.length()) { /* use players[i] */ }
loopv(players) { /* use players[i] */ }
loopirev(players.length()) { /* reverse iteration */ }
```

## Common Development Patterns

### Validation and Error Checking
```cpp
// Always validate inputs
bool checkweapon(int w)
{
    return isweap(w) && w >= 0 && w < W_MAX;
}

// Bounds checking with clamp
int health = clamp(newhealth, 1, maxhealth);
float speed = clamp(movespeed, 0.1f, 10.0f);

// Network input validation
void processmessage(ucharbuf &p)
{
    int msgtype = getint(p);
    if(msgtype < 0 || msgtype >= N_MAX) return; // Invalid message type
    
    string text;
    getstring(text, p);                         // Bounded string read (size taken from the buffer)
}
```

### Performance Optimization
```cpp
// Prefer stack allocation
void renderframe()
{
    static vector<entity*> visible;
    visible.setsize(0);
    
    // Use static buffers to avoid allocation
    static string tempbuf;
    formatstring(tempbuf, "temp %d", framecount);
}

// Efficient loops
loopv(entities)
{
    entity &e = entities[i];
    if(!e.visible) continue;
    render(e);
}
```
