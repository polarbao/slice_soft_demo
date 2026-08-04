$ErrorActionPreference = "Stop"

function Assert-Equal($Actual, $Expected, [string]$Message) {
  if ($Actual -ne $Expected) {
    throw "$Message expected=$Expected actual=$Actual"
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

function Write-GoldenRunConfig($Case) {
  $sourceConfig = [System.IO.Path]::GetFullPath($Case.config)
  $config = Read-Json $sourceConfig
  $modelPath = [string]$config.input.modelPath

  if (-not [System.IO.Path]::IsPathRooted($modelPath)) {
    if ($modelPath.StartsWith(".")) {
      $modelPath = [System.IO.Path]::GetFullPath(
        (Join-Path (Split-Path -Parent $sourceConfig) $modelPath))
    } else {
      $modelPath = [System.IO.Path]::GetFullPath($modelPath)
    }
  }
  $config.input.modelPath = $modelPath

  if ($null -eq $config.autoOrient) {
    $config | Add-Member -NotePropertyName autoOrient -NotePropertyValue ([pscustomobject]@{})
  }
  $config.autoOrient.enabled = $false

  $runtimeRoot = [System.IO.Path]::GetFullPath("output/golden_runtime_configs")
  New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
  $runtimeConfig = Join-Path $runtimeRoot "$($Case.name).json"
  [System.IO.File]::WriteAllText(
    $runtimeConfig,
    ($config | ConvertTo-Json -Depth 100),
    [System.Text.UTF8Encoding]::new($false))
  return $runtimeConfig
}

function Check-Golden($Case) {
  Write-Host "== golden $($Case.name)"
  Run-Slicer (Write-GoldenRunConfig $Case)

  $manifest = Read-Json (Join-Path $Case.package "manifest.json")
  $slice = Read-Json (Join-Path $Case.package "reports/slice_report.json")

  Assert-Equal $manifest.schema $Case.schema "$($Case.name) manifest schema"
  Assert-Equal $manifest.grid.widthPx $Case.widthPx "$($Case.name) widthPx"
  Assert-Equal $manifest.grid.heightPx $Case.heightPx "$($Case.name) heightPx"
  Assert-Equal $manifest.grid.layerCount $Case.layerCount "$($Case.name) layerCount"
  Assert-Equal $slice.totals.modelPixels $Case.modelPixels "$($Case.name) modelPixels"
  Assert-Equal $slice.totals.supportPixels $Case.supportPixels "$($Case.name) supportPixels"

  if ($Case.PSObject.Properties.Name -contains "supportShapeAddedPixels") {
    $shape = Read-Json (Join-Path $Case.package "reports/support_shape_report.json")
    Assert-Equal $shape.schema "p0.support_shape_report.1" "$($Case.name) support shape schema"
    Assert-Equal $shape.addedSupportPixels $Case.supportShapeAddedPixels "$($Case.name) support shape added pixels"
    Assert-Equal $shape.removedSupportPixels $Case.supportShapeRemovedPixels "$($Case.name) support shape removed pixels"
    if ($Case.PSObject.Properties.Name -contains "supportShapeBridgedGaps") {
      Assert-Equal $shape.bridgedGaps.Count $Case.supportShapeBridgedGaps "$($Case.name) support shape bridged gaps"
    }
  }
}

$cases = Read-Json "tests/golden/expected/r2_golden_summaries.json"
foreach ($case in $cases) {
  Check-Golden $case
}

Write-Host "== golden stage10 output contract"
.\scripts\run_10_output_contract_tests.ps1

Write-Host "Golden tests complete."
