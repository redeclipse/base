# Radiance hints split stability — design

Date: 2026-10-01
Status: approved 2026-10-01 (default off; see plan)

## Goal

Remove the two largest sources of visible pop-in in the radiance hints (RH) global
illumination, without temporal accumulation:

1. **Fov-independent split sizing.** Zooming must not rebuild the RH volume.
2. **Continuous cross-split crossfade.** A surface must move between the fine and the
   coarse split gradually, as the camera moves, instead of switching in whole-cell steps.

Temporal accumulation in the volume (research report items 3–5) is a later, separate
spec. This work must not make it harder: nothing here touches how cells are computed or
cached, only where the splits sit and how the lighting pass reads them.

## Decisions

Settled with the user:

| Topic | Decision |
|---|---|
| Scope | Fixes 1 and 2 only. Temporal accumulation, no-GI in all splits, and the camera-feed issue (*Out of scope*) come later. |
| Where smoothing happens | In the RH volume and its lookup, never in screen space. |
| Default (was open question 1) | **Off until tuned:** `rhblend 0`, which gives today's shaders and placement exactly. Use `rhblend 2` to try the crossfade. Deciding the default is left for after tuning. |
| `B` and `M` (was open question 2) | Configurable: `rhblend` is the band in cells, `rhblendmargin` the margin in cells, which defaults to 2. |
| Zoom (was open question 3) | Accept coarser GI while zoomed: `rhfov = max(curfov, basefov)`. The volume is not refined at the end of the zoom animation. |
| Verification (2026-10-01) | **Fixed-point probe instead of screenshot sweeps.** Animated models, foliage, exposure adaptation and parallax swamp the pop in screenshot differences (measured: the split pop moves the mean by 0.1–0.4, parallax by 1.4 or more). The sweeps read the real `getrhlight` at fixed world points (DEBUG_UTILS `rhprobe`) instead. See *Verification* and *Deviations during implementation*. |

Proposed in this spec and kept:

| Topic | Proposal |
|---|---|
| Reference fov | `max(curfov, basefov)`, where the game publishes its unzoomed fov as `basefov`. |
| Centre clamp | Part of fix 2: splits that blend are pulled towards the camera just enough to keep it in the fully fine region. See *Why the crossfade needs a centre clamp*. |
| Shader option letter | `h` (free in the `deferredlighttype` table, `config/glsl/deferred.cfg:18`). |
| Test hooks | A read-only counter `rhsplitresets`; DEBUG_UTILS `edzoom <fov>` (stands in for a weapon zoom in the editor) and `rhprobe`. |

## Background: facts the design rests on

All numbers use the defaults: `rhgrid 27`, `rhsplits 2`, `rhsplitweight 0.6`,
`rhnearplane 1`, `rhfarplane 1024`, 16:9, `firstpersonfov 100` (minimum 90).

| Fact | Where |
|---|---|
| Each split is a cube around the bounding sphere of its frustum slab, `calcfrustumboundsphere(near, far, camera1->o, camdir, c)`. For these splits the sphere is centred on the view axis **at the slab's far plane**, so the box sits far ahead of the camera. | `src/engine/rendergl.cpp:1432`, `src/engine/renderlights.cpp:2530` |
| The sphere's radius and centre depend on `curfov` and `aspect`. | `rendergl.cpp:1441` |
| `curfov` is set every frame by `game::fixview` from `fov()` minus the zoom. Zoom animates it. | `src/game/game.cpp:3051` |
| A split's cache survives only if its rounded radius is unchanged: `split.cached = split.bounds == pradius ? split.center : invalid`. A zoom animation therefore invalidates every split on every frame. | `renderlights.cpp:2538` |
| The reflective shadow map (RSM) uses the same sphere over the whole RH range, plus `gidist`. | `renderlights.cpp:2406` |
| The split centre is snapped to whole cells: `offset = floor((c - pradius)/step)`. | `renderlights.cpp:2534` |
| The lookup takes the **first** split whose box contains the point, extending half a cell into the border. Splits are not blended. The 1-cell border, filled from the next split by `radiancehintsborder`, is the only transition. | `config/glsl/deferred/deferredlight.frag:57`, `deferredlight_defs.glsl:150`, `renderlights.cpp:2561` |
| All splits share one 3D texture, stacked in z, and clamp in all three dimensions (`create3dtexture(..., 7, ...)`). A fetch outside a split's own z range reads another split. Every fetch must stay inside its split's bounds. | `renderlights.cpp:1472` |
| Default geometry, split 0: half-size 308u, cells 22.8u, centre 225u ahead of the camera. The camera is only **83u (3.6 cells)** inside the back face. At fov 90: half-size 259u, cells 19.2u, **34u (1.8 cells)**. | derived |
| Default geometry, split 1 (last): half-size about 1400u, cells about 104u, centre 1024u ahead. | derived |
| The game's unzoomed fov is `game::fov()`: `editfov`, `specfov`, `thirdpersonfov` or `firstpersonfov`. | `game.cpp:762` |
| The engine and the game share `curfov` through `engine.h:303`; `game.h` includes `engine.h`. | |

