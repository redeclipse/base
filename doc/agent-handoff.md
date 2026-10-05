# Agent handoff index — Red Eclipse

This is the starting point for an agent picking up work in this repo. It is an **index**: it points to
the documents that hold the detail rather than repeating them, so it stays correct when they change.
Read §1, check §3, then go to whichever section matches your task.

_Last updated: 2026-10-02, after the model port and the GLSL 3.30 modernisation were merged into local `master` (`71edd8fa`..`fd99febc`), not pushed._

---

## 1. Read first, in this order

| # | Document | Why |
|---|---|---|
| 1 | [CLAUDE.md](../CLAUDE.md) | Build command, UI architecture, verified engine behaviour, test-only commands, **CubeScript traps**, conventions. Claude Code loads it automatically. It is gitignored and exists only locally. |
| 2 | This file | Where everything is, and the current state. |
| 3 | [doc/map-editor-harness-findings.md](map-editor-harness-findings.md) | Open bugs, open issues, and engine facts that were expensive to discover. **Read §1 before touching entities or map loading.** |
| 4 | [tools/harness/README.md](../tools/harness/README.md) | How to drive the running game: UI harness, map editor harness, crash diagnostics. |

Read these only when your task needs them:

| Document | Covers |
|---|---|
| [doc/cubescript-reference.md](cubescript-reference.md) | CubeScript language |
| [doc/engine-systems.md](engine-systems.md) | Engine subsystems overview |
| [doc/shader-reference.md](shader-reference.md) | Shaders |
| [doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md](superpowers/specs/2026-09-25-shader-equivalence-harness-design.md) | **Shader refactor work.** How to prove a port changed nothing: tiers, corpus format, sweep. Read it before touching `config/glsl`. |
| [doc/gi-radiance-hints-findings.md](gi-radiance-hints-findings.md) | **GI (radiance hints) work.** How the RH volume works, why the splits pop, why an animated sun shimmers, measured costs, profiling traps, the recommendation list and temporal-accumulation design notes. Read it before touching RH, RSM or `getrhlight`. |
| [doc/refactor-plan.md](refactor-plan.md) | Upstream refactor goals (encapsulation, C++17) |
| [doc/todo-list.md](todo-list.md) | Upstream feature wishlist |

All documentation lives in `doc/` (tracked): the upstream project references, the harness work's
findings and this index, and the designs and plans under `doc/superpowers/`. The old `docs/` folder
was merged into it on 2026-10-05.

---

## 2. Repo map

| Path | What |
|---|---|
| `src/engine/` | Engine C++ (Tesseract-derived). `main.cpp` (init, crash handling), `world.cpp` (entities, editor view commands), `octaedit.cpp` (geometry editing, selection), `console.cpp` (key binds, input), `ui.cpp` (~150 `ui*` commands). |
| `src/game/` | Game-logic C++: modes, physics, clients, weapons. |
| `src/shared/` | Shared headers: `cube.h`, `tools.h` (`vector`, `ASSERT`), `geom.h`, `iengine.h`, `igame.h`. |
| `src/tests/` | C++ unit tests. Built only under `-D_DEBUG` and run at startup from `main.cpp`. |
| `src/build.sh` | Cross-compile script, run from WSL. |
| `config/` | **CubeScript. All UI and most game logic live here.** `config/ui/**` is the UI; `config/setup.cfg` holds binds and the `edit`/`start` aliases; `config/tool/` holds editor tool actions. |
| `bin/amd64/redeclipse.exe` | **The only runnable binary.** Its DLLs are beside it. Never launch `src/redeclipse_windows_amd64.exe`, which can't resolve its imports. |
| `data/` | Assets. **Mostly git submodules.** `data/maps/*.mpz` are the shipped maps, e.g. `atop`. |
| `tools/harness/` | Agent-driven harnesses: the UI harness, the map editor harness and the shader equivalence harness (§5). |
| `config/glsl/` | Shader generators: CubeScript that emits GLSL. The target of the shader refactor (§6 item 1). |
| `src/engine/shaderharness.cpp` | Shader harness engine side: `shaderdumpall`, `shaderbench`, pure helpers. `shader.cpp` holds `origin` tracking, `composeglslparts`, `slotparamsscope`, `shaderorigin`, `shaderforceall`. |
| `home/` | Gitignored runtime homes. The harness uses `home/uitest/`: `log.txt`, `harness/`, shots, crash dump. |
| `doc/` | All documentation: project references, findings, this index; specs and plans in `doc/superpowers/`. |
| `.superpowers/sdd/<date>-<plan>/` | Execution ledgers and per-task reports, one folder per plan (§7). Excluded from git by `.git/info/exclude`. |

---

## 3. Current state (check before you change anything)

- **Shader source loader** (6 commits, `573ad9b8`..`056978a3`, fast-forwarded into `master` on 2026-09-26;
  branch deleted): the shader source loader, step 1 of the shader refactor. It adds the
  CubeScript commands `shader_new`, `variantshader_new`, `shader_define`, `shader_include_vs`,
  `shader_include_fs` and `shader_source`, in `src/engine/shadersource.{h,cpp}`. They wrap the unchanged
  `shader()`/`variantshader()`. Docs: `doc/shader-reference.md`, "Shader Source Files".
  - Plan: [doc/superpowers/plans/2026-09-26-shader-source-loader.md](superpowers/plans/2026-09-26-shader-source-loader.md).
  - Ledger with all rulings: `.superpowers/sdd/2026-09-26-shader-source-loader/progress.md`.
  - Equivalence gate on 2026-09-26: the full `check` gave 80,060 `PASS-TEXT` rows, all with identical
    hashes, and 70 `MISSING` rows. The `MISSING` rows are map-generated model shaders (`modelA`, `modelA0`,
    `modelnme`, `modelnme0` and their variants) on gauntlet, challenges-port-06 and deathtrap. A `master`
    build misses them too, and they vary between runs, so they are not caused by the loader (§6 item 1a).
