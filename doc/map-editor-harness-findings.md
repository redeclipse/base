# Map editor harness — findings and outstanding issues

Status as of 2026-09-24, after the map editor harness work landed on branch `map-editor-harness`
(not pushed):

| Commit | Summary |
|---|---|
| `b2965b44` | `harness: add ui test harness scripts` — the pre-existing, never-committed UI harness, as it stood before this work |
| `b2b9ffcd` | `engine: add map editor harness commands` |
| `d06733fc` | `engine: add opt-in crash logging for harness runs` |
| `f4b2a61a` | `harness: add map editor harness` |

Verified on the final tree (debug build): parser unit tests 44/44, UI-harness smoke 7/7,
driver checks 8/8, end-to-end `editor-selftest.ps1` 84 checks, exit 0.

Design: [doc/superpowers/specs/2026-09-03-map-editor-harness-design.md](superpowers/specs/2026-09-03-map-editor-harness-design.md) ·
Plan: [doc/superpowers/plans/2026-09-03-map-editor-harness.md](superpowers/plans/2026-09-03-map-editor-harness.md) ·
Usage: [tools/harness/README.md](../tools/harness/README.md)

---

## 1. Outstanding bugs

### 1.1 Engine: stale `enthover` crashes the client on map load — **open, high**

Loading a map while an entity is hovered in the editor aborts the client.

| Step | Where |
|---|---|
| Map load calls `resetmap()` → `cancelsel()` | `src/engine/world.cpp:1605` |
| `cancelsel()` → `cubecancel()` + `entcancel()` | `src/engine/octaedit.cpp:190` |
| `entcancel()` clears `entgroup` **only**; `enthover` keeps indices into the old entity list | `src/engine/world.cpp:389-392` |
| During the load, `progress()` redraws the HUD, which runs `enthoverloopread` → `entfocus(enthover[i])` → unguarded `ents[...]` | `src/engine/world.cpp:1426` |
| `vector::operator[]` bounds assert: `i>=0 && i<ulen` | `src/shared/tools.h:904` |

- **Reachable in ordinary play**, not just from the harness: the captured backtrace includes
  `server::vote`, so a vote-driven map change while editing hits it.
- **Release builds are worse:** `ASSERT` compiles to nothing without `_DEBUG` (`src/shared/tools.h:28`),
  so the stale index becomes an out-of-bounds read instead of an assert. Before this work that
  crashed with a report; see 2.1 for why it still does.
- **Only other place `enthover` is cleared:** `src/engine/world.cpp:362`, inside the `entediting`
  handler.
- **Likely fix (one line):** also clear `enthover` in `entcancel()`. Not applied — engine fixes were
  outside this work's scope. Worth checking the other `enthover[i]`/`entgroup[i]` readers for the
  same stale-index assumption at the same time (see 1.2).
- **Current workaround:** `editor-selftest.ps1` wraps every map load in `entediting 0` / `entediting 1`
  (`Invoke-MapLoad`, with a try/finally). It is kept in the self-test on purpose so the bug stays
  visible; `editor.ps1 open`/`newmap` do **not** apply it, so a harness user who loads a map with an
  entity hovered will crash the client — but now gets a readable backtrace (section 2).

### 1.2 Engine: same stale-index pattern elsewhere — **latent, unconfirmed**

- `newundoent()` indexes `ents[entgroup[i]]` with no `inrange` check (`src/engine/world.cpp:406`).
  `entgroup` *is* cleared on map load, so the map-load path above does not reach it; any other path
  that shrinks the entity list while `entgroup` holds indices would. Not reproduced.
- `eddumpstate` (this work) guards against it: `dumpedent` returns false for an out-of-range index and
  the reported count only counts lines actually printed.

### 1.3 Engine: dead `findkeycode(char*)` — **latent trap**

`int findkeycode(char *key)` at `src/engine/console.cpp:331` has no callers, and it returns `0` rather
than a sentinel when a name is not found. During this work, a new `findkeycode(const char*)` lost
overload resolution to it without any error and made every `gamekeypress` call report success. The
new helper is now named `resolvekeycode`. The dead function is still there, ready to catch the next
similarly-named helper.

### 1.4 Build: pre-existing warnings

A clean debug build prints 7 warnings, none added by this branch:
`command.cpp:1568` (compilestr), `renderva.cpp:55`, `renderva.cpp:1907`, `renderlights.cpp:2960`,
`world.cpp:256` (unused `addentity`, also present at HEAD), `game.cpp:941`, and the `winver.h` rc
include.

---

## 2. Crash diagnostics (new) — what to know

Before this work, the harness could not see a crash at all. The `stackdumper()` crash reporter already
existed, but it was compiled out under `_DEBUG`, and `_DEBUG` is the only build type that compiles
`src/tests/*.o`. A failed assert therefore opened a modal dialog that blocked the process, and the
harness saw only a timeout.

