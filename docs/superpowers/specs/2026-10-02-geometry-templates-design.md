# Geometry templates — design

Date: 2026-10-02
Status: implemented on branch geometry-templates (2026-10-02 plan)

## Goal

Let a mapper mark a cuboid of octree geometry as a **template**, and place **instances**
of it anywhere in the map — rotated and uniformly scaled freely, like mapmodels — rendered
from one shared vertex array with hardware instancing. Templates live in the world: editing
the source geometry updates every instance on the same edit commit.

It is an alternative to mapmodels for repeated architecture (pillars, arches, trim, props
built from cubes), without leaving the editor or exporting a model.

## Decisions

Settled with the user:

| Topic | Decision |
|---|---|
| Where templates live | **In the world.** The source is ordinary octree geometry inside the template's box; it stays editable in place, and edits propagate live. |
| Instance transform | **Arbitrary** yaw/pitch/roll and **uniform** scale (one percentage, as the mapmodel `scale` attribute). No per-axis scale. |
| Drawing | **Hardware instancing** (`glDrawElementsInstanced`, per-instance matrix attributes with divisor 1). |
| GL floor | OpenGL 3.3 core / GLSL 3.30 (already on master, `2ae84f8d`). No non-instanced fallback. |
| Instance entity | Mapmodel-like: template id, yaw, pitch, roll, scale, flags, modes, muts, variant. |
| Pivot | The template entity's position, which is also the centre of its box. |
| Off-grid template entities | Allowed. The box is a *request*; the template captures whole leaf cubes fully inside it, and the editor shows the **captured** box. |
| Overlapping templates | Allowed. A cube inside two boxes belongs to both; an edit in the overlap rebuilds both. |
| Hiding the source | **Stretch goal**, not in this spec's plan. The source keeps rendering as normal world geometry. |
| Source collision | Unchanged. Sources are meant to be built somewhere out of play. |

## Overview

```
 world octree ──(cubes fully inside box)──▶ temp octree ──updateva──▶ template VAs (own VBOs)
                                                                  └─▶ template BIH
 instance ents ──octaentities.instances──▶ per-pass culling ──▶ instance matrices (stream VBO)
                                                                  └─▶ glDrawElementsInstanced
```

- `src/engine/geomtemplate.cpp` (new) owns template data: building, dirty tracking, BIH,
  the instance lists, and the `geotemplateinfo` test command.
- `src/engine/renderva.cpp` owns drawing, so it can reuse `renderstate`, `mergetexs`,
  `changeslottmus`, `changetexgen` and `changeshader` without exporting them.
- World vertex shaders gain three per-instance attributes that are identity for ordinary
  world geometry, so the world and instances share every shader.

## Data model

### Entity types

Two engine entity types are added before `ET_GAMESPECIFIC` in `src/shared/ents.h`, with
matching rows in the game's entity table (`src/game/game.h`) and the game's mirrored
enum:

| Type | Name / display name | Attributes |
|---|---|---|
| `ET_GEOTEMPLATE` | `geotemplate` / "Geometry Template" | `id`, `width`, `length`, `height` |
| `ET_GEOINSTANCE` | `geoinstance` / "Geometry Instance" | `template`, `yaw`, `pitch`, `roll`, `scale`, `flags`, `modes`, `muts`, `variant` |

`MAPVERSION` goes 56 → 57. Loading an older map shifts every type at or above
`ET_GEOTEMPLATE` up by two, following the precedent at `src/engine/worldio.cpp:1244`.

### Template entity

- The requested box is centred on the entity with half-extents `width`, `length`,
  `height` — the same layout as `soundenv` and `physics` zones, so the editor's box
  drawing and selection code (`src/engine/world.cpp`, the `ET_SOUNDENV` branch of the
  entity box calculation) is reused.
- The entity position is the pivot: the point an instance places at its own position.
  It may be off-grid; it is only ever used as a float.
- `id` is any integer. If two template entities share an id, the lowest entity index
  wins and the console prints a warning naming both.
- A template with no geometry inside its box is valid and empty.

### Instance entity

- `template`: the id to instantiate. A missing or empty template draws nothing; in edit
  mode the entity shows the default selection marker, like an invisible mapmodel.
- `yaw`, `pitch`, `roll`: degrees, applied as mapmodels apply them
  (`rotate_around_z(yaw)`, `rotate_around_x(pitch)`, `rotate_around_y(-roll)`).
- `scale`: percent; 0 means 100.
- `flags`: bit 0 no-shadow, bit 1 no-collide.
- `modes`, `muts`, `variant`: filtered through `entities::isallowed`, as for mapmodels.

