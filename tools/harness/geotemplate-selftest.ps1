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
