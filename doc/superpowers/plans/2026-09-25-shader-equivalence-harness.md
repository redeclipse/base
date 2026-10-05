# Shader Equivalence Harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let an agent prove, one shader family at a time, that porting CubeScript-generated GLSL to plain `#ifdef` GLSL changed nothing: the source the driver receives, the metadata the engine and editor read, and the pixels the shader produces.

**Architecture:** The engine gains `DEBUG_UTILS` commands:
- `shaderdumpall` writes a content-addressed corpus of every shader configuration (composed source, metadata, GL reflection).
- `shaderbench` renders a corpus copy and the live shader on identical seeded inputs and compares the output.
- `shaderorigin` and `shaderforceall` support them.

A PowerShell driver (`tools/harness/shaders.ps1`), built on the existing harness transport, sweeps render settings to record a baseline and a candidate corpus. It then climbs the tier ladder: contract (PowerShell) → text and SPIR-V (a Python checker under WSL, using glslang) → pixel (`shaderbench`).

**Tech Stack:** C++ (Tesseract-derived engine, MinGW cross-build from WSL), OpenGL 3.x reflection, CubeScript, Windows PowerShell 5.1, Python 3.11 + glslang-tools + spirv-tools in the `Ubuntu` WSL instance.

**Spec:** [doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md](../specs/2026-09-25-shader-equivalence-harness-design.md)

## Global Constraints

- **Every new engine command** is wrapped in `#ifdef DEBUG_UTILS` and returns early when `identflags&IDF_MAP`, matching `src/engine/world.cpp:931`. A downloaded map must never drive these commands.
- **Build:** `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`.
  - Stay on `debug` for the whole plan. Switching build type triggers a full `make clean`, and only `debug` compiles `src/tests/*.o` (`src/Makefile:350`).
  - `DEBUG_UTILS` is defined for both build types.
- **Runnable binary** is `bin/amd64/redeclipse.exe`. Never launch `src/redeclipse_windows_amd64.exe`.
- **CubeScript:** no bare `#`, avoid `@` (use `concat`/`concatword`), write `exec "path" 0 0`. See "CubeScript traps" in `CLAUDE.md`.
- **PowerShell:** Windows PowerShell 5.1.
  - No `&&`/`||`, no ternary, no `??`.
  - Write files the game reads with `Write-TextNoBom`.
  - Shader names can differ only by case, so compare them with `-ceq`/`-cne` and key maps with `[StringComparer]::Ordinal`. PowerShell's `@{}` and `-eq` are case-insensitive.
- **Corpus location:** the engine writes relative to its home dir. The harness home is `home/uitest`, so corpora live in `home/uitest/shadercorpus/<run>/` (gitignored with the rest of `home/`). The spec wrote `home/shadercorpus/`; the location in this plan supersedes it.
- **Corpus text is line-oriented, LF-terminated** (engine files are opened `"wb"`).
- **Do not commit or push unless the user asks.** The commit steps below are written for a normal workflow. If the user has not asked for commits, complete the step's file changes and skip the commit itself.
- **Commit message style:** lowercase `area: summary` (e.g. `shader: record where each shader was generated`).
- **Never minimize the game window** (screenshots come back black). Unfocused is fine.

## Deviations from the spec (decided while planning)

The spec should be updated to match these before execution starts (see Task 9, step 6).

1. **Manifest columns** are `name  settingsid  hash  origin`. There are no separate col/row columns: variants are table entries named `<variant:col,row>parent` (`src/engine/shader.cpp:911,1168`), so the name already carries them.
2. **Invalid shaders are recorded with hash `-` and treated as absent.** `resetshaders` leaves shaders generated at earlier sweep points as `SHADER_INVALID` stubs (`Shader::cleanup`, `shader.cpp:753`). These stubs can't be told apart from genuine compile failures, and they would make every subset run disagree with the baseline. Consequences:
   - A baseline-valid configuration that is invalid in the candidate reports `MISSING`.
   - The reverse reports `EXTRA`.
   - "Invalid stays invalid" is therefore reported, but not enforced as a contract failure.
3. **The sweep self-check** records defaults twice (`s00` first, `s99` last) and fails `record` if any shader's hash differs between them. That catches state leaking between sweep points, e.g. a var the reset list misses. It cannot detect a generator var the sweep *never varies*. That is a coverage question, answered by the var list in `shader-sweep.txt`, which is derived from the `$…` reads in `config/glsl/*.cfg` and the `generateshader` call sites.
4. **Each run has a `run.txt`** (commit, sweep order, maps) and a `gl.txt` (GL vendor, renderer, version, glslversion) in place of `baseline.txt`. The candidate run has them too.
5. **Acceptance test 5 (loop rewrite) moves to the first porting plan.** It *is* the first port: bilateral or AO taps as a `#define`d loop. Here, tiers 2 and 3 are proven with a local rename and a changed constant instead (Task 9).

## Related work: the Imprimis port

The same refactor is already underway in the sister project. It is the likely source of Red Eclipse's first ports:

