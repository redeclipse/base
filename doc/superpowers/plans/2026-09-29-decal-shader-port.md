# Decal Shader Port Implementation Plan

> Executed inline (no subagents), one commit per task on branch `decal-shader-port`.

**Goal:** Replace the CubeScript GLSL generator `decalvariantshader` (`config/glsl/decal.cfg`) with plain `config/glsl/decal/decal.vert`/`.frag` plus `#define`s, move the g-buffer packing helpers it uses (`gspecpack`, `gglowpack`, `gnormpack`) into shared includes, and prove every configuration compiles to the same code.

**Architecture:** The pattern in `doc/shader-reference.md`, "Porting a generator". `decal.cfg` keeps `decalshader` (registration, `defershader`, the editor list `decalshaders`, `finddecalshader`) and a thin `decalvariantshader` that stages the slot params and calls `variantshader_new`. No C++ changes.

## Decisions and evidence

1. **Inputs.** `decalvariantshader <name> <type>` reads its type letters and `$usepacknorm` (pass 1 outputs and `gnormpack`). `decalshader` reads `$maxdualdrawbufs` when the deferred shader is forced. Letter `e` only sets `SHADER_ENVMAP`; `o` (in a few type strings) is read by nothing.
2. **The define contract.**

   | Define | From | Meaning |
   |---|---|---|
   | `DECAL_REFLECT` | `r` | envmap reflection |
   | `DECAL_REFLECT_SPECMAP` | `R` | reflection scaled by the spec map |
   | `DECAL_SPEC` | `s` | spec |
   | `DECAL_SPECMAP` | `S` | spec map |
   | `DECAL_NORMALMAP` | `n` | normal map |
   | `DECAL_PARALLAX` | `p` | parallax |
   | `DECAL_GLOW` | `g` | glow |
   | `DECAL_PULSEGLOW` | `G` | pulse glow |
   | `DECAL_KEEPNORMALS` | `b` | leave the g-buffer normals alone |
   | `DECAL_DISPLACE` | `v` | displacement |
   | `DECAL_PASS0` / `DECAL_PASS1` | `0` / `1` | dual-source pass; neither = single pass |
   | `USEPACKNORM` | `$usepacknorm` | 0/1 |

3. **Output declarations cannot be `#if`'d.** `findfragdatalocs` (`src/engine/shader.cpp`) scans the raw fragment text for `fragdata(`/`fragblend(`, ignoring the preprocessor, and so does the harness contract (`scanfragdatalocs` → `fragdata` lines in `meta.txt`). Below GLSL 1.30 without `EXT_gpu_shader4` each scanned name becomes `#define <name> gl_FragData[<n>]`, so two textual `gnormal` declarations in different `#if` branches would redefine a macro. The four output sets therefore live in four includes, and the alias picks one (`decal/out_*.glsl`): the one exception to "all branching in GLSL". No comment may contain `fragdata(` or `fragblend(`.
4. **Shared modules** (one-line macros, same tokens as the aliases):
   - `shared/gnormal.glsl` (define `USEPACKNORM`): `GNORMAL_PACK(n)` = `gnormpack n`, `GNORMAL_PACK_BLEND(n, k)` = `gnormpack n k`.
   - `shared/gcolor.glsl`: `GSPEC_PACK(gloss)` / `GSPEC_PACK_SPEC(gloss, spec)` = two-argument `gspecpack`; `GGLOW_PACK(glow)` = `gglowpack glow`, with `GGLOW_PACKNORM` for the `packnorm` it used to `#define`. The blend-layer forms of `gspecpack` and the glow-less `gglowpack` (world/model) are left to those ports.
5. **Coverage.** The golden baseline forces every registered decal shader at every sweep point: 92 parents + 81 pass-1 variants (the non-`b` types) = 173 rows per sid, with `usepacknorm` 0 at s00 and 1 under MSAA (s01–s04, s40). The single-pass path (`$maxdualdrawbufs` 0) is unreachable here and the var is read-only, so the proof calls `decalvariantshader` directly (pure-generator proof, as for FXAA/SMAA): for each of the 92 types, dual (`<type>0` parent + `<type>1` variant unless `b`) and single pass, under `forcepacknorm` 0 and 1; `shaderdumpall decalgen s00 0` before, `decalgen-cand` after, pairs through `shadercheck.py --pairs`.
6. **Expected tiers:** every decal row `PASS-TEXT` (only `#define`s moved and `#if`s replaced CubeScript), registry identical. No other family changes.

## Tasks

### Task 1: Reference corpus
- [x] `harness.ps1 start`; run the decision-5 calls against the old generator; `shaderdumpall decalgen s00 0`. Row count and hashes present; log free of compile errors (note any, they are the reference's too).

### Task 2: Shared modules
- [x] Add `shared/gnormal.glsl`, `shared/gcolor.glsl`. Commit `glsl: share the g-buffer normal, spec and glow packing macros`.

### Task 3: Decals
- [x] `decal/decal.vert`, `decal/decal.frag`, `decal/out_*.glsl`; replace `decalvariantshader` in `decal.cfg`.
- [x] Commit `glsl: move decals into config/glsl/decal`.

### Task 4: Prove it
- [x] Candidate gen corpus from a fresh client; `shadercheck.py --pairs`: every pair `TEXT`.
- [x] Restart; `shaders.ps1 check -NoMaps -Filter '*decal'` over s00, s01, s99 and the full sid list: decal rows `PASS-TEXT`, registry unchanged.
- [x] Mutation run: change one constant in the pulse-glow path; exactly the `G` rows lose TEXT.
- [x] GLSL 1.20: preprocess the candidate texts at `#version 120` with glslang (no integer operators, no trailing backslashes).

### Task 5: Docs
- [x] `doc/shader-reference.md`: decal files, the two shared modules, the `fragdata(` rule. Commit `doc: describe the decal shader files and shared packing includes`.
- [x] `doc/agent-handoff.md` and memory (not committed).
