# Unit tests for tools/harness/shadercorpus.ps1. No game required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Assert-Throws([string]$Name, [scriptblock]$Body) {
    $threw = $false
    try { & $Body } catch { $threw = $true }
    Assert-That $Name $threw
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("shadercorpus-tests-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force $tmp | Out-Null

function New-Corpus([string]$Name, [string[]]$Rows, [hashtable]$Blobs) {
    $dir = Join-Path $tmp $Name
    New-Item -ItemType Directory -Force $dir | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $dir 'manifest.tsv'), (($Rows -join "`n") + "`n"))
    foreach ($h in $Blobs.Keys) {
        $b = Join-Path $dir "blobs\$h"
        New-Item -ItemType Directory -Force $b | Out-Null
        foreach ($f in $Blobs[$h].Keys) { [System.IO.File]::WriteAllText((Join-Path $b $f), $Blobs[$h][$f]) }
    }
    return $dir
}
function Blob([string]$Meta, [string]$Reflect) { return @{ 'meta.txt' = $Meta; 'reflect.txt' = $Reflect } }

try {
    Write-Host 'Read-Sweep'
    $sweepPath = Join-Path $tmp 'sweep.txt'
    [System.IO.File]::WriteAllText($sweepPath, "# comment`ns00`n`ns01 msaa=4   # trailing`ns02 aotaps=12 hdrgamma=0.5`n")
    $sweep = @(Read-Sweep $sweepPath)
    Assert-That 'three points' ($sweep.Count -eq 3)
    Assert-That 'first point has no settings' ($sweep[0].Id -ceq 's00' -and $sweep[0].Settings.Count -eq 0)
    Assert-That 'trailing comment ignored' ($sweep[1].Settings['msaa'] -ceq '4' -and $sweep[1].Settings.Count -eq 1)
    Assert-That 'float value kept as text' ($sweep[2].Settings['hdrgamma'] -ceq '0.5')
    Assert-That 'vars in first-seen order' ((Get-SweepVars $sweep) -join ',' -ceq 'msaa,aotaps,hdrgamma')
    [System.IO.File]::WriteAllText($sweepPath, "s00`ns00`n")
    Assert-Throws 'duplicate id rejected' { Read-Sweep $sweepPath }
    [System.IO.File]::WriteAllText($sweepPath, "s01 msaa`n")
    Assert-Throws 'bare var rejected' { Read-Sweep $sweepPath }
    [System.IO.File]::WriteAllText($sweepPath, "m-atop`n")
    Assert-Throws 'reserved m- id rejected' { Read-Sweep $sweepPath }

    Write-Host 'Read-RunPoints'
    $run = Join-Path $tmp 'run'
    New-Item -ItemType Directory -Force (Join-Path $run 'settings') | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $run 'run.txt'), "commit abc`nsweep s00 s01`nmaps atop`n")
    [System.IO.File]::WriteAllText((Join-Path $run 'settings\s00.txt'), '')
    [System.IO.File]::WriteAllText((Join-Path $run 'settings\s01.txt'), "msaa=4`n")
    $pts = @(Read-RunPoints $run)
    Assert-That 'recorded order kept' (($pts | ForEach-Object { $_.Id }) -join ',' -ceq 's00,s01')
    Assert-That 'recorded settings read' ($pts[1].Settings['msaa'] -ceq '4')
    Assert-That 'run info read' ((Read-RunInfo $run)['maps'] -ceq 'atop')

    Write-Host 'Compare-Corpus'
    $m1 = "type 1`nparam gloss 1 1 1 0 0 0 0`nparam specscale 2 2 2 0 0 0 0`n"
    $m2 = "type 1`nparam specscale 2 2 2 0 0 0 0`nparam gloss 1 1 1 0 0 0 0`n"
    $r1 = "attrib vvertex vec4 1 0`n"
    $r2 = "attrib vvertex vec4 1 1`n"
    $base = New-Corpus 'base' @(
        "same`ts00`taaaa`tx", "contract`ts00`tbbbb`tx", "order`ts00`tcccc`tx", "pending`ts00`tdddd`tx",
        "missing`ts00`teeee`tx", "gone`ts00`teeee`tx", "stub`ts00`t-`tx", "Case`ts00`taaaa`tx", "other`ts01`taaaa`tx",
        "reflonly`ts00`tdddd`tx", "bumpworld`ts00`taaaa`tx", "<variant:0,1>bumpworld`ts00`taaaa`tx", "bumpworldx`ts00`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); bbbb = (Blob $m1 $r1); cccc = (Blob $m1 $r1); dddd = (Blob $m1 $r1); eeee = (Blob $m1 $r1) }
    $cand = New-Corpus 'cand' @(
        "same`ts00`taaaa`ty", "contract`ts00`tffff`tx", "order`ts00`tgggg`tx", "pending`ts00`thhhh`tx",
        "gone`ts00`t-`tx", "extra`ts00`taaaa`tx", "case`ts00`taaaa`tx", "other`ts01`taaaa`tx",
        "reflonly`ts00`tiiii`tx", "bumpworld`ts00`taaaa`tx", "<variant:0,1>bumpworld`ts00`taaaa`tx", "bumpworldx`ts00`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); ffff = (Blob "type 1`nparam specscale 2 2 2 0 0 0 0`n" $r1); gggg = (Blob $m2 $r1); hhhh = (Blob $m1 $r1); iiii = (Blob $m1 $r2) }

    $res = @(Compare-Corpus -BaseDir $base -CandDir $cand)
    function Status([string]$n, [string]$s = 's00') { $x = @($res | Where-Object { $_.Name -ceq $n -and $_.Sid -ceq $s }); if ($x.Count) { $x[0].Status } else { '(none)' } }
    Assert-That 'identical hash passes as text' ((Status 'same') -ceq 'PASS-TEXT')
    Assert-That 'dropped param fails the contract' ((Status 'contract') -ceq 'FAIL')
    Assert-That 'contract detail names the param' (@($res | Where-Object { $_.Name -ceq 'contract' })[0].Detail -like '*meta: -param gloss*')
    Assert-That 'param order change fails' ((Status 'order') -ceq 'FAIL')
    Assert-That 'order detail says so' (@($res | Where-Object { $_.Name -ceq 'order' })[0].Detail -like '*order changed*')
    Assert-That 'same contract, new hash is pending' ((Status 'pending') -ceq 'PENDING')
    Assert-That 'absent in candidate is missing' ((Status 'missing') -ceq 'MISSING')
    Assert-That 'invalid in candidate is missing' ((Status 'gone') -ceq 'MISSING')
    Assert-That 'baseline stub is ignored' ((Status 'stub') -ceq '(none)')
    Assert-That 'candidate only is extra' ((Status 'extra') -ceq 'EXTRA')
    Assert-That 'names are case-sensitive' ((Status 'Case') -ceq 'MISSING' -and (Status 'case') -ceq 'EXTRA')
    Assert-That 'filter narrows by name' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'pend*').Count -eq 1)
    Assert-That 'sids narrow by settings id' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Sids 's01').Count -eq 1)
    Assert-That 'a reflection difference fails the contract' ((Status 'reflonly') -ceq 'FAIL' -and @($res | Where-Object { $_.Name -ceq 'reflonly' })[0].Detail -like '*reflect: -attrib*')
    $skip = @(Compare-Corpus -BaseDir $base -CandDir $cand -SkipReflection)
    function SkipStatus([string]$n) { $x = @($skip | Where-Object { $_.Name -ceq $n -and $_.Sid -ceq 's00' }); if ($x.Count) { $x[0].Status } else { '(none)' } }
    Assert-That 'skip reflection still fails a meta difference' ((SkipStatus 'contract') -ceq 'FAIL' -and (SkipStatus 'order') -ceq 'FAIL')
    Assert-That 'skip reflection leaves a reflection-only difference pending' ((SkipStatus 'reflonly') -ceq 'PENDING')
    Assert-That 'contract diff without reflection ignores reflect.txt' ((Get-ContractDiff (Join-Path $base 'blobs\dddd') (Join-Path $cand 'blobs\iiii') -SkipReflection).Count -eq 0)
    $bump = @(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'bumpworld' | ForEach-Object { $_.Name })
    Assert-That 'a filter includes its variants' (($bump -join ',') -ceq '<variant:0,1>bumpworld,bumpworld')
    $bumpGlob = @(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'bump*' | ForEach-Object { $_.Name })
    Assert-That 'a glob filter includes variants too' (($bumpGlob -join ',') -ceq '<variant:0,1>bumpworld,bumpworld,bumpworldx')
    Assert-That 'exit code is 1 with a failure' ((Get-CheckExitCode $res) -eq 1)
    Assert-That 'exit code is 0 when clean' ((Get-CheckExitCode @($res | Where-Object { $_.Status -eq 'PASS-TEXT' })) -eq 0)
    Assert-That 'summary counts' ((Format-Summary $res) -like '== * configs: * text, 0 spirv, 0 pixel, 0 weak, 3 fail, * missing, * extra')

    Write-Host 'Compare-Registry'
    function New-Registry([string]$Name, [string[]]$Lines) {
        $dir = Join-Path $tmp $Name
        New-Item -ItemType Directory -Force $dir | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $dir 'registry.txt'), (($Lines -join "`n") + "`n"))
        return $dir
    }
    $reg = @('world stdworld ', 'world specworld s', 'world bumpworld B', 'decal stddecal b')
    $regBase = New-Registry 'reg-base' $reg
    Assert-That 'identical registries give no result' (@(Compare-Registry $regBase (New-Registry 'reg-same' $reg)).Count -eq 0)
    $changed = @(Compare-Registry $regBase (New-Registry 'reg-changed' @('world stdworld ', 'world specworld sS', 'world bumpworld B', 'decal stddecal b')))
    Assert-That 'a changed option string fails' ($changed.Count -eq 1 -and $changed[0].Status -ceq 'FAIL' -and $changed[0].Name -ceq 'registry' -and $changed[0].Sid -ceq '-')
    Assert-That 'the detail shows the line both ways' ($changed[0].Detail -ceq 'line 2: -world specworld s; +world specworld sS')
    $reordered = @(Compare-Registry $regBase (New-Registry 'reg-order' @('world stdworld ', 'world bumpworld B', 'world specworld s', 'decal stddecal b')))
    Assert-That 'a reordered entry fails' ($reordered.Count -eq 1 -and $reordered[0].Status -ceq 'FAIL' -and $reordered[0].Detail -ceq 'line 2: -world specworld s; +world bumpworld B')
    $dropped = @(Compare-Registry $regBase (New-Registry 'reg-missing' @('world stdworld ', 'world bumpworld B', 'decal stddecal b')))
    Assert-That 'a missing entry fails' ($dropped.Count -eq 1 -and $dropped[0].Status -ceq 'FAIL' -and $dropped[0].Detail -like 'line 2: -world specworld s; +world bumpworld B (4 vs 3 lines)')
    $crlfBase = New-Registry 'reg-crlf-base' @(); [System.IO.File]::WriteAllText((Join-Path $crlfBase 'registry.txt'), (($reg -join "`r`n") + "`r`n"))
    $crlfCand = New-Registry 'reg-crlf-cand' @(); [System.IO.File]::WriteAllText((Join-Path $crlfCand 'registry.txt'), ((@('world stdworld ', 'world specworld sS', 'world bumpworld B', 'decal stddecal b') -join "`r`n") + "`r`n"))
    $crlf = @(Compare-Registry $crlfBase $crlfCand)
    Assert-That 'a CRLF registry (writetofile) shows the line without the CR' ($crlf.Count -eq 1 -and $crlf[0].Detail -ceq 'line 2: -world specworld s; +world specworld sS')
    $ending = @(Compare-Registry $regBase $crlfBase)
    Assert-That 'a line-end-only difference still fails: the comparison is byte-exact' ($ending.Count -eq 1 -and $ending[0].Detail -ceq 'same lines, different bytes')
    $noFile = @(Compare-Registry $regBase (Join-Path $tmp 'reg-none'))
    Assert-That 'a missing registry.txt fails' ($noFile.Count -eq 1 -and $noFile[0].Detail -like 'no registry.txt in *reg-none')

    Write-Host 'Test-SameGpu'
    $glA = "vendor A`nrenderer R1`nversion 4.6`nglslversion 460`n"
    $glB = "vendor A`nrenderer R2`nversion 4.6`nglslversion 460`n"
    Assert-That 'the same gl.txt passes' ((Test-SameGpu $glA $glA 'abc123') -eq $true)
    Assert-That '-AllowCrossGpu accepts a different GPU' ((Test-SameGpu $glA $glB 'abc123' -AllowCrossGpu) -eq $false)
    $msg = ''
    try { Test-SameGpu $glA $glB 'abc123' | Out-Null } catch { $msg = "$_" }
    Assert-That 'a different GPU is refused' ($msg -ne '')
    Assert-That 'the refusal names both GL strings and the commit' ($msg -like '*renderer R1*' -and $msg -like '*renderer R2*' -and $msg -like '*commit abc123*' -and $msg -like '*-AllowCrossGpu*')
    $noted = @([pscustomobject]@{ Detail = 'identical' }, [pscustomobject]@{ Detail = '' })
    Add-CrossGpuNote $noted
    Assert-That 'cross-gpu results say what was skipped' ($noted[0].Detail -ceq 'identical (cross-gpu: reflection and pixels skipped)' -and $noted[1].Detail -ceq '(cross-gpu: reflection and pixels skipped)')

    Write-Host 'Find-SweepLeaks'
    $rows = @(Read-Manifest (New-Corpus 'leak' @(
        "a`ts00`t1111`tx", "a`ts99`t1111`tx", "b`ts00`t2222`tx", "b`ts99`t3333`ty", "c`ts99`t-`tx", "d`ts00`t4444`tx",
        "e`ts99`t5555`tx", "f`ts00`t6666`tx", "f`ts99`t-`tx") @{}))
    $leaks = @(Find-SweepLeaks $rows 's00' 's99' | Where-Object { Test-SweepLeak $_ })
    Assert-That 'changed hash is a leak' (@($leaks | Where-Object { $_.Name -ceq 'b' }).Count -eq 1)
    Assert-That 'a stub after the sweep is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'c' }).Count -eq 0)
    Assert-That 'vanishing after the sweep is a leak' (@($leaks | Where-Object { $_.Name -ceq 'd' }).Count -eq 1)
    Assert-That 'valid at s00 but invalid at s99 is a leak' (@($leaks | Where-Object { $_.Name -ceq 'f' }).Count -eq 1)
    Assert-That 'appearing only at s99 is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'e' }).Count -eq 0 -and @(Find-SweepLeaks $rows 's00' 's99' | Where-Object { $_.Name -ceq 'e' }).Count -eq 1)
    Assert-That 'identical is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'a' }).Count -eq 0)

    Write-Host 'Test-PaletteCoverage'
    $gaps = @(Test-PaletteCoverage $rows @('world a ', 'world b sS', 'decal d b') @('s00', 's99'))
    Assert-That 'd is missing at s99 only' (($gaps -join ',') -ceq 'd s99')

    Write-Host 'ConvertTo-WslPath'
    Assert-That 'drive path converts' ((ConvertTo-WslPath 'F:\Red Eclipse\home\x.tsv') -ceq '/mnt/f/Red Eclipse/home/x.tsv')

    Write-Host 'ConvertFrom-BenchLine'
    $passLine = 'SHADERBENCH hud PASS maxerr=0.0001 cov=87 seeds=4'
    $pass = ConvertFrom-BenchLine $passLine
    Assert-That 'PASS maps to PASS-PIXEL' ($pass.Status -ceq 'PASS-PIXEL')
    Assert-That 'PASS keeps the name' ($pass.Name -ceq 'hud')
    Assert-That 'PASS keeps the detail' ($pass.Detail -ceq 'maxerr=0.0001 cov=87 seeds=4')

    $failLine = 'SHADERBENCH bumpworld FAIL maxerr=0.5 cov=91 seeds=4'
    $fail = ConvertFrom-BenchLine $failLine
    Assert-That 'FAIL with maxerr stays FAIL' ($fail.Status -ceq 'FAIL')

    $unsupportedLine = 'SHADERBENCH watervortex FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported uniform 0x8B5E'
    $unsupported = ConvertFrom-BenchLine $unsupportedLine
    Assert-That 'FAIL with reason=unsupported uniform maps to WEAK' ($unsupported.Status -ceq 'WEAK')
    Assert-That 'WEAK keeps the reason in the detail' ($unsupported.Detail -ceq 'maxerr=0.9 cov=40 seeds=4 reason=unsupported uniform 0x8B5E')
    $sampler = ConvertFrom-BenchLine 'SHADERBENCH watervortex FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported sampler sampler2DMS'
    Assert-That 'FAIL with reason=unsupported sampler stays FAIL' ($sampler.Status -ceq 'FAIL')
    $attrib = ConvertFrom-BenchLine 'SHADERBENCH skin FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported attribute ivec4'
    Assert-That 'FAIL with reason=unsupported attribute stays FAIL' ($attrib.Status -ceq 'FAIL')

    $weakLine = 'SHADERBENCH hudtext WEAK maxerr=0.0002 cov=30 seeds=4 reason=coverage'
    $weak = ConvertFrom-BenchLine $weakLine
    Assert-That 'WEAK coverage line stays WEAK' ($weak.Status -ceq 'WEAK')

    Assert-That 'a non-bench line gives $null' ($null -eq (ConvertFrom-BenchLine 'some other log line'))

    $timestamped = ConvertFrom-BenchLine '2026-09-25 15:04.12 SHADERBENCH hud PASS maxerr=0 cov=100 seeds=4'
    Assert-That 'a timestamp-prefixed line still parses' ($null -ne $timestamped -and $timestamped.Status -ceq 'PASS-PIXEL')
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'shadercorpus: all checks passed' -ForegroundColor Green
