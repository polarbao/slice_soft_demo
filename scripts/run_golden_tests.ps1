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

function Check-Golden($Case) {
  Write-Host "== golden $($Case.name)"
  Run-Slicer $Case.config

  $manifest = Read-Json (Join-Path $Case.package "manifest.json")
  $slice = Read-Json (Join-Path $Case.package "reports/slice_report.json")

  Assert-Equal $manifest.schema $Case.schema "$($Case.name) manifest schema"
  Assert-Equal $manifest.grid.widthPx $Case.widthPx "$($Case.name) widthPx"
  Assert-Equal $manifest.grid.heightPx $Case.heightPx "$($Case.name) heightPx"
  Assert-Equal $manifest.grid.layerCount $Case.layerCount "$($Case.name) layerCount"
  Assert-Equal $slice.totals.modelPixels $Case.modelPixels "$($Case.name) modelPixels"
  Assert-Equal $slice.totals.supportPixels $Case.supportPixels "$($Case.name) supportPixels"
}

$cases = Read-Json "tests/golden/expected/r2_golden_summaries.json"
foreach ($case in $cases) {
  Check-Golden $case
}

Write-Host "Golden tests complete."
