# Radiance Hints Split Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop the radiance hints (RH) GI from rebuilding on zoom (fix 1) and from popping in whole-cell steps at the split boundary (fix 2, an opt-in crossfade).

**Architecture:** Fix 1 sizes the RH splits and the RSM from `max(curfov, basefov)`, where the game publishes its unzoomed fov as `basefov`. Fix 2 pulls each blending split's centre towards the camera with a per-axis clamp. The deferred light shader gets a new `h` variant that fades each split into the next coarser one over a band of `rhblend` cells inside its faces. Both are proved with a new editor-driven harness script (`tools/harness/gi.ps1`): a zoom test on a split-resize counter, and sweeps that read the real `getrhlight` at fixed world points through a DEBUG_UTILS `rhprobe` command (`tools/harness/probestats.py` analyses them).

**Tech Stack:** C++ (engine `src/engine/renderlights.cpp`, `rendergl.cpp`; game `src/game/game.cpp`), GLSL 1.20-compatible fragment code in `config/glsl/deferred/`, CubeScript (`config/glsl/deferred.cfg`, `config/usage.cfg`), PowerShell harness, Python 3 + Pillow (Windows `python`, already installed: Pillow 12.2.0).

**Spec:** [doc/superpowers/specs/2026-10-01-rh-split-stability-design.md](../specs/2026-10-01-rh-split-stability-design.md). Background and line references: [doc/gi-radiance-hints-findings.md](../../gi-radiance-hints-findings.md).

## Review decisions (2026-10-01) and deviations from the spec

| Topic | Decision |
|---|---|
| Default (spec open question 1) | **Off until tuned:** `rhblend 0`. `rhblend 0` gives today's shaders and placement exactly. |
| `B` / `M` (open question 2) | Configurable through `rhblend` (band, cells) and `rhblendmargin` (margin, cells). `rhblendmargin` defaults to 2; use `rhblend 2` when testing. |
| Zoom (open question 3) | Accept coarser GI while zoomed: `rhfov = max(curfov, basefov)`. |
| Coarse side of the blend (**deviation**) | The spec's `mix(hardlookup(fine+1), fetch(fine), w)` jumps for `rhsplits >= 3` when a point is in two bands at once. Instead the lookup accumulates front to back: each blending split `j` takes `w_j` of the weight still left, and the last split takes the rest with today's hard test. For `rhsplits 2` this is exactly the spec's formula. For any `N` it is continuous wherever today's lookup is. |
| Verification metric (**deviation**, user 2026-10-01) | The spec's screenshot sweeps can't work: animated models, foliage, exposure adaptation and parallax swamp the pop. Sweeps read the real `getrhlight` at fixed world points instead (DEBUG_UTILS `rhprobe`). The criteria become: today's lookup must show a jump `J >= 0.01`; the crossfade must cut it to `J_ref/4` or less; around the camera the values must agree to 2/255. |
| Fix 1 test trigger | A DEBUG_UTILS command `edzoom <fov>` overrides `curfov` in edit mode, standing in for a weapon zoom (there is no weapon in the editor). A real in-game zoom is a manual check (Task 4, step 6). |

## Global Constraints

- Build: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release`. Stop the harness first (`tools\harness\harness.ps1 stop`), since the running exe is locked.
- Run only `bin/amd64/redeclipse.exe` (the harness does this).
- GLSL: must compile as GLSL 1.20. No `##`, no sampler arrays, no line ending in a backslash (`doc/shader-reference.md`), no integer-only operators.
- With `rhblend 0` (the default) the composed deferred light shaders must be token-identical after preprocessing (`PASS-TEXT`) to the pre-change build.
- Test-only commands go under `#ifdef DEBUG_UTILS` and are refused when `identflags&IDF_MAP`.
- CubeScript: no bare `#`; avoid `@` in harness scripts; `exec "path" 0 0`.
- Commit messages: lowercase `area: summary`, ending with the `Co-Authored-By` line from the session. Stage explicit paths only (the worktree has unrelated changes: `readme.md`, `chat_wip.cfg`, ...). `doc/` is untracked and stays local, so don't commit it.
- Do not push.

## File map

| File | Change | Task |
|---|---|---|
| `src/engine/rendergl.cpp` | `basefov` global; `calcfrustumboundsphere(..., float fov = -1)` | 1 |
| `src/engine/engine.h` | `extern float basefov`; default argument | 1 |
| `src/game/game.cpp` | `fixview` sets `basefov`; DEBUG_UTILS `edzoom` | 1 |
| `src/engine/renderlights.cpp` | `rhboundsfov()`, `rhsplitresets` (task 1); `rhblend`, `rhblendmargin`, clamp, `blendcenter`, `rhblendtc`/`rhblendedge`, `h` letter (task 3) | 1, 3 |
| `tools/harness/gi.ps1` | New: `zoom` (task 1), `sweep` and `near` (task 2) | 1, 2 |
| `src/engine/renderlights.cpp` (task 2) | DEBUG_UTILS `rhprobe` | 2 |
| `tools/harness/probestats.py` | New: probe sweep statistics | 2 |
| `tools/harness/tests/test_probestats.py` | New: unit tests | 2 |
| `config/glsl/deferred.cfg` | `deferredlightcommon`, `rhprobeshader` (task 2); `h` → `DL_RHBLEND` (task 3) | 2, 3 |
| `config/glsl/deferred/rhprobe.vert` | New: probe vertex stage | 2 |
| `config/glsl/deferred/deferredlight_decls.glsl` | `rhblendtc`, `rhblendedge` uniforms | 3 |
| `config/glsl/deferred/deferredlight_defs.glsl` | `DL_RH_LAST`, `DL_RH_OFFSETLAST`, `DL_RH_BLEND` | 3 |
| `config/glsl/deferred/deferredlight.frag` | `DL_RHPROBE` main (task 2); `addrhsplit`, blended `getrhlight` path (task 3) | 2, 3 |
| `config/usage.cfg` | `rhblend`, `rhblendmargin` descriptions | 3 |
| `tools/harness/README.md`, `doc/shader-reference.md` | Docs | 5 |
| `CLAUDE.md`, `doc/agent-handoff.md`, `doc/gi-radiance-hints-findings.md`, spec | Local docs (untracked) | 5 |

---

### Task 0: Branch, build and record the shader baseline

The shader check needs a corpus recorded from the pre-change code on this machine. The repo's `baseline` corpus is from `f250748d` (before the shader ports), so it can't give `PASS-TEXT`.

**Files:** none changed.

- [ ] **Step 1: Branch from master**

The current checkout is `model-shader-port` (unmerged model port). This work doesn't depend on it.

```bash
git switch -c rh-split-stability master
```

Expected: `Switched to a new branch 'rh-split-stability'`. The untracked files and the `readme.md` change come along. Leave them alone.

- [ ] **Step 2: Build**

```bash
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release
```

Expected: the build ends without errors and `bin/amd64/redeclipse.exe` has a fresh timestamp.

- [ ] **Step 3: Record the pre-change corpus**

```powershell
tools\harness\shaders.ps1 record -Run rhb-base -NoMaps
```

Expected: completes with exit 0. `home\uitest\shadercorpus\rhb-base\manifest.tsv` contains `deferredlight*r2*` rows. Check with:

```powershell
Select-String -Path home\uitest\shadercorpus\rhb-base\manifest.tsv -Pattern 'deferredlight\S*r2' | Measure-Object
```

Expected: a count above 0.

---

### Task 1: Fov-independent split sizing (fix 1)

**Files:**
- Modify: `src/engine/rendergl.cpp:1386` (globals), `:1432-1452` (`calcfrustumboundsphere`)
- Modify: `src/engine/engine.h:303`, `:352`
- Modify: `src/game/game.cpp:3051-3059` (`fixview`)
- Modify: `src/engine/renderlights.cpp` (rh vars near `:1560`, `reflectiveshadowmap::getprojmatrix` `:2406`, `radiancehints::setup` `:2521-2545`)
- Create: `tools/harness/gi.ps1`

**Interfaces:**
- Produces: `extern float basefov;` (engine, set by the game each frame). `float calcfrustumboundsphere(float nearplane, float farplane, const vec &pos, const vec &view, vec &center, float fov = -1);`. `static inline float rhboundsfov();` in `renderlights.cpp`. CubeScript read-only int `rhsplitresets`. DEBUG_UTILS command `edzoom <fov>` (0 is off). Harness `gi.ps1 zoom` (exit 0 = PASS, 1 = FAIL).

- [ ] **Step 1: Add the diagnostic counter and the test hook (no fix yet)**

In `src/engine/renderlights.cpp`, after `FVARF(0, rhsplitweight, ...)` (`:1560`):

```cpp
// Diagnostic: how many times a split was resized or had its cache cleared
// (radiancehints::setup). Each one is a full rebuild of that split.
VAR(IDF_READONLY, rhsplitresets, 0, 0, INT_MAX);
```

In `radiancehints::setup`, immediately before `split.cached = split.bounds == pradius ? ...` (`:2538`):