Now, with `RE_CRASHLOG` set (`harness.ps1 start` sets it for the game process only):

- asserts (SIGABRT) and access violations (SEH) write a symbolised backtrace to `log.txt`, one frame
  per line, and a minidump to `<home>/redeclipse-crash.dmp`, then `_exit`;
- the calling harness command throws `Game exited while running batch N. Last log: ...` as soon as the
  process dies, rather than after `-TimeoutSec`.

| Build | `RE_CRASHLOG` unset | `RE_CRASHLOG` set |
|---|---|---|
| Release | Same as before this work: walk the stack, `fatal()` logs it and shows a dialog | Harness mode |
| Debug | Fault reaches an attached debugger; asserts keep their dialog | Harness mode |

### 2.1 Open issues in the crash path

- **Crashes on other threads are not captured.** Only the main thread runs inside `__try`
  (`src/engine/main.cpp:1325`). In harness mode `SEM_NOGPFAULTERRORBOX` is set, so a fault on another
  thread (audio, for example) exits with no trace and no dump. The harness does notice the exit
  straight away. Fix: `SetUnhandledExceptionFilter`.
- **The release crash path was not exercised at runtime.** The work stayed on the debug build. The
  release branch of `stackdumper` was compared line by line with HEAD and matches it, apart from two
  stack-frame initialisers written `{}` instead of `0`, which are semantically identical. It compiles
  cleanly with release flags. Nobody has run it.
- **A deadlock is possible in the SIGABRT handler.** `crashlogabort` (`src/engine/main.cpp:986`)
  logs, symbolises and writes a dump. If the assert fires while the CRT heap lock or the log path is
  held, it can hang. That is no worse than the modal dialog it replaces, and acceptable for an opt-in
  tool.
- **One comment overstates its case.** `src/engine/main.cpp:915` says "only" `stackdumper()` and the
  SEH wrapper were compiled out under `_DEBUG`. The `fatalsignal` install block was as well, and still
  is.

### 2.2 Dead ends worth recording

- `<crtdbg.h>` cannot be included: it pulls in `vcruntime_new_debug.h`, which redefines the placement
  new/delete from `src/shared/tools.h:68`.
- `_CrtSetReportHook` does not link. The build is `-static` against the **release** CRT even though
  it defines `_DEBUG`, so `assert()` goes `_wassert` → `abort()`. Catching SIGABRT works whichever CRT
  is linked.
- `RtlCaptureStackBackTrace` is declared by hand, because `winbase.h` only declares it above the
  `_WIN32_WINNT 0x0500` that `src/shared/cube.h:32` pins.

---

## 3. Harness: open issues and deferred items

### 3.1 Should fix

| Item | Where | Notes |
|---|---|---|
| A paragraph inside the command table breaks it: the `seldrag` and `shot` rows render as text, not table rows | `tools/harness/README.md:180-185` | Move the "refused outside edit mode" paragraph below the `shot` row. |
| `-Pitch 20` default puts the orbit camera **below** its target, looking up | `tools/harness/editor.ps1:39` | `orbitpos` computes `target - dir*dist`, so a positive pitch puts the camera below. For a floor-level target it ends up inside solid geometry. `-20` is the likely intended default. The README/help `frame` example has the same problem. Left alone because changing it changes the interface you approved in the spec. |

### 3.2 Known behaviour, by design

- Outside edit mode, the view commands (`edgoto`, `edaim`, `edlookat`, `edframe`, `ednudge`) do
  nothing and return no error. `edlookatent`/`edframeent` return 0, and `editor.ps1` turns that into a
  throw. The edit-mode gate exists because `build.sh release` defines `DEBUG_UTILS`, and without it
  these commands would be scriptable teleport/aim in live multiplayer.
- `edselcube`, `edselbox`, `edentnear` (`sel`, and `entsel` without `-Hover`) skip the real input
  path. Use them for setup only; assert through `seldrag` and `entsel -Hover`.

### 3.3 Deferred minors (reviewed, left as they are)

| Item | Where |
|---|---|
| `edentnear` repeats the `nearestent` scan almost line for line | `src/engine/world.cpp` (`edentnear` vs `nearestent`) |
| The `tree` crash fix uses `$x = ...; $x \| ForEach-Object` where `find`/`click` use `@(...)` for the same problem | `tools/harness/harness.ps1` |
| A bare, truncated `EDSTATE` token (no trailing space) counts as log noise rather than malformed. `.Complete` still catches the truncated dump. | `tools/harness/edstate.ps1` |
| The verbatim "real capture 2" test does not assert its own `Cam.Pitch` of `-56.78864` | `tools/harness/tests/edstate.tests.ps1` |
| `key -Down -Up` together falls through to a tap | `tools/harness/editor.ps1` |
| `EDRESULT=` echo is built in two different styles | `tools/harness/editor.ps1` |
| `newmap` size uses `[int]` rather than the invariant parser | `tools/harness/editor.ps1` |
| The "camera position unchanged" check only compares Y | `tools/harness/editor-selftest.ps1` |
| No negative control showing the entity is *not* hovered before the aim | `tools/harness/editor-selftest.ps1` |
| The "grid size is positive" check is loose; the exact value is checked in a later step | `tools/harness/editor-selftest.ps1` |
| `GetVar` puts the variable name into a regex without escaping it. Its callers only pass literals. | `tools/harness/editor-selftest.ps1` |
| `edh_leave` has no dedicated assertion | `tools/harness/editor.cfg` |

