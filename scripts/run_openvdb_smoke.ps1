param(
    [string]$BuildDir = "build-openvdb",
    [string]$OutputDir = "output/GeometryKernelOpenVdb",
    [string]$Config = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt")))
{
    throw "OpenVDB build directory is not configured: $BuildDir. Run scripts/configure_openvdb_vcpkg.ps1 first."
}

$buildFileCandidates = @(
    (Join-Path $BuildDir "SliceSoftDemo.sln"),
    (Join-Path $BuildDir "build.ninja"),
    (Join-Path $BuildDir "Makefile")
)
$hasBuildFiles = $false
foreach ($candidate in $buildFileCandidates)
{
    if (Test-Path -LiteralPath $candidate)
    {
        $hasBuildFiles = $true
        break
    }
}

if (-not $hasBuildFiles)
{
    throw "OpenVDB build directory is incomplete: $BuildDir. Re-run scripts/configure_openvdb_vcpkg.ps1 after installing/configuring vcpkg."
}

Write-Host "Building geometry_kernel_demo from $BuildDir"
& cmake --build $BuildDir --config $Config --target geometry_kernel_demo
if ($LASTEXITCODE -ne 0)
{
    throw "geometry_kernel_demo build failed with exit code $LASTEXITCODE."
}

$exeCandidates = @(
    (Join-Path $BuildDir "$Config/geometry_kernel_demo.exe"),
    (Join-Path $BuildDir "geometry_kernel_demo.exe")
)
$demoExe = $null
foreach ($candidate in $exeCandidates)
{
    if (Test-Path -LiteralPath $candidate)
    {
        $demoExe = $candidate
        break
    }
}

if ($null -eq $demoExe)
{
    throw "geometry_kernel_demo.exe was not found under $BuildDir."
}

Write-Host "Running OpenVDB smoke"
& $demoExe --case openvdb-smoke --output $OutputDir
if ($LASTEXITCODE -ne 0)
{
    throw "openvdb-smoke failed with exit code $LASTEXITCODE."
}

$reportPath = Join-Path $OutputDir "reports/geometry_kernel_report.json"
if (-not (Test-Path -LiteralPath $reportPath))
{
    throw "geometry kernel report was not found: $reportPath"
}

$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
if ($report.openvdb.enabled -ne $true)
{
    throw "openvdb.enabled must be true for USE_OPENVDB=ON smoke."
}
if ($report.openvdb.available -ne $true)
{
    throw "openvdb.available must be true for USE_OPENVDB=ON smoke."
}
if ([int]$report.openvdb.activeVoxels -le 0)
{
    throw "openvdb.activeVoxels must be greater than 0."
}

Write-Host "OpenVDB smoke passed."
Write-Host "  version:      $($report.openvdb.version)"
Write-Host "  activeVoxels: $($report.openvdb.activeVoxels)"
