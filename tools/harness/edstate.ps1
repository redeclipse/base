# Parser for the structured output of the engine's eddumpstate command.
# Pure function over lines -- no game required, so it is unit tested directly
# by tests/edstate.tests.ps1.

if (-not (Get-Command ConvertTo-InvariantDouble -ErrorAction SilentlyContinue)) {
    . (Join-Path $PSScriptRoot 'core.ps1')
}

function ConvertFrom-EdState([string[]]$Lines) {
    $result = [pscustomobject]@{
        Mode      = $null
        Cam       = $null
        WorldPos  = $null
        Cur       = $null
        Sel       = $null
        Ui        = $null
        Hover     = @()
        EntSel    = @()
        Malformed = @()
        Complete  = $false
        EntCount  = 0
    }

    $hover  = New-Object System.Collections.ArrayList
    $entsel = New-Object System.Collections.ArrayList

    foreach ($raw in $Lines) {
        if ($null -eq $raw) { continue }
        # Strip the "YYYY-MM-DD HH:MM.SS " stamp the log adds to every line.
        $t = ($raw -replace '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ', '').Trim()

        # Anything that is not one of our tags is ordinary log output.
        if ($t -notmatch '^(EDSTATE|EDENT)\s') { continue }

        $parsed = $true
        # Each clause's numeric conversions are wrapped in try/catch: the regex only
        # constrains shape (\S+ for floats, so any non-whitespace token matches), and
        # validity is decided by ConvertTo-InvariantDouble / [int] at the point of use.
        # A torn or garbage token (e.g. a log read landing mid-clearlog swap) makes
        # those throw -- FormatException from a non-numeric string, OverflowException
        # from an out-of-range integer -- and an uncaught throw here would take the
        # whole parser down instead of landing the line in .Malformed as documented.
        switch -Regex ($t) {
            '^EDSTATE mode (-?\d+) (-?\d+) (-?\d+) (-?\d+)$' {
                try {
                    $result.Mode = @{
                        EditMode  = [int]$Matches[1]
                        GridPower = [int]$Matches[2]
                        GridSize  = [int]$Matches[3]
                        Orient    = [int]$Matches[4]
                    }
                } catch { $parsed = $false }
            }
            '^EDSTATE cam (\S+) (\S+) (\S+) (\S+) (\S+)$' {
                try {
                    $result.Cam = @{
                        X     = ConvertTo-InvariantDouble $Matches[1]
                        Y     = ConvertTo-InvariantDouble $Matches[2]
                        Z     = ConvertTo-InvariantDouble $Matches[3]
                        Yaw   = ConvertTo-InvariantDouble $Matches[4]
                        Pitch = ConvertTo-InvariantDouble $Matches[5]
                    }
                } catch { $parsed = $false }
            }
            '^EDSTATE worldpos (\S+) (\S+) (\S+)$' {
                try {
                    $result.WorldPos = @{
                        X = ConvertTo-InvariantDouble $Matches[1]
                        Y = ConvertTo-InvariantDouble $Matches[2]
                        Z = ConvertTo-InvariantDouble $Matches[3]
                    }
                } catch { $parsed = $false }
            }
            '^EDSTATE cur (-?\d+) (-?\d+) (-?\d+) (-?\d+)$' {
                try {
                    $result.Cur = @{
                        X      = [int]$Matches[1]
                        Y      = [int]$Matches[2]
                        Z      = [int]$Matches[3]
                        Orient = [int]$Matches[4]
                    }
                } catch { $parsed = $false }
            }
            '^EDSTATE sel ((?:-?\d+ ){14})(-?\d+)$' {
                try {
                    $f = ($Matches[1] + $Matches[2]) -split '\s+' | ForEach-Object { [int]$_ }
                    $result.Sel = @{
                        OX = $f[0];  OY = $f[1];  OZ = $f[2]
                        SX = $f[3];  SY = $f[4];  SZ = $f[5]
                        Grid = $f[6]; Orient = $f[7]
                        CX = $f[8];  CY = $f[9];  CXS = $f[10]; CYS = $f[11]
                        Corner = $f[12]; Children = $f[13]; HaveSel = $f[14]
                    }
                } catch { $parsed = $false }
            }
            '^EDSTATE ui (-?\d+) (-?\d+)$' {
                try {
                    $result.Ui = @{
                        CursorLock  = [int]$Matches[1]
                        FreeCursor  = [int]$Matches[2]
                    }
                } catch { $parsed = $false }
            }
            '^EDENT (hover|sel) (-?\d+) (\S+) (\S+) (\S+) (\S+)$' {
                try {
                    $ent = @{
                        Idx  = [int]$Matches[2]
                        Type = $Matches[3]
                        X    = ConvertTo-InvariantDouble $Matches[4]
                        Y    = ConvertTo-InvariantDouble $Matches[5]
                        Z    = ConvertTo-InvariantDouble $Matches[6]
                    }
                    if ($Matches[1] -eq 'hover') { [void]$hover.Add($ent) } else { [void]$entsel.Add($ent) }
                } catch { $parsed = $false }
            }
            '^EDSTATE end (-?\d+)$' {
                try {
                    $n = [int]$Matches[1]
                    $result.Complete = $true
                    $result.EntCount = $n
                } catch { $parsed = $false }
            }
            default { $parsed = $false }
        }

        # A tagged line we could not read is a real problem -- surface it rather
        # than silently returning a half-populated state object.
        if (-not $parsed) { $result.Malformed += $t }
    }

    $result.Hover  = @($hover)
    $result.EntSel = @($entsel)
    return $result
}
