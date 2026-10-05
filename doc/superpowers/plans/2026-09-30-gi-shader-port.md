# Radiance Hints (GI) Shader Port (results)

> Executed inline (no subagents) on branch `gi-shader-port`, from `master`
> 2c150e1c. Written as the record of decisions, proof and coverage gaps.

**Goal:** Replace the CubeScript GLSL in `config/glsl/gi.cfg` (`rsmsky`,
`radiancehintsshader`, `radiancehintsborder`, `radiancehintscached`,
`radiancehintsdisable`) with plain GLSL plus `#define`s, move repeated text
into includes, and prove every configuration compiles to the same code.

## Decisions

1. **Inputs.** Only `radiancehintsshader <taps>` is a generator, and it reads
   nothing but its argument (`$rhtaps`, 0..32, from
   `loadradiancehintsshader`). The other four are fixed `lazyshader`s and
   become `lazyshader_new`.
2. **Define contract.** `RH_TAPS` (the raw argument). `radiancehints.frag`
   maps it to the tap count the generator's `cond` picked (12/20/32) and to
   the printed `@(+f $numtaps)` literal (`12.0`/`20.0`/`32.0`) with an `#if`
   chain, so no arithmetic reaches the text.
3. **Taps.** The unrolled `calcrhsample` calls are written out, one line per
   tap, in one `#if` block per tap count, generated from the `rhtapoffsets*`/
   `rsmtapoffsets*` tables. A per-tap macro can't take the offsets: they
   contain commas.
4. **Shared text.**
   - `shared/rsm_out.glsl`: the RSM outputs `gcolor`/`gnormal` (`rsmsky`; the
     same pair as `world/rsm.frag` and the model RSM shaders). `rsm.frag` is
     left alone so `rsmworld` stays `PASS-TEXT`.
   - `gi/rh_out.glsl`: the four radiance hint outputs and `RH_ZERO`
     (`vec4(0.5, 0.5, 0.5, 0.0)`, an empty hint), for all four
     `radiancehints*` fragment stages. `RH_ENCODE` (radiancehints.frag) is
     the average-and-bias of the gathered sums.
   - `gi/rhslice.vert`: the identical vertex stage of `radiancehintsborder`
     and `radiancehintscached`.
5. **Expected tiers.** `rsmsky` and `radiancehintsdisable` declared their
   outputs first, so the include keeps the token order: `PASS-TEXT`.
   `radiancehints<n>`, `radiancehintsborder` and `radiancehintscached`
   declared uniforms first; the include moves the outputs ahead of them
   (the world port's ruling), so `PASS-SPIRV`.

## Proof

- **Generator corpora** `gigen` (old generator, recorded before any edit) /
  `gigen-cand`, from `home/uitest/gigen-calls.cfg`: `radiancehintsshader`
  for every `$rhtaps` value 0..32 (the old corpus has exactly three
  fragment hashes, split at 12/13 and 20/21), plus the four lazy shaders,
  fresh client each side. 37 pairs: `rsmsky` and `radiancehintsdisable`
  `TEXT`, the other 35 `SPIRV`, as predicted.
- **Token check** (`gitok.py`, scratchpad): for every pair the vertex tokens
  are identical, and the fragment tokens are identical once the output
  declarations are removed; the declarations are the same list in the same
  order. The include's move is the only difference.
- **Mutation:** one offset of the 20-tap table (`0.0540788` → `0.0540789`):
  exactly the 8 predicted rows (`radiancehints13`..`20`) DIFF, the rest
  unchanged. Reverted.
- **GLSL 1.20:** all 74 stages compiled with glslang at `#version 120` with
  the engine's pre-1.30 header (outputs as `gl_FragData[n]`): every stage
  compiles on both sides.
- **Golden `shaders.ps1 check -Filter 'r[as]*'`** (46 sids + 50 maps): 357
  configs, 184 TEXT, 173 SPIRV, 0 pixel/weak/fail/missing/extra, exit 0.
  `radiancehints0`/`20`/`32` (sweep points s10/s11 and the maps),
  `radiancehintsborder`/`cached` SPIRV at every sid; `radiancehintsdisable`,
  `rsmsky` and the untouched `rsmworld` TEXT. The baseline's valid
  `radiancehints*` hashes equal `gigen`'s.

## Coverage gaps (not proved)

1. **Pixels.** Every pair passed at TEXT or SPIR-V, so the pixel tier never
   ran; none was needed.
2. **Real 1.20 drivers.** Checked with glslang only.

Corpora `home/uitest/shadercorpus/gigen*` and `home/uitest/gigen-calls.cfg`
are scratch and deletable.
