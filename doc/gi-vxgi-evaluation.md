# Global illumination: voxel cone tracing (VXGI) vs radiance hints

_2026-10-04. Research only, nothing implemented. Question: would VXGI (voxel cone tracing,
VCT) be a viable, higher-quality replacement for the radiance hints (RH) GI? Background on the
current RH system is in [gi-radiance-hints-findings.md](gi-radiance-hints-findings.md)._

**Short answer: it would be higher quality in some areas, but it is not a viable replacement
under the engine's current constraints.** It costs much more GPU time every frame. In its
usual fully dynamic form it also breaks the GL 3.3 floor and brings back the kind of temporal
instability the RH work is fixing now. A Godot-VoxelGI-style variant that takes static voxels
from the octree avoids those last two problems (§4.1), but not the frame-time cost. Most of
the quality gap can be closed more cheaply inside RH (§5). VCT only makes sense as an optional
high tier, and only if multi-bounce, local-light GI or glossy reflections become goals.

"VXGI" here means voxel cone tracing implemented in this engine. NVIDIA's VXGI product is
GameWorks middleware under a proprietary licence, built around DX11/12 and Unreal 4, and no
longer developed. It is not a library we could drop in.

---

## 1. What RH does today (the baseline)

These facts were checked in the code:

| Property | RH today |
|---|---|
| Light sources | **Sun only.** One reflective shadow map (RSM) from the sun. The result is applied inside the `DL_CSM` path (`deferredlight.frag:284`, `rhlight.rgb * giscale`), so maps without a sun get no GI. |
| Bounces | One. |
| Visibility | **None between a bounce source and a cell.** `calcrhsample` (`config/glsl/gi/radiancehints.frag:26`) weights an RSM texel only by `dot(dir, rsmnormal)` and `1/(0.1 + dist·rhatten)`. Light bounced from a sunlit floor reaches cells on the other side of a wall if both are within `gidist`. |
| Resolution | 27³ cells per split, 2 splits. Split 0 has about 19–23u cells, split 1 about 90–100u. |
| Memory | 4 RGBA8 3D textures of 29×29×58, about 0.8 MB, or 1.6 MB with the `rhcache` ping-pong. |
| Cost | About 0.4 ms for a full rebuild on `park` at 1600×900 (measured, findings §4.2). **Zero** on frames where no split moves. The lookup is 4 texture fetches per pixel in the existing light pass. |
| GL | Works on the 3.3 floor (`rhslice` layered rendering, `glCopyImageSubData` where available). |

## 2. What VCT would give (quality)

These are real gains, roughly from most to least visible in Red Eclipse:

1. **Occlusion and fewer leaks at short range.** Cones march through an occupancy and radiance
   volume, so walls block bounced light. RH's missing visibility test (above) is its biggest
   quality flaw indoors, and VCT fixes it near the camera. It does **not** fix it everywhere:
   VCT leaks through any wall thinner than the voxel size at the current cone footprint, which
   is worst in the coarse clipmap levels. Godot replaced its VCT (GIProbe) with SDFGI as the
   large-scale solution and cited VCT's leaking as the reason.
2. **GI from local lights**, not only the sun. Indoor and night maps would gain the most.
   Caveat: injecting a point light into the voxels needs its shadow. Tesseract's shadow atlas
   holds shadows only for lights that matter to the current view, so lights outside it would
   inject unshadowed (leaky) light or need their own shadow pass.
3. **Multiple bounces**, by feeding last frame's radiance back into the injection pass
   (The Tomorrow Children used three).
4. **Glossy specular reflections** from a narrow cone, an alternative to the envmaps.
5. **Finer near-field detail.** A 64³ clipmap level 0 at 4u voxels is about 5× finer than RH
   split 0. Narrow contact-scale occlusion stays coarse, and SSAO still covers that.
6. **The sun moves more naturally.** A sun change re-injects light into existing voxels; no
   geometry is re-voxelised. RH re-renders the RSM and every cell instead.

## 3. What it would cost

### 3.1 GL requirements: above the 3.3 floor

VCT needs scattered writes into a 3D texture during voxelisation, which is `imageStore` or
`imageAtomic*` (GL 4.2 `ARB_shader_image_load_store`). In practice it also needs compute
shaders (4.3) for clearing, injection, clipmap scrolling and mip building. The engine floor is
3.3 core (`main.cpp:538` tries 4.0 then 3.3), and `rendergl.cpp` has no image load/store or
compute paths today. A 3.3-only voxeliser is possible, slice by slice like `rhslice`, but it
redraws the scene once per slice and isn't practical.

So VCT means **keeping RH as the fallback and maintaining two GI systems.** That works against
the direction of the GL 3.3 floor and shader-refactor work: fewer paths, each provably
equivalent.

### 3.2 GPU time every frame

