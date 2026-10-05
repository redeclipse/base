# Global illumination (radiance hints): findings

_2026-10-01. Scope: the sunlight GI, meaning the radiance hints (RH) volume and its reflective
shadow map (RSM) (`src/engine/renderlights.cpp`, `config/glsl/gi/`), and how the deferred light
pass reads it (`config/glsl/deferred/deferredlight.frag`, `deferredlight_defs.glsl`)._

This began as research only; fixes 1 and 2 have since been implemented (status table below). The user reported visible pop-in at the split
boundaries and wanted temporal accumulation to smooth it, and possibly to steady the GI when the
sun is animated. The investigation found that temporal accumulation can't fix the split pop
alone, and that it is the right tool for the animated sun.

Status of the recommendations (§5):

| # | Item | Status |
|---|---|---|
| 1 | Size the splits from the unzoomed fov | **Implemented** on branch `rh-split-stability` (not merged): [specs/2026-10-01-rh-split-stability-design.md](superpowers/specs/2026-10-01-rh-split-stability-design.md) |
| 2 | Continuous crossfade between splits, with a centre clamp | **Implemented** on branch `rh-split-stability` (not merged); the crossfade is off by default, `rhblend 2` to try it |
| 3 | Temporal accumulation in the volume | Open, design notes in §6 |
| 4 | Keep the history when the sun changes | Open, §6 |
| 5 | Jittered taps and RSM | Open, optional, §6 |
| 6 | Less forward-biased or smoothed split centres | Partly covered by the centre clamp in 2 |
| 7 | No-GI areas in every split | Open, small |
| 8 | Camera feeds and mapshots thrash the RH splits | Open, new finding (§4.4) |
| 9 | Near-field definition: dense gather, falloff tied to cell size, deringing, finer near split | Experiment only (§8), nothing changed in the repo |

---

## 1. How the RH system works

