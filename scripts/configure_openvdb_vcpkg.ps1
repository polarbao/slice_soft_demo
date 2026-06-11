param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$BuildDir = "build-openvdb",
    [string]$Triplet = "x64-windows"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($VcpkgRoot))
{
    throw "VcpkgRoot is empty. Pass -VcpkgRoot <path> or set VCPKG_ROOT."
}

$toolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path -LiteralPath $toolchain))
{
    throw "vcpkg toolchain file was not found: $toolchain"
}

Write-Host "Configuring OpenVDB build"
Write-Host "  VcpkgRoot: $VcpkgRoot"
Write-Host "  BuildDir:  $BuildDir"
Write-Host "  Triplet:   $Triplet"

& cmake -S . -B $BuildDir `
    -DUSE_OPENVDB=ON `
    -DENABLE_GEOMETRY_KERNEL_DEMO=ON `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_TARGET_TRIPLET=$Triplet" `
    "-DVCPKG_MANIFEST_FEATURES=openvdb"

if ($LASTEXITCODE -ne 0)
{
    throw "OpenVDB vcpkg configure failed with exit code $LASTEXITCODE."
}

Write-Host "OpenVDB configure complete."
