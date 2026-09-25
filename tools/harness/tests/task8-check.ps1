# Task 8 verification: an unchanged build checks clean against itself.
$ErrorActionPreference = 'Stop'
$shaders = Join-Path $PSScriptRoot '..\shaders.ps1'

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

& $shaders record -Run t8 -Sids s00 -NoMaps
Assert-That 'record exited cleanly' ($LASTEXITCODE -eq 0)

$res = @(& $shaders check -Run t8 -Sids s00 -NoMaps -PassThru)
Assert-That 'check produced results' ($res.Count -gt 300)
Assert-That 'every configuration is hash-identical' (@($res | Where-Object { $_.Status -cne 'PASS-TEXT' }).Count -eq 0)

$text = @(& $shaders check -Run t8 -Sids s00 -NoMaps -Filter 'hud*')
Assert-That 'check exits 0 when clean' ($LASTEXITCODE -eq 0)
Assert-That 'a summary line is printed' (@($text | Where-Object { $_ -like '== * configs:*' }).Count -eq 1)

$diff = @(& $shaders diff hud -Run t8 -Sid s00)
Assert-That 'diff shows the contract is identical' (@($diff | Where-Object { $_ -like '*contract: identical*' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 8: all checks passed' -ForegroundColor Green