---

## 4. Engine facts established during this work

Each one cost debugging time. Everything here was traced to source and confirmed on a live client.

| Fact | Detail |
|---|---|
| **`newmap` is not an entry point** | `emptymap()` refuses unless edit mode is already on (`src/engine/world.cpp` `emptymap`). `toggleedit()` refuses unless `connected(false)` and `allowedittoggle` (`src/engine/octaedit.cpp:198`). The boot-time `localconnect(false)` does nothing because `autoconnect` defaults to 0. Enter through the `edit` alias (`config/setup.cfg:29`) and then `edittoggle`. This is also what the game's own New Map button does. |
| Fresh `newmap 12` world | Centred at 2048, not 512. The bottom four octants are solid. Camera starts at (2048, 2048, 4096). |
| Strafe sign | `left` sets `strafe = +1` and `right` sets `-1` (`src/game/physics.cpp:88-89`). Increasing yaw turns **right**. The right vector is `(-dir.y, dir.x, 0)`. |
| `ui_freecursor` | Not a boolean. It is `1` at boot (`config/ui/lib.cfg:11`), and after a TAB toggle its "on" value is `$clockmillis` (`config/tool/tooledit.cfg:473`). |
| TAB cursor bind | `toolbind TAB ta_cursor_mode` (`config/setup.cfg:348`). It only acts in the `CS_EDITING` client state. |
| `selchildcount` | Negative (`-lusize/gridsize`) when the box sits inside a single large leaf (`src/engine/octaedit.cpp:591,648-651`). A 1×1×1 box in a fresh world gives `-256`. |
| Click vs drag | A plain click selects `1,1,1`, and so does a 1-cube drag. Tell them apart by the size of a drag across several cubes. |
| `selinfo::validate()` | Checks world bounds and clamps size. It does **not** check grid alignment (`src/shared/iengine.h:83-95`). |
| `onrelease` needs `keypressed` | Only `execbind` sets it. That is why synthetic input has to go through `processkey`, not the aliases. |
| `identexists` | Exists (`src/engine/command.cpp:1119`). Use it to test for an alias without triggering "Unknown alias lookup". |

### PowerShell 5.1 traps hit

- `Write-Host` writes to the information stream (6), not the success stream. To assert on its text,
  use `6>&1`.
- The implicit `[double]"400.5"` cast is already invariant-culture. Only an explicit
  `[double]::Parse(s)` misreads under `de-DE`.
- Don't comma-wrap a return (`return , $arr`) when the caller already does `@(...)`. The caller
  gets **one** item: the whole array. Piping it caused the `harness.ps1 tree` crash. Under `@(...)`
  it caused `find`/`click` to report a fake hit at `click(0,0)` when nothing matched, and to see
  several matches as one. `Find-Widget`/`Add-ClickPoint` now return their results unwrapped.
  Also, `return $x` with `$x = $null` still writes one `$null`, which `@(...)` counts. Emit the
  items in a loop instead.
- An empty scroll area makes `ScrollBar::vscale` (`src/engine/ui.cpp:5185`) compute 0/0, and
  `uidumptree` prints `-nan(ind)` for the button's rect. `harness.ps1` parses these through
  `ConvertTo-TreeDouble` into real NaN/Infinity values and leaves non-finite widgets out of
  `find`/`click`. `ConvertTo-InvariantDouble` stays strict for `edstate.ps1`.

---

## 5. Not committed

- `doc/superpowers/` (the spec and plan) and this file were untracked at the time; they were
  committed to `doc/` on 2026-10-05.
- `CLAUDE.md` was updated with the editor commands, `RE_CRASHLOG` and the known bug. It is gitignored.
- `.superpowers/sdd/2026-09-03-map-editor-harness/` is excluded from git by `.git/info/exclude`. It
  holds the execution ledger (every ruling and review outcome), per-task reports and review packages.
  Delete it once it is no longer useful.
- Pre-existing unrelated changes are untouched: `readme.md`, `chat_wip.cfg`, `deli.zip`,
  `gun_lore.txt`, `profile_daemon.ps1`, `profiler_tools.zip`, `profilerhook.cfg`, `unix/`.
- Only the final tree was built and tested. The intermediate commit `b2b9ffcd` (engine commands
  without crash logging) was not built on its own.
