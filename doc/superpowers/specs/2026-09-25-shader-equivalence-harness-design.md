# Shader equivalence harness — design

Date: 2026-09-25
Status: approved for planning

## Goal

Let agents port the shader library from CubeScript-generated GLSL to plain GLSL with
`#ifdef`s, one shader family at a time, and **prove** after each step that nothing
changed. That covers the GLSL the driver receives, the metadata the engine and editor
read, and the pixels the shader produces.

The refactor being supported is *behaviour-preserving*. The harness's job is to show
that **old CubeScript output ≡ new GLSL + injected `#define`s**, for every configuration
a player or mapper can reach.

Related work: the sister project has already started the same refactor.
[libprimis#264](https://github.com/project-imprimis/libprimis/pull/264) added a `shader_new` /
`shader_define` / `shader_include_*` / `shader_source` loader that assembles `#define`s, includes
and `.vert`/`.frag` files before `newshader`.
[imprimis#72](https://github.com/project-imprimis/imprimis/pull/72) ports AA, AO, blur and decals
to it. The harness compares the assembled source, so it covers that loader unchanged. See the
implementation plan's "Related work" section.

## Scope

In scope:

1. A **golden corpus** of the current pipeline's output, recorded once from a pinned
   baseline commit.
2. A **four-tier equivalence check** of the current build against that corpus:
   contract → text → SPIR-V → pixel.
3. A driver (`tools/harness/shaders.ps1`) with `record`, `check` and `diff`, built on the
   existing harness transport.

Out of scope:

- The design of the new `#ifdef` shader format and loader. The harness is agnostic: it
  only sees the composed source handed to the driver.
- Intentional rendering changes and scene-level visual regression. Those are a different
  oracle (tolerances plus a person's judgement), for later.
- Performance comparison. `begintimer` exists (`src/engine/rendergl.cpp:1108`) and can be
  added later.
- Cross-GPU validation beyond what glslang provides offline.

## Migration model

Golden snapshot, per-family cutover:

1. Build the baseline commit in a worktree and run `shaders.ps1 record` → `home/uitest/shadercorpus/`.
2. Port one family (e.g. `world.cfg` bump shaders) to `#ifdef` GLSL and delete its
   CubeScript generator.
3. `shaders.ps1 check -Filter <family>` until it is clean.
4. Repeat.

The corpus stores the old **composed source**, so tier 3 compiles "old" straight from
the corpus. The old generators never have to coexist with the new ones in the binary.

## Background: facts the design rests on

| Fact | Where |
|---|---|
| `dumpshader <name> [col row]` already logs the post-CubeScript, pre-header `vsstr`/`psstr`, but only for an existing shader | `src/engine/shader.cpp:1300` |
| The engine prepends a `glslversion`-dependent header and compat defines in `parts[]` before `glShaderSource` | `compileglslshader`, `src/engine/shader.cpp:150`–`297` |
| Attribute locations are assigned by name | `genattriblocs`, `src/engine/shader.cpp:773` |
| C++ builds shaders by formatting a CubeScript call with option strings and numeric params, some of them loop counts the script unrolls | `generateshader`, `src/engine/shader.cpp:55`; callers in `aa.cpp:139,223`, `renderlights.cpp:119,158`, `grass.cpp:293` |
| Generators read global vars **while generating**, so one name can produce different code under different settings | `$…` reads in `config/glsl/{ao,deferred,material,shared,tonemap,world}.cfg` |
| Generators emit more than GLSL: `defuniformparam` defaults, `SHADER_*` flags, variant rows, reuse links | e.g. `worldvariantshader`, `config/glsl/world.cfg:25` |
| The editor chooses world and decal shaders from the `worldshaders` / `decalshaders` registries, built by `worldshader` / `bumpshader` / `decalshader`; each entry is a `defershader` | `config/glsl/world.cfg:274`–`870`, `config/glsl/decal.cfg:263`; read by `config/tool/tooltex.cfg:210,254,566` |
| `resetshaders` tears down and regenerates everything | `src/engine/shader.cpp` (`resetshaders`) |
| `forceshader` / `useshaderbyname` instantiates a deferred shader | `src/engine/shader.cpp:1092` |

## Configuration identity

A **configuration** is keyed by `(name, variant col, variant row, settings id)` and maps
to a **content hash** over its body source, composed source, metadata and reflection.

The settings id names a *settings vector*: the values of the vars that the generators
read, plus the `generateshader` arguments. Name + variant alone is not a key, because
of the generator-time var reads above.

## Tier ladder

Each configuration is checked by the cheapest tier that proves equivalence. Tier 0 always
runs and always has to pass.

| Tier | Oracle | Runs | Result |
|---|---|---|---|
| 0. Contract | Generator metadata + GL reflection + registry entries | engine dump, compared offline | Must match exactly, or `FAIL` |
| 1. Text | Composed source → `glslangValidator -E` → strip comments and whitespace → token compare | offline (WSL) | `PASS-TEXT` |
| 2. SPIR-V | `glslangValidator -G --auto-map-locations --auto-map-bindings` → `spirv-opt -O` → `spirv-remap --map all --strip all` → byte compare | offline (WSL) | `PASS-SPIRV` |
| 3. Pixel | Render old and new on identical seeded inputs, compare RGBA32F | engine | `PASS-PIXEL` / `WEAK` / `FAIL` |

Short-circuit: identical content hashes are `PASS-TEXT` without running anything.

If glslang rejects a shader, tier 2 is recorded as `n/a` and the check falls through to
tier 3. That is not a failure: legacy constructs in the compat header may not map to GL
SPIR-V.

The report labels **how** each configuration passed, so a reviewer can see which rest
only on pixel evidence, the weakest oracle.

### Tier 0 contract contents

- `SHADER_*` type flags, including `SHADER_INVALID`: a shader that failed to compile on
  the baseline must fail the same way.
- Default slot params: name, four values, flags (`REUSE`), in order.
- Variant layout: rows, the number of columns per row, and reuse links (`reusevs` / `reuseps`).
- Reflection from the linked program:
  - active attributes: name, type, location
  - active uniforms: name, type, array size
  - uniform blocks: name, size, members
  - sampler uniform → texture unit
  - fragdata name → location
- Registry snapshot: every `worldshaders` / `decalshaders` entry, verbatim (name, option
  string, hints). Existing oddities are frozen as-is, e.g. the undocumented `o` in
  `bumpenvspecglowworld "eosrg"`; cleaning them up is a separate, deliberate change.
- `origin` is recorded but **not** compared, because it changes by design when a family
  is ported.

## Components

### 1. Recorder (engine, `DEBUG_UTILS`)

Two small hooks, no separate log:

- **`Shader::origin`**, set when the shader is created. It holds the innermost enclosing
  context: the formatted `generateshader` command (`ambientobscuranceshader "lp" 12`),
  else the `defershader` name, else the exec'd file. It exists so agents can map a
  failing configuration back to the code that made it.
- **Composed source capture** in `compileglslshader`: keep the joined `parts[]` for the
  vertex and fragment stages on the `Shader`, alongside the existing `vsstr` / `psstr`.

### 2. Dumper: `shaderdumpall <dir> <settingsid>` (engine, `DEBUG_UTILS`)

1. Force-instantiate every `SHADER_DEFERRED` shader in the `shaders` table. This covers
   the full `worldshaders` / `decalshaders` editor palette whether or not a map uses it.
2. Instantiate every variant of every shader (`getvariant` over every row and column).
3. For each configuration, write `blobs/<hash>/` if it is not already there, and append a
   manifest line.

Like the other test commands, it is refused under `identflags & IDF_MAP`.

### 3. Bench: `shaderbench <run> <hash> <name> <seeds>` (engine, `DEBUG_UTILS`)

`<run>`/`<hash>` name the baseline blob (`shadercorpus/<run>/blobs/<hash>/`), `<name>` the
live shader, variants included (`<variant:col,row>parent`).

- **Old program:** compiled from `vs.full.glsl` / `fs.full.glsl`, as-is, with no header
  injection. Attribute and fragdata locations are bound to match the new program. Tier 0
  has already established that the two interfaces are identical.
- **New program:** the live shader.
- **Inputs:** derived from the reflected interface, seeded by `(name, seed)`.
  - Scalar and vector uniforms get values in a safe range: positive, bounded, with no
    exact zeros where a division is plausible.
  - `mat4` uniforms get one fixed transform that frames the test mesh.
  - Samplers get seeded noise textures of the matching target (2D, rect, cube, 2D array,
    shadow). Depth-like samplers get values in a plausible depth range.
  - Time uniforms are seeded like any other, so no input is nondeterministic.
- **Geometry:** a tessellated grid carrying every standard attribute (`vvertex`,
  `vnormal`, `vtangent`, `vtexcoord0/1`, `vcolor`, `vboneweight`, `vboneindex`).
- **Output:** 256×256, four RGBA32F attachments, read back. Only the attachments whose
  location a `fragdata(n)` output declares are draw buffers (all four if none is declared);
  the rest stay at the clear sentinel in both renders, since the GL spec leaves an
  attachment no output writes undefined.
- **Compare:** a pixel matches if both values are NaN, or the relative error is ≤ 1e-5
  (the absolute error for values near zero). The default is 4 seeds.
- **Coverage:** the fraction of pixels written, i.e. not left at the clear sentinel. If
  it falls below 50% on every seed, the result is `WEAK`, not a pass.
- **Result line:** `SHADERBENCH <name> <PASS|FAIL|WEAK> maxerr=<e> cov=<pct> seeds=<n>[ reason=<why>]`.
  There is no col/row: the variant is part of `<name>`.
- **Unsupported inputs:** a uniform of a type the bench cannot seed is left unset, so the
  two programs may read different values from it; `shaders.ps1` remaps a `FAIL` carrying
  `reason=unsupported uniform` to `WEAK`. An unsupported sampler or attribute is bound on
  neither side, so both programs see the same inputs, and a `FAIL` there stays `FAIL`.

### 4. Offline checker (WSL script, `tools/harness/shadercheck.py`)

- Inputs: pairs of baseline/candidate blob directories, from `shaders.ps1` (tier 0 runs in
  `shadercorpus.ps1`, before it).
- Runs tiers 1–2 for every pair, in parallel.
- Reports a tier per pair; the rest go on to tier 3.
- Needs `glslang-tools` and `spirv-tools` from apt in the `Ubuntu` instance, the one the
  build already uses. Without `glslangValidator` (or when `-E` fails on either side, for
  both sides) tier 1 compares the comment-stripped raw source line by line, directive lines
  keeping their spacing, never as one flattened token stream: `#define M(x) (x)` and
  `#define M (x) (x)` tokenize identically.

### 5. Driver: `tools/harness/shaders.ps1`

- **`record [-Out dir]`:** runs the sweep against the running build and writes a corpus.
  The run:
  1. Boots and loads the representative map (see Open decisions).
  2. For each settings vector in `tools/harness/shader-sweep.txt`: applies the vars,
     runs `resetshaders`, renders a few frames (so the C++ setup paths call their
     `generateshader`s), then runs `shaderdumpall`.
  3. Loads each shipped map at default settings and dumps only the configurations that
     are a `mapshader` or were made by a `generateshader` command (`Shader::generated`:
     models, deferred lights, grass, AO and the rest, whose options C++ formats from the
     map's content).
  4. Runs the **self-check**: the sweep records defaults twice, `s00` first and `s99`
     last. If a shader valid at `s00` is gone at `s99` or has a different hash there,
     state leaked between sweep points — the sweep is missing a var the generator reads.
     `record` fails and names the shader and origin. (A shader valid only at `s99` is not
     a leak: `resetshaders`/`resetgl` recompile existing variants but never free ones an
     earlier sweep point created, so a var that unlocks new variant rows, e.g. `msaa`,
     legitimately leaves `s99` with rows `s00` doesn't have.) Every sweep point resets
     every var of the whole sweep file, also when recording a `-Sids` subset.
  5. **Some settings vars need a GL reset, not just `resetshaders`.** Vars carrying
     `initwarning(..., INIT_LOAD, CHANGE_SHADERS)` — `msaa*`, `gdepthstencil`,
     `gstencil`, `glineardepth`, `hdrgamma`, `textsupersample`, `gscalecubicsoft` — only
     queue a "Pending shader change" message on `resetshaders`; the g-buffer/deferred-light
     setup stays on the old value until a `resetgl` actually runs. `record` watches each
     sweep point's output for that message and issues `resetgl` when it appears, which
     requires `applydialog 1` (pinned for the whole run) since that gate is what makes the
     engine print the message at all.
  6. **Determinism depends on a `slotparamsscope` guard** (`src/engine/shader.cpp`) at
     every shader-definition entry point (`loadshaders`, `setupshaders`, `generateshader`,
     `Shader::force`). Without it, the global `slotparams` (leftover texture-slot params)
     leaked into shaders defined later, as stray `uniform vec4` declarations and default
     params — which would otherwise show up as spurious `s00`/`s99` leaks or candidate/
     baseline drift unrelated to any real change.
- **`check [-Filter glob] [-MaxTier n]`:** records a candidate corpus from the current
  build (same sweep, into a scratch dir), runs the offline checker, then runs
  `shaderbench` for the remaining keys. Prints one line per configuration, then a summary:
  ```
  PASS-TEXT   bumpenvworld         0 0  s03
  PASS-SPIRV  ambientobscurance    - -  s11
  PASS-PIXEL  bilateralx           - -  s11  seeds=4 cov=98%
  WEAK        alphaworld           1 0  s00  cov=31%
  FAIL        bumpworld            0 0  s00  contract: default specscale 1 1 1 -> missing
  MISSING     triplanarbumpworld   2 0  s05
  EXTRA       newthing             - -  s00
  == 2104 configs: 1980 text, 97 spirv, 22 pixel, 3 weak, 1 fail, 1 missing
  ```
  It exits non-zero on any `FAIL` or `MISSING`. `WEAK` and `EXTRA` are reported for a
  person to judge.
- **`diff <name> [col row sid]`:** prints the tier-0 contract diff, then a unified diff
  of the preprocessed, normalized composed source (baseline vs candidate).

## Corpus layout

`home/uitest/shadercorpus/` (gitignored). Every file is line-oriented so agents can grep it.

```
run.txt              commit, glsldirty, recorded timestamp, sweep map, sweep order, map list
gl.txt               GL vendor/renderer/version, glslversion
settings/<id>.txt    var=value, one per line
manifest.tsv         name  settingsid  hash  origin
registry.txt         worldshaders / decalshaders entries, verbatim
blobs/<hash>/
  vs.glsl fs.glsl             body (pre-header)
  vs.full.glsl fs.full.glsl   composed, as handed to the driver
  meta.txt                    flags, default params, variant layout, reuse links
  reflect.txt                 attributes, uniforms, blocks, samplers, fragdata
```

`run.txt` and `gl.txt` replace the single `baseline.txt` originally proposed: `run.txt`
holds what `check` needs to replay the exact recording (which settings ids and maps, and
whether `config/glsl` was dirty when it was recorded), while `gl.txt` holds only the
driver identity that gates which tiers a cross-machine comparison can run.

The manifest has no separate `col`/`row` columns: a variant is a table entry named
`<variant:col,row>parent` (`src/engine/shader.cpp:911,1168`), so the name already carries
them. A row's `hash` is `-` for a shader that is invalid at that settings id.
**Invalid rows are treated as absent, not compared.** `resetshaders` leaves shaders
generated at earlier sweep points as `SHADER_INVALID` stubs (`Shader::cleanup`,
`shader.cpp:753`) that can't be told apart from a genuine compile failure introduced by
the candidate, so a baseline-valid configuration that is invalid in the candidate reports
`MISSING` (and the reverse `EXTRA`) rather than `FAIL`. "Invalid stays invalid" is
therefore visible in the report but not enforced as a contract failure.

Blobs are deduplicated by hash, so the palette × settings product stays small on disk.

Reflection (`reflect.txt`) and pixels depend on the driver; `meta.txt` and the registry do
not. `check` refuses a baseline whose `gl.txt` (GL vendor, renderer, version, GLSL version)
differs from the running build's, before recording anything, and names both GL strings and
the baseline's commit (`run.txt`) to re-record from on this machine. With `-AllowCrossGpu`
it runs anyway with `-SkipReflection` (tier 0 compares `meta.txt` and the registry but not
`reflect.txt`) and at most tier 2, and every result's detail carries
`(cross-gpu: reflection and pixels skipped)`.

`check` also compares the two `registry.txt` files byte for byte, line by line and in order,
whatever `-Filter`/`-Sids` select, and reports a difference as one `FAIL` named `registry`.
`-Filter` matches a variant by its parent's name as well (`bump*` includes
`<variant:0,1>bumpworld`). `check -Run candidate` is refused: that is the scratch corpus.

## Agent loop

```
port family → harness reload <cfg> → shaders.ps1 check -Filter <family>
            → on FAIL: shaders.ps1 diff <name> … → fix → repeat
```

`.cfg` / `.glsl` edits need no rebuild. Only the engine hooks above need one, once.

## Acceptance tests (for the harness itself)

1. **Determinism:** `record` on the baseline, then `check` on the same unchanged build →
   100% `PASS-TEXT` (hash-identical), zero `MISSING`/`EXTRA`. Run it twice.
2. **Mutation, contract:** remove one `defuniformparam` from a world shader → `FAIL`
   with a contract message naming the param.
3. **Mutation, cosmetic:** reorder two declarations and rename a local → `PASS-SPIRV`
   (or `PASS-TEXT` if tier 1's normalization absorbs it). Never `FAIL`.
4. **Mutation, semantic:** change a numeric constant in one shader's math → `FAIL` at
   tier 3, with nonzero `maxerr`.
5. **Loop rewrite:** moved to the first porting plan, since it *is* the first port
   (bilateral or AO taps as a `#define`d loop) — there is no unrolled-generator rewrite to
   stage as a standalone acceptance test here. In its place, the harness self-test proves
   tiers 2 and 3 with a local rename (→ `PASS-SPIRV`/`PASS-PIXEL`) and a changed constant
   (→ `FAIL` at tier 3, nonzero `maxerr`) on a real shader (`hud`).
6. **Sweep self-check:** remove a var that a generator reads from `shader-sweep.txt` →
   `record` stops and names the shader.
7. **Palette coverage:** every `worldshaders` / `decalshaders` entry appears in the
   manifest at every settings id.

## Open decisions for the plan

- The exact var list and value grid for `shader-sweep.txt`. It is derived by grepping
  `$…` reads in `config/glsl/*.cfg` and the `generateshader` call sites; the self-check
  catches omissions.
- Which map is the representative sweep map. It must exercise water, lava, volfog,
  grass and decals so that the `useshaderbyname` paths in `material.cpp` / `grass.cpp`
  run.
- Whether tier 3 also compares vertex-stage outputs via transform feedback. Proposed:
  no (YAGNI). VS differences surface through rasterized FS output.
