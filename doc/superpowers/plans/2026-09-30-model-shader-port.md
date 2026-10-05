# Model Shader Port

> Executed inline (no subagents) on branch `model-shader-port`, from `master`
> 949694fd. Written before the code; the results section is filled in as the
> proof runs.

**Goal:** Replace the CubeScript GLSL in `config/glsl/model.cfg`
(`modelshader`, `rsmmodelshader`, `shadowmodelshader`, `halomodelshader` and
their helpers `skelanim*`, `windanim*`, `qtangentdecode`) with plain GLSL plus
`#define`s, move repeated text into shared modules, and prove every
configuration compiles to the same code.

## Inputs

- The type string, from `animmodel.h` `loadshader()`: `a`|`au`|`A`, `w`,
  `d`|`D`, `n`, `m`, `e`, one effect digit, `p`|`P`, `x`|`X`, `c` (in that
  order), plus `b` (skeletal vertex variants, 1-4 bones) and `t` (the
  transparent fragment variant) added by `modelshader`. `rsmmodelshader`
  gets `a`/`c`. Only effect `0` (`MDLFX_SHIMMER`) is reachable
  (`MDLFX_MAX` is 1); `1` is ported as written.
- Engine state: `$gdepthformat`, `$msaasamples`, `$usepacknorm`,
  `$debugvertcolors` (g-buffer model shader only), and for `b`
  `min($maxvsuniforms, $maxskelanimdata)` (both read-only vars).

## Decisions

1. **Define contract.** One `MODEL_*` define per type letter
   (`modeldefines`), `MODEL_BONES` for `b`, the engine vars as raw values
   (`GDEPTH_FORMAT`, `MSAA_SAMPLES`, `USEPACKNORM`, `DEBUG_VERTCOLORS`,
   `MAXVSUNIFORMS`, `MAXSKELANIMDATA`). The `min` is an `#if` in
   `skelanim.glsl`.
2. **Shared modules are directive-only** (one-line macros, like
   `gdepth.glsl`), invoked where the generator emitted the text, so token
   order is unchanged: vertex stages, `shadowmodel`/`halomodel` fragments stay
   `PASS-TEXT`.
   - `shared/rotateuv.glsl`: `ROTATEUV_FUNC` (the `rotateuv` alias in
     `init.cfg`, which `config/comp/misc.cfg` still uses).
   - `model/model_defs.glsl`: derived switches (`MODEL_PATTERNED`,
     `MODEL_MIXED`, `MODEL_MATERIAL4`, `MODEL_DECALED`, `MODEL_EFFECT`),
     `MODEL_QDECODE` (`qtangentdecode`), `MODEL_TEXCOORD` (scrolled/rotated
     `texcoord0`, 4 copies).
   - `model/skelanim.glsl`: `SKELANIM_DECLS`, `SKELANIM_BLEND` (per bone
     count), `SKELANIM_POS`, `SKELANIM_QUAT`.
   - `model/wind.glsl`: the `WIND_*` constants, `WIND_DECLS(proj)`,
     `WIND_FUNCS`, `WIND_ANIM(proj)`.
   - `model/effect.glsl`: `MODEL_EFFECT_DECLS`, `MODEL_EFFECT_RAND`,
     `MODEL_EFFECT_NOISE` (model and halo fragments).
   - `shared/gcolor.glsl`: `GGLOW_PACK_WEIGHT`, the glow-less `gglowpack`.
   - `shared/gdepth.glsl`: `GDEPTH_HASH_ALPHA` (`ghashdepth` with an alpha).
   - `shared/gbuffer.glsl`: `GBUFFER_PACK_DEPTH_ALPHA`,
     `GBUFFER_PACK_DEPTH_HASH_ALPHA` (`gdepthpackfrag` with an alpha);
     `GBUFFER_PACK_GDEPTH` becomes `GBUFFER_PACK_GDEPTH_ALPHA(0.0)` (same
     tokens).
   - In `model.frag`: `MODEL_MATSPLIT(v)` and `MODEL_MATMASK(buf, mask)`, the
     two material blends the mixer and pattern paths each repeated.
3. **The one CubeScript branch:** `skelanim.glsl` is included only for `b`.
   It carries the `animdata` uniform pragma, which `genuniformlocs` reads from
   the raw text whatever `#if` surrounds it.
4. **Outputs:** the g-buffer model uses `(gbufferoutputs)`, `rsmmodel` uses
   `shared/rsm_out.glsl`. Both move the outputs ahead of the other
   declarations, so those fragments are `PASS-SPIRV` (the world port's
   ruling), proved by a token check that the move is the only difference.
5. **Registration unchanged:** row 0 skeletal vertex variants, row 1 the `t`
   fragment variant plus the four reuse links (`variantshader ... "0 , i" 1`,
   which carry no GLSL), `maxvariants` 9 for `modelshader`.

