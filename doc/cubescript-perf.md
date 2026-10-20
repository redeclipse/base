# CubeScript Performance Measurement

How to measure what the CubeScript interpreter (`src/engine/command.cpp`) costs per frame: the `CSPERF` counters, the
benchmark scenes in `src/scripts/csperf/`, and the `perf stat` method used to decide whether a change is faster.

## Building with the counters

The counters are compiled in only when `CSPERF` is defined. Without it every counter macro is empty and the binary is
the same as one built without them. Only the client has them (`cube.h` drops `CSPERF` for `STANDALONE`), so build the
client alone:

```bash
make -C src clean
make -C src -j 8 install-client CXXFLAGS="-O3 -fomit-frame-pointer -ffast-math -fno-finite-math-only -DCSPERF"
```

The counters cost time themselves, so use this build to explain where time goes, never to decide whether a change is
faster. Run `make -C src clean` again before going back to a normal build, as the Makefile doesn't notice changed flags.

The commands it adds:

- `csperfreset` zeroes the counters and starts counting frames.
- `csperfreport <tag>` prints the counters, averaged per frame since the reset, as `CSPERF`, `CSIDENTS`, `CSLIST` and
  `CSOPS` lines in the console and `log.txt`.
- `csperfdump <file>` writes every identifier, sorted by name, to a file in the home directory, one per line: type,
  flags, name, then the value (variables), the value type and value (aliases) or the argument format (commands).
- `csperfcam` moves the player to the map's mapshot `CAMERA` entity, so the match scene has the same view every run.

## Running the scenes

```bash
src/scripts/csperf/drive.sh <checkout> <outdir> [mode]
src/scripts/csperf/stat.sh <outdir>
```

`drive.sh` starts the client built in `<checkout>` (through `.claude/re-test.sh` when it exists, so it reaches the
desktop session) with a new home in `<outdir>/home`, and runs `test.cfg`. That script waits for the game to load, then
runs four scenes of 12 seconds each: the main menu, Player Setup (`gameui_open ui_gameui_player`, with player previews),
the map browser (`gameui_open ui_gameui_maps`), and a deathmatch on bloodlust with bots, spectating from `csperfcam`. Each
scene echoes `CSPERF-START <scene>` when it begins, which `drive.sh` waits for before measuring 10 seconds of it. At the
end of each scene it echoes `CSFPS <scene> fps=<n>`, the average fps sampled every second, then in a `CSPERF` build it
calls `csperfreport` and `csperfdump csperf-<scene>.txt`.

The mode picks what `drive.sh` measures in each scene:

- **0**: nothing beyond the counters.
- **1**: `perf record` to `<scene>.data`. `PERFCG` sets the call graph option (default `-g`) and `PERFSEC` the length.
- **2** (the default): `perf stat` to `<scene>.stat`, which `stat.sh` turns into main-thread instructions and cycles per
  frame.

The client quits by itself at the end. `drive.sh` prints `EXIT: clean`, or `EXIT: the client did not quit by itself` and
kills it, followed by any `Fatal` or `double free` lines from the game's stdout. Run one client at a time; `drive.sh`
refuses to start while another is running.

The `perf record` data can be broken down with:

- `fold.py <scene.data>`: the share of main-thread samples in the interpreter's own functions (listed in `funcs.txt`),
  including the libc they call.
- `libc.py <scene.data> [rows]`: main-thread time in libc by family (alloc, strlen, strtod, strcmp and so on) and by the
  calling function. It resolves libc symbols through `debuginfod-find`.
- `who.py <scene.data> <function>`: where a function's samples land and who calls it.

Frame-pointer call graphs (`-g`) need `-fno-omit-frame-pointer` in `CXXFLAGS`, and still skip the direct caller of a leaf
libc function. Use `PERFCG="--call-graph dwarf,16384"` with a short `PERFSEC` when the exact caller matters.

## Reading the counters

All values are per frame.

- `CSPERF`:
  - `runms`, `compilems`: time inside top-level `runcode` calls and inside compiling. Wall-clock time, so only
    comparable within one run (see below).
  - `ops`: bytecode ops run. `CSOPS` lists each op seen at least 20 times per frame as `op:rettype=count`, the op and
    return type being `CODE_*` and `VAL_*` values.
  - `compiles`: calls to `compilemain`. `cachehit` and `cachemiss` count lookups in the compile cache.
  - `aliascall`, `aliascompile`: alias calls, and those that had to compile the alias first. `aliasdiscard` counts
    compiled alias code thrown away by `cleancode`, which happens whenever an alias is set.
  - `forcecode`: values compiled because a command wanted code but got a string.
  - `comcall`: calls of commands with a fixed format (`CODE_COM`); `callcmd`: calls through `callcommand`, which parses
    the format each time.
  - `strlookup`: identifiers looked up by name while running (`$[...]` and calls by a computed name).
  - `parselist`, `conc`: list elements parsed, and argument lists joined into a string.
  - `lockwalk`: steps through the locked string list when freeing an alias value.
  - `newstrs`: strings allocated by `newstring` while script runs; `newstrsall` counts every call.
  - `compqueued`, `compdone`: Composites queued, and drawn within the `compositemaxtime` budget.
- `CSIDENTS`: identifiers in `idents`, its buckets, the buckets in use and the longest chain, at the time of the report.
- `CSLIST`: calls of `listelem`, `at`, `listlen` and `floatstr`.

## Deciding whether a change is faster

Wall-clock time is no use on a desktop with a power-saving CPU governor: the same build has measured anywhere from 0.6 to
3.4 ms per frame in script. Count CPU cycles of the main thread instead, which `drive.sh` mode 2 does with
`perf stat --per-thread -e instructions:u,cycles:u`. `stat.sh` takes the thread with the most instructions and divides by
the frames: the `CSPERF` frame count when the build has it, otherwise the `CSFPS` average times 10 seconds. Cycles repeat
within about ±3% in the menu scenes and ±10% in the match, because of the bots. Instructions repeat within 0.2% but miss
memory stalls, so a change can cut instructions by 1% and cycles by 5–8%. `perf` needs `perf_event_paranoid` at 2 or
lower, which allows measuring your own processes in user space. The game is capped near 60 fps by vsync, so a gain shows
up as fewer cycles per frame, not more frames.

To compare a change against the code before it, build both in separate `git worktree`s. Worktrees have empty `data/*`
submodule directories, so replace each with a symlink into the main checkout's `data/`, and copy `.claude/re-test.sh`
in, as it starts the game from its own checkout. `make install-client` is enough.

A change is kept only when all of these hold:

1. **Faster**: built with the release flags from `src/Makefile` and without `CSPERF`, main-thread cycles per frame drop
   at least 3% in at least one menu scene, and rise beyond noise in none. Average at least two runs per build, run in
   turn (A, B, A, B).
2. **Same behaviour**: in `CSPERF` builds of both, the `csperf-<scene>.txt` dumps match. Values that change by
   themselves, like times and frame rates, differ between two runs of the same build too, so diff two runs of the old
   build first to see which lines those are.
3. **Clean exit**: `drive.sh` reports `EXIT: clean` with no `Fatal` or `double free` lines.
