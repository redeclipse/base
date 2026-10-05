# Geometry Templates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a mapper mark a cuboid of octree geometry as a geometry template and place
mapmodel-like instances of it (any rotation, uniform scale), drawn with hardware
instancing, kept live while the source is edited.

**Architecture:** `geotemplate` entities own a box. Every leaf cube fully inside it is
copied into a temporary octree, and the normal vertex-array builder (`updateva`) runs
over that octree into the template's own vertex arrays. `geoinstance` entities register
in `octaentities` like mapmodels, and are drawn per template with
`glDrawElementsInstanced`. The world vertex shaders read a per-instance 4x3 transform
whose constant (non-array) value is identity, so the world draws exactly as before. A
BIH built from the template's triangles gives collision, raycasts and stains.

**Tech Stack:** C++ (Tesseract/Red Eclipse engine), OpenGL 3.3 core, GLSL 3.30,
CubeScript (editor UI), PowerShell harness (`tools/harness`).

**Spec:** `doc/superpowers/specs/2026-10-02-geometry-templates-design.md`. Read the
"Revisions during planning" section at its end: it overrides earlier sections.

## Global Constraints

- OpenGL 3.3 core / GLSL 3.30 floor (master `2ae84f8d`). No non-instanced fallback.
- Entity types `geotemplate` (`ET_GEOTEMPLATE`) and `geoinstance` (`ET_GEOINSTANCE`),
  added before `ET_GAMESPECIFIC`. `MAPVERSION` 56 → 57.
- `geotemplate` attrs: `id`, `width`, `length`, `height` (half-extents of a box centred
  on the entity; the entity position is the pivot).
- `geoinstance` attrs: `template`, `yaw`, `pitch`, `roll`, `scale` (percent, 0 = 100),
  `flags` (bit 0 no-shadow, bit 1 no-collide), `modes`, `muts`, `variant`.
- Instance transform: `T(e.o) · Rz(yaw) · Rx(pitch) · Ry(-roll) · S(scale) · T(-pivot)`,
  with angles through `sincosmod360`, exactly as `BIH::ellipsecollide` orients a
  mapmodel.
- A non-empty leaf cube belongs to a template iff it lies entirely inside the requested
  box. Cubes are never split.
- World geometry must render bit-identically: the instance attributes' constant values
  are identity rows and an inverse scale of 1.
- Test-only commands go under `#ifdef DEBUG_UTILS` and refuse when
  `identflags&IDF_MAP`.
- Build: `wsl -d Ubuntu -- /mnt/f/Red\ Eclipse/src/build.sh debug` (from PowerShell:
  `wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug`). Debug builds run the unit
  tests in `src/tests/` at startup. The runnable binary is `bin/amd64/redeclipse.exe`.
- CubeScript traps (CLAUDE.md): no bare `#` in generated script, avoid `@` in harness
  strings, `exec "path" 0 0`.
- **If the harness itself misbehaves** (not the feature under test): `git stash`, stop
  the harness, and report. The user investigates harness problems in a separate session.
- Commits: on branch `geometry-templates` (created in Task 1), lowercase
  `area: summary` messages, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never push. `CLAUDE.md` is
  gitignored: edit it, but it never appears in a commit.

## File Map

| File | Responsibility |
|---|---|
| `src/engine/geomtemplate.cpp` (new) | Template data: box, capture, instance transform/bounds, registry, build, live-update hooks, BIH, editor boxes, test commands |
| `src/tests/geomtemplate.cpp` (new) | Unit tests of the pure functions |
| `src/engine/engine.h` | `geomtemplate` struct and declarations |
| `src/engine/octarender.cpp` | `buildtemplatevas` / `destroytemplateva`; `instances` in `vacollect`, `rendercube`, `updatevabb`; `allchanged` hook |
| `src/engine/octa.h` | `octaentities::instances`, `instquery`; `vtxarray::instances` |
| `src/engine/world.cpp` | Octree registration, bounding/selection boxes, template-entity hook, `removeoctaentity` |
| `src/engine/octaedit.cpp` | `changed()` / `commitchanges()` hooks, `edfillsel`, editor box call |
| `src/engine/renderva.cpp` | Instance stream buffer, instanced batches, G-buffer/shadow/RSM drawing, shadow-mesh baking, `geoinststats` |
| `src/engine/renderlights.cpp` | One call: `renderinstances()` after `rendergeom()` |
| `src/engine/physics.cpp`, `src/engine/bih.cpp`, `src/engine/bih.h`, `src/engine/stain.cpp` | Collision, raycasts, stains, `edraycast` |
| `src/shared/glemu.h`, `src/shared/glemu.cpp`, `src/shared/glexts.h`, `src/engine/rendergl.cpp` | Instance attributes, GL entry points, identity reset |
| `config/glsl/shared/instance.glsl` (new), `config/glsl/world.cfg`, `config/glsl/world/{world,bump,smworld,rsm}.vert`, `config/glsl/world/{world,bump}.frag` | Shader side |
| `src/shared/ents.h`, `src/engine/world.h`, `src/engine/worldio.cpp`, `src/shared/iengine.h` | Entity types, map version, engine API for the game |
| `src/game/game.h`, `src/game/entities.cpp` | Entity table rows, info strings, attribute fixups |
| `config/tool/ents/geo{template,instance}.cfg` (new), `config/ui/tool/ents/toolgeo{template,instance}.cfg` (new), `config/tool/toolent.cfg`, `config/tool/toolentparam.cfg`, `config/ui/tool/toolent.cfg` | Editor entity panels |
| `src/Makefile` | New object files |
| `tools/harness/geotemplate-selftest.ps1` (new), `tools/harness/tests/geotemplate.cfg` (new), `tools/harness/tests/geotemplate-atop-types.txt` (new) | End-to-end self-test |
| `tools/harness/README.md`, `CLAUDE.md` | Docs |

---

## Phase 1 — templates and G-buffer instances

### Task 1: Entity types, map version 57, editor panels

**Files:**
- Modify: `src/shared/ents.h:6`, `src/engine/world.h:8`, `src/engine/worldio.cpp:1245`
- Modify: `src/game/game.h:31-35` (enum) and the `enttype[]` table after the `WORLDCOL` row
- Modify: `src/game/entities.cpp` (`entinfo` after `case WORLDCOL`, `fixentity` after `case WORLDCOL`)
- Create: `config/tool/ents/geotemplate.cfg`, `config/tool/ents/geoinstance.cfg`
- Create: `config/ui/tool/ents/toolgeotemplate.cfg`, `config/ui/tool/ents/toolgeoinstance.cfg`
- Modify: `config/tool/toolentparam.cfg` (exec list), `config/ui/tool/toolent.cfg` (exec list), `config/tool/toolent.cfg` (`tool_ent_types_env`)
- Create: `tools/harness/geotemplate-selftest.ps1`, `tools/harness/tests/geotemplate.cfg`, `tools/harness/tests/geotemplate-atop-types.txt`

**Interfaces:**
- Produces: `ET_GEOTEMPLATE`, `ET_GEOINSTANCE` (engine), `GEOTEMPLATE`, `GEOINSTANCE` (game); entity type names `geotemplate`, `geoinstance`; CubeScript helpers `geot_count`, `geot_hist`, `geot_newent`, `geot_moveent`, `geot_setattr`, `geot_delent`, `geot_get`; PowerShell helpers in the self-test (`Step`, `Expect`, `ExpectTrue`, `Ed`, `EdState`, `Send`, `Eval`, `Invoke-MapLoad`, `Info`, `Shot`).

- [ ] **Step 1: Create the branch**

```bash
git checkout -b geometry-templates
```

- [ ] **Step 2: Write the CubeScript test helpers**

Create `tools/harness/tests/geotemplate.cfg`:

```cubescript
// Helpers for tools/harness/geotemplate-selftest.ps1 (edit mode only: they
// select entities). Loaded with: exec "tools/harness/tests/geotemplate.cfg" 0 0

// 1:<type> -- number of entities of that type
geot_count = [
    local _n
    _n = 0
    entcancel
    entselect 1
    entloopread [if (=s (at (entget) 0) $arg1) [_n = (+ $_n 1)]]
    entcancel
    result $_n
]

// "type:count" for every entity type present, in order of first appearance
geot_hist = [
    local _types _counts _t _k
    _types = []
    _counts = []
    entcancel
    entselect 1
    entloopread [
        _t = (at (entget) 0)
        _k = (indexof $_types $_t)
        if (< $_k 0) [
            _types = (concat $_types $_t)
            _counts = (concat $_counts 1)
        ] [
            _counts = (listsplice $_counts (+ (at $_counts $_k) 1) $_k 1)
        ]
    ]
    entcancel
    result (loopconcat i (listlen $_types) [concatword (at $_types $i) ":" (at $_counts $i)])
]

// 1:<type> 2:<attrs> 3:<x> 4:<y> 5:<z> -- new entity at an exact position, returns its index
geot_newent = [
    local _i
    entcancel
    _i = (newent $arg1 $arg2)
    entpos $arg3 $arg4 $arg5
    entcancel
    result $_i
]

// 1:<index> 2:<x> 3:<y> 4:<z>
geot_moveent = [
    entcancel
    enttoggleidx $arg1
    entpos $arg2 $arg3 $arg4
    entcancel
]

// 1:<index> 2:<attr> 3:<value>
geot_setattr = [
    entcancel
    enttoggleidx $arg1
    entattr $arg2 $arg3
    entcancel
]

// 1:<index>
geot_delent = [
    entcancel
    enttoggleidx $arg1
    delent
    entcancel
]

// 1:<index> -- "type attr0 attr1 ..."
geot_get = [
    local _s
    entcancel
    enttoggleidx $arg1
    entloopread [_s = (entget)]
    entcancel
    result $_s
]
```

- [ ] **Step 3: Record the entity-type baseline of a shipped (version 56) map, before any code change**

The current build loads `atop` and lists its entity types. After this task the same
map must give the same list (the type shift must map every old type to its new number).

```powershell
tools\harness\harness.ps1 start
tools\harness\harness.ps1 send 'entediting 0'
tools\harness\editor.ps1 open atop
tools\harness\harness.ps1 send 'entediting 1'
tools\harness\harness.ps1 send 'exec "tools/harness/tests/geotemplate.cfg" 0 0'
$out = @(tools\harness\harness.ps1 send 'echo (concatword "GEOT_HIST=" (geot_hist))')
$line = $out | Where-Object { $_ -match 'GEOT_HIST=(.*)$' } | Select-Object -First 1
($line -replace '.*GEOT_HIST=', '').Trim() | Set-Content -Encoding ascii tools\harness\tests\geotemplate-atop-types.txt
Get-Content tools\harness\tests\geotemplate-atop-types.txt
tools\harness\harness.ps1 stop
```

Expected: one line like `light:120 mapmodel:85 playerstart:12 ... weapon:30`. If it is
empty, stop: the helpers did not load. That's a harness problem (see Global
Constraints).

- [ ] **Step 4: Write the self-test with the first failing steps**

Create `tools/harness/geotemplate-selftest.ps1`:

```powershell
<#
.SYNOPSIS
    End-to-end self-test for geometry templates (geotemplate / geoinstance).

.DESCRIPTION
    Boots a client in the harness home and drives template capture, live
    updates, instances, rendering, shadows and collision through DEBUG_UTILS
    commands (geotemplateinfo, geoinstancebb, edfillsel, geoinststats,
    edraycast). Writes screenshots geot-*.png for review; the comment at each
    Shot says what it must show.
    Spec: doc/superpowers/specs/2026-10-02-geometry-templates-design.md

.EXAMPLE
    tools\harness\geotemplate-selftest.ps1
    tools\harness\geotemplate-selftest.ps1 -KeepRunning
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

# 'newmap 12' is 4096 units, solid below z = 2048, with an 8-unit grid.
$Floor = 2048
$Mid   = 2048
$Grid  = 8

# The test block: 4x4x4 cubes of 8 at (2048, 2048, 2112), i.e.
# x 2048..2080, y 2048..2080, z 2112..2144.
$BX = 2048
$BY = 2048
$BZ = 2112

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

# Loading a map while an entity is hovered trips an unguarded enthover read
# (world.cpp:1426, see CLAUDE.md). Entity editing off empties it.
function Invoke-MapLoad([scriptblock]$Body) {
    Send 'entediting 0' 300
    try { & $Body }
    finally { Send 'entediting 1' 300 }
}

# geotemplateinfo <id>, parsed; $null when there is no such template.
function Info([int]$Id) {
    $v = Eval "(geotemplateinfo $Id)"
    if (-not $v) { return $null }
    $f = @($v -split '\s+' | ForEach-Object { [int]$_ })
    return [pscustomobject]@{
        Box = ($f[0..5] -join ' '); Verts = $f[6]; Tris = $f[7]; Instances = $f[8]; Rebuilds = $f[9]
    }
}

function Shot([string]$Name) { & $harness shot $Name 6>$null | Out-Null }

# --------------------------------------------------------------------------

& $harness stop 6>$null | Out-Null
& $harness start 6>$null | Out-Null

try {
    Send 'exec "tools/harness/tests/geotemplate.cfg" 0 0'

    # ==== Task 1: entity types and the map format ==========================

    Step 'a version 56 map loads with every entity type intact' {
        Invoke-MapLoad { Ed open atop }
        $expected = (Get-Content (Join-Path $PSScriptRoot 'tests\geotemplate-atop-types.txt') -Raw).Trim()
        Expect 'atop entity types' (Eval '(geot_hist)') $expected
    }

    Step 'geotemplate and geoinstance entities exist and survive a save' {
        Invoke-MapLoad { Ed newmap 12 }
        Expect 'edit mode' (EdState).Mode.EditMode 1
        $t = Eval '(geot_newent geotemplate "1 16 16 16" 2064 2064 2128)'
        $i = Eval '(geot_newent geoinstance "1 90 0 0 50 0 0 0 0" 2300 2048 2200)'
        ExpectTrue 'newent geotemplate returned an index' ($t -match '^\d+$') "got '$t'"
        ExpectTrue 'newent geoinstance returned an index' ($i -match '^\d+$') "got '$i'"
        Expect 'one geotemplate' (Eval '(geot_count geotemplate)') '1'
        Expect 'one geoinstance' (Eval '(geot_count geoinstance)') '1'
        Send 'savemap harness_geot' 1500
        Invoke-MapLoad { Ed open harness_geot }
        Expect 'geotemplate after reload' (Eval "(geot_get $t)") 'geotemplate 1 16 16 16'
        Expect 'geoinstance after reload' (Eval "(geot_get $i)") 'geoinstance 1 90 0 0 50 0 0 0 0'
    }

    # ==== later tasks add their steps here, in order ======================
}
finally {
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'geotemplate self-test: all checks passed' -ForegroundColor Green
exit 0
```

- [ ] **Step 5: Run the self-test and confirm it fails**

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

Expected: step 1 passes (no code changed yet), and step 2 FAILs: `newent geotemplate` returns nothing because the type does not exist.

- [ ] **Step 6: Add the engine types and the map conversion**

