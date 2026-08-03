[CmdletBinding()]
param(
    [string]$HandwrittenBuildDir = "build-slicesoft/main",
    [string]$LibTiffBuildDir = "build-slicesoft/03d-libtiff",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputDir = "output/benchmarks/03e_01",
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

function Invoke-CompressionCase {
    param(
        [string]$Executable,
        [string]$Backend,
        [string]$Compression,
        [hashtable]$Case,
        [string]$RunRoot
    )

    $caseRoot = Join-Path $RunRoot "$Backend/$Compression/$($Case.Name)"
    $reportPath = Join-Path $caseRoot "benchmark.json"
    $workDir = Join-Path $caseRoot "files"
    Invoke-NativeStep `
        -Name "$Backend $Compression $($Case.Name)" `
        -Executable $Executable `
        -Arguments @(
            "--output", $reportPath,
            "--work-dir", $workDir,
            "--stage", "03E-01",
            "--case-name", $Case.Name,
            "--cache-condition", "warm",
            "--storage", $Case.Storage,
            "--compression", $Compression,
            "--pixel-pattern", "production_sparse",
            "--measure-read",
            "--width", [string]$Case.Width,
            "--height", [string]$Case.Height,
            "--rows-per-strip", "64",
            "--tile-width", "256",
            "--tile-height", "256",
            "--warmup", "1",
            "--iterations", [string]$Case.Iterations)

    $report = Get-Content -Raw -Encoding UTF8 -LiteralPath $reportPath |
        ConvertFrom-Json
    Assert-True `
        ($report.schema -eq "slicesoft.tiff_compression_benchmark.03e.1") `
        "$Backend $Compression $($Case.Name) schema mismatch."
    Assert-True `
        ($report.scope -eq "writer_and_project_reader") `
        "$Backend $Compression $($Case.Name) scope mismatch."
    Assert-True `
        ($report.configuredBackend -eq $Backend) `
        "$Backend $Compression $($Case.Name) backend mismatch."
    Assert-True `
        ($report.contract.compression -eq $Compression) `
        "$Backend $Compression $($Case.Name) compression mismatch."
    Assert-True `
        ($report.cases.Count -eq 1) `
        "$Backend $Compression $($Case.Name) case count mismatch."

    $result = $report.cases[0]
    Assert-True `
        ($result.decodedPixelsExact -eq $true) `
        "$Backend $Compression $($Case.Name) exact decode failed."
    Assert-True `
        ($result.readTiming.available -eq $true) `
        "$Backend $Compression $($Case.Name) read timing missing."
    Assert-True `
        ($result.p50Ms -gt 0 -and $result.readTiming.p50Ms -gt 0) `
        "$Backend $Compression $($Case.Name) timing invalid."

    return [pscustomobject][ordered]@{
        backend = $Backend
        compression = $Compression
        caseName = $Case.Name
        storageMode = $Case.Storage
        width = $Case.Width
        height = $Case.Height
        iterations = $Case.Iterations
        writeP50Ms = [double]$result.p50Ms
        writeP95Ms = [double]$result.p95Ms
        readP50Ms = [double]$result.readTiming.p50Ms
        readP95Ms = [double]$result.readTiming.p95Ms
        bytesWritten = [uint64]$result.bytesWritten
        rawPixelBytes = [uint64]$result.rawPixelBytes
        fileToRawRatio = [double]$result.fileToRawRatio
        decodedPixelsExact = [bool]$result.decodedPixelsExact
        sourceReport = $reportPath.Replace('\', '/')
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$handwrittenBuild = Resolve-RepositoryPath $repoRoot $HandwrittenBuildDir
$libTiffBuild = Resolve-RepositoryPath $repoRoot $LibTiffBuildDir
$outputRoot = Resolve-RepositoryPath $repoRoot $OutputDir
$runId = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$runRoot = Join-Path $outputRoot $runId
$matrixPath = Join-Path $runRoot "tiff_compression_matrix.json"

$cases = @(
    @{
        Name = "reality_single"
        Width = 226
        Height = 425
        Storage = "stripped"
        Iterations = 7
    },
    @{
        Name = "multi_model"
        Width = 1400
        Height = 600
        Storage = "stripped"
        Iterations = 5
    },
    @{
        Name = "non_integral_tile"
        Width = 229
        Height = 455
        Storage = "tiled"
        Iterations = 5
    }
)

Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    if (-not $SkipBuild) {
        Invoke-NativeStep `
            -Name "build handwritten compression targets" `
            -Executable "cmake" `
            -Arguments @(
                "--build", $handwrittenBuild,
                "--config", $Config,
                "--target", "tiff_writer_contract_unit_tests",
                "tiff_writer_benchmark",
                "--parallel", "8")
        Invoke-NativeStep `
            -Name "build LibTIFF compression targets" `
            -Executable "cmake" `
            -Arguments @(
                "--build", $libTiffBuild,
                "--config", $Config,
                "--target", "tiff_writer_equivalence_unit_tests",
                "tiff_writer_benchmark",
                "--parallel", "8")
    }

    Invoke-NativeStep `
        -Name "handwritten PackBits contract" `
        -Executable "ctest" `
        -Arguments @(
            "--test-dir", $handwrittenBuild,
            "-C", $Config,
            "-R", "^tiff_writer_contract_unit_tests$",
            "--output-on-failure")
    Invoke-NativeStep `
        -Name "LibTIFF PackBits equivalence" `
        -Executable "ctest" `
        -Arguments @(
            "--test-dir", $libTiffBuild,
            "-C", $Config,
            "-R", "^tiff_writer_equivalence_unit_tests$",
            "--output-on-failure")

    $backends = @(
        @{
            Name = "handwritten"
            Executable = Resolve-Executable `
                $handwrittenBuild $Config "tiff_writer_benchmark"
        },
        @{
            Name = "libtiff"
            Executable = Resolve-Executable `
                $libTiffBuild $Config "tiff_writer_benchmark"
        }
    )
    $results = [System.Collections.Generic.List[object]]::new()
    foreach ($backend in $backends) {
        foreach ($compression in @("none", "packbits")) {
            foreach ($case in $cases) {
                $results.Add((Invoke-CompressionCase `
                    -Executable $backend.Executable `
                    -Backend $backend.Name `
                    -Compression $compression `
                    -Case $case `
                    -RunRoot $runRoot))
            }
        }
    }

    $compressionComparisons = [System.Collections.Generic.List[object]]::new()
    foreach ($backend in $backends.Name) {
        foreach ($case in $cases) {
            $none = $results | Where-Object {
                $_.backend -eq $backend -and
                $_.compression -eq "none" -and
                $_.caseName -eq $case.Name
            } | Select-Object -First 1
            $packBits = $results | Where-Object {
                $_.backend -eq $backend -and
                $_.compression -eq "packbits" -and
                $_.caseName -eq $case.Name
            } | Select-Object -First 1
            $compressionComparisons.Add([pscustomobject][ordered]@{
                backend = $backend
                caseName = $case.Name
                storageMode = $case.Storage
                sizeReductionPercent = [math]::Round(
                    100.0 * ($none.bytesWritten - $packBits.bytesWritten) /
                        [double]$none.bytesWritten,
                    3)
                writeP50ChangePercent = [math]::Round(
                    100.0 * ($packBits.writeP50Ms - $none.writeP50Ms) /
                        $none.writeP50Ms,
                    3)
                readP50ChangePercent = [math]::Round(
                    100.0 * ($packBits.readP50Ms - $none.readP50Ms) /
                        $none.readP50Ms,
                    3)
                noneBytes = $none.bytesWritten
                packBitsBytes = $packBits.bytesWritten
                noneWriteP50Ms = $none.writeP50Ms
                packBitsWriteP50Ms = $packBits.writeP50Ms
                noneReadP50Ms = $none.readP50Ms
                packBitsReadP50Ms = $packBits.readP50Ms
            })
        }
    }

    $minimumSizeReduction = ($compressionComparisons |
        Measure-Object -Property sizeReductionPercent -Minimum).Minimum
    $maximumWriteIncrease = ($compressionComparisons |
        Measure-Object -Property writeP50ChangePercent -Maximum).Maximum
    $minimumReadImprovement = ($compressionComparisons |
        ForEach-Object { -1.0 * $_.readP50ChangePercent } |
        Measure-Object -Minimum).Minimum
    $decision = if ($minimumSizeReduction -ge 15.0 -and
                    $minimumReadImprovement -ge 15.0) {
        "GO_OPTIONAL_EXPERIMENTAL"
    } else {
        "NO_GO_DEFAULT"
    }
    $matrix = [ordered]@{
        schema = "slicesoft.tiff_compression_matrix.03e.1"
        stage = "03E-01"
        generatedAt = [DateTime]::UtcNow.ToString("o")
        scope = "writer_and_project_reader"
        environment = [ordered]@{
            machineName = [Environment]::MachineName
            operatingSystem = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
            processor = [Environment]::GetEnvironmentVariable("PROCESSOR_IDENTIFIER")
            configuration = $Config
            handwrittenBuild = $handwrittenBuild.Replace('\', '/')
            libtiffBuild = $libTiffBuild.Replace('\', '/')
        }
        contract = [ordered]@{
            channelOrder = @("R", "G", "B", "W", "S", "V")
            bitDepth = 8
            polarity = "black_is_print"
            storageModes = @("stripped", "tiled")
            compressionCandidates = @("none", "packbits")
            pixelPattern = "production_sparse"
        }
        measurementNotes = @(
            "Pixel buffers are generated before timing.",
            "Read timing uses the same project strict Reader for both Writer outputs.",
            "One untimed warmup is used; the OS file cache is not forcibly flushed."
        )
        results = @($results)
        compressionComparisons = @($compressionComparisons)
        gate = [ordered]@{
            minimumSizeReductionPercent = [double]$minimumSizeReduction
            requiredMinimumSizeReductionPercent = 15.0
            maximumWriteP50IncreasePercent = [double]$maximumWriteIncrease
            minimumReadP50ImprovementPercent = [double]$minimumReadImprovement
            requiredMinimumReadP50ImprovementPercent = 15.0
            defaultBackendChanged = $false
            productionCompressionChanged = $false
        }
        decision = $decision
    }
    Write-Utf8NoBom `
        -Path $matrixPath `
        -Content ($matrix | ConvertTo-Json -Depth 100)

    Write-Host "03E TIFF compression matrix PASS."
    Write-Host "  decision: $decision"
    Write-Host "  minimum size reduction: $minimumSizeReduction%"
    Write-Host "  maximum write p50 increase: $maximumWriteIncrease%"
    Write-Host "  minimum read p50 improvement: $minimumReadImprovement%"
    Write-Host "  report: $matrixPath"
} finally {
    Pop-Location
}
