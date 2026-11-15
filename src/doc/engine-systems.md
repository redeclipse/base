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

## Blendmap (`src/engine/blend.cpp`)

The blendmap paints each texture's blend layer (`texlayer`, `vlayer`) over its top texture. It is kept per axis in
4-unit slices along that axis, each a sparse quadtree over the other two axes (`R[axis]`, `C[axis]`) with a sample
every 2 units; 0xFF is the top texture and 0 the layer. A face reads the slice just in front of it on its own axis,
chosen by `blendslice` in `octa.h`; the shader's `blendlookup` (`config/glsl/shared.cfg`) applies the same rule, so
keep the two identical.

- Stamps (`paintblendmap`) project the brush from the camera like a projector. Its image lies on the aimed surface, in
  the plane through the aim point with its normal, `blendbrushscale` times 2 units per brush pixel, and every blend
  layer sample the camera can see through it is painted, in its own face's axis, so strokes wrap over bumps, corners
  and walls; only samples more than `blendbrushreach` units behind that plane are left, so paint never falls past a ledge.
  What the camera can see through the brush comes from a small depth render of the world geometry
  (`renderblendstampdepth`), so it matches what is drawn, concave terrain included; without one each line of sight is cast.
- Parallel surfaces that read the same slice share paint: same-facing surfaces less than 4 units apart, or a floor and
  a ceiling less than 8 units apart.
- Paint no face reads is pruned on save and by `pruneblendmap`, both only with no other players (they would keep it);
  samples just past a face's edge, diagonals included, are filled from it so filtering doesn't fade the edge, and where a
  slope carries on into the next slice they take the slope's value there, so a slope's slices meet without banding.
- The GPU copy is a sparse hierarchy per axis (a root over 4096-unit regions, then chunk, page and slice tables) in
  one index texture plus a tile atlas, made only where paint is, so it grows with the paint rather than the map. Pages
  with identical tiles share one, reference counted. It is updated by `updateblendtextures` for a box; blended faces
  pass their orientation in the vertex texcoord's z.
- Maps before version 57 have a top-down blendmap, converted at load by `convertblendmap`.
- Paint travels with geometry as a blend region (`copyblendregion`/`pasteblendregion`): the slices around a box, moved
  through a `BlendXform` (translate, flip, quarter turn). Cube undo records, the clipboard and prefabs (version 1)
  carry one, and stamps and selection edits record paint-only undo records (an empty block plus a region). A paste at
  another grid size scales its cubes, which slices can't follow, so each face sample in the pasted box instead takes
  the value the copy read at the matching point.
- Other clients get a stamp as the samples it changed (`BlendDelta`, `N_EDITPAINT`, `applyblenddelta`), since brushes
  are local and can be added or rotated, and so two players painting nearby keep each other's strokes; other blendmap
  edits are `N_EDITBLEND` (`BLENDEDIT_ARGS` ints, `mpeditblend`).

## World Colours (`src/engine/rendergl.cpp`)

World colours are the colours the world is drawn with, resolved once a frame by `updateworldcols` and read with
`getworldcol(slot)`. The slots are `WORLDCOL_S_*` (`src/shared/ents.h`): the 15 sky, fog and light colours, 16 palette
colours that start white, then each of the 14 material colours for materials 1 to 4. Palette 3 indexes the same list,
so textures, lights, models, decals and particles can take any world colour, and script reads them with `getworldcol`.

- A slot's base is its map var, read raw through its getter (`getfogcolour`, `getwatercolourbase` and so on). The
  material getters (`getwatercolour(mat)` and the rest) return the resolved world colour, and a volfog colour left black
  falls back to the fog's.
- Darkness (`game::darkness`) is applied when a colour is read, for the pass drawing it (`worldcoldark` picks env, sun
  or none; ambient, sky light and atmo light are dimmed through their scales). Never darken a base or a stored world colour.