`src/shared/ents.h:6`, add the two types before `ET_GAMESPECIFIC`:

```cpp
enum { ET_EMPTY = 0, ET_LIGHT, ET_MAPMODEL, ET_PLAYERSTART, ET_ENVMAP, ET_PARTICLES, ET_SOUND, ET_LIGHTFX, ET_DECAL, ET_WIND, ET_MAPUI, ET_SOUNDENV, ET_PHYSICS, ET_WORLDCOL, ET_GEOTEMPLATE, ET_GEOINSTANCE, ET_GAMESPECIFIC };
```

`src/engine/world.h:8`:

```cpp
#define MAPVERSION 57 // bump if map format changes, see worldio.cpp
```

`src/engine/worldio.cpp`, after the `hdr.version < 56` line:

```cpp
            if(hdr.version < 57 && e.type >= ET_GEOTEMPLATE) e.type += 2;
```

- [ ] **Step 7: Add the game-side types**

`src/game/game.h`, entity enum: append `GEOTEMPLATE = ET_GEOTEMPLATE, GEOINSTANCE = ET_GEOINSTANCE,` after `WORLDCOL = ET_WORLDCOL,`.

In the `enttype[]` table, after the `WORLDCOL` row:

```cpp
    {
        GEOTEMPLATE,    1,          0,      0,      EU_NONE,    4,              -1,         -1,         -1,     -1,     -1,         -1,         -1,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "geotemplate",  "Geometry Template", { "id", "width", "length", "height" }
    },
    {
        GEOINSTANCE,    1,          0,      0,      EU_NONE,    9,              -1,         6,          -1,     8,      -1,         1,          2,
            0, 0, 0,
            false,  false,  false,      false,      false,
                "geoinstance",  "Geometry Instance", { "template", "yaw", "pitch", "roll", "scale", "flags", "modes", "muts", "variant" }
    },
```

(The columns are: type, priority, links, radius, usetype, numattrs, palattr, modesattr,
idattr, mvattr, fxattr, yawattr, pitchattr. `modesattr` 6 and `mvattr` 8 make
`entities::isallowed` filter instances by mode, mutator and variant.)

`src/game/entities.cpp`, `entinfo()`, after the `case WORLDCOL` block:

```cpp
            case GEOTEMPLATE:
            {
                defformatstring(str, "id %d", attr[0]);
                addentinfo(str);
                break;
            }
            case GEOINSTANCE:
            {
                defformatstring(str, "template %d", attr[0]);
                addentinfo(str);
                break;
            }
```

`fixentity()`, after the `case WORLDCOL` block:

```cpp
            case GEOTEMPLATE:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // id, clamp
                loopk(3) if(e.attrs[k+1] < 0) e.attrs[k+1] = 0; // width, length, height, clamp
                break;
            }
            case GEOINSTANCE:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // template, clamp
                FIXDIRYPR(1, 2, 3); // yaw, pitch, roll
                if(e.attrs[4] < 0) e.attrs[4] = 0; // scale, clamp
                e.attrs[5] &= 3; // flags: no-shadow, no-collide
                break;
            }
```

- [ ] **Step 8: Add the editor panels**

Create `config/tool/ents/geotemplate.cfg`:

```cubescript
tool_ent_add_attrs_geotemplate = [
    tool_ent_add_attr geotemplate id $T_ENT_NODELTA
    tool_ent_add_attr geotemplate width 0
    tool_ent_add_attr geotemplate length 0
    tool_ent_add_attr geotemplate height 0
]
```

Create `config/tool/ents/geoinstance.cfg`:

```cubescript
tool_ent_geoinstance_flags = [
    noshadow
    nocollide
]

tool_ent_add_attrs_geoinstance = [
    tool_ent_add_attr geoinstance template $T_ENT_NODELTA
    tool_ent_add_attr geoinstance yaw 0
    tool_ent_add_attr geoinstance pitch 0
    tool_ent_add_attr geoinstance roll 0
    tool_ent_add_attr geoinstance scale 0
    tool_ent_add_attr geoinstance flags $T_ENT_NODELTA
    tool_ent_add_attr geoinstance modes $T_ENT_NODELTA
    tool_ent_add_attr geoinstance muts $T_ENT_NODELTA
    tool_ent_add_attr geoinstance variant $T_ENT_NODELTA
]
```

Create `config/ui/tool/ents/toolgeotemplate.cfg` (the `#` here is the intended macro
preprocessor, as in every sibling file):

```cubescript
# ui_tool_ent_geotemplate = [
    ui_tool_ent_param_group "Template" [
        ui_tool_numinput #(tool_ent_attr geotemplate id) 0 999999 1 [
            #(ui_tool_ent_attr_props geotemplate id [] 1)
            p_val_format = i
            p_label = "ID"
        ]
    ]

    ui_tool_ent_param_group "Half size" [
        ui_tool_numinput #(tool_ent_attr geotemplate width) 0 999999 1 [
            #(ui_tool_ent_attr_props geotemplate width [] 1)
            p_val_format = i
            p_label = "X"
        ]

        ui_tool_numinput #(tool_ent_attr geotemplate length) 0 999999 1 [
            #(ui_tool_ent_attr_props geotemplate length [] 1)
            p_val_format = i
            p_label = "Y"
        ]

        ui_tool_numinput #(tool_ent_attr geotemplate height) 0 999999 1 [
            #(ui_tool_ent_attr_props geotemplate height [] 1)
            p_val_format = i
            p_label = "Z"
        ]
    ]
]
```

Create `config/ui/tool/ents/toolgeoinstance.cfg`:

```cubescript
# ui_tool_ent_geoinstance = [
    ui_tool_ent_param_group "Template" [
        ui_tool_numinput #(tool_ent_attr geoinstance template) 0 999999 1 [
            #(ui_tool_ent_attr_props geoinstance template [] 1)
            p_val_format = i
            p_label = "ID"
        ]
    ]

    ui_tool_ent_param_group "Transform" [
        ui_tool_numinput #(tool_ent_attr geoinstance yaw) 0 360 1 [
            #(ui_tool_ent_attr_props geoinstance yaw [] 1)
            p_label = "Yaw"
            p_val_format = i
            p_circular = 1
        ]

        ui_tool_numinput #(tool_ent_attr geoinstance pitch) -180 180 1 [
            #(ui_tool_ent_attr_props geoinstance pitch [] 1)
            p_label = "Pitch"
            p_val_format = i
            p_circular = 1
        ]

        ui_tool_numinput #(tool_ent_attr geoinstance roll) -180 180 1 [
            #(ui_tool_ent_attr_props geoinstance roll [] 1)
            p_label = "Roll"
            p_val_format = i
            p_circular = 1
        ]

        uifill 0 $ui_tool_elem_space_l

        ui_tool_numinput #(tool_ent_attr geoinstance scale) 0 10000 10 [
            #(ui_tool_ent_attr_props geoinstance scale [] 1)
            p_label = "Scale ^%"
            p_val_format = i
        ]
    ]

    @(ui_tool_ent_flags_group geoinstance $tool_ent_geoinstance_flags [
        "No shadow"
        "No collision"
    ])

    @(ui_tool_ent_gamemode_group geoinstance)
    @(ui_tool_ent_variant_group geoinstance)
]
```

`config/tool/toolentparam.cfg`, after `exec "config/tool/ents/worldcol.cfg"`:

```cubescript
exec "config/tool/ents/geotemplate.cfg"
exec "config/tool/ents/geoinstance.cfg"
```

`config/ui/tool/toolent.cfg`, after `exec "config/ui/tool/ents/toolmapui.cfg"` (and after `toolworldcol.cfg` if that comes later):

```cubescript
exec "config/ui/tool/ents/toolgeotemplate.cfg"
exec "config/ui/tool/ents/toolgeoinstance.cfg"
```

`config/tool/toolent.cfg`, `tool_ent_types_env`: add `"geotemplate"` and `"geoinstance"` on their own lines after `"mapmodel"`.

- [ ] **Step 9: Build and run the self-test**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

Expected: both steps pass. Then open the editor's entity panel on a new geotemplate and
a new geoinstance (`harness.ps1 nav` / `click`, or by hand). Each panel must show its
groups, and editing a value must change the attribute.

- [ ] **Step 10: Commit**

```bash
git add src/shared/ents.h src/engine/world.h src/engine/worldio.cpp src/game/game.h src/game/entities.cpp config/tool/ents/geotemplate.cfg config/tool/ents/geoinstance.cfg config/ui/tool/ents/toolgeotemplate.cfg config/ui/tool/ents/toolgeoinstance.cfg config/tool/toolentparam.cfg config/ui/tool/toolent.cfg config/tool/toolent.cfg tools/harness/geotemplate-selftest.ps1 tools/harness/tests/geotemplate.cfg tools/harness/tests/geotemplate-atop-types.txt
git commit -m "world: add geotemplate and geoinstance entities (map version 57)"
```

---

### Task 2: Pure template geometry (box, capture, instance transform) with unit tests

**Files:**
- Create: `src/engine/geomtemplate.cpp`
- Create: `src/tests/geomtemplate.cpp`
- Modify: `src/engine/engine.h` (declarations, after the octarender/renderva declarations)
- Modify: `src/engine/main.cpp:1338` (run the unit test)
- Modify: `src/Makefile` (object lists and a dependency line)

**Interfaces:**
- Produces (engine.h):
  ```cpp
  enum { GEOINST_NOSHADOW = 1<<0, GEOINST_NOCOLLIDE = 1<<1 };
  extern void geomtemplatebox(const extentity &e, vec &bmin, vec &bmax);
  extern int capturegeomtemplate(cube *src, int rootsize, const vec &bmin, const vec &bmax, cube *dst, ivec &capmin, ivec &capmax);
  extern float geominstancescale(const extentity &e);
  extern void calcgeominstance(const extentity &e, const vec &pivot, matrix4x3 &m);
  extern void calcgeominstancebb(const matrix4x3 &m, const ivec &capmin, const ivec &capmax, ivec &bbmin, ivec &bbmax);
  ```
  `capturegeomtemplate` returns the number of leaf cubes copied. With none, `capmin` is
  `(1,1,1)` and `capmax` is `(0,0,0)`, so `capmin.x > capmax.x` means empty.

- [ ] **Step 1: Write the failing unit test**

Create `src/tests/geomtemplate.cpp`:

```cpp
#include "engine.h"

// Geometry template geometry, see engine/geomtemplate.cpp: the box an entity
// requests, which cubes a template captures, and the instance transform.

static bool nearvec(const vec &a, const vec &b) { return a.dist(b) < 1e-3f; }

static void setents(extentity &e, const vec &o, int n, const int *vals)
{
    e.o = o;
    e.attrs.setsize(0);
    loopi(n) e.attrs.add(vals[i]);
}

static void testbox()
{
    extentity e;
    const int a[] = { 1, 16, 8, 4 };
    setents(e, vec(100, 200, 300), 4, a);
    vec bmin, bmax;
    geomtemplatebox(e, bmin, bmax);
    ASSERT(nearvec(bmin, vec(84, 192, 296)));
    ASSERT(nearvec(bmax, vec(116, 208, 304)));

    const int b[] = { 1, -5, 8, 4 }; // negative half-extents count as 0
    setents(e, vec(100, 200, 300), 4, b);
    geomtemplatebox(e, bmin, bmax);
    ASSERT(bmin.x == 100 && bmax.x == 100);
}

static void testcapture()
{
    // A 64-unit octree: root children are 32, their children 16
    cube *src = newcubes(F_EMPTY);
    src[0].children = newcubes(F_EMPTY);
    cube &a = src[0].children[0]; // (0,0,0) size 16
    cube &b = src[0].children[1]; // (16,0,0) size 16
    solidfaces(a);
    solidfaces(b);
    a.material = MAT_ALPHA;

    // Surface data: a blended face loses its bottom layer, a merged one resets
    newcubeext(a, 4, false);
    memset(a.ext->surfaces, 0, sizeof(a.ext->surfaces));
    a.ext->surfaces[2].numverts = LAYER_BOTTOM;
    a.ext->surfaces[3].numverts = LAYER_TOP|4;
    a.merged = 1<<3;

    // b straddles the box's +x face (16..32 against a box ending at 24)
    cube *dst = newcubes(F_EMPTY);
    ivec capmin, capmax;
    int n = capturegeomtemplate(src, 64, vec(-1, -1, -1), vec(24, 17, 17), dst, capmin, capmax);
    ASSERT(n == 1);
    ASSERT(capmin == ivec(0, 0, 0) && capmax == ivec(16, 16, 16));
    ASSERT(dst[0].children && isentirelysolid(dst[0].children[0]) && isempty(dst[0].children[1]));
    const cube &c = dst[0].children[0];
    ASSERT(c.material == MAT_ALPHA);
    ASSERT(!c.merged);
    ASSERT(c.ext && c.ext->surfaces[2].numverts == LAYER_TOP);
    ASSERT(c.ext->surfaces[3].numverts == LAYER_TOP && c.ext->surfaces[3].verts == 0);
    ASSERT(!c.ext->va && !c.ext->ents && c.ext->tjoints < 0);
    freeocta(dst);

    // An off-grid box takes both
    dst = newcubes(F_EMPTY);
    n = capturegeomtemplate(src, 64, vec(-0.5f, -0.5f, -0.5f), vec(32.5f, 16.5f, 16.5f), dst, capmin, capmax);
    ASSERT(n == 2);
    ASSERT(capmin == ivec(0, 0, 0) && capmax == ivec(32, 16, 16));
    freeocta(dst);

    // Nothing inside
    dst = newcubes(F_EMPTY);
    n = capturegeomtemplate(src, 64, vec(40, 40, 40), vec(50, 50, 50), dst, capmin, capmax);
    ASSERT(n == 0 && capmin.x > capmax.x);
    ASSERT(!dst[0].children && isempty(dst[0]));
    freeocta(dst);

    freeocta(src);
}

static void testinstance()
{
    extentity e;
    matrix4x3 m;

    // yaw 90, scale 200, pivot (10,0,0): +x one unit from the pivot lands 2 along +y
    const int a[] = { 1, 90, 0, 0, 200, 0, 0, 0, 0 };
    setents(e, vec(100, 0, 0), 9, a);
    ASSERT(geominstancescale(e) == 2);
    calcgeominstance(e, vec(10, 0, 0), m);
    ASSERT(nearvec(m.transform(vec(10, 0, 0)), vec(100, 0, 0)));
    ASSERT(nearvec(m.transform(vec(11, 0, 0)), vec(100, 2, 0)));

    // scale 0 means 100
    const int b[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    setents(e, vec(0, 0, 0), 9, b);
    ASSERT(geominstancescale(e) == 1);

    // pitch and roll keep lengths (uniform scale only)
    const int c[] = { 1, 30, 40, 50, 150, 0, 0, 0, 0 };
    setents(e, vec(5, 6, 7), 9, c);
    calcgeominstance(e, vec(1, 2, 3), m);
    ASSERT(fabs(m.transform(vec(2, 2, 3)).dist(vec(5, 6, 7)) - 1.5f) < 1e-3f);
    ASSERT(fabs(m.transform(vec(1, 2, 4)).dist(vec(5, 6, 7)) - 1.5f) < 1e-3f);

    // bounds of the captured box under yaw 90 about its centre
    const int d[] = { 1, 90, 0, 0, 0, 0, 0, 0, 0 };
    setents(e, vec(100, 100, 100), 9, d);
    calcgeominstance(e, vec(8, 4, 2), m);
    ivec bbmin, bbmax;
    calcgeominstancebb(m, ivec(0, 0, 0), ivec(16, 8, 4), bbmin, bbmax);
    ASSERT(bbmin == ivec(96, 92, 98) && bbmax == ivec(104, 108, 102));
}

void testgeomtemplate()
{
    testbox();
    testcapture();
    testinstance();
    conoutf(colourwhite, "testgeomtemplate: ok");
}
```

