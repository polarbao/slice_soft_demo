param(
  [string]$BuildDir = "build"
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

function Read-Json([string]$Path) {
  return Get-Content -Raw -Encoding UTF8 $Path | ConvertFrom-Json
}

function Write-Utf8NoBomJson($Value, [string]$Path) {
  New-Item -ItemType Directory -Force -Path (Split-Path $Path -Parent) | Out-Null
  $jsonText = $Value | ConvertTo-Json -Depth 30
  $fullPath = [System.IO.Path]::GetFullPath($Path)
  [System.IO.File]::WriteAllText($fullPath, $jsonText, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-External([string]$Name, [string]$Exe, [string[]]$Arguments) {
  & $Exe @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed with exit code $LASTEXITCODE"
  }
}

function Get-PropertyValue($Object, [string]$Name) {
  return $Object.PSObject.Properties[$Name].Value
}

function Assert-ObjectValueEqual($Left, $Right, [string]$Name, [string]$Message) {
  Assert-Equal (Get-PropertyValue $Left $Name) (Get-PropertyValue $Right $Name) "$Message.$Name"
}

function Assert-SupportTypeStatsEqual($Left, $Right, [string]$Message) {
  foreach ($name in @("bottom_projection", "unsupported_island", "full_vertical_projection", "internal_void", "upper_projection")) {
    Assert-ObjectValueEqual $Left $Right $name $Message
  }
}

function New-ConsistencyConfig(
  [string]$ModelPath,
  [string]$PackageDir,
  [bool]$ColorTextureProfile
) {
  $textureConfig = [ordered]@{
    enabled = $ColorTextureProfile
    applyMode = "top_surface_band"
    topSurfaceLayers = 5
    sampler = "bilinear"
    uvAddressMode = "clamp"
    flipV = $true
    fallbackRgb = @(0, 0, 0)
    missingTexturePolicy = "warn_and_fallback"
    nonSurfaceRgbPolicy = "empty"
  }

  return [ordered]@{
    autoOrient = [ordered]@{
      enabled = $true
      maxHeightMm = 6
      strategy = "minimize_height_by_right_angle_rotation"
    }
    background = [ordered]@{
      value = 255
    }
    input = [ordered]@{
      modelPath = (Resolve-Path $ModelPath).Path
      format = "auto"
    }
    output = [ordered]@{
      packageDir = $PackageDir
      dpiX = 600
      dpiY = 600
      layerThicknessMm = 0.05
      channelOrder = @("R", "G", "B", "W", "S", "V")
      bitDepth = 8
      planarConfig = "contiguous"
      storageMode = "stripped"
      rowsPerStrip = 64
    }
    modelMaterial = [ordered]@{
      materialChannel = "RGB"
      applyMode = "solid_volume"
      rgb = @(0, 0, 0)
      whiteValue = 255
      varnishValue = 255
    }
    modelFill = [ordered]@{
      enabled = $true
      material = "white"
      scope = "below_texture_surface"
      value = 0
      emptyAllowedInProduction = $false
      legacyRgbFallback = $false
    }
    support = [ordered]@{
      enabled = $true
      mode = "bottom_projection"
      placement = "both"
      offsetMm = 0.05
      value = 0
      minAreaPx = 0
      connectivity = 8
      internalVoid = [ordered]@{
        enabled = $true
        minAreaPx = 16
        fillRule = "all_internal_voids"
      }
      upper = [ordered]@{
        enabled = $true
        outside = "outer_varnish_shell"
        reason = "optional_detachable_surface_support"
      }
    }
    surfaceVarnish = [ordered]@{
      enabled = $true
      outerSurface = $true
      innerSurface = $true
      thicknessPx = 1
      value = 0
      source = "explicit"
    }
    outerVarnish = [ordered]@{
      enabled = $true
      thicknessMm = 0.05
      thicknessStepMm = 0.01
      pixelPitchUm = 42.3
      allowXYExpansion = $true
      conflictPolicy = "varnish_shell_wins"
      value = 0
    }
    preview = [ordered]@{
      enabled = $false
      format = "png"
      interval = 20
      channels = @("texture_rgb", "support", "white", "varnish")
      onlyNonEmptyLayers = $true
    }
    relief = [ordered]@{
      baseZMm = 0
      fillMode = "intersection_range"
    }
    slicingMode = "relief_heightfield"
    texture = $textureConfig
  }
}

function Assert-ConsistencyHint($Slice, [string]$ExpectedProfileKind, $Golden, [string]$CaseId) {
  $hint = $Slice.totals.singleMaterialConsistency
  Assert-True ($null -ne $hint) "$CaseId missing singleMaterialConsistency hint"
  Assert-Equal $hint.schema $Golden.sliceHintSchema "$CaseId hint schema"
  Assert-Equal $hint.profileKind $ExpectedProfileKind "$CaseId hint profileKind"
  Assert-Equal $hint.comparisonStatus "not_evaluated_in_single_package" "$CaseId hint comparisonStatus"
  Assert-True $hint.pairComparisonRequired "$CaseId hint pairComparisonRequired"
}

function Compare-Case([string]$CaseId, [string]$ColorPackage, [string]$SinglePackage, $Golden) {
  $colorManifest = Read-Json (Join-Path $ColorPackage "manifest.json")
  $singleManifest = Read-Json (Join-Path $SinglePackage "manifest.json")
  $colorSlice = Read-Json (Join-Path $ColorPackage "reports/slice_report.json")
  $singleSlice = Read-Json (Join-Path $SinglePackage "reports/slice_report.json")
  $colorTexture = Read-Json (Join-Path $ColorPackage "reports/texture_report.json")

  Assert-Equal $colorManifest.schema "p0.rgbwsv.2" "$CaseId color schema"
  Assert-Equal $singleManifest.schema "p0.rgbwsv.2" "$CaseId single schema"
  Assert-Equal $colorManifest.grid.widthPx $singleManifest.grid.widthPx "$CaseId grid.widthPx"
  Assert-Equal $colorManifest.grid.heightPx $singleManifest.grid.heightPx "$CaseId grid.heightPx"
  Assert-Equal $colorManifest.grid.layerCount $singleManifest.grid.layerCount "$CaseId grid.layerCount"
  Assert-Equal $colorManifest.grid.layerThicknessMm $singleManifest.grid.layerThicknessMm "$CaseId grid.layerThicknessMm"
  Assert-Equal $colorManifest.tiff.channelOrder.Count 6 "$CaseId color channelOrder count"
  Assert-Equal $singleManifest.tiff.channelOrder.Count 6 "$CaseId single channelOrder count"

  Assert-ConsistencyHint $colorSlice "color_texture" $Golden "$CaseId color"
  Assert-ConsistencyHint $singleSlice "single_material" $Golden "$CaseId single"

  foreach ($field in @($Golden.equalTotals)) {
    Assert-ObjectValueEqual $colorSlice.totals $singleSlice.totals $field "$CaseId totals"
  }
  Assert-SupportTypeStatsEqual $colorSlice.totals.supportTypeStats $singleSlice.totals.supportTypeStats "$CaseId totals.supportTypeStats"

  $colorLayers = @($colorSlice.layers)
  $singleLayers = @($singleSlice.layers)
  Assert-Equal $colorLayers.Count $singleLayers.Count "$CaseId layer count"
  for ($i = 0; $i -lt $colorLayers.Count; ++$i) {
    foreach ($field in @($Golden.equalLayerFields)) {
      Assert-ObjectValueEqual $colorLayers[$i] $singleLayers[$i] $field "$CaseId layers[$i]"
    }
    Assert-SupportTypeStatsEqual $colorLayers[$i].supportTypeStats $singleLayers[$i].supportTypeStats "$CaseId layers[$i].supportTypeStats"
  }

  Assert-True ([int64]$colorSlice.totals.rgbPrintPixels -gt 0) "$CaseId color rgbPrintPixels must be positive"
  Assert-True ([int64]$singleSlice.totals.whitePrintPixels -gt 0) "$CaseId single whitePrintPixels must be positive"
  Assert-True ([int64]$colorTexture.sampledPixels -gt 0) "$CaseId color texture sampledPixels must be positive"
  Assert-True ([int64]$singleSlice.totals.modelPixels -eq [int64]$colorSlice.totals.modelPixels) "$CaseId modelPixels must match"

  return [ordered]@{
    schema = $Golden.comparisonSchema
    caseId = $CaseId
    status = "PASS"
    colorPackage = $ColorPackage
    singlePackage = $SinglePackage
    layerCount = $colorManifest.grid.layerCount
    widthPx = $colorManifest.grid.widthPx
    heightPx = $colorManifest.grid.heightPx
    equalTotals = @($Golden.equalTotals)
    allowedDifferentTotals = @($Golden.allowedDifferentTotals)
    totals = [ordered]@{
      modelPixels = $colorSlice.totals.modelPixels
      supportPrintPixels = $colorSlice.totals.supportPrintPixels
      outerVarnishPixels = $colorSlice.totals.outerVarnishPixels
      outerSurfaceVarnishPixels = $colorSlice.totals.outerSurfaceVarnishPixels
      innerSurfaceVarnishPixels = $colorSlice.totals.innerSurfaceVarnishPixels
      upperSurfaceSupportPixels = $colorSlice.totals.upperSurfaceSupportPixels
      colorRgbPrintPixels = $colorSlice.totals.rgbPrintPixels
      singleWhitePrintPixels = $singleSlice.totals.whitePrintPixels
    }
  }
}

$golden = Read-Json "tests/golden/expected/12a_single_material_consistency_summary.json"
Assert-Equal $golden.schema "p0.12a.single_material_consistency_summary.1" "golden schema"

$slicerExe = Join-Path $BuildDir "Debug/slicer_cli.exe"
$ripExe = Join-Path $BuildDir "Debug/rip_reader_test.exe"
Assert-True (Test-Path $slicerExe) "missing slicer_cli: $slicerExe"
Assert-True (Test-Path $ripExe) "missing rip_reader_test: $ripExe"

$summaryReports = @()
foreach ($case in @($golden.cases)) {
  $caseId = $case.id
  Write-Host "== 12A single-material consistency $caseId"
  $baseDir = "output/12a11_validation/$caseId"
  $colorPackage = "$baseDir/color/package"
  $singlePackage = "$baseDir/single/package"
  $colorConfigPath = "$baseDir/configs/color.json"
  $singleConfigPath = "$baseDir/configs/single.json"

  Write-Utf8NoBomJson (New-ConsistencyConfig $case.modelPath $colorPackage $true) $colorConfigPath
  Write-Utf8NoBomJson (New-ConsistencyConfig $case.modelPath $singlePackage $false) $singleConfigPath

  Invoke-External "slicer color $caseId" $slicerExe @("--config", $colorConfigPath)
  Invoke-External "rip color $caseId" $ripExe @("--package", $colorPackage, "--quiet")
  Invoke-External "slicer single $caseId" $slicerExe @("--config", $singleConfigPath)
  Invoke-External "rip single $caseId" $ripExe @("--package", $singlePackage, "--quiet")

  $report = Compare-Case $caseId $colorPackage $singlePackage $golden
  $reportPath = "$baseDir/consistency_report.json"
  Write-Utf8NoBomJson $report $reportPath
  $summaryReports += $report
  Write-Host "PASS $caseId"
}

$summary = [ordered]@{
  schema = $golden.comparisonSchema
  status = "PASS"
  cases = $summaryReports
}
Write-Utf8NoBomJson $summary "output/12a11_validation/summary.json"

Write-Host "12A single-material consistency tests complete."