## Fix 1: fov-independent split sizing

### Behaviour

The RH splits and the RSM are sized and placed from a **reference fov** instead of
`curfov`:

```
rhfov = basefov > 0 ? max(curfov, basefov) : curfov
```

`basefov` is the view's unzoomed fov. A zoomed frustum has the same apex, axis, near and
far planes as the unzoomed one and lies inside it, so the unzoomed bounding sphere still
contains it. Using `max` keeps that guarantee when an effect widens `curfov` past
`basefov`, such as the map-start reveal in `src/game/hud.cpp:1479`.

### Changes

| Where | Change |
|---|---|
| `src/engine/rendergl.cpp` | New global `float basefov = 0;` next to `curfov`. `calcfrustumboundsphere` gains a trailing parameter `float fov = -1`, where a negative value means `curfov`. Every existing caller is unchanged. |
| `src/engine/engine.h` | `extern float basefov;` and the new default argument. |
| `src/game/game.cpp` `fixview` | Set `basefov = float(fov());` on both branches. |
| `src/engine/renderlights.cpp` | A helper `static float rhboundsfov()` implementing the rule above. `radiancehints::setup` and `reflectiveshadowmap::getprojmatrix` pass it to `calcfrustumboundsphere`. CSM keeps `curfov`, since it is rebuilt every frame and has no cache to protect. |
| `src/engine/renderlights.cpp` | Read-only int var `rhsplitresets`. `radiancehints::setup` increments it once per split whose `pradius` differs from `split.bounds` (a resize or a cache clear). It is diagnostic only. |

### Consequences

- Zooming in or out leaves every split's size and centre unchanged, so the cache holds and
  the GI doesn't change. This holds in third person too: a zoom forces first person
  (`thirdpersonview` is false while `inzoom()`), which switches `fov()` from
  `thirdpersonfov` to `firstpersonfov`, so `fixview` sets `basefov` from `fov(false)`, the
  fov without that zoom-forced switch. `curfov` is unchanged.
- While zoomed, the RH volume no longer refines. Today a zoom shrinks split 0 and gives
  distant zoomed-in surfaces finer cells, at the cost of a full rebuild every animation
  frame. This is an accepted trade.
- Switching between first and third person (the `thirdperson` setting, not a zoom) still
  changes `basefov`, so it causes one rebuild, as today.
- Secondary views (envmaps at fov 90, UI camera feeds, mapshots) inherit the main view's
  `basefov`. Their spheres can only grow, so coverage is never lost.

## Fix 2: continuous cross-split crossfade

### Why the crossfade needs a centre clamp

A crossfade band has to sit inside split 0's box, near its faces. With today's placement
the camera is only 1.8–3.6 cells from split 0's back face. A band of 2 cells plus the
1-cell safety edge would cover the camera and the surfaces just behind it, and blend them
towards the 104u coarse cells. Near-player quality would drop, and a fast turn would sweep
the band across nearby surfaces.