| Piece | Where | Behaviour |
|---|---|---|
| Entry | `renderradiancehints`, `renderlights.cpp:4427` | `rh.setup()` and `rsm.setup()` every frame. The RSM and the slices are re-rendered only if `rhforce`, a dynamic bounds region (`rhdyntex`/`rhdynmm`, both off by default) or `!rh.allcached()` (some split's centre moved) applies. Otherwise it costs nothing. |
| Split distances | `radiancehints::updatesplitdist`, `:2508` | Mix of logarithmic and linear splits (`rhsplitweight` 0.6) between `rhnearplane` and `rhfarplane` (map vars, defaults 1 and 1024). |
| Split placement | `radiancehints::setup`, `:2521` | A cube around `calcfrustumboundsphere(near, far, camera1->o, camdir)` (`rendergl.cpp:1432`), radius rounded up (`ceil`), centre snapped to whole cells. `split.cached = split.bounds == pradius ? split.center : invalid` (`:2538`): the cache survives only if the rounded radius is unchanged. |
| RSM | `reflectiveshadowmap::getprojmatrix`, `:2395` | **One** RSM for the whole RH range: sphere radius plus `gidist`, `rsmsize`² (default 384), snapped to whole texels **in light space**. Stores N·L × albedo (`config/glsl/model/rsmmodel.frag:23`, world equivalent), so the **sun colour is not baked in**: it is applied afterwards in `deferredlight.frag:266`. |
| Volume | `setupradiancehints`, `:1465` | Four 3D textures (SH for R, G, B, plus sky occlusion), size `(G+2)² × (G+2)·N` with `G = rhgrid` (27) and a 1-cell border. **All splits are stacked in z in the same textures**, clamped in all three dimensions. `rhprec 0` (default) gives **RGBA8**; `rhprec 1` gives RGBA16F. With `rhcache 1`, `rhtex[4..7]` is a ping-pong copy of the previous frame, swapped at the start of `renderslices` (`:4117`). |
| Slice rendering | `radiancehints::renderslices`, `:4108` | Coarse split first (`loopirev`). Per z-slice: the border ring from the next split (`radiancehintsborder`), then for the cells that overlap last frame's box an exact copy with the integer-cell offset (`radiancehintscached`), and for new cells a full gather (`radiancehints<taps>`). A split whose centre hasn't moved and whose coarser neighbour didn't change is skipped, or bulk-copied once with `glCopyImageSubData` (`:4195`). |
| Gather | `config/glsl/gi/radiancehints.frag` | A **fixed** pattern of 12, 20 or 32 taps (`rhtaps` is rounded up), each with a 3D offset in the cell (× cell radius) and a 2D offset in the RSM (× `gidist·rsmspread`). Each tap point-samples the RSM (`sampler2DRect`). Encoding: `sh·(0.5/NUMTAPS) + 0.5`, with the weight in alpha; `RH_ZERO = (0.5,0.5,0.5,0)`. All four alpha channels carry data, so there is **no free channel** for an age or confidence value. |
| No-GI areas | `rendernogi`, `:4391` | Zeroes the cells inside no-GI material, **split 0 only** (`if(i) continue`). |
| Lookup | `getrhlight`, `deferredlight.frag:57`; `DL_RH_SPLIT`, `deferredlight_defs.glsl:150` | The **first** split whose box contains the point (`rhbounds = 0.5·(G+1)/(G+2)`, half a cell into the border), else `tc = vec3(4.0)`. No blending between splits. Position nudged by `rhnudge` along the normal. |
| Shader option | `renderlights.cpp:2770`, `config/glsl/deferred.cfg:61` | `r<N>` in the sun type string defines `DL_RH` and `DL_NUMRH`. |
| Invalidation | `clearradiancehintscache`, `:2502` | Called by most `rh*`/`rsm*` vars, by `setlightdir` (`src/engine/light.cpp:8`, every sun yaw or pitch change), by `sunlight`/`sunlightscale` changes, and at `renderlights.cpp:2108`. |

### Geometry at the defaults

16:9, `rhgrid 27`, `rhsplits 2`, `rhfarplane 1024`. Split 0's far plane is about 225u. In this
range `calcfrustumboundsphere` takes the "centre at the far plane" branch.

| | fov 90 | fov 100 (`firstpersonfov` default) |
|---|---|---|
| Split 0 half-size / cell size | 259u / 19.2u | 308u / 22.8u |
| Split 0 centre ahead of the camera | 225u | 225u |
| **Camera inside split 0's back face by** | **34u (1.8 cells)** | **83u (3.6 cells)** |
| Split 1 half-size / cell size | 1175u / 87u | about 1400u / 104u |
| Split 1 centre ahead of the camera | 1024u | 1024u |
| RSM texel (radius + `gidist` 384) | about 8u | about 9u |

`thirdpersonfov` defaults to 120, which gives a much larger split 0 (about 447u, cells about 33u).
Maps override `rhfarplane` and `gidist`: `park` has `gidist 1000`, `giscale 2`, `rhfarplane 1500`,
sun pitch 45 and yaw 35.

---

## 2. Why the splits pop

1. **The boundary moves in whole-cell steps.** When split 0's snapped box shifts one cell
   (about 20u), a slab of surfaces switches instantly between about 90–100u cells and about 20u
   cells. The two solutions differ: cell size, tap spread over the cell, and sampling bias.
   The 1-cell border only blends spatially at one instant. It does nothing about the step in
   time.
2. **New cells are computed fully in one frame.** There is no history, so a value appears at
   full strength.
3. **Placement depends on view direction.** The boxes sit far ahead of the camera, so a 180°
   turn moves split 0 by about 450u (about 23 cells) and split 1 by about 2048u. Turning around
   rebuilds nearly the whole volume, and the area you now face goes from coarse to fine in one
   frame.
4. **Zoom wipes the whole cache.** `game::fixview` (`src/game/game.cpp:3051`) animates `curfov`
   during a zoom. The sphere radius changes, `split.bounds == pradius` fails, and every split is
   invalidated and resized **on every animation frame**. The map-start reveal
   (`src/game/hud.cpp:1479`) does the same.
5. **No-GI is in split 0 only.** No-GI areas visibly change as they cross the split boundary.

**Key conclusion:** temporal accumulation in the volume can make *entering* cells fade in, but it
can't smooth *leaving* ones. When a surface drops out of split 0, the lookup switches to split 1,
which never had the fine values. Pop-out needs a spatial crossfade, which is fix 2. The crossfade
has to use the *unsnapped* centre to be continuous in time, and it needs a centre clamp because
of the small camera margin in the table above.

## 3. Why the animated sun is unstable

- There is **no built-in sun animation**. Sun yaw and pitch are map vars, changed by
  `setenv`/map scripts (`config/tool/templates/env.cfg`) or the editor (`getsundir`,
  `config/engine.cfg:370`). Any per-frame animation goes through `setlightdir` →
  `clearradiancehintscache`, which **discards all history**. Each frame is then a full rebuild:
  the RSM and every cell, with no cached copy.
- RSM texel snapping (`getprojmatrix`) only steadies **translation** in light space. When the
  light rotates, the model matrix rotates, the about 8u texels land on the geometry differently
  each step, and the fixed, point-sampled taps hit different texels. The GI shimmers.
- Sun colour and brightness, including `game::darkness(DARK_SUN)` (`light.cpp:36`), don't affect
  the RH data and need no rebuild.
- The current animated-sun cost is therefore already "full rebuild every frame". A temporal
  scheme that updates every frame while the sun moves costs no more than that.

## 4. Engine facts and side findings

### 4.1 Precision

`rhprec 0` (the default) stores the volume as RGBA8. An exponential moving average stalls in
8 bits: an update with `α·Δ < 0.5/255` never moves the value, so with α = 0.2 the result can stay
stuck up to about 2.5/255 away in encoded units. **Temporal accumulation needs RGBA16F.** At the
default grid, 16F costs about 3.1 MB for both ping-pong sets: `29·29·58` texels × 8 B × 8
textures.

### 4.2 Cost (measured)

`park`, 1600×900, defaults (grid 27, 2 splits, `rsmsize 384`), on the user's machine:

| Setup | Result |
|---|---|
| `rhforce 1`, `rhinoq 1` (default) | "Radiance Hints (gpu)" line **missing**. "G-Buffer (gpu)" goes from about 0.9 to 2.97 ms. |
| `rhforce 1`, `rhinoq 0`, `rhtaps 20` | Radiance Hints (gpu) **0.43 ms** |
| `rhforce 1`, `rhinoq 0`, `rhtaps 12` | Radiance Hints (gpu) **0.41 ms** |
| `rhforce 0`, `rhinoq 0` | 0.00 ms on the captured frame (no split moved that frame) |

- A full rebuild every frame is cheap. **The tap count barely matters:** the cost is RSM
  rasterisation plus 58 slice re-bindings × 4 colour attachments, not the gather. Reducing taps
  won't save time. Layered rendering (`gl_Layer`) would cut the binding overhead, but that is a
  separate change.
- **Profiling trap:** with the default `rhinoq 1`, RH runs inside the occlusion-query part of the
  G-buffer pass (`renderlights.cpp:5006`). GPU timer queries can't nest, so the RH GPU time is
  charged to "G-Buffer (gpu)". Profile RH with `rhinoq 0`.

How these were measured: `tools\harness\harness.ps1 start -Width 1600 -Height 900`, then
`send 'hideallui; map park'`, `send 'timer 1; rhforce 1; rhinoq 0'`, `shot <name> -Settle 2000`,
and crop the top-left about 210×220 px of the PNG to read the timer overlay. Timers only appear
in that HUD overlay, never in the log. CPU timers have whole-millisecond resolution.

### 4.3 Harness notes for GI work

- **A `map <name>` load in the harness starts demo mode: the camera moves.** It is not a static
  camera. Any "baseline" frame is just whatever frame you caught. For repeatable work use the
  editor harness (`editor.ps1 open <map>`, `edframe`) so each screenshot depends only on the
  given camera.
- `curfov` is not a CubeScript variable (`Unknown alias lookup: curfov`), and `fov` is an alias.
  The real values are `firstpersonfov`, `thirdpersonfov`, `editfov` and `specfov`.
- To make GI differences stand out against direct light, raise `giscale` (map var) during a
  test.

### 4.4 Camera feeds and mapshots thrash the splits (not fixed)

`ViewSurface::render` (`src/engine/renderfx.cpp:1020`) runs `gl_drawview` → `renderradiancehints`
with its own camera and fov. It is used by UI camera feeds (`src/engine/ui.cpp:3709`, every
`viewportuprate` ms, default 50) and mapshots (`worldio.cpp:749`). The RH split state is global,
so each feed render moves the splits and forces rebuilds, and the main view rebuilds again on its
next frame. The main view's lighting stays correct, because it calls `setup` again before
lighting; only performance and stability suffer. Possible fixes: skip RH for secondary views, or
give them their own RH state. Envmap generation (`rendergl.cpp:2221`, fov 90) does the same, but
only once, at load.

### 4.5 Smaller notes

- The 3D textures clamp, and splits share the z range, so a fetch outside a split's own bounds
  reads a *different split's* layers. Any new lookup code must keep the bounds test, as
  `DL_RH_SPLIT` does.
- `rhrect` (renderbuffer path) and the non-`hasCI` path are legacy fallbacks. A temporal scheme
  should support only `!rhrect && rhcache`.
- `radiancehintsborder` reads the current frame's coarser split while the same 3D texture is the
  render target (different layers). This already works today.
- The AO term threshold `rhaothreshold` uses split 0's cell size for every split
  (`renderlights.cpp:4127`).

---

## 5. Recommendations, in order

1. **Size the splits from the unzoomed fov.** See the spec.
2. **Crossfade between splits in `getrhlight`**, using the unsnapped centre, with a centre clamp
   that keeps a fully fine margin around the camera. See the spec.
3. **Temporal accumulation in the RH volume**, not in screen space (§6).
4. **Keep the history across sun changes.** Separate "layout changed" (reset) from "lighting
   changed" (mark dirty, keep history, accumulate). Reset anyway on a large per-frame change.
5. **Optional jitter.** Offset the tap patterns per frame (wrapped to stay inside the cell and tap
   square) and offset the RSM projection by a fraction of a texel, as TAA does, so the accumulated
   result converges instead of shimmering.
6. **Less forward-biased or smoothed split centres** to reduce rebuilds on rotation. Fix 2's
   centre clamp does part of this.
7. **No-GI in every split** (`renderlights.cpp:4391`).

**Rejected: screen-space temporal accumulation (TAA-style).** The pop happens in world space.
The GI term is already merged into the light accumulation, and a screen-space history would need
reprojection and disocclusion handling and would ghost. The volume is about 49k cells; the screen
is about 1.4M pixels.

## 6. Temporal accumulation: design notes (items 3–5, not yet specced)

- **Blend:** each updated cell writes `mix(history, fresh, α)`, with `α = 1 − exp(−dt/τ)` so the
  result doesn't depend on frame rate.
- **History source:** for a cell that was in last frame's box, the ping-pong texture at the
  integer-cell offset. This is the same maths as `radiancehintscached`: an exact copy with no
  resampling, because the lattice is anchored in world space (`smin` is a multiple of the cell
  size). For a new cell, the **coarser split** sampled at the cell's position (the
  `radiancehintsborder` maths), so it starts from what the pixel already showed. For new cells in
  the coarsest split: the fresh value.
