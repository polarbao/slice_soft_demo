param(
  [switch]$SkipHeavyRelief
)

$ErrorActionPreference = "Stop"

function Run-Step([string]$Name, [scriptblock]$Block) {
  Write-Host "== $Name"
  & $Block
  if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
    throw "Step failed: $Name"
  }
}

function Run-Slicer([string]$Config) {
  & .\build\Debug\slicer_cli.exe --config $Config
  if ($LASTEXITCODE -ne 0) { throw "slicer failed: $Config" }
}

function Run-Rip([string]$Package) {
  & .\build\Debug\rip_reader_test.exe --package $Package
  if ($LASTEXITCODE -ne 0) { throw "rip_reader_test failed: $Package" }
}

Run-Step "build Debug" {
  cmake --build build --config Debug
}

$positive = @(
  @{ Config = "samples/configs/slice_config.json"; Package = "output/SlicePackage" },
  @{ Config = "samples/configs/support/support_bottom_projection.json"; Package = "output/SupportBottomProjection" },
  @{ Config = "samples/configs/support/support_unsupported_only.json"; Package = "output/SupportUnsupportedOnly" },
  @{ Config = "samples/configs/support/support_bottom_plus_unsupported.json"; Package = "output/SupportBottomPlusUnsupported" },
  @{ Config = "samples/configs/support/support_island_filter.json"; Package = "output/SupportIslandFilter" },
  @{ Config = "samples/configs/textured/textured_relief_rgb.json"; Package = "output/TexturedReliefRgb" },
  @{ Config = "samples/configs/textured/textured_missing_texture_fallback.json"; Package = "output/TexturedMissingTextureFallback" },
  @{ Config = "samples/configs/textured/textured_no_uv_fallback.json"; Package = "output/TexturedNoUvFallback" }
)

if (-not $SkipHeavyRelief) {
  $positive += @(
    @{ Config = "samples/configs/relief/relief_nail_varnish_support.json"; Package = "output/ReliefNailVarnishSupport" },
    @{ Config = "samples/configs/relief/relief_nail_white_support.json"; Package = "output/ReliefNailWhiteSupport" },
    @{ Config = "samples/configs/relief/relief_rgb_gray.json"; Package = "output/ReliefRgbGray" }
  )
}

foreach ($case in $positive) {
  Run-Step "slicer $($case.Config)" { Run-Slicer $case.Config }
  Run-Step "rip $($case.Package)" { Run-Rip $case.Package }
}

Run-Step "make bad packages" {
  & .\scripts\make_bad_packages.ps1 -GoodPackage "output/SlicePackage" -OutputRoot "tests/packages/bad"
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
  @{ Name = "bad_planar_config"; Code = "E_TIFF_PLANAR_CONFIG_INVALID" }
)

foreach ($case in $bad) {
  Run-Step "bad package $($case.Name)" {
    & .\build\Debug\rip_reader_test.exe --package "tests/packages/bad/$($case.Name)" --expect-error --expect-code $case.Code
    if ($LASTEXITCODE -ne 0) { throw "bad package expectation failed: $($case.Name)" }
  }
}

Write-Host "Regression complete."
