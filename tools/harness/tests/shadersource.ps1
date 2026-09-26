# shader_new loader: shader_new, shader_define, shader_include_vs/fs,
# shader_source, variantshader_new. Needs a running harness on a build with
# the loader (tools\harness\harness.ps1 start); no map required.
# Fixtures go to home\uitest\config\glsl\harness\ (findfile searches the home
# dir first), plus one fixture at home\uitest\data\harness\ to prove the
# "outside config/glsl" refusal is a real refusal and not just a missing
# file. All fixture dirs are removed at the end, even on failure. Shader
# names carry a per-run tag, so the script can be rerun in the same game
# session.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\core.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}
function Test-Echo([string[]]$Lines, [string]$Expected) {
    # Every log line carries a "YYYY-MM-DD HH:MM.SS " timestamp (conoutf,
    # src/engine/server.cpp), so an exact match must strip it first -- same
    # regex core.ps1's Show-BatchResult uses for display.
    return @($Lines | Where-Object { ($_ -replace '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ', '').Trim() -ceq $Expected }).Count -eq 1
}
function Test-Logged([string[]]$Lines, [string]$Text) {
    return @($Lines | Where-Object { $_.Contains($Text) }).Count -ge 1
}
function Test-Contains([string]$Text, [string]$Needle) {
    # Get-Stage returns $null for a missing manifest row (e.g. a shader that
    # was never created); a bare .Contains($Needle) on that throws instead
    # of failing the assertion, which stops the whole script short.
    if ($null -eq $Text) { return $false }
    return $Text.Contains($Needle)
}

$t = 'src' + (Get-Random -Maximum 1000000)
$fixDir = Join-Path $HomeDir 'config\glsl\harness'
$outsideDir = Join-Path $HomeDir 'data\harness'
$runDir = Join-Path $HomeDir 'shadercorpus\srcloader'
New-Item -ItemType Directory -Force $fixDir | Out-Null
New-Item -ItemType Directory -Force $outsideDir | Out-Null
Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue

try {

$vert = "attribute vec4 vvertex;`nvoid main(void)`n{`n    gl_Position = vvertex;`n}`n"
$frag = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n#ifdef SRC_FLAG`n    #if SRC_MODE == 2`n    fragcolor = SRC_COLOR;`n    #endif`n#else`n    fragcolor = vec4(1.0);`n#endif`n}`n"
$frag2 = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n    fragcolor = vec4(0.0);`n}`n"
$fogfrag = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n    fragcolor = vec4(1.0);`n    //:fog`n}`n"
$variantfrag = "fragdata(0) vec4 fragcolor;`nvoid main(void)`n{`n    fragcolor = vec4(1.0);`n    //:variant`n}`n"
# CRLF and no final newline: the loader must drop the CRs and add the newline.
$common = "// shared`r`n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)"
Write-TextNoBom (Join-Path $fixDir 't.vert') $vert
Write-TextNoBom (Join-Path $fixDir 't.frag') $frag
Write-TextNoBom (Join-Path $fixDir 't2.frag') $frag2
Write-TextNoBom (Join-Path $fixDir 'fog.frag') $fogfrag
Write-TextNoBom (Join-Path $fixDir 'variant.frag') $variantfrag
Write-TextNoBom (Join-Path $fixDir 'common.glsl') $common
# F3's "empty" is the assembled TEXT being empty, not the file being 0 bytes:
# loadfile already refuses a literal 0-byte file (loadstream's flen <= 0
# check), so that case was always caught, just by the wrong ("cannot read")
# message. A lone '\r' is 1 byte on disk (loadfile reads it fine) but
# appendtext strips '\r' and adds no line of its own, so the stage still
# assembles to "" -- the actual case F3 fixes.
Write-TextNoBom (Join-Path $fixDir 'empty.frag') "`r"
# F10b: a real file outside config/glsl, so the SRC_OUTSIDE refusal below is
# proven to come from the path rule, not from the target simply not existing.
Write-TextNoBom (Join-Path $outsideDir 't.frag') $frag

$defs = "#define SRC_MODE 2`n#define SRC_FLAG`n"
$expectVs = $defs + $vert
$expectFs = $defs + "// shared`n#define SRC_COLOR vec4(0.25, 0.5, 0.75, 1.0)`n" + $frag