`src/engine/main.cpp`, in the `#ifdef _DEBUG` unit-test block after `testshadersource();`:

```cpp
        extern void testgeomtemplate();
        testgeomtemplate();
```

`src/Makefile`: add `tests/geomtemplate.o` to the debug test list:

```make
    CLIENT_OBJS += tests/geomtemplate.o
```

- [ ] **Step 2: Build and confirm it fails**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
```

Expected: the link fails with undefined `geomtemplatebox`, `capturegeomtemplate`, `geominstancescale`, `calcgeominstance`, `calcgeominstancebb`.

- [ ] **Step 3: Implement**

Create `src/engine/geomtemplate.cpp`:

```cpp
// geomtemplate.cpp: geometry templates -- the octree cubes inside a geotemplate
// entity's box, built into their own vertex arrays and drawn as hardware
// instances by geoinstance entities.
// Design: doc/superpowers/specs/2026-10-02-geometry-templates-design.md

#include "engine.h"

// The box a geotemplate requests: centred on the entity, half-extents in
// attributes 1-3 (as soundenv and physics zones). The entity is the pivot.
void geomtemplatebox(const extentity &e, vec &bmin, vec &bmax)
{
    vec half(max(e.attrs[1], 0), max(e.attrs[2], 0), max(e.attrs[3], 0));
    bmin = vec(e.o).sub(half);
    bmax = vec(e.o).add(half);
}

static inline bool cubeinsidebox(const ivec &o, int size, const vec &bmin, const vec &bmax)
{
    return o.x >= bmin.x && o.y >= bmin.y && o.z >= bmin.z &&
           o.x + size <= bmax.x && o.y + size <= bmax.y && o.z + size <= bmax.z;
}

static inline bool cubeoverlapsbox(const ivec &o, int size, const vec &bmin, const vec &bmax)
{
    return o.x < bmax.x && o.y < bmax.y && o.z < bmax.z &&
           o.x + size > bmin.x && o.y + size > bmin.y && o.z + size > bmin.z;
}

// The cube of `size` at `o` in the octree `root` (whose children are
// rootsize/2), subdividing empty space on the way down
static cube &makecube(cube *root, int rootsize, const ivec &o, int size)
{
    int scale = 0;
    while(1<<scale < rootsize) scale++;
    cube *c = root;
    for(scale--;; scale--)
    {
        cube &cur = c[octastep(o.x, o.y, o.z, scale)];
        if(1<<scale == size) return cur;
        if(!cur.children) cur.children = newcubes(F_EMPTY);
        c = cur.children;
    }
}

// A leaf of the source as a template cube: shape, textures and surfaces (which
// carry smoothed normals), never its vertex array, entities or t-joints.
// Merged faces are reset, as clearmerge() does: a merge can reach past the
// box, and the template's own octree is re-merged. Blended faces keep their
// top layer only, the blendmap being world-locked. Alpha stays alpha, so it
// stays out of the opaque range; other materials are dropped.
static void copytemplatecube(const cube &src, cube &dst)
{
    dst.children = NULL;
    dst.ext = NULL;
    memcpy(dst.faces, src.faces, sizeof(dst.faces));
    memcpy(dst.texture, src.texture, sizeof(dst.texture));
    dst.material = src.material&MAT_ALPHA;
    dst.merged = 0;
    dst.visible = 0;
    if(!src.ext) return;
    cubeext *ext = newcubeext(dst, src.ext->maxverts, false);
    memcpy(ext->surfaces, src.ext->surfaces, sizeof(ext->surfaces));
    memcpy(ext->verts(), src.ext->verts(), src.ext->maxverts*sizeof(vertinfo));
    loopi(6)
    {
        surfaceinfo &surf = ext->surfaces[i];
        if(src.merged&(1<<i)) surf = brightsurface;
        else if(surf.numverts&LAYER_BOTTOM) surf.numverts = (surf.numverts&~LAYER_BLEND)|LAYER_TOP;
    }
}

static int capturecubes(cube *c, const ivec &co, int size, const vec &bmin, const vec &bmax, cube *dst, int rootsize, ivec &capmin, ivec &capmax)
{
    int count = 0;
    loopi(8)
    {
        ivec o(i, co, size);
        if(!cubeoverlapsbox(o, size, bmin, bmax)) continue;
        if(c[i].children) count += capturecubes(c[i].children, o, size>>1, bmin, bmax, dst, rootsize, capmin, capmax);
        else if(!isempty(c[i]) && cubeinsidebox(o, size, bmin, bmax))
        {
            copytemplatecube(c[i], makecube(dst, rootsize, o, size));
            capmin.min(o);
            capmax.max(ivec(o).add(size));
            count++;
        }
    }
    return count;
}

// Copies every non-empty leaf of `src` lying fully inside the box into `dst`
// (an empty octree of the same size) at the same coordinates. Returns the
// number of leaves; capmin/capmax are their union, or (1,1,1)/(0,0,0).
int capturegeomtemplate(cube *src, int rootsize, const vec &bmin, const vec &bmax, cube *dst, ivec &capmin, ivec &capmax)
{
    capmin = ivec(INT_MAX, INT_MAX, INT_MAX);
    capmax = ivec(INT_MIN, INT_MIN, INT_MIN);
    int count = capturecubes(src, ivec(0, 0, 0), rootsize>>1, bmin, bmax, dst, rootsize, capmin, capmax);
    if(!count)
    {
        capmin = ivec(1, 1, 1);
        capmax = ivec(0, 0, 0);
    }
    return count;
}

float geominstancescale(const extentity &e) { return e.attrs[4] > 0 ? e.attrs[4]/100.0f : 1.0f; }

// T(e.o) Rz(yaw) Rx(pitch) Ry(-roll) S(scale) T(-pivot): the orientation
// BIH::ellipsecollide gives a mapmodel, about the template's pivot
void calcgeominstance(const extentity &e, const vec &pivot, matrix4x3 &m)
{
    m.identity();
    if(e.attrs[1]) m.rotate_around_z(sincosmod360(e.attrs[1]));
    if(e.attrs[2]) m.rotate_around_x(sincosmod360(e.attrs[2]));
    if(e.attrs[3]) m.rotate_around_y(sincosmod360(-e.attrs[3]));
    float scale = geominstancescale(e);
    if(scale != 1) m.scale(scale);
    m.settranslation(e.o);
    m.translate(vec(pivot).neg());
}

// The world box around the 8 transformed corners of a captured box
void calcgeominstancebb(const matrix4x3 &m, const ivec &capmin, const ivec &capmax, ivec &bbmin, ivec &bbmax)
{
    vec lo(1e16f, 1e16f, 1e16f), hi(-1e16f, -1e16f, -1e16f);
    loopi(8)
    {
        vec corner(i&1 ? capmax.x : capmin.x, i&2 ? capmax.y : capmin.y, i&4 ? capmax.z : capmin.z);
        vec p = m.transform(corner);
        lo.min(p);
        hi.max(p);
    }
    bbmin = ivec::floor(lo);
    bbmax = ivec::ceil(hi);
}
```

`src/engine/engine.h`: add after the renderva declarations:

```cpp
// geomtemplate
enum { GEOINST_NOSHADOW = 1<<0, GEOINST_NOCOLLIDE = 1<<1 };
extern void geomtemplatebox(const extentity &e, vec &bmin, vec &bmax);
extern int capturegeomtemplate(cube *src, int rootsize, const vec &bmin, const vec &bmax, cube *dst, ivec &capmin, ivec &capmax);
extern float geominstancescale(const extentity &e);
extern void calcgeominstance(const extentity &e, const vec &pivot, matrix4x3 &m);
extern void calcgeominstancebb(const matrix4x3 &m, const ivec &capmin, const ivec &capmax, ivec &bbmin, ivec &bbmax);
```

`src/Makefile`: add `engine/geomtemplate.o \` to `CLIENT_OBJS` between `engine/fxprop.o \` and `engine/grass.o \`, and at the end of the dependency lines:

```make
engine/geomtemplate.o: engine/engine.h
tests/geomtemplate.o: engine/engine.h
```

- [ ] **Step 4: Build and run the unit test**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
tools\harness\harness.ps1 start
Get-Content home\uitest\log.txt | Select-String 'testgeomtemplate'
tools\harness\harness.ps1 stop
```

Expected: `testgeomtemplate: ok`. A failed ASSERT ends the client during
`harness.ps1 start` with a backtrace in `log.txt` (`RE_CRASHLOG`). Fix the cause; don't
change the test unless it contradicts the spec.

- [ ] **Step 5: Commit**

```bash
git add src/engine/geomtemplate.cpp src/tests/geomtemplate.cpp src/engine/engine.h src/engine/main.cpp src/Makefile
git commit -m "geomtemplate: add template capture and instance transform"
```

---

### Task 3: Template registry and build, entity hooks, `geotemplateinfo`, `edfillsel`

**Files:**
- Modify: `src/engine/geomtemplate.cpp` (registry, build, sync, commands)
- Modify: `src/engine/octarender.cpp` (`buildtemplatevas`, `destroytemplateva` after `destroyva`; `allchanged` PROGRESS(5))
- Modify: `src/engine/engine.h` (struct + declarations)
- Modify: `src/engine/world.cpp` (`removeoctaentity`; `modifyoctaent` template hook)
- Modify: `src/engine/octaedit.cpp` (`commitchanges`; `edfillsel` in the DEBUG_UTILS block next to `edselbox`)
- Modify: `src/engine/renderva.cpp` (`cleanupva`)
- Modify: `src/shared/iengine.h`, `src/game/entities.cpp` (`geotemplatestate`, "empty"/"missing" info)
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Consumes: Task 2's functions.
- Produces (engine.h):
  ```cpp
  struct geomtemplate
  {
      int id, ent;               // template id; the entity defining it
      vec pivot, reqmin, reqmax; // the entity's position and requested box
      ivec capmin, capmax;       // captured box; capmin.x > capmax.x when empty
      vector<vtxarray *> vas;    // own vertex arrays and VBOs, source coordinates
      BIH *bih;                  // collision (Task 11); NULL until then and when empty
      int verts, tris, rebuilds;
      bool dirty;

      geomtemplate() : id(-1), ent(-1), pivot(0, 0, 0), reqmin(0, 0, 0), reqmax(0, 0, 0), capmin(1, 1, 1), capmax(0, 0, 0), bih(NULL), verts(0), tris(0), rebuilds(0), dirty(true) {}
      bool empty() const { return capmin.x > capmax.x; }
  };
  extern vector<geomtemplate *> geomtemplates;
  extern geomtemplate *findgeomtemplate(int id);
  extern geomtemplate *geominstancetemplate(const extentity &e); // NULL when missing or empty
  extern bool geominstancebb(const extentity &e, ivec &bbmin, ivec &bbmax); // false without a template
  extern bool geomtemplatesdirty();
  extern void geomtemplateentschanged();
  extern void updategeomtemplates(bool rebuildall);
  extern void cleargeomtemplates();
  extern void buildtemplatevas(cube *root, vector<vtxarray *> &vas);   // octarender.cpp
  extern void destroytemplateva(vtxarray *va);                         // octarender.cpp
  extern void removeoctaentity(int id);                                 // world.cpp
  ```
- Produces (iengine.h): `extern int geotemplatestate(int id); // -1 no template, else its triangle count (0 = empty)`.
- Produces (commands, DEBUG_UTILS): `geotemplateinfo <id>` → `capminx capminy capminz capmaxx capmaxy capmaxz verts tris instances rebuilds` (box zeros when empty), or empty; `edfillsel <solid>`.

- [ ] **Step 1: Write the failing self-test steps**

In `tools/harness/geotemplate-selftest.ps1`, replace the line `# ==== later tasks add their steps here, in order ======================` with:

```powershell
    # ==== Task 3: capture, entity edits, save ==============================

    Step 'capture: whole cubes inside the requested box, straddlers excluded' {
        Invoke-MapLoad { Ed newmap 12 }
        Ed sel $BX $BY $BZ -Size 4,4,4
        Send 'edfillsel 1'
        # One cube straddling the box's +x face: 2080..2088 against a box ending at 2084
        Ed sel ($BX + 32) $BY $BZ -Size 1,1,1
        Send 'edfillsel 1'
        $script:T1 = [int](Eval '(geot_newent geotemplate "1 20 20 20" 2064 2064 2128)')
        $i = Info 1
        Expect 'captured box' $i.Box '2048 2048 2112 2080 2080 2144'
        ExpectTrue 'triangles built' ($i.Tris -gt 0) "tris $($i.Tris)"
        Expect 'geotemplateinfo of a missing id' (Eval '(geotemplateinfo 99)') ''
    }

    Step 'an off-grid template entity: the captured box snaps to the cubes it takes' {
        $before = Info 1
        # Box x 2048.5..2088.5: the block's first column (x 2048) leaves, the straddler joins
        Send "geot_moveent $($script:T1) 2068.5 2064 2128"
        $after = Info 1
        Expect 'captured box after an off-grid move' $after.Box '2056 2048 2112 2088 2080 2144'
        Expect 'one rebuild for the move' $after.Rebuilds ($before.Rebuilds + 1)
        Send "geot_moveent $($script:T1) 2064 2064 2128"
        Expect 'captured box after moving back' (Info 1).Box '2048 2048 2112 2080 2080 2144'
    }

    Step 're-id and duplicate ids' {
        Send "geot_setattr $($script:T1) 0 5"
        Expect 'the old id is gone' (Eval '(geotemplateinfo 1)') ''
        Expect 'the new id has the same box' (Info 5).Box '2048 2048 2112 2080 2080 2144'
        $dup = [int](Eval '(geot_newent geotemplate "5 8 8 8" 2300 2300 2300)')
        Expect 'a duplicate id keeps the first definition' (Info 5).Box '2048 2048 2112 2080 2080 2144'
        Send "geot_delent $dup"
        Send "geot_setattr $($script:T1) 0 1"
        Expect 'id 1 again' (Info 1).Box '2048 2048 2112 2080 2080 2144'
    }

    Step 'templates survive a save and reload' {
        $before = Info 1
        Send 'savemap harness_geot' 1500
        Invoke-MapLoad { Ed open harness_geot }
        $after = Info 1
        Expect 'box after reload' $after.Box $before.Box
        Expect 'triangles after reload' $after.Tris $before.Tris
    }

    # ==== later tasks add their steps here, in order ======================
```