Instance transform: `M = T(e.o) · R(yaw, pitch, roll) · S(scale) · T(-pivot)`, applied to
template vertices, which stay in source coordinates.

### What a template captures

- A **non-empty leaf cube belongs to a template iff it lies entirely inside the requested
  box**. Cubes are never split; a cube straddling the boundary is excluded whole.
- The **captured box** is the union of the included cubes. It is always cube-aligned.
- Entities inside the box are never part of the template: no nested instances, no
  mapmodels, lights, decals or sounds.
- Overlapping templates capture independently.

### Editor display

- The captured box is drawn solid, the requested box faint. With nothing captured, only
  the faint box is drawn and the entity info reads "empty".
- Instances use their bounding box (below) for hover and selection, as mapmodels do.

## Building a template

One template, in `geomtemplate.cpp`:

1. Swap `worldroot` for a fresh empty octree (`newcubes()`); `worldsize`/`worldscale`
   are unchanged so coordinates mean the same thing.
2. Walk the real octree over the requested box. Copy every non-empty leaf cube that lies
   fully inside it into the temporary octree at the same coordinates, accumulating the
   captured box. Each copied cube keeps its `ext` **surfaces and vertex data** (which
   carry smoothed normals), with `va`, `ents` and `tjoints` dropped and `LAYER_BOTTOM`
   cleared on every surface (so only the top layer of a blended face is built).
3. Run `genmerges` on the temporary octree.
4. Save and swap out `valist`, `varoot`, `wverts`, `wtris`, `allocva`; run the normal
   `updateva` over the temporary octree; then `flushvbo()`. The template gets its own
   vertex arrays in its own VBOs; the world's lists and VBO accumulators are untouched.
   A large template simply becomes several vertex arrays, which also respects the
   `setva` 0x1000 size assert and 16-bit indices.
5. Detach the new vertex arrays from the temporary cubes (`ext->va = NULL`), free the
   temporary octree, and restore every swapped global.
6. Build the BIH (see *Collision*).

Consequences, accepted for version 1:

- No decals in templates (the temporary octree has no entities).
- No t-joint filling inside templates: `findtjoints` rebuilds the global `tjoints`
  vector, so it cannot run here. Worst case is hairline cracks, as with `filltjoints 0`.
- Blend layers render as their top layer only.
- Alpha, refractive, sky and material faces may be built into the vertex arrays but are
  never drawn for instances (only the opaque range, `va->texs`, is).
- Envmap ids come from `closestenvmap` at the source's location.

## Keeping templates live

| Trigger | Effect |
|---|---|
| Map load (`allchanged`) | Build every template **before** `entitiesinoctanodes()`, because instance bounding boxes depend on captured boxes. |
| Geometry edit, `changed(bbmin, bbmax)` | Mark dirty every template whose requested box, grown by 1, overlaps the change. |
| Template entity edit | A hook in `modifyoctaent` for `ET_GEOTEMPLATE` with `MODOE_CHANGED` marks that template dirty, plus any template sharing its old or new id. Covers move, resize, re-id, delete and undo. |
| `commitchanges` | First rebuild every dirty template; for each whose captured box changed, remove and re-add its instances. Then continue as today (`entitiesinoctanodes`, `octarender`, …, `clearshadowcache`). |

Undo, multiplayer edits, paste and texture changes reach `changed()` or `allchanged()`,
so they need no hooks of their own.

### Instances in the octree

- `getentboundingbox` gains an `ET_GEOINSTANCE` case: the axis-aligned bounds of the eight
  transformed corners of the template's captured box (falling back to `entselradius`
  when the template is missing or empty).
- `octaentities` gains `vector<int> instances`, and `vtxarray` gains
  `vector<octaentities *> instances`, both maintained by `modifyoctaentity` exactly as
  `mapmodels` is, and included in `updatevabb`.
- `freeoctaentities` pops `instances` like the other lists.

## Rendering

### Shaders and attributes

- `gle` gains `ATTRIB_INSTANCE0..2` (slots 10–12, `MAXATTRIBS` 13) named `vinstance0`,
  `vinstance1`, `vinstance2`; `shader.cpp` binds them through the existing
  `attribnames` loop.
- `world.vert`, `bump.vert`, `smworld.vert`, `shadowmap.vert` and `rsm.vert` declare
  `in vec4 vinstance0, vinstance1, vinstance2;` (the three rows of a 4×3 matrix) and use
  `wpos = vec3(dot(vinstance0, vvertex), dot(vinstance1, vvertex), dot(vinstance2, vvertex))`
  wherever `vvertex` stood for a world position: `gl_Position`, `camvec`, blendmap
  coordinates. Normals and tangents go through the 3×3 part and are renormalised.
  Triplanar texture coordinates keep using the local `vvertex`, so textures stay attached
  to the instance.
