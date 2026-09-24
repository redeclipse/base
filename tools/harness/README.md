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