- **[project-imprimis/libprimis#264](https://github.com/project-imprimis/libprimis/pull/264)** ("Groundwork for shader refactor", merged) is the engine side. `src/engine/render/shader.cpp` gains:
  - the CubeScript commands `shader_new`, `variantshader_new`, `shader_define`, `shader_include_vs`, `shader_include_fs`, `shader_source` and `shader_get_{defines,includes_vs,includes_fs}`;
  - `shader_assemble`, which concatenates the `#define`s, the includes and the `.vert`/`.frag` file text, then hands the result to the existing `shader()` → `newshader` path. `genuniformdefs` and `genfogshader` are rewritten over `std::string`.
- **[project-imprimis/imprimis#72](https://github.com/project-imprimis/imprimis/pull/72)** ("glsl: untangle cubescript", open, WIP) is the content side.
  - AA (SMAA, FXAA, TQAA), AO, blur and shared gbuffer/gdepth helpers move into `config/glsl/<family>/*.vert|frag`.
  - Decal shaders are marked "needs testing".
  - The `.cfg` generators shrink to `shader_new` blocks, e.g. `shader_define AO_FILTER_TAPS $numtaps` + `shader_source "config/glsl/ao/bilateral.vert" "config/glsl/ao/bilateral.frag"`.

What this means for this plan:

1. **The harness needs no changes for that loader.** `shader_assemble` finishes before `newshader`, so `vsstr`/`psstr`, and therefore `composeglslparts` and the corpus, hold the assembled text. That is exactly what should be compared. Two gaps follow:
   - `origin` still names the generator context (`generateshader` command or `defer:`), not the `.frag` file.
   - `diff` shows the normalized composed source, so include boundaries are not visible in it.
2. **Order of work:** land this plan and record the baseline first. Porting libprimis#264 into Red Eclipse's `shader.cpp` comes after, because it rewrites the same code Task 2 touches (`shader()`, `genuniformdefs`, `genfogshader`, next to `compileglslshader`). Keep Task 2's `composeglslparts` split and `origin` tracking when merging it.
3. **The first porting plan should bring over imprimis#72's AO.** Its `AO_FILTER_TAPS` / `AO_TAPS` defines replace CubeScript-unrolled taps with preprocessor-driven code, which is acceptance test 5 (deviation 5 above). Expect those configurations to resolve at `PASS-SPIRV` or `PASS-PIXEL`, not `PASS-TEXT`, across the `aotaps`/`aobilateral`/`aoreduce` sweep points (`s06`, `s07`).
   - The AA files are largely verbatim moves and should mostly land at `PASS-TEXT`.
   - The decals the PR marks "needs testing" are covered at every sweep point by the `decalshaders` palette check.
4. **Imprimis's files are a starting point, not a drop-in.** Red Eclipse's generators have diverged from Imprimis's (extra options, and shaders such as halo, haze and visor), and the harness is what shows where. If a port makes a generator read a var not already in `shader-sweep.txt`, add a vector for it and re-record the baseline.

## File Structure

| File | Responsibility |
|---|---|
| `src/engine/shaderharness.h` (new) | Declarations: pure helpers (hash, seeded values, GL type names) and the shader-table accessors `shader.cpp` exports |
| `src/engine/shaderharness.cpp` (new) | Pure helpers; `shaderdumpall` (dump + reflection); `shaderbench` (tier 3) |
| `src/tests/shaderharness.cpp` (new) | Boot-time unit test of the pure helpers (debug builds) |
| `src/engine/shader.cpp` | `origin` tracking, `composeglslparts` extracted from `compileglslshader`, `DEBUG_UTILS` accessors, `shaderorigin` / `shaderforceall` commands |
| `src/engine/texture.h` | `Shader::origin` field |
| `src/engine/command.cpp` | `getsourcefile()` accessor |
| `src/shared/glexts.h`, `src/engine/rendergl.cpp` | Five more GL entry points for reflection |
| `src/engine/main.cpp`, `src/Makefile` | Register the unit test and the new objects |
| `tools/harness/shadercorpus.ps1` (new) | Pure PowerShell: sweep/manifest/run parsing, tier-0 contract diff, corpus comparison, self-checks, report formatting |
| `tools/harness/tests/shadercorpus.tests.ps1` (new) | Unit tests for the above; no game |
| `tools/harness/shader-sweep.txt` (new) | The settings vectors |
| `tools/harness/shaders.ps1` (new) | Driver: `record`, `check`, `diff` |
| `tools/harness/shadercheck.py` (new) | Tiers 1–2 offline, run under WSL |
| `tools/harness/tests/test_shadercheck.py` (new) | Python unit tests; the glslang cases skip when it's absent |
| `tools/harness/tests/task{2,3,5,7,8}-*.{cfg,ps1}` (new) | Per-task verification against the running game |
| `tools/harness/shaders-selftest.ps1` (new) | Acceptance tests 1–4, 6, 7 |
| `tools/harness/README.md`, `CLAUDE.md` | Usage docs |

---

### Task 1: Pure helpers and their unit test

**Files:**
- Create: `src/engine/shaderharness.h`
- Create: `src/engine/shaderharness.cpp`
- Create: `src/tests/shaderharness.cpp`
- Modify: `src/engine/main.cpp:1331-1336` (unit test calls)
- Modify: `src/Makefile:325` (engine objs), `src/Makefile:350-354` (test objs), and the end of the file (dependency lines)

**Interfaces:**
- Consumes: nothing.
- Produces (namespace `shaderharness`, all under `#ifdef DEBUG_UTILS`):
  - `ullong fnv1a(const void *data, size_t len, ullong h = FNVBASIS)`, `ullong fnv1astr(const char *str, ullong h = FNVBASIS)`. The string version has its own name on purpose: as an overload, `fnv1a(buf, len)` would resolve to `(const char *, ullong h)` and silently hash with `len` as the basis.
  - `void hexhash(ullong h, char out[17])`: 16 lowercase hex digits.
  - `float benchfloat(const char *name, int index, int seed)` in [0.1, 1.0]; `int benchint(const char *name, int index, int seed)` in [1, 4].
  - `const char *gltypename(GLenum type)`, `int gltypecomponents(GLenum type)` (0 = unsupported), `bool issamplertype(GLenum type)`.

- [ ] **Step 1: Write the failing test**

Create `src/tests/shaderharness.cpp`:

```cpp
#include "engine.h"
#include "shaderharness.h"

// Shader equivalence harness helpers, see tools/harness/shaders.ps1.
void testshaderharness()
{
    using namespace shaderharness;

    // Published FNV-1a 64-bit test vectors.
    ASSERT(fnv1astr("") == 0xcbf29ce484222325ULL);
    ASSERT(fnv1astr("a") == 0xaf63dc4c8601ec8cULL);
    ASSERT(fnv1astr("foobar") == 0x85944171f73967e8ULL);

    // Chaining hashes the concatenation, which is how a blob's files are hashed as one.
    ASSERT(fnv1astr("bar", fnv1astr("foo")) == fnv1astr("foobar"));
    // A (buffer, length) call must hash the bytes, not treat the length as a basis.
    const char *buf = "foobar";
    int len = 6;
    ASSERT(fnv1a(buf, len) == fnv1astr("foobar"));

    char hex[17];
    hexhash(0x85944171f73967e8ULL, hex);
    ASSERT(!strcmp(hex, "85944171f73967e8"));
    hexhash(1, hex);
    ASSERT(!strcmp(hex, "0000000000000001"));

    // Bench inputs are a pure function of (name, index, seed), and stay in range.
    ASSERT(benchfloat("gloss", 0, 1) == benchfloat("gloss", 0, 1));
    ASSERT(benchfloat("gloss", 0, 1) != benchfloat("gloss", 0, 2));
    ASSERT(benchfloat("gloss", 0, 1) != benchfloat("gloss", 1, 1));
    loopi(256)
    {
        float f = benchfloat("x", i, 3);
        ASSERT(f >= 0.1f && f <= 1.0f);
        int n = benchint("x", i, 3);
        ASSERT(n >= 1 && n <= 4);
    }

    ASSERT(!strcmp(gltypename(GL_FLOAT_VEC4), "vec4"));
    ASSERT(!strcmp(gltypename(GL_SAMPLER_2D_RECT), "sampler2DRect"));
    ASSERT(!strcmp(gltypename(0x1234), "0x1234"));
    ASSERT(gltypecomponents(GL_FLOAT_VEC3) == 3);
    ASSERT(gltypecomponents(GL_FLOAT_MAT4) == 16);
    ASSERT(gltypecomponents(GL_INT) == 1);
    ASSERT(gltypecomponents(0x1234) == 0);
    ASSERT(issamplertype(GL_SAMPLER_2D_SHADOW));
    ASSERT(!issamplertype(GL_FLOAT_VEC4));

    conoutf(colourwhite, "testshaderharness: ok");
}
```

Register it in `src/engine/main.cpp`, inside the `#ifdef _DEBUG` block after `testprefab();`:

```cpp
        extern void testshaderharness();
        testshaderharness();
```

Add to `src/Makefile`. After `    engine/shader.o \` (line 325):

```make
    engine/shaderharness.o \
```

After `    CLIENT_OBJS += tests/prefab.o` (line 353):

```make
    CLIENT_OBJS += tests/shaderharness.o
```

Append at the end of `src/Makefile` so that header edits rebuild the users:

```make
engine/shaderharness.o: engine/shaderharness.h engine/engine.h engine/texture.h shared/glexts.h
engine/shader.o: engine/shaderharness.h
tests/shaderharness.o: engine/shaderharness.h
```

Create an empty `src/engine/shaderharness.h` and `src/engine/shaderharness.cpp` (a single comment line each) so that make finds the files.

- [ ] **Step 2: Run the build to verify it fails**

Run: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`
Expected: FAIL. The compile of `tests/shaderharness.cpp` errors with `'shaderharness' has not been declared` (or `fnv1a was not declared`).

- [ ] **Step 3: Write the implementation**

`src/engine/shaderharness.h`:

```cpp
// Shader equivalence harness, see tools/harness/shaders.ps1 and
// doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.
#ifndef SHADERHARNESS_H
#define SHADERHARNESS_H

#ifdef DEBUG_UTILS
namespace shaderharness
{
    static const ullong FNVBASIS = 0xcbf29ce484222325ULL;

    // 64-bit FNV-1a. Pass the previous result as h to hash several buffers as one.
    ullong fnv1a(const void *data, size_t len, ullong h = FNVBASIS);
    // Not an fnv1a overload: fnv1a(buf, len) would bind len to h.
    ullong fnv1astr(const char *str, ullong h = FNVBASIS);

    // 16 lowercase hex digits and a terminator.
    void hexhash(ullong h, char out[17]);

    // Deterministic bench inputs: the same (name, index, seed) gives the same
    // value on every run, and in both programs of a comparison.
    float benchfloat(const char *name, int index, int seed); // in [0.1, 1.0]
    int benchint(const char *name, int index, int seed);     // in [1, 4]

    // GLSL spelling of a reflected type, or "0x<hex>" for one not listed.
    const char *gltypename(GLenum type);
    // Scalars per element (mat4 is 16); 0 for a type the bench cannot feed.
    int gltypecomponents(GLenum type);
    bool issamplertype(GLenum type);
}

// Implemented in shader.cpp, which owns the shader table.
extern Shader *findstagesource(Shader &s, GLenum type);
extern bool composeglslsource(Shader &s, GLenum type, vector<char> &out);
extern void scanfragdatalocs(Shader &s, vector<FragDataLoc> &out);
extern void forceallshaders();
extern void collectshaders(vector<Shader *> &out);
#endif

#endif
```

`src/engine/shaderharness.cpp`:

```cpp
// Shader equivalence harness: corpus dump (shaderdumpall) and pixel bench
// (shaderbench). See tools/harness/shaders.ps1. Test-only, like ui.cpp's
// uidumptree: nothing here runs unless the harness asks for it.
#include "engine.h"
#include "shaderharness.h"

#ifdef DEBUG_UTILS
namespace shaderharness
{
    ullong fnv1a(const void *data, size_t len, ullong h)
    {
        const uchar *p = (const uchar *)data;
        for(size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
        return h;
    }

    ullong fnv1astr(const char *str, ullong h) { return fnv1a(str, strlen(str), h); }

    void hexhash(ullong h, char out[17])
    {
        static const char digits[] = "0123456789abcdef";
        for(int i = 15; i >= 0; i--) { out[i] = digits[h&0xF]; h >>= 4; }
        out[16] = '\0';
    }

    static ullong benchhash(const char *name, int index, int seed)
    {
        ullong h = fnv1astr(name);
        h = fnv1a(&index, sizeof(index), h);
        return fnv1a(&seed, sizeof(seed), h);
    }

    float benchfloat(const char *name, int index, int seed)
    {
        return 0.1f + 0.9f*float((benchhash(name, index, seed)>>40)&0xFFFFFF)/float(0xFFFFFF);
    }

    int benchint(const char *name, int index, int seed)
    {
        return 1 + int((benchhash(name, index, seed)>>32)&3);
    }

    static const struct { GLenum type; const char *name; int components; bool sampler; } gltypes[] =
    {
        { GL_FLOAT, "float", 1, false }, { GL_FLOAT_VEC2, "vec2", 2, false }, { GL_FLOAT_VEC3, "vec3", 3, false }, { GL_FLOAT_VEC4, "vec4", 4, false },
        { GL_INT, "int", 1, false }, { GL_INT_VEC2, "ivec2", 2, false }, { GL_INT_VEC3, "ivec3", 3, false }, { GL_INT_VEC4, "ivec4", 4, false },
        { GL_UNSIGNED_INT, "uint", 1, false }, { GL_UNSIGNED_INT_VEC2, "uvec2", 2, false }, { GL_UNSIGNED_INT_VEC3, "uvec3", 3, false }, { GL_UNSIGNED_INT_VEC4, "uvec4", 4, false },
        { GL_BOOL, "bool", 1, false }, { GL_BOOL_VEC2, "bvec2", 2, false }, { GL_BOOL_VEC3, "bvec3", 3, false }, { GL_BOOL_VEC4, "bvec4", 4, false },
        { GL_FLOAT_MAT2, "mat2", 4, false }, { GL_FLOAT_MAT3, "mat3", 9, false }, { GL_FLOAT_MAT4, "mat4", 16, false },
        { GL_SAMPLER_2D, "sampler2D", 1, true }, { GL_SAMPLER_3D, "sampler3D", 1, true }, { GL_SAMPLER_CUBE, "samplerCube", 1, true },
        { GL_SAMPLER_2D_SHADOW, "sampler2DShadow", 1, true }, { GL_SAMPLER_2D_RECT, "sampler2DRect", 1, true },
        { GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow", 1, true }, { GL_SAMPLER_2D_ARRAY, "sampler2DArray", 1, true },
        { GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow", 1, true }, { GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow", 1, true },
        { GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS", 1, true }, { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray", 1, true },
        { GL_SAMPLER_BUFFER, "samplerBuffer", 1, true },
        { GL_INT_SAMPLER_2D, "isampler2D", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D, "usampler2D", 1, true },
        { GL_INT_SAMPLER_2D_RECT, "isampler2DRect", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D_RECT, "usampler2DRect", 1, true }
    };

    const char *gltypename(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].name;
        static string unknown;
        formatstring(unknown, "0x%X", type);
        return unknown;
    }

    int gltypecomponents(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].components;
        return 0;
    }

    bool issamplertype(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].sampler;
        return false;
    }
}
#endif
```

The declarations after the namespace in the header refer to functions that Task 2 implements. Nothing calls them yet, so the link succeeds.

- [ ] **Step 4: Build and run the test**

Run: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`
Expected: build succeeds.

Run:
```powershell
tools\harness\harness.ps1 start
Select-String -Path home\uitest\harness\boot-log.txt -Pattern 'testshaderharness'
tools\harness\harness.ps1 stop
```
Expected: `testshaderharness: ok`. `harness.ps1 start` sets `RE_CRASHLOG=1`, so a failed `ASSERT` exits the game with a backtrace in the log and `start` throws.

- [ ] **Step 5: Commit**

```bash
git add src/engine/shaderharness.h src/engine/shaderharness.cpp src/tests/shaderharness.cpp src/engine/main.cpp src/Makefile
git commit -m "shader: add equivalence harness helpers"
```

---

### Task 2: Shader origin tracking and shared source composition

`compileglslshader` builds the version header and compat defines inline (`src/engine/shader.cpp:150-300`). This task moves that into `composeglslparts` so the dump in Task 3 can reproduce the exact driver input. It also records on each `Shader` what generated it.

**Files:**
- Modify: `src/engine/texture.h:140,157,161-168` (`Shader::origin`)
- Modify: `src/engine/command.cpp` (after `static const char *sourcefile` at line 237)
- Modify: `src/engine/shader.cpp`: top of file, `generateshader` (:55), `compileglslshader` (:150), `newshader` (:821), `Shader::force` (:1053); new `DEBUG_UTILS` block at the end of the file
- Create: `tools/harness/tests/task2-origin.cfg`

**Interfaces:**
- Consumes: `shaderharness.h` declarations from Task 1.
- Produces:
  - `char *Shader::origin`: never NULL on a shader created by `newshader`. It is one of:
    - the formatted `generateshader` command (e.g. `ambientobscuranceshader "lp" 12`)
    - `defer:<name>` for a shader created while forcing a `defershader`
    - the exec'd file (e.g. `config/glsl/init.cfg`)
    - `-`
  - `Shader *findstagesource(Shader &s, GLenum type)`: the shader whose `vsstr`/`psstr` supplies that stage, following `reusevs`/`reuseps`. NULL if none.
  - `bool composeglslsource(Shader &s, GLenum type, vector<char> &out)`: the NUL-terminated composed source for that stage. Returns false if the stage has no source.
  - `void scanfragdatalocs(Shader &s, vector<FragDataLoc> &out)`: fragment outputs parsed from `fragdata(`/`fragblend(` in the source. The engine only records these itself on pre-330 GLSL.
  - `void forceallshaders()`, `void collectshaders(vector<Shader *> &out)`.
  - Commands: `shaderorigin <name>` → origin string (`""` if unknown); `shaderforceall`.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task2-origin.cfg`:

```cubescript
// Task 2 verification. Run with:
//   tools\harness\harness.ps1 send -File tools\harness\tests\task2-origin.cfg
forceshader stdworld
echo (concatword "T2_STDWORLD=" (shaderorigin stdworld))
echo (concatword "T2_VARIANT=" (shaderorigin "<variant:0,0>stdworld"))
echo (concatword "T2_HUD=" (shaderorigin hud))
echo (concatword "T2_NONE=[" (shaderorigin nosuchshader) "]")
shaderforceall
echo (concatword "T2_FORCED=" (hasshader bumpenvspecmapparallaxpulseglowdispworld))
```

- [ ] **Step 2: Run it to verify it fails**

Before changing anything, record the baseline compile-error count so the refactor can be shown to be neutral:

```powershell
tools\harness\harness.ps1 start
(Select-String -Path home\uitest\harness\boot-log.txt -Pattern 'GLSL ERROR').Count
tools\harness\harness.ps1 send -File tools\harness\tests\task2-origin.cfg
```
Expected: note the error count (call it N). The send prints `Unknown command: shaderorigin` warnings.

- [ ] **Step 3: Implement**

`src/engine/texture.h`, in `struct Shader`: change `char *name, *vsstr, *psstr, *defer;` to

```cpp
    char *name, *vsstr, *psstr, *defer, *origin;
```

and add `origin(NULL)` to the constructor initialiser list after `defer(NULL)`, plus `DELETEA(origin);` in `~Shader()` after `DELETEA(defer);`.

`src/engine/command.cpp`, directly after line 237 (`static const char *sourcefile = NULL, *sourcestr = NULL;`):

```cpp
// The file being exec'd right now, or NULL. Recorded as a shader's origin.
const char *getsourcefile() { return sourcefile; }
```

`src/engine/shader.cpp`:

1. After `#include "engine.h"` add `#include "shaderharness.h"`.
2. After line 15 (`bool loadedshaders = false;`) add:

```cpp
// What is generating shaders right now, recorded as Shader::origin for the
// equivalence harness (tools/harness/shaders.ps1). Innermost wins: a
// generateshader call inside a forced defershader reports the generateshader.
static const char *shaderorigin = NULL;
extern const char *getsourcefile();
```

3. In `generateshader`, replace `execute(cmd, true);` with:

```cpp
        const char *oldorigin = shaderorigin;
        shaderorigin = cmd;
        execute(cmd, true);
        shaderorigin = oldorigin;
```

4. Split `compileglslshader`. Rename the function signature and its first part to:

```cpp
// The parts handed to glShaderSource: the version header and compat defines
// for this GL, then the source. Sets modsource when the source had to be
// rewritten; the caller frees it. The equivalence harness dump composes
// through here too, so the corpus records exactly what the driver received.
static int composeglslparts(Shader &s, GLenum type, const char *def, const char **parts, char *&modsource)
{
    const char *source = def + strspn(def, " \t\r\n");
    int numparts = 0;
```

   Keep everything from `static const struct { int version; ...} glslversions[]` down to and including `parts[numparts++] = modsource ? modsource : source;` unchanged. Then close it with `return numparts; }`. Delete the old `char *modsource = NULL;` and `const char *parts[16];` / `int numparts = 0;` declarations at the top: they are now the parameters and the local above. Then add the new `compileglslshader`:

```cpp
static void compileglslshader(Shader &s, GLenum type, GLuint &obj, const char *def, const char *name, bool msg = true)
{
    const char *parts[16];
    char *modsource = NULL;
    int numparts = composeglslparts(s, type, def, parts, modsource);

    obj = glCreateShader_(type);
    glShaderSource_(obj, numparts, (const GLchar **)parts, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetShaderiv_(obj, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        if(msg) showglslinfo(type, obj, name, parts, numparts);
        glDeleteShader_(obj);
        obj = 0;
    }
    else if(dbgshader > 1 && msg) showglslinfo(type, obj, name, parts, numparts);

    if(modsource) delete[] modsource;
}
```

5. In `newshader`, after `s.mapdef = mapdef;` (line 821):

```cpp
    DELETEA(s.origin);
    const char *origin = shaderorigin;
    if(!origin && variant) origin = variant->origin;
    if(!origin) origin = getsourcefile();
    s.origin = newstring(origin ? origin : "-");
```

6. In `Shader::force`, replace `execute(cmd, true);` with:

```cpp
    defformatstring(origin, "defer:%s", name);
    const char *oldorigin = shaderorigin;
    shaderorigin = origin;
    execute(cmd, true);
    shaderorigin = oldorigin;
```

7. Append at the end of `src/engine/shader.cpp`:

```cpp
#ifdef DEBUG_UTILS
// Shader equivalence harness support, see src/engine/shaderharness.cpp.

Shader *findstagesource(Shader &s, GLenum type)
{
    Shader *src = &s;
    while(src && !(type == GL_VERTEX_SHADER ? src->vsstr : src->psstr))
        src = type == GL_VERTEX_SHADER ? src->reusevs : src->reuseps;
    return src;
}

bool composeglslsource(Shader &s, GLenum type, vector<char> &out)
{
    Shader *src = findstagesource(s, type);
    if(!src) return false;
    const char *parts[16];
    char *modsource = NULL;
    int numparts = composeglslparts(*src, type, type == GL_VERTEX_SHADER ? src->vsstr : src->psstr, parts, modsource);
    loopi(numparts) out.put(parts[i], strlen(parts[i]));
    out.add('\0');
    if(modsource) delete[] modsource;
    return true;
}

void scanfragdatalocs(Shader &s, vector<FragDataLoc> &out)
{
    Shader *src = findstagesource(s, GL_FRAGMENT_SHADER);
    if(!src) return;
    // Pre-330 paths already recorded (and blanked) them at compile time.
    if(src->fragdatalocs.length()) { out = src->fragdatalocs; return; }
    Shader tmp;
    char *ps = newstring(src->psstr);
    findfragdatalocs(tmp, ps, "fragdata(", 0);
    findfragdatalocs(tmp, ps, "fragblend(", 1);
    delete[] ps;
    out = tmp.fragdatalocs;
}

void forceallshaders()
{
    // Forcing runs CubeScript that adds shaders, so never walk the table
    // while doing it. Repeat, because a forced definition can declare more
    // deferred shaders; a failed force leaves the shader invalid, not deferred.
    loopk(16)
    {
        vector<char *> names;
        enumerate(shaders, Shader, s, if(s.deferred() && s.defer) names.add(newstring(s.name)));
        if(names.empty()) return;
        loopv(names) useshaderbyname(names[i]);
        names.deletearrays();
    }
}

void collectshaders(vector<Shader *> &out)
{
    enumerate(shaders, Shader, s, out.add(&s));
}

ICOMMAND(0, shaderorigin, "s", (char *name),
{
    if(identflags&IDF_MAP) return;
    Shader *s = shaders.access(name);
    result(s && s->origin ? s->origin : "");
});

ICOMMAND(0, shaderforceall, "", (),
{
    if(identflags&IDF_MAP) return;
    forceallshaders();
});
#endif
```

- [ ] **Step 4: Build and run the test**

Run: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`

```powershell
tools\harness\harness.ps1 stop
tools\harness\harness.ps1 start
(Select-String -Path home\uitest\harness\boot-log.txt -Pattern 'GLSL ERROR').Count
tools\harness\harness.ps1 send -File tools\harness\tests\task2-origin.cfg
tools\harness\harness.ps1 shot task2
```
Expected:
- The error count equals N, so the refactor is neutral.
- The send prints:
  - `T2_STDWORLD=defer:stdworld`
  - `T2_VARIANT=defer:stdworld`
  - `T2_HUD=config/glsl/init.cfg`
  - `T2_NONE=[]`
  - `T2_FORCED=1`
- The screenshot shows the normal main menu. Read the PNG.

- [ ] **Step 5: Commit**

```bash
git add src/engine/texture.h src/engine/command.cpp src/engine/shader.cpp tools/harness/tests/task2-origin.cfg
git commit -m "shader: record where each shader was generated"
```

---

### Task 3: GL reflection entry points and `shaderdumpall`

**Files:**
- Modify: `src/shared/glexts.h` (next to `glGetActiveUniform_` at :396, `glGetActiveUniformBlockiv_` at :489, `glBindFragDataLocation_` at :574)
- Modify: `src/engine/rendergl.cpp` (definitions next to :134, :171, :206; loads next to :477, :576, :639, :778)
- Modify: `src/engine/shaderharness.cpp` (append the dump)
- Create: `tools/harness/tests/task3-dump.ps1`

**Interfaces:**
- Consumes: Task 2's accessors; Task 1's helpers.
- Produces:
  - Command `shaderdumpall <run> <settingsid> <mapsonly>`. It prints `SHADERDUMP <run> <sid> <rows> <newblobs>` and returns the row count (-1 on bad arguments).
    - It appends one `name<TAB>sid<TAB>hash<TAB>origin` row per shader to `shadercorpus/<run>/manifest.tsv`, with hash `-` for an invalid shader.
    - It writes `blobs/<hash>/{vs.glsl,fs.glsl,vs.full.glsl,fs.full.glsl,meta.txt,reflect.txt}` once per distinct hash, and rewrites `gl.txt`.
    - `mapsonly 1` limits the dump to `mapdef` shaders and those whose origin starts with `grassshader`.
  - `meta.txt` format (fixed order, then sorted sections):
    ```
    type <int>
    mapdef <0|1>
    variantof <name|->
    reusevs <name|->
    reuseps <name|->
    variants <row> <count>              (one per non-empty row)
    param <name> <x> <y> <z> <w> <flags> <palette> <palindex>   (declaration order: setslotparams indexes by it)
    attribloc <name> <loc>              (sorted)
    uniformloc <name> <block|-> <binding> <stride>   (sorted)
    ```
  - `reflect.txt` format (all lines sorted):
    ```
    attrib <name> <type> <size> <location>
    uniform <name> <type> <size>
    sampler <name> <type> <unit>
    block <name> <datasize> <binding>
    blockuniform <block> <name> <type> <size> <offset>
    fragdata <name> <type> <location> <index>
    ```
    Uniform locations are deliberately left out: the driver assigns them, and reordering declarations can change them legitimately.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task3-dump.ps1`:

```powershell
# Task 3 verification: shaderdumpall writes a complete, deterministic corpus.
# Needs a running harness (tools\harness\harness.ps1 start); no map required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

$root = Join-Path $HomeDir 'shadercorpus'
foreach ($r in 't3a', 't3b') { Remove-Item -Recurse -Force (Join-Path $root $r) -ErrorAction SilentlyContinue }

$out = @(Invoke-Batch 'shaderdumpall t3a s00 0' 1 300) + @(Invoke-Batch 'shaderdumpall t3b s00 0' 1 300)
Assert-That 'both dumps reported' (@($out | Where-Object { $_ -match 'SHADERDUMP t3[ab] s00 \d+ \d+' }).Count -eq 2)
$bad = @(Invoke-Batch 'echo (concatword "T3_BAD=" (shaderdumpall "../x" s00 0))' 1 60)
Assert-That 'a path-like run name is refused' (@($bad | Where-Object { $_ -match 'T3_BAD=-1' }).Count -eq 1)

$a = [System.IO.File]::ReadAllLines((Join-Path $root 't3a\manifest.tsv'))
$b = [System.IO.File]::ReadAllLines((Join-Path $root 't3b\manifest.tsv'))
Assert-That 'manifest has hundreds of rows' ($a.Count -gt 300)
Assert-That 'every row has four tab-separated fields' (@($a | Where-Object { ($_ -split "`t").Count -ne 4 }).Count -eq 0)
Assert-That 'the same state gives the same manifest' (($a -join "`n") -ceq ($b -join "`n"))

$std = @($a | Where-Object { $_.StartsWith("stdworld`t") })
Assert-That 'stdworld has exactly one row' ($std.Count -eq 1)
$f = $std[0] -split "`t"
Assert-That 'stdworld origin is its defershader' ($f[3] -ceq 'defer:stdworld')
Assert-That 'stdworld hash is 16 hex digits' ($f[2] -match '^[0-9a-f]{16}$')
Assert-That 'the stdworld variant is dumped' (@($a | Where-Object { $_.StartsWith("<variant:0,0>stdworld`t") }).Count -eq 1)
$hudRow = @($a | Where-Object { $_.StartsWith("hud`t") })
Assert-That 'the hud shader came from init.cfg' ($hudRow.Count -eq 1 -and ($hudRow[0] -split "`t")[3] -ceq 'config/glsl/init.cfg')

$blob = Join-Path $root "t3a\blobs\$($f[2])"
foreach ($n in 'vs.glsl', 'fs.glsl', 'vs.full.glsl', 'fs.full.glsl', 'meta.txt', 'reflect.txt') {
    Assert-That "blob has $n" (Test-Path (Join-Path $blob $n))
}
$meta = [System.IO.File]::ReadAllLines((Join-Path $blob 'meta.txt'))
Assert-That 'meta starts with the type' ($meta[0] -match '^type \d+$')
Assert-That 'stdworld lists the gloss default' (@($meta | Where-Object { $_ -like 'param gloss *' }).Count -eq 1)
Assert-That 'stdworld has variant rows' (@($meta | Where-Object { $_ -like 'variants *' }).Count -ge 1)
$reflect = [System.IO.File]::ReadAllLines((Join-Path $blob 'reflect.txt'))
Assert-That 'reflection lists vvertex' (@($reflect | Where-Object { $_ -like 'attrib vvertex *' }).Count -eq 1)
Assert-That 'reflection lists the diffusemap sampler with its unit' (@($reflect | Where-Object { $_ -match '^sampler diffusemap sampler2D \d+$' }).Count -eq 1)
Assert-That 'reflection lists a fragment output' (@($reflect | Where-Object { $_ -like 'fragdata *' }).Count -ge 1)
$ordinal = [string[]]@($reflect)
[Array]::Sort($ordinal, [StringComparer]::Ordinal)   # strcmp order; Sort-Object is culture-aware
Assert-That 'reflection is sorted' (($reflect -join "`n") -ceq ($ordinal -join "`n"))
Assert-That 'composed source starts with the version header' ([System.IO.File]::ReadAllText((Join-Path $blob 'fs.full.glsl')) -match '^#version \d+')
Assert-That 'composed source ends with the body' ([System.IO.File]::ReadAllText((Join-Path $blob 'fs.full.glsl')).EndsWith([System.IO.File]::ReadAllText((Join-Path $blob 'fs.glsl')).TrimStart()))
Assert-That 'gl.txt names the renderer' (@([System.IO.File]::ReadAllLines((Join-Path $root 't3a\gl.txt')) | Where-Object { $_ -like 'renderer *' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 3: all checks passed' -ForegroundColor Green
```

A note on `composed source ends with the body`: `composeglslparts` strips the body's leading whitespace (`def + strspn(...)`), hence the `TrimStart()`. On a GL below 150 the body can be rewritten to insert a precision line; if this machine's `glslversion` (in `gl.txt`) is below 150, drop that one assertion.

- [ ] **Step 2: Run it to verify it fails**

Run (harness started): `powershell -NoProfile -File tools\harness\tests\task3-dump.ps1`
Expected: FAIL. `both dumps reported` fails, and the script throws reading `t3a\manifest.tsv`, because `shaderdumpall` is unknown.

- [ ] **Step 3: Add the GL entry points**

`src/shared/glexts.h`, after `extern PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform_;`:

```cpp
extern PFNGLGETACTIVEATTRIBPROC          glGetActiveAttrib_;
extern PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation_;
extern PFNGLGETUNIFORMIVPROC             glGetUniformiv_;
```

After `extern PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv_;`:

```cpp
extern PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName_;
```

After `extern PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation_;`:

```cpp
extern PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation_;
```

`src/engine/rendergl.cpp`: matching definitions next to the existing ones (`= NULL`), then the loads:
- after `glGetActiveUniform_ = ...getprocaddress("glGetActiveUniform");` (:477):

```cpp
    glGetActiveAttrib_ =          (PFNGLGETACTIVEATTRIBPROC)          getprocaddress("glGetActiveAttrib");
    glGetAttribLocation_ =        (PFNGLGETATTRIBLOCATIONPROC)        getprocaddress("glGetAttribLocation");
    glGetUniformiv_ =             (PFNGLGETUNIFORMIVPROC)             getprocaddress("glGetUniformiv");
```

- after `glBindFragDataLocation_ = ...("glBindFragDataLocation");` (:576): `glGetFragDataLocation_ = (PFNGLGETFRAGDATALOCATIONPROC)getprocaddress("glGetFragDataLocation");`
- after the EXT variant at :639: `glGetFragDataLocation_ = (PFNGLGETFRAGDATALOCATIONPROC)getprocaddress("glGetFragDataLocationEXT");`
- after `glGetActiveUniformBlockiv_ = ...` (:778): `glGetActiveUniformBlockName_ = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)getprocaddress("glGetActiveUniformBlockName");`

- [ ] **Step 4: Implement the dump**

Append to `src/engine/shaderharness.cpp`, inside the existing `#ifdef DEBUG_UTILS` and after the namespace:

```cpp
using namespace shaderharness;

// ---------------------------------------------------------------- dump ----

static void addline(vector<char *> &lines, const char *fmt, ...)
{
    defvformatstring(line, fmt, fmt);
    lines.add(newstring(line));
}

static bool linesort(const char *a, const char *b) { return strcmp(a, b) < 0; }

static void putlines(vector<char> &buf, vector<char *> &lines, bool sorted)
{
    if(sorted) lines.sort(linesort);
    loopv(lines) { buf.put(lines[i], strlen(lines[i])); buf.add('\n'); }
    lines.deletearrays();
}

static void writemeta(Shader &s, vector<char> &buf)
{
    vector<char *> lines;
    addline(lines, "type %d", s.type);
    addline(lines, "mapdef %d", s.mapdef ? 1 : 0);
    addline(lines, "variantof %s", s.variantshader ? s.variantshader->name : "-");
    addline(lines, "reusevs %s", s.reusevs ? s.reusevs->name : "-");
    addline(lines, "reuseps %s", s.reuseps ? s.reuseps->name : "-");
    loopi(MAXVARIANTROWS) if(s.numvariants(i)) addline(lines, "variants %d %d", i, s.numvariants(i));
    putlines(buf, lines, false);
    // Declaration order is part of the contract: setslotparams indexes by it.
    loopv(s.defaultparams)
    {
        SlotShaderParamState &p = s.defaultparams[i];
        addline(lines, "param %s %.9g %.9g %.9g %.9g %d %d %d", p.name, p.val[0], p.val[1], p.val[2], p.val[3], p.flags, p.palette, p.palindex);
    }
    putlines(buf, lines, false);
    loopv(s.attriblocs) addline(lines, "attribloc %s %d", s.attriblocs[i].name, s.attriblocs[i].loc);
    loopv(s.uniformlocs)
    {
        UniformLoc &u = s.uniformlocs[i];
        addline(lines, "uniformloc %s %s %d %d", u.name, u.blockname ? u.blockname : "-", u.binding, u.stride);
    }
    putlines(buf, lines, true);
}

static void writereflect(Shader &s, vector<char> &buf)
{
    GLuint p = s.program;
    vector<char *> lines;
    GLchar name[256], bname[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;

    glGetProgramiv_(p, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(p, i, sizeof(name), &len, &size, &type, name);
        addline(lines, "attrib %s %s %d %d", name, gltypename(type), size, glGetAttribLocation_(p, name));
    }

    glGetProgramiv_(p, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(p, i, sizeof(name), &len, &size, &type, name);
        GLint block = -1, offset = -1;
        if(glGetActiveUniformsiv_)
        {
            GLuint idx = i;
            glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_BLOCK_INDEX, &block);
            glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_OFFSET, &offset);
        }
        if(block >= 0 && glGetActiveUniformBlockName_)
        {
            bname[0] = '\0';
            glGetActiveUniformBlockName_(p, block, sizeof(bname), NULL, bname);
            addline(lines, "blockuniform %s %s %s %d %d", bname, name, gltypename(type), size, offset);
        }
        else if(issamplertype(type))
        {
            GLint unit = -1;
            glGetUniformiv_(p, glGetUniformLocation_(p, name), &unit);
            addline(lines, "sampler %s %s %d", name, gltypename(type), unit);
        }
        else addline(lines, "uniform %s %s %d", name, gltypename(type), size);
    }

    if(glGetActiveUniformBlockiv_ && glGetActiveUniformBlockName_)
    {
        glGetProgramiv_(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
        loopi(n)
        {
            bname[0] = '\0';
            glGetActiveUniformBlockName_(p, i, sizeof(bname), NULL, bname);
            GLint datasize = 0, binding = 0;
            glGetActiveUniformBlockiv_(p, i, GL_UNIFORM_BLOCK_DATA_SIZE, &datasize);
            glGetActiveUniformBlockiv_(p, i, GL_UNIFORM_BLOCK_BINDING, &binding);
            addline(lines, "block %s %d %d", bname, datasize, binding);
        }
    }

    vector<FragDataLoc> outs;
    scanfragdatalocs(s, outs);
    loopv(outs) addline(lines, "fragdata %s %s %d %d", outs[i].name, gltypename(outs[i].format), outs[i].loc, outs[i].index);

    putlines(buf, lines, true);
}

// Settings ids and run names become path components.
static bool validfield(const char *s)
{
    if(!*s) return false;
    for(; *s; s++) if(!isalnum(*s) && *s != '_' && *s != '-') return false;
    return true;
}

static void writecorpusfile(const char *run, const char *hash, const char *file, const char *data, int len)
{
    defformatstring(path, "shadercorpus/%s/blobs/%s/%s", run, hash, file);
    stream *f = openrawfile(path, "wb");
    if(!f) { conoutf(colourred, "shaderdumpall: cannot write %s", path); return; }
    f->write(data, len);
    delete f;
}

static void writeglinfo(const char *run)
{
    defformatstring(path, "shadercorpus/%s/gl.txt", run);
    stream *f = openrawfile(path, "wb");
    if(!f) return;
    f->printf("vendor %s\nrenderer %s\nversion %s\nglslversion %d\n",
        (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), glslversion);
    delete f;
}

// Map content decides these, so the per-map pass dumps only them.
static bool mapdependent(Shader &s)
{
    return s.mapdef || (s.origin && !strncmp(s.origin, "grassshader", strlen("grassshader")));
}

// Manifest fields are tab-separated, one row per line.
static void manifestfield(const char *in, char *out, size_t outlen)
{
    size_t i = 0;
    for(; *in && i + 1 < outlen; in++) out[i++] = (*in == '\t' || *in == '\n' || *in == '\r') ? ' ' : *in;
    out[i] = '\0';
}

static bool shadersort(Shader *a, Shader *b) { return strcmp(a->name, b->name) < 0; }

ICOMMAND(0, shaderdumpall, "ssi", (char *run, char *sid, int *mapsonly),
{
    if(identflags&IDF_MAP) return;
    if(!validfield(run) || !validfield(sid))
    {
        conoutf(colourred, "shaderdumpall: run and settings id must be [A-Za-z0-9_-]+");
        intret(-1);
        return;
    }
    forceallshaders();
    vector<Shader *> all;
    collectshaders(all);
    all.sort(shadersort);

    defformatstring(manifestpath, "shadercorpus/%s/manifest.tsv", run);
    stream *manifest = openrawfile(manifestpath, "ab");
    if(!manifest) { conoutf(colourred, "shaderdumpall: cannot open %s", manifestpath); intret(-1); return; }

    int rows = 0, blobs = 0;
    loopv(all)
    {
        Shader &s = *all[i];
        if(*mapsonly && !mapdependent(s)) continue;
        string origin;
        manifestfield(s.origin ? s.origin : "-", origin, sizeof(origin));
        rows++;
        if(!s.loaded() || !s.program)
        {
            manifest->printf("%s\t%s\t-\t%s\n", s.name, sid, origin);
            continue;
        }

        vector<char> vsfull, fsfull, meta, reflect;
        if(!composeglslsource(s, GL_VERTEX_SHADER, vsfull)) vsfull.add('\0');
        if(!composeglslsource(s, GL_FRAGMENT_SHADER, fsfull)) fsfull.add('\0');
        writemeta(s, meta);
        writereflect(s, reflect);

        ullong h = fnv1a(vsfull.getbuf(), vsfull.length());
        h = fnv1a(fsfull.getbuf(), fsfull.length(), h);
        h = fnv1a(meta.getbuf(), meta.length(), h);
        h = fnv1a(reflect.getbuf(), reflect.length(), h);
        char hex[17];
        hexhash(h, hex);

        defformatstring(metapath, "shadercorpus/%s/blobs/%s/meta.txt", run, hex);
        if(!fileexists(findfile(metapath, "r"), "r"))
        {
            Shader *vsrc = findstagesource(s, GL_VERTEX_SHADER), *fsrc = findstagesource(s, GL_FRAGMENT_SHADER);
            const char *vsbody = vsrc ? vsrc->vsstr : "", *fsbody = fsrc ? fsrc->psstr : "";
            writecorpusfile(run, hex, "vs.glsl", vsbody, strlen(vsbody));
            writecorpusfile(run, hex, "fs.glsl", fsbody, strlen(fsbody));
            writecorpusfile(run, hex, "vs.full.glsl", vsfull.getbuf(), vsfull.length()-1);
            writecorpusfile(run, hex, "fs.full.glsl", fsfull.getbuf(), fsfull.length()-1);
            writecorpusfile(run, hex, "reflect.txt", reflect.getbuf(), reflect.length());
            // meta.txt last: its presence marks the blob complete.
            writecorpusfile(run, hex, "meta.txt", meta.getbuf(), meta.length());
            blobs++;
        }
        manifest->printf("%s\t%s\t%s\t%s\n", s.name, sid, hex, origin);
    }
    delete manifest;
    writeglinfo(run);
    conoutf(colourwhite, "SHADERDUMP %s %s %d %d", run, sid, rows, blobs);
    intret(rows);
});
```

- [ ] **Step 5: Build and run the test**

Run: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`, then restart the harness (`stop`, `start`) and run `powershell -NoProfile -File tools\harness\tests\task3-dump.ps1`.
Expected: `task 3: all checks passed`.

If `stdworld lists the gloss default` fails, read `meta.txt` in the blob. `defuniformparam "gloss" 1` must appear as `param gloss 1 1 1 0 0 0 0` (the `F` args default to the previous value). A different line means `writemeta` is reading the wrong field.

- [ ] **Step 6: Commit**

```bash
git add src/shared/glexts.h src/engine/rendergl.cpp src/engine/shaderharness.cpp tools/harness/tests/task3-dump.ps1
git commit -m "shader: dump a content-addressed shader corpus"
```

---

### Task 4: Corpus library (pure PowerShell) and its tests

**Files:**
- Create: `tools/harness/shadercorpus.ps1`
- Create: `tools/harness/tests/shadercorpus.tests.ps1`

**Interfaces:**
- Consumes: the corpus format from Task 3.
- Produces (dot-sourced by `shaders.ps1` and the tests):
  - `Read-Sweep([string]$Path)` → emits `{Id; Settings}` objects to the pipeline (`Settings` is an ordered map of var → value string). Like `Read-Manifest` and `Read-RunPoints`, callers wrap it in `@()`.
  - `Get-SweepVars($Points)` → string[] of var names, first-seen order.
  - `Read-RunInfo([string]$RunDir)` → ordinal hashtable of `run.txt` (`key rest-of-line`).
  - `Read-RunPoints([string]$RunDir)` → `{Id; Settings}` objects in the recorded sweep order.
  - `Read-Manifest([string]$Dir)` → `{Name; Sid; Hash; Origin; Key}` objects (`Key` = `"$Sid<TAB>$Name"`).
  - `Get-ContractDiff([string]$BaseBlob, [string]$CandBlob)` → string[] of `meta: -line`, `reflect: +line`, `meta: order changed`.
  - `Compare-Corpus -BaseDir -CandDir [-Filter] [-Sids] [-SkipContract]` → `{Status; Name; Sid; BaseHash; CandHash; Detail}`. Status is one of `PASS-TEXT` (hash-identical), `FAIL` (contract), `MISSING`, `EXTRA`, `PENDING`.
  - `Find-SweepLeaks($Rows, [string]$First, [string]$Last)` → `{Name; First; Last; Origin}`.
  - `Test-PaletteCoverage($Rows, [string[]]$RegistryLines, [string[]]$Sids)` → `"<name> <sid>"` strings for missing coverage.
  - `ConvertTo-WslPath([string]$Path)` → `/mnt/<drive>/...`.
  - `Format-Result($R)`, `Format-Summary($Results)`, `Get-CheckExitCode($Results)` (1 on any FAIL/MISSING).

- [ ] **Step 1: Write the failing tests**

Create `tools/harness/tests/shadercorpus.tests.ps1`:

```powershell
# Unit tests for tools/harness/shadercorpus.ps1. No game required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Assert-Throws([string]$Name, [scriptblock]$Body) {
    $threw = $false
    try { & $Body } catch { $threw = $true }
    Assert-That $Name $threw
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("shadercorpus-tests-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force $tmp | Out-Null

function New-Corpus([string]$Name, [string[]]$Rows, [hashtable]$Blobs) {
    $dir = Join-Path $tmp $Name
    New-Item -ItemType Directory -Force $dir | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $dir 'manifest.tsv'), (($Rows -join "`n") + "`n"))
    foreach ($h in $Blobs.Keys) {
        $b = Join-Path $dir "blobs\$h"
        New-Item -ItemType Directory -Force $b | Out-Null
        foreach ($f in $Blobs[$h].Keys) { [System.IO.File]::WriteAllText((Join-Path $b $f), $Blobs[$h][$f]) }
    }
    return $dir
}
function Blob([string]$Meta, [string]$Reflect) { return @{ 'meta.txt' = $Meta; 'reflect.txt' = $Reflect } }

try {
    Write-Host 'Read-Sweep'
    $sweepPath = Join-Path $tmp 'sweep.txt'
    [System.IO.File]::WriteAllText($sweepPath, "# comment`ns00`n`ns01 msaa=4   # trailing`ns02 aotaps=12 hdrgamma=0.5`n")
    $sweep = @(Read-Sweep $sweepPath)
    Assert-That 'three points' ($sweep.Count -eq 3)
    Assert-That 'first point has no settings' ($sweep[0].Id -ceq 's00' -and $sweep[0].Settings.Count -eq 0)
    Assert-That 'trailing comment ignored' ($sweep[1].Settings['msaa'] -ceq '4' -and $sweep[1].Settings.Count -eq 1)
    Assert-That 'float value kept as text' ($sweep[2].Settings['hdrgamma'] -ceq '0.5')
    Assert-That 'vars in first-seen order' ((Get-SweepVars $sweep) -join ',' -ceq 'msaa,aotaps,hdrgamma')
    [System.IO.File]::WriteAllText($sweepPath, "s00`ns00`n")
    Assert-Throws 'duplicate id rejected' { Read-Sweep $sweepPath }
    [System.IO.File]::WriteAllText($sweepPath, "s01 msaa`n")
    Assert-Throws 'bare var rejected' { Read-Sweep $sweepPath }
    [System.IO.File]::WriteAllText($sweepPath, "m-atop`n")
    Assert-Throws 'reserved m- id rejected' { Read-Sweep $sweepPath }

    Write-Host 'Read-RunPoints'
    $run = Join-Path $tmp 'run'
    New-Item -ItemType Directory -Force (Join-Path $run 'settings') | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $run 'run.txt'), "commit abc`nsweep s00 s01`nmaps atop`n")
    [System.IO.File]::WriteAllText((Join-Path $run 'settings\s00.txt'), '')
    [System.IO.File]::WriteAllText((Join-Path $run 'settings\s01.txt'), "msaa=4`n")
    $pts = @(Read-RunPoints $run)
    Assert-That 'recorded order kept' (($pts | ForEach-Object { $_.Id }) -join ',' -ceq 's00,s01')
    Assert-That 'recorded settings read' ($pts[1].Settings['msaa'] -ceq '4')
    Assert-That 'run info read' ((Read-RunInfo $run)['maps'] -ceq 'atop')

    Write-Host 'Compare-Corpus'
    $m1 = "type 1`nparam gloss 1 1 1 0 0 0 0`nparam specscale 2 2 2 0 0 0 0`n"
    $m2 = "type 1`nparam specscale 2 2 2 0 0 0 0`nparam gloss 1 1 1 0 0 0 0`n"
    $r1 = "attrib vvertex vec4 1 0`n"
    $base = New-Corpus 'base' @(
        "same`ts00`taaaa`tx", "contract`ts00`tbbbb`tx", "order`ts00`tcccc`tx", "pending`ts00`tdddd`tx",
        "missing`ts00`teeee`tx", "gone`ts00`teeee`tx", "stub`ts00`t-`tx", "Case`ts00`taaaa`tx", "other`ts01`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); bbbb = (Blob $m1 $r1); cccc = (Blob $m1 $r1); dddd = (Blob $m1 $r1); eeee = (Blob $m1 $r1) }
    $cand = New-Corpus 'cand' @(
        "same`ts00`taaaa`ty", "contract`ts00`tffff`tx", "order`ts00`tgggg`tx", "pending`ts00`thhhh`tx",
        "gone`ts00`t-`tx", "extra`ts00`taaaa`tx", "case`ts00`taaaa`tx", "other`ts01`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); ffff = (Blob "type 1`nparam specscale 2 2 2 0 0 0 0`n" $r1); gggg = (Blob $m2 $r1); hhhh = (Blob $m1 $r1) }

    $res = @(Compare-Corpus -BaseDir $base -CandDir $cand)
    function Status([string]$n, [string]$s = 's00') { $x = @($res | Where-Object { $_.Name -ceq $n -and $_.Sid -ceq $s }); if ($x.Count) { $x[0].Status } else { '(none)' } }
    Assert-That 'identical hash passes as text' ((Status 'same') -ceq 'PASS-TEXT')
    Assert-That 'dropped param fails the contract' ((Status 'contract') -ceq 'FAIL')
    Assert-That 'contract detail names the param' (@($res | Where-Object { $_.Name -ceq 'contract' })[0].Detail -like '*meta: -param gloss*')
    Assert-That 'param order change fails' ((Status 'order') -ceq 'FAIL')
    Assert-That 'order detail says so' (@($res | Where-Object { $_.Name -ceq 'order' })[0].Detail -like '*order changed*')
    Assert-That 'same contract, new hash is pending' ((Status 'pending') -ceq 'PENDING')
    Assert-That 'absent in candidate is missing' ((Status 'missing') -ceq 'MISSING')
    Assert-That 'invalid in candidate is missing' ((Status 'gone') -ceq 'MISSING')
    Assert-That 'baseline stub is ignored' ((Status 'stub') -ceq '(none)')
    Assert-That 'candidate only is extra' ((Status 'extra') -ceq 'EXTRA')
    Assert-That 'names are case-sensitive' ((Status 'Case') -ceq 'MISSING' -and (Status 'case') -ceq 'EXTRA')
    Assert-That 'filter narrows by name' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'pend*').Count -eq 1)
    Assert-That 'sids narrow by settings id' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Sids 's01').Count -eq 1)
    Assert-That 'skip contract leaves it pending' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'contract' -SkipContract)[0].Status -ceq 'PENDING')
    Assert-That 'exit code is 1 with a failure' ((Get-CheckExitCode $res) -eq 1)
    Assert-That 'exit code is 0 when clean' ((Get-CheckExitCode @($res | Where-Object { $_.Status -eq 'PASS-TEXT' })) -eq 0)
    Assert-That 'summary counts' ((Format-Summary $res) -like '== * configs: * text, 0 spirv, 0 pixel, 0 weak, 2 fail, * missing, * extra')

    Write-Host 'Find-SweepLeaks'
    $rows = @(Read-Manifest (New-Corpus 'leak' @(
        "a`ts00`t1111`tx", "a`ts99`t1111`tx", "b`ts00`t2222`tx", "b`ts99`t3333`ty", "c`ts99`t-`tx", "d`ts00`t4444`tx") @{}))
    $leaks = @(Find-SweepLeaks $rows 's00' 's99')
    Assert-That 'changed hash is a leak' (@($leaks | Where-Object { $_.Name -ceq 'b' }).Count -eq 1)
    Assert-That 'a stub after the sweep is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'c' }).Count -eq 0)
    Assert-That 'vanishing after the sweep is a leak' (@($leaks | Where-Object { $_.Name -ceq 'd' }).Count -eq 1)
    Assert-That 'identical is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'a' }).Count -eq 0)

    Write-Host 'Test-PaletteCoverage'
    $gaps = @(Test-PaletteCoverage $rows @('world a ', 'world b sS', 'decal d b') @('s00', 's99'))
    Assert-That 'd is missing at s99 only' (($gaps -join ',') -ceq 'd s99')

    Write-Host 'ConvertTo-WslPath'
    Assert-That 'drive path converts' ((ConvertTo-WslPath 'F:\Red Eclipse\home\x.tsv') -ceq '/mnt/f/Red Eclipse/home/x.tsv')
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'shadercorpus: all checks passed' -ForegroundColor Green
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -NoProfile -File tools\harness\tests\shadercorpus.tests.ps1`
Expected: FAIL with `...shadercorpus.ps1 ... does not exist` (the dot-source target is missing).

- [ ] **Step 3: Implement `tools/harness/shadercorpus.ps1`**

```powershell
# Pure helpers for the shader equivalence harness (shaders.ps1): sweep file
# and corpus parsing, the tier-0 contract comparison, self-checks and report
# formatting. Nothing here talks to the game, so
# tests/shadercorpus.tests.ps1 runs without one.
#
# Shader names can differ only by case, and PowerShell's @{} and -eq are
# case-insensitive: key maps with New-OrdinalMap and compare with -ceq.

