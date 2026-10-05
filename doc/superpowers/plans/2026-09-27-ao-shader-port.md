# AO Shader Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the CubeScript GLSL generators in `config/glsl/ao.cfg` (`linearizedepth`, `ambientobscurance*`, `bilateral[xy]*`) with plain `.vert`/`.frag` files plus `#define`s, and prove with the shader harness that every configuration compiles to the same code.

**Architecture:** The GLSL moves to `config/glsl/ao/`. `ao.cfg` keeps only thin `shader_new` aliases, which pass the engine state (`$msaasamples`, `$gdepthformat`, `$aodepthformat`, the texel-offset limits) and the `generateshader` options from `renderlights.cpp` as `#define`s. All branching happens in the GLSL preprocessor. Tap loops the generator unrolled stay unrolled, as one-line macro invocations, so the result stays provable at the SPIR-V tier.

**Tech Stack:** GLSL (compat header from `composeglslparts`), CubeScript, the shader source loader (`shader_new`, `shader_define`, `shader_source`), the shader equivalence harness (`tools/harness/shaders.ps1`), PowerShell 5.1, WSL (`glslang-tools`, `spirv-tools`).

**Spec:** [doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md](../specs/2026-09-25-shader-equivalence-harness-design.md), "Migration model" and "Acceptance tests" item 5. The loader it uses is in `doc/shader-reference.md`, "Shader Source Files", and was built by [2026-09-26-shader-source-loader.md](2026-09-26-shader-source-loader.md), whose "Out of scope, for the family ports" list this plan starts from.

## Global Constraints

- Behaviour-preserving. Every AO-family configuration in the baseline must come back `PASS-TEXT` (`linearizedepth`) or `PASS-SPIRV` (`ambientobscurance*`, `bilateral[xy]*`). `PASS-PIXEL`, `WEAK`, `FAIL`, `MISSING` or `EXTRA` on an AO-family row is a defect in this plan's code, not noise.
- Every non-AO row must stay what it was: `PASS-TEXT`, apart from the known model-shader `MISSING`/`EXTRA` rows on `m-gauntlet`, `m-challenges-port-06` and `m-deathtrap`. The baseline predates the map-pass fix `61b3543a`; see `doc/agent-handoff.md` §6 item 1a.
- **Never delete or re-record `home/uitest/shadercorpus/baseline/`.** Re-recording it is the user's call. This plan adds a separate, small corpus, `home/uitest/shadercorpus/aogaps/`.
- No C++ changes. No rebuild after Task 0.
- GLSL macros stay on **one line each**. Backslash line continuation is only guaranteed from GLSL 4.20, and the engine emits `#version 400` or lower.
- No text before `void main` may contain the substring `main` (`findglslmain` caveat, `doc/shader-reference.md`), comments included: no "remain", "domain", "maintain".
- CubeScript: no bare `#`, no `@`, and `exec "path" 0 0` (CLAUDE.md "CubeScript traps").
- Commit messages are lowercase `area: summary` and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Stage explicit paths only, never `git add -A`. Never push.
- Leave the user's unrelated files alone: `readme.md`, `chat_wip.cfg`, `deli.zip`, `gun_lore.txt`, `profile_daemon.ps1`, `profiler_tools.zip`, `profilerhook.cfg`, `redeclipse-crash.dmp`, `unix/`.
- `doc/` is untracked. Update `doc/agent-handoff.md`, but don't commit it.

---

## Decisions and evidence (read before Task 1)

1. **Taps are unrolled, not looped.** This was the user's decision on 2026-09-27, and it deviates from the spec's acceptance test 5, which expected a loop. A planning spike ran candidate sources through `tools/harness/shadercheck.py` against baseline blobs:
   - a `for` loop over the AO taps gave `DIFF` at SPIR-V (`spirv-opt -O` does not unroll), and so did the same loop marked `[[unroll]]` (`GL_EXT_control_flow_attributes`);
   - helper functions called once per tap gave `DIFF` too;
   - a macro-unrolled tap list gave `SPIRV` for both `ambientobscurancelp5` and `bilateralxlp3`.

   With a loop, evidence would drop to the pixel tier. The bench can't seed `sampler2DMS`, so the MSAA configurations (s01–s04, s40) would go essentially unverified. A bilateral loop would also lose its `texture2DRectOffset` fetches, because a loop index is not a constant expression.

2. **Offline pre-validation.** The files in Tasks 2–4 were assembled the way `shader_new` assembles them, with each blob's own version header in front and the defines derived from each sweep point. They were then checked against **every** AO-family blob in the baseline, 179 (name, sid) pairs. Results: `linearizedepth` 46/46 `TEXT`; `ambientobscurance*` and `bilateral[xy]*` 133/133 `SPIRV`. A mutation run proved the check is sensitive:
   - one AO tap constant changed: every row with more than one tap went `DIFF`, and `ambientobscurance1` stayed `SPIRV`;
   - one bilateral weight changed: every bilateral row went `DIFF`.

   The live harness is still the authority. This only means that a failure in Tasks 2–4 most likely comes from the CubeScript alias, not the GLSL.

3. **The define contract.**

   | Define | Value | From |
   |---|---|---|
   | `MSAA_SAMPLES` | `$msaasamples` | `aoshaderdefines` |
   | `GDEPTH_FORMAT` | `$gdepthformat` (0 hyperbolic, 1 packed RGB8, >1 linear float) | `aoshaderdefines` |
   | `AO_DEPTH_FORMAT` | `$aodepthformat` (0 RGB8-packed, 1 R16F/RG16F, 2 R32F) | `aoshaderdefines` |
   | `AO_LINEAR` / `AO_DERIVNORMAL` / `AO_PACKED` | present for option `l` / `d` / `p` | `ambientobscuranceshader` |
   | `AO_TAPS` | 1..12 | `ambientobscuranceshader` arg 2 |
   | `BILATERAL_LINEAR` / `BILATERAL_PACKED` / `BILATERAL_UPSCALED` | present for option `l` / `p` / `u` | `bilateralvariantshader` |
   | `BILATERAL_TAPS` | 1..10 | arg 3 |
   | `BILATERAL_REDUCE` | 0, or `aoreduce` when the depth is read at full size | arg 4 |
   | `BILATERAL_X` | present for the x pass | arg 5 |
   | `TEXRECT_MINOFFSET` / `TEXRECT_MAXOFFSET` | `$mintexrectoffset` / `$maxtexrectoffset` (-8/7 on the RTX 3080) | `bilateralvariantshader` |

   The aliases pass raw values only and never compute GLSL logic.

4. **Name trap in `bilateral.frag`.** At function scope the shader `#define`s `color` (as `vals.x`/`vals.a`) and, in one path, `depth` (as `vals.y`). Tap blocks must not declare `vals`, `color` or `depth`. They use `tapvals`, `tapcolor` and `tapdepth`, which is also why the per-tap read is a separate macro, `BILATERAL_TAPREAD`.

5. **Coverage gaps, and the `aogaps` corpus.** Several `#if` branches are unreachable from any recorded sweep point: non-linear AO depth with `glineardepth` 1 or 3, AO `gpackdepth` output, `d` with `gdepthformat` ≠ 0, bilateral linear-unpacked with float AO depth, bilateral non-linear with `gdepthformat` ≠ 0, `r2`, and MSAA non-upscaled bilateral. Task 1 adds sweep points s45–s55 that reach them and records them **from the unported build** into a separate corpus, `aogaps`. The golden baseline isn't touched. The next full re-record, which is already pending for the map-pass fix and is the user's call, will fold these points in, and `aogaps` can go then.