## Proof

- **Generator corpora** `mdlgen` (old generators, recorded first) /
  `mdlgen-cand`: `home/uitest/mdlgen-calls.cfg` in a fresh client per engine
  state (`mdlgen.ps1`, scratchpad): state `f0` (gdepthformat 0) runs the
  fragment factorial, 1296 `modelshader` types (alpha x masks/env x effect x
  pattern x mixer x normal map x cullface, `w` and the decal letter crossed
  by a scrambled index); states `g1`, `g3`, `m4` (msaa 4, depth format 1),
  `m4g0`, `m4g3`, `pn`/`pn3` (forcepacknorm), `dv`/`dvm` (debugvertcolors)
  run a 131-type subset. Every state also has the 4 `rsmmodel` types and the
  9 standard shadow/halo shaders. Pairs by name through
  `shadercheck.py --pairs`.
- **Token check** for the SPIRV rows: identical once the output declarations
  are removed.
- **Offline:** `skelanim.glsl`'s size `#if` with `MAXVSUNIFORMS` below
  `MAXSKELANIMDATA` (not reachable on this GPU) through `glslang -E`.
- **Mutation:** one constant in a shared macro; exactly the predicted rows DIFF.
- **GLSL 1.20:** glslang at `#version 120`, same outcome per pair.
- **Golden** `shaders.ps1 check -Filter '*model*'`.

## Results

- **Generator corpora:** `mdlgen` and `mdlgen-cand` both recorded with no
  GLSL errors. 25,400 model-family pairs: 450 `TEXT` (the 9 shadow/halo
  shaders x 5 rows x 10 states) + 24,950 `SPIRV` (every `model<type>` and
  `rsmmodel<type>` row, since each contains `model.frag`/`rsmmodel.frag`), 0
  DIFF/NA. The 10,220 non-model rows of the same dumps are all `TEXT`, so the
  `gbuffer.glsl` change (`GBUFFER_PACK_GDEPTH` via `_ALPHA(0.0)`) left world
  and the other families' tokens alone. 240 rows are invalid on both sides
  (model shaders the client generated at startup, which `resetgl`
  invalidated), and 20 rows exist only in the candidate: `modelanme0`/
  `modelnme0PX` at f0, generated by the client itself, with no reference to
  pair against.
- **Token check** (`mdltok.py`, scratchpad): all 24,770 distinct `SPIRV`
  pairs have identical vertex tokens, and fragments that are identical once
  the output declarations are set aside, with the same declarations in the
  same order.
- **skelanim size:** `glslang -E` gives `animdata[100]` for
  `MAXVSUNIFORMS` 100 / `MAXSKELANIMDATA` 192, `[192]` for 192/192 and
  1024/192.
- **Mutation:** `WIND_DETAIL2_ZSWAY` 0.75 -> 0.76 at state g1: exactly the 655
  predicted rows DIFF (64 wind model types x 10 rows + 3 wind shadow/halo
  shaders x 5), nothing else. Reverted.
- **GLSL 1.20** (`mdl120.py`, scratchpad: `#version 120`, no
  `EXT_gpu_shader4`, `fragdata` turned into `gl_FragData` defines): all 24,815
  distinct pairs compile on both sides, both stages. A planted `1 << 2` fails
  all 255 sampled candidate stages, so the check is live.
- **Golden** `check -Filter '*model*'` (46 sids + 50 maps): 21,176 configs,
  2,116 TEXT, 11,800 SPIRV, 0 pixel/weak/fail; 520 MISSING / 6,740 EXTRA, the
  known model-shader noise (handoff §6 item 1a, the same counts as the
  deferred/volumetric full checks), hence exit 1.

## Coverage gaps (not proved)

1. Not every one of the 7,776 reachable type strings was compiled: the f0
   factorial covers every fragment letter combination, but `w` and the decal
   letter only by a scrambled index, and the other states use a 131-type
   subset. The letters are independent `#ifdef`s, and the token check shows
   no stray text.
2. `MAXVSUNIFORMS < MAXSKELANIMDATA` only through `glslang -E`: both vars are
   read-only.
3. Effect `1` has no `MDLFX` value; it was proved only through the direct
   calls.
4. Real 1.20 drivers: checked with glslang only.
5. Where the engine inserts `precision highp float;` below GLSL 1.50: in a
   `shadowmodel` fragment without `a`, the first declaration is now inside
   `#ifdef MODEL_ALPHATEST`, so the statement is compiled out with it. It does
   nothing on desktop GL (see "Porting a generator").

Corpora `home/uitest/shadercorpus/mdlgen*` and `mdlmut`, and
`home/uitest/mdlgen-calls.cfg`, are scratch and deletable.
