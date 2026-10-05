# Map editor test harness — design

Date: 2026-09-03
Status: approved for planning

## Goal

Give an agent (or a person) the same outside-the-process control over the **map editor**
that `tools/harness/` already provides over the UI: drive the first-person viewport,
lock and unlock the mouse cursor, select entities, select world geometry — and read
back enough state to verify that each of those actually happened.

Scope is the four capabilities named in the request:

1. FPP viewport manipulation
2. Cursor lock/unlock for UI interaction (the TAB path)
3. Entity selection
4. World geometry selection

Out of scope: texture editing, heightmap/blendmap painting, prefabs, entity attribute
editing beyond what selection readback needs. Those stay reachable through the existing
`send` passthrough.

## Background: what already works, and what does not

The existing UI harness (`tools/harness/harness.ps1` + `boot.cfg`) launches the client
against an isolated home dir, installs a `sleep`-driven poll loop that executes
`<home>/harness/cmd_<n>.cfg` files in order, and reads results back out of `log.txt`.
That transport is sound and is reused unchanged.

### Already scriptable — no engine work needed

| Capability | Existing surface |
|---|---|
| Entity selection | `numenthover`, `enthoveridx`, `entadd`, `enttoggle`, `enttoggleidx`, `enthavesel`, `entgroupidx`, `entloopread`/`enthoverloopread` with `entget`, `entpos`, `enttype`, `entattr` (`src/engine/world.cpp:810`–`1364`) |
| Geometry selection | `dragging` (`src/engine/octaedit.cpp:136`), driven by the `drag` / `corners` / `editdrag` / `editextend` aliases (`config/engine.cfg:200`–`255`); plus `hassel`, `havesel`, `selorient`, `selextend`, `reorient`, `cancelsel` |
| Cursor lock | `ui_freecursor` — a plain script var (`config/ui/lib.cfg:11`), flipped by `ta_cursor_mode` (`config/tool/tooledit.cfg:467`), consumed as `uiallowinput (? $ui_freecursor 2 0)` (`config/ui/hud/package.cfg:518`) |
| Camera **readback** | `$cameraposx/y/z`, `$camerayaw`, `$camerapitch`, `$cameraroll` (`src/game/client.cpp:761`) |
| Entering a session | `map <name>` does a `localconnect` (`src/game/client.cpp:1568`); `newmap <size>` (`src/engine/world.cpp:1598`); `edittoggle` (`src/engine/octaedit.cpp:260`) |

### Blocked without engine work

**1. The view cannot be set.** `game::mousemove` writes `player1->yaw/pitch` directly
(`src/game/game.cpp:3062`). Nothing script-facing writes position or angles. Replaying
mouse deltas is not an option — there is no script path into `mousemove` — so discrete
`goto` / `aim` / `lookat` commands are the only workable design.

**2. The edit cursor and cube selection cannot be read.** `cur`, `orient`, `gridsize`
and `selchildcount` are file-scope in `src/engine/octaedit.cpp:116`–`121`. `hassel` and
`havesel` return only counts. So today a harness can *perform* a geometry selection but
can never confirm **what** got selected — which makes the capability untestable.

**3. Game-level key synthesis is missing.** `uikeypress` reaches only the UI. Editor
input runs through `processkey` (`src/engine/console.cpp:867`) → `execbind`
(`:625`). This is load-bearing, not convenience: `addreleaseaction` returns early
unless `keypressed` is set (`:578`), and `execbind` is the only thing that sets it.
So `onrelease` — and therefore `drag`, `moving`, `tool_grabbing` — is unreachable by
calling the aliases directly.

### Two findings that make this safe

- `getkeymodifiers()` (`src/engine/console.cpp:609`) resolves modifiers through
  `iskeypressed()` → the engine's own `keym::pressed` flags, **not** `SDL_GetModState`.
  Synthetic LSHIFT/LCTRL/LALT therefore gate modifier binds correctly, and `$curshiftmod`
  works because it is set by the `LSHIFT` bind itself (`config/setup.cfg:264`).
