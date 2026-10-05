# AA Shader Port Implementation Plan

> Executed inline by the planning agent (no subagents), task by task, one commit per task on branch `aa-shader-port`.

**Goal:** Replace the CubeScript GLSL generators for anti-aliasing — `tqaaresolve` (`config/glsl/aa.cfg`), `fxaa*` (`config/glsl/fxaa.cfg`) and the four `SMAA*` passes (`config/glsl/smaa.cfg`) — with plain `.vert`/`.frag`/`.glsl` files under `config/glsl/aa/` plus `#define`s, and prove every configuration compiles to the same code.

**Architecture:** Same pattern as the AO port (`doc/shader-reference.md`, "Porting a generator"). `aa.cfg` keeps thin `shader_new` aliases; `fxaa.cfg` and `smaa.cfg` are deleted. The aliases pass raw values (engine vars, `generateshader` arguments) as defines; all branching is `#if` in GLSL. A new one-line-body alias `lazyshader_new` (`defershader` around `shader_new`) in `config/glsl/shared.cfg` replaces `lazyshader` for `tqaaresolve`. No C++ changes.

**Spec / precedent:** `doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md`; `doc/superpowers/plans/2026-09-27-ao-shader-port.md` (its Global Constraints apply here unchanged, AO → AA).

## Decisions and evidence

