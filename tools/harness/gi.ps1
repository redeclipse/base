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
    sweep   Probes a lattice of world points (rhprobe: the real getrhlight)
            while the camera moves -Step units per step along its facing
            (-Kind translate; choose -Yaw 0/90/180/270) or turns -Step
            degrees (-Kind rotate). Always runs once at rhblend 0; with
            -Blend > 0, again at that rhblend. probestats.py then reports J,
            the largest per-step change at any fixed point. Reference only:
            DETECTED when today's lookup pops. With -Blend: PASS when the
            crossfade cuts J to a quarter or less.
    near    Probes a fine lattice around the camera at rhblend 0 and at
            -Blend (required, > 0). PASS when the points within -Radius
            agree to 2/255.

    Lattice: points every -Spacing units over +-Extent around (-X, -Y) at
    heights -Z - Spacing, -Z and -Z + Spacing, normal up. Defaults: sweep
    600/40, near 40/8. Files go to home\uitest\harness\ (rhprobe-points.txt,
    <name>_<kind>_b<blend>.tsv).

    The view is made deterministic first: HUD, editor cursor, outlines and
    entity markers off, giscale raised to -GiScale so GI changes stand out.

.EXAMPLE
    tools\harness\gi.ps1 zoom -Fov 30 -Frames 20
    tools\harness\gi.ps1 sweep -Kind translate -X 700 -Y 1000 -Z 1120 -Yaw 90 -Name t
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('zoom', 'sweep', 'near')]
    [string]$Command = 'zoom',

    [double]$Fov = 30,
    [int]$Frames = 20,

    [ValidateSet('translate', 'rotate')]
    [string]$Kind = 'translate',
    [double]$X,
    [double]$Y,
    [double]$Z,
    [double]$Yaw = 0,
    [int]$Steps = 72,
    [double]$Step = 1,
    [double]$Blend = 0,
    [double]$Extent = -1,
    [double]$Spacing = -1,
    [double]$Radius = 30,
    [string]$Name = 'gi',
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

$ProbeStats = Join-Path $PSScriptRoot 'probestats.py'
$PointsRel  = 'harness/rhprobe-points.txt'
$OutRel     = 'harness/rhprobe-out.txt'

$ScriptArgs = $PSBoundParameters
function Assert-Pose {
    foreach ($p in 'X', 'Y', 'Z') {
        if (-not $ScriptArgs.ContainsKey($p)) {
            throw "-$p is required (read a start pose with tools\harness\editor.ps1 state)."
        }
    }
}

function Set-Blend([double]$Value) {
    $exists = Get-Value '(identexists rhblend)'
    if ($exists -ne '1') {
        if ($Value -gt 0) { throw 'rhblend does not exist in this build.' }
        return
    }
    Show-BatchResult (Invoke-Batch "rhblend $(Format-Coord $Value)" 300 $TimeoutSec)
}

function Write-Lattice([double]$Ext, [double]$Gap) {
    $lines = New-Object System.Collections.Generic.List[string]
    $n = [int][Math]::Floor($Ext / $Gap)
    foreach ($k in -1, 0, 1) {
        for ($i = -$n; $i -le $n; $i++) {
            for ($j = -$n; $j -le $n; $j++) {
                $lines.Add(('{0} {1} {2} 0 0 1' -f (Format-Coord ($X + $i * $Gap)), (Format-Coord ($Y + $j * $Gap)),
                    (Format-Coord ($Z + $k * $Gap))))
            }
        }
    }
    $path = Join-Path $HomeDir $PointsRel
    Write-TextNoBom $path (($lines -join "`n") + "`n")
    return $path
}

