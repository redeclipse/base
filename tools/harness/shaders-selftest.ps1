<#
.SYNOPSIS
    Acceptance tests for the shader equivalence harness (shaders.ps1).

.DESCRIPTION
    Records a one-point baseline, then checks deliberate mutations of real
    shader configs and expects each to land on the right tier:
      unchanged                   -> all PASS-TEXT          (determinism)
      a changed param default     -> FAIL, contract         (tier 0)
      a comment in hud            -> PASS-TEXT              (tier 1)
      a renamed local in hud      -> PASS-SPIRV/PASS-PIXEL  (tier 2, or 3 without glslang)
      a changed constant in hud   -> FAIL, pixel            (tier 3)
    Every mutation is reverted byte-for-byte in a finally block.

.EXAMPLE
    tools\harness\shaders-selftest.ps1
#>
[CmdletBinding()]
param([switch]$KeepRunning)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'core.ps1')
. (Join-Path $PSScriptRoot 'shadercorpus.ps1')

$shaders = Join-Path $PSScriptRoot 'shaders.ps1'
$harness = Join-Path $PSScriptRoot 'harness.ps1'
$run = 'selftest-base'
$script:failures = 0
$script:step = 0

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch { Write-Host "     FAIL  $_" -ForegroundColor Red; $script:failures++ }
}

function Expect([string]$What, [bool]$Condition, [string]$Got) {
    if ($Condition) { Write-Host "     ok    $What" -ForegroundColor Green }
    else { Write-Host "     FAIL  $What -- got: $Got" -ForegroundColor Red; $script:failures++ }
}

function Check([string]$Filter) {
    return @(& $shaders check -Run $run -Sids s00 -NoMaps -Filter $Filter -PassThru)
}

function Result($Results, [string]$Name) {
    $r = @($Results | Where-Object { $_.Name -ceq $Name -and $_.Sid -ceq 's00' })
    if ($r.Count) { return $r[0] }
    return [pscustomobject]@{ Status = '(none)'; Detail = '' }
}

# Replaces the first occurrence of $Find in a repo file for the duration of
# $Body, then restores the original bytes exactly.
function Invoke-WithMutation([string]$RelPath, [string]$Find, [string]$Replace, [scriptblock]$Body) {
    $path = Join-Path $RepoRoot $RelPath
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $i = $text.IndexOf($Find, [StringComparison]::Ordinal)
    if ($i -lt 0) { throw "Mutation anchor not found in ${RelPath}: $Find" }
    Write-TextNoBom $path ($text.Substring(0, $i) + $Replace + $text.Substring($i + $Find.Length))
    try { & $Body }
    finally { [System.IO.File]::WriteAllBytes($path, $bytes) }
}

$hudLine = 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale;'

& $harness start 6>$null | Out-Null
try {
    Step 'record a one-point baseline' {
        & $shaders record -Run $run -Sids s00 -NoMaps
        Expect 'record reports no leaks or palette gaps' ($LASTEXITCODE -eq 0) "exit $LASTEXITCODE"
        $dir = Join-Path $HomeDir "shadercorpus\$run"
        $rows = @(Read-Manifest $dir)
        $gaps = @(Test-PaletteCoverage $rows ([System.IO.File]::ReadAllLines((Join-Path $dir 'registry.txt'))) @('s00'))
        Expect 'every registered world/decal shader is in the corpus' ($gaps.Count -eq 0) ($gaps -join ', ')
    }

    Step 'an unchanged build checks clean (determinism)' {
        $res = Check '*'
        $bad = @($res | Where-Object { $_.Status -cne 'PASS-TEXT' })
        Expect "all $($res.Count) configurations PASS-TEXT" ($res.Count -gt 300 -and $bad.Count -eq 0) (($bad | Select-Object -First 5 | ForEach-Object { Format-Result $_ }) -join ' | ')
    }

    Step 'a changed param default fails the contract' {
        Invoke-WithMutation 'config/glsl/world.cfg' 'defuniformparam "gloss" 1 // glossiness' 'defuniformparam "gloss" 2 // glossiness' {
            $r = Result (Check 'stdworld') 'stdworld'
            Expect 'stdworld FAIL naming gloss' ($r.Status -ceq 'FAIL' -and $r.Detail -like '*gloss*') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a comment is textually equivalent' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine ($hudLine + ' // harness selftest') {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud PASS-TEXT' ($r.Status -ceq 'PASS-TEXT') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a renamed local is equivalent past the text tier' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine 'vec4 texel = texture2D(tex0, texcoord0), color = texel * colorscale;' {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud PASS-SPIRV (or PASS-PIXEL without glslang)' ($r.Status -ceq 'PASS-SPIRV' -or $r.Status -ceq 'PASS-PIXEL') "$($r.Status) $($r.Detail)"
        }
    }

    Step 'a changed constant fails at the pixel tier' {
        Invoke-WithMutation 'config/glsl/init.cfg' $hudLine 'vec4 diffuse = texture2D(tex0, texcoord0), color = diffuse * colorscale * 0.5;' {
            $r = Result (Check 'hud') 'hud'
            Expect 'hud FAIL with a pixel error' ($r.Status -ceq 'FAIL' -and $r.Detail -match 'maxerr=') "$($r.Status) $($r.Detail)"
            & $shaders check -Run $run -Sids s00 -NoMaps -Filter 'hud' | Out-Null
            Expect 'check exits non-zero' ($LASTEXITCODE -ne 0) "exit $LASTEXITCODE"
        }
    }

    # Assumes these two files were clean (no uncommitted changes) when the test started;
    # if git already showed them modified beforehand, this step fails for that reason alone.
    Step 'the mutated files are restored' {
        $dirty = @(& git -C $RepoRoot status --porcelain -- config/glsl/init.cfg config/glsl/world.cfg)
        Expect 'git sees no change' ($dirty.Count -eq 0) ($dirty -join ', ')
    }
}
finally {
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) { Write-Host "$script:failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'shader harness self-test: all checks passed' -ForegroundColor Green
exit 0
