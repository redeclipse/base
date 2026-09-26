# UI test harness

Drives a running Red Eclipse client from outside so an agent (or a person) can
work on the UI without rebuilding: send CubeScript, read the output, hot-reload
edited `.cfg` files, and take screenshots.

The whole UI is CubeScript (`config/ui/**`), so **editing UI needs no compile**.
The loop is: edit `.cfg` → `reload` → `nav` → `shot` → look at the PNG.

## Requirements

- A built client at `bin/amd64/redeclipse.exe` (`src/build.sh` under WSL).
  `src/redeclipse_windows_amd64.exe` is the same build but cannot resolve its DLLs — don't use it.
- Windows PowerShell 5.1 or later.

## Usage

```powershell
tools\harness\harness.ps1 start                                  # launch, unfocused
tools\harness\harness.ps1 nav ui_gameui_settings_graphics         # open a panel
tools\harness\harness.ps1 shot graphics                           # -> prints the PNG path
tools\harness\harness.ps1 reload config/ui/game/settings.cfg      # apply a .cfg edit live
tools\harness\harness.ps1 send 'echo "top=" $uitopname'           # arbitrary CubeScript
tools\harness\harness.ps1 send -File batch.cfg                    # multi-line script
tools\harness\harness.ps1 tree -Drawn -Text                       # widget tree + click points
tools\harness\harness.ps1 find Apply                              # locate a widget by label
tools\harness\harness.ps1 click Back                              # click it
tools\harness\harness.ps1 log                                     # current log contents
tools\harness\harness.ps1 status
tools\harness\harness.ps1 stop
```

| Option | Applies to | Default | Meaning |
|---|---|---|---|
| `-Width` / `-Height` | `start` | 1280 / 720 | Window size. |
| `-Focus` | `start` | off | Leave the window focused instead of `SW_SHOWNOACTIVATE`. |
| `-Settle <ms>` | `send`/`nav`/`shot`/`reload`/`click` | 1 / 400 / 300 / 200 / 500 | Wait before the batch is considered finished. Raise it if a screenshot catches a fade-in mid-animation. |
| `-TimeoutSec` | all batches | 30 | Give up waiting for the batch sentinel. |
| `-Drawn` | `tree` | off | Only objects actually rendered this frame. |
| `-Text` | `tree` | off | Only objects carrying text. |

Everything lives in `home/uitest/` (gitignored):
`log.txt`, `harness/cmd_<n>.cfg` (the batches sent), `harness/shots/*.png`,
`harness/boot-log.txt` (startup log, kept because the first batch clears `log.txt`).

## How it works

`start` launches the game with an isolated home dir (`-h`) and runs
[`boot.cfg`](boot.cfg) via `-x`. That script installs a `sleep`-driven poll loop
which executes `<home>/harness/cmd_<n>.cfg` files in order.

Each batch the driver writes is:

```cubescript
clearlog                       // truncate, so the log now holds only this batch
<your script>
sleep <settle> [ echo "HARNESS_END <n>" ]
```

The driver waits for that sentinel, then reads the whole log — no byte-offset
bookkeeping, and no output bleeding between hot-reload iterations.

## Behaviour worth knowing

- **Do not minimize the window.** Screenshots are `glReadPixels` of the back
  buffer; minimized comes back solid black. Unfocused is fine — `boot.cfg` sets
  `renderunfocused 1`, and the harness deliberately runs the window unfocused.
- **Screenshots need a settled frame.** `shot` waits by default; a panel caught
  mid-animation just means raising `-Settle`.
- **Repeated errors are collapsed.** UI scripts re-evaluate every frame, so one
  bad alias emits the same line dozens of times per batch. Output shows each
  distinct line once with a `(xN)` count.
- **The poll loop survives map load** — `clearsleep` only drops `IDF_MAP` sleeps.
  `harnessrestart` is available if it ever needs re-arming.
- The game writes CubeScript errors (`Unknown command:` / `Unknown alias lookup:`)
  with a call stack; the driver re-surfaces them as warnings.

## Clicking