- **Keep updating while settling:** today static cells are computed once and frozen. Keep a
  per-split counter of frames since the last change and stop once it passes the convergence time
  (about 25 frames at α = 0.2). A static scene then returns to zero cost and, without jitter,
  converges to exactly today's output.
- **Deterministic taps:** re-running the same fixed taps gives the same value, so without jitter
  the blend is a pure fade from the seed to the fresh value. That is enough for pop-in. Jitter is
  only needed for noise or aliasing reduction (the sun case).
- **Precision:** needs RGBA16F (§4.1).
- **Paths:** only the ping-pong path (`!rhrect && rhcache`).
- **Ghosting:** a large discontinuous lighting change (an editor jump, a map variant switch)
  should reset the history instead of smearing it over τ. Alternatively, as in DDGI, raise α per
  cell when `|fresh − history|` is large.
- **Interaction with fix 2:** the crossfade already hides entering cells (they appear at weight
  0). Temporal accumulation then mostly serves the sun case and the fine-versus-coarse value
  difference during the crossfade.

## 8. Near-camera definition (experiment, 2026-10-04)

The user's map `gitest`: a closed 96×96×48u room (interior x,y 400..496, z 512..560) with one
32×32u opening in the −X wall, sun yaw 126°, pitch 11.7°, `gidist 384`. The sunlit floor
(`dziq/gypsum_wall`, nearly white) gave little visible bounce, and at `giscale` ×10 the bounce
looked blotchy and flat. Nothing in the repo was changed; the throwaway files are in
`home/rhexp-nearfield/` (gitignored).

