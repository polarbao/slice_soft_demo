[CmdletBinding()]
param(
    [string]$HandwrittenBuildDir = "build-slicesoft/main",
    [string]$LibTiffBuildDir = "build-slicesoft/03d-libtiff",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputDir = "output/benchmarks/03d_07",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$SkipBuild,
    [switch]$SkipFullRegression
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-True
{
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition)
    {
        throw $Message
    }
}

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
        [string]$Root,
        [string]$Name
    )

    foreach ($candidate in @(
        (Join-Path $Root "$Name.exe"),
        (Join-Path $Root "$Config/$Name.exe")))
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Executable $Name was not found under $Root."
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

function New-RuntimeSmokeConfig
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

function Assert-BackendPresetPolicy
{
    param([string]$PresetPath)

    $presets = Get-Content -Raw -Encoding UTF8 -LiteralPath $PresetPath |
        ConvertFrom-Json
    $defaultPreset = $presets.configurePresets |
        Where-Object { $_.name -eq "slicesoft-base" } |
        Select-Object -First 1
    $libTiffPreset = $presets.configurePresets |
        Where-Object { $_.name -eq "slicesoft-libtiff" } |
        Select-Object -First 1
    Assert-True ($null -ne $defaultPreset) "slicesoft-base preset is missing."
    Assert-True ($null -ne $libTiffPreset) "slicesoft-libtiff preset is missing."
    Assert-True `
        ($defaultPreset.cacheVariables.SLICESOFT_TIFF_BACKEND -eq "handwritten") `
        "The default TIFF backend must remain handwritten."
    Assert-True `
        ($libTiffPreset.cacheVariables.SLICESOFT_TIFF_BACKEND -eq "libtiff") `
        "The optional LibTIFF preset must explicitly select libtiff."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$handwrittenBuild = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $HandwrittenBuildDir
$libTiffBuild = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $LibTiffBuildDir
$outputRoot = Resolve-RepositoryPath -Root $repoRoot -Path $OutputDir
$runId = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$runRoot = Join-Path $outputRoot $runId
$reportPath = Join-Path $runRoot "optional_closure_report.json"

Push-Location $repoRoot
try
{
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    Assert-BackendPresetPolicy -PresetPath (Join-Path $repoRoot "CMakePresets.json")

    $matrixScript = Join-Path $PSScriptRoot "run_03d_libtiff_writer_matrix.ps1"
    $matrixOutput = Join-Path $runRoot "matrix"
    $matrixArguments = @{
        HandwrittenBuildDir = $handwrittenBuild
        LibTiffBuildDir = $libTiffBuild
        Config = $Config
        OutputDir = $matrixOutput
    }
    if ($SkipBuild)
    {
        $matrixArguments.SkipBuild = $true
    }
    & $matrixScript @matrixArguments

    $matrixReportPath = Get-ChildItem `
        -LiteralPath $matrixOutput `
        -Filter "tiff_writer_matrix.json" `
        -File `
        -Recurse |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    Assert-True `
        (-not [string]::IsNullOrWhiteSpace($matrixReportPath)) `
        "03D-06 matrix report was not generated."
    $matrixReport = Get-Content -Raw -Encoding UTF8 `
        -LiteralPath $matrixReportPath | ConvertFrom-Json
    Assert-True `
        ($matrixReport.decision -eq "GO_OPTIONAL") `
        "03D-07 optional closure requires decision=GO_OPTIONAL; got $($matrixReport.decision)."
    Assert-True `
        ($matrixReport.defaultBackendChanged -eq $false) `
        "03D-06 matrix unexpectedly reports a default backend change."

    $runtimeRoot = Join-Path $runRoot "runtime-libtiff"
    $runtimeArguments = @{
        BuildDir = $libTiffBuild
        RuntimeDir = $runtimeRoot
        Config = $Config
        BuildSystem = "VisualStudio"
        TiffBackend = "libtiff"
        VcpkgRoot = $VcpkgRoot
    }
    if ($SkipBuild)
    {
        $runtimeArguments.DeployOnly = $true
    }
    & (Join-Path $PSScriptRoot "PrepareSliceSoftRuntime.ps1") @runtimeArguments

    $runtimeDir = Join-Path $runtimeRoot $Config
    $runtimeManifestPath = Join-Path $runtimeDir "runtime_manifest.json"
    Assert-True `
        (Test-Path -LiteralPath $runtimeManifestPath -PathType Leaf) `
        "LibTIFF runtime manifest is missing."
    $runtimeManifest = Get-Content -Raw -Encoding UTF8 `
        -LiteralPath $runtimeManifestPath | ConvertFrom-Json
    Assert-True `
        ($runtimeManifest.schema -eq "slicesoft.runtime.1") `
        "Unexpected runtime manifest schema."
    Assert-True `
        ($runtimeManifest.tiffWriter.configuredBackend -eq "libtiff") `
        "The isolated runtime does not select LibTIFF."
    Assert-True `
        ($runtimeManifest.tiffWriter.libtiffStrippedWriterImplemented -and
         $runtimeManifest.tiffWriter.libtiffTiledWriterImplemented) `
        "The isolated runtime does not expose stripped+tiled LibTIFF writers."
    Assert-True `
        (@($runtimeManifest.tiffWriter.runtimeLibraries).Count -gt 0) `
        "The isolated runtime does not declare tiff.dll."
    Assert-True `
        (-not [string]::IsNullOrWhiteSpace($runtimeManifest.tiffWriter.license)) `
        "The isolated runtime does not declare the LibTIFF license."
    foreach ($library in @($runtimeManifest.tiffWriter.runtimeLibraries))
    {
        Assert-True `
            (Test-Path -LiteralPath (Join-Path $runtimeDir $library.file) -PathType Leaf) `
            "Runtime library is missing: $($library.file)"
    }
    Assert-True `
        (Test-Path `
            -LiteralPath (Join-Path $runtimeDir $runtimeManifest.tiffWriter.license) `
            -PathType Leaf) `
        "LibTIFF runtime license file is missing."

    $smokeRoot = Join-Path $runRoot "runtime-smoke"
    $smokeConfig = Join-Path $smokeRoot "slice_config.generated.json"
    $smokePackage = Join-Path $smokeRoot "package"
    New-RuntimeSmokeConfig `
        -SourcePath "samples/configs/golden/material_process_top2_fixture.json" `
        -DestinationPath $smokeConfig `
        -PackagePath $smokePackage
    Invoke-NativeStep `
        -Name "LibTIFF isolated runtime package" `
        -Executable (Resolve-Executable -Root $runtimeDir -Name "slicer_cli") `
        -Arguments @("--config", $smokeConfig)
    Invoke-NativeStep `
        -Name "LibTIFF isolated runtime RIP strict" `
        -Executable (Resolve-Executable -Root $runtimeDir -Name "rip_reader_test") `
        -Arguments @("--package", $smokePackage, "--quiet")

    $fullRegression = "skipped_by_request"
    if (-not $SkipFullRegression)
    {
        $regressionArguments = @{
            Mode = "full"
            BuildDir = $handwrittenBuild
            Config = $Config
        }
        if ($SkipBuild)
        {
            $regressionArguments.SkipBuild = $true
        }
        & (Join-Path $PSScriptRoot "run_regression.ps1") @regressionArguments
        $fullRegression = "passed"
    }

    $report = [ordered]@{
        schema = "slicesoft.tiff_optional_closure.03d.1"
        stage = "03D-07"
        generatedAt = [DateTime]::UtcNow.ToString("o")
        result = "GO_OPTIONAL"
        defaultBackend = "handwritten"
        defaultBackendChanged = $false
        optionalBackend = "libtiff"
        matrix = [ordered]@{
            report = $matrixReportPath.Replace('\', '/')
            decision = [string]$matrixReport.decision
            minimumWarmStrippedP50ImprovementPercent =
                [double]$matrixReport.gate.minimumWarmStrippedP50ImprovementPercent
            maximumWarmStrippedPeakMemoryRatio =
                [double]$matrixReport.gate.maximumWarmStrippedPeakMemoryRatio
        }
        runtime = [ordered]@{
            manifest = $runtimeManifestPath.Replace('\', '/')
            backend = [string]$runtimeManifest.tiffWriter.configuredBackend
            libtiffVersion = [string]$runtimeManifest.tiffWriter.libtiffVersion
            runtimeLibraries = @($runtimeManifest.tiffWriter.runtimeLibraries)
            license = [string]$runtimeManifest.tiffWriter.license
            packageRipStrict = "passed"
        }
        regression = [ordered]@{
            lane = "handwritten_default"
            mode = "full"
            result = $fullRegression
        }
        readerPerformance = [ordered]@{
            availability = "not_comparable"
            reason =
                "Both writer outputs are validated by the same project-owned strict Reader; no LibTIFF Reader backend exists."
        }
    }
    Write-Utf8NoBom `
        -Path $reportPath `
        -Content ($report | ConvertTo-Json -Depth 20)

    Write-Host "03D-07 optional LibTIFF closure: PASS"
    Write-Host "  result: GO_OPTIONAL"
    Write-Host "  defaultBackend: handwritten"
    Write-Host "  optionalBackend: libtiff"
    Write-Host "  report: $reportPath"
}
catch
{
    $failure = [ordered]@{
        schema = "slicesoft.tiff_optional_closure.03d.1"
        stage = "03D-07"
        generatedAt = [DateTime]::UtcNow.ToString("o")
        result = "NO_GO"
        defaultBackend = "handwritten"
        defaultBackendChanged = $false
        reason = $_.Exception.Message
    }
    Write-Utf8NoBom `
        -Path $reportPath `
        -Content ($failure | ConvertTo-Json -Depth 10)
    Write-Host "03D-07 optional closure failure report: $reportPath"
    throw
}
finally
{
    Pop-Location
}