The fix is to pull each blending split's centre towards the camera, per axis, just far
enough to keep a margin of fully fine cells around the camera. The pull is a `clamp`, so
it is continuous in camera position and direction. A side benefit is that the box moves
less when the camera turns.

### Definitions

For split `j` of `N` (`N = rhsplits`), `G = rhgrid`, `o = camera1->o`:

| Symbol | Meaning |
|---|---|
| `c_j`, `pr_j` | Unsnapped centre and rounded radius from `calcfrustumboundsphere` (with `rhboundsfov()`, fix 1). |
| `s_j = 2*pr_j/G` | Cell size. |
| `B = rhblend` | Band width in cells (float). |
| `M = rhblendmargin` | Fully fine margin around the camera in cells (float). |
| `m_j = max(pr_j - (1 + B + M)*s_j, 0)` | Largest offset of the centre from the camera allowed per axis. |
| `c'_j` | The placed centre. For `j < N-1` and `B > 0`: `o + clamp(c_j - o, -m_j, m_j)`, per axis. Otherwise `c_j`, which is today's behaviour. |

Snapping (`renderlights.cpp:2534`) uses `c'_j` instead of `c_j`. The snapping formula is
unchanged.

**Fine weight** of split `j` at a lookup position `p` (already nudged by `rhnudge`):

```
w_j(p) = clamp((pr_j - s_j - max_k |p_k - c'_j,k|) / (B*s_j), 0, 1)
```

### Guarantees (why this is safe and continuous)

1. **Inside the box.** The snapped minimum corner is `floor((c' - pr)/s)*s`, which lies
   between `c' - pr - s` (exclusive) and `c' - pr`. So the snapped box contains every `p`
   with `max|p - c'| <= pr - s`. Wherever `w_j > 0`, `p` lies inside split `j`'s interior
   cells, and the fine fetch needs no bounds test.
2. **Continuity.** `c_j` is continuous in the camera position and direction, the clamp is
   continuous, and `w_j` is continuous in `p` and `c'_j`. Snapping changes which texels
   hold the data, but not the weight. A one-cell move of the snapped box changes no
   pixel's weight discontinuously.
3. **New cells are invisible.** Cells that appear when the snapped box moves are within
   one cell of a face, where `w_j = 0`.
4. **Fully fine around the camera.** `max|o - c'| <= m`, so every `p` within `M` cells of
   the camera has `max|p - c'| <= pr - (1 + B)*s`, which means `w_j = 1`.

### Lookup

Replaces `getrhlight` when `DL_RHBLEND` is defined:

```glsl
// rhblendtc[j] = vec4(-c'_j/(B*s_j), 1/(B*s_j)); rhblendedge = (G/2 - 1)/B
fine = -1;
for j in 0 .. N-2 (unrolled):
    w = clamp(rhblendedge - max3(abs(rhblendtc[j].xyz + pos*rhblendtc[j].w)), 0, 1);
    if(w > 0) { fine = j; break; }
if(fine < 0)        sh = hardlookup(N-1)                       // fully coarse: last split
else if(w >= 1)     sh = fetch(fine)                           // fully fine, no coarse fetch
else                sh = mix(hardlookup(fine+1), fetch(fine), w)
```

- `rhblendedge` is the same for every split, because `pr_j/s_j = G/2`.
- **`hardlookup(a)`** is today's `DL_RH_SPLIT` chain, starting at split `a`: the first split whose box contains `p`, else the existing `vec3(4.0)` fallback.
  This keeps the coarse fetch inside its own split's z range (*Background*: shared
  texture).
- **Blend before decoding.** Blend the four decoded SH vectors (after `-0.5`), then apply
  the basis dot products and the clamp once. The SH are linear, so this is exact and saves
  ALU.
- **The last split** keeps today's hard outer edge. It has nothing coarser to fade to. *(Superseded 2026-10-01, user request: it now fades out to an empty hint over the same `rhblend` band; see Deviations.)*
- With `rhsplits 1` there is nothing to blend. The engine emits no `h`, and the shader and
  placement match today's.

### Changes

