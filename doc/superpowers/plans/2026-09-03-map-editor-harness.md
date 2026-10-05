# Map Editor Test Harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give an agent outside-the-process control over Red Eclipse's map editor — move the first-person view, lock/unlock the cursor with TAB, select entities and world geometry — with enough state readback to verify each action happened.

**Architecture:** Three layers. New `#ifdef DEBUG_UTILS` engine commands supply what CubeScript cannot reach (setting the view, reading `cur`/`sel`, synthesising `processkey` input). A `tools/harness/editor.cfg` script layer wraps those into idempotent editor operations that drive the *real* bind path. A `tools/harness/editor.ps1` driver shares the existing harness transport — one game instance, one command channel — via plumbing extracted into `tools/harness/core.ps1`.

**Tech Stack:** C++ (Cube 2 / Tesseract-derived engine, MSVC cross-build from WSL), CubeScript, Windows PowerShell 5.1.

**Spec:** [doc/superpowers/specs/2026-09-03-map-editor-harness-design.md](../specs/2026-09-03-map-editor-harness-design.md)

## Global Constraints

- **Every new engine command** is wrapped in `#ifdef DEBUG_UTILS` and returns early when `identflags&IDF_MAP`, matching `src/engine/ui.cpp:7462`. A downloaded map must never be able to drive the editor or synthesise input.
- **Build:** `wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug`. Stay on `debug` for the whole plan — switching build type triggers a full `make clean` (`src/build.sh:36`), and `debug` is the only type that compiles `src/tests/*.o` (`src/Makefile:350`). `DEBUG_UTILS` is defined for both types, so the harness works either way. A final `release` build is optional.
- **Runnable binary** is `bin/amd64/redeclipse.exe`. Never launch `src/redeclipse_windows_amd64.exe` — it cannot resolve its DLLs.
- **CubeScript:** no bare `#` (it is a macro preprocessor), avoid `@` (depth-sensitive substitution — use `concat`/`concatword`), always write `exec "path" 0 0` (three args), `onevent` takes an alias name not a block. See "CubeScript traps" in `CLAUDE.md`.
- **PowerShell:** Windows PowerShell 5.1. No `&&`/`||` chain operators, no ternary, no null-coalescing. Write files with `Write-TextNoBom` — a UTF-8 BOM is parsed by CubeScript as part of the first token.
- **Number parsing:** always via `ConvertTo-InvariantDouble`. The game prints `.` decimals; a comma-decimal locale otherwise mis-reads every coordinate.
- **Do not commit or push unless the user asks.** The `git commit` steps below are written for a normal workflow; if the user has not asked for commits, complete the step's file changes and skip the commit itself.
- **Commit message style:** lowercase `area: summary` (e.g. `engine: add gamekeypress for the editor harness`).
- **Screenshots:** never minimize the game window (they come back solid black). Unfocused is fine.

---

### Task 1: `gamekeypress` — synthetic game-level input

The editor's input path is `processkey` (`src/engine/console.cpp:867`) → `execbind` (`:625`). This is load-bearing rather than convenience: `addreleaseaction` returns early unless `keypressed` is set (`:578`), and only `execbind` sets it — so `onrelease`, and therefore `drag`, `moving` and `tool_grabbing`, cannot be reached by calling the aliases directly.

**Files:**
- Modify: `src/engine/console.cpp` (append after `processkey`, which ends at `:894`)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: CubeScript command `gamekeypress <key> <down>` → `1` if the key name/code resolved, `0` otherwise. `key` is a `config/keymap.cfg` name (`TAB`, `MOUSE1`, `LSHIFT`) or a raw integer code. Used by Tasks 7, 8, 9.

- [ ] **Step 1: Write the failing test**

There is no C++ unit-test path here — `findkeycode` reads the `keyms` table, which is only populated once `config/keymap.cfg` has been executed. This is verified against the running game instead.

Create `tools/harness/tests/task1-gamekeypress.cfg`:

```cubescript
// Task 1 verification. Run with: tools\harness\harness.ps1 send -File tools\harness\tests\task1-gamekeypress.cfg
echo (concatword "T1_NAME=" (gamekeypress TAB 0))
echo (concatword "T1_CODE=" (gamekeypress 9 0))
echo (concatword "T1_MOUSE=" (gamekeypress MOUSE1 0))
echo (concatword "T1_BOGUS=" (gamekeypress NOTAKEY 0))
```

- [ ] **Step 2: Run it to verify it fails**

```bash
tools\harness\harness.ps1 start
```

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task1-gamekeypress.cfg
```

Expected: four `Unknown command: gamekeypress` warnings, and `T1_NAME=` etc. empty.

- [ ] **Step 3: Write the implementation**

Append to `src/engine/console.cpp`, immediately after the closing brace of `processkey` (currently `:894`):

```cpp
#ifdef DEBUG_UTILS
// Map editor test harness support, see tools/harness/. Gated like the UI test
// commands in ui.cpp, and refused to map scripts so downloaded content cannot
// synthesise input.
//
// keyms is keyed by code, so a name lookup is a linear scan. That is fine at
// harness call rates, and keeps config/keymap.cfg as the single source of
// truth for key names.
int findkeycode(const char *name)
{
    if(!name || !*name) return INT_MIN;
    // Raw codes pass straight through, so a caller can use -1 for MOUSE1
    // without depending on the keymap having been loaded.
    if(isdigit(name[0]) || ((name[0] == '-' || name[0] == '+') && isdigit(name[1])))
        return atoi(name);
    int found = INT_MIN;
    enumerate(keyms, keym, km, { if(found == INT_MIN && !strcmp(km.name, name)) found = km.code; });
    return found;
}

// Unlike uikeypress, this drives the full dispatch chain (console, hud, UI,
// then binds), which is what makes execbind set keypressed -- without that,
// onrelease is a no-op and drag/moving cannot be driven at all.
ICOMMAND(0, gamekeypress, "si", (char *key, int *down),
{
    if(identflags&IDF_MAP) { intret(0); return; }
    int code = findkeycode(key);
    if(code == INT_MIN) { intret(0); return; }
    processkey(code, *down != 0);
    intret(1);
});
#endif
```

- [ ] **Step 4: Build**

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

Expected: build succeeds, `bin/amd64/redeclipse.exe` updated.

- [ ] **Step 5: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task1-gamekeypress.cfg
```

Expected: `T1_NAME=1`, `T1_CODE=1`, `T1_MOUSE=1`, `T1_BOGUS=0`, and no `Unknown command` warning.

- [ ] **Step 6: Commit**

```bash
git add src/engine/console.cpp tools/harness/tests/task1-gamekeypress.cfg
git commit -m "engine: add gamekeypress for the map editor harness"
```

---

### Task 2: `eddumpstate` — editor state readback

`cur`, `orient`, `gridsize` and `selchildcount` are file-scope in `src/engine/octaedit.cpp:116`–`313`; `hassel`/`havesel` return only counts. Without this command a geometry selection can be performed but never asserted, which makes the capability untestable. One structured dump keeps the driver to a single parser.

**Files:**
- Modify: `src/engine/octaedit.cpp` (append at end of file, currently 3615 lines)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: CubeScript command `eddumpstate` → prints the line format below, returns the number of `EDENT` lines emitted. Parsed by Task 6's `ConvertFrom-EdState`.

Line format (all coordinates `%.5f` except integer fields):

```
EDSTATE mode <editmode> <gridpower> <gridsize> <orient>
EDSTATE cam <x> <y> <z> <yaw> <pitch>
EDSTATE worldpos <x> <y> <z>
EDSTATE cur <x> <y> <z> <orient>
EDSTATE sel <ox> <oy> <oz> <sx> <sy> <sz> <grid> <orient> <cx> <cy> <cxs> <cys> <corner> <children> <havesel>
EDSTATE ui <cursorlock> <freecursor>
EDENT hover <idx> <type> <x> <y> <z>
EDENT sel <idx> <type> <x> <y> <z>
EDSTATE end <numents>
```

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task2-eddumpstate.cfg`:

```cubescript
// Task 2 verification. Needs a loaded map and edit mode.
newmap 12
sleep 500 [
    if (! $editing) [ edittoggle ]
    sleep 300 [ eddumpstate ]
]
```

- [ ] **Step 2: Run it to verify it fails**

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task2-eddumpstate.cfg -Settle 1500
```

Expected: `Unknown command: eddumpstate`.

- [ ] **Step 3: Write the implementation**

Append to the end of `src/engine/octaedit.cpp`. Everything referenced is already in scope: `cur`/`orient`/`gridsize`/`gridpower`/`selchildcount` are file-scope here, `sel`/`havesel`/`editmode` are extern in `src/shared/iengine.h:97`, `worldpos` at `:190`, `entgroup` in `src/engine/engine.h:1103`, `enthover` in `src/shared/ents.h:206`, `entities::findname`/`getents` via `src/shared/igame.h` (included by `cube.h`), and `getalias` in `src/shared/command.h:419`.

