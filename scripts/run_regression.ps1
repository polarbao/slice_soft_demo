param(
  [ValidateSet("quick", "full", "heavy")]
  [string]$Mode = "quick",
  [switch]$SkipBuild,
  [switch]$SkipHeavyRelief,
  [switch]$SkipHeavyTexture
)

$ErrorActionPreference = "Stop"

function Run-Step([string]$Name, [scriptblock]$Block) {
  Write-Host "== $Name"
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  try {
    & $Block
    $timer.Stop()
    Write-Host ("PASS {0:N2}s" -f $timer.Elapsed.TotalSeconds)
  } catch {
    $timer.Stop()
    Write-Host ("FAIL {0:N2}s: {1}" -f $timer.Elapsed.TotalSeconds, $_.Exception.Message)
    throw
  }
}

function Run-Slicer([string]$Config) {
  & .\build\Debug\slicer_cli.exe --config $Config
  if ($LASTEXITCODE -ne 0) { throw "slicer failed: $Config" }
}

function Run-Rip([string]$Package) {
  & .\build\Debug\rip_reader_test.exe --package $Package --quiet
  if ($LASTEXITCODE -ne 0) { throw "rip_reader_test failed: $Package" }
}

function Read-Json([string]$Path) {
  return Get-Content -Raw $Path | ConvertFrom-Json
}

function Run-Cases($Cases) {
  foreach ($case in $Cases) {
    Run-Step "slicer $($case.Config)" { Run-Slicer $case.Config }
    Run-Step "rip $($case.Package)" { Run-Rip $case.Package }
  }
}

$basicCases = @(
  @{ Config = "samples/configs/slice_config.json"; Package = "output/SlicePackage" }
)

$storageCases = @(
  @{ Config = "samples/configs/storage_mode/storage_stripped_default.json"; Package = "output/StorageStrippedDefault" },
  @{ Config = "samples/configs/storage_mode/storage_tiled_compat.json"; Package = "output/StorageTiledCompat" },
  @{ Config = "samples/configs/storage_mode/storage_material_policy_rgbwv_stripped.json"; Package = "output/StorageMaterialPolicyRgbwvStripped" },
  @{ Config = "samples/configs/storage_mode/storage_material_policy_rgbwv_tiled.json"; Package = "output/StorageMaterialPolicyRgbwvTiled" }
)

$supportCases = @(
  @{ Config = "samples/configs/support/support_bottom_projection.json"; Package = "output/SupportBottomProjection" },
  @{ Config = "samples/configs/support/support_unsupported_only.json"; Package = "output/SupportUnsupportedOnly" },
  @{ Config = "samples/configs/support/support_bottom_plus_unsupported.json"; Package = "output/SupportBottomPlusUnsupported" },
  @{ Config = "samples/configs/support/support_island_filter.json"; Package = "output/SupportIslandFilter" }
)

$textureSmallCases = @(
  @{ Config = "samples/configs/textured/textured_missing_texture_fallback.json"; Package = "output/TexturedMissingTextureFallback" },
  @{ Config = "samples/configs/textured/textured_no_uv_fallback.json"; Package = "output/TexturedNoUvFallback" }
)

$materialPolicyCases = @(
  @{ Config = "samples/configs/material_policy/textured_rgb_only.json"; Package = "output/MaterialPolicyRgbOnly" },
  @{ Config = "samples/configs/material_policy/textured_rgb_white_underbase.json"; Package = "output/MaterialPolicyRgbWhiteUnderbase" },
  @{ Config = "samples/configs/material_policy/textured_rgb_varnish_top2.json"; Package = "output/MaterialPolicyRgbVarnishTop2" },
  @{ Config = "samples/configs/material_policy/textured_rgb_white_varnish.json"; Package = "output/MaterialPolicyRgbWhiteVarnish" },
  @{ Config = "samples/configs/material_policy/varnish_only_all_model.json"; Package = "output/MaterialPolicyVarnishOnly" },
  @{ Config = "samples/configs/material_policy/white_only_all_model.json"; Package = "output/MaterialPolicyWhiteOnly" }
)

