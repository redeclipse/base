<#
.SYNOPSIS
    Drives the Red Eclipse map editor from outside, for agent/CI editor work.

.DESCRIPTION
    Shares the running client, home dir and command channel with harness.ps1 --
    one game instance. Start the client with 'harness.ps1 start', then use
    'open' or 'newmap' here to get into an editing session. The view commands
    (goto, aim, lookat, lookatent, frame, frameent, nudge) do nothing outside
    edit mode.

    A fresh 'newmap 12' world is 4096 units: it centres on (2048, 2048) with
    the floor surface at z = 2048, so that is where the examples aim.

    Unlike the UI harness, the engine side of this needs a build that has the
    editor test commands compiled in (src/build.sh defines DEBUG_UTILS).

.EXAMPLE
    tools\harness\harness.ps1 start
    tools\harness\editor.ps1 newmap 12
    tools\harness\editor.ps1 frame 2048 2048 2300 -Dist 128 -Yaw 45 -Pitch 20
    tools\harness\editor.ps1 seldrag 2052 2052 2048 2076 2076 2048
    tools\harness\editor.ps1 state
    tools\harness\editor.ps1 shot mysel
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('open', 'newmap', 'state', 'goto', 'aim', 'lookat', 'lookatent',
                 'frame', 'frameent', 'nudge', 'cursor', 'key', 'entsel', 'sel',
                 'seldrag', 'shot')]
    [string]$Command = 'state',

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest,

    [double]$Dist = 128,
    [double]$Yaw = 0,
    [double]$Pitch = 20,
    [double]$Radius = 32,
    [int[]]$Size,
    [int]$Settle = -1,
    [int]$TimeoutSec = 30,
    [switch]$Down,
    [switch]$Up,
    [switch]$Hover,
    [switch]$Raw
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'edstate.ps1')

# ------------------------------------------------------------- helpers ----

function Assert-EditorCfg {
    # editor.cfg is loaded on demand rather than from boot.cfg, so a UI-only
    # session never pays for it. Re-exec is cheap and idempotent.
    #
    # 'edh_ready' is unset on a cold session, so reading $edh_ready would log
    # "Unknown alias lookup: edh_ready" -- a string that matches core.ps1's
    # own $ErrorPattern -- before falling back to falsy. identexists is a real
    # command (src/engine/command.cpp:1119) that answers without the lookup.
    Invoke-Batch 'if (! (identexists edh_ready)) [ exec "tools/harness/editor.cfg" 0 0 ]' 1 $TimeoutSec | Out-Null
}

function Get-Args([int]$Count, [string]$Usage) {
    $values = @($Rest | Where-Object { $_ -ne '' })
    if ($values.Count -lt $Count) { throw "Usage: $Usage" }
    return $values[0..($Count - 1)] | ForEach-Object { Format-Coord (ConvertTo-InvariantDouble $_) }
}

function Invoke-Editor([string]$Script, [int]$DefaultSettle) {
    Assert-EditorCfg
    $settleMs = if ($Settle -ge 0) { $Settle } else { $DefaultSettle }
    return Invoke-Batch $Script $settleMs $TimeoutSec
}

function Get-EdState {
    $lines = Invoke-Editor 'eddumpstate' 100
    $state = ConvertFrom-EdState $lines
    if (-not $state.Complete) {
        throw "eddumpstate returned an incomplete dump. Raw output:`n$($lines -join "`n")"
    }
    if ($state.Malformed.Count) {
        Write-Warning "Unparsed state lines:`n  $($state.Malformed -join "`n  ")"
    }
    return $state
}