- The Windows TAB→minimize intercept in `processkey` fires only under
  `SDL_GetModState()&MOD_ALTS`, so a synthetic TAB cannot minimize the window.
  This matters: a minimized window screenshots as solid black.

## Layer 1 — engine additions (C++)

All new commands are `#ifdef DEBUG_UTILS` and refuse when `identflags&IDF_MAP`, exactly
like the existing UI test commands (`src/engine/ui.cpp:7462`), so a downloaded map cannot
drive the editor or synthesise input. `src/build.sh` already defines `DEBUG_UTILS` for
both `debug` and `release`.

**Unlike the UI harness, this requires a rebuild:**
`wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh release`

### View control — `src/engine/world.cpp`

The editor already has a convention for moving the view: `entautoview`
(`src/engine/world.cpp:896`) and `nearestent` (`:1247`) both use
`game::focusedent(true)`, falling back to `camera1`, then `resetinterp(true)`.
The new commands follow that same convention rather than reproducing `mousemove`'s
physent expression — it is the pattern they sit beside, and `entautoview` already
proves it moves the view correctly in edit mode. A single static helper
`static physent *editviewent()` captures it, used by every command below.

| Command | Args | Behaviour | Returns |
|---|---|---|---|
| `edgoto` | `x y z` | set position, `resetinterp(true)` | — |
| `edaim` | `yaw pitch` | set angles, `fixrange` | — |
| `edlookat` | `x y z` | aim at a world point via `game::getyawpitch` (`src/game/game.h:2917`) | — |
| `edlookatent` | `idx` | aim at entity `idx`'s `e.o` | `1` ok, `0` bad idx |
| `edframe` | `x y z dist yaw pitch` | place `dist` from the target at those orbit angles **and** look at it | — |
| `edframeent` | `idx dist yaw pitch` | same, targeting entity `idx` | `1` ok, `0` bad idx |
| `ednudge` | `fwd right up` | move relative to current facing | — |

`edframe` / `edframeent` are the reproducible-screenshot primitive: one call fully
determines what the camera sees, so a screenshot is a function of its arguments alone.

**Entity id space.** `idx` indexes `entities::getents()` — the same space used by
`enthoveridx`, `entgroupidx` and `enttoggleidx`, and reported by the `EDENT` lines below.
`edlookatent` / `edframeent` guard `ents.inrange(idx)` and return `0` for out-of-range or
`ET_EMPTY` slots; empty slots are recycled placeholders (`src/engine/world.cpp:1031`) with
no meaningful position, so aiming at one would silently point at the origin.

### State readback — `src/engine/octaedit.cpp`

One command, `eddumpstate`, printing structured lines in the style of `uidumptree` so the
driver needs exactly one parser. It lives in `octaedit.cpp` because `cur`, `orient`,
`gridsize` and `selchildcount` are all in scope there; `sel`, `havesel` and `editmode` are
already extern in `src/shared/iengine.h:97`, and `worldpos` at `:190`.

```
EDSTATE mode <editmode> <gridpower> <gridsize> <orient>
EDSTATE cam <x> <y> <z> <yaw> <pitch>
EDSTATE worldpos <x> <y> <z>
EDSTATE cur <x> <y> <z> <orient>
EDSTATE sel <ox> <oy> <oz> <sx> <sy> <sz> <grid> <orient> <cx> <cy> <cxs> <cys> <corner> <children> <havesel>
EDSTATE ui <cursorlock> <freecursor>
EDENT hover <idx> <type> <x> <y> <z>
EDENT sel <idx> <type> <x> <y> <z>
EDSTATE end
```

`sel` fields follow `struct selinfo` (`src/shared/iengine.h:73`). `cursorlock` comes from
the existing `uigetcursorlock`; `freecursor` is read from the `ui_freecursor` alias.
One `EDENT hover` line per `enthover` entry and one `EDENT sel` per `entgroup` entry, so
a single round trip answers "what is under the crosshair" and "what is selected".