$materialMappingCases = @(
  @{ Config = "samples/configs/material_mapping/obj_mtl_material_mapping_rgbwv.json"; Package = "output/ObjMtlMaterialMappingRgbwv" },
  @{ Config = "samples/configs/material_mapping/obj_mtl_material_mapping_ignore.json"; Package = "output/ObjMtlMaterialMappingIgnore" },
  @{ Config = "samples/configs/material_mapping/obj_mtl_texture_material_mapping_rgbwv.json"; Package = "output/ObjMtlTextureMaterialMappingRgbwv" }
)

$threeMfCases = @(
  @{ Config = "samples/configs/3mf/three_mf_single_rgb.json"; Package = "output/ThreeMfSingleRgb" },
  @{ Config = "samples/configs/3mf/three_mf_multi_object_transform.json"; Package = "output/ThreeMfMultiObjectTransform" },
  @{ Config = "samples/configs/3mf/three_mf_multi_material_rgbwv.json"; Package = "output/ThreeMfMultiMaterialRgbwv" },
  @{ Config = "samples/configs/3mf/three_mf_single_rgb_stored.json"; Package = "output/ThreeMfSingleRgbStored" },
  @{ Config = "samples/configs/3mf/three_mf_single_rgb_deflate.json"; Package = "output/ThreeMfSingleRgbDeflate" },
  @{ Config = "samples/configs/3mf/three_mf_multi_material_deflate.json"; Package = "output/ThreeMfMultiMaterialDeflate" },
  @{ Config = "samples/configs/3mf/three_mf_color_group_rgb.json"; Package = "output/ThreeMfColorGroupRgb" },
  @{ Config = "samples/configs/3mf/three_mf_texture2d_checker.json"; Package = "output/ThreeMfTexture2dChecker" },
  @{ Config = "samples/configs/3mf/three_mf_mixed_color_texture.json"; Package = "output/ThreeMfMixedColorTexture" }
)

$heavyReliefCases = @(
  @{ Config = "samples/configs/relief/relief_nail_varnish_support.json"; Package = "output/ReliefNailVarnishSupport" },
  @{ Config = "samples/configs/relief/relief_nail_white_support.json"; Package = "output/ReliefNailWhiteSupport" },
  @{ Config = "samples/configs/relief/relief_rgb_gray.json"; Package = "output/ReliefRgbGray" }
)

$heavyTextureCases = @(
  @{ Config = "samples/configs/textured/textured_relief_rgb.json"; Package = "output/TexturedReliefRgb" }
)

$quickCases = $basicCases + $storageCases + $supportCases + $textureSmallCases + $materialPolicyCases + $materialMappingCases + $threeMfCases
$heavyCases = @()
if (-not $SkipHeavyRelief) {
  $heavyCases += $heavyReliefCases
}
if (-not $SkipHeavyTexture) {
  $heavyCases += $heavyTextureCases
}

if (-not $SkipBuild) {
  Run-Step "build Debug" {
    cmake --build build --config Debug
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
  }
}

if ($Mode -eq "quick" -or $Mode -eq "full") {
  Run-Step "make 3MF samples" {
    & .\scripts\make_3mf_samples.ps1
    if ($LASTEXITCODE -ne 0) { throw "make_3mf_samples failed" }
  }
}

if ($Mode -eq "quick") {
  Run-Cases $quickCases
} elseif ($Mode -eq "full") {
  Run-Cases ($quickCases + $heavyCases)
} elseif ($Mode -eq "heavy") {
  Run-Cases $heavyCases
}

