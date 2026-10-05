# Blur Shader Port Implementation Plan

> Executed inline (no subagents), one commit per task on branch `blur-shader-port`.

**Goal:** Replace the CubeScript GLSL generator `blurshader` (`config/glsl/blur.cfg`) with plain `config/glsl/blur/blur.vert`/`.frag` plus `#define`s, extract the code other families repeat into shared includes, and prove every configuration compiles to the same code.

**Architecture:** The pattern in `doc/shader-reference.md`, "Porting a generator". `blur.cfg` keeps a thin `shader_new` alias and the registration loop. No C++ changes.

## Decisions and evidence

1. **The generator reads only its arguments** (plus the `lumweights` constant): `blurshader <name> <radius> <x|y> <2D|2DRect>`, alpha when the name contains `alpha`. `glsl.cfg` registers all 56 (`MAXBLURRADIUS` 7 × x/y × 2D/rect × plain/alpha) as standard shaders, so every one is in the golden baseline at every sweep point (one hash per name, identical across sids). `check -Sids s00 -NoMaps` covers them all; no reference corpus or sweep points are needed.
2. **The define contract.**

   | Define | Value | Used by |
   |---|---|---|
   | `BLUR_RADIUS` | 1..7, taps each side | both stages |
   | `BLUR_X` | present for `x`, else y | both stages |
   | `BLUR_RECT` | present for `2DRect`, else `2D` | `blur.frag` |
   | `BLUR_ALPHA` | present when the name contains `alpha` | `blur.frag` |

3. **Array sizes stay literals.** `uniform float weights[BLUR_RADIUS + 1]` would preprocess to `[3 + 1]` where the old text had `[4]`, dropping the tier from TEXT to SPIRV. `blur_defs.glsl` maps the radius to `BLUR_SIZE` with a literal `#if` chain (as `BILATERAL_DEPTHSCALE` does) and the axis to `BLUR_AXIS`; both stages include it.
4. **Taps stay unrolled**, one macro line per tap under `#if BLUR_RADIUS >= n`. Taps 1–3 read the interpolated `texcoordp/n<i>` varyings; taps 4–7 build the coordinate in the fragment shader, as before.
5. **Shared modules.**
   - `config/glsl/shared/luma.glsl`: `LUMWEIGHTS`, the GLSL counterpart of the `lumweights` alias (`shared.cfg`), expanding to the same `vec3(...)` tokens. Blur is its first user; hud/sky/tonemap will reuse it when ported.
   - `config/glsl/shared/screentexcoord.glsl`: the `vtexcoord0`/`vtexcoord1` macros, the GLSL counterpart of the `screentexcoord` alias. Every ported `.vert` (6 in `ao/` and `aa/`) re-spells them. The shader keeps declaring the `screentexcoord<n>` uniforms, as with gdepth; only the `#define` moves, so the preprocessed tokens are unchanged and the AO/AA rows stay at their current tier.
6. **Expected tiers:** every blur row `PASS-TEXT`; AO/AA rows unchanged from their last check (AA `PASS-TEXT`, AO `PASS-TEXT`/`PASS-SPIRV`).

## Tasks

### Task 1: Shared modules
- [ ] Add `shared/screentexcoord.glsl` and `shared/luma.glsl`. Switch the `ao/` and `aa/` vertex shaders to `shader_include_vs "config/glsl/shared/screentexcoord.glsl"`.
- [ ] Commit `glsl: share the screen texcoord macros in config/glsl/shared/screentexcoord.glsl`.

### Task 2: Blur
- [ ] Write `blur/blur_defs.glsl`, `blur/blur.vert`, `blur/blur.frag`; replace the generator in `blur.cfg`.
- [ ] `resetshaders`; log clean.
- [ ] Commit `glsl: move blur into config/glsl/blur`.

### Task 3: Prove it
- [ ] Restart the harness. `shaders.ps1 check -Sids s00 -NoMaps -Filter 'blur*'`: 56 blur rows `PASS-TEXT`.
- [ ] AO/AA after the screentexcoord change: `check -Sids s00,s01,s24,s25,s26,s27,s28,s31,s32,s99 -NoMaps` (the AA proof's sid list) and `check -Run aogaps -Sids s00,s45..s55,s99 -NoMaps`, AO/AA rows at their previous tiers.
- [ ] Mutation run: change one constant in the tap-5 alpha path; exactly the alpha radius ≥ 5 rows go `DIFF`/non-TEXT.
- [ ] GLSL 1.20: preprocess and compile the blur files with `glslangValidator` at `#version 120` for all 56 define sets.

### Task 4: Docs
- [ ] `doc/shader-reference.md`: blur files and the two shared modules. Commit `doc: describe the blur shader files and shared includes`.
- [ ] `doc/agent-handoff.md` §3/§6 and memory (not committed).
