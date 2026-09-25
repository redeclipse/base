# Task 3 verification: shaderdumpall writes a complete, deterministic corpus.
# Needs a running harness (tools\harness\harness.ps1 start); no map required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

$root = Join-Path $HomeDir 'shadercorpus'
foreach ($r in 't3a', 't3b') { Remove-Item -Recurse -Force (Join-Path $root $r) -ErrorAction SilentlyContinue }

$out = @(Invoke-Batch 'shaderdumpall t3a s00 0' 1 300) + @(Invoke-Batch 'shaderdumpall t3b s00 0' 1 300)
Assert-That 'both dumps reported' (@($out | Where-Object { $_ -match 'SHADERDUMP t3[ab] s00 \d+ \d+' }).Count -eq 2)
$bad = @(Invoke-Batch 'echo (concatword "T3_BAD=" (shaderdumpall "../x" s00 0))' 1 60)
Assert-That 'a path-like run name is refused' (@($bad | Where-Object { $_ -match 'T3_BAD=-1' }).Count -eq 1)

$a = [System.IO.File]::ReadAllLines((Join-Path $root 't3a\manifest.tsv'))
$b = [System.IO.File]::ReadAllLines((Join-Path $root 't3b\manifest.tsv'))
Assert-That 'manifest has hundreds of rows' ($a.Count -gt 300)
Assert-That 'every row has four tab-separated fields' (@($a | Where-Object { ($_ -split "`t").Count -ne 4 }).Count -eq 0)
Assert-That 'the same state gives the same manifest' (($a -join "`n") -ceq ($b -join "`n"))

$std = @($a | Where-Object { $_.StartsWith("stdworld`t") })
Assert-That 'stdworld has exactly one row' ($std.Count -eq 1)
$f = $std[0] -split "`t"
Assert-That 'stdworld origin is its defershader' ($f[3] -ceq 'defer:stdworld')
Assert-That 'stdworld hash is 16 hex digits' ($f[2] -match '^[0-9a-f]{16}$')
Assert-That 'the stdworld variant is dumped' (@($a | Where-Object { $_.StartsWith("<variant:0,0>stdworld`t") }).Count -eq 1)
$hudRow = @($a | Where-Object { $_.StartsWith("hud`t") })
Assert-That 'the hud shader came from init.cfg' ($hudRow.Count -eq 1 -and ($hudRow[0] -split "`t")[3] -ceq 'config/glsl/init.cfg')

$blob = Join-Path $root "t3a\blobs\$($f[2])"
foreach ($n in 'vs.glsl', 'fs.glsl', 'vs.full.glsl', 'fs.full.glsl', 'meta.txt', 'reflect.txt') {
    Assert-That "blob has $n" (Test-Path (Join-Path $blob $n))
}
$meta = [System.IO.File]::ReadAllLines((Join-Path $blob 'meta.txt'))
Assert-That 'meta starts with the type' ($meta[0] -match '^type \d+$')
Assert-That 'stdworld lists the gloss default' (@($meta | Where-Object { $_ -like 'param gloss *' }).Count -eq 1)
Assert-That 'stdworld has variant rows' (@($meta | Where-Object { $_ -like 'variants *' }).Count -ge 1)
$reflect = [System.IO.File]::ReadAllLines((Join-Path $blob 'reflect.txt'))
Assert-That 'reflection lists vvertex' (@($reflect | Where-Object { $_ -like 'attrib vvertex *' }).Count -eq 1)
Assert-That 'reflection lists the diffusemap sampler with its unit' (@($reflect | Where-Object { $_ -match '^sampler diffusemap sampler2D \d+$' }).Count -eq 1)
Assert-That 'reflection lists a fragment output' (@($reflect | Where-Object { $_ -like 'fragdata *' }).Count -ge 1)
$ordinal = [string[]]@($reflect)
[Array]::Sort($ordinal, [StringComparer]::Ordinal)   # strcmp order; Sort-Object is culture-aware
Assert-That 'reflection is sorted' (($reflect -join "`n") -ceq ($ordinal -join "`n"))
Assert-That 'composed source starts with the version header' ([System.IO.File]::ReadAllText((Join-Path $blob 'fs.full.glsl')) -match '^#version \d+')
Assert-That 'composed source ends with the body' ([System.IO.File]::ReadAllText((Join-Path $blob 'fs.full.glsl')).EndsWith([System.IO.File]::ReadAllText((Join-Path $blob 'fs.glsl')).TrimStart()))
Assert-That 'gl.txt names the renderer' (@([System.IO.File]::ReadAllLines((Join-Path $root 't3a\gl.txt')) | Where-Object { $_ -like 'renderer *' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 3: all checks passed' -ForegroundColor Green
