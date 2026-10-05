# Map Editor Prefabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the map editor a prefab workflow: save a geometry selection as a named prefab, browse the library in a docked panel with spinning 3D previews, load a prefab onto the clipboard and paste it with the existing paste, and manage the library (folders, overwrite, rename, delete).

**Architecture:** Three layers. A thin engine layer in `src/engine/octaedit.cpp` adds `prefabinfo`, `removeprefab` and `renameprefab`, plus fixes to `saveprefab` and `copyprefab`. The existing asset browser widget `ui_tool_filelist` gains a prefab file type, a configurable root, callbacks and per-instance state, so it can serve as a docked browser. A new CubeScript library (`config/tool/toolprefab.cfg`) and panel (`config/ui/tool/toolprefab.cfg`) tie these together through `tool_action`s, so every operation is also searchable and bindable.

**Tech Stack:** C++ (Tesseract-derived engine, MSVC cross-build from WSL), CubeScript, Windows PowerShell 5.1 (test harness).

**Spec:** [doc/superpowers/specs/2026-09-24-map-editor-prefabs-design.md](../specs/2026-09-24-map-editor-prefabs-design.md)

## Global Constraints

- **Geometry only.** Keep the `.obr` format (`"OEBR"`, version 0) unchanged. No texture tables, no entities in prefab files.
- **The UI always names a prefab by its full relative path without extension**, e.g. `prefab/trees/oak`. The engine drops its `prefab/` prefix whenever a name contains `/` (`octaedit.cpp:1565`), so a bare `trees/oak` names a different file.
- **New engine commands** are `ICOMMAND(0, …)`, refuse when `identflags&IDF_MAP`, and are **not** behind `DEBUG_UTILS` (they are shipped editor features). None sends network messages.
- **Build:** from PowerShell, `wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug`. Git Bash rewrites the `/mnt/f` path and the build fails. Stay on `debug`: switching build type triggers a full `make clean`, and only `debug` compiles `src/tests/*.o`. **Stop the harness before building** (`tools\harness\harness.ps1 stop`); the running game locks `bin/amd64/redeclipse.exe` and the copy step fails with `Error 1`.
- **Runnable binary** is `bin/amd64/redeclipse.exe`. Never launch `src/redeclipse_windows_amd64.exe`.
- **CubeScript:** no bare `#` (it is a macro preprocessor); prefer `concat` / `concatword` over `@` substitution (all code in this plan avoids `@` outside the existing `@(props …)` idiom); write `exec "path" 0 0` (three args). See "CubeScript traps" in `CLAUDE.md`.
- **CubeScript scoping facts this plan relies on:** `local` variables are dynamically scoped, so aliases called from a widget see the widget's `p_*` props. Props blocks handed to `toolpanel_open_menu` and `toolpanel_open … [p_user_data = …]` are **re-evaluated every frame in another window**, so they may only reference globals, never locals. `tool_confirm_prompt` bakes its code argument, which is also evaluated later, so it too must reference globals only.
- **UI edits need no rebuild:** `tools\harness\harness.ps1 reload <file>`. A docked panel copies its content alias **when it opens**, so after reloading `config/ui/tool/toolprefab.cfg`, close and reopen the Prefabs panel.
- **PowerShell:** Windows PowerShell 5.1. No `&&` / `||`, no ternary, no `??`. Write files with `Write-TextNoBom` if a script ever writes CubeScript.
- **Harness problems stop the work.** If the harness itself misbehaves (the command channel, `find` / `click`, screenshots, a synthesised click or double-click not registering while the same UI works by hand, crash logging), **run `git stash -u`, stop, and report** what you observed with log excerpts. The user investigates harness issues in a separate session. A failure in the code this plan adds is not a harness problem: fix it and carry on.
- **Do not commit or push unless the user asks.** The commit steps below are written for a normal workflow; if the user has not asked for commits, complete the step's file changes and skip the commit. Commit style: lowercase `area: summary`. There is no git identity in the agent shell; if asked to commit, pass it per commit: `git -c user.name="Sławomir 'Q009' Błauciak" -c user.email="q009q009@gmail.com" commit …`.
- **Leave unrelated uncommitted files alone:** `readme.md`, `chat_wip.cfg`, `deli.zip`, `gun_lore.txt`, `profile_daemon.ps1`, `profiler_tools.zip`, `profilerhook.cfg`, `unix/`. `git stash -u` would sweep these up too; prefer `git stash push -u -- <paths you touched>` when pausing.

## File map

