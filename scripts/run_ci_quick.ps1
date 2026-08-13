param(
  [string]$SourceGuardBaseRef = "",
  [string]$BuildDir = "build",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug",
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Run-Step([string]$Name, [scriptblock]$Block) {
  Write-Host "== $Name"
  & $Block
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed"
  }
}

function Resolve-Executable([string]$BuildRoot, [string]$BuildConfig, [string]$Name) {
  $candidates = @(
    (Join-Path $BuildRoot "$BuildConfig/$Name.exe"),
    (Join-Path $BuildRoot "$Name.exe")
  )
  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) {
      return (Resolve-Path -LiteralPath $candidate).Path
    }
  }
  throw "missing executable $Name under build directory: $BuildRoot"
}

$resolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$slicerExe = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$supportShapeExe = Resolve-Executable $resolvedBuildDir $Config "support_shape_unit_tests"
$hostUiCandidates = @(
  (Join-Path $resolvedBuildDir "apps/slicer_ui_host_sim/$Config/slicer_ui_host_sim.exe"),
  (Join-Path $resolvedBuildDir "$Config/slicer_ui_host_sim.exe")
)
$hostUiExe = $hostUiCandidates |
  Where-Object { Test-Path -LiteralPath $_ } |
  Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($hostUiExe)) {
  throw "missing executable slicer_ui_host_sim under build directory: $resolvedBuildDir"
}
$hostUiExe = (Resolve-Path -LiteralPath $hostUiExe).Path

Run-Step "source size guard" {
  if ([string]::IsNullOrWhiteSpace($SourceGuardBaseRef)) {
    python .\scripts\ValidateSourceSizeGuard.py
  } else {
    python .\scripts\ValidateSourceSizeGuard.py --base-ref $SourceGuardBaseRef
  }
}

if (-not $SkipBuild) {
  Run-Step "build $Config" {
    cmake --build $resolvedBuildDir --config $Config
  }
}

Run-Step "support shape unit tests" {
  & $supportShapeExe
}

Run-Step "regression quick" {
  .\scripts\run_regression.ps1 `
    -Mode quick `
    -BuildDir $resolvedBuildDir `
    -Config $Config `
    -SkipBuild
}

Run-Step "schema tests" {
  .\scripts\run_schema_tests.ps1 `
    -BuildDir $resolvedBuildDir `
    -Config $Config
}

Run-Step "support shape tests" {
  .\scripts\run_support_shape_tests.ps1 `
    -BuildDir $resolvedBuildDir `
    -Config $Config
}

Run-Step "golden tests" {
  .\scripts\run_golden_tests.ps1 `
    -BuildDir $resolvedBuildDir `
    -Config $Config
}

Run-Step "packaged host ui self-test" {
  & $hostUiExe --self-test
}

Write-Host "CI quick complete."
