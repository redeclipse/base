# Pure helpers for the shader equivalence harness (shaders.ps1): sweep file
# and corpus parsing, the tier-0 contract comparison, self-checks and report
# formatting. Nothing here talks to the game, so
# tests/shadercorpus.tests.ps1 runs without one.
#
# Shader names can differ only by case, and PowerShell's @{} and -eq are
# case-insensitive: key maps with New-OrdinalMap and compare with -ceq.

function New-OrdinalMap { return New-Object System.Collections.Hashtable ([StringComparer]::Ordinal) }

function Read-Sweep([string]$Path) {
    $points = New-Object System.Collections.Generic.List[object]
    $seen = New-OrdinalMap
    $n = 0
    foreach ($raw in [System.IO.File]::ReadAllLines($Path)) {
        $n++
        $line = ($raw -replace '#.*$', '').Trim()
        if (-not $line) { continue }
        $parts = @($line -split '\s+')
        $id = $parts[0]
        if ($id -notmatch '^[A-Za-z0-9_-]+$') { throw "${Path}:${n}: bad settings id '$id'" }
        if ($id.StartsWith('m-')) { throw "${Path}:${n}: ids starting 'm-' are reserved for the per-map pass" }
        if ($seen.ContainsKey($id)) { throw "${Path}:${n}: duplicate settings id '$id'" }
        $seen[$id] = $true
        $settings = [ordered]@{}
        foreach ($p in @($parts | Select-Object -Skip 1)) {
            if ($p -notmatch '^([A-Za-z_][A-Za-z0-9_]*)=(-?[0-9]+(\.[0-9]+)?)$') { throw "${Path}:${n}: expected var=number, got '$p'" }
            $settings[$Matches[1]] = $Matches[2]
        }
        $points.Add([pscustomobject]@{ Id = $id; Settings = $settings })
    }
    return $points.ToArray()
}

function Get-SweepVars($Points) {
    $vars = New-Object System.Collections.Generic.List[string]
    foreach ($p in $Points) { foreach ($k in $p.Settings.Keys) { if (-not $vars.Contains($k)) { $vars.Add($k) } } }
    return , $vars.ToArray()
}

function Format-Settings($Settings) {
    return (@($Settings.Keys | ForEach-Object { "$_=$($Settings[$_])" }) -join ' ')
}

function Read-RunInfo([string]$RunDir) {
    $path = Join-Path $RunDir 'run.txt'
    if (-not (Test-Path $path)) { throw "No run.txt in $RunDir -- not a recorded corpus?" }
    $info = New-OrdinalMap
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        $i = $line.IndexOf(' ')
        if ($i -gt 0) { $info[$line.Substring(0, $i)] = $line.Substring($i + 1) }
        elseif ($line) { $info[$line] = '' }
    }
    return $info
}

function Read-RunPoints([string]$RunDir) {
    $info = Read-RunInfo $RunDir
    foreach ($id in @($info['sweep'] -split ' ' | Where-Object { $_ })) {
        $settings = [ordered]@{}
        foreach ($line in [System.IO.File]::ReadAllLines((Join-Path $RunDir "settings\$id.txt"))) {
            if ($line -match '^([A-Za-z_][A-Za-z0-9_]*)=(\S+)$') { $settings[$Matches[1]] = $Matches[2] }
        }
        [pscustomobject]@{ Id = $id; Settings = $settings }
    }
}

function Read-Manifest([string]$Dir) {
    $path = Join-Path $Dir 'manifest.tsv'
    if (-not (Test-Path $path)) { throw "No manifest: $path" }
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        if (-not $line) { continue }
        $f = $line -split "`t"
        if ($f.Count -ne 4) { throw "Malformed manifest row in ${path}: $line" }
        [pscustomobject]@{ Name = $f[0]; Sid = $f[1]; Hash = $f[2]; Origin = $f[3]; Key = "$($f[1])`t$($f[0])" }
    }
}