| File | Status | Responsibility |
|---|---|---|
| `src/engine/octaedit.cpp` | modify | `validprefabpath`, cache/path helpers, `prefabinfo` / `removeprefab` / `renameprefab`, `saveprefab` and `copyprefab` fixes |
| `src/engine/world.cpp` | modify | `entcopyclear()`, which empties the engine entity clipboard |
| `src/engine/engine.h` | modify | declare `validprefabpath`, `entcopyclear` |
| `src/tests/prefab.cpp` | create | `testprefab()`: `validprefabpath` cases |
| `src/engine/main.cpp`, `src/Makefile` | modify | register the unit test (debug builds only) |
| `config/usage.cfg` | modify | `setdesc` for the new commands, updated `copyprefab` text |
| `config/tool/toolcommon.cfg` | modify | `TOOL_FILE_PREFAB` |
| `config/ui/tool/toolview/widgets/toolfilelist.cfg` | modify | root, callbacks, footer toggle, refetch, per-instance state, dedup, prefab tiles |
| `config/tool/toolprefab.cfg` | create | prefab library state, validation, operations, actions |
| `config/tool.cfg` | modify | exec the library |
| `config/ui/tool/toolprefab.cfg` | create | Prefabs panel, context menu, Save and Rename popups |
| `config/ui/tool.cfg` | modify | exec the panel |
| `config/ui/tool/toolview/toolbar.cfg` | modify | toolbar button |
| `config/tool/binds/default.cfg` | modify | `I` → `ta_prefabs` (the spec's F6 is taken by `ta_ents`, `config/setup.cfg:329`) |
| `tools/harness/prefab-selftest.ps1` | create | end-to-end checks, grown one section per task |
| `tools/harness/README.md`, `doc/agent-handoff.md`, the spec | modify | documentation |

---

### Task 1: `validprefabpath` and its unit test

The two file-moving commands in Task 2 must never touch anything outside `prefab/`. The validator is a pure function, so it gets a C++ unit test that runs at startup in debug builds, like `testedharness`.

**Files:**
- Create: `src/tests/prefab.cpp`
- Modify: `src/engine/octaedit.cpp` (insert before `struct prefabheader`, currently `:1511`)
- Modify: `src/engine/engine.h:607` (after `extern void cleanupprefabs();`)
- Modify: `src/engine/main.cpp:1333-1334` (after `testedharness();`)
- Modify: `src/Makefile:352` (after `CLIENT_OBJS += tests/edharness.o`)

**Interfaces:**
- Consumes: nothing.
- Produces: `bool validprefabpath(const char *name)`, declared in `engine.h`. True only for `prefab/<seg>[/<seg>…]` where every segment is non-empty, is not `.` or `..`, and uses only `[A-Za-z0-9_.-]`, and `strlen(name) + 4 < MAXSTRLEN`. Used by Task 2.

- [ ] **Step 1: Write the failing test**

Create `src/tests/prefab.cpp`:

```cpp
#include "engine.h"

// Prefab path validation, see validprefabpath() in engine/octaedit.cpp. It
// guards removeprefab and renameprefab, which move files, so the rejections
// matter more than the acceptances.
void testprefab()
{
    ASSERT(validprefabpath("prefab/oak"));
    ASSERT(validprefabpath("prefab/trees/oak"));
    ASSERT(validprefabpath("prefab/a.b-c_d"));
    ASSERT(validprefabpath("prefab/Trees/Oak2"));

    ASSERT(!validprefabpath(""));
    ASSERT(!validprefabpath("oak"));
    ASSERT(!validprefabpath("prefab"));
    ASSERT(!validprefabpath("prefab/"));
    ASSERT(!validprefabpath("prefabx/y"));
    ASSERT(!validprefabpath("/prefab/x"));
    ASSERT(!validprefabpath("prefab//x"));
    ASSERT(!validprefabpath("prefab/x/"));
    ASSERT(!validprefabpath("prefab/../x"));
    ASSERT(!validprefabpath("prefab/x/.."));
    ASSERT(!validprefabpath("prefab/./x"));
    ASSERT(!validprefabpath("prefab\\x"));
    ASSERT(!validprefabpath("C:/x"));
    ASSERT(!validprefabpath("prefab/sp ace"));

    // The name plus ".obr" must still fit in a string.
    string name;
    copystring(name, "prefab/");
    size_t len = strlen(name);
    while(len < MAXSTRLEN-5) name[len++] = 'a';
    name[len] = '\0';
    ASSERT(validprefabpath(name)); // len+4 == MAXSTRLEN-1
    name[len++] = 'a';
    name[len] = '\0';
    ASSERT(!validprefabpath(name)); // len+4 == MAXSTRLEN

    conoutf(colourwhite, "testprefab: ok");
}
```

Register it. In `src/engine/main.cpp`, extend the `_DEBUG` block so it reads:

```cpp
    #ifdef _DEBUG
        // Run unit tests
        extern void testslotmanager();
        testslotmanager();
        extern void testedharness();
        testedharness();
        extern void testprefab();
        testprefab();
    #endif
```

In `src/Makefile`, extend the test objects so they read:

```make
ifneq (,$(findstring -D_DEBUG,$(CXXFLAGS)))
    CLIENT_OBJS += tests/slotmanager.o
    CLIENT_OBJS += tests/edharness.o
    CLIENT_OBJS += tests/prefab.o
endif
```

In `src/engine/engine.h`, after `extern void cleanupprefabs();`:

```cpp
extern bool validprefabpath(const char *name);
```

- [ ] **Step 2: Build to verify it fails**

```powershell
tools\harness\harness.ps1 stop
```

```powershell
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
```

Expected: link failure, unresolved external symbol `validprefabpath` (referenced from `tests/prefab.o`).

- [ ] **Step 3: Implement**

In `src/engine/octaedit.cpp`, immediately before `struct prefabheader`:

```cpp
// Prefab paths as the editor's prefab browser handles them: the full relative
// path under prefab/, without the extension ("prefab/trees/oak"). removeprefab
// and renameprefab move files, so this is strict: no "..", nothing absolute,
// no drive letters or backslashes, and a name that still fits once ".obr" is
// appended.
bool validprefabpath(const char *name)
{
    static const char prefix[] = "prefab/";
    const size_t prefixlen = sizeof(prefix)-1;
    size_t len = strlen(name);
    if(len <= prefixlen || strncmp(name, prefix, prefixlen)) return false;
    if(len + strlen(".obr") >= MAXSTRLEN) return false;
    const char *seg = name;
    for(const char *p = name;; p++)
    {
        if(*p == '/' || !*p)
        {
            size_t seglen = p - seg;
            if(!seglen) return false; // "//" or a trailing '/'
            if(seg[0] == '.' && (seglen == 1 || (seglen == 2 && seg[1] == '.'))) return false;
            if(!*p) break;
            seg = p + 1;
        }
        else if(!isalnum(uchar(*p)) && *p != '_' && *p != '-' && *p != '.') return false;
    }
    return true;
}
```

- [ ] **Step 4: Build and run to verify it passes**

```powershell
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
```

```powershell
tools\harness\harness.ps1 start
```

```powershell
Select-String -Path home\uitest\harness\boot-log.txt, home\uitest\log.txt -Pattern 'testprefab: ok'
```

Expected: at least one match. If `harness.ps1 start` instead throws `Game exited …`, a failed `ASSERT` aborted startup: `home/uitest/log.txt` holds the backtrace with the failing line.

Then run `tools\harness\harness.ps1 stop`.

- [ ] **Step 5: Commit**

```bash
git add src/tests/prefab.cpp src/engine/octaedit.cpp src/engine/engine.h src/engine/main.cpp src/Makefile
git commit -m "engine: add prefab path validation"
```

---

### Task 2: Engine prefab commands and fixes, with the selftest skeleton

**Files:**
- Create: `tools/harness/prefab-selftest.ps1`
- Modify: `src/engine/octaedit.cpp` (the prefab block, `:1511`–`:1638`)
- Modify: `src/engine/world.cpp` (after `entcopy`, `:1257`)
- Modify: `src/engine/engine.h`
- Modify: `config/usage.cfg:620-621`

**Interfaces:**
- Consumes: `validprefabpath` (Task 1).
- Produces (CubeScript commands):
  - `prefabinfo <name>` returns `"sx sy sz grid user"` or `""`. `user` is `1` when the file is in the home dir.
  - `removeprefab <path>` returns `1`/`0`; moves `<home>/<path>.obr` to `<home>/backups/<path>.obr`.
  - `renameprefab <from> <to>` returns `1`/`0`.
  - `copyprefab <name>` no longer needs the selection in view, and empties the engine entity clipboard.
  - `saveprefab <name>` no longer crashes on selections over 100 MB, rebuilds previews after an overwrite, and drops the cache entry on a failed write.
- Produces (C++): `void entcopyclear()` in `world.cpp`, declared in `engine.h`.
- Produces (test): `tools/harness/prefab-selftest.ps1` with helpers `Step`, `Expect`, `ExpectTrue`, `Ed`, `EdState`, `Send`, `Eval`, `Invoke-MapLoad`, `Read-GzBase64`, and section markers where Tasks 3–5 insert their steps.

- [ ] **Step 1: Write the failing test**

Create `tools/harness/prefab-selftest.ps1`:

```powershell
<#
.SYNOPSIS
    End-to-end self-test for the map editor prefab workflow.

.DESCRIPTION
    Boots a client in the harness home, builds the deterministic 'newmap 12'
    scratch world, and drives the prefab engine commands, the file browser
    widget, the CubeScript prefab library and the Prefabs panel.

    User prefabs live under home/uitest/prefab/ and are wiped first. One
    "shipped" (read-only) prefab is simulated by a file under the repo's own
    prefab/ folder: the game runs with the repo root as its working directory,
    which listfiles and findfile search after the home dir. It is removed on
    exit, along with the folder if this script created it.

.EXAMPLE
    tools\harness\prefab-selftest.ps1
    tools\harness\prefab-selftest.ps1 -KeepRunning
#>
[CmdletBinding()]
param([switch]$KeepRunning)

$ErrorActionPreference = 'Stop'

# $RepoRoot and $HomeDir
. (Join-Path $PSScriptRoot 'core.ps1')

$harness = Join-Path $PSScriptRoot 'harness.ps1'
$editor  = Join-Path $PSScriptRoot 'editor.ps1'

$script:failures = 0
$script:step = 0
$script:createdShippedDir = $false

# Same scratch world as editor-selftest.ps1: 'newmap 12' is 4096 units, solid
# below z = 2048, with an 8-unit grid at the default gridpower.
$Floor = 2048
$Mid   = 2048
$Grid  = 8

$UserDir     = Join-Path $HomeDir 'prefab'
$BackupDir   = Join-Path $HomeDir 'backups\prefab'
$ShippedDir  = Join-Path $RepoRoot 'prefab'
$ShippedFile = Join-Path $ShippedDir 'zz_selftest_shipped.obr'

# ------------------------------------------------------------- reporting ----

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    $before = $script:failures
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
    if ($script:failures -gt $before) {
        Write-Host '     --- editor state at failure ---' -ForegroundColor DarkYellow
        try { & $editor state | Out-Null }
        catch { Write-Host "     (state unavailable: $_)" -ForegroundColor DarkYellow }
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ("$Actual" -ceq "$Expected") { Write-Host "     ok    $What = '$Actual'" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

function ExpectTrue([string]$What, [bool]$Condition, [string]$Detail) {
    if ($Condition) { Write-Host "     ok    $What" -ForegroundColor Green }
    else {
        $msg = "     FAIL  $What"
        if ($Detail) { $msg += " -- $Detail" }
        Write-Host $msg -ForegroundColor Red
        $script:failures++
    }
}

# --------------------------------------------------------------- driving ----

function Ed { & $editor @args 6>$null | Out-Null }

function EdState { return (& $editor state -Raw 6>$null) }

function Send([string]$Script, [int]$SettleMs = 250) {
    & $harness send $Script -Settle $SettleMs 6>$null | Out-Null
}

# Evaluates a CubeScript expression in the game; returns its value as text.
function Eval([string]$Expr) {
    $out = @(& $harness send "echo (concatword ""SELFTEST_EVAL="" $Expr)" 6>$null)
    foreach ($line in $out) {
        if ($line -match 'SELFTEST_EVAL=(.*)$') { return $Matches[1].Trim() }
    }
    return $null
}

# See editor-selftest.ps1: loading a map while an entity is hovered trips an
# unguarded enthover read (world.cpp:1426). Entity editing off empties it.
function Invoke-MapLoad([scriptblock]$Body) {
    Send 'entediting 0' 300
    try { & $Body }
    finally { Send 'entediting 1' 300 }
}

# A prefab file's decompressed bytes, for exact comparison. Decompressing
# keeps the comparison independent of the gzip header.
function Read-GzBase64([string]$Path) {
    $in = [System.IO.File]::OpenRead($Path)
    try {
        $gz = New-Object System.IO.Compression.GZipStream($in, [System.IO.Compression.CompressionMode]::Decompress)
        $ms = New-Object System.IO.MemoryStream
        $gz.CopyTo($ms)
        return [Convert]::ToBase64String($ms.ToArray())
    }
    finally { $in.Dispose() }
}

function UserFile([string]$Rel) { return (Join-Path $UserDir $Rel) }
function BackupFile([string]$Rel) { return (Join-Path $BackupDir $Rel) }

# --------------------------------------------------------------------------

foreach ($dir in @($UserDir, $BackupDir)) {
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
}

& $harness stop 6>$null | Out-Null
& $harness start 6>$null | Out-Null

try {
    Step 'setup: scratch world, camera over the test area' {
        Invoke-MapLoad { Ed newmap 12 }
        Ed frame $Mid $Mid $Floor -Dist 256 -Yaw 0 -Pitch -45
        Expect 'edit mode' (EdState).Mode.EditMode 1
    }

    # ==== panel: empty library (Task 5 inserts here) ======================

    # ==== engine commands =================================================

    Step 'saveprefab writes a user prefab; prefabinfo reports it' {
        # Two cubes deep, straddling the floor surface: solid below, air above.
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2,2
        Send 'saveprefab prefab/st/box'
        ExpectTrue 'file written under the home prefab dir' (Test-Path (UserFile 'st\box.obr'))
        Expect 'prefabinfo' (Eval '(prefabinfo prefab/st/box)') '2 2 2 8 1'
        Expect 'prefabinfo of a missing prefab' (Eval '(prefabinfo prefab/st/nothing)') ''
    }

    Step 'copyprefab + paste reproduces the geometry exactly' {
        Send 'copyprefab prefab/st/box'
        # Up in the air, clear of the floor, so the paste is all that is there.
        Ed sel ($Mid + 64) $Mid ($Floor + 64) -Size 2,2,2
        Send 'pastehilight; paste'
        Send 'saveprefab prefab/st/rt'
        $a = Read-GzBase64 (UserFile 'st\box.obr')
        $b = Read-GzBase64 (UserFile 'st\rt.obr')
        ExpectTrue 'decompressed files are identical' ($a -eq $b) "base64 lengths $($a.Length) vs $($b.Length)"
    }

    Step 'copyprefab: works with the selection out of view, replaces copied entities' {
        # Put an entity on the engine's entity clipboard first.
        Send 'cancelsel'
        Ed sel $Mid $Mid $Floor
        Send 'newent playerstart'
        ExpectTrue 'an entity is selected to copy' ((EdState).EntSel.Count -eq 1)
        Send 'entcopy; entcancel; pasteclear; cancelsel'
        # Look at the sky: the stale selection box is out of view.
        Ed aim 180 80
        Send 'copyprefab prefab/st/box'
        Send 'pastehilight'
        $s = EdState
        Expect 'pastehilight took the prefab size (x)' $s.Sel.SX 2
        Expect 'pastehilight took the prefab size (y)' $s.Sel.SY 2
        Expect 'pastehilight took the prefab size (z)' $s.Sel.SZ 2
        Send 'entpaste'
        Expect 'no stale entities pasted with the prefab' (EdState).EntSel.Count 0
        Ed frame $Mid $Mid $Floor -Dist 256 -Yaw 0 -Pitch -45
    }

    Step 'saveprefab over an existing prefab replaces it' {
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 3,1,1
        Send 'saveprefab prefab/st/box'
        Expect 'prefabinfo reflects the new selection' (Eval '(prefabinfo prefab/st/box)') '3 1 1 8 1'
    }

    Step 'renameprefab' {
        Expect 'rename into a new folder' (Eval '(renameprefab prefab/st/rt prefab/st2/rt2)') '1'
        ExpectTrue 'source file is gone' (-not (Test-Path (UserFile 'st\rt.obr')))
        ExpectTrue 'target file exists' (Test-Path (UserFile 'st2\rt2.obr'))
        Expect 'rename onto an existing prefab is refused' (Eval '(renameprefab prefab/st2/rt2 prefab/st/box)') '0'
        ExpectTrue 'both files survive the refused rename' ((Test-Path (UserFile 'st2\rt2.obr')) -and (Test-Path (UserFile 'st\box.obr')))
        Expect 'a target outside prefab/ is refused' (Eval '(renameprefab prefab/st2/rt2 st2/rt2)') '0'
        Expect 'a target with .. is refused' (Eval '(renameprefab prefab/st2/rt2 prefab/../rt2)') '0'
        Expect 'a case-only rename is allowed' (Eval '(renameprefab prefab/st2/rt2 prefab/st2/RT2)') '1'
        $names = @(Get-ChildItem (UserFile 'st2') | ForEach-Object { $_.Name })
        ExpectTrue 'the file took the new case' ($names -ccontains 'RT2.obr') "files: $($names -join ', ')"
        Expect 'prefabinfo under the new name' (Eval '(prefabinfo prefab/st2/RT2)') '2 2 2 8 1'
    }

    Step 'removeprefab moves the file to backups/' {
        Expect 'remove' (Eval '(removeprefab prefab/st2/RT2)') '1'
        ExpectTrue 'file left the prefab dir' (-not (Test-Path (UserFile 'st2\RT2.obr')))
        ExpectTrue 'file is in backups' (Test-Path (BackupFile 'st2\RT2.obr'))
        Expect 'prefabinfo no longer finds it' (Eval '(prefabinfo prefab/st2/RT2)') ''
        Expect 'removing it again fails' (Eval '(removeprefab prefab/st2/RT2)') '0'
        Expect 'a bare name is refused' (Eval '(removeprefab box)') '0'
    }

    Step 'a prefab outside the home dir is read-only' {
        if (-not (Test-Path $ShippedDir)) {
            New-Item -ItemType Directory -Path $ShippedDir | Out-Null
            $script:createdShippedDir = $true
        }
        Copy-Item (UserFile 'st\box.obr') $ShippedFile
        Expect 'prefabinfo marks it shipped' (Eval '(prefabinfo prefab/zz_selftest_shipped)') '3 1 1 8 0'
        Expect 'remove is refused' (Eval '(removeprefab prefab/zz_selftest_shipped)') '0'
        Expect 'rename is refused' (Eval '(renameprefab prefab/zz_selftest_shipped prefab/zz2)') '0'
        ExpectTrue 'the file is untouched' (Test-Path $ShippedFile)
        Expect 'a user prefab cannot be renamed over it' (Eval '(renameprefab prefab/st/box prefab/zz_selftest_shipped)') '0'
    }

    # Not covered here, by design: a selection over 100 MB (saveprefab's NULL
    # guard) and IDF_MAP refusal are unreachable from the harness. Review them.

    # ==== file browser widget (Task 3 inserts here) =======================

    # ==== prefab library (Task 4 inserts here) ============================

    # ==== prefabs panel (Task 5 inserts here) =============================
}
finally {
    if (Test-Path $ShippedFile) { Remove-Item -Force $ShippedFile }
    if ($script:createdShippedDir -and (Test-Path $ShippedDir) -and -not (Get-ChildItem $ShippedDir)) {
        Remove-Item -Force $ShippedDir
    }
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'prefab self-test: all checks passed' -ForegroundColor Green
exit 0
```

- [ ] **Step 2: Run it to verify it fails**

The Task 1 build is current, so the game runs, but the new commands don't exist yet.

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: the setup step and the first two `saveprefab` checks pass (those commands already exist). Every `prefabinfo` / `renameprefab` / `removeprefab` expectation fails with `got ''`. The out-of-view step fails on the size checks (`copyprefab` refuses, "Selection not in view") and on the entity count. Exit code 1.

- [ ] **Step 3: Implement the engine side**

In `src/engine/world.cpp`, directly after the `COMMAND(0, entcopy, "");` line:

```cpp
// Loading a prefab replaces the whole clipboard: a later paste must not also
// drop the entities of an earlier copy.
void entcopyclear()
{
    entcopybuf.shrink(0);
}
```

In `src/engine/engine.h`, next to the `validprefabpath` declaration:

```cpp
extern void entcopyclear();
```

In `src/engine/octaedit.cpp`, directly after `static hashnameset<prefab> prefabs;` and before `void cleanupprefabs()`:

```cpp
extern string homedir;

// "prefab/oak" and "oak" name the same file (see saveprefab), so both cache
// keys go. Also frees the preview mesh's GL buffers.
static void uncacheprefab(const char *name)
{
    const char *keys[2] = { name, NULL };
    if(!strncmp(name, "prefab/", 7) && !strpbrk(name + 7, "/\\")) keys[1] = name + 7;
    loopi(2) if(keys[i])
    {
        prefab *p = prefabs.access(keys[i]);
        if(!p) continue;
        p->cleanup();
        prefabs.remove(keys[i]);
    }
}

// The home-dir file a prefab name refers to, mapped the way saveprefab and
// loadprefab map it. Only files there are the user's to remove or rename.
static bool homeprefabfile(const char *name, string &file)
{
    if(strlen(homedir) + strlen("prefab/") + strlen(name) + strlen(".obr") >= MAXSTRLEN) return false;
    if(strpbrk(name, "/\\")) formatstring(file, "%s%s.obr", homedir, name);
    else formatstring(file, "%sprefab/%s.obr", homedir, name);
    path(file);
    return true;
}
```

Replace the body of `saveprefab` (from `void saveprefab(char *name)` to its closing brace, **not** the `ICOMMAND` line after it) with:

```cpp
void saveprefab(char *name)
{
    if(!name[0] || noedit(true) || (nompedit && multiplayer())) return;
    prefab *b = prefabs.access(name);
    if(!b)
    {
        b = &prefabs[name];
        b->name = newstring(name);
    }
    // An overwrite: the preview mesh was built from the old block
    b->cleanup();
    if(b->copy) { freeblock(b->copy); b->copy = NULL; }
    protectsel(b->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
    if(!b->copy)
    {
        uncacheprefab(name);
        conoutf(colourred, "Selection too large for a prefab");
        return;
    }
    defformatstring(filename, strpbrk(name, "/\\") ? "%s.obr" : "prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "wb");
    // The cache must not claim a prefab the disk does not have
    if(!f) { uncacheprefab(name); conoutf(colourred, "Could not write prefab to %s", filename); return; }
    prefabheader hdr;
    memcpy(hdr.magic, "OEBR", 4);
    hdr.version = 0;
    lilswap(&hdr.version, 1);
    f->write(&hdr, sizeof(hdr));
    streambuf<uchar> s(f);
    if(!packblock(*b->copy, s)) { delete f; uncacheprefab(name); conoutf(colourred, "Could not pack prefab %s", filename); return; }
    delete f;
    conoutf(colourwhite, "Wrote prefab file %s", filename);
}
```

Replace `copyprefab` (from the `/* Copy prefab `name` to clipboard */` comment through `COMMAND(0, copyprefab, "s");`) with:

```cpp
/* Copy prefab `name` to clipboard */
void copyprefab(char *name)
{
    // noedit(true): loading does not touch the world, so the selection need
    // not be in view (the prefab browser loads with the camera anywhere)
    if(!name[0] || noedit(true)) return;
    prefab *b = loadprefab(name, true);
    if(!b) return;
    if(multiplayer(false)) client::edittrigger(sel, EDIT_COPY, 1);
    if(!localedit) localedit = editinfos.add(new editinfo);
    if(localedit->copy) freeblock(localedit->copy);
    localedit->copy = copyblock(b->copy);
    entcopyclear();
}
COMMAND(0, copyprefab, "s");

ICOMMAND(0, prefabinfo, "s", (char *name),
{
    if(identflags&IDF_MAP || !name[0]) { result(""); return; }
    prefab *p = loadprefab(name, false);
    if(!p || !p->copy) { result(""); return; }
    string file;
    bool user = homeprefabfile(name, file) && fileexists(file, "r");
    defformatstring(info, "%d %d %d %d %d", p->copy->s.x, p->copy->s.y, p->copy->s.z, p->copy->grid, user ? 1 : 0);
    result(info);
});

// Resolves a user prefab's file; says why not on the console otherwise
static bool finduserprefab(const char *name, string &file)
{
    if(!validprefabpath(name)) { conoutf(colourred, "Invalid prefab path: %s", name); return false; }
    if(!homeprefabfile(name, file) || !fileexists(file, "r")) { conoutf(colourred, "Prefab %s is not a user prefab", name); return false; }
    return true;
}

static bool removeprefabfile(const char *name)
{
    string src;
    if(!finduserprefab(name, src)) return false;
    // Moved aside rather than deleted, like map saves keep their backups
    defformatstring(bakname, "backups/%s.obr", name);
    string dst;
    copystring(dst, findfile(bakname, "w"));
    remove(dst);
    if(rename(src, dst)) { conoutf(colourred, "Could not remove prefab %s", name); return false; }
    uncacheprefab(name);
    conoutf(colourwhite, "Removed prefab %s (backup in backups/)", name);
    return true;
}
ICOMMAND(0, removeprefab, "s", (char *name), intret(!(identflags&IDF_MAP) && removeprefabfile(name) ? 1 : 0));

static bool renameprefabfile(const char *from, const char *to)
{
    string src;
    if(!finduserprefab(from, src)) return false;
    if(!validprefabpath(to)) { conoutf(colourred, "Invalid prefab path: %s", to); return false; }
    defformatstring(toname, "%s.obr", to);
    // Never shadow another prefab, shipped ones included. A case-only rename
    // names the same file on Windows, so it would always look taken.
    if(strcasecmp(from, to) && findfile(toname, "e")) { conoutf(colourred, "Prefab %s already exists", to); return false; }
    string dst;
    copystring(dst, findfile(toname, "w"));
    if(rename(src, dst)) { conoutf(colourred, "Could not rename prefab %s", from); return false; }
    uncacheprefab(from);
    uncacheprefab(to);
    conoutf(colourwhite, "Renamed prefab %s to %s", from, to);
    return true;
}
ICOMMAND(0, renameprefab, "ss", (char *from, char *to), intret(!(identflags&IDF_MAP) && renameprefabfile(from, to) ? 1 : 0));
```

Notes for the implementer:
- `findfile` returns a static buffer. That's why both results are copied with `copystring` before the next call.
- `findfile(…, "w")` creates the directories under the home dir (`stream.cpp:538`). `findfile(…, "e")` returns `NULL` when the file exists nowhere (home, working dir, package dirs, zips).
- `strcasecmp` maps to `_stricmp` on MSVC (`tools.h:322`).

- [ ] **Step 4: Document the commands**

In `config/usage.cfg`, replace the two prefab lines (`:620-621`) with:

```cubescript
setdesc "saveprefab" "saves the current geometry selection to a prefab file^n(does not faithfully copy textures)" "name"
setdesc "copyprefab" "loads a prefab file onto the clipboard, to be pasted with /paste^n(replaces any copied entities)" "name"
setdesc "prefabinfo" "returns 'sx sy sz grid user' for a prefab, or nothing if it cannot be read^nuser is 1 when the file is in the home directory" "name"
setdesc "removeprefab" "moves a user prefab to backups/ in the home directory^nreturns 1 on success" "prefab/path"
setdesc "renameprefab" "renames or moves a user prefab, refusing to replace any existing prefab^nreturns 1 on success" "prefab/from prefab/to"
```

- [ ] **Step 5: Build and run the test to verify it passes**

```powershell
tools\harness\harness.ps1 stop
```

```powershell
wsl -d Ubuntu -- '/mnt/f/Red Eclipse/src/build.sh' debug
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: `prefab self-test: all checks passed`, exit 0. If the **round-trip** check alone fails, the paste did not reproduce the block: compare `prefabinfo prefab/st/rt` with `prefab/st/box` and inspect the two decompressed files. That is feature/engine behaviour, not a harness problem.

- [ ] **Step 6: Commit**

```bash
git add src/engine/octaedit.cpp src/engine/world.cpp src/engine/engine.h config/usage.cfg tools/harness/prefab-selftest.ps1
git commit -m "engine: add prefab info, remove and rename commands"
```

---

### Task 3: Extend the file browser widget

`ui_tool_filelist` becomes usable as a docked prefab browser. Every addition defaults to today's behaviour. Its only caller today is `ui_tool_fileselect_picker`, which is used by `toolenv.cfg`, `toolimgedit.cfg`, `toolmap.cfg`, `toolmat.cfg` and `tooltex.cfg`.

**Files:**
- Modify: `config/tool/toolcommon.cfg:220-237` (file type constants and `tool_file_type_exts`)
- Modify: `config/ui/tool/toolview/widgets/toolfilelist.cfg`
- Modify: `tools/harness/prefab-selftest.ps1` (the Task 3 section)

**Interfaces:**
- Consumes: nothing from earlier tasks at the CubeScript level. The tile renders with the existing `uiprefabpreview`.
- Produces:
  - `TOOL_FILE_PREFAB` (= 5), with `tool_file_type_exts $TOOL_FILE_PREFAB` → `obr`.
  - `ui_tool_filelist` props: `p_root` (default `"data"`), `p_strip_ext` (default 1), `p_footer` (default 1), `p_refetch` (default 0), `p_instance` (default `""`), and `p_on_select`, `p_on_activate`, `p_on_item_menu`. Each of the last three is called with `$arg1` = the file's path relative to the root, with the extension stripped when `p_strip_ext` is set.
  - With `p_instance = <x>`, the widget's state lives in `tool_filelist_<x>_{curdir,dirs,files,numdirs,numfiles,sel_type,sel_index,sel_path,active_path,filter_query}`.
  - Helpers: `tool_filelist_instance_enter <x>`, `tool_filelist_instance_leave <x>`, `tool_filelist_relpath <name>`, `tool_filelist_dirpath <root>`, `tool_filelist_item_value <name>`, `tool_filelist_dedup <list>`.

- [ ] **Step 1: Write the failing test**

In `tools/harness/prefab-selftest.ps1`, replace the line `    # ==== file browser widget (Task 3 inserts here) =======================` with:

```powershell
    # ==== file browser widget =============================================

    Step 'file browser: prefab type and helpers' {
        Expect 'prefab type extension' (Eval '(tool_file_type_exts $TOOL_FILE_PREFAB)') 'obr'
        Expect 'dedup keeps one of each' (Eval '(listlen (tool_filelist_dedup "a b a c b"))') '3'
        Expect 'dedup keeps the first order' (Eval '(at (tool_filelist_dedup "a b a c b") 2)') 'c'
        Send 'tool_filelist_curdir = ""'
        Expect 'relpath at the root' (Eval '(tool_filelist_relpath oak)') 'oak'
        Expect 'dirpath at the root' (Eval '(tool_filelist_dirpath prefab)') 'prefab'
        Send 'tool_filelist_curdir = trees'
        Expect 'relpath in a folder' (Eval '(tool_filelist_relpath oak)') 'trees/oak'
        Expect 'dirpath in a folder' (Eval '(tool_filelist_dirpath prefab)') 'prefab/trees'
        Send 'tool_filelist_curdir = ""'
    }

    Step 'file browser: per-instance state' {
        Send 'tool_filelist_curdir = sounds; tool_filelist_filter_query = x; tool_filelist_instance_enter pfltest'
        Expect 'a fresh instance starts at the root' (Eval '$tool_filelist_curdir') ''
        Expect 'a fresh instance has no selection' (Eval '$tool_filelist_sel_index') '-1'
        Expect 'a fresh instance has no query' (Eval '$tool_filelist_filter_query') ''
        Send 'tool_filelist_curdir = st; tool_filelist_instance_leave pfltest'
        Expect "leave restores the caller's directory" (Eval '$tool_filelist_curdir') 'sounds'
        Expect "leave restores the caller's query" (Eval '$tool_filelist_filter_query') 'x'
        Expect 'leave keeps the instance directory' (Eval '(getalias tool_filelist_pfltest_curdir)') 'st'
        Send 'tool_filelist_instance_enter pfltest'
        Expect 're-entering restores the instance directory' (Eval '$tool_filelist_curdir') 'st'
        Send 'tool_filelist_instance_leave pfltest; tool_filelist_curdir = ""; tool_filelist_filter_query = ""'
    }

    Step 'file browser: an existing picker still browses data/' {
        Ed cursor on
        Send 'tool_filelist_curdir = ""; toolpanel_open tool_fileselect_picker center [p_title = "File browser"; p_width = (uiwidth 0.25); p_user_data = [p_var = mapmusic; p_file_type = 2; p_width = (uiwidth 0.25)]]' 600
        ExpectTrue 'the data/sounds folder is listed' ($null -ne (Find-Click 'sounds'))
        ExpectTrue 'the OK/Cancel footer is shown' ($null -ne (Find-Click 'Cancel'))
        # Look at this one: "Go up" (arrow icon, top left) must be greyed out at the root.
        & $harness shot prefab-picker-root 6>$null | Out-Null
        Send 'toolpanel_close tool_fileselect_picker' 300
        Ed cursor off
    }
```

These steps use a `Find-Click` helper, which Task 5 also uses. Add it to the helpers, after `BackupFile`:

```powershell
# Click point of the first drawn widget whose text matches $Label (exact
# first, then wildcard), as harness.ps1 find reports it; $null if none.
function Find-Click([string]$Label) {
    $out = @(& $harness find $Label 6>$null)
    foreach ($line in $out) {
        if ("$line" -match 'click\(([-0-9.]+),([-0-9.]+)\)') { return @($Matches[1], $Matches[2]) }
    }
    return $null
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: the Task 2 steps pass. The helper and instance steps fail (`got ''` / unknown alias). The picker step passes; it's a regression guard.

- [ ] **Step 3: Add the file type**

In `config/tool/toolcommon.cfg`, after `TOOL_FILE_ASSETPACK = 4`:

```cubescript
TOOL_FILE_PREFAB = 5
```

and extend `tool_file_type_exts` with a final case, so its `case` reads:

```cubescript
    case $arg1 @TOOL_FILE_ANY [
        result []
    ] @TOOL_FILE_IMAGE [
        result [@@tool_image_exts]
    ] @TOOL_FILE_SOUND [
        result [@@tool_sound_exts]
    ] @TOOL_FILE_CONFIG [
        result "cfg"
    ] @TOOL_FILE_PREFAB [
        result "obr"
    ]
```

(The `@TOOL_FILE_*` substitutions are the file's existing idiom. The constant must be defined above the alias, which it is.)

- [ ] **Step 4: Add the helpers and per-instance state**

In `config/ui/tool/toolview/widgets/toolfilelist.cfg`, after the line `tool_filelist_preview      = 1`:

```cubescript

// Widget state that p_instance swaps in and out, and each variable's value
// for an instance that has never been shown
tool_filelist_state_vars     = [curdir dirs files numdirs numfiles sel_type sel_index sel_path active_path filter_query]
tool_filelist_state_defaults = [[] [] [] 0 0 0 -1 [] [] []]

// Makes tool_filelist_<instance>_* the widget's state, stashing the current
// one. The UI is immediate-mode, so everything the widget runs happens between
// this and tool_filelist_instance_leave. A handler inside an instanced widget
// must not open another file picker: the picker's setup would be undone on leave.
// 1:<instance>
tool_filelist_instance_enter = [
    local _var _inst
    loop i (listlen $tool_filelist_state_vars) [
        _var  = (at $tool_filelist_state_vars $i)
        _inst = (concatword "tool_filelist_" $arg1 "_" $_var)
        set (concatword "tool_filelist_saved_" $_var) (getalias (concatword "tool_filelist_" $_var))
        if (identexists $_inst) [
            set (concatword "tool_filelist_" $_var) (getalias $_inst)
        ] [
            set (concatword "tool_filelist_" $_var) (at $tool_filelist_state_defaults $i)
        ]
    ]
]

// 1:<instance>
tool_filelist_instance_leave = [
    looplist _var $tool_filelist_state_vars [
        set (concatword "tool_filelist_" $arg1 "_" $_var) (getalias (concatword "tool_filelist_" $_var))
        set (concatword "tool_filelist_" $_var) (getalias (concatword "tool_filelist_saved_" $_var))
    ]
]

// Path of an item in the current directory, relative to the root
// 1:<name>
tool_filelist_relpath = [
    ? (=s $tool_filelist_curdir) $arg1 (concatword $tool_filelist_curdir "/" $arg1)
]

// The current directory as listfiles takes it
// 1:<root>
tool_filelist_dirpath = [
    ? (=s $tool_filelist_curdir) $arg1 (concatword $arg1 "/" $tool_filelist_curdir)
]

// What a file item reports to p_on_select, p_on_activate and p_on_item_menu
// 1:<name>
tool_filelist_item_value = [
    tool_filelist_relpath (? $p_strip_ext (filenoext $arg1) $arg1)
]

// listfiles merges the working, home and package directories, so one name can
// come back more than once
// 1:<list>
tool_filelist_dedup = [
    local _out
    _out = []
    looplist _item $arg1 [
        if (listhas $_out $_item) [] [
            append _out (escape $_item)
        ]
    ]
    result $_out
]
```

- [ ] **Step 5: Use `p_root` and dedup when fetching**

In `tool_filelist_fetchdir`, replace:

```cubescript
        tool_filelist_dirs  = (listfiles (concatword "data/" $tool_filelist_curdir) "" 2)
        tool_filelist_files = (listfiles (concatword "data/" $tool_filelist_curdir) "" 1)

        tool_filelist_rem_hidden tool_filelist_dirs
        tool_filelist_rem_hidden tool_filelist_files
```

with:

```cubescript
        tool_filelist_dirs  = (listfiles (tool_filelist_dirpath $p_root) "" 2)
        tool_filelist_files = (listfiles (tool_filelist_dirpath $p_root) "" 1)

        tool_filelist_rem_hidden tool_filelist_dirs
        tool_filelist_rem_hidden tool_filelist_files

        tool_filelist_dirs  = (tool_filelist_dedup $tool_filelist_dirs)
        tool_filelist_files = (tool_filelist_dedup $tool_filelist_files)
```

(With `curdir` empty this lists `data` where it used to list `data/`. `listfiles` strips trailing separators, so the two are the same.)

- [ ] **Step 6: Prefab tiles and item callbacks**

In `ui_tool_filelist_item`, add the prefab branch right after the directory branch. Replace:

```cubescript
                caseif [!= $arg4 0] [
                    uiimage "<grey>textures/icons/action" 0xffffff 0 $_icon_size $_icon_size
                ] [tool_file_isimage $arg2] [
```

with:

```cubescript
                caseif [!= $arg4 0] [
                    uiimage "<grey>textures/icons/action" 0xffffff 0 $_icon_size $_icon_size
                ] [= $p_file_type $TOOL_FILE_PREFAB] [
                    uiprefabpreview (concatword $p_root "/" (tool_filelist_relpath (filenoext $arg2))) 0xFFFFFF 1 $_icon_size $_icon_size [
                        uipreviewyaw (mod (div $totalmillis 10) 360)
                    ]
                ] [tool_file_isimage $arg2] [
```

At the end of the `uipress [ … ]` block, after the `previewsound` `if`, add:

```cubescript

                if (= $arg4 0) [
                    p_on_select (tool_filelist_item_value $arg2)
                ]
```

In the `uidoublepress … [ … ]` block, replace the file branch:

```cubescript
                ] [
                    tool_filelist_select $arg5
                ]
```

with:

```cubescript
                ] [
                    if (=s $p_on_activate) [
                        tool_filelist_select $arg5
                    ] [
                        p_on_activate (tool_filelist_item_value $arg2)
                    ]
                ]
```

Directly after the whole `uidoublepress … [ … ]` block (still inside `if $uidrawn [`), add:

```cubescript

            if (&& [= $arg4 0] [!=s $p_on_item_menu]) [
                uialtrelease [
                    p_on_item_menu (tool_filelist_item_value $arg2)
                ]

                uihover [
                    tool_rightclickable
                ]
            ]
```

- [ ] **Step 7: New props, instance swap, refetch, footer toggle, Up fix**

Replace `tool_filelist_props` with:

```cubescript
tool_filelist_props = [
    [ p_sel_size            0.08                      ]
    [ p_sel_area            0.45                      ]
    [ p_sel_space           0.0035                    ]
    [ p_filter_query_length 32                        ]
    [ p_columns             5                         ]
    [ p_slider_size         $ui_toolpanel_slider_size ]
    [ p_file_type           $TOOL_FILE_ANY            ]
    [ p_on_change           []                        ]
    [ p_get                 [[result $arg1]]          ]
    [ p_set                 [[result $arg1]]          ]
    [ p_width               0                         ]
    [ p_can_deselect        1                         ]
    [ p_strip_ext           1                         ]
    [ p_root                "data"                    ]
    [ p_footer              1                         ]
    [ p_refetch             0                         ]
    [ p_instance            ""                        ]
    [ p_on_select           []                        ]
    [ p_on_activate         []                        ]
    [ p_on_item_menu        []                        ]
]
```

(`p_strip_ext` used to be read from the calling picker's scope. The picker always passes it, so declaring it here changes nothing for existing callers.)

In `ui_tool_filelist`, replace:

```cubescript
    if $toolpanel_this_isinit [
        tool_filelist_get_active $arg1
        tool_filelist_fetchdir
    ]
```

with:

```cubescript
    if (!=s $p_instance) [
        tool_filelist_instance_enter $p_instance
    ]

    if (|| $toolpanel_this_isinit $p_refetch) [
        tool_filelist_get_active $arg1
        tool_filelist_fetchdir
    ]
```

Replace the "Go up" button's disabled check:

```cubescript
                    p_disabled   = (=s $tool_filelist_curdir "data")
```

with:

```cubescript
                    p_disabled   = (=s $tool_filelist_curdir)
```

Make the footer optional and leave the instance on exit. Replace everything from the `uivscroll` line after the scroll area to the end of `ui_tool_filelist` with the following. It's the existing footer wrapped in `if $p_footer [ … ]`, with no content changes. Its `@` / `@@` substitutions stay correct because each counts brackets relative to its immediately enclosing block, which the wrapper doesn't change.

```cubescript
            uivscroll $p_slider_size $p_sel_area 1
            uialign 1
        ]

        if $p_footer [
            uifill 0 $ui_tool_elem_space_l
            uiline $ui_tool_dark_accent_colour 0 0 [ uistyle clampx ]
            uifill 0 $ui_tool_elem_space_l

            uivlist $ui_tool_elem_space_l [
                uistyle clampx
                uigrid 2 0 $ui_tool_elem_space_s [
                    uitext "^fASelected: " $ui_tool_text_size_s
                    ui_tool_autoscroll_text $tool_filelist_sel_path [
                        p_width     = @(*f $p_width 0.8)
                        p_text_size = $ui_tool_text_size_s
                    ]

                    uitext "^fACurrent: " $ui_tool_text_size_s
                    ui_tool_autoscroll_text $tool_filelist_active_path [
                        p_width     = @(*f $p_width 0.8)
                        p_text_size = $ui_tool_text_size_s
                    ]

                    uialign -1
                ]

                if (= $p_file_type $TOOL_FILE_SOUND) [
                    ui_tool_checkbox tool_filelist_preview [
                        p_label = "Enable preview"
                    ]
                    uiprev [uialign -1]
                ]

                uihlist $ui_tool_elem_space_l [
                    if $p_can_deselect [
                        ui_tool_button [
                            p_icon       = $exittex
                            p_icon_size  = 0.015
                            p_on_click   = [
                                tool_param_set @arg1 "" [@@p_on_change]
                            ]
                            p_tip_simple = "Clear selection"
                        ]
                    ]

                    ui_tool_button [
                        p_label    = "Ok"
                        p_width    = 0.18
                        p_on_click = [
                            tool_filelist_select @arg1
                        ]
                    ]
                    ui_tool_button [
                        p_label    = "Cancel"
                        p_width    = 0.18
                        p_on_click = [
                            toolpanel_pop_close_this
                        ]
                    ]
                ]
            ]
        ]
    ]

    if (!=s $p_instance) [
        tool_filelist_instance_leave $p_instance
    ]
]
```

Before replacing, diff the footer in the file against the block above. If it has drifted from what this plan was written against, keep the file's version of the footer's contents and only add the wrapper and the `tool_filelist_instance_leave` tail.

- [ ] **Step 8: Reload and run the test**

UI changes need no rebuild. The selftest boots its own game, so just run it:

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: all checks pass. Open `home/uitest/harness/shots/prefab-picker-root.png` (the path is also printed by `harness.ps1 shot`) and confirm the "Go up" arrow is greyed out at the root and the grid lists `data/` folders.

- [ ] **Step 9: Commit**

```bash
git add config/tool/toolcommon.cfg config/ui/tool/toolview/widgets/toolfilelist.cfg tools/harness/prefab-selftest.ps1
git commit -m "ui: let the file browser host a prefab browser"
```

---

### Task 4: Prefab library and actions

**Files:**
- Create: `config/tool/toolprefab.cfg`
- Modify: `config/tool.cfg` (append after `exec "config/tool/toolimgedit.cfg"`)
- Modify: `tools/harness/prefab-selftest.ps1` (the Task 4 section)

**Interfaces:**
- Consumes: `prefabinfo`, `removeprefab`, `renameprefab`, `copyprefab`, `saveprefab` (Task 2); `tool_filelist_dedup`, `tool_filelist_rem_hidden` (Task 3, called at runtime only).
- Produces:
  - Constants: `TOOL_PREFAB_ROOT` = `prefab`.
  - State: `tool_prefab_cur` (path or `""`), `tool_prefab_cur_info` (`prefabinfo` of it), `tool_prefab_needs_rescan`, `tool_prefab_pending`, `tool_prefab_browse`.
  - Validation: `tool_prefab_name_valid <name>`, `tool_prefab_folder_valid <folder>` → 1/0.
  - Paths: `tool_prefab_path <folder> <name>`, `tool_prefab_path_folder <path>`, `tool_prefab_path_name <path>`.
  - Info: `tool_prefab_exists <path>`, `tool_prefab_info_user <info>`.
  - Operations: `tool_prefab_select <path|"">`, `tool_prefab_rescan`, `tool_prefab_load <path>`, `tool_prefab_save <path>`, `tool_prefab_overwrite <path>` (asks first), `tool_prefab_remove <path>` (asks first), `tool_prefab_remove_confirmed` (acts on `tool_prefab_pending`), `tool_prefab_rename <from> <to>`, `tool_prefab_list_folders`.
  - Actions in category `Prefabs`: `ta_prefabs`, `ta_prefab_save`, `ta_prefab_load`. `ta_prefabs` toggles panel `tool_prefabs`; `ta_prefab_save` opens popup `tool_prefab_save`. Task 5 defines both. There is deliberately **no** "paste prefab" action: loading puts the prefab on the one clipboard, so the existing paste places it (user decision, 2026-09-24).

- [ ] **Step 1: Write the failing test**

In `tools/harness/prefab-selftest.ps1`, replace `    # ==== prefab library (Task 4 inserts here) ============================` with:

