# Deferred Light Shader Port Implementation Plan

> Executed inline (no subagents), one commit per task on branch `deferred-shader-port`.

**Goal:** Replace the CubeScript GLSL generators in `config/glsl/deferred.cfg` (`deferredlightvariantshader`, `msaadetectedges` and the `msaaedgedetect` lazyshader) and the shadow-map filters in `config/glsl/smfilter.cfg` with plain GLSL plus `#define`s. Move the code other families repeat into `config/glsl/shared/`, and prove every configuration compiles to the same code.

**Architecture:** The pattern in `doc/shader-reference.md`, "Porting a generator". `deferred.cfg` keeps `deferredlightshader` (row registration: which variant rows exist for a type string) and a thin `deferredlightvariantshader` that hands the GLSL the row, the counts, the type letters and the engine state as `#define`s through `variantshader_new`. No C++ changes.

## Decisions and evidence

1. **Inputs.** `deferredlightvariantshader <name> <row> <type> <splits> <rh> <lights> <maxvariants>` reads the type letters (`dlopt`), the row (base light, spot, transparent, avatar, colour shadow), `$msaasamples`, `$usepacknorm`, `$ghasstencil`, `$gdepthformat` (via `gdepthunpack*`), `$usetexgather` (via the `smfilter*` aliases), `$glslversion`, `glext GL_EXT_shader_samples_identical`, `$avatarshadowbias` and `$avatarshadowdist`, and the constants `unpacknormscale`/`unpacknormbias`. The `smfilter*` aliases have no other caller (volumetric has its own filter). `unpacknorm`/`unpackspec` are also used by `ui.cfg` (`modelpreview`), so those aliases stay until ui is ported, next to their GLSL counterparts.
2. **The define contract.**

   | Define | From | Meaning |
   |---|---|---|
   | `DL_ROW` | `<row>` | variant row, `-1` = the parent; the flags below are derived from it in `deferredlight_defs.glsl` |
   | `DL_NUMLIGHTS`, `DL_NUMSPLITS`, `DL_NUMRH` | args | tile-batch lights (0–8), CSM splits (0–8), RH splits (0–4) |
   | `DL_LIGHTSHADOW` | `p` | shadowed point/spot lights |
   | `DL_CSM`, `DL_CSMCOLOR` | `c`, `C` | sunlight CSM, colour CSM |
   | `DL_AO`, `DL_AOSUN` | `a`, `A` | AO, AO on sunlight |
   | `DL_RH` | `r` | radiance hints |
   | `DL_MINIMAP` | `m` | minimap |
   | `DL_MSAA`, `DL_RESOLVE`, `DL_SAMPLE1`, `DL_SAMPLESHADING`, `DL_EDGEDETECT` | `M`, `R`, `O`, `S`, `T` | multisample paths |
   | `DL_AVATARVARIANTS`, `DL_NODISTBIAS`, `DL_SPECTOGGLE` | `d`, `D`, `z` | avatar rows exist, no dist bias, per-light spec toggle |
   | `SMFILTER_GATHER5` … `SMFILTER_ROTATED` | `G g E F f` | filter kind, none = plain compare (`N`) |
   | `MSAA_SAMPLES`, `USEPACKNORM`, `GHASSTENCIL`, `GDEPTH_FORMAT`, `USETEXGATHER`, `GLEXT_SAMPLES_IDENTICAL` | engine | raw values |
   | `AVATAR_SHADOW_BIAS`, `AVATAR_SHADOW_DIST` | engine | raw floats, same text `@` substituted |

   `$glslversion < 400` becomes `__VERSION__ < 400`: the engine emits the largest of 400/330/150/140/130/120 that `glslversion` reaches (`composeglslparts`), so the two agree.