- **Identity for world geometry:** the three attributes' current values are set to
  identity rows at GL init and restored after every instanced draw, because GL leaves an
  enabled array's current value undefined.
- `rendergl.cpp` loads `glDrawElementsInstanced` and `glVertexAttribDivisor` (core in 3.3).

### Per-frame instance data

- One streaming instance VBO, orphaned when full. Each pass appends the matrices it needs
  and points the instance attributes at its own offset with divisor 1.
- `geombatch` gains an instance offset and count, both 0 for world geometry;
  `renderbatch` issues `glDrawElementsInstanced` when the count is non-zero.
- Instances of one template are grouped, so each texture batch of a template vertex array
  is one instanced draw with textures bound once.

### Passes

| Pass | Instances |
|---|---|
| G-buffer: new `renderinstances()` right after `rendergeom()` | Collected from `visibleva` → `va->instances` → `oe->instances`, with the fog and PVS tests and `oe->query` occlusion queries of `findvisiblemms` (gated by `oqmm`). Per template vertex array: `mergetexs` over the opaque range, then instanced batches. |
| Shadow maps, alongside each `findshadowmms` / `batchshadowmapmodels` site in `renderlights.cpp` | Per light, collect instances from `shadowva`, skipping no-shadow ones. CSM splits and cube-map faces use per-instance masks (`calcbbcsmsplits`, `calcbbsidemask`); one upload per split or face. Drawn with `smworld`. |
| Cached point-light shadow meshes, `genshadowmesh` | Bake instance triangles, transformed on the CPU from `va->vdata`, and flag those instances so the live path skips them — as `genshadowmeshmapmodels` and `EF_SHADOWMESH` do. |
| RSM / GI, `renderrsmgeom` | Same collection as shadow maps, drawn with `rsmworldshader`. |
| Occlusion | Instances are occludees only (bounding-box queries, as mapmodels). They do not take part in the world z-prepass. |

Not drawn for instances in version 1: alpha and refraction, blend layers, sky, grass,
materials and caustics, decals, and the edit-mode wireframe outline.

Editing an instance entity already calls `clearshadowcache()` through `enteditv`;
template rebuilds go through `commitchanges`, which also calls it.

## Collision

- Each template builds a `BIH` from its opaque triangles. Positions are copied with the
  pivot subtracted; meshes are split at `BIH::mesh::MAXTRIS`. Rebuilt with the template.
- `mmcollide` and `mmintersect` (`src/engine/physics.cpp`) gain an `oe->instances`
  branch calling the template BIH's `ellipsecollide`/`boxcollide`/`traverse` with the
  instance's `e.o`, yaw, pitch, roll and scale. No-collide instances are skipped.
- The stain generator gets the matching branch via `BIH::genstaintris`, so bullet stains
  land on instances.

## Test-only commands

Gated by `DEBUG_UTILS` and refused under `IDF_MAP`, per CLAUDE.md.

| Command | Returns |
|---|---|
| `geotemplateinfo <id>` | `capminx capminy capminz capmaxx capmaxy capmaxz verts tris instances rebuilds`, or empty if no such template. |
| `edraycast <ox> <oy> <oz> <dx> <dy> <dz>` | Hit distance through the normal `raycube` path including entities, or -1. |

## Verification

All through `tools/harness/editor.ps1` on a scratch `newmap` unless noted.

1. **Capture and live edits**
   - Build geometry with `edselbox` and texture commands; place a template.
     `geotemplateinfo` reports the expected captured box. A cube straddling the requested
     box is excluded. Moving the entity off-grid snaps the captured box as expected.
   - An edit inside the box changes the triangle count and bumps `rebuilds` on the same
     commit; an edit outside leaves both alone.
   - Two overlapping templates both rebuild on an edit inside the overlap.
2. **Instances**
   - `edframe` screenshots of instances at several rotations and scales, inspected by eye.
     (Lighting is position-dependent, so there is no pixel comparison against the source.)
   - `vtris` grows by the template's triangle count per visible instance.
3. **Shadows and GI**
   - Screenshots under a sun light and a point light, with `smmesh` on and off.
   - `rhprobe` values near an instance differ from those with the instance deleted.
4. **Collision**: `edraycast` hits a rotated, scaled instance at the analytically
   expected distance, and misses it with the no-collide flag set.
5. **Save and load**
   - Save, reload: identical `geotemplateinfo`.
   - A shipped version-56 map loads with unchanged entity type counts after the shift.
