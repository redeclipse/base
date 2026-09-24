<#
.SYNOPSIS
    End-to-end self-test for the map editor prefab workflow.

.DESCRIPTION
    Boots a client in the harness home, builds the deterministic 'newmap 12'
    scratch world, and drives the prefab engine commands, the file browser
    widget, the CubeScript prefab library and the Prefabs panel.

    User prefabs live under home/uitest/prefab/ and are wiped first. One
    "shipped" (read-only) prefab is simulated by a file under the repo's own
    prefab/ folder: the game runs with the repo root as its working directory,
    which listfiles and findfile search after the home dir. It is removed on
    exit, along with the folder if this script created it.

.EXAMPLE
    tools\harness\prefab-selftest.ps1
    tools\harness\prefab-selftest.ps1 -KeepRunning
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
$script:createdShippedDir = $false

# Same scratch world as editor-selftest.ps1: 'newmap 12' is 4096 units, solid
# below z = 2048, with an 8-unit grid at the default gridpower.
$Floor = 2048
$Mid   = 2048
$Grid  = 8

$UserDir     = Join-Path $HomeDir 'prefab'
$BackupDir   = Join-Path $HomeDir 'backups\prefab'
$ShippedDir  = Join-Path $RepoRoot 'prefab'
$ShippedFile = Join-Path $ShippedDir 'zz_selftest_shipped.obr'

# ------------------------------------------------------------- reporting ----

function Step([string]$Name, [scriptblock]$Body) {
    $script:step++
    $before = $script:failures
    Write-Host ''
    Write-Host ("[{0}] {1}" -f $script:step, $Name) -ForegroundColor Cyan
    try { & $Body }
    catch {
        Write-Host "     FAIL  $_" -ForegroundColor Red
        $script:failures++
    }
    if ($script:failures -gt $before) {
        Write-Host '     --- editor state at failure ---' -ForegroundColor DarkYellow
        try { & $editor state | Out-Null }
        catch { Write-Host "     (state unavailable: $_)" -ForegroundColor DarkYellow }
    }
}

function Expect([string]$What, $Actual, $Expected) {
    if ("$Actual" -ceq "$Expected") { Write-Host "     ok    $What = '$Actual'" -ForegroundColor Green }
    else {
        Write-Host "     FAIL  $What -- expected '$Expected', got '$Actual'" -ForegroundColor Red
        $script:failures++
    }
}

function ExpectTrue([string]$What, [bool]$Condition, [string]$Detail) {
    if ($Condition) { Write-Host "     ok    $What" -ForegroundColor Green }
    else {
        $msg = "     FAIL  $What"
        if ($Detail) { $msg += " -- $Detail" }
        Write-Host $msg -ForegroundColor Red
        $script:failures++
    }
}

# --------------------------------------------------------------- driving ----

function Ed { & $editor @args 6>$null | Out-Null }

function EdState { return (& $editor state -Raw 6>$null) }

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

# A prefab file's decompressed bytes, for comparison, with the stored origin
# masked out. Decompressed layout: 8-byte prefabheader (magic + version),
# then packblock's block3 header -- ivec o (3 little-endian int32s, bytes
# 8..19), then s/grid/orient, then the cubes. `o` is the absolute world
# position of the selection at save time; pasteblock() never reads it back,
# so two prefabs saved from different selections legitimately differ only in
# those 12 bytes even when their geometry is identical. Zero them so the
# comparison is over everything paste actually reproduces.
function Read-PrefabPayload([string]$Path) {
    $in = [System.IO.File]::OpenRead($Path)
    try {
        $gz = New-Object System.IO.Compression.GZipStream($in, [System.IO.Compression.CompressionMode]::Decompress)
        $ms = New-Object System.IO.MemoryStream
        $gz.CopyTo($ms)
        $bytes = $ms.ToArray()
        for ($i = 8; $i -lt 20 -and $i -lt $bytes.Length; $i++) { $bytes[$i] = 0 }
        return [Convert]::ToBase64String($bytes)
    }
    finally { $in.Dispose() }
}

function UserFile([string]$Rel) { return (Join-Path $UserDir $Rel) }
function BackupFile([string]$Rel) { return (Join-Path $BackupDir $Rel) }

# Click point of the first drawn widget whose text matches $Label (exact
# first, then wildcard), as harness.ps1 find reports it; $null if none.
function Find-Click([string]$Label) {
    $out = @(& $harness find $Label 6>$null)
    foreach ($line in $out) {
        if ("$line" -match 'click\(([-0-9.]+),([-0-9.]+)\)') { return @($Matches[1], $Matches[2]) }
    }
    return $null
}