RH's lookup is nearly free, and its update is zero on static frames. VCT's main cost is the
per-pixel cone trace, and it is paid **every frame**, whether or not anything changed:

| Source | Numbers |
|---|---|
| The Tomorrow Children (PS4, GDC 2015) | About 3 ms of voxel cascade updates plus about 3 ms of per-pixel diffuse, at 30 Hz. To get there they traced at 1/16 resolution, used 16 fixed directions, precomputed "far cones" and staggered cascade updates (level 0 every 2nd frame, level 1 every 4th, ...). The first naive version took over 30 ms. |
| Naive full-resolution VCT (reports collected on gamedev.net) | 8 ms at 512², 27 ms at 720p, 62 ms at 1080p. |

On a current mid-range desktop GPU, a well-optimised half-resolution diffuse VCT plus an
incremental clipmap update will probably land around **1.5–4 ms at 1080p**. That is an
estimate from the figures above, not a measurement here. Red Eclipse is a fast arena shooter
played at high refresh rates on a wide range of hardware. At 144 Hz (6.9 ms per frame), 2 ms
or more for GI is a large share, against roughly 0 ms today.

### 3.3 Memory

| Layout | Size |
|---|---|
| RH today | about 0.8–1.6 MB |
| 6 levels × 64³, isotropic RGBA16F radiance | about 12.6 MB |
| 6 levels × 64³, **anisotropic** (6 directions) RGBA16F, the usual choice for quality | about 75 MB |
| Plus voxel albedo, normal and emissive (about 12 B per voxel) | about 19 MB |
| 128³ levels | 8× the above |

### 3.4 It brings back the temporal problems

The current RH work is about **pop-in and shimmer**: whole-cell split snapping, rebuilds caused
by the view direction, the animated sun. VCT clipmaps have the same class of problems in a
stronger form:

- Each clipmap level scrolls in whole-voxel steps. Re-voxelising the new slab means every
  triangle is rasterised against a shifted grid, so voxel occupancy and colour change
  ("voxel crawl"). Cone traces through those voxels flicker, most visibly in the coarse levels.
- Boundaries between levels need the same crossfade as `rhblend`.
- Conservative-rasterisation gaps and voxel aliasing on thin or diagonal geometry
  (`GL_NV_conservative_raster` is NVIDIA-only, so others need a geometry-shader workaround).
- Shipped VCT systems all rely on **temporal accumulation** to hide this. That is item 3 of
  the RH findings, which isn't built yet.

In other words, switching to VCT would not remove the work on the RH backlog (crossfade,
temporal accumulation, jitter). It would need all of it again, against a noisier signal.

### 3.5 Industry direction

- **Unreal 4** showed sparse voxel octree GI (SVOGI) in 2012 and removed it before release for
  performance reasons. **Unreal 5** uses Lumen: SDF and screen traces plus a surface cache.