`tree`, `find` and `click` need the engine's test commands (`uisetcursor`,
`uikeypress`, `uidumptree`), which are compiled in behind `#ifdef DEBUG_UTILS`.
`src/build.sh` defines it for both `debug` and `release`, so a normal local build
has them; a build that doesn't pass the flag will not, and those three
subcommands will fail with `Unknown command`.

`click <label>` matches on **visible text** — exact first, then substring — among
drawn widgets, and clicks the centre of the match. Use `find` first if a label is
ambiguous; it lists every match. Widgets with no text (bare arrows, sliders,
swatches) can't be addressed this way — read their rect out of `tree` and drive
`uisetcursor` / `uikeypress` directly via `send`.

A text field's contents are not in `tree` either (only its prompt, when that is
drawn), so `find`/`click` never match what was typed into a field. To read
them, `send uidumpeditors`: one
`UIEDITOR <drawn> <x> <y> <w> <h> <editor name> <text>` line per text editor
(`uifield`, `uimlfield`, `uitexteditor`), rects as in `uidumptree`. The editor
name is the bound variable's, and the engine shares one editor buffer per name.
`prefab-selftest.ps1`'s `Get-DrawnEditors` parses it.

## Crash diagnostics

`harness.ps1 start` sets `RE_CRASHLOG=1` for the game process only (it saves
and restores the caller's value around `Start-Process`, so the variable does
not leak into your shell). With it, in any build, the engine
(`src/engine/main.cpp`: `installcrashlog`, `crashlogabort`,
`crashlogexception`, `crashlogbacktrace`, `crashlogminidump`) suppresses the
modal assert dialog, the Windows fault box and `fatal()`'s dialog, and for
both a failed assert and a fault (access violation etc.) writes a header line
plus a symbolised backtrace to `log.txt` (one `logoutf` per frame; for a fault
it walks the faulting context) and a minidump to
`<home>/redeclipse-crash.dmp`, then exits.

Without the variable, behaviour depends on the build:

- **Release** (no `_DEBUG`): unchanged from before the harness. A fault walks
  the stack and `fatal()` logs the trace and shows it in a dialog — the crash
  report players paste into bug reports.
- **Debug** (`_DEBUG`): a failed assert keeps its modal dialog, and a fault is
  left unhandled so an attached debugger breaks at it.

Either dialog is right for a human and wrong for an agent: a blocked modal
reaches the harness only as a timeout, never a diagnosis.

A crash mid-batch surfaces to the caller as soon as the process is gone (the
batch poll checks for it every 100 ms) as
`Game exited while running batch N. Last log: ...` — check `log.txt` (or the
`.dmp` in a debugger) for the backtrace.

### Coordinate spaces (the easy thing to get wrong)

The cursor is a **screen fraction** — x runs 0..1 across the full window width.
`uidumptree` rects have **x in aspect space**, 0..`hudw/hudh` (1.7778 at 16:9).
y is 0..1 in both. So:

```
cursor_x = (obj_x + obj_w/2) / aspect
cursor_y =  obj_y + obj_h/2
```

`tree` and `find` already print the converted `click(x,y)` point, so prefer those
over doing the arithmetic yourself.

`boot.cfg` also raises `cursorsize` to 0.08 (default 0.03) so the pointer is
actually legible in a screenshot when checking where a click landed.

## Gotchas when writing batches

CubeScript has traps that bite generated script in particular — `#` is a macro
preprocessor, `@` is depth-sensitive substitution, `exists` ignores the home dir,
and `exec` needs `exec "path" 0 0` to stay quiet. See "CubeScript traps" in
`CLAUDE.md` before writing anything non-trivial.

## Map editor harness

`editor.ps1` drives the map editor from the same running client as `harness.ps1`
— one game instance, one command channel. Start the client first.

