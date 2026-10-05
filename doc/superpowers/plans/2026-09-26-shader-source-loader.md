# Shader Source Loader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add CubeScript commands that build a shader from `#define`s, include files and `.vert`/`.frag` files under `config/glsl/`, so later plans can replace CubeScript-generated GLSL one family at a time. This plan changes no shader.

**Architecture:** A new `src/engine/shadersource.{h,cpp}` holds pure text helpers (path and define validation, stage assembly) and the command glue. `shader_new`/`variantshader_new` run their CubeScript body against a per-call build record, read and assemble the files, and then call the **existing** `shader()`/`variantshader()` with the assembled text. Everything downstream is untouched: fog and uniform injection, generic variants, `newshader`, `origin` tracking, `composeglslparts` and the equivalence harness dump.

**Tech Stack:** C++ (Tesseract-derived engine: `vector`, `loopv`, `conoutf`, `ICOMMAND`), CubeScript, PowerShell 5.1 harness, WSL cross-compile to MSVC.

**Spec:** There is no separate design document. The design and its reasoning are in §Design below. The verification contract is the shader equivalence harness: [doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md](../specs/2026-09-25-shader-equivalence-harness-design.md). Background: [doc/agent-handoff.md](../../agent-handoff.md) §3 and §6 item 1.

## Global Constraints