# Invalid rows ('-') are treated as absent: resetshaders leaves stubs of
# shaders generated at earlier sweep points, so they are not comparable.
function Get-ValidRowMap($Rows) {
    $map = New-OrdinalMap
    foreach ($r in $Rows) { if ($r.Hash -cne '-') { $map[$r.Key] = $r } }
    return $map
}

function Get-BlobLines([string]$Path) {
    if (-not (Test-Path $Path)) { return , @() }
    return , @([System.IO.File]::ReadAllLines($Path))
}

function Get-ContractDiff([string]$BaseBlob, [string]$CandBlob) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($file in 'meta.txt', 'reflect.txt') {
        $section = [System.IO.Path]::GetFileNameWithoutExtension($file)
        $a = Get-BlobLines (Join-Path $BaseBlob $file)
        $b = Get-BlobLines (Join-Path $CandBlob $file)
        if (($a -join "`n") -ceq ($b -join "`n")) { continue }
        $setA = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        $setB = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        foreach ($l in $a) { [void]$setA.Add($l) }
        foreach ($l in $b) { [void]$setB.Add($l) }
        $before = $out.Count
        foreach ($l in $a) { if (-not $setB.Contains($l)) { $out.Add("${section}: -$l") } }
        foreach ($l in $b) { if (-not $setA.Contains($l)) { $out.Add("${section}: +$l") } }
        if ($out.Count -eq $before) { $out.Add("${section}: order changed") }
    }
    return , $out.ToArray()
}

function New-Result([string]$Status, $Row, [string]$BaseHash, [string]$CandHash, [string]$Detail) {
    return [pscustomobject]@{ Status = $Status; Name = $Row.Name; Sid = $Row.Sid; BaseHash = $BaseHash; CandHash = $CandHash; Detail = $Detail }
}

function Compare-Corpus {
    param([string]$BaseDir, [string]$CandDir, [string]$Filter = '*', [string[]]$Sids, [switch]$SkipContract)
    $base = Get-ValidRowMap (Read-Manifest $BaseDir)
    $cand = Get-ValidRowMap (Read-Manifest $CandDir)
    $keys = New-Object 'System.Collections.Generic.SortedSet[string]' ([StringComparer]::Ordinal)
    foreach ($k in $base.Keys) { [void]$keys.Add($k) }
    foreach ($k in $cand.Keys) { [void]$keys.Add($k) }
    foreach ($k in $keys) {
        $b = $base[$k]; $c = $cand[$k]
        $row = if ($b) { $b } else { $c }
        if ($Sids -and $Sids -cnotcontains $row.Sid) { continue }
        if ($row.Name -notlike $Filter) { continue }
        if (-not $c) { New-Result 'MISSING' $row $b.Hash '' 'no valid shader in the candidate'; continue }
        if (-not $b) { New-Result 'EXTRA' $row '' $c.Hash 'not in the baseline'; continue }
        if ($b.Hash -ceq $c.Hash) { New-Result 'PASS-TEXT' $row $b.Hash $c.Hash 'identical'; continue }
        if (-not $SkipContract) {
            $diff = Get-ContractDiff (Join-Path $BaseDir "blobs\$($b.Hash)") (Join-Path $CandDir "blobs\$($c.Hash)")
            if ($diff.Count) {
                $shown = @($diff | Select-Object -First 3) -join '; '
                if ($diff.Count -gt 3) { $shown += "; (+$($diff.Count - 3) more)" }
                New-Result 'FAIL' $row $b.Hash $c.Hash "contract: $shown"
                continue
            }
        }
        New-Result 'PENDING' $row $b.Hash $c.Hash ''
    }
}