```cpp
#ifdef DEBUG_UTILS
// Map editor test harness support, see tools/harness/. One structured dump so
// the external driver needs a single parser; the format deliberately mirrors
// uidumptree's "TAG field field ..." convention.
//
// cur, orient, gridsize and selchildcount are file-scope here, which is why
// this lives in octaedit.cpp rather than beside the other harness commands.
static void dumpedent(const char *kind, int idx)
{
    const vector<extentity *> &ents = entities::getents();
    if(!ents.inrange(idx)) return;
    const extentity &e = *ents[idx];
    conoutf(colourwhite, "EDENT %s %d %s %.5f %.5f %.5f",
        kind, idx, entities::findname(e.type), e.o.x, e.o.y, e.o.z);
}

ICOMMAND(0, eddumpstate, "", (),
{
    conoutf(colourwhite, "EDSTATE mode %d %d %d %d",
        editmode ? 1 : 0, gridpower, gridsize, orient);
    conoutf(colourwhite, "EDSTATE cam %.5f %.5f %.5f %.5f %.5f",
        camera1->o.x, camera1->o.y, camera1->o.z, camera1->yaw, camera1->pitch);
    conoutf(colourwhite, "EDSTATE worldpos %.5f %.5f %.5f",
        worldpos.x, worldpos.y, worldpos.z);
    conoutf(colourwhite, "EDSTATE cur %d %d %d %d",
        cur.x, cur.y, cur.z, orient);
    conoutf(colourwhite, "EDSTATE sel %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
        sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z,
        sel.grid, sel.orient, sel.cx, sel.cy, sel.cxs, sel.cys, sel.corner,
        selchildcount, havesel ? 1 : 0);

    // ui_freecursor is a CubeScript alias (config/ui/lib.cfg:11), not a var,
    // so it has to come back through getalias.
    const char *freecursor = getalias("ui_freecursor");
    conoutf(colourwhite, "EDSTATE ui %d %s",
        UI::cursorlock() ? 1 : 0, freecursor && *freecursor ? freecursor : "0");

    int count = 0;
    loopv(enthover) { dumpedent("hover", enthover[i]); count++; }
    loopv(entgroup) { dumpedent("sel", entgroup[i]); count++; }

    conoutf(colourwhite, "EDSTATE end %d", count);
    intret(count);
});
#endif
```

`UI::cursorlock()` is declared in `src/shared/iengine.h:567`, so no extra declaration is needed.

- [ ] **Step 4: Build**

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

Expected: build succeeds.

- [ ] **Step 5: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task2-eddumpstate.cfg -Settle 1500
```

Expected: an `EDSTATE mode 1 3 8 0`-shaped line (edit mode on), `EDSTATE cam`/`worldpos`/`cur`/`sel`/`ui` lines, and `EDSTATE end 0` on a fresh empty map with nothing hovered. `EDSTATE sel` should end `... 0 0` (no children, no selection).

- [ ] **Step 6: Commit**

```bash
git add src/engine/octaedit.cpp tools/harness/tests/task2-eddumpstate.cfg
git commit -m "engine: add eddumpstate for the map editor harness"
```

---

### Task 3: View control commands

`game::mousemove` writes `player1->yaw/pitch` directly (`src/game/game.cpp:3062`) and nothing script-facing writes position or angles. These commands follow the editor's own established convention for moving the view — `game::focusedent(true)` falling back to `camera1`, then `resetinterp(true)` — as used by `entautoview` (`src/engine/world.cpp:896`) and `nearestent` (`:1247`).

**Files:**
- Modify: `src/engine/world.cpp` (append after `COMMAND(0, entautoview, "ii");` at `:920`)
- Modify: `src/engine/engine.h` (declare `orbitpos` near the other world helpers)
- Create: `src/tests/edharness.cpp`
- Modify: `src/Makefile:350-352` (add the new test object)
- Modify: `src/engine/main.cpp:1126-1127` (call the new test)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - C++ `vec orbitpos(const vec &target, float dist, float yaw, float pitch)` — camera position for an orbit view, declared in `engine.h`.
  - CubeScript: `edgoto <x> <y> <z>`, `edaim <yaw> <pitch>`, `edlookat <x> <y> <z>`, `ednudge <fwd> <right> <up>` (no return value); `edlookatent <idx>`, `edframeent <idx> <dist> <yaw> <pitch>` → `1` ok / `0` bad index; `edframe <x> <y> <z> <dist> <yaw> <pitch>` (no return value). Used by Tasks 8 and 9.

**Header availability, already checked:** `fixrange` is declared in `src/engine/engine.h:954`, `vectoyawpitch` in `src/shared/geom.h:2080`, `game::focusedent` in `src/shared/igame.h:153`, `entities::getents`/`findname` in `igame.h` (reached via `cube.h`). `game::getyawpitch` is **not** engine-visible — it lives in `src/game/game.h` — which is why `edaimat` uses `vectoyawpitch` instead.

- [ ] **Step 1: Write the failing test**

`orbitpos` is the one piece of pure logic here, and the repo already has a C++ test convention (`src/tests/slotmanager.cpp`, built only under `-D_DEBUG` per `src/Makefile:350`, invoked from `src/engine/main.cpp:1126`).

`vec(yaw, pitch)` is `(-sin(yaw)cos(pitch), cos(yaw)cos(pitch), sin(pitch))` (`src/shared/geom.h:109`) and is already unit length, so the expected values below are exact.

Create `src/tests/edharness.cpp`:

```cpp
#include "cube.h"

// Map editor test harness helpers, see tools/harness/.
static bool nearenough(float a, float b) { return fabs(a - b) < 0.001f; }

static bool vecnear(const vec &v, float x, float y, float z)
{
    return nearenough(v.x, x) && nearenough(v.y, y) && nearenough(v.z, z);
}

void testedharness()
{
    // vec(yaw, pitch) points +Y at yaw 0, so an orbit camera at yaw 0 sits on
    // the -Y side of its target and looks back along +Y.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, 0), 0, -10, 0));

    // Yaw 90 puts the direction on -X, so the camera sits on +X.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 90, 0), 10, 0, 0));

    // Looking straight down means sitting straight above.
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, 90), 0, 0, -10));
    ASSERT(vecnear(orbitpos(vec(0, 0, 0), 10, 0, -90), 0, 0, 10));

    // The target is an offset, not an origin.
    ASSERT(vecnear(orbitpos(vec(5, 5, 5), 10, 0, 0), 5, -5, 5));

    // Zero distance degenerates to the target itself.
    ASSERT(vecnear(orbitpos(vec(3, 4, 5), 0, 45, 30), 3, 4, 5));

    conoutf(colourwhite, "testedharness: ok");
}
```

Wire it in. In `src/Makefile`, extend the existing debug-only block at `:350`:

```make
# Build tests
ifneq (,$(findstring -D_DEBUG,$(CXXFLAGS)))
    CLIENT_OBJS += tests/slotmanager.o
    CLIENT_OBJS += tests/edharness.o
endif
```

In `src/engine/main.cpp`, beside the existing call at `:1126`:

```cpp
        extern void testslotmanager();
        testslotmanager();
        extern void testedharness();
        testedharness();
```

- [ ] **Step 2: Run it to verify it fails**

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

Expected: compile error — `orbitpos` was not declared in this scope.

- [ ] **Step 3: Write the implementation**

In `src/engine/engine.h`, beside the other world helpers (near the `entgroup` declaration at `:1103`):

```cpp
extern vec orbitpos(const vec &target, float dist, float yaw, float pitch);
```

In `src/engine/world.cpp`, append after `COMMAND(0, entautoview, "ii");` (`:920`):

```cpp
// Position for a camera sitting `dist` from `target` at orbit angles (yaw,
// pitch) and looking back at it. Same construction entautoview uses; kept
// non-static and declared in engine.h so src/tests/edharness.cpp can reach it.
vec orbitpos(const vec &target, float dist, float yaw, float pitch)
{
    vec dir(yaw*RAD, pitch*RAD);
    return vec(target).sub(dir.mul(dist));
}

#ifdef DEBUG_UTILS
// Map editor test harness support, see tools/harness/. Gated like the UI test
// commands in ui.cpp, and refused to map scripts.
//
// The view entity follows the editor's own convention (entautoview above,
// nearestent below) rather than reproducing mousemove's expression, so these
// stay correct alongside the code they sit with.
static physent *editviewent()
{
    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;
    return player;
}

// vectoyawpitch rather than game::getyawpitch: the latter is declared in
// src/game/game.h, which engine code does not include. Same math, and it
// additionally guards the near-zero-length case.
static void edaimat(physent *player, const vec &target)
{
    float yaw = 0, pitch = 0;
    vectoyawpitch(vec(target).sub(player->o), yaw, pitch);
    player->yaw = yaw;
    player->pitch = pitch;
    fixrange(player->yaw, player->pitch);
}

// Returns NULL for an out-of-range index or an ET_EMPTY slot; empty slots are
// recycled placeholders with no meaningful position, so aiming at one would
// silently point at the origin.
static extentity *edliveent(int idx)
{
    const vector<extentity *> &ents = entities::getents();
    if(!ents.inrange(idx)) return NULL;
    extentity *e = ents[idx];
    return e->type != ET_EMPTY ? e : NULL;
}

ICOMMAND(0, edgoto, "fff", (float *x, float *y, float *z),
{
    if(identflags&IDF_MAP) return;
    physent *player = editviewent();
    player->o = vec(*x, *y, *z);
    player->resetinterp(true);
});

ICOMMAND(0, edaim, "ff", (float *yaw, float *pitch),
{
    if(identflags&IDF_MAP) return;
    physent *player = editviewent();
    player->yaw = *yaw;
    player->pitch = *pitch;
    fixrange(player->yaw, player->pitch);
});

ICOMMAND(0, edlookat, "fff", (float *x, float *y, float *z),
{
    if(identflags&IDF_MAP) return;
    edaimat(editviewent(), vec(*x, *y, *z));
});

ICOMMAND(0, edlookatent, "i", (int *idx),
{
    if(identflags&IDF_MAP) { intret(0); return; }
    extentity *e = edliveent(*idx);
    if(!e) { intret(0); return; }
    edaimat(editviewent(), e->o);
    intret(1);
});

// One call fully determines the view, which is what makes a screenshot a pure
// function of its arguments.
ICOMMAND(0, edframe, "ffffff", (float *x, float *y, float *z, float *dist, float *yaw, float *pitch),
{
    if(identflags&IDF_MAP) return;
    vec target(*x, *y, *z);
    physent *player = editviewent();
    player->o = orbitpos(target, *dist, *yaw, *pitch);
    player->resetinterp(true);
    edaimat(player, target);
});