1. **The FXAA and SMAA generators read no engine state.** `fxaashaders <preset> <opts>` and `smaashaders <preset> <opts>` depend only on their arguments (checked: nothing in `fxaa.cfg`/`smaa.cfg` reads a `$var` other than the generator's own globals). So instead of adding sweep points, the proof calls the **old** generators directly for every argument combination and dumps them (`shaderdumpall <run> <sid> 0`) into a reference corpus `aagen`, before any file changes. After the port, the same calls against the new aliases are dumped into `aagen-cand`, and every same-name pair goes through `shadercheck.py --pairs` (tiers 1–2).
   - FXAA: presets 0..3 × options `""`, `"g"` → 8 shaders.
   - SMAA: presets 0..3 × every ordered subset of `a d s g t` (the order `loadsmaashaders` builds, `aa.cpp:204-210`) → 128 option sets × 4 passes = 512 shaders. This includes `a` (`!hasTRG`), unreachable on this GPU, and combinations the engine never builds; both are harmless to prove.
   - Called outside `generateshader`, these shaders are non-standard and have no generator origin. That only matters to the manifest's origin column, which this comparison does not use; the golden-baseline check below covers origin.
2. **`tqaaresolve` reads engine state:** `$msaalight` (via `gfetchdefs`), `$gdepthformat` (via `gdepthunpack`) and `$tqaaresolvegather`. The baseline holds four distinct blobs: s00 (defaults), s01 (MSAA), s31 (`gdepthformat` 1), s32 (`gdepthformat` 3), all with `tqaaresolvegather` 1 (`rendergl.cpp:1081`, `hasGPU5 && hasTG`). The gather-0 branch is unreachable here, and the var is read-only. Reference for it: a scratch cfg (`home/uitest/aagen-ref.cfg`, gitignored) holding a verbatim copy of the old `tqaaresolve` block, renamed `tqaaresolvenogather`, as a plain `shader` with `$tqaaresolvegather` replaced by `0`. The candidate side is `shader_new` on the new files with `TQAA_RESOLVE_GATHER 0`. The three engine inputs select disjoint text, so proving each branch once suffices.
3. **Lazy semantics are unchanged.** The old `lazyshader` baked `$msaalight`/`$gdepthformat`/`$tqaaresolvegather` into the text when `glsl.cfg` ran; `lazyshader_new` reads them when the shader is forced. These can't differ: every `initgbuffer` (which sets them) is immediately followed by `loadshaders` re-executing `glsl.cfg` (`resetshaders`, `resetgl`, startup), and forcing happens after both. The origin stays `defer:tqaaresolve`.
4. **The define contract.**

   | Define | Value | Used by |
   |---|---|---|
   | `MSAA_LIGHT` | `$msaalight` | `tqaa_resolve.frag` |
   | `GDEPTH_FORMAT` | `$gdepthformat` (0 hyperbolic, 1 packed RGB8, >1 linear float) | `tqaa_resolve.frag` |
   | `TQAA_RESOLVE_GATHER` | `$tqaaresolvegather` | `tqaa_resolve.vert`/`.frag` |
   | `FXAA_PRESET` | 0..3 (`fxaaquality`) | `fxaa.frag` |
   | `FXAA_GREENLUMA` | present for option `g` | `fxaa.frag` |
   | `SMAA_PRESET` | 0..3 (`smaaquality`) | `smaa_defs.glsl` |
   | `SMAA_ALPHAAREA` / `SMAA_DISCARD` / `SMAA_SPLIT` / `SMAA_GREENLUMA` / `SMAA_TEMPORAL` | present for option `a` / `d` / `s` / `g` / `t` | SMAA files |

5. **Unchanged loops stay loops.** SMAA's search loops are real GLSL loops in the original; they stay. FXAA's `loopconcat` unrolled search steps become the same `#if (FXAA_QUALITY_PS > n)` blocks written out, so the tokens match.
6. Expected tiers: every AA row `PASS-TEXT` (the edits only move text and replace generator branches with `#if`s, so preprocessed tokens are identical).

## File structure

| File | Responsibility |
|---|---|
| `config/glsl/aa/screenquad.vert` (new) | Screen quad, one texcoord: `fxaa`, SMAA edge detection and neighborhood blending |
| `config/glsl/aa/tqaa_resolve.vert` / `.frag` (new) | TQAA resolve |
| `config/glsl/aa/fxaa.frag` (new) | FXAA 3.11, quality presets inlined |
| `config/glsl/aa/smaa_defs.glsl` (new) | SMAA preset and option macros, a fragment include |
| `config/glsl/aa/smaa_luma_edge_detect.frag`, `smaa_color_edge_detect.frag`, `smaa_blend_weight_calc.vert`/`.frag`, `smaa_neighborhood_blend.frag` (new) | The SMAA passes |
| `config/glsl/aa.cfg` | `tqaaresolvedefines`, `lazyshader_new` `tqaaresolve`, `fxaashaders`, `smaadefines`, `smaashaders` |
| `config/glsl/fxaa.cfg`, `config/glsl/smaa.cfg` | Deleted |
| `config/glsl/shared.cfg` | `lazyshader_new` |
| `doc/shader-reference.md` | `lazyshader_new`; AA in "Porting a generator" |
| `doc/agent-handoff.md` | State and queue (untracked) |

## Tasks

### Task 0: Branch, reference corpus
- [ ] `git switch -c aa-shader-port`; confirm `git status --short config` is clean.
- [ ] `harness.ps1 start`; write `home/uitest/aagen-ref.cfg` (decision 2); `exec` it; call the old `fxaashaders`/`smaashaders` for every combination (decision 1); `shaderdumpall aagen s00 0`. Confirm 8 `fxaa*`, 512 `SMAA*` and `tqaaresolvenogather` rows with hashes, and no compile errors in `log.txt`.

### Task 1: `lazyshader_new` and `tqaaresolve`
- [ ] Add `lazyshader_new` to `shared.cfg`; write `tqaa_resolve.vert`/`.frag`; replace the `lazyshader` block in `aa.cfg`.
- [ ] `resetshaders`; `forceshader tqaaresolve`; check the log is clean.
- [ ] Commit `glsl: move tqaaresolve into config/glsl/aa`.

### Task 2: FXAA
- [ ] Write `screenquad.vert`, `fxaa.frag`; `fxaashaders` in `aa.cfg`; delete `fxaa.cfg`.
- [ ] Commit `glsl: move fxaa into config/glsl/aa`.

### Task 3: SMAA
- [ ] Write `smaa_defs.glsl` and the pass files; `smaadefines`/`smaashaders` in `aa.cfg`; delete `smaa.cfg`.
- [ ] Commit `glsl: move smaa into config/glsl/aa`.

### Task 4: Prove it
- [ ] Candidate dump: restart the harness, same calls plus the candidate `tqaaresolvenogather`, `shaderdumpall aagen-cand s00 0`; pair by name against `aagen`; `shadercheck.py --pairs`. Every pair `TEXT` (or `SPIRV`, which then needs a reason).
- [ ] Golden baseline: `shaders.ps1 check -Sids s00,s01,s24,s25,s26,s27,s28,s31,s32,s99 -NoMaps`. Every AA row `PASS-*`; other rows as before (known model-shader noise only).
- [ ] GLSL 1.20 check: compile the new files through `glslangValidator` at `#version 120` with the compat header, for the reachable presets, as the AO review did.

### Task 5: Docs
- [ ] `doc/shader-reference.md`: `lazyshader_new`, AA files. Commit `doc: describe the aa shader files and lazyshader_new`.
- [ ] Update `doc/agent-handoff.md` §3/§6 and memory (not committed).

## Deviations during execution

- **SMAA ASCII-art banner dropped.** Copied into `smaa_defs.glsl`, its lines ending in `\` are GLSL line continuations even inside `//` comments; glslang warned, the warnings entered the preprocessed output and every SMAA pair fell from `TEXT` to `SPIRV`. With the banner replaced by a plain title, all 522 pairs are `TEXT`. Documented in "Porting a generator".
- **Blend-weight vertex stage has no `smaa_defs` include.** The old generator put `@smaadefs` there, but it is `#define`s only and unused in that stage; tier 1 proves the tokens identical.
- **Mutation run added** (not in the task list): one constant each in SMAA preset 2, FXAA preset 1 and the gather-off resolve path. Exactly the expected 67 rows went `DIFF` (64 preset-2 edge-detection shaders, `fxaa1`, `fxaa1g`, `tqaaresolvenogather`); the other 455 stayed `TEXT`.
- **Contaminated first baseline check.** `shaders.ps1 check` reuses the running client. Shaders created by calling a generator alias directly are non-standard, so they survive `resetshaders`, and `generateshader` then finds them instead of generating. The first check ran on the mutation client and was stopped; it was rerun from a fresh client with the `candidate` corpus removed. Rule: restart the harness after any direct generator calls, before `check`.
