<#
.SYNOPSIS
    Shader equivalence harness: record shader corpora and compare them.

.DESCRIPTION
    record  Sweeps render settings in the running game and writes a corpus
            (every shader configuration: composed source, metadata, GL
            reflection) to home\uitest\shadercorpus\<Run>\.
    check   Records a candidate corpus from the current build and compares it
            with a baseline through the tier ladder: contract -> text ->
            SPIR-V -> pixel. Exits 1 on any FAIL or MISSING. Refuses a
            baseline recorded on another GPU/driver unless -AllowCrossGpu,
            which skips reflection and pixels.
    diff    Shows why one configuration differs.

    See doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.

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
    [switch]$AllowCrossGpu,
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
        # Reset after every map load, not once before the first: generated
        # shaders (models, deferred lights) outlive a map change, so without it
        # a map's rows would also hold whatever earlier maps generated, and
        # `check -Sids m-<map>` would report those as MISSING.
        if ($script:CurrentMap -cne $mapName) { Open-Map $mapName }
        if ($script:CurrentPoint -cne $PointId) { Set-SweepPoint ([pscustomobject]@{ Id = $PointId; Settings = [ordered]@{} }) }
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
    $stuck = @($out | Where-Object { $_ -match 'forceallshaders: .*still deferred' })
    if ($stuck.Count) { Write-Warning "${PointId}: $($stuck[0] -replace '^.*forceallshaders: ', '')" }
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

# $VarPoints: the points whose vars every sweep point resets to their
# defaults. Pass the whole sweep even when recording a subset of it, or a var
# only an unselected point sets keeps whatever value the session last left.
function Invoke-Record([string]$RunName, $Points, [string[]]$MapList, $VarPoints) {
    if (-not $VarPoints) { $VarPoints = $Points }
    if (-not (Get-HarnessProcess)) { & $HarnessPs start 6>$null | Out-Null }
    $runDir = Join-Path $CorpusRoot $RunName
    if (Test-Path $runDir) { Remove-Item -Recurse -Force $runDir }
    New-Item -ItemType Directory -Force (Join-Path $runDir 'settings') | Out-Null

    # addchange() (menus.cpp) -- the source of the "Pending shader change:"
    # message Set-SweepPoint's resetgl trigger looks for -- returns early
    # when $applydialog is 0, so a session with it disabled would silently
    # never fire resetgl. Pin it on for the sweep.
    Invoke-Checked 'applydialog 1' 300 60 | Out-Null
    $script:Defaults = Get-VarDefaults (Get-SweepVars $VarPoints)
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
        # than s00 even though both are pure defaults. That excuse only runs
        # one way. A shader valid at s00 must still be valid at s99, with the
        # same hash: gone at s99 means the defaults no longer produce it, and
        # a different hash means it generates different content at
        # nominally-equal settings. Both are the real "var missing from the
        # reset list" leak.
        $allDiffs = @(Find-SweepLeaks $rows 's00' 's99')
        $leaks = @($allDiffs | Where-Object { Test-SweepLeak $_ })
        $grown = @($allDiffs | Where-Object { -not (Test-SweepLeak $_) })
        foreach ($l in $leaks) { Write-Host "  LEAK  $($l.Name): s00 $($l.First) vs s99 $($l.Last) (origin $($l.Origin))" -ForegroundColor Red }
        if ($leaks.Count) { Write-Host '  State leaked between sweep points: a var these shaders read is missing from the reset list in shader-sweep.txt.' -ForegroundColor Red; $problems++ }
        else { Write-Host '  no state leaked between s00 and s99' -ForegroundColor Green }
        if ($grown.Count) { Write-Host "  ($($grown.Count) shader row(s) exist only at s99: permanent registrations left behind by an earlier sweep point, not a leak -- see comment above)" -ForegroundColor DarkYellow }
    }
    $registry = [System.IO.File]::ReadAllLines((Join-Path $runDir 'registry.txt'))
    $gaps = @(Test-PaletteCoverage $rows $registry $ids)
    foreach ($g in $gaps) { Write-Host "  PALETTE  $g has no valid shader" -ForegroundColor Red }
    if ($gaps.Count) { $problems++ } else { Write-Host "  all $($registry.Count) registered world/decal shaders valid at every point" -ForegroundColor Green }
    if ($dirty -and $RunName -ceq 'baseline') { Write-Warning 'config/glsl has uncommitted changes: this baseline is not a pinned commit.' }
    return $problems
}

