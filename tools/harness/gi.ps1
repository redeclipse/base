<#
.SYNOPSIS
    GI (radiance hints) stability checks, driven through the map editor.

.DESCRIPTION
    Needs a running harness client in edit mode on a map with sunlight and GI:
        tools\harness\harness.ps1 start -Width 1600 -Height 900
        tools\harness\editor.ps1 open park

    zoom    Animates edzoom (a DEBUG_UTILS stand-in for a weapon zoom) from
            $editfov down to -Fov and back over -Frames frames each way, and
            counts RH split resizes ($rhsplitresets). PASS when there are none.

    The view is made deterministic first: HUD, editor cursor, outlines and
    entity markers off, giscale raised to -GiScale so GI changes stand out.

.EXAMPLE
    tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('zoom')]
    [string]$Command = 'zoom',

    [double]$Fov = 30,
    [int]$Frames = 20,
    [double]$GiScale = 8,
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')

# ------------------------------------------------------------- helpers ----

# Evaluates a CubeScript expression and returns its value as a string.
function Get-Value([string]$Expr) {
    $lines = Invoke-Batch "echo (concatword ""GIVALUE="" $Expr)" 1 $TimeoutSec
    $value = $null
    foreach ($line in $lines) { if ($line -match 'GIVALUE=(\S*)') { $value = $Matches[1] } }
    if ($null -eq $value) { throw "No value for '$Expr'. Output:`n$($lines -join "`n")" }
    return $value
}

function Set-GiView {
    $lines = Invoke-Batch "showhud 0; editinhibit 1; outline 0; entediting 0; giscale $(Format-Coord $GiScale)" 200 $TimeoutSec
    Show-BatchResult $lines
    if ((Get-Value '$editing') -ne '1') { throw 'Not in edit mode: run tools\harness\editor.ps1 open <map> first.' }
}

# ------------------------------------------------------------ commands ----

switch ($Command) {

    'zoom' {
        Set-GiView
        $base = ConvertTo-InvariantDouble (Get-Value '$editfov')
        $before = [int](Get-Value '$rhsplitresets')

        $fovs = @()
        for ($i = 1; $i -le $Frames; $i++) { $fovs += $base + ($Fov - $base) * $i / $Frames }
        for ($i = $Frames - 1; $i -ge 0; $i--) { $fovs += $base + ($Fov - $base) * $i / $Frames }
        # One batch per step; the settle makes sure a frame renders at each fov.
        foreach ($f in $fovs) { Invoke-Batch "edzoom $(Format-Coord $f)" 50 $TimeoutSec | Out-Null }
        Invoke-Batch 'edzoom 0' 100 $TimeoutSec | Out-Null

        $after = [int](Get-Value '$rhsplitresets')
        $delta = $after - $before
        Write-Output ("ZOOM base={0} min={1} frames={2} rhsplitresets {3} -> {4} (+{5})" -f `
            (Format-Coord $base), (Format-Coord $Fov), $fovs.Count, $before, $after, $delta)
        if ($delta -eq 0) { Write-Output 'PASS'; exit 0 }
        Write-Output 'FAIL'; exit 1
    }
}
