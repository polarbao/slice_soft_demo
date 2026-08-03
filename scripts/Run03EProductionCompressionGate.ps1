[CmdletBinding()]
param(
    [string]$HandwrittenBuildDir = "build-slicesoft/main",
    [string]$LibTiffBuildDir = "build-slicesoft/03d-libtiff",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$FixtureConfig = "samples/configs/golden/material_process_top2_fixture.json",
    [string]$RealModelConfig = "samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json",
    [string]$OutputDir = "output/benchmarks/03e_02",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepositoryPath {
    param(
        [string]$Root,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Resolve-Executable {
    param(
        [string]$BuildDir,
        [string]$BuildConfig,
        [string]$Name
    )

    foreach ($candidate in @(
        (Join-Path $BuildDir "$BuildConfig/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe"))) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Executable $Name was not found under $BuildDir."
}

function Invoke-NativeStep {
    param(
        [string]$Name,
        [string]$Executable,
        [string[]]$Arguments
    )

    Write-Host "== $Name"
    & $Executable @Arguments | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force |
        Out-Null
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function New-ProductionConfig {
    param(
        [string]$SourcePath,
        [string]$DestinationPath,
        [string]$PackagePath,
        [string]$Compression
    )

    $sourceAbsolute = (Resolve-Path -LiteralPath $SourcePath).Path
    $sourceDirectory = Split-Path -Parent $sourceAbsolute
    $document = Get-Content -Raw -Encoding UTF8 -LiteralPath $sourceAbsolute |
        ConvertFrom-Json
    if (-not [System.IO.Path]::IsPathRooted($document.input.modelPath)) {
        $document.input.modelPath = [System.IO.Path]::GetFullPath(
            (Join-Path $sourceDirectory $document.input.modelPath))
    }
    $document.output.packageDir = [System.IO.Path]::GetFullPath($PackagePath)
    $document.output | Add-Member `
        -NotePropertyName tiffCompression `
        -NotePropertyValue ([pscustomobject]@{ algorithm = $Compression }) `
        -Force
    Write-Utf8NoBom `
        -Path $DestinationPath `
        -Content ($document | ConvertTo-Json -Depth 100)
}

function Get-FirstLayerPath {
    param($Manifest)

    $layers = $Manifest.layers
    if ($null -eq $layers) {
        $layers = $Manifest.tiff.layers
    }
    if ($null -eq $layers -or $layers.Count -eq 0) {
        throw "Manifest does not contain layers."
    }
    return [string]$layers[0].path
}

function Get-TiffCompressionTag {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 8 -or $bytes[0] -ne 0x49 -or $bytes[1] -ne 0x49) {
        throw "Only little-endian TIFF is supported by this gate: $Path"
    }
    $ifdOffset = [BitConverter]::ToUInt32($bytes, 4)
    $entryCount = [BitConverter]::ToUInt16($bytes, $ifdOffset)
    for ($index = 0; $index -lt $entryCount; ++$index) {
        $entryOffset = $ifdOffset + 2 + ($index * 12)
        if ([BitConverter]::ToUInt16($bytes, $entryOffset) -eq 259) {
            return [int][BitConverter]::ToUInt16($bytes, $entryOffset + 8)
        }
    }
    throw "TIFF Compression(259) tag was not found: $Path"
}

function Copy-Package {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force |
        Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse
}

function Read-Manifest {
    param([string]$PackagePath)

    return Get-Content -Raw -Encoding UTF8 `
        -LiteralPath (Join-Path $PackagePath "manifest.json") |
        ConvertFrom-Json
}

function Write-Manifest {
    param(
        [string]$PackagePath,
        $Manifest
    )

    Write-Utf8NoBom `
        -Path (Join-Path $PackagePath "manifest.json") `
        -Content ($Manifest | ConvertTo-Json -Depth 100)
}

function Invoke-ProductionCase {
    param(
        [hashtable]$Backend,
        [string]$Compression,
        [string]$CaseName,
        [string]$SourceConfig,
        [string]$RunRoot
    )

    $caseRoot = Join-Path $RunRoot "$CaseName/$($Backend.Name)/$Compression"
    $packagePath = Join-Path $caseRoot "package"
    $generatedConfig = Join-Path $caseRoot "slice_config.generated.json"
    New-ProductionConfig `
        -SourcePath $SourceConfig `
        -DestinationPath $generatedConfig `
        -PackagePath $packagePath `
        -Compression $Compression

    Write-Host "== $CaseName $($Backend.Name) $Compression production package"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    & $Backend.Cli --config $generatedConfig |
        ForEach-Object { Write-Host $_ }
    $exitCode = $LASTEXITCODE
    $stopwatch.Stop()
    if ($exitCode -ne 0) {
        throw "$CaseName $($Backend.Name) $Compression package failed with exit code $exitCode."
    }

    $summary = & $Backend.Rip --package $packagePath --summary 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "$CaseName $($Backend.Name) $Compression RIP strict failed: $($summary -join [Environment]::NewLine)"
    }
    $summaryText = $summary -join [Environment]::NewLine
    Assert-True `
        ($summaryText -match "compression:\s+$Compression") `
        "$CaseName $($Backend.Name) $Compression RIP summary did not report the expected compression."

    $manifest = Read-Manifest $packagePath
    Assert-True `
        ($manifest.schema -eq "p0.rgbwsv.2") `
        "$CaseName $($Backend.Name) $Compression changed the package schema."
    Assert-True `
        ($manifest.tiff.compression -eq $Compression) `
        "$CaseName $($Backend.Name) $Compression manifest declaration mismatch."
    Assert-True `
        (($manifest.tiff.channelOrder -join " ") -eq "R G B W S V") `
        "$CaseName $($Backend.Name) $Compression changed channel order."
    Assert-True `
        ($manifest.tiff.bitDepth -eq 8 -and
         $manifest.tiff.polarity -eq "black_is_print") `
        "$CaseName $($Backend.Name) $Compression changed bit depth or polarity."

    $firstLayer = Join-Path $packagePath (Get-FirstLayerPath $manifest)
    $expectedTag = if ($Compression -eq "packbits") { 32773 } else { 1 }
    $compressionTag = Get-TiffCompressionTag $firstLayer
    Assert-True `
        ($compressionTag -eq $expectedTag) `
        "$CaseName $($Backend.Name) $Compression Compression(259) mismatch."
    $tiffFiles = @(Get-ChildItem -LiteralPath (Join-Path $packagePath "layers") `
        -Filter "*.tiff" -File)
    $tiffBytes = ($tiffFiles | Measure-Object -Property Length -Sum).Sum

    return [pscustomobject][ordered]@{
        caseName = $CaseName
        backend = $Backend.Name
        compression = $Compression
        packageElapsedMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
        tiffBytes = [uint64]$tiffBytes
        layerCount = [int]$manifest.grid.layerCount
        storageMode = [string]$manifest.tiff.storageMode
        compressionTag = $compressionTag
        ripStrict = "PASS"
        configPath = $generatedConfig.Replace('\', '/')
        packagePath = $packagePath.Replace('\', '/')
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$handwrittenBuild = Resolve-RepositoryPath $repoRoot $HandwrittenBuildDir
$libTiffBuild = Resolve-RepositoryPath $repoRoot $LibTiffBuildDir
$sourceConfig = Resolve-RepositoryPath $repoRoot $FixtureConfig
$realModelConfig = Resolve-RepositoryPath $repoRoot $RealModelConfig
$outputRoot = Resolve-RepositoryPath $repoRoot $OutputDir
$runId = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$runRoot = Join-Path $outputRoot $runId
$reportPath = Join-Path $runRoot "production_compression_gate.json"

Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    $testRegex =
        "^(output_resolution_config_unit_tests|" +
        "rgbwsv_production_package_writer_unit_tests|" +
        "tiff_layer_source_unit_tests|" +
        "non_square_raster_pipeline_unit_tests|" +
        "global_surface_shell_production_pipeline_unit_tests|" +
        "multi_model_production_service_unit_tests)$"
    $targets = @(
        "output_resolution_config_unit_tests",
        "rgbwsv_production_package_writer_unit_tests",
        "tiff_layer_source_unit_tests",
        "non_square_raster_pipeline_unit_tests",
        "global_surface_shell_production_pipeline_unit_tests",
        "multi_model_production_service_unit_tests",
        "slicer_cli",
        "rip_reader_test"
    )
    if (-not $SkipBuild) {
        foreach ($buildDir in @($handwrittenBuild, $libTiffBuild)) {
            $arguments = @(
                "--build", $buildDir,
                "--config", $Config,
                "--target")
            $arguments += $targets
            $arguments += @("--parallel", "8")
            Invoke-NativeStep `
                -Name "build 03E-02 targets under $buildDir" `
                -Executable "cmake" `
                -Arguments $arguments
        }
    }
    foreach ($buildDir in @($handwrittenBuild, $libTiffBuild)) {
        Invoke-NativeStep `
            -Name "03E-02 shared contract under $buildDir" `
            -Executable "ctest" `
            -Arguments @(
                "--test-dir", $buildDir,
                "-C", $Config,
                "-R", $testRegex,
                "--output-on-failure")
    }

    $backends = @(
        @{
            Name = "handwritten"
            Cli = Resolve-Executable $handwrittenBuild $Config "slicer_cli"
            Rip = Resolve-Executable $handwrittenBuild $Config "rip_reader_test"
        },
        @{
            Name = "libtiff"
            Cli = Resolve-Executable $libTiffBuild $Config "slicer_cli"
            Rip = Resolve-Executable $libTiffBuild $Config "rip_reader_test"
        }
    )
    $results = [System.Collections.Generic.List[object]]::new()
    $cases = @(
        @{
            Name = "deterministic_small"
            Config = $sourceConfig
        },
        @{
            Name = "real_meigui_04"
            Config = $realModelConfig
        }
    )
    foreach ($case in $cases) {
        foreach ($backend in $backends) {
            foreach ($compression in @("none", "packbits")) {
                $results.Add((Invoke-ProductionCase `
                    -Backend $backend `
                    -Compression $compression `
                    -CaseName $case.Name `
                    -SourceConfig $case.Config `
                    -RunRoot $runRoot))
            }
        }
    }

    $badRoot = Join-Path $runRoot "bad_packages"
    $packBitsPackage = ($results | Where-Object {
        $_.caseName -eq "deterministic_small" -and
        $_.backend -eq "handwritten" -and
        $_.compression -eq "packbits"
    } | Select-Object -First 1).packagePath
    $nonePackage = ($results | Where-Object {
        $_.caseName -eq "deterministic_small" -and
        $_.backend -eq "handwritten" -and
        $_.compression -eq "none"
    } | Select-Object -First 1).packagePath
    $strictRip = $backends[0].Rip

    $mismatchPackage = Join-Path $badRoot "manifest_none_actual_packbits"
    Copy-Package $packBitsPackage $mismatchPackage
    $manifest = Read-Manifest $mismatchPackage
    $manifest.tiff.compression = "none"
    Write-Manifest $mismatchPackage $manifest
    Invoke-NativeStep `
        -Name "compression mismatch bad package" `
        -Executable $strictRip `
        -Arguments @(
            "--package", $mismatchPackage,
            "--expect-error",
            "--expect-code", "E_TIFF_COMPRESSION_MISMATCH",
            "--quiet")

    $invalidPackage = Join-Path $badRoot "manifest_deflate"
    Copy-Package $packBitsPackage $invalidPackage
    $manifest = Read-Manifest $invalidPackage
    $manifest.tiff.compression = "deflate"
    Write-Manifest $invalidPackage $manifest
    Invoke-NativeStep `
        -Name "unsupported compression bad package" `
        -Executable $strictRip `
        -Arguments @(
            "--package", $invalidPackage,
            "--expect-error",
            "--expect-code", "E_TIFF_COMPRESSION_INVALID",
            "--quiet")

    $historicalPackage = Join-Path $badRoot "historical_compression_omitted"
    Copy-Package $nonePackage $historicalPackage
    $manifest = Read-Manifest $historicalPackage
    $manifest.tiff.PSObject.Properties.Remove("compression")
    Write-Manifest $historicalPackage $manifest
    Invoke-NativeStep `
        -Name "historical compression omission compatibility" `
        -Executable $strictRip `
        -Arguments @("--package", $historicalPackage, "--quiet")

    $comparisons = [System.Collections.Generic.List[object]]::new()
    foreach ($case in $cases.Name) {
        foreach ($backend in $backends.Name) {
            $none = $results | Where-Object {
                $_.caseName -eq $case -and
                $_.backend -eq $backend -and
                $_.compression -eq "none"
            } | Select-Object -First 1
            $packBits = $results | Where-Object {
                $_.caseName -eq $case -and
                $_.backend -eq $backend -and
                $_.compression -eq "packbits"
            } | Select-Object -First 1
            $comparisons.Add([pscustomobject][ordered]@{
                caseName = $case
                backend = $backend
                sizeReductionPercent = [math]::Round(
                    100.0 * ($none.tiffBytes - $packBits.tiffBytes) /
                        [double]$none.tiffBytes,
                    3)
                fullPackageElapsedChangePercent = [math]::Round(
                    100.0 * ($packBits.packageElapsedMs - $none.packageElapsedMs) /
                        $none.packageElapsedMs,
                    3)
                nonePackageElapsedMs = $none.packageElapsedMs
                packBitsPackageElapsedMs = $packBits.packageElapsedMs
                noneTiffBytes = $none.tiffBytes
                packBitsTiffBytes = $packBits.tiffBytes
            })
        }
    }

    $report = [ordered]@{
        schema = "slicesoft.tiff_production_compression_gate.03e.2"
        stage = "03E-02"
        generatedAt = (Get-Date).ToString("o")
        config = $Config
        fixtures = @(
            [ordered]@{
                name = "deterministic_small"
                path = $sourceConfig.Replace('\', '/')
                class = "golden_fixture"
            },
            [ordered]@{
                name = "real_meigui_04"
                path = $realModelConfig.Replace('\', '/')
                class = "real_obj_mtl_texture"
            }
        )
        protocol = [ordered]@{
            packageSchema = "p0.rgbwsv.2"
            channelOrder = @("R", "G", "B", "W", "S", "V")
            bitDepth = 8
            polarity = "black_is_print"
            compressionCandidates = @("none", "packbits")
            omittedCompressionCompatibility = "none"
        }
        results = @($results)
        comparisons = @($comparisons)
        badPackageMatrix = @(
            [ordered]@{
                name = "manifest_none_actual_packbits"
                expectedCode = "E_TIFF_COMPRESSION_MISMATCH"
                result = "PASS"
            },
            [ordered]@{
                name = "manifest_deflate"
                expectedCode = "E_TIFF_COMPRESSION_INVALID"
                result = "PASS"
            },
            [ordered]@{
                name = "historical_compression_omitted"
                expectedResult = "PASS_AS_NONE"
                result = "PASS"
            }
        )
        externalTargetRip = [ordered]@{
            status = "PENDING"
            reason = "Target RIP/Photoshop interoperability was not executed by this internal gate."
        }
        decision = "NO_GO_DEFAULT_EXTERNAL_INTEROP_PENDING"
        defaultCompression = "none"
        pass = $true
    }
    Write-Utf8NoBom `
        -Path $reportPath `
        -Content ($report | ConvertTo-Json -Depth 100)

    Write-Host "03E-02 production compression gate: PASS"
    Write-Host "Report: $reportPath"
    Write-Host "Decision: $($report.decision)"
}
finally {
    Pop-Location
}
