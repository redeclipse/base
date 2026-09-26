# Task 5 verification: record a small corpus and check its shape.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

# The m-<map> pass dumps map shaders and every generateshader-made shader
# (grass, models, deferred lights, ...); conquest's texgrass slots give the
# map-dependent-shader assertions below something real to find.
& (Join-Path $PSScriptRoot '..\shaders.ps1') record -Run t5 -Sids s00, s01, s07, s99 -Maps conquest
Assert-That 'record exited cleanly' ($LASTEXITCODE -eq 0)

$dir = Join-Path $HomeDir 'shadercorpus\t5'
$rows = @(Read-Manifest $dir)
$info = Read-RunInfo $dir
Assert-That 'run.txt has the sweep order' ($info['sweep'] -ceq 's00 s01 s07 s99')
Assert-That 'run.txt has the commit' ($info['commit'] -match '^[0-9a-f]{40}$')
Assert-That 's01 settings recorded' ([System.IO.File]::ReadAllText((Join-Path $dir 'settings\s01.txt')).Trim() -ceq 'msaa=4')
foreach ($sid in 's00', 's01', 's07', 's99', 'm-conquest') {
    Assert-That "rows for $sid" (@($rows | Where-Object { $_.Sid -ceq $sid }).Count -gt 0)
}
Assert-That 'msaa generated multisample light shaders' (@($rows | Where-Object { $_.Sid -ceq 's01' -and $_.Hash -cne '-' -and $_.Name.StartsWith('deferredlightM') }).Count -gt 0)
Assert-That 'aotaps=12 reached the AO generator' (@($rows | Where-Object { $_.Sid -ceq 's07' -and $_.Hash -cne '-' -and $_.Origin -cmatch '^ambientobscuranceshader .* 12$' }).Count -gt 0)
Assert-That 'map pass dumps only map-dependent shaders' (@($rows | Where-Object { $_.Sid -ceq 'm-conquest' -and $_.Name -ceq 'stdworld' }).Count -eq 0)
Assert-That 'map pass includes generator shaders' (@($rows | Where-Object { $_.Sid -ceq 'm-conquest' -and $_.Hash -cne '-' -and $_.Origin -cmatch '^(deferredlightshader|modelshader) ' }).Count -gt 0)
# The map pass generates model shaders for every state a skin can be drawn in,
# not only the ones that happened to be drawn: each model shader comes with its
# shimmer twin ('0' goes after the a/A/u/w/d/D/n/m/e options, see loadshader()).
$mapModels = @($rows | Where-Object { $_.Sid -ceq 'm-conquest' -and $_.Hash -cne '-' -and -not $_.Name.StartsWith('<') -and $_.Origin -cmatch '^modelshader ' })
$mapModelNames = @($mapModels | ForEach-Object { $_.Name })
$noTwin = @($mapModels | Where-Object { $_.Name -cnotmatch '0' } | Where-Object { $mapModelNames -cnotcontains ($_.Name -creplace '^model([aAuwdDnme]*)', 'model${1}0') })
Assert-That 'map pass has model shaders' ($mapModels.Count -gt 0)
Assert-That "map pass has every model shader's shimmer twin ($($noTwin.Count) missing)" ($noTwin.Count -eq 0)
$registry = [System.IO.File]::ReadAllLines((Join-Path $dir 'registry.txt'))
Assert-That 'registry lists stdworld' ($registry -ccontains 'world stdworld ')
Assert-That 'registry lists stddecal' ($registry -ccontains 'decal stddecal b')
Assert-That 'registry is the whole palette' ($registry.Count -gt 150)
Assert-That 'every registered shader is valid at every point' (@(Test-PaletteCoverage $rows $registry @('s00', 's01', 's07', 's99')).Count -eq 0)
# A name that is '(none)' on one side is not a reset failure: resetshaders/
# resetgl recompile existing Shader variants but never free ones an earlier
# sweep point created (e.g. s01's msaa=4 permanently registers deferredlightM*
# rows that s07/s99 then also see), so s99 legitimately carries more rows
# than s00 even though both are pure defaults. A name valid at s00 must stay
# valid at s99 with the same hash, or it is a genuine "var missing from the
# reset list" leak -- see Test-SweepLeak and shaders.ps1's Invoke-Record.
Assert-That 'no state leaked between s00 and s99' (@(Find-SweepLeaks $rows 's00' 's99' | Where-Object { Test-SweepLeak $_ }).Count -eq 0)
$state = @(Invoke-Batch 'echo (concatword "T5_MSAA=" $msaa " T5_TAPS=" $aotaps)' 1 30)
Assert-That 'defaults restored afterwards' (@($state | Where-Object { $_ -match 'T5_MSAA=0 T5_TAPS=5' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 5: all checks passed' -ForegroundColor Green