- **AO shader port** (9 commits `3f4c6cb0`..`0aa77371`, fast-forwarded into `master` on 2026-09-27;
  branch `ao-shader-port` deleted; not pushed): the shader refactor's first family port
  (§6 item 1 step 2). All three `ao.cfg` generators (`linearizedepth`, `ambientobscurance`, `bilateral`)
  moved from CubeScript-generated GLSL to plain `#ifdef` GLSL files under `config/glsl/ao/`, loaded with
  `shader_new`. Commits: `3f4c6cb0` harness: sweep the remaining ao depth-format branches (adds sweep
  points `s45`-`s55` to `tools/harness/shader-sweep.txt`); `7dfc596f` glsl: move linearizedepth into
  config/glsl/ao; `c0427777` glsl: move ambientobscurance into config/glsl/ao; `658a4a8d` glsl: move
  the ao bilateral filter into config/glsl/ao; `651a9199` doc: describe the ao shader files and how to
  port a generator. Commits `c0427777`/`658a4a8d`/`651a9199` carry a `Co-Authored-By: Claude Sonnet 5`
  trailer; `3f4c6cb0`/`7dfc596f` carry `Claude Opus 5.5`.
  - Full `shaders.ps1 check` (maps included) against the baseline, on 2026-09-27: 79,327 `PASS-TEXT`,
    283 `PASS-SPIRV`, 0 `FAIL`, and every AO-family row `PASS-TEXT`/`PASS-SPIRV`. The remaining 520
    `MISSING`/6,720 `EXTRA` rows are model-shader noise (§6 item 1a), but from two different causes:
    the 6,720 `EXTRA` rows are explained by the map-pass fix `61b3543a` — this branch's build already
    has it, so it now generates model shaders the pre-fix golden baseline never recorded, everywhere
    that fix applies, not only the 3 named maps. The 520 `MISSING` rows rest on the separate,
    still-open nondeterminism in that same pass, reproduced on the unchanged build during Task 1. No
    full check with maps was run against an unported build, so there's no baseline-vs-baseline number
    to compare either count to.
  - Family check against the `aogaps` corpus (13 sweep points, `-NoMaps`): every AO-family row
    `PASS-SPIRV`/`PASS-TEXT`; only model-shader `MISSING`/`EXTRA` noise otherwise, this time at sweep
    sids `s47`/`s51` rather than a map — confirming the noise isn't confined to the 3 maps (§6 item 1a).
  - The `aogaps` corpus (`home/uitest/shadercorpus/aogaps/`, untracked, gitignored) was recorded from
    the unported build at the Task 1 commit (`2734a70a`) at 13 extra sweep points (`s45`-`s55`, plus
    `s00`/`s99`) that the golden baseline's sweep didn't reach, to prove the `ao.cfg` branches those
    points exercise. It can be deleted once the golden baseline is re-recorded with `s45`-`s55` folded
    into the regular sweep (§6 item 1a).
  - **Post-review fix** (commit `9311af39`, after the commits above): `config/glsl/ao/bilateral.frag`
    defined `BILATERAL_DEPTHSCALE` as `(1 << BILATERAL_REDUCE)`, and GLSL 1.20 (no `EXT_gpu_shader4`,
    which the engine can emit) rejects `<<` in code — confirmed with `glslangValidator` at `#version 120`
    before and after. Replaced with a literal `#if`/`#elif` chain on `BILATERAL_REDUCE` (0..2). Re-check
    after the fix, both against the baseline (13 sids, `-NoMaps`) and against `aogaps` (13 sids,
    `-NoMaps`): every AO-family row `PASS-SPIRV`/`PASS-TEXT` in both runs; the baseline run had zero
    non-`PASS-*` rows, the `aogaps` run had the same known `modelA0`/`modelnme0` `MISSING` noise at
    `s51` (20 rows) and nothing else.
  - Docs: `doc/shader-reference.md` "Shader Source Files" and its new "Porting a generator" subsection.
  - Reports: `.superpowers/sdd/2026-09-27-ao-shader-port/task-{1,2,3,4,5}-report.md` (task 0 is inside
    `task-1-report.md`); ledger `.superpowers/sdd/2026-09-27-ao-shader-port/progress.md`.
