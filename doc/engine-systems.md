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

### Weapon Definition Patterns (`src/game/weapdef.h`)
```cpp
// Multi-weapon variable definition using WPVAR macro
WPVAR(IDF_GAMEMOD, 0, damage, 1, 1000, 
    50, 35, 100, 80, 25,    // claw, pistol, sword, shotgun, smg
    30, 40, 35, 80, 50,     // flamer, plasma, zapper, rifle, corroder
    150, 75, 200, 40, 200,  // grenade, mine, rocket, minigun, jetsaw
    250, 50);               // eclipse, melee

// Access weapon stats
int damage = *weap_stat_damage[weapon];
float spread = *weap_stat_spread[weapon];
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
// CubeScript-accessible enums using ENUM_DLN macro
#define W_ENUM(en, um) en(um, claw, CLAW) en(um, pistol, PISTOL) en(um, sword, SWORD)
ENUM_DLN(W);  // Creates W_CLAW, W_PISTOL, W_SWORD

// Entity type definitions with full metadata
extern const enttypes enttype[];
// Access: enttype[WEAPON].name, enttype[WEAPON].attrs[0]

// Entity/physics patterns
gameent *d = getclient(clientnum);
if(d && d->isalive()) physics::move(d, 10, true);
```

## Network Protocol System

### Basic Network Communication
```cpp
// Always validate network input, use bounds checking
packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
putint(p, N_SERVMSG);
sendstring(text, p);
sendpacket(ci->clientnum, 1, p.finalize());

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

// Access via getter functions
#define GETMPV(name, type) \
    type get##name() { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

GETMPV(hazemix, float);
GETMPV(hazecolour, int);
```

## Namespace Organization Patterns

### HUD System (`src/game/hud.cpp`)
```cpp
namespace hud
{
    FVAR(IDF_PERSIST, visorfxdelay, 0, 1.0f, FVAR_MAX);
    VAR(IDF_PERSIST, visorhud, 0, 1, 1);
    
    void drawpointer(int w, int h, int s, int index, float x, float y, float blend)
    {
        // HUD rendering logic
    }
    
    void drawindicator(int w, int h, int s, float x, float y, float blend, int type)
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
        if(!d || !d->isalive()) return false;
        return physics::move(d, 10, true);
    }
    
    void gameconnect(bool local)
    {
        if(local) localconnect();
        else { /* remote connection setup */ }
        player1->resetstate();
    }
}
```

### AI Behavior (`src/game/ai.cpp`)
```cpp
namespace ai
{
    bool wantsweap(gameent *d, int weap, bool instant)
    {
        if(!isweap(weap) || !d->isalive()) return false;
        // AI weapon preference logic
        return hasweap(d, weap) && d->ammo[weap] > 0;
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
    getstring(text, p, MAXSTRLEN);              // Bounded string read
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