# 'setshader null' clears leftover texture-slot params, which shader() would
# otherwise turn into extra uniform declarations and break the exact text
# comparisons below.
$make = @"
setshader null
shader_new 0 ${t}ok [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OK=" (hasshader ${t}ok))
shader_new 0 ${t}fog [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/fog.frag" ]
echo (concatword "SRC_FOG=" (hasshader ${t}fog))
setshaderparam srcparam 1 2 3 4
shader_new 0 ${t}param [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_PARAM=" (hasshader ${t}param))
shader_new 0 ${t}outer [
    shader_define SRC_MODE 2
    shader_new 0 ${t}inner [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_OUTER=" (hasshader ${t}outer) " SRC_INNER=" (hasshader ${t}inner))
srcran = 0
shader_new 0 ${t}ok [ srcran = 1 ]
echo (concatword "SRC_RAN=" `$srcran)
shader_new 0 ${t}twice [
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag"
    shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t2.frag"
]
echo (concatword "SRC_TWICE=" (hasshader ${t}twice))
shader_new 0 ${t}genvariant [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/variant.frag" ]
echo (concatword "SRC_VARIANTGEN=" (hasshader "<variant:0,0>${t}genvariant"))
"@
# No outer @() -- Invoke-Batch already returns ", @($lines)" so a direct
# assignment keeps the flat line array; wrapping it again nests it one level
# too deep and [string[]]-typed params below would coerce that down to the
# single string "System.Object[]".
$made = Invoke-Batch $make 1 120
Assert-That 'a shader from defines, an include and two files is created' (Test-Echo $made 'SRC_OK=1')
Assert-That 'a //:fog pragma inside a file still creates the shader' (Test-Echo $made 'SRC_FOG=1')
Assert-That 'a shader with a texture-slot param is created' (Test-Echo $made 'SRC_PARAM=1')
Assert-That 'nested shader_new bodies both create their shaders' (Test-Echo $made 'SRC_OUTER=1 SRC_INNER=1')
Assert-That 'the body of an existing shader is not run' (Test-Echo $made 'SRC_RAN=0')
Assert-That 'shader_source called twice creates the shader' (Test-Echo $made 'SRC_TWICE=1')
Assert-That 'a //:variant pragma inside a file creates the generic variant' (Test-Echo $made 'SRC_VARIANTGEN=1')

$bad = @"
shader_new 0 ${t}missing [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/nope.frag" ]
echo (concatword "SRC_MISSING=" (hasshader ${t}missing))
shader_new 0 ${t}dotdot [ shader_source "config/glsl/../config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_DOTDOT=" (hasshader ${t}dotdot))
shader_new 0 ${t}outside [ shader_source "config/glsl/harness/t.vert" "data/harness/t.frag" ]
echo (concatword "SRC_OUTSIDE=" (hasshader ${t}outside))
shader_new 0 ${t}bothbad [ shader_source "data/harness/t.frag" "config/glsl/../config/glsl/harness/t.frag" ]
echo (concatword "SRC_BOTHBAD=" (hasshader ${t}bothbad))
shader_new 0 ${t}badname [ shader_define "1BAD" 1; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADNAME=" (hasshader ${t}badname))
shader_new 0 ${t}badvalue [ shader_define SRC_X "1^n2"; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADVALUE=" (hasshader ${t}badvalue))
shader_new 0 ${t}onestage [ shader_source "config/glsl/harness/t.vert" "" ]
echo (concatword "SRC_ONESTAGE=" (hasshader ${t}onestage))
shader_new 0 ${t}orphan [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ORPHAN=" (hasshader ${t}orphan))
shader_new 0 ${t}badinclude [ shader_include_fs "config/glsl/harness/noinclude.glsl"; shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_BADINCLUDE=" (hasshader ${t}badinclude))
shader_new 0 ${t}emptyfrag [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/empty.frag" ]
echo (concatword "SRC_EMPTYFRAG=" (hasshader ${t}emptyfrag))
shader_define SRC_LOOSE 1
"@
$failed = Invoke-Batch $bad 1 120
Assert-That 'a missing file creates no shader' (Test-Echo $failed 'SRC_MISSING=0')
Assert-That 'a missing file is named in the log' (Test-Logged $failed 'cannot read config/glsl/harness/nope.frag')
Assert-That 'a .. path is refused' (Test-Echo $failed 'SRC_DOTDOT=0')
Assert-That 'a path outside config/glsl is refused' (Test-Echo $failed 'SRC_OUTSIDE=0')
Assert-That 'both bad paths in one shader_source call are refused' (Test-Echo $failed 'SRC_BOTHBAD=0')
Assert-That 'refused paths are logged' (@($failed | Where-Object { $_.Contains('refusing') }).Count -eq 4)
Assert-That 'an invalid define name is refused' (Test-Echo $failed 'SRC_BADNAME=0')
Assert-That 'a define value with a newline is refused' (Test-Echo $failed 'SRC_BADVALUE=0')
Assert-That 'invalid defines are logged' (@($failed | Where-Object { $_.Contains('invalid define') }).Count -eq 2)
Assert-That 'shader_new needs both stages' ((Test-Echo $failed 'SRC_ONESTAGE=0') -and (Test-Logged $failed 'needs both a vertex and a fragment source'))
Assert-That 'includes without a source for that stage are refused' ((Test-Echo $failed 'SRC_ORPHAN=0') -and (Test-Logged $failed 'includes given but no vertex source'))
Assert-That 'an unreadable include creates no shader' (Test-Echo $failed 'SRC_BADINCLUDE=0')
Assert-That 'an unreadable include is named in the log' (Test-Logged $failed 'cannot read config/glsl/harness/noinclude.glsl')
Assert-That 'an empty source file creates no shader' (Test-Echo $failed 'SRC_EMPTYFRAG=0')
Assert-That 'an empty source file is logged' (Test-Logged $failed 'config/glsl/harness/empty.frag is empty')
Assert-That 'shader_define outside a body is reported' (Test-Logged $failed 'only valid inside a shader_new or variantshader_new body')

$dump = Invoke-Batch 'shaderdumpall srcloader s00 0' 1 300
Assert-That 'the dump ran' (@($dump | Where-Object { $_ -match 'SHADERDUMP srcloader s00 \d+ \d+' }).Count -eq 1)
$rows = [System.IO.File]::ReadAllLines((Join-Path $runDir 'manifest.tsv'))
function Get-Blob([string]$Name) {
    $row = @($rows | Where-Object { ($_ -split "`t")[0] -ceq $Name })
    if ($row.Count -ne 1) { return $null }
    return Join-Path $runDir ('blobs\' + ($row[0] -split "`t")[2])
}
function Get-Stage([string]$Name, [string]$File) {
    $blob = Get-Blob $Name
    if (-not $blob) { return $null }
    return [System.IO.File]::ReadAllText((Join-Path $blob $File))
}

Assert-That 'vertex text is the defines then the file' ((Get-Stage "${t}ok" 'vs.glsl') -ceq $expectVs)
Assert-That 'fragment text is the defines, the CR-stripped include, then the file' ((Get-Stage "${t}ok" 'fs.glsl') -ceq $expectFs)
Assert-That 'genfogshader still runs on assembled text (fs)' (Test-Contains (Get-Stage "${t}fog" 'fs.glsl') 'uniform vec3 fogcolor;')
Assert-That 'genfogshader still runs on assembled text (vs)' (Test-Contains (Get-Stage "${t}fog" 'vs.glsl') 'lineardepth = dot(lineardepthscale, gl_Position.zw);')
Assert-That 'texture-slot params still become uniforms (vs)' (Test-Contains (Get-Stage "${t}param" 'vs.glsl') 'uniform vec4 srcparam;')
Assert-That 'texture-slot params still become uniforms (fs)' (Test-Contains (Get-Stage "${t}param" 'fs.glsl') 'uniform vec4 srcparam;')
Assert-That 'the outer body keeps its own defines' ((Get-Stage "${t}outer" 'fs.glsl') -ceq $expectFs)
Assert-That 'the inner body gets none of the outer defines' ((Get-Stage "${t}inner" 'fs.glsl') -ceq $frag)
Assert-That 'shader_source called twice: the last call wins (vs)' ((Get-Stage "${t}twice" 'vs.glsl') -ceq $vert)
Assert-That 'shader_source called twice: the last call wins (fs)' ((Get-Stage "${t}twice" 'fs.glsl') -ceq $frag2)

# variantshader_new. A variant with only a fragment file reuses the parent's
# vertex stage. The dump above ran before these existed, so dump again.
$variants = @"
setshader null
shader_new 0 ${t}parent [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
variantshader_new 0 ${t}parent 1 1 [
    shader_define SRC_MODE 2
    shader_define SRC_FLAG ""
    shader_include_fs "config/glsl/harness/common.glsl"
    shader_source "" "config/glsl/harness/t.frag"
]
echo (concatword "SRC_VARIANT=" (hasshader "<variant:0,1>${t}parent"))
srcvarran = 0
variantshader_new 0 ${t}noparent 1 1 [ srcvarran = 1 ]
echo (concatword "SRC_VARRAN=" `$srcvarran)
variantshader_new 0 ${t}parent 1 1 [ shader_include_vs "config/glsl/harness/common.glsl"; shader_source "" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_VARORPHAN=" (hasshader "<variant:1,1>${t}parent"))
variantshader_new 0 ${t}rowless -1 0 [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/t.frag" ]
echo (concatword "SRC_ROWLESS=" (hasshader ${t}rowless))
variantshader_new 0 ${t}parent 2 1 [ shader_source "config/glsl/harness/t.vert" "config/glsl/harness/nope.frag" ]
echo (concatword "SRC_VARBADPATH=" (hasshader "<variant:0,2>${t}parent"))
variantshader_new 0 ${t}parent 3 1 [ shader_source "" "config/glsl/harness/empty.frag" ]
echo (concatword "SRC_VAREMPTY=" (hasshader "<variant:0,3>${t}parent"))
srcvarrow99ran = 0
variantshader_new 0 ${t}parent 99 1 [ srcvarrow99ran = 1 ]
echo (concatword "SRC_ROW99RAN=" `$srcvarrow99ran)
"@
$v = Invoke-Batch $variants 1 120
Assert-That 'a variant with only a fragment file is created' (Test-Echo $v 'SRC_VARIANT=1')
Assert-That 'no body runs when the parent is missing' (Test-Echo $v 'SRC_VARRAN=0')
Assert-That 'variant includes without a source are refused' ((Test-Echo $v 'SRC_VARORPHAN=0') -and (Test-Logged $v 'includes given but no vertex source'))
Assert-That 'row -1 behaves like shader_new' (Test-Echo $v 'SRC_ROWLESS=1')
Assert-That 'a failed variant body creates no variant' (Test-Echo $v 'SRC_VARBADPATH=0')
Assert-That 'a failed variant body names the variant, not the parent' (Test-Logged $v "<variant row 2>${t}parent: cannot read config/glsl/harness/nope.frag")
Assert-That 'a failed variant build is reported against the variant label' (Test-Logged $v "shader <variant row 2>${t}parent: not created")
Assert-That 'an empty variant source file creates no variant' (Test-Echo $v 'SRC_VAREMPTY=0')
Assert-That 'an empty variant source file is logged against the variant label' (Test-Logged $v "shader <variant row 3>${t}parent: config/glsl/harness/empty.frag is empty")
Assert-That 'row >= MAXVARIANTROWS does not run the body' (Test-Echo $v 'SRC_ROW99RAN=0')

Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue
$null = Invoke-Batch 'shaderdumpall srcloader s00 0' 1 300
$rows = [System.IO.File]::ReadAllLines((Join-Path $runDir 'manifest.tsv'))
Assert-That 'the variant fragment stage is assembled' ((Get-Stage "<variant:0,1>${t}parent" 'fs.glsl') -ceq $expectFs)
Assert-That 'the variant reuses the parent vertex stage' ((Get-Stage "<variant:0,1>${t}parent" 'vs.glsl') -ceq $vert)

}
finally {
    # Cleanup -- always, even if an assertion or Invoke-Batch threw.
    Remove-Item -Recurse -Force $fixDir -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $outsideDir -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $runDir -ErrorAction SilentlyContinue
}

if ($failures) { Write-Host "$failures failure(s)" -ForegroundColor Red; exit 1 }
Write-Host 'All passed' -ForegroundColor Green
exit 0