6. **Shaders**: `tools/harness/shaders.ps1 check` shows contract and text changes for the
   world shaders, and no pixel differences on a scene without instances.

## Phases

One plan, three phases, each ending in a working build:

1. **Templates and G-buffer instances**: entity types and `MAPVERSION` 57, template build,
   captured box and editor display, octree registration, live updates, shader attributes,
   instanced G-buffer drawing, `geotemplateinfo`.
2. **Shadows and GI**: shadow maps, cached shadow meshes, RSM.
3. **Collision**: BIH, `mmcollide`, `mmintersect`, stains, `edraycast`.

## Stretch goals (out of this plan)

- **Hiding the source.** World vertex arrays would exclude geometry in hidden template
  boxes, and edit mode would draw the template at identity in its place. Needs: template
  box edges as merge barriers in `genmerges`; world faces next to a hidden box treating
  cubes inside it as empty in `visibletris`; overlap rule — a cube is hidden from the
  world if any template that owns it is hidden; optionally, hidden sources non-solid
  outside edit mode.
- Alpha and refraction faces, blend layers (world-locked blendmap), decals on instances.
- Spin and tint, as mapmodels have.
- Edit-mode wireframe outline for instances.

## Out of scope

- Per-axis scale.
- Templates stored outside the world (embedded blocks, `.obr` files).
- Nested templates (instances inside a template's box are not captured).

## Revisions during planning

Found while reading the code for the plan (2026-10-02). These override the sections above.

- **Names.** The entity types are `geotemplate` / `geoinstance` (`ET_GEOTEMPLATE`,
  `ET_GEOINSTANCE`), not `template` / `instance`: the editor already has an unrelated
  "entity templates" feature (`config/tool/toolenttemp.cfg`, `T_ENT_UI_TEMPLATES`).
- **A fourth instance attribute.** Uniformly scaled instances need unit normals
  (`bump.frag`'s triplanar blend and `rsm.vert` use the normal unnormalised), and
  normalising in the vertex shader would change world pixels. So instances carry
  `vinstancescale` (slot 13, the *inverse* scale, constant 1 for the world) and
  directions are `rows.xyz · n * vinstancescale`. Identity rows and a factor of 1 keep
  world output bit-identical. `MAXATTRIBS` becomes 14.
- **Merges.** Copied cubes have their merged faces reset (as `clearmerge` does) and
  `genmerges` runs on the temporary octree. A merged polygon crossing the box edge would
  otherwise be copied half-owned. Template builds in `commitchanges` run while
  `inbetweenframes` is false, so `progress()` never draws a loading screen mid-edit.
- **Instanced draw state.** `renderstate` gains an instance count (0 for the world)
  instead of `geombatch` gaining an offset and count: all batches of one template vertex
  array share the instance rows bound before `renderbatches`.
- **Where shadow and RSM instances hook in.** Inside `renderva.cpp` only: collected in
  `findshadowmms` (called at every shadow and RSM site, and by `genshadowmesh`), drawn at
  the end of `rendershadowmapworld` and `renderrsmgeom`. `renderlights.cpp` changes only
  for the G-buffer call. A light with a cached shadow mesh draws `rendershadowmesh`
  instead of `rendershadowmapworld`, so baked instances are never drawn twice and need no
  `EF_SHADOWMESH`-style flag.
- **De-duplication.** An instance registers in every octree node its box overlaps, so
  each collection pass marks visited instances with `EF_RENDER` and clears it afterwards,
  as mapmodels do.
- **Occlusion queries.** `octaentities` gains its own `instquery`, owned by
  `&oe->instquery`, so it never aliases the mapmodels' `oe->query`.
- **Removal before rebuild.** An instance's octree placement depends on its template's
  captured box, so the instances of a template are removed from the octree before that
  template is rebuilt or deleted, and `entitiesinoctanodes()` re-adds them.
- **Test commands** (all `DEBUG_UTILS`, refused under `IDF_MAP`): `geotemplateinfo <id>`;
  `edfillsel <solid>` (fills or empties the current selection, for building test
  geometry); `geoinststats` (instances and triangles drawn in the last G-buffer pass);
  `edraycast`. Pure logic (box, capture, instance matrix and bounds) is unit-tested in
  `src/tests/geomtemplate.cpp`, run at startup by debug builds.
- **Picking.** As with `soundenv`, hovering picks a template or instance by the small
  entity box. Once selected, a template draws its requested box and an instance its
  world bounds (`entselectionbox`'s `full` path). A full-size pick box would swallow
  every click inside it.