ICOMMAND(0, edframeent, "ifff", (int *idx, float *dist, float *yaw, float *pitch),
{
    if(identflags&IDF_MAP) { intret(0); return; }
    extentity *e = edliveent(*idx);
    if(!e) { intret(0); return; }
    vec target = e->o;
    physent *player = editviewent();
    player->o = orbitpos(target, *dist, *yaw, *pitch);
    player->resetinterp(true);
    edaimat(player, target);
    intret(1);
});

ICOMMAND(0, ednudge, "fff", (float *fwd, float *right, float *up),
{
    if(identflags&IDF_MAP) return;
    physent *player = editviewent();
    vec dir(player->yaw*RAD, player->pitch*RAD);
    // Right is the facing vector rotated -90 degrees about Z, flattened.
    vec side(dir.y, -dir.x, 0);
    if(side.magnitude() > 0) side.normalize();
    player->o.add(vec(dir).mul(*fwd));
    player->o.add(side.mul(*right));
    player->o.z += *up;
    player->resetinterp(true);
});
#endif
```

- [ ] **Step 4: Build and run the unit test**

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

Expected: build succeeds.

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

Then read the boot log, which is where startup output lands (the first batch's `clearlog` wipes `log.txt`):

```bash
findstr testedharness home\uitest\harness\boot-log.txt
```

Expected: `testedharness: ok`. An `ASSERT` failure would instead abort startup, and `harness.ps1 start` would report the game exited.

- [ ] **Step 5: Verify the commands against the running game**

Create `tools/harness/tests/task3-view.cfg`:

```cubescript
// Task 3 verification. Needs a loaded map.
newmap 12
sleep 500 [
    if (! $editing) [ edittoggle ]
    sleep 300 [
        edgoto 512 512 512
        edaim 90 0
        sleep 100 [
            echo (concatword "T3_POS=" $cameraposx " " $cameraposy " " $cameraposz)
            echo (concatword "T3_YAW=" $camerayaw " PITCH=" $camerapitch)
            edframe 512 512 512 100 0 0
            sleep 100 [
                echo (concatword "T3_FRAMEPOS=" $cameraposx " " $cameraposy " " $cameraposz)
                echo (concatword "T3_FRAMEYAW=" $camerayaw)
                echo (concatword "T3_BADENT=" (edlookatent 99999))
            ]
        ]
    ]
]
```

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task3-view.cfg -Settle 2000
```

Expected: `T3_POS=512 512 512`, `T3_YAW=90`, `T3_FRAMEPOS=512 412 512` (100 units on the -Y side of the target), `T3_FRAMEYAW=0` (looking back along +Y), `T3_BADENT=0`.

- [ ] **Step 6: Commit**

```bash
git add src/engine/world.cpp src/engine/engine.h src/tests/edharness.cpp src/Makefile src/engine/main.cpp tools/harness/tests/task3-view.cfg
git commit -m "engine: add editor harness view control commands"
```

---

### Task 4: Direct action wrappers

Coordinate-addressed shortcuts that skip the aim-then-click flow. **These bypass the input path real users take**, so they are for deterministic *setup* only — Task 9's self-test asserts selection through the real path and must not use these as the thing under test.