```cpp
        if(split.bounds != pradius) rhsplitresets++;
```

In `src/engine/rendergl.cpp`, after the `float curfov = 100, fovy = 100, ...` line (`:1386`):

```cpp
float basefov = 0; // the view's unzoomed fov, set by the game (game::fixview); 0 until then
```

In `src/engine/engine.h:303` change

```cpp
extern float curfov, fovy, aspect, forceaspect;
```

to

```cpp
extern float curfov, basefov, fovy, aspect, forceaspect;
```

In `src/game/game.cpp`, replace `fixview` (`:3051-3059`) with:

```cpp
#ifdef DEBUG_UTILS
    // Test harness (tools/harness/gi.ps1): stands in for a weapon zoom in the
    // editor, which has no weapon, so the RH split sizing can be checked.
    // 0 is off. Edit mode only and refused to map scripts, like the editor
    // test commands in src/engine/world.cpp.
    float edzoomfov = 0;
    ICOMMAND(0, edzoom, "f", (float *fov), { if(identflags&IDF_MAP) return; edzoomfov = max(*fov, 0.0f); });
#endif

    void fixview()
    {
        basefov = float(fov());
        if(inzoom())
        {
            checkzoom();
            curfov = fov()-(zoomscale()*(fov()-(W(focus->weapselect, cookzoommax)-((W(focus->weapselect, cookzoommax)-W(focus->weapselect, cookzoommin))/float(zoomlevels)*zoomlevel))));
        }
        else curfov = float(fov());
#ifdef DEBUG_UTILS
        if(edzoomfov > 0 && player1->isediting()) curfov = edzoomfov;
#endif
    }
```

`basefov` is set here already, but nothing reads it yet, so the counter shows today's behaviour.

- [ ] **Step 2: Write the harness test**

Create `tools/harness/gi.ps1`:

```powershell
<#
.SYNOPSIS
    GI (radiance hints) stability checks, driven through the map editor.

.DESCRIPTION
    Needs a running harness client in edit mode on a map with sunlight and GI:
        tools\harness\harness.ps1 start -Width 1600 -Height 900
        tools\harness\editor.ps1 open park

    zoom    Animates edzoom (a DEBUG_UTILS stand-in for a weapon zoom) from
            $editfov down to -Fov and back over -Frames frames each way, and
            counts RH split resizes ($rhsplitresets). PASS when there are none.

    The view is made deterministic first: HUD, editor cursor, outlines and
    entity markers off, giscale raised to -GiScale so GI changes stand out.

.EXAMPLE
    tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('zoom')]
    [string]$Command = 'zoom',

    [double]$Fov = 30,
    [int]$Frames = 20,
    [double]$GiScale = 8,
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')

# ------------------------------------------------------------- helpers ----

# Evaluates a CubeScript expression and returns its value as a string.
function Get-Value([string]$Expr) {
    $lines = Invoke-Batch "echo (concatword ""GIVALUE="" $Expr)" 1 $TimeoutSec
    $value = $null
    foreach ($line in $lines) { if ($line -match 'GIVALUE=(\S*)') { $value = $Matches[1] } }
    if ($null -eq $value) { throw "No value for '$Expr'. Output:`n$($lines -join "`n")" }
    return $value
}

function Set-GiView {
    $lines = Invoke-Batch "showhud 0; editinhibit 1; outline 0; entediting 0; giscale $(Format-Coord $GiScale)" 200 $TimeoutSec
    Show-BatchResult $lines
    if ((Get-Value '$editing') -ne '1') { throw 'Not in edit mode: run tools\harness\editor.ps1 open <map> first.' }
}

# ------------------------------------------------------------ commands ----