This is the verification surface for the whole harness. Without it, geometry selection
can be performed but not asserted.

### Direct action wrappers — `src/engine/octaedit.cpp`, `src/engine/world.cpp`

Coordinate-addressed shortcuts that skip the aim-then-click flow:

| Command | Behaviour |
|---|---|
| `edselcube x y z` | select the single grid cube containing that world point |
| `edselbox x y z sx sy sz` | set `sel` from world coords, snapped to the current grid |
| `edentnear x y z radius` | coordinate-based `nearestent` — `entadd` the closest entity within `radius` |

Both selection wrappers run the result through `selinfo::validate()`
(`src/shared/iengine.h:83`) rather than hand-rolling bounds checks, and set `havesel`
only when it passes.

**Known trade-off.** These bypass the input path real users take, so a harness leaning on
them can pass while aim-then-click is broken. Mitigation is a testing rule, not a code
rule: the self-test exercises selection through the **real** path and uses the wrappers
only for setup. See "Testing".

### Key synthesis — `src/engine/console.cpp`

`gamekeypress <name|code> <down>` → `processkey`. Accepts either a keymap name
(`TAB`, `MOUSE1`, `LSHIFT`) or a raw code; names resolve by enumerating `keyms`, which is
keyed by code and so needs a linear scan — fine at harness call rates. Returns `1` if the
key was known, `0` otherwise, so a typo surfaces instead of silently doing nothing.

Codes for reference (`config/keymap.cfg`): `MOUSE1` `-1`, `MOUSE3` `-2`, `MOUSE2` `-3`,
`TAB` `9`, `ESCAPE` `27`.

## Layer 2 — `tools/harness/editor.cfg`

Loaded on demand by `editor.ps1` (`exec "tools/harness/editor.cfg" 0 0`), **not** from
`boot.cfg`. UI-only sessions stay byte-identical to today. Contains no `#` and no `@`,
per the CubeScript traps in `CLAUDE.md`.

| Alias | Behaviour |
|---|---|
| `edh_enter` | `if (! $editing) [ edittoggle ]` — idempotent |
| `edh_leave` | the inverse |
| `edh_cursor <0\|1>` | read `$ui_freecursor`; tap TAB via `gamekeypress` only if it needs to flip. Absolute, not a toggle, so it is safe to call repeatedly |
| `edh_dragsel` | `gamekeypress MOUSE1 1` → re-aim → `gamekeypress MOUSE1 0`, so `drag` and its `onrelease` both run for real |
| `edh_tap <key>` | press + release with a frame between |

`edh_cursor` deliberately drives the real TAB bind rather than assigning `ui_freecursor`
directly — the point is to test the path the user takes, including `ta_cursor_mode`'s
`getclientstate` guard (`config/tool/tooledit.cfg:467`).

## Layer 3 — PowerShell drivers

### `tools/harness/core.ps1` (new)

Shared plumbing extracted from `harness.ps1`, dot-sourced by both drivers. Moves
verbatim — no behaviour change: path/dir constants, `Write-TextNoBom`, `Read-LogSafe`,
`Get-HarnessProcess`, `Assert-Running`, `Get-NextSeq`, `Invoke-Batch`, `Show-BatchResult`,
`Set-WindowNoActivate`, `ConvertTo-InvariantDouble`, `Format-Coord`, and the screenshot
helper (lifted out of the `shot` case so both drivers share it).

`harness.ps1` keeps all 11 existing subcommands and its exact current interface,
including `-Settle` / `-TimeoutSec` / `-Drawn` / `-Text` defaults. It only stops defining
what it now dot-sources.

### `tools/harness/editor.ps1` (new)

Drives the **same** client, home dir and command channel as `harness.ps1` — one game
instance, one `cmd_<n>.cfg` sequence. `editor.ps1` does not launch anything itself
beyond delegating to the shared start.