- **Behaviour-preserving.** No file under `config/glsl/` changes in this plan. The final gate is a full `shaders.ps1 check` against `home/uitest/shadercorpus/baseline/` (commit `f250748d`, RTX 3080, driver 591.86) in which **every** result is `PASS-TEXT` with `BaseHash == CandHash`.
- **Do not edit** `composeglslparts`, the `origin`/`generated` tracking (`shaderorigin`, `shaderorigingenerated`, `newshader`'s origin block), `slotparamsscope`, `genuniformdefs`, `genfogshader`, `findglslmain`, `gengenericvariant`, or the bodies of `shader()`/`variantshader()` in `src/engine/shader.cpp`. The loader wraps them.
- **Never delete `home/uitest/shadercorpus/baseline/`.** Re-recording takes about an hour.
- **Build** from PowerShell, never Git Bash: `wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug`. **Stop the harness first** (`tools\harness\harness.ps1 stop`): the running game locks `bin/amd64/redeclipse.exe`. The last build was `debug`, so this is incremental.
- **C++ style** matches `src/engine/shader.cpp`: 4-space indent, Tesseract containers (`vector`, `loopv`, `loopi`), `NULL`, `conoutf(colourred, ...)` for errors, `defformatstring`/`copystring`, and short comments that say why.
- **CubeScript in tests:** no bare `#`, no `@`. Use `concatword` to build strings (CLAUDE.md "CubeScript traps").
- **PowerShell 5.1:** no `&&`, `||`, ternary or `??`. Write files CubeScript reads with `Write-TextNoBom` (`tools/harness/core.ps1`).
- **Git:**
  - Work on branch `shader-source-loader`, created from `master` at `f250748d`.
  - Commit messages are lowercase `area: summary`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  - Stage explicit paths only; **never** `git add -A`. The working tree has unrelated user files (`readme.md`, `chat_wip.cfg`, `deli.zip`, …) that must stay untouched.
  - Never push. Fast-forward into `master` only when the user asks.
- **Test-only commands stay test-only.** The loader commands are production code, not `DEBUG_UTILS`. Tests use the existing `shaderdumpall` harness command.

---

## Design

### Commands

```cubescript
shader_new <type> <name> [ body ]
variantshader_new <type> <name> <row> <maxvariants> [ body ]

// Only valid inside a body:
shader_define <NAME> [value]           // "#define NAME value" (or "#define NAME")
shader_include_vs <config/glsl/...>    // text inserted before the vertex source
shader_include_fs <config/glsl/...>    // text inserted before the fragment source
shader_source <vs path> <fs path>      // either may be "" (no file for that stage)
```

Example, as the AO plan will use it:

```cubescript
shader_new $SHADER_DEFAULT "linearizedepth" [
    shader_define AO_DEPTH_FORMAT $aodepthformat
    shader_include_fs "config/glsl/shared/gdepth.glsl"
    shader_source "config/glsl/ao/linearizedepth.vert" "config/glsl/ao/linearizedepth.frag"
]
```

### Assembly

Each stage is its defines (in call order), then each include for that stage (in call order), then the stage's source file. Two normalisations apply:
- Every `'\r'` is dropped. The repo uses `* text=auto` and `core.autocrlf=true`, so on Windows `.frag` files are checked out with CRLF. Without this, the same file would assemble to different bytes on different machines.
- Each piece ends with `'\n'`; one is added if the file doesn't end with one.

Defines go into every stage that has a source file. A stage with no source file assembles to `""`: for `variantshader_new` that means "reuse the parent's stage", which is existing `newshader` behaviour. For `shader_new` it is an error.

The assembled text then goes through `shader()`/`variantshader()` unchanged, so the post-processing still happens after assembly:
- `//:fog` and `//:variant` pragmas inside files still work;
- texture-slot params still become `uniform vec4` declarations.

`composeglslparts` later puts the `#version` header and the compat macros *before* this text, so a `shader_define` value may use `texture2DRect` and similar.

### Behaviour

| Case | Behaviour |
|---|---|
| `shader_new` for a name that is already loaded | Returns at once. The body does not run and no file is read. `shader()` would have kept the existing shader anyway. |
| `variantshader_new` with `row < 0` | Same as `shader_new` (mirrors `variantshader`). |
| `variantshader_new` with `row >= MAXVARIANTROWS` or no loaded parent | Returns at once, without running the body (`variantshader` drops these too). |
| A command inside a body fails (bad path, bad define) | Logs the reason and marks the build failed. At the end: "shader `<name>`: not created", and no shader is made. |
| A file cannot be read | Logs "shader `<name>`: cannot read `<path>`"; no shader is made. |
| Includes for a stage with no source | Error; no shader is made. |
| `shader_new` with an empty vertex or fragment source | Error; no shader is made. |
| `shader_define`/`shader_include_*`/`shader_source` outside a body | Logs "only valid inside a shader_new or variantshader_new body" and does nothing. |
| Nested bodies (a body that runs another `shader_new`, e.g. through a generator) | Each call has its own build record, and the outer one is restored afterwards. Defines don't leak between them. |
| `shader_source` called twice | The last call wins. |
| Paths | Must start with `config/glsl/`, use forward slashes, contain no `\` or `:`, and have no empty, `.` or `..` component. Resolved through `loadfile` → `findfile` (home dir first, then packages), like `exec`. |
| Define names | Must match `[A-Za-z_][A-Za-z0-9_]*`. Values must not contain `\r` or `\n`. |

**Maps.** The loader does not refuse `IDF_MAP`, which keeps parity with the existing `shader` command that maps can already call.
- The path rule is what guards file access: a map can only read shipped GLSL.
- `shader_new` never creates a `mapdef` shader, so `savemapshaders` never writes assembled text into a map.
- Maps keep `mapshader` for their own inline shaders.

### Where this departs from libprimis#264 (a hint, not a blueprint)

[project-imprimis/libprimis#264](https://github.com/project-imprimis/libprimis/pull/264) is the model for the command names and the defines → includes → file assembly order. Deliberate differences:

| libprimis#264 | Here | Why |
|---|---|---|
| `shader_new` copies `shader()`'s body | Calls `shader()`/`variantshader()` | A copy drops Red Eclipse's `overwrite`/`mapdef` handling, the `progress()` text and the `slotparams` path. Wrapping keeps one code path, so the harness proves equivalence once. |
| Rewrites `genfogshader`/`genuniformdefs`/`findglslmain` over `std::string` | Not touched | That rewrite drops the `//:fog <colour>` and `//:fog rgba` pragma handling. Its `findglslmain` loop (`size_t main >= 0`) never ends at 0 and underflows. The loader doesn't need either change. |
| A missing file logs an error, then still compiles the empty text | The build fails and no shader is made | An empty shader that compiles is worse than a missing one: `generateshader` falls back to `nullshader` for a missing one. |
| `loadfile` buffers are never freed | Freed | — |
| Commands outside a body silently do nothing | They log an error | A typo in a generator should show up in `log.txt`, which the harness reads. |
| Any path | `config/glsl/` only | `loadfile` would otherwise read anything, and the text could end up in `dumpshader` output or compile errors. |
| One global build state | A stack of build records | Generators can nest. |
| `shader_get_defines`/`shader_get_includes_vs`/`_fs` | Not added | Nothing reads them yet. Add them in the port that needs them. |
| `ao.cpp`: removes `aoderivnormal`, changes `debugao` | Not taken | These are behaviour changes. The baseline covers `aoderivnormal` (s39, s40, s42, s44). |

### Out of scope, for the family ports

- A `lazyshader` equivalent (`defershader` around `shader_new`), a one-line alias in `config/glsl/shared.cfg`. The AA port needs it (`tqaaresolve` is a `lazyshader`); AO does not.
- `"row, col"` reuse strings for `variantshader_new` stages (world/model ports).
- `.gitattributes` `eol=lf` for `*.vert`/`*.frag`/`*.glsl`. The loader normalises CRLF regardless; add it with the first content port.
- An `origin` that names the source files, and `#line` markers at include boundaries.
- **Caveat for content authors (documented in Task 2):** `genuniformdefs` and `genfogshader` insert declarations at the line holding the *first* occurrence of the text `main` (`findglslmain`). An include containing `main` inside a function body (say `domain`, or a comment) before the real `main` would get declarations inserted in the wrong place.

## File Structure

| File | Responsibility |
|---|---|
| `src/engine/shadersource.h` (new) | `namespace shadersource`: pure helpers `validpath`, `validdefine`, `appenddefine`, `appendtext`, `assemblestage` |
| `src/engine/shadersource.cpp` (new) | Those helpers; the build record; the commands `shader_new`, `variantshader_new`, `shader_define`, `shader_include_vs`, `shader_include_fs`, `shader_source` |
| `src/tests/shadersource.cpp` (new) | Boot-time unit test of the pure helpers (debug builds) |
| `src/engine/texture.h` | Declare `variantshader()`, already defined non-static in `shader.cpp` |
| `src/engine/main.cpp` | Run `testshadersource()` with the other unit tests |
| `src/Makefile` | Add `engine/shadersource.o`, `tests/shadersource.o` and the header dependencies. `src/CMakeLists.txt` globs `engine/*.cpp` and needs nothing. |
| `tools/harness/tests/shadersource.ps1` (new) | Live test against a running game |
| `doc/shader-reference.md` | New section "Shader source files" |
| `doc/agent-handoff.md` | State and work-queue update (untracked, not committed) |

---

### Task 0: Branch

- [ ] **Step 1: Create the branch**

```powershell
git status --short
git switch -c shader-source-loader
git log --oneline -1
```

Expected: `git log` shows `f250748d harness: sweep aofloatdepth`. `git status` lists only the user's unrelated files (`M readme.md`, `?? chat_wip.cfg`, …).

---

### Task 1: Pure assembly helpers

**Files:**
- Create: `src/engine/shadersource.h`
- Create: `src/engine/shadersource.cpp`
- Create: `src/tests/shadersource.cpp`
- Modify: `src/engine/main.cpp` (the `#ifdef _DEBUG` unit test block, after `testshaderharness();`)
- Modify: `src/Makefile` (object list next to `engine/shaderharness.o`, test list next to `tests/shaderharness.o`, and the dependency lines next to `engine/shaderharness.o: ...`)

**Interfaces:**
- Produces (used by Tasks 2 and 3):
  - `bool shadersource::validpath(const char *path)`
  - `bool shadersource::validdefine(const char *name, const char *value)`
  - `void shadersource::appenddefine(vector<char> &out, const char *name, const char *value)`
  - `void shadersource::appendtext(vector<char> &out, const char *text)`
  - `void shadersource::assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body)`: `out` is reset and NUL-terminated.

- [ ] **Step 1: Write the header**

`src/engine/shadersource.h`:

```cpp
// Shader source loader: shader_new and variantshader_new build a shader from
// #defines, include files and .vert/.frag files under config/glsl/, then hand
// the assembled text to shader()/variantshader(). See doc/shader-reference.md,
// "Shader source files".
#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

namespace shadersource
{
    // A path shader_source and shader_include_* may read: under config/glsl/,
    // forward slashes only, and no empty, "." or ".." component.
    bool validpath(const char *path);
    // A macro name ([A-Za-z_][A-Za-z0-9_]*) and a value that fits on one line.
    bool validdefine(const char *name, const char *value);
    // "#define <name> <value>\n", or "#define <name>\n" when value is empty.
    void appenddefine(vector<char> &out, const char *name, const char *value);
    // Appends text without its '\r's, then '\n' unless it already ends with
    // one, so a CRLF checkout assembles to the same bytes as an LF one.
    void appendtext(vector<char> &out, const char *text);
    // One stage, NUL-terminated: the defines, each include, then the body. A
    // NULL body (no file for this stage) gives "", which variantshader reads
    // as "reuse the parent's stage".
    void assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body);
}

#endif
```

- [ ] **Step 2: Write the failing unit test**

`src/tests/shadersource.cpp`:

```cpp
#include "engine.h"
#include "shadersource.h"

// out holds exactly expected and its terminator.
static bool assembled(const vector<char> &out, const char *expected)
{
    return out.length() == int(strlen(expected)) + 1 && !strcmp(out.getbuf(), expected);
}

// shader_new loader helpers, see engine/shadersource.h.
void testshadersource()
{
    using namespace shadersource;

    ASSERT(validpath("config/glsl/ao/ao.frag"));
    ASSERT(validpath("config/glsl/x.vert"));
    ASSERT(validpath("config/glsl/..x.frag")); // a dotted name, not a ".." component
    ASSERT(!validpath("config/glsl/"));
    ASSERT(!validpath("config/glsl"));
    ASSERT(!validpath("config/glslx/a.frag"));
    ASSERT(!validpath("data/x.frag"));
    ASSERT(!validpath("/config/glsl/x.frag"));
    ASSERT(!validpath("C:/config/glsl/x.frag"));
    ASSERT(!validpath("config/glsl/../autoexec.cfg"));
    ASSERT(!validpath("config/glsl/ao/../../x.frag"));
    ASSERT(!validpath("config/glsl/./x.frag"));
    ASSERT(!validpath("config/glsl//x.frag"));
    ASSERT(!validpath("config/glsl/ao/"));
    ASSERT(!validpath("config/glsl/ao\\x.frag"));
    ASSERT(!validpath("config/glsl/c:x.frag"));

    ASSERT(validdefine("AO_TAPS", "12"));
    ASSERT(validdefine("_x1", ""));
    ASSERT(validdefine("TAPVEC", "vec2(i, 0.0)"));
    ASSERT(!validdefine("", "1"));
    ASSERT(!validdefine("1X", "1"));
    ASSERT(!validdefine("A-B", "1"));
    ASSERT(!validdefine("A(x)", "x"));
    ASSERT(!validdefine("A", "1\n#define B 2"));
    ASSERT(!validdefine("A", "1\r"));

    vector<char> defs;
    appenddefine(defs, "AO_TAPS", "12");
    appenddefine(defs, "AO_FLAG", "");
    vector<char> shown;
    shown.put(defs.getbuf(), defs.length());
    shown.add('\0');
    ASSERT(!strcmp(shown.getbuf(), "#define AO_TAPS 12\n#define AO_FLAG\n"));

    vector<char> text;
    appendtext(text, "a\r\nb\r\n");
    text.add('\0');
    ASSERT(!strcmp(text.getbuf(), "a\nb\n"));
    text.setsize(0);
    appendtext(text, "no newline");
    text.add('\0');
    ASSERT(!strcmp(text.getbuf(), "no newline\n"));
    text.setsize(0);
    appendtext(text, "");
    ASSERT(text.empty());

    vector<const char *> includes;
    includes.add("inc1\r\n");
    includes.add("inc2");
    vector<char> out;
    assemblestage(out, defs, includes, "body\n");
    ASSERT(assembled(out, "#define AO_TAPS 12\n#define AO_FLAG\ninc1\ninc2\nbody\n"));
    assemblestage(out, defs, includes, NULL);
    ASSERT(assembled(out, ""));
    vector<char> nodefs;
    vector<const char *> noincludes;
    assemblestage(out, nodefs, noincludes, "body");
    ASSERT(assembled(out, "body\n"));

    conoutf(colourwhite, "testshadersource: ok");
}
```

Register it in `src/engine/main.cpp`, directly after the existing `testshaderharness();` line in the `#ifdef _DEBUG` block:

```cpp
        extern void testshadersource();
        testshadersource();
```

Register the objects in `src/Makefile`:
- After the line `    engine/shaderharness.o \`, add `    engine/shadersource.o \`.
- After the line `    CLIENT_OBJS += tests/shaderharness.o`, add `    CLIENT_OBJS += tests/shadersource.o`.
- After the line `tests/shaderharness.o: engine/shaderharness.h`, add:

```make
engine/shadersource.o: engine/shadersource.h
tests/shadersource.o: engine/shadersource.h
```

Create `src/engine/shadersource.cpp` with only the include lines for now, so the build reaches the link step:

```cpp
// shadersource.cpp: the shader_new loader, see shadersource.h.

#include "engine.h"
#include "shadersource.h"
```

- [ ] **Step 3: Build and confirm it fails**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
```

Expected: the link fails with unresolved externals for `shadersource::validpath`, `validdefine`, `appenddefine`, `appendtext` and `assemblestage`.

- [ ] **Step 4: Implement the helpers**

Append to `src/engine/shadersource.cpp`:

```cpp
namespace shadersource
{
    bool validpath(const char *path)
    {
        static const char prefix[] = "config/glsl/";
        static const int prefixlen = sizeof(prefix) - 1;
        if(strncmp(path, prefix, prefixlen) || strpbrk(path, "\\:")) return false;
        for(const char *seg = path + prefixlen;;)
        {
            const char *end = strchr(seg, '/');
            int len = end ? int(end - seg) : int(strlen(seg));
            if(!len || (len == 1 && seg[0] == '.') || (len == 2 && seg[0] == '.' && seg[1] == '.')) return false;
            if(!end) return true;
            seg = end + 1;
        }
    }

    bool validdefine(const char *name, const char *value)
    {
        if(!isalpha(uchar(name[0])) && name[0] != '_') return false;
        for(const char *c = name + 1; *c; c++) if(!isalnum(uchar(*c)) && *c != '_') return false;
        return !strpbrk(value, "\r\n");
    }

    void appenddefine(vector<char> &out, const char *name, const char *value)
    {
        out.put("#define ", 8);
        out.put(name, strlen(name));
        if(*value)
        {
            out.add(' ');
            out.put(value, strlen(value));
        }
        out.add('\n');
    }

    void appendtext(vector<char> &out, const char *text)
    {
        char last = '\n';
        for(const char *c = text; *c; c++) if(*c != '\r')
        {
            out.add(*c);
            last = *c;
        }
        if(last != '\n') out.add('\n');
    }

    void assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body)
    {
        out.setsize(0);
        if(body)
        {
            if(defines.length()) out.put(defines.getbuf(), defines.length());
            loopv(includes) appendtext(out, includes[i]);
            appendtext(out, body);
        }
        out.add('\0');
    }
}
```

- [ ] **Step 5: Build and confirm the unit test passes**

```powershell
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
tools\harness\harness.ps1 start
Select-String -Path home\uitest\log.txt -Pattern 'Assert|assert|backtrace' -SimpleMatch
```

Expected:
- The build succeeds.
- `start` prints `Ready.`
- `Select-String` prints nothing.

The unit tests run before the log opens, so a pass leaves no `ok` line. Under `RE_CRASHLOG=1`, a failed `ASSERT` aborts startup, `start` throws `Game exited ...`, and `log.txt` gets a backtrace. Then:

```powershell
tools\harness\harness.ps1 stop
```

- [ ] **Step 6: Commit**

```powershell
git add src/engine/shadersource.h src/engine/shadersource.cpp src/tests/shadersource.cpp src/engine/main.cpp src/Makefile
git commit -m @'
engine: add shader source assembly helpers

Pure helpers for the shader_new loader: path and define validation, and
assembling a stage from defines, includes and a source file. CRs are
dropped so a CRLF checkout assembles to the same bytes as an LF one.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 2: `shader_new` and the body commands

**Files:**
- Modify: `src/engine/shadersource.cpp` (append)
- Create: `tools/harness/tests/shadersource.ps1`
- Modify: `doc/shader-reference.md` (new section after "### Shader Utilities and Includes", before "### Shader Parameter Binding")

**Interfaces:**
- Consumes: Task 1's `shadersource::*`. From `texture.h`: `Shader *lookupshaderbyname(const char *)`, `Shader *shader(int type, char *name, char *vs, char *ps, bool mapdef = false, bool overwrite = false)`. From `tools.h`: `char *loadfile(const char *fn, size_t *size, bool utf8 = true)`. From `command.cpp`: `int execute(const uint *code)`.
- Produces (used by Task 3):
  - `struct shaderbuild`
  - `static bool runbuild(const char *name, uint *body, vector<char> &vs, vector<char> &ps)`
  - `static void shadernew(int type, char *name, uint *body)`
  - The CubeScript commands `shader_new` (`"ise"`), `shader_define` (`"ss"`), `shader_include_vs` / `shader_include_fs` (`"s"`) and `shader_source` (`"ss"`).
  - `tools/harness/tests/shadersource.ps1`, which ends with a `# Cleanup` section that Task 3 inserts before.

- [ ] **Step 1: Write the failing live test**

`tools/harness/tests/shadersource.ps1`:

```powershell
# shader_new loader: shader_new, shader_define, shader_include_vs/fs,
# shader_source (and variantshader_new, Task 3). Needs a running harness on a
# build with the loader (tools\harness\harness.ps1 start); no map required.
# Fixtures go to home\uitest\config\glsl\harness\ (findfile searches the home
# dir first) and are removed at the end. Shader names carry a per-run tag, so
# the script can be rerun in the same game session.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Test-Echo([string[]]$Lines, [string]$Expected) {
    return @($Lines | Where-Object { $_.Trim() -ceq $Expected }).Count -eq 1
}
function Test-Logged([string[]]$Lines, [string]$Text) {
    return @($Lines | Where-Object { $_.Contains($Text) }).Count -ge 1
}

$t = 'src' + (Get-Random -Maximum 1000000)
$fixDir = Join-Path $HomeDir 'config\glsl\harness'
$runDir = Join-Path $HomeDir 'shadercorpus\srcloader'
New-Item -ItemType Directory -Force $fixDir | Out-Null
Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue

$vert = "attribute vec4 vvertex;`nvoid main(void)`n{`n    gl_Position = vvertex;`n}`n"
$frag = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n#ifdef SRC_FLAG`n    #if SRC_MODE == 2`n    fragcolor = SRC_COLOR;`n    #endif`n#else`n    fragcolor = vec4(1.0);`n#endif`n}`n"
$fogfrag = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n    fragcolor = vec4(1.0);`n    //:fog`n}`n"
# CRLF and no final newline: the loader must drop the CRs and add the newline.
$common = "// shared`r`n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)"
Write-TextNoBom (Join-Path $fixDir 't.vert') $vert
Write-TextNoBom (Join-Path $fixDir 't.frag') $frag
Write-TextNoBom (Join-Path $fixDir 'fog.frag') $fogfrag
Write-TextNoBom (Join-Path $fixDir 'common.glsl') $common

$defs = "#define SRC_MODE 2`n#define SRC_FLAG`n"
$expectVs = $defs + $vert
$expectFs = $defs + "// shared`n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)`n" + $frag