switch ($Command) {

    'zoom' {
        Set-GiView
        $base = ConvertTo-InvariantDouble (Get-Value '$editfov')
        $before = [int](Get-Value '$rhsplitresets')

        $fovs = @()
        for ($i = 1; $i -le $Frames; $i++) { $fovs += $base + ($Fov - $base) * $i / $Frames }
        for ($i = $Frames - 1; $i -ge 0; $i--) { $fovs += $base + ($Fov - $base) * $i / $Frames }
        # One batch per step; the settle makes sure a frame renders at each fov.
        foreach ($f in $fovs) { Invoke-Batch "edzoom $(Format-Coord $f)" 50 $TimeoutSec | Out-Null }
        Invoke-Batch 'edzoom 0' 100 $TimeoutSec | Out-Null

        $after = [int](Get-Value '$rhsplitresets')
        $delta = $after - $before
        Write-Output ("ZOOM base={0} min={1} frames={2} rhsplitresets {3} -> {4} (+{5})" -f `
            (Format-Coord $base), (Format-Coord $Fov), $fovs.Count, $before, $after, $delta)
        if ($delta -eq 0) { Write-Output 'PASS'; exit 0 }
        Write-Output 'FAIL'; exit 1
    }
}
```

`$editing` is the engine's edit-mode variable (`VAR(0, editing, 1, 0, 0)`, `src/engine/octaedit.cpp:173`).

- [ ] **Step 3: Build and run the test to see it fail**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
```

Expected: `ZOOM ... rhsplitresets a -> b (+n)` with `n > 0` (every split resized at most fov steps), then `FAIL`, exit 1. If it says `+0`, the hook isn't reaching `curfov`. Stop and check that `fixview` runs in edit mode (`game::recomputecamera`, `game.cpp:4092`) before going on.

- [ ] **Step 4: Implement fix 1**

In `src/engine/rendergl.cpp`, change `calcfrustumboundsphere` (`:1432`) to take the fov:

```cpp
float calcfrustumboundsphere(float nearplane, float farplane, const vec &pos, const vec &view, vec &center, float fov)
{
    if(drawtex == DRAWTEX_MINIMAP)
    {
        center = minimapcenter;
        return minimapradius.magnitude();
    }

    float width = tan((fov < 0 ? curfov : fov)/2.0f*RAD), height = width / aspect,
```

(The rest of the function is unchanged.)

In `src/engine/engine.h:352`:

```cpp
extern float calcfrustumboundsphere(float nearplane, float farplane, const vec &pos, const vec &view, vec &center, float fov = -1);
```

In `src/engine/renderlights.cpp`, after the `rhsplitresets` var from step 1:

```cpp
// The RH splits and the RSM are sized from the unzoomed fov, so a zoom
// doesn't resize them and throw the cache away. max() keeps the bounds
// around the frustum when an effect widens curfov past basefov (the
// map-start reveal in src/game/hud.cpp).
static inline float rhboundsfov() { return basefov > 0 ? max(curfov, basefov) : curfov; }
```

In `reflectiveshadowmap::getprojmatrix` (`:2406`):

```cpp
    float radius = calcfrustumboundsphere(getrhnearplane(), getrhfarplane(), camera1->o, camdir, c, rhboundsfov());
```

In `radiancehints::setup` (`:2530`):

```cpp
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c, rhboundsfov());
```

The CSM caller (`:2246`) keeps `curfov`.

- [ ] **Step 5: Build and run the test to see it pass**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
```

Expected: `(+0)`, `PASS`, exit 0.

Also check that widening still resizes, because `max()` must let `curfov` win:

```powershell
tools\harness\gi.ps1 zoom -Fov 130 -Frames 5
```

Expected: `+n` with `n > 0`, `FAIL`. That is correct here, since a wider view must grow the splits.

- [ ] **Step 6: Commit**

```bash
git add src/engine/rendergl.cpp src/engine/engine.h src/game/game.cpp src/engine/renderlights.cpp tools/harness/gi.ps1
git commit -m "renderlights: size the rh splits from the unzoomed fov"
```

---

### Task 2: GI probe and probe sweeps (must detect today's pop)

Screenshot differences can't see the pop: animated models, shimmering foliage, exposure adaptation and parallax swamp it (the first attempt measured a +0.1..0.4 pop under ≥1.4 of parallax). This task measures the GI term itself instead. A DEBUG_UTILS command `rhprobe` runs `getrhlight`, the real lookup from `deferredlight.frag`, at fixed world points through a small shader variant and writes the values to a file. Sweeps then move the camera and watch the same world points: a pop is a jump at a fixed point.

**Files:**
- Modify: `config/glsl/deferred.cfg` (`deferredlightvariantshader`, `:48-82`; new `deferredlightcommon`, `rhprobeshader`)
- Modify: `config/glsl/deferred/deferredlight.frag` (`main`, `:89-329`)
- Create: `config/glsl/deferred/rhprobe.vert`
- Modify: `src/engine/renderlights.cpp` (DEBUG_UTILS `rhprobe`, after `useradiancehints()`, `:2564`)
- Modify: `tools/harness/gi.ps1` (add `sweep` and `near`)
- Create: `tools/harness/probestats.py`
- Create: `tools/harness/tests/test_probestats.py`

**Interfaces:**
- Consumes: `gi.ps1` helpers `Get-Value`, `Set-GiView` (task 1); `core.ps1` `Invoke-Batch`, `Format-Coord`, `$HomeDir`, `$CmdDir`.
- Produces:
  - CubeScript `deferredlightcommon <splits> <rh> <lights>` (reads `$deferredlighttype`), `rhprobeshader <rh> <sunopts>` (shader name `rhprobe<sunopts>`, e.g. `rhprober2`).
  - GLSL define `DL_RHPROBE` (replaces `main` in `deferredlight.frag`).
  - DEBUG_UTILS command `rhprobe <pointsfile> <outfile>` (home-relative paths), returning the number of points probed or -1. The output file has one `RHSPLIT <split> <cx> <cy> <cz> <half>` line per split, then one `RHPROBE <point> <r> <g> <b> <a>` line per point. Task 3 adds `h` to `<sunopts>` when blending.
  - `gi.ps1 sweep -Kind translate|rotate -X -Y -Z -Yaw -Steps -Step -Blend -Extent -Spacing -Name` and `gi.ps1 near -X -Y -Z -Yaw -Blend -Radius -Name`.
  - `probestats.py sweep <points> <grid> <ref.tsv> [<cand.tsv>]` and `probestats.py near <points> <radius> <x> <y> <z> <ref.tsv> <cand.tsv>`. Thresholds: `DETECT = 0.01`, `RATIO = 4`, `NEAR = 2/255`.

**Run TSV format** (written by gi.ps1, read by probestats.py), tab-separated:
`S <step> <split> <cx> <cy> <cz> <half>` and `P <step> <point> <r> <g> <b> <a>`.
**Points file** (written by gi.ps1, read by the engine and probestats.py): one `x y z nx ny nz` line per point; a point's index is its line number from 0.

- [ ] **Step 1: Write the failing unit tests for `probestats.py`**

Create `tools/harness/tests/test_probestats.py`:

```python
"""Tests for tools/harness/probestats.py.

Run: python -m unittest discover -s tools/harness/tests -p "test_probestats.py" -v
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import probestats  # noqa: E402

# One last split (index 1) centred on the origin, half-size 270, grid 27:
# cells are 20 units, so the excluded margin is 30 units inside each face.
SPLITS = [(0, 0.0, 0.0, 0.0, 100.0), (1, 0.0, 0.0, 0.0, 270.0)]


class ProbeStatsTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, name, lines):
        path = os.path.join(self.dir, name)
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")
        return path

    def points(self, pts):
        return self.write("points.txt", ["%g %g %g 0 0 1" % p for p in pts])

    def run(self, name, steps):
        """steps: list of per-step lists of (r, g, b, a), one per point."""
        lines = []
        for s, values in enumerate(steps):
            for split in SPLITS:
                lines.append("S\t%d\t%d\t%g\t%g\t%g\t%g" % ((s,) + split))
            for p, v in enumerate(values):
                lines.append("P\t%d\t%d\t%g\t%g\t%g\t%g" % ((s, p) + v))
        return self.write(name, lines)

    def test_jump_is_largest_component_change(self):
        pts = self.points([(0, 0, 0), (10, 0, 0)])
        run = self.run("ref.tsv", [[(0.1, 0, 0, 0), (0.2, 0, 0, 0)],
                                   [(0.1, 0, 0, 0), (0.2, 0.05, 0, 0)],
                                   [(0.13, 0, 0, 0), (0.2, 0.05, 0, 0)]])
        j, step, point, kept = probestats.jumps(probestats.read_points(pts), 27, probestats.read_run(run))
        self.assertAlmostEqual(j, 0.05)
        self.assertEqual((step, point, kept), (0, 1, 4))

    def test_points_near_last_split_faces_are_excluded(self):
        # 250 is within 30 of the face at 270; 300 is outside the split.
        pts = self.points([(0, 0, 0), (250, 0, 0), (0, 300, 0)])
        run = self.run("ref.tsv", [[(0, 0, 0, 0)] * 3,
                                   [(0, 0, 0, 0), (0.9, 0, 0, 0), (0.9, 0, 0, 0)]])
        j, step, point, kept = probestats.jumps(probestats.read_points(pts), 27, probestats.read_run(run))
        self.assertEqual((j, kept), (0.0, 1))

    def test_sweep_detects_and_passes(self):
        pts = self.points([(0, 0, 0)])
        ref = self.run("ref.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.2, 0, 0, 0)]])
        smooth = self.run("smooth.tsv", [[(0.1, 0, 0, 0)], [(0.12, 0, 0, 0)], [(0.14, 0, 0, 0)]])
        steep = self.run("steep.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.15, 0, 0, 0)]])
        flat = self.run("flat.tsv", [[(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)], [(0.1, 0, 0, 0)]])
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref]), 0)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", flat]), 1)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref, smooth]), 0)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", ref, steep]), 1)
        self.assertEqual(probestats.main(["probestats.py", "sweep", pts, "27", flat, smooth]), 1)

    def test_near_compares_only_points_inside_radius(self):
        pts = self.points([(0, 0, 0), (5, 0, 0), (50, 0, 0)])
        ref = self.run("ref.tsv", [[(0.1, 0, 0, 0), (0.1, 0, 0, 0), (0.1, 0, 0, 0)]])
        same = self.run("same.tsv", [[(0.1, 0, 0, 0), (0.1 + 1 / 255.0, 0, 0, 0), (0.5, 0, 0, 0)]])
        moved = self.run("moved.tsv", [[(0.1, 0, 0, 0), (0.2, 0, 0, 0), (0.1, 0, 0, 0)]])
        self.assertEqual(probestats.main(["probestats.py", "near", pts, "10", "0", "0", "0", ref, same]), 0)
        self.assertEqual(probestats.main(["probestats.py", "near", pts, "10", "0", "0", "0", ref, moved]), 1)

    def test_bad_usage(self):
        self.assertEqual(probestats.main(["probestats.py"]), 2)
        self.assertEqual(probestats.main(["probestats.py", "sweep", "p.txt"]), 2)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run them to see them fail**

Run: `python -m unittest discover -s tools/harness/tests -p "test_probestats.py" -v`
Expected: ERROR, `ModuleNotFoundError: No module named 'probestats'`.

- [ ] **Step 3: Implement `probestats.py`**

Create `tools/harness/probestats.py`:

```python
"""Statistics over the rhprobe sweeps of tools/harness/gi.ps1.

  python probestats.py sweep <points> <grid> <ref.tsv> [<cand.tsv>]
      For each run, J: the largest change of any probe value (r, g, b or a)
      between consecutive steps. Probes outside the last split, or within 1.5
      of its cells of its faces, at either step are left out: the last split
      keeps today's hard edge, which this work doesn't change. With only
      <ref.tsv>: DETECTED (exit 0) when J >= DETECT, else NOT DETECTED
      (exit 1). With <cand.tsv>: PASS (exit 0) when J_ref >= DETECT and
      J_cand <= J_ref / RATIO, else FAIL (exit 1).
  python probestats.py near <points> <radius> <x> <y> <z> <ref.tsv> <cand.tsv>
      The largest difference between the two runs' step-0 values over the
      probes within <radius> of (x, y, z). PASS (exit 0) when <= NEAR.

<points>: one "x y z nx ny nz" line per probe, indexed from 0. Run files
are tab-separated "S step split cx cy cz half" and "P step point r g b a".
"""
import sys

DETECT = 0.01
RATIO = 4.0
NEAR = 2.0 / 255.0


def read_points(path):
    with open(path) as f:
        return [tuple(float(v) for v in line.split()[:3]) for line in f if line.strip()]


def read_run(path):
    """Returns (splits, probes): splits[step][split] = (cx, cy, cz, half),
    probes[step][point] = (r, g, b, a)."""
    splits, probes = {}, {}
    with open(path) as f:
        for line in f:
            fields = line.split()
            if not fields:
                continue
            step, index = int(fields[1]), int(fields[2])
            values = tuple(float(v) for v in fields[3:7])
            if fields[0] == "S":
                splits.setdefault(step, {})[index] = values
            elif fields[0] == "P":
                probes.setdefault(step, {})[index] = values
    return splits, probes


def near_last_face(point, split, grid):
    cx, cy, cz, half = split
    margin = 1.5 * 2.0 * half / grid
    return any(abs(p - c) > half - margin for p, c in zip(point, (cx, cy, cz)))


def jumps(points, grid, run):
    """Returns (J, step, point, kept): the largest change between step and
    step + 1, where it happened, and how many probe-steps were compared."""
    splits, probes = run
    steps = sorted(probes)
    best, where, kept = 0.0, (None, None), 0
    for s, t in zip(steps, steps[1:]):
        lasts = splits[s][max(splits[s])], splits[t][max(splits[t])]
        for p, a in probes[s].items():
            b = probes[t].get(p)
            if b is None or any(near_last_face(points[p], last, grid) for last in lasts):
                continue
            kept += 1
            d = max(abs(x - y) for x, y in zip(a, b))
            if d > best:
                best, where = d, (s, p)
    return best, where[0], where[1], kept


def report(label, result):
    j, step, point, kept = result
    print("%s J %.6f at step %s point %s (%d probe-steps)" % (label, j, step, point, kept))
    return j


def main(argv):
    if len(argv) in (5, 6) and argv[1] == "sweep":
        points, grid = read_points(argv[2]), float(argv[3])
        ref = report("REF", jumps(points, grid, read_run(argv[4])))
        if len(argv) == 5:
            ok = ref >= DETECT
            print("DETECTED" if ok else "NOT DETECTED")
            return 0 if ok else 1
        cand = report("CAND", jumps(points, grid, read_run(argv[5])))
        ok = ref >= DETECT and cand <= ref / RATIO
        print("RATIO %.3f" % (cand / ref if ref > 0 else float("inf")))
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1
    if len(argv) == 9 and argv[1] == "near":
        points, radius = read_points(argv[2]), float(argv[3])
        centre = tuple(float(v) for v in argv[4:7])
        ref, cand = read_run(argv[7])[1][0], read_run(argv[8])[1][0]
        diff, count = 0.0, 0
        for p, a in ref.items():
            if sum((x - c) ** 2 for x, c in zip(points[p], centre)) > radius * radius or p not in cand:
                continue
            count += 1
            diff = max(diff, max(abs(x - y) for x, y in zip(a, cand[p])))
        print("NEAR %.6f over %d probes (limit %.6f)" % (diff, count, NEAR))
        ok = count > 0 and diff <= NEAR
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

- [ ] **Step 4: Run the unit tests to see them pass**

Run: `python -m unittest discover -s tools/harness/tests -p "test_probestats.py" -v`
Expected: 5 tests, `OK`.

- [ ] **Step 5: Share the deferred light defines**

In `config/glsl/deferred.cfg`, move the body of `deferredlightvariantshader` into a new alias so the probe shader builds exactly the same defines and includes. The result must be:

```
// deferredlightcommon <splits> <rh> <lights>: the defines and includes every
// deferredlight shader shares, for the type in $deferredlighttype. Called
// inside a shader body by deferredlightvariantshader and rhprobeshader.
deferredlightcommon = [
    shader_define DL_NUMSPLITS (+ $arg1 0)
    shader_define DL_NUMRH (+ $arg2 0)
    shader_define DL_NUMLIGHTS (+ $arg3 0)
    (every line from the current `if (dlopt "p") ...` through
     `shader_include_fs "config/glsl/shared/smfilter.glsl"`, unchanged and in order)
]

// deferredlightvariantshader <name> <row> <type> <splits> <rh> <lights> <maxvariants>
deferredlightvariantshader = [
    local deferredlighttype
    deferredlighttype = $arg3
    variantshader_new $SHADER_DEFAULT $arg1 $arg2 $arg7 [
        shader_define DL_ROW $arg2
        deferredlightcommon $arg4 $arg5 $arg6
        // Only the parent has a vertex stage; the rows reuse it.
        shader_source (? (< $arg2 0) "config/glsl/deferred/deferredlight.vert") "config/glsl/deferred/deferredlight.frag"
    ]
]
```

(Write the moved lines out in full; the parenthesised line above only stands for them.) The defines are emitted in the same order as before, so the composed shaders must not change. Then add, after `deferredlightvariantshader`:

```
// rhprobeshader <rh> <sunopts>, from rhprobe (DEBUG_UTILS, renderlights.cpp):
// getrhlight from deferredlight.frag at the points rhprobe.vert passes in.
// <sunopts> is the radiance hint part of the sun type: "r2", or "r2h" with
// split blending.
rhprobeshader = [
    local deferredlighttype
    deferredlighttype = (concatword "c1" $arg2)
    shader_new $SHADER_DEFAULT (concatword "rhprobe" $arg2) [
        shader_define DL_ROW 0
        shader_define DL_RHPROBE ""
        deferredlightcommon 1 $arg1 0
        shader_source "config/glsl/deferred/rhprobe.vert" "config/glsl/deferred/deferredlight.frag"
    ]
]
```

- [ ] **Step 6: The probe stages**

Create `config/glsl/deferred/rhprobe.vert`:

```glsl
// rhprobe (DEBUG_UTILS, renderlights.cpp): one point per probe, drawn at its
// own pixel of the readback target, carrying its world position and normal
// to the DL_RHPROBE main in deferredlight.frag.
attribute vec4 vvertex;
attribute vec3 vtexcoord0, vnormal;
varying vec3 probepos, probenorm;
void main(void)
{
    gl_Position = vvertex;
    probepos = vtexcoord0;
    probenorm = vnormal;
}
```

In `config/glsl/deferred/deferredlight.frag`, add `DL_RHPROBE` to the header comment's define list (after `AVATAR_SHADOW_DIST.`, a line `//   DL_RHPROBE replaces main with the rhprobe test output (rhprobeshader).`). Directly before `void main(void)` (`:89`) insert:

```glsl
#ifdef DL_RHPROBE
// rhprobe (DEBUG_UTILS, renderlights.cpp): the radiance hint light at the
// point and normal rhprobe.vert passes in, one point per pixel.
varying vec3 probepos, probenorm;
void main(void)
{
    fragcolor = getrhlight(probepos, probenorm);
}
#else
```

and after the closing `}` of the existing `main` (the last line, `:329`) add `#endif`. Nothing inside the existing `main` changes, so the deferredlight shaders keep the same tokens.

- [ ] **Step 7: The `rhprobe` command**

In `src/engine/renderlights.cpp`, after `useradiancehints()` (`:2564`), add:

```cpp
#ifdef DEBUG_UTILS
// Test harness (tools/harness/gi.ps1): runs getrhlight, the deferred lighting's
// radiance hint lookup, at world points and writes the results. <pointsfile>
// holds one "x y z nx ny nz" line per point. <outfile> gets one
// "RHSPLIT <split> <cx> <cy> <cz> <half>" line per split, as placed for the
// last rendered frame, then one "RHPROBE <point> <r> <g> <b> <a>" line per
// point. Both are home-relative. Returns the number of points, or -1.
static int rhprobe(const char *pointsfile, const char *outfile)
{
    if(!useradiancehints() || rhrect || rh.splits[0].bounds <= 0) return -1;
    char *buf = loadfile(pointsfile, NULL);
    if(!buf) return -1;
    vector<vec> pos, norm;
    for(const char *s = buf;;)
    {
        vec p, n;
        int len = 0;
        if(sscanf(s, " %f %f %f %f %f %f%n", &p.x, &p.y, &p.z, &n.x, &n.y, &n.z, &len) != 6) break;
        pos.add(p);
        norm.add(n.normalize());
        s += len;
    }
    delete[] buf;
    int n = pos.length();
    if(!n || n > hwtexsize) return -1;

    defformatstring(opts, "r%d", rhsplits);
    defformatstring(name, "rhprobe%s", opts);
    Shader *probeshader = generateshader(name, "rhprobeshader %d \"%s\"", rhsplits, opts);
    if(!probeshader) return -1;

    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    createtexture(tex, n, 1, NULL, 3, 0, GL_RGBA32F, GL_TEXTURE_RECTANGLE);
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex, 0);
    glViewport(0, 0, n, 1);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    loopi(4)
    {
        glActiveTexture_(GL_TEXTURE6 + i);
        glBindTexture(GL_TEXTURE_3D, rhtex[i]);
    }
    glActiveTexture_(GL_TEXTURE0);
    rh.bindparams();
    probeshader->set();
    gle::defvertex(2);
    gle::deftexcoord0(3);
    gle::defnormal(3);
    gle::begin(GL_POINTS);
    loopi(n)
    {
        gle::attribf(2*(i + 0.5f)/n - 1, 0);
        gle::attrib(pos[i]);
        gle::attrib(norm[i]);
    }
    gle::end();
    vector<vec4> result;
    result.growbuf(n);
    glReadPixels(0, 0, n, 1, GL_RGBA, GL_FLOAT, result.getbuf());
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);
    glDeleteFramebuffers_(1, &fbo);
    glDeleteTextures(1, &tex);

    stream *f = openutf8file(outfile, "w");
    if(!f) return -1;
    loopi(rhsplits) f->printf("RHSPLIT %d %.4f %.4f %.4f %.4f\n", i, rh.splits[i].center.x, rh.splits[i].center.y, rh.splits[i].center.z, rh.splits[i].bounds);
    loopi(n) f->printf("RHPROBE %d %.6f %.6f %.6f %.6f\n", i, result.getbuf()[i].x, result.getbuf()[i].y, result.getbuf()[i].z, result.getbuf()[i].w);
    delete f;
    return n;
}
ICOMMAND(0, rhprobe, "ss", (char *pointsfile, char *outfile),
{
    if(identflags&IDF_MAP) return;
    intret(rhprobe(pointsfile, outfile));
});
#endif
```

Check each API against the tree before building, and adapt only the call, never the behaviour. Check `hwtexsize`, `createtexture`'s parameters (`engine.h:236`), `generateshader`'s format form (`renderlights.cpp:1446`), `stream::printf`, and `vector::growbuf`/`getbuf` (`src/shared/tools.h`). Note each adaptation in the report.

- [ ] **Step 8: Probe sweeps in `gi.ps1`**

Change the `ValidateSet` to `'zoom', 'sweep', 'near'` and add the parameters after `$Frames`:

```powershell
    [ValidateSet('translate', 'rotate')]
    [string]$Kind = 'translate',
    [double]$X,
    [double]$Y,
    [double]$Z,
    [double]$Yaw = 0,
    [int]$Steps = 72,
    [double]$Step = 1,
    [double]$Blend = 0,
    [double]$Extent = -1,
    [double]$Spacing = -1,
    [double]$Radius = 30,
    [string]$Name = 'gi',
```

Add to the help text:

```
    sweep   Probes a lattice of world points (rhprobe: the real getrhlight)
            while the camera moves -Step units per step along its facing
            (-Kind translate; choose -Yaw 0/90/180/270) or turns -Step
            degrees (-Kind rotate). Always runs once at rhblend 0; with
            -Blend > 0, again at that rhblend. probestats.py then reports J,
            the largest per-step change at any fixed point. Reference only:
            DETECTED when today's lookup pops. With -Blend: PASS when the
            crossfade cuts J to a quarter or less.
    near    Probes a fine lattice around the camera at rhblend 0 and at
            -Blend. PASS when the points within -Radius agree to 2/255.

    Lattice: points every -Spacing units over +-Extent around (-X, -Y) at
    heights -Z - Spacing, -Z and -Z + Spacing, normal up. Defaults: sweep
    600/40, near 40/8. Files go to home\uitest\harness\ (rhprobe-points.txt,
    <name>_<kind>_b<blend>.tsv).
```

Add below `Set-GiView` (script level):

```powershell
$ProbeStats = Join-Path $PSScriptRoot 'probestats.py'
$PointsRel  = 'harness/rhprobe-points.txt'
$OutRel     = 'harness/rhprobe-out.txt'

$ScriptArgs = $PSBoundParameters
function Assert-Pose {
    foreach ($p in 'X', 'Y', 'Z') {
        if (-not $ScriptArgs.ContainsKey($p)) {
            throw "-$p is required (read a start pose with tools\harness\editor.ps1 state)."
        }
    }
}

function Set-Blend([double]$Value) {
    $exists = Get-Value '(identexists rhblend)'
    if ($exists -ne '1') {
        if ($Value -gt 0) { throw 'rhblend does not exist in this build.' }
        return
    }
    Show-BatchResult (Invoke-Batch "rhblend $(Format-Coord $Value)" 300 $TimeoutSec)
}

function Write-Lattice([double]$Ext, [double]$Gap) {
    $lines = New-Object System.Collections.Generic.List[string]
    $n = [int][Math]::Floor($Ext / $Gap)
    foreach ($k in -1, 0, 1) {
        for ($i = -$n; $i -le $n; $i++) {
            for ($j = -$n; $j -le $n; $j++) {
                $lines.Add(('{0} {1} {2} 0 0 1' -f (Format-Coord ($X + $i * $Gap)), (Format-Coord ($Y + $j * $Gap)),
                    (Format-Coord ($Z + $k * $Gap))))
            }
        }
    }
    $path = Join-Path $HomeDir $PointsRel
    Write-TextNoBom $path (($lines -join "`n") + "`n")
    return $path
}

# Places the camera, lets a frame render, probes, and appends the step's
# S and P lines to $Tsv.
function Invoke-ProbeStep([int]$Index, [double]$PX, [double]$PY, [double]$PZ, [double]$PYaw, [string]$Tsv, [int]$Count) {
    Invoke-Batch ('edgoto {0} {1} {2}; edaim {3} 0' -f (Format-Coord $PX), (Format-Coord $PY), (Format-Coord $PZ),
        (Format-Coord $PYaw)) 100 $TimeoutSec | Out-Null
    $got = Get-Value "(rhprobe ""$PointsRel"" ""$OutRel"")"
    if ([int]$got -ne $Count) { throw "rhprobe returned $got, expected $Count (GI off, or not rendered yet?)" }
    $out = Get-Content (Join-Path $HomeDir $OutRel)
    $rows = foreach ($line in $out) {
        $f = $line -split ' '
        if ($f[0] -eq 'RHSPLIT') { "S`t$Index`t" + ($f[1..5] -join "`t") }
        elseif ($f[0] -eq 'RHPROBE') { "P`t$Index`t" + ($f[1..5] -join "`t") }
    }
    Add-Content -Path $Tsv -Value $rows -Encoding ascii
}

function Invoke-ProbeRun([double]$B, [string]$Tag, [int]$Count, [scriptblock]$PoseAt, [int]$StepCount) {
    Set-Blend $B
    $tsv = Join-Path $CmdDir ('{0}_{1}_b{2}.tsv' -f $Name, $Tag, (Format-Coord $B))
    Remove-Item $tsv -Force -ErrorAction SilentlyContinue
    for ($i = 0; $i -le $StepCount; $i++) {
        $p = & $PoseAt $i
        Invoke-ProbeStep $i $p[0] $p[1] $p[2] $p[3] $tsv $Count
    }
    return $tsv
}
```

Add the commands inside the `switch`:

```powershell
    'sweep' {
        Assert-Pose
        Set-GiView
        $ext = if ($Extent -gt 0) { $Extent } else { 600 }
        $gap = if ($Spacing -gt 0) { $Spacing } else { 40 }
        $points = Write-Lattice $ext $gap
        $count = @(Get-Content $points).Count
        $grid = Get-Value '$rhgrid'
        # The engine's facing (vec(yaw, 0), src/shared/geom.h): x = -sin(yaw), y = cos(yaw).
        $rad = [Math]::PI / 180
        $dx = -[Math]::Sin($Yaw * $rad)
        $dy = [Math]::Cos($Yaw * $rad)
        $poseAt = if ($Kind -eq 'translate') {
            { param($i) @(($X + $dx * $i * $Step), ($Y + $dy * $i * $Step), $Z, $Yaw) }
        } else {
            { param($i) @($X, $Y, $Z, ($Yaw + $i * $Step)) }
        }
        $ref = Invoke-ProbeRun 0 $Kind $count $poseAt $Steps
        $runs = @($ref)
        if ($Blend -gt 0) { $runs += Invoke-ProbeRun $Blend $Kind $count $poseAt $Steps; Set-Blend 0 }
        & python $ProbeStats sweep $points $grid @runs
        exit $LASTEXITCODE
    }

    'near' {
        Assert-Pose
        Set-GiView
        $ext = if ($Extent -gt 0) { $Extent } else { 40 }
        $gap = if ($Spacing -gt 0) { $Spacing } else { 8 }
        $points = Write-Lattice $ext $gap
        $count = @(Get-Content $points).Count
        $poseAt = { param($i) @($X, $Y, $Z, $Yaw) }
        $ref = Invoke-ProbeRun 0 'near' $count $poseAt 0
        $cand = Invoke-ProbeRun $Blend 'near' $count $poseAt 0
        Set-Blend 0
        & python $ProbeStats near $points (Format-Coord $Radius) (Format-Coord $X) (Format-Coord $Y) (Format-Coord $Z) $ref $cand
        exit $LASTEXITCODE
    }
```

The pose scriptblocks read `$X`, `$Y`, `$dx`... from the script scope when invoked, which is intended. `Write-TextNoBom` and `$HomeDir`/`$CmdDir` come from `core.ps1`.

- [ ] **Step 9: Build and check the deferred shaders are unchanged**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release
tools\harness\shaders.ps1 check -Run rhb-base -NoMaps -Sids s00 -Filter 'deferredlight*'
```

Expected: every row `PASS-TEXT`, exit 0. Task 4 repeats this over every settings point. Any other status means the `deferredlightcommon` refactor or the `main` wrapper changed the composed text. Find out why with `tools\harness\shaders.ps1 diff <name> -Sid s00 -Run rhb-base` before going on.

- [ ] **Step 10: Pick the pose and check the probe is deterministic**

```powershell
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\editor.ps1 state
```

Take `cam (x, y, z)` as the start pose and round the yaw to the nearest of 0/90/180/270. Any pose with the sun and GI on works: animation and parallax don't matter now. Then probe the same pose twice:

```powershell
tools\harness\gi.ps1 sweep -Kind translate -X <x> -Y <y> -Z <z> -Yaw <yaw> -Steps 1 -Step 0 -Name det
```

Expected: `REF J 0.000000 ...` and `NOT DETECTED` (exit 1). Two probes at the same pose must match exactly. If `harness.ps1 log` shows `GLSL ERROR` for `rhprobe*`, fix the shader. A nonzero J here means the probe isn't deterministic: stop and report it. Write the pose into this plan's Results.

- [ ] **Step 11: Run the reference sweeps and check they detect the pop**

```powershell
tools\harness\gi.ps1 sweep -Kind translate -X <x> -Y <y> -Z <z> -Yaw <yaw> -Steps 72 -Step 1 -Name t
tools\harness\gi.ps1 sweep -Kind rotate    -X <x> -Y <y> -Z <z> -Yaw <yaw> -Steps 180 -Step 1 -Name r
```

Expected: both `DETECTED` (exit 0), with J ≥ 0.01. Record J, its step and point, and the probe-step count in Results. Check the point against the `S` lines of the TSV at that step: it should be a probe that crosses split 0's face between the two steps.

If either says `NOT DETECTED`, don't change the thresholds. Report J and the 10 largest per-step changes (from the TSV) as BLOCKED.

- [ ] **Step 12: Commit**

```bash
git add config/glsl/deferred.cfg config/glsl/deferred/deferredlight.frag config/glsl/deferred/rhprobe.vert src/engine/renderlights.cpp tools/harness/gi.ps1 tools/harness/probestats.py tools/harness/tests/test_probestats.py
git commit -m "harness: probe the rh lighting at fixed points"
```

---

### Task 3: Cross-split crossfade (fix 2)

**Files:**
- Modify: `src/engine/renderlights.cpp` (vars near `:1560`; `splitinfo` `:2479`; `setup` `:2521`; `bindparams` `:2549`; shader name `:2770`)
- Modify: `config/glsl/deferred.cfg:18-46`, `:57`
- Modify: `config/glsl/deferred/deferredlight_decls.glsl:57-62`
- Modify: `config/glsl/deferred/deferredlight_defs.glsl` (after the `#endif` of the `DL_NUMRH` chain, `:173`)
- Modify: `config/glsl/deferred/deferredlight.frag:11-15` (header), `:56-86` (`getrhlight`)
- Modify: `config/usage.cfg` (after `setdesc "rhtaps"`, `:522`)

**Interfaces:**
- Consumes: `rhboundsfov()`, `rhsplitresets` (task 1); `deferredlightcommon`, the DEBUG_UTILS `rhprobe`, `gi.ps1 sweep`/`near` and `probestats.py` (task 2).
- Produces: vars `rhblend` (float 0..8, default 0) and `rhblendmargin` (float 0..8, default 2). `static inline bool rhblendactive()`. `splitinfo::blendcenter` (`vec`). Uniforms `vec4 rhblendtc[DL_NUMRH]` and `float rhblendedge`. Type letter `h` → `DL_RHBLEND`.

Definitions (spec *Definitions*): `pr` is the split's rounded radius, `s = 2*pr/G` its cell size, `B = rhblend`, `M = rhblendmargin`. The placed centre is `c' = o + clamp(c - o, -m, m)` per axis, with `m = max(pr - (1+B+M)*s, 0)`, for every split but the last, when `B > 0`. The fine weight is `w = clamp((pr - s - max_k|p_k - c'_k|)/(B*s), 0, 1)`. The uniforms encode it as `rhblendtc[j] = vec4(-c'/(B*s), 1/(B*s))` and `rhblendedge = (G/2 - 1)/B`, so `w = clamp(rhblendedge - max|rhblendtc.xyz + p*rhblendtc.w|, 0, 1)`.

- [ ] **Step 1: Engine side**

In `src/engine/renderlights.cpp`, after `rhboundsfov()`:

```cpp
// Crossfade between RH splits: each split but the last fades into the next
// coarser one over rhblend cells inside its faces, and its centre is pulled
// towards the camera so rhblendmargin cells around the camera stay fully
// fine. 0 turns it off: today's lookup and placement.
FVARF(0, rhblend, 0, 0, 8, { cleardeferredlightshaders(); clearradiancehintscache(); });
FVARF(0, rhblendmargin, 0, 2, 8, clearradiancehintscache());

static inline bool rhblendactive() { return rhblend > 0 && rhsplits > 1; }
```

In `struct splitinfo` (`:2481`), add `blendcenter` to the members:

```cpp
        float nearplane, farplane;
        vec offset, scale;
        vec center; float bounds;
        vec cached; bool copied;
        vec blendcenter; // unsnapped placed centre, for the crossfade weights
```

`radiancehints::setup` loop body becomes:

```cpp
        splitinfo &split = splits[i];

        vec c;
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c, rhboundsfov());

        // compute the projected bounding box of the sphere
        const float pradius = ceil(radius * rhpradiustweak), step = (2*pradius) / rhgrid;
        if(rhblend > 0 && i < rhsplits-1)
        {
            // Pull the centre towards the camera, per axis, just far enough to
            // keep the camera and rhblendmargin cells around it out of the
            // band. A clamp, so the box still moves continuously.
            float maxoffset = max(pradius - (1 + rhblend + rhblendmargin)*step, 0.0f);
            c.sub(camera1->o).clamp(-maxoffset, maxoffset).add(camera1->o);
        }
        split.blendcenter = c;
        vec offset = vec(c).sub(pradius).div(step);
        offset.x = floor(offset.x);
        offset.y = floor(offset.y);
        offset.z = floor(offset.z);
        if(split.bounds != pradius) rhsplitresets++;
        split.cached = split.bounds == pradius ? split.center : vec(-1e16f, -1e16f, -1e16f);
        split.center = vec(offset).mul(step).add(pradius);
        split.bounds = pradius;
```

(The `split.scale` and `split.offset` lines after it are unchanged.)

At the end of `radiancehints::bindparams`, after `GLOBALPARAMF(rhbounds, ...)`:

```cpp
    if(rhblendactive())
    {
        // Fine weight of split j at p: clamp(rhblendedge - max|rhblendtc[j].xyz + p*rhblendtc[j].w|, 0, 1),
        // which is 1 inside and falls to 0 one cell inside the faces. The last split has no band.
        static GlobalShaderParam rhblendtc("rhblendtc");
        vec4 *rhblendtcv = rhblendtc.reserve<vec4>(rhsplits);
        loopi(rhsplits)
        {
            splitinfo &split = splits[i];
            float band = rhblend*2*split.bounds/rhgrid;
            rhblendtcv[i] = i < rhsplits-1 ? vec4(vec(split.blendcenter).mul(-1/band), 1/band) : vec4(0, 0, 0, 0);
        }
        GLOBALPARAMF(rhblendedge, (0.5f*rhgrid - 1)/rhblend);
    }
```

In `loaddeferredlightshader`, after `sun[sunlen++] = '0' + rhsplits;` (`:2772`):

```cpp
                if(rhblendactive()) sun[sunlen++] = 'h';
```

In the DEBUG_UTILS `rhprobe` (task 2), give the probe shader the same option, so it runs the blended lookup whenever the lighting does:

```cpp
    defformatstring(opts, "r%d%s", rhsplits, rhblendactive() ? "h" : "");
```

- [ ] **Step 2: Shader option**

In `config/glsl/deferred.cfg`, add to the `deferredlighttype` table after the `r` line:

```
//    h -> radiance hint split blending
```

and in `deferredlightcommon` (task 2), after `if (dlopt "r") [shader_define DL_RH ""]`:

```
        if (dlopt "h") [shader_define DL_RHBLEND ""]
```

- [ ] **Step 3: Uniforms**

In `config/glsl/deferred/deferredlight_decls.glsl`, change the `DL_RH` block to:

```glsl
#ifdef DL_RH
uniform vec3 skylightcolor;
uniform float giscale, rhnudge, rhbounds;
uniform vec4 rhtc[DL_NUMRH];
#ifdef DL_RHBLEND
uniform vec4 rhblendtc[DL_NUMRH];
uniform float rhblendedge;
#endif
uniform sampler3D tex6, tex7, tex8, tex9;
#endif
```

- [ ] **Step 4: Macros**

In `config/glsl/deferred/deferredlight_defs.glsl`, directly after the `#endif` that closes the `DL_NUMRH == 1 ... == 4` chain (`:173`), add the block below. Keep `DL_RH_BLEND` on one line: no line may end in a backslash.

```glsl

// getrhlight with split blending (DL_RHBLEND). The last split has no coarser
// split to fade to and keeps the hard edge. DL_RH_BLEND(j, offs) adds split
// j's fine weight w (1 inside, 0 one cell inside its faces) times rest, the
// weight no finer split took.
#ifdef DL_RHBLEND
#if DL_NUMRH == 2
#define DL_RH_LAST 1
#define DL_RH_OFFSETLAST DL_RH_OFFSET1
#elif DL_NUMRH == 3
#define DL_RH_LAST 2
#define DL_RH_OFFSETLAST DL_RH_OFFSET2
#elif DL_NUMRH == 4
#define DL_RH_LAST 3
#define DL_RH_OFFSETLAST DL_RH_OFFSET3
#endif
#define DL_RH_BLEND(j, offs) if(rest > 0.0) { tc = rhblendtc[j].xyz + pos*rhblendtc[j].w; w = clamp(rhblendedge - max(max(abs(tc.x), abs(tc.y)), abs(tc.z)), 0.0, 1.0); if(w > 0.0) { addrhsplit(rhtc[j].xyz + pos*rhtc[j].w, offs, w*rest, shr, shg, shb, sha); rest -= w*rest; } }
#endif
```

- [ ] **Step 5: Lookup**

In `config/glsl/deferred/deferredlight.frag`, replace the header's type-letter list (lines 11-15) so it includes `DL_RHBLEND h` after `DL_RH r`:

```
//   DL_LIGHTSHADOW p, DL_CSM c, DL_CSMCOLOR C, DL_AO a, DL_AOSUN A, DL_RH r,
//   DL_RHBLEND h, DL_MINIMAP m, DL_MSAA M, DL_SAMPLE1 O, DL_RESOLVE R,
//   DL_SAMPLESHADING S, DL_EDGEDETECT T, DL_AVATARVARIANTS d, DL_NODISTBIAS D,
//   DL_SPECTOGGLE z, SMFILTER_GATHER5 G, SMFILTER_GATHER3 g, SMFILTER_BILINEAR5 E,
//   SMFILTER_BILINEAR3 F, SMFILTER_ROTATED f
```

Replace lines 56-86 (from `#ifdef DL_RH` through the function's closing brace and the `#endif` after it) with the code below. The `#else` branch is today's text, unchanged token for token:

```glsl
#ifdef DL_RH
#ifdef DL_RHBLEND
// Adds w times one split's four hint texels at tc (that split's box
// coordinates, as DL_RH_SPLIT computes them) to the running sums. The RH
// textures have no mipmaps, so fetching under a branch is safe.
void addrhsplit(vec3 tc, float layer, float w, inout vec4 shr, inout vec4 shg, inout vec4 shb, inout vec4 sha)
{
    tc.xy += 0.5;
    tc.z = tc.z * DL_RH_SCALE + layer;
    shr += w*texture3D(tex6, tc);
    shg += w*texture3D(tex7, tc);
    shb += w*texture3D(tex8, tc);
    sha += w*texture3D(tex9, tc);
}
#endif

vec4 getrhlight(vec3 pos, vec3 norm)
{
#ifdef DL_RHBLEND
    // Each split fades into the next coarser one over rhblend cells inside
    // its faces (renderlights.cpp, radiancehints::bindparams). The weights
    // sum to 1 and the hints are linear, so blending the raw texels and
    // decoding once is exact.
    vec3 tc;
    float w, rest = 1.0;
    vec4 shr = vec4(0.0), shg = vec4(0.0), shb = vec4(0.0), sha = vec4(0.0);
    pos += norm*rhnudge;
    DL_RH_BLEND(0, DL_RH_OFFSET0)
#if DL_NUMRH > 2
    DL_RH_BLEND(1, DL_RH_OFFSET1)
#endif
#if DL_NUMRH > 3
    DL_RH_BLEND(2, DL_RH_OFFSET2)
#endif
    if(rest > 0.0)
    {
        tc = rhtc[DL_RH_LAST].xyz + pos*rhtc[DL_RH_LAST].w;
        if(max(max(abs(tc.x), abs(tc.y)), abs(tc.z)) >= rhbounds) tc = vec3(4.0);
        addrhsplit(tc, DL_RH_OFFSETLAST, rest, shr, shg, shb, sha);
    }
#else
    vec3 tc;
    float offset;
    pos += norm*rhnudge;
#if DL_NUMRH > 0
    DL_RH_SPLIT(0, DL_RH_OFFSET0)
#endif
#if DL_NUMRH > 1
    DL_RH_SPLIT(1, DL_RH_OFFSET1)
#endif
#if DL_NUMRH > 2
    DL_RH_SPLIT(2, DL_RH_OFFSET2)
#endif
#if DL_NUMRH > 3
    DL_RH_SPLIT(3, DL_RH_OFFSET3)
#endif
            tc = vec3(4.0);
    DL_RH_CLOSE
    tc.xy += 0.5;
    tc.z = tc.z * DL_RH_SCALE + offset;
    vec4 shr = texture3D(tex6, tc), shg = texture3D(tex7, tc), shb = texture3D(tex8, tc), sha = texture3D(tex9, tc);
#endif
    shr.rgb -= 0.5;
    shg.rgb -= 0.5;
    shb.rgb -= 0.5;
    sha.rgb -= 0.5;
    vec4 basis = vec4(norm*-(1.023326*0.488603/3.14159*2.0), (0.886226*0.282095/3.14159));
    return clamp(vec4(dot(basis, shr), dot(basis, shg), dot(basis, shb), min(dot(basis, sha), norm.z + 1.0)), 0.0, 1.0);
}
#endif
```

Before replacing, diff the `#else` branch against the current file's lines 59-78 (`vec3 tc;` through the `vec4 shr = texture3D(...)` line). It must match exactly. The decode lines after `#endif` are the current lines 79-84 (moved out of the branch, unchanged). The `#endif` on line 87 that closes `DL_CSM` stays.

- [ ] **Step 6: Usage text**

In `config/usage.cfg`, after `setdesc "rhtaps" ...` (`:522`):

```
setdesc "rhblend" "width in cells of the crossfade between global illumination splits; 0 turns it off" "value"
setdesc "rhblendmargin" "cells around the camera kept at the finest global illumination split while crossfading" "value"
```

- [ ] **Step 7: Build and run the sweeps with the crossfade on**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" release
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\harness.ps1 log
```

Expected: no `GLSL ERROR` in the log.

Use the pose from Task 2's Results. Each sweep runs its own `rhblend 0` reference pass first, then the `-Blend 2` pass:

```powershell
tools\harness\gi.ps1 sweep -Kind translate -X <x> -Y <y> -Z <z> -Yaw <yaw> -Steps 72 -Step 1 -Blend 2 -Name t
tools\harness\gi.ps1 sweep -Kind rotate    -X <x> -Y <y> -Z <z> -Yaw <yaw> -Steps 180 -Step 1 -Blend 2 -Name r
tools\harness\gi.ps1 near  -X <x> -Y <y> -Z <z> -Yaw <yaw> -Blend 2
```

Expected: both sweeps print `PASS`, meaning `J_cand <= J_ref/4` with `J_ref >= 0.01`. The analysis predicts about `J_ref/38` for translation (1u per step over a 2-cell band of about 19u cells) and at most about `0.1*J_ref` for rotation. `near` must print `PASS` (≤ 2/255 within 30u of the camera). Record J_ref, J_cand, the ratios and the near difference in Results.

If a sweep fails: find the probe and step `CAND J` names, and look at its value over the steps and the `S` lines in `<name>_<kind>_b2.tsv`. A probe that jumps as it crosses split 0's band edge, or a jump where the fallback starts, is a bug in the weights or the bounds. Debug it with superpowers:systematic-debugging before going on. Don't loosen `RATIO`.

- [ ] **Step 8: Check the zoom test still passes**

```powershell
tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
```

Expected: `(+0)`, `PASS`. Set `rhblend 2` first (`tools\harness\harness.ps1 send 'rhblend 2'`) and run it again: still `PASS`.

- [ ] **Step 9: Commit**

```bash
git add src/engine/renderlights.cpp config/glsl/deferred.cfg config/glsl/deferred/deferredlight_decls.glsl config/glsl/deferred/deferredlight_defs.glsl config/glsl/deferred/deferredlight.frag config/usage.cfg
git commit -m "renderlights: crossfade between radiance hint splits"
```

---

### Task 4: Shader equivalence, compile coverage and cost

**Files:** none changed. The scratch sweep file goes in the session scratchpad.

- [ ] **Step 1: `rhblend 0` is token-identical**

```powershell
tools\harness\harness.ps1 stop
tools\harness\shaders.ps1 check -Run rhb-base -NoMaps -Filter 'deferredlight*'
```

Expected: every row `PASS-TEXT`. No `SPIRV`, `PIXEL`, `WEAK`, `FAIL`, `MISSING` or `EXTRA`. Exit 0. Fix 1 changes no shader text, and with the default `rhblend 0` the engine never emits `h`.

- [ ] **Step 2: Compile every `h` variant**

Write `<scratchpad>\rhblend-sweep.txt`:

```
s00
b01 rhblend=2
b02 rhblend=2 rhsplits=3
b03 rhblend=2 rhsplits=4 rhborder=0
b04 rhblend=2 msaa=4
b05 rhblend=2 rhsplits=1
s99
```

```powershell
tools\harness\shaders.ps1 record -Run rhb-blend -NoMaps -SweepFile <scratchpad>\rhblend-sweep.txt
```

Expected: exit 0. Then check the variants:

```powershell
foreach ($sid in 'b01', 'b02', 'b03', 'b04', 'b05') {
    $rows = Select-String -Path home\uitest\shadercorpus\rhb-blend\manifest.tsv -Pattern "deferredlight\S*r\dh\S*\t$sid\t"
    '{0} {1}' -f $sid, $rows.Count
}
```

Expected: b01..b04 have counts above 0. b05 has 0, because `rhsplits 1` must not emit `h`. A sid with 0 rows other than b05 means the variant failed to compile. Re-create it in the harness (`harness.ps1 start`, `editor.ps1 open park`, `harness.ps1 send 'rhblend 2; rhsplits 3'`), read the `GLSL ERROR` lines with `harness.ps1 log`, fix, and re-run steps 1-2.

- [ ] **Step 3: GLSL 1.20 front-end check (if `glslangValidator` is in the Ubuntu WSL instance)**

Run `wsl -d Ubuntu -- which glslangValidator`. If it is present, run `tools\harness\shaders.ps1 check -Run rhb-blend -NoMaps -Filter 'deferredlight*' -SweepFile <scratchpad>\rhblend-sweep.txt`. The corpus is compared with itself, so all rows must pass, and tier 1 runs glslang `-E` on the `h` sources. If it is missing, record that as a coverage gap in Results.

- [ ] **Step 4: Cost**

```powershell
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\harness.ps1 send 'edgoto <x> <y> <z>; edaim <yaw> 0; showhud 1; editinhibit 1; timer 1; rhinoq 0; rhblend 0'
tools\harness\harness.ps1 shot cost_b0 -Settle 2000
tools\harness\harness.ps1 send 'rhblend 2'
tools\harness\harness.ps1 shot cost_b2 -Settle 2000
```

Crop the timer overlay (top-left 210×220 px) of each PNG with Pillow and read `Deferred Shading (gpu)`:

```powershell
python -c "from PIL import Image; import sys; [Image.open(p).crop((0,0,210,220)).save(p.replace('.png','_timer.png')) for p in sys.argv[1:]]" home\uitest\harness\shots\cost_b0.png home\uitest\harness\shots\cost_b2.png
```

Expected: the increase is at most 0.05 ms. Record both numbers in Results either way. If it is above that, report it; don't tune here.

- [ ] **Step 5: Stop the harness**

```powershell
tools\harness\harness.ps1 stop
```

- [ ] **Step 6: Manual zoom check (for the user, not the agent)**

List this for the user in the final report. Start a local game on `park` with a zoom weapon (rifle), set `rhforce 0; rhinoq 0; timer 1`, and zoom in and out repeatedly while standing still. `echo $rhsplitresets` must not change, and the Radiance Hints timer stays at 0.00 ms. Run it twice, with `thirdperson 0` and then `thirdperson 1`: in third person a zoom forces first person, and `fixview` keeps `basefov` at `thirdpersonfov` through it (`fov(false)`). Keep camera feeds out of view; secondary views also bump the counter.

---

### Task 5: Documentation

**Files:**
- Modify: `tools/harness/README.md` (map editor harness section: a `gi.ps1` subsection; test-only command `edzoom`)
- Modify: `doc/shader-reference.md` (Deferred Rendering Integration, `:514`)
- Local, uncommitted: `CLAUDE.md` (test-only command table), `doc/agent-handoff.md`, `doc/gi-radiance-hints-findings.md`, the spec, and memory `gi-split-pop-in`

- [ ] **Step 1: Harness README**

After the `### Prefab self-test` subsection, add:

```markdown
### GI stability checks

`gi.ps1` checks the radiance hints splits from the editor (spec
`doc/superpowers/specs/2026-10-01-rh-split-stability-design.md`):

```powershell
tools\harness\harness.ps1 start -Width 1600 -Height 900
tools\harness\editor.ps1 open park
tools\harness\gi.ps1 zoom -Fov 30 -Frames 20          # no split resized by a zoom
tools\harness\gi.ps1 sweep -Kind translate -X <x> -Y <y> -Z <z> -Yaw 90 -Blend 2
tools\harness\gi.ps1 sweep -Kind rotate    -X <x> -Y <y> -Z <z> -Yaw 90 -Step 1 -Steps 180 -Blend 2
tools\harness\gi.ps1 near  -X <x> -Y <y> -Z <z> -Yaw 90 -Blend 2
```

- `zoom` drives `edzoom <fov>` (DEBUG_UTILS, `src/game/game.cpp`): it overrides
  `curfov` in edit mode as a weapon zoom would, and 0 turns it off. It passes when
  `$rhsplitresets` (read-only, counts split resizes and cache clears) doesn't move.
- `sweep` and `near` don't look at screenshots, which animation, exposure and
  parallax make useless here. They use `rhprobe <points> <out>` (DEBUG_UTILS,
  `src/engine/renderlights.cpp`): it runs the real `getrhlight` from
  `deferredlight.frag` (the `DL_RHPROBE` main, shader `rhprobeshader`) at the world
  points in a file and writes the values plus each split's placement.
- `sweep` moves or turns the camera and probes a fixed lattice at every step, once at
  `rhblend 0` and once at `-Blend`. `probestats.py` reports J, the largest per-step
  change at any point, leaving out points near the last split's faces (its hard
  edge is unchanged). PASS needs `J_ref >= 0.01` (today's pop is visible) and
  `J_cand <= J_ref/4`. With `-Blend 0` alone it just reports DETECTED.
- `near` probes a fine lattice around the camera at `rhblend 0` and `-Blend`. Within
  `-Radius` the values must agree to 2/255.
- Every command turns off the HUD and editor overlays (`editinhibit 1`, `outline 0`,
  `entediting 0`), raises `giscale` to 8, and leaves them that way.
- Unit tests: `python -m unittest discover -s tools/harness/tests -p "test_probestats.py" -v`.
```

- [ ] **Step 2: Shader reference**

In `doc/shader-reference.md`, at the end of the "Deferred Rendering Integration" section, add:

```markdown
### Radiance hint split blending (`h`)

`deferredlight` gets `h` after `r<N>` when `rhblend > 0` and `rhsplits > 1`
(`loaddeferredlightshader`, `src/engine/renderlights.cpp`). `deferred.cfg` turns it
into `DL_RHBLEND`, which switches `getrhlight` (`deferred/deferredlight.frag`) to the
blended path: each split but the last fades into the next coarser one over `rhblend`
cells inside its faces, with weights from `rhblendtc[]` and `rhblendedge`
(`radiancehints::bindparams`). Without `h`, the text is the hard lookup, token for
token.
```

- [ ] **Step 3: Commit**

```bash
git add tools/harness/README.md doc/shader-reference.md
git commit -m "doc: describe the rh split crossfade and gi harness"
```

- [ ] **Step 4: Local docs (not committed)**

- `CLAUDE.md`: add a UI-and-editor table row: `` `edzoom <fov>` `` | Overrides `curfov` in edit mode (a stand-in for a weapon zoom); 0 is off. `game.cpp`.
- `doc/gi-radiance-hints-findings.md`: in the status table, set rows 1 and 2 to **Implemented** on branch `rh-split-stability` (crossfade off by default, `rhblend 2` to try it).
- The spec: set `Status: approved 2026-10-01 (default off; see plan)`, move the three open questions into the *Decisions* table with the answers, and note the front-to-back lookup deviation.
- `doc/agent-handoff.md`: update the "Last updated" line, §3 (work done: commits and results), and §6 row 14 (fixes 1–2 done; tuning and a decision on the default are next; then items 3–5, 7, 8).
- Memory `gi-split-pop-in`: fixes 1–2 implemented on `rh-split-stability`, default off; next is tuning the default.

---

## Results

Recorded 2026-10-01.

| Item | Value |
|---|---|
| Sweep pose (park) | X=1358.7 Y=1296.56 Z=1199.62 Yaw=180 |
| Probe determinism (same pose twice, J) | 0 |
| Translate J_ref / J_cand / ratio | 0.027532 / 0.001227 / 0.045 (72 steps of 1u) PASS |
| Rotate J_ref / J_cand / ratio | 0.037417 / 0.003154 / 0.084 (180 steps of 1 deg) PASS |
| Near max difference / probes compared | 0.000000 / 135 PASS |
| Zoom `rhsplitresets` delta, before fix / after fix / `rhblend 2` | +80 / +0 / +0 |
| `rhsplits 3` / `4` translate sweep (J_ref -> J_cand, ratio) | 0.027371 -> 0.000668 (0.024) PASS / 0.018752 -> 0.000714 (0.038) PASS |
| `shaders.ps1 check` (rhb-base) | 13518/13518 PASS-TEXT |
| `h` variants per sid | compiled rows b01..b04 = 196/196/196/262, b05 (`rhsplits 1`) = 0; `glslangValidator -E`: 850/850 unique `h` fragment sources OK |
| Deferred Shading (gpu), `rhblend 0` / `2` | 0.12 ms / 0.12 ms (1600x900, park; timer resolution 0.01 ms) |
| Coverage gaps | The real in-game weapon zoom check (Task 4 Step 6) is still manual and has **not been run**: `edzoom` stands in for it in the editor, so a zoom driven by an actual weapon, with `rhforce 0; rhinoq 0; timer 1`, is unverified, in both `thirdperson 0` and `thirdperson 1`. The third-person case (a zoom forces first person, which used to move `basefov` from `thirdpersonfov` to `firstpersonfov` and back) was found by the final review and fixed by `basefov = fov(false)`; it is verified by code reading only, since the editor can't drive it. Deferred minors are in the ledger (`progress.md`). |
