$ErrorActionPreference = "Stop"

function Run-Step([string]$Name, [scriptblock]$Block) {
  Write-Host "== $Name"
  & $Block
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed"
  }
}

Run-Step "build Debug" {
  cmake --build build --config Debug
}

Run-Step "regression quick" {
  .\scripts\run_regression.ps1 -Mode quick
}

Run-Step "schema tests" {
  .\scripts\run_schema_tests.ps1
}

Run-Step "support shape tests" {
  .\scripts\run_support_shape_tests.ps1
}

Run-Step "golden tests" {
  .\scripts\run_golden_tests.ps1
}

Run-Step "ui self-test" {
  .\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
}

Run-Step "ui overlay fixture" {
  .\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json
}

Run-Step "ui overlay-load-real" {
  .\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
}

Write-Host "CI quick complete."