| Subcommand | Meaning |
|---|---|
| `open <map>` | `map <name>`, wait for load, `edh_enter` |
| `newmap [size]` | `newmap <size>` (default 12), `edh_enter` |
| `state` | `eddumpstate` → parsed object; default view is a readable summary |
| `goto <x> <y> <z>` | `edgoto` |
| `aim <yaw> <pitch>` | `edaim` |
| `lookat <x> <y> <z>` | `edlookat` |
| `lookatent <idx>` | `edlookatent` |
| `frame <x> <y> <z> -Dist -Yaw -Pitch` | `edframe` |
| `frameent <idx> -Dist -Yaw -Pitch` | `edframeent` |
| `nudge <fwd> <right> <up>` | `ednudge` |
| `cursor <on\|off\|toggle>` | `edh_cursor` |
| `key <name> [-Down] [-Up]` | `gamekeypress`; default is a tap |
| `entsel <x> <y> <z> [-Radius]` | `edentnear`; with `-Hover`, aim first then `entadd` |
| `sel <x y z> [-Size sx sy sz]` | `edselcube` / `edselbox` |
| `seldrag <x1 y1 z1> <x2 y2 z2>` | the real path: aim → MOUSE1 down → aim → up |
| `shot <name>` | shared screenshot helper |

`state` is the command an agent calls after every action. Its parser is the one piece of
real logic on this side and is unit-tested (below).

Neither `open` nor `newmap` is implicit: `start` boots the client exactly as today, and
the caller chooses a shipped map (real geometry and entities to select) or a deterministic
empty world.

## Testing

Split by what can honestly be tested without a running game.

**Unit — `tools/harness/tests/state-parser.tests.ps1`.** The `EDSTATE`/`EDENT` parser is a
pure function over lines. Tested against canned input with no game: well-formed dumps,
`havesel 0`, zero hover entities, multiple selected entities, log timestamp prefixes,
and malformed lines (which must be reported, not silently dropped). Also covers the
invariant-culture number parsing — a comma-decimal locale would otherwise mis-read every
coordinate, the same trap the UI harness already guards.

**Integration — `tools/harness/editor-selftest.ps1`.** End-to-end against a live client,
reporting actual vs expected per step:

1. `start`, `newmap 12`, assert `EDSTATE mode` shows `editmode 1`
2. `edframe` a known point, assert `EDSTATE cam` matches within tolerance
3. assert `EDSTATE cur` names the expected grid cube for that aim
4. `seldrag` between two aims — the **real** MOUSE1 path — then assert `sel` covers the
   expected box and `children` > 1
5. `cursor on`, assert `EDSTATE ui` shows `freecursor` non-zero; `cursor off`, assert it
   returns. This is the TAB path end to end
6. `newent` a known entity, `lookatent` it, assert it appears in `EDENT hover`;
   `entadd`, assert it appears in `EDENT sel`
7. `stop`

Step 4 and step 6 deliberately use the real input path. The direct wrappers
(`edselcube`, `edentnear`) are used only where a step needs deterministic *setup*, never
as the thing under test.

## Documentation

- `tools/harness/README.md` — an editor section covering the subcommands, the
  aim-then-click vs direct-wrapper distinction, and the rebuild requirement.
- `CLAUDE.md` — the new commands added to the "Test-only commands" table, plus a note
  that the editor harness (unlike the UI harness) needs a rebuild when the engine side
  changes.

## Risks

| Risk | Mitigation |
|---|---|
| Direct wrappers mask a broken real input path | Self-test asserts through the real path; wrappers restricted to setup |
| `core.ps1` extraction regresses the working UI harness | Move code verbatim, no behaviour change; `harness.ps1`'s public interface is covered by its existing subcommands being exercised after the refactor |
| Engine changes drift from `mousemove`'s notion of the view entity | Follow the editor's own established `focusedent(true) ?: camera1` convention, in one shared helper |
| Screenshots caught mid-frame | Same `-Settle` discipline as the UI harness; `edframe` makes the view a pure function of its arguments so a retry is identical |
