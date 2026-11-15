# Scripted input

The `input*` commands let a CubeScript test script control the client as a player would: hold keys, move the mouse, click menus and type text. They live in `src/engine/main.cpp` (`inputinjector`) and feed the same paths as real SDL events:

- keys go through `processkey()`, so the console, the HUD, the UI and key binds (including "D" action commands like `primary`) all see them
- mouse motion goes through `game::mousemove()`, which turns the view in game or moves the cursor when a menu has input
- text goes through `processtextinput()`

They don't depend on window focus or the pointer grab, so they work when the window isn't focused or when external tools like ydotool can't reach it (KDE Plasma on Wayland, for example).

## Commands

| Command | Effect |
|---|---|
| `inputkey <KEY> 1\|0` | Hold (1) or release (0) a key. Holding it runs the key's bind, as a real key press does |
| `inputpress <KEY> [ms]` | Press a key and release it after `ms` milliseconds, holding it for at least one frame |
| `inputrelease` | Release every key held by these commands |
| `inputheld` | Return the names of the keys held by these commands, separated by spaces |
| `inputmouse <dx> <dy> [ms]` | Move the mouse by `dx`,`dy` counts, spread evenly over `ms` milliseconds (all at once if 0 or left out) |
| `inputcursor <x> <y>` | Put the menu cursor at `x`,`y`, from 0 to 1 across and down the screen |
| `inputtext <str>` | Type `str` into the focused text field, or into the console if it is open |

Keys use the names from `config/keymap.cfg`: `W`, `A`, `S`, `D`, `SPACE`, `RETURN`, `ESCAPE`, `BACKQUOTE` (console), `MOUSE1` (left), `MOUSE2` (right), `MOUSE3` (middle), `MOUSE4`/`MOUSE5` (wheel up/down). An unknown name prints an error and does nothing.

Timing uses `totalmillis`, so it keeps running while the game is paused. `inputmouse` counts are scaled by the player's sensitivity settings the same way real mouse counts are, so set those explicitly if a test needs exact angles.

## Example

The script runs from `-x` at launch (see the testing section of `CLAUDE.md`). Each `sleep` gives the previous step time to take effect.

```
sleep 3000 [ mode 2 0; map bloodlust
  sleep 5000 [ spectate 0
    sleep 2000 [
      inputmouse 400 0 500                  // turn right over half a second
      sleep 1000 [ inputkey W 1             // start walking forward
        sleep 1000 [ inputkey W 0           // stop
          inputpress MOUSE1 400             // fire for 400ms
          sleep 1000 [ inputpress ESCAPE    // open the menu
            sleep 1000 [ inputcursor 0.834 0.665; inputpress MOUSE1  // click a menu item
              sleep 1000 [ screenshot; quit ]
] ] ] ] ] ] ]
```

To check the results, read back state such as `getclientyaw (getclientnum)`, `$cameraposx`, or `getclientweapclip (getclientnum) (getclientweapselect (getclientnum))` and `echo` it to the log. For menus, take a `screenshot` first to find where things are, then use those positions with `inputcursor`.

## Notes

- The player starts as a spectator. Use `spectate 0` and wait before sending any input meant for the player.
- `ESCAPE` closes an open console before it opens the menu, so allow for that after typing into the console.
- The menu cursor stays where `inputcursor` put it, which can also move the in-game crosshair. Set it back to `0.5 0.5` before testing aim.
