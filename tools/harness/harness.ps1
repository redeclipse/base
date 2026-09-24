<#
.SYNOPSIS
    Drives a running Red Eclipse client from outside, for agent/CI UI work.

.DESCRIPTION
    Launches the game against an isolated home dir with a CubeScript command
    channel installed (tools/harness/boot.cfg), then sends CubeScript batches
    and reads their output back.

    The UI is CubeScript, so editing config/ui/**.cfg and calling 'reload'
    applies the change live -- no rebuild, no restart.

.EXAMPLE
    tools\harness\harness.ps1 start
    tools\harness\harness.ps1 nav ui_gameui_settings_graphics
    tools\harness\harness.ps1 shot graphics
    tools\harness\harness.ps1 reload config/ui/game/settings.cfg
    tools\harness\harness.ps1 send 'echo "top=" $uitopname'
    tools\harness\harness.ps1 stop
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('start', 'stop', 'status', 'send', 'shot', 'reload', 'nav', 'log', 'tree', 'find', 'click')]
    [string]$Command = 'status',

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest,

    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$Settle = -1,
    [int]$TimeoutSec = 30,
    [string]$File,
    [switch]$Focus,
    [switch]$Drawn,
    [switch]$Text
)

$ErrorActionPreference = 'Stop'

# Shared transport and process plumbing, also used by editor.ps1.
. (Join-Path $PSScriptRoot 'core.ps1')

# ------------------------------------------------------- widget queries ----
#
# uidumptree reports object rects with x in ASPECT space (0 .. hudw/hudh) while
# the cursor is in screen fractions (0..1 across the full width), so clicking a
# widget means dividing its x by the aspect ratio. Verified by hovering a known
# button and confirming it highlights.

function ConvertTo-TreeDouble([string]$Value) {
    # An empty scroll area divides 0 by 0 in the engine (ScrollBar::vscale), so
    # uidumptree can print MSVC's 'nan', '-nan(ind)', 'inf'... for a rect. One
    # such widget must not take the whole tree down; map these onto real
    # non-finite doubles and leave the rest to the strict parser.
    switch -Regex ($Value) {
        '^[+-]?nan'  { return [double]::NaN }
        '^\+?inf'    { return [double]::PositiveInfinity }
        '^-inf'      { return [double]::NegativeInfinity }
    }
    return ConvertTo-InvariantDouble $Value
}

function Get-UiTree {
    $lines = Invoke-Batch "echo `"UIASPECT `" `$uiaspect`nuidumptree" 1 $TimeoutSec

    $aspect = 16 / 9
    $nodes = New-Object System.Collections.ArrayList

    foreach ($raw in $lines) {
        $t = $raw -replace '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ', ''
        if ($t -match '^UIASPECT\s+([0-9.]+)') { $aspect = ConvertTo-InvariantDouble $Matches[1]; continue }
        if ($t -match '^UITREE (\d+) ([01]) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)\s?(.*)$') {
            [void]$nodes.Add([pscustomobject]@{
                Depth = [int]$Matches[1]
                Drawn = ($Matches[2] -eq '1')
                Type  = $Matches[3]
                X     = ConvertTo-TreeDouble $Matches[4]
                Y     = ConvertTo-TreeDouble $Matches[5]
                W     = ConvertTo-TreeDouble $Matches[6]
                H     = ConvertTo-TreeDouble $Matches[7]
                Tag   = $Matches[8]
                Text  = $Matches[9].Trim()
            })
        }
    }

    return [pscustomobject]@{ Aspect = $aspect; Nodes = $nodes }
}

function Find-Widget($Tree, [string]$Label) {
    # The sum is NaN/infinite if any coordinate is (see ConvertTo-TreeDouble);
    # such a widget has no usable click point.
    $candidates = @($Tree.Nodes | Where-Object {
        $sum = $_.X + $_.Y + $_.W + $_.H
        $_.Drawn -and $_.Text -and $_.W -gt 0 -and $_.H -gt 0 -and
        -not ([double]::IsNaN($sum) -or [double]::IsInfinity($sum))
    })
    Write-Verbose "Find-Widget: label='$Label' candidates=$($candidates.Count) of $($Tree.Nodes.Count)"

    $hits = @($candidates | Where-Object { $_.Text -eq $Label })
    if (-not $hits.Count) { $hits = @($candidates | Where-Object { $_.Text -like "*$Label*" }) }
    Write-Verbose "Find-Widget: hits=$($hits.Count)"
    # Unrolled on purpose: callers wrap the result in @(...), which restores the
    # array. Comma-wrapping here would hand them ONE element holding the whole
    # array -- a fake hit at (0,0) when nothing matched.
    return $hits
}

function Add-ClickPoint($Tree, $Nodes) {
    # Emits each node rather than returning $Nodes: no match arrives here as
    # $null, and 'return $null' would still write one $null to the pipeline.
    foreach ($n in $Nodes) {
        $n | Add-Member -NotePropertyName CursorX -NotePropertyValue (($n.X + $n.W / 2) / $Tree.Aspect) -Force
        $n | Add-Member -NotePropertyName CursorY -NotePropertyValue ($n.Y + $n.H / 2) -Force
        $n
    }
}

# ------------------------------------------------------------ commands ----

function Invoke-Start {
    if (-not (Test-Path $Exe)) {
        throw "Game binary not found: $Exe`nBuild it first with src/build.sh under WSL."
    }

    $existing = Get-HarnessProcess
    if ($existing) {
        Write-Host "Harness already running (PID $($existing.Id)). Use 'stop' first to restart." -ForegroundColor Yellow
        return
    }

    New-Item -ItemType Directory -Force -Path $CmdDir, $ShotDir | Out-Null

    # Both sides count from zero, so clear any batches from a previous run.
    Get-ChildItem (Join-Path $CmdDir 'cmd_*.cfg') -ErrorAction SilentlyContinue | Remove-Item -Force
    Remove-Item $LogFile -Force -ErrorAction SilentlyContinue

    # Each argument that can contain a space must arrive at the game as ONE
    # quoted token: Start-Process joins ArgumentList with spaces and quotes
    # nothing. The repo path normally contains a space ("Red Eclipse"), so an
    # unquoted -h silently becomes a truncated home dir.
    $gameArgs = @(
        "`"-h$HomeDir`""
        '-df0'
        "-dw$Width"
        "-dh$Height"
        '"-xexec tools/harness/boot.cfg"'
    )

    # Opt into the engine's crash logging (src/engine/main.cpp, installcrashlog).
    # Without it a failed assert raises a modal dialog that blocks the process,
    # so the harness sees a timeout instead of a diagnosis. With it, the message
    # and a symbolised backtrace go to log.txt and a .dmp lands in the home dir.
    # Start-Process passes the current environment through to the child, so set
    # it only around the launch and put the caller's value back afterwards --
    # otherwise it leaks into the calling shell and every game started from it.
    $oldCrashLog = $env:RE_CRASHLOG
    $env:RE_CRASHLOG = '1'
    try {
        $proc = Start-Process -FilePath $Exe -ArgumentList $gameArgs -WorkingDirectory $RepoRoot -PassThru
    }
    finally {
        if ($null -eq $oldCrashLog) { Remove-Item Env:RE_CRASHLOG -ErrorAction SilentlyContinue }
        else { $env:RE_CRASHLOG = $oldCrashLog }
    }
    Set-Content -Path $PidFile -Value $proc.Id -Encoding ascii

    Write-Host "Starting Red Eclipse (PID $($proc.Id), ${Width}x${Height})..." -ForegroundColor Cyan

    $deadline = (Get-Date).AddSeconds(90)
    while ((Get-Date) -lt $deadline) {
        if ($proc.HasExited) { throw "Game exited during startup (code $($proc.ExitCode)).`n$(Read-LogSafe)" }
        if ((Test-Path $LogFile) -and (Read-LogSafe).Contains('HARNESS_READY')) { break }
        Start-Sleep -Milliseconds 500
    }

    if (-not (Read-LogSafe).Contains('HARNESS_READY')) {
        throw "Harness did not come up within 90s. Log:`n$(Read-LogSafe)"
    }

    # Keep the boot diagnostics: the first batch's clearlog wipes the log.
    Copy-Item $LogFile $BootLog -Force -ErrorAction SilentlyContinue

    if (-not $Focus) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { Set-WindowNoActivate $proc.MainWindowHandle }
    }

    Write-Host "Ready. Home: $HomeDir" -ForegroundColor Green
    Write-Host 'Window is visible but unfocused; do not minimize it (screenshots go black).'
}

function Invoke-Stop {
    $p = Get-HarnessProcess
    if (-not $p) {
        Write-Host 'Harness is not running.'
        Remove-Item $PidFile -Force -ErrorAction SilentlyContinue
        return
    }
    Stop-Process -Id $p.Id -Force
    Remove-Item $PidFile -Force -ErrorAction SilentlyContinue
    Write-Host "Stopped PID $($p.Id)." -ForegroundColor Green
}

function Invoke-Status {
    $p = Get-HarnessProcess
    if ($p) {
        Write-Host "running   PID $($p.Id)" -ForegroundColor Green
        Write-Host "home      $HomeDir"
        Write-Host "batches   $(Get-NextSeq) sent"
        Write-Host "shots     $ShotDir"
    }
    else {
        Write-Host 'not running' -ForegroundColor Yellow
        Write-Host 'start with: tools\harness\harness.ps1 start'
    }
}

# --------------------------------------------------------------- main -----

switch ($Command) {

    'start'  { Invoke-Start }
    'stop'   { Invoke-Stop }
    'status' { Invoke-Status }

    'send' {
        $script = if ($File) { Get-Content -Raw -Path $File } else { ($Rest -join ' ') }
        if (-not $script) { throw 'Nothing to send. Pass CubeScript as an argument, or use -File <path>.' }
        $settleMs = if ($Settle -ge 0) { $Settle } else { 1 }
        Show-BatchResult (Invoke-Batch $script $settleMs $TimeoutSec)
    }

    'nav' {
        $panel = ($Rest -join ' ').Trim()
        if (-not $panel) { throw 'Usage: harness.ps1 nav ui_gameui_settings_graphics' }
        $settleMs = if ($Settle -ge 0) { $Settle } else { 400 }
        Show-BatchResult (Invoke-Batch "gameui_open $panel" $settleMs $TimeoutSec)
    }

    'reload' {
        $cfg = ($Rest -join ' ').Trim()
        if (-not $cfg) { throw 'Usage: harness.ps1 reload config/ui/game/settings.cfg' }
        $cfg = $cfg -replace '\\', '/'
        if (-not (Test-Path (Join-Path $RepoRoot $cfg))) { Write-Warning "No such file under repo root: $cfg" }
        $settleMs = if ($Settle -ge 0) { $Settle } else { 200 }
        Show-BatchResult (Invoke-Batch "exec ""$cfg"" 0 0" $settleMs $TimeoutSec)
    }

    'shot' {
        $settleMs = if ($Settle -ge 0) { $Settle } else { 300 }
        Write-Output (Invoke-Shot ($Rest -join '_').Trim() $settleMs $TimeoutSec)
    }

    'tree' {
        $tree = Get-UiTree
        $nodes = $tree.Nodes
        if ($Drawn) { $nodes = @($nodes | Where-Object { $_.Drawn }) }
        if ($Text)  { $nodes = @($nodes | Where-Object { $_.Text }) }
        Write-Host ("aspect {0}, {1} objects" -f (Format-Coord $tree.Aspect), $tree.Nodes.Count) -ForegroundColor Cyan
        Add-ClickPoint $tree $nodes | ForEach-Object {
            '{0}{1} {2} [{3} {4} {5} {6}] click({7},{8}) {9}' -f `
                (' ' * $_.Depth), $_.Type, $(if ($_.Drawn) { 'drawn' } else { 'hidden' }),
                (Format-Coord $_.X), (Format-Coord $_.Y), (Format-Coord $_.W), (Format-Coord $_.H),
                (Format-Coord $_.CursorX), (Format-Coord $_.CursorY), $_.Text
        }
    }

    'find' {
        $label = ($Rest -join ' ').Trim()
        if (-not $label) { throw 'Usage: harness.ps1 find Apply' }
        $tree = Get-UiTree
        $hits = @(Add-ClickPoint $tree (Find-Widget $tree $label))
        if (-not $hits.Count) { Write-Host "No drawn widget matching '$label'." -ForegroundColor Yellow; return }
        $hits | ForEach-Object {
            '{0} [{1} {2} {3} {4}] click({5},{6}) {7}' -f `
                $_.Type, (Format-Coord $_.X), (Format-Coord $_.Y), (Format-Coord $_.W), (Format-Coord $_.H),
                (Format-Coord $_.CursorX), (Format-Coord $_.CursorY), $_.Text
        }
    }

    'click' {
        $label = ($Rest -join ' ').Trim()
        if (-not $label) { throw 'Usage: harness.ps1 click Back' }

        $tree = Get-UiTree
        $hits = @(Add-ClickPoint $tree (Find-Widget $tree $label))
        if (-not $hits.Count) { throw "No drawn widget matching '$label'. Try: harness.ps1 tree -Text" }
        if ($hits.Count -gt 1) {
            Write-Warning ("{0} widgets match '{1}'; clicking the first ({2}). Use 'find' to disambiguate." -f $hits.Count, $label, $hits[0].Text)
        }

        $n  = $hits[0]
        $cx = Format-Coord $n.CursorX
        $cy = Format-Coord $n.CursorY
        Write-Host ("clicking '{0}' at ({1},{2})" -f $n.Text, $cx, $cy) -ForegroundColor Cyan

        # Move first, let a frame register the hover, then press and release.
        $script = @"
uisetcursor $cx $cy
sleep 100 [
    uikeypress -1 1
    sleep 60 [ uikeypress -1 0 ]
]
"@
        $settleMs = if ($Settle -ge 0) { $Settle } else { 500 }
        Show-BatchResult (Invoke-Batch $script $settleMs $TimeoutSec)
    }

    'log' {
        if (Test-Path $LogFile) { Read-LogSafe } else { Write-Host 'No log yet.' }
    }
}
