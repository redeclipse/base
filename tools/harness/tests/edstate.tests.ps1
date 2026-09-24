# Unit tests for the EDSTATE parser. No game required.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\edstate.ps1')

$failures = 0
function Assert-That([string]$Name, [bool]$Condition) {
    if ($Condition) { Write-Host "  PASS  $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $Name" -ForegroundColor Red; $script:failures++ }
}

$full = @(
    'EDSTATE mode 1 3 8 2'
    'EDSTATE cam 512.00000 400.50000 600.00000 90.00000 -15.25000'
    'EDSTATE worldpos 512.00000 512.00000 512.00000'
    'EDSTATE cur 512 512 512 2'
    'EDSTATE sel 512 512 512 2 3 4 8 2 0 0 4 6 1 24 1'
    'EDSTATE ui 0 1234'
    'EDENT hover 17 playerstart 100.00000 200.00000 300.00000'
    'EDENT sel 17 playerstart 100.00000 200.00000 300.00000'
    'EDENT sel 18 mapmodel 110.00000 210.00000 310.00000'
    'EDSTATE end 3'
)

$r = ConvertFrom-EdState $full
Assert-That 'complete dump is marked complete'   ($r.Complete)
Assert-That 'edit mode parsed'                    ($r.Mode.EditMode -eq 1)
Assert-That 'gridpower parsed'                    ($r.Mode.GridPower -eq 3)
Assert-That 'gridsize parsed'                     ($r.Mode.GridSize -eq 8)
Assert-That 'cam x parsed'                        ($r.Cam.X -eq 512)
Assert-That 'cam negative pitch parsed'           ($r.Cam.Pitch -eq -15.25)
Assert-That 'cur parsed'                          ($r.Cur.X -eq 512 -and $r.Cur.Orient -eq 2)
Assert-That 'sel size parsed'                     ($r.Sel.SX -eq 2 -and $r.Sel.SY -eq 3 -and $r.Sel.SZ -eq 4)
Assert-That 'sel children parsed'                 ($r.Sel.Children -eq 24)
Assert-That 'havesel parsed'                      ($r.Sel.HaveSel -eq 1)
Assert-That 'freecursor parsed'                   ($r.Ui.FreeCursor -eq 1234)
Assert-That 'one hover entity'                    ($r.Hover.Count -eq 1)
Assert-That 'hover type parsed'                   ($r.Hover[0].Type -eq 'playerstart')
Assert-That 'two selected entities'               ($r.EntSel.Count -eq 2)
Assert-That 'second selected idx parsed'          ($r.EntSel[1].Idx -eq 18)
Assert-That 'ent count parsed'                    ($r.EntCount -eq 3)
Assert-That 'nothing malformed'                   ($r.Malformed.Count -eq 0)

# The harness strips log timestamps, but the parser must not depend on that.
$stamped = $full | ForEach-Object { "2026-09-03 12:34.56 $_" }
$s = ConvertFrom-EdState $stamped
Assert-That 'timestamped lines parse'             ($s.Complete -and $s.Mode.EditMode -eq 1)
Assert-That 'timestamped entities parse'          ($s.EntSel.Count -eq 2)

# Empty selection.
$empty = @(
    'EDSTATE mode 1 3 8 0'
    'EDSTATE cam 0.00000 0.00000 0.00000 0.00000 0.00000'
    'EDSTATE worldpos 0.00000 0.00000 0.00000'
    'EDSTATE cur 0 0 0 0'
    'EDSTATE sel 0 0 0 0 0 0 8 0 0 0 0 0 0 0 0'
    'EDSTATE ui 0 0'
    'EDSTATE end 0'
)
$e = ConvertFrom-EdState $empty
Assert-That 'havesel 0 parsed'                    ($e.Sel.HaveSel -eq 0)
Assert-That 'no hover entities'                   ($e.Hover.Count -eq 0)
Assert-That 'no selected entities'                ($e.EntSel.Count -eq 0)
Assert-That 'empty dump still complete'           ($e.Complete)

# A truncated dump must be reported, not silently accepted.
$t = ConvertFrom-EdState @('EDSTATE mode 1 3 8 0')
Assert-That 'truncated dump is incomplete'        (-not $t.Complete)

# Malformed lines are captured, never dropped.
$bad = ConvertFrom-EdState @(
    'EDSTATE mode 1 3 8 0'
    'EDSTATE cur nonsense'
    'EDENT hover 17'
    'EDSTATE end 0'
)
Assert-That 'malformed lines captured'            ($bad.Malformed.Count -eq 2)
Assert-That 'unparsed cur left null'              ($null -eq $bad.Cur)

# Unrelated log output is ignored, not treated as malformed.
$noise = ConvertFrom-EdState @('some other log line', 'EDSTATE mode 1 3 8 0', 'EDSTATE end 0')
Assert-That 'non-EDSTATE lines ignored'           ($noise.Malformed.Count -eq 0)