function Show-EdState($State) {
    Write-Host ('mode      editing={0} gridpower={1} gridsize={2} orient={3}' -f `
        $State.Mode.EditMode, $State.Mode.GridPower, $State.Mode.GridSize, $State.Mode.Orient) -ForegroundColor Cyan
    Write-Host ('camera    ({0}, {1}, {2}) yaw={3} pitch={4}' -f `
        (Format-Coord $State.Cam.X), (Format-Coord $State.Cam.Y), (Format-Coord $State.Cam.Z),
        (Format-Coord $State.Cam.Yaw), (Format-Coord $State.Cam.Pitch))
    Write-Host ('worldpos  ({0}, {1}, {2})' -f `
        (Format-Coord $State.WorldPos.X), (Format-Coord $State.WorldPos.Y), (Format-Coord $State.WorldPos.Z))
    Write-Host ('cursor    ({0}, {1}, {2}) orient={3}' -f `
        $State.Cur.X, $State.Cur.Y, $State.Cur.Z, $State.Cur.Orient)
    if ($State.Sel.HaveSel) {
        Write-Host ('selection o=({0}, {1}, {2}) s=({3}, {4}, {5}) grid={6} orient={7} children={8}' -f `
            $State.Sel.OX, $State.Sel.OY, $State.Sel.OZ,
            $State.Sel.SX, $State.Sel.SY, $State.Sel.SZ,
            $State.Sel.Grid, $State.Sel.Orient, $State.Sel.Children) -ForegroundColor Green
    }
    else { Write-Host 'selection none' -ForegroundColor Yellow }
    Write-Host ('ui        cursorlock={0} freecursor={1}' -f $State.Ui.CursorLock, $State.Ui.FreeCursor)
    foreach ($e in $State.Hover)  { Write-Host ('hover     [{0}] {1} ({2}, {3}, {4})' -f $e.Idx, $e.Type, (Format-Coord $e.X), (Format-Coord $e.Y), (Format-Coord $e.Z)) }
    foreach ($e in $State.EntSel) { Write-Host ('entsel    [{0}] {1} ({2}, {3}, {4})' -f $e.Idx, $e.Type, (Format-Coord $e.X), (Format-Coord $e.Y), (Format-Coord $e.Z)) -ForegroundColor Green }
}

# ------------------------------------------------------------ commands ----

switch ($Command) {

    'open' {
        $name = ($Rest -join ' ').Trim()
        if (-not $name) { throw 'Usage: editor.ps1 open <mapname>' }
        # emptymap() refuses unless edit mode is already on, toggleedit()
        # refuses unless connected(false) and allowedittoggle
        # (src/engine/octaedit.cpp:198), and the boot-time localconnect is a
        # no-op (autoconnect defaults to 0), so plain 'map' cannot get us
        # into an editing session. 'edit' (the
        # config/setup.cfg alias) sets mode G_EDITING and force-local-connects
        # before loading the map -- that is the real entry point.
        # Map load is slow and clears IDF_MAP sleeps; give it room.
        Show-BatchResult (Invoke-Editor "edit $name" 4000)
        Show-BatchResult (Invoke-Editor 'edh_enter' 600)
        Show-EdState (Get-EdState)
    }

    'newmap' {
        $size = if ($Rest -and $Rest[0]) { [int]$Rest[0] } else { 12 }
        # newmap (emptymap()) is legal only once edit mode is already on, so
        # first 'edit' into a stable scratch map, then enter edit mode, then
        # replace it with a deterministic empty world of the requested size.
        Show-BatchResult (Invoke-Editor 'edit harness_scratch' 4000)
        Show-BatchResult (Invoke-Editor 'edh_enter' 600)
        Show-BatchResult (Invoke-Editor "newmap $size" 3000)
        Show-EdState (Get-EdState)
    }

    'state' {
        $state = Get-EdState
        if ($Raw) { Write-Output $state } else { Show-EdState $state; Write-Output $state }
    }

    'goto' {
        $a = Get-Args 3 'editor.ps1 goto <x> <y> <z>'
        Show-BatchResult (Invoke-Editor "edgoto $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'aim' {
        $a = Get-Args 2 'editor.ps1 aim <yaw> <pitch>'
        Show-BatchResult (Invoke-Editor "edaim $($a[0]) $($a[1])" 150)
    }

    'lookat' {
        $a = Get-Args 3 'editor.ps1 lookat <x> <y> <z>'
        Show-BatchResult (Invoke-Editor "edlookat $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'lookatent' {
        $a = Get-Args 1 'editor.ps1 lookatent <idx>'
        $lines = Invoke-Editor "echo (concatword ""EDRESULT="" (edlookatent $($a[0])))" 150
        Show-BatchResult $lines
        if ($lines -match 'EDRESULT=0') { throw "No live entity at index $($a[0]), or not in edit mode." }
    }

    'frame' {
        $a = Get-Args 3 'editor.ps1 frame <x> <y> <z> [-Dist n] [-Yaw n] [-Pitch n]'
        $script = 'edframe {0} {1} {2} {3} {4} {5}' -f $a[0], $a[1], $a[2],
            (Format-Coord $Dist), (Format-Coord $Yaw), (Format-Coord $Pitch)
        Show-BatchResult (Invoke-Editor $script 200)
    }

    'frameent' {
        $a = Get-Args 1 'editor.ps1 frameent <idx> [-Dist n] [-Yaw n] [-Pitch n]'
        $script = 'echo (concatword "EDRESULT=" (edframeent {0} {1} {2} {3}))' -f $a[0],
            (Format-Coord $Dist), (Format-Coord $Yaw), (Format-Coord $Pitch)
        $lines = Invoke-Editor $script 200
        Show-BatchResult $lines
        if ($lines -match 'EDRESULT=0') { throw "No live entity at index $($a[0]), or not in edit mode." }
    }

    'nudge' {
        $a = Get-Args 3 'editor.ps1 nudge <fwd> <right> <up>'
        Show-BatchResult (Invoke-Editor "ednudge $($a[0]) $($a[1]) $($a[2])" 150)
    }

    'cursor' {
        $mode = ($Rest -join '').Trim().ToLower()
        $script = switch ($mode) {
            'on'     { 'edh_cursor 1' }
            'off'    { 'edh_cursor 0' }
            'toggle' { 'edh_tap TAB' }
            default  { throw 'Usage: editor.ps1 cursor <on|off|toggle>' }
        }
        Show-BatchResult (Invoke-Editor $script 500)
    }

    'key' {
        $name = ($Rest -join '').Trim()
        if (-not $name) { throw 'Usage: editor.ps1 key <NAME> [-Down] [-Up]' }
        # Default branch taps once (press, report, release) rather than
        # pressing twice -- gamekeypress $name 1, THEN edh_tap $name (which
        # itself presses again) would press-press-release.
        $script =
            if ($Down -and -not $Up) { "echo (concatword ""EDRESULT="" (gamekeypress $name 1))" }
            elseif ($Up -and -not $Down) { "echo (concatword ""EDRESULT="" (gamekeypress $name 0))" }
            else { "echo (concatword ""EDRESULT="" (gamekeypress $name 1))`nsleep 50 (concat gamekeypress $name 0)" }
        $lines = Invoke-Editor $script 300
        Show-BatchResult $lines
        # gamekeypress returns 0 only when the name resolves to no key code
        # (config/keymap.cfg, matched case-insensitively).
        if ($lines -match 'EDRESULT=0') { throw "Unknown key name '$name'." }
    }

    'entsel' {
        $a = Get-Args 3 'editor.ps1 entsel <x> <y> <z> [-Radius n] [-Hover]'
        $script =
            if ($Hover) {
                # The path a user takes: aim at it, then add what is hovered.
                "edlookat $($a[0]) $($a[1]) $($a[2])`nsleep 150 [ entadd ]"
            }
            else {
                'echo (concatword "EDRESULT=" (edentnear {0} {1} {2} {3}))' -f `
                    $a[0], $a[1], $a[2], (Format-Coord $Radius)
            }
        Show-BatchResult (Invoke-Editor $script 400)
        Show-EdState (Get-EdState)
    }

    'sel' {
        $a = Get-Args 3 'editor.ps1 sel <x> <y> <z> [-Size sx,sy,sz]'
        # -Size given with the wrong arity must not silently fall back to a
        # single cube -- the caller asked for a box.
        if ($PSBoundParameters.ContainsKey('Size') -and @($Size).Count -ne 3) {
            throw "Usage: editor.ps1 sel <x> <y> <z> [-Size sx,sy,sz] -- -Size needs exactly 3 values, got $(@($Size).Count)."
        }
        $script =
            if ($Size) {
                "edselbox $($a[0]) $($a[1]) $($a[2]) $($Size[0]) $($Size[1]) $($Size[2])"
            }
            else { "edselcube $($a[0]) $($a[1]) $($a[2])" }
        Show-BatchResult (Invoke-Editor $script 300)
        Show-EdState (Get-EdState)
    }

    'seldrag' {
        $a = Get-Args 6 'editor.ps1 seldrag <x1> <y1> <z1> <x2> <y2> <z2>'
        $script = 'edh_dragsel {0} {1} {2} {3} {4} {5}' -f $a[0], $a[1], $a[2], $a[3], $a[4], $a[5]
        Show-BatchResult (Invoke-Editor $script 900)
        Show-EdState (Get-EdState)
    }

    'shot' {
        Assert-EditorCfg
        $settleMs = if ($Settle -ge 0) { $Settle } else { 300 }
        Write-Output (Invoke-Shot (($Rest -join '_').Trim()) $settleMs $TimeoutSec)
    }
}