# Clicks a widget by label the way harness.ps1 click does (move, let a frame
# see the hover, press, release), optionally twice or with the alt button.
# If a synthesised double or alt click does not register while the same UI
# works by hand, that is a harness problem: stash and report, per the plan.
function Invoke-Click([string]$Label, [switch]$Double, [switch]$Right) {
    $pt = Find-Click $Label
    if (-not $pt) { throw "no drawn widget matching '$Label'" }
    Invoke-ClickAt $pt -Double:$Double -Right:$Right
}

function Invoke-ClickAt($Point, [switch]$Double, [switch]$Right) {
    $code = if ($Right) { -3 } else { -1 }
    $second = if ($Double) { "sleep 40 [ uikeypress $code 1; sleep 40 [ uikeypress $code 0 ] ]" } else { '' }
    $script = "uisetcursor $($Point[0]) $($Point[1])`nsleep 100 [ uikeypress $code 1; sleep 40 [ uikeypress $code 0; $second ] ]"
    & $harness send $script -Settle 600 6>$null | Out-Null
}

# Clicks a file browser's "Go up" button. It is an icon without a label, so
# it is found in the tree instead: the nearest button (#BorderedImage) before
# the "Directory:" label, at the same depth. Assumes one file browser is drawn.
function Invoke-GoUp {
    $lines = @(& $harness tree -Drawn 6>$null | ForEach-Object { "$_" })
    $dir = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '#TextString drawn .*\) Directory:\s*$') { $dir = $i; break }
    }
    if ($dir -lt 0) { throw 'no drawn "Directory:" label' }
    $indent = $lines[$dir].Length - $lines[$dir].TrimStart().Length
    for ($i = $dir - 1; $i -ge 0; $i--) {
        $ind = $lines[$i].Length - $lines[$i].TrimStart().Length
        if ($ind -lt $indent) { break }
        if ($ind -eq $indent -and $lines[$i] -match '#BorderedImage drawn .*click\(([-0-9.]+),([-0-9.]+)\)') {
            Invoke-ClickAt @($Matches[1], $Matches[2])
            return
        }
    }
    throw 'no "Go up" button before the "Directory:" label'
}

# The drawn text editors (uifield and friends), left to right, with the text
# each one shows. uidumptree leaves that text out; uidumpeditors reports it.
function Get-DrawnEditors {
    $out = @(& $harness send 'uidumpeditors' 6>$null)
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $eds = @(foreach ($line in $out) {
        if ("$line" -match 'UIEDITOR ([01]) ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) (\S+) ?(.*)$' -and $Matches[1] -eq '1') {
            [pscustomobject]@{
                X    = [double]::Parse($Matches[2], $inv)
                Y    = [double]::Parse($Matches[3], $inv)
                Name = $Matches[6]
                Text = $Matches[7].Trim()
            }
        }
    })
    return @($eds | Sort-Object X)
}

function Open-PrefabPanel {
    Send 'if (toolpanel_isopen tool_prefabs) [] [tool_do_action ta_prefabs]' 500
}

# --------------------------------------------------------------------------

foreach ($dir in @($UserDir, $BackupDir)) {
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
}

& $harness stop 6>$null | Out-Null
& $harness start 6>$null | Out-Null