function New-OrdinalMap { return New-Object System.Collections.Hashtable ([StringComparer]::Ordinal) }

function Read-Sweep([string]$Path) {
    $points = New-Object System.Collections.Generic.List[object]
    $seen = New-OrdinalMap
    $n = 0
    foreach ($raw in [System.IO.File]::ReadAllLines($Path)) {
        $n++
        $line = ($raw -replace '#.*$', '').Trim()
        if (-not $line) { continue }
        $parts = @($line -split '\s+')
        $id = $parts[0]
        if ($id -notmatch '^[A-Za-z0-9_-]+$') { throw "${Path}:${n}: bad settings id '$id'" }
        if ($id.StartsWith('m-')) { throw "${Path}:${n}: ids starting 'm-' are reserved for the per-map pass" }
        if ($seen.ContainsKey($id)) { throw "${Path}:${n}: duplicate settings id '$id'" }
        $seen[$id] = $true
        $settings = [ordered]@{}
        foreach ($p in @($parts | Select-Object -Skip 1)) {
            if ($p -notmatch '^([A-Za-z_][A-Za-z0-9_]*)=(-?[0-9]+(\.[0-9]+)?)$') { throw "${Path}:${n}: expected var=number, got '$p'" }
            $settings[$Matches[1]] = $Matches[2]
        }
        $points.Add([pscustomobject]@{ Id = $id; Settings = $settings })
    }
    return , $points.ToArray()
}