```powershell
    # ==== prefab library ==================================================

    Step 'library: name and folder validation' {
        Expect 'plain name' (Eval '(tool_prefab_name_valid "oak_2-b.c")') '1'
        Expect 'empty name' (Eval '(tool_prefab_name_valid "")') '0'
        Expect 'dot-dot' (Eval '(tool_prefab_name_valid "..")') '0'
        Expect 'space' (Eval '(tool_prefab_name_valid "o k")') '0'
        Expect 'slash' (Eval '(tool_prefab_name_valid "a/b")') '0'
        Expect 'trailing dot (Windows strips it)' (Eval '(tool_prefab_name_valid "oak.")') '0'
        Expect 'root folder' (Eval '(tool_prefab_folder_valid "")') '1'
        Expect 'nested folder' (Eval '(tool_prefab_folder_valid "a/b")') '1'
        Expect 'double slash' (Eval '(tool_prefab_folder_valid "a//b")') '0'
        Expect 'leading slash' (Eval '(tool_prefab_folder_valid "/a")') '0'
        Expect 'trailing slash' (Eval '(tool_prefab_folder_valid "a/")') '0'
        Expect 'space in folder' (Eval '(tool_prefab_folder_valid "a b")') '0'
    }

    Step 'library: paths' {
        Expect 'root path' (Eval '(tool_prefab_path "" oak)') 'prefab/oak'
        Expect 'folder path' (Eval '(tool_prefab_path trees oak)') 'prefab/trees/oak'
        Expect 'folder of a nested path' (Eval '(tool_prefab_path_folder prefab/trees/big/oak)') 'trees/big'
        Expect 'folder of a root path' (Eval '(tool_prefab_path_folder prefab/oak)') ''
        Expect 'name of a path' (Eval '(tool_prefab_path_name prefab/trees/oak)') 'oak'
        Expect 'top-level folders' (Eval '(tool_prefab_list_folders)') '"st" "st2"'
    }

    Step 'library: actions are registered' {
        Expect 'Prefabs category' (Eval '$tool_actions_Prefabs') 'ta_prefabs ta_prefab_save ta_prefab_load'
    }

    Step 'library: save, load, rename, delete through the wrappers' {
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 1,1,3
        Send 'tool_prefab_save prefab/lib/one'
        ExpectTrue 'saved' (Test-Path (UserFile 'lib\one.obr'))
        Expect 'the new prefab is selected' (Eval '$tool_prefab_cur') 'prefab/lib/one'
        Expect 'its info is cached' (Eval '$tool_prefab_cur_info') '1 1 3 8 1'
        Expect 'a rescan is requested' (Eval '$tool_prefab_needs_rescan') '1'

        Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2,2
        Send 'entcopybuf = "playerstart 0"; pasteclear; tool_prefab_load prefab/lib/one'
        Expect 'loading clears the script entity clipboard' (Eval '$entcopybuf') ''
        Send 'cancelsel; pastehilight'
        Expect 'the clipboard holds the prefab' (EdState).Sel.SZ 3

        Send 'tool_prefab_rename prefab/lib/one prefab/lib/two'
        ExpectTrue 'renamed on disk' (Test-Path (UserFile 'lib\two.obr'))
        Expect 'the selection follows the rename' (Eval '$tool_prefab_cur') 'prefab/lib/two'

        # tool_prefab_remove asks first; its confirmed half does the work.
        Send 'tool_prefab_pending = prefab/lib/two; tool_prefab_remove_confirmed'
        ExpectTrue 'moved to backups' (Test-Path (BackupFile 'lib\two.obr'))
        Expect 'the selection is cleared' (Eval '$tool_prefab_cur') ''
    }
```

