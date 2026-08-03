[CmdletBinding()]
param(
    [string]$HandwrittenBuildDir = "build-slicesoft/main",
    [string]$LibTiffBuildDir = "build-slicesoft/03d-libtiff",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputDir = "output/benchmarks/03d_06",
    [switch]$SkipBuild
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
        [string]$BuildDir,
        [string]$BuildConfig,
        [string]$Name
    )

    foreach ($candidate in @(
        (Join-Path $BuildDir "$BuildConfig/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe"),
        (Join-Path $BuildDir $Name)))
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
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
    & $Executable @Arguments | ForEach-Object { Write-Host $_ }
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0)
    {
        throw "$Name failed with exit code $exitCode."
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

function Invoke-WriterCase
{
    param(
        [string]$Executable,
        [string]$ExpectedBackend,
        [hashtable]$Case,
        [hashtable]$Condition,
        [string]$RunRoot
    )

    $caseRoot = Join-Path $RunRoot (
        "$ExpectedBackend/$($Condition.Name)/$($Case.Name)")
    $reportPath = Join-Path $caseRoot "writer_report.json"
    $workDir = Join-Path $caseRoot "files"
    Invoke-NativeStep `
        -Name "$ExpectedBackend $($Condition.Name) $($Case.Name)" `
        -Executable $Executable `
        -Arguments @(
            "--output", $reportPath,
            "--work-dir", $workDir,
            "--stage", "03D-06",
            "--case-name", $Case.Name,
            "--cache-condition", $Condition.Name,
            "--storage", $Case.Storage,
            "--width", [string]$Case.Width,
            "--height", [string]$Case.Height,
            "--rows-per-strip", [string]$Case.RowsPerStrip,
            "--tile-width", [string]$Case.TileWidth,
            "--tile-height", [string]$Case.TileHeight,
            "--warmup", [string]$Condition.Warmup,
            "--iterations", [string]$Case.Iterations)

    $report = Get-Content -Raw -Encoding UTF8 -LiteralPath $reportPath |
        ConvertFrom-Json
    Assert-True `
        ($report.schema -eq "slicesoft.tiff_writer_benchmark.03d.1") `
        "$ExpectedBackend $($Case.Name) schema mismatch."
    Assert-True `
        ($report.stage -eq "03D-06") `
        "$ExpectedBackend $($Case.Name) stage mismatch."
    Assert-True `
        ($report.buildType -eq "Release") `
        "$ExpectedBackend $($Case.Name) must use Release."
    Assert-True `
        ($report.scope -eq "writer_only") `
        "$ExpectedBackend $($Case.Name) scope mismatch."
    Assert-True `
        ($report.configuredBackend -eq $ExpectedBackend) `
        "$ExpectedBackend $($Case.Name) configured backend mismatch."
    Assert-True `
        ($report.caseName -eq $Case.Name) `
        "$ExpectedBackend $($Case.Name) case metadata mismatch."
    Assert-True `
        ($report.cacheCondition -eq $Condition.Name) `
        "$ExpectedBackend $($Case.Name) cache condition mismatch."
    Assert-True `
        ($report.input.width -eq $Case.Width -and
         $report.input.height -eq $Case.Height) `
        "$ExpectedBackend $($Case.Name) dimensions mismatch."
    Assert-True `
        ($report.cases.Count -eq 1) `
        "$ExpectedBackend $($Case.Name) must contain one storage case."

    $result = $report.cases[0]
    Assert-True `
        ($result.backend -eq $ExpectedBackend) `
        "$ExpectedBackend $($Case.Name) effective backend mismatch."
    Assert-True `
        ($result.storageMode -eq $Case.Storage) `
        "$ExpectedBackend $($Case.Name) storage mismatch."
    Assert-True `
        ($result.measurementIterations -eq $Case.Iterations) `
        "$ExpectedBackend $($Case.Name) iteration mismatch."
    Assert-True `
        ($result.decodedPixelsExact -eq $true) `
        "$ExpectedBackend $($Case.Name) exact decode failed."
    Assert-True `
        ($result.failureCount -eq 0) `
        "$ExpectedBackend $($Case.Name) contains writer failures."
    Assert-True `
        ($result.p50Ms -gt 0 -and $result.p95Ms -gt 0) `
        "$ExpectedBackend $($Case.Name) wall timing is invalid."
    Assert-True `
        ($result.p50CpuMs -ge 0 -and $result.p95CpuMs -ge 0) `
        "$ExpectedBackend $($Case.Name) CPU timing is invalid."
    Assert-True `
        ($result.bytesWritten -gt 0) `
        "$ExpectedBackend $($Case.Name) output size is invalid."
    Assert-True `
        ($result.memory.available -eq $true -and
         $result.memory.workingSetBytes -gt 0 -and
         $result.memory.peakWorkingSetBytes -gt 0) `
        "$ExpectedBackend $($Case.Name) process memory is unavailable."
    Assert-True `
        ($result.phaseTiming.availability -eq "not_available") `
        "$ExpectedBackend $($Case.Name) phase timing must be explicit."
    if ($ExpectedBackend -eq "libtiff")
    {
        Assert-True `
            (-not [string]::IsNullOrWhiteSpace(
                [string]$report.capabilities.libtiffVersion)) `
            "LibTIFF version metadata is missing."
    }

    return [pscustomobject][ordered]@{
        backend = $ExpectedBackend
        caseName = $Case.Name
        cacheCondition = $Condition.Name
        storageMode = $Case.Storage
        width = $Case.Width
        height = $Case.Height
        iterations = $Case.Iterations
        p50Ms = [double]$result.p50Ms
        p95Ms = [double]$result.p95Ms
        p50CpuMs = [double]$result.p50CpuMs
        p95CpuMs = [double]$result.p95CpuMs
        workingSetBytes = [uint64]$result.memory.workingSetBytes
        peakWorkingSetBytes = [uint64]$result.memory.peakWorkingSetBytes
        writerStagingBytesEstimate =
            [uint64]$result.writerStagingBytesEstimate
        bytesWritten = [uint64]$result.bytesWritten
        failureCount = [int]$result.failureCount
        decodedPixelsExact = [bool]$result.decodedPixelsExact
        compiler = [string]$report.compiler
        libtiffVersion = [string]$report.capabilities.libtiffVersion
        sourceReport = $reportPath.Replace('\', '/')
    }
}

$script:repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$handwrittenBuild = Resolve-RepositoryPath `
    -Root $script:repoRoot `
    -Path $HandwrittenBuildDir
$libTiffBuild = Resolve-RepositoryPath `
    -Root $script:repoRoot `
    -Path $LibTiffBuildDir
$outputRoot = Resolve-RepositoryPath `
    -Root $script:repoRoot `
    -Path $OutputDir
$runId = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$runRoot = Join-Path $outputRoot $runId
$matrixPath = Join-Path $runRoot "tiff_writer_matrix.json"

$cases = @(
    @{
        Name = "small_fixture"
        Width = 256
        Height = 512
        Storage = "stripped"
        DecisionEligible = $false
        Iterations = 10
        RowsPerStrip = 64
        TileWidth = 256
        TileHeight = 256
    },
    @{
        Name = "reality_single"
        Width = 226
        Height = 425
        Storage = "stripped"
        DecisionEligible = $true
        Iterations = 5
        RowsPerStrip = 64
        TileWidth = 256
        TileHeight = 256
    },
    @{
        Name = "multi_model"
        Width = 1400
        Height = 600
        Storage = "stripped"
        DecisionEligible = $true
        Iterations = 5
        RowsPerStrip = 64
        TileWidth = 256
        TileHeight = 256
    },
    @{
        Name = "non_integral_tile"
        Width = 229
        Height = 455
        Storage = "tiled"
        DecisionEligible = $false
        Iterations = 5
        RowsPerStrip = 64
        TileWidth = 256
        TileHeight = 256
    }
)
$conditions = @(
    @{ Name = "warm"; Warmup = 1 },
    @{ Name = "cold_output_directory"; Warmup = 0 }
)

Push-Location $script:repoRoot
try
{
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

    $compatibilityScript = Join-Path `
        $PSScriptRoot `
        "Run03DTiffCompatibilityGate.ps1"
    $compatibilityArguments = @{
        HandwrittenBuildDir = $handwrittenBuild
        LibTiffBuildDir = $libTiffBuild
        Config = $Config
        OutputDir = (Join-Path $runRoot "compatibility")
    }
    if ($SkipBuild)
    {
        $compatibilityArguments.SkipBuild = $true
    }
    & $compatibilityScript @compatibilityArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "03D-05 compatibility gate failed."
    }

    if (-not $SkipBuild)
    {
        foreach ($build in @($handwrittenBuild, $libTiffBuild))
        {
            Invoke-NativeStep `
                -Name "build Release TIFF writer benchmark ($build)" `
                -Executable "cmake" `
                -Arguments @(
                    "--build", $build,
                    "--config", $Config,
                    "--target", "tiff_writer_benchmark",
                    "--parallel", "8")
        }
    }

    $backends = @(
        @{
            Name = "handwritten"
            Executable = Resolve-Executable `
                -BuildDir $handwrittenBuild `
                -BuildConfig $Config `
                -Name "tiff_writer_benchmark"
        },
        @{
            Name = "libtiff"
            Executable = Resolve-Executable `
                -BuildDir $libTiffBuild `
                -BuildConfig $Config `
                -Name "tiff_writer_benchmark"
        }
    )

    $results = [System.Collections.Generic.List[object]]::new()
    foreach ($backend in $backends)
    {
        foreach ($condition in $conditions)
        {
            foreach ($case in $cases)
            {
                $results.Add((Invoke-WriterCase `
                    -Executable $backend.Executable `
                    -ExpectedBackend $backend.Name `
                    -Case $case `
                    -Condition $condition `
                    -RunRoot $runRoot))
            }
        }
    }

    $comparisons = [System.Collections.Generic.List[object]]::new()
    foreach ($condition in $conditions)
    {
        foreach ($case in $cases)
        {
            $handwritten = $results | Where-Object {
                $_.backend -eq "handwritten" -and
                $_.caseName -eq $case.Name -and
                $_.cacheCondition -eq $condition.Name
            } | Select-Object -First 1
            $libtiff = $results | Where-Object {
                $_.backend -eq "libtiff" -and
                $_.caseName -eq $case.Name -and
                $_.cacheCondition -eq $condition.Name
            } | Select-Object -First 1
            Assert-True `
                ($null -ne $handwritten -and $null -ne $libtiff) `
                "Comparison input is incomplete for $($case.Name)."
            $comparisons.Add([pscustomobject][ordered]@{
                caseName = $case.Name
                cacheCondition = $condition.Name
                storageMode = $case.Storage
                decisionEligible = [bool]$case.DecisionEligible
                handwrittenP50Ms = $handwritten.p50Ms
                libtiffP50Ms = $libtiff.p50Ms
                p50ImprovementPercent = [math]::Round(
                    100.0 * ($handwritten.p50Ms - $libtiff.p50Ms) /
                        $handwritten.p50Ms,
                    3)
                handwrittenP95Ms = $handwritten.p95Ms
                libtiffP95Ms = $libtiff.p95Ms
                p95ImprovementPercent = [math]::Round(
                    100.0 * ($handwritten.p95Ms - $libtiff.p95Ms) /
                        $handwritten.p95Ms,
                    3)
                peakMemoryRatio = [math]::Round(
                    $libtiff.peakWorkingSetBytes /
                        [double]$handwritten.peakWorkingSetBytes,
                    4)
            })
        }
    }

    $warmStrippedProduction = @($comparisons | Where-Object {
        $_.cacheCondition -eq "warm" -and
        $_.storageMode -eq "stripped" -and
        $_.decisionEligible
    })
    $minimumP50Improvement = ($warmStrippedProduction |
        Measure-Object -Property p50ImprovementPercent -Minimum).Minimum
    $maximumPeakMemoryRatio = ($warmStrippedProduction |
        Measure-Object -Property peakMemoryRatio -Maximum).Maximum
    $decision = "GO_OPTIONAL"
    $decisionReason =
        "Compatibility passed, but the default-switch performance gate was not met."
    if ($minimumP50Improvement -ge 15.0 -and
        $maximumPeakMemoryRatio -le 1.10)
    {
        $decision = "GO_DEFAULT_CANDIDATE"
        $decisionReason =
            "Warm stripped p50 and peak-memory gates passed; explicit 03D-07 authorization is still required."
    }

    $matrix = [ordered]@{
        schema = "slicesoft.tiff_writer_matrix.03d.1"
        stage = "03D-06"
        generatedAt = [DateTime]::UtcNow.ToString("o")
        scope = "writer_only"
        contract = [ordered]@{
            channelOrder = @("R", "G", "B", "W", "S", "V")
            bitsPerSample = 8
            planarConfig = "contiguous"
            compression = "none"
            polarity = "black_is_print"
            printValue = 0
            emptyValue = 255
        }
        environment = [ordered]@{
            machineName = [Environment]::MachineName
            operatingSystem = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
            processor = [Environment]::GetEnvironmentVariable(
                "PROCESSOR_IDENTIFIER")
            configuration = $Config
            repository = $script:repoRoot.Replace('\', '/')
            handwrittenBuild = $handwrittenBuild.Replace('\', '/')
            libtiffBuild = $libTiffBuild.Replace('\', '/')
            compiler = ($results | Select-Object -First 1).compiler
            libtiffVersion = ($results | Where-Object {
                $_.backend -eq "libtiff"
            } | Select-Object -First 1).libtiffVersion
        }
        cacheConditions = @(
            [ordered]@{
                name = "warm"
                description =
                    "Fresh output directory with one untimed writer warmup."
            },
            [ordered]@{
                name = "cold_output_directory"
                description =
                    "Fresh output directory without writer warmup; OS disk cache is not forcibly flushed."
            }
        )
        compatibilityGate = [ordered]@{
            passed = $true
            source = "scripts/Run03DTiffCompatibilityGate.ps1"
        }
        packageTiffTotalTiming = [ordered]@{
            availability = "not_measured"
            reason = "03D-06 primary decision uses the isolated Writer-only lane."
        }
        failureCount = [int](($results |
            Measure-Object -Property failureCount -Sum).Sum)
        results = @($results)
        comparisons = @($comparisons)
        gate = [ordered]@{
            decisionCaseNames = @("reality_single", "multi_model")
            minimumWarmStrippedP50ImprovementPercent =
                [double]$minimumP50Improvement
            requiredP50ImprovementPercent = 15.0
            maximumWarmStrippedPeakMemoryRatio =
                [double]$maximumPeakMemoryRatio
            allowedPeakMemoryRatio = 1.10
        }
        decision = $decision
        decisionReason = $decisionReason
        defaultBackendChanged = $false
    }
    Write-Utf8NoBom `
        -Path $matrixPath `
        -Content ($matrix | ConvertTo-Json -Depth 100)

    Write-Host "03D-06 LibTIFF Writer matrix PASS."
    Write-Host "  decision: $decision"
    Write-Host (
        "  minimum warm stripped p50 improvement: {0:N3}%" -f
        $minimumP50Improvement)
    Write-Host (
        "  maximum warm stripped peak-memory ratio: {0:N4}" -f
        $maximumPeakMemoryRatio)
    Write-Host "  report: $matrixPath"
}
catch
{
    $failure = [ordered]@{
        schema = "slicesoft.tiff_writer_matrix.03d.1"
        stage = "03D-06"
        generatedAt = [DateTime]::UtcNow.ToString("o")
        scope = "writer_only"
        failureCount = 1
        decision = "NO_GO"
        decisionReason = $_.Exception.Message
        defaultBackendChanged = $false
    }
    Write-Utf8NoBom `
        -Path $matrixPath `
        -Content ($failure | ConvertTo-Json -Depth 20)
    Write-Host "03D-06 failure report: $matrixPath"
    throw
}
finally
{
    Pop-Location
}
