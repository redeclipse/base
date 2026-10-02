<#
.SYNOPSIS
    End-to-end self-test for geometry templates (geotemplate / geoinstance).

.DESCRIPTION
    Boots a client in the harness home and drives template capture, live
    updates, instances, rendering, shadows and collision through DEBUG_UTILS
    commands (geotemplateinfo, geoinstancebb, edfillsel, geoinststats,
    edraycast). Writes screenshots geot-*.png for review; the comment at each
    Shot says what it must show.
    Spec: docs/superpowers/specs/2026-10-02-geometry-templates-design.md

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
        # entities::numattrs pads every type to at least 5 attributes, so the
        # 4-attribute geotemplate reads back with one trailing 0.
        Expect 'geotemplate after reload' (Eval "(geot_get $t)") 'geotemplate 1 16 16 16 0'
        Expect 'geoinstance after reload' (Eval "(geot_get $i)") 'geoinstance 1 90 0 0 50 0 0 0 0'
    }

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
        # undo refuses (noedit(), octaedit.cpp:1117) unless the selection is in view,
        # unlike edfillsel, which needs no view: put the camera on it first
        Ed frame 2060 2060 ($BZ + 4) -Dist 60 -Yaw 0 -Pitch 0
        Expect 'undo succeeded' (Eval '(undo)') '1'
        Expect 'undo rebuilds template 1 again' (Info 1).Rebuilds ($a.Rebuilds + 2)
        Expect 'undo restores the triangles' (Info 1).Tris $a.Tris
        Send "geot_delent $t2"
        Expect 'template 2 gone' (Eval '(geotemplateinfo 2)') ''
    }

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

    Step 'a registered instance is picked by the editor ray and its box follows scale' {
        Ed cursor off
        Ed lookatent $script:I1
        Start-Sleep -Milliseconds 400
        $hover = @((EdState).Hover | ForEach-Object { $_.Idx })
        Expect 'hovered entity' ($hover -join ' ') "$($script:I1)"
        # A selected instance draws its full bounds, not the picking box: check the
        # shared selection-box function through the bounds it is built from
        Send "geot_setattr $($script:I1) 4 300"
        Expect 'bounds at scale 300' (Eval "(geoinstancebb $($script:I1))") '2252 2000 2152 2348 2096 2248'
        Send "geot_setattr $($script:I1) 4 0"
        Shot 'geot-instance-box'   # the instance's selection box, hovered, at scale 100
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