`tool_prefab_list_folders` must equal exactly `"st" "st2"` at that point. `st2` is left empty by the removal in Task 2's steps, and `lib` doesn't exist yet. If the order differs (the file system doesn't guarantee one), compare the sorted list instead. Don't loosen the check to "contains".

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: Tasks 2–3 steps pass; every library step fails (`got ''`).

- [ ] **Step 3: Implement the library**

Create `config/tool/toolprefab.cfg`:

```cubescript
// Prefab library, see doc/superpowers/specs/2026-09-24-map-editor-prefabs-design.md.
//
// Prefabs are geometry-only .obr files under prefab/, in the home directory
// (user prefabs) or the install/package directories (shipped, read-only). The
// editor always names one by its full relative path without extension, e.g.
// "prefab/trees/oak": the engine drops its prefab/ prefix whenever a name
// contains a '/', so a bare "trees/oak" would name a different file.

TOOL_PREFAB_ROOT       = "prefab"
TOOL_PREFAB_NAME_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-"

// Selected prefab, or ""
tool_prefab_cur          = ""
// prefabinfo of tool_prefab_cur: "sx sy sz grid user", or ""
tool_prefab_cur_info     = ""
// Makes the browser refetch its directory on its next build
tool_prefab_needs_rescan = 0
// Prefab a confirmation prompt is about
tool_prefab_pending      = ""
// Variable the browser widget reports its selection through
tool_prefab_browse       = ""



////////////////
// VALIDATION //
////////////////



// 1:<string> 2:<allowed characters>
tool_prefab_chars_valid = [
    local _valid
    _valid = 1
    loopwhile i (strlen $arg1) [$_valid] [
        if (< (strstr $arg2 (substr $arg1 $i 1)) 0) [
            _valid = 0
        ]
    ]
    result $_valid
]

// One path segment. Stricter than the engine's validprefabpath: a trailing
// '.' is refused too, because Windows silently strips it from folder names.
// 1:<name>
tool_prefab_name_valid = [
    local _len
    _len = (strlen $arg1)
    && [> $_len 0] [
        !=s (substr $arg1 (- $_len 1) 1) "."
    ] [
        tool_prefab_chars_valid $arg1 $TOOL_PREFAB_NAME_CHARS
    ]
]

// "" (the root), or segments separated by '/'
// 1:<folder>
tool_prefab_folder_valid = [
    if (=s $arg1) [
        result 1
    ] [
        local _valid _len
        _len   = (strlen $arg1)
        _valid = (&& [
            tool_prefab_chars_valid $arg1 (concatword $TOOL_PREFAB_NAME_CHARS "/")
        ] [
            < (strstr $arg1 "//") 0
        ] [
            !=s (substr $arg1 0 1) "/"
        ] [
            !=s (substr $arg1 (- $_len 1) 1) "/"
        ])

        if $_valid [
            looplist _seg (strreplace $arg1 "/" " ") [
                if (tool_prefab_name_valid $_seg) [] [
                    _valid = 0
                ]
            ]
        ]

        result $_valid
    ]
]



///////////
// PATHS //
///////////



// 1:<folder> 2:<name>
tool_prefab_path = [
    if (=s $arg1) [
        concatword $TOOL_PREFAB_ROOT "/" $arg2
    ] [
        concatword $TOOL_PREFAB_ROOT "/" $arg1 "/" $arg2
    ]
]

// "prefab/trees/oak" -> "trees", "prefab/oak" -> ""
// 1:<path>
tool_prefab_path_folder = [
    local _rel _slash
    _rel   = (substr $arg1 (+ (strlen $TOOL_PREFAB_ROOT) 1))
    _slash = (strrstr $_rel "/")
    if (< $_slash 0) [
        result ""
    ] [
        substr $_rel 0 $_slash
    ]
]

// "prefab/trees/oak" -> "oak"
// 1:<path>
tool_prefab_path_name = [
    substr $arg1 (+ (strrstr $arg1 "/") 1)
]

// Top-level folders under prefab/, from every search directory
tool_prefab_list_folders = [
    local _dirs
    _dirs = (tool_filelist_dedup (listfiles $TOOL_PREFAB_ROOT "" 2))
    tool_filelist_rem_hidden _dirs
    result $_dirs
]



//////////
// INFO //
//////////



// 1:<path>
tool_prefab_exists = [
    !=s (prefabinfo $arg1) ""
]

// Whether a prefabinfo result is a user prefab (in the home directory)
// 1:<info>
tool_prefab_info_user = [
    = (at $arg1 4) 1
]



////////////////
// OPERATIONS //
////////////////



// 1:<path, or "" to clear>
tool_prefab_select = [
    tool_prefab_cur = $arg1
    if (=s $arg1) [
        tool_prefab_cur_info = ""
    ] [
        tool_prefab_cur_info = (prefabinfo $arg1)
    ]
]

tool_prefab_rescan = [
    tool_prefab_needs_rescan = 1
]

// Loading replaces the whole clipboard (geometry and copied entities), so the
// notice says so
tool_prefab_paste_hint = [
    local _bind
    _bind = (tool_action_pretty_bind_info ta_paste)
    if (=s $_bind) [
        result "Replaced your copied selection. Paste to place it."
    ] [
        concat "Replaced your copied selection. Paste with" $_bind "to place it."
    ]
]

// 1:<path>
tool_prefab_load = [
    caseif (=s $arg1) [
        tool_info_show "No prefab selected"
    ] (! (tool_prefab_exists $arg1)) [
        tool_info_show "Could not load prefab" [
            p_subtext = $arg1
        ]
    ] () [
        // editpaste replaces entities instead of pasting geometry while the
        // script-side entity clipboard is set (config/engine.cfg)
        entcopybuf = ""
        copyprefab $arg1
        tool_info_show "Prefab loaded to clipboard" [
            p_subtext = (tool_prefab_paste_hint)
        ]
    ]
]

// 1:<path>
tool_prefab_save = [
    if $havesel [
        saveprefab $arg1
        // saveprefab only reports failure on the console; a failed write
        // drops the prefab from the cache, so this asks the disk
        if (tool_prefab_exists $arg1) [
            tool_prefab_select $arg1
            tool_prefab_rescan
            tool_info_show "Saved prefab" [
                p_subtext = $arg1
            ]
        ] [
            tool_info_show "Could not save prefab" [
                p_subtext = $arg1
            ]
        ]
    ] [
        tool_info_show "Select geometry first"
    ]
]

// 1:<path>
tool_prefab_overwrite = [
    tool_prefab_pending = $arg1
    tool_confirm_prompt (format "Overwrite %1 with the selection?" $arg1) [
        tool_prefab_save $tool_prefab_pending
    ]
]

// 1:<path>
tool_prefab_remove = [
    tool_prefab_pending = $arg1
    tool_confirm_prompt (format "Delete %1? (a copy is kept in backups/)" $arg1) [
        tool_prefab_remove_confirmed
    ]
]

tool_prefab_remove_confirmed = [
    if (removeprefab $tool_prefab_pending) [
        if (=s $tool_prefab_pending $tool_prefab_cur) [
            tool_prefab_select ""
        ]
        tool_prefab_rescan
        tool_info_show "Deleted prefab" [
            p_subtext = $tool_prefab_pending
        ]
    ] [
        tool_info_show "Could not delete prefab" [
            p_subtext = $tool_prefab_pending
        ]
    ]
]

// 1:<from> 2:<to>
tool_prefab_rename = [
    if (renameprefab $arg1 $arg2) [
        tool_prefab_select $arg2
        tool_prefab_rescan
        tool_info_show "Renamed prefab" [
            p_subtext = $arg2
        ]
    ] [
        tool_info_show "Could not rename prefab" [
            p_subtext = $arg2
        ]
    ]
]



/////////////
// ACTIONS //
/////////////



tool_action ta_prefabs [
    p_short_desc = "Prefabs panel"
    p_long_desc  = "Open the prefab browser"
    p_icon       = "<grey>textures/icons/edit/cube"
    p_category   = "Prefabs"
    p_code       = [
        toolpanel_toggle tool_prefabs right [
            p_title       = "Prefabs"
            p_clear_stack = 1
        ]
    ]
]

tool_action ta_prefab_save [
    p_short_desc = "Save selection as prefab"
    p_icon       = "<grey>textures/icons/edit/new"
    p_category   = "Prefabs"
    p_code       = [
        if $havesel [
            toolpanel_open tool_prefab_save popup [
                p_position = (uicursorpos)
                p_width    = 0.3
            ]
        ] [
            tool_info_show_action "Select geometry first" ta_prefab_save
        ]
    ]
]

tool_action ta_prefab_load [
    p_short_desc = "Load prefab to clipboard"
    p_long_desc  = "Load the prefab selected in the prefab browser onto the clipboard"
    p_icon       = "<grey>textures/icons/edit/copy"
    p_category   = "Prefabs"
    p_code       = [
        tool_prefab_load $tool_prefab_cur
    ]
]

```

In `config/tool.cfg`, append:

```cubescript
exec "config/tool/toolprefab.cfg"
```

Notes for the implementer:
- `tool_prefab_overwrite` and `tool_prefab_remove` hand `tool_confirm_prompt` code that runs later, from the menu window. That's why it reads the global `tool_prefab_pending` instead of `$arg1`.
- The Delete prompt deliberately does **not** pass `p_noundo_warn = 1`. That option prints "This operation cannot be undone!", which isn't true here: the file is kept in `backups/`. This deviates from the spec; Task 6 records it.
- No paste action of its own: after a Load, the existing `ta_paste` (Ctrl+V) places the prefab. Don't add one; the user ruled it out.

- [ ] **Step 4: Run the test to verify it passes**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: all checks pass.

- [ ] **Step 5: Commit**

```bash
git add config/tool/toolprefab.cfg config/tool.cfg tools/harness/prefab-selftest.ps1
git commit -m "tool: add prefab library and actions"
```

---

### Task 5: Prefabs panel, popups, toolbar button, bind

**Files:**
- Create: `config/ui/tool/toolprefab.cfg`
- Modify: `config/ui/tool.cfg` (append after `exec "config/ui/tool/toolimgedit.cfg"`, before `toolview_init`)
- Modify: `config/ui/tool/toolview/toolbar.cfg` (`toolbar_init`)
- Modify: `config/tool/binds/default.cfg`
- Modify: `tools/harness/prefab-selftest.ps1` (the Task 5 sections)

**Interfaces:**
- Consumes: everything Task 4 produces; `ui_tool_filelist` props from Task 3.
- Produces: panel `tool_prefabs` (content `ui_tool_prefabs`, hook `ui_tool_prefabs_on_open`); popups `tool_prefab_save` and `tool_prefab_rename`; context menu `tool_prefab_menu <path>` with `tool_prefab_menu_disabled <idx>` and `tool_prefab_menu_do <idx>`; form state `tool_prefab_form_{name,folder,newfolder,folders}` with `tool_prefab_form_set_folder <folder>`, `tool_prefab_form_get_folder`, `tool_prefab_form_target`, `tool_prefab_form_valid`.

- [ ] **Step 1: Write the failing test**

Add a click helper to `tools/harness/prefab-selftest.ps1`, after `Find-Click`:

```powershell
# Clicks a widget by label the way harness.ps1 click does (move, let a frame
# see the hover, press, release), optionally twice or with the alt button.
# If a synthesised double or alt click does not register while the same UI
# works by hand, that is a harness problem: stash and report, per the plan.
function Invoke-Click([string]$Label, [switch]$Double, [switch]$Right) {
    $pt = Find-Click $Label
    if (-not $pt) { throw "no drawn widget matching '$Label'" }
    $code = if ($Right) { -3 } else { -1 }
    $second = if ($Double) { "sleep 40 [ uikeypress $code 1; sleep 40 [ uikeypress $code 0 ] ]" } else { '' }
    $script = "uisetcursor $($pt[0]) $($pt[1])`nsleep 100 [ uikeypress $code 1; sleep 40 [ uikeypress $code 0; $second ] ]"
    & $harness send $script -Settle 600 6>$null | Out-Null
}