```powershell
tools\harness\harness.ps1 start
tools\harness\editor.ps1 newmap 12                    # or: open atop
tools\harness\editor.ps1 frame 2048 2048 2300 -Dist 128 -Yaw 45 -Pitch 20
tools\harness\editor.ps1 state                        # camera, cursor, selection, entities
tools\harness\editor.ps1 seldrag 2052 2052 2048 2076 2076 2048
tools\harness\editor.ps1 cursor on                    # TAB, for UI interaction
tools\harness\editor.ps1 shot mysel
tools\harness\harness.ps1 stop
```

A fresh `newmap 12` world is 4096 units and **centres on (2048, 2048)**, floor
surface at z = 2048 (`emptymap()`'s bottom four octants are solid,
`src/engine/world.cpp:1656-1657`). Coordinates near 512 land in empty air —
`seldrag`/`sel` need to target the floor, and a `frame`/`goto` camera needs to
be above it and aimed down to see anything.

| Command | Meaning |
|---|---|
| `open <map>` | enter an editing session on a shipped/downloaded map |
| `newmap [size]` | build a deterministic empty scratch world (default size 12) |
| `state` | parsed editor state; the thing to read after every action |
| `goto` / `aim` / `lookat` / `lookatent` | absolute view placement (edit mode only) |
| `frame` / `frameent` | place **and** aim in one call — reproducible screenshots (edit mode only) |
| `nudge <fwd> <right> <up>` | move relative to facing (edit mode only) |
| `cursor <on\|off\|toggle>` | drives the real TAB bind |
| `key <NAME> [-Down] [-Up]` | synthetic input through the full bind path; names are case-insensitive, an unknown one throws |
| `entsel <x> <y> <z> [-Radius] [-Hover]` | select an entity |
| `sel <x> <y> <z> [-Size sx,sy,sz]` | select geometry by coordinates; `-Size` must have exactly 3 values |

The view commands are refused outside edit mode (the engine commands behind
them are compiled into release builds too, so ungated they would be
scriptable teleport and aim in multiplayer). `goto`/`aim`/`lookat`/`frame`/
`nudge` then do nothing; `lookatent`/`frameent` throw, as for a dead index.
| `seldrag <x1 y1 z1> <x2 y2 z2>` | select geometry through the real MOUSE1 drag |
| `shot <name>` | screenshot |