---

## File Structure

| File | Responsibility |
|---|---|
| `config/glsl/ao/linearizedepth.vert` (new) | Screen quad, one texcoord |
| `config/glsl/ao/linearizedepth.frag` (new) | G-buffer depth → reduced AO depth (packed RGB8 or float) |
| `config/glsl/ao/ambientobscurance.vert` (new) | Screen quad, two texcoords |
| `config/glsl/ao/ambientobscurance.frag` (new) | AO: depth, normal (fetched or derived), 12-entry unrolled tap list, prefilter, output |
| `config/glsl/ao/bilateral.vert` (new) | Screen quad; texcoords only when reduced/upscaled |
| `config/glsl/ao/bilateral.frag` (new) | Bilateral filter, one direction; 20-entry unrolled tap table with offset/plain fetch selection |
| `config/glsl/ao.cfg` | Shrinks to `aoshaderdefines`, `ambientobscuranceshader`, `linearizedepth`, `bilateralvariantshader`, `bilateralshader` |
| `.gitattributes` | `eol=lf` for `*.vert`, `*.frag`, `*.glsl` |
| `tools/harness/shader-sweep.txt` | Sweep points s45–s55 |
| `doc/shader-reference.md` | Real example and a "Porting a generator" subsection |
| `doc/agent-handoff.md` | State and queue (untracked) |

No other file reads the globals the old generators set (`lineardepth`, `packeddepth`, `derivnormal`, `maxaotaps`, `linear`, `packed`, `upscaled`, `numtaps`, `reduced`, `filterdir`, `cur*`), nor `aotapoffsets`. `gi.cfg` and `volumetric.cfg` set their own `numtaps`/`reduced`/`filterdir` before use. Nothing outside `ao.cfg` calls `ambientobscurancevariantshader` or `bilateralvariantshader`, and C++ calls only `ambientobscuranceshader` and `bilateralshader` (`src/engine/renderlights.cpp:119,158`).

## Checking a family: the command used by every task

Run it in a PowerShell session at the repo root with the harness running. It records a candidate corpus, runs the tiers and filters the result to the AO family. `.cfg` and `.frag` edits need no restart, because `check` runs `resetshaders`, which re-executes `config/glsl.cfg` and re-reads the files.

```powershell
$r = & tools\harness\shaders.ps1 check -Sids s00,s01,s06,s07,s08,s31,s32,s39,s40,s41,s42,s43,s44 -NoMaps -PassThru
$ao = @($r | Where-Object { ($_.Name -replace '^<variant:[^>]*>', '') -match '^(linearizedepth$|ambientobscurance|bilateral[xy])' })
$ao | Group-Object Status, Name | Sort-Object Name | Format-Table Count, Name -AutoSize
@($r | Where-Object { $_.Status -notlike 'PASS-*' }) | Format-Table Status, Name, Sid, Detail -AutoSize -Wrap
```

Those 13 sids hold every distinct AO-family blob of the baseline. s02–s04 duplicate s01, and the rest duplicate s00. For the gap points, the same command gets `-Run aogaps` and `-Sids s00,s45,s46,s47,s48,s49,s50,s51,s52,s53,s54,s55,s99`. On any unexpected row:

```powershell
tools\harness\shaders.ps1 diff <name> -Sid <sid>              # golden baseline
tools\harness\shaders.ps1 diff <name> -Sid <sid> -Run aogaps  # gap corpus
```

---

### Task 0: Branch and build

**Files:** none.

- [ ] **Step 1: Branch**

```powershell
git status --short
git switch -c ao-shader-port
git log --oneline -1
```

Expected: `2734a70a harness: test and document the deterministic map pass`, or a later `master` commit if the user has committed since. `git status` lists only the user's unrelated files, and `config/glsl` is clean:

```powershell
git status --short config/glsl
```

Expected: no output.

- [ ] **Step 2: Build debug and start the harness**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
tools\harness\harness.ps1 start
```

Expected: the build ends by copying `redeclipse.exe` to `bin/amd64/`, and `start` reports the game up, with no assert or backtrace in `home/uitest/log.txt`.

- [ ] **Step 3: Record the texel-offset limits**

```powershell
tools\harness\harness.ps1 send 'echo (concat TEXRECT $mintexrectoffset $maxtexrectoffset)'
```

Expected: `TEXRECT -8 7`. Note the values in the task report. If they differ, the plan still holds, because the defines carry whatever the engine reports, but the offline evidence in "Decisions" 2 assumed -8/7.

- [ ] **Step 4: Confirm the oracle on the unported code**

Run the family check command above (golden baseline, 13 sids). Expected: every AO-family row `PASS-TEXT` (hash-identical), and no non-`PASS-*` rows at all. This proves the build and harness agree with the baseline before anything changes.

---

### Task 1: Sweep the unreachable AO branches into the `aogaps` corpus

**Files:**
- Modify: `tools/harness/shader-sweep.txt` (insert before the final `s99` line)

**Interfaces:**
- Produces: sweep ids `s45`–`s55`, and the corpus `home/uitest/shadercorpus/aogaps/`. Tasks 2–5 check against it with `-Run aogaps`.

- [ ] **Step 1: Add the sweep points**

Insert this block directly above the final `s99` line of `tools/harness/shader-sweep.txt`:

```text
# ao.cfg branches no point above reaches. The option strings follow
# loadambientobscuranceshader/loadbilateralshader (renderlights.cpp):
# aoreducedepth=0 makes AO read the full g-buffer (no 'l'), aopackdepth=0
# drops 'p' and lets bilateral read depth at full size ('r'), and
# glineardepth picks $gdepthformat 1 or 3.
s45 aopackdepth=0
s46 aoreducedepth=0 glineardepth=1
s47 aofloatdepth=0 aoreducedepth=0
s48 aofloatdepth=0 aoreducedepth=0 glineardepth=1
s49 aoreducedepth=0 aopackdepth=0 glineardepth=3
s50 aoreducedepth=0 aopackdepth=0 glineardepth=1
s51 aoderivnormal=1 aobilateralupscale=1 glineardepth=1
s52 aoderivnormal=1 aobilateralupscale=1 glineardepth=3
s53 aofloatdepth=0 glineardepth=3
s54 aoreduce=2 aoreducedepth=0 aopackdepth=0
s55 msaa=4 aoreducedepth=0 aopackdepth=0
```

- [ ] **Step 2: Record the gap corpus from the unported build**

```powershell
git status --short config/glsl
tools\harness\shaders.ps1 record -Run aogaps -Sids s00,s45,s46,s47,s48,s49,s50,s51,s52,s53,s54,s55,s99 -NoMaps
```

Expected: `git status` prints nothing (the corpus must come from unported GLSL), `record` exits 0 with no leak report, and `home\uitest\shadercorpus\aogaps\run.txt` has `glsldirty 0`.

- [ ] **Step 3: Verify each point reached what it was added for**

```powershell
$m = Get-Content home\uitest\shadercorpus\aogaps\manifest.tsv | ForEach-Object { $f = $_ -split "`t"; [pscustomobject]@{ Name = $f[0]; Sid = $f[1]; Hash = $f[2] } } |
    Where-Object { $_.Hash -ne '-' -and $_.Name -match '^(linearizedepth$|ambientobscurance|bilateral[xy])' }