function Open-PrefabPanel {
    Send 'if (toolpanel_isopen tool_prefabs) [] [tool_do_action ta_prefabs]' 500
}
```

Replace `    # ==== panel: empty library (Task 5 inserts here) ======================` with:

```powershell
    # ==== panel: empty library ============================================

    Step 'panel: empty library' {
        Ed cursor on
        Open-PrefabPanel
        Expect 'panel is open' (Eval '(toolpanel_isopen tool_prefabs)') '1'
        ExpectTrue 'empty-state text is shown' ($null -ne (Find-Click 'No prefabs yet.'))
        & $harness shot prefab-empty 6>$null | Out-Null
        Send 'toolpanel_close tool_prefabs' 300
        Ed cursor off
    }
```

Replace `    # ==== prefabs panel (Task 5 inserts here) =============================` with:

```powershell
    # ==== prefabs panel ===================================================

    Step 'panel: toolbar button and default binds' {
        Expect 'toolbar has the Prefabs button' (Eval '(listhas $toolbar_actions_right ta_prefabs)') '1'
        # Checked in the file, not by exec'ing the preset: that would rebind
        # the harness home's keys for every later suite.
        $binds = Join-Path $RepoRoot 'config\tool\binds\default.cfg'
        ExpectTrue 'default preset binds I to the panel' (Select-String -Path $binds -Pattern '^toolbind I ta_prefabs$' -Quiet)
        $taken = @(Select-String -Path (Join-Path $RepoRoot 'config\setup.cfg'), $binds -Pattern '^\s*(tool|edit)bind I\b')
        Expect 'nothing else binds I in edit mode' $taken.Count 1
    }

    Step 'panel: browse into a folder, select, search' {
        Ed cursor on
        Send 'tool_filelist_prefab_curdir = ""; tool_prefab_select ""'
        Open-PrefabPanel
        ExpectTrue 'folder st is listed' ($null -ne (Find-Click 'st'))
        Invoke-Click 'st' -Double
        Expect 'double-click entered the folder' (Eval '(getalias tool_filelist_prefab_curdir)') 'st'
        Invoke-Click 'box'
        Expect 'click selects the prefab' (Eval '$tool_prefab_cur') 'prefab/st/box'
        Expect 'the header has its info' (Eval '$tool_prefab_cur_info') '3 1 1 8 1'
        # Look at this one: the header preview must be a 3x1x1 bar (the
        # overwrite in the engine steps), not the original 2x2x2 block.
        & $harness shot prefab-panel 6>$null | Out-Null
        Send 'tool_filelist_prefab_filter_query = zz' 400
        ExpectTrue 'search hides non-matching tiles' ($null -eq (Find-Click 'box'))
        Send 'tool_filelist_prefab_filter_query = ""' 400
        ExpectTrue 'clearing the search shows it again' ($null -ne (Find-Click 'box'))
    }

    Step 'panel: double-click loads the prefab to the clipboard' {
        Send 'pasteclear; cancelsel'
        Invoke-Click 'box' -Double
        Send 'pastehilight'
        $s = EdState
        Expect 'clipboard size x' $s.Sel.SX 3
        Expect 'clipboard size y' $s.Sel.SY 1
        Expect 'clipboard size z' $s.Sel.SZ 1
    }

    Step 'panel: the context menu respects read-only prefabs' {
        Invoke-Click 'box' -Right
        Expect 'menu opened' (Eval '(toolpanel_isopen toolpanel_menu)') '1'
        Expect 'Rename enabled for a user prefab' (Eval '(tool_prefab_menu_disabled 2)') '0'
        Expect 'Delete enabled for a user prefab' (Eval '(tool_prefab_menu_disabled 3)') '0'
        Send 'toolpanel_close toolpanel_menu' 300
        Send 'tool_filelist_prefab_curdir = ""; tool_prefab_rescan' 400
        Invoke-Click 'zz_selftest_shipped' -Right
        Expect 'Overwrite disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 1)') '1'
        Expect 'Rename disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 2)') '1'
        Expect 'Delete disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 3)') '1'
        # Look at this one: three greyed-out items.
        & $harness shot prefab-menu-shipped 6>$null | Out-Null
        Send 'toolpanel_close toolpanel_menu' 300
    }

    Step 'panel: the save popup saves, and offers Overwrite for an existing name' {
        Send 'tool_filelist_prefab_curdir = st; tool_prefab_rescan' 400
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 1,2,1
        Invoke-Click 'Save...'
        Expect 'popup opened' (Eval '(toolpanel_isopen tool_prefab_save)') '1'
        Expect 'folder defaults to the browser folder' (Eval '(tool_prefab_form_get_folder)') 'st'
        Send 'tool_prefab_form_name = box' 300
        ExpectTrue 'an existing name offers Overwrite' ($null -ne (Find-Click 'Overwrite'))
        & $harness shot prefab-save-overwrite 6>$null | Out-Null
        Send 'tool_prefab_form_name = newone' 300
        Invoke-Click 'Save'
        ExpectTrue 'saved' (Test-Path (UserFile 'st\newone.obr'))
        Expect 'the new prefab is selected' (Eval '$tool_prefab_cur') 'prefab/st/newone'
        Expect 'popup closed' (Eval '(toolpanel_isopen tool_prefab_save)') '0'
    }

    Step 'panel: the rename popup renames, and refuses clashes' {
        Send 'toolpanel_open tool_prefab_rename popup [p_position = [0.4 0.3]; p_width = 0.3]' 400
        Expect 'name prefilled' (Eval '$tool_prefab_form_name') 'newone'
        Expect 'folder prefilled' (Eval '(tool_prefab_form_get_folder)') 'st'
        Send 'tool_prefab_form_name = box' 300
        ExpectTrue 'the clash is reported' ($null -ne (Find-Click 'A prefab with that name already exists'))
        Send 'tool_prefab_form_name = renamed' 300
        Invoke-Click 'Rename'
        ExpectTrue 'renamed on disk' (Test-Path (UserFile 'st\renamed.obr'))
        Expect 'the selection follows' (Eval '$tool_prefab_cur') 'prefab/st/renamed'
    }

    Step 'panel: delete asks, then moves the file to backups/' {
        Send 'tool_prefab_remove prefab/st/renamed' 400
        ExpectTrue 'nothing happens before confirming' (Test-Path (UserFile 'st\renamed.obr'))
        Invoke-Click 'Confirm'
        ExpectTrue 'moved to backups' (Test-Path (BackupFile 'st\renamed.obr'))
        ExpectTrue 'gone from the library' (-not (Test-Path (UserFile 'st\renamed.obr')))
    }

    Step 'panel: the browser keeps its own state beside a file picker' {
        Send 'tool_filelist_prefab_curdir = st; tool_prefab_rescan' 400
        Send 'tool_filelist_curdir = ""; toolpanel_open tool_fileselect_picker center [p_title = "File browser"; p_width = (uiwidth 0.25); p_user_data = [p_var = mapmusic; p_file_type = 2; p_width = (uiwidth 0.25)]]' 600
        Invoke-Click 'sounds' -Double
        Expect 'the picker navigated' (Eval '$tool_filelist_curdir') 'sounds'
        Expect 'the prefab browser did not move' (Eval '(getalias tool_filelist_prefab_curdir)') 'st'
        ExpectTrue 'the prefab browser still shows its tiles' ($null -ne (Find-Click 'box'))
        & $harness shot prefab-with-picker 6>$null | Out-Null
        Send 'toolpanel_close tool_fileselect_picker' 300
        Ed cursor off
    }
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: every panel step fails (the panel doesn't exist; `toolpanel_isopen tool_prefabs` → `0`). Earlier sections still pass.

- [ ] **Step 3: Implement the panel**

Create `config/ui/tool/toolprefab.cfg`:

```cubescript
// Prefabs panel and its popups; the library is config/tool/toolprefab.cfg.