# A well-shaped but non-numeric float field must land the line in .Malformed,
# not crash the whole parser. The float fields are matched with (\S+) -- any
# non-whitespace token satisfies the regex -- so validity is decided only at
# ConvertTo-InvariantDouble/[int], and a torn read (e.g. mid-clearlog swap,
# per core.ps1's Read-LogSafe comment) must not take the caller down with it.
$crashFloat = ConvertFrom-EdState @(
    'EDSTATE mode 1 3 8 2'
    'EDSTATE cam abc 483.99988 1024.00000 0.00000 0.00000'
    'EDSTATE end 0'
)
Assert-That 'non-numeric float field is malformed, not a crash' ($crashFloat.Malformed.Count -eq 1)
Assert-That 'non-numeric float field leaves Cam null'           ($null -eq $crashFloat.Cam)

# Same hazard, integer side: a field that matches \d+ but overflows Int32 must
# also land in .Malformed rather than throwing an OverflowException.
$crashOverflow = ConvertFrom-EdState @(
    'EDSTATE mode 1 3 8 2'
    'EDSTATE cur 99999999999999 0 0 0'
    'EDSTATE end 0'
)
Assert-That 'overflowing integer field is malformed, not a crash' ($crashOverflow.Malformed.Count -eq 1)
Assert-That 'overflowing integer field leaves Cur null'          ($null -eq $crashOverflow.Cur)

# Coordinates must parse the same way regardless of the machine's locale. This
# pair only proves something if the hazard is real on this platform: the first
# assertion shows the culture-naive [double]::Parse genuinely misparses a
# dot-decimal string under de-DE (there, '.' is a thousands separator and ','
# is the decimal point, so "400.50000" comes back as 40050000, not 400.5); the
# second shows ConvertFrom-EdState is immune to it via ConvertTo-InvariantDouble.
# Without the first assertion, replacing ConvertTo-InvariantDouble with a bare
# [double] cast (which PS 5.1 parses invariantly regardless of thread culture)
# would still pass the second, silently defeating the point of this test.
$culture = [System.Threading.Thread]::CurrentThread.CurrentCulture
try {
    [System.Threading.Thread]::CurrentThread.CurrentCulture = 'de-DE'
    Assert-That 'de-DE sanity: culture-naive Parse actually misparses' ([double]::Parse('400.50000') -ne 400.5)
    $l = ConvertFrom-EdState $full
    Assert-That 'comma-decimal locale parses correctly via ConvertTo-InvariantDouble' ($l.Cam.Y -eq 400.5)
}
finally { [System.Threading.Thread]::CurrentThread.CurrentCulture = $culture }

# Verbatim captures from the live engine (task-2-report.md), not the brief's
# idealised format. The first shows selchildcount going negative ("... 0 -64
# 0") -- the parser must accept that, not "fix" it.
$real1 = @(
    'EDSTATE mode 1 3 8 2'
    'EDSTATE cam 497.99994 483.99988 1024.00000 0.00000 0.00000'
    'EDSTATE worldpos 498.00000 2532.00000 1024.00000'
    'EDSTATE cur 496 480 1024 2'
    'EDSTATE sel 496 480 1024 1 1 1 8 2 0 0 2 2 0 -64 0'
    'EDSTATE ui 0 1'
    'EDSTATE end 0'
)
$g1 = ConvertFrom-EdState $real1
Assert-That 'real capture 1 is complete'          ($g1.Complete)
Assert-That 'real capture 1 negative children'    ($g1.Sel.Children -eq -64)
Assert-That 'real capture 1 havesel 0'            ($g1.Sel.HaveSel -eq 0)
Assert-That 'real capture 1 nothing malformed'    ($g1.Malformed.Count -eq 0)

$real2 = @(
    'EDSTATE mode 1 3 8 5'
    'EDSTATE cam 504.99997 497.99994 1024.00000 260.62119 -56.78864'
    'EDSTATE worldpos 835.73645 443.36655 511.91632'
    'EDSTATE cur 832 440 504 5'
    'EDSTATE sel 832 440 504 0 0 0 8 5 0 0 2 2 0 0 0'
    'EDSTATE ui 0 1'
    'EDENT hover 0 playerstart 836.00000 444.00000 516.00000'
    'EDENT sel 0 playerstart 836.00000 444.00000 516.00000'
    'EDSTATE end 2'
)
$g2 = ConvertFrom-EdState $real2
Assert-That 'real capture 2 is complete'          ($g2.Complete)
Assert-That 'real capture 2 ent count parsed'     ($g2.EntCount -eq 2)
Assert-That 'real capture 2 one hover entity'     ($g2.Hover.Count -eq 1)
Assert-That 'real capture 2 one selected entity'  ($g2.EntSel.Count -eq 1)
Assert-That 'real capture 2 hover idx zero'       ($g2.Hover[0].Idx -eq 0)
Assert-That 'real capture 2 nothing malformed'    ($g2.Malformed.Count -eq 0)

if ($failures) { Write-Host "$failures failed" -ForegroundColor Red; exit 1 }
Write-Host 'all passed' -ForegroundColor Green
