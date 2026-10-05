<#
.SYNOPSIS
    In-game self-test for drilling projectiles against BIH geometry.

.DESCRIPTION
    Boots a client, builds a scratch map with geometry instances in front of a
    world wall, leaves edit mode and fires the SMG (drill 2) through the real
    MOUSE1 bind. Each shot is classified by the stain buffer its first stain
    lands in (dbgstain): "mapmodel" is a stain on an instance, "opaque" one on
    plain world geometry, and "transparent" one on the wall behind the targets,
    which has alpha material so that it can be told from them.

    A BIH mesh is a hollow shell: sphere collision only finds its triangles
    near the surface, so the drill test once let a bullet that was merely
    inside a thick instance carry on through it, without a stain, whenever the
    hit was within about 60 degrees of the normal. Thin instances and thin
    world geometry must still be drilled through.

.EXAMPLE
    tools\harness\drill-selftest.ps1
    tools\harness\drill-selftest.ps1 -KeepRunning
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

$W_SMG = 4
$Shots = 3

# ------------------------------------------------------------- reporting ----

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ("$Actual" -ceq "$Expected") { Write-Host "     ok    $What = '$Actual'" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

# --------------------------------------------------------------- driving ----

function Ed { & $editor @args 6>$null | Out-Null }

function Send([string]$Script, [int]$SettleMs = 250) {
    $out = @(& $harness send $Script -Settle $SettleMs 6>$null)
    foreach ($line in $out) {
        if ("$line" -match 'Unknown (command|alias)|assert|exception') {
            Write-Host "     FAIL  engine reported an error: $line" -ForegroundColor Red
            $script:failures++
        }
    }
    return $out
}

function Eval([string]$Expr) {
    $out = @(& $harness send "echo (concatword ""SELFTEST_EVAL="" $Expr)" 6>$null)
    foreach ($line in $out) {
        if ($line -match 'SELFTEST_EVAL=(.*)$') { return $Matches[1].Trim() }
    }
    return $null
}

function SetEditing([int]$On) { Send "if (!= `$editing $On) [edittoggle]" 900 | Out-Null }

# Stands the player at $Pos looking at $Look (edit mode places and aims the
# view, which is the player), then leaves edit mode with the weapon out.
function Stand([string]$Pos, [string]$Look, [int]$Weapon) {
    SetEditing 1
    Send "edgoto $Pos; edlookat $Look" 300 | Out-Null
    SetEditing 0
    Expect 'editing' (Eval '$editing') '0'
    Send "weapon $Weapon 1" 1500 | Out-Null
}

# One shot through the real bind; returns the stain buffer of its first stain,
# or 'none'. The settle is long enough for a ricochet to land in this batch.
function Fire {
    $out = Send 'gamekeypress MOUSE1 1; sleep 30 [gamekeypress MOUSE1 0]' 1500
    foreach ($line in $out) {
        if ("$line" -match '^tris = \d+, verts = \d+, total tris = \d+, (\w+)') { return $Matches[1] }
    }
    return 'none'
}

function ExpectShots([string]$Expected) {
    1..$Shots | ForEach-Object { Expect "shot $_ first stain" (Fire) $Expected }
}

# --------------------------------------------------------------------------

& $harness stop 6>$null | Out-Null
& $harness start 6>$null | Out-Null

try {
    Send 'exec "tools/harness/tests/geotemplate.cfg" 0 0' | Out-Null

    Step 'scene: a thick and a thin geometry instance in front of a world wall' {
        Send 'entediting 0' 300 | Out-Null
        Ed newmap 12
        Send 'entediting 1' 300 | Out-Null
        # Template 1: a solid 32-unit block, x 1504..1536, y 1504..1536, z 2048..2080
        Ed sel 1504 1504 2048 -Size 4,4,4
        Send 'edfillsel 1' | Out-Null
        # The wall behind the targets: x 1984..2368, y 2200..2208, z 2048..2112
        Ed sel 1984 2200 2048 -Size 48,1,8
        Send 'edfillsel 1' | Out-Null
        # editmat, unlike edfillsel, refuses a selection that is not in view
        Ed frame 2176 2200 2080 -Dist 300 -Yaw 0 -Pitch 0
        $out = Send 'editmat alpha' 300
        Expect 'editmat took the wall' (@($out | Where-Object { "$_" -match 'not in view|Unknown material' }).Count) '0'
        # Template 2: a slab one unit thick, x 1600..1632, y 1504..1505, z 2048..2080
        Send 'gridpower 0' | Out-Null
        Ed sel 1600 1504 2048 -Size 32,1,32
        Send 'edfillsel 1' | Out-Null
        # A world slab one unit thick: x 2342..2358, y 2100..2101, z 2048..2080
        Ed sel 2342 2100 2048 -Size 16,1,32
        Send 'edfillsel 1' | Out-Null
        # Template 3: a slab two units thick, x 1700..1732, y 1504..1506, z 2048..2080
        Ed sel 1700 1504 2048 -Size 32,2,32
        Send 'edfillsel 1' | Out-Null
        # A world slab two units thick: x 2310..2326, y 2100..2102, z 2048..2080
        Ed sel 2310 2100 2048 -Size 16,2,32
        Send 'edfillsel 1' | Out-Null
        Send 'gridpower 3; cancelsel' | Out-Null
        Send 'geot_newent geotemplate "1 20 20 18" 1520 1520 2066' | Out-Null
        Send 'geot_newent geotemplate "2 18 2 18" 1616 1504.5 2064' | Out-Null
        Send 'geot_newent geotemplate "3 18 3 18" 1716 1505 2064' | Out-Null
        $box1 = ((Eval '(geotemplateinfo 1)') -split ' ')[0..5] -join ' '
        $box2 = ((Eval '(geotemplateinfo 2)') -split ' ')[0..5] -join ' '
        Expect 'template 1 box' $box1 '1504 1504 2048 1536 1536 2080'
        Expect 'template 2 box' $box2 '1600 1504 2048 1632 1505 2080'
        # Thick: x 2032..2064, y 2084..2116, z 2062..2094
        $thick = Eval '(geot_newent geoinstance "1 0 0 0 0 0 0 0 0" 2048 2100 2080)'
        # Thin, at half size: x 2192..2208, y 2099.75..2100.25, z 2064..2080
        $thin = Eval '(geot_newent geoinstance "2 0 0 0 50 0 0 0 0" 2200 2100 2072)'
        # Thin, at full size (as thick as the world slab): x 2104..2136, y 2099.5..2100.5, z 2056..2088
        $slab = Eval '(geot_newent geoinstance "2 0 0 0 0 0 0 0 0" 2120 2100 2072)'
        Expect 'one-unit instance bounds (rounded out)' (Eval "(geoinstancebb $slab)") '2104 2099 2056 2136 2101 2088'
        Expect 'ray to the one-unit instance' (Eval '(edraycast 2120 2000 2072 0 1 0)') '99.5'
        # Two units thick: x 2264..2296, y 2099..2101, z 2056..2088
        Send 'geot_newent geoinstance "3 0 0 0 0 0 0 0 0" 2280 2100 2072' | Out-Null
        $box3 = ((Eval '(geotemplateinfo 3)') -split ' ')[0..5] -join ' '
        Expect 'template 3 box' $box3 '1700 1504 2048 1732 1506 2080'
        Expect 'ray to the two-unit instance' (Eval '(edraycast 2280 2000 2072 0 1 0)') '99'
        Expect 'ray to the two-unit world slab' (Eval '(edraycast 2318 2000 2066 0 1 0)') '100.1'
        Expect 'thick instance bounds' (Eval "(geoinstancebb $thick)") '2032 2084 2062 2064 2116 2094'
        Expect 'ray to the thick front face' (Eval '(edraycast 2048 2000 2072 0 1 0)') '84.0'
        Expect 'ray to the thin slab' (Eval '(edraycast 2200 2000 2072 0 1 0)') '99.75'
        Expect 'ray to the world slab' (Eval '(edraycast 2350 2000 2066 0 1 0)') '100.1'
        Send 'dbgstain 1' | Out-Null
    }

    Step 'thick instance, head-on: the bullet stops on the front face' {
        Stand '2048 2010 2066' '2048 2084 2072' $W_SMG
        ExpectShots 'mapmodel'
    }

    Step 'thick instance, side face at about 32 degrees: the bullet stops on it' {
        # Reaches x = 2032 at y ~ 2076, 32 degrees off the -x face's normal
        Stand '1990 2050 2066' '2045 2084 2068' $W_SMG
        ExpectShots 'mapmodel'
    }

    Step 'thick instance, front face at about 70 degrees: the bullet stops on it' {
        Stand '1993.6 2063.5 2066' '2050 2084 2066' $W_SMG
        ExpectShots 'mapmodel'
    }

    Step 'thin instance (0.5 units), head-on: drilled through onto the wall' {
        Stand '2200 2030 2066' '2200 2100 2072' $W_SMG
        ExpectShots 'transparent'
    }

    Step 'thin instance (1 unit), head-on: drilled through onto the wall, as the world slab is' {
        Stand '2120 2030 2066' '2120 2100 2072' $W_SMG
        ExpectShots 'transparent'
    }

    Step 'thin world slab (1 unit), head-on: drilled through onto the wall' {
        Stand '2350 2030 2066' '2350 2100 2068' $W_SMG
        ExpectShots 'transparent'
    }

    Step 'world slab (2 units), head-on: too thick to drill' {
        Stand '2318 2030 2066' '2318 2100 2068' $W_SMG
        ExpectShots 'opaque'
    }

    Step 'instance slab (2 units), head-on: too thick to drill, as the world slab is' {
        Stand '2280 2030 2066' '2280 2100 2072' $W_SMG
        ExpectShots 'mapmodel'
    }
}
finally {
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'drill self-test: all checks passed' -ForegroundColor Green
exit 0
