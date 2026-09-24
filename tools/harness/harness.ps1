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

# ---------------------------------------------------------------- paths ----

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Exe      = Join-Path $RepoRoot 'bin\amd64\redeclipse.exe'
$HomeDir  = Join-Path $RepoRoot 'home\uitest'
$CmdDir   = Join-Path $HomeDir  'harness'
$ShotDir  = Join-Path $CmdDir   'shots'
$LogFile  = Join-Path $HomeDir  'log.txt'
$PidFile  = Join-Path $CmdDir   'pid.txt'
$BootLog  = Join-Path $CmdDir   'boot-log.txt'

# Errors the game reports for bad script; surfaced after each batch.
$ErrorPattern = 'Unknown command:|Unknown alias lookup:|Unknown variable|Could not read|Cannot find'

# ------------------------------------------------------------- helpers ----

function Write-TextNoBom([string]$Path, [string]$Text) {
    # CubeScript parses a UTF-8 BOM as part of the first token, so it must not
    # be written. Out-File/Set-Content -Encoding utf8 emit one on PS 5.1.
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding $false))
}

function Read-LogSafe {
    # The game holds log.txt open for writing (fflush'd per line, so content is
    # current). It MUST therefore be opened with FileShare::ReadWrite --
    # File.ReadAllText uses FileShare::Read and fails with a sharing violation.
    # 'clearlog' also truncates and reopens it, so a read can land mid-swap.
    for ($i = 0; $i -lt 5; $i++) {
        try {
            $fs = New-Object System.IO.FileStream($LogFile, 'Open', 'Read', 'ReadWrite')
            try {
                $sr = New-Object System.IO.StreamReader($fs, [System.Text.Encoding]::UTF8)
                return $sr.ReadToEnd()
            }
            finally { $fs.Dispose() }
        }
        catch { Start-Sleep -Milliseconds 60 }
    }
    return ''
}

function Get-HarnessProcess {
    if (-not (Test-Path $PidFile)) { return $null }
    $harnessPid = (Get-Content $PidFile -Raw).Trim()
    if (-not $harnessPid) { return $null }
    return Get-Process -Id ([int]$harnessPid) -ErrorAction SilentlyContinue
}

function Assert-Running {
    $p = Get-HarnessProcess
    if (-not $p) { throw "Harness is not running. Start it with: tools\harness\harness.ps1 start" }
    return $p
}

function Get-NextSeq {
    $existing = Get-ChildItem (Join-Path $CmdDir 'cmd_*.cfg') -ErrorAction SilentlyContinue
    if (-not $existing) { return 0 }
    $max = ($existing | ForEach-Object {
        if ($_.BaseName -match '^cmd_(\d+)$') { [int]$Matches[1] } else { -1 }
    } | Measure-Object -Maximum).Maximum
    return $max + 1
}

function Invoke-Batch([string]$Script, [int]$SettleMs, [int]$Timeout) {
    Assert-Running | Out-Null

    $n = Get-NextSeq
    $sentinel = "HARNESS_END $n"

    # clearlog truncates the log, so what remains afterwards is exactly this
    # batch's output -- no byte-offset bookkeeping on this side.
    # The sentinel goes inside a sleep so at least one frame has been rendered
    # before the batch is considered finished (screenshots need this).
    $body = @(
        'clearlog'
        $Script
        "sleep $([Math]::Max($SettleMs, 1)) [ echo ""$sentinel"" ]"
    ) -join "`n"

    Write-TextNoBom (Join-Path $CmdDir "cmd_$n.cfg") ($body + "`n")

    $deadline = (Get-Date).AddSeconds($Timeout)
    while ((Get-Date) -lt $deadline) {
        $text = Read-LogSafe
        if ($text -and $text.Contains($sentinel)) {
            $lines = $text -split "`r?`n" | Where-Object { $_ -ne '' -and $_ -notmatch [regex]::Escape($sentinel) }
            return , @($lines)
        }
        Start-Sleep -Milliseconds 100
    }

    if (-not (Get-HarnessProcess)) { throw "Game exited while running batch $n. Last log:`n$(Read-LogSafe)" }
    throw "Timed out after ${Timeout}s waiting for batch $n. Last log:`n$(Read-LogSafe)"
}

