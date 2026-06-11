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
  return Get-Content -Raw $Path | ConvertFrom-Json
}

Write-Host "== support shape smoke"
& .\build\Debug\slicer_cli.exe --config samples\configs\support\support_shape_smoke.json
if ($LASTEXITCODE -ne 0) {
  throw "slicer_cli failed: support_shape_smoke"
}

& .\build\Debug\rip_reader_test.exe --package output\SupportShapeSmoke --quiet
if ($LASTEXITCODE -ne 0) {
  throw "rip_reader_test failed: SupportShapeSmoke"
}

$manifest = Read-Json "output/SupportShapeSmoke/manifest.json"
$report = Read-Json "output/SupportShapeSmoke/reports/support_shape_report.json"
$slice = Read-Json "output/SupportShapeSmoke/reports/slice_report.json"

Assert-Equal $manifest.schema "p0.rgbwsv.2" "manifest schema mismatch"
Assert-Equal $manifest.tiff.bitDepth 8 "manifest bitDepth mismatch"
Assert-Equal $manifest.tiff.polarity "black_is_print" "manifest polarity mismatch"
Assert-Equal $report.schema "p0.support_shape_report.1" "support_shape_report schema mismatch"
Assert-Equal $report.enabled $true "support_shape_report enabled mismatch"
Assert-True ($report.layers.Count -gt 0) "support_shape_report expected layer data"
Assert-True ($report.addedSupportPixels -gt 0 -or $report.removedSupportPixels -gt 0 -or $report.bridgedGaps.Count -gt 0) `
  "support_shape_report expected shape changes"
Assert-True ($slice.totals.supportPixels -gt 0) "slice_report expected support pixels"

Write-Host "== support bridge gap smoke"
& .\build\Debug\slicer_cli.exe --config samples\configs\support\support_bridge_gap_smoke.json
if ($LASTEXITCODE -ne 0) {
  throw "slicer_cli failed: support_bridge_gap_smoke"
}

& .\build\Debug\rip_reader_test.exe --package output\SupportBridgeGapSmoke --quiet
if ($LASTEXITCODE -ne 0) {
  throw "rip_reader_test failed: SupportBridgeGapSmoke"
}

$bridgeManifest = Read-Json "output/SupportBridgeGapSmoke/manifest.json"
$bridgeReport = Read-Json "output/SupportBridgeGapSmoke/reports/support_shape_report.json"
$bridgeSlice = Read-Json "output/SupportBridgeGapSmoke/reports/slice_report.json"

Assert-Equal $bridgeManifest.schema "p0.rgbwsv.2" "bridge manifest schema mismatch"
Assert-Equal $bridgeManifest.tiff.bitDepth 8 "bridge manifest bitDepth mismatch"
Assert-Equal $bridgeManifest.tiff.polarity "black_is_print" "bridge manifest polarity mismatch"
Assert-Equal $bridgeReport.schema "p0.support_shape_report.1" "bridge support_shape_report schema mismatch"
Assert-Equal $bridgeReport.enabled $true "bridge support_shape_report enabled mismatch"
Assert-True ($bridgeReport.bridgedGaps.Count -gt 0) "bridge support_shape_report expected bridgedGaps"
Assert-True ($bridgeReport.addedSupportPixels -gt 0) "bridge support_shape_report expected addedSupportPixels"
Assert-True ($bridgeSlice.totals.supportPixels -gt 0) "bridge slice_report expected support pixels"

Write-Host "Support shape tests complete."
