param(
  [string]$BuildDir = "build"
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

function Assert-GreaterOrEqual([double]$Actual, [double]$Expected, [string]$Message) {
  if ($Actual -lt $Expected) {
    throw "$Message expected >= $Expected actual=$Actual"
  }
}

function Assert-LessOrEqual([double]$Actual, [double]$Expected, [string]$Message) {
  if ($Actual -gt $Expected) {
    throw "$Message expected <= $Expected actual=$Actual"
  }
}

function Has-Property($Object, [string]$Name) {
  return $Object.PSObject.Properties.Name -contains $Name
}

function Assert-HasProperty($Object, [string]$Name, [string]$Prefix) {
  Assert-True (Has-Property $Object $Name) "$Prefix missing property: $Name"
}

function Assert-Properties($Object, $Names, [string]$Prefix) {
  foreach ($name in @($Names)) {
    Assert-HasProperty $Object $name $Prefix
  }
}

function Assert-ArrayEqual($Actual, $Expected, [string]$Message) {
  $actualArray = @($Actual)
  $expectedArray = @($Expected)
  Assert-Equal $actualArray.Count $expectedArray.Count "$Message length"
  for ($i = 0; $i -lt $expectedArray.Count; ++$i) {
    Assert-Equal $actualArray[$i] $expectedArray[$i] "$Message[$i]"
  }
}

function Read-Json([string]$Path) {
  return Get-Content -Raw $Path | ConvertFrom-Json
}

function Invoke-External([string]$Name, [string]$Exe, [string[]]$Arguments) {
  & $Exe @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed with exit code $LASTEXITCODE"
  }
}

function Assert-NumberRule([double]$Actual, $Rule, [string]$Message) {
  if ($null -eq $Rule) {
    return
  }
  if (Has-Property $Rule "equals") {
    Assert-Equal $Actual $Rule.equals $Message
  }
  if (Has-Property $Rule "min") {
    Assert-GreaterOrEqual $Actual $Rule.min $Message
  }
  if (Has-Property $Rule "max") {
    Assert-LessOrEqual $Actual $Rule.max $Message
  }
}

function Get-Rate([double]$Numerator, [double]$Denominator) {
  if ($Denominator -le 0) {
    return 0.0
  }
  return $Numerator / $Denominator
}

function Assert-Protocol($Manifest, $Contract, [string]$CaseId) {
  Assert-Properties $Manifest $Contract.requiredManifestFields "$CaseId manifest"
  Assert-Properties $Manifest.grid $Contract.requiredGridFields "$CaseId manifest.grid"
  Assert-Properties $Manifest.tiff $Contract.requiredTiffFields "$CaseId manifest.tiff"

  Assert-Equal $Manifest.schema $Contract.packageSchema "$CaseId manifest.schema"
  Assert-Equal $Manifest.schemaVersion $Contract.packageSchema "$CaseId manifest.schemaVersion"
  Assert-ArrayEqual $Manifest.tiff.channelOrder $Contract.protocol.channelOrder "$CaseId tiff.channelOrder"
  Assert-Equal $Manifest.tiff.channelCount $Contract.protocol.channelCount "$CaseId tiff.channelCount"
  Assert-Equal $Manifest.tiff.bitDepth $Contract.protocol.bitDepth "$CaseId tiff.bitDepth"
  Assert-Equal $Manifest.tiff.sampleFormat $Contract.protocol.sampleFormat "$CaseId tiff.sampleFormat"
  Assert-Equal $Manifest.tiff.planarConfig $Contract.protocol.planarConfig "$CaseId tiff.planarConfig"
  Assert-Equal $Manifest.tiff.polarity $Contract.protocol.polarity "$CaseId tiff.polarity"
  Assert-Equal $Manifest.tiff.printValue $Contract.protocol.printValue "$CaseId tiff.printValue"
  Assert-Equal $Manifest.tiff.emptyValue $Contract.protocol.emptyValue "$CaseId tiff.emptyValue"
  Assert-True (@($Contract.protocol.allowedStorageModes) -contains $Manifest.tiff.storageMode) "$CaseId unsupported storageMode: $($Manifest.tiff.storageMode)"
  Assert-True ($Manifest.grid.widthPx -gt 0) "$CaseId widthPx must be positive"
  Assert-True ($Manifest.grid.heightPx -gt 0) "$CaseId heightPx must be positive"
  Assert-True ($Manifest.grid.layerCount -gt 0) "$CaseId layerCount must be positive"
}

function Assert-Reports($Package, $Contract, $Case, [string]$CaseId) {
  $required = @($Contract.requiredReports)
  if (Has-Property $Case "requiredReports") {
    $required += @($Case.requiredReports)
  }
  foreach ($report in $required) {
    $path = Join-Path $Package ("reports/" + $report)
    Assert-True (Test-Path $path) "$CaseId missing report: $report"
  }
}

