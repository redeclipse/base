$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- paths ----

# core.ps1 sits in the same directory as its callers, so this resolves the same
# way harness.ps1's own $PSScriptRoot did.
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
        # A crashed game never writes the sentinel, so notice the exit now
        # rather than after the whole timeout. Re-read the log after the
        # process is gone: the crash handler's last lines land just before exit.
        if (-not (Get-HarnessProcess)) { throw "Game exited while running batch $n. Last log:`n$(Read-LogSafe)" }
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

$Invariant = [System.Globalization.CultureInfo]::InvariantCulture

function ConvertTo-InvariantDouble([string]$Value) {
    # The game always prints '.' decimals; a comma-decimal locale would
    # otherwise mis-parse every coordinate.
    return [double]::Parse($Value, $Invariant)
}

function Format-Coord([double]$Value) { return $Value.ToString('0.#####', $Invariant) }

function Set-WindowNoActivate([IntPtr]$Handle) {
    if (-not ([System.Management.Automation.PSTypeName]'ReHarnessWin').Type) {
        $sig = '[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);'
        Add-Type -Name 'ReHarnessWin' -Namespace '' -MemberDefinition $sig
    }
    # SW_SHOWNOACTIVATE (4): visible but not focused. Never minimize -- a
    # minimized window screenshots as solid black.
    [void][ReHarnessWin]::ShowWindow($Handle, 4)
}

function Invoke-Shot([string]$Name, [int]$SettleMs, [int]$Timeout) {
    if (-not $Name) { $Name = 'shot' }
    if ($Name -notmatch '^[A-Za-z0-9_.-]+$') { throw "Screenshot name must be [A-Za-z0-9_.-]+ (got '$Name')." }

    $target = Join-Path $ShotDir "$Name.png"
    Remove-Item $target -Force -ErrorAction SilentlyContinue

    # The back buffer must hold a rendered frame, so settle before shooting.
    $lines = Invoke-Batch "sleep $SettleMs [ screenshot ""harness/shots/$Name"" ]" ($SettleMs + 300) $Timeout
    Show-BatchResult $lines

    for ($i = 0; $i -lt 20 -and -not (Test-Path $target); $i++) { Start-Sleep -Milliseconds 100 }
    if (-not (Test-Path $target)) { throw "Screenshot was not written: $target" }
    return $target
}