3. **No token pasting.** glslang rejects `##` below `#version 130` ("token pasting (##): not supported for this version"), and the engine emits 120. Names a loop suffixed with its index must be passed or scoped:
   - MSAA edge taps and CSM/RH splits keep their names: the tap macro takes the name (`MSAA_EDGE_TAP(e1, 1)`) or only an index (`csmtc[j]`), so they stay `TEXT`.
   - The per-light block (`light<j>dir`, `light<j>shadow`, `spot<j>atten`, … — 11 names) becomes one `DL_LIGHT(j)` macro whose locals are unsuffixed inside a `{ }` scope, one line per light under `#if DL_NUMLIGHTS > j` (at most 8, `MAXLIGHTTILEBATCH`). Renamed locals and the extra braces change the tokens but not the code: rows with lights are expected `PASS-SPIRV` (names are stripped by `spirv-remap --strip all`), rows without lights `PASS-TEXT`. A helper function per light (the user's fallback) was not needed; per-tap functions gave SPIR-V DIFF in the AO spike.
4. **Nested-if ladders.** The MSAA edge detect wraps an action (`discard;` / `shouldresolve = false;`) inside `samples-1` nested ifs and is used three times, so it is one macro `MSAA_EDGE_DETECT(action)` built from cumulative per-count macros (`msaaedge.glsl`). The CSM and RH ladders are used once each: one line per split under `#if` for the opening, a count-selected close macro for the braces.
5. **Literal constants.** Values the generator computed with `divf` are spelt as CubeScript prints them (`%g`-style, integers as `1.0`, checked in-game): the RH offsets and scale per split count (`0.166667`, `0.833333`, …) and the MSAA resolve scale (`0.5` … `0.0625`) through `#if` chains; `unpacknorm`'s `2.02`/`0.51` literally.
6. **Include order is token order.** The shadow filters read `tex4` and `shadowatlasscale` and sit between the per-light shadow helpers and `getcsmtc`, so the fragment is assembled from `deferred/deferredlight_defs.glsl` (macros only), `deferred/deferredlight_decls.glsl` (extensions, uniforms, output, `getspottc`/`getshadowtc`), `shared/smfilter.glsl`, then `deferred/deferredlight.frag`. `#extension` lines stay ahead of every non-preprocessor token because the includes before them hold only directives.
7. **Main-scope `#define`s** (`distbias`, `alpha`, `glowscale`, `aomask`, `lightshadow`) produce no tokens, so where the old text defined one inside the per-light block they are hoisted to just before the lights; `distbias` must stay after the helper functions whose parameter has that name.
8. **Shared modules.**
   - `shared/gdepth.glsl`: add `GDEPTH_UNPACK_ORTHO(val)` (`gdepthunpackortho`).
   - `shared/gnormal.glsl`: add `GNORMAL_UNPACK_SCALE(k)` (`unpacknorm`).
   - `shared/gcolor.glsl`: add `GSPEC_UNPACK(camera, pos, normal, diffuse)` (`unpackspec`).
   - `shared/smfilter.glsl`: `filtershadow`, and `filtercolorshadow` under `SMFILTER_COLOR`; replaces the `smfilter*` aliases (the `smalpha*` shaders stay in `smfilter.cfg`).
   - `deferred/msaaedge.glsl`: `MSAA_EDGE_DETECT(action)`, for `msaaedgedetect` and the `T` light shaders.
9. **Coverage.**
   - Golden baseline: 38 deferred-light names, 3,821 distinct blobs over the sweep and the 51 map passes. The machine only produces `msaalight` 3 (`MS`/`M` types), `usetexgather` 1, `glslversion` 400, no samples-identical extension, CSM 1/3 splits and RH 0–2.
   - Reference generator corpus `dlgen` (recorded from the old generator before any edit): `home/uitest/dlgen-calls.cfg` (filters N/f/F/E/g/G, CSM 1–8 × RH 1–4, 1–8 lights, `D`, `z`, `c` without `C`, no `a`/`A`/`b`/`d`, minimap) at `gA` gdepthformat 0, `gB` defaults, `gC` gdepthformat 3 + packnorm + no stencil; plus `dlgen-calls-msaa.cfg` (`MS`, `MO`, `MOT`, `MR`, `MRT`, `M`, `D`) at `m2`, `m4`, `m8` (no stencil), `m16`; and `gX` = msaa 4 with `$usetexgather` 2 through a mechanically renamed copy of the generator. Two call types (`A` without `a`) fail to compile in the old generator too; they stay invalid on both sides.
   - `GLEXT_SAMPLES_IDENTICAL` 1 can't compile on this GPU, so it is proved on the macro alone: the old `msaadetectedges` text (glext renamed) against `MSAA_EDGE_DETECT` preprocessed by glslang, for 2/4/8/16 samples.
10. **Expected tiers** (met; results in `doc/agent-handoff.md` §3): deferred rows without lights and `msaaedgedetect` `PASS-TEXT`; rows with lights `PASS-SPIRV`; no other family changes. Registry unchanged.

## Tasks

### Task 1: Reference corpus
- [x] `dlgen` recorded from the old generator (commit c2e906be) with `scratchpad/dlgen.ps1`.

### Task 2: Shared modules
- [x] Add `GDEPTH_UNPACK_ORTHO`, `GNORMAL_UNPACK_SCALE`, `GSPEC_UNPACK`; `shared/smfilter.glsl`. Commit `glsl: share the g-buffer unpacking and shadow filter code`.

### Task 3: Deferred lights

Deviations: at the user's request the depth-to-position block moved into `shared/gdepth.glsl` as `GDEPTH_UNPACK_POS(depth, pos, val, coord)` (also the shape `ui.cfg`'s `modelpreview` uses). The snippet proof caught a missing outer `{ }` in `MSAA_EDGE_DETECT` (the old alias wrapped its output in one), fixed before the commit.
- [x] `deferred/deferredlight.vert`, `deferredlight_defs.glsl`, `deferredlight_decls.glsl`, `deferredlight.frag`, `msaaedge.glsl`, `msaaedgedetect.vert/.frag`; thin aliases in `deferred.cfg`; drop the filter aliases from `smfilter.cfg`.
- [x] Commit `glsl: move the deferred light shaders into config/glsl/deferred`.

### Task 4: Prove it
- [x] Candidate `dlgen-cand` with the same driver; pairs through `shadercheck.py --pairs`: no DIFF/NA, the tiers of decision 10.
- [x] Restart; `shaders.ps1 check -Filter '*deferredlight*'` over the sweep sids and the maps, then `msaaedgedetect`: tiers as decision 10, registry unchanged.
- [x] Snippet proof for `GLEXT_SAMPLES_IDENTICAL` 1 (decision 9).
- [x] Mutation run: change one constant in the spot-light shadow path; exactly the spot rows with `p` lose their tier.
- [x] GLSL 1.20: glslang-compile candidate texts rewritten to `#version 120` (no `##`, no integer operators, no trailing backslashes).

### Task 5: Docs
- [x] `doc/shader-reference.md`: deferred files, new shared macros, the no-`##` rule. Commit `doc: describe the deferred light shader files`.
- [x] `doc/agent-handoff.md` and memory (not committed).
