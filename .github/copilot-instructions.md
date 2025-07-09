# Rules
**Avoid modifying**: `src/lib`, `src/include`, `src/support`, as well as `src/steam` and `src/enet` as these are external dependencies.
**File creation**: Use lowercase names without underscores (`src/module/file.h`) unless markdown files. Organize files by feature or subsystem, e.g., `src/render`, `src/network`, `src/game`.
**Standard**: C++17 maximum - Prefer using engine containers and systems instead of external resources, and create or extend engine types when they are insufficient.
**Naming:** Classes PascalCase (`GameEntity`), namespaces/functions/variables lowercase no underscores (`playerhealth`), constants UPPERCASE (`MAX_PLAYERS`)
**Formatting:** Opening braces on new lines, no spaces after keywords `if(condition)`, 4 spaces indent, keep lines <200 chars.
**Data:** Use engine containers `vector<T>`, `string`, `hashtable<K,V>`. Iteration macros `loopi(n)`, `loopv(container)`. Prefer stack allocation, use `copystring()`, `formatstring()`. Dynamic: `new`/`DELETEA`/`DELETEP`
**Enums:** Use `enum.h` macros for CubeScript-accessible enumerations: `ENUM_DLN(prefix)` for enums with names/lists, `ENUM_VAR(name, val)` for constants
**Error Handling:** `conoutf(colourred, "Error: %s", msg)` for errors, `conoutf(colouryellow, "Warning")` for warnings. Use `ASSERT(condition)` for debug, bounds checking for runtime checks.
**Input Validation:** Always validate: `if(!clients.inrange(clientid)) return false`. Network input: bounds check arrays, null-check pointers. Use `clamp(value, min, max)` for ranges.
**Performance:** Reuse containers: `static vector<int> list; list.setsize(0)`. Use `formatstring(msg, "format", args)`.
**Encapsulation:** Use classes and namespaces to encapsulate functionality. Avoid global variables unless necessary. Use RAII for resource management.
**Changes:** Carefully add and remove elements rather than rewriting entire files as doing so can cause corruption.
**Comments:** Use `//` only. Your code should be self-explanatory and should have as few comments as possible but provide useful information to systems such as IntelliSense.
**Reviews:** When reviewing code, focus on architecture, security, and validation. Ensure changes follow the rules above and improve the overall structure of the codebase.

# Engine Overview
Self-contained engine with minimal dependencies. Cube 2: Sauerbraten fork with Tesseract renderer. ENet-based networking with reliable UDP. CubeScript handles configuration, UI, shaders, and game logic.

# Code Examples
```cpp
// Variable definitions using engine macros
VAR(IDF_PERSIST, playerhealth, 1, 100, 1000);
FVAR(IDF_PERSIST, movespeed, 0.1f, 1.0f, 10.0f);
SVAR(IDF_PERSIST, playername, "");

// Iteration patterns
loopi(MAX_PLAYERS) if(clients[i]) updateclient(clients[i]);
loopv(entities) if(entities[i].type == ENT_WEAPON) spawnweapon(i);

// Memory management
char *msg = new char[MAXSTRLEN];
copystring(msg, "Hello World", MAXSTRLEN);
DELETEA(msg);

// Engine containers
vector<GameEntity *> entities;
hashtable<const char *, int> weaponstats;

// Console output
conoutf(colourred, "Error: %s", "Something went wrong");
conoutf(colouryellow, "Warning: %s", "This is a warning");

// Enumerations (CubeScript accessible)
#define WEAPON_ENUM(en, um) \
    en(um, Melee, MELEE) en(um, Pistol, PISTOL) en(um, Sword, SWORD)
ENUM_DLN(W)  // Creates W_MELEE, W_PISTOL, W_SWORD + W_LIST, W_NAMES, W_STR
ENUM_VAR(W_OFFSET, 1);  // Creates constant accessible from CubeScript
```
