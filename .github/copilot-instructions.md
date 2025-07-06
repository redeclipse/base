# Red Eclipse GitHub Copilot Instructions

## Engine Overview
Self-contained engine with no external dependencies. Cube 2: Sauerbraten fork with Tesseract renderer. ENet-based networking with reliable UDP. CubeScript handles configuration, UI, shaders, and game logic.

## Core Patterns

### Code Examples
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
vector<gameentity *> entities;
hashtable<const char*, int> weaponstats;
```

### File Organization
File naming: lowercase without underscores (`src/foundation/module/file.h`). Use focused, single-purpose files.
Avoid modifying: `src/lib`, `src/include`, `src/support` (core functionality).
Compatibility: C++11 maximum - prefer legacy C-style with minimal C++ extensions for portability.

**Naming:** Classes PascalCase (`GameEntity`), functions/variables lowercase no underscores (`playerhealth`), constants UPPERCASE (`MAX_PLAYERS`)

**Formatting:** Opening braces on new lines, no spaces after keywords `if(condition)`, 4 spaces indent, lines <200 chars

**Data:** Use engine containers `vector<T>`, `string`, `hashtable<K,V>`. Iteration macros `loopi(n)`, `loopv(container)`. Prefer stack allocation, use `copystring()`, `formatstring()`. Dynamic: `new`/`DELETEA`/`DELETEP`

**Headers:** Include order: `"cube.h"`, `"engine.h"`, `"game.h"`

**Anti-patterns:** Don't use `std::vector`, use `vector`. Don't use manual loops, use `loopi(count)`. Don't use `strcpy`, use `copystring(dest, src, sizeof(dest))`

**Error Handling:** `conoutf(colourred, "Error: %s", msg)` for errors, `conoutf(colouryellow, "Warning")` for warnings. Use `ASSERT(condition)` for debug, bounds checking for runtime checks.

**Input Validation:** Always validate: `if(!clients.inrange(clientid)) return false`. Network input: bounds check arrays, null-check pointers. Use `clamp(value, min, max)` for ranges.

**Performance:** Reuse containers: `static vector<int> list; list.setsize(0)`. Use `formatstring(msg, "format", args)`. Cleanup: `DELETEP(ptr)`, `DELETEA(array)`
