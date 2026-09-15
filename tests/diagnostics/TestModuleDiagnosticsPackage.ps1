[CmdletBinding()]
param([string]$BuildDir = "build-slicesoft/main")
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repo $BuildDir }
$output = Join-Path $repo ("output/logdump/module-package-" + [Guid]::NewGuid().ToString("N"))
& (Join-Path $repo "scripts/PackageSlicerModule.ps1") -BuildDir $build -OutputRoot $output -SkipBuild
$package = Join-Path $output "Release/modules/slicer"
$inventory = Get-Content -LiteralPath (Join-Path $package "runtime_dependencies.json") -Raw | ConvertFrom-Json
foreach ($name in @("slicer_crash_reporter.exe", "slicer_logging.h")) {
    $entry = @($inventory.artifacts | Where-Object { $_.path -eq $name })
    if ($entry.Count -ne 1) { throw "Missing inventory entry: $name" }
    $actual = (Get-FileHash -LiteralPath (Join-Path $package $name)).Hash.ToLowerInvariant()
    if ($actual -ne $entry[0].sha256) { throw "Package hash mismatch: $name" }
}
$source = (Get-FileHash -LiteralPath (Join-Path $build "Release/slicer_crash_reporter.exe")).Hash
if ($source -ne (Get-FileHash -LiteralPath (Join-Path $package "slicer_crash_reporter.exe")).Hash) {
    throw "Packaged reporter differs from build."
}
if (@($inventory.imports | Where-Object { $_.source -eq "slicer_crash_reporter.exe" }).Count -eq 0) {
    throw "Reporter dependency closure was not inspected."
}
if (@(Get-ChildItem -LiteralPath $package -Recurse -Filter *.pdb).Count -ne 0) {
    throw "Private symbols leaked into runtime."
}
Write-Output "MODULE_DIAGNOSTICS_PACKAGE_PASS package=$package"