$gold = @{}
Get-Content home\uitest\shadercorpus\baseline\manifest.tsv | ForEach-Object { $f = $_ -split "`t"; $gold["$($f[0])`t$($f[2])"] = 1 }
$m | Sort-Object Sid, Name | Format-Table Sid, Name, @{ n = 'new'; e = { -not $gold.ContainsKey("$($_.Name)`t$($_.Hash)") } } -AutoSize
```

Expected names per sid, each also carrying `linearizedepth`:

| sid | AO | bilateral (x and y) | must be `new` |
|---|---|---|---|
| s45 | `ambientobscurancel5` | `bilateral[xy]l3` | both AO and bilateral |
| s46 | `ambientobscurancep5` | `bilateral[xy]p3` | AO |
| s47 | `ambientobscurancep5` | `bilateral[xy]p3` | AO |
| s48 | `ambientobscurancep5` | `bilateral[xy]p3` | AO |
| s49 | `ambientobscurance5` | `bilateral[xy]r13` | AO and bilateral |
| s50 | `ambientobscurance5` | `bilateral[xy]r13` | AO and bilateral |
| s51 | `ambientobscuranceldp5` | `bilateral[xy]r1up3` | AO and bilateral |
| s52 | `ambientobscuranceldp5` | `bilateral[xy]r1up3` | AO and bilateral |
| s53 | `ambientobscurancelp5` | `bilateral[xy]lp3` | `linearizedepth` |
| s54 | `ambientobscurance5` | `bilateral[xy]r23` | bilateral |
| s55 | `ambientobscurance5` | `bilateral[xy]r13` | AO and bilateral |

`new` means that (name, hash) doesn't occur anywhere in the golden baseline, i.e. the point reached code the baseline never recorded. If a row expected to be `new` isn't, or a name differs, stop and report. The point doesn't reach its branch on this GPU, and the gap stays open.

- [ ] **Step 4: Confirm the oracle on the gap corpus**

Run the family check command with `-Run aogaps` and the gap sids. Expected: every row `PASS-TEXT`.

- [ ] **Step 5: Commit**

```powershell
git add tools/harness/shader-sweep.txt
git commit -m @'
harness: sweep the remaining ao depth-format branches

s45-s55 reach the ao.cfg branches no earlier point did: full-size AO
depth with glineardepth 1 and 3, the AO gpackdepth output, derived
normals on a linear g-buffer, linear unpacked and r2 bilateral, and
MSAA without upscaling. The AO port is checked against them.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 2: `linearizedepth`, `aoshaderdefines` and `eol=lf`

**Files:**
- Create: `config/glsl/ao/linearizedepth.vert`, `config/glsl/ao/linearizedepth.frag`
- Modify: `config/glsl/ao.cfg` (header, new alias, the `linearizedepth` block)
- Modify: `.gitattributes`

**Interfaces:**
- Produces: the alias `aoshaderdefines` (no args; emits `MSAA_SAMPLES`, `GDEPTH_FORMAT`, `AO_DEPTH_FORMAT` via `shader_define`, valid only inside a `shader_new` body). Tasks 3 and 4 call it.

- [ ] **Step 1: Line endings**

Append to `.gitattributes`, after the `*.sh        text eol=lf` line:

```text
*.vert      text eol=lf
*.frag      text eol=lf
*.glsl      text eol=lf
```

- [ ] **Step 2: Write `config/glsl/ao/linearizedepth.vert`**

```glsl
// Linearizes the g-buffer depth into the reduced AO depth buffer (renderao).
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
}
```

- [ ] **Step 3: Write `config/glsl/ao/linearizedepth.frag`**

```glsl
// Linearizes the g-buffer depth into the reduced AO depth buffer (renderao).
// Engine state, from aoshaderdefines in config/glsl/ao.cfg:
//   MSAA_SAMPLES     $msaasamples: nonzero reads a multisampled g-buffer
//   GDEPTH_FORMAT    $gdepthformat: 0 hyperbolic, 1 packed RGB8, >1 linear float
//   AO_DEPTH_FORMAT  $aodepthformat: 0 packs into RGB8, nonzero writes a float
#if MSAA_SAMPLES
uniform sampler2DMS tex0;
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex0;
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform vec3 gdepthpackparams;
varying vec2 texcoord0;
fragdata(0) vec4 fragcolor;
void main(void)
{
#if AO_DEPTH_FORMAT == 0 && GDEPTH_FORMAT == 1
    fragcolor = gfetch(tex0, texcoord0);
#else
  #if GDEPTH_FORMAT > 1
    float depth = gfetch(tex0, texcoord0).r;
  #elif GDEPTH_FORMAT == 1
    float depth = dot(gfetch(tex0, texcoord0).rgb, gdepthunpackparams);
  #else
    float depth = gdepthscale.x / (gfetch(tex0, texcoord0).r*gdepthscale.y + gdepthscale.z);
  #endif
  #if AO_DEPTH_FORMAT == 0
    vec3 packdepth = depth * gdepthpackparams;
    packdepth = vec3(packdepth.x, fract(packdepth.yz));
    packdepth.xy -= packdepth.yz * (1.0/255.0);
    fragcolor = vec4(packdepth, 1.0);
  #else
    fragcolor.r = depth;
  #endif
#endif
}
```

- [ ] **Step 4: Replace the header of `config/glsl/ao.cfg` and add `aoshaderdefines`**

Replace the first five lines:

```cubescript
////////////////////////////////////////////////
//
// ambient obscurance
//
////////////////////////////////////////////////
```

with:

```cubescript
////////////////////////////////////////////////
//
// ambient obscurance
//
// The GLSL is in config/glsl/ao/. The aliases below only hand it the engine
// state and the options renderlights.cpp asks for, as #defines; each file's
// header lists the ones it reads.
//
////////////////////////////////////////////////

// Engine state every AO shader branches on. Call inside a shader_new body.
aoshaderdefines = [
    shader_define MSAA_SAMPLES $msaasamples
    shader_define GDEPTH_FORMAT $gdepthformat
    shader_define AO_DEPTH_FORMAT $aodepthformat
]
```

- [ ] **Step 5: Replace the `linearizedepth` block**

Replace the whole block from `shader $SHADER_DEFAULT "linearizedepth" [` down to its closing `]` (the line before `bilateralvariantshader = [`) with:

```cubescript
shader_new $SHADER_DEFAULT "linearizedepth" [
    aoshaderdefines
    shader_source "config/glsl/ao/linearizedepth.vert" "config/glsl/ao/linearizedepth.frag"
]
```

- [ ] **Step 6: Check against the golden baseline**

Run the family check command (13 sids). Expected:
- every `linearizedepth` row is `PASS-TEXT` with detail "same tokens after preprocessing", not "identical", since the composed text now carries the defines;
- every `ambientobscurance*`/`bilateral*` row is `PASS-TEXT` "identical", because they're untouched;
- no non-`PASS-*` rows.

A `MISSING` `linearizedepth` means `shader_new` refused something. Look in `home\uitest\log.txt` for `shader linearizedepth: not created` and the line above it.

- [ ] **Step 7: Check against the gap corpus**

Run the family check command with `-Run aogaps` and the gap sids. Expected: `linearizedepth` is `PASS-TEXT` at every sid, including s53 (`AO_DEPTH_FORMAT 0`, `GDEPTH_FORMAT 3`), and everything else is `PASS-TEXT` "identical".

- [ ] **Step 8: Commit**