| Where | Change |
|---|---|
| `src/engine/renderlights.cpp` | `FVARF(0, rhblend, 0, 0, 8, { cleardeferredlightshaders(); clearradiancehintscache(); })` and `FVARF(0, rhblendmargin, 0, 2, 8, clearradiancehintscache())`. Clearing the cache isn't strictly needed, since a moved centre is handled like camera movement, but it makes a settings change take effect in one clean rebuild. |
| `radiancehints::splitinfo` | Store the placed unsnapped centre `blendcenter` (`c'_j`). |
| `radiancehints::setup` | Compute `c'_j` and snap it. |
| `radiancehints::bindparams` | Bind `rhblendtc[N]` (the last entry unused, zero) and `rhblendedge`, only when blending is active. |
| `deferredlightshader` name (`renderlights.cpp:2770`) | Append `h` after `r<N>` when `rhblend > 0 && rhsplits > 1`. |
| `config/glsl/deferred.cfg` | Document `h -> radiance hint split blending` in the type table. Add `if (dlopt "h") [shader_define DL_RHBLEND ""]`. |
| `config/glsl/deferred/deferredlight_decls.glsl` | Under `DL_RH` and `DL_RHBLEND`: `uniform vec4 rhblendtc[DL_NUMRH]; uniform float rhblendedge;`. |
| `config/glsl/deferred/deferredlight.frag` / `deferredlight_defs.glsl` | The blended `getrhlight` path under `DL_RHBLEND`. The non-blend path stays **byte-identical**. GLSL 1.20: no `##`, no sampler or array-of-sampler tricks. Splitting it into a helper that fetches one split's four SH texels is fine. |
| `config/usage.cfg` | Descriptions for `rhblend` and `rhblendmargin`. |
| `doc/shader-reference.md` / `doc/agent-handoff.md` | Note the new option letter and vars, as the shader-port docs do. |

### Consequences at fov 100 with `B = M = 2` (`rhblend 2`; the default is off)

| | Today | With the crossfade |
|---|---|---|
| Split 0 centre ahead of the camera (per axis, worst case) | 225u | at most 194u (`m = 8.5` cells) |
| Fully fine, behind the camera (view along an axis) | up to the hard edge, 83u | 46u (2 cells, guaranteed), fading to coarse by 91u |
| Fully fine, ahead | up to the hard edge, about 545u | about 434u, fading to coarse by about 479u |
| Split 0 movement for a 180° turn | 450u | at most 388u |
| Pixels needing a second fetch | none | only those in the band (4 extra 3D fetches) |

At fov 90 the clamp matters more: the centre goes from 225u ahead to at most 163u.

### Known limitations kept

- `rendernogi` writes only split 0 (`renderlights.cpp:4391`). Inside the band, no-GI
  areas get a partial value from split 1, which has no no-GI. This already happens at
  today's hard edge; research report item 7 fixes it.
- The `radiancehintsborder` fill stays. It still serves the hard edge of the last split
  and the trilinear footprint at `w = 0`.

## Verification

### Shader equivalence

With `rhblend 0`, `tools/harness/shaders.ps1 check` against the pre-change baseline must
pass for every `deferredlight*` configuration at the text tier. Fix 1 changes no shader
text. With `rhblend > 0`, the only differences are the new `…r<N>h…` variants.

### Behaviour

**Superseded (user decision, 2026-10-01): the screenshot sweeps below were replaced by the fixed-point probe, see *Deviations during implementation*.** They are kept as the original proposal.

Use the map editor harness (`tools/harness/editor.ps1`, `edframe`), so each screenshot is
a pure function of the camera. To make GI changes stand out against direct light, raise
`giscale` (for example to 8) for the sweeps.

| Sweep | Path | Pass criterion |
|---|---|---|
| Translation | 1u steps along a world axis, view along the same axis, across at least 3 split-0 cells (about 70u) | The largest step-to-step mean absolute difference is at most 1.5× the sweep's median. |
| Rotation | Yaw in 2° steps over 180° at a fixed position | Same criterion. |
| Near camera | With the view along an axis, compare a shot today and with the crossfade, inside a radius of `M` cells around the camera | No difference beyond the noise floor. The cell lattice is anchored in world space (`smin` is always a multiple of `s`), and the RSM doesn't depend on the clamp, so the cells there hold the same values. |

