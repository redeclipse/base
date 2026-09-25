<#
.SYNOPSIS
    Shader equivalence harness: record shader corpora and compare them.

.DESCRIPTION
    record  Sweeps render settings in the running game and writes a corpus
            (every shader configuration: composed source, metadata, GL
            reflection) to home\uitest\shadercorpus\<Run>\.
    check   Records a candidate corpus from the current build and compares it
            with a baseline through the tier ladder: contract -> text ->
            SPIR-V -> pixel. Exits 1 on any FAIL or MISSING.
    diff    Shows why one configuration differs.

    See docs/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.

.EXAMPLE
    tools\harness\shaders.ps1 record
    tools\harness\shaders.ps1 check -Filter 'bump*'
    tools\harness\shaders.ps1 check -Sids s00 -NoMaps
    tools\harness\shaders.ps1 diff bumpworld -Sid s00
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet('record', 'check', 'diff')]
    [string]$Command,

    [Parameter(Position = 1)]
    [string]$Name,

    [string]$Run = 'baseline',
    [string[]]$Sids,
    [string[]]$Maps,
    [switch]$NoMaps,
    [string]$Map = 'atop',
    [string]$SweepFile,
    [string]$Filter = '*',
    [ValidateRange(0, 3)]
    [int]$MaxTier = 3,
    [int]$Seeds = 4,
    [string]$Sid = 's00',
    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'shadercorpus.ps1')

$CorpusRoot = Join-Path $HomeDir 'shadercorpus'
$HarnessPs  = Join-Path $PSScriptRoot 'harness.ps1'
$EditorPs   = Join-Path $PSScriptRoot 'editor.ps1'
if (-not $SweepFile) { $SweepFile = Join-Path $PSScriptRoot 'shader-sweep.txt' }

$script:Defaults = $null      # var -> default value text, for every swept var
$script:CurrentMap = $null
$script:CurrentPoint = $null

# ---------------------------------------------------------------- game ----