- Worldcol entities (`ET_WORLDCOL`: type, flags, value, select, modes, muts, variant) then change them: copies first,
  then the other blends, each in entity order, so a colour is set and then adjusted.
  Each combines its linked lights (`getlightfx` with `mapcheck`, so lightfx glow and triggers apply, and a light counts
  wherever the map has it, in dark modes and at any map effects level) into one colour, each weighted by how far pulse
  and flicker have shrunk it: summed for add and subtract, otherwise averaged with the strength scaled by the least
  shrunk light. It blends that in once by its type (`worldcolmix`) at its strength, `value` percent with 0 as full. Lights read palette 3 against the base colours
  during this, so no light sees an entity's result. A lit light that reads palette 3 still draws with the resolved
  colour, so one reading a slot its own entity changes shows the new colour while the entity mixed in the old one.
- Flags (`WORLDCOL_F_*`) bits 0-14 pick world colours, bit 15 the palettes named in select bits 0-15, and bits 16-29 the
  material colours for the materials named in select bits 16-19 (`WORLDCOL_SEL_*`). Bits 30-31 are free.
- A light with `L_COLOURONLY` only supplies its colour to worldcol entities: it lights nothing, casts no shadow and has
  no flare. Without the flag a linked light lights the world as well.
- Envmaps, the minimap and the deferred light shaders are made from world colours when a map loads, so `updateworldcols`
  runs before them in `load_world` and `allchanged`. Envmaps and the minimap keep the colours they were made with, and
  as triggers aren't known yet then, a worldcol whose lights a trigger switches is baked as untriggered.
- A change of the sunlight world colour clears the radiance hints and shadow cache, and the deferred light shaders when
  the sun turns on or off, as the `sunlight` var does. An animated sun therefore redoes these every frame.
- Decals batch by their palette and index rather than its colour, since the batch draws with its first decal's palette.
- Maps before version 61 kept the palettes in flags bits 15-30; `load_world` moves them to select as it reads the entities.

## Redirects (`src/shared/stream.cpp`)

A redirect names what to read in place of a file that has been removed, so maps, models and configs that still name
it keep working. It only fills in for a missing file, so it can never hide a copy someone actually has, and it builds
the replacement by plain text substitution rather than CubeScript, so a file lookup never runs script.

```cubescript
fileredirect "textures/old/*" "textures/new/$1"         // a renamed directory
fileredirect "sounds/gone*.ogg" "sounds/placeholder.ogg" // many removed files to one
fileredirect "<comp*>oldui *" "<comp$1>newui $2"         // a composite whose UI was removed
fileredirect "textures/gone.png" "<comp>placeholder"     // a texture file to a composite
fileredirect "textures/red.png" "textures/grey.png" 0xC04040 // a desaturated stand-in, tinted on map slots
fileredirect "textures/red_s.png" ""                       // a slot texture with no stand-in, dropped from the slot
```

- Redirects are defined in `config/redirect.cfg`, run by `setlocations` on the client and the server right after the
  package directories are added, before any other config, so every load sees them. The commands are not `IDF_SAFE`,
  so map script can't add or clear them.
- A redirect only applies to reads (`r`, `rb`, and `e` for `findfile`), and only once the file can't be found in the
  home directory, our own packages, the package directories or a zip. A copy that still exists always wins. Writes and
  directory listings (`listfiles`) never see redirects. Images, sounds and music try several names for one file (with
  each extension, in each sound folder), so when there are redirects they look for the original under all of them
  before taking a redirect for any (`redirectpasses`, `startredirectpass`, the second pass only following redirects);
  models load their textures as they go, so they don't, and a redirect for an earlier model file can win over a later
  one that's there.
