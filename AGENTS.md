# Red Eclipse — Agent Guide

Open-source arena shooter. C++ / SDL2 / OpenGL, on an expanded Tesseract (Cube 2) engine.
Part of game logic and **all UI** are written in **CubeScript** (`config/**/*.cfg`), not C++.

This file is the agent-agnostic guide. Machine- or tool-specific setup (how to build on a given
box, local preferences) belongs in the agent's own local file (`CLAUDE.md`, `GEMINI.md`,
`.github/copilot-instructions.md`), which are gitignored.

See also: [doc/cubescript-reference.md](doc/cubescript-reference.md), [doc/engine-systems.md](doc/engine-systems.md).

**Picking up work? Start with [doc/agent-handoff.md](doc/agent-handoff.md)** — repo map, current
branch/commit state, how to build and verify, the open work queue, and where the decision trail lives.
Open bugs and hard-won engine facts: [doc/map-editor-harness-findings.md](doc/map-editor-harness-findings.md).

## Running

- The Windows binary is `bin/amd64/redeclipse.exe`; its DLLs live beside it.
- Working directory must be the repo root (or a descendant; `setlocations` walks up 4 levels looking for `config/version.cfg`).
- The test-only commands below exist only in a build compiled with `-DDEBUG_UTILS`.

## UI architecture

**Editing UI does not require a rebuild.** The UI is CubeScript; re-`exec` the file and the change is live.

- `src/engine/ui.cpp` (~8k lines) exposes ~150 `ui*` commands. Layout/widgets are declared in `config/ui/**`.
- There is **one** foreground window named `main` (`config/ui/game/common.cfg`) that hosts *panels*.
  Navigation is **not** `showui <screen>` — it is:

  ```cubescript
  gameui_open ui_gameui_settings_graphics
  ```

  Panel names: `ui_gameui_{main,player,maps,online,settings,support,editor,team,vote,variables}` plus
  sub-panels like `ui_gameui_settings_{graphics,sound,controls,game,interface}`,
  `ui_gameui_support_{about,accounts,modes,scoring,system,weapons}`.
- Standalone windows that *are* `showui`-able: `main`, `main_overlay`, `console`, `scoreboard`, `tooltip`.
- Introspection vars: `$uitopname`, `$uiname`, `$uicursorx`, `$uicursory`, `$uisurfacetype`, `$uiaspect`,
  `$uitype`, `$uitagid`, `$uicurtag`. Commands: `uitest`, `uitopwindow`, `hideui`, `hideallui`.

## UI test harness (agent-driven)

`tools/harness/` drives a running game from outside: send CubeScript, read output, take screenshots,
hot-reload `.cfg` edits. See [tools/harness/README.md](tools/harness/README.md).

```powershell
tools\harness\harness.ps1 start
tools\harness\harness.ps1 nav ui_gameui_settings_graphics
tools\harness\harness.ps1 shot graphics          # -> PNG path; read it
tools\harness\harness.ps1 reload config/ui/game/settings.cfg
tools\harness\harness.ps1 tree -Drawn -Text      # widgets with rects + click points
tools\harness\harness.ps1 click Back             # click by visible label
tools\harness\harness.ps1 stop
```

Loop for UI work: edit `.cfg` → `reload` → `nav` → `shot` → look at the PNG. No rebuild, no restart.

For the **map editor**, see `tools/harness/editor.ps1` (same client, same channel) — it needs
the editor test commands compiled in, so engine-side changes there do require a rebuild.

### Verified engine behaviour

