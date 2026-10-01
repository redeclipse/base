$ErrorActionPreference = 'Stop'
$archive = Join-Path $PSScriptRoot '@ARCHIVE@'
$destination = Join-Path $PSScriptRoot '@NAME@-extracted'
if ((Test-Path $archive) -or (Test-Path $destination)) {
    throw 'Extraction destination or temporary archive already exists.'
}
Write-Host 'Checking downloaded files...'
foreach ($line in Get-Content (Join-Path $PSScriptRoot '@NAME@.files.sha256')) {
    $fields = $line -split '  ', 2
    $file = Join-Path $PSScriptRoot $fields[1]
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLower() -ne $fields[0]) {
        throw "Checksum mismatch: $file"
    }
}
Write-Host 'Joining archive parts...'
$output = [System.IO.File]::Open($archive, [System.IO.FileMode]::CreateNew)
try {
    for ($part = 1; $part -le @COUNT@; $part++) {
        $file = '{0}.{1:000}' -f $archive, $part
        $inputStream = [System.IO.File]::OpenRead($file)
        try { $inputStream.CopyTo($output) } finally { $inputStream.Dispose() }
    }
} finally { $output.Dispose() }
Expand-Archive -LiteralPath $archive -DestinationPath $destination
Remove-Item -LiteralPath $archive
Write-Host "Ready: $destination\@TOP@\Eclipse Recoil.bat"