function Assert-Layers($Manifest, $Slice, $Contract, [string]$Package, [string]$CaseId) {
  $layers = @($Manifest.layers)
  Assert-Equal $layers.Count $Manifest.grid.layerCount "$CaseId manifest layer count"
  for ($i = 0; $i -lt $layers.Count; ++$i) {
    $layer = $layers[$i]
    Assert-Properties $layer $Contract.requiredLayerFields "$CaseId manifest.layers[$i]"
    Assert-Equal $layer.index $i "$CaseId manifest.layers[$i].index"
    Assert-Equal $layer.widthPx $Manifest.grid.widthPx "$CaseId manifest.layers[$i].widthPx"
    Assert-Equal $layer.heightPx $Manifest.grid.heightPx "$CaseId manifest.layers[$i].heightPx"
    Assert-True (Test-Path (Join-Path $Package $layer.path)) "$CaseId missing TIFF layer: $($layer.path)"
  }

  $sliceLayers = @($Slice.layers)
  Assert-Equal $sliceLayers.Count $Manifest.grid.layerCount "$CaseId slice layer count"
  for ($i = 0; $i -lt $sliceLayers.Count; ++$i) {
    $layer = $sliceLayers[$i]
    Assert-Properties $layer $Contract.requiredSliceLayerFields "$CaseId slice.layers[$i]"
    Assert-Equal $layer.layerIndex $i "$CaseId slice.layers[$i].layerIndex"
  }
}

function Assert-ChannelStats($Stats, [int64]$ExpectedPixels, $Contract, [string]$Prefix) {
  foreach ($channel in @("R", "G", "B", "W", "S", "V")) {
    Assert-HasProperty $Stats $channel $Prefix
    $channelStats = $Stats.$channel
    Assert-Properties $channelStats $Contract.requiredChannelStatsFields "$Prefix.$channel"
    Assert-Equal ([int64]$channelStats.printPixels + [int64]$channelStats.emptyPixels) $ExpectedPixels "$Prefix.$channel print+empty"
    Assert-Equal ([int64]$channelStats.fullPrintPixels + [int64]$channelStats.partialPrintPixels) ([int64]$channelStats.printPixels) "$Prefix.$channel full+partial"
    Assert-True ($channelStats.minValue -ge 0) "$Prefix.$channel minValue >= 0"
    Assert-True ($channelStats.maxValue -le 255) "$Prefix.$channel maxValue <= 255"
    Assert-True ($channelStats.minValue -le $channelStats.maxValue) "$Prefix.$channel minValue <= maxValue"
  }
}

function Assert-SliceSummary($Manifest, $Slice, $Contract, [string]$CaseId) {
  $totalPixels = [int64]$Manifest.grid.widthPx * [int64]$Manifest.grid.heightPx * [int64]$Manifest.grid.layerCount
  $layerPixels = [int64]$Manifest.grid.widthPx * [int64]$Manifest.grid.heightPx
  Assert-ChannelStats $Slice.totals.channelStats $totalPixels $Contract "$CaseId slice.totals.channelStats"
  foreach ($layer in @($Slice.layers)) {
    Assert-ChannelStats $layer.channelStats $layerPixels $Contract "$CaseId slice.layers[$($layer.layerIndex)].channelStats"
  }
}

function Assert-Texture($Package, $Contract, $Expected, [string]$CaseId) {
  if (-not (Has-Property $Expected "texture")) {
    return
  }
  $texture = Read-Json (Join-Path $Package "reports/texture_report.json")
  Assert-Properties $texture $Contract.requiredTextureFields "$CaseId texture_report"
  Assert-Properties $texture.stats $Contract.requiredTextureStatsFields "$CaseId texture_report.stats"

  $rule = $Expected.texture
  if (Has-Property $rule "enabled") {
    Assert-Equal $texture.enabled $rule.enabled "$CaseId texture.enabled"
  }
  Assert-NumberRule $texture.sampledPixels $rule.sampledPixels "$CaseId texture.sampledPixels"
  Assert-NumberRule $texture.fallbackPixels $rule.fallbackPixels "$CaseId texture.fallbackPixels"
  Assert-NumberRule $texture.missingTextures $rule.missingTextures "$CaseId texture.missingTextures"
  Assert-NumberRule $texture.stats.facesWithUv $rule.facesWithUv "$CaseId texture.facesWithUv"
  Assert-NumberRule $texture.stats.facesWithoutUv $rule.facesWithoutUv "$CaseId texture.facesWithoutUv"

  $textureDenominator = [double]$texture.sampledPixels + [double]$texture.fallbackPixels + [double]$texture.uvOutOfRangePixels
  $textureResolvedRate = Get-Rate $texture.sampledPixels $textureDenominator
  $fallbackPixelRate = Get-Rate $texture.fallbackPixels $textureDenominator
  $uvCoverageRate = Get-Rate $texture.stats.facesWithUv ([double]$texture.stats.facesWithUv + [double]$texture.stats.facesWithoutUv)

  Assert-NumberRule $textureResolvedRate $rule.textureResolvedRate "$CaseId textureResolvedRate"
  Assert-NumberRule $fallbackPixelRate $rule.fallbackPixelRate "$CaseId fallbackPixelRate"
  Assert-NumberRule $uvCoverageRate $rule.uvCoverageRate "$CaseId uvCoverageRate"
}