- [ ] **Step 2: Build and run; confirm the new steps fail**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

Expected: the Task 1 steps pass. The new steps fail with `Unknown command: edfillsel`
and `Unknown command: geotemplateinfo` in the log, and empty `Info` results.

- [ ] **Step 3: Template vertex arrays (`octarender.cpp`)**

After `destroyva()`:

```cpp
// A geometry template's vertex arrays (geomtemplate.cpp): built from an octree
// that is not the world, into their own VBOs, leaving the world's lists,
// counters and pending VBO data untouched. updateva forces a vertex array per
// top-level cube, so the empty ones are dropped.
static void detachvas(cube *c)
{
    loopi(8)
    {
        if(c[i].ext) c[i].ext->va = NULL;
        if(c[i].children) detachvas(c[i].children);
    }
}

void destroytemplateva(vtxarray *va)
{
    if(va->vbuf) destroyvbo(va->vbuf);
    if(va->ebuf) destroyvbo(va->ebuf);
    if(va->skybuf) destroyvbo(va->skybuf);
    if(va->decalbuf) destroyvbo(va->decalbuf);
    if(va->texelems) delete[] va->texelems;
    if(va->decalelems) delete[] va->decalelems;
    if(va->matbuf) delete[] va->matbuf;
    delete va;
}

void buildtemplatevas(cube *root, vector<vtxarray *> &vas)
{
    flushvbo();
    vector<vtxarray *> oldvalist, oldvaroot;
    oldvalist.move(valist);
    oldvaroot.move(varoot);
    int oldwverts = wverts, oldwtris = wtris, oldallocva = allocva, oldrecalc = recalcprogress;
    cube *oldroot = worldroot;
    worldroot = root;

    int csi = 0;
    while(1<<csi < worldsize) csi++;
    updateva(worldroot, ivec(0, 0, 0), worldsize/2, csi-1);
    flushvbo();
    detachvas(worldroot);

    worldroot = oldroot;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        va->parent = NULL;
        va->children.setsize(0);
        if(va->verts) vas.add(va);
        else destroytemplateva(va);
    }
    valist.setsize(0);
    varoot.setsize(0);
    valist.move(oldvalist);
    varoot.move(oldvaroot);
    wverts = oldwverts;
    wtris = oldwtris;
    allocva = oldallocva;
    recalcprogress = oldrecalc;
    loadprogress = 0;
}
```

`updateva` and `recalcprogress` are defined before this point (`recalcprogress` at
`octarender.cpp:694`). If `updateva` is defined later in the file, put these functions
after it.

In `allchanged()`, change `PROGRESS(5); entitiesinoctanodes();` to:

```cpp
    PROGRESS(5); updategeomtemplates(true); entitiesinoctanodes();
```

- [ ] **Step 4: Registry, build and sync (`geomtemplate.cpp`)**

Append:

```cpp
vector<geomtemplate *> geomtemplates;
static bool geomtemplatesync = true; // template entities changed since the last update

geomtemplate *findgeomtemplate(int id)
{
    loopv(geomtemplates) if(geomtemplates[i]->id == id) return geomtemplates[i];
    return NULL;
}

geomtemplate *geominstancetemplate(const extentity &e)
{
    geomtemplate *t = findgeomtemplate(e.attrs[0]);
    return t && t->vas.length() ? t : NULL;
}

bool geominstancebb(const extentity &e, ivec &bbmin, ivec &bbmax)
{
    geomtemplate *t = geominstancetemplate(e);
    if(!t) return false;
    matrix4x3 m;
    calcgeominstance(e, t->pivot, m);
    calcgeominstancebb(m, t->capmin, t->capmax, bbmin, bbmax);
    return true;
}

int geotemplatestate(int id)
{
    geomtemplate *t = findgeomtemplate(id);
    return t ? t->tris : -1;
}

static void freegeomtemplate(geomtemplate &t)
{
    loopv(t.vas) destroytemplateva(t.vas[i]);
    t.vas.setsize(0);
    DELETEP(t.bih);
    t.verts = t.tris = 0;
}

static void buildgeomtemplate(geomtemplate &t)
{
    freegeomtemplate(t);
    cube *root = newcubes(F_EMPTY);
    capturegeomtemplate(worldroot, worldsize, t.reqmin, t.reqmax, root, t.capmin, t.capmax);
    if(!t.empty())
    {
        // calcmerges() works on worldroot and tells the root level apart by it
        cube *oldroot = worldroot;
        worldroot = root;
        calcmerges();
        worldroot = oldroot;
        buildtemplatevas(root, t.vas);
    }
    freeocta(root);
    loopv(t.vas)
    {
        t.verts += t.vas[i]->verts;
        t.tris += t.vas[i]->tris;
    }
    t.rebuilds++;
    t.dirty = false;
}

// An instance's place in the octree follows its template's pivot and captured
// box, so its instances leave the octree before either changes;
// entitiesinoctanodes() puts them back
static void removegeominstances(int id)
{
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type == ET_GEOINSTANCE && e.attrs[0] == id && e.flags&EF_OCTA) removeoctaentity(i);
    }
}

// Matches the templates to the geotemplate entities: the lowest index defining
// an id wins
static void syncgeomtemplates()
{
    const vector<extentity *> &ents = entities::getents();
    vector<int> seen;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_GEOTEMPLATE) continue;
        int id = e.attrs[0];
        geomtemplate *t = findgeomtemplate(id);
        if(seen.find(id) >= 0)
        {
            conoutf(colourred, "Geometry template %d is defined by entities %d and %d, using %d", id, t->ent, i, t->ent);
            continue;
        }
        seen.add(id);
        vec bmin, bmax;
        geomtemplatebox(e, bmin, bmax);
        if(!t)
        {
            removegeominstances(id);
            t = geomtemplates.add(new geomtemplate);
            t->id = id;
        }
        else if(t->ent != i || t->pivot != e.o || t->reqmin != bmin || t->reqmax != bmax)
        {
            removegeominstances(id);
            t->dirty = true;
        }
        t->ent = i;
        t->pivot = e.o;
        t->reqmin = bmin;
        t->reqmax = bmax;
    }
    loopvrev(geomtemplates)
    {
        geomtemplate *t = geomtemplates[i];
        if(seen.find(t->id) >= 0) continue;
        removegeominstances(t->id);
        freegeomtemplate(*t);
        delete t;
        geomtemplates.remove(i);
    }
}

void geomtemplateentschanged() { geomtemplatesync = true; }

bool geomtemplatesdirty()
{
    if(geomtemplatesync) return true;
    loopv(geomtemplates) if(geomtemplates[i]->dirty) return true;
    return false;
}

// Called by allchanged() (rebuildall) and commitchanges(), before
// entitiesinoctanodes(), which re-adds the instances removed here
void updategeomtemplates(bool rebuildall)
{
    if(geomtemplatesync || rebuildall)
    {
        syncgeomtemplates();
        geomtemplatesync = false;
    }
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        if(!t.dirty && !rebuildall) continue;
        removegeominstances(t.id);
        buildgeomtemplate(t);
    }
}

// GL teardown (cleanupva): instances leave the octree while their templates
// still say where they are, then everything is freed and resynced later
void cleargeomtemplates()
{
    loopv(geomtemplates)
    {
        removegeominstances(geomtemplates[i]->id);
        freegeomtemplate(*geomtemplates[i]);
    }
    geomtemplates.deletecontents();
    geomtemplatesync = true;
}

#ifdef DEBUG_UTILS
// "capmin capmax verts tris instances rebuilds" of a template, or "" -- the
// verification surface of tools/harness/geotemplate-selftest.ps1
ICOMMAND(0, geotemplateinfo, "i", (int *id),
{
    if(identflags&IDF_MAP) { result(""); return; }
    geomtemplate *t = findgeomtemplate(*id);
    if(!t) { result(""); return; }
    int instances = 0;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents) if(ents[i]->type == ET_GEOINSTANCE && ents[i]->attrs[0] == *id) instances++;
    ivec cmin = t->empty() ? ivec(0, 0, 0) : t->capmin, cmax = t->empty() ? ivec(0, 0, 0) : t->capmax;
    defformatstring(s, "%d %d %d %d %d %d %d %d %d %d", cmin.x, cmin.y, cmin.z, cmax.x, cmax.y, cmax.z, t->verts, t->tris, instances, t->rebuilds);
    result(s);
});
#endif
```

`engine.h`: add the `geomtemplate` struct and the declarations from this task's
Interfaces block after Task 2's. `src/shared/iengine.h`: add
`extern int geotemplatestate(int id);` near the other world/entity engine functions.

- [ ] **Step 5: Hooks**

`src/engine/world.cpp`, after `removeentityedit`:

```cpp
void removeoctaentity(int id) { removeentity(id); }
```

In `modifyoctaent(int flags, int id, extentity &e)`, in the `switch(e.type)` at its end, add:

```cpp
        case ET_GEOTEMPLATE:
            if(flags&MODOE_CHANGED) geomtemplateentschanged();
            break;
```

`src/engine/octaedit.cpp`, replace `commitchanges` with:

```cpp
void commitchanges(bool force)
{
    // Entity edits reach here without a geometry change; templates may still need work
    bool templates = geomtemplatesdirty();
    if(!force && !haschanged && !templates) return;
    haschanged = false;

    int oldlen = valist.length();
    resetclipplanes();
    inbetweenframes = false; // keeps progress() from drawing mid-edit
    if(templates) updategeomtemplates(false);
    entitiesinoctanodes();
    octarender();
    inbetweenframes = true;
    setupmaterials(oldlen);
    clearshadowcache();
    updatevabbs();
}
```

`src/engine/renderva.cpp`, `cleanupva()`: make `cleargeomtemplates();` its first line.

In `octaedit.cpp`'s DEBUG_UTILS block, after `edselbox`:

```cpp
// Fills (1) or empties (0) the current selection: deterministic test geometry
// for tools/harness/geotemplate-selftest.ps1. Local only (no edittrigger).
ICOMMAND(0, edfillsel, "i", (int *solid),
{
    if(identflags&IDF_MAP || noedit(true)) return;
    bool local = true;
    loopselxyz(discardchildren(c, true); if(*solid) solidfaces(c); else emptyfaces(c));
});
```

`src/game/entities.cpp`, extend the Task 1 info cases:

```cpp
            case GEOTEMPLATE:
            {
                defformatstring(str, "id %d", attr[0]);
                addentinfo(str);
                if(full && !geotemplatestate(attr[0])) addentinfo("empty");
                break;
            }
            case GEOINSTANCE:
            {
                defformatstring(str, "template %d", attr[0]);
                addentinfo(str);
                if(full && geotemplatestate(attr[0]) <= 0) addentinfo("missing");
                break;
            }
```

- [ ] **Step 6: Build and run the self-test**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

Expected: all steps pass, and the log has a console warning naming template 5 for the
duplicate. If the captured box is right but `Tris` is 0, `buildtemplatevas` produced no
vertex arrays. Check that `updateva` ran on the swapped `worldroot` and that `flushvbo`
ran.

- [ ] **Step 7: Commit**

```bash
git add src/engine/geomtemplate.cpp src/engine/octarender.cpp src/engine/engine.h src/engine/world.cpp src/engine/octaedit.cpp src/engine/renderva.cpp src/shared/iengine.h src/game/entities.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "geomtemplate: build templates into their own vertex arrays"
```

---

### Task 4: Live geometry edits

**Files:**
- Modify: `src/engine/geomtemplate.cpp` (`markgeomtemplates`), `src/engine/engine.h`
- Modify: `src/engine/octaedit.cpp` (both `changed()` overloads)
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Produces: `extern void markgeomtemplates(const ivec &bbmin, const ivec &bbmax);`

- [ ] **Step 1: Write the failing self-test steps**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 4: live geometry edits ======================================

    Step 'an edit inside the box rebuilds the template on the same commit' {
        $before = Info 1
        Ed sel $BX $BY ($BZ + 24) -Size 1,1,1   # the block's top corner cube
        Send 'edfillsel 0'
        $after = Info 1
        Expect 'rebuilt once' $after.Rebuilds ($before.Rebuilds + 1)
        ExpectTrue 'triangle count changed' ($after.Tris -ne $before.Tris) "$($before.Tris) -> $($after.Tris)"
        Expect 'captured box unchanged' $after.Box $before.Box
    }

    Step 'an edit outside every box does not rebuild' {
        $before = Info 1
        Ed sel 2400 2400 $BZ -Size 1,1,1
        Send 'edfillsel 1'
        Expect 'not rebuilt' (Info 1).Rebuilds $before.Rebuilds
    }

    Step 'overlapping templates both rebuild; undo rebuilds again' {
        $t2 = [int](Eval '(geot_newent geotemplate "2 20 20 20" 2048 2064 2128)')
        $a = Info 1
        $b = Info 2
        Ed sel 2056 2056 $BZ -Size 1,1,1   # inside both boxes
        Send 'edfillsel 0'
        Expect 'template 1 rebuilt' (Info 1).Rebuilds ($a.Rebuilds + 1)
        Expect 'template 2 rebuilt' (Info 2).Rebuilds ($b.Rebuilds + 1)
        Send 'undo'
        Expect 'undo rebuilds template 1 again' (Info 1).Rebuilds ($a.Rebuilds + 2)
        Expect 'undo restores the triangles' (Info 1).Tris $a.Tris
        Send "geot_delent $t2"
        Expect 'template 2 gone' (Eval '(geotemplateinfo 2)') ''
    }
```

- [ ] **Step 2: Build and run; confirm the first and third steps fail**

Expected: `rebuilt once` fails (Rebuilds unchanged), as do `template 1 rebuilt` and `undo rebuilds template 1 again`.

- [ ] **Step 3: Implement**

`geomtemplate.cpp`, after `geomtemplatesdirty`:

```cpp
// A geometry change in bbmin..bbmax (changed()): every template whose
// requested box it touches, grown by one, is rebuilt on the next commit
void markgeomtemplates(const ivec &bbmin, const ivec &bbmax)
{
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        if(t.reqmax.x < bbmin.x - 1 || t.reqmax.y < bbmin.y - 1 || t.reqmax.z < bbmin.z - 1 ||
           t.reqmin.x > bbmax.x + 1 || t.reqmin.y > bbmax.y + 1 || t.reqmin.z > bbmax.z + 1)
            continue;
        t.dirty = true;
    }
}
```

`octaedit.cpp`, the two `changed()` overloads:

```cpp
void changed(const ivec &bbmin, const ivec &bbmax, bool commit)
{
    readychanges(bbmin, bbmax, worldroot, ivec(0, 0, 0), worldsize/2);
    markgeomtemplates(bbmin, bbmax);
    haschanged = true;

    if(commit) commitchanges();
}