```powershell
git add .gitattributes config/glsl/ao.cfg config/glsl/ao/linearizedepth.vert config/glsl/ao/linearizedepth.frag
git commit -m @'
glsl: move linearizedepth into config/glsl/ao

The shader is plain GLSL now; ao.cfg passes $msaasamples, $gdepthformat
and $aodepthformat as #defines through the new aoshaderdefines, which
the other AO shaders will share. Shader sources get eol=lf.

shaders.ps1 check: PASS-TEXT at every recorded configuration, the
baseline and the aogaps points.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 3: `ambientobscurance`

**Files:**
- Create: `config/glsl/ao/ambientobscurance.vert`, `config/glsl/ao/ambientobscurance.frag`
- Modify: `config/glsl/ao.cfg` (from `aotapoffsets = [` through the end of `ambientobscuranceshader = [ ... ]`)

**Interfaces:**
- Consumes: `aoshaderdefines` (Task 2).
- Produces: `ambientobscuranceshader <options> <taps>`, the same signature `renderlights.cpp:158` calls, creating `ambientobscurance<options><taps>`. `ambientobscurancevariantshader` and `aotapoffsets` are removed.

- [ ] **Step 1: Write `config/glsl/ao/ambientobscurance.vert`**

```glsl
// Ambient obscurance; see ambientobscurance.frag.
attribute vec4 vvertex;
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
uniform vec4 screentexcoord1;
#define vtexcoord1 (vvertex.xy * screentexcoord1.xy + screentexcoord1.zw)
varying vec2 texcoord0, texcoord1;
void main(void)
{
    gl_Position = vvertex;
    texcoord0 = vtexcoord0;
    texcoord1 = vtexcoord1;
}
```

- [ ] **Step 2: Write `config/glsl/ao/ambientobscurance.frag`**

```glsl
// Ambient obscurance: AO_TAPS depth taps around each pixel, taken from a fixed
// offset table reflected by a noise texture.
// Options, from ambientobscuranceshader in config/glsl/ao.cfg:
//   AO_LINEAR       read the reduced linear depth from linearizedepth ("l")
//   AO_DERIVNORMAL  derive normals from depth derivatives ("d")
//   AO_PACKED       write depth beside the result for the bilateral filter ("p")
//   AO_TAPS         1..12
// Engine state, from aoshaderdefines: MSAA_SAMPLES, GDEPTH_FORMAT, AO_DEPTH_FORMAT
// (see linearizedepth.frag).
//
// The taps are unrolled on purpose: every tap is its own AO_TAP line below,
// so the code matches the unrolled CubeScript generator it replaced. Keep the
// macros on one line each; line continuation needs GLSL 4.20.
#if MSAA_SAMPLES && !defined(AO_LINEAR)
uniform sampler2DMS tex0;
#define gdepthfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex0;
#define gdepthfetch(sampler, coords) texture2DRect(sampler, coords)
#endif
#if MSAA_SAMPLES
uniform sampler2DMS tex1;
#define gnormfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex1;
#define gnormfetch(sampler, coords) texture2DRect(sampler, coords)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform sampler2D tex2;
uniform vec3 tapparams;
uniform vec2 contrastparams;
uniform vec4 offsetscale;
uniform float prefilterdepth;
#ifndef AO_DERIVNORMAL
uniform mat3 normalmatrix;
#endif
#ifdef AO_LINEAR
#define depthtc gl_FragCoord.xy
#else
#define depthtc texcoord0
#endif
uniform vec3 gdepthpackparams;
varying vec2 texcoord0, texcoord1;
fragdata(0) vec4 fragcolor;

// Depth at one tap.
#if defined(AO_LINEAR) && AO_DEPTH_FORMAT == 0 || !defined(AO_LINEAR) && GDEPTH_FORMAT == 1
#define AO_TAPDEPTH(coords) dot(gdepthfetch(tex0, coords).rgb, gdepthunpackparams)
#elif defined(AO_LINEAR) || GDEPTH_FORMAT > 1
#define AO_TAPDEPTH(coords) gdepthfetch(tex0, coords).r
#else
#define AO_TAPDEPTH(coords) gdepthscale.x / (gdepthfetch(tex0, coords).r*gdepthscale.y + gdepthscale.z)
#endif

// One tap at table offset (ox, oy), added to obscure.
#define AO_TAP(ox, oy) { vec2 tapoffset = reflect(vec2(ox, oy), noise); tapoffset = depthtc + tapscale * tapoffset; float tapdepth = AO_TAPDEPTH(tapoffset); vec3 v = vec3(tapdepth*(tapoffset*offsetscale.xy + offsetscale.zw) - pos, tapdepth - depth); float dist2 = dot(v, v); obscure += step(dist2, tapparams.z) * max(0.0, dot(v, normal) + depth*1.0e-2) / (dist2 + 1.0e-5); }

void main(void)
{
#if defined(AO_DERIVNORMAL) && AO_DEPTH_FORMAT == 1
    // tex1 holds the full-size g-buffer depth here, not normals.
  #if GDEPTH_FORMAT > 1
    float depth = gnormfetch(tex1, texcoord0).r;
    vec2 tapscale = tapparams.xy/depth;
  #elif GDEPTH_FORMAT == 1
    float depth = dot(gnormfetch(tex1, texcoord0).rgb, gdepthunpackparams);
    vec2 tapscale = tapparams.xy/depth;
  #else
    float depth = gnormfetch(tex1, texcoord0).r;
    float w = depth*gdepthscale.y + gdepthscale.z;
    depth = gdepthscale.x/w;
    vec2 tapscale = tapparams.xy*w;
  #endif
#elif defined(AO_LINEAR) && AO_DEPTH_FORMAT == 0 || !defined(AO_LINEAR) && GDEPTH_FORMAT == 1
    vec3 packdepth = gdepthfetch(tex0, depthtc).rgb;
    float depth = dot(packdepth, gdepthunpackparams);
    vec2 tapscale = tapparams.xy/depth;
#elif defined(AO_LINEAR) || GDEPTH_FORMAT > 1
    float depth = gdepthfetch(tex0, depthtc).r;
    vec2 tapscale = tapparams.xy/depth;
#else
    float depth = gdepthfetch(tex0, depthtc).r;
    float w = depth*gdepthscale.y + gdepthscale.z;
    depth = gdepthscale.x/w;
    vec2 tapscale = tapparams.xy*w;
#endif
    vec2 dpos = depthtc*offsetscale.xy + offsetscale.zw, pos = depth*dpos;
#ifdef AO_DERIVNORMAL
    vec2 ddepth = vec2(dFdx(depth), dFdy(depth));
    ddepth *= step(abs(ddepth), vec2(4.0));
    vec3 normal;
    normal.xy = (depth+ddepth.yx)*offsetscale.yx;
    normal.z = normal.x*normal.y;
    normal.xy *= -ddepth;
    normal.z -= dot(dpos, normal.xy);
    normal = normalize(normal);
#else
    vec3 normal = gnormfetch(tex1, texcoord0).rgb*2.0 - 1.0;
    float normscale = inversesqrt(dot(normal, normal));
    normal *= normscale > 0.75 ? normscale : 0.0;
    normal = normalmatrix * normal;
#endif
    vec2 noise = texture2D(tex2, texcoord1).rg*2.0-1.0;
    float obscure = 0.0;
#if AO_TAPS > 0
    AO_TAP(-0.933103, 0.025116)
#endif
#if AO_TAPS > 1
    AO_TAP(-0.432784, -0.989868)
#endif
#if AO_TAPS > 2
    AO_TAP(0.432416, -0.413800)
#endif
#if AO_TAPS > 3
    AO_TAP(-0.117770, 0.970336)
#endif
#if AO_TAPS > 4
    AO_TAP(0.837276, 0.531114)
#endif
#if AO_TAPS > 5
    AO_TAP(-0.184912, 0.200232)
#endif
#if AO_TAPS > 6
    AO_TAP(-0.955748, 0.815118)
#endif
#if AO_TAPS > 7
    AO_TAP(0.946166, -0.998596)
#endif
#if AO_TAPS > 8
    AO_TAP(-0.897519, -0.581102)
#endif
#if AO_TAPS > 9
    AO_TAP(0.979248, -0.046602)
#endif
#if AO_TAPS > 10
    AO_TAP(-0.155736, -0.488204)
#endif
#if AO_TAPS > 11
    AO_TAP(0.460310, 0.982178)
#endif
    obscure = pow(clamp(1.0 - contrastparams.x*obscure, 0.0, 1.0), contrastparams.y);
#ifdef AO_DERIVNORMAL
    vec2 weights = step(abs(ddepth), vec2(prefilterdepth)) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);
#else
    vec2 weights = step(fwidth(depth), prefilterdepth) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);