### Method

- **Probes, not screenshots:** `rhprobe` on a 4u lattice over all six interior surfaces (2550 points),
  camera fixed with `edgoto 488 408 528; edaim 52 0`. Repeat runs match bit for bit.
- **Leak control:** the same probes with the opening filled (`edselbox 384 416 512 2 4 4; edfillsel 1`).
  Whatever is left inside a sealed room is light through the walls.
- **Reference:** single-bounce sun irradiance traced with `edraycast` at 23 surface points (cosine-weighted
  rays, a sun-visibility ray at each hit, albedo 1.0 for the floor and 0.5 for the default checker).
  Scored as `shapeErr`, the geometric RMS ratio RH/reference after one best-fit scale (1 is perfect;
  the reference's own Monte Carlo noise is roughly 1.1–1.2).
- **Variants without a rebuild:** a home-dir `config/glsl/gi.cfg` override (`findfile` searches the home
  dir first) points `radiancehintsshader 29` at an experimental fragment shader whose parameters come
  from an included `params.glsl`. Traps: `shader_new` skips a shader that is already loaded, and
  `resetshaders` re-runs `config/glsl.cfg`, which resets an alias override defined any other way.
  `shader_source` only accepts paths under `config/glsl/`.

### Findings

1. **The stock gather is sparse.** 20 point taps cover a ±57.6u window (`gidist·rsmspread`) of 9u RSM
   texels, about 12% of the window. Neighbouring cells sample different texels, which is the blotchiness:
   a dense read of every texel in the same window cuts the roughness metric from 11.6% to 3.8%.
2. **The falloff is flat at room scale.** `1/(0.1 + d²/gidist²)` is 1/d² softened over
   √0.1·gidist ≈ 121u, so inside a 96u room the bounce barely depends on distance. The walls next to the
   sunlit floor come out 3.5–7× too dark against the reference (e.g. +Y wall near the patch: 0.030
   reference, 0.004 stock). This is the missing definition. Tightening the softening to 15–30u (about
   a split-0 cell) restores it, but only with the dense read, because sparse taps plus a sharp kernel
   are noisy.
3. **A sharp kernel makes the L1 SH ring.** The lookup's L1 weight is twice the
   Ramamoorthi one (`*2.0` in `getrhlight`), so a strong nearby source makes the opposite direction go
   negative. It clamps to 0 and leaves dark, sky-tinted holes. Capping `|L1| ≤ 0.4·L0` per channel at
   gather time removes them (holes 4% → 0.4%) with no loss in shape.
4. **Leak:** 30% of the stock interior GI on average, and over 90% at the ceiling next to the −Y wall.
   Widening the window doesn't add real bounce beyond 2× here (the low sun already puts the whole room in
   the window), but leak grows 2–4×. Occlusion is still the fix for that (VXGI evaluation, occupancy
   volume).
