<#
.SYNOPSIS
    Regression self-test for the map editor's texture slot editor.

.DESCRIPTION
    Boots a client in the harness home, opens the shipped map 'atop' in the
    editor and drives the slot editor the way its panel does
    (tool_tex_editslot -> ui_tool_texeditslot_on_open -> tool_tex_editslot_apply),
    plus the engine's editslot and cloneslot commands directly. Each step is
    one of the defects in doc/texture-slot-editing-findings.md.

    Slot indices are discovered on the loaded map rather than hardcoded, but
    the test does rely on atop having a glowdecal decal and a loaded 512px
    world slot, and on appleflap/danger.jpg being 64x64.

.EXAMPLE
    tools\harness\texslot-selftest.ps1
    tools\harness\texslot-selftest.ps1 -KeepRunning
#>
[CmdletBinding()]
param([switch]$KeepRunning)

$ErrorActionPreference = 'Stop'

# $RepoRoot and $HomeDir
. (Join-Path $PSScriptRoot 'core.ps1')

$harness = Join-Path $PSScriptRoot 'harness.ps1'
$editor  = Join-Path $PSScriptRoot 'editor.ps1'

$script:failures = 0
$script:step = 0

# ------------------------------------------------------------- reporting ----

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ("$Actual" -ceq "$Expected") { Write-Host "     ok    $What = '$Actual'" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

# --------------------------------------------------------------- driving ----

function Ed { & $editor @args 6>$null | Out-Null }

function Send([string]$Script, [int]$SettleMs = 250) {
    & $harness send $Script -Settle $SettleMs 6>$null | Out-Null
}

# Evaluates a CubeScript expression in the game; returns its value as text.
function Eval([string]$Expr) {
    $out = @(& $harness send "echo (concatword ""SELFTEST_EVAL="" $Expr)" 6>$null)
    foreach ($line in $out) {
        if ($line -match 'SELFTEST_EVAL=(.*)$') { return $Matches[1].Trim() }
    }
    return $null
}

# See editor-selftest.ps1: loading a map while an entity is hovered trips an
# unguarded enthover read (world.cpp:1426). Entity editing off empties it.
function Invoke-MapLoad([scriptblock]$Body) {
    Send 'entediting 0' 300
    try { & $Body }
    finally { Send 'entediting 1' 300 }
}

# ------------------------------------------------------------------ tests ----

& $harness start 6>$null | Out-Null

try {
    Step 'setup: atop is open in the editor' {
        Invoke-MapLoad { Ed open atop }
        Expect 'edit mode' (Eval '$editing') '1'
    }

    Step 'an unchanged apply keeps a non-bump decal shader (glowdecal fell back to stddecal)' {
        $d = Eval '(listfind i (loopconcat j $numdecalslots [result $j]) [=s (getvshadername $i 1) glowdecal])'
        if ([int]$d -lt 0) { throw 'atop has no glowdecal decal slot' }
        Send "tool_tex_cur = 7; tool_tex_editslot $d 1; tool_tex_editslot_apply" 400
        Expect "decal $d shader" (Eval "(getvshadername $d 1)") 'glowdecal'
        Expect 'editing a decal leaves the world texture brush alone' (Eval '$tool_tex_cur') '7'
        Send 'toolpanel_close tool_texeditslot' 200
    }

    Step 'shrinking the diffuse with compensate scale gives a finite, clamped scale (was inf)' {
        $s = Eval '(listfind i (loopconcat j $numslots [result $j]) [&& [> $i 1] [texloaded $i] [= (getvteximgwidth (getslottex $i) 0) 512]])'
        if ([int]$s -lt 0) { throw 'atop has no loaded 512px slot' }
        $v = Eval "(getslottex $s)"
        $scale = [double](ConvertTo-InvariantDouble (Eval "(getvscale $v)"))
        $want = [math]::Min(8.0, [math]::Max(0.125, $scale * 8.0))
        Send "tool_tex_editslot $v 0; tool_tex_editslot_apply_rescale = 1; tool_texeditslot_tex0 = ""appleflap/danger.jpg""; tool_tex_editslot_apply" 400
        Expect "slot $s scale after 512 -> 64" (ConvertTo-InvariantDouble (Eval "(getvscale $v)")) $want
        Send "tool_texeditslot_tex0 = ""textures/default""; tool_tex_editslot_apply" 400
        Expect "slot $s scale after 64 -> 1024" (ConvertTo-InvariantDouble (Eval "(getvscale $v)")) ($want / 16.0)
        Send 'toolpanel_close tool_texeditslot' 200
    }

    Step 'apply writes to the slot the editor was opened on, not the current texture' {
        $before = Eval '(getvtexname 6 0)'
        Send 'tool_tex_editslot 5 0; tool_focus_tex 6; tool_tex_editslot_apply' 400
        Expect 'slot 6 diffuse' (Eval '(getvtexname 6 0)') $before
        Send 'toolpanel_close tool_texeditslot' 200
    }

    Step 'reopening the editor on another slot re-reads its fields' {
        Send 'tool_tex_editslot 5 0; tool_tex_editslot 7 0' 400
        Expect 'diffuse field' (Eval '$tool_texeditslot_tex0') (Eval '(fixpathslashes (getvtexname 7 0))')
        Send 'toolpanel_close tool_texeditslot' 200
    }

    Step 'apply is refused when the slot changed under an open editor' {
        $orig = Eval '(getvtexname 8 0)'
        Send 'tool_tex_editslot 8 0; editslot 8 [ texture 0 "textures/default" ] 0 0; tool_texeditslot_tex0 = "appleflap/danger.jpg"; tool_tex_editslot_apply' 400
        Expect 'slot 8 diffuse' (Eval '(getvtexname 8 0)') 'textures\default'
        Send "toolpanel_close tool_texeditslot; editslot 8 [ texture 0 ""$($orig -replace '\\', '/')"" ] 0 0" 300
    }

    Step 'editslot refuses an out of range index instead of editing the fallback slot' {
        $geom = Eval '(getvtexname 1 0)'
        Send 'editslot 99999 [ texture 0 "appleflap/danger.jpg" ] 0 0' 300
        Expect 'default geometry slot diffuse' (Eval '(getvtexname 1 0)') $geom
        Send 'editslot 99999 [ texture 0 "appleflap/danger.jpg" ] 0 1' 300
        Expect 'dummy decal slot diffuse' (Eval '(getvtexname 99999 0 1)') ''
    }

    Step 'a clone can be cloned by slot index (the conversion the texture list menu now does; the menu wiring itself is not driven)' {
        $a = Eval '(cloneslot 5)'
        $item = Eval "(getslottex $a)"
        $slots = [int](Eval '$numslots')
        $b = Eval "(cloneslot (getvindex $item))"
        Expect 'numslots' (Eval '$numslots') ($slots + 1)
        Expect 'the second clone diffuse' (Eval "(getvtexname (getslottex $b) 0)") (Eval '(getvtexname 5 0)')
    }

    Step 'the slot editor finds a shader for every hint set it can produce' {
        $decal = @{ '' = 'stddecal'; 'g' = 'glowdecal'; 'gG' = 'pulseglowdecal'; 'v' = 'dispdecal'; 'erg' = 'envglowdecal';
                    's' = 'specdecal'; 'er' = 'envdecal'; 'n' = 'bumpdecal'; 'nesrg' = 'bumpenvspecglowdecal' }
        foreach ($h in $decal.Keys) {
            Expect "decal hints '$h'" (Eval "(tool_tex_editslot_decal = 1; at `$decalshaders (tool_tex_editslot_findshader ""$h"") 0)") $decal[$h]
        }
        $world = @{ '' = 'stdworld'; 'ngers' = 'bumpenvspecglowworld'; 'ngerG' = 'bumpenvpulseglowworld'; 'nsS' = 'bumpspecmapworld';
                    'Td' = 'triplanardetailworld' }
        foreach ($h in $world.Keys) {
            Expect "world hints '$h'" (Eval "(tool_tex_editslot_decal = 0; at `$worldshaders (tool_tex_editslot_findshader ""$h"") 0)") $world[$h]
        }
    }
}
finally {
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'texture slot self-test: all checks passed' -ForegroundColor Green
exit 0