ui_tool_prefab_preview_size = 0.1

// Whether prefab/ has nothing in any search directory; refreshed on rescans
tool_prefab_is_empty = 0



//////////////////
// CONTEXT MENU //
//////////////////



// The menu's props are re-evaluated every frame from the menu window, so the
// prefab it is about lives in globals rather than locals.
tool_prefab_menu_path = ""
tool_prefab_menu_info = ""
tool_prefab_menu_tips = []

// 1:<path>
tool_prefab_menu = [
    local _user _ro
    tool_prefab_menu_path = $arg1
    tool_prefab_menu_info = (prefabinfo $arg1)
    _user = (tool_prefab_info_user $tool_prefab_menu_info)
    _ro   = (? $_user "" "Shipped prefab (read-only)")

    tool_prefab_menu_tips = (concat "[]" (escape (? $_user (? $havesel "" "Nothing selected") $_ro)) (escape $_ro) (escape $_ro))

    toolpanel_open_menu [
        p_width      = 0.2
        p_item_names = [
            "Load to clipboard"
            "Overwrite with selection"
            "Rename / move..."
            "Delete"
        ]
        p_tips       = $tool_prefab_menu_tips
        p_disabled   = [tool_prefab_menu_disabled $arg1]
        p_on_select  = [tool_prefab_menu_do $arg1]
    ]
]