**Files:**
- Modify: `src/engine/octaedit.cpp` (append after Task 2's block)
- Modify: `src/engine/world.cpp` (append after Task 3's block)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `edselcube <x> <y> <z>` and `edselbox <x> <y> <z> <sx> <sy> <sz>` → `1` if the selection validated, `0` otherwise; `edentnear <x> <y> <z> <radius>` → the entity index added, or `-1`. Used by Tasks 8 and 9.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task4-wrappers.cfg`:

```cubescript
// Task 4 verification.
newmap 12
sleep 500 [
    if (! $editing) [ edittoggle ]
    sleep 300 [
        echo (concatword "T4_CUBE=" (edselcube 512 512 512))
        echo (concatword "T4_HAVESEL=" $hassel)
        echo (concatword "T4_BOX=" (edselbox 512 512 512 2 2 2))
        echo (concatword "T4_OOB=" (edselbox -9999 -9999 -9999 1 1 1))
        eddumpstate
    ]
]
```

- [ ] **Step 2: Run it to verify it fails**

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task4-wrappers.cfg -Settle 1500
```

Expected: `Unknown command: edselcube` and `Unknown command: edselbox`.

- [ ] **Step 3: Write the implementation**

Append to `src/engine/octaedit.cpp`, inside a new `#ifdef DEBUG_UTILS` block after Task 2's:

```cpp
#ifdef DEBUG_UTILS
// Coordinate-addressed selection for the harness. These skip the aim-then-click
// path deliberately, for deterministic setup -- selection itself is tested
// through the real input path, see tools/harness/editor-selftest.ps1.
//
// selinfo::validate() (iengine.h) does the bounds and grid checking, so this
// does not hand-roll it.
static int edsetsel(const ivec &o, const ivec &s)
{
    selinfo n;
    n.grid = gridsize;
    n.orient = orient;
    // Snap the origin down to the grid, or validate() rejects an unaligned box.
    n.o = ivec(o).mask(~(gridsize - 1));
    n.s = s;
    n.cx = n.cy = 0;
    n.cxs = n.s[R[dimension(n.orient)]]*2;
    n.cys = n.s[C[dimension(n.orient)]]*2;
    n.corner = 0;

    if(!n.validate()) { havesel = false; return 0; }
    sel = n;
    havesel = true;
    forcenextundo();
    return 1;
}

ICOMMAND(0, edselcube, "fff", (float *x, float *y, float *z),
{
    if(identflags&IDF_MAP) { intret(0); return; }
    intret(edsetsel(ivec(int(*x), int(*y), int(*z)), ivec(1, 1, 1)));
});

ICOMMAND(0, edselbox, "ffffff", (float *x, float *y, float *z, float *sx, float *sy, float *sz),
{
    if(identflags&IDF_MAP) { intret(0); return; }
    intret(edsetsel(ivec(int(*x), int(*y), int(*z)),
                    ivec(max(int(*sx), 1), max(int(*sy), 1), max(int(*sz), 1))));
});
#endif
```

`ivec::mask(int)` is defined at `src/shared/geom.h:1290`, and `R[]`/`C[]`/`dimension()` are already used by `reorient()` at `src/engine/octaedit.cpp:232`, so everything above is in scope.

Append to `src/engine/world.cpp`, inside Task 3's `#ifdef DEBUG_UTILS` block (before its `#endif`):

```cpp
// Coordinate-based nearestent -- the existing one (below) is relative to the
// player's own position, which the harness would have to move first.
ICOMMAND(0, edentnear, "ffff", (float *x, float *y, float *z, float *radius),
{
    if(identflags&IDF_MAP) { intret(-1); return; }
    if(noentedit()) { intret(-1); return; }
    vec target(*x, *y, *z);
    float best = *radius > 0 ? *radius : 1e16f;
    int closest = -1;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        const extentity &e = *ents[i];
        if(e.type == ET_EMPTY || e.flags&EF_VIRTUAL) continue;
        float dist = e.o.dist(target);
        if(dist < best) { best = dist; closest = i; }
    }
    if(closest >= 0) entadd(closest);
    intret(closest);
});
```

- [ ] **Step 4: Build**

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

Expected: build succeeds.

- [ ] **Step 5: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task4-wrappers.cfg -Settle 1500
```

Expected: `T4_CUBE=1`, `T4_HAVESEL=1`, `T4_BOX=1`, `T4_OOB=0`, and an `EDSTATE sel` line whose last field is `1` and whose `sx sy sz` are `2 2 2`.

- [ ] **Step 6: Commit**

```bash
git add src/engine/octaedit.cpp src/engine/world.cpp tools/harness/tests/task4-wrappers.cfg
git commit -m "engine: add coordinate-addressed editor selection wrappers"
```

---

### Task 5: Extract `tools/harness/core.ps1`

Pure refactor — no behaviour change. `harness.ps1` keeps all 11 subcommands and its exact current interface, including every `-Settle` default (`send` 1, `nav` 400, `shot` 300, `reload` 200, `click` 500).

**Files:**
- Create: `tools/harness/core.ps1`
- Modify: `tools/harness/harness.ps1`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces, for Task 8 to dot-source:
  - `$RepoRoot`, `$Exe`, `$HomeDir`, `$CmdDir`, `$ShotDir`, `$LogFile`, `$PidFile`, `$BootLog`, `$ErrorPattern`, `$Invariant`
  - `Write-TextNoBom([string]$Path, [string]$Text)`
  - `Read-LogSafe()` → `[string]`
  - `Get-HarnessProcess()` → process or `$null`; `Assert-Running()` → process, throws if not
  - `Get-NextSeq()` → `[int]`
  - `Invoke-Batch([string]$Script, [int]$SettleMs, [int]$Timeout)` → `[string[]]`
  - `Show-BatchResult([string[]]$Lines)`
  - `Set-WindowNoActivate([IntPtr]$Handle)`
  - `ConvertTo-InvariantDouble([string]$Value)` → `[double]`; `Format-Coord([double]$Value)` → `[string]`
  - `Invoke-Shot([string]$Name, [int]$SettleMs, [int]$Timeout)` → the PNG path

- [ ] **Step 1: Write the failing test**

The test is that `harness.ps1`'s behaviour is unchanged. Create `tools/harness/tests/task5-smoke.ps1`:

```powershell
# Task 5: harness.ps1's public behaviour must be identical after the extraction.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$harness = Join-Path $root 'tools\harness\harness.ps1'
$failures = 0

function Check([string]$Name, [scriptblock]$Body) {
    try {
        $result = & $Body
        if ($result) { Write-Host "  PASS  $Name" -ForegroundColor Green }
        else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
    }
    catch { Write-Host "  FAIL  $Name -- $_" -ForegroundColor Red; $script:failures++ }
}

Check 'core.ps1 exists' { Test-Path (Join-Path $root 'tools\harness\core.ps1') }
Check 'harness.ps1 dot-sources core.ps1' { (Get-Content -Raw $harness) -match 'core\.ps1' }
Check 'status runs' { (& $harness status) -ne $null }
Check 'send echoes' { (& $harness send 'echo "SMOKE_OK"') -match 'SMOKE_OK' }
Check 'nav opens a panel' {
    & $harness nav ui_gameui_settings_graphics | Out-Null
    (& $harness send 'echo (concatword "TOP=" $uitopname)') -match 'TOP=main'
}
Check 'tree reports objects' { (& $harness tree -Drawn) -match 'aspect' }
Check 'shot writes a png' { Test-Path (& $harness shot task5smoke) }

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
```

- [ ] **Step 2: Run it to verify it fails**

```bash
tools\harness\harness.ps1 start
```

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task5-smoke.ps1
```

Expected: the first two checks FAIL (no `core.ps1`), the rest PASS — establishing the baseline the refactor must preserve.

- [ ] **Step 3: Do the extraction**

Create `tools/harness/core.ps1` containing, moved **verbatim** from `harness.ps1`: the `$ErrorActionPreference` line, the entire `# ---- paths ----` block, `$ErrorPattern`, `Write-TextNoBom`, `Read-LogSafe`, `Get-HarnessProcess`, `Assert-Running`, `Get-NextSeq`, `Invoke-Batch`, `Show-BatchResult`, `$Invariant`, `ConvertTo-InvariantDouble`, `Format-Coord`, and `Set-WindowNoActivate`.

Two adjustments are needed because `$PSScriptRoot` and the parameters no longer come from `harness.ps1`:

```powershell
# core.ps1 sits in the same directory as its callers, so this resolves the same
# way harness.ps1's own $PSScriptRoot did.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
```

and lift the screenshot body out of `harness.ps1`'s `shot` case into a shared function, so `editor.ps1` gets it too:

```powershell
function Invoke-Shot([string]$Name, [int]$SettleMs, [int]$Timeout) {
    if (-not $Name) { $Name = 'shot' }
    if ($Name -notmatch '^[A-Za-z0-9_.-]+$') { throw "Screenshot name must be [A-Za-z0-9_.-]+ (got '$Name')." }

    $target = Join-Path $ShotDir "$Name.png"
    Remove-Item $target -Force -ErrorAction SilentlyContinue

    # The back buffer must hold a rendered frame, so settle before shooting.
    $lines = Invoke-Batch "sleep $SettleMs [ screenshot ""harness/shots/$Name"" ]" ($SettleMs + 300) $Timeout
    Show-BatchResult $lines

    for ($i = 0; $i -lt 20 -and -not (Test-Path $target); $i++) { Start-Sleep -Milliseconds 100 }
    if (-not (Test-Path $target)) { throw "Screenshot was not written: $target" }
    return $target
}
```

In `harness.ps1`, delete those definitions and add immediately after the `param(...)` block:

```powershell
$ErrorActionPreference = 'Stop'

# Shared transport and process plumbing, also used by editor.ps1.
. (Join-Path $PSScriptRoot 'core.ps1')
```

Replace the body of the `shot` case with:

```powershell
        $settleMs = if ($Settle -ge 0) { $Settle } else { 300 }
        Write-Output (Invoke-Shot ($Rest -join '_').Trim() $settleMs $TimeoutSec)
```

Leave `Find-Widget`, `Add-ClickPoint`, `Get-UiTree`, `Invoke-Start`, `Invoke-Stop`, `Invoke-Status` and the `switch` in `harness.ps1` — they are UI-specific.

- [ ] **Step 4: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task5-smoke.ps1
```

Expected: `all passed`, all seven checks green.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/core.ps1 tools/harness/harness.ps1 tools/harness/tests/task5-smoke.ps1
git commit -m "harness: extract shared plumbing into core.ps1"
```

---

### Task 6: `EDSTATE` parser and its unit tests

The parser is the only real logic on the PowerShell side and is a pure function over lines, so it gets proper unit tests with no game running. Pester 3.4.0 is present on this machine but its `Should Be` syntax differs from every modern example, and the repo has no PowerShell test convention — so this uses a self-contained runner with no dependency.

**Files:**
- Create: `tools/harness/edstate.ps1`
- Create: `tools/harness/tests/edstate.tests.ps1`

**Interfaces:**
- Consumes: `ConvertTo-InvariantDouble` from `core.ps1` (Task 5).
- Produces: `ConvertFrom-EdState([string[]]$Lines)` → object with `.Mode`, `.Cam`, `.WorldPos`, `.Cur`, `.Sel`, `.Ui` (hashtables), `.Hover`, `.EntSel` (arrays of hashtables), `.Malformed` (array of raw strings), `.Complete` (bool), `.EntCount` (int). Used by Tasks 8 and 9.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/edstate.tests.ps1`:

```powershell
# Unit tests for the EDSTATE parser. No game required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\edstate.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

$full = @(
    'EDSTATE mode 1 3 8 2'
    'EDSTATE cam 512.00000 400.50000 600.00000 90.00000 -15.25000'
    'EDSTATE worldpos 512.00000 512.00000 512.00000'
    'EDSTATE cur 512 512 512 2'
    'EDSTATE sel 512 512 512 2 3 4 8 2 0 0 4 6 1 24 1'
    'EDSTATE ui 0 1234'
    'EDENT hover 17 playerstart 100.00000 200.00000 300.00000'
    'EDENT sel 17 playerstart 100.00000 200.00000 300.00000'
    'EDENT sel 18 mapmodel 110.00000 210.00000 310.00000'
    'EDSTATE end 3'
)

$r = ConvertFrom-EdState $full
Assert-That 'complete dump is marked complete'   ($r.Complete)
Assert-That 'edit mode parsed'                    ($r.Mode.EditMode -eq 1)
Assert-That 'gridpower parsed'                    ($r.Mode.GridPower -eq 3)
Assert-That 'gridsize parsed'                     ($r.Mode.GridSize -eq 8)
Assert-That 'cam x parsed'                        ($r.Cam.X -eq 512)
Assert-That 'cam negative pitch parsed'           ($r.Cam.Pitch -eq -15.25)
Assert-That 'cur parsed'                          ($r.Cur.X -eq 512 -and $r.Cur.Orient -eq 2)
Assert-That 'sel size parsed'                     ($r.Sel.SX -eq 2 -and $r.Sel.SY -eq 3 -and $r.Sel.SZ -eq 4)
Assert-That 'sel children parsed'                 ($r.Sel.Children -eq 24)
Assert-That 'havesel parsed'                      ($r.Sel.HaveSel -eq 1)
Assert-That 'freecursor parsed'                   ($r.Ui.FreeCursor -eq 1234)
Assert-That 'one hover entity'                    ($r.Hover.Count -eq 1)
Assert-That 'hover type parsed'                   ($r.Hover[0].Type -eq 'playerstart')
Assert-That 'two selected entities'               ($r.EntSel.Count -eq 2)
Assert-That 'second selected idx parsed'          ($r.EntSel[1].Idx -eq 18)
Assert-That 'ent count parsed'                    ($r.EntCount -eq 3)
Assert-That 'nothing malformed'                   ($r.Malformed.Count -eq 0)

# The harness strips log timestamps, but the parser must not depend on that.
$stamped = $full | ForEach-Object { "2026-09-03 12:34.56 $_" }
$s = ConvertFrom-EdState $stamped
Assert-That 'timestamped lines parse'             ($s.Complete -and $s.Mode.EditMode -eq 1)
Assert-That 'timestamped entities parse'          ($s.EntSel.Count -eq 2)

# Empty selection.
$empty = @(
    'EDSTATE mode 1 3 8 0'
    'EDSTATE cam 0.00000 0.00000 0.00000 0.00000 0.00000'
    'EDSTATE worldpos 0.00000 0.00000 0.00000'
    'EDSTATE cur 0 0 0 0'
    'EDSTATE sel 0 0 0 0 0 0 8 0 0 0 0 0 0 0 0'
    'EDSTATE ui 0 0'
    'EDSTATE end 0'
)
$e = ConvertFrom-EdState $empty
Assert-That 'havesel 0 parsed'                    ($e.Sel.HaveSel -eq 0)
Assert-That 'no hover entities'                   ($e.Hover.Count -eq 0)
Assert-That 'no selected entities'                ($e.EntSel.Count -eq 0)
Assert-That 'empty dump still complete'           ($e.Complete)

# A truncated dump must be reported, not silently accepted.
$t = ConvertFrom-EdState @('EDSTATE mode 1 3 8 0')
Assert-That 'truncated dump is incomplete'        (-not $t.Complete)

# Malformed lines are captured, never dropped.
$bad = ConvertFrom-EdState @(
    'EDSTATE mode 1 3 8 0'
    'EDSTATE cur nonsense'
    'EDENT hover 17'
    'EDSTATE end 0'
)
Assert-That 'malformed lines captured'            ($bad.Malformed.Count -eq 2)
Assert-That 'unparsed cur left null'              ($null -eq $bad.Cur)

# Unrelated log output is ignored, not treated as malformed.
$noise = ConvertFrom-EdState @('some other log line', 'EDSTATE mode 1 3 8 0', 'EDSTATE end 0')
Assert-That 'non-EDSTATE lines ignored'           ($noise.Malformed.Count -eq 0)

# Coordinates must parse the same way regardless of the machine's locale.
$culture = [System.Threading.Thread]::CurrentThread.CurrentCulture
try {
    [System.Threading.Thread]::CurrentThread.CurrentCulture = 'de-DE'
    $l = ConvertFrom-EdState $full
    Assert-That 'comma-decimal locale parses correctly' ($l.Cam.Y -eq 400.5)
}
finally { [System.Threading.Thread]::CurrentThread.CurrentCulture = $culture }

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
```

- [ ] **Step 2: Run it to verify it fails**

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\edstate.tests.ps1
```

Expected: fails immediately — `tools/harness/edstate.ps1` does not exist.

- [ ] **Step 3: Write the implementation**

Create `tools/harness/edstate.ps1`:

```powershell
# Parser for the structured output of the engine's eddumpstate command.
# Pure function over lines -- no game required, so it is unit tested directly
# by tests/edstate.tests.ps1.

if (-not (Get-Command ConvertTo-InvariantDouble -ErrorAction SilentlyContinue)) {
    . (Join-Path $PSScriptRoot 'core.ps1')
}

function ConvertFrom-EdState([string[]]$Lines) {
    $result = [pscustomobject]@{
        Mode      = $null
        Cam       = $null
        WorldPos  = $null
        Cur       = $null
        Sel       = $null
        Ui        = $null
        Hover     = @()
        EntSel    = @()
        Malformed = @()
        Complete  = $false
        EntCount  = 0
    }

    $hover  = New-Object System.Collections.ArrayList
    $entsel = New-Object System.Collections.ArrayList

    foreach ($raw in $Lines) {
        if ($null -eq $raw) { continue }
        # Strip the "YYYY-MM-DD HH:MM.SS " stamp the log adds to every line.
        $t = ($raw -replace '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ', '').Trim()

        # Anything that is not one of our tags is ordinary log output.
        if ($t -notmatch '^(EDSTATE|EDENT)\s') { continue }

        $parsed = $true
        switch -Regex ($t) {
            '^EDSTATE mode (-?\d+) (-?\d+) (-?\d+) (-?\d+)$' {
                $result.Mode = @{
                    EditMode  = [int]$Matches[1]
                    GridPower = [int]$Matches[2]
                    GridSize  = [int]$Matches[3]
                    Orient    = [int]$Matches[4]
                }
            }
            '^EDSTATE cam (\S+) (\S+) (\S+) (\S+) (\S+)$' {
                $result.Cam = @{
                    X     = ConvertTo-InvariantDouble $Matches[1]
                    Y     = ConvertTo-InvariantDouble $Matches[2]
                    Z     = ConvertTo-InvariantDouble $Matches[3]
                    Yaw   = ConvertTo-InvariantDouble $Matches[4]
                    Pitch = ConvertTo-InvariantDouble $Matches[5]
                }
            }
            '^EDSTATE worldpos (\S+) (\S+) (\S+)$' {
                $result.WorldPos = @{
                    X = ConvertTo-InvariantDouble $Matches[1]
                    Y = ConvertTo-InvariantDouble $Matches[2]
                    Z = ConvertTo-InvariantDouble $Matches[3]
                }
            }
            '^EDSTATE cur (-?\d+) (-?\d+) (-?\d+) (-?\d+)$' {
                $result.Cur = @{
                    X      = [int]$Matches[1]
                    Y      = [int]$Matches[2]
                    Z      = [int]$Matches[3]
                    Orient = [int]$Matches[4]
                }
            }
            '^EDSTATE sel ((?:-?\d+ ){14})(-?\d+)$' {
                $f = ($Matches[1] + $Matches[2]) -split '\s+' | ForEach-Object { [int]$_ }
                $result.Sel = @{
                    OX = $f[0];  OY = $f[1];  OZ = $f[2]
                    SX = $f[3];  SY = $f[4];  SZ = $f[5]
                    Grid = $f[6]; Orient = $f[7]
                    CX = $f[8];  CY = $f[9];  CXS = $f[10]; CYS = $f[11]
                    Corner = $f[12]; Children = $f[13]; HaveSel = $f[14]
                }
            }
            '^EDSTATE ui (-?\d+) (-?\d+)$' {
                $result.Ui = @{
                    CursorLock  = [int]$Matches[1]
                    FreeCursor  = [int]$Matches[2]
                }
            }
            '^EDENT (hover|sel) (-?\d+) (\S+) (\S+) (\S+) (\S+)$' {
                $ent = @{
                    Idx  = [int]$Matches[2]
                    Type = $Matches[3]
                    X    = ConvertTo-InvariantDouble $Matches[4]
                    Y    = ConvertTo-InvariantDouble $Matches[5]
                    Z    = ConvertTo-InvariantDouble $Matches[6]
                }
                if ($Matches[1] -eq 'hover') { [void]$hover.Add($ent) } else { [void]$entsel.Add($ent) }
            }
            '^EDSTATE end (-?\d+)$' {
                $result.Complete = $true
                $result.EntCount = [int]$Matches[1]
            }
            default { $parsed = $false }
        }

        # A tagged line we could not read is a real problem -- surface it rather
        # than silently returning a half-populated state object.
        if (-not $parsed) { $result.Malformed += $t }
    }

    $result.Hover  = @($hover)
    $result.EntSel = @($entsel)
    return $result
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\edstate.tests.ps1
```

Expected: `all passed`, all 26 checks green.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/edstate.ps1 tools/harness/tests/edstate.tests.ps1
git commit -m "harness: add EDSTATE parser with unit tests"
```

---

### Task 7: `tools/harness/editor.cfg`

The CubeScript layer. Loaded on demand so UI-only sessions stay byte-identical to today. `edh_cursor` deliberately drives the real TAB bind rather than assigning `ui_freecursor` directly — the point is to exercise the path a user takes, including `ta_cursor_mode`'s `getclientstate` guard (`config/tool/tooledit.cfg:467`).

**Files:**
- Create: `tools/harness/editor.cfg`

**Interfaces:**
- Consumes: `gamekeypress` (Task 1), `eddumpstate` (Task 2).
- Produces: aliases `edh_enter`, `edh_leave`, `edh_tap <key>`, `edh_cursor <0|1>`, `edh_dragsel <x1> <y1> <z1> <x2> <y2> <z2>`, `edh_ready`. Used by Task 8.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task7-editorcfg.cfg`:

```cubescript
// Task 7 verification.
newmap 12
sleep 500 [
    edh_enter
    sleep 300 [
        echo (concatword "T7_EDITING=" $editing)
        edh_cursor 1
        sleep 300 [
            echo (concatword "T7_FREE_ON=" (? $ui_freecursor 1 0))
            edh_cursor 1
            sleep 200 [
                echo (concatword "T7_IDEMPOTENT=" (? $ui_freecursor 1 0))
                edh_cursor 0
                sleep 300 [ echo (concatword "T7_FREE_OFF=" (? $ui_freecursor 1 0)) ]
            ]
        ]
    ]
]
```

- [ ] **Step 2: Run it to verify it fails**

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task7-editorcfg.cfg -Settle 2500
```

Expected: `Unknown command: edh_enter` (and the rest).

- [ ] **Step 3: Write the implementation**

Create `tools/harness/editor.cfg`:

```cubescript
// Red Eclipse map editor test harness -- in-game editor operations.
//
// Loaded on demand by tools/harness/editor.ps1, not from boot.cfg, so a
// UI-only harness session is unaffected.
//
// Deliberately contains no '#' and no '@' -- see CLAUDE.md, "CubeScript traps".

// Idempotent, so a driver can call it without first querying the state.
edh_enter = [
    if (! $editing) [ edittoggle ]
]

edh_leave = [
    if $editing [ edittoggle ]
]

// Press and release with a frame in between, so the bind's press action and
// its onrelease both get a chance to run.
edh_tap = [
    gamekeypress $arg1 1
    sleep 50 (concat gamekeypress $arg1 0)
]

// Absolute, not a toggle, so repeated calls are safe. Drives the real TAB
// bind (toolbind TAB ta_cursor_mode, config/setup.cfg:348) rather than
// assigning ui_freecursor, so the user's actual path is what gets tested.
edh_cursor = [
    if (!= (? $ui_freecursor 1 0) (? $arg1 1 0)) [
        edh_tap TAB
    ]
]

// Geometry selection through the real input path: aim at the first corner,
// hold MOUSE1, aim at the second, release. 'drag' registers an onrelease,
// which only works because gamekeypress goes through execbind.
edh_dragsel = [
    // 'concat' bakes the second corner in now; '@' substitution here would be
    // nesting-depth dependent and brittle.
    edlookat $arg1 $arg2 $arg3
    sleep 100 (concat edh_dragsel_finish $arg4 $arg5 $arg6)
]

edh_dragsel_finish = [
    gamekeypress MOUSE1 1
    sleep 100 (concat edh_dragsel_release $arg1 $arg2 $arg3)
]

edh_dragsel_release = [
    edlookat $arg1 $arg2 $arg3
    sleep 100 [ gamekeypress MOUSE1 0 ]
]

edh_ready = 1
echo "EDITOR_HARNESS_READY"
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 send 'exec "tools/harness/editor.cfg" 0 0'
```

Expected: `EDITOR_HARNESS_READY`.

```bash
tools\harness\harness.ps1 send -File tools\harness\tests\task7-editorcfg.cfg -Settle 2500
```

Expected: `T7_EDITING=1`, `T7_FREE_ON=1`, `T7_IDEMPOTENT=1`, `T7_FREE_OFF=0`.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/editor.cfg tools/harness/tests/task7-editorcfg.cfg
git commit -m "harness: add editor.cfg in-game editor operations"
```

---

### Task 8: `tools/harness/editor.ps1`

The driver. Shares the running client, home dir and `cmd_<n>.cfg` sequence with `harness.ps1` — one game instance, one command channel.

**Files:**
- Create: `tools/harness/editor.ps1`

**Interfaces:**
- Consumes: everything from `core.ps1` (Task 5), `ConvertFrom-EdState` (Task 6), the `edh_*` aliases (Task 7), and the engine commands (Tasks 1–4).
- Produces: the `editor.ps1` command-line interface, used by Task 9.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task8-editor.ps1`:

```powershell
# Task 8: editor.ps1 subcommand behaviour.
$ErrorActionPreference = 'Stop'
$root   = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$editor = Join-Path $root 'tools\harness\editor.ps1'
$failures = 0

function Check([string]$Name, [scriptblock]$Body) {
    try {
        if (& $Body) { Write-Host "  PASS  $Name" -ForegroundColor Green }
        else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
    }
    catch { Write-Host "  FAIL  $Name -- $_" -ForegroundColor Red; $script:failures++ }
}

& $editor newmap 12 | Out-Null

Check 'state reports edit mode'  { (& $editor state).Mode.EditMode -eq 1 }
Check 'goto moves the view'      {
    & $editor goto 512 512 512 | Out-Null
    $s = & $editor state
    [Math]::Abs($s.Cam.X - 512) -lt 1
}
Check 'aim sets yaw'             {
    & $editor aim 90 0 | Out-Null
    [Math]::Abs((& $editor state).Cam.Yaw - 90) -lt 1
}
Check 'frame positions and aims' {
    & $editor frame 512 512 512 -Dist 100 -Yaw 0 -Pitch 0 | Out-Null
    $s = & $editor state
    ([Math]::Abs($s.Cam.Y - 412) -lt 1) -and ([Math]::Abs($s.Cam.Yaw) -lt 1)
}
Check 'cursor on then off'       {
    & $editor cursor on | Out-Null
    $on = (& $editor state).Ui.FreeCursor
    & $editor cursor off | Out-Null
    $off = (& $editor state).Ui.FreeCursor
    ($on -ne 0) -and ($off -eq 0)
}
Check 'sel selects a cube'       {
    & $editor sel 512 512 512 | Out-Null
    (& $editor state).Sel.HaveSel -eq 1
}
Check 'key reports unknown keys' { (& $editor key NOTAKEY) -match '0' }
Check 'shot writes a png'        { Test-Path (& $editor shot task8) }

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
```

- [ ] **Step 2: Run it to verify it fails**

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task8-editor.ps1
```

Expected: fails — `editor.ps1` does not exist.

- [ ] **Step 3: Write the implementation**

Create `tools/harness/editor.ps1`:

```powershell
<#
.SYNOPSIS
    Drives the Red Eclipse map editor from outside, for agent/CI editor work.

.DESCRIPTION
    Shares the running client, home dir and command channel with harness.ps1 --
    one game instance. Start the client with 'harness.ps1 start', then use
    'open' or 'newmap' here to get into an editing session.

    Unlike the UI harness, the engine side of this needs a build that has the
    editor test commands compiled in (src/build.sh defines DEBUG_UTILS).

.EXAMPLE
    tools\harness\harness.ps1 start
    tools\harness\editor.ps1 newmap 12
    tools\harness\editor.ps1 frame 512 512 512 -Dist 128 -Yaw 45 -Pitch 20
    tools\harness\editor.ps1 seldrag 512 512 512 544 544 512
    tools\harness\editor.ps1 state
    tools\harness\editor.ps1 shot mysel
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('open', 'newmap', 'state', 'goto', 'aim', 'lookat', 'lookatent',
                 'frame', 'frameent', 'nudge', 'cursor', 'key', 'entsel', 'sel',
                 'seldrag', 'shot')]
    [string]$Command = 'state',

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest,

    [double]$Dist = 128,
    [double]$Yaw = 0,
    [double]$Pitch = 20,
    [double]$Radius = 32,
    [int[]]$Size,
    [int]$Settle = -1,
    [int]$TimeoutSec = 30,
    [switch]$Down,
    [switch]$Up,
    [switch]$Hover,
    [switch]$Raw
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'edstate.ps1')

# ------------------------------------------------------------- helpers ----

function Assert-EditorCfg {
    # editor.cfg is loaded on demand rather than from boot.cfg, so a UI-only
    # session never pays for it. Re-exec is cheap and idempotent.
    Invoke-Batch 'if (! $edh_ready) [ exec "tools/harness/editor.cfg" 0 0 ]' 1 $TimeoutSec | Out-Null
}

function Get-Args([int]$Count, [string]$Usage) {
    $values = @($Rest | Where-Object { $_ -ne '' })
    if ($values.Count -lt $Count) { throw "Usage: $Usage" }
    return $values[0..($Count - 1)] | ForEach-Object { Format-Coord (ConvertTo-InvariantDouble $_) }
}

function Invoke-Editor([string]$Script, [int]$DefaultSettle) {
    Assert-EditorCfg
    $settleMs = if ($Settle -ge 0) { $Settle } else { $DefaultSettle }
    return Invoke-Batch $Script $settleMs $TimeoutSec
}

function Get-EdState {
    $lines = Invoke-Editor 'eddumpstate' 100
    $state = ConvertFrom-EdState $lines
    if (-not $state.Complete) {
        throw "eddumpstate returned an incomplete dump. Raw output:`n$($lines -join "`n")"
    }
    if ($state.Malformed.Count) {
        Write-Warning "Unparsed state lines:`n  $($state.Malformed -join "`n  ")"
    }
    return $state
}

function Show-EdState($State) {
    Write-Host ('mode      editing={0} gridpower={1} gridsize={2} orient={3}' -f `
        $State.Mode.EditMode, $State.Mode.GridPower, $State.Mode.GridSize, $State.Mode.Orient) -ForegroundColor Cyan
    Write-Host ('camera    ({0}, {1}, {2}) yaw={3} pitch={4}' -f `
        (Format-Coord $State.Cam.X), (Format-Coord $State.Cam.Y), (Format-Coord $State.Cam.Z),
        (Format-Coord $State.Cam.Yaw), (Format-Coord $State.Cam.Pitch))
    Write-Host ('worldpos  ({0}, {1}, {2})' -f `
        (Format-Coord $State.WorldPos.X), (Format-Coord $State.WorldPos.Y), (Format-Coord $State.WorldPos.Z))
    Write-Host ('cursor    ({0}, {1}, {2}) orient={3}' -f `
        $State.Cur.X, $State.Cur.Y, $State.Cur.Z, $State.Cur.Orient)
    if ($State.Sel.HaveSel) {
        Write-Host ('selection o=({0}, {1}, {2}) s=({3}, {4}, {5}) grid={6} orient={7} children={8}' -f `
            $State.Sel.OX, $State.Sel.OY, $State.Sel.OZ,
            $State.Sel.SX, $State.Sel.SY, $State.Sel.SZ,
            $State.Sel.Grid, $State.Sel.Orient, $State.Sel.Children) -ForegroundColor Green
    }
    else { Write-Host 'selection none' -ForegroundColor Yellow }
    Write-Host ('ui        cursorlock={0} freecursor={1}' -f $State.Ui.CursorLock, $State.Ui.FreeCursor)
    foreach ($e in $State.Hover)  { Write-Host ('hover     [{0}] {1} ({2}, {3}, {4})' -f $e.Idx, $e.Type, (Format-Coord $e.X), (Format-Coord $e.Y), (Format-Coord $e.Z)) }
    foreach ($e in $State.EntSel) { Write-Host ('entsel    [{0}] {1} ({2}, {3}, {4})' -f $e.Idx, $e.Type, (Format-Coord $e.X), (Format-Coord $e.Y), (Format-Coord $e.Z)) -ForegroundColor Green }
}

