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
//        rifle, corroder, trident, grenade, mine, rocket, minigun, jetsaw, eclipse, melee)
WPVAR(IDF_GAMEMOD, 0, ammoclip, 1, VAR_MAX,
    1, 10, 1, 8, 40, 100, 30, 48, 6, 200, 1, 2, 2, 1, 500, 5, 99, 1
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

## Movie Recording (`src/engine/movie.cpp`)

`movie [name]` records the screen and sound to `<home>/<moviedir>/<name>.mp4` (or `.avi`), or stops the recording in
progress. With no name the file is named by `filetimeformat`. `moviecodec` picks the format:

- **AV1 MP4** (the default) or **VP9 MP4**: fragmented MP4 with Opus sound, encoded by SVT-AV1 or libvpx, at the constant
  quality `moviecrf`. A fragment starts at each key frame (`moviekeyint`), so a file cut short still plays up to its last
  one, and the lengths are filled in at the start when it closes. The video is BT.709 studio range and is marked as that.
- **MJPEG AVI** or **uncompressed I420 AVI**: OpenDML AVI, which can grow past 1GB, with 16 bit PCM sound. It only needs
  libraries every platform already links (SDL_image's JPEG writer and OpenAL Soft).

The compressed codecs aren't linked. `moviecodecs` loads opus, libvpx and SVT-AV1 with `SDL_LoadObject` the first time
they're wanted, using the headers bundled in `src/support` (SVT-AV1 must be version 4, as its configuration's layout
changes between versions). 64 bit Windows ships them in `bin/amd64`; elsewhere they come from the system. If one is
missing, AV1 falls back to VP9 and then to MJPEG AVI, saying so in the console, and without Opus a movie with sound is
recorded as MJPEG AVI. `dbgmovieskip` pretends libraries are missing, to test that.

- **Fixed timestep**: while recording, `updatetimer()` (`src/engine/server.cpp`) advances time by exactly one frame at
  `moviefps` (`recorder::step()`) instead of by the clock. Every rendered frame is one movie frame, so the movie plays back
  smoothly however long each frame takes to render and encode, and anything timed by `lastmillis`, like `sleep`, runs on
  the movie's time. The clock is rebased with `clockreset()` when it stops.
- **Sound**: starting a recording with `moviesound` reopens the sound device as an OpenAL Soft loopback device
  (`soundloopback`, `src/engine/sound.cpp`) through `resetsound()`, so the sound device itself plays nothing. Each frame renders
  exactly the samples up to the end of that frame with `soundcapture()`, which keeps the music stream filled first. As
  `musicmillis` counts what has been played, anything kept in time with the music stays in time with the movie. Stopping
  reopens the real device. With `moviemonitor`, each frame's sound is also queued to a second OpenAL device, opened the
  same way as the real one (`sounddevice`, then the default), so it can be heard while recording, keeping at most a
  quarter second queued (`soundmonitor()`).
- **Capture**: `swapbuffers()` calls `recorder::capture()` for frames drawn by the main loop, reading the back buffer at
  `renderw`×`renderh`. Frames go through a ring of jobs to `movieencoders` encoder threads, then a writer thread puts them
  in the file in order, each followed by its sound. Capture waits when the ring is full, which only slows recording down.
- Frames are scaled to `moview`×`movieh` when set (box filtered for I420, AV1 and VP9, linear for MJPEG). AV1 and VP9 are
  then fed to their encoder in order on the writer thread, each encoder using its own threads, and the sound is encoded
  to Opus in 20ms packets.

```
// record 10 seconds of whatever is on screen, starting the menu theme from the top with it
soundmusicmenuvol 1; movie test; sleep 10000 [movie]
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
