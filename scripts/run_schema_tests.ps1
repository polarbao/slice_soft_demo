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

function Run-Slicer([string]$Config) {
  & .\build\Debug\slicer_cli.exe --config $Config
  if ($LASTEXITCODE -ne 0) {
    throw "slicer_cli failed: $Config"
  }
}

function Run-Rip([string]$Package) {
  & .\build\Debug\rip_reader_test.exe --package $Package --quiet
  if ($LASTEXITCODE -ne 0) {
    throw "rip_reader_test failed: $Package"
  }
}

Write-Host "== schema test: slicer.config.1 sample"
$schemaConfig = Read-Json "samples/configs/schema_v1/slicer_config_v1_basic.json"
Assert-Equal $schemaConfig.schema "slicer.config.1" "schema_v1 config schema mismatch"
Run-Slicer "samples/configs/schema_v1/slicer_config_v1_basic.json"
Run-Rip "output/SchemaV1Basic"

$manifest = Read-Json "output/SchemaV1Basic/manifest.json"
Assert-Equal $manifest.schema "p0.rgbwsv.2" "manifest schema mismatch"
Assert-Equal $manifest.tiff.bitDepth 8 "manifest bitDepth mismatch"
Assert-Equal $manifest.tiff.polarity "black_is_print" "manifest polarity mismatch"
Assert-Equal $manifest.tiff.printValue 0 "manifest printValue mismatch"
Assert-Equal $manifest.tiff.emptyValue 255 "manifest emptyValue mismatch"

Write-Host "== schema test: preview report"
Run-Slicer "samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json"
$preview = Read-Json "output/UiSmokeOverlayRgbwv/reports/preview_report.json"
Assert-Equal $preview.schema "p0.preview_report.1" "preview_report schema mismatch"
Assert-True ($preview.files.Count -gt 0) "preview_report expected files"
Assert-True ($preview.files[0].PSObject.Properties.Name -contains "path") "preview_report files[] missing path"
Assert-True ($preview.files[0].PSObject.Properties.Name -contains "channel") "preview_report files[] missing channel"
Assert-True ($preview.files[0].PSObject.Properties.Name -contains "layerIndex") "preview_report files[] missing layerIndex"

Write-Host "== schema test: material process report"
Run-Slicer "samples/configs/material_process/nail_rgb_white_varnish_top2.json"
$process = Read-Json "output/NailRgbWhiteVarnishTop2/reports/material_process_report.json"
Assert-Equal $process.enabled $true "material_process_report enabled mismatch"
Assert-True ($process.rgb.printPixels -gt 0) "material_process_report expected RGB printPixels"
Assert-True ($process.white.printPixels -gt 0) "material_process_report expected W printPixels"
Assert-True ($process.varnish.printPixels -gt 0) "material_process_report expected V printPixels"

Write-Host "== schema test: support shape report"
Run-Slicer "samples/configs/support/support_shape_smoke.json"
$supportShape = Read-Json "output/SupportShapeSmoke/reports/support_shape_report.json"
Assert-Equal $supportShape.schema "p0.support_shape_report.1" "support_shape_report schema mismatch"
Assert-Equal $supportShape.enabled $true "support_shape_report enabled mismatch"
Assert-True ($supportShape.layers.Count -gt 0) "support_shape_report expected layer data"

Write-Host "Schema tests complete."