**Entry points.** Neither is a bare engine command: `emptymap()` refuses unless
edit mode is already on, `toggleedit()` refuses unless `connected(false)` and
`allowedittoggle` (`src/engine/octaedit.cpp:198`), and the boot-time
`localconnect` is a no-op, so the raw `newmap` command cannot get a cold client
into edit mode by itself.
`open <map>` runs `edit <map>` — the `config/setup.cfg` alias that sets
`G_EDITING` and force-local-connects before loading the map — that is the real
entry point, not `map`. `editor.ps1 newmap [size]` runs `edit harness_scratch`,
then `edh_enter` (toggles edit mode on if it isn't already), then
`newmap <size>` (default 12) to replace the scratch map with a deterministic
empty world.

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

### `state` field notes

- `Ui.FreeCursor`'s "on" value is a `$clockmillis` timestamp, not `1`
  (`ta_cursor_mode`, `config/tool/tooledit.cfg:473`) — check `-gt 1`, not `-eq 1`,
  after driving `cursor on` through the real TAB bind.
- `Sel.Children` (`selchildcount`) goes **negative** (`-lusize/gridsize`) when the
  selection box sits entirely inside one large octree leaf — that is the normal
  case for a small selection in open space, not an error.

### Known issue: stale `enthover` crashes the client on map load

`entcancel()` (`src/engine/world.cpp:389`) clears `entgroup` but not `enthover`.
On map load, `resetmap()` calls `cancelsel()`, which leaves stale `enthover`
indices pointing past the freshly-cleared entity list. `enthoverloopread`
(`world.cpp:1426`) then does an unguarded `ents[enthover[i]]`, which asserts at
`shared/tools.h:904`. In release builds `ASSERT` compiles out
(`shared/tools.h:28`), so there the stale index is a silent out-of-bounds read
rather than an assert.

**Trigger:** any map load (`open`, `newmap`, a vote, a server map change) while
an entity is hovered in the editor. Not harness-specific — reachable by a human
player.

**Workaround** (used by `editor-selftest.ps1`, not baked into `editor.ps1`):
wrap the load in `entediting 0` / `entediting 1` — the VARF empties `enthover`
and stops it repopulating while the map loads.

```powershell
tools\harness\harness.ps1 send 'entediting 0'
tools\harness\editor.ps1 open atop
tools\harness\harness.ps1 send 'entediting 1'
```

This is a known engine bug, left unfixed pending a decision on
`entcancel()`/`resetmap()`; not a defect in the harness.

### Tests

```powershell
powershell -File tools\harness\tests\edstate.tests.ps1   # parser units, no game
powershell -File tools\harness\tests\task5-smoke.ps1     # UI harness unchanged
powershell -File tools\harness\editor-selftest.ps1       # end to end
powershell -File tools\harness\tests\shadersource.ps1    # shader_new/variantshader_new loader, live, needs a running harness, ~1 minute
```

`tools/harness/tests/` also holds one `.cfg` check per earlier task
(`task1-gamekeypress.cfg`, `task2-eddumpstate.cfg`, …) — exercised in isolation
during that task's development, superseded for regression purposes by
`editor-selftest.ps1`.

### Prefab self-test

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1 [-KeepRunning]
```

Starts its own game and checks the prefab engine commands (`prefabinfo`,
`removeprefab`, `renameprefab`, `copyprefab`/`saveprefab` fixes), the file
browser widget's prefab mode and per-instance state, the existing file pickers
through their real controls (map music, heat haze texture: browse `data/`, OK
selects; the values are restored), the CubeScript prefab library, and the
Prefabs panel (clicks, double-clicks, context menu, popups, its search field
beside a picker's).

- Wipes `home/uitest/prefab/` and `home/uitest/backups/prefab/` first.
- Simulates a shipped (read-only) prefab with `<repo>/prefab/zz_selftest_shipped.obr`,
  deleted on exit together with `<repo>/prefab/` if the script created it.
- Writes screenshots `prefab-*.png` for review; the script's comments say what
  each must show.
- Not covered: `saveprefab` on a selection over 100 MB, and `IDF_MAP`
  refusal of the new commands. Neither is reachable from the harness. Nor is
  `renameprefab` refusing a case-only rename onto another existing file,
  which only applies on case-sensitive filesystems (not Windows).

## Shader equivalence harness

`shaders.ps1` proves a shader refactor changed nothing, one configuration at a time.
Design: `docs/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md`.

```powershell
tools\harness\shaders.ps1 record                 # baseline: every setting in shader-sweep.txt x every shipped map
tools\harness\shaders.ps1 check -Filter 'bump*'  # after an edit: record a candidate, compare
tools\harness\shaders.ps1 check -Sids s00 -NoMaps  # fast loop: defaults only
tools\harness\shaders.ps1 diff bumpworld -Sid s00  # why one configuration differs
tools\harness\shaders-selftest.ps1               # acceptance tests for the harness itself
```

A configuration is `(shader name, settings id)`. Variants are named `<variant:col,row>parent`.
Corpora live in `home\uitest\shadercorpus\<run>\` (`manifest.tsv`, `blobs\<hash>\`,
`settings\`, `registry.txt`, `gl.txt`, `run.txt`).

| Status | Meaning |
|---|---|
| `PASS-TEXT` | Hash-identical, or the same tokens after preprocessing |
| `PASS-SPIRV` | Same SPIR-V after `spirv-opt -O` and `spirv-remap` |
| `PASS-PIXEL` | Same RGBA32F output on seeded inputs: the weakest evidence, so review it |
| `WEAK` | The bench could not exercise it: coverage < 50%, or it couldn't feed both sides identical inputs (see below) |
| `FAIL` | Contract mismatch (metadata/reflection/registry), or pixels differ |
| `MISSING` / `EXTRA` | Valid in only one of the two corpora |

- Recording and the pixel tier need the running game. Tiers 1–2 need `glslang-tools` and
  `spirv-tools` in the `Ubuntu` WSL instance. Without `glslangValidator`, tier 1 still runs
  on the unpreprocessed source: comments stripped, compared line by line, with directive
  lines keeping their spacing (`#define M(x)` and `#define M (x)` differ), so only
  whitespace and comment changes pass as `PASS-TEXT`. The same fallback applies to both
  sides when `-E` fails on either, and the result's detail says so
  (`tier1 raw fallback: ...`). Tier 2 then reports `NA`, and everything else falls
  through to pixels.
- **The editor registry is part of the contract.** `check` compares the two
  `registry.txt` files byte for byte, in order (the editor picks entries by position), and
  reports a difference as `FAIL registry -` with the first differing line in `-`/`+` form,
  whatever `-Filter` and `-Sids` say.
- **`-Filter` includes variants:** `-Filter 'bump*'` matches `<variant:0,1>bumpworld` too.
- A corpus is only fully comparable on the GPU/driver that recorded it (`gl.txt`): reflection
  and pixels come from the driver. On another, `check` refuses to run, names both GL
  strings and tells you to re-record the baseline on this machine from the commit in its
  `run.txt`. `-AllowCrossGpu` runs anyway, without reflection and pixels (max tier 2); the
  metadata contract (`meta.txt`) and the registry are still compared, and every result's
  detail says `(cross-gpu: reflection and pixels skipped)`.
- `check -Run candidate` is refused: `candidate` is the scratch corpus `check` records into.
- `record` fails if a shader valid at `s00` is gone or different at `s99` (both defaults):
  state leaked between sweep points. A shader valid only at `s99` is not a leak (an earlier
  point registered it for good, e.g. `msaa`'s `deferredlightM*` rows). It also fails if a
  registered world/decal shader is not valid at every point. Recording a `-Sids` subset,
  and `check`, still reset every var of the whole sweep.
- The per-map pass (`m-<map>`) dumps map shaders and every shader a `generateshader`
  command made (models, deferred lights, grass, AO, ...): C++ formats their options from
  what the map contains.
- **`WEAK` has two distinct sources.** One is low coverage (< 50% of pixels written).
  The other: a `SHADERBENCH ... FAIL ...` line whose detail carries
  `reason=unsupported uniform`. The bench leaves a uniform of a type it can't seed unset,
  so the two programs may have read different values from it, the mismatch is
  inconclusive, and `shaders.ps1` remaps it from `FAIL` to `WEAK` (`ConvertFrom-BenchLine`
  in `shadercorpus.ps1`). An unsupported **sampler** or **attribute** stays `FAIL`: the bench
  binds nothing for it on either side, so both programs saw the same inputs and a pixel
  difference is real.
- **Applying a sweep point can require a GL reset.** Some settings vars (`msaa*`,
  `gdepthstencil`, `gstencil`, `glineardepth`, `hdrgamma`, `textsupersample`,
  `gscalecubicsoft`) only queue a "Pending shader change" on `resetshaders`; the real
  reload needs a follow-up `resetgl`. `record` watches the log for that message and issues
  `resetgl` automatically, which requires `applydialog 1` (pinned for the sweep) since
  that is what makes the engine print it in the first place.
- **Determinism relies on a `slotparamsscope` guard** (`src/engine/shader.cpp`) around
  every shader-definition entry point (`loadshaders`, `setupshaders`, `generateshader`,
  `Shader::force`). Without it, leftover texture-slot params leaked into shaders defined
  later as stray `uniform vec4` declarations and default params — which is why the s00/s99
  leak check, and the corpus in general, can trust two dumps of "the same" shader to
  actually match.
- **Draw buffers follow the shader's outputs.** `shaderbench` draws only to the attachments
  whose location a `fragdata(n)` output declares (all 4 if it declares none), so an
  attachment no output writes, which the GL spec leaves undefined, stays at the clear value
  in both renders instead of depending on the driver.
- **Adding a generator input:** when a `config/glsl` generator starts reading a new var,
  add a vector for it to `shader-sweep.txt` and re-record the baseline.