void changed(const block3 &sel, bool commit)
{
    if(sel.s.iszero()) return;
    ivec bbmin = ivec(sel.o).sub(1), bbmax = ivec(sel.s).mul(sel.grid).add(sel.o).add(1);
    readychanges(bbmin, bbmax, worldroot, ivec(0, 0, 0), worldsize/2);
    markgeomtemplates(bbmin, bbmax);
    haschanged = true;

    if(commit) commitchanges();
}
```

- [ ] **Step 4: Build and run the self-test**

Expected: all steps pass.

- [ ] **Step 5: Commit**

```bash
git add src/engine/geomtemplate.cpp src/engine/engine.h src/engine/octaedit.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "geomtemplate: rebuild templates on geometry edits"
```

---

### Task 5: Instances in the octree

**Files:**
- Modify: `src/engine/octa.h` (`octaentities`, `vtxarray`)
- Modify: `src/engine/octarender.cpp` (`vacollect`, `rendercube` ×2, `updatevabb`)
- Modify: `src/engine/world.cpp` (`getentboundingbox`, `modifyoctaentity`, `freeoctaentities`, `entselectionbox`)
- Modify: `src/engine/geomtemplate.cpp` (`geoinstancebb` command)
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Produces: `octaentities::instances` (`vector<int>`), `octaentities::instquery` (`occludequery *`, owner `&oe->instquery`), `vtxarray::instances` (`vector<octaentities *>`); command `geoinstancebb <entity index>` → `minx miny minz maxx maxy maxz` or empty.
- An instance is in `oe.instances` iff `geominstancetemplate(e)` was non-NULL when it was added. Otherwise it is in `oe.other`, like an invisible mapmodel.

- [ ] **Step 1: Write the failing self-test steps**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 5: instances in the octree ==================================

    Step 'instance bounds follow rotation and scale' {
        # Rebuild the block whole, so its captured box and pivot are symmetric:
        # pivot (2064,2064,2128), captured box +-16 around it
        Ed sel $BX $BY $BZ -Size 4,4,4
        Send 'edfillsel 1'
        Expect 'block whole again' (Info 1).Box '2048 2048 2112 2080 2080 2144'
        $script:I1 = [int](Eval '(geot_newent geoinstance "1 90 0 0 0 0 0 0 0" 2300 2048 2200)')
        Expect 'instance count' (Info 1).Instances 1
        Expect 'bounds at yaw 90' (Eval "(geoinstancebb $($script:I1))") '2284 2032 2184 2316 2064 2216'
        Send "geot_setattr $($script:I1) 4 50"
        Expect 'bounds at scale 50' (Eval "(geoinstancebb $($script:I1))") '2292 2040 2192 2308 2056 2208'
        Send "geot_setattr $($script:I1) 0 42"
        Expect 'no bounds without a template' (Eval "(geoinstancebb $($script:I1))") ''
        Send "geot_setattr $($script:I1) 0 1"
        Send "geot_setattr $($script:I1) 4 0"
        Expect 'bounds back' (Eval "(geoinstancebb $($script:I1))") '2284 2032 2184 2316 2064 2216'
    }
```

- [ ] **Step 2: Build and run; confirm it fails**

Expected: `Unknown command: geoinstancebb`, so the bounds checks fail.

- [ ] **Step 3: Data structures (`octa.h`)**

```cpp
struct octaentities
{
    vector<int> mapmodels, decals, other, instances;
    occludequery *query, *instquery;
    octaentities *next, *rnext;
    int distance;
    ivec o;
    int size;
    ivec bbmin, bbmax;

    octaentities(const ivec &o, int size) : query(0), instquery(0), o(o), size(size), bbmin(o), bbmax(o)
    {
        bbmin.add(size);
    }
};
```

In `struct vtxarray`: `vector<octaentities *> mapmodels, decals, instances;`.

- [ ] **Step 4: Vertex-array bookkeeping (`octarender.cpp`)**

In `struct vacollect`:
- change the member to `vector<octaentities *> mapmodels, decals, extdecals, instances;`;
- in `clear()` add `instances.setsize(0);`;
- in `setupdata()`, next to `if(mapmodels.length()) va->mapmodels.put(...)`, add
  `if(instances.length()) va->instances.put(instances.getbuf(), instances.length());`;
- in `emptyva()` add `&& instances.empty()`.

In `rendercube()`, in both places with `if(c.ext->ents->mapmodels.length()) vc.mapmodels.add(c.ext->ents);`, add:

```cpp
            if(c.ext->ents->instances.length()) vc.instances.add(c.ext->ents);
```

In `updatevabb()`, after the `va->mapmodels` loop:

```cpp
    loopv(va->instances)
    {
        octaentities *oe = va->instances[i];
        va->bbmin.min(oe->bbmin);
        va->bbmax.max(oe->bbmax);
    }
```

- [ ] **Step 5: Octree registration (`world.cpp`)**

`getentboundingbox`, first case in the switch:

```cpp
        case ET_GEOINSTANCE:
            if(geominstancebb(e, o, r)) break;
            o = ivec(vec(e.o).sub(entselradius));
            r = ivec(vec(e.o).add(entselradius+1));
            break;
```

Before `modifyoctaentity`:

```cpp
// The node's bounds from everything still registered in it
static void recalcoctaentbb(octaentities &oe)
{
    oe.bbmin = oe.bbmax = oe.o;
    oe.bbmin.add(oe.size);
    const vector<extentity *> &ents = entities::getents();
    const vector<int> *lists[3] = { &oe.mapmodels, &oe.decals, &oe.instances };
    loopk(3) loopvj(*lists[k])
    {
        ivec eo, er;
        if(getentboundingbox(*ents[(*lists[k])[j]], eo, er))
        {
            oe.bbmin.min(eo);
            oe.bbmax.max(er);
        }
    }
    oe.bbmin.max(oe.o);
    oe.bbmax.min(ivec(oe.o).add(oe.size));
}
```

In `modifyoctaentity`, the add switch (`flags&MODOE_ADD`), before `case ET_MAPMODEL:`:

```cpp
                case ET_GEOINSTANCE:
                    if(geominstancetemplate(e))
                    {
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.instances.empty()) va->instances.add(&oe);
                        }
                        oe.instances.add(id);
                        oe.bbmin.min(bo).max(oe.o);
                        oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    }
                    else oe.other.add(id);
                    break;
```

In the remove switch, before `case ET_MAPMODEL:`. It takes the instance out of both
lists, so it is right whatever the template's state:

```cpp
                case ET_GEOINSTANCE:
                    oe.instances.removeobj(id);
                    oe.other.removeobj(id);
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.instances.empty()) va->instances.removeobj(&oe);
                    }
                    recalcoctaentbb(oe);
                    break;
```

Change the free check after the remove switch to include `&& oe.instances.empty()`, and the line after it to:

```cpp
        if(c[i].ext && c[i].ext->ents) c[i].ext->ents->query = c[i].ext->ents->instquery = NULL;
```

`freeoctaentities`: add after the `decals` loop:

```cpp
        while(c.ext->ents && !c.ext->ents->instances.empty()) removeentity(c.ext->ents->instances.pop());
```

`entselectionbox`, before `if(!found)`:

```cpp
    else if(e.type == ET_GEOTEMPLATE)
    {
        if(!full) faked = true;
        else
        {
            vec bmin, bmax;
            geomtemplatebox(e, bmin, bmax);
            eo = vec(bmin).add(bmax).mul(0.5f);
            es = vec(bmax).sub(bmin).mul(0.5f);
            found = true;
        }
    }
    else if(e.type == ET_GEOINSTANCE)
    {
        ivec bbmin, bbmax;
        if(!full) faked = true;
        else if(geominstancebb(e, bbmin, bbmax))
        {
            eo = vec(bbmin).add(vec(bbmax)).mul(0.5f);
            es = vec(bbmax).sub(vec(bbmin)).mul(0.5f);
            found = true;
        }
    }
```

(As for `soundenv`: picking uses the small box, and a selected entity draws its full
box.)

- [ ] **Step 6: `geoinstancebb` (`geomtemplate.cpp`, inside the DEBUG_UTILS block)**

```cpp
ICOMMAND(0, geoinstancebb, "i", (int *idx),
{
    const vector<extentity *> &ents = entities::getents();
    ivec bbmin, bbmax;
    if(identflags&IDF_MAP || !ents.inrange(*idx) || ents[*idx]->type != ET_GEOINSTANCE || !geominstancebb(*ents[*idx], bbmin, bbmax)) { result(""); return; }
    defformatstring(s, "%d %d %d %d %d %d", bbmin.x, bbmin.y, bbmin.z, bbmax.x, bbmax.y, bbmax.z);
    result(s);
});
```

- [ ] **Step 7: Build and run the self-test**

Expected: all steps pass. Then, with `-KeepRunning`, hover the instance in the editor:
its selection box must appear. Edit its scale and confirm the selected box follows.

- [ ] **Step 8: Commit**

```bash
git add src/engine/octa.h src/engine/octarender.cpp src/engine/world.cpp src/engine/geomtemplate.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "geomtemplate: register instances in the octree"
```

---

### Task 6: Instance attributes in GL and the world shaders

**Files:**
- Modify: `src/shared/glemu.h` (attribute enum, `resetinstance`), `src/shared/glemu.cpp:22` (`attribnames`)
- Modify: `src/shared/glexts.h` (after the `GL_ARB_blend_func_extended` block), `src/engine/rendergl.cpp` (pointers near line 242, loads in the OpenGL 3.1 and 3.3 sections, `gl_init`)
- Create: `config/glsl/shared/instance.glsl`
- Modify: `config/glsl/world.cfg` (`worldvariantdefines`, `smworld`, `rsmworld`)
- Modify: `config/glsl/world/world.vert`, `bump.vert`, `smworld.vert`, `rsm.vert`, `world.frag`, `bump.frag`

**Interfaces:**
- Produces: `gle::ATTRIB_INSTANCE0..2` (10–12), `gle::ATTRIB_INSTANCESCALE` (13), `gle::MAXATTRIBS` 14, `gle::resetinstance()`, `glDrawElementsInstanced_`, `glVertexAttribDivisor_`; GLSL `INSTANCE_POS(v)`, `INSTANCE_DIR(n)`.

- [ ] **Step 1: Record the shader baseline before touching anything**

```powershell
tools\harness\harness.ps1 start
tools\harness\shaders.ps1 record -Sids s00 -NoMaps
tools\harness\harness.ps1 stop
```

Expected: a baseline corpus under `home\uitest\shadercorpus\`. If `record` does not
accept `-NoMaps`, run it without that flag.

- [ ] **Step 2: GL side**

`src/shared/glemu.h`, the attribute enum:

```cpp
        ATTRIB_HINTBLEND    = 9,
        ATTRIB_INSTANCE0    = 10,
        ATTRIB_INSTANCE1    = 11,
        ATTRIB_INSTANCE2    = 12,
        ATTRIB_INSTANCESCALE = 13,
        MAXATTRIBS          = 14
```

After the `GLE_INITATTRIBF(hintblend, ...)` group:

```cpp
    // Geometry template instances (renderva.cpp): rows of a 4x3 transform and
    // the inverse of its uniform scale. Arrays only during instanced draws;
    // otherwise their current values, identity and 1, leave world geometry
    // exactly as it was (config/glsl/shared/instance.glsl).
    static inline void resetinstance()
    {
        glVertexAttrib4f_(ATTRIB_INSTANCE0, 1, 0, 0, 0);
        glVertexAttrib4f_(ATTRIB_INSTANCE1, 0, 1, 0, 0);
        glVertexAttrib4f_(ATTRIB_INSTANCE2, 0, 0, 1, 0);
        glVertexAttrib1f_(ATTRIB_INSTANCESCALE, 1);
    }
```

`src/shared/glemu.cpp:22`:

```cpp
    extern const char * const attribnames[MAXATTRIBS] = { "vvertex", "vcolor", "vtexcoord0", "vtexcoord1", "vnormal", "vtangent", "vboneweight", "vboneindex", "vhintcolor", "vhintblend", "vinstance0", "vinstance1", "vinstance2", "vinstancescale" };
