# Task 5 verification: record a small corpus and check its shape.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')
. (Join-Path $PSScriptRoot '..\shadercorpus.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

# atop and backrooms have no grass geometry/mapdef shaders, so their m-<map>
# pass legitimately dumps 0 rows; conquest's texgrass slots give the
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
Assert-That 'aotaps=12 reached the AO generator' (@($rows | Where-Object { $_.Sid -ceq 's07' -and $_.Hash -cne '-' -and $_.Origin -match '^ambientobscuranceshader .* 12$' }).Count -gt 0)
Assert-That 'map pass dumps only map-dependent shaders' (@($rows | Where-Object { $_.Sid -ceq 'm-conquest' -and $_.Name -ceq 'stdworld' }).Count -eq 0)
$registry = [System.IO.File]::ReadAllLines((Join-Path $dir 'registry.txt'))
Assert-That 'registry lists stdworld' ($registry -ccontains 'world stdworld ')
Assert-That 'registry lists stddecal' ($registry -ccontains 'decal stddecal b')
Assert-That 'registry is the whole palette' ($registry.Count -gt 150)
Assert-That 'every registered shader is valid at every point' (@(Test-PaletteCoverage $rows $registry @('s00', 's01', 's07', 's99')).Count -eq 0)
# A name that is '(none)' on one side is not a reset failure: resetshaders/
# resetgl recompile existing Shader variants but never free ones an earlier
# sweep point created (e.g. s01's msaa=4 permanently registers deferredlightM*
# rows that s07/s99 then also see), so s99 legitimately carries more rows
# than s00 even though both are pure defaults.
#
# deferredlightshader (renderlights.cpp) rows are a separate, verified case:
# they are shared across every material that requests their (row, col) combo,
# and shader.cpp:864 seeds a newly-created variant's defaultparams from
# whichever slot's params are current at that moment -- never re-derived by a
# later recompile. Direct repro (open a map, dump, bare 'resetshaders' with NO
# setting change, dump again) shows exactly one such call irreversibly moves
# these variants off their natural map-load binding, and no resetshaders/
# resetgl combination afterwards moves them back. So once a run touches any
# CHANGE_SHADERS var (msaa here), these rows' baked-in default uniforms are
# permanently history-dependent -- s00 and s99 can legitimately differ on the
# *same* name. See the matching comment in shaders.ps1's Invoke-Record.
#
# Any other same-name-different-hash row is a genuine "var missing from the
# reset list" leak.
Assert-That 'no state leaked between s00 and s99' (@(Find-SweepLeaks $rows 's00' 's99' | Where-Object { $_.First -cne '(none)' -and $_.Last -cne '(none)' -and $_.Origin -notmatch '^deferredlightshader ' }).Count -eq 0)
$state = @(Invoke-Batch 'echo (concatword "T5_MSAA=" $msaa " T5_TAPS=" $aotaps)' 1 30)
Assert-That 'defaults restored afterwards' (@($state | Where-Object { $_ -match 'T5_MSAA=0 T5_TAPS=5' }).Count -eq 1)

if ($failures) { Write-Host "$failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'task 5: all checks passed' -ForegroundColor Green
