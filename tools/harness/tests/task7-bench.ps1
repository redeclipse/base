# Task 7 verification: shaderbench against the running game.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Bench([string]$Script) {
    $hit = @(Invoke-Batch $Script 1 120 | Where-Object { $_ -match 'SHADERBENCH ' })
    if ($hit.Count) { return ($hit[0] -replace '^.*SHADERBENCH ', '') }
    return ''
}

$dir = Join-Path $HomeDir 'shadercorpus\t7'
Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
Invoke-Batch 'shaderdumpall t7 s00 0' 1 300 | Out-Null
$rows = @(Read-Manifest $dir)
$hud = @($rows | Where-Object { $_.Name -ceq 'hud' })[0]
$plain = @($rows | Where-Object { $_.Name -ceq 'hudnotexture' })[0]

$same = Bench "shaderbench t7 $($hud.Hash) ""hud"" 2"
Assert-That "hud against its own blob passes exactly ($same)" ($same -match '^hud PASS maxerr=0 ')
if ($same -match 'cov=(\d+)') { Assert-That 'hud covers most of the target' ([int]$Matches[1] -ge 50) }
$other = Bench "shaderbench t7 $($plain.Hash) ""hud"" 2"
Assert-That "a different shader fails ($other)" ($other -match '^hud FAIL ')
$nosrc = Bench 'shaderbench t7 0000000000000000 "hud" 1'
Assert-That "a missing blob fails ($nosrc)" ($nosrc -match '^hud FAIL .*reason=oldsource')
$nolive = Bench "shaderbench t7 $($hud.Hash) ""nosuchshader"" 1"
Assert-That "a missing live shader fails ($nolive)" ($nolive -match '^nosuchshader FAIL .*reason=missing')
$again = Bench "shaderbench t7 $($hud.Hash) ""hud"" 2"
Assert-That 'the bench is repeatable' ($again -ceq $same)
$shot = & (Join-Path $PSScriptRoot '..\harness.ps1') shot task7
Assert-That 'the game still renders afterwards (read the PNG)' (Test-Path $shot)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 7: all checks passed' -ForegroundColor Green