function Invoke-Wsl([string[]]$Arguments) {
    $out = @(& wsl -d Ubuntu --exec @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "wsl $($Arguments -join ' ') failed:`n$($out -join "`n")" }
    return , $out
}

# Tiers 1-2 for every PENDING result; returns index -> { Tier; Detail }.
function Invoke-OfflineTiers($Pending, [string]$BaseDir, [string]$CandDir) {
    $map = @{}
    if (-not @($Pending).Count) { return $map }
    $pairs = Join-Path $CorpusRoot 'pairs.tsv'
    $lines = for ($i = 0; $i -lt $Pending.Count; $i++) {
        $p = $Pending[$i]
        "$i`t$(ConvertTo-WslPath (Join-Path $BaseDir "blobs\$($p.BaseHash)"))`t$(ConvertTo-WslPath (Join-Path $CandDir "blobs\$($p.CandHash)"))"
    }
    Write-TextNoBom $pairs ((@($lines) -join "`n") + "`n")
    $out = Invoke-Wsl @('python3', (ConvertTo-WslPath (Join-Path $PSScriptRoot 'shadercheck.py')), '--pairs', (ConvertTo-WslPath $pairs))
    foreach ($line in $out) {
        $f = $line -split "`t", 3
        if ($f.Count -ge 2 -and $f[0] -match '^\d+$') {
            $detail = ''
            if ($f.Count -gt 2) { $detail = $f[2] }
            $map[[int]$f[0]] = [pscustomobject]@{ Tier = $f[1]; Detail = $detail }
        }
    }
    return $map
}

# Tier 3 for the queued results, grouped by settings point; returns
# "<sid><TAB><name>" -> { Status; Detail }.
function Invoke-Benches($Queue, [string]$BaseRun, $Points) {
    $results = New-OrdinalMap
    foreach ($group in @($Queue | Group-Object Sid)) {
        Enter-Point $group.Name $Points
        Invoke-Checked 'shaderforceall' 1 300 | Out-Null
        $items = @($group.Group)
        for ($i = 0; $i -lt $items.Count; $i += 20) {
            $chunk = @($items[$i..([Math]::Min($i + 19, $items.Count - 1))])
            $script = ($chunk | ForEach-Object { "shaderbench $BaseRun $($_.BaseHash) ""$($_.Name)"" $Seeds" }) -join "`n"
            foreach ($line in (Invoke-Batch $script 1 600)) {
                $b = ConvertFrom-BenchLine $line
                if ($b) { $results["$($group.Name)`t$($b.Name)"] = [pscustomobject]@{ Status = $b.Status; Detail = $b.Detail } }
            }
        }
    }
    return $results
}

# The game's GL identity, in gl.txt's format (shaderharness.cpp:
# writeglinfo), so a cross-GPU check is refused before the candidate sweep
# rather than after it.
function Get-GameGlText {
    $probe = Join-Path $CorpusRoot 'glprobe.txt'
    if (Test-Path $probe) { Remove-Item -Force $probe }
    Invoke-Checked 'writetofile "shadercorpus/glprobe.txt" (concatword "vendor " $gfxvendor "^n" "renderer " $gfxrenderer "^n" "version " $gfxversion "^n" "glslversion " $glslversion "^n")' 1 60 | Out-Null
    if (-not (Test-Path $probe)) { throw "The GL probe was not written: $probe" }
    # writetofile opens in text mode, so undo a CRLF translation.
    return [System.IO.File]::ReadAllText($probe).Replace("`r`n", "`n")
}