| Behaviour | Detail |
|---|---|
| `-h<dir>` | Home dir (config, `log.txt`, screenshots). `fixdir` appends the separator, so no trailing slash needed. |
| `-x<script>` | Runs a CubeScript string after init. Quote it — it contains spaces. |
| `-dw<n> -dh<n> -df0` | Windowed at a fixed size. Keep the harness windowed. |
| `log.txt` | `writelog` calls `fflush` on **every** line (`src/engine/server.cpp:300`), so the file is always current and safe to tail live. |
| Reading the log | The game keeps it open for writing, so a reader **must** share read-write. `[System.IO.File]::ReadAllText` uses `FileShare::Read` and fails with a sharing violation; `Get-Content` / an explicit `FileShare::ReadWrite` stream work. |
| Arguments with spaces | The usual checkout path ("Red Eclipse") contains a space. `Start-Process -ArgumentList` joins with spaces and quotes nothing, so `-h`/`-x` must each be passed as one already-quoted token or the game silently gets a truncated path. |
| `screenshot <name>` | `glReadPixels` of the **back buffer** → needs a rendered frame to have happened; settle ~300ms after a UI change before shooting. |
| `renderunfocused 1` | Required, or rendering stops when the window loses focus. |
| Unfocused window | Renders and screenshots correctly. The harness deliberately runs the window unfocused. |
| **Minimized window** | Screenshots come back **solid black**. Never minimize; use `SW_SHOWNOACTIVATE`. |
| Script errors | Surface in the log as `Unknown command: X` / `Unknown alias lookup: X` plus a call stack. |
| `sleep` loop | Survives map load — `clearsleep(bool clearmapdefs = true)` clears only `IDF_MAP` sleeps. |
| `RE_CRASHLOG=1` | Set by `harness.ps1 start` for the game process only (the caller's value is restored after launch). In any build, opts the engine into `src/engine/main.cpp`'s `installcrashlog`/`crashlogabort`/`crashlogexception`/`crashlogbacktrace`/`crashlogminidump`: suppresses the modal assert dialog, the Windows fault box and `fatal()`'s dialog (all block the process, so without this a crash reaches the harness only as a timeout); for a failed assert **or** a fault (access violation etc.) writes a header line and a symbolised backtrace to `log.txt` (one frame per line; a fault walks the faulting context) and a minidump to `<home>/redeclipse-crash.dmp`, then exits. A crash mid-batch surfaces immediately as `Game exited while running batch N. Last log: ...`. Without the variable: a **release** build behaves as before the harness (a fault walks the stack, `fatal()` logs it and shows a dialog — the players' crash report); a **`_DEBUG`** build keeps the assert dialog and leaves a fault unhandled so an attached debugger breaks at it. |

### Shader equivalence harness

`tools/harness/shaders.ps1 record|check|diff` records every shader configuration (composed
GLSL, metadata, GL reflection) across a settings sweep and compares a candidate build against
a baseline: contract → text → SPIR-V → pixel. Engine side: `shaderdumpall`, `shaderbench`,
`shaderorigin`, `shaderforceall` in `src/engine/shaderharness.cpp` / `shader.cpp`
(`DEBUG_UTILS`). See `tools/harness/README.md`.

### Test-only commands (`#ifdef DEBUG_UTILS`)

Gated like `writetofile` in `main.cpp`, and refused when `identflags&IDF_MAP` so a downloaded map
cannot synthesise input. Release builds may define `DEBUG_UTILS` too, so treat these as reachable
in a release binary.

UI (`src/engine/ui.cpp`):

| Command | Meaning |
|---|---|
| `uisetcursor <x> <y>` | Place the cursor, clamped to 0..1. |
| `$uicursorrawx` / `$uicursorrawy` | Cursor in the same space `uisetcursor` takes (`$uicursorx` is aspect-scaled). |
| `uikeypress <code> <down>` | Synthetic input via `UI::keypress`; returns whether the UI consumed it. `-1` press, `-2` escape, `-3` alt, `-4`/`-5` scroll, else an SDL keycode. |
| `uidumptree [surface]` | One `UITREE <depth> <drawn> <type> <x> <y> <w> <h> <tag> <text>` line per object, rects absolute. |

**Coordinate spaces do not match, and this is the thing to get right.** The cursor is a screen
fraction (x runs 0..1 across the full width), but `uidumptree` rects have **x in aspect space**
(0..`hudw/hudh`, e.g. 0..1.7778 at 16:9); y is 0..1 in both. To click an object:

```
cursor_x = (obj_x + obj_w/2) / $uiaspect
cursor_y =  obj_y + obj_h/2
```

