param(
  [string]$BuildDir = "build",
  [switch]$IncludeRealObjMatrix
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

function Invoke-External([string]$Name, [string]$Exe, [string[]]$Arguments) {
  & $Exe @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$Name failed with exit code $LASTEXITCODE"
  }
}

function Assert-StackReport([string]$Package, $Golden, [string]$CaseId) {
  $manifest = Read-Json (Join-Path $Package "manifest.json")
  $slice = Read-Json (Join-Path $Package "reports/slice_report.json")
  $reportPath = Join-Path $Package "reports/cross_section_material_stack_report.json"
  $report = Read-Json $reportPath

  Assert-True (Test-Path $reportPath) "$CaseId missing cross section stack report"
  Assert-Equal $report.schema $Golden.reportSchema "$CaseId report schema"
  Assert-Equal $manifest.reports.crossSectionMaterialStack "reports/cross_section_material_stack_report.json" "$CaseId manifest report entry"
  Assert-Equal $report.stackOrder $Golden.expectedStackOrderText "$CaseId stackOrder"
  Assert-Equal $report.reference.diagram $Golden.requiredReference.diagram "$CaseId reference.diagram"
  Assert-Equal $report.reference.realRipLayer $Golden.requiredReference.realRipLayer "$CaseId reference.realRipLayer"
  Assert-Equal $report.reference.oldConceptDiagramGeometryAcceptance $Golden.requiredReference.oldConceptDiagramGeometryAcceptance "$CaseId old diagram status"
  Assert-True $report.summary.canExplainRealRipCrossSection "$CaseId cannot explain real RIP cross section"
  Assert-Equal @($report.summary.missingElements).Count 0 "$CaseId missing material stack elements"
  Assert-True ($null -ne $slice.totals.crossSectionMaterialStack) "$CaseId slice totals missing crossSectionMaterialStack"

  $stack = @($report.stack)
  $expected = @($Golden.expectedOrder)
  Assert-Equal $stack.Count $expected.Count "$CaseId stack count"
  for ($i = 0; $i -lt $expected.Count; ++$i) {
    $entry = $stack[$i]
    Assert-Equal $entry.order ($i + 1) "$CaseId stack[$i].order"
    Assert-Equal $entry.id $expected[$i] "$CaseId stack[$i].id"
    Assert-True $entry.present "$CaseId stack[$i] should be present"
    Assert-True ([int64]$entry.printPixels -gt 0) "$CaseId stack[$i] printPixels must be positive"
  }
}

function Run-Case([string]$CaseId, [string]$Config, [string]$Package, [string]$SlicerExe, [string]$RipExe, $Golden) {
  Write-Host "== 12A cross section stack $CaseId"
  Invoke-External "slicer $CaseId" $SlicerExe @("--config", $Config)
  Invoke-External "rip_reader $CaseId" $RipExe @("--package", $Package, "--quiet")
  Assert-StackReport $Package $Golden $CaseId
}

function Write-RealObjConfig([string]$CaseId, [string]$ModelPath, [string]$PackageDir, [string]$ConfigPath) {
  $config = [ordered]@{
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
    texture = [ordered]@{
      enabled = $true
      applyMode = "top_surface_band"
      topSurfaceLayers = 5
      sampler = "bilinear"
      uvAddressMode = "clamp"
      flipV = $true
      fallbackRgb = @(0, 0, 0)
      missingTexturePolicy = "warn_and_fallback"
      nonSurfaceRgbPolicy = "empty"
    }
  }

  New-Item -ItemType Directory -Force -Path (Split-Path $ConfigPath -Parent) | Out-Null
  $jsonText = $config | ConvertTo-Json -Depth 20
  [System.IO.File]::WriteAllText(
    (Resolve-Path (Split-Path $ConfigPath -Parent)).Path + [System.IO.Path]::DirectorySeparatorChar + (Split-Path $ConfigPath -Leaf),
    $jsonText,
    [System.Text.UTF8Encoding]::new($false))
}

$golden = Read-Json "tests/golden/expected/12a_cross_section_material_stack_summary.json"
Assert-Equal $golden.schema "p0.12a.cross_section_material_stack_summary.1" "golden schema"

$slicerExe = Join-Path $BuildDir "Debug/slicer_cli.exe"
$ripExe = Join-Path $BuildDir "Debug/rip_reader_test.exe"
Assert-True (Test-Path $slicerExe) "missing slicer_cli: $slicerExe"
Assert-True (Test-Path $ripExe) "missing rip_reader_test: $ripExe"

Run-Case `
  "cross_section_real_obj" `
  "samples/configs/support/cross_section_material_stack_real_obj.json" `
  "output/CrossSectionMaterialStackRealObj" `
  $slicerExe `
  $ripExe `
  $golden

if ($IncludeRealObjMatrix) {
  $matrix = @(
    @{ id = "nai_you"; model = "model/obj/nai_you_new/MF_nai_you.obj" },
    @{ id = "aishen"; model = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj" },
    @{ id = "yecan"; model = "model/obj/yecan/3.obj" }
  )
  foreach ($case in $matrix) {
    $configPath = "output/12a10_validation/configs/$($case.id).json"
    $package = "output/12a10_validation/$($case.id)/package"
    Write-RealObjConfig $case.id $case.model $package $configPath
    Run-Case $case.id $configPath $package $slicerExe $ripExe $golden
  }
}

Write-Host "12A cross section material stack tests complete."
