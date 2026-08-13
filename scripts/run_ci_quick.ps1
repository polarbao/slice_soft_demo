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
$uiCandidates = @(
  (Join-Path $resolvedBuildDir "apps/slicer_debug_ui/$Config/slicer_debug_ui.exe"),
  (Join-Path $resolvedBuildDir "$Config/slicer_debug_ui.exe")
)
$uiExe = $uiCandidates |
  Where-Object { Test-Path -LiteralPath $_ } |
  Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($uiExe)) {
  throw "missing executable slicer_debug_ui under build directory: $resolvedBuildDir"
}
$uiExe = (Resolve-Path -LiteralPath $uiExe).Path

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

Run-Step "ui self-test" {
  & $uiExe --self-test
}

Run-Step "ui overlay fixture" {
  & $slicerExe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json
}

Run-Step "ui overlay-load-real" {
  & $uiExe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
}

Write-Host "CI quick complete."