function Invoke-Checked([string]$Script, [int]$SettleMs, [int]$Timeout) {
    # Invoke-Batch already returns its array wrapped for a bare assignment
    # (core.ps1: 'return , @($lines)'); wrapping again with @() here would
    # nest a 1-element array around it, and -match on that nested element
    # silently array-joins with spaces instead of comparing per line.
    $lines = Invoke-Batch $Script $SettleMs $Timeout
    $bad = @($lines | Where-Object { $_ -match $ErrorPattern -or $_ -match 'valid range for' })
    if ($bad.Count) { throw "The game reported errors:`n$($bad -join "`n")" }
    return , $lines
}

function Get-VarDefaults([string[]]$Vars) {
    $defaults = [ordered]@{}
    if (-not $Vars.Count) { return $defaults }
    $script = ($Vars | ForEach-Object {
        "echo (concatword ""SWEEPVAR $_ "" (getvartype $_) "" "" (? (= (getvartype $_) 1) (getfvardef $_ 0) (getvardef $_ 0)))"
    }) -join "`n"
    foreach ($line in (Invoke-Checked $script 1 60)) {
        if ($line -match 'SWEEPVAR (\S+) (-?\d+) (\S+)') {
            if ($Matches[2] -ne '0' -and $Matches[2] -ne '1') { throw "Sweep var '$($Matches[1])' is not an int or float var (type $($Matches[2]))." }
            $defaults[$Matches[1]] = $Matches[3]
        }
    }
    foreach ($v in $Vars) { if (-not $defaults.Contains($v)) { throw "The game did not report sweep var '$v'." } }
    return $defaults
}

# Applies one settings vector on top of the defaults and regenerates shaders.
# The settle lets frames render, which is when the C++ setup paths call
# their generateshaders (AO, bilateral, deferred lights, volumetrics, AA).
function Set-SweepPoint($Point) {
    $lines = foreach ($v in $script:Defaults.Keys) {
        if ($Point.Settings.Contains($v)) { "$v $($Point.Settings[$v])" } else { "$v $($script:Defaults[$v])" }
    }
    $lines = @($lines) + 'resetshaders'
    $out = Invoke-Checked ($lines -join "`n") 2000 300
    # Vars carrying initwarning(..., INIT_LOAD, CHANGE_SHADERS) (msaa*,
    # gdepthstencil, gstencil, glineardepth, hdrgamma, gscalecubicsoft,
    # textsupersample -- see renderlights.cpp/rendertext.cpp) log
    # "Pending shader change: <desc>" (menus.cpp:25, addchange) the moment
    # the var's setter runs, whether or not engineready gates the print --
    # 'resetshaders' alone leaves the change queued/"Pending" rather than
    # applied, so the actual g-buffer/deferred-light setup stays on the old
    # value. Rather than hand-list the affected vars (a previous version of
    # this file did, and missed gscalecubicsoft -- s34 silently dumped
    # default-state shaders), detect the message itself and force the real
    # reload with a follow-up 'resetgl' whenever it appears. The var-set
    # lines run before 'resetshaders' in this same batch, so the message (if
    # any) is already in $out by the time we check.
    if (@($out | Where-Object { $_ -match 'Pending shader change:' }).Count) {
        $out = @($out) + @(Invoke-Checked 'resetgl' 3000 300)
    }
    $errors = @($out | Where-Object { $_ -match 'GLSL ERROR' }).Count
    if ($errors) { Write-Warning "$($Point.Id): $errors GLSL compile error(s); those shaders are recorded as invalid." }
    $script:CurrentPoint = $Point.Id
}

function Open-Map([string]$MapName) {
    # Loading a map while an entity is hovered trips an unguarded enthover
    # read (world.cpp:1426); entity editing off empties it. See editor-selftest.ps1.
    Invoke-Checked 'entediting 0' 300 60 | Out-Null
    & $EditorPs open $MapName 6>$null | Out-Null
    $script:CurrentMap = $MapName
    $script:CurrentPoint = $null
}

# Puts the game into the state a settings id was recorded in.
function Enter-Point([string]$PointId, $Points) {
    if ($PointId.StartsWith('m-')) {
        $mapName = $PointId.Substring(2)
        if ($script:CurrentPoint -cne 'defaults') { Set-SweepPoint ([pscustomobject]@{ Id = 'defaults'; Settings = [ordered]@{} }) }
        if ($script:CurrentMap -cne $mapName) { Open-Map $mapName; $script:CurrentPoint = 'defaults' }
        return
    }
    if ($script:CurrentMap -cne $Map) { Open-Map $Map }
    if ($script:CurrentPoint -cne $PointId) {
        $p = @($Points | Where-Object { $_.Id -ceq $PointId })
        if (-not $p.Count) { throw "Unknown settings id '$PointId'." }
        Set-SweepPoint $p[0]
    }
}

function Invoke-Dump([string]$RunName, [string]$PointId, [bool]$MapsOnly) {
    $flag = 0
    if ($MapsOnly) { $flag = 1 }
    $out = Invoke-Checked "shaderdumpall $RunName $PointId $flag" 1 600
    $hit = @($out | Where-Object { $_ -match "SHADERDUMP $([regex]::Escape($RunName)) $([regex]::Escape($PointId)) (\d+) (\d+)" })
    if (-not $hit.Count) { throw "shaderdumpall did not report for ${PointId}:`n$($out -join "`n")" }
    $errors = @($out | Where-Object { $_ -match 'GLSL ERROR' }).Count
    if ($errors) { Write-Warning "${PointId}: $errors GLSL compile error(s) while forcing; those shaders are recorded as invalid." }
    Write-Host ("  {0,-14} {1}" -f $PointId, ($hit[0] -replace '^.*SHADERDUMP \S+ \S+ ', 'rows/new blobs: '))
}

function Write-Registry([string]$RunName) {
    $script = 'writetofile "shadercorpus/RUN/registry.txt" (concatword (looplistconcatword e $worldshaders [concatword "world " (at $e 0) " " (at $e 1) "^n"]) (looplistconcatword e $decalshaders [concatword "decal " (at $e 0) " " (at $e 1) "^n"]))'
    Invoke-Checked $script.Replace('RUN', $RunName) 1 60 | Out-Null
    $path = Join-Path $CorpusRoot "$RunName\registry.txt"
    if (-not (Test-Path $path)) { throw "registry.txt was not written: $path" }
}

function Get-ShippedMaps {
    return @(Get-ChildItem (Join-Path $RepoRoot 'data\maps\*.mpz') | ForEach-Object { $_.BaseName } | Sort-Object)
}

function Invoke-Record([string]$RunName, $Points, [string[]]$MapList) {
    if (-not (Get-HarnessProcess)) { & $HarnessPs start 6>$null | Out-Null }
    $runDir = Join-Path $CorpusRoot $RunName
    if (Test-Path $runDir) { Remove-Item -Recurse -Force $runDir }
    New-Item -ItemType Directory -Force (Join-Path $runDir 'settings') | Out-Null

    $script:Defaults = Get-VarDefaults (Get-SweepVars $Points)
    Write-Host "Recording '$RunName': $(@($Points).Count) settings point(s), $(@($MapList).Count) map(s)" -ForegroundColor Cyan

    foreach ($p in $Points) {
        Enter-Point $p.Id $Points
        Invoke-Dump $RunName $p.Id $false
        Write-TextNoBom (Join-Path $runDir "settings\$($p.Id).txt") ((@($p.Settings.Keys | ForEach-Object { "$_=$($p.Settings[$_])" }) -join "`n") + "`n")
    }
    Write-Registry $RunName
    foreach ($m in $MapList) {
        Enter-Point "m-$m" $Points
        Invoke-Dump $RunName "m-$m" $true
    }

    # Leave the persisted settings at their defaults.
    Set-SweepPoint ([pscustomobject]@{ Id = 'defaults'; Settings = [ordered]@{} })

    $commit = (& git -C $RepoRoot rev-parse HEAD).Trim()
    $dirty = 0
    if (@(& git -C $RepoRoot status --porcelain -- config/glsl).Count) { $dirty = 1 }
    $info = @(
        "commit $commit"
        "glsldirty $dirty"
        "recorded $((Get-Date).ToString('s'))"
        "map $Map"
        "sweep $(@($Points | ForEach-Object { $_.Id }) -join ' ')"
        "maps $(@($MapList) -join ' ')"
    )
    Write-TextNoBom (Join-Path $runDir 'run.txt') (($info -join "`n") + "`n")

    # Self-checks.
    $rows = @(Read-Manifest $runDir)
    $ids = @($Points | ForEach-Object { $_.Id })
    $problems = 0
    if ($ids -ccontains 's00' -and $ids -ccontains 's99') {
        # Find-SweepLeaks flags every name whose hash differs between s00 and
        # s99, including '(none)' on one side. A name that is '(none)' at s00
        # but present at s99 is not a reset failure: 'resetshaders'/'resetgl'
        # recompile existing Shader variants but never free ones an earlier
        # sweep point created (shader.cpp: cleanupshaders()/reloadshaders()
        # walk the existing registry, they don't purge it), so a var like
        # msaa that unlocks new variant rows (e.g. deferredlightM*) leaves
        # them permanently registered -- s99 legitimately dumps more rows
        # than s00 even though both are pure defaults. Only a name present
        # on BOTH sides with a DIFFERENT hash indicates the same shader
        # actually generating different content at nominally-equal settings,
        # which is the real "var missing from the reset list" leak.
        $allDiffs = @(Find-SweepLeaks $rows 's00' 's99')
        $leaks = @($allDiffs | Where-Object { $_.First -cne '(none)' -and $_.Last -cne '(none)' })
        $grown = @($allDiffs | Where-Object { $_.First -ceq '(none)' -or $_.Last -ceq '(none)' })
        foreach ($l in $leaks) { Write-Host "  LEAK  $($l.Name): s00 $($l.First) vs s99 $($l.Last) (origin $($l.Origin))" -ForegroundColor Red }
        if ($leaks.Count) { Write-Host '  State leaked between sweep points: a var these shaders read is missing from the reset list in shader-sweep.txt.' -ForegroundColor Red; $problems++ }
        else { Write-Host '  no state leaked between s00 and s99' -ForegroundColor Green }
        if ($grown.Count) { Write-Host "  ($($grown.Count) shader variant row(s) exist only at s99: permanent registrations left behind by an earlier sweep point, not a leak -- see comment above)" -ForegroundColor DarkYellow }
    }
    $registry = [System.IO.File]::ReadAllLines((Join-Path $runDir 'registry.txt'))
    $gaps = @(Test-PaletteCoverage $rows $registry $ids)
    foreach ($g in $gaps) { Write-Host "  PALETTE  $g has no valid shader" -ForegroundColor Red }
    if ($gaps.Count) { $problems++ } else { Write-Host "  all $($registry.Count) registered world/decal shaders valid at every point" -ForegroundColor Green }
    if ($dirty -and $RunName -ceq 'baseline') { Write-Warning 'config/glsl has uncommitted changes: this baseline is not a pinned commit.' }
    return $problems
}

# ---------------------------------------------------------------- main ----

switch ($Command) {
    'record' {
        $points = @(Read-Sweep $SweepFile)
        if ($Sids) { $points = @($points | Where-Object { $Sids -ccontains $_.Id }) }
        $mapList = @()
        if (-not $NoMaps) { if ($Maps) { $mapList = $Maps } else { $mapList = Get-ShippedMaps } }
        $problems = Invoke-Record $Run $points $mapList
        if ($problems) { exit 1 }
        exit 0
    }
    'check' { throw 'check is implemented in Task 8.' }
    'diff'  { throw 'diff is implemented in Task 8.' }
}
