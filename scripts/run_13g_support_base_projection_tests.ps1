param(
  [string]$BuildDir = "build-slicesoft/main",
  [ValidateSet("Debug", "Release", "RelWithDebInfo")]
  [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

function Assert-Equal($Actual, $Expected, [string]$Message) {
  if ($Actual -ne $Expected) {
    throw "$Message expected=$Expected actual=$Actual"
  }
}

function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) {
    throw $Message
  }
}

function Read-Json([string]$Path) {
  return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

$slicerCli = Join-Path $BuildDir "$Configuration/slicer_cli.exe"
$ripReader = Join-Path $BuildDir "$Configuration/rip_reader_test.exe"
$configPath = "samples/configs/support/support_base_projection_30_layers.json"
$packagePath = "output/SupportBaseProjection30Layers"

Assert-True (Test-Path -LiteralPath $slicerCli) "未找到 slicer_cli：$slicerCli"
Assert-True (Test-Path -LiteralPath $ripReader) "未找到 rip_reader_test：$ripReader"

Write-Host "== 13G support base projection fixture"
& $slicerCli --config $configPath
if ($LASTEXITCODE -ne 0) {
  throw "slicer_cli failed: support_base_projection_30_layers"
}

& $ripReader --package $packagePath --quiet
if ($LASTEXITCODE -ne 0) {
  throw "rip_reader_test failed: SupportBaseProjection30Layers"
}

$manifest = Read-Json "$packagePath/manifest.json"
$supportReport = Read-Json "$packagePath/reports/support_report.json"
$sliceReport = Read-Json "$packagePath/reports/slice_report.json"
$baseProjection = $supportReport.baseProjection

Assert-Equal $manifest.schema "p0.rgbwsv.2" "manifest schema mismatch"
Assert-Equal $manifest.tiff.bitDepth 8 "manifest bitDepth mismatch"
Assert-Equal $manifest.tiff.polarity "black_is_print" "manifest polarity mismatch"
Assert-Equal $baseProjection.configuredEnabled $true "base projection enabled mismatch"
Assert-Equal $baseProjection.configuredLayerCount 30 "configured layer count mismatch"
Assert-Equal $baseProjection.source "max_support_footprint" "projection source mismatch"
Assert-Equal $baseProjection.effectiveLayerRange[0] 0 "effective range start mismatch"
Assert-Equal $baseProjection.effectiveLayerRange[1] 15 "effective range end mismatch"
Assert-Equal $baseProjection.effectiveLayerCount 16 "effective layer count mismatch"
Assert-True ($baseProjection.footprintPixels -gt 0) "expected non-empty support footprint"
Assert-True ($baseProjection.addedSupportPixelsBeforeMaterialPriority -gt 0) `
  "expected base projection to add support"
Assert-True ($baseProjection.printPixels -gt 0) `
  "expected final projection_base support pixels"
Assert-Equal `
  $supportReport.supportTypeStats.projection_base `
  $baseProjection.printPixels `
  "projection_base report totals mismatch"
Assert-True ($sliceReport.totals.supportPixels -gt 0) `
  "slice report expected support pixels"

Write-Host "13G support base projection tests complete."