// 1:<item index>
tool_prefab_menu_disabled = [
    local _user
    _user = (tool_prefab_info_user $tool_prefab_menu_info)
    case $arg1 1 [
        || [! $_user] [! $havesel]
    ] 2 [
        ! $_user
    ] 3 [
        ! $_user
    ] () [
        result 0
    ]
]

// 1:<item index>
tool_prefab_menu_do = [
    case $arg1 0 [
        tool_prefab_select $tool_prefab_menu_path
        tool_prefab_load $tool_prefab_menu_path
    ] 1 [
        tool_prefab_overwrite $tool_prefab_menu_path
    ] 2 [
        tool_prefab_select $tool_prefab_menu_path
        toolpanel_open tool_prefab_rename popup [
            p_position = (uicursorpos)
            p_width    = 0.3
        ]
    ] 3 [
        tool_prefab_remove $tool_prefab_menu_path
    ]
]



//////////
// FORM //
//////////



// Shared by the Save and Rename popups
tool_prefab_form_name      = ""
// Index into "(root)", tool_prefab_form_folders, "New folder..."
tool_prefab_form_folder    = 0
tool_prefab_form_newfolder = ""
tool_prefab_form_folders   = []

// Points the form at a folder: a top-level one by index, anything else as a
// new folder text
// 1:<folder>
tool_prefab_form_set_folder = [
    local _idx
    tool_prefab_form_folders = (tool_prefab_list_folders)
    _idx = (indexof $tool_prefab_form_folders $arg1)
    caseif (=s $arg1) [
        tool_prefab_form_folder    = 0
        tool_prefab_form_newfolder = ""
    ] (>= $_idx 0) [
        tool_prefab_form_folder    = (+ $_idx 1)
        tool_prefab_form_newfolder = ""
    ] () [
        tool_prefab_form_folder    = (+ (listlen $tool_prefab_form_folders) 1)
        tool_prefab_form_newfolder = $arg1
    ]
]

// The folder the form points at, "" for the root
tool_prefab_form_get_folder = [
    caseif (= $tool_prefab_form_folder 0) [
        result ""
    ] (<= $tool_prefab_form_folder (listlen $tool_prefab_form_folders)) [
        at $tool_prefab_form_folders (- $tool_prefab_form_folder 1)
    ] () [
        result $tool_prefab_form_newfolder
    ]
]

tool_prefab_form_target = [
    tool_prefab_path (tool_prefab_form_get_folder) $tool_prefab_form_name
]

tool_prefab_form_valid = [
    && [tool_prefab_name_valid $tool_prefab_form_name] [tool_prefab_folder_valid (tool_prefab_form_get_folder)]
]

ui_tool_prefab_form_fields = [
    ui_tool_textinput tool_prefab_form_name 32 [
        p_label  = "Name"
        p_prompt = "[prefab name]"
    ]

    ui_tool_dropdown tool_prefab_form_folder (concat "[(root)]" $tool_prefab_form_folders "[New folder...]") [
        p_label = "Folder"
        p_width = 0.15
    ]

    if (> $tool_prefab_form_folder (listlen $tool_prefab_form_folders)) [
        ui_tool_textinput tool_prefab_form_newfolder 48 [
            p_label  = "New folder"
            p_prompt = "[e.g. trees or trees/oak]"
        ]
    ]

    if (&& [!=s $tool_prefab_form_name] [! (tool_prefab_form_valid)]) [
        uicolourtext "Use letters, digits and _ - . only" $ui_tool_warn_colour $ui_tool_text_size_xs
    ]
]



////////////////
// SAVE POPUP //
////////////////



ui_tool_prefab_save_on_open = [
    tool_prefab_form_name = ""
    // Default to the top-level folder the browser is showing
    tool_prefab_form_set_folder (at (strreplace (getalias tool_filelist_prefab_curdir) "/" " ") 0)
]

tool_prefab_save_form = [
    local _target
    _target = (tool_prefab_form_target)
    // Close first: a confirmation opened from inside the popup would be
    // closed along with it, as its child
    toolpanel_close tool_prefab_save
    if (tool_prefab_exists $_target) [
        tool_prefab_overwrite $_target
    ] [
        tool_prefab_save $_target
    ]
]

ui_tool_prefab_save = [
    local _valid _exists _size
    _valid  = (tool_prefab_form_valid)
    _exists = (&& $_valid [tool_prefab_exists (tool_prefab_form_target)])

    if $havesel [
        _size = (format "^fASize: ^fw%1x%2x%3 @ grid %4" (getenginestat 30) (getenginestat 31) (getenginestat 32) (getenginestat 41))
    ] [
        _size = "^foNothing selected"
    ]

    uispace $ui_tool_elem_space_l $ui_tool_elem_space_l [
        uistyle clampx
        uivlist $ui_tool_elem_space_l [
            uistyle clampx

            uitext "Save selection as prefab" $ui_tool_text_size_s
            ui_tool_prefab_form_fields
            uitext $_size $ui_tool_text_size_xs

            ui_tool_button [
                p_label      = (? $_exists "Overwrite" "Save")
                p_width      = 0.2
                p_disabled   = (! (&& $_valid $havesel))
                p_tip_simple = (? $_exists "Replaces it. A shipped prefab gets a user copy that takes its place." "")
                p_on_click   = [
                    tool_prefab_save_form
                ]
            ]

            uipropchild [uialign -1]
        ]
    ]
]



//////////////////
// RENAME POPUP //
//////////////////



tool_prefab_rename_from = ""

ui_tool_prefab_rename_on_open = [
    tool_prefab_rename_from = $tool_prefab_cur
    tool_prefab_form_name   = (tool_prefab_path_name $tool_prefab_cur)
    tool_prefab_form_set_folder (tool_prefab_path_folder $tool_prefab_cur)
]

tool_prefab_rename_form = [
    local _target
    _target = (tool_prefab_form_target)
    toolpanel_close tool_prefab_rename
    tool_prefab_rename $tool_prefab_rename_from $_target
]

ui_tool_prefab_rename = [
    local _target _valid _same _taken
    _target = (tool_prefab_form_target)
    _valid  = (tool_prefab_form_valid)
    // A case-only rename names the same file on Windows
    _same   = (=s (strlower $_target) (strlower $tool_prefab_rename_from))
    _taken  = (&& $_valid [! $_same] [tool_prefab_exists $_target])

    uispace $ui_tool_elem_space_l $ui_tool_elem_space_l [
        uistyle clampx
        uivlist $ui_tool_elem_space_l [
            uistyle clampx

            uitext "Rename or move prefab" $ui_tool_text_size_s
            uicolourtext $tool_prefab_rename_from $ui_tool_dark_accent_colour $ui_tool_text_size_xs
            ui_tool_prefab_form_fields

            if $_taken [
                uicolourtext "A prefab with that name already exists" $ui_tool_warn_colour $ui_tool_text_size_xs
            ]

            ui_tool_button [
                p_label    = "Rename"
                p_width    = 0.2
                p_disabled = (|| [! $_valid] $_taken [=s $_target $tool_prefab_rename_from])
                p_on_click = [
                    tool_prefab_rename_form
                ]
            ]

            uipropchild [uialign -1]
        ]
    ]
]



