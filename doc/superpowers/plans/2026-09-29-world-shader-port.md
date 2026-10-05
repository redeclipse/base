# World Shader Port Implementation Plan

> Executed inline (no subagents), one commit per task on branch `world-shader-port`.

**Goal:** Replace the CubeScript GLSL generators in `config/glsl/world.cfg` (`worldvariantshader`, `bumpvariantshader`, `shadowmapworldvariantshader`, the `rsmworld` loop and the inline `smworld`) with plain GLSL plus `#define`s under `config/glsl/world/`. Move the g-buffer code every geometry family repeats (`ginterpvert`/`ginterpfrag`, `gdepthpackvert`/`gdepthpackfrag`, `ghashdepth`, the blend-layer `gspecpack`, `gfetchdefs`) into `config/glsl/shared/`, and prove every configuration compiles to the same code.

**Architecture:** The pattern in `doc/shader-reference.md`, "Porting a generator". `world.cfg` keeps the registration aliases (`worldshader`, `bumpshader`, `shadowmapshader`, the editor list `worldshaders`, `findworldshader`) and thin variant aliases that stage the slot params (unchanged, the self-test mutates them) and call `variantshader_new` with one define per type letter and the raw engine state. No C++ changes.

## Decisions and evidence

1. **Inputs.** `worldvariantshader`/`bumpvariantshader <name> <type>` read the type letters and `$msaalight`, `$msaasamples` (through the MSAA depth condition and `gfetchdefs`), `$gdepthformat` (`ginterp*`, `gdepthpack*`) and `$usepacknorm` (`gnormpack`, `gdepthpackfrag`). `e` only sets `SHADER_ENVMAP`; `o` (in a few bump type strings) is read by nothing. `shadowmapworldvariantshader` and `rsmworld` read only their type/row; `smworld` nothing.
2. **The define contract** (world and bump share the names):

   | Define | Letter | Meaning |
   |---|---|---|
   | `WORLD_REFLECT`, `WORLD_REFLECT_SPECMAP` | `r`, `R` | envmap reflection, scaled by the spec map |
   | `WORLD_SPEC`, `WORLD_SPECMAP` | `s`, `S` | spec, spec map |
   | `WORLD_GLOW`, `WORLD_PULSEGLOW` | `g`, `G` | glow, pulse glow |
   | `WORLD_BLEND` | `b` | blend-map layer (row 0) |
   | `WORLD_ALPHA`, `WORLD_REFRACT` | `a`, `A` | transparent (row 1), refractive |
   | `WORLD_ALPHAMASK` | `m` | alpha mask |
   | `WORLD_TRIPLANAR`, `WORLD_DETAIL` | `T`, `d` | triplanar, detail |
   | `WORLD_DISPLACE` | `v` | displacement |
   | `WORLD_PARALLAX` | `p` | parallax (bump only) |
   | `SM_ALPHA`, `SM_ALPHAMASK`, `SM_NORMALMAP` | `a`, `m`, `n` | shadow-map pass |
   | `RSM_BLEND` | row 0 | `rsmworld` blend-map variant |
   | `MSAA_LIGHT`, `MSAA_SAMPLES`, `GDEPTH_FORMAT`, `USEPACKNORM` | engine | raw values |

   The generator's `(|| $msaalight [&& $msaasamples [! (wtopt "a")]])` becomes `WORLD_MSAADEPTH` in `world/world_defs.glsl`; bump's lineardepth interpolation also takes `A`.
3. **Output declarations.** `ginterpfrag` declares `gglow` at location 2, or `gdepth` at 2 and `gglow` at 3 when `$gdepthformat` is set. `findfragdatalocs` and the harness contract read `fragdata(` from the raw text, so the two sets are `shared/gbuffer_out.glsl` and `shared/gbuffer_out_depth.glsl`, picked by a `shared.cfg` alias `gbufferoutputs`. Includes precede the source, so the outputs move ahead of the fragment's other declarations: **world and bump fragment stages are expected `PASS-SPIRV`, not `PASS-TEXT`.** Checked before planning on a baseline blob (`<variant:1,1>stdworld`): moving the three `fragdata` lines to the top gives `SPIRV` (glslang creates variables at first use; `reflect.txt` is sorted by name, so the contract is unaffected). The alternative, a family "head" include before the outputs, keeps TEXT but splits every g-buffer shader into pieces; rejected.
4. **Shared modules** (one-line macros, same tokens as the aliases):
   - `shared/gbuffer.glsl`: `GBUFFER_DEPTH_DECLS` (`ginterpdepth`), `GBUFFER_DEPTH_VERT` (`gdepthpackvert`), `GBUFFER_PACK_DEPTH` / `GBUFFER_PACK_DEPTH_HASH(hashid)` (`gdepthpackfrag` without and with the MSAA hash; needs `GDEPTH_FORMAT`, `USEPACKNORM`, and `GDEPTH_PACK` from `gdepth.glsl`).
   - `shared/gdepth.glsl`: `GDEPTH_HASH(depth, hashid)` (`ghashdepth` with no alpha).
   - `shared/gcolor.glsl`: `GSPEC_PACK_BLEND(gloss, layer)`, `GSPEC_PACK_SPEC_BLEND(gloss, spec, layer, blend)` (the blend-layer `gspecpack`).
   - `shared/gfetch.glsl`: `GFETCH_SAMPLER`, `gfetch`/`gfetchoffset`/`gfetchproj` (`gfetchdefs` without a prefix), multisampled when `GFETCH_MS` (default `MSAA_LIGHT`).
   - `world/world_defs.glsl` (family): `WORLD_MSAADEPTH`, `WORLD_ROTTEXCOORD(tc, rot)` (`rottexcoord`, only world uses it), `WORLD_DISP(...)`.
   The aliases stay in `shared.cfg` while other families still call them.