function Get-SweepVars($Points) {
    $vars = New-Object System.Collections.Generic.List[string]
    foreach ($p in $Points) { foreach ($k in $p.Settings.Keys) { if (-not $vars.Contains($k)) { $vars.Add($k) } } }
    return , $vars.ToArray()
}

function Format-Settings($Settings) {
    return (@($Settings.Keys | ForEach-Object { "$_=$($Settings[$_])" }) -join ' ')
}

function Read-RunInfo([string]$RunDir) {
    $path = Join-Path $RunDir 'run.txt'
    if (-not (Test-Path $path)) { throw "No run.txt in $RunDir -- not a recorded corpus?" }
    $info = New-OrdinalMap
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        $i = $line.IndexOf(' ')
        if ($i -gt 0) { $info[$line.Substring(0, $i)] = $line.Substring($i + 1) }
        elseif ($line) { $info[$line] = '' }
    }
    return $info
}

function Read-RunPoints([string]$RunDir) {
    $info = Read-RunInfo $RunDir
    foreach ($id in @($info['sweep'] -split ' ' | Where-Object { $_ })) {
        $settings = [ordered]@{}
        foreach ($line in [System.IO.File]::ReadAllLines((Join-Path $RunDir "settings\$id.txt"))) {
            if ($line -match '^([A-Za-z_][A-Za-z0-9_]*)=(\S+)$') { $settings[$Matches[1]] = $Matches[2] }
        }
        [pscustomobject]@{ Id = $id; Settings = $settings }
    }
}

function Read-Manifest([string]$Dir) {
    $path = Join-Path $Dir 'manifest.tsv'
    if (-not (Test-Path $path)) { throw "No manifest: $path" }
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        if (-not $line) { continue }
        $f = $line -split "`t"
        if ($f.Count -ne 4) { throw "Malformed manifest row in ${path}: $line" }
        [pscustomobject]@{ Name = $f[0]; Sid = $f[1]; Hash = $f[2]; Origin = $f[3]; Key = "$($f[1])`t$($f[0])" }
    }
}

# Invalid rows ('-') are treated as absent: resetshaders leaves stubs of
# shaders generated at earlier sweep points, so they are not comparable.
function Get-ValidRowMap($Rows) {
    $map = New-OrdinalMap
    foreach ($r in $Rows) { if ($r.Hash -cne '-') { $map[$r.Key] = $r } }
    return $map
}

function Get-BlobLines([string]$Path) {
    if (-not (Test-Path $Path)) { return , @() }
    return , @([System.IO.File]::ReadAllLines($Path))
}

function Get-ContractDiff([string]$BaseBlob, [string]$CandBlob) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($file in 'meta.txt', 'reflect.txt') {
        $section = [System.IO.Path]::GetFileNameWithoutExtension($file)
        $a = Get-BlobLines (Join-Path $BaseBlob $file)
        $b = Get-BlobLines (Join-Path $CandBlob $file)
        if (($a -join "`n") -ceq ($b -join "`n")) { continue }
        $setA = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        $setB = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        foreach ($l in $a) { [void]$setA.Add($l) }
        foreach ($l in $b) { [void]$setB.Add($l) }
        $before = $out.Count
        foreach ($l in $a) { if (-not $setB.Contains($l)) { $out.Add("${section}: -$l") } }
        foreach ($l in $b) { if (-not $setA.Contains($l)) { $out.Add("${section}: +$l") } }
        if ($out.Count -eq $before) { $out.Add("${section}: order changed") }
    }
    return , $out.ToArray()
}

function New-Result([string]$Status, $Row, [string]$BaseHash, [string]$CandHash, [string]$Detail) {
    return [pscustomobject]@{ Status = $Status; Name = $Row.Name; Sid = $Row.Sid; BaseHash = $BaseHash; CandHash = $CandHash; Detail = $Detail }
}

function Compare-Corpus {
    param([string]$BaseDir, [string]$CandDir, [string]$Filter = '*', [string[]]$Sids, [switch]$SkipContract)
    $base = Get-ValidRowMap (Read-Manifest $BaseDir)
    $cand = Get-ValidRowMap (Read-Manifest $CandDir)
    $keys = New-Object 'System.Collections.Generic.SortedSet[string]' ([StringComparer]::Ordinal)
    foreach ($k in $base.Keys) { [void]$keys.Add($k) }
    foreach ($k in $cand.Keys) { [void]$keys.Add($k) }
    foreach ($k in $keys) {
        $b = $base[$k]; $c = $cand[$k]
        $row = if ($b) { $b } else { $c }
        if ($Sids -and $Sids -cnotcontains $row.Sid) { continue }
        if ($row.Name -notlike $Filter) { continue }
        if (-not $c) { New-Result 'MISSING' $row $b.Hash '' 'no valid shader in the candidate'; continue }
        if (-not $b) { New-Result 'EXTRA' $row '' $c.Hash 'not in the baseline'; continue }
        if ($b.Hash -ceq $c.Hash) { New-Result 'PASS-TEXT' $row $b.Hash $c.Hash 'identical'; continue }
        if (-not $SkipContract) {
            $diff = Get-ContractDiff (Join-Path $BaseDir "blobs\$($b.Hash)") (Join-Path $CandDir "blobs\$($c.Hash)")
            if ($diff.Count) {
                $shown = @($diff | Select-Object -First 3) -join '; '
                if ($diff.Count -gt 3) { $shown += "; (+$($diff.Count - 3) more)" }
                New-Result 'FAIL' $row $b.Hash $c.Hash "contract: $shown"
                continue
            }
        }
        New-Result 'PENDING' $row $b.Hash $c.Hash ''
    }
}

function Find-SweepLeaks($Rows, [string]$First, [string]$Last) {
    $a = New-OrdinalMap; $b = New-OrdinalMap
    foreach ($r in $Rows) {
        if ($r.Hash -ceq '-') { continue }
        if ($r.Sid -ceq $First) { $a[$r.Name] = $r } elseif ($r.Sid -ceq $Last) { $b[$r.Name] = $r }
    }
    $names = New-Object 'System.Collections.Generic.SortedSet[string]' ([StringComparer]::Ordinal)
    foreach ($k in $a.Keys) { [void]$names.Add($k) }
    foreach ($k in $b.Keys) { [void]$names.Add($k) }
    foreach ($name in $names) {
        $x = $a[$name]; $y = $b[$name]
        $hx = if ($x) { $x.Hash } else { '(none)' }
        $hy = if ($y) { $y.Hash } else { '(none)' }
        if ($hx -cne $hy) {
            $origin = if ($x) { $x.Origin } else { $y.Origin }
            [pscustomobject]@{ Name = $name; First = $hx; Last = $hy; Origin = $origin }
        }
    }
}

function Test-PaletteCoverage($Rows, [string[]]$RegistryLines, [string[]]$Sids) {
    $valid = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($r in $Rows) { if ($r.Hash -cne '-') { [void]$valid.Add("$($r.Sid)`t$($r.Name)") } }
    foreach ($line in $RegistryLines) {
        $f = @($line.Trim() -split '\s+')
        if ($f.Count -lt 2) { continue }
        foreach ($sid in $Sids) { if (-not $valid.Contains("$sid`t$($f[1])")) { "$($f[1]) $sid" } }
    }
}

function ConvertTo-WslPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -notmatch '^([A-Za-z]):\\(.*)$') { throw "Not a drive path: $Path" }
    return '/mnt/' + $Matches[1].ToLower() + '/' + ($Matches[2] -replace '\\', '/')
}

function Format-Result($R) { return ('{0,-11} {1,-44} {2,-8} {3}' -f $R.Status, $R.Name, $R.Sid, $R.Detail).TrimEnd() }