function Find-SweepLeaks($Rows, [string]$First, [string]$Last) {
    $a = New-OrdinalMap; $b = New-OrdinalMap
    foreach ($r in $Rows) {
        if ($r.Hash -ceq '-') { continue }
        if ($r.Sid -ceq $First) { $a[$r.Name] = $r } elseif ($r.Sid -ceq $Last) { $b[$r.Name] = $r }
    }
    $names = New-Object 'System.Collections.Generic.SortedSet[string]' ([StringComparer]::Ordinal)
    foreach ($k in $a.Keys) { [void]$names.Add($k) }
    foreach ($k in $b.Keys) { [void]$names.Add($k) }
    foreach ($name in $names) {
        $x = $a[$name]; $y = $b[$name]
        $hx = if ($x) { $x.Hash } else { '(none)' }
        $hy = if ($y) { $y.Hash } else { '(none)' }
        if ($hx -cne $hy) {
            $origin = if ($x) { $x.Origin } else { $y.Origin }
            [pscustomobject]@{ Name = $name; First = $hx; Last = $hy; Origin = $origin }
        }
    }
}

function Test-PaletteCoverage($Rows, [string[]]$RegistryLines, [string[]]$Sids) {
    $valid = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($r in $Rows) { if ($r.Hash -cne '-') { [void]$valid.Add("$($r.Sid)`t$($r.Name)") } }
    foreach ($line in $RegistryLines) {
        $f = @($line.Trim() -split '\s+')
        if ($f.Count -lt 2) { continue }
        foreach ($sid in $Sids) { if (-not $valid.Contains("$sid`t$($f[1])")) { "$($f[1]) $sid" } }
    }
}

# Parses one "SHADERBENCH <name> <PASS|FAIL|WEAK> maxerr=... cov=... seeds=...[ reason=...]"
# line (real log lines carry a "YYYY-MM-DD HH:MM.SS " timestamp in front, which this
# tolerates since the match isn't anchored to the start of the line). Returns
# {Name; Status; Detail} or $null when the line isn't a bench result line.
function ConvertFrom-BenchLine([string]$Line) {
    if (-not ($Line -cmatch 'SHADERBENCH (\S+) (PASS|FAIL|WEAK) (.*)$')) { return $null }
    $name = $Matches[1]
    $status = $Matches[2]
    $detail = $Matches[3]
    if ($status -ceq 'PASS') { $status = 'PASS-PIXEL' }
    # A FAIL whose detail carries reason=unsupported means the bench
    # couldn't even feed identical inputs to both sides (e.g. an input
    # format/seed the shader path rejects), so the mismatch is
    # inconclusive rather than a genuine pixel difference -- downgrade it
    # to WEAK instead of FAIL.
    if ($status -ceq 'FAIL' -and $detail.Contains('reason=unsupported')) { $status = 'WEAK' }
    return [pscustomobject]@{ Name = $name; Status = $status; Detail = $detail }
}

function ConvertTo-WslPath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -notmatch '^([A-Za-z]):\\(.*)$') { throw "Not a drive path: $Path" }
    return '/mnt/' + $Matches[1].ToLower() + '/' + ($Matches[2] -replace '\\', '/')
}

function Format-Result($R) { return ('{0,-11} {1,-44} {2,-8} {3}' -f $R.Status, $R.Name, $R.Sid, $R.Detail).TrimEnd() }

function Format-Summary($Results) {
    $all = @($Results)
    $n = @{}
    foreach ($s in 'PASS-TEXT', 'PASS-SPIRV', 'PASS-PIXEL', 'WEAK', 'FAIL', 'MISSING', 'EXTRA') {
        $n[$s] = @($all | Where-Object { $_.Status -ceq $s }).Count
    }
    return '== {0} configs: {1} text, {2} spirv, {3} pixel, {4} weak, {5} fail, {6} missing, {7} extra' -f `
        $all.Count, $n['PASS-TEXT'], $n['PASS-SPIRV'], $n['PASS-PIXEL'], $n['WEAK'], $n['FAIL'], $n['MISSING'], $n['EXTRA']
}

function Get-CheckExitCode($Results) {
    if (@($Results | Where-Object { $_.Status -ceq 'FAIL' -or $_.Status -ceq 'MISSING' }).Count) { return 1 }
    return 0
}
