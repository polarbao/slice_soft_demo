[CmdletBinding()]
param(
    [string]$BuildDir = "build-12c-ui",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [string]$Qt5Dir = $env:Qt5_DIR,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"

function ResolveQt5CMakeDir
{
    param([string]$Candidate)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($Candidate))
    {
        $candidates += $Candidate
        $candidates += Join-Path $Candidate "lib/cmake/Qt5"
    }
    $candidates += "C:/Qt/Qt5.15.2/5.15.2/msvc2019_64/lib/cmake/Qt5"

    foreach ($path in $candidates)
    {
        if (Test-Path -LiteralPath (Join-Path $path "Qt5Config.cmake"))
        {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    throw "Qt5Config.cmake was not found. Set Qt5_DIR or pass -Qt5Dir."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedQt5Dir = ResolveQt5CMakeDir -Candidate $Qt5Dir
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir))
{
    $BuildDir
}
else
{
    Join-Path $repoRoot $BuildDir
}

& cmake -S $repoRoot -B $resolvedBuildDir `
    "-DQt5_DIR=$resolvedQt5Dir" `
    "-DBUILD_SLICER_DEBUG_UI=ON" `
    "-DUSE_OPENVDB=OFF"
if ($LASTEXITCODE -ne 0)
{
    throw "12C Qt UI configure failed with exit code $LASTEXITCODE."
}

if ($ConfigureOnly)
{
    Write-Output "12C Qt UI configure passed: $resolvedBuildDir"
    exit 0
}

& cmake --build $resolvedBuildDir --config $Config --target slicer_debug_ui -- /m
if ($LASTEXITCODE -ne 0)
{
    throw "12C Qt UI build failed with exit code $LASTEXITCODE."
}

Write-Output "12C Qt UI build passed: $resolvedBuildDir ($Config)"