- The removed path is a `cubepattern`: `*`, `?`, and `|` between alternatives, matched case-sensitively, with `\` read
  as `/`. Each wildcard is a capture, numbered from the left within the alternative that matched, and each `*` takes
  as little as it can. Paths are matched after `path` tidies them, so `a/b/../c.png` matches a redirect for
  `a/c.png`. The replacement fills in `$1` to `$9` as plain text, and `$$` is a `$`; a `$` for a capture every
  alternative doesn't have, or any other `$`, is rejected when the redirect is defined.
- Exact removed paths are checked before patterns, then patterns in the order they were defined; the first match
  wins. Defining the same removed path again replaces its replacement.
- An empty replacement drops the removed path: nothing is read in its place, and `fileredirectmissing` doesn't list
  it. A map slot loses the texture when the map loads, and moves to the shader without it, named by taking that part
  out of its shader's name: a specular map turns `specmap` into `spec` (keeping uniform specular at the slot's
  `specscale`), a height map takes out `parallax`, a normal map `bump` and `parallax` (and takes the height map with
  it, as the height is kept in the normal's alpha), a glow map `pulseglow` or `glow`, and an alpha mask `alpha`, and a
  shader left with no other part, such as `glowworld` without its glow, becomes `stdworld` (or `stddecal`). When no
  shader has the new name, the texture is still dropped and the shader kept, with a warning. A diffuse texture is the
  slot itself, so a drop that matches one is ignored with a warning. Saving the map saves the slot without the texture
  and with its new shader.
- A replacement that doesn't exist either is redirected again, up to `REDIRECT_DEPTH` (8) steps; a chain longer than
  that is reported once as a loop.
- Composites aren't files. A composite request matches its whole `<comp...>name args` string, and is redirected when
  its UI isn't defined (`UI::composite`). A texture file that can't be loaded is redirected to a composite when its
  redirects lead to one (`textureredirect`); only the file name is matched, so commands in front of it are dropped.
- What a map loads is ported when it loads (`portmap` in `worldio.cpp`, after the map config and materials run):
  anything that's gone but redirected takes its replacement's name, so saving the map saves the replacement and a
  resaved map no longer needs the redirect. Each is found the way its loader probes for it (`redirectprobe`), and is
  left alone if any probe finds the original, so a broad pattern can't port something that's still there:
  - slot textures (world, decal and material) and the map variables naming a texture (`cloudlayer`, `envlayer`, the
    cylinder layers, `hazetex`), with `loadsurface`'s extensions; commands in front of the file name are kept unless
    the replacement is a composite, and composite slot textures aren't ported, as whether their UI is gone is only
    known when they're built;
  - sky boxes (`skybox`, `cloudbox`), from the first side's replacement: the plain base name when every side is there
    under it, otherwise that replacement with its side name as the `*` wildcard `loadsky` takes;
  - map models, from the files each model type loads first (`md2.cfg` through `iqm.cfg`, then `tris.*`), so a folder
    needs a `name/*` redirect whose replacement ends with the same file;
  - map sounds and `mapmusic`, from each sound or music folder with each sound extension, saved without the
    replacement's extension; every variant of a map sound has to be there under the new name, or it isn't ported.
- The optional colour (`0xRRGGBB`, default white) is for desaturated stand-ins: when a slot's diffuse texture is
  ported, every variant of the slot has its colour multiplied by it, so it's saved as `texcolour` and in the variants.
  The colours along a chain of redirects multiply. Other texture types, and anything that isn't a slot, ignore it.
- What a path matched (or that nothing did) is cached per step and cleared whenever a redirect is defined or cleared.
  Whether the original exists is checked on every request, so a file that comes back takes over at once.
- `fileredirectverbose` (default 1) logs each removed path the first time it's redirected.
- `fileredirectmissing` (default 0) logs `Missing with no redirect: name` once for each piece of content that can't be
  found and has no redirect, even where loading is otherwise quiet, to list what needs one. It covers texture files,
  composites, sound samples (named without an extension, which a redirect can match as is), and models that are
  required (map models, collide models and preloads not marked quiet) whose folder is gone; a model folder needs a
  `name/*` redirect. Content that exists but fails to load isn't listed, and neither are optional probes, such as the
  texture an `.md5mesh` names for each mesh, which is the exporter's own path and which the model's config replaces.
- `fileredirectlist` returns the redirects as a list of removed path, replacement and colour triples, `fileredirectmatch path`
  returns the replacement one step from `path` whether or not it exists, and `fileredirectclear` removes them all.
- The table and its cache are main-thread state, like `findfile`'s result buffer.

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
