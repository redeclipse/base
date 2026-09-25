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
    $base = New-Corpus 'base' @(
        "same`ts00`taaaa`tx", "contract`ts00`tbbbb`tx", "order`ts00`tcccc`tx", "pending`ts00`tdddd`tx",
        "missing`ts00`teeee`tx", "gone`ts00`teeee`tx", "stub`ts00`t-`tx", "Case`ts00`taaaa`tx", "other`ts01`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); bbbb = (Blob $m1 $r1); cccc = (Blob $m1 $r1); dddd = (Blob $m1 $r1); eeee = (Blob $m1 $r1) }
    $cand = New-Corpus 'cand' @(
        "same`ts00`taaaa`ty", "contract`ts00`tffff`tx", "order`ts00`tgggg`tx", "pending`ts00`thhhh`tx",
        "gone`ts00`t-`tx", "extra`ts00`taaaa`tx", "case`ts00`taaaa`tx", "other`ts01`taaaa`tx"
    ) @{ aaaa = (Blob $m1 $r1); ffff = (Blob "type 1`nparam specscale 2 2 2 0 0 0 0`n" $r1); gggg = (Blob $m2 $r1); hhhh = (Blob $m1 $r1) }

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
    Assert-That 'skip contract leaves it pending' (@(Compare-Corpus -BaseDir $base -CandDir $cand -Filter 'contract' -SkipContract)[0].Status -ceq 'PENDING')
    Assert-That 'exit code is 1 with a failure' ((Get-CheckExitCode $res) -eq 1)
    Assert-That 'exit code is 0 when clean' ((Get-CheckExitCode @($res | Where-Object { $_.Status -eq 'PASS-TEXT' })) -eq 0)
    Assert-That 'summary counts' ((Format-Summary $res) -like '== * configs: * text, 0 spirv, 0 pixel, 0 weak, 2 fail, * missing, * extra')

    Write-Host 'Find-SweepLeaks'
    $rows = @(Read-Manifest (New-Corpus 'leak' @(
        "a`ts00`t1111`tx", "a`ts99`t1111`tx", "b`ts00`t2222`tx", "b`ts99`t3333`ty", "c`ts99`t-`tx", "d`ts00`t4444`tx") @{}))
    $leaks = @(Find-SweepLeaks $rows 's00' 's99')
    Assert-That 'changed hash is a leak' (@($leaks | Where-Object { $_.Name -ceq 'b' }).Count -eq 1)
    Assert-That 'a stub after the sweep is not a leak' (@($leaks | Where-Object { $_.Name -ceq 'c' }).Count -eq 0)
    Assert-That 'vanishing after the sweep is a leak' (@($leaks | Where-Object { $_.Name -ceq 'd' }).Count -eq 1)
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

    $unsupportedLine = 'SHADERBENCH watervortex FAIL maxerr=0.9 cov=40 seeds=4 reason=unsupported sampler sampler2DMS'
    $unsupported = ConvertFrom-BenchLine $unsupportedLine
    Assert-That 'FAIL with reason=unsupported maps to WEAK' ($unsupported.Status -ceq 'WEAK')
    Assert-That 'WEAK keeps the reason in the detail' ($unsupported.Detail -ceq 'maxerr=0.9 cov=40 seeds=4 reason=unsupported sampler sampler2DMS')

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