**The sweeps must fail on today's build first.** Each sweep must show spikes at the cell
crossings with `rhblend 0`, or the metric isn't sensitive enough and has to be changed
before it can prove anything.

### Fix 1

In a local game with a zoom weapon, zoom in and out repeatedly. `rhsplitresets` must not
change, and screenshots before, during and after the zoom must show no GI change beyond
the view change. With `rhforce 0` and `rhinoq 0`, the RH timer must stay at zero during a
zoom at a fixed position. With `rhinoq 1` the RH GPU time is charged to the G-buffer timer
instead. The plan chooses how to trigger the zoom (for example `gamekeypress` on the zoom
bind).

### Cost

On `park` at 1600×900, record `Deferred Shading (gpu)` with `rhblend 0` and `rhblend 2`.
Expected increase: at most 0.05 ms. Report the measured numbers either way.

## Out of scope

- Temporal accumulation in the volume, keeping history across sun changes, and jitter
  (report items 3–5).
- Moving the last split. (A fade at its outer edge was added afterwards; see Deviations.)
- No-GI in all splits (report item 7).
- **New finding, not fixed here.** UI camera feeds (`ViewSurface`, `src/engine/ui.cpp:3709`,
  every `viewportuprate` ms) and mapshots (`worldio.cpp:749`) run `gl_drawview` →
  `renderradiancehints` with their own camera. They move the shared split centres, so
  every feed update forces RH rebuilds in both the feed and the main view. It needs its
  own fix: skip RH for secondary views or give them their own state.

## Open questions (answered 2026-10-01)

1. Default on or off? **Off until tuned** (`rhblend 0`).
2. Are `B = 2` and `M = 2` reasonable? **Configurable** (`rhblend`, `rhblendmargin`); `rhblendmargin` defaults to 2.
3. Is losing finer GI while zoomed acceptable? **Yes.** `rhfov = max(curfov, basefov)`.

## Deviations during implementation

- **Front-to-back lookup.** The spec's `mix(hardlookup(fine+1), fetch(fine), w)` jumps for
  `rhsplits >= 3` when a point is in two bands at once (the inner split's band and the next
  one's). The shipped lookup accumulates front to back instead: each blending split `j` takes
  `w_j` of the weight still left, and the last split takes the rest with today's hard test. For
  `rhsplits 2` this is exactly the spec's formula. For any `N` it is continuous wherever
  today's lookup is. Checked with translate sweeps at `rhsplits 3` and `4`.
- **Verification by fixed-point probe, not screenshots** (user decision, 2026-10-01). `rhprobe`
  runs the real `getrhlight` at fixed world points; `tools/harness/gi.ps1 sweep` and `near`
  drive it and `tools/harness/probestats.py` scores the result. The criteria became: today's
  lookup must show a per-step jump `J >= 0.01`; the crossfade must cut it to `J_ref/4` or
  less; within `-Radius` of the camera the values must agree to 2/255. Points near the last
  split's faces are excluded from J (the sweeps don't measure that edge).
- **Default off.** `rhblend 0` (see the decisions table), so `rhblend 0` is token-identical to the
  pre-change shaders (`shaders.ps1 check`, `PASS-TEXT`).
- **Zoom test trigger.** `edzoom <fov>` (DEBUG_UTILS) stands in for a weapon zoom, since the
  editor has no weapon. A real in-game zoom remains a manual check.

- **Last-split fade (2026-10-01, user request after in-game testing, commit 88956482).** With
  `rhblend > 0` the last split also gets the band: its weight falls to 0 one cell inside its faces and
  the remaining weight goes to an empty hint (`RH_ZERO`), so GI and sky light thin out towards the
  edge of the volume instead of stopping at it. The last split's centre is not clamped. Checked by
  build, shader compile at 2-4 splits (probe) and `near` (0 difference); no sweep, at the user's
  request.
