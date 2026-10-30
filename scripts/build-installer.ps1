param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $project 'build\CharterMediaPlayer.exe'
$script = Join-Path $project 'installer\CharterMediaPlayer.iss'

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-program.bat')
    if ($LASTEXITCODE -ne 0) { throw 'Charter build failed.' }
}
if (-not (Test-Path -LiteralPath $binary)) { throw "Missing build: $binary" }

$compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty Source
if (-not $compiler) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    $compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}
if (-not $compiler) {
    throw 'Inno Setup 6 is required. Install JRSoftware.InnoSetup with winget.'
}

& $compiler "/DCharterBinary=$binary" $script
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
