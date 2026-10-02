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