function Format-Summary($Results) {
    $all = @($Results)
    $n = @{}
    foreach ($s in 'PASS-TEXT', 'PASS-SPIRV', 'PASS-PIXEL', 'WEAK', 'FAIL', 'MISSING', 'EXTRA') {
        $n[$s] = @($all | Where-Object { $_.Status -ceq $s }).Count
    }
    return '== {0} configs: {1} text, {2} spirv, {3} pixel, {4} weak, {5} fail, {6} missing, {7} extra' -f `
        $all.Count, $n['PASS-TEXT'], $n['PASS-SPIRV'], $n['PASS-PIXEL'], $n['WEAK'], $n['FAIL'], $n['MISSING'], $n['EXTRA']
}

function Get-CheckExitCode($Results) {
    if (@($Results | Where-Object { $_.Status -ceq 'FAIL' -or $_.Status -ceq 'MISSING' }).Count) { return 1 }
    return 0
}
```

- [ ] **Step 4: Run the tests**

Run: `powershell -NoProfile -File tools\harness\tests\shadercorpus.tests.ps1`
Expected: `shadercorpus: all checks passed`.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/shadercorpus.ps1 tools/harness/tests/shadercorpus.tests.ps1
git commit -m "harness: add shader corpus comparison library"
```

---

### Task 5: Sweep file and `shaders.ps1 record`

**Files:**
- Create: `tools/harness/shader-sweep.txt`
- Create: `tools/harness/shaders.ps1` (record only; `check`/`diff` land in Task 8)
- Create: `tools/harness/tests/task5-record.ps1`

**Interfaces:**
- Consumes:
  - `core.ps1`: `Invoke-Batch`, `Get-HarnessProcess`, `Write-TextNoBom`, `$HomeDir`, `$RepoRoot`, `$ErrorPattern`.
  - `shadercorpus.ps1` (Task 4).
  - `shaderdumpall` (Task 3), `editor.ps1 open` (existing).
- Produces:
  - `tools\harness\shaders.ps1 record [-Run baseline] [-Sids s00,s05] [-Maps atop,deli] [-NoMaps] [-Map atop] [-SweepFile path]`. It writes `home/uitest/shadercorpus/<Run>/` with `manifest.tsv`, `blobs/`, `settings/<sid>.txt`, `registry.txt`, `gl.txt` and `run.txt`.
    - `run.txt` lines: `commit <sha>`, `glsldirty <0|1>`, `recorded <iso>`, `map <map>`, `sweep <ids...>`, `maps <names...>`.
  - Script-level functions reused by Task 8: `Invoke-Checked`, `Get-VarDefaults`, `Set-SweepPoint`, `Open-Map`, `Enter-Point`, `Invoke-Dump`, `Invoke-Record`.
  - `$CorpusRoot` = `home\uitest\shadercorpus`.

- [ ] **Step 1: Write the sweep file**

Create `tools/harness/shader-sweep.txt`:

```
# Shader equivalence sweep: one settings vector per line, "<id> [var=value ...]".
# Each vector is applied on top of the engine defaults of every var named in
# this file, then 'resetshaders' regenerates everything. s00 and s99 are both
# pure defaults: 'record' compares them to catch state leaking between points.
#
# The vars are the user-settable inputs to what config/glsl/*.cfg reads while
# generating ($msaasamples, $msaalight, $gdepthformat, $aodepthformat,
# $tqaaresolvegather, $textsupersample, $hdrgamma, $gscalecubicsoft, ...) and
# to the generateshader call sites (aa.cpp:139,223, grass.cpp:293,
# renderlights.cpp:119,158,1446,2630,2640,2785). When a generator starts
# reading a new var, add a vector for it here.
s00
s01 msaa=4
s02 msaa=8 msaatonemap=1
s03 msaa=4 msaaedgedetect=0 msaalineardepth=0
s04 msaa=2 msaadepthstencil=0
s05 ao=0
s06 aotaps=1 aobilateral=0 aoreduce=0
s07 aotaps=12 aobilateral=10 aoreduce=2
s08 aopackdepth=0 aoreducedepth=0
s09 gi=0
s10 rhtaps=0 rhborder=0 rhcache=0
s11 rhtaps=32 rhsplits=1
s12 csmshadowmap=0
s13 csmsplits=1
s14 smfilter=0
s15 smfilter=1
s16 smfilter=3 smgather=1
s17 smalpha=0
s18 smalpha=1
s19 lighttilebatch=0
s20 batchsunlight=0
s21 volumetric=0
s22 volsteps=1 volbilateral=0 volreduce=0
s23 volsteps=64 volbilateral=3 volreduce=2
s24 fxaa=1 fxaaquality=0
s25 fxaa=1 fxaaquality=3 fxaagreenluma=1
s26 smaa=1 smaaquality=0
s27 smaa=1 smaaquality=3 smaacoloredge=1 smaagreenluma=1
s28 tqaa=1
s29 hdrprec=0
s30 hdrprec=3
s31 glineardepth=1
s32 glineardepth=3
s33 gdepthstencil=0 gstencil=1
s34 gscale=50 gscalecubic=1 gscalecubicsoft=0.5
s35 textsupersample=0
s36 textsupersample=2
s37 hdrgamma=1
s38 waterreflect=0 caustics=0
s99
```

- [ ] **Step 2: Write the failing test**

Create `tools/harness/tests/task5-record.ps1`:

```powershell
# Task 5 verification: record a small corpus and check its shape.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

& (Join-Path $PSScriptRoot '..\shaders.ps1') record -Run t5 -Sids s00, s01, s07, s99 -Maps atop
Assert-That 'record exited cleanly' ($LASTEXITCODE -eq 0)

$dir = Join-Path $HomeDir 'shadercorpus\t5'
$rows = @(Read-Manifest $dir)
$info = Read-RunInfo $dir
Assert-That 'run.txt has the sweep order' ($info['sweep'] -ceq 's00 s01 s07 s99')
Assert-That 'run.txt has the commit' ($info['commit'] -match '^[0-9a-f]{40}$')
Assert-That 's01 settings recorded' ([System.IO.File]::ReadAllText((Join-Path $dir 'settings\s01.txt')).Trim() -ceq 'msaa=4')
foreach ($sid in 's00', 's01', 's07', 's99', 'm-atop') {
    Assert-That "rows for $sid" (@($rows | Where-Object { $_.Sid -ceq $sid }).Count -gt 0)
}
Assert-That 'msaa generated multisample light shaders' (@($rows | Where-Object { $_.Sid -ceq 's01' -and $_.Hash -cne '-' -and $_.Name.StartsWith('deferredlightM') }).Count -gt 0)
Assert-That 'aotaps=12 reached the AO generator' (@($rows | Where-Object { $_.Sid -ceq 's07' -and $_.Hash -cne '-' -and $_.Origin -match '^ambientobscuranceshader .* 12$' }).Count -gt 0)
Assert-That 'map pass dumps only map-dependent shaders' (@($rows | Where-Object { $_.Sid -ceq 'm-atop' -and $_.Name -ceq 'stdworld' }).Count -eq 0)
$registry = [System.IO.File]::ReadAllLines((Join-Path $dir 'registry.txt'))
Assert-That 'registry lists stdworld' ($registry -ccontains 'world stdworld ')
Assert-That 'registry lists stddecal' ($registry -ccontains 'decal stddecal b')
Assert-That 'registry is the whole palette' ($registry.Count -gt 150)
Assert-That 'every registered shader is valid at every point' (@(Test-PaletteCoverage $rows $registry @('s00', 's01', 's07', 's99')).Count -eq 0)
Assert-That 'no state leaked between s00 and s99' (@(Find-SweepLeaks $rows 's00' 's99').Count -eq 0)
$state = @(Invoke-Batch 'echo (concatword "T5_MSAA=" $msaa " T5_TAPS=" $aotaps)' 1 30)
Assert-That 'defaults restored afterwards' (@($state | Where-Object { $_ -match 'T5_MSAA=0 T5_TAPS=5' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 5: all checks passed' -ForegroundColor Green
```

The `registry lists stdworld` check expects `world stdworld ` with a trailing space: `worldshader "stdworld" ""` appends `[stdworld ]`, and `(at $e 1)` is empty. If the file shows `world stdworld` without the space, the CubeScript list parser trimmed it; change the expectation to match what `registry.txt` actually contains. Do not change the writer.

- [ ] **Step 3: Run it to verify it fails**

Run (harness started): `powershell -NoProfile -File tools\harness\tests\task5-record.ps1`
Expected: FAIL. `shaders.ps1` does not exist.

- [ ] **Step 4: Implement `tools/harness/shaders.ps1` (record)**

```powershell
<#
.SYNOPSIS
    Shader equivalence harness: record shader corpora and compare them.

.DESCRIPTION
    record  Sweeps render settings in the running game and writes a corpus
            (every shader configuration: composed source, metadata, GL
            reflection) to home\uitest\shadercorpus\<Run>\.
    check   Records a candidate corpus from the current build and compares it
            with a baseline through the tier ladder: contract -> text ->
            SPIR-V -> pixel. Exits 1 on any FAIL or MISSING.
    diff    Shows why one configuration differs.

    See doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.

.EXAMPLE
    tools\harness\shaders.ps1 record
    tools\harness\shaders.ps1 check -Filter 'bump*'
    tools\harness\shaders.ps1 check -Sids s00 -NoMaps
    tools\harness\shaders.ps1 diff bumpworld -Sid s00
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet('record', 'check', 'diff')]
    [string]$Command,

    [Parameter(Position = 1)]
    [string]$Name,

    [string]$Run = 'baseline',
    [string[]]$Sids,
    [string[]]$Maps,
    [switch]$NoMaps,
    [string]$Map = 'atop',
    [string]$SweepFile,
    [string]$Filter = '*',
    [ValidateRange(0, 3)]
    [int]$MaxTier = 3,
    [int]$Seeds = 4,
    [string]$Sid = 's00',
    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'shadercorpus.ps1')

$CorpusRoot = Join-Path $HomeDir 'shadercorpus'
$HarnessPs  = Join-Path $PSScriptRoot 'harness.ps1'
$EditorPs   = Join-Path $PSScriptRoot 'editor.ps1'
if (-not $SweepFile) { $SweepFile = Join-Path $PSScriptRoot 'shader-sweep.txt' }

$script:Defaults = $null      # var -> default value text, for every swept var
$script:CurrentMap = $null
$script:CurrentPoint = $null

# ---------------------------------------------------------------- game ----

function Invoke-Checked([string]$Script, [int]$SettleMs, [int]$Timeout) {
    $lines = @(Invoke-Batch $Script $SettleMs $Timeout)
    $bad = @($lines | Where-Object { $_ -match $ErrorPattern -or $_ -match 'valid range for' })
    if ($bad.Count) { throw "The game reported errors:`n$($bad -join "`n")" }
    return , $lines
}

function Get-VarDefaults([string[]]$Vars) {
    $defaults = [ordered]@{}
    if (-not $Vars.Count) { return $defaults }
    $script = ($Vars | ForEach-Object {
        "echo (concatword ""SWEEPVAR $_ "" (getvartype $_) "" "" (? (= (getvartype $_) 1) (getfvardef $_ 0) (getvardef $_ 0)))"
    }) -join "`n"
    foreach ($line in (Invoke-Checked $script 1 60)) {
        if ($line -match 'SWEEPVAR (\S+) (-?\d+) (\S+)') {
            if ($Matches[2] -ne '0' -and $Matches[2] -ne '1') { throw "Sweep var '$($Matches[1])' is not an int or float var (type $($Matches[2]))." }
            $defaults[$Matches[1]] = $Matches[3]
        }
    }
    foreach ($v in $Vars) { if (-not $defaults.Contains($v)) { throw "The game did not report sweep var '$v'." } }
    return $defaults
}

# Applies one settings vector on top of the defaults and regenerates shaders.
# The settle lets frames render, which is when the C++ setup paths call
# their generateshaders (AO, bilateral, deferred lights, volumetrics, AA).
function Set-SweepPoint($Point) {
    $lines = foreach ($v in $script:Defaults.Keys) {
        if ($Point.Settings.Contains($v)) { "$v $($Point.Settings[$v])" } else { "$v $($script:Defaults[$v])" }
    }
    $lines = @($lines) + 'resetshaders'
    $out = Invoke-Checked ($lines -join "`n") 2000 300
    $errors = @($out | Where-Object { $_ -match 'GLSL ERROR' }).Count
    if ($errors) { Write-Warning "$($Point.Id): $errors GLSL compile error(s); those shaders are recorded as invalid." }
    $script:CurrentPoint = $Point.Id
}

function Open-Map([string]$MapName) {
    # Loading a map while an entity is hovered trips an unguarded enthover
    # read (world.cpp:1426); entity editing off empties it. See editor-selftest.ps1.
    Invoke-Checked 'entediting 0' 300 60 | Out-Null
    & $EditorPs open $MapName 6>$null | Out-Null
    $script:CurrentMap = $MapName
    $script:CurrentPoint = $null
}

# Puts the game into the state a settings id was recorded in.
function Enter-Point([string]$PointId, $Points) {
    if ($PointId.StartsWith('m-')) {
        $mapName = $PointId.Substring(2)
        if ($script:CurrentPoint -cne 'defaults') { Set-SweepPoint ([pscustomobject]@{ Id = 'defaults'; Settings = [ordered]@{} }) }
        if ($script:CurrentMap -cne $mapName) { Open-Map $mapName; $script:CurrentPoint = 'defaults' }
        return
    }
    if ($script:CurrentMap -cne $Map) { Open-Map $Map }
    if ($script:CurrentPoint -cne $PointId) {
        $p = @($Points | Where-Object { $_.Id -ceq $PointId })
        if (-not $p.Count) { throw "Unknown settings id '$PointId'." }
        Set-SweepPoint $p[0]
    }
}

function Invoke-Dump([string]$RunName, [string]$PointId, [bool]$MapsOnly) {
    $flag = 0
    if ($MapsOnly) { $flag = 1 }
    $out = Invoke-Checked "shaderdumpall $RunName $PointId $flag" 1 600
    $hit = @($out | Where-Object { $_ -match "SHADERDUMP $([regex]::Escape($RunName)) $([regex]::Escape($PointId)) (\d+) (\d+)" })
    if (-not $hit.Count) { throw "shaderdumpall did not report for ${PointId}:`n$($out -join "`n")" }
    $errors = @($out | Where-Object { $_ -match 'GLSL ERROR' }).Count
    if ($errors) { Write-Warning "${PointId}: $errors GLSL compile error(s) while forcing; those shaders are recorded as invalid." }
    Write-Host ("  {0,-14} {1}" -f $PointId, ($hit[0] -replace '^.*SHADERDUMP \S+ \S+ ', 'rows/new blobs: '))
}

function Write-Registry([string]$RunName) {
    $script = 'writetofile "shadercorpus/RUN/registry.txt" (concatword (looplistconcatword e $worldshaders [concatword "world " (at $e 0) " " (at $e 1) "^n"]) (looplistconcatword e $decalshaders [concatword "decal " (at $e 0) " " (at $e 1) "^n"]))'
    Invoke-Checked $script.Replace('RUN', $RunName) 1 60 | Out-Null
    $path = Join-Path $CorpusRoot "$RunName\registry.txt"
    if (-not (Test-Path $path)) { throw "registry.txt was not written: $path" }
}

function Get-ShippedMaps {
    return @(Get-ChildItem (Join-Path $RepoRoot 'data\maps\*.mpz') | ForEach-Object { $_.BaseName } | Sort-Object)
}

function Invoke-Record([string]$RunName, $Points, [string[]]$MapList) {
    if (-not (Get-HarnessProcess)) { & $HarnessPs start 6>$null | Out-Null }
    $runDir = Join-Path $CorpusRoot $RunName
    if (Test-Path $runDir) { Remove-Item -Recurse -Force $runDir }
    New-Item -ItemType Directory -Force (Join-Path $runDir 'settings') | Out-Null

    $script:Defaults = Get-VarDefaults (Get-SweepVars $Points)
    Write-Host "Recording '$RunName': $(@($Points).Count) settings point(s), $(@($MapList).Count) map(s)" -ForegroundColor Cyan

    foreach ($p in $Points) {
        Enter-Point $p.Id $Points
        Invoke-Dump $RunName $p.Id $false
        Write-TextNoBom (Join-Path $runDir "settings\$($p.Id).txt") ((@($p.Settings.Keys | ForEach-Object { "$_=$($p.Settings[$_])" }) -join "`n") + "`n")
    }
    Write-Registry $RunName
    foreach ($m in $MapList) {
        Enter-Point "m-$m" $Points
        Invoke-Dump $RunName "m-$m" $true
    }

    # Leave the persisted settings at their defaults.
    Set-SweepPoint ([pscustomobject]@{ Id = 'defaults'; Settings = [ordered]@{} })

    $commit = (& git -C $RepoRoot rev-parse HEAD).Trim()
    $dirty = 0
    if (@(& git -C $RepoRoot status --porcelain -- config/glsl).Count) { $dirty = 1 }
    $info = @(
        "commit $commit"
        "glsldirty $dirty"
        "recorded $((Get-Date).ToString('s'))"
        "map $Map"
        "sweep $(@($Points | ForEach-Object { $_.Id }) -join ' ')"
        "maps $(@($MapList) -join ' ')"
    )
    Write-TextNoBom (Join-Path $runDir 'run.txt') (($info -join "`n") + "`n")

    # Self-checks.
    $rows = @(Read-Manifest $runDir)
    $ids = @($Points | ForEach-Object { $_.Id })
    $problems = 0
    if ($ids -ccontains 's00' -and $ids -ccontains 's99') {
        $leaks = @(Find-SweepLeaks $rows 's00' 's99')
        foreach ($l in $leaks) { Write-Host "  LEAK  $($l.Name): s00 $($l.First) vs s99 $($l.Last) (origin $($l.Origin))" -ForegroundColor Red }
        if ($leaks.Count) { Write-Host '  State leaked between sweep points: a var these shaders read is missing from the reset list in shader-sweep.txt.' -ForegroundColor Red; $problems++ }
        else { Write-Host '  no state leaked between s00 and s99' -ForegroundColor Green }
    }
    $registry = [System.IO.File]::ReadAllLines((Join-Path $runDir 'registry.txt'))
    $gaps = @(Test-PaletteCoverage $rows $registry $ids)
    foreach ($g in $gaps) { Write-Host "  PALETTE  $g has no valid shader" -ForegroundColor Red }
    if ($gaps.Count) { $problems++ } else { Write-Host "  all $($registry.Count) registered world/decal shaders valid at every point" -ForegroundColor Green }
    if ($dirty -and $RunName -ceq 'baseline') { Write-Warning 'config/glsl has uncommitted changes: this baseline is not a pinned commit.' }
    return $problems
}

# ---------------------------------------------------------------- main ----

switch ($Command) {
    'record' {
        $points = @(Read-Sweep $SweepFile)
        if ($Sids) { $points = @($points | Where-Object { $Sids -ccontains $_.Id }) }
        $mapList = @()
        if (-not $NoMaps) { if ($Maps) { $mapList = $Maps } else { $mapList = Get-ShippedMaps } }
        $problems = Invoke-Record $Run $points $mapList
        if ($problems) { exit 1 }
        exit 0
    }
    'check' { throw 'check is implemented in Task 8.' }
    'diff'  { throw 'diff is implemented in Task 8.' }
}
```

- [ ] **Step 5: Run the test**

Run (harness started, Task 3's build): `powershell -NoProfile -File tools\harness\tests\task5-record.ps1`
Expected: `task 5: all checks passed`.

Diagnose failures by symptom:
- **`msaa generated multisample light shaders` fails:** read the `s01` rows. The MSAA setup may need `resetgl` rather than `resetshaders` (the vars carry `initwarning(..., INIT_LOAD, CHANGE_SHADERS)`). Confirm with `harness.ps1 send 'msaa 4; resetshaders' -Settle 2000` followed by `echo $msaasamples`. If `$msaasamples` stays 0, switch `Set-SweepPoint` to append `resetgl` for points that set `msaa*`, `gdepthstencil`, `gstencil`, `glineardepth`, `hdrgamma` or `textsupersample`. Then note that in `shader-sweep.txt`'s header.
- **A leak is reported:** the named var is read by a generator and missing from the sweep file. Add a vector for it.

- [ ] **Step 6: Commit**

```bash
git add tools/harness/shader-sweep.txt tools/harness/shaders.ps1 tools/harness/tests/task5-record.ps1
git commit -m "harness: record shader corpora across a settings sweep"
```

---

### Task 6: Offline tiers 1–2 (`shadercheck.py`)

**Files:**
- Create: `tools/harness/shadercheck.py`
- Create: `tools/harness/tests/test_shadercheck.py`

**Interfaces:**
- Consumes: blob dirs (`vs.full.glsl`, `fs.full.glsl`) from Task 3.
- Produces:
  - `python3 shadercheck.py --pairs <file>`: input lines `key<TAB>baseblobdir<TAB>candblobdir` (WSL paths). Output lines `key<TAB>TEXT|SPIRV|DIFF|NA<TAB>detail`, one per input line, in input order. Exit 0 unless the input is malformed.
  - `python3 shadercheck.py --normalize <file> --stage vert|frag`: prints the tier-1 normal form, one line per non-empty source line with tokens joined by single spaces.
  - Functions `tokens(text)`, `strip_comments(text)`, `normal_lines(path, stage, tmp, have_glslang)`, `check_pair(key, base, cand, have_glslang)`.

- [ ] **Step 1: Install the tools**

These are system package installs, so ask the user to run the command (it prompts for their WSL sudo password):

```bash
wsl -d Ubuntu -- sudo apt-get install -y glslang-tools spirv-tools
```

Verify: `wsl -d Ubuntu --exec sh -c "glslangValidator --version | head -1; spirv-opt --version; spirv-remap --help 2>&1 | head -1"`.
Expected: glslang 12.x, SPIRV-Tools v2023.x, a spirv-remap usage line. If the user declines, continue: the glslang tests skip, and tier 2 reports `NA`.

- [ ] **Step 2: Write the failing tests**

Create `tools/harness/tests/test_shadercheck.py`:

```python
"""Tests for tools/harness/shadercheck.py.

Run: wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py" -v
The glslang cases skip when glslangValidator is not installed.
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import shadercheck  # noqa: E402

VS = "#version 400\nin vec4 vvertex;\nvoid main(void)\n{\n    gl_Position = vvertex;\n}\n"
FS = ("#version 400\nlayout(location = 0) out vec4 fragcolor;\nuniform vec4 c;\n"
      "void main(void)\n{\n    vec4 x = c * 2.0;\n    fragcolor = x;\n}\n")
HAVE_GLSLANG = shutil.which("glslangValidator") is not None
HAVE_SPIRV = HAVE_GLSLANG and shutil.which("spirv-opt") is not None and shutil.which("spirv-remap") is not None


class Pure(unittest.TestCase):
    def test_tokens_ignore_spacing(self):
        self.assertEqual(shadercheck.tokens("a+=b;"), shadercheck.tokens("a  +=  b ;"))

    def test_tokens_keep_numbers_whole(self):
        self.assertEqual(shadercheck.tokens("x = 1.5e-3f;"), ["x", "=", "1.5e-3f", ";"])

    def test_tokens_distinguish_operators(self):
        self.assertNotEqual(shadercheck.tokens("a += b"), shadercheck.tokens("a + = b"))

    def test_strip_comments(self):
        self.assertEqual(shadercheck.tokens(shadercheck.strip_comments("a /* x */ b // y\nc")), ["a", "b", "c"])

    def test_line_directives_dropped(self):
        self.assertEqual(shadercheck.tokens('#line 3 "x"\na'), ["a"])


class Pairs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def blob(self, name, fs, vs=VS):
        d = os.path.join(self.tmp, name)
        os.makedirs(d)
        with open(os.path.join(d, "vs.full.glsl"), "w") as f:
            f.write(vs)
        with open(os.path.join(d, "fs.full.glsl"), "w") as f:
            f.write(fs)
        return d

    def test_comment_only_change_is_text(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("fragcolor = x;", "fragcolor = x; // note"))
        self.assertEqual(shadercheck.check_pair("k", a, b, HAVE_GLSLANG)[1], "TEXT")

    @unittest.skipUnless(HAVE_GLSLANG, "glslangValidator not installed")
    def test_macro_spelling_is_text_after_preprocessing(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("c * 2.0", "c * TWO").replace("uniform vec4 c;", "uniform vec4 c;\n#define TWO 2.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "TEXT")

    @unittest.skipUnless(HAVE_SPIRV, "glslang/spirv-tools not installed")
    def test_local_rename_is_spirv(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("vec4 x", "vec4 y").replace("= x;", "= y;"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "SPIRV")

    @unittest.skipUnless(HAVE_SPIRV, "glslang/spirv-tools not installed")
    def test_constant_change_is_diff(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("2.0", "3.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, True)[1], "DIFF")

    def test_without_glslang_a_real_change_is_na(self):
        a = self.blob("a", FS)
        b = self.blob("b", FS.replace("2.0", "3.0"))
        self.assertEqual(shadercheck.check_pair("k", a, b, False)[1], "NA")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Run to verify it fails**

Run: `wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py" -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'shadercheck'`.

- [ ] **Step 4: Implement `tools/harness/shadercheck.py`**

```python
#!/usr/bin/env python3
"""Offline tiers of the shader equivalence harness (tools/harness/shaders.ps1).

Tier 1 (TEXT):  both composed sources, preprocessed by glslangValidator -E
                (or comment-stripped when it is absent), have the same tokens.
Tier 2 (SPIRV): both compile to the same SPIR-V after spirv-opt -O and
                spirv-remap --map all --strip all.
Otherwise DIFF, or NA when a tier could not run (glslang missing or it
rejected the source; legacy compat-header constructs may not map to GL
SPIR-V). NA and DIFF both go on to the pixel tier.

Usage:
  shadercheck.py --pairs pairs.tsv       lines: key<TAB>baseblobdir<TAB>candblobdir
  shadercheck.py --normalize FILE --stage vert|frag
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

STAGES = (("vs.full.glsl", "vert"), ("fs.full.glsl", "frag"))

TOKEN = re.compile(r"""
    [A-Za-z_]\w*                                    # identifier or keyword
  | (?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuU]?  # number
  | <<=|>>=|\+\+|--|&&|\|\||\^\^|[<>=!+\-*/%&|^]=|<<|>>
  | \S                                              # any other single character
""", re.X)
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
VERSION = re.compile(r"^\s*#\s*version\s+\d+.*$", re.M)


def run(cmd):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def strip_comments(text):
    return COMMENT.sub(" ", text)


def tokens(text):
    out = []
    for line in text.splitlines():
        if line.lstrip().startswith("#line"):
            continue
        out.extend(TOKEN.findall(line))
    return out


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def preprocessed(path, stage, tmp, have_glslang):
    """The source as the compiler sees it, or comment-stripped raw text."""
    if have_glslang:
        src = os.path.join(tmp, "pp." + stage)
        shutil.copyfile(path, src)
        r = run(["glslangValidator", "-E", src])
        if r.returncode == 0:
            return r.stdout
    return strip_comments(read(path))


def normal_lines(path, stage, tmp, have_glslang):
    lines = []
    for line in preprocessed(path, stage, tmp, have_glslang).splitlines():
        t = tokens(line)
        if t:
            lines.append(" ".join(t))
    return lines


def spirv(path, stage, tmp, tag):
    """Canonical SPIR-V bytes, or (None, reason)."""
    text = read(path)
    err = ""
    for version in (None, "450"):
        src = os.path.join(tmp, "%s.%s" % (tag, stage))
        with open(src, "w") as f:
            f.write(text if version is None else VERSION.sub("#version " + version, text, count=1))
        spv = src + ".spv"
        r = run(["glslangValidator", "-G", "--auto-map-locations", "--auto-map-bindings", "-o", spv, src])
        if r.returncode == 0:
            break
        err = (r.stdout.strip().splitlines() or ["glslang failed"])[-1]
    else:
        return None, "glslang: " + err
    opt = spv + ".opt"
    r = run(["spirv-opt", "-O", spv, "-o", opt])
    if r.returncode != 0:
        return None, "spirv-opt: " + (r.stdout.strip().splitlines() or ["failed"])[-1]
    outdir = os.path.join(tmp, tag + "-remap")
    os.makedirs(outdir, exist_ok=True)
    r = run(["spirv-remap", "--map", "all", "--strip", "all", "-i", opt, "-o", outdir])
    if r.returncode != 0:
        return None, "spirv-remap: " + (r.stdout.strip().splitlines() or ["failed"])[-1]
    with open(os.path.join(outdir, os.path.basename(opt)), "rb") as f:
        return f.read(), ""


def check_pair(key, base, cand, have_glslang):
    with tempfile.TemporaryDirectory() as tmp:
        same = True
        for fname, stage in STAGES:
            a = normal_lines(os.path.join(base, fname), stage, tmp, have_glslang)
            b = normal_lines(os.path.join(cand, fname), stage, tmp, have_glslang)
            if tokens(" ".join(a)) != tokens(" ".join(b)):
                same = False
                break
        if same:
            return key, "TEXT", ""
        if not have_glslang or not shutil.which("spirv-opt") or not shutil.which("spirv-remap"):
            return key, "NA", "glslang/spirv-tools not installed"
        for fname, stage in STAGES:
            sa, ea = spirv(os.path.join(base, fname), stage, tmp, "a")
            sb, eb = spirv(os.path.join(cand, fname), stage, tmp, "b")
            if sa is None or sb is None:
                return key, "NA", "%s %s" % (stage, ea or eb)
            if sa != sb:
                return key, "DIFF", "%s SPIR-V differs" % stage
        return key, "SPIRV", ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs")
    ap.add_argument("--normalize")
    ap.add_argument("--stage", choices=("vert", "frag"), default="frag")
    args = ap.parse_args()
    have_glslang = shutil.which("glslangValidator") is not None

    if args.normalize:
        with tempfile.TemporaryDirectory() as tmp:
            print("\n".join(normal_lines(args.normalize, args.stage, tmp, have_glslang)))
        return 0

    if not args.pairs:
        ap.error("--pairs or --normalize is required")
    jobs = []
    with open(args.pairs, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) != 3:
                sys.stderr.write("%s:%d: expected key<TAB>base<TAB>cand\n" % (args.pairs, n))
                return 2
            jobs.append(parts)
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        for key, tier, detail in pool.map(lambda j: check_pair(j[0], j[1], j[2], have_glslang), jobs):
            print("%s\t%s\t%s" % (key, tier, detail))
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 5: Run the tests**

Run: `wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py" -v`
Expected: all pass, or the glslang cases show `skipped`.

If `test_local_rename_is_spirv` returns `DIFF` with the tools installed, `spirv-remap` output still differs. Print both remapped modules with `spirv-dis` and compare. Adding `--do-everything` in place of `--map all --strip all` is the documented alternative.

- [ ] **Step 6: Commit**

```bash
git add tools/harness/shadercheck.py tools/harness/tests/test_shadercheck.py
git commit -m "harness: add offline text and SPIR-V shader equivalence tiers"
```

---

### Task 7: `shaderbench` (tier 3)

**Files:**
- Modify: `src/engine/shaderharness.cpp` (append the bench)
- Create: `tools/harness/tests/task7-bench.ps1`

**Interfaces:**
- Consumes: Task 1 helpers, Task 2 `scanfragdatalocs`, Task 3 reflection entry points and corpus.
- Produces: command `shaderbench <run> <hash> <name> <seeds>`. It prints one line and returns nothing:
  ```
  SHADERBENCH <name> <PASS|FAIL|WEAK> maxerr=<g> cov=<0-100> seeds=<n>[ reason=<text>]
  ```
  - `FAIL` reasons: `missing` (no live shader), `oldsource`, `oldcompile`, `fbo`.
  - `WEAK` reasons: `coverage`, or `unsupported <what> <type>` when an input could not be fed.
  - Otherwise `FAIL` means `maxerr > 1e-5`.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task7-bench.ps1`:

```powershell
# Task 7 verification: shaderbench against the running game.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Bench([string]$Script) {
    $hit = @(Invoke-Batch $Script 1 120 | Where-Object { $_ -match 'SHADERBENCH ' })
    if ($hit.Count) { return ($hit[0] -replace '^.*SHADERBENCH ', '') }
    return ''
}

$dir = Join-Path $HomeDir 'shadercorpus\t7'
Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
Invoke-Batch 'shaderdumpall t7 s00 0' 1 300 | Out-Null
$rows = @(Read-Manifest $dir)
$hud = @($rows | Where-Object { $_.Name -ceq 'hud' })[0]
$plain = @($rows | Where-Object { $_.Name -ceq 'hudnotexture' })[0]

$same = Bench "shaderbench t7 $($hud.Hash) ""hud"" 2"
Assert-That "hud against its own blob passes exactly ($same)" ($same -match '^hud PASS maxerr=0 ')
if ($same -match 'cov=(\d+)') { Assert-That 'hud covers most of the target' ([int]$Matches[1] -ge 50) }
$other = Bench "shaderbench t7 $($plain.Hash) ""hud"" 2"
Assert-That "a different shader fails ($other)" ($other -match '^hud FAIL ')
$nosrc = Bench 'shaderbench t7 0000000000000000 "hud" 1'
Assert-That "a missing blob fails ($nosrc)" ($nosrc -match '^hud FAIL .*reason=oldsource')
$nolive = Bench "shaderbench t7 $($hud.Hash) ""nosuchshader"" 1"
Assert-That "a missing live shader fails ($nolive)" ($nolive -match '^nosuchshader FAIL .*reason=missing')
$again = Bench "shaderbench t7 $($hud.Hash) ""hud"" 2"
Assert-That 'the bench is repeatable' ($again -ceq $same)
$shot = & (Join-Path $PSScriptRoot '..\harness.ps1') shot task7
Assert-That 'the game still renders afterwards (read the PNG)' (Test-Path $shot)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 7: all checks passed' -ForegroundColor Green
```

- [ ] **Step 2: Run to verify it fails**

Run (harness started): `powershell -NoProfile -File tools\harness\tests\task7-bench.ps1`
Expected: FAIL. Every bench line is empty (`Unknown command: shaderbench`).

- [ ] **Step 3: Implement**

Append to `src/engine/shaderharness.cpp`, inside `#ifdef DEBUG_UTILS`:

```cpp
// --------------------------------------------------------------- bench ----
//
// Renders the corpus copy of a shader (compiled from its composed source, so
// no header is injected) and the live shader into RGBA32F targets with the
// same seeded inputs, and compares the pixels. Inputs come from the live
// program's reflection; tier 0 has already shown the two interfaces match.

static const int BENCHSIZE = 256, BENCHGRID = 32, BENCHTARGETS = 4, BENCHVERTS = BENCHGRID*BENCHGRID*6;
static const float BENCHCLEAR = -12345.0f;
static const double BENCHTOLERANCE = 1e-5;

struct benchtex { char *name; GLenum target; GLuint tex; };
struct benchattrib { char *name; int loc, comps; GLuint vbo; };
struct benchblock { char *name; GLuint buf; };

struct benchinputs
{
    vector<benchtex> textures;
    vector<benchattrib> attribs;
    vector<benchblock> blocks;
    string reason;

    benchinputs() { reason[0] = '\0'; }
    ~benchinputs()
    {
        loopv(textures) { glDeleteTextures(1, &textures[i].tex); delete[] textures[i].name; }
        loopv(attribs) { glDeleteBuffers_(1, &attribs[i].vbo); delete[] attribs[i].name; }
        loopv(blocks) { glDeleteBuffers_(1, &blocks[i].buf); delete[] blocks[i].name; }
    }

    void unsupported(const char *what, GLenum type)
    {
        if(!reason[0]) formatstring(reason, "unsupported %s %s", what, gltypename(type));
    }
};

static void benchnoise(vector<float> &data, int count, const char *name, int seed, float lo, float hi)
{
    data.setsize(0);
    loopi(count) data.add(lo + (hi - lo)*(benchfloat(name, i, seed) - 0.1f)/0.9f);
}

static GLuint benchtexture(GLenum samplertype, const char *name, int seed, GLenum &target)
{
    bool shadow = false;
    int w = 64, h = 64, d = 1;
    switch(samplertype)
    {
        case GL_SAMPLER_2D: target = GL_TEXTURE_2D; break;
        case GL_SAMPLER_2D_SHADOW: target = GL_TEXTURE_2D; shadow = true; break;
        case GL_SAMPLER_2D_RECT: target = GL_TEXTURE_RECTANGLE; w = h = BENCHSIZE; break;
        case GL_SAMPLER_2D_RECT_SHADOW: target = GL_TEXTURE_RECTANGLE; w = h = BENCHSIZE; shadow = true; break;
        case GL_SAMPLER_3D: target = GL_TEXTURE_3D; w = h = d = 16; break;
        case GL_SAMPLER_2D_ARRAY: target = GL_TEXTURE_2D_ARRAY; d = 4; break;
        case GL_SAMPLER_2D_ARRAY_SHADOW: target = GL_TEXTURE_2D_ARRAY; d = 4; shadow = true; break;
        case GL_SAMPLER_CUBE: target = GL_TEXTURE_CUBE_MAP; w = h = 32; break;
        default: return 0;
    }
    int comps = shadow ? 1 : 4, faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    vector<float> data;
    benchnoise(data, w*h*d*comps*faces, name, seed, shadow ? 0.1f : 0.0f, shadow ? 0.9f : 1.0f);
    GLenum ifmt = shadow ? GL_DEPTH_COMPONENT32F : GL_RGBA32F, fmt = shadow ? GL_DEPTH_COMPONENT : GL_RGBA;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(target, tex);
    switch(target)
    {
        case GL_TEXTURE_3D: case GL_TEXTURE_2D_ARRAY:
            glTexImage3D_(target, 0, ifmt, w, h, d, 0, fmt, GL_FLOAT, data.getbuf());
            break;
        case GL_TEXTURE_CUBE_MAP:
            loopi(6) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, ifmt, w, h, 0, fmt, GL_FLOAT, &data[i*w*h*comps]);
            break;
        default:
            glTexImage2D(target, 0, ifmt, w, h, 0, fmt, GL_FLOAT, data.getbuf());
            break;
    }
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP) glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if(shadow)
    {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    return tex;
}

static bool uniforminblock(GLuint p, int i)
{
    if(!glGetActiveUniformsiv_) return false;
    GLuint idx = i;
    GLint block = -1;
    glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_BLOCK_INDEX, &block);
    return block >= 0;
}

// Textures, vertex arrays and uniform buffers for one seed, shared by both
// programs so they read identical data.
static void prepareinputs(GLuint live, int seed, benchinputs &in)
{
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;

    glGetProgramiv_(live, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(live, i, sizeof(name), &len, &size, &type, name);
        if(!issamplertype(type) || uniforminblock(live, i)) continue;
        benchtex &t = in.textures.add();
        t.name = newstring(name);
        t.tex = benchtexture(type, name, seed, t.target);
        if(!t.tex) in.unsupported("sampler", type);
    }

    // A grid of triangles covering most of clip space. Every matrix uniform
    // is identity, so shaders that transform vvertex keep it on screen.
    vector<float> xy;
    loopi(BENCHGRID) loopj(BENCHGRID)
    {
        float x0 = -0.95f + 1.9f*i/BENCHGRID, x1 = -0.95f + 1.9f*(i+1)/BENCHGRID,
              y0 = -0.95f + 1.9f*j/BENCHGRID, y1 = -0.95f + 1.9f*(j+1)/BENCHGRID;
        const float quad[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
        loopk(12) xy.add(quad[k]);
    }

    glGetProgramiv_(live, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(live, i, sizeof(name), &len, &size, &type, name);
        GLint loc = glGetAttribLocation_(live, name);
        if(loc < 0) continue;
        int comps = 0;
        switch(type)
        {
            case GL_FLOAT: comps = 1; break;
            case GL_FLOAT_VEC2: comps = 2; break;
            case GL_FLOAT_VEC3: comps = 3; break;
            case GL_FLOAT_VEC4: comps = 4; break;
        }
        if(!comps) { in.unsupported("attribute", type); continue; }
        vector<float> data;
        for(int v = 0; v < BENCHVERTS; v++) loopk(comps)
        {
            float val;
            if(!strcmp(name, "vvertex")) val = k < 2 ? xy[v*2+k] : (k == 2 ? 0.5f : 1.0f);
            else if(!strcmp(name, "vboneindex")) val = float(int((benchfloat(name, v*4+k, seed) - 0.1f)/0.9f*3.999f));
            else val = benchfloat(name, v*4+k, seed);
            data.add(val);
        }
        benchattrib &a = in.attribs.add();
        a.name = newstring(name);
        a.loc = loc;
        a.comps = comps;
        glGenBuffers_(1, &a.vbo);
        glBindBuffer_(GL_ARRAY_BUFFER, a.vbo);
        glBufferData_(GL_ARRAY_BUFFER, data.length()*sizeof(float), data.getbuf(), GL_STATIC_DRAW);
    }
    glBindBuffer_(GL_ARRAY_BUFFER, 0);

    if(glGetActiveUniformBlockiv_ && glGetActiveUniformBlockName_)
    {
        glGetProgramiv_(live, GL_ACTIVE_UNIFORM_BLOCKS, &n);
        loopi(n)
        {
            glGetActiveUniformBlockName_(live, i, sizeof(name), NULL, name);
            GLint datasize = 0;
            glGetActiveUniformBlockiv_(live, i, GL_UNIFORM_BLOCK_DATA_SIZE, &datasize);
            vector<float> data;
            benchnoise(data, (datasize + 3)/4, name, seed, 0.1f, 1.0f);
            benchblock &b = in.blocks.add();
            b.name = newstring(name);
            glGenBuffers_(1, &b.buf);
            glBindBuffer_(GL_UNIFORM_BUFFER, b.buf);
            glBufferData_(GL_UNIFORM_BUFFER, data.length()*sizeof(float), data.getbuf(), GL_STATIC_DRAW);
        }
        glBindBuffer_(GL_UNIFORM_BUFFER, 0);
    }
}

static const float benchidentity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

// Sets program p's uniforms from the live program's list, so both programs
// get the same values for the same names.
static void bindinputs(GLuint p, GLuint live, int seed, benchinputs &in)
{
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;
    int sampler = 0;

    glGetProgramiv_(live, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(live, i, sizeof(name), &len, &size, &type, name);
        if(uniforminblock(live, i)) continue;
        GLint loc = glGetUniformLocation_(p, name);
        if(issamplertype(type))
        {
            int unit = sampler++;
            if(!in.textures.inrange(unit)) continue;
            benchtex &t = in.textures[unit];
            glActiveTexture_(GL_TEXTURE0 + unit);
            if(t.tex) glBindTexture(t.target, t.tex);
            if(loc >= 0) glUniform1i_(loc, unit);
            continue;
        }
        if(loc < 0) continue;
        int comps = gltypecomponents(type), count = size*comps;
        if(!comps) { in.unsupported("uniform", type); continue; }
        if(type == GL_FLOAT_MAT2 || type == GL_FLOAT_MAT3 || type == GL_FLOAT_MAT4)
        {
            vector<float> m;
            loopj(size)
            {
                if(type == GL_FLOAT_MAT4) loopk(16) m.add(benchidentity[k]);
                else if(type == GL_FLOAT_MAT3) loopk(9) m.add(k%4 == 0 ? 1.0f : 0.0f);
                else loopk(4) m.add(k == 0 || k == 3 ? 1.0f : 0.0f);
            }
            if(type == GL_FLOAT_MAT4) glUniformMatrix4fv_(loc, size, GL_FALSE, m.getbuf());
            else if(type == GL_FLOAT_MAT3) glUniformMatrix3fv_(loc, size, GL_FALSE, m.getbuf());
            else glUniformMatrix2fv_(loc, size, GL_FALSE, m.getbuf());
            continue;
        }
        switch(type)
        {
            case GL_FLOAT: case GL_FLOAT_VEC2: case GL_FLOAT_VEC3: case GL_FLOAT_VEC4:
            {
                vector<float> v;
                loopj(count) v.add(benchfloat(name, j, seed));
                if(comps == 1) glUniform1fv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2fv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3fv_(loc, size, v.getbuf());
                else glUniform4fv_(loc, size, v.getbuf());
                break;
            }
            case GL_INT: case GL_INT_VEC2: case GL_INT_VEC3: case GL_INT_VEC4:
            case GL_BOOL: case GL_BOOL_VEC2: case GL_BOOL_VEC3: case GL_BOOL_VEC4:
            {
                bool isbool = type == GL_BOOL || type == GL_BOOL_VEC2 || type == GL_BOOL_VEC3 || type == GL_BOOL_VEC4;
                vector<GLint> v;
                loopj(count) v.add(isbool ? benchint(name, j, seed)&1 : benchint(name, j, seed));
                if(comps == 1) glUniform1iv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2iv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3iv_(loc, size, v.getbuf());
                else glUniform4iv_(loc, size, v.getbuf());
                break;
            }
            case GL_UNSIGNED_INT: case GL_UNSIGNED_INT_VEC2: case GL_UNSIGNED_INT_VEC3: case GL_UNSIGNED_INT_VEC4:
            {
                vector<GLuint> v;
                loopj(count) v.add(GLuint(benchint(name, j, seed)));
                if(comps == 1) glUniform1uiv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2uiv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3uiv_(loc, size, v.getbuf());
                else glUniform4uiv_(loc, size, v.getbuf());
                break;
            }
            default: in.unsupported("uniform", type); break;
        }
    }

    loopv(in.blocks)
    {
        GLuint idx = glGetUniformBlockIndex_(p, in.blocks[i].name);
        if(idx == GL_INVALID_INDEX) continue;
        glUniformBlockBinding_(p, idx, i);
        glBindBufferBase_(GL_UNIFORM_BUFFER, i, in.blocks[i].buf);
    }

    loopv(in.attribs)
    {
        benchattrib &a = in.attribs[i];
        glBindBuffer_(GL_ARRAY_BUFFER, a.vbo);
        glVertexAttribPointer_(a.loc, a.comps, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray_(a.loc);
    }
    glBindBuffer_(GL_ARRAY_BUFFER, 0);
}

static GLuint compilebenchstage(GLenum type, const char *src)
{
    GLuint obj = glCreateShader_(type);
    glShaderSource_(obj, 1, (const GLchar **)&src, NULL);
    glCompileShader_(obj);
    GLint ok = 0;
    glGetShaderiv_(obj, GL_COMPILE_STATUS, &ok);
    if(!ok) { glDeleteShader_(obj); return 0; }
    return obj;
}

// Links the corpus copy with the live program's attribute and fragment
// output locations, so both read the same arrays and write the same targets.
static GLuint linkoldprogram(const char *vs, const char *fs, Shader &live)
{
    GLuint vsobj = compilebenchstage(GL_VERTEX_SHADER, vs), fsobj = compilebenchstage(GL_FRAGMENT_SHADER, fs);
    if(!vsobj || !fsobj)
    {
        if(vsobj) glDeleteShader_(vsobj);
        if(fsobj) glDeleteShader_(fsobj);
        return 0;
    }
    GLuint p = glCreateProgram_();
    glAttachShader_(p, vsobj);
    glAttachShader_(p, fsobj);
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;
    glGetProgramiv_(live.program, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(live.program, i, sizeof(name), &len, &size, &type, name);
        GLint loc = glGetAttribLocation_(live.program, name);
        if(loc >= 0) glBindAttribLocation_(p, loc, name);
    }
    if(glBindFragDataLocation_)
    {
        vector<FragDataLoc> outs;
        scanfragdatalocs(live, outs);
        loopv(outs) if(!outs[i].index) glBindFragDataLocation_(p, outs[i].loc, outs[i].name);
    }
    glLinkProgram_(p);
    // Flagged for deletion; freed with the program.
    glDeleteShader_(vsobj);
    glDeleteShader_(fsobj);
    GLint ok = 0;
    glGetProgramiv_(p, GL_LINK_STATUS, &ok);
    if(!ok) { glDeleteProgram_(p); return 0; }
    return p;
}

static void benchrender(GLuint fbo, GLuint p, GLuint live, int seed, benchinputs &in, vector<float> &out)
{
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, BENCHSIZE, BENCHSIZE);
    glClearColor(BENCHCLEAR, BENCHCLEAR, BENCHCLEAR, BENCHCLEAR);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram_(p);
    bindinputs(p, live, seed, in);
    glDrawArrays(GL_TRIANGLES, 0, BENCHVERTS);
    loopv(in.attribs) glDisableVertexAttribArray_(in.attribs[i].loc);
    out.setsize(0);
    loopi(BENCHTARGETS)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        glReadPixels(0, 0, BENCHSIZE, BENCHSIZE, GL_RGBA, GL_FLOAT, out.pad(BENCHSIZE*BENCHSIZE*4));
    }
}

static void benchcompare(const vector<float> &a, const vector<float> &b, double &maxerr, int &written)
{
    const int pixels = BENCHSIZE*BENCHSIZE;
    written = 0;
    loopi(pixels)
    {
        bool w = false;
        loopk(BENCHTARGETS) loopj(4)
        {
            int idx = (k*pixels + i)*4 + j;
            float x = a[idx], y = b[idx];
            if(x != BENCHCLEAR || y != BENCHCLEAR) w = true;
            if(x == y || (isnan(x) && isnan(y))) continue;
            double err = 1e30;
            if(!isnan(x) && !isnan(y) && !isinf(x) && !isinf(y)) err = fabs(double(x) - double(y))/max(1.0, fabs(double(x)));
            maxerr = max(maxerr, err);
        }
        if(w) written++;
    }
}

struct benchglstate
{
    GLint vao, fbo, viewport[4];
    GLfloat clear[4];
    GLboolean blend, depth, cull, stencil, scissor;

    void save()
    {
        vao = 0;
        if(hasVAO) glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        blend = glIsEnabled(GL_BLEND); depth = glIsEnabled(GL_DEPTH_TEST); cull = glIsEnabled(GL_CULL_FACE);
        stencil = glIsEnabled(GL_STENCIL_TEST); scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    static void setcap(GLenum cap, GLboolean on) { if(on) glEnable(cap); else glDisable(cap); }

    void restore()
    {
        glUseProgram_(0);
        Shader::lastshader = NULL;
        glActiveTexture_(GL_TEXTURE0);
        glBindBuffer_(GL_ARRAY_BUFFER, 0);
        if(hasVAO) glBindVertexArray_(vao);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        setcap(GL_BLEND, blend); setcap(GL_DEPTH_TEST, depth); setcap(GL_CULL_FACE, cull);
        setcap(GL_STENCIL_TEST, stencil); setcap(GL_SCISSOR_TEST, scissor);
    }
};

static void reportbench(const char *name, const char *status, double maxerr, int cov, int seeds, const char *reason)
{
    conoutf(colourwhite, "SHADERBENCH %s %s maxerr=%g cov=%d seeds=%d%s%s", name, status, maxerr, cov, seeds, reason && reason[0] ? " reason=" : "", reason ? reason : "");
}

ICOMMAND(0, shaderbench, "sssi", (char *run, char *hash, char *name, int *seeds),
{
    if(identflags&IDF_MAP) return;
    int numseeds = *seeds > 0 ? min(*seeds, 16) : 4;
    if(!validfield(run) || !validfield(hash)) { reportbench(name, "FAIL", 0, 0, numseeds, "oldsource"); return; }
    Shader *live = lookupshaderbyname(name);
    if(!live || !live->program) { reportbench(name, "FAIL", 0, 0, numseeds, "missing"); return; }

    defformatstring(vspath, "shadercorpus/%s/blobs/%s/vs.full.glsl", run, hash);
    defformatstring(fspath, "shadercorpus/%s/blobs/%s/fs.full.glsl", run, hash);
    size_t vslen = 0, fslen = 0;
    char *vs = loadfile(vspath, &vslen, false), *fs = loadfile(fspath, &fslen, false);
    if(!vs || !fs) { DELETEA(vs); DELETEA(fs); reportbench(name, "FAIL", 0, 0, numseeds, "oldsource"); return; }

    gle::disable();
    benchglstate state;
    state.save();
    GLuint vao = 0;
    if(hasVAO) { glGenVertexArrays_(1, &vao); glBindVertexArray_(vao); }

    GLuint old = linkoldprogram(vs, fs, *live);
    delete[] vs;
    delete[] fs;

    GLuint fbo = 0, targets[BENCHTARGETS];
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glGenTextures(BENCHTARGETS, targets);
    GLenum bufs[BENCHTARGETS];
    loopi(BENCHTARGETS)
    {
        glBindTexture(GL_TEXTURE_2D, targets[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, BENCHSIZE, BENCHSIZE, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, targets[i], 0);
        bufs[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers_(BENCHTARGETS, bufs);
    bool fbook = glCheckFramebufferStatus_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    const char *status = "PASS";
    string reason;
    reason[0] = '\0';
    double maxerr = 0;
    int bestcov = 0;
    if(!old) { status = "FAIL"; copystring(reason, "oldcompile"); }
    else if(!fbook) { status = "FAIL"; copystring(reason, "fbo"); }
    else
    {
        loopi(numseeds)
        {
            benchinputs in;
            prepareinputs(live->program, i + 1, in);
            vector<float> a, b;
            benchrender(fbo, old, live->program, i + 1, in, a);
            benchrender(fbo, live->program, live->program, i + 1, in, b);
            int written = 0;
            benchcompare(a, b, maxerr, written);
            bestcov = max(bestcov, written*100/(BENCHSIZE*BENCHSIZE));
            if(in.reason[0] && !reason[0]) copystring(reason, in.reason);
        }
        if(maxerr > BENCHTOLERANCE) status = "FAIL";
        else if(reason[0]) status = "WEAK";
        else if(bestcov < 50) { status = "WEAK"; copystring(reason, "coverage"); }
    }

    if(old) glDeleteProgram_(old);
    glDeleteTextures(BENCHTARGETS, targets);
    glDeleteFramebuffers_(1, &fbo);
    if(vao) glDeleteVertexArrays_(1, &vao);
    state.restore();
    reportbench(name, status, maxerr, bestcov, numseeds, reason);
});
```

Checks while building:
- `isnan`/`isinf` come from `<cmath>` through `cube.h`. With `-ffast-math -fno-finite-math-only` in release builds they keep NaN semantics.
- `glDeleteVertexArrays_`, `glTexImage3D_`, `glUniform*uiv_` are existing engine entry points. If any is missing from `glexts.h`, add it the way Task 3 did.

- [ ] **Step 4: Build and run the test**

Run: `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`, restart the harness, then run `powershell -NoProfile -File tools\harness\tests\task7-bench.ps1`. Read the `task7` PNG it names.
Expected:
- `task 7: all checks passed`.
- The screenshot shows the normal menu. A black or garbled menu means the GL state restore is incomplete.

- [ ] **Step 5: Commit**

```bash
git add src/engine/shaderharness.cpp tools/harness/tests/task7-bench.ps1
git commit -m "shader: add pixel differential bench for the equivalence harness"
```

---

### Task 8: `shaders.ps1 check` and `diff`

**Files:**
- Modify: `tools/harness/shaders.ps1` (replace the two `throw` placeholders in the `switch`; add functions above it)
- Create: `tools/harness/tests/task8-check.ps1`

**Interfaces:**
- Consumes: Tasks 4–7.
- Produces:
  - `shaders.ps1 check [-Run baseline] [-Sids ...] [-NoMaps] [-Filter glob] [-MaxTier 0-3] [-Seeds n] [-PassThru]`. It prints one `Format-Result` line per configuration and a `Format-Summary` line, then exits with `Get-CheckExitCode`. With `-PassThru` it emits the result objects and does not exit.
  - `shaders.ps1 diff <name> [-Sid s00] [-Run baseline]`: prints the hashes and origins, the contract diff, and a unified diff of the normalized vs/fs sources.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/tests/task8-check.ps1`:

```powershell
# Task 8 verification: an unchanged build checks clean against itself.
$ErrorActionPreference = 'Stop'
$shaders = Join-Path $PSScriptRoot '..\shaders.ps1'

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

& $shaders record -Run t8 -Sids s00 -NoMaps
Assert-That 'record exited cleanly' ($LASTEXITCODE -eq 0)

$res = @(& $shaders check -Run t8 -Sids s00 -NoMaps -PassThru)
Assert-That 'check produced results' ($res.Count -gt 300)
Assert-That 'every configuration is hash-identical' (@($res | Where-Object { $_.Status -cne 'PASS-TEXT' }).Count -eq 0)

$text = @(& $shaders check -Run t8 -Sids s00 -NoMaps -Filter 'hud*')
Assert-That 'check exits 0 when clean' ($LASTEXITCODE -eq 0)
Assert-That 'a summary line is printed' (@($text | Where-Object { $_ -like '== * configs:*' }).Count -eq 1)

$diff = @(& $shaders diff hud -Run t8 -Sid s00)
Assert-That 'diff shows the contract is identical' (@($diff | Where-Object { $_ -like '*contract: identical*' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 8: all checks passed' -ForegroundColor Green
```

- [ ] **Step 2: Run to verify it fails**

Run (harness started): `powershell -NoProfile -File tools\harness\tests\task8-check.ps1`
Expected: FAIL with `check is implemented in Task 8.`

- [ ] **Step 3: Implement**

Add these functions to `shaders.ps1` after `Invoke-Record`:

```powershell
function Invoke-Wsl([string[]]$Arguments) {
    $out = @(& wsl -d Ubuntu --exec @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "wsl $($Arguments -join ' ') failed:`n$($out -join "`n")" }
    return , $out
}

# Tiers 1-2 for every PENDING result; returns index -> { Tier; Detail }.
function Invoke-OfflineTiers($Pending, [string]$BaseDir, [string]$CandDir) {
    $map = @{}
    if (-not @($Pending).Count) { return $map }
    $pairs = Join-Path $CorpusRoot 'pairs.tsv'
    $lines = for ($i = 0; $i -lt $Pending.Count; $i++) {
        $p = $Pending[$i]
        "$i`t$(ConvertTo-WslPath (Join-Path $BaseDir "blobs\$($p.BaseHash)"))`t$(ConvertTo-WslPath (Join-Path $CandDir "blobs\$($p.CandHash)"))"
    }
    Write-TextNoBom $pairs ((@($lines) -join "`n") + "`n")
    $out = Invoke-Wsl @('python3', (ConvertTo-WslPath (Join-Path $PSScriptRoot 'shadercheck.py')), '--pairs', (ConvertTo-WslPath $pairs))
    foreach ($line in $out) {
        $f = $line -split "`t", 3
        if ($f.Count -ge 2 -and $f[0] -match '^\d+$') {
            $detail = ''
            if ($f.Count -gt 2) { $detail = $f[2] }
            $map[[int]$f[0]] = [pscustomobject]@{ Tier = $f[1]; Detail = $detail }
        }
    }
    return $map
}

# Tier 3 for the queued results, grouped by settings point; returns
# "<sid><TAB><name>" -> { Status; Detail }.
function Invoke-Benches($Queue, [string]$BaseRun, $Points) {
    $results = New-OrdinalMap
    foreach ($group in @($Queue | Group-Object Sid)) {
        Enter-Point $group.Name $Points
        Invoke-Checked 'shaderforceall' 1 300 | Out-Null
        $items = @($group.Group)
        for ($i = 0; $i -lt $items.Count; $i += 20) {
            $chunk = @($items[$i..([Math]::Min($i + 19, $items.Count - 1))])
            $script = ($chunk | ForEach-Object { "shaderbench $BaseRun $($_.BaseHash) ""$($_.Name)"" $Seeds" }) -join "`n"
            foreach ($line in (Invoke-Batch $script 1 600)) {
                if ($line -match 'SHADERBENCH (\S+) (PASS|FAIL|WEAK) (.*)$') {
                    $status = $Matches[2]
                    if ($status -ceq 'PASS') { $status = 'PASS-PIXEL' }
                    $results["$($group.Name)`t$($Matches[1])"] = [pscustomobject]@{ Status = $status; Detail = $Matches[3] }
                }
            }
        }
    }
    return $results
}

function Invoke-Check {
    $baseDir = Join-Path $CorpusRoot $Run
    $candDir = Join-Path $CorpusRoot 'candidate'
    if (-not (Test-Path (Join-Path $baseDir 'manifest.tsv'))) { throw "No baseline corpus at $baseDir. Record one first: tools\harness\shaders.ps1 record" }

    # Replay exactly what the baseline recorded, not the current sweep file.
    $info = Read-RunInfo $baseDir
    $points = @(Read-RunPoints $baseDir)
    if ($Sids) { $points = @($points | Where-Object { $Sids -ccontains $_.Id }) }
    $mapList = @()
    if (-not $NoMaps) {
        $mapList = @($info['maps'] -split ' ' | Where-Object { $_ })
        if ($Sids) { $mapList = @($mapList | Where-Object { $Sids -ccontains "m-$_" }) }
    }
    $script:Map = $info['map']
    $recordProblems = Invoke-Record 'candidate' $points $mapList
    if ($recordProblems) { Write-Warning 'The candidate recording reported leaks or palette gaps (above).' }

    $wanted = @($points | ForEach-Object { $_.Id }) + @($mapList | ForEach-Object { "m-$_" })
    $baseGl = [System.IO.File]::ReadAllText((Join-Path $baseDir 'gl.txt'))
    $candGl = [System.IO.File]::ReadAllText((Join-Path $candDir 'gl.txt'))
    $sameGpu = $baseGl -ceq $candGl
    $maxTier = $MaxTier
    if (-not $sameGpu) {
        Write-Warning "The baseline was recorded on a different GPU/driver:`n$baseGl`nContract and pixel tiers are skipped; only text and SPIR-V run."
        $maxTier = [Math]::Min($maxTier, 2)
    }

    $results = @(Compare-Corpus -BaseDir $baseDir -CandDir $candDir -Filter $Filter -Sids $wanted -SkipContract:(-not $sameGpu))
    $pending = @($results | Where-Object { $_.Status -ceq 'PENDING' })
    $offline = @{}
    if ($maxTier -ge 1) { $offline = Invoke-OfflineTiers $pending $baseDir $candDir }
    $queue = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $pending.Count; $i++) {
        $r = $pending[$i]
        $o = $offline[$i]
        if ($o -and $o.Tier -ceq 'TEXT') { $r.Status = 'PASS-TEXT'; $r.Detail = 'same tokens after preprocessing'; continue }
        if ($o -and $o.Tier -ceq 'SPIRV' -and $maxTier -ge 2) { $r.Status = 'PASS-SPIRV'; $r.Detail = ''; continue }
        if ($maxTier -ge 3) { $queue.Add($r); continue }
        $r.Status = 'FAIL'
        if ($o) { $r.Detail = "tier $($o.Tier): $($o.Detail)" } else { $r.Detail = 'content differs (text tiers disabled)' }
    }
    if ($queue.Count) {
        $bench = Invoke-Benches $queue $Run $points
        foreach ($r in $queue) {
            $b = $bench["$($r.Sid)`t$($r.Name)"]
            if ($b) { $r.Status = $b.Status; $r.Detail = $b.Detail }
            else { $r.Status = 'FAIL'; $r.Detail = 'bench did not report' }
        }
    }

    $order = @{ 'FAIL' = 0; 'MISSING' = 1; 'WEAK' = 2; 'EXTRA' = 3; 'PASS-PIXEL' = 4; 'PASS-SPIRV' = 5; 'PASS-TEXT' = 6 }
    return , @($results | Sort-Object @{ Expression = { $order[$_.Status] } }, Sid, Name)
}

function Invoke-Diff([string]$ShaderName, [string]$PointId) {
    if (-not $ShaderName) { throw 'Usage: shaders.ps1 diff <name> [-Sid s00]' }
    $baseDir = Join-Path $CorpusRoot $Run
    $candDir = Join-Path $CorpusRoot 'candidate'
    $b = @(Read-Manifest $baseDir | Where-Object { $_.Name -ceq $ShaderName -and $_.Sid -ceq $PointId -and $_.Hash -cne '-' })
    $c = @(Read-Manifest $candDir | Where-Object { $_.Name -ceq $ShaderName -and $_.Sid -ceq $PointId -and $_.Hash -cne '-' })
    if (-not $b.Count) { throw "No valid baseline row for '$ShaderName' at $PointId in $baseDir." }
    if (-not $c.Count) { throw "No valid candidate row for '$ShaderName' at $PointId. Run 'shaders.ps1 check' first." }
    Write-Output "baseline   $($b[0].Hash)  origin $($b[0].Origin)"
    Write-Output "candidate  $($c[0].Hash)  origin $($c[0].Origin)"
    $bb = Join-Path $baseDir "blobs\$($b[0].Hash)"
    $cb = Join-Path $candDir "blobs\$($c[0].Hash)"
    $contract = Get-ContractDiff $bb $cb
    if ($contract.Count) { Write-Output '--- contract'; $contract | ForEach-Object { Write-Output "    $_" } }
    else { Write-Output '--- contract: identical' }
    foreach ($s in @(@('vs', 'vert'), @('fs', 'frag'))) {
        $files = foreach ($side in @(@('baseline', $bb), @('candidate', $cb))) {
            $norm = Invoke-Wsl @('python3', (ConvertTo-WslPath (Join-Path $PSScriptRoot 'shadercheck.py')), '--normalize', (ConvertTo-WslPath (Join-Path $side[1] "$($s[0]).full.glsl")), '--stage', $s[1])
            $path = Join-Path $CorpusRoot "diff-$($side[0]).$($s[0]).txt"
            Write-TextNoBom $path (($norm -join "`n") + "`n")
            $path
        }
        Write-Output "--- $($s[0]) (normalized)"
        & git --no-pager diff --no-index --no-color -U3 -- $files[0] $files[1]
        $global:LASTEXITCODE = 0
    }
}
```

Replace the two placeholder cases in the `switch`:

```powershell
    'check' {
        $results = Invoke-Check
        if ($PassThru) { return $results }
        foreach ($r in $results) { Write-Output (Format-Result $r) }
        Write-Output (Format-Summary $results)
        exit (Get-CheckExitCode $results)
    }
    'diff' { Invoke-Diff $Name $Sid }
```

`Invoke-Check` returns only the sorted result objects. Printing and the exit code live in the `switch`, so nothing the function writes can end up mixed into its return value. The record step inside it writes progress with `Write-Host`, which is not pipeline output. `Invoke-Record`'s return value is the problem count, captured into `$recordProblems`.

- [ ] **Step 4: Run the test**

Run (harness started): `powershell -NoProfile -File tools\harness\tests\task8-check.ps1`
Expected: `task 8: all checks passed`. Every configuration must be `PASS-TEXT`. Any other status here means recording is non-deterministic: run `shaders.ps1 diff <name> -Run t8` on it and fix the source of nondeterminism in the dump before moving on.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/shaders.ps1 tools/harness/tests/task8-check.ps1
git commit -m "harness: compare shader corpora through the tier ladder"
```

---

### Task 9: Acceptance self-test and docs

**Files:**
- Create: `tools/harness/shaders-selftest.ps1`
- Modify: `tools/harness/README.md` (new section), `CLAUDE.md` (UI test harness section, a short pointer)
- Modify: `doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md` (apply "Deviations from the spec")

**Interfaces:**
- Consumes: everything above.
- Produces: `tools\harness\shaders-selftest.ps1 [-KeepRunning]`, exit 0 when all acceptance checks pass.

- [ ] **Step 1: Write the self-test**

Create `tools/harness/shaders-selftest.ps1`:

```powershell
<#
.SYNOPSIS
    Acceptance tests for the shader equivalence harness (shaders.ps1).

.DESCRIPTION
    Records a one-point baseline, then checks deliberate mutations of real
    shader configs and expects each to land on the right tier:
      unchanged                   -> all PASS-TEXT          (determinism)
      a changed param default     -> FAIL, contract         (tier 0)
      a comment in hud            -> PASS-TEXT              (tier 1)
      a renamed local in hud      -> PASS-SPIRV/PASS-PIXEL  (tier 2, or 3 without glslang)
      a changed constant in hud   -> FAIL, pixel            (tier 3)
    Every mutation is reverted byte-for-byte in a finally block.

.EXAMPLE
    tools\harness\shaders-selftest.ps1
#>
[CmdletBinding()]
param([switch]$KeepRunning)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'shadercorpus.ps1')

$shaders = Join-Path $PSScriptRoot 'shaders.ps1'
$harness = Join-Path $PSScriptRoot 'harness.ps1'
$run = 'selftest-base'
$script:failures = 0
$script:step = 0

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch { Write-Host "     FAIL  $_" -ForegroundColor Red; $script:failures++ }
}

function Expect([string]$What, [bool]$Condition, [string]$Got) {
    if ($Condition) { Write-Host "     ok    $What" -ForegroundColor Green }
    else { Write-Host "     FAIL  $What -- got: $Got" -ForegroundColor Red; $script:failures++ }
}

function Check([string]$Filter) {
    return @(& $shaders check -Run $run -Sids s00 -NoMaps -Filter $Filter -PassThru)
}

function Result($Results, [string]$Name) {
    $r = @($Results | Where-Object { $_.Name -ceq $Name -and $_.Sid -ceq 's00' })
    if ($r.Count) { return $r[0] }
    return [pscustomobject]@{ Status = '(none)'; Detail = '' }
}

# Replaces the first occurrence of $Find in a repo file for the duration of
# $Body, then restores the original bytes exactly.
function Invoke-WithMutation([string]$RelPath, [string]$Find, [string]$Replace, [scriptblock]$Body) {
    $path = Join-Path $RepoRoot $RelPath
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $i = $text.IndexOf($Find, [StringComparison]::Ordinal)
    if ($i -lt 0) { throw "Mutation anchor not found in ${RelPath}: $Find" }
    Write-TextNoBom $path ($text.Substring(0, $i) + $Replace + $text.Substring($i + $Find.Length))
    try { & $Body }
    finally { [System.IO.File]::WriteAllBytes($path, $bytes) }
}

$hudLine = 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale;'

& $harness start 6>$null | Out-Null
try {
    Step 'record a one-point baseline' {
        & $shaders record -Run $run -Sids s00 -NoMaps
        Expect 'record reports no leaks or palette gaps' ($LASTEXITCODE -eq 0) "exit $LASTEXITCODE"
        $dir = Join-Path $HomeDir "shadercorpus\$run"
        $rows = @(Read-Manifest $dir)
        $gaps = @(Test-PaletteCoverage $rows ([System.IO.File]::ReadAllLines((Join-Path $dir 'registry.txt'))) @('s00'))
        Expect 'every registered world/decal shader is in the corpus' ($gaps.Count -eq 0) ($gaps -join ', ')
    }

    Step 'an unchanged build checks clean (determinism)' {
        $res = Check '*'
        $bad = @($res | Where-Object { $_.Status -cne 'PASS-TEXT' })
        Expect "all $($res.Count) configurations PASS-TEXT" ($res.Count -gt 300 -and $bad.Count -eq 0) (($bad | Select-Object -First 5 | ForEach-Object { Format-Result $_ }) -join ' | ')
    }

    Step 'a changed param default fails the contract' {
        Invoke-WithMutation 'config/glsl/world.cfg' 'defuniformparam "gloss" 1 // glossiness' 'defuniformparam "gloss" 2 // glossiness' {
            $r = Result (Check 'stdworld') 'stdworld'
            Expect 'stdworld FAIL naming gloss' ($r.Status -ceq 'FAIL' -and $r.Detail -like '*gloss*') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a comment is textually equivalent' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine ($hudLine + ' // harness selftest') {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud PASS-TEXT' ($r.Status -ceq 'PASS-TEXT') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a renamed local is equivalent past the text tier' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine 'vec4 texel = texture2D(tex0, texcoord0), color = texel * colorscale;' {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud PASS-SPIRV (or PASS-PIXEL without glslang)' ($r.Status -ceq 'PASS-SPIRV' -or $r.Status -ceq 'PASS-PIXEL') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a changed constant fails at the pixel tier' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale * 0.5;' {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud FAIL with a pixel error' ($r.Status -ceq 'FAIL' -and $r.Detail -match 'maxerr=') "$($r.Status) $($r.Detail)"
            & $shaders check -Run $run -Sids s00 -NoMaps -Filter 'hud' | Out-Null
            Expect 'check exits non-zero' ($LASTEXITCODE -ne 0) "exit $LASTEXITCODE"
        }
    }

    Step 'the mutated files are restored' {
        $dirty = @(& git -C $RepoRoot status --porcelain -- config/glsl/init.cfg config/glsl/world.cfg)
        Expect 'git sees no change' ($dirty.Count -eq 0) ($dirty -join ', ')
    }
}
finally {
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) { Write-Host "$script:failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'shader harness self-test: all checks passed' -ForegroundColor Green
exit 0
```

`the mutated files are restored` assumes those two files were clean when the test started. If `git status` shows them modified beforehand, the check fails for that reason alone. The test does not touch anything else.

- [ ] **Step 2: Run it**

Run: `tools\harness\shaders-selftest.ps1`
Expected: `shader harness self-test: all checks passed`.

Diagnose failures by symptom:
- **The constant-change step reports `PASS-PIXEL`:** the bench did not reach `hud`'s output (check `cov=`), or the multiply was folded away. Read `shaders.ps1 diff hud -Run selftest-base` while the mutation is applied: pause the test with `-KeepRunning` and re-apply the mutation by hand.
- **The rename step reports `FAIL`:** the pixel tier disagrees on a pure rename. That is a bench bug: identical code must render identical pixels. Fix the bench before trusting any `PASS-PIXEL`.

- [ ] **Step 3: Document**

Append a section to `tools/harness/README.md`:

````markdown
## Shader equivalence harness

`shaders.ps1` proves a shader refactor changed nothing, one configuration at a time.
Design: `doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md`.

```powershell
tools\harness\shaders.ps1 record                 # baseline: every setting in shader-sweep.txt x every shipped map
tools\harness\shaders.ps1 check -Filter 'bump*'  # after an edit: record a candidate, compare
tools\harness\shaders.ps1 check -Sids s00 -NoMaps  # fast loop: defaults only
tools\harness\shaders.ps1 diff bumpworld -Sid s00  # why one configuration differs
tools\harness\shaders-selftest.ps1               # acceptance tests for the harness itself
```

A configuration is `(shader name, settings id)`. Variants are named `<variant:col,row>parent`.
Corpora live in `home\uitest\shadercorpus\<run>\` (`manifest.tsv`, `blobs\<hash>\`,
`settings\`, `registry.txt`, `gl.txt`, `run.txt`).

| Status | Meaning |
|---|---|
| `PASS-TEXT` | Hash-identical, or the same tokens after preprocessing |
| `PASS-SPIRV` | Same SPIR-V after `spirv-opt -O` and `spirv-remap` |
| `PASS-PIXEL` | Same RGBA32F output on seeded inputs: the weakest evidence, so review it |
| `WEAK` | The bench could not exercise it (coverage < 50%, or an input type it cannot feed) |
| `FAIL` | Contract mismatch (metadata/reflection), or pixels differ |
| `MISSING` / `EXTRA` | Valid in only one of the two corpora |

- Recording and the pixel tier need the running game. Tiers 1–2 need `glslang-tools` and
  `spirv-tools` in the `Ubuntu` WSL instance; without them those tiers report `NA` and fall
  through to pixels.
- A corpus is only comparable on the GPU/driver that recorded it (`gl.txt`). On another,
  `check` runs the text and SPIR-V tiers only.
- `record` fails if a shader differs between `s00` and `s99` (both defaults): state leaked
  between sweep points. It also fails if a registered world/decal shader is not valid at
  every point.
- **Adding a generator input:** when a `config/glsl` generator starts reading a new var,
  add a vector for it to `shader-sweep.txt` and re-record the baseline.
````

Add to `CLAUDE.md`, directly before the `### Test-only commands` heading:

````markdown
### Shader equivalence harness

`tools/harness/shaders.ps1 record|check|diff` records every shader configuration (composed
GLSL, metadata, GL reflection) across a settings sweep and compares a candidate build against
a baseline: contract → text → SPIR-V → pixel. Engine side: `shaderdumpall`, `shaderbench`,
`shaderorigin`, `shaderforceall` in `src/engine/shaderharness.cpp` / `shader.cpp`
(`DEBUG_UTILS`). See `tools/harness/README.md`.
````

- [ ] **Step 4: Update the spec**

In `doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md`, apply the five items under "Deviations from the spec" at the top of this plan:
- corpus location `home/uitest/shadercorpus/`
- manifest columns
- invalid rows treated as absent
- the `s00`/`s99` leak check replacing the "same key, two hashes" wording
- `run.txt`/`gl.txt`
- acceptance test 5 moved to the first porting plan

- [ ] **Step 5: Run every test once more**

```powershell
powershell -NoProfile -File tools\harness\tests\shadercorpus.tests.ps1
wsl -d Ubuntu --exec python3 -m unittest discover -s "/mnt/f/Red Eclipse/tools/harness/tests" -p "test_shadercheck.py"
tools\harness\shaders-selftest.ps1
tools\harness\texslot-selftest.ps1
```
Expected: all pass. The texture slot self-test is the regression guard for Task 2's change to `shader.cpp`, since slot shaders go through `setshader`/`changeslotshader`.

- [ ] **Step 6: Record the real baseline**

On a clean `config/glsl` tree at the commit that contains this plan's work:

```powershell
tools\harness\shaders.ps1 record
```

Expected: exit 0, no leaks, no palette gaps. Note how long it took; `README.md` can quote it. This baseline is what the first porting plan checks against.

- [ ] **Step 7: Commit**

```bash
git add tools/harness/shaders-selftest.ps1 tools/harness/README.md doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md
git commit -m "harness: add shader equivalence self-test and docs"
```

`CLAUDE.md` is gitignored; its edit stays local.