5. **RSM stores albedo·N·L**, but a light-space texel's flux is albedo × texel area, so grazing-lit
   surfaces (the floor at 11.7° sun) count about 5× too little. Dividing by N·L (`rsmdir` is already a
   uniform) is physically right but also boosts the sunlit ground outside, and so the leak. Its effect on
   `shapeErr` was within noise here.
6. **Finer cells alone make it worse.** `rhsplits 3` with the stock gather: shapeErr 2.18 → 3.14, roughness doubles.
   The finer cells expose the sampling noise. They only help together with 1–3.
7. **Bounce level:** at the default `giscale 1.5` the stock mean is about the physical single bounce
   (best-fit scale 1.8). The room looks dark because there is one bounce only (a 0.5-albedo room would
   roughly double it with more), and the exposure follows the bright outside.
8. **Cost** (`rhforce 1`, `rhinoq 0`, 1600×900, single frames): full rebuild 0.31 ms stock, about 0.45 ms
   with the dense read (169 texels per cell), and `rhsplits 3` is about the same. Rebuilds only happen
   when a split moves.

| Variant (all `rhprec 1`) | shapeErr | Roughness | Holes | Leak |
|---|---|---|---|---|
| stock | 2.18 | 11.6% | 5.0% | 30% |
| stock, `rhsplits 3` | 3.14 | 23.5% | 8.5% | 26% |
| dense, stock falloff | 2.21 | 3.8% | — | 32% |
| dense, ε 15u, `rhsplits 3`, no dering | 1.54 | 10.2% | 4.0% | 26% |
| dense, ε 15u, `rhsplits 3`, dering 0.4 | **1.38** | 9.5% | 0.4% | 22% |
| same + N·L | 1.48 | 8.1% | 0.4% | 29% |

The remaining roughness under a sharp kernel is mostly real variation; the reference has it too.

### What a real implementation needs

- A gather that is dense or prefiltered (a mipmapped RSM read at the tap footprint) so the kernel can
  be sharp without noise. The RSM is a rectangle texture today, so mips mean switching to `sampler2D`.
- A softening radius per split, from its cell size, instead of the fixed `0.1·gidist²`. This changes
  the absolute scale, so `giscale` defaults and existing maps need retuning (best-fit scale 0.30 vs 1.8).
- Deringing at gather time.
- Probably `rhsplits 3` as the default, so the near split's cells are small enough for the sharper kernel.
- Optional N·L normalisation, preferably after leak is dealt with.

## 7. References

- G. Papaioannou, "Real-time diffuse global illumination using radiance hints", HPG 2011: the
  technique this implementation is based on, via Tesseract.
- A. Kaplanyan, C. Dachsbacher, "Cascaded light propagation volumes for real-time indirect
  illumination", I3D 2010: cascades snapped to their cell grid, the same snapping as here.
- Z. Majercik et al., "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields",
  JCGT 2019: per-probe exponential blending ("hysteresis"), the pattern proposed in §6.