if ($Mode -eq "quick" -or $Mode -eq "full") {
  Run-Step "verify missing texture fallback" {
    $texture = Read-Json "output/TexturedMissingTextureFallback/reports/texture_report.json"
    if ($texture.missingTextures -le 0) { throw "missing texture fallback did not report missingTextures > 0" }
    if ($texture.warnings.Count -le 0) { throw "missing texture fallback did not report warnings" }
    if ($texture.stats.fallbackPixels -le 0) { throw "missing texture fallback did not report fallbackPixels > 0" }
  }

  Run-Step "verify no-UV fallback" {
    $model = Read-Json "output/TexturedNoUvFallback/reports/model_report.json"
    $texture = Read-Json "output/TexturedNoUvFallback/reports/texture_report.json"
    if ($model.facesWithUv -ne 0) { throw "no-UV fallback expected facesWithUv = 0" }
    if ($model.facesWithoutUv -le 0) { throw "no-UV fallback expected facesWithoutUv > 0" }
    if ($texture.stats.fallbackPixels -le 0) { throw "no-UV fallback did not report fallbackPixels > 0" }
  }

  Run-Step "verify TIFF storage modes" {
    $stripped = Read-Json "output/SlicePackage/manifest.json"
    if ($stripped.schema -ne "p0.rgbwsv.2") { throw "default package expected schema p0.rgbwsv.2" }
    if ($stripped.tiff.storageMode -ne "stripped") { throw "default package expected storageMode stripped" }
    if ($stripped.tiff.tiled -ne $false) { throw "default package expected tiled=false" }
    if ($stripped.tiff.rowsPerStrip -ne 64) { throw "default package expected rowsPerStrip=64" }

    $tiled = Read-Json "output/StorageTiledCompat/manifest.json"
    if ($tiled.schema -ne "p0.rgbwsv.2") { throw "tiled compat package expected schema p0.rgbwsv.2" }
    if ($tiled.tiff.storageMode -ne "tiled") { throw "tiled compat package expected storageMode tiled" }
    if ($tiled.tiff.tiled -ne $true) { throw "tiled compat package expected tiled=true" }
    if ($tiled.tiff.tileSize[0] -ne 256 -or $tiled.tiff.tileSize[1] -ne 256) { throw "tiled compat package expected tileSize 256x256" }
  }

  Run-Step "verify material policy samples" {
    $rgbOnly = Read-Json "output/MaterialPolicyRgbOnly/reports/material_policy_report.json"
    if ($rgbOnly.rgb.printPixels -le 0) { throw "RGB only expected RGB printPixels > 0" }
    if ($rgbOnly.white.printPixels -ne 0) { throw "RGB only expected W printPixels = 0" }
    if ($rgbOnly.varnish.printPixels -ne 0) { throw "RGB only expected V printPixels = 0" }

    $rgbWhite = Read-Json "output/MaterialPolicyRgbWhiteUnderbase/reports/material_policy_report.json"
    if ($rgbWhite.rgb.printPixels -le 0) { throw "RGB+W expected RGB printPixels > 0" }
    if ($rgbWhite.white.printPixels -le 0) { throw "RGB+W expected W printPixels > 0" }
    if ($rgbWhite.varnish.printPixels -ne 0) { throw "RGB+W expected V printPixels = 0" }

    $rgbVarnish = Read-Json "output/MaterialPolicyRgbVarnishTop2/reports/material_policy_report.json"
    if ($rgbVarnish.rgb.printPixels -le 0) { throw "RGB+V expected RGB printPixels > 0" }
    if ($rgbVarnish.white.printPixels -ne 0) { throw "RGB+V expected W printPixels = 0" }
    if ($rgbVarnish.varnish.printPixels -le 0) { throw "RGB+V expected V printPixels > 0" }

    $rgbWhiteVarnish = Read-Json "output/MaterialPolicyRgbWhiteVarnish/reports/material_policy_report.json"
    if ($rgbWhiteVarnish.rgb.printPixels -le 0) { throw "RGB+W+V expected RGB printPixels > 0" }
    if ($rgbWhiteVarnish.white.printPixels -le 0) { throw "RGB+W+V expected W printPixels > 0" }
    if ($rgbWhiteVarnish.varnish.printPixels -le 0) { throw "RGB+W+V expected V printPixels > 0" }

    $varnishOnly = Read-Json "output/MaterialPolicyVarnishOnly/reports/material_policy_report.json"
    if ($varnishOnly.rgb.printPixels -ne 0) { throw "V only expected RGB printPixels = 0" }
    if ($varnishOnly.white.printPixels -ne 0) { throw "V only expected W printPixels = 0" }
    if ($varnishOnly.varnish.printPixels -le 0) { throw "V only expected V printPixels > 0" }

    $whiteOnly = Read-Json "output/MaterialPolicyWhiteOnly/reports/material_policy_report.json"
    if ($whiteOnly.rgb.printPixels -ne 0) { throw "W only expected RGB printPixels = 0" }
    if ($whiteOnly.white.printPixels -le 0) { throw "W only expected W printPixels > 0" }
    if ($whiteOnly.varnish.printPixels -ne 0) { throw "W only expected V printPixels = 0" }

    $storageRgbwvStripped = Read-Json "output/StorageMaterialPolicyRgbwvStripped/reports/material_policy_report.json"
    if ($storageRgbwvStripped.rgb.printPixels -le 0) { throw "storage RGB+W+V stripped expected RGB printPixels > 0" }
    if ($storageRgbwvStripped.white.printPixels -le 0) { throw "storage RGB+W+V stripped expected W printPixels > 0" }
    if ($storageRgbwvStripped.varnish.printPixels -le 0) { throw "storage RGB+W+V stripped expected V printPixels > 0" }

    $storageRgbwvTiled = Read-Json "output/StorageMaterialPolicyRgbwvTiled/reports/material_policy_report.json"
    if ($storageRgbwvTiled.rgb.printPixels -le 0) { throw "storage RGB+W+V tiled expected RGB printPixels > 0" }
    if ($storageRgbwvTiled.white.printPixels -le 0) { throw "storage RGB+W+V tiled expected W printPixels > 0" }
    if ($storageRgbwvTiled.varnish.printPixels -le 0) { throw "storage RGB+W+V tiled expected V printPixels > 0" }
  }

  Run-Step "verify material role mapping samples" {
    $objRgbwv = Read-Json "output/ObjMtlMaterialMappingRgbwv/reports/material_role_mapping_report.json"
    if ($objRgbwv.enabled -ne $true) { throw "OBJ/MTL RGBWV expected material role mapping enabled" }
    if ($objRgbwv.mappedRgb -le 0) { throw "OBJ/MTL RGBWV expected mappedRgb > 0" }
    if ($objRgbwv.mappedWhite -le 0) { throw "OBJ/MTL RGBWV expected mappedWhite > 0" }
    if ($objRgbwv.mappedVarnish -le 0) { throw "OBJ/MTL RGBWV expected mappedVarnish > 0" }

    $objMtl = Read-Json "output/ObjMtlMaterialMappingRgbwv/reports/obj_mtl_material_report.json"
    if ($objMtl.inputFormat -ne "obj") { throw "OBJ/MTL report expected inputFormat obj" }
    if ($objMtl.materialCount -lt 3) { throw "OBJ/MTL RGBWV expected at least 3 materials" }
    if ($objMtl.facesWithMaterial -le 0) { throw "OBJ/MTL RGBWV expected facesWithMaterial > 0" }

    $objIgnore = Read-Json "output/ObjMtlMaterialMappingIgnore/reports/material_role_mapping_report.json"
    if ($objIgnore.mappedIgnore -le 0) { throw "OBJ/MTL ignore expected mappedIgnore > 0" }

    $objTexture = Read-Json "output/ObjMtlTextureMaterialMappingRgbwv/reports/texture_report.json"
    if ($objTexture.stats.sampledPixels -le 0) { throw "OBJ/MTL texture mapping expected sampledPixels > 0" }
    $objTextureRole = Read-Json "output/ObjMtlTextureMaterialMappingRgbwv/reports/material_role_mapping_report.json"
    if ($objTextureRole.mappedRgb -le 0) { throw "OBJ/MTL texture mapping expected mappedRgb > 0" }
    if ($objTextureRole.mappedWhite -le 0) { throw "OBJ/MTL texture mapping expected mappedWhite > 0" }
    if ($objTextureRole.mappedVarnish -le 0) { throw "OBJ/MTL texture mapping expected mappedVarnish > 0" }
  }

  Run-Step "verify 3MF samples" {
    $threeMfSingleModel = Read-Json "output/ThreeMfSingleRgb/reports/model_report.json"
    if ($threeMfSingleModel.format -ne "3mf") { throw "3MF single expected model_report format 3mf" }
    $threeMfSingle = Read-Json "output/ThreeMfSingleRgb/reports/three_mf_report.json"
    if ($threeMfSingle.enabled -ne $true) { throw "3MF single expected three_mf_report enabled" }
    if ($threeMfSingle.objectCount -le 0) { throw "3MF single expected objectCount > 0" }
    if ($threeMfSingle.triangleCount -le 0) { throw "3MF single expected triangleCount > 0" }

    $threeMfTransform = Read-Json "output/ThreeMfMultiObjectTransform/reports/three_mf_report.json"
    if ($threeMfTransform.componentCount -le 0) { throw "3MF transform expected componentCount > 0" }
    if ($threeMfTransform.meshObjectCount -le 0) { throw "3MF transform expected meshObjectCount > 0" }

    $threeMfRgbwv = Read-Json "output/ThreeMfMultiMaterialRgbwv/reports/material_role_mapping_report.json"
    if ($threeMfRgbwv.inputFormat -ne "3mf") { throw "3MF RGBWV expected material mapping inputFormat 3mf" }
    if ($threeMfRgbwv.mappedRgb -le 0) { throw "3MF RGBWV expected mappedRgb > 0" }
    if ($threeMfRgbwv.mappedWhite -le 0) { throw "3MF RGBWV expected mappedWhite > 0" }
    if ($threeMfRgbwv.mappedVarnish -le 0) { throw "3MF RGBWV expected mappedVarnish > 0" }
    if ($threeMfRgbwv.mappedSupport -ne 0) { throw "3MF RGBWV expected mappedSupport = 0 when allowInputSupportMaterial=false" }

    $threeMfDeflate = Read-Json "output/ThreeMfSingleRgbDeflate/reports/three_mf_report.json"
    if ($threeMfDeflate.zip.deflatedEntryCount -le 0) { throw "3MF deflate expected deflatedEntryCount > 0" }
    if ($threeMfDeflate.xml.parser -ne "restricted_string_xml_reader") { throw "3MF report expected restricted XML parser" }
    if ($threeMfDeflate.validation.invalidReferenceCount -ne 0) { throw "3MF deflate expected invalidReferenceCount = 0" }

    $threeMfStored = Read-Json "output/ThreeMfSingleRgbStored/reports/three_mf_report.json"
    if ($threeMfStored.zip.storedEntryCount -le 0) { throw "3MF stored expected storedEntryCount > 0" }

    $threeMfColor = Read-Json "output/ThreeMfColorGroupRgb/reports/three_mf_report.json"
    if ($threeMfColor.colorGroups.count -le 0) { throw "3MF ColorGroup expected colorGroups.count > 0" }
    if ($threeMfColor.colorGroups.colorCount -le 0) { throw "3MF ColorGroup expected colorCount > 0" }
    if ($threeMfColor.colorGroups.resolvedTriangles -le 0) { throw "3MF ColorGroup expected resolvedTriangles > 0" }
    if ($threeMfColor.colorGroups.interpolatedColorFallbackCount -le 0) { throw "3MF ColorGroup expected interpolatedColorFallbackCount > 0" }

    $threeMfTexture = Read-Json "output/ThreeMfTexture2dChecker/reports/three_mf_report.json"
    if ($threeMfTexture.textures.texture2dCount -le 0) { throw "3MF Texture2D expected texture2dCount > 0" }
    if ($threeMfTexture.textures.texture2dGroupCount -le 0) { throw "3MF Texture2D expected texture2dGroupCount > 0" }
    if ($threeMfTexture.textures.loadedCount -le 0) { throw "3MF Texture2D expected loadedCount > 0" }
    if ($threeMfTexture.textures.sampledPixels -le 0) { throw "3MF Texture2D expected sampledPixels > 0" }
    $threeMfTextureReport = Read-Json "output/ThreeMfTexture2dChecker/reports/texture_report.json"
    if ($threeMfTextureReport.source -ne "3mf_internal") { throw "3MF Texture2D expected texture_report source 3mf_internal" }
    if ($threeMfTextureReport.stats.sampledPixels -le 0) { throw "3MF Texture2D expected texture_report sampledPixels > 0" }

    $threeMfMixed = Read-Json "output/ThreeMfMixedColorTexture/reports/three_mf_report.json"
    if ($threeMfMixed.colorGroups.resolvedTriangles -le 0) { throw "3MF mixed expected ColorGroup resolvedTriangles > 0" }
    if ($threeMfMixed.textures.resolvedTriangles -le 0) { throw "3MF mixed expected Texture2DGroup resolvedTriangles > 0" }
  }

  Run-Step "make bad 3MF packages" {
    & .\scripts\make_bad_3mf_packages.ps1
    if ($LASTEXITCODE -ne 0) { throw "make_bad_3mf_packages failed" }
  }

  Run-Step "bad 3MF packages" {
    & .\scripts\run_3mf_negative_tests.ps1
    if ($LASTEXITCODE -ne 0) { throw "run_3mf_negative_tests failed" }
  }

  Run-Step "make bad packages" {
    & .\scripts\make_bad_packages.ps1 -GoodPackage "output/SlicePackage" -OutputRoot "tests/packages/bad"
    if ($LASTEXITCODE -ne 0) { throw "make_bad_packages failed" }
  }

  $bad = @(
    @{ Name = "bad_missing_manifest"; Code = "E_MANIFEST_MISSING" },
    @{ Name = "bad_manifest_parse"; Code = "E_MANIFEST_PARSE_FAILED" },
    @{ Name = "bad_schema"; Code = "E_SCHEMA_UNSUPPORTED" },
    @{ Name = "bad_bit_depth"; Code = "E_BIT_DEPTH_INVALID" },
    @{ Name = "bad_channel_order"; Code = "E_CHANNEL_ORDER_INVALID" },
    @{ Name = "bad_channel_count"; Code = "E_CHANNEL_COUNT_INVALID" },
    @{ Name = "bad_polarity"; Code = "E_POLARITY_INVALID" },
    @{ Name = "bad_print_value"; Code = "E_PRINT_EMPTY_VALUE_INVALID" },
    @{ Name = "bad_empty_value"; Code = "E_PRINT_EMPTY_VALUE_INVALID" },
    @{ Name = "bad_grid"; Code = "E_GRID_INVALID" },
    @{ Name = "bad_missing_layer"; Code = "E_LAYER_MISSING" },
    @{ Name = "bad_layer_size"; Code = "E_LAYER_SIZE_MISMATCH" },
    @{ Name = "bad_samples_per_pixel"; Code = "E_TIFF_SAMPLE_COUNT_INVALID" },
    @{ Name = "bad_planar_config"; Code = "E_TIFF_PLANAR_CONFIG_INVALID" },
    @{ Name = "bad_storage_mode"; Code = "E_TIFF_STORAGE_MODE_INVALID" },
    @{ Name = "bad_rows_per_strip"; Code = "E_ROWS_PER_STRIP_INVALID" },
    @{ Name = "bad_tiff_storage_mismatch"; Code = "E_TIFF_STORAGE_MISMATCH" },
    @{ Name = "bad_tile_size"; Code = "E_TILE_SIZE_INVALID" }
  )

  foreach ($case in $bad) {
    Run-Step "bad package $($case.Name)" {
      & .\build\Debug\rip_reader_test.exe --package "tests/packages/bad/$($case.Name)" --expect-error --expect-code $case.Code --quiet
      if ($LASTEXITCODE -ne 0) { throw "bad package expectation failed: $($case.Name)" }
    }
  }
}

Write-Host "Regression complete. mode=$Mode"
