param(
  [string]$BuildDir = "build",
  [ValidateSet("RepairDisabled", "RepairEnabled")]
  [string]$Mode = "RepairDisabled",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) {
    throw $Message
  }
}

function Assert-Equal($Actual, $Expected, [string]$Message) {
  if ($Actual -ne $Expected) {
    throw "$Message expected=$Expected actual=$Actual"
  }
}

function Assert-ArrayEqual($Actual, $Expected, [string]$Message) {
  $actualItems = @($Actual)
  $expectedItems = @($Expected)
  Assert-Equal $actualItems.Count $expectedItems.Count "$Message count"
  for ($index = 0; $index -lt $expectedItems.Count; ++$index) {
    Assert-Equal $actualItems[$index] $expectedItems[$index] "$Message[$index]"
  }
}

function Read-Json([string]$Path) {
  Assert-True (Test-Path -LiteralPath $Path) "missing JSON file: $Path"
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Invoke-External([string]$Name, [string]$Executable, [string[]]$Arguments) {
  & $Executable @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed with exit code $LASTEXITCODE"
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

function Assert-ConfigurationPair($Baseline, $Diagnostic) {
  Assert-Equal $Baseline.output.packageDir "output/MaterialClosureRepairDisabledBaseline" "baseline packageDir"
  Assert-Equal $Diagnostic.output.packageDir "output/MaterialClosureRepairDisabledDiagnostic" "diagnostic packageDir"
  Assert-Equal $Baseline.materialClosure.enabled $false "baseline materialClosure.enabled"
  Assert-Equal $Diagnostic.materialClosure.enabled $true "diagnostic materialClosure.enabled"
  Assert-Equal $Baseline.materialClosure.mode "diagnostic" "baseline mode"
  Assert-Equal $Diagnostic.materialClosure.mode "diagnostic" "diagnostic mode"
  Assert-Equal $Baseline.materialClosure.repair.enabled $false "baseline repair.enabled"
  Assert-Equal $Diagnostic.materialClosure.repair.enabled $false "diagnostic repair.enabled"
  Assert-Equal $Baseline.preview.enabled $false "baseline preview.enabled"
  Assert-Equal $Diagnostic.preview.enabled $false "diagnostic preview.enabled"

  $Baseline.output.packageDir = "<package>"
  $Diagnostic.output.packageDir = "<package>"
  $Baseline.materialClosure.enabled = $false
  $Diagnostic.materialClosure.enabled = $false
  $baselineNormalized = $Baseline | ConvertTo-Json -Depth 32 -Compress
  $diagnosticNormalized = $Diagnostic | ConvertTo-Json -Depth 32 -Compress
  Assert-Equal $baselineNormalized $diagnosticNormalized "configuration pair differs outside admitted fields"
}

function Assert-Protocol($Manifest, [string]$CaseId) {
  Assert-Equal $Manifest.schema "p0.rgbwsv.2" "$CaseId schema"
  Assert-Equal $Manifest.schemaVersion "p0.rgbwsv.2" "$CaseId schemaVersion"
  Assert-ArrayEqual $Manifest.tiff.channelOrder @("R", "G", "B", "W", "S", "V") "$CaseId channelOrder"
  Assert-Equal $Manifest.tiff.bitDepth 8 "$CaseId bitDepth"
  Assert-Equal $Manifest.tiff.polarity "black_is_print" "$CaseId polarity"
  Assert-Equal $Manifest.tiff.printValue 0 "$CaseId printValue"
  Assert-Equal $Manifest.tiff.emptyValue 255 "$CaseId emptyValue"
}

function Resolve-PackageFile([string]$Package, [string]$RelativePath, [string]$CaseId) {
  $packageRoot = [System.IO.Path]::GetFullPath($Package)
  $filePath = [System.IO.Path]::GetFullPath((Join-Path $packageRoot $RelativePath))
  $allowedPrefix = $packageRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
  Assert-True ($filePath.StartsWith($allowedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) "$CaseId layer path escapes package: $RelativePath"
  Assert-True (Test-Path -LiteralPath $filePath) "$CaseId missing layer file: $RelativePath"
  return $filePath
}

function Assert-LayerHashesEqual($BaselineManifest, $DiagnosticManifest, [string]$BaselinePackage, [string]$DiagnosticPackage) {
  Assert-Equal $BaselineManifest.grid.layerCount $DiagnosticManifest.grid.layerCount "manifest grid.layerCount"
  Assert-Equal $BaselineManifest.grid.widthPx $DiagnosticManifest.grid.widthPx "manifest grid.widthPx"
  Assert-Equal $BaselineManifest.grid.heightPx $DiagnosticManifest.grid.heightPx "manifest grid.heightPx"

  $baselineLayers = @($BaselineManifest.layers | Sort-Object { [int]$_.index })
  $diagnosticLayers = @($DiagnosticManifest.layers | Sort-Object { [int]$_.index })
  Assert-Equal $baselineLayers.Count $BaselineManifest.grid.layerCount "baseline manifest layer count"
  Assert-Equal $diagnosticLayers.Count $DiagnosticManifest.grid.layerCount "diagnostic manifest layer count"
  Assert-Equal @($baselineLayers | Group-Object index | Where-Object { $_.Count -ne 1 }).Count 0 "baseline duplicate layer index"
  Assert-Equal @($diagnosticLayers | Group-Object index | Where-Object { $_.Count -ne 1 }).Count 0 "diagnostic duplicate layer index"

  for ($position = 0; $position -lt $baselineLayers.Count; ++$position) {
    $baselineLayer = $baselineLayers[$position]
    $diagnosticLayer = $diagnosticLayers[$position]
    Assert-Equal $baselineLayer.index $diagnosticLayer.index "layer[$position].index"
    Assert-Equal $baselineLayer.zMm $diagnosticLayer.zMm "layer[$position].zMm"
    Assert-Equal $baselineLayer.widthPx $diagnosticLayer.widthPx "layer[$position].widthPx"
    Assert-Equal $baselineLayer.heightPx $diagnosticLayer.heightPx "layer[$position].heightPx"

    $baselinePath = Resolve-PackageFile $BaselinePackage $baselineLayer.path "baseline"
    $diagnosticPath = Resolve-PackageFile $DiagnosticPackage $diagnosticLayer.path "diagnostic"
    $baselineHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $baselinePath).Hash
    $diagnosticHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $diagnosticPath).Hash
    if ($baselineHash -ne $diagnosticHash) {
      throw "TIFF hash mismatch layerIndex=$($baselineLayer.index) baseline=$baselineHash diagnostic=$diagnosticHash"
    }
  }

  Write-Host "TIFF SHA-256 invariant: PASS layers=$($baselineLayers.Count)"
}

function Assert-Reports([string]$BaselinePackage, [string]$DiagnosticPackage) {
  $baseline = Read-Json (Join-Path $BaselinePackage "reports/material_closure_report.json")
  $diagnostic = Read-Json (Join-Path $DiagnosticPackage "reports/material_closure_report.json")

  Assert-Equal $baseline.enabled $false "baseline report enabled"
  Assert-Equal $baseline.repair.attempted $false "baseline repair attempted"
  Assert-Equal $baseline.repair.repairedPixels 0 "baseline repairedPixels"

  Assert-Equal $diagnostic.source "semantic_masks" "diagnostic source"
  Assert-Equal $diagnostic.confidence "exact" "diagnostic confidence"
  Assert-Equal $diagnostic.repair.enabled $false "diagnostic repair enabled"
  Assert-Equal $diagnostic.repair.attempted $false "diagnostic repair attempted"
  Assert-Equal $diagnostic.repair.repairedPixels 0 "diagnostic repair repairedPixels"
  Assert-Equal $diagnostic.totals.repairedPixels 0 "diagnostic totals repairedPixels"
  foreach ($layer in @($diagnostic.layers)) {
    Assert-Equal $layer.repair.attempted $false "diagnostic layer $($layer.layerIndex) repair attempted"
    Assert-Equal $layer.repair.repairedPixels 0 "diagnostic layer $($layer.layerIndex) repairedPixels"
    Assert-Equal $layer.repair.remainingGapPixels $layer.gapPixels "diagnostic layer $($layer.layerIndex) remaining gap"
  }
}

function Assert-RepairEnabledReport([string]$Package) {
  $report = Read-Json (Join-Path $Package "reports/material_closure_report.json")
  Assert-Equal $report.source "semantic_masks" "repair source"
  Assert-Equal $report.confidence "exact" "repair confidence"
  Assert-Equal $report.mode "repair_then_report" "repair mode"
  Assert-Equal $report.repair.enabled $true "repair enabled"
  Assert-Equal $report.repair.attempted $true "repair attempted"
  Assert-Equal $report.repair.maxGapPx 1 "repair maxGapPx"

  $layerRepairedPixels = 0
  $layerRemainingGapPixels = 0
  foreach ($layer in @($report.layers)) {
    Assert-Equal $layer.repair.attempted $true "repair layer $($layer.layerIndex) attempted"
    Assert-True ($layer.repair.repairedPixels -ge 0) "repair layer $($layer.layerIndex) repairedPixels negative"
    Assert-True ($layer.repair.remainingGapPixels -ge 0) "repair layer $($layer.layerIndex) remainingGapPixels negative"
    $layerRepairedPixels += [int64]$layer.repair.repairedPixels
    $layerRemainingGapPixels += [int64]$layer.repair.remainingGapPixels
  }
  Assert-Equal $report.totals.repairedPixels $layerRepairedPixels "repair totals.repairedPixels"
  Assert-Equal $report.totals.remainingGapPixels $layerRemainingGapPixels "repair totals.remainingGapPixels"
  Assert-Equal $report.repair.repairedPixels $report.totals.repairedPixels "repair summary repairedPixels"
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot
try {
  $resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
  $slicerExe = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
  $ripExe = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
  if ($Mode -eq "RepairEnabled") {
    $repairConfigPath = Join-Path $repoRoot "samples/configs/material_closure/repair_enabled_exact.json"
    $repairPackage = Join-Path $repoRoot "output/MaterialClosureRepairEnabledExact"
    Write-Host "== 12D-07 repair-enabled exact package"
    Invoke-External "repair-enabled slicer" $slicerExe @("--config", $repairConfigPath)
    Invoke-External "repair-enabled RIP reader" $ripExe @("--package", $repairPackage, "--summary")
    $repairManifest = Read-Json (Join-Path $repairPackage "manifest.json")
    Assert-Protocol $repairManifest "repair-enabled"
    Assert-RepairEnabledReport $repairPackage
    Write-Host "12D-07 Repair Enabled verification: PASS"
    return
  }

  $baselineConfigPath = Join-Path $repoRoot "samples/configs/material_closure/repair_disabled_baseline.json"
  $diagnosticConfigPath = Join-Path $repoRoot "samples/configs/material_closure/repair_disabled_diagnostic.json"
  $baselineConfig = Read-Json $baselineConfigPath
  $diagnosticConfig = Read-Json $diagnosticConfigPath
  Assert-ConfigurationPair $baselineConfig $diagnosticConfig

  $baselinePackage = Join-Path $repoRoot "output/MaterialClosureRepairDisabledBaseline"
  $diagnosticPackage = Join-Path $repoRoot "output/MaterialClosureRepairDisabledDiagnostic"

  Write-Host "== 12D-06 repair-disabled baseline"
  Invoke-External "baseline slicer" $slicerExe @("--config", $baselineConfigPath)
  Invoke-External "baseline RIP reader" $ripExe @("--package", $baselinePackage, "--summary")

  Write-Host "== 12D-06 repair-disabled exact diagnostic"
  Invoke-External "diagnostic slicer" $slicerExe @("--config", $diagnosticConfigPath)
  Invoke-External "diagnostic RIP reader" $ripExe @("--package", $diagnosticPackage, "--summary")

  $baselineManifest = Read-Json (Join-Path $baselinePackage "manifest.json")
  $diagnosticManifest = Read-Json (Join-Path $diagnosticPackage "manifest.json")
  Assert-Protocol $baselineManifest "baseline"
  Assert-Protocol $diagnosticManifest "diagnostic"
  Assert-LayerHashesEqual $baselineManifest $diagnosticManifest $baselinePackage $diagnosticPackage
  Assert-Reports $baselinePackage $diagnosticPackage

  Write-Host "12D-06 Repair Disabled verification: PASS"
}
finally {
  Pop-Location
}
