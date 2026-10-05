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
    $out = @(& $harness send $Script -Settle $SettleMs 6>$null)
    # A script error never fails a command by itself: it only reaches the log
    foreach ($line in $out) {
        if ("$line" -match 'Unknown (command|alias)|assert|exception') {
            Write-Host "     FAIL  engine reported an error: $line" -ForegroundColor Red
            $script:failures++
        }
    }
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
# (world.cpp:1426, see AGENTS.md). Entity editing off empties it.
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

# Pixels of a screenshot, within a rectangle given as fractions of the image,
# whose green channel is below $GreenBelow (the shadowed floor is far darker
# than the lit one).
function CountDark([string]$Name, [double]$X0, [double]$X1, [double]$Y0, [double]$Y1, [int]$GreenBelow) {
    Add-Type -AssemblyName System.Drawing
    $bitmap = [System.Drawing.Bitmap]::new((Join-Path $ShotDir "$Name.png"))
    try {
        $count = 0
        for ($x = [int]($bitmap.Width * $X0); $x -lt [int]($bitmap.Width * $X1); $x++) {
            for ($y = [int]($bitmap.Height * $Y0); $y -lt [int]($bitmap.Height * $Y1); $y++) {
                if ($bitmap.GetPixel($x, $y).G -lt $GreenBelow) { $count++ }
            }
        }
    }
    finally { $bitmap.Dispose() }
    return $count
}

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

    Step 'an instance whose pivot lies outside its captured geometry is still picked at its position' {
        # Box z 2112..2240 (centre 2176) holds the block (z 2112..2144): the pivot is 32 units
        # above the captured cubes, so the instance position (2400,2048,2200) lies 32 above
        # the transformed capture. The position is near the block, where the octree is
        # subdivided: the capture lies in the empty leaf below z 2176, the position above
        $t3 = [int](Eval '(geot_newent geotemplate "3 16 16 64" 2064 2064 2176)')
        Expect 'captured box is the block' (Info 3).Box '2048 2048 2112 2080 2080 2144'
        $i3 = [int](Eval '(geot_newent geoinstance "3 0 0 0 0 0 0 0 0" 2112 2064 2200)')
        Expect 'bounds exclude the position' (Eval "(geoinstancebb $i3)") '2096 2048 2136 2128 2080 2168'
        Ed cursor off
        Ed lookatent $i3
        Start-Sleep -Milliseconds 400
        $hover = @((EdState).Hover | ForEach-Object { $_.Idx })
        Expect 'hovered entity' ($hover -join ' ') "$i3"
        Send "geot_delent $i3"
        Send "geot_delent $t3"
    }

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

    Step 'a small, distant instance is drawn on every frame' {
        # The node's box query must not test against the instance itself: drawn
        # after it, the box's faces z-fight with the instance's and a small box
        # fails the pixel threshold. One unrotated, unscaled instance, alone.
        Send "geot_setattr $($script:I1) 0 42"
        Send "geot_setattr $($script:I2) 0 42"
        $script:I3 = [int](Eval '(geot_newent geoinstance "1 0 0 0 0 0 0 0 0" 2048 3900 2150)')
        foreach ($dist in 1500, 2200, 3000) {
            Ed frame 2048 3900 2150 -Dist $dist -Yaw 0 -Pitch 0
            Send 'sleep 1 []' 500
            $counts = @()
            for ($k = 0; $k -lt 12; $k++) {
                Send 'sleep 1 []' 80
                $counts += @((Eval '(geoinststats)') -split ' ')[0]
            }
            Expect "instances drawn at distance $dist, 12 frames" ($counts -join ' ') ((1..12 | ForEach-Object { '1' }) -join ' ')
        }
    }

    Step 'an instance behind world geometry is occlusion-culled, and drawn again once it is gone' {
        Send 'oqinst 1'
        Ed frame 2048 3900 2150 -Dist 300 -Yaw 0 -Pitch 0
        Send 'sleep 1 []' 600
        Expect 'visible without a wall' (@((Eval '(geoinststats)') -split ' ')[0]) '1'
        # A wall 40x40 cells, one cell thick, 150 units in front of the instance
        Ed sel 1888 3752 2056 -Size 40,1,40
        Send 'edfillsel 1'
        Ed frame 2048 3900 2150 -Dist 300 -Yaw 0 -Pitch 0
        Send 'sleep 1 []' 600
        Send 'sleep 1 []' 600
        Send 'sleep 1 []' 600
        Expect 'culled behind the wall' (@((Eval '(geoinststats)') -split ' ')[0]) '0'
        Shot 'geot-instance-occluded'   # a wall filling the view; no block behind it showing through
        Ed sel 1888 3752 2056 -Size 40,1,40
        Send 'edfillsel 0'
        Send 'sleep 1 []' 600
        Send 'sleep 1 []' 600
        Expect 'drawn again with the wall gone' (@((Eval '(geoinststats)') -split ' ')[0]) '1'
        Send "geot_delent $($script:I3)"
        Send "geot_setattr $($script:I1) 0 1"
        Send "geot_setattr $($script:I2) 0 1"
    }

    # ==== Task 8: editor boxes =============================================

    Step 'edit mode draws each template box' {
        Ed frame 2064 2064 2128 -Dist 140 -Yaw 30 -Pitch -25
        Send 'sleep 1 []' 400
        # Must show the source block with a faint grey box (requested, 2044..2084)
        # around a bright cyan box tight on the block (captured, 2048..2080).
        Shot 'geot-boxes'

        # The captured box is drawn at (0,160,160) with additive blending, so on
        # its edges over the dark teal ground the sum is R<40, G>150, B>150.
        # Nothing else in this view matches that: the ground is about (0,112,84),
        # the sky (74,133,136), and the faint grey requested box over ground is
        # about (48,160,132) (R too high). Measured on the 1280x720 shot: 684
        # matching pixels in the block rectangle (684-687) and 0 in the rest of the image;
        # with the box drawn before the line polygon offset it is 2 (see the
        # task 8 report). The rectangle is the block's neighbourhood for the
        # fixed camera above, as fractions of the image: x 0.40..0.62 of the
        # width, y 0.35..0.70 of the height.
        $imagePath = Join-Path $ShotDir 'geot-boxes.png'
        Add-Type -AssemblyName System.Drawing
        $bitmap = [System.Drawing.Bitmap]::new($imagePath)
        try {
            $xStart = [int]($bitmap.Width * 0.40)
            $xEnd   = [int]($bitmap.Width * 0.62)
            $yStart = [int]($bitmap.Height * 0.35)
            $yEnd   = [int]($bitmap.Height * 0.70)
            $cyanCount = 0
            for ($x = $xStart; $x -lt $xEnd; $x++) {
                for ($y = $yStart; $y -lt $yEnd; $y++) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    if ($pixel.R -lt 40 -and $pixel.G -gt 150 -and $pixel.B -gt 150) { $cyanCount++ }
                }
            }
        }
        finally { $bitmap.Dispose() }
        ExpectTrue 'captured box draws cyan edges' ($cyanCount -ge 300) "found $cyanCount cyan edge pixels around the block (need >= 300, expect ~680)"
    }

    # ==== Task 9: shadows and GI ===========================================

    Step 'instances cast sun and point-light shadows' {
        Send 'sunlightpitch 50; sunlightyaw 30' 400
        Ed frame 2330 2048 2120 -Dist 260 -Yaw 200 -Pitch -35
        Send 'sleep 1 []' 500
        # Must show both instances' shadows on the floor below them, matching
        # their shapes (one square-ish, one turned 45 degrees and larger).
        Shot 'geot-sun-shadow'
        $sun = CountDark 'geot-sun-shadow' 0.44 0.62 0.40 0.56 30
        # Control: the same view with both instances flagged no-shadow
        Send "geot_setattr $($script:I1) 5 1; geot_setattr $($script:I2) 5 1" 500
        Send 'sleep 1 []' 500
        Shot 'geot-sun-noshadow'
        $sunControl = CountDark 'geot-sun-noshadow' 0.44 0.62 0.40 0.56 30
        Write-Host "     counts: sun $sun / control $sunControl"
        ExpectTrue 'sun shadows of both instances on the floor' (($sun -ge 3000) -and ($sunControl -le 300)) "dark floor pixels: $sun with shadows (need >= 3000), $sunControl with no-shadow (need <= 300)"

        $script:L1 = [int](Eval '(geot_newent light "400 255 255 255" 2330 2120 2240)')
        Send 'sunlight 0' 500
        Send 'sleep 1 []' 500
        Shot 'geot-point-noshadow'
        $pointControl = CountDark 'geot-point-noshadow' 0.30 0.66 0.42 0.58 20
        Send "geot_setattr $($script:I1) 5 0; geot_setattr $($script:I2) 5 0" 500
        Send 'sleep 1 []' 500
        # Must show point-light shadows of both instances on the floor, cast away from the light.
        Shot 'geot-point-shadow'
        $point = CountDark 'geot-point-shadow' 0.30 0.66 0.42 0.58 20
        Write-Host "     counts: point $point / control $pointControl"
        ExpectTrue 'point-light shadows of both instances on the floor' (($point -ge 8000) -and ($pointControl -le 1500)) "dark floor pixels: $point with shadows (need >= 8000), $pointControl with no-shadow (need <= 1500)"
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
        Invoke-MapLoad { Ed open harness_geot }
        # A loaded map's entities come back in a different order: find them again
        $script:L2 = [int](Eval '(geot_find light)')
        $script:I1 = [int](Eval '(geot_find geoinstance 2 90)')
        $script:I2 = [int](Eval '(geot_find geoinstance 2 45)')
        Write-Host "     after reload: light $($script:L2), instances $($script:I1) $($script:I2)"
        ExpectTrue 'entities found again after the reload' (($script:L2 -ge 0) -and ($script:I1 -ge 0) -and ($script:I2 -ge 0))
        Send 'smmesh 1' 300
        Send 'sunlight 1; sunlight 0' 300          # sunlight's VARF clears the shadow-map cache (and the meshes)
        # allchanged() discards the meshes it builds at load, so build them now
        $meshes = [int](Eval '(edgenshadowmeshes)')
        ExpectTrue 'shadow meshes were generated' ($meshes -gt 0) "$meshes meshes"
        Ed frame 2330 2048 2120 -Dist 260 -Yaw 200 -Pitch -35
        Send 'sleep 1 []' 600
        ExpectTrue 'the meshes are still there when shot' ([int](Eval '(edshadowmeshcount)') -gt 0)
        Shot 'geot-mesh-on'
        Send 'smmesh 0' 300                        # clears the meshes: the live path
        Send 'sunlight 1; sunlight 0' 300          # clears the shadow-map cache too
        Send 'sleep 1 []' 600
        Expect 'no meshes on the live path' (Eval '(edshadowmeshcount)') '0'
        Shot 'geot-mesh-off'
        $frac = Compare-Shots 'geot-mesh-on' 'geot-mesh-off'
        Write-Host ("     mesh vs live: {0:P2} of pixels differ" -f $frac)
        ExpectTrue 'cached and live shadows agree' ($frac -lt 0.01) ("{0:P2} of pixels differ" -f $frac)
        Send 'smmesh 1; sunlight 0xA0A090' 300
        Send "geot_delent $($script:L2)" 300
    }

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
        $d = [double](Eval '(edraycast 2402 2700 2300 0 -1 0)')
        ExpectTrue 'hit distance from the other side' ([math]::Abs($d - 79.373) -lt 0.05) "got $d, expected 79.373"
        Send "geot_setattr $($script:I7) 5 2"     # no-collide
        $d = [double](Eval '(edraycast 2402 2500 2300 0 1 0)')
        ExpectTrue 'a no-collide instance is not hit' ($d -lt 0) "got $d"
        Send "geot_setattr $($script:I7) 5 0"
        Send "geot_setattr $($script:I7) 0 8"     # no such template
        $d = [double](Eval '(edraycast 2402 2500 2300 0 1 0)')
        ExpectTrue 'an instance without a template is not hit' ($d -lt 0) "got $d"
        Send "geot_setattr $($script:I7) 0 7"
    }

    Step 'a ray reports the nearest face when a template has several vertex arrays' {
        # A 16-unit block straddling x = 2048, a top-level octree boundary: the
        # template gets one vertex array (one BIH mesh) per side, 5 faces (20 vertices) each; one array would have 24
        Ed sel 2040 2760 2200 -Size 2,2,2
        Send 'edfillsel 1'
        $null = [int](Eval '(geot_newent geotemplate "10 12 12 12" 2048 2768 2208)')
        Expect 'template 10' (Info 10).Box '2040 2760 2200 2056 2776 2216'
        Expect 'two vertex arrays: 2 x 20 vertices' (Info 10).Verts 40
        # Pivot at the block's centre, so the block is the instance's position +-8
        $null = [int](Eval '(geot_newent geoinstance "10 0 0 0 0 0 0 0 0" 2400 3000 2300)')
        $d = [double](Eval '(edraycast 2350 3003 2302 1 0 0)')
        ExpectTrue 'near face from -x' ([math]::Abs($d - 42) -lt 0.05) "got $d, expected 42"
        $d = [double](Eval '(edraycast 2450 3003 2302 -1 0 0)')
        ExpectTrue 'near face from +x' ([math]::Abs($d - 42) -lt 0.05) "got $d, expected 42"
    }

    Step 'a ray grazing the far corner of a lopsided template is not rejected early' {
        # A 16 x 16 x 32 block lying to one side of its pivot: x 0..16, y -16..0,
        # z -16..16 from it. Its farthest corner is 27.7 from the pivot, but the
        # two diagonal corners are only 22.6 away.
        Ed sel 2056 2624 2200 -Size 2,2,4
        Send 'edfillsel 1'
        $null = [int](Eval '(geot_newent geotemplate "9 16 16 16" 2056 2640 2216)')
        Expect 'template 9' (Info 9).Box '2056 2624 2200 2072 2640 2232'
        # Roll 180 turns it to x -16..0 around the instance. The ray runs along
        # (1, -1, 0), 26.9 from the pivot at its closest, and enters the x = -16
        # face 1 unit inside the corner: from (-25.5, -5.5, 15.5) that is 9.5*sqrt(2).
        $null = [int](Eval '(geot_newent geoinstance "9 0 0 180 0 0 0 0 0" 2700 3300 2400)')
        $d = [double](Eval '(edraycast 2674.5 3294.5 2415.5 1 -1 0)')
        ExpectTrue 'hit distance' ([math]::Abs($d - 13.435) -lt 0.05) "got $d, expected 13.435"
    }

    # ==== Pitch and roll, end to end =======================================

    # edraycast from $O along $D: a hit at $Expected (a distance), or a miss when $null
    function Ray([string]$What, [double[]]$Origin, [double[]]$Dir, $Expected) {
        $d = [double](Eval "(edraycast $($Origin[0]) $($Origin[1]) $($Origin[2]) $($Dir[0]) $($Dir[1]) $($Dir[2]))")
        if ($null -eq $Expected) { ExpectTrue "$What misses" ($d -lt 0) "got $d" }
        else { ExpectTrue "$What hits at $Expected" ([math]::Abs($d - $Expected) -lt 0.05) "got $d, expected $Expected" }
    }

    # edcollide: a probe centred on x y z, type 1 = ellipsoid, 2 = oriented box
    function Collides([double]$X, [double]$Y, [double]$Z, [int]$Type) {
        return (Eval "(edcollide $X $Y $Z $Type 2 2)")
    }

    Step 'a pitched and rolled instance is raycast and collided where the transform puts it' {
        # Template 9 is a block lopsided about its pivot (see above): relative to the
        # pivot, x 0..16, y -16..0, z -16..16. The instance has pitch 90 and roll 90.
        # The transform is Rz(yaw) Rx(pitch) Ry(-roll): Ry(-90) first, (x, y, z) ->
        # (-z, y, x), then Rx(90), (x, y, z) -> (x, -z, y), so a local (x, y, z) lands
        # at the offset (-z, -x, y) from the instance. The block therefore occupies
        # X -16..16, Y -16..0, Z -16..0 around the instance at P. (Pitch alone would give
        # X 0..16, Y -16..16, Z -16..0; roll alone X -16..16, Y -16..0, Z 0..16; Rx
        # applied first, X 0..16, Y -16..16, Z 0..16.) With P = (2700, 3500, 2400):
        $px = 2700; $py = 3500; $pz = 2400
        $ip = [int](Eval "(geot_newent geoinstance ""9 0 90 90 0 0 0 0 0"" $px $py $pz)")
        Expect 'bounds' (Eval "(geoinstancebb $ip)") '2684 3484 2384 2716 3500 2400'
        # down onto the top face (Z = 0) from 50 above, at X +3, Y -5
        Ray 'down onto the top' @(($px + 3), ($py - 5), ($pz + 50)) @(0, 0, -1) 50
        # towards -y onto the Y = 0 face from 50 away, at X +3, Z -5: 50. Roll alone
        # (Z 0..16) and Rx-first (Z 0..16) would miss; pitch alone would hit at 34
        Ray 'onto the +y face' @(($px + 3), ($py + 50), ($pz - 5)) @(0, -1, 0) 50
        # towards -x onto the X = +16 face from 50 away: 34
        Ray 'onto the +x face' @(($px + 50), ($py - 5), ($pz - 5)) @(-1, 0, 0) 34
        # towards +y onto the Y = -16 face from 50 away: 34
        Ray 'onto the -y face' @(($px + 3), ($py - 50), ($pz - 5)) @(0, 1, 0) 34
        # a ray at Y +8 passes beside the block (pitch alone has it reach Y +16)
        Ray 'beside the block' @(($px + 50), ($py + 8), ($pz - 5)) @(-1, 0, 0) $null

        # Collision with a probe of radius 2 centred 1 unit off a face: the ellipsoid and
        # the oriented box (the physics code takes a different path for each)
        foreach ($type in 1, 2) {
            $name = @{ 1 = 'ellipsoid'; 2 = 'box' }[$type]
            Expect "$name touching the +y face" (Collides ($px + 3) ($py + 1) ($pz - 5) $type) '1'
            Expect "$name touching the +x face" (Collides ($px + 17) ($py - 5) ($pz - 5) $type) '1'
            Expect "$name touching the -x face (X -16: pitch alone has no face there)" (Collides ($px - 17) ($py - 5) ($pz - 5) $type) '1'
            Expect "$name touching the top face" (Collides ($px + 3) ($py - 5) ($pz + 1) $type) '1'
            Expect "$name touching the bottom face" (Collides ($px + 3) ($py - 5) ($pz - 17) $type) '1'
            Expect "$name touching the -y face" (Collides ($px + 3) ($py - 17) ($pz - 5) $type) '1'
            Expect "$name past the +y face (Y 17: pitch alone has a face at 16)" (Collides ($px + 3) ($py + 17) ($pz - 5) $type) '0'
            Expect "$name above the top face (Z 8: roll alone has a face at 0)" (Collides ($px + 3) ($py - 5) ($pz + 8) $type) '0'
            Expect "$name inside, clear of every face" (Collides $px ($py - 8) ($pz - 8) $type) '0'
        }
        # Beside the vertical edge X 16, Y 0, the probe centre 1.6 off both faces: the
        # ellipsoid (radius 2) is 2.26 from the edge and clear, the box overlaps it
        Expect 'ellipsoid clear of the edge' (Collides ($px + 17.6) ($py + 1.6) ($pz - 8) 1) '0'
        Expect 'box overlapping the edge' (Collides ($px + 17.6) ($py + 1.6) ($pz - 8) 2) '1'
        Ed frame $px $py $pz -Dist 90 -Yaw 30 -Pitch 20
        Send 'sleep 1 []' 400
        Shot 'geot-pitch-roll'   # a flat slab standing on its edge: 32 long in x, 16 in y and z, its top at the instance's height
    }

    # ==== Lifecycle with live instances ====================================

    # Template 20: a 16-unit block, the entity at its centre, raised by $Dz.
    # Instances A (yaw 0) and B (yaw 180, equivalent: the block is symmetric about
    # the pivot in x and y) stand at the places below; D is replaced in every state.
    $LifePos = @{ A = @(3300, 2908, 2300); B = @(3300, 3100, 2300); D = @(3300, 3250, 2300) }
    $LifeBlock = @(3000, 2904, 2112)    # the block's minimum corner
    $script:Life = @{ T = -1; A = -1; B = -1; D = -1 }

    function LifeProbe([string]$Name, [int]$Idx, [double[]]$P, [bool]$Present, [int]$Dz) {
        # The block spans, relative to the instance, x and y -8..8 and z -8-Dz .. 8-Dz
        $bb = ''
        if ($Present) { $bb = "$($P[0] - 8) $($P[1] - 8) $($P[2] - 8 - $Dz) $($P[0] + 8) $($P[1] + 8) $($P[2] + 8 - $Dz)" }
        Expect "$Name bounds" (Eval "(geoinstancebb $Idx)") $bb
        # Two rays along +x, 9.5 below and 7.5 above the instance: the first hits
        # once the block has dropped by 2 (z spans -10..6), the second only while it
        # has not (-8..8). Distance 42 from 50 away to the x = -8 face.
        $low = $null; if ($Present -and $Dz -ge 2) { $low = 42 }
        $high = $null; if ($Present -and $Dz -le 0) { $high = 42 }
        Ray "$Name low ray" @(($P[0] - 50), ($P[1] + 3), ($P[2] - 9.5)) @(1, 0, 0) $low
        Ray "$Name high ray" @(($P[0] - 50), ($P[1] + 3), ($P[2] + 7.5)) @(1, 0, 0) $high
    }

    function LifeHover([string]$Name, [int]$Idx, [double[]]$P) {
        Ed cursor off
        Ed goto ($P[0] - 100) $P[1] $P[2]
        Ed lookatent $Idx
        Start-Sleep -Milliseconds 400
        Expect "$Name hovered" (@((EdState).Hover | ForEach-Object { $_.Idx }) -join ' ') "$Idx"
    }

    function LifeNewInstance([string]$Name, [string]$Yaw) {
        $p = $LifePos[$Name]
        $script:Life[$Name] = [int](Eval "(geot_newent geoinstance ""20 $Yaw 0 0 0 0 0 0 0"" $($p[0]) $($p[1]) $($p[2]))")
    }

    # The state every transition must leave: the template present or missing, raised
    # by $Dz. Probes both live instances, hovers and moves one, deletes the instance
    # an earlier state left behind and makes a new one.
    function LifeState([string]$Label, [bool]$Present, [int]$Dz) {
        $A = $LifePos.A; $B = $LifePos.B; $L = $script:Life
        $withD = if ($L.D -ge 0) { 3 } else { 2 }
        if ($Present) { Expect "$Label template 20 instances" (Info 20).Instances $withD }
        else { Expect "$Label template 20 absent" (Eval '(geotemplateinfo 20)') '' }
        LifeProbe "$Label A" $L.A $A $Present $Dz
        LifeProbe "$Label B" $L.B $B $Present $Dz
        if ($L.D -ge 0) {
            LifeProbe "$Label old D" $L.D $LifePos.D $Present $Dz
            Send "geot_delent $($L.D)"
            if ($Present) { Expect "$Label instances after deleting D" (Info 20).Instances 2 }
            Expect "$Label D gone" (Eval "(at (geot_get $($L.D)) 0)") 'none'
            $L.D = -1
        }
        LifeHover "$Label A" $L.A $A
        LifeHover "$Label B" $L.B $B
        $moved = @($A[0], ($A[1] + 40), $A[2])
        Send "geot_moveent $($L.A) $($moved[0]) $($moved[1]) $($moved[2])"
        LifeProbe "$Label A moved" $L.A $moved $Present $Dz
        LifeHover "$Label A moved" $L.A $moved
        Send "geot_moveent $($L.A) $($A[0]) $($A[1]) $($A[2])"
        LifeProbe "$Label A back" $L.A $A $Present $Dz
        LifeNewInstance 'D' '90'
        LifeProbe "$Label new D" $L.D $LifePos.D $Present $Dz
        # Drawn: all three are in view when the template is there, none when it is not
        Ed frame 3300 3050 2300 -Dist 500 -Yaw 0 -Pitch 0
        Send 'sleep 1 []' 600
        Send 'sleep 1 []' 600
        $want = if ($Present) { '3' } else { '0' }
        Expect "$Label instances drawn" (@((Eval '(geoinststats)') -split ' ')[0]) $want
    }

    Step 'lifecycle: instances with a live template' {
        Invoke-MapLoad { Ed newmap 12 }
        Send 'oqinst 1'
        Ed sel $LifeBlock[0] $LifeBlock[1] $LifeBlock[2] -Size 2,2,2
        Send 'edfillsel 1'
        $script:Life.T = [int](Eval '(geot_newent geotemplate "20 12 12 12" 3008 2912 2120)')
        Expect 'captured box' (Info 20).Box '3000 2904 2112 3016 2920 2128'
        LifeNewInstance 'A' '0'
        LifeNewInstance 'B' '180'
        LifeNewInstance 'D' '90'
        LifeState 'baseline' $true 0
    }

    Step 'lifecycle: move the template entity' {
        # Up by 2: the box is z 2110..2134 and still takes the whole block
        Send "geot_moveent $($script:Life.T) 3008 2912 2122"
        Expect 'captured box' (Info 20).Box '3000 2904 2112 3016 2920 2128'
        LifeState 'moved' $true 2
    }

    Step 'lifecycle: re-id the template, so the instances reference a missing id' {
        Send "geot_setattr $($script:Life.T) 0 21"
        Expect 'template 21 has no instances' (Info 21).Instances 0
        LifeState 'missing id' $false 2
    }

    Step 'lifecycle: re-id it back' {
        Send "geot_setattr $($script:Life.T) 0 20"
        LifeState 'id back' $true 2
    }

    Step 'lifecycle: delete the template entity, then recreate it' {
        Send "geot_delent $($script:Life.T)"
        LifeState 'template deleted' $false 2
        $script:Life.T = [int](Eval '(geot_newent geotemplate "20 12 12 12" 3008 2912 2122)')
        LifeState 'template recreated' $true 2
    }

    Step 'lifecycle: resetgl (cleanupva, then allchanged)' {
        Send 'resetgl' 5000
        LifeState 'after resetgl' $true 2
    }

    Step 'lifecycle: switch maps and come back' {
        Send 'savemap harness_geot_life' 1500
        Invoke-MapLoad { Ed newmap 12 }
        Expect 'no template in the other map' (Eval '(geotemplateinfo 20)') ''
        Invoke-MapLoad { Ed open harness_geot_life }
        # A loaded map's entities come back in a different order: find them again
        $script:Life.T = [int](Eval '(geot_find geotemplate)')
        $script:Life.A = [int](Eval '(geot_find geoinstance 2 0)')
        $script:Life.B = [int](Eval '(geot_find geoinstance 2 180)')
        $script:Life.D = [int](Eval '(geot_find geoinstance 2 90)')
        Write-Host "     after reload: $($script:Life.T) $($script:Life.A) $($script:Life.B) $($script:Life.D)"
        ExpectTrue 'entities found again after the reload' (($script:Life.T -ge 0) -and ($script:Life.A -ge 0) -and ($script:Life.B -ge 0) -and ($script:Life.D -ge 0))
        Send 'oqinst 1'
        LifeState 'after reload' $true 2
        Send "geot_delent $($script:Life.D)"
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