# ------------------------------------------------------------ commands ----

switch ($Command) {

    'open' {
        $name = ($Rest -join ' ').Trim()
        if (-not $name) { throw 'Usage: editor.ps1 open <mapname>' }
        # Map load is slow and clears IDF_MAP sleeps; give it room.
        Show-BatchResult (Invoke-Editor "map $name" 4000)
        Show-BatchResult (Invoke-Editor 'edh_enter' 600)
        Show-EdState (Get-EdState)
    }

    'newmap' {
        $size = if ($Rest -and $Rest[0]) { [int]$Rest[0] } else { 12 }
        Show-BatchResult (Invoke-Editor "newmap $size" 3000)
        Show-BatchResult (Invoke-Editor 'edh_enter' 600)
        Show-EdState (Get-EdState)
    }

    'state' {
        $state = Get-EdState
        if ($Raw) { Write-Output $state } else { Show-EdState $state; Write-Output $state }
    }

    'goto' {
        $a = Get-Args 3 'editor.ps1 goto <x> <y> <z>'
        Show-BatchResult (Invoke-Editor "edgoto $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'aim' {
        $a = Get-Args 2 'editor.ps1 aim <yaw> <pitch>'
        Show-BatchResult (Invoke-Editor "edaim $($a[0]) $($a[1])" 150)
    }

    'lookat' {
        $a = Get-Args 3 'editor.ps1 lookat <x> <y> <z>'
        Show-BatchResult (Invoke-Editor "edlookat $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'lookatent' {
        $a = Get-Args 1 'editor.ps1 lookatent <idx>'
        $lines = Invoke-Editor "echo (concatword ""EDRESULT="" (edlookatent $($a[0])))" 150
        Show-BatchResult $lines
        if ($lines -match 'EDRESULT=0') { throw "No live entity at index $($a[0])." }
    }

    'frame' {
        $a = Get-Args 3 'editor.ps1 frame <x> <y> <z> [-Dist n] [-Yaw n] [-Pitch n]'
        $script = 'edframe {0} {1} {2} {3} {4} {5}' -f $a[0], $a[1], $a[2],
            (Format-Coord $Dist), (Format-Coord $Yaw), (Format-Coord $Pitch)
        Show-BatchResult (Invoke-Editor $script 200)
    }

    'frameent' {
        $a = Get-Args 1 'editor.ps1 frameent <idx> [-Dist n] [-Yaw n] [-Pitch n]'
        $script = 'echo (concatword "EDRESULT=" (edframeent {0} {1} {2} {3}))' -f $a[0],
            (Format-Coord $Dist), (Format-Coord $Yaw), (Format-Coord $Pitch)
        $lines = Invoke-Editor $script 200
        Show-BatchResult $lines
        if ($lines -match 'EDRESULT=0') { throw "No live entity at index $($a[0])." }
    }

    'nudge' {
        $a = Get-Args 3 'editor.ps1 nudge <fwd> <right> <up>'
        Show-BatchResult (Invoke-Editor "ednudge $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'cursor' {
        $mode = ($Rest -join '').Trim().ToLower()
        $script = switch ($mode) {
            'on'     { 'edh_cursor 1' }
            'off'    { 'edh_cursor 0' }
            'toggle' { 'edh_tap TAB' }
            default  { throw 'Usage: editor.ps1 cursor <on|off|toggle>' }
        }
        Show-BatchResult (Invoke-Editor $script 500)
    }

    'key' {
        $name = ($Rest -join '').Trim()
        if (-not $name) { throw 'Usage: editor.ps1 key <NAME> [-Down] [-Up]' }
        $script =
            if ($Down -and -not $Up) { "echo (concatword ""EDRESULT="" (gamekeypress $name 1))" }
            elseif ($Up -and -not $Down) { "echo (concatword ""EDRESULT="" (gamekeypress $name 0))" }
            else { "echo (concatword ""EDRESULT="" (gamekeypress $name 1))`nedh_tap $name" }
        Show-BatchResult (Invoke-Editor $script 300)
    }

    'entsel' {
        $a = Get-Args 3 'editor.ps1 entsel <x> <y> <z> [-Radius n] [-Hover]'
        $script =
            if ($Hover) {
                # The path a user takes: aim at it, then add what is hovered.
                "edlookat $($a[0]) $($a[1]) $($a[2])`nsleep 150 [ entadd ]"
            }
            else {
                'echo (concatword "EDRESULT=" (edentnear {0} {1} {2} {3}))' -f `
                    $a[0], $a[1], $a[2], (Format-Coord $Radius)
            }
        Show-BatchResult (Invoke-Editor $script 400)
        Show-EdState (Get-EdState)
    }

    'sel' {
        $a = Get-Args 3 'editor.ps1 sel <x> <y> <z> [-Size sx,sy,sz]'
        $script =
            if ($Size -and $Size.Count -eq 3) {
                "edselbox $($a[0]) $($a[1]) $($a[2]) $($Size[0]) $($Size[1]) $($Size[2])"
            }
            else { "edselcube $($a[0]) $($a[1]) $($a[2])" }
        Show-BatchResult (Invoke-Editor $script 300)
        Show-EdState (Get-EdState)
    }

    'seldrag' {
        $a = Get-Args 6 'editor.ps1 seldrag <x1> <y1> <z1> <x2> <y2> <z2>'
        $script = 'edh_dragsel {0} {1} {2} {3} {4} {5}' -f $a[0], $a[1], $a[2], $a[3], $a[4], $a[5]
        Show-BatchResult (Invoke-Editor $script 900)
        Show-EdState (Get-EdState)
    }

    'shot' {
        Assert-EditorCfg
        $settleMs = if ($Settle -ge 0) { $Settle } else { 300 }
        Write-Output (Invoke-Shot (($Rest -join '_').Trim()) $settleMs $TimeoutSec)
    }
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
tools\harness\harness.ps1 stop
```

```bash
tools\harness\harness.ps1 start
```

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task8-editor.ps1
```

Expected: `all passed`, all eight checks green.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/editor.ps1 tools/harness/tests/task8-editor.ps1
git commit -m "harness: add editor.ps1 map editor driver"
```

---

### Task 9: `editor-selftest.ps1` — end-to-end verification

Covers all four capabilities from the spec through the **real** input path. The direct wrappers from Task 4 are used only for setup, never as the thing under test — otherwise the suite could pass while aim-then-click is broken.

**Files:**
- Create: `tools/harness/editor-selftest.ps1`

**Interfaces:**
- Consumes: `harness.ps1` (start/stop), `editor.ps1` (all subcommands), `core.ps1`, `edstate.ps1`.
- Produces: exit code 0 on success, 1 on any failure.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/editor-selftest.ps1`:

```powershell
<#
.SYNOPSIS
    End-to-end self-test for the map editor harness.

.DESCRIPTION
    Boots a client, drives every editor capability the harness claims to
    support, and asserts the resulting editor state. Selection is exercised
    through the REAL input path (gamekeypress -> execbind -> drag/onrelease);
    the coordinate-addressed wrappers are used only for deterministic setup.

.EXAMPLE
    tools\harness\editor-selftest.ps1
    tools\harness\editor-selftest.ps1 -KeepRunning
#>
[CmdletBinding()]
param(
    [switch]$KeepRunning,
    [string]$Map
)

$ErrorActionPreference = 'Stop'

$harness = Join-Path $PSScriptRoot 'harness.ps1'
$editor  = Join-Path $PSScriptRoot 'editor.ps1'

$script:failures = 0
$script:step = 0

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try {
        & $Body
    }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ($Actual -eq $Expected) { Write-Host "     ok    $What = $Actual" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

function ExpectNear([string]$What, [double]$Actual, [double]$Expected, [double]$Tolerance = 2.0) {
    if ([Math]::Abs($Actual - $Expected) -le $Tolerance) {
        Write-Host ("     ok    {0} = {1} (~{2})" -f $What, $Actual, $Expected) -ForegroundColor Green
    }
    else {
        Write-Host ("     FAIL  {0} -- expected ~{1} (+/-{2}), got {3}" -f $What, $Expected, $Tolerance, $Actual) -ForegroundColor Red
        $script:failures++
    }
}

function ExpectTrue([string]$What, [bool]$Condition) {
    if ($Condition) { Write-Host "     ok    $What" -ForegroundColor Green }
    else { Write-Host "     FAIL  $What" -ForegroundColor Red; $script:failures++ }
}

# --------------------------------------------------------------------------

& $harness stop | Out-Null
& $harness start | Out-Null

Step 'Enter an editing session' {
    if ($Map) { & $editor open $Map | Out-Null } else { & $editor newmap 12 | Out-Null }
    $s = & $editor state -Raw
    Expect 'edit mode' $s.Mode.EditMode 1
    ExpectTrue 'grid size is positive' ($s.Mode.GridSize -gt 0)
}

Step 'Viewport: frame a known point' {
    & $editor frame 512 512 512 -Dist 100 -Yaw 0 -Pitch 0 | Out-Null
    $s = & $editor state -Raw
    # Yaw 0 puts the camera on the -Y side, looking back along +Y.
    ExpectNear 'camera x' $s.Cam.X 512
    ExpectNear 'camera y' $s.Cam.Y 412
    ExpectNear 'camera z' $s.Cam.Z 512
    ExpectNear 'camera yaw' $s.Cam.Yaw 0
}

Step 'Viewport: goto and aim are absolute' {
    & $editor goto 600 600 600 | Out-Null
    & $editor aim 45 -30 | Out-Null
    $s = & $editor state -Raw
    ExpectNear 'camera x' $s.Cam.X 600
    ExpectNear 'camera yaw' $s.Cam.Yaw 45
    ExpectNear 'camera pitch' $s.Cam.Pitch -30
}

Step 'Viewport: nudge is relative' {
    & $editor goto 512 512 512 | Out-Null
    & $editor aim 0 0 | Out-Null
    & $editor nudge 0 0 64 | Out-Null
    $s = & $editor state -Raw
    ExpectNear 'camera z rose by 64' $s.Cam.Z 576
}

Step 'Cursor lock via the real TAB bind' {
    & $editor cursor on | Out-Null
    $on = (& $editor state -Raw).Ui.FreeCursor
    ExpectTrue 'freecursor set after TAB' ($on -ne 0)

    # Absolute, so a second 'on' must not toggle it back off.
    & $editor cursor on | Out-Null
    ExpectTrue 'cursor on is idempotent' ((& $editor state -Raw).Ui.FreeCursor -ne 0)

    & $editor cursor off | Out-Null
    Expect 'freecursor cleared' (& $editor state -Raw).Ui.FreeCursor 0
}

Step 'Geometry selection through the real MOUSE1 path' {
    & $editor goto 512 400 560 | Out-Null
    & $editor seldrag 512 512 512 544 544 512 | Out-Null
    $s = & $editor state -Raw
    ExpectTrue 'a selection exists' ($s.Sel.HaveSel -eq 1)
    ExpectTrue 'selection covers more than one cube' ($s.Sel.SX -ge 1 -and $s.Sel.SY -ge 1)
    ExpectTrue 'selection has children' ($s.Sel.Children -ge 1)
}

Step 'Entity selection through the real hover path' {
    # Setup only: place an entity at a known point.
    & $harness send 'edgoto 512 512 520' | Out-Null
    & $harness send 'newent playerstart' | Out-Null

    & $editor frame 512 512 512 -Dist 64 -Yaw 0 -Pitch 0 | Out-Null
    $s = & $editor state -Raw
    ExpectTrue 'an entity is hovered' ($s.Hover.Count -ge 1)

    if ($s.Hover.Count -ge 1) {
        $idx = $s.Hover[0].Idx
        & $editor lookatent $idx | Out-Null
        & $harness send 'entadd' | Out-Null
        $after = & $editor state -Raw
        ExpectTrue 'the entity is selected' (@($after.EntSel | Where-Object { $_.Idx -eq $idx }).Count -eq 1)

        & $editor frameent $idx -Dist 96 -Yaw 30 -Pitch 25 | Out-Null
        ExpectTrue 'frameent moved the camera' ((& $editor state -Raw).Cam.Z -ne $s.Cam.Z)
    }
}

Step 'Screenshot' {
    $png = & $editor shot editor-selftest
    ExpectTrue "screenshot written to $png" (Test-Path $png)
}

# --------------------------------------------------------------------------

Write-Host ''
if (-not $KeepRunning) { & $harness stop | Out-Null }

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'editor self-test: all checks passed' -ForegroundColor Green
exit 0
```

- [ ] **Step 2: Run it**

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1 -KeepRunning
```

Expected on a correct implementation: every step green, `all checks passed`.

- [ ] **Step 3: Fix what the self-test finds**

Real failures are likely in two places, and both are timing rather than logic:

- **`seldrag` selects nothing.** The camera must be positioned so the crosshair actually strikes world geometry at both corners — `worldpos` in `state` shows where the ray lands. Adjust the camera position in the step, or raise `edh_dragsel`'s inter-step `sleep` (100ms) if the drag registers before the aim has taken effect for a frame.
- **`entadd` finds nothing hovered.** `enthover` is recomputed during the editor cursor update, so a frame must render between the aim and the read. Raise the `-Settle` on the `frame` call.

Re-run until green. Do not weaken an assertion to make it pass; if an expectation is genuinely wrong, correct the expectation and say why in the commit message.

- [ ] **Step 4: Verify the full suite together**

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\edstate.tests.ps1
```

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task5-smoke.ps1
```

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1
```

Expected: all three report all-passed, and the last exits 0.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/editor-selftest.ps1
git commit -m "harness: add editor end-to-end self-test"
```

---

### Task 10: Documentation

**Files:**
- Modify: `tools/harness/README.md`
- Modify: `CLAUDE.md` (gitignored — update it, but it will not appear in `git status`)

**Interfaces:**
- Consumes: the finished behaviour of every earlier task.
- Produces: nothing code-facing.

- [ ] **Step 1: Add the editor section to the harness README**

Append to `tools/harness/README.md`:

````markdown
## Map editor harness

`editor.ps1` drives the map editor from the same running client as `harness.ps1`
— one game instance, one command channel. Start the client first.

```powershell
tools\harness\harness.ps1 start
tools\harness\editor.ps1 newmap 12                    # or: open atop
tools\harness\editor.ps1 frame 512 512 512 -Dist 128 -Yaw 45 -Pitch 20
tools\harness\editor.ps1 state                        # camera, cursor, selection, entities
tools\harness\editor.ps1 seldrag 512 512 512 544 544 512
tools\harness\editor.ps1 cursor on                    # TAB, for UI interaction
tools\harness\editor.ps1 shot mysel
tools\harness\harness.ps1 stop
```

| Command | Meaning |
|---|---|
| `open <map>` / `newmap [size]` | enter an editing session |
| `state` | parsed editor state; the thing to read after every action |
| `goto` / `aim` / `lookat` / `lookatent` | absolute view placement |
| `frame` / `frameent` | place **and** aim in one call — reproducible screenshots |
| `nudge <fwd> <right> <up>` | move relative to facing |
| `cursor <on\|off\|toggle>` | drives the real TAB bind |
| `key <NAME> [-Down] [-Up]` | synthetic input through the full bind path |
| `entsel <x> <y> <z> [-Radius] [-Hover]` | select an entity |
| `sel <x> <y> <z> [-Size sx,sy,sz]` | select geometry by coordinates |
| `seldrag <x1 y1 z1> <x2 y2 z2>` | select geometry through the real MOUSE1 drag |
| `shot <name>` | screenshot |

**This half needs a rebuild.** Unlike the UI harness, `editor.ps1` depends on
engine commands (`edgoto`, `eddumpstate`, `gamekeypress`, …) compiled in behind
`DEBUG_UTILS`. Changing them means:

```bash
wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug
```

`editor.cfg` (the CubeScript layer) still hot-reloads like any other `.cfg`.

### Two paths, deliberately

`seldrag` and `entsel -Hover` go through the **real** input path — synthetic key
events reach `processkey` → `execbind`, which is what makes `onrelease` fire, and
without that `drag` cannot work at all. `sel` and `entsel` (without `-Hover`) are
coordinate-addressed shortcuts that skip that path.

Use the shortcuts for **setup**; assert through the real path. A test suite built
only on the shortcuts can pass while the path users actually take is broken.

### Tests

```powershell
powershell -File tools\harness\tests\edstate.tests.ps1   # parser units, no game
powershell -File tools\harness\tests\task5-smoke.ps1     # UI harness unchanged
powershell -File tools\harness\editor-selftest.ps1       # end to end
```
````

- [ ] **Step 2: Update `CLAUDE.md`**

In the "Test-only commands (`src/engine/ui.cpp`, `#ifdef DEBUG_UTILS`)" section, change the heading to cover the new files and add the editor commands to the table:

```markdown
### Test-only commands (`#ifdef DEBUG_UTILS`)

Gated like `writetofile` in `main.cpp`, and refused when `identflags&IDF_MAP` so a downloaded map
cannot synthesise input. `build.sh` defines `DEBUG_UTILS` for both `debug` and `release`.

UI (`src/engine/ui.cpp`):

| Command | Meaning |
|---|---|
| `uisetcursor <x> <y>` | Place the cursor, clamped to 0..1. |
| `$uicursorrawx` / `$uicursorrawy` | Cursor in the same space `uisetcursor` takes (`$uicursorx` is aspect-scaled). |
| `uikeypress <code> <down>` | Synthetic input via `UI::keypress`; returns whether the UI consumed it. `-1` press, `-2` escape, `-3` alt, `-4`/`-5` scroll, else an SDL keycode. |
| `uidumptree [surface]` | One `UITREE <depth> <drawn> <type> <x> <y> <w> <h> <tag> <text>` line per object, rects absolute. |

Map editor (`src/engine/world.cpp`, `src/engine/octaedit.cpp`, `src/engine/console.cpp`):

| Command | Meaning |
|---|---|
| `edgoto <x> <y> <z>` | Place the view. |
| `edaim <yaw> <pitch>` | Absolute view angles. |
| `edlookat <x> <y> <z>` / `edlookatent <idx>` | Aim at a point or an entity; `edlookatent` returns 0 for a dead index. |
| `edframe <x> <y> <z> <dist> <yaw> <pitch>` / `edframeent <idx> <dist> <yaw> <pitch>` | Place **and** aim in one call, so a screenshot is a pure function of the arguments. |
| `ednudge <fwd> <right> <up>` | Move relative to facing. |
| `eddumpstate` | `EDSTATE` / `EDENT` lines: mode, camera, worldpos, cursor, selection, cursor lock, hovered and selected entities. The verification surface. |
| `edselcube <x> <y> <z>` / `edselbox <x> <y> <z> <sx> <sy> <sz>` | Coordinate-addressed geometry selection; returns 0 if `selinfo::validate()` rejects it. |
| `edentnear <x> <y> <z> <radius>` | Coordinate-based `nearestent`; returns the index added, or -1. |
| `gamekeypress <name\|code> <down>` | Synthetic input through the **full** dispatch chain (`processkey` → `execbind`). Unlike `uikeypress` this sets `keypressed`, which is what makes `onrelease` — and therefore `drag`/`moving` — work at all. |

**Editing the engine side needs a rebuild**; the `.cfg` and `.ps1` layers do not.
`tools/harness/editor.ps1` drives all of the above; see `tools/harness/README.md`.
```

Also add a line to the "UI test harness (agent-driven)" section pointing at the editor harness:

```markdown
For the **map editor**, see `tools/harness/editor.ps1` (same client, same channel) — it needs
the editor test commands compiled in, so engine-side changes there do require a rebuild.
```

- [ ] **Step 3: Verify the documented commands actually work**

Run each command in the README table once, exactly as written, against a running client. Any that errors is a documentation bug — fix the doc or the code.

```bash
powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1
```

Expected: exit 0.

- [ ] **Step 4: Commit**

`CLAUDE.md` is gitignored, so only the README is staged.

```bash
git add tools/harness/README.md
git commit -m "harness: document the map editor harness"
```

---

## Self-Review

**Spec coverage** — every section maps to a task:

| Spec section | Task |
|---|---|
| View control (`edgoto`…`ednudge`, incl. `edlookatent`/`edframeent`) | 3 |
| State readback (`eddumpstate`) | 2 |
| Direct action wrappers (`edselcube`/`edselbox`/`edentnear`) | 4 |
| Key synthesis (`gamekeypress`) | 1 |
| `tools/harness/editor.cfg` | 7 |
| `tools/harness/core.ps1` extraction | 5 |
| `tools/harness/editor.ps1` | 8 |
| Unit tests (`EDSTATE` parser, invariant culture) | 6 |
| Integration (`editor-selftest.ps1`, all 7 spec steps) | 9 |
| Documentation (README + `CLAUDE.md`) | 10 |
| Risk: wrappers masking a broken real path | 4 (stated), 9 (enforced), 10 (documented) |

**Additions beyond the spec**, both minor and justified: `orbitpos` was pulled out as a named non-static helper so the one piece of pure math gets a real C++ unit test via the repo's existing `src/tests/` convention (`src/Makefile:350`); and `Invoke-Shot` was lifted into `core.ps1` so both drivers share one screenshot implementation rather than duplicating it.

**Type consistency** — checked across tasks: `ConvertFrom-EdState` field names used in Tasks 8 and 9 (`.Mode.EditMode`, `.Mode.GridSize`, `.Cam.X/Y/Z/Yaw/Pitch`, `.WorldPos.*`, `.Cur.X/Y/Z/Orient`, `.Sel.OX…HaveSel`, `.Ui.CursorLock/FreeCursor`, `.Hover[].Idx/Type/X/Y/Z`, `.EntSel[]`, `.Complete`, `.Malformed`, `.EntCount`) all match Task 6's definitions. Engine command names and arities are consistent between the C++ in Tasks 1–4, the CubeScript in Task 7, the driver in Task 8, and the docs in Task 10. `orbitpos`'s signature matches between `engine.h`, `world.cpp` and `src/tests/edharness.cpp`.

**Ordering note:** Tasks 1–4 all stay on `build.sh debug`, so no `make clean` thrash. Task 5 onward touches no C++.