///////////
// PANEL //
///////////



ui_tool_prefabs_on_open = [
    tool_prefab_rescan
    // Re-read the selected prefab: its file may have changed meanwhile
    tool_prefab_select $tool_prefab_cur
]

ui_tool_prefab_header = [
    local _info _has
    _info = $tool_prefab_cur_info
    _has  = (!=s $_info)

    uihlist $ui_tool_elem_space_l [
        uistyle clampx

        uicolour 0 $ui_tool_prefab_preview_size $ui_tool_prefab_preview_size [
            if $_has [
                uiprefabpreview $tool_prefab_cur 0xFFFFFF 1 $ui_tool_prefab_preview_size $ui_tool_prefab_preview_size [
                    uipreviewyaw (mod (div $totalmillis 10) 360)
                ]
            ] [
                uioutline $ui_tool_dark_accent_colour
                uiprev [uistyle clampxy]
            ]
        ]

        uivlist $ui_tool_elem_space_s [
            if $_has [
                uitext (substr $tool_prefab_cur (+ (strlen $TOOL_PREFAB_ROOT) 1)) $ui_tool_text_size_s
                uitext (format "^fASize: ^fw%1x%2x%3 @ grid %4" (at $_info 0) (at $_info 1) (at $_info 2) (at $_info 3)) $ui_tool_text_size_xs
                uicolourtext (? (tool_prefab_info_user $_info) "User prefab" "Shipped prefab (read-only)") $ui_tool_dark_accent_colour $ui_tool_text_size_xs
            ] [
                uitext "No prefab selected" $ui_tool_text_size_s
            ]
            uicolourtext "Textures follow this map's slot order" $ui_tool_dark_accent_colour $ui_tool_text_size_xs

            uihlist $ui_tool_elem_space_s [
                ui_tool_button [
                    p_label      = "Load to clipboard"
                    p_label_size = $ui_tool_text_size_xs_unscaled
                    p_tip_action = ta_prefab_load
                    p_disabled   = (! $_has)
                    p_on_click   = [
                        tool_do_action ta_prefab_load
                    ]
                ]
                ui_tool_button [
                    p_label      = "Save..."
                    p_label_size = $ui_tool_text_size_xs_unscaled
                    p_tip_action = ta_prefab_save
                    p_disabled   = (! $havesel)
                    p_on_click   = [
                        tool_do_action ta_prefab_save
                    ]
                ]
            ]

            uipropchild [uialign -1]
        ]
    ]
]

ui_tool_prefabs = [
    if $tool_prefab_needs_rescan [
        tool_prefab_is_empty = (=s (listfiles $TOOL_PREFAB_ROOT "" 0))
    ]

    uivlist $ui_toolpanel_elem_space [
        uistyle clampx

        ui_tool_prefab_header
        uiline $ui_tool_dark_accent_colour 0 0 [ uistyle clampx ]

        if $tool_prefab_is_empty [
            uitext "No prefabs yet." $ui_tool_text_size_s
            uitext "Select geometry and press Save... to create one." $ui_tool_text_size_xs
        ]

        ui_tool_filelist tool_prefab_browse [
            p_root         = $TOOL_PREFAB_ROOT
            p_file_type    = $TOOL_FILE_PREFAB
            p_instance     = prefab
            p_footer       = 0
            p_can_deselect = 0
            p_columns      = 3
            p_sel_size     = 0.1
            p_sel_area     = 0.5
            p_width        = (uiwidth 0.18)
            p_refetch      = $tool_prefab_needs_rescan
            p_on_select    = [
                tool_prefab_select (concatword $TOOL_PREFAB_ROOT "/" $arg1)
            ]
            p_on_activate  = [
                tool_prefab_select (concatword $TOOL_PREFAB_ROOT "/" $arg1)
                tool_prefab_load $tool_prefab_cur
            ]
            p_on_item_menu = [
                tool_prefab_menu (concatword $TOOL_PREFAB_ROOT "/" $arg1)
            ]
        ]

        tool_prefab_needs_rescan = 0
    ]
]
```

Notes for the implementer:
- The popups deliberately don't auto-focus their name field. A focused, immediate `uifield` writes its own text back into the variable every frame (`ui.cpp:5931-5941`). With auto-focus, programmatic prefills and the selftest's variable-based input would be overwritten. This matches the entity-template popups, which don't auto-focus either.
- `tool_prefab_menu`'s tip list is `[]` (no tip for Load) followed by three escaped strings. An empty string means "no tip".
- `uipreviewyaw` is a `Preview` command (`ui.cpp:6332`), so it applies to `uiprefabpreview`. It's the same call that `ui_tool_preview_spin` inlines.
- Control registration (`tool_register_control`) is dropped. Every panel function is an action, and actions are already searchable (F3). This deviates from the spec; Task 6 records it.

In `config/ui/tool.cfg`, add after `exec "config/ui/tool/toolimgedit.cfg"` (before `toolview_init`):

```cubescript
exec "config/ui/tool/toolprefab.cfg"
```

In `config/ui/tool/toolview/toolbar.cfg` `toolbar_init`, after `append toolbar_actions_right ta_ents`:

```cubescript
    append toolbar_actions_right ta_prefabs
```

In `config/tool/binds/default.cfg`, after `toolbind F4 ta_textures`:

```cubescript
toolbind I ta_prefabs
```

(The spec proposed F6, but F1–F9 are all bound in edit mode, F6 to `ta_ents` in `config/setup.cfg:329`, and F10/F11 take screenshots globally. `I` is the only letter with no edit-mode bind.)

- [ ] **Step 4: Run the test to verify it passes**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: all checks pass. If only the checks that depend on **synthesised double-click or right-click** fail (`double-click entered the folder`, `double-click loads…`, `menu opened`, the `sounds` navigation), confirm by hand in a running game (`harness.ps1 start`, open the panel, double-click and right-click a tile). If the UI works by hand, the harness can't synthesise that input, which is a **harness problem**: stash and report, per the Global Constraints.

- [ ] **Step 5: Review the screenshots**

Open each PNG under `home/uitest/harness/shots/` and check:

| Shot | Must show |
|---|---|
| `prefab-empty` | The Prefabs panel with "No prefabs yet." and an empty header outline |
| `prefab-panel` | Header with a spinning 3×1×1 bar (the overwrite, **not** the original 2×2×2 block), "st/box", "Size: 3x1x1 @ grid 8", "User prefab"; a grid of tiles with names |
| `prefab-menu-shipped` | The four-item menu; items 2–4 greyed out |
| `prefab-save-overwrite` | The Save popup with Name, Folder `st`, the size line, and an **Overwrite** button |
| `prefab-with-picker` | The Prefabs panel still inside `st` next to the file picker showing `sounds` |

Fix layout problems by editing `config/ui/tool/toolprefab.cfg`. Tune `p_sel_size`, `p_columns`, `p_width` and `ui_tool_prefab_preview_size` by eye so tiles don't clip in the right panel. Then `tools\harness\harness.ps1 reload config/ui/tool/toolprefab.cfg`, close and reopen the panel, and reshoot. Rerun the selftest after any change.

- [ ] **Step 6: Commit**

```bash
git add config/ui/tool/toolprefab.cfg config/ui/tool.cfg config/ui/tool/toolview/toolbar.cfg config/tool/binds/default.cfg tools/harness/prefab-selftest.ps1
git commit -m "ui: add prefabs panel"
```

---

### Task 6: Documentation and full regression

**Files:**
- Modify: `tools/harness/README.md`
- Modify: `doc/agent-handoff.md` (§3 current state, §4 test suites)
- Modify: `doc/superpowers/specs/2026-09-24-map-editor-prefabs-design.md` (add a "Deviations during implementation" section)

**Interfaces:**
- Consumes: everything above.
- Produces: documentation only.

- [ ] **Step 1: Document the selftest**

In `tools/harness/README.md`, next to the `editor-selftest.ps1` documentation, add a section:

````markdown
### Prefab self-test

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1 [-KeepRunning]
```

Starts its own game and checks the prefab engine commands (`prefabinfo`,
`removeprefab`, `renameprefab`, `copyprefab`/`saveprefab` fixes), the file
browser widget's prefab mode and per-instance state, the CubeScript prefab
library, and the Prefabs panel (clicks, double-clicks, context menu, popups).

- Wipes `home/uitest/prefab/` and `home/uitest/backups/prefab/` first.
- Simulates a shipped (read-only) prefab with `<repo>/prefab/zz_selftest_shipped.obr`,
  deleted on exit together with `<repo>/prefab/` if the script created it.
- Writes screenshots `prefab-*.png` for review; the script's comments say what
  each must show.
- Not covered: `saveprefab` on a selection over 100 MB, and `IDF_MAP`
  refusal of the new commands. Neither is reachable from the harness.
````

- [ ] **Step 2: Update the handoff**

In `doc/agent-handoff.md`:
- §3 Current state: add the prefab work (branch, commits if any were made, "UI in `config/ui/tool/toolprefab.cfg`, library in `config/tool/toolprefab.cfg`, engine in `octaedit.cpp`"). Link the spec and this plan.
- §4 test-suite table: add a row: `Prefab self-test | starts its own game | ~2 min | powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1`.

- [ ] **Step 3: Record deviations in the spec**

Append to the spec:

```markdown
## Deviations during implementation

| Spec said | Built | Why |
|---|---|---|
| `copyprefab` unchanged | `copyprefab` uses `noedit(true)` and empties the engine entity clipboard (`entcopyclear`); `tool_prefab_load` also clears the script-side `entcopybuf` | `noedit()` refused whenever the (possibly stale) selection was out of view, so Load failed silently. `editpaste` pasted stale copied entities along with the prefab, or replaced entities instead of pasting geometry when `entcopybuf` was set. |
| Delete confirm with `p_noundo_warn = 1` | Plain confirm; the text says a copy is kept in `backups/` | The warning reads "This operation cannot be undone!", which is false here. |
| Panel registers controls with `tool_register_control` | Not registered | Every panel function is a `tool_action`, and actions are already searchable (F3). |
| Popups focus the name field | No auto-focus | A focused immediate field writes its text back into the variable every frame, which overwrites prefilled values (Rename) and scripted input. Matches the entity-template popups. |
| Folder field: dropdown of top-level folders + "New folder…" | As specified; the new-folder text accepts nested paths (`trees/oak`) | Rename must be able to show a nested source folder. |
| Default bind F6 → `ta_prefabs` | `I` → `ta_prefabs` | F6 is bound to `ta_ents` (`config/setup.cfg:329`); F1–F9 are all taken in edit mode and F10/F11 take screenshots. |
| Action `ta_prefab_paste` (load + paste) | Dropped | Loading puts the prefab on the single clipboard, so the existing paste already places it; a second paste key only makes sense with a separate prefab buffer, which was deferred (user decision). |
| Load leaves the clipboard implicit | The Load notice says "Replaced your copied selection…" | Loading a prefab replaces the whole copy buffer (geometry and copied entities); the user should not lose a copy silently. |
```

- [ ] **Step 4: Full regression**

Run every suite on the debug build, cheapest first:

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\tests\edstate.tests.ps1
```

```powershell
tools\harness\harness.ps1 start
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task5-smoke.ps1
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\tests\task8-editor.ps1
```

```powershell
tools\harness\harness.ps1 stop
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: all green. The startup log also contains `testprefab: ok`. Report each suite's result with its summary line. If a pre-existing suite fails, check first whether it also fails at the base commit (`git stash`, rerun, `git stash pop`) before blaming this work.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/README.md
git commit -m "harness: document the prefab self-test"
```

(`doc/` is untracked on this branch; leave the handoff, spec and plan uncommitted unless the user asks otherwise.)