try {
    Step 'setup: scratch world, camera over the test area' {
        Invoke-MapLoad { Ed newmap 12 }
        Ed frame $Mid $Mid $Floor -Dist 256 -Yaw 0 -Pitch -45
        Expect 'edit mode' (EdState).Mode.EditMode 1
    }

    # ==== panel: empty library ============================================

    Step 'panel: empty library' {
        Ed cursor on
        Open-PrefabPanel
        Expect 'panel is open' (Eval '(toolpanel_isopen tool_prefabs)') '1'
        ExpectTrue 'empty-state text is shown' ($null -ne (Find-Click 'No prefabs yet.'))
        & $harness shot prefab-empty 6>$null | Out-Null
        Send 'toolpanel_close tool_prefabs' 300
        Ed cursor off
    }

    # ==== engine commands =================================================

    Step 'saveprefab writes a user prefab; prefabinfo reports it' {
        # Two cubes deep, straddling the floor surface: solid below, air above.
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2,2
        Send 'saveprefab prefab/st/box'
        ExpectTrue 'file written under the home prefab dir' (Test-Path (UserFile 'st\box.obr'))
        Expect 'prefabinfo' (Eval '(prefabinfo prefab/st/box)') '2 2 2 8 1'
        Expect 'prefabinfo of a missing prefab' (Eval '(prefabinfo prefab/st/nothing)') ''
    }

    Step 'copyprefab + paste reproduces the geometry exactly' {
        Send 'copyprefab prefab/st/box'
        # Up in the air, clear of the floor, so the paste is all that is there.
        Ed sel ($Mid + 64) $Mid ($Floor + 64) -Size 2,2,2
        Send 'pastehilight; paste'
        Send 'saveprefab prefab/st/rt'
        $a = Read-PrefabPayload (UserFile 'st\box.obr')
        $b = Read-PrefabPayload (UserFile 'st\rt.obr')
        ExpectTrue 'decompressed files are identical apart from the stored origin' ($a -eq $b) "base64 lengths $($a.Length) vs $($b.Length)"
    }

    Step 'copyprefab: works with the selection out of view, replaces copied entities' {
        # Put an entity on the engine's entity clipboard first.
        Send 'cancelsel'
        Ed sel $Mid $Mid $Floor
        Send 'newent playerstart'
        ExpectTrue 'an entity is selected to copy' ((EdState).EntSel.Count -eq 1)
        Send 'entcopy; entcancel; pasteclear; cancelsel'
        # Look at the sky: the stale selection box is out of view.
        Ed aim 180 80
        Send 'copyprefab prefab/st/box'
        Send 'pastehilight'
        $s = EdState
        Expect 'pastehilight took the prefab size (x)' $s.Sel.SX 2
        Expect 'pastehilight took the prefab size (y)' $s.Sel.SY 2
        Expect 'pastehilight took the prefab size (z)' $s.Sel.SZ 2
        Send 'entpaste'
        Expect 'no stale entities pasted with the prefab' (EdState).EntSel.Count 0
        Ed frame $Mid $Mid $Floor -Dist 256 -Yaw 0 -Pitch -45
    }

    Step 'saveprefab over an existing prefab replaces it' {
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 3,1,1
        Send 'saveprefab prefab/st/box'
        Expect 'prefabinfo reflects the new selection' (Eval '(prefabinfo prefab/st/box)') '3 1 1 8 1'
    }

    Step 'renameprefab' {
        Expect 'rename into a new folder' (Eval '(renameprefab prefab/st/rt prefab/st2/rt2)') '1'
        ExpectTrue 'source file is gone' (-not (Test-Path (UserFile 'st\rt.obr')))
        ExpectTrue 'target file exists' (Test-Path (UserFile 'st2\rt2.obr'))
        Expect 'rename onto an existing prefab is refused' (Eval '(renameprefab prefab/st2/rt2 prefab/st/box)') '0'
        ExpectTrue 'both files survive the refused rename' ((Test-Path (UserFile 'st2\rt2.obr')) -and (Test-Path (UserFile 'st\box.obr')))
        Expect 'a target outside prefab/ is refused' (Eval '(renameprefab prefab/st2/rt2 st2/rt2)') '0'
        Expect 'a target with .. is refused' (Eval '(renameprefab prefab/st2/rt2 prefab/../rt2)') '0'
        Expect 'a target segment ending in . is refused' (Eval '(renameprefab prefab/st2/rt2 prefab/st2./rt2)') '0'
        # Valid on its own (500 chars: it fits a MAXSTRLEN string with ".obr"),
        # but not once the home dir is prefixed: refused rather than truncated.
        # Windows would refuse a path that long anyway (MAX_PATH), so the
        # console message is what shows the guard, not the OS, refused it.
        $long = 'prefab/' + ('a' * 493)
        $out = @(& $harness send "echo (concatword ""SELFTEST_EVAL="" (renameprefab prefab/st2/rt2 $long))" 6>$null)
        $res = $null
        foreach ($line in $out) { if ("$line" -match 'SELFTEST_EVAL=(.*)$') { $res = $Matches[1].Trim() } }
        Expect 'a target too long for the home dir is refused' $res '0'
        ExpectTrue 'by the length guard' (@($out | Where-Object { "$_" -match 'Prefab path too long' }).Count -gt 0) ($out -join ' | ')
        ExpectTrue 'the source survives the refused long rename' (Test-Path (UserFile 'st2\rt2.obr'))
        Expect 'a case-only rename is allowed' (Eval '(renameprefab prefab/st2/rt2 prefab/st2/RT2)') '1'
        $names = @(Get-ChildItem (UserFile 'st2') | ForEach-Object { $_.Name })
        ExpectTrue 'the file took the new case' ($names -ccontains 'RT2.obr') "files: $($names -join ', ')"
        Expect 'prefabinfo under the new name' (Eval '(prefabinfo prefab/st2/RT2)') '2 2 2 8 1'
    }

    Step 'removeprefab moves the file to backups/' {
        Expect 'remove' (Eval '(removeprefab prefab/st2/RT2)') '1'
        ExpectTrue 'file left the prefab dir' (-not (Test-Path (UserFile 'st2\RT2.obr')))
        ExpectTrue 'file is in backups' (Test-Path (BackupFile 'st2\RT2.obr'))
        Expect 'prefabinfo no longer finds it' (Eval '(prefabinfo prefab/st2/RT2)') ''
        Expect 'removing it again fails' (Eval '(removeprefab prefab/st2/RT2)') '0'
        Expect 'a bare name is refused' (Eval '(removeprefab box)') '0'
    }

    Step 'a prefab outside the home dir is read-only' {
        if (-not (Test-Path $ShippedDir)) {
            New-Item -ItemType Directory -Path $ShippedDir | Out-Null
            $script:createdShippedDir = $true
        }
        Copy-Item (UserFile 'st\box.obr') $ShippedFile
        Expect 'prefabinfo marks it shipped' (Eval '(prefabinfo prefab/zz_selftest_shipped)') '3 1 1 8 0'
        Expect 'remove is refused' (Eval '(removeprefab prefab/zz_selftest_shipped)') '0'
        Expect 'rename is refused' (Eval '(renameprefab prefab/zz_selftest_shipped prefab/zz2)') '0'
        ExpectTrue 'the file is untouched' (Test-Path $ShippedFile)
        Expect 'a user prefab cannot be renamed over it' (Eval '(renameprefab prefab/st/box prefab/zz_selftest_shipped)') '0'
    }

    # Not covered here, by design: a selection over 100 MB (saveprefab's NULL
    # guard) and IDF_MAP refusal are unreachable from the harness. Review them.

    # ==== file browser widget =============================================

    Step 'file browser: prefab type and helpers' {
        Expect 'prefab type extension' (Eval '(tool_file_type_exts $TOOL_FILE_PREFAB)') 'obr'
        Expect 'dedup keeps one of each' (Eval '(listlen (tool_filelist_dedup "a b a c b"))') '3'
        Expect 'dedup keeps the first order' (Eval '(at (tool_filelist_dedup "a b a c b") 2)') 'c'
        Send 'tool_filelist_curdir = ""'
        Expect 'relpath at the root' (Eval '(tool_filelist_relpath oak)') 'oak'
        Expect 'dirpath at the root' (Eval '(tool_filelist_dirpath prefab)') 'prefab'
        Send 'tool_filelist_curdir = trees'
        Expect 'relpath in a folder' (Eval '(tool_filelist_relpath oak)') 'trees/oak'
        Expect 'dirpath in a folder' (Eval '(tool_filelist_dirpath prefab)') 'prefab/trees'
        Send 'tool_filelist_curdir = ""'
    }

    Step 'file browser: per-instance state' {
        Send 'tool_filelist_curdir = sounds; tool_filelist_filter_query = x; tool_filelist_instance_enter pfltest'
        Expect 'a fresh instance starts at the root' (Eval '$tool_filelist_curdir') ''
        Expect 'a fresh instance has no selection' (Eval '$tool_filelist_sel_index') '-1'
        # The search query is not swapped: the engine keeps one text editor
        # (and its focus) per variable name, so an instance searches through
        # a variable of its own instead.
        Expect 'the plain query is left alone' (Eval '$tool_filelist_filter_query') 'x'
        Send 'tool_filelist_curdir = st; tool_filelist_instance_leave pfltest'
        Expect "leave restores the caller's directory" (Eval '$tool_filelist_curdir') 'sounds'
        Expect "the caller's query is untouched" (Eval '$tool_filelist_filter_query') 'x'
        Expect 'the query is not stashed per instance' (Eval '(identexists tool_filelist_pfltest_filter_query)') '0'
        Expect 'leave keeps the instance directory' (Eval '(getalias tool_filelist_pfltest_curdir)') 'st'
        Send 'tool_filelist_instance_enter pfltest'
        Expect 're-entering restores the instance directory' (Eval '$tool_filelist_curdir') 'st'
        Send 'tool_filelist_instance_leave pfltest; tool_filelist_curdir = ""; tool_filelist_filter_query = ""'

        Expect 'search variable without an instance' (Eval '(tool_filelist_query_var "")') 'tool_filelist_filter_query'
        Expect 'search variable of an instance' (Eval '(tool_filelist_query_var prefab)') 'tool_filelist_prefab_filter_query'
        Expect 'filter: no query shows everything' (Eval '(tool_filelist_filter sounds "")') '1'
        Expect 'filter: a match, any case' (Eval '(tool_filelist_filter Sounds oun)') '1'
        Expect 'filter: no match' (Eval '(tool_filelist_filter sounds zz)') '0'
    }

    Step 'file browser: an existing picker still browses data/' {
        Ed cursor on
        Send 'tool_filelist_curdir = ""; toolpanel_open tool_fileselect_picker center [p_title = "File browser"; p_width = (uiwidth 0.25); p_user_data = [p_var = mapmusic; p_file_type = 2; p_width = (uiwidth 0.25)]]' 600
        # data/ has ~40 top-level folders and "sounds" sorts past what fits in
        # the unscrolled root view (uidumptree only reports what was actually
        # laid out this frame, nothing phantom-scrolled-off); narrow the
        # picker's own search to bring it into view, then clear the search
        # again before the root-view screenshot below.
        Send 'tool_filelist_filter_query = sounds' 400
        ExpectTrue 'the data/sounds folder is listed' ($null -ne (Find-Click 'sounds'))
        Send 'tool_filelist_filter_query = ""' 400
        ExpectTrue 'the OK/Cancel footer is shown' ($null -ne (Find-Click 'Cancel'))
        # Look at this one: "Go up" (arrow icon, top left) must be greyed out at the root.
        & $harness shot prefab-picker-root 6>$null | Out-Null
        Send 'toolpanel_close tool_fileselect_picker' 300
        Ed cursor off
    }

    # The spec's "existing pickers" checks, through the real ui_tool_fileselect
    # controls: their Browse sets tool_filelist_curdir from the current value
    # (or p_dir) and the picker lists it through tool_filelist_dirpath.
    Step 'file browser: the mapmusic control browses data/ and OK selects' {
        $prev = Eval '$mapmusic'
        Ed cursor on
        try {
            Send 'tool_do_action ta_mapsettings' 600
            # The control shows the value, or "Random" when there is none
            $label = if ($prev) { $prev } else { 'Random' }
            Invoke-Click $label
            Expect 'Browse opened the picker' (Eval '(toolpanel_isopen tool_fileselect_picker)') '1'
            Expect "the picker starts in the control's folder" (Eval '$tool_filelist_curdir') 'sounds/music'
            ExpectTrue 'it lists data/sounds/music' ($null -ne (Find-Click 'track_01.ogg'))
            Invoke-GoUp
            Invoke-GoUp
            Expect 'Go up reaches the root' (Eval '$tool_filelist_curdir') ''
            # See the step above: narrow the search to bring "sounds" into view
            Send 'tool_filelist_filter_query = sounds' 400
            ExpectTrue 'the root lists the data/ folders' ($null -ne (Find-Click 'sounds'))
            Invoke-Click 'sounds' -Double
            Expect 'double-click entered sounds' (Eval '$tool_filelist_curdir') 'sounds'
            Send 'tool_filelist_filter_query = music' 400
            Invoke-Click 'music' -Double
            Send 'tool_filelist_filter_query = ""' 400
            Expect 'double-click entered sounds/music' (Eval '$tool_filelist_curdir') 'sounds/music'
            Invoke-Click 'track_02.ogg'
            Invoke-Click 'Ok'
            Expect 'OK set mapmusic' (Eval '$mapmusic') 'sounds/music/track_02.ogg'
            Expect 'OK closed the picker' (Eval '(toolpanel_isopen tool_fileselect_picker)') '0'
        }
        finally {
            Send ('toolpanel_close tool_fileselect_picker; toolpanel_close tool_map_settings; tool_filelist_filter_query = ""; mapmusic "' + $prev + '"') 400
            Ed cursor off
        }
        Expect 'mapmusic restored' (Eval '$mapmusic') $prev
    }

    Step 'file browser: the hazetex control browses data/ and OK selects' {
        $prev = Eval '$hazetex'
        $prevTab = Eval '(getalias tool_env_tabs)'
        if (-not $prevTab) { $prevTab = '0' }
        Ed cursor on
        try {
            # Environment panel, Haze tab (index 8 in ui_tool_env_view)
            Send 'tool_env_tabs = 8; tool_do_action ta_env' 700
            # The Default column's control comes first (the Alternate one shows
            # the same value further right)
            Invoke-Click $prev
            Expect 'Browse opened the picker' (Eval '(toolpanel_isopen tool_fileselect_picker)') '1'
            Expect "the picker starts in the value's folder" (Eval '$tool_filelist_curdir') 'textures'
            Invoke-GoUp
            Expect 'Go up reaches the root' (Eval '$tool_filelist_curdir') ''
            Send 'tool_filelist_filter_query = textures' 400
            ExpectTrue 'the root lists the data/ folders' ($null -ne (Find-Click 'textures'))
            Invoke-Click 'textures' -Double
            Expect 'double-click entered textures' (Eval '$tool_filelist_curdir') 'textures'
            Send 'tool_filelist_filter_query = waterfalln' 400
            Invoke-Click 'waterfalln'
            Invoke-Click 'Ok'
            Expect 'OK set hazetex' (Eval '$hazetex') 'textures/waterfalln'
            Expect 'OK closed the picker' (Eval '(toolpanel_isopen tool_fileselect_picker)') '0'
        }
        finally {
            Send ('toolpanel_close tool_fileselect_picker; toolpanel_close tool_env; tool_filelist_filter_query = ""; tool_env_tabs = "' + $prevTab + '"; hazetex "' + $prev + '"') 400
            Ed cursor off
        }
        Expect 'hazetex restored' (Eval '$hazetex') $prev
    }

    # ==== prefab library ==================================================

    Step 'library: name and folder validation' {
        Expect 'plain name' (Eval '(tool_prefab_name_valid "oak_2-b.c")') '1'
        Expect 'empty name' (Eval '(tool_prefab_name_valid "")') '0'
        Expect 'dot-dot' (Eval '(tool_prefab_name_valid "..")') '0'
        Expect 'space' (Eval '(tool_prefab_name_valid "o k")') '0'
        Expect 'slash' (Eval '(tool_prefab_name_valid "a/b")') '0'
        Expect 'trailing dot (Windows strips it)' (Eval '(tool_prefab_name_valid "oak.")') '0'
        Expect 'leading dot (the browser hides it)' (Eval '(tool_prefab_name_valid ".oak")') '0'
        Expect 'dot in the middle' (Eval '(tool_prefab_name_valid "o.ak")') '1'
        Expect 'root folder' (Eval '(tool_prefab_folder_valid "")') '1'
        Expect 'nested folder' (Eval '(tool_prefab_folder_valid "a/b")') '1'
        Expect 'double slash' (Eval '(tool_prefab_folder_valid "a//b")') '0'
        Expect 'leading slash' (Eval '(tool_prefab_folder_valid "/a")') '0'
        Expect 'trailing slash' (Eval '(tool_prefab_folder_valid "a/")') '0'
        Expect 'space in folder' (Eval '(tool_prefab_folder_valid "a b")') '0'
        Expect 'hidden folder' (Eval '(tool_prefab_folder_valid "a/.b")') '0'
    }

    Step 'library: paths' {
        Expect 'root path' (Eval '(tool_prefab_path "" oak)') 'prefab/oak'
        Expect 'folder path' (Eval '(tool_prefab_path trees oak)') 'prefab/trees/oak'
        Expect 'folder of a nested path' (Eval '(tool_prefab_path_folder prefab/trees/big/oak)') 'trees/big'
        Expect 'folder of a root path' (Eval '(tool_prefab_path_folder prefab/oak)') ''
        Expect 'name of a path' (Eval '(tool_prefab_path_name prefab/trees/oak)') 'oak'
        Expect 'top-level folders' (Eval '(tool_prefab_list_folders)') '"st" "st2"'
    }

    Step 'library: actions are registered' {
        Expect 'Prefabs category' (Eval '$tool_actions_Prefabs') 'ta_prefabs ta_prefab_save ta_prefab_load'
    }

    Step 'library: save, load, rename, delete through the wrappers' {
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 1,1,3
        Send 'tool_prefab_save prefab/lib/one'
        ExpectTrue 'saved' (Test-Path (UserFile 'lib\one.obr'))
        Expect 'the new prefab is selected' (Eval '$tool_prefab_cur') 'prefab/lib/one'
        Expect 'its info is cached' (Eval '$tool_prefab_cur_info') '1 1 3 8 1'
        Expect 'a rescan is requested' (Eval '$tool_prefab_needs_rescan') '1'

        Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,2,2
        Send 'entcopybuf = "playerstart 0"; pasteclear; tool_prefab_load prefab/lib/one'
        Expect 'loading clears the script entity clipboard' (Eval '$entcopybuf') ''
        Send 'cancelsel; pastehilight'
        Expect 'the clipboard holds the prefab' (EdState).Sel.SZ 3

        Send 'tool_prefab_rename prefab/lib/one prefab/lib/two'
        ExpectTrue 'renamed on disk' (Test-Path (UserFile 'lib\two.obr'))
        Expect 'the selection follows the rename' (Eval '$tool_prefab_cur') 'prefab/lib/two'

        # tool_prefab_remove asks first; its confirmed half does the work.
        Send 'tool_prefab_pending = prefab/lib/two; tool_prefab_remove_confirmed'
        ExpectTrue 'moved to backups' (Test-Path (BackupFile 'lib\two.obr'))
        Expect 'the selection is cleared' (Eval '$tool_prefab_cur') ''
    }

    Step 'library: save reports a write failure without touching the old file' {
        $roFile = UserFile 'lib\ro.obr'
        try {
            Ed sel $Mid $Mid ($Floor - $Grid) -Size 2,1,1
            Send 'tool_prefab_save prefab/lib/ro'
            ExpectTrue 'saved' (Test-Path $roFile)
            Expect 'prefabinfo before the failed overwrite' (Eval '(prefabinfo prefab/lib/ro)') '2 1 1 8 1'

            Set-ItemProperty $roFile -Name IsReadOnly -Value $true

            Ed sel $Mid $Mid ($Floor - $Grid) -Size 3,3,1
            Send 'tool_prefab_save prefab/lib/ro'
            Expect 'the failure is reported' (Eval '$tool_info_text') 'Could not save prefab'
            Expect 'prefabinfo still reports the old size' (Eval '(prefabinfo prefab/lib/ro)') '2 1 1 8 1'
        }
        finally {
            if (Test-Path $roFile) {
                Set-ItemProperty $roFile -Name IsReadOnly -Value $false -ErrorAction SilentlyContinue
                Remove-Item -Force $roFile -ErrorAction SilentlyContinue
            }
        }
    }

    # ==== prefabs panel ===================================================

    Step 'panel: toolbar button and default binds' {
        Expect 'toolbar has the Prefabs button' (Eval '(listhas $toolbar_actions_right ta_prefabs)') '1'
        # Checked in the file, not by exec'ing the preset: that would rebind
        # the harness home's keys for every later suite.
        $binds = Join-Path $RepoRoot 'config\tool\binds\default.cfg'
        ExpectTrue 'default preset binds I to the panel' (Select-String -Path $binds -Pattern '^toolbind I ta_prefabs$' -Quiet)
        $taken = @(Select-String -Path (Join-Path $RepoRoot 'config\setup.cfg'), $binds -Pattern '^\s*(tool|edit)bind I\b')
        Expect 'nothing else binds I in edit mode' $taken.Count 1
    }

    Step 'panel: browse into a folder, select, search' {
        Ed cursor on
        Send 'tool_filelist_prefab_curdir = ""; tool_prefab_select ""'
        Open-PrefabPanel
        ExpectTrue 'folder st is listed' ($null -ne (Find-Click 'st'))
        Invoke-Click 'st' -Double
        Expect 'double-click entered the folder' (Eval '(getalias tool_filelist_prefab_curdir)') 'st'
        # Search before selecting: once box is selected the header prints its
        # full path ("st/box"), which itself contains the needle "box" -- a
        # search check done afterwards would find that label instead of the
        # (correctly hidden) tile and pass for the wrong reason.
        ExpectTrue 'box tile is listed' ($null -ne (Find-Click 'box'))
        Send 'tool_filelist_prefab_filter_query = zz' 400
        ExpectTrue 'search hides non-matching tiles' ($null -eq (Find-Click 'box'))
        Send 'tool_filelist_prefab_filter_query = ""' 400
        ExpectTrue 'clearing the search shows it again' ($null -ne (Find-Click 'box'))
        Invoke-Click 'box'
        Expect 'click selects the prefab' (Eval '$tool_prefab_cur') 'prefab/st/box'
        # Double-click ids are global: the instance keeps a click here and one
        # on the same index in a picker from counting as a double-click
        Expect "the tile's double-click id names the instance" (Eval '$ui_last_press_id') 'tool_filelist_item_prefab_f_0'
        Expect 'the header has its info' (Eval '$tool_prefab_cur_info') '3 1 1 8 1'
        # Look at this one: the header preview must be a 3x1x1 bar (the
        # overwrite in the engine steps), not the original 2x2x2 block.
        & $harness shot prefab-panel 6>$null | Out-Null
    }

    Step 'panel: double-click loads the prefab to the clipboard' {
        Send 'pasteclear; cancelsel'
        Invoke-Click 'box' -Double
        Send 'pastehilight'
        $s = EdState
        Expect 'clipboard size x' $s.Sel.SX 3
        Expect 'clipboard size y' $s.Sel.SY 1
        Expect 'clipboard size z' $s.Sel.SZ 1
    }

    Step 'panel: the context menu respects read-only prefabs' {
        Invoke-Click 'box' -Right
        Expect 'menu opened' (Eval '(toolpanel_isopen toolpanel_menu)') '1'
        Expect 'Rename enabled for a user prefab' (Eval '(tool_prefab_menu_disabled 2)') '0'
        Expect 'Delete enabled for a user prefab' (Eval '(tool_prefab_menu_disabled 3)') '0'
        Send 'toolpanel_close toolpanel_menu' 300
        Send 'tool_filelist_prefab_curdir = ""; tool_prefab_rescan' 400
        Invoke-Click 'zz_selftest_shipped' -Right
        Expect 'Overwrite disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 1)') '1'
        Expect 'Rename disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 2)') '1'
        Expect 'Delete disabled for a shipped prefab' (Eval '(tool_prefab_menu_disabled 3)') '1'
        # Look at this one: three greyed-out items.
        & $harness shot prefab-menu-shipped 6>$null | Out-Null
        Send 'toolpanel_close toolpanel_menu' 300
    }

    Step 'panel: the save popup saves, and offers Overwrite for an existing name' {
        Send 'tool_filelist_prefab_curdir = st; tool_prefab_rescan' 400
        Ed sel $Mid $Mid ($Floor - $Grid) -Size 1,2,1
        Invoke-Click 'Save...'
        Expect 'popup opened' (Eval '(toolpanel_isopen tool_prefab_save)') '1'
        Expect 'folder defaults to the browser folder' (Eval '(tool_prefab_form_get_folder)') 'st'
        Send 'tool_prefab_form_name = box' 300
        ExpectTrue 'an existing name offers Overwrite' ($null -ne (Find-Click 'Overwrite'))
        & $harness shot prefab-save-overwrite 6>$null | Out-Null
        Send 'tool_prefab_form_name = newone' 300
        Invoke-Click 'Save'
        ExpectTrue 'saved' (Test-Path (UserFile 'st\newone.obr'))
        Expect 'the new prefab is selected' (Eval '$tool_prefab_cur') 'prefab/st/newone'
        Expect 'popup closed' (Eval '(toolpanel_isopen tool_prefab_save)') '0'
    }

    Step 'panel: the rename popup renames, and refuses clashes' {
        Send 'toolpanel_open tool_prefab_rename popup [p_position = [0.4 0.3]; p_width = 0.3]' 400
        Expect 'name prefilled' (Eval '$tool_prefab_form_name') 'newone'
        Expect 'folder prefilled' (Eval '(tool_prefab_form_get_folder)') 'st'
        Send 'tool_prefab_form_name = box' 300
        ExpectTrue 'the clash is reported' ($null -ne (Find-Click 'A prefab with that name already exists'))
        Send 'tool_prefab_form_name = renamed' 300
        Invoke-Click 'Rename'
        ExpectTrue 'renamed on disk' (Test-Path (UserFile 'st\renamed.obr'))
        Expect 'the selection follows' (Eval '$tool_prefab_cur') 'prefab/st/renamed'
    }

    Step 'panel: delete asks, then moves the file to backups/' {
        Send 'tool_prefab_remove prefab/st/renamed' 400
        ExpectTrue 'nothing happens before confirming' (Test-Path (UserFile 'st\renamed.obr'))
        Invoke-Click 'Confirm'
        ExpectTrue 'moved to backups' (Test-Path (BackupFile 'st\renamed.obr'))
        ExpectTrue 'gone from the library' (-not (Test-Path (UserFile 'st\renamed.obr')))
    }

    Step 'panel: the browser keeps its own state beside a file picker' {
        Send 'tool_filelist_prefab_curdir = st; tool_prefab_rescan' 400
        Send 'tool_filelist_curdir = ""; toolpanel_open tool_fileselect_picker center [p_title = "File browser"; p_width = (uiwidth 0.25); p_user_data = [p_var = mapmusic; p_file_type = 2; p_width = (uiwidth 0.25)]]' 600
        # data/ has 40-odd top-level folders and "sounds" sorts past the
        # unscrolled page; narrow the picker's own search to bring it into
        # view instead of teaching the harness to scroll.
        Send 'tool_filelist_filter_query = sounds' 400

        # Each search field shows its own query. The picker is centred and
        # the Prefabs panel docked right, so left to right: picker, Prefabs.
        $eds = @(Get-DrawnEditors)
        Expect 'two search fields are drawn' $eds.Count 2
        if ($eds.Count -eq 2) {
            Expect "the picker's search field" $eds[0].Text 'sounds'
            Expect "the Prefabs panel's search field is still empty" $eds[1].Text ''
            Expect "the Prefabs panel's field has its own editor" $eds[1].Name 'tool_filelist_prefab_filter_query'
        }
        Send 'tool_filelist_prefab_filter_query = zz' 400
        $eds = @(Get-DrawnEditors)
        if ($eds.Count -eq 2) {
            Expect "the Prefabs panel's search field" $eds[1].Text 'zz'
            Expect "the picker's search field is unchanged" $eds[0].Text 'sounds'
        }
        ExpectTrue "the Prefabs query does not filter the picker" ($null -ne (Find-Click 'sounds'))
        Send 'tool_filelist_prefab_filter_query = ""' 400

        Invoke-Click 'sounds' -Double
        Expect 'the picker navigated' (Eval '$tool_filelist_curdir') 'sounds'
        # Nothing in data/sounds matches "sounds": the empty list must keep
        # its height, not push the footer out of the panel
        ExpectTrue 'an empty list keeps the OK/Cancel footer' ($null -ne (Find-Click 'Cancel'))
        Expect "a picker tile's double-click id has no instance" (Eval '(strstr $ui_last_press_id "tool_filelist_item_d_")') '0'
        Expect 'the prefab browser did not move' (Eval '(getalias tool_filelist_prefab_curdir)') 'st'
        ExpectTrue 'the prefab browser still shows its tiles' ($null -ne (Find-Click 'box'))
        # Look at this one: the picker's search reads "sounds", the Prefabs
        # panel's is empty (its "[Enter text here]" prompt only).
        & $harness shot prefab-with-picker 6>$null | Out-Null
        Send 'toolpanel_close tool_fileselect_picker' 300
        Ed cursor off
    }
}
finally {
    if (Test-Path $ShippedFile) { Remove-Item -Force $ShippedFile }
    if ($script:createdShippedDir -and (Test-Path $ShippedDir) -and -not (Get-ChildItem $ShippedDir)) {
        Remove-Item -Force $ShippedDir
    }
    Write-Host ''
    if (-not $KeepRunning) { & $harness stop 6>$null | Out-Null }
}

if ($script:failures) {
    Write-Host "$script:failures check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'prefab self-test: all checks passed' -ForegroundColor Green
exit 0