function Assert-ThreeMf($Package, $Contract, $Expected, [string]$CaseId) {
  if (-not (Has-Property $Expected "threeMf")) {
    return
  }
  $report = Read-Json (Join-Path $Package "reports/three_mf_report.json")
  Assert-Properties $report $Contract.requiredThreeMfFields "$CaseId three_mf_report"
  $rule = $Expected.threeMf
  Assert-NumberRule $report.invalidReferenceCount $rule.invalidReferenceCount "$CaseId three_mf.invalidReferenceCount"
  Assert-NumberRule $report.colorGroupCount $rule.colorGroupCount "$CaseId three_mf.colorGroupCount"
  Assert-NumberRule $report.colorGroupResolvedTriangles $rule.colorGroupResolvedTriangles "$CaseId three_mf.colorGroupResolvedTriangles"
  Assert-NumberRule $report.texture2dGroupCount $rule.texture2dGroupCount "$CaseId three_mf.texture2dGroupCount"
  Assert-NumberRule $report.textureLoadedCount $rule.textureLoadedCount "$CaseId three_mf.textureLoadedCount"
  Assert-NumberRule $report.textureMissingCount $rule.textureMissingCount "$CaseId three_mf.textureMissingCount"
  Assert-NumberRule $report.textureGroupResolvedTriangles $rule.textureGroupResolvedTriangles "$CaseId three_mf.textureGroupResolvedTriangles"
}

function Assert-MaterialPolicy($Package, $Expected, [string]$CaseId) {
  if (-not (Has-Property $Expected "materialPolicy")) {
    return
  }
  $report = Read-Json (Join-Path $Package "reports/material_policy_report.json")
  $rule = $Expected.materialPolicy
  if (Has-Property $rule "enabled") {
    Assert-Equal $report.enabled $rule.enabled "$CaseId materialPolicy.enabled"
  }
  Assert-NumberRule $report.rgb.printPixels $rule.rgbPrintPixels "$CaseId materialPolicy.rgb.printPixels"
  Assert-NumberRule $report.white.printPixels $rule.whitePrintPixels "$CaseId materialPolicy.white.printPixels"
  Assert-NumberRule $report.varnish.printPixels $rule.varnishPrintPixels "$CaseId materialPolicy.varnish.printPixels"
}

function Assert-Case($Case, $Contract, [string]$SlicerExe, [string]$RipExe) {
  Write-Host "== stage10 output contract $($Case.id)"
  Invoke-External "slicer $($Case.id)" $SlicerExe @("--config", $Case.config)

  $package = $Case.package
  Invoke-External "rip_reader $($Case.id)" $RipExe @("--package", $package, "--quiet")

  $manifest = Read-Json (Join-Path $package "manifest.json")
  $slice = Read-Json (Join-Path $package "reports/slice_report.json")
  Assert-Protocol $manifest $Contract $Case.id
  Assert-Reports $package $Contract $Case $Case.id
  Assert-Layers $manifest $slice $Contract $package $Case.id
  Assert-SliceSummary $manifest $slice $Contract $Case.id
  Assert-Equal $manifest.source.format $Case.sourceFormat "$($Case.id) source.format"

  $expected = $Case.expect
  Assert-NumberRule $slice.totals.rgbPrintPixels $expected.rgbPrintPixels "$($Case.id) totals.rgbPrintPixels"
  Assert-NumberRule $slice.totals.whitePrintPixels $expected.whitePrintPixels "$($Case.id) totals.whitePrintPixels"
  Assert-NumberRule $slice.totals.varnishPrintPixels $expected.varnishPrintPixels "$($Case.id) totals.varnishPrintPixels"
  Assert-NumberRule $slice.totals.supportPrintPixels $expected.supportPrintPixels "$($Case.id) totals.supportPrintPixels"
  Assert-Texture $package $Contract $expected $Case.id
  Assert-ThreeMf $package $Contract $expected $Case.id
  Assert-MaterialPolicy $package $expected $Case.id
}

$contract = Read-Json "tests/golden/expected/10_output_contract_schema.json"
$summary = Read-Json "tests/golden/expected/10_output_contract_summary.json"

Assert-Equal $contract.schema "p0.10.output_contract_schema.1" "Stage 10 contract schema"
Assert-Equal $summary.schema "p0.10.output_contract_summary.1" "Stage 10 summary schema"

$slicerExe = Join-Path $BuildDir "Debug/slicer_cli.exe"
$ripExe = Join-Path $BuildDir "Debug/rip_reader_test.exe"
Assert-True (Test-Path $slicerExe) "missing slicer_cli: $slicerExe"
Assert-True (Test-Path $ripExe) "missing rip_reader_test: $ripExe"

foreach ($case in @($summary.cases)) {
  Assert-Case $case $contract $slicerExe $ripExe
}

Write-Host "Stage 10 output contract tests complete."
