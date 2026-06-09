param(
  [Parameter(Mandatory = $true)]
  [string]$PackageA,
  [Parameter(Mandatory = $true)]
  [string]$PackageB,
  [string]$Output
)

$ErrorActionPreference = "Stop"

function Read-Json([string]$Path) {
  if (-not (Test-Path $Path)) {
    throw "missing json file: $Path"
  }
  return Get-Content -Raw $Path | ConvertFrom-Json
}

function Get-ReportPath([string]$Package) {
  return Join-Path $Package "reports/material_process_report.json"
}

function Get-LayerMap($Report) {
  $map = @{}
  foreach ($layer in $Report.layers) {
    $map[[int]$layer.layerIndex] = $layer
  }
  return $map
}

$reportAPath = Get-ReportPath $PackageA
$reportBPath = Get-ReportPath $PackageB
$reportA = Read-Json $reportAPath
$reportB = Read-Json $reportBPath

if (-not $Output -or $Output.Trim().Length -eq 0) {
  $Output = Join-Path $PackageA "reports/material_profile_compare_report.json"
}

$layersA = Get-LayerMap $reportA
$layersB = Get-LayerMap $reportB
$allLayerIndices = @($layersA.Keys + $layersB.Keys | Sort-Object -Unique)
$changedLayers = 0
$layerDeltas = @()

foreach ($index in $allLayerIndices) {
  $a = $layersA[$index]
  $b = $layersB[$index]
  $deltaRgb = [int64]($b.rgbPrintPixels) - [int64]($a.rgbPrintPixels)
  $deltaWhite = [int64]($b.whitePrintPixels) - [int64]($a.whitePrintPixels)
  $deltaVarnish = [int64]($b.varnishPrintPixels) - [int64]($a.varnishPrintPixels)
  $deltaSupport = [int64]($b.supportPrintPixels) - [int64]($a.supportPrintPixels)
  if ($deltaRgb -ne 0 -or $deltaWhite -ne 0 -or $deltaVarnish -ne 0 -or $deltaSupport -ne 0) {
    $changedLayers += 1
  }
  $layerDeltas += [ordered]@{
    layerIndex = [int]$index
    rgbPrintPixels = $deltaRgb
    whitePrintPixels = $deltaWhite
    varnishPrintPixels = $deltaVarnish
    supportPrintPixels = $deltaSupport
  }
}

$result = [ordered]@{
  schema = "material_profile_compare.v1"
  packageA = $PackageA
  packageB = $PackageB
  profileA = $reportA.profileName
  profileB = $reportB.profileName
  delta = [ordered]@{
    rgbPrintPixels = [int64]($reportB.rgb.printPixels) - [int64]($reportA.rgb.printPixels)
    whitePrintPixels = [int64]($reportB.white.printPixels) - [int64]($reportA.white.printPixels)
    varnishPrintPixels = [int64]($reportB.varnish.printPixels) - [int64]($reportA.varnish.printPixels)
    supportPrintPixels = [int64]($reportB.support.printPixels) - [int64]($reportA.support.printPixels)
  }
  changedLayers = $changedLayers
  validation = [ordered]@{
    pass = ($reportA.validation.pass -eq $true -and $reportB.validation.pass -eq $true)
    packageAPass = $reportA.validation.pass
    packageBPass = $reportB.validation.pass
  }
  layers = $layerDeltas
}

$outputParent = Split-Path -Parent $Output
if ($outputParent -and -not (Test-Path $outputParent)) {
  New-Item -ItemType Directory -Force $outputParent | Out-Null
}

$result | ConvertTo-Json -Depth 8 | Set-Content -Path $Output -Encoding UTF8
Write-Host "Wrote material profile compare report: $Output"
