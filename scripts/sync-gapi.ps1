$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\CharterGAPI'))
$targetRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\components\CharterGAPI'))

foreach ($relative in @('include\charter_gapi.h', 'src\charter_gapi.c', 'README.md')) {
    $source = Join-Path $sourceRoot $relative
    $target = Join-Path $targetRoot $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "CharterGAPI source file is missing: $source"
    }
    $targetDirectory = Split-Path -Parent $target
    New-Item -ItemType Directory -Force -Path $targetDirectory | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
}

Write-Output "Synced CharterGAPI from $sourceRoot"
