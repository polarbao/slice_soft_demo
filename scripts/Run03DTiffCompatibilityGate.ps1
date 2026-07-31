[CmdletBinding()]
param(
    [string]$HandwrittenBuildDir = "build-slicesoft/main",
    [string]$LibTiffBuildDir = "build-slicesoft/03d-libtiff",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$OutputDir = "output/benchmarks/03d_05",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepositoryPath
{
    param(
        [string]$Root,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Resolve-Executable
{
    param(
        [string]$BuildDir,
        [string]$BuildConfig,
        [string]$Name
    )

    foreach ($candidate in @(
        (Join-Path $BuildDir "$BuildConfig/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe")))
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Executable $Name was not found under $BuildDir."
}

function Invoke-NativeStep
{
    param(
        [string]$Name,
        [string]$Executable,
        [string[]]$Arguments
    )

    Write-Host "== $Name"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Write-Utf8NoBom
{
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function New-GateConfig
{
    param(
        [string]$SourcePath,
        [string]$DestinationPath,
        [string]$PackagePath
    )

    $sourceAbsolute = (Resolve-Path -LiteralPath $SourcePath).Path
    $sourceDirectory = Split-Path -Parent $sourceAbsolute
    $config = Get-Content -Raw -Encoding UTF8 -LiteralPath $sourceAbsolute |
        ConvertFrom-Json
    if (-not [System.IO.Path]::IsPathRooted($config.input.modelPath))
    {
        $config.input.modelPath = [System.IO.Path]::GetFullPath(
            (Join-Path $sourceDirectory $config.input.modelPath))
    }
    $config.output.packageDir = [System.IO.Path]::GetFullPath($PackagePath)
    Write-Utf8NoBom `
        -Path $DestinationPath `
        -Content ($config | ConvertTo-Json -Depth 100)
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$handwrittenBuild = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $HandwrittenBuildDir
$libTiffBuild = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $LibTiffBuildDir
$outputRoot = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $OutputDir

Push-Location $repoRoot
try
{
    $writerTargets = @(
        "tiff_writer_contract_unit_tests",
        "tiff_backend_build_info_unit_tests",
        "tiff_writer_backend_unit_tests",
        "tiff_writer_equivalence_unit_tests",
        "slicer_cli",
        "rip_reader_test"
    )
    $packageTargets = @(
        "rgbwsv_production_package_writer_unit_tests",
        "global_surface_shell_production_pipeline_unit_tests",
        "multi_model_package_writer_unit_tests",
        "multi_model_production_service_unit_tests"
    )
    if (-not $SkipBuild)
    {
        $handwrittenBuildArguments = @(
            "--build", $handwrittenBuild,
            "--config", $Config,
            "--target")
        $handwrittenBuildArguments += $writerTargets
        $handwrittenBuildArguments += $packageTargets
        $handwrittenBuildArguments += @("--parallel", "8")
        Invoke-NativeStep `
            -Name "build handwritten compatibility targets" `
            -Executable "cmake" `
            -Arguments $handwrittenBuildArguments
        $libTiffBuildArguments = @(
            "--build", $libTiffBuild,
            "--config", $Config,
            "--target")
        $libTiffBuildArguments += $writerTargets
        $libTiffBuildArguments += $packageTargets
        $libTiffBuildArguments += @("--parallel", "8")
        Invoke-NativeStep `
            -Name "build LibTIFF compatibility targets" `
            -Executable "cmake" `
            -Arguments $libTiffBuildArguments
    }

    $compatibilityRegex =
        "^(tiff_writer_contract_unit_tests|" +
        "tiff_backend_build_info_unit_tests|" +
        "tiff_writer_backend_unit_tests|" +
        "tiff_writer_equivalence_unit_tests|" +
        "rgbwsv_production_package_writer_unit_tests|" +
        "global_surface_shell_production_pipeline_unit_tests|" +
        "multi_model_package_writer_unit_tests|" +
        "multi_model_production_service_unit_tests)$"
    Invoke-NativeStep `
        -Name "handwritten writer and shared package contract" `
        -Executable "ctest" `
        -Arguments @(
            "--test-dir", $handwrittenBuild,
            "-C", $Config,
            "--output-on-failure",
            "-R", $compatibilityRegex)

    Invoke-NativeStep `
        -Name "LibTIFF writer and shared package contract" `
        -Executable "ctest" `
        -Arguments @(
            "--test-dir", $libTiffBuild,
            "-C", $Config,
            "--output-on-failure",
            "-R", $compatibilityRegex)

    $handwrittenCli = Resolve-Executable `
        -BuildDir $handwrittenBuild `
        -BuildConfig $Config `
        -Name "slicer_cli"
    $handwrittenRip = Resolve-Executable `
        -BuildDir $handwrittenBuild `
        -BuildConfig $Config `
        -Name "rip_reader_test"
    $libTiffCli = Resolve-Executable `
        -BuildDir $libTiffBuild `
        -BuildConfig $Config `
        -Name "slicer_cli"
    $libTiffRip = Resolve-Executable `
        -BuildDir $libTiffBuild `
        -BuildConfig $Config `
        -Name "rip_reader_test"

    $capabilityJson = & $libTiffCli --tiff-backend-info-json
    if ($LASTEXITCODE -ne 0)
    {
        throw "LibTIFF capability query failed."
    }
    $capability = $capabilityJson | ConvertFrom-Json
    if ($capability.configuredBackend -ne "libtiff" -or
        -not $capability.libtiffStrippedWriterImplemented -or
        -not $capability.libtiffTiledWriterImplemented)
    {
        throw "The selected LibTIFF build does not expose stripped+tiled writer capability."
    }

    $fixtures = @(
        [ordered]@{
            Name = "legacy_stripped"
            Config = "samples/configs/golden/material_process_top2_fixture.json"
        },
        [ordered]@{
            Name = "legacy_tiled"
            Config = "samples/configs/storage_mode/storage_tiled_compat.json"
        }
    )
    foreach ($backend in @(
        [ordered]@{
            Name = "handwritten"
            Cli = $handwrittenCli
            Rip = $handwrittenRip
        },
        [ordered]@{
            Name = "libtiff"
            Cli = $libTiffCli
            Rip = $libTiffRip
        }))
    {
        foreach ($fixture in $fixtures)
        {
            $caseRoot = Join-Path `
                $outputRoot `
                "$($backend.Name)/$($fixture.Name)"
            $generatedConfig = Join-Path $caseRoot "slice_config.generated.json"
            $packagePath = Join-Path $caseRoot "package"
            New-GateConfig `
                -SourcePath $fixture.Config `
                -DestinationPath $generatedConfig `
                -PackagePath $packagePath
            Invoke-NativeStep `
                -Name "$($backend.Name) $($fixture.Name) package" `
                -Executable $backend.Cli `
                -Arguments @("--config", $generatedConfig)
            Invoke-NativeStep `
                -Name "$($backend.Name) $($fixture.Name) RIP strict" `
                -Executable $backend.Rip `
                -Arguments @("--package", $packagePath, "--quiet")
        }
    }

    $badPackages = @(
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
    foreach ($case in $badPackages)
    {
        Invoke-NativeStep `
            -Name "RIP bad package $($case.Name)" `
            -Executable $libTiffRip `
            -Arguments @(
                "--package", "tests/packages/bad/$($case.Name)",
                "--expect-error",
                "--expect-code", $case.Code,
                "--quiet")
    }

    Write-Host "03D-05 compatibility gate: PASS"
}
finally
{
    Pop-Location
}
