<#
.SYNOPSIS
    End-to-end self-test for the map editor harness.

.DESCRIPTION
    Boots a client, drives every editor capability the harness claims to
    support, and asserts the resulting editor state. Selection is exercised
    through the REAL input path (gamekeypress -> execbind -> drag/onrelease);
    the coordinate-addressed wrappers are used only for deterministic setup.

    Every one of editor.ps1's 16 subcommands is invoked at least once:
      open newmap state goto aim lookat lookatent frame frameent nudge
      cursor key entsel sel seldrag shot

.EXAMPLE
    tools\harness\editor-selftest.ps1
    tools\harness\editor-selftest.ps1 -KeepRunning
    tools\harness\editor-selftest.ps1 -Map complex
#>
[CmdletBinding()]
param(
    [switch]$KeepRunning,
    # Which shipped map the 'open' step loads. Only that step uses it -- every
    # other step works in the deterministic 'newmap 12' scratch world, whose
    # geometry the coordinate expectations below are derived from.
    [string]$Map = 'atop'
)

$ErrorActionPreference = 'Stop'

$harness = Join-Path $PSScriptRoot 'harness.ps1'
$editor  = Join-Path $PSScriptRoot 'editor.ps1'

$script:failures = 0
$script:step = 0
# Set by the entity step, reused by the edentnear setup-shortcut check.
$script:entIdx = $null
$script:entPos = $null

# --------------------------------------------------------- scratch world ----
#
# 'newmap 12' builds a 4096-unit world whose bottom four octants are solid
# (emptymap(), src/engine/world.cpp:1656-1657), so the floor surface is the
# z = 2048 plane and everything above it is air. Default gridpower is 3, i.e.
# an 8-unit grid. Every coordinate below is derived from that, not guessed --
# the brief's 512-ish numbers come from a smaller world and land in empty air.
# Kept integral: these are interpolated into CubeScript and passed to editor.ps1
# as strings, and only an integer formats identically in every locale.
$Floor  = 2048       # z of the floor surface
$Mid    = 2048       # x/y of the world centre
$Grid   = 8          # gridsize at the default gridpower 3

