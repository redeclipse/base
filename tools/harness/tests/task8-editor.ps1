# Task 8: editor.ps1 subcommand behaviour.
$ErrorActionPreference = 'Stop'
$root   = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$editor = Join-Path $root 'tools\harness\editor.ps1'
$failures = 0

function Check([string]$Name, [scriptblock]$Body) {
    try {
        if (& $Body) { Write-Host "  PASS  $Name" -ForegroundColor Green }
        else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
    }
    catch { Write-Host "  FAIL  $Name -- $_" -ForegroundColor Red; $script:failures++ }
}

& $editor newmap 12 | Out-Null

Check 'state reports edit mode'  { (& $editor state).Mode.EditMode -eq 1 }
Check 'goto moves the view'      {
    & $editor goto 512 512 512 | Out-Null
    $s = & $editor state
    [Math]::Abs($s.Cam.X - 512) -lt 1
}
Check 'aim sets yaw'             {
    & $editor aim 90 0 | Out-Null
    [Math]::Abs((& $editor state).Cam.Yaw - 90) -lt 1
}
Check 'frame positions and aims' {
    & $editor frame 512 512 512 -Dist 100 -Yaw 0 -Pitch 0 | Out-Null
    $s = & $editor state
    ([Math]::Abs($s.Cam.Y - 412) -lt 1) -and ([Math]::Abs($s.Cam.Yaw) -lt 1)
}
Check 'cursor on then off'       {
    & $editor cursor on | Out-Null
    $on = (& $editor state).Ui.FreeCursor
    & $editor cursor off | Out-Null
    $off = (& $editor state).Ui.FreeCursor
    ($on -ne 0) -and ($off -eq 0)
}
Check 'sel selects a cube'       {
    & $editor sel 512 512 512 | Out-Null
    (& $editor state).Sel.HaveSel -eq 1
}
# editor.ps1 key now throws on an unknown name (EDRESULT=0), like lookatent /
# frameent on a dead index, instead of printing the 0 and returning normally.
Check 'key rejects unknown keys' {
    try { & $editor key NOTAKEY | Out-Null; $false }
    catch { "$_" -match "Unknown key name 'NOTAKEY'" }
}
Check 'shot writes a png'        { Test-Path (& $editor shot task8) }

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