```

`src/shared/glexts.h`, after the `GL_ARB_blend_func_extended` block:

```cpp
// OpenGL 3.1 / 3.3: instanced drawing (geometry templates)
#ifndef GL_VERSION_3_1
typedef void (APIENTRYP PFNGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
#endif
#ifndef GL_VERSION_3_3
typedef void (APIENTRYP PFNGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);
#endif
extern PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced_;
extern PFNGLVERTEXATTRIBDIVISORPROC   glVertexAttribDivisor_;
```

`src/engine/rendergl.cpp`, after `PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed_ = NULL;`:

```cpp
// OpenGL 3.1 / 3.3: instanced drawing
PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced_ = NULL;
PFNGLVERTEXATTRIBDIVISORPROC   glVertexAttribDivisor_   = NULL;
```

In the `// OpenGL 3.1` load section add
`glDrawElementsInstanced_ = (PFNGLDRAWELEMENTSINSTANCEDPROC)getprocaddress("glDrawElementsInstanced");`,
and in `// OpenGL 3.3` add
`glVertexAttribDivisor_ = (PFNGLVERTEXATTRIBDIVISORPROC)getprocaddress("glVertexAttribDivisor");`.

In `gl_init()`, right after `gle::setup();`:

```cpp
    gle::resetinstance();
```

- [ ] **Step 3: Shader side**

Create `config/glsl/shared/instance.glsl`:

```glsl
// Geometry template instances (src/engine/geomtemplate.cpp, renderva.cpp):
// world geometry through the rows of a 4x3 transform. Instanced draws feed
// per-instance rows and the inverse of their uniform scale; everything else
// gets the attributes' constant values, identity rows and 1
// (gle::resetinstance), for which INSTANCE_POS and INSTANCE_DIR return their
// input bit for bit. Include with shader_include_vs.
in vec4 vinstance0, vinstance1, vinstance2;
in float vinstancescale;
#define INSTANCE_POS(v) vec4(dot(vinstance0, v), dot(vinstance1, v), dot(vinstance2, v), v.w)
#define INSTANCE_DIR(n) (vec3(dot(vinstance0.xyz, n), dot(vinstance1.xyz, n), dot(vinstance2.xyz, n))*vinstancescale)
```

`config/glsl/world.cfg`, `worldvariantdefines`: after `shader_include_vs "config/glsl/shared/gbuffer.glsl"` add `shader_include_vs "config/glsl/shared/instance.glsl"`. The `smworld` body and the `rsmworld` variant body each get `shader_include_vs "config/glsl/shared/instance.glsl"` before their `shader_source`.

`config/glsl/world/smworld.vert`:

```glsl
void main(void)
{
    gl_Position = shadowmatrix * INSTANCE_POS(vvertex);
}
```

`config/glsl/world/rsm.vert`, `main()`:

```glsl
void main(void)
{
    vec4 wpos = INSTANCE_POS(vvertex);
    gl_Position = rsmmatrix * wpos;
    texcoord0 = vtexcoord0 + texgenscroll;
#ifdef RSM_BLEND
    texcoord1 = (wpos.xy - blendmapparams.xy)*blendmapparams.zw;
#endif
    vec3 wnormal = INSTANCE_DIR(vnormal);
    normal = vec4(wnormal, dot(wnormal, rsmdir));
}
```

`config/glsl/world/world.vert`:
- in the `#ifdef WORLD_TRIPLANAR` declarations add `out vec3 tnormal;`;
- `main()` starts with `vec4 wpos = INSTANCE_POS(vvertex);` and
  `gl_Position = camprojmatrix * wpos;`;
- the triplanar texcoord lines keep using `vvertex` (local space, so the texture stays
  on the instance). Add `tnormal = vnormal;` at the end of the triplanar branch;
- `texcoord1 = (wpos.xy - blendmapparams.xy)*blendmapparams.zw;`;
- `nvec = INSTANCE_DIR(vnormal);`;
- `camvec = camera - wpos.xyz;`.

`config/glsl/world/world.frag`:
- in the `#ifdef WORLD_TRIPLANAR` inputs add `in vec3 tnormal;`;
- line 76 becomes `vec3 triblend = max(abs(normalize(tnormal)) - triplanarbias.xyz, 0.001);`.
  The blend weights must come from the axes the texture is projected on, which are
  local. For the world `tnormal == nvec`, so nothing changes there.

`config/glsl/world/bump.vert`:
- in the triplanar declarations add `out vec3 tnormal;`;
- `main()` starts with:
  ```glsl
      vec4 wpos = INSTANCE_POS(vvertex);
      vec3 wnormal = INSTANCE_DIR(vnormal);
      gl_Position = camprojmatrix * wpos;
  ```
- triplanar branch: texcoords keep `vvertex`; then
  ```glsl
      normal = wnormal;
      tnormal = vnormal;
      tangentx = INSTANCE_DIR(normalize(vec3(1.001, 0.0, 0.0) - vnormal*vnormal.x));
      tangenty = INSTANCE_DIR(normalize(vec3(0.0, 1.001, 0.0) - vnormal*vnormal.y));
      tangentz = INSTANCE_DIR(normalize(vec3(0.0, 0.0, -1.001) + vnormal*vnormal.z));
  ```
- non-triplanar branch:
  ```glsl
      vec3 wtangent = INSTANCE_DIR(vtangent.xyz);
      vec3 bitangent = cross(wnormal, wtangent) * vtangent.w;
      // calculate tangent -> world transform
      world = mat3(wtangent, bitangent, wnormal);
  ```
- `camvec = camera - wpos.xyz;` and `texcoord1 = (wpos.xy - blendmapparams.xy)*blendmapparams.zw;`.

`config/glsl/world/bump.frag`: add `in vec3 tnormal;` to the triplanar inputs (line 24),
and line 64 becomes `vec3 triblend = max(abs(tnormal) - triplanarbias.xyz, 0.001);`.

- [ ] **Step 4: Build, then check that world shaders still render the same**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
tools\harness\harness.ps1 start
tools\harness\shaders.ps1 check -Sids s00 -NoMaps
```

Expected: every shader other than `*world*` / `smworld` / `rsmworld` passes. The world
shaders change contract (new `vinstance*` attributes, a `tnormal` varying in triplanar
variants). If they report `FAIL`, run `tools\harness\shaders.ps1 diff bumpworld -Sid s00`
and `... diff triplanarworld -Sid s00` and confirm the only differences are those
attributes and varyings, not pixels. A pixel mismatch on any world shader is a
regression: fix it before continuing.

Then run the self-test again to confirm nothing else broke:

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

- [ ] **Step 5: Commit**

```bash
git add src/shared/glemu.h src/shared/glemu.cpp src/shared/glexts.h src/engine/rendergl.cpp config/glsl/shared/instance.glsl config/glsl/world.cfg config/glsl/world/world.vert config/glsl/world/bump.vert config/glsl/world/smworld.vert config/glsl/world/rsm.vert config/glsl/world/world.frag config/glsl/world/bump.frag
git commit -m "glsl: read a per-instance transform in the world shaders"
```

---

### Task 7: Instanced G-buffer drawing

**Files:**
- Modify: `src/engine/renderva.cpp` (`renderstate`, `renderbatch`, new instance section before `rendergeom`, `cleanupva`)
- Modify: `src/engine/renderlights.cpp` (`rendergbuffer`)
- Modify: `src/engine/engine.h`
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Consumes: Task 5's lists, Task 6's attributes.
- Produces (static in `renderva.cpp`, used by Tasks 9–10): `struct instancedata`, `struct instancegroup { geomtemplate *t; int offset, count; }`, `static vector<instancegroup> instgroups`, `static void prepareinstances(const vector<int> &list)`, `static void bindinstances(int offset)`, `static void unbindinstances()`, `static void renderinstancegroups(renderstate &cur, int pass)`, `static inline bool geominstancevisible(extentity &e)`; public `void renderinstances()`, `void cleanupinstances()`; command `geoinststats` → `instances triangles` of the last G-buffer pass.

- [ ] **Step 1: Write the failing self-test steps**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 7: G-buffer instances =======================================

    Step 'G-buffer: visible instances are drawn instanced' {
        $script:I2 = [int](Eval '(geot_newent geoinstance "1 45 0 0 150 0 0 0 0" 2360 2048 2200)')
        Ed frame 2330 2048 2200 -Dist 220 -Yaw 0 -Pitch -15
        Send 'sleep 1 []' 400
        $st = @((Eval '(geoinststats)') -split ' ')
        Expect 'instances drawn' $st[0] '2'
        Expect 'triangles drawn' $st[1] ([string](2 * (Info 1).Tris))
        # Must show two copies of the block in the air: one turned 90 degrees, one
        # turned 45 degrees and 1.5x larger, textured like the source.
        Shot 'geot-instances'
    }

    Step 'editing the source changes every instance at once' {
        Ed sel $BX $BY ($BZ + 24) -Size 1,1,1
        Send 'edfillsel 0'
        Send 'sleep 1 []' 400
        $st = @((Eval '(geoinststats)') -split ' ')
        Expect 'triangles drawn follow the edit' $st[1] ([string](2 * (Info 1).Tris))
        # Both copies now miss the same top corner cube as the source.
        Shot 'geot-instances-edited'
        Ed sel $BX $BY ($BZ + 24) -Size 1,1,1
        Send 'edfillsel 1'
    }

    Step 'instances out of view are not drawn' {
        Ed goto 2330 2048 2600
        Ed aim 0 90
        Send 'sleep 1 []' 400
        Expect 'nothing drawn looking at the sky' (@((Eval '(geoinststats)') -split ' ')[0]) '0'
    }
```

- [ ] **Step 2: Build and run; confirm it fails**

Expected: `Unknown command: geoinststats`, so the instance counts fail.

- [ ] **Step 3: Implement (`renderva.cpp`)**

`renderstate`: add `int instances;` after `bool blend;`, and `instances(0)` to the
constructor's initialiser list after `blend(false)`.

`renderbatch`:

```cpp
static void renderbatch(renderstate &cur, int pass, geombatch &b)
{
    gbatches++;
    for(geombatch *curbatch = &b;; curbatch = &geombatches[curbatch->batch])
    {
        ushort len = curbatch->es.length;
        if(len)
        {
            const ushort *indices = (const ushort *)0 + curbatch->va->eoffset + curbatch->offset;
            if(cur.instances)
            {
                glDrawElementsInstanced_(GL_TRIANGLES, len, GL_UNSIGNED_SHORT, indices, cur.instances);
                glde++;
                vtris += (len/3)*cur.instances;
            }
            else
            {
                drawtris(len, indices, curbatch->es.minvert, curbatch->es.maxvert);
                vtris += len/3;
            }
        }
        if(curbatch->batch < 0) break;
    }
}
```

New section before `void rendergeom()`:

```cpp
////////// geometry template instances //////////

// Per-instance vertex data, see config/glsl/shared/instance.glsl
struct instancedata
{
    vec4 rows[3];
    float invscale;
};

struct instancegroup
{
    geomtemplate *t;
    int offset, count; // byte offset in the stream buffer, number of instances
};

static GLuint instvbo = 0;
static int instvbosize = 0, instvbooffset = 0;
static vector<instancedata> instdata;
static vector<instancegroup> instgroups;

// Appends to the stream buffer, orphaning it when full; returns the byte offset
static int uploadinstances(const vector<instancedata> &data)
{
    int len = data.length()*sizeof(instancedata);
    if(!instvbo) glGenBuffers_(1, &instvbo);
    gle::bindvbo(instvbo);
    if(instvbooffset + len > instvbosize)
    {
        instvbosize = max(instvbosize, max(len, 1<<16));
        glBufferData_(GL_ARRAY_BUFFER, instvbosize, NULL, GL_STREAM_DRAW);
        instvbooffset = 0;
    }
    glBufferSubData_(GL_ARRAY_BUFFER, instvbooffset, len, data.getbuf());
    int offset = instvbooffset;
    instvbooffset += len;
    gle::clearvbo();
    return offset;
}

static void bindinstances(int offset)
{
    gle::bindvbo(instvbo);
    loopi(3)
    {
        glVertexAttribPointer_(gle::ATTRIB_INSTANCE0 + i, 4, GL_FLOAT, GL_FALSE, sizeof(instancedata), (const void *)(size_t)(offset + i*sizeof(vec4)));
        glVertexAttribDivisor_(gle::ATTRIB_INSTANCE0 + i, 1);
        glEnableVertexAttribArray_(gle::ATTRIB_INSTANCE0 + i);
    }
    glVertexAttribPointer_(gle::ATTRIB_INSTANCESCALE, 1, GL_FLOAT, GL_FALSE, sizeof(instancedata), (const void *)(size_t)(offset + 3*sizeof(vec4)));
    glVertexAttribDivisor_(gle::ATTRIB_INSTANCESCALE, 1);
    glEnableVertexAttribArray_(gle::ATTRIB_INSTANCESCALE);
    gle::clearvbo();
}

// GL leaves an enabled array's current value undefined: put identity back
static void unbindinstances()
{
    loopi(4)
    {
        glDisableVertexAttribArray_(gle::ATTRIB_INSTANCE0 + i);
        glVertexAttribDivisor_(gle::ATTRIB_INSTANCE0 + i, 0);
    }
    gle::resetinstance();
}

static inline bool geominstancecmp(const int &a, const int &b)
{
    const vector<extentity *> &ents = entities::getents();
    int ta = ents[a]->attrs[0], tb = ents[b]->attrs[0];
    return ta < tb || (ta == tb && a < b);
}

// Groups instances (entity indices) by template and uploads their transforms
// into instgroups; instances without a drawable template are skipped
static void prepareinstances(const vector<int> &list)
{
    static vector<int> sorted;
    instgroups.setsize(0);
    instdata.setsize(0);
    if(list.empty()) return;
    sorted.setsize(0);
    sorted.put(list.getbuf(), list.length());
    sorted.sort(geominstancecmp);
    const vector<extentity *> &ents = entities::getents();
    loopv(sorted)
    {
        extentity &e = *ents[sorted[i]];
        geomtemplate *t = geominstancetemplate(e);
        if(!t) continue;
        if(instgroups.empty() || instgroups.last().t != t)
        {
            instancegroup &g = instgroups.add();
            g.t = t;
            g.offset = instdata.length();
            g.count = 0;
        }
        matrix4x3 m;
        calcgeominstance(e, t->pivot, m);
        instancedata &d = instdata.add();
        loopk(3) d.rows[k] = vec4(m.a[k], m.b[k], m.c[k], m.d[k]);
        d.invscale = 1/geominstancescale(e);
        instgroups.last().count++;
    }
    if(instdata.empty()) return;
    int base = uploadinstances(instdata);
    loopv(instgroups) instgroups[i].offset = base + instgroups[i].offset*sizeof(instancedata);
}

// Opaque batches of each group's template, every instance at once
static void renderinstancegroups(renderstate &cur, int pass)
{
    loopv(instgroups)
    {
        instancegroup &g = instgroups[i];
        bindinstances(g.offset);
        cur.vbuf = 0; // bindinstances changed the array buffer
        cur.instances = g.count;
        loopvj(g.t->vas) if(g.t->vas[j]->texs) mergetexs(cur, g.t->vas[j]);
        if(geombatches.length()) renderbatches(cur, pass);
        cur.instances = 0;
        unbindinstances();
    }
}

static inline bool geominstancevisible(extentity &e)
{
    return !(e.flags&EF_NOVIS) && entities::isallowed(e) && geominstancetemplate(e);
}

VAR(0, oqinst, 0, 1, 1);
static vector<int> visinsts;
static vector<octaentities *> instoes;
static int instdrawn = 0, insttrisdrawn = 0;

// Instances in visible, unfogged nodes, once each (EF_RENDER marks them while
// collecting: an instance registers in every node its box overlaps)
static void findvisibleinstances(bool doquery)
{
    visinsts.setsize(0);
    instoes.setsize(0);
    const vector<extentity *> &ents = entities::getents();
    for(vtxarray *va = visibleva; va; va = va->next) if(va->occluded < OCCLUDE_BB && va->curvfc < VFC_FOGGED) loopv(va->instances)
    {
        octaentities *oe = va->instances[i];
        if(isfoggedcube(oe->o, oe->size) || pvsoccluded(oe->bbmin, oe->bbmax)) continue;
        instoes.add(oe);
        if(doquery && oe->instquery && oe->instquery->owner == &oe->instquery && checkquery(oe->instquery)) continue;
        loopvj(oe->instances)
        {
            int n = oe->instances[j];
            extentity &e = *ents[n];
            if(e.flags&EF_RENDER || !geominstancevisible(e)) continue;
            ivec bbmin, bbmax;
            if(!geominstancebb(e, bbmin, bbmax) || isvisiblebb(bbmin, ivec(bbmax).sub(bbmin)) >= VFC_FOGGED) continue;
            e.flags |= EF_RENDER;
            visinsts.add(n);
        }
    }
    loopv(visinsts) ents[visinsts[i]]->flags &= ~EF_RENDER;
}

// A bounding-box query per node, read by the next frame's findvisibleinstances
static void queryinstances(bool doquery)
{
    if(!doquery)
    {
        loopv(instoes) instoes[i]->instquery = NULL;
        return;
    }
    bool started = false;
    loopv(instoes)
    {
        octaentities *oe = instoes[i];
        if(camera1->o.insidebb(oe->bbmin, oe->bbmax, 1)) { oe->instquery = NULL; continue; }
        oe->instquery = newquery(&oe->instquery);
        if(!oe->instquery) continue;
        if(!started) { startbb(); started = true; }
        startquery(oe->instquery);
        drawbb(oe->bbmin, ivec(oe->bbmax).sub(oe->bbmin));
        endquery(oe->instquery);
    }
    if(started) endbb();
}

// G-buffer pass, right after rendergeom()
void renderinstances()
{
    bool doquery = (!drawtex || isoqstate()) && oqfrags && oqinst;
    findvisibleinstances(doquery);
    instdrawn = insttrisdrawn = 0;
    prepareinstances(visinsts);
    if(instgroups.length())
    {
        loopv(instgroups)
        {
            instdrawn += instgroups[i].count;
            insttrisdrawn += instgroups[i].count*instgroups[i].t->tris;
        }
        renderstate cur;
        setupgeom(cur);
        resetbatches();
        renderinstancegroups(cur, RENDERPASS_GBUFFER);
        cleanupgeom(cur);
    }
    queryinstances(doquery);
}

void cleanupinstances()
{
    if(instvbo) { glDeleteBuffers_(1, &instvbo); instvbo = 0; }
    instvbosize = instvbooffset = 0;
}

#ifdef DEBUG_UTILS
// "instances triangles" drawn by the last G-buffer pass
ICOMMAND(0, geoinststats, "", (),
{
    if(identflags&IDF_MAP) { result(""); return; }
    defformatstring(s, "%d %d", instdrawn, insttrisdrawn);
    result(s);
});
#endif
```

`cleanupva()`: add `cleanupinstances();` after `cleargeomtemplates();`.

`engine.h`: `extern void renderinstances();` and `extern void cleanupinstances();` with the renderva declarations.

`src/engine/renderlights.cpp`, `rendergbuffer()`:

```cpp
    rendergeom();
    renderinstances();
```

- [ ] **Step 4: Build and run the self-test; look at the screenshots**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
```

Expected: all steps pass. Open `geot-instances.png` and `geot-instances-edited.png` (the
harness prints their paths under `home\uitest\`). They must match the comments in the
self-test, with lit, textured faces and no black or stretched textures. If `instances
drawn` is 0 but no step errors, check the octree registration (Task 5) and that
`va->instances` is filled for the vertex arrays rebuilt by `octarender`.

- [ ] **Step 5: Commit**

```bash
git add src/engine/renderva.cpp src/engine/renderlights.cpp src/engine/engine.h tools/harness/geotemplate-selftest.ps1
git commit -m "renderva: draw geometry instances in the g-buffer"
```

---

### Task 8: Template boxes in the editor

**Files:**
- Modify: `src/engine/geomtemplate.cpp`, `src/engine/engine.h`, `src/engine/octaedit.cpp:664`
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Produces: `extern void rendergeomtemplateboxes();`

- [ ] **Step 1: Add the review step**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 8: editor boxes =============================================

    Step 'edit mode draws each template box' {
        Ed frame 2064 2064 2128 -Dist 140 -Yaw 30 -Pitch -25
        Send 'sleep 1 []' 400
        # Must show the source block with a faint grey box (requested, 2044..2084)
        # around a bright cyan box tight on the block (captured, 2048..2080).
        Shot 'geot-boxes'
    }
```

- [ ] **Step 2: Run it before implementing, as the "failing" baseline**

Expected: the step passes (it only shoots), but `geot-boxes.png` shows no template
boxes. Keep it to compare.

- [ ] **Step 3: Implement**

`geomtemplate.cpp`:

```cpp
extern void boxs3D(const vec &o, vec s, int g);

// Edit mode: each template's requested box faint, its captured box bright,
// so the cubes a template takes are visible at a glance. Called with the
// entity selection's GL state (ldrnotextureshader, additive blend).
void rendergeomtemplateboxes()
{
    if(!editmode) return;
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        gle::colorub(48, 48, 48);
        boxs3D(t.reqmin, vec(t.reqmax).sub(t.reqmin), 1);
        if(t.empty()) continue;
        gle::colorub(0, 160, 160);
        boxs3D(vec(t.capmin), vec(ivec(t.capmax).sub(t.capmin)), 1);
    }
}
```

`octaedit.cpp`, after `renderentselection(player->o, cursordir, entmoving!=0);`:

```cpp
    rendergeomtemplateboxes();
```

`engine.h`: declare it with the other geomtemplate functions.

- [ ] **Step 4: Build, run, and compare the screenshot**

Expected: `geot-boxes.png` now shows both boxes as described in the step comment.

- [ ] **Step 5: Commit**

```bash
git add src/engine/geomtemplate.cpp src/engine/engine.h src/engine/octaedit.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "geomtemplate: show requested and captured boxes in the editor"
```

---

## Phase 2 — shadows and GI

### Task 9: Shadow maps and the reflective shadow map

**Files:**
- Modify: `src/engine/renderva.cpp` (`findshadowmms`, `rendershadowmapworld`, `renderrsmgeom`)
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Consumes: Task 7's `prepareinstances`, `bindinstances`, `unbindinstances`, `renderinstancegroups`, `geominstancevisible`.
- Produces: `static vector<int> shadowinsts; static vector<uchar> shadowinstmasks;` filled by `findshadowmms()` for every caller (shadow maps, RSM, `genshadowmesh`).

- [ ] **Step 1: Write the self-test steps**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 9: shadows and GI ===========================================

    Step 'instances cast sun and point-light shadows' {
        Send 'sunlightpitch 50; sunlightyaw 30' 400
        Ed frame 2330 2048 2120 -Dist 260 -Yaw 200 -Pitch -35
        Send 'sleep 1 []' 500
        # Must show both instances' shadows on the floor below them, matching
        # their shapes (one square-ish, one turned 45 degrees and larger).
        Shot 'geot-sun-shadow'
        $script:L1 = [int](Eval '(geot_newent light "400 255 255 255" 2330 2120 2240)')
        Send 'sunlight 0' 500
        # Must show point-light shadows of both instances on the floor, cast away from the light.
        Shot 'geot-point-shadow'
        Send 'sunlight 0xA0A090' 300
    }

    Step 'instances bounce light into the radiance hints' {
        $pts = Join-Path $HomeDir 'harness\geot-rh-points.txt'
        New-Item -ItemType Directory -Force (Split-Path $pts) | Out-Null
        Write-TextNoBom $pts ("2300 2048 2049 0 0 1`n2330 2048 2049 0 0 1`n2360 2048 2049 0 0 1`n")
        Send "geot_delent $($script:L1)" 300
        Send 'sleep 1 []' 600
        $n = Eval '(rhprobe "harness/geot-rh-points.txt" "harness/geot-rh-a.txt")'
        Expect 'probed points' $n '3'
        Send "geot_setattr $($script:I1) 5 1; geot_setattr $($script:I2) 5 1" 600   # no-shadow: out of the RSM
        Send 'sleep 1 []' 600
        $n = Eval '(rhprobe "harness/geot-rh-points.txt" "harness/geot-rh-b.txt")'
        $a = Get-Content (Join-Path $HomeDir 'harness\geot-rh-a.txt') -Raw
        $b = Get-Content (Join-Path $HomeDir 'harness\geot-rh-b.txt') -Raw
        ExpectTrue 'GI under the instances changes when they leave the RSM' ($a -ne $b)
        Send "geot_setattr $($script:I1) 5 0; geot_setattr $($script:I2) 5 0" 300
    }
```

- [ ] **Step 2: Run; confirm the failures**

Expected: the screenshots show no instance shadows, and `GI ... changes` FAILs (the
instances are not in the RSM yet). If `probed points` is 0, GI is off in this world:
add `Send 'rh 1' 300` (or the GI var that `tools/harness/gi.ps1` sets) at the start of
the step and re-run. That is a test setup issue, not the feature.

- [ ] **Step 3: Implement (`renderva.cpp`)**

After `findshadowmms()`'s body (as a new static function called from its last line):

```cpp
// Instances for the current shadow map, RSM or shadow mesh (findshadowvas has
// set shadowva), with the sides each one touches -- the same tests
// findshadowvas makes for a vertex array
static void findshadowinstances()
{
    shadowinsts.setsize(0);
    shadowinstmasks.setsize(0);
    const vector<extentity *> &ents = entities::getents();
    for(vtxarray *va = shadowva; va; va = va->rnext) loopvj(va->instances)
    {
        octaentities *oe = va->instances[j];
        loopvk(oe->instances)
        {
            int n = oe->instances[k];
            extentity &e = *ents[n];
            if(e.flags&EF_RENDER || e.attrs[5]&GEOINST_NOSHADOW || !geominstancevisible(e)) continue;
            ivec bbmin, bbmax;
            if(!geominstancebb(e, bbmin, bbmax)) continue;
            int mask = 0;
            switch(shadowmapping)
            {
                case SM_REFLECT: mask = calcbbrsmsplits(bbmin, bbmax); break;
                case SM_CASCADE: mask = calcbbcsmsplits(bbmin, bbmax); break;
                case SM_CUBEMAP:
                    if(smdistcull && shadoworigin.dist_to_bb(bbmin, bbmax) >= shadowradius) continue;
                    mask = smbbcull ? 0x3F : calcbbsidemask(bbmin, bbmax, shadoworigin, shadowradius, shadowbias);
                    break;
                case SM_SPOT:
                    if(smdistcull && shadoworigin.dist_to_bb(bbmin, bbmax) >= shadowradius) continue;
                    mask = !smbbcull || bbinsidespot(shadoworigin, shadowdir, shadowspot, bbmin, bbmax) ? 1 : 0;
                    break;
            }
            if(!mask) continue;
            e.flags |= EF_RENDER;
            shadowinsts.add(n);
            shadowinstmasks.add(mask);
        }
    }
    loopv(shadowinsts) ents[shadowinsts[i]]->flags &= ~EF_RENDER;
}
```

Placement: Task 7's section uses `renderstate`, `mergetexs` and `renderbatches`, so it
must stay after them (just before `rendergeom`). `findshadowmms` (~line 1296) and
`rendershadowmapworld` (~line 1246) come earlier. So, above `rendershadowmapworld`:

```cpp
static vector<int> shadowinsts;     // filled by findshadowinstances (below)
static vector<uchar> shadowinstmasks;
static void findshadowinstances();
static void rendershadowinstances();
```

Then put both function definitions after Task 7's section. Call
`findshadowinstances();` as the last line of `findshadowmms()`, and don't declare the two
vectors again where the definition goes.

After Task 7's section:

```cpp
// The current shadow map side; rendershadowmapworld's state (smworld bound,
// vertex array enabled)
static void rendershadowinstances()
{
    static vector<int> side;
    side.setsize(0);
    loopv(shadowinsts) if(shadowinstmasks[i]&(1<<shadowside)) side.add(shadowinsts[i]);
    prepareinstances(side);
    loopv(instgroups)
    {
        instancegroup &g = instgroups[i];
        bindinstances(g.offset);
        loopvj(g.t->vas)
        {
            vtxarray *va = g.t->vas[j];
            if(!va->tris) continue;
            gle::bindvbo(va->vbuf);
            gle::bindebo(va->ebuf);
            const vertex *ptr = 0;
            gle::vertexpointer(sizeof(vertex), ptr->pos.v);
            glDrawElementsInstanced_(GL_TRIANGLES, 3*va->tris, GL_UNSIGNED_SHORT, (const ushort *)0 + va->eoffset, g.count);
            glde++;
            xtravertsva += 3*va->tris*g.count;
        }
        unbindinstances();
    }
}
```

`rendershadowmapworld()`: after the world-triangle loop and before `if(getskyshadow())`:

```cpp
    if(shadowinsts.length() && !smnodraw) rendershadowinstances();
```

`renderrsmgeom()` comes after Task 7's section, so it needs no declarations. After
`if(geombatches.length()) renderbatches(cur, RENDERPASS_RSM);`:

```cpp
    // One RSM side: every collected instance (calcbbrsmsplits did the culling)
    prepareinstances(shadowinsts);
    renderinstancegroups(cur, RENDERPASS_RSM);
```

- [ ] **Step 4: Build, run, look at the screenshots**

Expected: all steps pass. `geot-sun-shadow.png` and `geot-point-shadow.png` show the
shadows described in the step comments.

- [ ] **Step 5: Commit**

```bash
git add src/engine/renderva.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "renderva: draw geometry instances into shadow maps and the rsm"
```

---

### Task 10: Cached point-light shadow meshes

**Files:**
- Modify: `src/engine/renderva.cpp` (`genshadowmesh`)
- Modify: `tools/harness/geotemplate-selftest.ps1`

- [ ] **Step 1: Write the self-test step**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 10: cached shadow meshes ====================================

    function Compare-Shots([string]$A, [string]$B) {
        Add-Type -AssemblyName System.Drawing
        $pa = Get-ChildItem $HomeDir -Recurse -Filter "$A.png" | Select-Object -First 1
        $pb = Get-ChildItem $HomeDir -Recurse -Filter "$B.png" | Select-Object -First 1
        $ia = [System.Drawing.Bitmap]::new($pa.FullName)
        $ib = [System.Drawing.Bitmap]::new($pb.FullName)
        try {
            $diff = 0
            for ($y = 0; $y -lt $ia.Height; $y += 2) {
                for ($x = 0; $x -lt $ia.Width; $x += 2) {
                    $ca = $ia.GetPixel($x, $y); $cb = $ib.GetPixel($x, $y)
                    if ([math]::Abs($ca.R - $cb.R) + [math]::Abs($ca.G - $cb.G) + [math]::Abs($ca.B - $cb.B) -gt 24) { $diff++ }
                }
            }
            return $diff / (($ia.Width / 2) * ($ia.Height / 2))
        } finally { $ia.Dispose(); $ib.Dispose() }
    }

    Step 'a cached shadow mesh includes the instances' {
        $script:L2 = [int](Eval '(geot_newent light "400 255 255 255" 2330 2120 2240)')
        Send 'sunlight 0' 300
        Send 'savemap harness_geot' 1500
        Invoke-MapLoad { Ed open harness_geot }   # shadow meshes are built at load
        Send 'smmesh 1' 300
        Ed frame 2330 2048 2120 -Dist 260 -Yaw 200 -Pitch -35
        Send 'sleep 1 []' 600
        Shot 'geot-mesh-on'
        Send 'smmesh 0' 300                        # clears the meshes: the live path
        Send 'sunlight 1; sunlight 0' 300          # sunlight's VARF clears the shadow-map cache too
        Send 'sleep 1 []' 600
        Shot 'geot-mesh-off'
        $frac = Compare-Shots 'geot-mesh-on' 'geot-mesh-off'
        ExpectTrue 'cached and live shadows agree' ($frac -lt 0.01) ("{0:P2} of pixels differ" -f $frac)
        Send 'smmesh 1; sunlight 0xA0A090' 300
        Send "geot_delent $($script:L2)" 300
    }
```

- [ ] **Step 2: Run; confirm it fails**

Expected: `cached and live shadows agree` FAILs. In `geot-mesh-on.png` the instances
cast no point-light shadow.

- [ ] **Step 3: Implement (`renderva.cpp`)**

After `genshadowmeshmapmodels`:

```cpp
// Instance triangles into a light's cached mesh. rendershadowmesh replaces
// rendershadowmapworld for that light, so baked instances are not drawn twice.
static void genshadowmeshinstances(shadowmesh &m, int sides, shadowdrawinfo draws[6])
{
    const vector<extentity *> &ents = entities::getents();
    loopv(shadowinsts)
    {
        extentity &e = *ents[shadowinsts[i]];
        geomtemplate *t = geominstancetemplate(e);
        if(!t) continue;
        matrix4x3 orient;
        calcgeominstance(e, t->pivot, orient);
        loopvj(t->vas)
        {
            vtxarray *va = t->vas[j];
            const ushort *idx = va->edata + va->eoffset;
            loopk(va->tris)
            {
                vec v0 = orient.transform(va->vdata[idx[3*k]].pos),
                    v1 = orient.transform(va->vdata[idx[3*k+1]].pos),
                    v2 = orient.transform(va->vdata[idx[3*k+2]].pos);
                addshadowmeshtri(m, sides, draws, v0, v1, v2);
            }
        }
    }
}
```

`genshadowmesh`: after `if(shadowmms) genshadowmeshmapmodels(m, sides, draws);`:

```cpp
    if(shadowinsts.length()) genshadowmeshinstances(m, sides, draws);
```

(`shadowinsts` must be declared above `genshadowmesh`. It is, if Task 9 put its
declaration above `findshadowmms`.)

- [ ] **Step 4: Build and run**

Expected: all steps pass. Both screenshots show the same instance shadows.

- [ ] **Step 5: Commit**

```bash
git add src/engine/renderva.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "renderva: bake geometry instances into cached shadow meshes"
```

---

## Phase 3 — collision

### Task 11: BIH, collision, raycasts, stains, `edraycast`

**Files:**
- Modify: `src/engine/geomtemplate.cpp` (BIH build in `buildgeomtemplate`)
- Modify: `src/engine/bih.cpp`, `src/engine/bih.h` (`bihintersect`, `geominstanceintersect`)
- Modify: `src/engine/physics.cpp` (`disttoent`, `mmcollide`, `edraycast`)
- Modify: `src/engine/stain.cpp` (`gentris`)
- Modify: `tools/harness/geotemplate-selftest.ps1`

**Interfaces:**
- Consumes: `geomtemplate::bih` (Task 3 frees it).
- Produces: `extern bool geominstanceintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);` (bih.h); command `edraycast ox oy oz dx dy dz` → distance, or -1.

- [ ] **Step 1: Write the failing self-test steps**

Insert before the `# ==== later tasks` marker:

```powershell
    # ==== Task 11: collision and raycasts ==================================

    Step 'raycasts hit a rotated, scaled instance where its geometry is' {
        # A 16-unit block, template 7 pivoted at its centre
        Ed sel 2048 2400 2112 -Size 2,2,2
        Send 'edfillsel 1'
        $t7 = [int](Eval '(geot_newent geotemplate "7 12 12 12" 2056 2408 2120)')
        Expect 'template 7' (Info 7).Box '2048 2400 2112 2064 2416 2128'
        # yaw 45, scale 200: a 32-unit cube on its edge, centred at (2400, 2600, 2300).
        # Its -y corner is 16*sqrt(2) = 22.627 from the centre. The ray runs 2 units
        # off that vertical edge (not along it, where two faces meet): the faces there
        # are y = -22.627 + |x|, so it hits at y = 2600 - 20.627 = 2579.373.
        $script:I7 = [int](Eval '(geot_newent geoinstance "7 45 0 0 200 0 0 0 0" 2400 2600 2300)')
        $d = [double](Eval '(edraycast 2402 2500 2300 0 1 0)')
        ExpectTrue 'hit distance' ([math]::Abs($d - 79.373) -lt 0.05) "got $d, expected 79.373"
        Send "geot_setattr $($script:I7) 5 2"     # no-collide
        $d = [double](Eval '(edraycast 2402 2500 2300 0 1 0)')
        ExpectTrue 'a no-collide instance is not hit' ($d -lt 0) "got $d"
        Send "geot_setattr $($script:I7) 5 0"
        Send "geot_setattr $($script:I7) 0 8"     # no such template
        $d = [double](Eval '(edraycast 2402 2500 2300 0 1 0)')
        ExpectTrue 'an instance without a template is not hit' ($d -lt 0) "got $d"
        Send "geot_setattr $($script:I7) 0 7"
    }
```

- [ ] **Step 2: Build and run; confirm it fails**

Expected: `Unknown command: edraycast`, so the checks fail.

- [ ] **Step 3: BIH per template (`geomtemplate.cpp`)**

Before `buildgeomtemplate`:

```cpp
// Collision and raycasts against a template: its opaque triangles, straight out
// of the vertex arrays' CPU copies, in pivot-relative space (mesh xform)
static void buildgeomtemplatebih(geomtemplate &t)
{
    vector<BIH::mesh> meshes;
    matrix4x3 xform;
    xform.identity();
    xform.settranslation(vec(t.pivot).neg());
    loopv(t.vas)
    {
        vtxarray *va = t.vas[i];
        if(!va->tris) continue;
        BIH::mesh &m = meshes.add();
        m.xform = xform;
        m.tris = (const BIH::tri *)(va->edata + va->eoffset);
        m.numtris = va->tris;
        m.pos = (const uchar *)&va->vdata->pos;
        m.posstride = sizeof(vertex);
        m.tc = (const uchar *)&va->vdata->tc;
        m.tcstride = sizeof(vertex);
        m.flags = BIH::MESH_RENDER|BIH::MESH_COLLIDE;
        while(meshes.last().numtris > BIH::mesh::MAXTRIS)
        {
            BIH::mesh &overflow = meshes.dup();
            overflow.tris += BIH::mesh::MAXTRIS;
            overflow.numtris -= BIH::mesh::MAXTRIS;
            meshes[meshes.length()-2].numtris = BIH::mesh::MAXTRIS;
        }
    }
    t.bih = meshes.length() ? new BIH(meshes) : NULL;
}
```

In `buildgeomtemplate`, after the `t.verts`/`t.tris` loop: `buildgeomtemplatebih(t);`.

- [ ] **Step 4: Raycasts (`bih.cpp`, `bih.h`, `physics.cpp`)**

`bih.cpp`, replace `mmintersect` with:

```cpp
// A ray against a BIH placed at `eo` with mapmodel-style rotation and a
// uniform size (1 = as built)
static bool bihintersect(BIH *bih, const vec &eo, int yaw, int pitch, int roll, float size, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    float scale = 1/size;
    vec mo = vec(o).sub(eo).mul(scale), mray(ray);
    float v = mo.dot(mray), inside = bih->entradius - mo.squaredlen();
    if((inside < 0 && v > 0) || inside + v*v < 0) return false;
    if(yaw != 0)
    {
        const vec2 &rot = sincosmod360(-yaw);
        mo.rotate_around_z(rot);
        mray.rotate_around_z(rot);
    }
    if(pitch != 0)
    {
        const vec2 &rot = sincosmod360(-pitch);
        mo.rotate_around_x(rot);
        mray.rotate_around_x(rot);
    }
    if(roll != 0)
    {
        const vec2 &rot = sincosmod360(roll);
        mo.rotate_around_y(rot);
        mray.rotate_around_y(rot);
    }
    if(bih->traverse(mo, mray, maxdist ? maxdist*scale : 1e16f, dist, mode))
    {
        dist /= scale;
        if(roll != 0) hitsurface.rotate_around_y(sincosmod360(-roll));
        if(pitch != 0) hitsurface.rotate_around_x(sincosmod360(pitch));
        if(yaw != 0) hitsurface.rotate_around_z(sincosmod360(yaw));
        return true;
    }
    return false;
}

bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    model *m = loadmapmodel(e.attrs[0]);
    if(!m) return false;
    if((mode&RAY_ENTS)!=RAY_ENTS && (!m->collide || e.flags&EF_NOCOLLIDE)) return false;
    if(!m->bih && !m->setBIH()) return false;
    return bihintersect(m->bih, e.o, e.attrs[1], e.attrs[2], e.attrs[3], e.attrs[5] ? e.attrs[5]/100.0f : 1.0f, o, ray, maxdist, mode, dist);
}

bool geominstanceintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    if((mode&RAY_ENTS)!=RAY_ENTS && (e.attrs[5]&GEOINST_NOCOLLIDE || e.flags&EF_NOCOLLIDE)) return false;
    geomtemplate *t = geominstancetemplate(e);
    if(!t || !t->bih) return false;
    return bihintersect(t->bih, e.o, e.attrs[1], e.attrs[2], e.attrs[3], geominstancescale(e), o, ray, maxdist, mode, dist);
}
```

(`mmintersect` used `scale = 100/attrs[5]`, which is `1/size`. The result is the same.)

`bih.h`, after `mmintersect`'s declaration:

```cpp
extern bool geominstanceintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);
```

`physics.cpp`, `disttoent`, after the `entintersect(RAY_POLY, mapmodels, ...)` block:

```cpp
    entintersect(RAY_POLY, instances, {
        if((mode&RAY_ENTS)!=RAY_ENTS)
        {
            if(e.flags&EF_NOVIS || !entities::isallowed(e)) continue;
        }
        else if(!entities::cansee(n)) continue;
        if(!geominstanceintersect(e, o, ray, radius, mode, f)) continue;
    });
```

and inside `if((mode&RAY_ENTS) == RAY_ENTS)`, add `entselintersect(instances);`.

At the end of `physics.cpp`:

```cpp
#ifdef DEBUG_UTILS
// Distance along a ray to the first hit through raycube -- world geometry,
// mapmodels and geometry instances -- or -1
ICOMMAND(0, edraycast, "ffffff", (float *ox, float *oy, float *oz, float *dx, float *dy, float *dz),
{
    if(identflags&IDF_MAP) { floatret(-1); return; }
    vec o(*ox, *oy, *oz), ray(*dx, *dy, *dz);
    if(ray.iszero()) { floatret(-1); return; }
    ray.normalize();
    floatret(raycube(o, ray, 0, RAY_CLIPMAT|RAY_POLY));
});
#endif
```

- [ ] **Step 5: Movement collision (`physics.cpp`) and stains (`stain.cpp`)**

`mmcollide`, before its final `return false;`:

```cpp
    loopv(oc.instances)
    {
        extentity &e = *ents[oc.instances[i]];
        if(e.attrs[5]&GEOINST_NOCOLLIDE || e.flags&(EF_NOCOLLIDE|EF_NOVIS) || !entities::isallowed(e)) continue;
        geomtemplate *t = geominstancetemplate(e);
        if(!t || !t->bih) continue;
        float scale = geominstancescale(e);
        if(d->o.reject(e.o, d->radius + max(d->height, d->aboveeye) + sqrtf(t->bih->entradius)*scale)) continue;
        int yaw = e.attrs[1], pitch = e.attrs[2], roll = e.attrs[3];
        switch(d->collidetype)
        {
            case COLLIDE_ELLIPSE:
                if(t->bih->ellipsecollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale)) return true;
                break;
            case COLLIDE_OBB:
                if(t->bih->boxcollide(d, dir, cutoff, e.o, yaw, pitch, roll, scale)) return true;
                break;
            default: break;
        }
    }
```

`stain.cpp`, in the stain renderer struct, after `genmmtris`:

```cpp
    void geninsttris(octaentities &oe)
    {
        const vector<extentity *> &ents = entities::getents();
        loopv(oe.instances)
        {
            extentity &e = *ents[oe.instances[i]];
            geomtemplate *t = geominstancetemplate(e);
            if(!t || !t->bih) continue;
            float scale = geominstancescale(e);
            if(staincenter.reject(e.o, stainradius + sqrtf(t->bih->entradius)*scale)) continue;
            t->bih->genstaintris(this, staincenter, stainradius, e.o, e.attrs[1], e.attrs[2], e.attrs[3], scale);
        }
    }
```

and in `gentris`, after the `genmmtris` line:

```cpp
                    if(cu.ext->ents && cu.ext->ents->instances.length()) geninsttris(*cu.ext->ents);
```

- [ ] **Step 6: Build and run the self-test**

Expected: all steps pass. Then a manual check with `-KeepRunning`: leave edit mode
(`edittoggle`), walk into an instance (it must block the player), and shoot it (a stain
must appear on its surface). Report the result. There is no harness command for player
movement or stains yet.

- [ ] **Step 7: Commit**

```bash
git add src/engine/geomtemplate.cpp src/engine/bih.cpp src/engine/bih.h src/engine/physics.cpp src/engine/stain.cpp tools/harness/geotemplate-selftest.ps1
git commit -m "geomtemplate: collide, raycast and stain geometry instances"
```

---

### Task 12: Documentation

**Files:**
- Modify: `tools/harness/README.md` (new "Geometry template self-test" section after "Prefab self-test")
- Modify: `CLAUDE.md` (DEBUG_UTILS tables; gitignored, not committed)
- Modify: `doc/superpowers/specs/2026-10-02-geometry-templates-design.md` (status line)

- [ ] **Step 1: README section**

Add after the "Prefab self-test" section:

````markdown
### Geometry template self-test

```powershell
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1 [-KeepRunning]
```

Starts its own game and checks geometry templates end to end: the entity types and the
version 57 map conversion (against `tests/geotemplate-atop-types.txt`, recorded from
`atop` before the types were added), capture of whole cubes inside a template box,
off-grid boxes, re-id and duplicate ids, save/reload, rebuilds on geometry edits inside
a box (and none outside), overlapping boxes, undo, instance bounds, instanced G-buffer
drawing, editor boxes, sun and point-light shadows, GI bounce (`rhprobe`), cached shadow
meshes against the live path, and raycasts against a rotated, scaled instance.

- Engine commands it relies on (all `DEBUG_UTILS`): `geotemplateinfo`,
  `geoinstancebb`, `edfillsel`, `geoinststats`, `edraycast`.
- CubeScript helpers: `tests/geotemplate.cfg` (`geot_*`, edit mode only).
- Writes screenshots `geot-*.png`. The comment at each `Shot` says what it must show.
- Not covered: player movement collision and bullet stains on instances (manual, see the
  plan's Task 11), and multiplayer edit propagation.
````

- [ ] **Step 2: CLAUDE.md**

In the "Map editor" DEBUG_UTILS table, add:

```markdown
| `edfillsel <solid>` | Fills (1) or empties (0) the current selection — deterministic test geometry. Local only. `octaedit.cpp`. |
| `geotemplateinfo <id>` | `capmin capmax verts tris instances rebuilds` of a geometry template, or empty. `geomtemplate.cpp`. |
| `geoinstancebb <idx>` | World bounds of a geoinstance entity, or empty without a template. `geomtemplate.cpp`. |
| `geoinststats` | `instances triangles` drawn by the last G-buffer pass. `renderva.cpp`. |
| `edraycast <ox> <oy> <oz> <dx> <dy> <dz>` | Hit distance through `raycube` (world, mapmodels, geometry instances), or -1. `physics.cpp`. |
```

- [ ] **Step 3: Spec status**

Change the spec's `Status:` line to `Status: implemented on branch geometry-templates (2026-10-02 plan)`.

- [ ] **Step 4: Run everything once more**

```powershell
wsl -d Ubuntu -- "/mnt/f/Red Eclipse/src/build.sh" debug
powershell -ExecutionPolicy Bypass -File tools\harness\geotemplate-selftest.ps1
powershell -ExecutionPolicy Bypass -File tools\harness\editor-selftest.ps1
powershell -ExecutionPolicy Bypass -File tools\harness\prefab-selftest.ps1
```

Expected: all three pass. The editor and prefab self-tests guard the octree, entity and
commit paths that this feature changed.

- [ ] **Step 5: Commit**

```bash
git add tools/harness/README.md doc/superpowers/specs/2026-10-02-geometry-templates-design.md
git commit -m "doc: describe the geometry template self-test"
```
