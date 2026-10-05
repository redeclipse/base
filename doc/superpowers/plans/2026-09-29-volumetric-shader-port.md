# Volumetric Light Shader Port (results)

> Executed inline (no subagents) on branch `volumetric-shader-port`, stacked on
> `world-shader-port` (it uses `shared/gfetch.glsl`). Written after the fact as
> the record of decisions, proof and coverage gaps.

**Goal:** Replace the CubeScript GLSL generators in `config/glsl/volumetric.cfg`
(`volumetricvariantshader`, `volumetricbilateralvariantshader`) with plain GLSL
plus `#define`s, move code shared with other families into `config/glsl/shared/`,
and prove every configuration compiles to the same code.

## Decisions

1. **Inputs.** `volumetricvariantshader <name> <row> <type> <steps>` reads the
   row (spot: row >= 2; colour shadow: rows 1 and 4), the type letters (`p`,
   the filter letter; `P` and `s` only decide which rows `volumetricshader`
   registers), `$msaalight` (via `gfetchdefs`), `$gdepthformat` (via
   `gdepthunpack`) and `$usetexgather`. The bilateral reads its tap count,
   `$volreduce`, the direction, `$msaalight`, `$gdepthformat` and
   `$mintexrectoffset`/`$maxtexrectoffset`.
2. **Define contract.** `VOL_ROW`, `VOL_STEPS`, `VOL_SHADOW`, the filter
   letters under the deferred names (`SMFILTER_GATHER5` G, `_GATHER3` g,
   `_BILINEAR5` E, `_BILINEAR3` F, `_ROTATED` f, none for N),
   `SMFILTER_SINGLE`, `SMFILTER_COLOR`, `GFETCH_MS`, `GDEPTH_FORMAT`,
   `USETEXGATHER`; bilateral: `VOLBILATERAL_TAPS`, `BILATERAL_REDUCE`,
   `BILATERAL_X`, `TEXRECT_MINOFFSET`, `TEXRECT_MAXOFFSET`.
3. **Printed constants.** `@(divf 1.0 $maxsteps)` printed `%.6g` (integers as
   `1.0`); `1.0/float(n)` would round differently, so
   `volumetric/volumetric_steps.glsl` spells out all 64 values (generated).
   The bilateral's depth offsets are literal per reduction.
4. **Tap names.** The generator named taps `color<i>`/`depth<i>`/`weight<i>`
   with at most 3 taps a side, so per-offset macros take the names and
   `bilateral.frag` has one block per tap count: tokens unchanged (TEXT).
5. **Shared modules.** `shared/bilateral.glsl` (tap fetch macros,
   `BILATERAL_FITS`, `BILATERAL_DEPTHSCALE`, from `ao/bilateral.frag`),
   `shared/bilateral.vert` (was `ao/bilateral.vert`, identical tokens for the
   volumetric vertex stage), `SMFILTER_SINGLE` in `shared/smfilter.glsl` (the
   one-compare `filtershadow` macro) and `SMFILTER_COLOR` un-nested from
   `SMFILTER` so volumetric can use `filtercolorshadow` alone. The
   point/spot `getshadowtc`/`getspottc` differ from deferred's (no distance
   bias, different sign convention), so they stay in `volumetric.frag`.

## Proof

- **Generator corpora** `volgen` (old generator, recorded before any edit) /
  `volgen-cand`, from `home/uitest/volgen-calls.cfg`: filters N/f/F/E/g/G ×
  spot or not × `p`/`pP` × steps 1/16, plus steps 3/7/64; bilateral taps 1–3
  × reduce 0–2. Fresh client per state: gdepthformat 0/1/3 without MSAA;
  msaa 4 with `$msaalight` 3 (gdepthformat 1); msaa 4 with `$msaalight` 0
  (`msaapreserve -1`); `gX` = `$usetexgather` 2 through renamed copies of each
  generator (`volgen-override-{ref,cand}.cfg`). 1,792 pairs, all `TEXT`.
- **MSAA × depth format** `volgen-ref2` / `volgen-cand2`: the harness home
  persists `msaalineardepth 1`, which overrides `glineardepth` under MSAA on
  this GPU (`msaamaxdepthtexsamples` 32), so the states use
  `msaalineardepth 0`/`3` (gdepthformat 0/3 with `$msaalight` 3). Both sides
  ran through the override copies; the old copy reproduces `volgen`'s gB
  hashes exactly (224/224). 672 pairs (gB, m4h, m4l3), all `TEXT`.
- **Step table:** in-game `divf 1.0 n` for n = 1..64 matches
  `volumetric_steps.glsl` for all 64.
- **Mutation:** `getspottc` `1e-5`→`2e-5` and the reduce-1
  `VOLBILATERAL_DEPTH2`: exactly the 48 predicted rows DIFF (44 shadowed-spot
  rows 3/4, 4 bilateral shaders with taps >= 2 at reduce 1).
- **GLSL 1.20:** 1,120 pairs compiled at `#version 120` with an engine-like
  header: same outcome on both sides for every pair (the failures are
  `sampler2DMS` and `texture2DRectOffset`, identical in the old generator).
- **Golden `shaders.ps1 check`** (46 sids + 50 maps, no filter): 0
  FAIL/WEAK/PIXEL. Volumetric 209 `TEXT`; AO bilateral and deferred-light rows
  keep exactly the tiers of the deferred port's check; every other tier change
  is the world port's TEXT→SPIRV. 520 MISSING / 6,720 EXTRA model-shader rows
  (known, handoff §6 item 1a), hence exit 1.

## Coverage gaps (not proved)

1. **Other texture-rectangle offset limits.** `$mintexrectoffset`/
   `$maxtexrectoffset` come from the driver (-8/7 here), so only the
   offset-vs-plain fetch choices those limits produce were compared. Other
   limits pick branches through the same `BILATERAL_FITS` formula, which the
   AO bilateral has used since its port, but no corpus covers them. Provable
   the `$usetexgather` way (rename the vars in both generators) if wanted.
2. **Step counts in shaders.** Steps 1, 3, 7, 16 and 64 were compiled and
   compared; the other 59 are covered only by the table check above (the
   step count reaches the text only through `VOL_STEPSCALE` and the loop bound).
3. **Golden baseline.** The baseline holds volumetric shaders only from the
   per-map pass at default settings (`volumetricFsp16`, `volumetricFp16`,
   `volumetricFpP16`, `volumetricbilateral{x,y}21`); sweep points s21–s23
   change `volsteps`/`volbilateral`/`volreduce` but make no volumetric shader
   because only a map with volumetric lights generates them. Everything else
   rests on the generator corpora.
4. **Real 1.20 drivers.** 1.20 was checked with glslang only; the shaders that
   fail there fail identically before and after the port.
5. **Pixels.** Every pair was `TEXT`, so the pixel tier never ran; none was
   needed.

Corpora `home/uitest/shadercorpus/volgen*` and `home/uitest/volgen-*.cfg` are
scratch and deletable; the drivers were `volgen.ps1`/`volgen2.ps1`,
`volpairs.py`, `vol120.py` in the session scratchpad.