Verified by hovering a known button and confirming it highlights, then clicking it and confirming
the panel changed. `harness.ps1 find` / `click` do this conversion for you.

Map editor (`src/engine/world.cpp`, `src/engine/octaedit.cpp`, `src/engine/console.cpp`):

| Command | Meaning |
|---|---|
| `edgoto <x> <y> <z>` | Place the view. Edit mode only. |
| `edaim <yaw> <pitch>` | Absolute view angles. Edit mode only. |
| `edlookat <x> <y> <z>` / `edlookatent <idx>` | Aim at a point or an entity; `edlookatent` returns 0 for a dead index. Edit mode only (`edlookatent` returns 0 outside it). |
| `edframe <x> <y> <z> <dist> <yaw> <pitch>` / `edframeent <idx> <dist> <yaw> <pitch>` | Place **and** aim in one call, so a screenshot is a pure function of the arguments. Edit mode only (`edframeent` returns 0 outside it). |
| `ednudge <fwd> <right> <up>` | Move relative to facing. Edit mode only. |
| `eddumpstate` | `EDSTATE` / `EDENT` lines: mode, camera, worldpos, cursor, selection, cursor lock, hovered and selected entities. The verification surface. `Ui.FreeCursor`'s "on" value is a `$clockmillis` timestamp, not 1 (`ta_cursor_mode`, `config/tool/tooledit.cfg:473`); `Sel.Children` (`selchildcount`) is negative (`-lusize/gridsize`) when the selection sits inside one large octree leaf — normal for a small selection in open space. |
| `edselcube <x> <y> <z>` / `edselbox <x> <y> <z> <sx> <sy> <sz>` | Coordinate-addressed geometry selection; returns 0 if `selinfo::validate()` rejects it. |
| `edentnear <x> <y> <z> <radius>` | Coordinate-based `nearestent`; returns the index added, or -1. Refused (-1) by `noentedit()` outside edit mode. |
| `gamekeypress <name\|code> <down>` | Synthetic input through the **full** dispatch chain (`processkey` → `execbind`). Unlike `uikeypress` this sets `keypressed`, which is what makes `onrelease` — and therefore `drag`/`moving` — work at all. Key names match case-insensitively; returns 0 for an unknown name. |
| `edzoom <fov>` | Overrides `curfov` in edit mode (a stand-in for a weapon zoom); 0 is off. `game.cpp`. |
| `rhprobe <points> <out>` | Runs the real `getrhlight` (the `DL_RHPROBE` main of `deferredlight.frag`) at the world points in a file and writes the values plus each radiance hints split's placement. Returns the point count. `renderlights.cpp`; driven by `tools/harness/gi.ps1`. |
| `edfillsel <solid>` | Fills (1) or empties (0) the current selection — deterministic test geometry. Local only. `octaedit.cpp`. |
| `geotemplateinfo <id>` | `capmin capmax verts tris instances rebuilds` of a geometry template, or empty. `geomtemplate.cpp`. |
| `geoinstancebb <idx>` | World bounds of a geoinstance entity, or empty without a template. `geomtemplate.cpp`. |
| `geoinststats` | `instances triangles` drawn by the last G-buffer pass. `renderva.cpp`. |
| `edraycast <ox> <oy> <oz> <dx> <dy> <dz>` | Hit distance through `raycube` (world, mapmodels, geometry instances), or -1. `physics.cpp`. |
| `edcollide <x> <y> <z> <type> <radius> <halfheight>` | Whether a stationary probe centred there collides with the world (`type` 1 ellipsoid, 2 oriented box); `physics.cpp`. |
| `edgenshadowmeshes` | Regenerates the cached point-light shadow meshes now and returns how many exist. On master `allchanged()` discards the meshes right after building them at load, so the cached-mesh path (and instance baking into it) is dormant in normal play; this makes it reachable. `renderva.cpp`. |
| `edshadowmeshcount` | How many shadow meshes exist now (`smmesh 0` clears them). Lets a test assert it is on the cached path or the live one. `renderva.cpp`. |