function Show-BatchResult([string[]]$Lines) {
    # Strip the "YYYY-MM-DD HH:MM.SS " stamp the log adds to every line.
    $clean = $Lines | ForEach-Object { $_ -replace '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ', '' }

    # UI scripts are re-evaluated every frame, so one bad alias emits the same
    # line (and its call stack) dozens of times per batch. Collapse repeats,
    # keeping first-seen order, or the real output is buried. UITREE lines are
    # structural -- identical siblings are meaningful, so never collapse those.
    $counts = [ordered]@{}
    foreach ($line in $clean) {
        if ($line -like 'UITREE *') { Write-Output $line; continue }
        if ($counts.Contains($line)) { $counts[$line] = $counts[$line] + 1 }
        else { $counts[$line] = 1 }
    }

    foreach ($entry in $counts.GetEnumerator()) {
        if ($entry.Value -gt 1) { Write-Output ('{0}   (x{1})' -f $entry.Key, $entry.Value) }
        else { Write-Output $entry.Key }
    }

    $errors = @($counts.Keys | Where-Object { $_ -match $ErrorPattern })
    if ($errors.Count) {
        Write-Warning 'Script errors reported by the game:'
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# ------------------------------------------------------- widget queries ----
#
# uidumptree reports object rects with x in ASPECT space (0 .. hudw/hudh) while
# the cursor is in screen fractions (0..1 across the full width), so clicking a
# widget means dividing its x by the aspect ratio. Verified by hovering a known
# button and confirming it highlights.

$Invariant = [System.Globalization.CultureInfo]::InvariantCulture

function ConvertTo-InvariantDouble([string]$Value) {
    # The game always prints '.' decimals; a comma-decimal locale would
    # otherwise mis-parse every coordinate.
    return [double]::Parse($Value, $Invariant)
}

function Format-Coord([double]$Value) { return $Value.ToString('0.#####', $Invariant) }

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
                X     = ConvertTo-InvariantDouble $Matches[4]
                Y     = ConvertTo-InvariantDouble $Matches[5]
                W     = ConvertTo-InvariantDouble $Matches[6]
                H     = ConvertTo-InvariantDouble $Matches[7]
                Tag   = $Matches[8]
                Text  = $Matches[9].Trim()
            })
        }
    }

    return [pscustomobject]@{ Aspect = $aspect; Nodes = $nodes }
}

function Find-Widget($Tree, [string]$Label) {
    $candidates = @($Tree.Nodes | Where-Object { $_.Drawn -and $_.Text -and $_.W -gt 0 -and $_.H -gt 0 })
    Write-Verbose "Find-Widget: label='$Label' candidates=$($candidates.Count) of $($Tree.Nodes.Count)"

    $hits = @($candidates | Where-Object { $_.Text -eq $Label })
    if (-not $hits.Count) { $hits = @($candidates | Where-Object { $_.Text -like "*$Label*" }) }
    Write-Verbose "Find-Widget: hits=$($hits.Count)"
    return , $hits
}

function Add-ClickPoint($Tree, $Nodes) {
    foreach ($n in $Nodes) {
        $n | Add-Member -NotePropertyName CursorX -NotePropertyValue (($n.X + $n.W / 2) / $Tree.Aspect) -Force
        $n | Add-Member -NotePropertyName CursorY -NotePropertyValue ($n.Y + $n.H / 2) -Force
    }
    # Comma-wrapped: returning a one-element array would otherwise unroll to a
    # scalar at the call site and lose .Count.
    return , $Nodes
}

function Set-WindowNoActivate([IntPtr]$Handle) {
    if (-not ([System.Management.Automation.PSTypeName]'ReHarnessWin').Type) {
        $sig = '[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);'
        Add-Type -Name 'ReHarnessWin' -Namespace '' -MemberDefinition $sig
    }
    # SW_SHOWNOACTIVATE (4): visible but not focused. Never minimize -- a
    # minimized window screenshots as solid black.
    [void][ReHarnessWin]::ShowWindow($Handle, 4)
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

    $proc = Start-Process -FilePath $Exe -ArgumentList $gameArgs -WorkingDirectory $RepoRoot -PassThru
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
        $name = ($Rest -join '_').Trim()
        if (-not $name) { $name = 'shot' }
        if ($name -notmatch '^[A-Za-z0-9_.-]+$') { throw "Screenshot name must be [A-Za-z0-9_.-]+ (got '$name')." }

        $target = Join-Path $ShotDir "$name.png"
        Remove-Item $target -Force -ErrorAction SilentlyContinue

        # The back buffer must hold a rendered frame, so settle before shooting.
        $settleMs = if ($Settle -ge 0) { $Settle } else { 300 }
        $lines = Invoke-Batch "sleep $settleMs [ screenshot ""harness/shots/$name"" ]" ($settleMs + 300) $TimeoutSec
        Show-BatchResult $lines

        for ($i = 0; $i -lt 20 -and -not (Test-Path $target); $i++) { Start-Sleep -Milliseconds 100 }
        if (-not (Test-Path $target)) { throw "Screenshot was not written: $target" }
        Write-Output $target
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