function Invoke-Check {
    # 'candidate' is the scratch corpus check records into: checking against
    # it would delete the baseline and compare it with itself. The corpus
    # directory names are case-insensitive on Windows, so any spelling is refused.
    if ($Run -eq 'candidate') { throw "check -Run candidate is refused: 'candidate' is the scratch corpus check records into, so it would overwrite the baseline and compare it with itself. Record the baseline under another name." }
    $baseDir = Join-Path $CorpusRoot $Run
    $candDir = Join-Path $CorpusRoot 'candidate'
    if (-not (Test-Path (Join-Path $baseDir 'manifest.tsv'))) { throw "No baseline corpus at $baseDir. Record one first: tools\harness\shaders.ps1 record" }

    # Replay exactly what the baseline recorded, not the current sweep file.
    $info = Read-RunInfo $baseDir
    $baseGl = [System.IO.File]::ReadAllText((Join-Path $baseDir 'gl.txt'))
    if (-not (Get-HarnessProcess)) { & $HarnessPs start 6>$null | Out-Null }
    $sameGpu = Test-SameGpu $baseGl (Get-GameGlText) $info['commit'] -AllowCrossGpu:$AllowCrossGpu

    $allPoints = @(Read-RunPoints $baseDir)
    $points = $allPoints
    if ($Sids) { $points = @($points | Where-Object { $Sids -ccontains $_.Id }) }
    $mapList = @()
    if (-not $NoMaps) {
        $mapList = @($info['maps'] -split ' ' | Where-Object { $_ })
        if ($Sids) { $mapList = @($mapList | Where-Object { $Sids -ccontains "m-$_" }) }
    }
    $script:Map = $info['map']
    # Reset the vars of every recorded point, not only the selected ones.
    $recordProblems = Invoke-Record 'candidate' $points $mapList $allPoints
    if ($recordProblems) { Write-Warning 'The candidate recording reported leaks or palette gaps (above).' }

    $wanted = @($points | ForEach-Object { $_.Id }) + @($mapList | ForEach-Object { "m-$_" })
    # gl.txt is the authority; the probe above only saves a wasted sweep.
    $candGl = [System.IO.File]::ReadAllText((Join-Path $candDir 'gl.txt'))
    $sameGpu = Test-SameGpu $baseGl $candGl $info['commit'] -AllowCrossGpu:$AllowCrossGpu
    $maxTier = $MaxTier
    if (-not $sameGpu) {
        Write-Warning "The baseline was recorded on a different GPU/driver (-AllowCrossGpu): reflection and pixel tiers are skipped; the metadata contract, text and SPIR-V still run."
        $maxTier = [Math]::Min($maxTier, 2)
    }

    # The registry is compared whatever -Filter and -Sids say.
    $results = @(Compare-Registry $baseDir $candDir) + @(Compare-Corpus -BaseDir $baseDir -CandDir $candDir -Filter $Filter -Sids $wanted -SkipReflection:(-not $sameGpu))
    $pending = @($results | Where-Object { $_.Status -ceq 'PENDING' })
    $offline = @{}
    if ($maxTier -ge 1) { $offline = Invoke-OfflineTiers $pending $baseDir $candDir }
    $queue = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $pending.Count; $i++) {
        $r = $pending[$i]
        $o = $offline[$i]
        # Keep the checker's detail when it has one: it names a raw fallback.
        if ($o -and $o.Tier -ceq 'TEXT') { $r.Status = 'PASS-TEXT'; if ($o.Detail) { $r.Detail = $o.Detail } else { $r.Detail = 'same tokens after preprocessing' }; continue }
        if ($o -and $o.Tier -ceq 'SPIRV' -and $maxTier -ge 2) { $r.Status = 'PASS-SPIRV'; $r.Detail = $o.Detail; continue }
        if ($maxTier -ge 3) { $queue.Add($r); continue }
        $r.Status = 'FAIL'
        if ($o) { $r.Detail = "tier $($o.Tier): $($o.Detail)" }
        elseif ($maxTier -ge 1) { $r.Detail = 'no offline tier result reported for this pair' }
        else { $r.Detail = 'content differs (text tiers disabled)' }
    }
    if ($queue.Count) {
        $bench = Invoke-Benches $queue $Run $points
        foreach ($r in $queue) {
            $b = $bench["$($r.Sid)`t$($r.Name)"]
            if ($b) { $r.Status = $b.Status; $r.Detail = $b.Detail }
            else { $r.Status = 'FAIL'; $r.Detail = 'bench did not report' }
        }
    }
    if (-not $sameGpu) { Add-CrossGpuNote $results }

    $order = @{ 'FAIL' = 0; 'MISSING' = 1; 'WEAK' = 2; 'EXTRA' = 3; 'PASS-PIXEL' = 4; 'PASS-SPIRV' = 5; 'PASS-TEXT' = 6 }
    return , @($results | Sort-Object @{ Expression = { $order[$_.Status] } }, Sid, Name)
}