# ------------------------------------------------------------- reporting ----

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    $before = $script:failures
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try {
        & $Body
    }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
    # A red step is not debuggable without the state that produced it, so dump
    # it once per failing step (editor.ps1's own 'state' printout).
    if ($script:failures -gt $before) {
        Write-Host '     --- editor state at failure ---' -ForegroundColor DarkYellow
        try { & $editor state | Out-Null }
        catch { Write-Host "     (state unavailable: $_)" -ForegroundColor DarkYellow }
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ($Actual -eq $Expected) { Write-Host "     ok    $What = $Actual" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

function ExpectNear([string]$What, [double]$Actual, [double]$Expected, [double]$Tolerance = 2.0) {
    if ([Math]::Abs($Actual - $Expected) -le $Tolerance) {
        Write-Host ("     ok    {0} = {1} (~{2})" -f $What, $Actual, $Expected) -ForegroundColor Green
    }
    else {
        Write-Host ("     FAIL  {0} -- expected ~{1} (+/-{2}), got {3}" -f $What, $Expected, $Tolerance, $Actual) -ForegroundColor Red
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

# editor.ps1 and harness.ps1 narrate to the host (Write-Host -> the information
# stream) and echo the game's own log lines to the success stream. Neither is
# wanted between assertions, so both are dropped; warnings and errors are not.
function Ed { & $editor @args 6>$null | Out-Null }

function EdOut { return @(& $editor @args 6>$null) }

function EdState { return (& $editor state -Raw 6>$null) }

function Send([string]$Script, [int]$SettleMs = 250) {
    & $harness send $Script -Settle $SettleMs 6>$null | Out-Null
}

function GetVar([string]$Name) {
    $out = @(& $harness send "echo (concatword ""SELFTEST=$Name="" `$$Name)" 6>$null)
    foreach ($line in $out) {
        if ($line -match "SELFTEST=$Name=(.*)$") { return $Matches[1].Trim() }
    }
    return ''
}

# ENGINE BUG WORKAROUND -- read this before removing it.
#
# 'enthover' (src/engine/world.cpp:353) holds indices into entities::getents().
# resetmap() clears the entity list (PROGRESS(18) entities::clearents()) but the
# cancelsel() it runs first only clears 'entgroup' -- entcancel(), world.cpp:389,
# never touches enthover. load_world() then calls progress(), which builds the
# HUD, and config/ui reads the hovered entity through 'enthoverloopread'
# (world.cpp:1426) -- an UNGUARDED ents[enthover[i]]. So loading a map while an
# entity is hovered aborts the client on tools.h:904 (i>=0 && i<ulen).
#
# Turning entediting off first empties enthover through its own VARF
# (world.cpp:356-364) and makes hoveringonent() bail out via noentedit(), so
# nothing repopulates it while the map loads. Restored immediately after --
# in a finally, so a load that throws cannot leave entity editing off and have
# a later, unrelated step fail for that reason instead of its own.
function Invoke-MapLoad([scriptblock]$Body) {
    Send 'entediting 0' 300
    try { & $Body }
    finally { Send 'entediting 1' 300 }
}

# The gridsize-aligned cube containing coordinate $v. This is the engine's own
# rule for cur (lookupcube + normalizelookupcube, octaedit.cpp:589-594): floor
# to a multiple of the grid.
function CubeAt([double]$v) { return [int]([Math]::Floor($v / $Grid) * $Grid) }

# O_TOP in the orient enum, src/engine/octa.h (LEFT, RIGHT, BACK, FRONT,
# BOTTOM, TOP = 0..5): the face a ray coming down onto the floor strikes.
$OTop = 5

# --------------------------------------------------------------------------

& $harness stop 6>$null | Out-Null
& $harness start 6>$null | Out-Null

Step 'open: load a shipped map into an editing session' {
    Invoke-MapLoad { Ed open $Map }
    $s = EdState
    Expect 'edit mode' $s.Mode.EditMode 1
    ExpectTrue 'grid size is positive' ($s.Mode.GridSize -gt 0) "gridsize=$($s.Mode.GridSize)"
    $name = GetVar 'mapname'
    ExpectTrue "the requested map is loaded (mapname='$name')" ($name -like "*$Map") "expected a name ending in '$Map'"
}

Step 'newmap: build the deterministic scratch world' {
    Invoke-MapLoad { Ed newmap 12 }
    $s = EdState
    Expect 'edit mode' $s.Mode.EditMode 1
    Expect 'grid size' $s.Mode.GridSize $Grid
    Expect 'world size' (GetVar 'mapsize') '4096'
}

Step 'frame: one call fully determines the view' {
    # Yaw 0 / pitch 0 puts the camera dist units on the -Y side of the target,
    # looking back along +Y (orbitpos, src/engine/world.cpp:925).
    Ed frame $Mid $Mid 2300 -Dist 100 -Yaw 0 -Pitch 0
    $s = EdState
    ExpectNear 'camera x' $s.Cam.X $Mid
    ExpectNear 'camera y' $s.Cam.Y ($Mid - 100)
    ExpectNear 'camera z' $s.Cam.Z 2300
    ExpectNear 'camera yaw' $s.Cam.Yaw 0
    ExpectNear 'camera pitch' $s.Cam.Pitch 0
}

Step 'goto / aim: both are absolute' {
    Ed goto $Mid 2000 2400
    Ed aim 45 -30
    $s = EdState
    ExpectNear 'camera x' $s.Cam.X $Mid
    ExpectNear 'camera y' $s.Cam.Y 2000
    ExpectNear 'camera z' $s.Cam.Z 2400
    ExpectNear 'camera yaw' $s.Cam.Yaw 45
    ExpectNear 'camera pitch' $s.Cam.Pitch -30
}

Step 'lookat: aiming at a point yields the angles to it' {
    Ed goto $Mid $Mid 2300
    # 100 out along +Y and 100 down: straight ahead at yaw 0, 45 degrees down.
    Ed lookat $Mid ($Mid + 100) 2200
    $s = EdState
    ExpectNear 'camera position is unchanged' $s.Cam.Y $Mid
    ExpectNear 'yaw towards +Y' $s.Cam.Yaw 0
    ExpectNear 'pitch 45 degrees down' $s.Cam.Pitch -45

    # A second, different target -- otherwise a lookat that silently did nothing
    # would still pass the yaw-0 check above.
    Ed lookat ($Mid + 100) $Mid 2300
    $s = EdState
    ExpectNear 'yaw towards +X' $s.Cam.Yaw 270
    ExpectNear 'pitch is level' $s.Cam.Pitch 0
}

Step 'nudge: relative to the facing, not to the axes' {
    Ed goto $Mid $Mid 2300
    Ed aim 0 0
    # At yaw 0 the facing is +Y, so "right" is -X (world.cpp:1043, corrected in
    # Task 3): forward 100 -> +Y, right 64 -> -X, up 32 -> +Z.
    Ed nudge 100 64 32
    $s = EdState
    ExpectNear 'moved right along -X' $s.Cam.X ($Mid - 64)
    ExpectNear 'moved forward along +Y' $s.Cam.Y ($Mid + 100)
    ExpectNear 'moved up along +Z' $s.Cam.Z 2332
}

Step 'cursor: lock and unlock through the real TAB bind' {
    # ui_freecursor boots as 1 (config/ui/lib.cfg:11), which edh_cursor already
    # reads as "on" -- so 'cursor on' from boot presses nothing. Drive it off
    # first, so the 'on' below has to go through the TAB bind.
    Ed cursor off
    Expect 'freecursor cleared before the on-test' (EdState).Ui.FreeCursor 0

    # The only writer is ta_cursor_mode (config/tool/tooledit.cfg:473), whose
    # "on" value is $clockmillis. -gt 1 therefore cannot be met by the boot
    # default of 1 -- only a real TAB press produces it.
    Ed cursor on
    $on = (EdState).Ui.FreeCursor
    ExpectTrue 'TAB set freecursor to a clockmillis timestamp' ($on -gt 1) "freecursor=$on"

    # Absolute, so a second 'on' must not toggle it back off.
    Ed cursor on
    $again = (EdState).Ui.FreeCursor
    ExpectTrue 'cursor on is idempotent' ($again -gt 1) "freecursor=$again"

    Ed cursor off
    Expect 'freecursor cleared' (EdState).Ui.FreeCursor 0
}

Step 'seldrag: geometry selection through the real MOUSE1 path' {
    # Stand above the floor looking down at it, so the crosshair ray strikes
    # world geometry at both drag corners. Both corners are aimed at the middle
    # of a grid cell, not its edge, so sub-unit ray jitter cannot round the
    # cursor into the neighbouring cube.
    Ed goto $Mid ($Mid - 148) 2300
    $near = $Mid + 4                          # inside the cell at 2048
    $far  = $Mid + 3 * $Grid + 4              # inside the cell at 2072

    # ---- EDSTATE cur: the cube under the crosshair, before any drag -------
    # Expected values are derived, not measured. rendereditcursor()
    # (octaedit.cpp:578-594) steps 0.05 past the ray's hit point and sets cur
    # to the gridsize cube containing that point. Aimed down onto the z = Floor
    # plane, the step lands just inside the solid below it, so
    #   cur = (CubeAt x, CubeAt y, Floor - Grid), orient = O_TOP.
    # Aiming at the middle of a cell (+4 on an 8 grid) keeps float jitter in
    # the ray from rounding into a neighbour.
    Ed lookat $near $near $Floor
    $s = EdState
    Expect 'cur x under the near corner' $s.Cur.X (CubeAt $near)
    Expect 'cur y under the near corner' $s.Cur.Y (CubeAt $near)
    Expect 'cur z (the cube under the surface)' $s.Cur.Z ($Floor - $Grid)
    Expect 'cur orient (top face)' $s.Cur.Orient $OTop

    Ed seldrag $near $near $Floor $far $far $Floor

    $s = EdState
    # The drag ends aimed at the far corner, so cur must have followed it to a
    # different cube -- a stale cur cannot satisfy both this and the above.
    Expect 'cur x under the far corner' $s.Cur.X (CubeAt $far)
    Expect 'cur y under the far corner' $s.Cur.Y (CubeAt $far)
    Expect 'cur z after the drag' $s.Cur.Z ($Floor - $Grid)
    Expect 'cur orient after the drag' $s.Cur.Orient $OTop
    Expect 'a selection exists' $s.Sel.HaveSel 1
    Expect 'selection grid' $s.Sel.Grid $Grid
    Expect 'selection origin x' $s.Sel.OX $Mid
    Expect 'selection origin y' $s.Sel.OY $Mid
    Expect 'selection origin z (the cube under the surface)' $s.Sel.OZ ($Floor - $Grid)
    # The drag spanned 3 whole cells, so the selection must be 4 cubes wide in
    # both dragged axes. 1x1 would mean the release landed on the press corner,
    # i.e. the drag never registered -- which is exactly what this step exists
    # to catch.
    Expect 'selection spans the drag in x' $s.Sel.SX 4
    Expect 'selection spans the drag in y' $s.Sel.SY 4
    Expect 'selection is one cube deep' $s.Sel.SZ 1
    # selchildcount counts the octree leaves the box overlaps (octaedit.cpp:
    # 327-339), then is replaced by -mag when that count is 1 and mag > 1
    # (:648-651). mag = lusize / gridsize (:591). The whole box sits inside one
    # untouched 2048-unit solid root octant, so mag = 2048 / 8 = 256 and the
    # exact value is -256. (This pins the octree state; it does not by itself
    # prove the ray hit the floor -- origin z = 2040 above is what proves that.)
    Expect 'selchildcount (one 2048 leaf at grid 8 -> -2048/8)' $s.Sel.Children -256
}

Step 'key: a synthetic keypress reaches its editbind' {
    # Precondition: the drag above left a selection in place.
    ExpectTrue 'a selection is in place to cancel' ((EdState).Sel.HaveSel -eq 1)

    # SPACE is bound to [ entmoving 0; cancelsel ] (config/setup.cfg:290), so a
    # real press must clear the selection. Nothing but the bind can do that.
    $out = EdOut key SPACE
    ExpectTrue 'the key was dispatched' (@($out -match 'EDRESULT=1').Count -gt 0) "output: $($out -join ' | ')"
    Expect 'the SPACE editbind cancelled the selection' (EdState).Sel.HaveSel 0

    # Key names resolve case-insensitively (resolvekeycode uses strcasecmp,
    # like findkeycode/findbind), so a lower-case name reaches the same bind.
    # Setup through the shortcut; the assertion is on the bind's effect.
    Ed sel $Mid $Mid ($Floor - $Grid)
    ExpectTrue 'a selection is in place for the lower-case press' ((EdState).Sel.HaveSel -eq 1)
    $out = EdOut key space
    ExpectTrue 'the lower-case key was dispatched' (@($out -match 'EDRESULT=1').Count -gt 0) "output: $($out -join ' | ')"
    Expect 'the lower-case space reached the same editbind' (EdState).Sel.HaveSel 0

    # An unknown name is an error, like lookatent/frameent on a dead index.
    $err = ''
    try { Ed key NOTAKEY } catch { $err = "$_" }
    ExpectTrue 'an unknown key name is rejected with an error' ($err -match "Unknown key name 'NOTAKEY'") "error: '$err'"
}

Step 'sel: the coordinate-addressed setup shortcut' {
    # NOT the path under test -- geometry selection proper is asserted through
    # seldrag above. This only confirms the deterministic setup wrapper other
    # tests lean on still places the box it is asked for.
    Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2,1
    $s = EdState
    Expect 'a selection exists' $s.Sel.HaveSel 1
    Expect 'origin x' $s.Sel.OX $Mid
    Expect 'origin y' $s.Sel.OY $Mid
    Expect 'origin z' $s.Sel.OZ ($Floor - $Grid)
    Expect 'size x' $s.Sel.SX 2
    Expect 'size y' $s.Sel.SY 2
    Expect 'size z' $s.Sel.SZ 1

    # A -Size of the wrong arity is a usage error, not a silent single cube.
    $err = ''
    try { Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2 } catch { $err = "$_" }
    ExpectTrue 'sel -Size with 2 values is rejected' ($err -match '-Size needs exactly 3 values') "error: '$err'"
}

Step 'lookatent / entsel / frameent: entity selection through the real hover path' {
    # ---- setup only -------------------------------------------------------
    # newent drops the entity relative to the current selection (entdrop), so
    # pin it afterwards: newent implicitly selects what it made, and entpos
    # edits the selected group.
    $ex = $Mid; $ey = $Mid; $ez = 2100
    Send 'newent "playerstart" ""' 400
    Send "entpos $ex $ey $ez" 400

    $s = EdState
    ExpectTrue 'the entity was created and implicitly selected' ($s.EntSel.Count -eq 1) "entsel count=$($s.EntSel.Count)"
    if ($s.EntSel.Count -ne 1) { return }
    $idx = $s.EntSel[0].Idx
    $script:entIdx = $idx
    $script:entPos = @($ex, $ey, $ez)
    Expect 'it is a playerstart' $s.EntSel[0].Type 'playerstart'
    ExpectNear 'entity z is where entpos put it' $s.EntSel[0].Z $ez 0.01

    Send 'entcancel' 300
    ExpectTrue 'the implicit selection is cleared' ((EdState).EntSel.Count -eq 0)

    # ---- lookatent: aim only, and let the editor recompute the hover -------
    Ed goto $ex ($ey - 96) ($ez + 40)
    Ed lookatent $idx
    $s = EdState
    ExpectNear 'lookatent yaw towards +Y' $s.Cam.Yaw 0
    # atan2(-40, 96) = -22.62 degrees.
    ExpectNear 'lookatent pitch down onto the entity' $s.Cam.Pitch -22.62 0.1
    # @()-wrapped: a lone match comes back as the Hashtable itself, whose .Count
    # is its key count (5), not 1.
    ExpectTrue 'the entity is hovered' (@($s.Hover | Where-Object { $_.Idx -eq $idx }).Count -eq 1) "hover=$($s.Hover.Count)"
    ExpectTrue 'hovering alone does not select' ($s.EntSel.Count -eq 0) "entsel count=$($s.EntSel.Count)"

    # ---- entsel -Hover: aim, then add what the editor says is hovered ------
    # This is the user's path (edlookat + the engine's own entadd), NOT the
    # coordinate-addressed edentnear shortcut.
    Ed entsel $ex $ey $ez -Hover
    $s = EdState
    ExpectTrue 'the hovered entity is now selected' (@($s.EntSel | Where-Object { $_.Idx -eq $idx }).Count -eq 1) "entsel=$($s.EntSel.Count)"

    # ---- frameent: orbit the camera around it -----------------------------
    $dist = 96.0; $yaw = 30.0; $pitch = -25.0
    Ed frameent $idx -Dist $dist -Yaw $yaw -Pitch $pitch
    $s = EdState
    # orbitpos(): camera = target - dir(yaw,pitch)*dist, with
    # dir = (-sin(yaw)cos(pitch), cos(yaw)cos(pitch), sin(pitch)).
    $ry = $yaw * [Math]::PI / 180; $rp = $pitch * [Math]::PI / 180
    ExpectNear 'frameent camera x' $s.Cam.X ($ex + [Math]::Sin($ry) * [Math]::Cos($rp) * $dist) 0.01
    ExpectNear 'frameent camera y' $s.Cam.Y ($ey - [Math]::Cos($ry) * [Math]::Cos($rp) * $dist) 0.01
    ExpectNear 'frameent camera z' $s.Cam.Z ($ez - [Math]::Sin($rp) * $dist) 0.01
    ExpectNear 'frameent camera yaw' $s.Cam.Yaw $yaw 0.01
    ExpectNear 'frameent camera pitch' $s.Cam.Pitch $pitch 0.01
    ExpectTrue 'the entity is still hovered from the new angle' (@($s.Hover | Where-Object { $_.Idx -eq $idx }).Count -eq 1) "hover=$($s.Hover.Count)"
}

Step 'entsel (no -Hover): the coordinate-addressed setup shortcut' {
    # NOT the entity selection test -- that is 'entsel -Hover' above, through
    # the real aim-and-hover path. This only confirms the edentnear shortcut
    # other tests lean on for setup still selects what it is pointed at.
    ExpectTrue 'the entity from the previous step exists' ($null -ne $script:entIdx)
    if ($null -eq $script:entIdx) { return }
    $idx = $script:entIdx
    $ex, $ey, $ez = $script:entPos

    Send 'entcancel' 300
    ExpectTrue 'the selection is cleared first' ((EdState).EntSel.Count -eq 0)

    # 5 units off the entity, inside a 16-unit radius.
    $out = EdOut entsel ($ex + 5) $ey $ez -Radius 16
    ExpectTrue "edentnear reported index $idx" (@($out -match "EDRESULT=$idx$").Count -gt 0) "output: $($out -join ' | ')"
    $s = EdState
    ExpectTrue "entity $idx is selected" (@($s.EntSel | Where-Object { $_.Idx -eq $idx }).Count -eq 1) "entsel=$(@($s.EntSel | ForEach-Object { $_.Idx }) -join ',')"
    ExpectTrue 'nothing else is selected' ($s.EntSel.Count -eq 1) "entsel count=$($s.EntSel.Count)"

    # Negative: a point ~1400 units from the only entity with an 8-unit radius
    # must select nothing, and report it as -1 rather than throwing.
    Send 'entcancel' 300
    $out = @()
    $err = ''
    try { $out = EdOut entsel ($ex + 1000) ($ey + 1000) $ez -Radius 8 } catch { $err = "$_" }
    ExpectTrue 'a miss does not throw' ($err -eq '') "error: '$err'"
    ExpectTrue 'a miss reports -1' (@($out -match 'EDRESULT=-1$').Count -gt 0) "output: $($out -join ' | ')"
    Expect 'a miss selects nothing' (EdState).EntSel.Count 0
}

Step 'shot: a screenshot of the framed entity is written' {
    # 'shot' echoes the batch's log lines before the path it returns, so take the
    # last value rather than assuming a scalar.
    $out = @(& $editor shot editor-selftest 6>$null)
    $png = if ($out.Count) { [string]$out[-1] } else { '' }
    ExpectTrue "screenshot written to $png" ($png -and (Test-Path $png)) "shot returned: $($out -join ' | ')"
}

# --------------------------------------------------------------------------

Write-Host ''
if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'editor self-test: all checks passed' -ForegroundColor Green
exit 0