#endif
    obscure -= dFdx(obscure) * weights.x;
    obscure -= dFdy(obscure) * weights.y;
#if defined(AO_PACKED) && AO_DEPTH_FORMAT != 0
    fragcolor.rg = vec2(obscure, depth);
#elif defined(AO_PACKED)
  #if !defined(AO_LINEAR) && GDEPTH_FORMAT != 1
    vec3 packdepth = depth * gdepthpackparams;
    packdepth = vec3(packdepth.x, fract(packdepth.yz));
    packdepth.xy -= packdepth.yz * (1.0/255.0);
  #endif
    fragcolor = vec4(packdepth, obscure);
#else
    fragcolor = vec4(obscure, 0.0, 0.0, 1.0);
#endif
}
```

Why the branches look like this, against the old `gdepthunpack` calls, `config/glsl/shared.cfg:120`:
- The centre depth used `arg6 = (? lineardepth (! aodepthformat) (= gdepthformat 1))` with `arg7 = packdepth`. That is the `vec3 packdepth` branch, and it is also why the packed `AO_DEPTH_FORMAT 0` output only runs `gpackdepth` when `packdepth` wasn't declared there.
- The taps used `arg6 = (&& lineardepth (! aodepthformat))`. That condition and the non-linear `GDEPTH_FORMAT 1` case both land on `dot(...)`, hence the one `AO_TAPDEPTH` condition.

- [ ] **Step 3: Replace the generator in `config/glsl/ao.cfg`**

Replace everything from `aotapoffsets = [` down to and including the closing `]` of `ambientobscuranceshader = [ ... ]` (the lines just above `shader_new $SHADER_DEFAULT "linearizedepth" [`) with:

```cubescript
// ambientobscuranceshader <options> <taps>, from loadambientobscuranceshader
// (renderlights.cpp). Options: l linear depth, d derived normals, p packed depth.
ambientobscuranceshader = [
    shader_new $SHADER_DEFAULT (format "ambientobscurance%1%2" $arg1 $arg2) [
        aoshaderdefines
        if (>= (strstr $arg1 "l") 0) [shader_define AO_LINEAR ""]
        if (>= (strstr $arg1 "d") 0) [shader_define AO_DERIVNORMAL ""]
        if (>= (strstr $arg1 "p") 0) [shader_define AO_PACKED ""]
        shader_define AO_TAPS $arg2
        shader_source "config/glsl/ao/ambientobscurance.vert" "config/glsl/ao/ambientobscurance.frag"
    ]
]
```

The body runs inside `ambientobscuranceshader`'s frame, so `$arg1`/`$arg2` are its arguments. The call to `aoshaderdefines` restores them when it returns.

- [ ] **Step 4: Check against the golden baseline**

Run the family check command (13 sids). Expected:
- every `ambientobscurance*` row is `PASS-SPIRV`: `ambientobscurance1` (s06), `ambientobscurance5` (s08), `ambientobscuranced5` (s40), `ambientobscurancel5` (s43), `ambientobscuranceldp5` (s39, s42, s44), `ambientobscurancelp12` (s07), `ambientobscurancelp5` (s00, s01, s31, s32, s41);
- `linearizedepth` stays `PASS-TEXT`, `bilateral*` stays `PASS-TEXT` "identical";
- there are no non-`PASS-*` rows.

A tier-0 `FAIL` (contract) usually means an option didn't reach the GLSL, e.g. `normalmatrix` active when it shouldn't be. Run `diff` and check the defines at the top of the candidate's composed source.

- [ ] **Step 5: Check against the gap corpus**

Run the family check command with `-Run aogaps` and the gap sids. Expected: every `ambientobscurance*` row is `PASS-SPIRV` (s45–s55 plus s00/s99), and nothing is non-`PASS-*`. These rows exercise branches with no offline evidence behind them: s46/s48/s50 take the non-linear `GDEPTH_FORMAT 1` `packdepth` branch, s47 the `gpackdepth` output, s49 `GDEPTH_FORMAT 3`, s51/s52 `AO_DERIVNORMAL` on `GDEPTH_FORMAT` 1/3, and s55 the MSAA `tex0`. A `DIFF` here is a real branch error. Run `diff <name> -Sid <sid> -Run aogaps`, fix the branch, and re-run both checks.

- [ ] **Step 6: Commit**

```powershell
git add config/glsl/ao.cfg config/glsl/ao/ambientobscurance.vert config/glsl/ao/ambientobscurance.frag
git commit -m @'
glsl: move ambientobscurance into config/glsl/ao

The generator becomes plain GLSL with #ifs on the l/d/p options and
the depth formats; the taps stay unrolled, one AO_TAP line each, so the
compiled code is unchanged (a loop can't be proved past the pixel tier,
which can't seed the MSAA inputs).

shaders.ps1 check: PASS-SPIRV at every recorded configuration, the
baseline and the aogaps points.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 4: `bilateral`

**Files:**
- Create: `config/glsl/ao/bilateral.vert`, `config/glsl/ao/bilateral.frag`
- Modify: `config/glsl/ao.cfg` (`bilateralvariantshader`; `bilateralshader` stays as it is)

**Interfaces:**
- Consumes: `aoshaderdefines` (Task 2).
- Produces: `bilateralvariantshader <name> <options> <taps> <reduce> <x|y>`, the same arguments `bilateralshader` already passes. `bilateralshader <options> <taps> <reduce>` (`renderlights.cpp:119`) is unchanged.

- [ ] **Step 1: Write `config/glsl/ao/bilateral.vert`**

```glsl
// Bilateral AO filter; see bilateral.frag.
attribute vec4 vvertex;
#if BILATERAL_REDUCE
uniform vec4 screentexcoord0;
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
varying vec2 texcoord0;
#endif
#ifdef BILATERAL_UPSCALED
uniform vec4 screentexcoord1;
#define vtexcoord1 (vvertex.xy * screentexcoord1.xy + screentexcoord1.zw)
varying vec2 texcoord1;
#endif
void main(void)
{
    gl_Position = vvertex;
#if BILATERAL_REDUCE
    texcoord0 = vtexcoord0;
#endif
#ifdef BILATERAL_UPSCALED
    texcoord1 = vtexcoord1;
#endif
}
```

- [ ] **Step 2: Write `config/glsl/ao/bilateral.frag`**

The tap table has one entry per tap position: −10…−1, then 1…10. That's the order the old generator summed the weights in, and float addition order matters. Each entry picks the fetch form the old generator picked: an offset fetch when the offset fits `TEXRECT_MINOFFSET..TEXRECT_MAXOFFSET`, else a plain one. The depth offset is the tap offset times `1 << BILATERAL_REDUCE`, so "depth fits" implies "tap fits", and three cases cover it. In the `BILATERAL_PACKED` paths `depthv` is never read.

```glsl
// Bilateral AO filter: one direction (BILATERAL_X, else y) of a separable
// blur that weights each tap by its depth difference from the centre.
// Options, from bilateralshader in config/glsl/ao.cfg:
//   BILATERAL_LINEAR    depth comes from linearizedepth ("l")
//   BILATERAL_PACKED    AO carries its depth, see AO_PACKED ("p")
//   BILATERAL_UPSCALED  filter from the reduced AO buffer to full size ("u")
//   BILATERAL_REDUCE    aoreduce when the depth is read at full size, else 0
//   BILATERAL_TAPS      taps each side of the centre, 1..10
//   BILATERAL_X         filter along x; the x pass also writes the packed depth
// Engine state, from aoshaderdefines: MSAA_SAMPLES, GDEPTH_FORMAT,
// AO_DEPTH_FORMAT (see linearizedepth.frag), and TEXRECT_MINOFFSET/
// TEXRECT_MAXOFFSET ($mintexrectoffset/$maxtexrectoffset).
//
// The taps are unrolled on purpose, as in ambientobscurance.frag. Each one
// chooses between an offset fetch and a plain one, because offset fetches
// need a constant offset inside the TEXRECT limits. Keep the macros on one
// line each; line continuation needs GLSL 4.20.
#if MSAA_SAMPLES && !defined(BILATERAL_LINEAR)
uniform sampler2DMS tex1;
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#define gfetchoffset(sampler, coords, offset) texelFetch(sampler, ivec2(coords) + offset, 0)
#else
uniform sampler2DRect tex1;
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#define gfetchoffset(sampler, coords, offset) texture2DRectOffset(sampler, coords, offset)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform sampler2DRect tex0;
uniform vec2 bilateralparams;
uniform vec3 gdepthpackparams;
#if BILATERAL_REDUCE
varying vec2 texcoord0;
#endif
#ifdef BILATERAL_UPSCALED
varying vec2 texcoord1;
#endif
fragdata(0) vec4 fragcolor;

#ifdef BILATERAL_UPSCALED
#define tc texcoord1
#else
#define tc gl_FragCoord.xy
#endif
#if BILATERAL_REDUCE
#define depthtc texcoord0
#else
#define depthtc gl_FragCoord.xy
#endif
#ifdef BILATERAL_X
#define tapvec(type, i) type(i, 0.0)
#else
#define tapvec(type, i) type(0.0, i)
#endif
#define texval(i) texture2DRect(tex0, tc + tapvec(vec2, i))
#define texvaloffset(i) texture2DRectOffset(tex0, tc, tapvec(ivec2, i))
#define depthval(i) gfetch(tex1, depthtc + tapvec(vec2, i))
#define depthvaloffset(i) gfetchoffset(tex1, depthtc, tapvec(ivec2, i))

// A g-buffer depth sample to linear depth.
#if GDEPTH_FORMAT > 1
#define BILATERAL_UNPACKDEPTH(val) val.r
#elif GDEPTH_FORMAT == 1
#define BILATERAL_UNPACKDEPTH(val) dot(val.rgb, gdepthunpackparams)
#else
#define BILATERAL_UNPACKDEPTH(val) gdepthscale.x / (val.r*gdepthscale.y + gdepthscale.z)
#endif

// tapcolor and tapdepth of one tap, from its AO sample texv and depth sample depthv.
#if defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
#define BILATERAL_TAPREAD(texv, depthv) vec2 tapvals = texv.rg;
#define tapcolor tapvals.x
#define tapdepth tapvals.y
#elif defined(BILATERAL_PACKED)
#define BILATERAL_TAPREAD(texv, depthv) vec4 tapvals = texv; float tapdepth = dot(tapvals.rgb, gdepthunpackparams);
#define tapcolor tapvals.a
#elif defined(BILATERAL_LINEAR) && AO_DEPTH_FORMAT != 0
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = depthv.r;
#elif defined(BILATERAL_LINEAR)
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = dot(depthv.rgb, gdepthunpackparams);
#else
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = BILATERAL_UNPACKDEPTH(depthv);
#endif

// One tap: w is minus its squared distance, texv and depthv its samples.
#define BILATERAL_TAP(w, texv, depthv) { BILATERAL_TAPREAD(texv, depthv) tapdepth -= depth; float tapweight = exp2(w*bilateralparams.x - tapdepth*tapdepth*bilateralparams.y); weights += tapweight; color += tapweight * tapcolor; }

// Whether an offset fits an offset fetch, and the depth offset's scale.
#define BILATERAL_FITS(o) ((o) >= TEXRECT_MINOFFSET && (o) <= TEXRECT_MAXOFFSET)
#define BILATERAL_DEPTHSCALE (1 << BILATERAL_REDUCE)

void main(void)
{
#if defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
    vec2 vals = texture2DRect(tex0, tc).rg;
    #define color vals.x
  #ifdef BILATERAL_UPSCALED
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
  #else
    #define depth vals.y
  #endif
#elif defined(BILATERAL_PACKED)
    vec4 vals = texture2DRect(tex0, tc);
    #define color vals.a
  #ifdef BILATERAL_UPSCALED
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
  #else
    float depth = dot(vals.rgb, gdepthunpackparams);
  #endif
#elif defined(BILATERAL_LINEAR)
    float color = gfetch(tex0, tc).r;
  #if AO_DEPTH_FORMAT != 0
    float depth = gfetch(tex1, depthtc).r;
  #else
    float depth = dot(gfetch(tex1, depthtc).rgb, gdepthunpackparams);
  #endif
#else
    float color = texture2DRect(tex0, tc).r;
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
#endif
    float weights = 1.0;
    // Taps run -BILATERAL_TAPS..-1, then 1..BILATERAL_TAPS: the weights are summed in that order.
#if BILATERAL_TAPS >= 10
  #if BILATERAL_FITS(-20*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-100.0, texvaloffset(-20.0), depthvaloffset(float(-20*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-20)
    BILATERAL_TAP(-100.0, texvaloffset(-20.0), depthval(float(-20*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-100.0, texval(-20.0), depthval(float(-20*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 9
  #if BILATERAL_FITS(-18*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-81.0, texvaloffset(-18.0), depthvaloffset(float(-18*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-18)
    BILATERAL_TAP(-81.0, texvaloffset(-18.0), depthval(float(-18*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-81.0, texval(-18.0), depthval(float(-18*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 8
  #if BILATERAL_FITS(-16*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-64.0, texvaloffset(-16.0), depthvaloffset(float(-16*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-16)
    BILATERAL_TAP(-64.0, texvaloffset(-16.0), depthval(float(-16*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-64.0, texval(-16.0), depthval(float(-16*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 7
  #if BILATERAL_FITS(-14*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-49.0, texvaloffset(-14.0), depthvaloffset(float(-14*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-14)
    BILATERAL_TAP(-49.0, texvaloffset(-14.0), depthval(float(-14*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-49.0, texval(-14.0), depthval(float(-14*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 6
  #if BILATERAL_FITS(-12*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-36.0, texvaloffset(-12.0), depthvaloffset(float(-12*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-12)
    BILATERAL_TAP(-36.0, texvaloffset(-12.0), depthval(float(-12*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-36.0, texval(-12.0), depthval(float(-12*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 5
  #if BILATERAL_FITS(-10*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-25.0, texvaloffset(-10.0), depthvaloffset(float(-10*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-10)
    BILATERAL_TAP(-25.0, texvaloffset(-10.0), depthval(float(-10*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-25.0, texval(-10.0), depthval(float(-10*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 4
  #if BILATERAL_FITS(-8*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-16.0, texvaloffset(-8.0), depthvaloffset(float(-8*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-8)
    BILATERAL_TAP(-16.0, texvaloffset(-8.0), depthval(float(-8*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-16.0, texval(-8.0), depthval(float(-8*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 3
  #if BILATERAL_FITS(-6*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-9.0, texvaloffset(-6.0), depthvaloffset(float(-6*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-6)
    BILATERAL_TAP(-9.0, texvaloffset(-6.0), depthval(float(-6*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-9.0, texval(-6.0), depthval(float(-6*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 2
  #if BILATERAL_FITS(-4*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-4.0, texvaloffset(-4.0), depthvaloffset(float(-4*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-4)
    BILATERAL_TAP(-4.0, texvaloffset(-4.0), depthval(float(-4*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-4.0, texval(-4.0), depthval(float(-4*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 1
  #if BILATERAL_FITS(-2*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-1.0, texvaloffset(-2.0), depthvaloffset(float(-2*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-2)
    BILATERAL_TAP(-1.0, texvaloffset(-2.0), depthval(float(-2*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-1.0, texval(-2.0), depthval(float(-2*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 1
  #if BILATERAL_FITS(2*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-1.0, texvaloffset(2.0), depthvaloffset(float(2*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(2)
    BILATERAL_TAP(-1.0, texvaloffset(2.0), depthval(float(2*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-1.0, texval(2.0), depthval(float(2*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 2
  #if BILATERAL_FITS(4*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-4.0, texvaloffset(4.0), depthvaloffset(float(4*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(4)
    BILATERAL_TAP(-4.0, texvaloffset(4.0), depthval(float(4*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-4.0, texval(4.0), depthval(float(4*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 3
  #if BILATERAL_FITS(6*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-9.0, texvaloffset(6.0), depthvaloffset(float(6*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(6)
    BILATERAL_TAP(-9.0, texvaloffset(6.0), depthval(float(6*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-9.0, texval(6.0), depthval(float(6*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 4
  #if BILATERAL_FITS(8*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-16.0, texvaloffset(8.0), depthvaloffset(float(8*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(8)
    BILATERAL_TAP(-16.0, texvaloffset(8.0), depthval(float(8*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-16.0, texval(8.0), depthval(float(8*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 5
  #if BILATERAL_FITS(10*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-25.0, texvaloffset(10.0), depthvaloffset(float(10*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(10)
    BILATERAL_TAP(-25.0, texvaloffset(10.0), depthval(float(10*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-25.0, texval(10.0), depthval(float(10*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 6
  #if BILATERAL_FITS(12*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-36.0, texvaloffset(12.0), depthvaloffset(float(12*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(12)
    BILATERAL_TAP(-36.0, texvaloffset(12.0), depthval(float(12*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-36.0, texval(12.0), depthval(float(12*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 7
  #if BILATERAL_FITS(14*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-49.0, texvaloffset(14.0), depthvaloffset(float(14*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(14)
    BILATERAL_TAP(-49.0, texvaloffset(14.0), depthval(float(14*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-49.0, texval(14.0), depthval(float(14*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 8
  #if BILATERAL_FITS(16*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-64.0, texvaloffset(16.0), depthvaloffset(float(16*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(16)
    BILATERAL_TAP(-64.0, texvaloffset(16.0), depthval(float(16*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-64.0, texval(16.0), depthval(float(16*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 9
  #if BILATERAL_FITS(18*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-81.0, texvaloffset(18.0), depthvaloffset(float(18*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(18)
    BILATERAL_TAP(-81.0, texvaloffset(18.0), depthval(float(18*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-81.0, texval(18.0), depthval(float(18*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 10
  #if BILATERAL_FITS(20*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-100.0, texvaloffset(20.0), depthvaloffset(float(20*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(20)
    BILATERAL_TAP(-100.0, texvaloffset(20.0), depthval(float(20*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-100.0, texval(20.0), depthval(float(20*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if defined(BILATERAL_X) && defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
    fragcolor.rg = vec2(color / weights, depth);
#elif defined(BILATERAL_X) && defined(BILATERAL_PACKED)
  #ifdef BILATERAL_UPSCALED
    vec3 packdepth = depth * gdepthpackparams;
    packdepth = vec3(packdepth.x, fract(packdepth.yz));
    packdepth.xy -= packdepth.yz * (1.0/255.0);
  #else
    #define packdepth vals.rgb
  #endif
    fragcolor = vec4(packdepth, color / weights);
#else
    fragcolor = vec4(color / weights, 0.0, 0.0, 1.0);
#endif
}
```

Check the table after writing it:

```powershell
(Select-String -Path config\glsl\ao\bilateral.frag -Pattern '^#if BILATERAL_TAPS >= ').Count
(Select-String -Path config\glsl\ao\bilateral.frag -Pattern 'BILATERAL_TAP\(').Count
```

Expected: `20`, then `61` (60 table invocations plus the `#define`).

- [ ] **Step 3: Replace `bilateralvariantshader` in `config/glsl/ao.cfg`**

Replace everything from `bilateralvariantshader = [` down to the `]` just before `bilateralshader = [` with:

```cubescript
// bilateralvariantshader <name> <options> <taps> <reduce> <x|y>, via
// bilateralshader. Options (loadbilateralshader, renderlights.cpp): r<n>
// reduced, u upscaled, l linear depth, p packed depth.
bilateralvariantshader = [
    shader_new $SHADER_DEFAULT $arg1 [
        aoshaderdefines
        shader_define TEXRECT_MINOFFSET $mintexrectoffset
        shader_define TEXRECT_MAXOFFSET $maxtexrectoffset
        if (>= (strstr $arg2 "l") 0) [shader_define BILATERAL_LINEAR ""]
        if (>= (strstr $arg2 "p") 0) [shader_define BILATERAL_PACKED ""]
        if (>= (strstr $arg2 "u") 0) [shader_define BILATERAL_UPSCALED ""]
        shader_define BILATERAL_TAPS $arg3
        shader_define BILATERAL_REDUCE $arg4
        if (=s $arg5 "x") [shader_define BILATERAL_X ""]
        shader_source "config/glsl/ao/bilateral.vert" "config/glsl/ao/bilateral.frag"
    ]
]
```

Leave `bilateralshader = [ ... ]` exactly as it is.

- [ ] **Step 4: Check against the golden baseline**

Run the family check command (13 sids). Expected:
- every `bilateral[xy]*` row is `PASS-SPIRV`: `l3` (s43), `lp10` (s07), `lp3` (s00, s01, s31, s32, s41, s44), `r13` (s08), `r1u3` (s40), `r1up3` (s39, s42);
- `ambientobscurance*` stays `PASS-SPIRV` and `linearizedepth` stays `PASS-TEXT`;
- there are no non-`PASS-*` rows.

- [ ] **Step 5: Check against the gap corpus**

Run the family check command with `-Run aogaps` and the gap sids. Expected: every row `PASS-SPIRV` (`bilateral*`, `ambientobscurance*`) or `PASS-TEXT` (`linearizedepth`), and no non-`PASS-*` rows. What the gap points exercise:
- s45: linear unpacked with float AO depth;
- s49/s50: non-linear unpacked at `GDEPTH_FORMAT` 3/1;
- s51/s52: the upscaled centre at `GDEPTH_FORMAT` 1/3;
- s54: `BILATERAL_REDUCE 2`, whose depth offsets ±8, ±16, … hit the asymmetric −8/7 limit, so tap −1 fetches with an offset and tap +1 without;
- s55: the MSAA `gfetchoffset`.

- [ ] **Step 6: Commit**

```powershell
git add config/glsl/ao.cfg config/glsl/ao/bilateral.vert config/glsl/ao/bilateral.frag
git commit -m @'
glsl: move the ao bilateral filter into config/glsl/ao

Plain GLSL with #ifs on the l/p/u options, the reduce level, the
direction and the depth formats. The 20 tap positions are an unrolled
table in the old summing order; each picks an offset or a plain fetch
from the TEXRECT offset limits, as the generator did.

shaders.ps1 check: PASS-SPIRV at every recorded configuration, the
baseline and the aogaps points.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 5: Full check and documentation

**Files:**
- Modify: `doc/shader-reference.md` ("Shader Source Files")
- Modify: `doc/agent-handoff.md` (untracked, not committed)

- [ ] **Step 1: Full check against the golden baseline, maps included (about an hour)**

```powershell
$r = & tools\harness\shaders.ps1 check -PassThru
$known = { $_.Status -in 'MISSING', 'EXTRA' -and $_.Sid -in 'm-gauntlet', 'm-challenges-port-06', 'm-deathtrap' -and ($_.Name -replace '^<variant:[^>]*>', '') -like 'model*' }
$ao = { ($_.Name -replace '^<variant:[^>]*>', '') -match '^(linearizedepth$|ambientobscurance|bilateral[xy])' }
$r | Group-Object Status | Format-Table Count, Name -AutoSize
@($r | Where-Object $known) | Group-Object Status | Format-Table Count, Name -AutoSize
@($r | Where-Object {
    if (& $known) { return $false }
    # PowerShell's -and and -or have equal precedence, hence the if/else.
    if (& $ao) { $_.Status -notin 'PASS-TEXT', 'PASS-SPIRV' } else { $_.Status -ne 'PASS-TEXT' }
}) | Format-Table Status, Name, Sid, Detail -AutoSize -Wrap
```

Expected:
- the third table is **empty**: every AO-family row is `PASS-TEXT`/`PASS-SPIRV` (the `m-*` map rows included), and every other row is `PASS-TEXT`;
- the known rows are the map-pass-fix difference, recorded as 90 `MISSING` / 240 `EXTRA` in `doc/agent-handoff.md` §6 item 1a. Note the counts in the task report. If they differ, report it but don't treat it as this port's failure unless the rows are AO-family.

- [ ] **Step 2: Full check against the gap corpus**

Run the family check command with `-Run aogaps` and the gap sids once more. Expected: as in Task 4 Step 5.

- [ ] **Step 3: Update `doc/shader-reference.md`**

In "Shader Source Files", replace the example block, from `shader_new $SHADER_DEFAULT "linearizedepth" [` down to its closing fence, with the real AO alias:

````markdown
```cubescript
ambientobscuranceshader = [
    shader_new $SHADER_DEFAULT (format "ambientobscurance%1%2" $arg1 $arg2) [
        aoshaderdefines                                            // engine state, one shader_define each
        if (>= (strstr $arg1 "l") 0) [shader_define AO_LINEAR ""]  // "#define AO_LINEAR"
        shader_define AO_TAPS $arg2                                // "#define AO_TAPS 5"
        shader_source "config/glsl/ao/ambientobscurance.vert" "config/glsl/ao/ambientobscurance.frag"
    ]
]
```
````

Then, directly before `### Shader Parameter Binding`, add:

```markdown
#### Porting a generator

The AO family (`config/glsl/ao.cfg`, `config/glsl/ao/`) is the first port and
the pattern for the rest:

- The alias passes raw values only (engine vars such as `$gdepthformat` and
  the `generateshader` arguments) as defines. All branching is `#if` in the GLSL.
- Don't turn a loop the generator unrolled into a GLSL loop. Write one macro
  line per tap, each under `#if TAPS > n`. `shaders.ps1 check` proves an unrolled port at the SPIR-V tier.
  A loop compiles differently (`spirv-opt -O` doesn't unroll), so it could only be
  proved by pixels, and offset fetches (`texture2DRectOffset`) need a constant
  offset, which a loop index isn't.
- Keep every macro on one line. Line continuation needs GLSL 4.20, and the engine
  emits lower versions.
- A macro used inside a block must not declare names the file `#define`s at
  function scope (`bilateral.frag` defines `color` and `depth`).
- Expect `PASS-TEXT` when the tokens are unchanged and `PASS-SPIRV` otherwise.
  `PASS-PIXEL` means the compiled code changed.
- Make sure the sweep reaches every `#if` branch. When the golden baseline
  can't, record the missing points from the unported build into a separate run
  (`shaders.ps1 record -Run <name> -Sids ... -NoMaps`) and check against it with
  `-Run <name>`.
```

- [ ] **Step 4: Commit the doc**

```powershell
git add doc/shader-reference.md
git commit -m @'
doc: describe the ao shader files and how to port a generator

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

- [ ] **Step 5: Update `doc/agent-handoff.md` (don't commit)**

- Change the date at the top.
- In §3, add the AO port: branch `ao-shader-port`, its commits, the check results (counts per status, including the known rows), and the `aogaps` corpus (what it is, that it was recorded from the unported build at the Task 1 commit, and that it can be deleted once the golden baseline is re-recorded with s45–s55).
- In §6 item 1, mark step 2 (AO) done and point step 3 (AA, blur, decals) at `doc/shader-reference.md` "Porting a generator". AA needs a `lazyshader` equivalent (the loader plan's out-of-scope list).
- In §6 item 1a, note that the pending re-record should now include s45–s55 (they're in the sweep file) and can then replace `aogaps`.

- [ ] **Step 6: Final state**

```powershell
git log --oneline master..ao-shader-port
git status --short
```

Expected: five commits (Task 1–4 and the doc), and `git status` shows only the user's unrelated files plus untracked `doc/` changes. Don't merge or push. The user decides.

---

## Self-review notes

- **Coverage of the ask** ("refactor AO shaders, stripping out the CubeScript"): all three `ao.cfg` generators move to GLSL (Tasks 2–4). What remains in `ao.cfg` is option parsing and `shader_define`/`shader_source` calls, with no GLSL text and no `@`.
- **Loader plan's out-of-scope list:** `.gitattributes eol=lf` is done (Task 2). A `lazyshader` equivalent isn't needed, because no AO shader is lazy. `"row, col"` reuse isn't needed, because there are no variants. `#line` markers and `origin` naming files aren't needed for the proof.
- **Names across tasks:** `aoshaderdefines` (Task 2) is used in Tasks 3 and 4. `ambientobscuranceshader`/`bilateralshader` keep the signatures `renderlights.cpp` calls. The define names match the table in "Decisions" 3 and the file headers.
- **Known risk:** the gap-point branches (Tasks 3–4, Step 5) have no offline evidence. The recorded `aogaps` corpus is their only oracle, so Task 1 Step 3's `new` column must hold before it is trusted.
- **Known risk:** if a gap point doesn't reach its branch on this GPU (Task 1 Step 3), that branch stays unproven. Report it rather than adding more sweep vars.