# Places the camera, lets a frame render, probes, and appends the step's
# S and P lines to $Tsv.
function Invoke-ProbeStep([int]$Index, [double]$PX, [double]$PY, [double]$PZ, [double]$PYaw, [string]$Tsv, [int]$Count) {
    Invoke-Batch ('edgoto {0} {1} {2}; edaim {3} 0' -f (Format-Coord $PX), (Format-Coord $PY), (Format-Coord $PZ),
        (Format-Coord $PYaw)) 100 $TimeoutSec | Out-Null
    $got = Get-Value "(rhprobe ""$PointsRel"" ""$OutRel"")"
    if ([int]$got -ne $Count) { throw "rhprobe returned $got, expected $Count (GI off, or not rendered yet?)" }
    $out = Get-Content (Join-Path $HomeDir $OutRel)
    $rows = foreach ($line in $out) {
        $f = $line -split ' '
        if ($f[0] -eq 'RHSPLIT') { "S`t$Index`t" + ($f[1..5] -join "`t") }
        elseif ($f[0] -eq 'RHPROBE') { "P`t$Index`t" + ($f[1..5] -join "`t") }
    }
    Add-Content -Path $Tsv -Value $rows -Encoding ascii
}

function Invoke-ProbeRun([double]$B, [string]$Tag, [int]$Count, [scriptblock]$PoseAt, [int]$StepCount) {
    Set-Blend $B
    $tsv = Join-Path $CmdDir ('{0}_{1}_b{2}.tsv' -f $Name, $Tag, (Format-Coord $B))
    Remove-Item $tsv -Force -ErrorAction SilentlyContinue
    for ($i = 0; $i -le $StepCount; $i++) {
        $p = & $PoseAt $i
        Invoke-ProbeStep $i $p[0] $p[1] $p[2] $p[3] $tsv $Count
    }
    return $tsv
}

# ------------------------------------------------------------ commands ----

switch ($Command) {

    'zoom' {
        if ($Frames -lt 1) { throw '-Frames must be at least 1 (with no frames there is no zoom to check).' }
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

    'sweep' {
        Assert-Pose
        Set-GiView
        $ext = if ($Extent -gt 0) { $Extent } else { 600 }
        $gap = if ($Spacing -gt 0) { $Spacing } else { 40 }
        $points = Write-Lattice $ext $gap
        $count = @(Get-Content $points).Count
        $grid = Get-Value '$rhgrid'
        # The engine's facing (vec(yaw, 0), src/shared/geom.h): x = -sin(yaw), y = cos(yaw).
        $rad = [Math]::PI / 180
        $dx = -[Math]::Sin($Yaw * $rad)
        $dy = [Math]::Cos($Yaw * $rad)
        $poseAt = if ($Kind -eq 'translate') {
            { param($i) @(($X + $dx * $i * $Step), ($Y + $dy * $i * $Step), $Z, $Yaw) }
        } else {
            { param($i) @($X, $Y, $Z, ($Yaw + $i * $Step)) }
        }
        $ref = Invoke-ProbeRun 0 $Kind $count $poseAt $Steps
        $runs = @($ref)
        if ($Blend -gt 0) { $runs += Invoke-ProbeRun $Blend $Kind $count $poseAt $Steps; Set-Blend 0 }
        & python $ProbeStats sweep $points $grid @runs
        exit $LASTEXITCODE
    }

    'near' {
        if ($Blend -le 0) { throw '-Blend must be > 0 for near (rhblend 0 against itself always agrees).' }
        Assert-Pose
        Set-GiView
        $ext = if ($Extent -gt 0) { $Extent } else { 40 }
        $gap = if ($Spacing -gt 0) { $Spacing } else { 8 }
        $points = Write-Lattice $ext $gap
        $count = @(Get-Content $points).Count
        $poseAt = { param($i) @($X, $Y, $Z, $Yaw) }
        $ref = Invoke-ProbeRun 0 'near' $count $poseAt 0
        $cand = Invoke-ProbeRun $Blend 'near' $count $poseAt 0
        Set-Blend 0
        & python $ProbeStats near $points (Format-Coord $Radius) (Format-Coord $X) (Format-Coord $Y) (Format-Coord $Z) $ref $cand
        exit $LASTEXITCODE
    }
}