5. **Kept as is:** bump's `dispcoordy1`/`dispcoordz1`/`dispcoord1` read `dispscroll.xw`, world's read `.zw`: behaviour, not tidied. `rsmworld`'s `(= $i 2)` branch is dead (the loop gives 0 and 1), so `alpha` is always `colorparams.a`.
6. **Coverage.**
   - Golden baseline: every world row is registered, so `shaderdumpall` forces them at all 46 sids; `-Filter '*world'` covers `smworld`, `smalphaworld`, `rsmworld` and every variant.
   - The sweep only has MSAA with `$gdepthformat` 0 and `$msaalight` 3, and `$usepacknorm` 1 only under MSAA. New sweep points recorded from the unported build into a separate corpus `worldgaps` (as `aogaps`): `s56 msaa=4 glineardepth=1`, `s57 msaa=4 glineardepth=3`, `s58 msaa=4 msaapreserve=-1`, `s59 msaa=4 msaapreserve=-1 glineardepth=1`, `s60 msaa=4 msaapreserve=-1 glineardepth=3`, `s61 forcepacknorm=1`, `s62 forcepacknorm=1 glineardepth=1`, `s63 forcepacknorm=1 glineardepth=3`. With `s00`, `s01`, `s31`, `s32` that is every reachable (`WORLD_MSAADEPTH`, `GDEPTH_FORMAT` class, `USEPACKNORM`, `MSAA_LIGHT`) combination.
7. **Expected tiers:** vertex text unchanged; `smworld`, `smalphaworld`, `rsmworld` rows `PASS-TEXT`; world/bump rows `PASS-SPIRV` (decision 3); no other family changes; registry unchanged.

## Tasks

### Task 1: Reference corpus
- [x] Add s56–s63 to `tools/harness/shader-sweep.txt`; commit `harness: sweep the g-buffer states the world shaders read`.
- [x] `shaders.ps1 record -Run worldgaps -Sids s00,s01,s31,s32,s56..s63 -NoMaps` from the unported build.

### Task 2: Shared modules
- [x] `shared/gbuffer.glsl`, `shared/gbuffer_out{,_depth}.glsl`, `shared/gfetch.glsl`, `GDEPTH_HASH`, the blend `GSPEC_PACK_*`, `gbufferoutputs` alias. Commit `glsl: share the g-buffer output, depth packing and fetch code`.

### Task 3: World shaders
- [x] `world/world.vert/.frag`, `world/bump.vert/.frag`, `world/world_defs.glsl`, `world/shadowmap.vert/.frag`, `world/rsm.vert/.frag`, `world/smworld.vert/.frag`; thin aliases in `world.cfg`.
- [x] Commit `glsl: move the world shaders into config/glsl/world`.

### Task 4: Prove it
- [x] Restart; `check -NoMaps -Filter '*world'` over all 46 baseline sids: tiers as decision 7, registry unchanged.
- [x] `check -Run worldgaps -NoMaps -Filter '*world'` over the worldgaps sids.
- [x] Full `check -NoMaps` at s00/s01: no other family changes.
- [x] Mutation run: one constant in the triplanar displacement path; exactly the `Tv` rows lose their tier.
- [x] GLSL 1.20: glslang-compile candidate texts rewritten to `#version 120`.
- [x] Harness self-test (`shaders-selftest.ps1`): its anchors in `world.cfg` still exist.

### Task 5: Docs
- [x] `doc/shader-reference.md`: world files, new shared modules. Commit `doc: describe the world shader files and shared g-buffer includes`.
- [x] `doc/agent-handoff.md` and memory (not committed).

## Results (2026-09-29)

- Golden `check -NoMaps -Filter '*world'`, all 46 baseline sids: 28,796 rows = 276 `PASS-TEXT` (`smworld`, `smalphaworld` ×3, `rsmworld` ×2 at every sid) + 28,520 `PASS-SPIRV`; 0 pixel/weak/fail/missing/extra; registry unchanged; no leak; all 247 editor shaders valid everywhere.
- `check -Run worldgaps` (s00, s01, s31, s32, s56–s63): 7,512 rows = 72 TEXT + 7,440 SPIRV, nothing else.
- Stricter token proof (`scratchpad/worldtok.py`, both corpora): every vertex stage is token-identical after `glslangValidator -E`; every SPIRV fragment stage is token-identical once the output declarations are set aside, and those are identical and in the same order. So decision 3's move is the only change.
- Unfiltered `check -NoMaps` at s00/s01/s31/s32: 5,855 rows, 0 fail/missing; the only non-world SPIRV rows are the AO and deferred-light ones from earlier ports; 20 `EXTRA` `modelA0`/`modelnme0` at s31 (known noise, handoff §6 item 1a).
- Mutation (`dispcoordz0 … * 2.0` in both vertex files): exactly the 26 `T`+`v` shaders × 4 rows = 104 `FAIL` (pixel), everything else unchanged. Reverted.
- GLSL 1.20 (`scratchpad/glsl120.py`, the no-`EXT_gpu_shader4` header of `composeglslparts`): same outcome for every base/candidate pair in both corpora (golden 4,346 distinct pairs, worldgaps 7,446). 465 pairs fail on both sides, all refractive variants under `$msaalight` (`sampler2DMS` isn't a 1.20 type); a planted `<<` fails every candidate.
- `shaders-selftest.ps1`: 9/9 steps pass (its `world.cfg` anchors are kept).