- **Godot 4** still ships VCT, as `VoxelGI` (Godot 3's GIProbe, renamed). It is a different
  design from the camera-following clipmap above: **static geometry is voxelised once, at bake
  time, into a fixed volume placed by the level designer**. Lights, and meshes marked Dynamic,
  are injected in real time. Godot's docs aim it at small and medium scenes and dedicated GPUs,
  warn that large extents lower the quality, and allow at most 8 volumes with 2 blended per
  pixel. For large worlds Godot added SDFGI, which it describes as "mostly leak free, unlike
  VCT". VoxelGI is the variant worth considering here (§4.1).
- **Wicked Engine** kept its VXGI, but added DDGI and SurfelGI next to it.
- **CryEngine** shipped SVOTI. It is the main production user of VCT at scale.

Production engines moved from VCT towards probe and SDF methods. VCT stays as one option, not
the default.

## 4. Red Eclipse specifics

- **The world is already an octree of cubes.** Static geometry occupancy could be built on the
  CPU from the octree at load and edit time, with no GPU voxelisation and exactly aligned to the
  map's own grid. This is the one way this engine is unusually well suited to voxel methods.
  It matters more for §5.1 than for full VCT: VCT still needs albedo and normals per voxel and
  must voxelise mapmodels, geometry template instances and players on the GPU.
- Maps mix large outdoor spaces (`park`: `gidist 1000`, `rhfarplane 1500`) with tight
  interiors. VCT is good at the interiors and needs 5–6 clipmap levels for the outdoors, which
  is where its leaking and flicker are worst.
- Distributed build targets are Windows and Linux (`src/install/{win,nix,steam}`). macOS,
  which tops out at GL 4.1, isn't a concern. The GPUs excluded by 4.3 are older Intel iGPUs and
  legacy drivers, the same group the 3.3 floor still deliberately serves.
- The GI harness (`rhprobe`, `gi.ps1`) measures `getrhlight` at world points. A VCT tier
  would need its own probe command; the screenshot rule (no screenshot-diff GI metrics) applies
  even more to a temporally accumulated tracer.

### 4.1 A Godot-VoxelGI-style variant: static voxels from the octree

Godot's split (static geometry voxelised once, lights injected live) fits this engine better
than a fully dynamic clipmap, and it removes several objections in §3:

- **No GPU voxelisation, so no GL 4.2/4.3 requirement for it.** Occupancy, normals and albedo
  for the world come from the octree on the CPU at load and edit time. Albedo would be a
  per-texture-slot average, which the engine doesn't compute today. The result is uploaded with
  `glTexSubImage3D`. Light injection and mip building can be layered draws into 3D-texture
  slices, the way `rhslice` already works. Cone tracing is ordinary 3D texture sampling.
  **The whole pipeline can stay on GL 3.3.**
- **No voxel crawl.** If every level's voxel size is a power of two aligned to the world
  origin, the octree's cubes land exactly on voxel boundaries, so a scrolling level re-reads
  identical voxels instead of re-rasterising against a shifted grid. This removes the main
  source of the flicker in §3.4. Level boundaries still need a crossfade.
- **Still open:** the per-pixel cone-trace cost (§3.2) is unchanged. Mapmodels, geometry
  template instances and players are not in the octree: they need GPU voxelisation (back to
  GL 4.2+), a coarse CPU approximation from their bounds, or exclusion. Memory still rules out
  a single fixed volume over the whole map: a 4096u map at 8u voxels is 512³, about 134M voxels.
  It has to be a clipmap or several volumes, as in Godot.

This is the version to prototype if VCT is pursued. It is still a second GI system, and the
frame-time cost is the deciding factor.

## 5. Recommendation

**Don't replace RH with VCT.** In order:

1. **Add visibility to RH (occlusion-aware RH).** This captures VCT's biggest gain (§2.1) on the
   3.3 floor. Build a coarse occupancy volume, an R8 3D texture covering the RH splits, from the
   octree on the CPU, or by thresholding the existing geometry. In `calcrhsample`, march a few
   steps from `rsmpos` to `rhpos` through it and attenuate the tap. That is a few extra fetches
   per tap during an update that already costs about 0.4 ms, and it is free on cached frames.
   Papaioannou's later RH work and DDGI's visibility term solve the same problem.
2. **Finish the RH temporal backlog** (findings items 3–5). It is needed whatever GI method is
   used, and it fixes the problems the user is seeing now.
3. **If local-light GI or multi-bounce become goals**, evaluate probe-based GI with visibility
   (DDGI-style irradiance and depth probes) before VCT. It costs less per pixel, its per-probe
   hysteresis is the temporal model already planned for RH, and it degrades more gracefully.
4. **VCT only as an optional `gi 2` tier**, if glossy reflections or multi-bounce justify a
   second system. Prototype the octree-sourced variant (§4.1), which can stay on GL 3.3:
   anisotropic 3D clipmap (6 × 64³) aligned to the world grid, static voxels from the octree,
   models excluded or approximated at first, sun injection from the CSM, half-resolution
   diffuse with 5–6 cones and the existing bilateral upsample, staggered level updates and
   temporal accumulation from day one, and a `vctprobe` harness command that mirrors
   `rhprobe`. Measure the frame time before anything else; that decides it.

## 6. Sources

- C. Crassin et al., "Interactive Indirect Illumination Using Voxel Cone Tracing", Pacific
  Graphics 2011 (the original technique).
- J. McLaren, "Cascaded Voxel Cone Tracing in The Tomorrow Children", GDC 2015 —
  [slides](https://fumufumu.q-games.com/archives/Cascaded_Voxel_Cone_Tracing_final.pdf),
  [Game Developer write-up](https://www.gamedeveloper.com/programming/graphics-deep-dive-cascaded-voxel-cone-tracing-in-i-the-tomorrow-children-i-).
- Godot: [SDFGI announcement](https://godotengine.org/article/godot-40-gets-sdf-based-real-time-global-illumination/)
  (on VCT's leaking), [VoxelGI class](https://docs.godotengine.org/en/latest/classes/class_voxelgi.html),
  [Using VoxelGI](https://docs.godotengine.org/en/stable/tutorials/3d/global_illumination/using_voxel_gi.html)
  (baked geometry, live lights, limits).
- [Wicked Engine](https://deepwiki.com/turanszkij/WickedEngine) (VXGI next to DDGI and SurfelGI).
- [gamedev.net: implementing voxel cone tracing](https://gamedev.net/forums/topic/696476-implement-and-understand-voxel-cone-tracing/5378037/)
  (naive VCT timings).
- [compix/VoxelConeTracingGI](https://github.com/compix/VoxelConeTracingGI) (open-source 3D clipmap VCT reference).
- [Nvidia GameWorks](https://en.wikipedia.org/wiki/Nvidia_GameWorks) (VXGI licensing).
- Z. Majercik et al., "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields",
  JCGT 2019 (DDGI).