The view commands (`edgoto` … `ednudge`) require edit mode because a release build can define
`DEBUG_UTILS` too — ungated, they would be scriptable teleport and aim in live multiplayer.

**Editing the engine side needs a rebuild**; the `.cfg` and `.ps1` layers do not.
`tools/harness/editor.ps1` drives all of the above; see `tools/harness/README.md`.

The bare engine `newmap` cannot enter edit mode from a cold start — `emptymap()` refuses unless
edit mode is already on, `toggleedit()` refuses unless `connected(false)` and `allowedittoggle`
(`octaedit.cpp:198`), and boot-time `localconnect` is a no-op. The real entry point is the
`edit <name>` alias (`config/setup.cfg`), which sets `G_EDITING` and force-local-connects first;
`editor.ps1 open` uses it directly, `editor.ps1 newmap` uses it against a scratch map name. A
fresh `newmap 12` world is 4096 units and centres on **(2048, 2048)**, not 512.

**Known engine bug, unfixed:** `entcancel()` (`world.cpp:389`) clears `entgroup` but not
`enthover`. A map load runs `resetmap()` → `cancelsel()`, leaving stale `enthover` indices;
`enthoverloopread` (`world.cpp:1426`) then does an unguarded `ents[enthover[i]]` and asserts at
`shared/tools.h:904` (in release builds `ASSERT` compiles out, `shared/tools.h:28`, so there it
is a silent out-of-bounds read instead). Triggered by any map load while an entity is hovered in the editor —
reachable by a normal player, not harness-specific. `editor-selftest.ps1` works around it with
`entediting 0` / `entediting 1` around map loads; see `tools/harness/README.md`.

## CubeScript traps

These cost real debugging time. Read before writing or generating CubeScript.

1. **`#` is a macro preprocessor**, custom-defined in `config/stdlib.cfg`. Used as a command prefix, it
   rewrites `#` into `@` according to the current nesting level (`#2` sets a level offset). Most UI panels
   are declared with it (e.g. `# ui_gameui_settings_graphics = [`).
   **Never emit a bare `#` in generated script** unless you mean the preprocessor.

2. **`@` is substitution inside `[...]` blocks**, and nesting depth decides how many you need (`@`, `@@`, …).
   Avoid `@` in harness scripts; use `concat` / `concatword` to bake a value in at definition time:
   ```cubescript
   sleep 50 (concat harnesspoll $harnessgen)    // good
   sleep 50 [harnesspoll @@harnessgen]          // fragile: depth-dependent
   ```

3. **`exists` does not search the home dir.** It calls `fileexists` on the raw path (relative to cwd).
   Only `findfile`-based commands (`exec`, `loadfile`, `screenshot`, …) search home-then-packages.

4. **`exec` needs three args to stay quiet.** Signature is `exec <file> <flags> <msg>`; `msg` is a `'b'`
   arg whose default is `INT_MIN` — *truthy*. `exec f 0` logs "Could not read" on every miss.
   Always write `exec "path" 0 0`.

5. **`onevent` takes an alias name, not a block.** Its `"ir"` signature rejects anything that is not
   `ID_ALIAS`, **silently**. `onevent 0 [ ... ]` does nothing; define an alias and pass its name.

6. **`clearlog` ignores `-g`.** It reopens the hardcoded `LOG_FILE` ("log.txt"), so after the first
   `clearlog` the output silently migrates from your custom log to `log.txt`. Don't combine them —
   the harness stays on the default `log.txt` inside its own `-h` home.

7. **`clearlog` truncates** (`fopen(..., "w")`), which is what makes per-batch log isolation work,
   but it also wipes boot diagnostics. Snapshot the log after startup if you need them.

## Conventions

- `home/` and `cache/` are gitignored; the harness keeps its state in `home/uitest/`.
- Commit messages are lowercase `area: summary` (`ui: add composite debugging`, `server: fix checktrigid properly`).
- Documentation lives in `doc/`; designs and plans go in `doc/superpowers/{specs,plans}/`.
