# Task 5: harness.ps1's public behaviour must be identical after the extraction.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$harness = Join-Path $root 'tools\harness\harness.ps1'
$failures = 0

function Check([string]$Name, [scriptblock]$Body) {
    try {
        $result = & $Body
        if ($result) { Write-Host "  PASS  $Name" -ForegroundColor Green }
        else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
    }
    catch { Write-Host "  FAIL  $Name -- $_" -ForegroundColor Red; $script:failures++ }
}

Check 'core.ps1 exists' { Test-Path (Join-Path $root 'tools\harness\core.ps1') }
Check 'harness.ps1 dot-sources core.ps1' { (Get-Content -Raw $harness) -match 'core\.ps1' }
Check 'status runs' { (& $harness status 6>&1) -match 'running|not running' }
Check 'send echoes' { (& $harness send 'echo "SMOKE_OK"') -match 'SMOKE_OK' }
Check 'nav opens a panel' {
    & $harness nav ui_gameui_settings_graphics | Out-Null
    (& $harness send 'echo (concatword "TOP=" $uitopname)') -match 'TOP=main'
}
Check 'tree reports objects' { (& $harness tree -Drawn 6>&1) -match 'aspect' }
Check 'shot writes a png' { Test-Path (& $harness shot task5smoke) }

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