# 'setshader null' clears leftover texture-slot params, which shader() would
# otherwise turn into extra uniform declarations and break the exact text
# comparisons below.
$make = @"
setshader null
shader_new 0 ${t}ok [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OK=" (hasshader ${t}ok))
shader_new 0 ${t}fog [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/fog.frag" ]
echo (concatword "SRC_FOG=" (hasshader ${t}fog))
setshaderparam srcparam 1 2 3 4
shader_new 0 ${t}param [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_PARAM=" (hasshader ${t}param))
shader_new 0 ${t}outer [
    shader_define SRC_MODE 2
    shader_new 0 ${t}inner [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OUTER=" (hasshader ${t}outer) " SRC_INNER=" (hasshader ${t}inner))
srcran = 0
shader_new 0 ${t}ok [ srcran = 1 ]
echo (concatword "SRC_RAN=" `$srcran)
"@
$made = @(Invoke-Batch $make 1 120)
Assert-That 'a shader from defines, an include and two files is created' (Test-Echo $made 'SRC_OK=1')
Assert-That 'a //:fog pragma inside a file still creates the shader' (Test-Echo $made 'SRC_FOG=1')
Assert-That 'a shader with a texture-slot param is created' (Test-Echo $made 'SRC_PARAM=1')
Assert-That 'nested shader_new bodies both create their shaders' (Test-Echo $made 'SRC_OUTER=1 SRC_INNER=1')
Assert-That 'the body of an existing shader is not run' (Test-Echo $made 'SRC_RAN=0')

$bad = @"
shader_new 0 ${t}missing [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/nope.frag" ]
echo (concatword "SRC_MISSING=" (hasshader ${t}missing))
shader_new 0 ${t}dotdot [ shader_source "config/glsl/../config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_DOTDOT=" (hasshader ${t}dotdot))
shader_new 0 ${t}outside [ shader_source "config/glsl/harness/t.vert" "data/x.frag" ]
echo (concatword "SRC_OUTSIDE=" (hasshader ${t}outside))
shader_new 0 ${t}badname [ shader_define "1BAD" 1; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADNAME=" (hasshader ${t}badname))
shader_new 0 ${t}badvalue [ shader_define SRC_X "1^n2"; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADVALUE=" (hasshader ${t}badvalue))
shader_new 0 ${t}onestage [ shader_source "config/glsl/harness/t.vert" "" ]
echo (concatword "SRC_ONESTAGE=" (hasshader ${t}onestage))
shader_new 0 ${t}orphan [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ORPHAN=" (hasshader ${t}orphan))
shader_define SRC_LOOSE 1
"@
$failed = @(Invoke-Batch $bad 1 120)
Assert-That 'a missing file creates no shader' (Test-Echo $failed 'SRC_MISSING=0')
Assert-That 'a missing file is named in the log' (Test-Logged $failed 'cannot read config/glsl/harness/nope.frag')
Assert-That 'a .. path is refused' (Test-Echo $failed 'SRC_DOTDOT=0')
Assert-That 'a path outside config/glsl is refused' (Test-Echo $failed 'SRC_OUTSIDE=0')
Assert-That 'refused paths are logged' (@($failed | Where-Object { $_.Contains('refusing') }).Count -eq 2)
Assert-That 'an invalid define name is refused' (Test-Echo $failed 'SRC_BADNAME=0')
Assert-That 'a define value with a newline is refused' (Test-Echo $failed 'SRC_BADVALUE=0')
Assert-That 'invalid defines are logged' (@($failed | Where-Object { $_.Contains('invalid define') }).Count -eq 2)
Assert-That 'shader_new needs both stages' ((Test-Echo $failed 'SRC_ONESTAGE=0') -and (Test-Logged $failed 'needs both a vertex and a fragment source'))
Assert-That 'includes without a source for that stage are refused' ((Test-Echo $failed 'SRC_ORPHAN=0') -and (Test-Logged $failed 'includes given but no vertex source'))
Assert-That 'shader_define outside a body is reported' (Test-Logged $failed 'only valid inside a shader_new or variantshader_new body')

$dump = @(Invoke-Batch 'shaderdumpall srcloader s00 0' 1 300)
Assert-That 'the dump ran' (@($dump | Where-Object { $_ -match 'SHADERDUMP srcloader s00 \d+ \d+' }).Count -eq 1)
$rows = [System.IO.File]::ReadAllLines((Join-Path $runDir 'manifest.tsv'))
function Get-Blob([string]$Name) {
    $row = @($rows | Where-Object { ($_ -split "`t")[0] -ceq $Name })
    if ($row.Count -ne 1) { return $null }
    return Join-Path $runDir ('blobs\' + ($row[0] -split "`t")[2])
}
function Get-Stage([string]$Name, [string]$File) {
    $blob = Get-Blob $Name
    if (-not $blob) { return $null }
    return [System.IO.File]::ReadAllText((Join-Path $blob $File))
}

Assert-That 'vertex text is the defines then the file' ((Get-Stage "${t}ok" 'vs.glsl') -ceq $expectVs)
Assert-That 'fragment text is the defines, the CR-stripped include, then the file' ((Get-Stage "${t}ok" 'fs.glsl') -ceq $expectFs)
Assert-That 'genfogshader still runs on assembled text (fs)' ((Get-Stage "${t}fog" 'fs.glsl').Contains('uniform vec3 fogcolor;'))
Assert-That 'genfogshader still runs on assembled text (vs)' ((Get-Stage "${t}fog" 'vs.glsl').Contains('lineardepth = dot(lineardepthscale, gl_Position.zw);'))
Assert-That 'texture-slot params still become uniforms (vs)' ((Get-Stage "${t}param" 'vs.glsl').Contains('uniform vec4 srcparam;'))
Assert-That 'texture-slot params still become uniforms (fs)' ((Get-Stage "${t}param" 'fs.glsl').Contains('uniform vec4 srcparam;'))
Assert-That 'the outer body keeps its own defines' ((Get-Stage "${t}outer" 'fs.glsl') -ceq $expectFs)
Assert-That 'the inner body gets none of the outer defines' ((Get-Stage "${t}inner" 'fs.glsl') -ceq $frag)

# Cleanup
Remove-Item -Recurse -Force $fixDir -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue
if ($failures) { Write-Host "$failures failure(s)" -ForegroundColor Red; exit 1 }
Write-Host 'All passed' -ForegroundColor Green
exit 0
```

- [ ] **Step 2: Run it and confirm it fails**

```powershell
tools\harness\harness.ps1 start
powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1
```

Expected: FAIL on the creation asserts (`SRC_OK=0` etc.). The log has `Unknown command: shader_new`. Some "refused"/"is 0" asserts pass vacuously at this point; that's expected, because the commands don't exist yet.

- [ ] **Step 3: Implement the commands**

Append to `src/engine/shadersource.cpp`:

```cpp
// What a shader_new/variantshader_new body has described so far. Bodies can
// nest (a body may run a generator that defines another shader), so each run
// keeps its own record and restores the outer one afterwards.
struct shaderbuild
{
    vector<char> defines;
    vector<char *> includes[2]; // paths; [0] vertex, [1] fragment
    string source[2];
    bool failed;

    shaderbuild() : failed(false) { source[0][0] = source[1][0] = '\0'; }
    ~shaderbuild() { loopi(2) includes[i].deletearrays(); }
};
static shaderbuild *curbuild = NULL;

static shaderbuild *getbuild(const char *cmd)
{
    if(!curbuild) conoutf(colourred, "%s: only valid inside a shader_new or variantshader_new body", cmd);
    return curbuild;
}

static bool checkpath(shaderbuild &b, const char *cmd, const char *path)
{
    if(shadersource::validpath(path)) return true;
    conoutf(colourred, "%s: refusing \"%s\": shader sources must be relative paths under config/glsl/", cmd, path);
    b.failed = true;
    return false;
}

ICOMMAND(0, shader_define, "ss", (char *name, char *value),
{
    shaderbuild *b = getbuild("shader_define");
    if(!b) return;
    if(!shadersource::validdefine(name, value))
    {
        conoutf(colourred, "shader_define: invalid define \"%s\"", name);
        b->failed = true;
        return;
    }
    shadersource::appenddefine(b->defines, name, value);
});

static void includesource(int stage, const char *cmd, const char *path)
{
    shaderbuild *b = getbuild(cmd);
    if(!b || !checkpath(*b, cmd, path)) return;
    b->includes[stage].add(newstring(path));
}
ICOMMAND(0, shader_include_vs, "s", (char *path), includesource(0, "shader_include_vs", path));
ICOMMAND(0, shader_include_fs, "s", (char *path), includesource(1, "shader_include_fs", path));

ICOMMAND(0, shader_source, "ss", (char *vs, char *fs),
{
    shaderbuild *b = getbuild("shader_source");
    if(!b) return;
    if(vs[0] && !checkpath(*b, "shader_source", vs)) return;
    if(fs[0] && !checkpath(*b, "shader_source", fs)) return;
    copystring(b->source[0], vs);
    copystring(b->source[1], fs);
});

// Reads one file of the build; NULL, with the reason logged, if it cannot.
static char *loadsource(const char *name, const char *path)
{
    char *text = loadfile(path, NULL);
    if(!text) conoutf(colourred, "shader %s: cannot read %s", name, path);
    return text;
}

// Assembles one stage of the build into out. False if it could not be.
static bool buildstage(const char *name, shaderbuild &b, int stage, vector<char> &out)
{
    const char *kind = stage ? "fragment" : "vertex";
    if(!b.source[stage][0])
    {
        if(b.includes[stage].length())
        {
            conoutf(colourred, "shader %s: %s includes given but no %s source", name, kind, kind);
            return false;
        }
        out.setsize(0);
        out.add('\0');
        return true;
    }
    vector<const char *> texts;
    bool ok = true;
    loopv(b.includes[stage])
    {
        char *text = loadsource(name, b.includes[stage][i]);
        if(!text) { ok = false; break; }
        texts.add(text);
    }
    char *body = ok ? loadsource(name, b.source[stage]) : NULL;
    ok = body != NULL;
    if(ok) shadersource::assemblestage(out, b.defines, texts, body);
    texts.deletearrays();
    DELETEA(body);
    return ok;
}

// Runs a body with a fresh build record and assembles both stages. False if
// the body or a file failed; the reason is already logged.
static bool runbuild(const char *name, uint *body, vector<char> &vs, vector<char> &ps)
{
    shaderbuild b, *outer = curbuild;
    curbuild = &b;
    execute(body);
    curbuild = outer;
    if(!b.failed && buildstage(name, b, 0, vs) && buildstage(name, b, 1, ps)) return true;
    conoutf(colourred, "shader %s: not created", name);
    return false;
}

static void shadernew(int type, char *name, uint *body)
{
    // shader() would keep an existing shader anyway; returning first also
    // skips running the body and reading its files on every resetshaders.
    if(lookupshaderbyname(name)) return;
    vector<char> vs, ps;
    if(!runbuild(name, body, vs, ps)) return;
    if(!vs[0] || !ps[0])
    {
        conoutf(colourred, "shader %s: needs both a vertex and a fragment source", name);
        return;
    }
    shader(type, name, vs.getbuf(), ps.getbuf());
}
ICOMMAND(0, shader_new, "ise", (int *type, char *name, uint *body), shadernew(*type, name, body));
```

If `isalpha`/`isalnum` (Task 1) or `execute(const uint *)` are not visible through `engine.h`, find their declarations (`grep -n "int execute(const uint" src/shared/command.h`) and include the matching header. Do not redeclare them.

- [ ] **Step 4: Build, then run the live test and confirm it passes**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
tools\harness\harness.ps1 start
powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1
```

Expected: every line `PASS`, then `All passed`, exit code 0. If a `vs.glsl`/`fs.glsl` comparison fails, print both texts with the line endings visible before changing anything, e.g. `(Get-Stage "${t}ok" 'fs.glsl') -replace "`n", '\n'`. An extra `uniform vec4` line means leftover slot params: check that `setshader null` ran first.

- [ ] **Step 5: Document the commands**

In `doc/shader-reference.md`, insert this section before the line `### Shader Parameter Binding`:

````markdown
### Shader Source Files

`shader_new` builds a shader from GLSL files instead of CubeScript-generated text.
The body describes the shader; the engine then reads the files, assembles each stage
and passes the result to `shader`, so fog (`//:fog`), generic variants (`//:variant`)
and texture-slot uniforms behave exactly as for inline shaders.

```cubescript
shader_new $SHADER_DEFAULT "linearizedepth" [
    shader_define AO_DEPTH_FORMAT $aodepthformat   // "#define AO_DEPTH_FORMAT 1"
    shader_define AO_PACKED ""                     // "#define AO_PACKED"
    shader_include_fs "config/glsl/shared/gdepth.glsl"
    shader_source "config/glsl/ao/linearizedepth.vert" "config/glsl/ao/linearizedepth.frag"
]
```

- Each stage is: its defines in call order, its includes in call order (`shader_include_vs`,
  `shader_include_fs`), then its `shader_source` file. Defines go to every stage that has a file.
- Carriage returns are dropped and every piece ends with a newline, so CRLF and LF checkouts
  assemble to the same text.
- Paths must be under `config/glsl/`, with forward slashes and no `.` or `..` components. They
  are found like `exec` finds files: home directory first, then the packages.
- `shader_new` does nothing if the shader is already loaded; its body does not run.
- Any failure (unreadable file, refused path, invalid define, a missing stage) is logged and
  no shader is created. `shader_define`, `shader_include_*` and `shader_source` outside a body
  are logged and ignored.
- The `#version` header and compatibility macros are added in front of the assembled text
  when it is compiled, so defines may use them.
- Caveat: uniform and fog declarations are inserted at the line containing the first
  occurrence of the text `main`. Keep that word out of includes (identifiers such as `domain`
  and comments included), or the declarations can land inside an include's function.
````

- [ ] **Step 6: Commit**

```powershell
git add src/engine/shadersource.cpp tools/harness/tests/shadersource.ps1 doc/shader-reference.md
git commit -m @'
engine: add shader_new and its body commands

shader_new runs a body of shader_define, shader_include_vs/fs and
shader_source calls, assembles each stage from those files and hands the
text to shader(), so fog, variants and slot uniforms work unchanged.
Paths are confined to config/glsl/, and failures are logged and create
no shader.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 3: `variantshader_new`

**Files:**
- Modify: `src/engine/texture.h` (next to the `extern Shader *shader(...)` declaration)
- Modify: `src/engine/shadersource.cpp` (append)
- Modify: `tools/harness/tests/shadersource.ps1` (insert before the `# Cleanup` line)
- Modify: `doc/shader-reference.md` (append to the "Shader Source Files" section)

**Interfaces:**
- Consumes: Task 2's `runbuild`, `shadernew` and `tools/harness/tests/shadersource.ps1` (including its `$t`, `Invoke-Batch`, `Test-Echo`, `Test-Logged`, `$rows`, `Get-Blob`, `Get-Stage`, `$vert`, `$expectFs`, and the `# Cleanup` marker). `variantshader(int type, char *name, int row, char *vs, char *ps, int maxvariants)` from `shader.cpp`.
- Produces: the CubeScript command `variantshader_new <type> <name> <row> <maxvariants> [body]` (`"isiie"`).

- [ ] **Step 1: Write the failing test cases**

In `tools/harness/tests/shadersource.ps1`, insert directly before the line `# Cleanup`:

```powershell
# variantshader_new (Task 3). A variant with only a fragment file reuses the
# parent's vertex stage. The dump above ran before these existed, so dump again.
$variants = @"
setshader null
shader_new 0 ${t}parent [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
variantshader_new 0 ${t}parent 1 1 [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_VARIANT=" (hasshader "<variant:0,1>${t}parent"))
srcvarran = 0
variantshader_new 0 ${t}noparent 1 1 [ srcvarran = 1 ]
echo (concatword "SRC_VARRAN=" `$srcvarran)
variantshader_new 0 ${t}parent 1 1 [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_VARORPHAN=" (hasshader "<variant:1,1>${t}parent"))
variantshader_new 0 ${t}rowless -1 0 [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ROWLESS=" (hasshader ${t}rowless))
"@
$v = @(Invoke-Batch $variants 1 120)
Assert-That 'a variant with only a fragment file is created' (Test-Echo $v 'SRC_VARIANT=1')
Assert-That 'no body runs when the parent is missing' (Test-Echo $v 'SRC_VARRAN=0')
Assert-That 'variant includes without a source are refused' ((Test-Echo $v 'SRC_VARORPHAN=0') -and (Test-Logged $v 'includes given but no vertex source'))
Assert-That 'row -1 behaves like shader_new' (Test-Echo $v 'SRC_ROWLESS=1')

Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue
$null = Invoke-Batch 'shaderdumpall srcloader s00 0' 1 300
$rows = [System.IO.File]::ReadAllLines((Join-Path $runDir 'manifest.tsv'))
Assert-That 'the variant fragment stage is assembled' ((Get-Stage "<variant:0,1>${t}parent" 'fs.glsl') -ceq $expectFs)
Assert-That 'the variant reuses the parent vertex stage' ((Get-Stage "<variant:0,1>${t}parent" 'vs.glsl') -ceq $vert)
```

- [ ] **Step 2: Run it and confirm it fails**

```powershell
tools\harness\harness.ps1 start
powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1
```

Expected: Task 2's asserts pass. The new ones fail: `SRC_VARIANT=0`, `SRC_ROWLESS=0`, and `Unknown command: variantshader_new` in the log. `Get-Stage` returns `$null`, so those two asserts fail as well.

- [ ] **Step 3: Implement**

In `src/engine/texture.h`, directly after the line `extern Shader *shader(int type, char *name, char *vs, char *ps, bool mapdef = false, bool overwrite = false);`, add:

```cpp
extern Shader *variantshader(int type, char *name, int row, char *vs, char *ps, int maxvariants);
```

Append to `src/engine/shadersource.cpp`:

```cpp
static void variantshadernew(int type, char *name, int row, int maxvariants, uint *body)
{
    if(row < 0) { shadernew(type, name, body); return; }
    // variantshader() drops these too; checking first skips the body and files.
    if(row >= MAXVARIANTROWS || !lookupshaderbyname(name)) return;
    vector<char> vs, ps;
    if(!runbuild(name, body, vs, ps)) return;
    // An empty stage makes newshader reuse the parent's.
    variantshader(type, name, row, vs.getbuf(), ps.getbuf(), maxvariants);
}
ICOMMAND(0, variantshader_new, "isiie", (int *type, char *name, int *row, int *maxvariants, uint *body), variantshadernew(*type, name, *row, *maxvariants, body));
```

- [ ] **Step 4: Build, then run the live test and confirm it passes**

```powershell
tools\harness\harness.ps1 stop
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
tools\harness\harness.ps1 start
powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1
```

Expected: every line `PASS`, `All passed`, exit code 0.

- [ ] **Step 5: Document**

Append to the "### Shader Source Files" section of `doc/shader-reference.md`, before `### Shader Parameter Binding`:

````markdown
`variantshader_new <type> <name> <row> <maxvariants> [body]` is the file-based `variantshader`.
A stage with no file (`shader_source "" "config/glsl/..."`) reuses the parent's stage. A
negative row behaves like `shader_new`. The body does not run when the parent is not loaded
or the row is out of range.

```cubescript
variantshader_new $SHADER_DEFAULT "bumpworld" 1 2 [
    shader_define BUMP_TRIPLANAR ""
    shader_source "" "config/glsl/world/bump.frag"
]
```
````

- [ ] **Step 6: Commit**

```powershell
git add src/engine/texture.h src/engine/shadersource.cpp tools/harness/tests/shadersource.ps1 doc/shader-reference.md
git commit -m @'
engine: add variantshader_new

The file-based variantshader: a stage with no file reuses the parent's,
a negative row behaves like shader_new, and a missing parent or row out
of range skips the body.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 4: Equivalence gate

No code. This proves the loader changed no shader, and leaves the state recorded for the AO port.

**Files:**
- Modify: `doc/agent-handoff.md` (untracked; not committed)

- [ ] **Step 1: Start from a fresh game on the new build**

```powershell
tools\harness\harness.ps1 stop
git status --short config/glsl
```

Expected: `git status` prints nothing. `check` starts its own game, so no test shaders from Tasks 2 and 3 are present; those would show up as `EXTRA`.

- [ ] **Step 2: Fast check**

```powershell
tools\harness\shaders.ps1 check -Sids s00 -NoMaps
```

Expected: exit code 0, and the summary lists only `PASS-TEXT`.

- [ ] **Step 3: Full check (about an hour)**

```powershell
$out = tools\harness\shaders.ps1 check -PassThru
$rows = @($out | Where-Object { $_ -is [System.Management.Automation.PSCustomObject] -and $_.PSObject.Properties['Status'] })
$rows | Group-Object Status | Format-Table Count, Name -AutoSize
$odd = @($rows | Where-Object { $_.Status -cne 'PASS-TEXT' -or $_.BaseHash -cne $_.CandHash })
"rows=$($rows.Count) odd=$($odd.Count)"
$odd | Select-Object -First 20 | Format-Table Status, Name, Sid, Detail -AutoSize
```

Expected: one group, `PASS-TEXT`, and `odd=0`. `rows` should be close to the baseline's number of valid manifest rows.

- A `PASS-TEXT` row whose hashes differ means the text changed while its tokens did not. That is still a failure for this plan, which must be byte-identical.
- For any odd row, run `tools\harness\shaders.ps1 diff <name> -Sid <sid>` and stop. Report it before changing anything: a difference means the loader changed a path it was meant to leave alone.

- [ ] **Step 4: Update the handoff**

In `doc/agent-handoff.md`:
- Set the "_Last updated_" line to today's date, noting the loader.
- In §3, add a bullet: the loader (`shader_new`, `variantshader_new`, `shader_define`, `shader_include_vs/fs`, `shader_source`) is on branch `shader-source-loader`, with the commit range. The full `check` gave `PASS-TEXT` with identical hashes on every row, with its date.
- In §4's suite table, add a row: "Shader loader live test | yes | ~1 min | `powershell -ExecutionPolicy Bypass -File tools\harness\tests\shadersource.ps1`", and add `testshadersource` to the C++ unit test list.
- In §6 item 1, mark step 1 (the loader) done. Point step 2 (AO) at this plan's "Out of scope, for the family ports" list.

Do not commit `doc/`: it is untracked.

---

## Self-review notes

- **Coverage of the ask** ("port the loader, keep `composeglslparts`, `origin`/`generated` and `slotparamsscope`"):
  - The loader is Tasks 1–3.
  - Those three are untouched by construction (Global Constraints), and Task 4 proves nothing downstream changed.
  - `origin` stays right because `shader()` → `newshader` still reads `shaderorigin`/`getsourcefile()`. Inside a generator, `shader_new` runs within the same `generateshader`/`force` scope as `shader` did.
- **Names used across tasks:**
  - `shadersource::validpath`/`validdefine`/`appenddefine`/`appendtext`/`assemblestage` (Task 1) are consumed in Task 2.
  - `runbuild`/`shadernew` (Task 2) are consumed in Task 3.
  - The test helpers `Test-Echo`, `Test-Logged`, `Get-Stage`, `$expectFs`, `$vert` and the `# Cleanup` marker are defined in Task 2 and used in Task 3.
- **Known risk: `hasshader` on a variant name.** Task 3's `hasshader "<variant:0,1>…"` depends on variants living in the same `shaders` table (`newshader` inserts them under `varname`, `shader.cpp:870-872`). If it returns 0 while the dump has the row, assert on the manifest row instead and record why in the task report.