function Invoke-Diff([string]$ShaderName, [string]$PointId) {
    if (-not $ShaderName) { throw 'Usage: shaders.ps1 diff <name> [-Sid s00]' }
    $baseDir = Join-Path $CorpusRoot $Run
    $candDir = Join-Path $CorpusRoot 'candidate'
    $b = @(Read-Manifest $baseDir | Where-Object { $_.Name -ceq $ShaderName -and $_.Sid -ceq $PointId -and $_.Hash -cne '-' })
    $c = @(Read-Manifest $candDir | Where-Object { $_.Name -ceq $ShaderName -and $_.Sid -ceq $PointId -and $_.Hash -cne '-' })
    if (-not $b.Count) { throw "No valid baseline row for '$ShaderName' at $PointId in $baseDir." }
    if (-not $c.Count) { throw "No valid candidate row for '$ShaderName' at $PointId. Run 'shaders.ps1 check' first." }
    Write-Output "baseline   $($b[0].Hash)  origin $($b[0].Origin)"
    Write-Output "candidate  $($c[0].Hash)  origin $($c[0].Origin)"
    $bb = Join-Path $baseDir "blobs\$($b[0].Hash)"
    $cb = Join-Path $candDir "blobs\$($c[0].Hash)"
    $contract = Get-ContractDiff $bb $cb
    if ($contract.Count) { Write-Output '--- contract'; $contract | ForEach-Object { Write-Output "    $_" } }
    else { Write-Output '--- contract: identical' }
    foreach ($s in @(@('vs', 'vert'), @('fs', 'frag'))) {
        $files = foreach ($side in @(@('baseline', $bb), @('candidate', $cb))) {
            $norm = Invoke-Wsl @('python3', (ConvertTo-WslPath (Join-Path $PSScriptRoot 'shadercheck.py')), '--normalize', (ConvertTo-WslPath (Join-Path $side[1] "$($s[0]).full.glsl")), '--stage', $s[1])
            $path = Join-Path $CorpusRoot "diff-$($side[0]).$($s[0]).txt"
            Write-TextNoBom $path (($norm -join "`n") + "`n")
            $path
        }
        Write-Output "--- $($s[0]) (normalized)"
        & git --no-pager diff --no-index --no-color -U3 -- $files[0] $files[1]
        $global:LASTEXITCODE = 0
    }
}

# ---------------------------------------------------------------- main ----

switch ($Command) {
    'record' {
        $sweep = @(Read-Sweep $SweepFile)
        $points = $sweep
        if ($Sids) { $points = @($points | Where-Object { $Sids -ccontains $_.Id }) }
        $mapList = @()
        if (-not $NoMaps) { if ($Maps) { $mapList = $Maps } else { $mapList = Get-ShippedMaps } }
        # Reset every var in the sweep file, even for a -Sids subset.
        $problems = Invoke-Record $Run $points $mapList $sweep
        if ($problems) { exit 1 }
        exit 0
    }
    'check' {
        $results = Invoke-Check
        if ($PassThru) { return $results }
        foreach ($r in $results) { Write-Output (Format-Result $r) }
        Write-Output (Format-Summary $results)
        exit (Get-CheckExitCode $results)
    }
    'diff'  { Invoke-Diff $Name $Sid }
}