- **AA shader port** (5 commits `f442379f`..`9aca1fdc`, rebased onto the user's gdepth refactor `ccc3e31a`/`37b07852` and fast-forwarded into `master` on 2026-09-29; branch `aa-shader-port` deleted; not pushed): the second family port. The last commit, `9aca1fdc`, makes `tqaa_resolve.frag` use `GDEPTH_UNPACK` from `config/glsl/shared/gdepth.glsl` for the packed and linear depth reads; its hyperbolic path stays local (it reprojects the raw sample, not linearized depth). Re-verified after that commit: 522/522 `TEXT` against the old generators, and the golden check below with 0 `EXTRA` this time. `tqaaresolve` (`aa.cfg`), `fxaa*` (`fxaa.cfg`, deleted) and the four `SMAA*` passes (`smaa.cfg`, deleted) moved to plain GLSL under `config/glsl/aa/`, loaded with `shader_new`; `tqaaresolve` uses the new `lazyshader_new` alias (`config/glsl/shared.cfg`). No C++ changes.
  - Proof 1, every argument combination: `fxaashaders`/`smaashaders` read only their arguments, so the old generators were called directly for all 8 FXAA and 512 SMAA configurations (including the unreachable `a` option), plus a gather-off `tqaaresolve` reference, and dumped (`shaderdumpall aagen s00 0`) before porting. After porting, all 522 same-name pairs are `TEXT` (`shadercheck.py --pairs`), and the 1,064 non-AA rows of the same dumps are hash-identical. A mutation run (one constant each in SMAA preset 2, FXAA preset 1, gather-off resolve) turned exactly the expected 67 rows `DIFF`. Corpora `home/uitest/shadercorpus/aagen{,-cand,-mut}` and scratch cfgs `home/uitest/aagen-*.cfg` are gitignored and can be deleted.
  - Proof 2, golden baseline: `shaders.ps1 check -Sids s00,s01,s24,s25,s26,s27,s28,s31,s32,s99 -NoMaps`: every AA row `PASS-TEXT` (20), 14,461 `PASS-TEXT` overall, 30 `PASS-SPIRV` (the AO rows, as before), 20 `EXTRA` (`modelA0`/`modelnme0` at s31, known noise, §6 item 1a), 0 `MISSING`/`FAIL`.
  - GLSL 1.20: all new files preprocess cleanly under glslang at `#version 120` for 173 define sets (the only new constructs are directives; code tokens are proven unchanged).
  - Trap found: `shaders.ps1 check` reuses the running client, and shaders made by calling a generator alias directly survive `resetshaders`, so `generateshader` finds them instead of generating. Restart the harness after direct generator calls. Plan with deviations: `doc/superpowers/plans/2026-09-27-aa-shader-port.md`.
- **Blur shader port** (3 commits `4e1f3104`..`dfaae744` fast-forwarded into `master` on 2026-09-29; branch `blur-shader-port` deleted; not pushed): the third family port. `blurshader` (`config/glsl/blur.cfg`) now passes `BLUR_RADIUS`/`BLUR_X`/`BLUR_RECT`/`BLUR_ALPHA` to `config/glsl/blur/blur.vert`/`.frag` (+ `blur_defs.glsl`: literal `BLUR_SIZE`, `BLUR_AXIS`); taps unrolled one macro per line. New shared includes: `config/glsl/shared/screentexcoord.glsl` (`vtexcoord0/1`, now used by all 6 `ao/`+`aa/` vertex shaders too) and `config/glsl/shared/luma.glsl` (`LUMWEIGHTS`, must stay in step with the `lumweights` alias until hud/sky/tonemap are ported). No C++ changes.
  - Proof: all 56 blur shaders are standard shaders registered by `glsl.cfg`, so the golden baseline has them at every sid. `check -Sids s00,s01,s24,s25,s26,s27,s28,s31,s32,s99 -NoMaps`: 560/560 blur rows `PASS-TEXT`; totals identical to the AA proof (14,461 TEXT, the same 30 AO SPIRV rows, 20 `EXTRA` model noise at s32, 0 MISSING/FAIL). `check -Run aogaps` (13 sids): every AO row TEXT/SPIRV as in the AO port; model noise only (20 MISSING/20 EXTRA, and the s00/s99 leak check trips on `modelA0`/`modelnme0`, which makes the command exit 1).
  - Mutation run (tap 5 sampling the wrong side, `LUMWEIGHTS` +0.0001): the 24 radius-5+ rows `FAIL`, the 16 alpha radius-1–4 rows fell from TEXT to PIXEL (the luma change is below pixel resolution; tiers 1–2 flagged it), the other 17 stayed TEXT. Reverted.
  - GLSL 1.20: all 112 stages compile under glslang at `#version 120` (no-`EXT_gpu_shader4` header); a planted `<<` is rejected, so the test is live. Plan: `doc/superpowers/plans/2026-09-29-blur-shader-port.md`.
- **Decal shader port** (3 commits `5f74772b`..`c2e906be` fast-forwarded into `master` on 2026-09-29; branch `decal-shader-port` deleted; not pushed): the fourth family port. `decalvariantshader` (`config/glsl/decal.cfg`) still stages the slot params, then calls `variantshader_new` with `DECAL_*` defines (one per type letter, `DECAL_PASS0`/`DECAL_PASS1` for the dual-source passes) and `USEPACKNORM` into `config/glsl/decal/decal.vert`/`.frag`. The output declarations are in `decal/out_{pass0,pass1,pass1_blend,single}.glsl`, which the alias picks, because `findfragdatalocs` (and the harness contract) scan `fragdata(`/`fragblend(` from the raw text, ignoring `#if` (documented in "Porting a generator"). New shared includes: `shared/gnormal.glsl` (`GNORMAL_PACK`, `GNORMAL_PACK_BLEND`; needs `USEPACKNORM`) and `shared/gcolor.glsl` (`GSPEC_PACK`, `GSPEC_PACK_SPEC`, `GGLOW_PACK`, `GGLOW_PACKNORM`). World/model/material ports will need the blend-layer `gspecpack` and the glow-less `gglowpack` forms added. No C++ changes.
  - Proof: golden `check` over all 46 baseline sids, `-NoMaps -Filter '*decal'`: 7958/7958 `PASS-TEXT` (173 rows × 46), registry unchanged, no leak. Single-pass path (`$maxdualdrawbufs` 0, unreachable here): pure-generator corpora `decalgen`/`decalgen-cand` (every type × dual/single × `forcepacknorm` 0/1, through `defershader` + `shaderforceall`), 518/518 pairs `TEXT` via `shadercheck.py --pairs`, `meta.txt`/`reflect.txt` identical. Mutation (pulse constant in `decal.vert`): exactly the 56 pulse-glow rows FAIL. GLSL 1.20: the 172 valid single-pass shaders compile under the no-`EXT_gpu_shader4` header on both sides; planted `<<` rejected in all 172.
  - **Pre-existing bug, unchanged by the port:** the single-pass `R` types without glow (`envspecmapdecal`, `envspecmapdispdecal`, `bumpenvspecmap{,disp,parallax,parallaxdisp}decal`) fail to compile, `undefined variable "spec"`: `spec` is only declared in pass 0 or by glow. Only reachable without dual-source blending. Both sides fail identically (12 shaders in the gen corpora).
  - Direct calls to a variant generator leak `defuniformparam`s into the next shader (a variant takes its parent's defaults); wrap them in `defershader` + `shaderforceall`. Plan: `doc/superpowers/plans/2026-09-29-decal-shader-port.md`.
- **Deferred light shader port** (3 commits `7103f059`..`815aa029` fast-forwarded into `master` on 2026-09-29; branch `deferred-shader-port` deleted; not pushed): the fifth family port. `deferredlightvariantshader` (`config/glsl/deferred.cfg`) passes `DL_ROW`, `DL_NUMLIGHTS`/`DL_NUMSPLITS`/`DL_NUMRH`, one define per type letter (`DL_*`, `SMFILTER_*` for the filter letters) and the engine state (`MSAA_SAMPLES`, `USEPACKNORM`, `GHASSTENCIL`, `GDEPTH_FORMAT`, `USETEXGATHER`, `GLEXT_SAMPLES_IDENTICAL`, `AVATAR_SHADOW_BIAS/DIST`) to `config/glsl/deferred/`: `deferredlight_defs.glsl` (derived switches, per-light macros), `deferredlight_decls.glsl` (extensions, uniforms, output, spot/point shadow lookups), then `shared/smfilter.glsl`, then `deferredlight.frag`. `msaaedgedetect` is a `lazyshader_new` over `deferred/msaaedge.glsl` (`MSAA_EDGE_DETECT(action)`). `deferredlightshader` (row registration) is unchanged. New shared code: `shared/smfilter.glsl` (the former `smfilter*` aliases, deleted from `smfilter.cfg`; its `smalpha*` shaders stay), `GDEPTH_UNPACK_ORTHO` and `GDEPTH_UNPACK_POS` (`gdepth.glsl`, the latter at the user's request), `GNORMAL_UNPACK_SCALE` (`gnormal.glsl`), `GSPEC_UNPACK` (`gcolor.glsl`). `unpacknorm`/`unpackspec` moved to `shared.cfg` for `ui.cfg`. No C++ changes.
  - **No `##`:** glslang rejects token pasting below `#version 130`, and the engine can emit 120, so the per-light code (11 index-suffixed names) is one `DL_LIGHT(j)` macro with `{ }`-scoped plain names, one line per light under `#if DL_NUMLIGHTS > j`. Rows with lights are therefore `PASS-SPIRV`, rows without `PASS-TEXT`. The user offered a per-light function as the fallback; not needed.
  - Proof 1, golden `shaders.ps1 check` (all 46 sids + 50 maps, no filter): deferred-light rows 585 `PASS-TEXT` + 17,607 `PASS-SPIRV`, 0 FAIL; `msaaedgedetect`, `modelpreview`, `smalpha*` TEXT at every sid; registry unchanged; no leak. Totals 61,720 TEXT, 17,890 SPIRV (the rest are the AO rows as before), 0 fail; 520 MISSING / 6,740 EXTRA, all model shaders (§6 item 1a, baseline predates the deterministic map pass), which is why the command exits 1.
  - Proof 2, generator corpora `dlgen` (old generator, recorded first) / `dlgen-cand2`: direct `deferredlightshader` calls (`home/uitest/dlgen-calls*.cfg`: all filters, CSM 1–8 × RH 1–4, 1–8 lights, `D`/`z`/no-`a`/no-`b`/minimap, and `MS`/`MO`/`MOT`/`MR`/`MRT`/`M`/`D` MSAA types) in fresh clients at 8 engine states (gdepthformat 0/1/3, packnorm + no stencil, msaa 2/4/8/16, and `gX` = `$usetexgather` 2 through a mechanically renamed copy of each generator, `dlgen-override-{ref,cand}.cfg`). 16,266 pairs: 1,122 TEXT, 15,144 SPIRV, 0 DIFF/NA/one-sided. Every SPIRV pair differs from the old text only by the light renames and braces (checked token by token). Driver/pairing scripts were scratch (`dlgen.ps1`, `dlpairs.py`); corpora under `home/uitest/shadercorpus/dlgen*` are deletable.
  - `GL_EXT_shader_samples_identical` is absent on this GPU, so its path was proved on the helper: old `msaadetectedges` text (via `writetofile`, glext renamed) vs `MSAA_EDGE_DETECT` after `glslangValidator -E`, 2/4/8/16 samples × both actions × ext on/off: 16/16 TEXT. This caught a missing outer `{ }`, fixed before commit.
  - Mutation (`1e-5` → `2e-5` in `getspottc`): exactly the 309 shadowed-spot rows (5, 7, 13, 15, 21, 23, 29, 31) DIFF, nothing else. GLSL 1.20: 600 sampled non-MSAA shaders compiled at `#version 120` on both sides, same outcome per pair (57 fail identically: glslang has no 1.20 `textureGatherOffset` overload).
  - **Pre-existing, unchanged:** type strings with `A` but no `a` fail to compile (`aomask` undefined); the engine never makes them. Plan: `doc/superpowers/plans/2026-09-29-deferred-shader-port.md`.
- **World shader port** (4 commits `1cb4a96c`..`305dbcc6`, fast-forwarded into `master` with the volumetric port on 2026-09-29; branch deleted; not pushed): the sixth family port. `config/glsl/world.cfg` keeps the registrations (`worldshader`, `bumpshader`, `shadowmapshader`, `worldshaders`, `findworldshader`) and the slot-param staging; `worldvariantshader`/`bumpvariantshader` call `variantshader_new` through a shared `worldvariantdefines` (one `WORLD_*` define per type letter, plus `MSAA_LIGHT`, `MSAA_SAMPLES`, `GDEPTH_FORMAT`, `USEPACKNORM`). GLSL in `config/glsl/world/`: `world.vert/.frag`, `bump.vert/.frag`, `world_defs.glsl` (derived switches, `WORLD_TC`/`WORLD_DISP`/`WORLD_ROTTEXCOORD`), `shadowmap.*` (`smalphaworld`), `rsm.*` (`rsmworld`), `smworld.*`. New shared code: `shared/gbuffer.glsl` (`GBUFFER_DEPTH_DECLS/_VERT`, `GBUFFER_PACK_DEPTH[_HASH]`), `shared/gbuffer_out{,_depth}.glsl` picked by the new `gbufferoutputs` alias (`shared.cfg`), `shared/gfetch.glsl`, `GDEPTH_UNPACK_DECLS`/`GDEPTH_HASH` (`gdepth.glsl`), `GSPEC_PACK_BLEND`/`GSPEC_PACK_SPEC_BLEND` (`gcolor.glsl`). `rottexcoord` deleted from `shared.cfg` (world was its only user). Sweep points `s56`–`s63` added (msaa × glineardepth, `msaapreserve=-1`, `forcepacknorm=1`), recorded from the unported build into `home/uitest/shadercorpus/worldgaps/` (deletable once the baseline is re-recorded with them). No C++ changes.
  - **Expected tier change:** the g-buffer outputs now come from an include, so they precede the fragment's other declarations: world/bump rows are `PASS-SPIRV`, not `PASS-TEXT`. A token-level script proved that the move is the only difference (vertex stages token-identical; fragments identical with the output declarations set aside).
  - Proof: golden `check -NoMaps -Filter '*world'` over all 46 sids: 28,796 rows, 276 TEXT + 28,520 SPIRV, 0 fail/missing/extra, registry unchanged. `-Run worldgaps`: 7,512 rows, all TEXT/SPIRV. Unfiltered check at s00/s01/s31/s32: no other family changed (20 `modelA0`/`modelnme0` EXTRA at s31, §6 item 1a). Mutation (triplanar displacement): exactly the 104 `T`+`v` rows FAIL. GLSL 1.20: same outcome per pair in both corpora (refractive `$msaalight` variants fail identically: `sampler2DMS`). `shaders-selftest.ps1` 9/9. Plan with results: `doc/superpowers/plans/2026-09-29-world-shader-port.md`; proof scripts were scratch (`worldtok.py`, `glsl120.py`).
  - Kept as is: bump's `dispcoordy1`/`dispcoordz1`/`dispcoord1` read `dispscroll.xw` where world's read `.zw`; `rsmworld`'s dead `(= $i 2)` branch dropped (same tokens).
- **Volumetric light shader port** (3 commits `3ec4ef95`..`2c150e1c` on top of the world port, fast-forwarded into `master` on 2026-09-29; branch deleted; not pushed): the seventh family port. `config/glsl/volumetric.cfg` keeps the row registration (`volumetricshader`: parent + rows 0–4) and passes `VOL_ROW`, `VOL_STEPS`, `VOL_SHADOW` (`p`), the filter letters as the deferred `SMFILTER_*` names, `SMFILTER_SINGLE`/`SMFILTER_COLOR`, `GFETCH_MS` (`$msaalight`), `GDEPTH_FORMAT`, `USETEXGATHER`; the bilateral passes `VOLBILATERAL_TAPS`, `BILATERAL_REDUCE`, `BILATERAL_X`, `TEXRECT_MIN/MAXOFFSET`. GLSL in `config/glsl/volumetric/`: `volumetric.vert/.frag`, `volumetric_steps.glsl` (the `divf 1.0 steps` literal as printed, `%.6g`, generated for 1..64), `bilateral.frag` (per-tap macros take the `color<i>`/`depth<i>`/`weight<i>` names, one line per tap count, so TEXT). New shared code: `shared/bilateral.glsl` (`tapvec`, `texval[offset]`, `depthval[offset]` over `BILATERAL_DEPTHTEX`, `BILATERAL_FITS`, `BILATERAL_DEPTHSCALE`, moved out of `ao/bilateral.frag`), `shared/bilateral.vert` (was `ao/bilateral.vert`, now used by both), `SMFILTER_SINGLE` in `smfilter.glsl` (the volumetric one-compare `filtershadow` macro), and `SMFILTER_COLOR` no longer nested in `SMFILTER`. AO and deferred only had `#define`s moved, so their tokens are unchanged. No C++ changes.
  - Proof 1, generator corpora `volgen` (old generator, recorded first) / `volgen-cand`: `home/uitest/volgen-calls.cfg` (filters N/f/F/E/g/G × spot or not × `p`/`pP` × steps 1/16, plus steps 3/7/64; bilateral taps 1–3 × reduce 0–2) in fresh clients at 8 states (gdepthformat 0/1/3; msaa 4 with `$msaalight` 3, and 0 via `msaapreserve -1`; `gX` = `$usetexgather` 2 through renamed copies `volgen-override-{ref,cand}.cfg`). 1,792 pairs, **all TEXT**, none one-sided. Under MSAA the depth format follows the persisted `msaalineardepth 1` (not `glineardepth`), so MSAA × gdepthformat 0/3 was recorded separately with `msaalineardepth 0`/`3` (`volgen-ref2`/`volgen-cand2`, both through the override copies; the old copy reproduces `volgen` byte for byte): 672 pairs, all TEXT. The step table matches in-game `divf 1.0 n` for all 64. Mutation (`getspottc` `1e-5`→`2e-5`, reduce-1 `VOLBILATERAL_DEPTH2`): exactly the predicted 48 rows DIFF (44 shadowed-spot rows 3/4, 4 bilateral taps≥2 reduce 1). GLSL 1.20: same outcome on both sides for all 1,120 pairs checked (the failures are MSAA `sampler2DMS` and bilateral `texture2DRectOffset`, identical in the old generator). Scripts were scratch (`volgen.ps1`, `volpairs.py`, `vol120.py`); corpora `home/uitest/shadercorpus/volgen*` are deletable.
  - Proof 2, golden `shaders.ps1 check` (all 46 sids + 50 maps, no filter): 0 FAIL/WEAK/PIXEL; volumetric 209 TEXT (baseline has only the default map configs), bilateral 72 TEXT + 188 SPIRV and deferred-light 585 TEXT + 17,607 SPIRV, identical per row to the deferred port's check; every other tier change is the world port's TEXT→SPIRV (28,520 rows). 520 MISSING / 6,720 EXTRA, all model shaders (§6 item 1a), hence exit 1.
  - **Coverage gaps (not proved):** texture-rectangle offset limits other than the driver's -8/7; step counts other than 1/3/7/16/64 as compiled shaders (table-checked only); the golden baseline has volumetric rows only from the default-settings map pass (sweep points s21–s23 make none); 1.20 checked with glslang only. Details: `doc/superpowers/plans/2026-09-29-volumetric-shader-port.md` §"Coverage gaps".
- **Model shader port** (rebased as `71edd8fa`, `7b8fb9a3`, then the umbrella `fd99febc glsl: write the ported shaders as glsl 3.30` covering every ported family; fast-forwarded into `master` 2026-10-02, not pushed. Re-proved after the 3.30 rewrite: same 450 TEXT + 24,950 SPIRV vs the unported generators, all distinct model blobs compile at `#version 330`): `modelshader`, `rsmmodelshader`, `shadowmodelshader`, `halomodelshader` (`config/glsl/model.cfg`) pass one `MODEL_*` define per type letter plus engine state to `config/glsl/model/` (`model`, `rsmmodel`, `shadowmodel`, `halomodel` `.vert`/`.frag`; directive-only `model_defs.glsl`, `skelanim.glsl` (included for `b` only: it carries the `animdata` uniform pragma), `wind.glsl`, `effect.glsl`) and new `shared/rotateuv.glsl` (`ROTATEUV_FUNC`). Adds `GGLOW_PACK_WEIGHT`, `GDEPTH_HASH_ALPHA`, `GBUFFER_PACK_DEPTH[_HASH]_ALPHA`; `rsmmodel` uses `shared/rsm_out.glsl`. Proof: generator corpora `mdlgen`/`mdlgen-cand` over 10 engine states, 25,400 pairs (450 TEXT, 24,950 SPIRV = outputs moved by the include, token-checked), 0 DIFF; non-model rows all TEXT; mutation exact (655 wind rows); GLSL 1.20 same per pair; golden `-Filter *model*` 0 fail. Open question to the user: shared GLSL functions are one-line macros (`ROTATEUV_FUNC`, `WIND_FUNCS`, `MODEL_EFFECT_RAND`) to keep TEXT; real functions in includes would cost TEXT for SPIRV. Plan with results: `doc/superpowers/plans/2026-09-30-model-shader-port.md`.
- **Shared g-buffer depth helpers** (`ccc3e31a`, `37b07852`, on `master` 2026-09-27): `config/glsl/shared/gdepth.glsl` holds
  `GDEPTH_UNPACK`/`GDEPTH_PACK` (the default `gdepthunpack` and `gpackdepth`); the three AO shaders include it. Checked with the
  AO family checks (baseline s00–s44 and `aogaps`): same tiers as before. Later ports that unpack g-buffer depth should use it
  (`doc/shader-reference.md` "Porting a generator"). **Merging `aa-shader-port` onto this `master` conflicts in
  `doc/shader-reference.md`**: both append a bullet to the end of "Porting a generator"; keep both.
- **`master`**, HEAD `815aa029 doc: describe the deferred light shader files`. **79 commits ahead of
  `origin/master`, not pushed.** Everything below is on `master`, landed as fast-forwards:
  - the map editor harness and prefabs (2026-09-24);
  - the texture slot fixes;
  - the chat scroll refactor (`bb33a50d`);
  - the shader equivalence harness (2026-09-25/26);
  - the user's occlusion-query fix (`2122efb9`).
- **Branch `texture-slot-editing-fixes`** still exists but is fully contained in `master`. Safe to
  delete (`git branch -d texture-slot-editing-fixes`); left for the user.
- **Shader equivalence harness** (the start of the shader refactor) is **done and committed**. It is a
  tool, not the refactor itself: it proves that porting CubeScript-generated GLSL to plain `#ifdef`
  GLSL changes nothing. Pieces:
  - engine commands `shaderdumpall`, `shaderbench`, `shaderorigin`, `shaderforceall`;
  - the driver `tools/harness/shaders.ps1` (`record`/`check`/`diff`);
  - the library `tools/harness/shadercorpus.ps1`;
  - the offline text and SPIR-V tiers `tools/harness/shadercheck.py`, run under WSL, which needs
    `glslang-tools` and `spirv-tools` (both installed);
  - the sweep `tools/harness/shader-sweep.txt`;
  - the self-test `tools/harness/shaders-selftest.ps1`.

  Usage is in [tools/harness/README.md](../tools/harness/README.md), "Shader equivalence harness".
- **The shader baseline is recorded** at `home/uitest/shadercorpus/baseline/`:
  - taken from commit `f250748d` with a clean `config/glsl`, on 2026-09-26. It was re-recorded after
    the sweep gained `s39`–`s44`, which cover `aoderivnormal`, `aobilateralupscale` and `aofloatdepth` 0/2;
  - 46 settings points plus all 50 shipped maps;
  - no state leaked between the first and last default points, and all 247 editor world/decal shaders
    are valid everywhere.

  It is **tied to this GPU and driver** (RTX 3080, 591.86; see its `gl.txt`). `check` refuses to
  compare after a driver change unless `-AllowCrossGpu` is passed. Re-record from the commit in its
  `run.txt` instead.

  **Never delete `home/uitest/shadercorpus/baseline/`.** Re-recording takes about an hour.

  The acceptance "self-check" (a full `check` of the unchanged build against the baseline, twice) was
  deliberately **not run**, at the user's call.
- **Engine changes made alongside the harness** (they affect normal play, reviewed):
  - `slotparamsscope` in `src/engine/shader.cpp` stops leftover texture-slot params from leaking into
    shaders created later. They became stray `uniform vec4` declarations and made generation depend on
    history. It is applied at `loadshaders`, `setupshaders`, `generateshader` and `Shader::force`.
  - `compileglslshader` was split into `composeglslparts` (byte-identical output).
  - Each `Shader` now records its `origin` and whether it was `generated`.
- **Map editor prefabs** (engine `saveprefab`/`loadprefab` fixes plus `prefabinfo`, `removeprefab`,
  `renameprefab`; UI in `config/ui/tool/toolprefab.cfg`; library in `config/tool/toolprefab.cfg`;
  engine surface in `src/engine/octaedit.cpp`) is **done and committed** (`1833a4b9`). Open
  follow-ups: `uidumpeditors` and `uidumptree` (`src/engine/ui.cpp`) lack the `IDF_MAP` refusal that
  test-only commands should have (one line each); deferred polish is listed in the ledger
  `.superpowers/sdd/2026-09-24-map-editor-prefabs/progress.md` ("parked" lines). Design and plan:
  [doc/superpowers/specs/2026-09-24-map-editor-prefabs-design.md](superpowers/specs/2026-09-24-map-editor-prefabs-design.md)
  (see its "Deviations during implementation" section),
  [doc/superpowers/plans/2026-09-24-map-editor-prefabs.md](superpowers/plans/2026-09-24-map-editor-prefabs.md).
- **Texture slot editing fixes** (4 engine, 3 ui, 1 harness commit, `3b148100`..`0dd7aae5`) are on
  `master` now. Defects, repro steps and what's left:
  [doc/texture-slot-editing-findings.md](texture-slot-editing-findings.md).
- **GI split stability** (2026-10-01, 4 commits `33203a02`..`c1702225` on branch `rh-split-stability`, from `master` `949694fd`;
  **not merged, not pushed**). The research is in
  [doc/gi-radiance-hints-findings.md](gi-radiance-hints-findings.md); fixes 1 and 2 are **implemented** from
  [the spec](superpowers/specs/2026-10-01-rh-split-stability-design.md) (approved, default off) and
  [the plan](superpowers/plans/2026-10-01-rh-split-stability.md) (its `## Results` table holds the numbers).
  - `33203a02` renderlights: size the rh splits from the unzoomed fov (`basefov`, `rhboundsfov()`, read-only
    `rhsplitresets`; DEBUG_UTILS `edzoom <fov>`; `gi.ps1 zoom`).
  - `143f9316` harness: probe the rh lighting at fixed points (DEBUG_UTILS `rhprobe`, the real `getrhlight` via the
    `DL_RHPROBE` main and `rhprobeshader`; `gi.ps1 sweep`/`near`; `probestats.py`).
  - `f1150cef` renderlights: crossfade between radiance hint splits. Opt-in: `rhblend` (band in cells, **default 0 =
    off**) and `rhblendmargin` (cells kept fully fine around the camera, default 2). Shader option `h` ->
    `DL_RHBLEND`. The lookup accumulates front to back (deviation from the spec, see its last section).
  - `c1702225` doc: describe the rh split crossfade and gi harness (`tools/harness/README.md` "GI stability checks",
    `doc/shader-reference.md` "Radiance hint split blending").
  - Results (park, 1600x900): zoom `rhsplitresets` +80 before the fix, +0 after, +0 with `rhblend 2`. Translate sweep
    (72 x 1u) J 0.0275 -> 0.0012 (0.045), rotate (180 x 1 deg) 0.0374 -> 0.0032 (0.084), `rhsplits 3` 0.0274 -> 0.00067,
    `rhsplits 4` 0.0188 -> 0.00071, near 0.000000 over 135 probes; all PASS. `shaders.ps1 check` against the
    pre-change corpus `rhb-base`: 13518/13518 `PASS-TEXT`. Cost: Deferred Shading (gpu) 0.12 ms at `rhblend 0` and `2`.
  - **Verification is the fixed-point probe, not screenshots** (user decision): screenshot diffs are swamped by
    animation, exposure and parallax. Don't go back to screenshot sweeps.
  - **Not covered:** a real in-game weapon zoom (`edzoom` is a stand-in), still a manual check; see §6 item 14.
  - Temporal accumulation (findings items 3-5) is not specced yet.
- **Stale stash `stash@{0}` ("WIP on map-editor-harness: f4b2a61a")** is a leftover from an earlier
  pause and is superseded by commit `1833a4b9`. Safe to drop (confirm it's still index 0 first). Left
  for the user.
- **Remotes:** `origin` is upstream `redeclipse/base`; `fork` is the user's `q009/base`. **Never push
  unless asked**, and never push to `origin`.
- **Uncommitted and unrelated to agent work. Leave these alone:** `readme.md` (modified),
  `chat_wip.cfg`, `deli.zip`, `gun_lore.txt`, `profile_daemon.ps1`, `profiler_tools.zip`,
  `profilerhook.cfg`, `redeclipse-crash.dmp`, `unix/`. **Stage explicit paths only; never
  `git add -A`.**
- **Git identity is configured** (`Sławomir 'Q009' Błauciak`). The shader harness commits carry a
  `Co-Authored-By` trailer.
- **OpenGL 3.3 floor** (`2ae84f8d`, `7de21564`, fast-forwarded into `master` on 2026-10-02; branch deleted; not pushed): the minimum went
  from GL 2.0 / GLSL 1.20 to **GL 3.3 core / GLSL 3.30**, and the pre-3.3 fallback code is removed
  (context ladder `main.cpp`, extension ladder `gl_checkextensions`, `glemu.cpp` client-array path,
  GLSL 1.20 prelude in `composeglslparts`, luminance/no-swizzle texture paths, `glcompat`,
  `intel_mapbufferrange_bug`). `amd_eal_bug` and `mesa_texrectoffset_bug` are kept. The `#version 400`
  prelude is byte-identical (`shaders.ps1 check`, 0 fail). The shader porting rules in
  `doc/shader-reference.md` are updated: `##`, `<<` and `uint` are now allowed.
- **The last build was `release`** (2026-10-02, `7de21564`). Earlier it was `debug` at `e1ffc0ba`. Switching build type triggers a full
  `make clean`.

Quick orientation:

```bash
git status --short
```

```bash
git log --oneline origin/master..master
```

---

## 4. Build, run, verify

Build from PowerShell. From Git Bash, the `/mnt/f` path gets rewritten into a Windows path and the
build fails.

```powershell
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
```

- Use `debug` for harness and test work. It is the only build type that compiles `src/tests/*.o`.
  `DEBUG_UTILS` (the harness commands) is on in both debug and release.
- **Stop the harness before building.** The running game locks `bin/amd64/redeclipse.exe`, and the
  copy step fails with `Error 1` after a successful compile.
- The debug build still links the **release** CRT (`-static`). `_CrtSetReportHook` and other debug-CRT
  APIs don't exist in it, and `<crtdbg.h>` clashes with `tools.h`. See findings §2.2.

Test suites, cheapest first:

| Suite | Needs a running game? | Time | Command |
|---|---|---|---|
| EDSTATE parser units | no | seconds | `powershell -ExecutionPolicy Bypass -File tools\harness\tests\edstate.tests.ps1` |
| UI harness smoke | yes | ~1 min | `powershell -ExecutionPolicy Bypass -File tools\harness\tests\task5-smoke.ps1` |
| Editor driver checks | yes | ~1 min | `powershell -ExecutionPolicy Bypass -File tools\harness\tests\task8-editor.ps1` |
| Editor end-to-end | starts its own game | several min | `powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1` |
| Prefab self-test | starts its own game | ~2 min | `powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1` |
| Texture slot self-test | starts its own game | ~1 min | `powershell -ExecutionPolicy Bypass -File tools\harness\texslot-selftest.ps1` |
| Shader corpus library units | no | seconds | `powershell -NoProfile -File tools\harness\tests\shadercorpus.tests.ps1` |
| Shader offline tiers (Python) | no (WSL) | seconds | `wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py"` |
| Shader harness self-test | starts its own game | ~5 min | `powershell -ExecutionPolicy Bypass -File tools\harness\shaders-selftest.ps1`. It mutates `config/glsl/world.cfg` and `init.cfg` and restores them byte for byte, so those two files must be clean first. |
| Shader per-task checks | yes | 1–3 min each | `tools\harness\tests\task{3-dump,5-record,7-bench,8-check}.ps1` |
| Shader loader live test | yes | ~1 min | `powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1` |
| C++ unit tests | run at game startup | — | `src/tests/*.cpp` (`testslotmanager`, `testedharness`, `testprefab`, `testshaderharness`, `testshadersource`) |

The C++ unit tests run before the log file is opened, so a passing `ok` line never appears in
`log.txt`; a clean `harness.ps1 start` under `RE_CRASHLOG` (no abort, no backtrace) is the pass
signal. A failing `ASSERT` aborts startup and writes a backtrace to `log.txt`.

Last known result, on 2026-09-24 at HEAD `f6823f98` with the map-editor-prefabs work staged: all
suites green — EDSTATE parser units 42/42, UI harness smoke 7/7, editor driver checks 8/8, editor
end-to-end 84/84, prefab self-test 25/25, and a clean startup (no assert/backtrace). See the Task 6
report for the full regression run: `.superpowers/sdd/2026-09-24-map-editor-prefabs/task-6-report.md`.

Shader harness, last known result (2026-09-25/26, at `cdc8f038` and after):
- corpus units all pass; Python 20/20;
- `task3`/`task5`/`task7`/`task8` pass;
- the self-test (9 steps) passes;
- the full baseline recorded cleanly at `e1ffc0ba`.

Reports: `.superpowers/sdd/2026-09-25-shader-equivalence-harness/final-fix-report.md`.

For suites that need a running game, start one first:

```powershell
tools\harness\harness.ps1 start
```

---

## 5. The harnesses (agent tooling)

All harnesses drive **one** running client over **one** command channel. `harness.ps1 start` launches
it against the isolated home `home/uitest/`.

| Tool | Use it for | Entry points |
|---|---|---|
| `tools/harness/harness.ps1` | UI work: send CubeScript, navigate panels, screenshot, hot-reload `.cfg`, dump the widget tree, click by label | `start`, `stop`, `status`, `send`, `nav`, `shot`, `reload`, `tree`, `find`, `click`, `log` |
| `tools/harness/editor.ps1` | Map editor work: enter editing, place and aim the view, TAB cursor lock, select geometry and entities, read state | `open <map>`, `newmap [size]`, `state`, `goto`, `aim`, `lookat`, `lookatent`, `frame`, `frameent`, `nudge`, `cursor`, `key`, `sel`, `seldrag`, `entsel`, `shot` |
| `tools/harness/core.ps1` | Shared transport and plumbing. Both drivers dot-source it. | `Invoke-Batch`, `Invoke-Shot`, `ConvertTo-InvariantDouble`, … |
| `tools/harness/edstate.ps1` | Parser for `eddumpstate` output | `ConvertFrom-EdState` |
| `tools/harness/gi.ps1` | GI (radiance hints) stability checks from the editor: zoom must not resize splits, and a probe of the real `getrhlight` at fixed world points across a camera sweep (needs a running client in edit mode on a map with GI, e.g. `park`) | `zoom`, `sweep -Kind translate\|rotate -Blend`, `near`; `probestats.py` scores, README "GI stability checks" |
| `tools/harness/shaders.ps1` | Shader refactor: record a corpus of every shader configuration across a settings sweep; compare a candidate build with the baseline (contract → text → SPIR-V → pixel) | `record`, `check [-Filter] [-Sids] [-NoMaps] [-MaxTier] [-PassThru]`, `diff <name> -Sid` |
| `tools/harness/shadercorpus.ps1` | Pure library behind `shaders.ps1` (manifest parsing, contract diff, registry compare, bench-line parsing) | `Compare-Corpus`, `Compare-Registry`, `ConvertFrom-BenchLine`, … |
| `tools/harness/shadercheck.py` | Offline tiers 1–2, run under WSL | `--pairs`, `--normalize` |
| `tools/harness/editor.cfg` | In-game aliases that drive the **real** binds | `edh_enter`, `edh_cursor`, `edh_tap`, `edh_dragsel`, … |

Rules that matter:

- **Editing UI needs no rebuild.** UI is CubeScript: edit `.cfg`, run `reload`, run `shot`, look at the
  PNG. Engine-side harness commands (`ed*`, `gamekeypress`, `eddumpstate`) do need a rebuild.
- **Two paths, deliberately.** `sel` and `entsel` without `-Hover` are coordinate shortcuts **for
  setup only**. Assert behaviour through `seldrag` (a real MOUSE1 drag) and `entsel -Hover` (aim, then
  `entadd`).
- **Enter the editor with `editor.ps1 open` or `newmap`**, never the raw engine `newmap`. It cannot
  enter edit mode from a cold start (findings §4).
- **Crashes are now visible.** `harness.ps1 start` sets `RE_CRASHLOG=1` for the game only. A crash
  throws `Game exited while running batch N. Last log: ...`, and `home/uitest/log.txt` has a
  per-frame backtrace. The minidump is at `home/uitest/redeclipse-crash.dmp` if you need windbg/cdb.
  Details are in README "Crash diagnostics".
- **Never minimize the game window.** Screenshots come back black. Unfocused is fine.
- **Shader harness loop:** port a family, then run `shaders.ps1 check -Filter '<family>*'` (on the
  full sweep, about as long as a recording). For a fast inner loop use `-Sids s00 -NoMaps`. On
  anything other than `PASS-*`, run `shaders.ps1 diff <name> -Sid <sid>`.
  - `.cfg` edits need no rebuild; `check` runs `resetshaders` itself.
  - `-Filter` also matches `<variant:…>` rows.
  - `WEAK` and `EXTRA` exit 0 but need a human look. `PASS-PIXEL` is the weakest evidence.
  - Adding a var that a generator reads means adding a vector to `shader-sweep.txt` and re-recording
    the baseline.
- **All test-only engine commands** are `#ifdef DEBUG_UTILS` and refuse `IDF_MAP`, so a downloaded map
  can't reach them. The editor view commands also refuse outside edit mode. Any new command must keep
  both properties. The command tables are in CLAUDE.md.

---

## 6. Open work queue

In rough priority order. Each item links to its detail.

| # | Item | Kind | Detail |
|---|---|---|---|
| 1 | **Shader refactor, family ports (next session).** Replace CubeScript GLSL generation with plain `#ifdef` GLSL one family at a time, and prove each port with `shaders.ps1 check` against the recorded baseline. Planned order:<br>1. ~~Port the loader~~ **Done** and merged into `master` (§3).<br>2. ~~Bring over the AO family~~ **Done** and merged into `master` (§3). Diverged from the imprimis#72 starting point as expected; the taps stayed unrolled and landed at `PASS-SPIRV`, not `PASS-PIXEL`.<br>3. ~~AA~~ **Done** and merged into `master` (§3); `lazyshader_new` added, `tqaaresolve` uses the shared gdepth helpers.<br>4. ~~Blur~~ **Done** and merged into `master` (§3); adds `shared/screentexcoord.glsl` and `shared/luma.glsl`.<br>5. ~~Decals~~ **Done** and merged into `master` (§3); adds `shared/gnormal.glsl` and `shared/gcolor.glsl`.<br>6. ~~Deferred lights~~ **Done** and merged into `master` (§3); adds `shared/smfilter.glsl`, `GDEPTH_UNPACK_ORTHO`/`_POS`, `GNORMAL_UNPACK_SCALE`, `GSPEC_UNPACK`.<br>7. ~~World~~ **Done** and merged into `master` (§3); adds `shared/gbuffer.glsl`, `gbuffer_out*.glsl`, `gfetch.glsl`, the blend `GSPEC_PACK_*`.<br>8. ~~Volumetric~~ **Done** and merged into `master` (§3); adds `shared/bilateral.glsl`/`bilateral.vert`, `SMFILTER_SINGLE`.<br>9. Radiance hints and models done (models on branch `model-shader-port`). Next: the remaining families (material, grass, ui, ...); they reuse `gbuffer.glsl`/`gbufferoutputs`/`gfetch.glsl`. Follow the pattern in [doc/shader-reference.md](shader-reference.md) "Porting a generator".<br>Imprimis files are a starting point, not a drop-in: Red Eclipse's generators have diverged. | feature | spec §"Related work"; plan §"Related work: the Imprimis port"; memory `shader-refactor` |
| 1a | **The per-map pass of `shaders.ps1 check` is nondeterministic.** Map-generated model shaders (`modelA`, `modelA0`, `modelnme`, `modelnme0` and their variants) on gauntlet, challenges-port-06 and deathtrap are sometimes not produced, giving `MISSING` rows that also occur on the baseline's own commit. It blocks nothing: treat these rows as noise until fixed. **Root cause found and fixed on `master` in `61b3543a`/`2734a70a` (2026-09-26).** Model shaders were only generated for models that were drawn after `resetshaders`, mostly the roaming AI `actors/janitor` and its debris. Now the maps-only `shaderdumpall` generates every map-entity model's shaders in every skin state and dumps only those. Five runs gave identical output, and the fix makes the new `task5-record.ps1` assertion pass. **It changes baseline rows (90 MISSING / 240 EXTRA on the 3 maps, the same set every run), so the baseline needs a re-record, from a rebuilt `master`. The user decides.** The pending re-record should now also fold sweep points `s45`-`s55` into the golden baseline's sweep (they're already in `tools/harness/shader-sweep.txt`, added for the AO port, §3); once they're in the baseline, the separate `aogaps` corpus (`home/uitest/shadercorpus/aogaps/`) can be deleted. **Also occurs at sweep sids, not only the 3 maps**: reproduced on the unchanged build during the AO port's Task 1 (sids `s01`/`s31`/`s41` as `EXTRA`, then `s51` as `MISSING` and `s99` as `EXTRA` on a second recording of the same build) — see `.superpowers/sdd/2026-09-27-ao-shader-port/task-1-report.md`. | harness | ledger `.superpowers/sdd/2026-09-26-shader-source-loader/progress.md`, "Task 4 evidence"; `.superpowers/sdd/2026-09-27-ao-shader-port/task-1-report.md` |
| 2 | **Stale `enthover` crashes the client on map load.** Reachable in normal play, e.g. a vote map change. Likely a one-line fix in `entcancel()` (`src/engine/world.cpp:389`). The user has not yet decided whether to fix it. | engine bug | findings §1.1 |
| 3 | **`fatal()` hangs the game instead of exiting under `RE_CRASHLOG=1`.** Seen at `renderlights.cpp:857` "Failed allocating g-buffer!" during the shader sweep: the harness only saw a 300 s timeout. This contradicts the documented crash-log behaviour. For a separate harness session. | harness / engine | ledger `.superpowers/sdd/2026-09-25-shader-equivalence-harness/progress.md`, "Baseline attempt 2" |
| 4 | Decide whether to push `master` (70 commits ahead of `origin/master`) to `fork` | user decision | §3 |
| 5 | README command table is split by a paragraph; the `seldrag`/`shot` rows don't render | doc bug | findings §3.1 |
| 6 | `editor.ps1 -Pitch` default of 20 aims the orbit camera up from below the target. `-20` is probably intended. It is an interface change, so ask first. | harness | findings §3.1 |
| 7 | Crashes on non-main threads leave no trace or dump in harness mode (use `SetUnhandledExceptionFilter`) | harness / engine | findings §2.1 |
| 8 | The same stale-index pattern exists in `newundoent()` (`src/engine/world.cpp:406`) | latent engine bug | findings §1.2 |
| 9 | Dead `findkeycode(char*)` (`src/engine/console.cpp:331`) is an overload-resolution trap | cleanup | findings §1.3 |
| 10 | Release crash path verified by reading and diffing only; never run | verification gap | findings §2.1 |
| 11 | 12 small deferred harness items | polish | findings §3.3 |
| 12 | Texture slot editing leftovers: material `editslot` creates a world slot; null derefs in `getvshadername`/`getvgrasstex*`; dead `tool_tex_shader_param_init` | engine / ui | [texture-slot-editing-findings.md](texture-slot-editing-findings.md) "Not fixed" |
| 13 | Shader harness deferred minors. The ones that matter:<br>- struct-array uniforms aren't restored after a bench;<br>- pre-330 dual-source outputs aren't bound on the old program;<br>- only one `<variant:…>` prefix is stripped by `-Filter`;<br>- `glprobe.txt` is left in the corpus root. | polish | ledger `minor (deferred):` and "residual" lines |
| 14 | **GI split pop-in: fixes 1-2 are done** and merged into local `master` (33203a02..88956482, not pushed; §3), crossfade off by default. With `rhblend > 0` the last split also fades out to no GI at its edge (88956482, user request; no sweep). Next, in order:<br>1. **Manual zoom check** (not run): start a local game on `park` with a zoom weapon, `rhforce 0; rhinoq 0; timer 1`, zoom in and out while standing still. `echo $rhsplitresets` must not change and the Radiance Hints timer stays at 0.00 ms. Do it twice: with `thirdperson 0`, then with `thirdperson 1` (a zoom forces first person, so `basefov` comes from `fov(false)` in `game::fixview`). Keep camera feeds and envmap-heavy spots out of view: they also bump the counter.<br>2. **Tune `rhblend` / `rhblendmargin` and decide the default** (now 0 = off; `rhblend 2` to try it).<br>3. **Deferred minors** from the review ledger (now deleted; this is the full list): `gi.ps1` `zoom` can pass vacuously on a map without GI and has no control showing the counter moves; `gi.ps1` has no try/finally, so a failed run can leave `edzoom` or `rhblend` set; `rhprobe` sets only part of the GL state (safe between frames only); with 3+ splits at unusual `rhsplitweight`/`rhgrid` the crossfade can skip a middle split; `Invoke-ProbeStep` settles with a fixed 100 ms instead of a frame counter; the stale `.EXAMPLE` pose; `probestats` tests miss two edge cases; `splitinfo::blendcenter` is not initialised in the constructor; `rhblend` + `rhblendmargin` above `G/2-1`, or a tiny `rhblend`, are ill-conditioned and undocumented; the `rhblend` callback rebuilds shaders on every change, not only on 0 <-> >0; `rhboundsfov()` splits the RH var block.<br>4. Then findings items 3-5 (temporal accumulation, keeping history across sun changes, jitter), 7 (no-GI in every split) and 8 (camera feeds and mapshots rebuild the RH volume). | feature / engine | [gi-radiance-hints-findings.md](gi-radiance-hints-findings.md) §5-6; [spec](superpowers/specs/2026-10-01-rh-split-stability-design.md); [plan](superpowers/plans/2026-10-01-rh-split-stability.md) |

For engine bugs, verify with `editor-selftest.ps1`. It exercises map loads with entities present, and
it deliberately keeps the `entediting 0/1` workaround for item 2. If you fix item 2, remove that
workaround in `Invoke-MapLoad` (and in `shaders.ps1`'s `Open-Map`), and confirm the suites still pass
without it.

---

## 7. Decision trail (why things are the way they are)

| Document | Holds |
|---|---|
| [doc/superpowers/specs/2026-09-03-map-editor-harness-design.md](superpowers/specs/2026-09-03-map-editor-harness-design.md) | The approved design: goals, the three layers, the user's choices (discrete view commands, broadest command set, `open` + `newmap` both explicit) |
| [doc/superpowers/plans/2026-09-03-map-editor-harness.md](superpowers/plans/2026-09-03-map-editor-harness.md) | The 10-task plan. **It contains known defects** that were corrected during execution; don't treat it as current truth. |
| `.superpowers/sdd/2026-09-03-map-editor-harness/progress.md` | The execution ledger: 13 rulings, each with its reasoning and "cost if wrong", per-task review outcomes, every deferred and parked finding. Start here when asking "why does X deviate from the plan?" |
| `.superpowers/sdd/2026-09-03-map-editor-harness/task-N-report.md` | Per-task implementer reports with test evidence, including live `EDSTATE` captures (Task 2) and the engine-bug diagnosis (Task 9) |
| Commit messages on `master` | What each commit contains and why |
| [doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md](superpowers/specs/2026-09-25-shader-equivalence-harness-design.md) | Shader harness design: goal, tier ladder, corpus format, sweep and self-checks. Updated to match what was built. |
| [doc/superpowers/plans/2026-09-25-shader-equivalence-harness.md](superpowers/plans/2026-09-25-shader-equivalence-harness.md) | Its 9-task plan, including "Related work: the Imprimis port". Like the other plans, **it is not current truth**: the ledger records the corrections. |
| [doc/superpowers/specs/2026-10-01-rh-split-stability-design.md](superpowers/specs/2026-10-01-rh-split-stability-design.md) | GI split stability spec (fixes 1–2): its decisions table separates what the user settled from what is still proposed. The reasoning behind it is in [gi-radiance-hints-findings.md](gi-radiance-hints-findings.md). |
| `.superpowers/sdd/2026-10-01-rh-split-stability/progress.md`, `task-N-report.md` | GI split stability ledger: the rulings (default off, front-to-back lookup, probe instead of screenshots, dropped screenshot sweep `d78d453f`), per-task reviews, the deferred minors, and the measured results. [Plan](superpowers/plans/2026-10-01-rh-split-stability.md) has a `## Results` table. |
| `.superpowers/sdd/2026-09-25-shader-equivalence-harness/progress.md` | Shader harness ledger. Every ruling with its "cost if wrong", per-task review loops, the final-review fix wave, deferred minors, and the three baseline attempts (crash at s19 → the user's occlusion-query fix; `fatal()` at s33 → sweep fix; success). |

The plan's defects, corrected during execution:
- `newmap` as an entry point
- `ednudge`'s sign
- the `findkeycode` collision
- `havesel` clobbering
- vacuous test assertions

Each has a ruling in the ledger.

The shader harness plan's main corrections are recorded in its ledger:
- an `fnv1a` overload that hashed with the length as its basis;
- the slot-param leak, fixed in the engine rather than excused;
- `resetgl` driven by the engine's "Pending shader change" output;
- the registry compare and the cross-GPU metadata contract, both added after the final review;
- per-map resets.

---

## 8. Conventions (short form; CLAUDE.md is authoritative)

- **Do not commit or push unless asked.** When asked, branch first if on `master`. Subagent-driven
  runs commit per task on a feature branch; the user has then asked for it to be fast-forwarded into
  `master`.
- Commit messages are lowercase `area: summary`, e.g. `engine: ...`, `harness: ...`, `ui: ...`.
- `CLAUDE.md`, `GEMINI.md` and `.github/copilot-instructions.md` are gitignored: agent guidance stays
  local. Only `CLAUDE.md` exists at present.
- Windows PowerShell 5.1:
  - no `&&`, `||`, ternary or null-coalescing
  - parse game numbers with `ConvertTo-InvariantDouble`
  - write CubeScript files without a BOM (`Write-TextNoBom`)
- CubeScript:
  - no bare `#`
  - avoid `@` and use `concat` instead
  - `exec "path" 0 0`
  - the full list is in CLAUDE.md "CubeScript traps"
- Verify engine behaviour against source and a live client before writing it into docs. Several
  plausible-sounding facts in this project turned out to be wrong; findings §4 lists the ones that
  were checked.

---

## 9. Keeping this index current

- Update §3 (state) and §6 (queue) whenever you merge, push, fix a queued item, or leave new work
  behind. Change the date at the top.
- Put **pointers** here and **detail** in the linked document. If you're writing more than a few
  lines about one topic, it belongs in the findings doc, the README, or CLAUDE.md.
- New open bugs go in the findings doc first, then get a row in §6.
- This file is tracked (it lives in `doc/`); commit changes to it like any other doc.
