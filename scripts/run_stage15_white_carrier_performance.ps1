param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/stage15",
    [ValidateRange(1, 20)]
    [int]$WarmupRuns = 1,
    [ValidateRange(3, 31)]
    [int]$MeasurementRuns = 7,
    [ValidateRange(0.0, 100.0)]
    [double]$MaxRegressionPercent = 2.0,
    [ValidateRange(600, 2400)]
    [int]$BenchmarkDpi = 2400,
    [ValidateRange(1.0, 8.0)]
    [double]$BenchmarkScaleXY = 4.0,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Assert-True
{
    param([bool]$Condition, [string]$Message)

    if (-not $Condition)
    {
        throw $Message
    }
}

function Resolve-RepositoryPath
{
    param([string]$RepositoryRoot, [string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $Path))
}

function Read-Json
{
    param([string]$Path)

    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Write-Json
{
    param([string]$Path, $Value)

    $directory = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($Path),
        ($Value | ConvertTo-Json -Depth 100),
        [System.Text.UTF8Encoding]::new($false))
}

function Resolve-Executable
{
    param([string]$BuildPath, [string]$BuildConfig, [string]$Name)

    foreach ($candidate in @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath "$Name.exe")))
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "无法在 $BuildPath 下找到 $Name.exe"
}

function Write-BenchmarkConfig
{
    param(
        [string]$RepositoryRoot,
        [string]$TemplatePath,
        [string]$PackagePath,
        [string]$ConfigPath,
        [string]$Policy,
        [int]$Dpi,
        [double]$ScaleXY)

    $resolvedTemplate = Resolve-RepositoryPath $RepositoryRoot $TemplatePath
    $configObject = Read-Json $resolvedTemplate
    $templateDirectory = Split-Path -Parent $resolvedTemplate
    $configObject.input.modelPath = [System.IO.Path]::GetFullPath(
        (Join-Path $templateDirectory ([string]$configObject.input.modelPath)))
    $configObject.output.packageDir = [System.IO.Path]::GetFullPath($PackagePath)
    $configObject.output.dpiX = $Dpi
    $configObject.output.dpiY = $Dpi
    if ($null -eq $configObject.modelTransform)
    {
        $configObject | Add-Member `
            -NotePropertyName modelTransform `
            -NotePropertyValue ([pscustomobject]@{
                unit = "mm"
                scale = @(1.0, 1.0, 1.0)
                rotationDeg = @(0.0, 0.0, 0.0)
                translationMm = @(0.0, 0.0, 0.0)
            })
    }
    $configObject.modelTransform.scale = @($ScaleXY, $ScaleXY, 1.0)
    $configObject.preview.enabled = $false
    $configObject.texture.unprintableWhitePolicy = $Policy
    Write-Json $ConfigPath $configObject
}

function ConvertFrom-TimingLine
{
    param([string]$Line)

    $values = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, '([A-Za-z][A-Za-z0-9]*)=([^\s]+)'))
    {
        $name = $match.Groups[1].Value
        $value = $match.Groups[2].Value
        $number = 0.0
        if ([double]::TryParse(
                $value,
                [System.Globalization.NumberStyles]::Float,
                [System.Globalization.CultureInfo]::InvariantCulture,
                [ref]$number))
        {
            $values[$name] = $number
        }
        else
        {
            $values[$name] = $value
        }
    }
    return [pscustomobject]$values
}

function Invoke-TimingRun
{
    param([string]$Slicer, [string]$ConfigPath)

    $lines = @(& $Slicer --config $ConfigPath 2>&1 | ForEach-Object { $_.ToString() })
    Assert-True ($LASTEXITCODE -eq 0) "性能 fixture 切片失败：$ConfigPath"
    $timingLine = @($lines | Where-Object { $_ -like 'SLICE_TIMING *' }) | Select-Object -Last 1
    Assert-True (-not [string]::IsNullOrWhiteSpace($timingLine)) `
        "性能 fixture 缺少 SLICE_TIMING：$ConfigPath"
    $timing = ConvertFrom-TimingLine $timingLine
    Assert-True ($null -ne $timing.sliceProcessingMs) `
        "SLICE_TIMING 缺少 sliceProcessingMs：$ConfigPath"
    return $timing
}

function Get-Median
{
    param([double[]]$Values)

    $sorted = @($Values | Sort-Object)
    return [double]$sorted[[int][Math]::Floor($sorted.Count / 2)]
}

function New-MeasurementSummary
{
    param($Timings)

    $sliceProcessing = @($Timings | ForEach-Object { [double]$_.sliceProcessingMs })
    return [ordered]@{
        runs = $sliceProcessing
        p50Ms = Get-Median $sliceProcessing
        minMs = ($sliceProcessing | Measure-Object -Minimum).Minimum
        maxMs = ($sliceProcessing | Measure-Object -Maximum).Maximum
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedBuildDir = Resolve-RepositoryPath $repoRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repoRoot $OutputRoot

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target slicer_cli
    Assert-True ($LASTEXITCODE -eq 0) "Stage 15 Release slicer_cli 构建失败"
}

$slicer = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$configRoot = Join-Path $resolvedOutputRoot "performance_configs"
$packageRoot = Join-Path $resolvedOutputRoot "performance_packages"
$fixtureDefinitions = @(
    [ordered]@{
        id = "F-03"
        config = "samples/configs/material_process/stage15_f03_four_value.json"
    },
    [ordered]@{
        id = "F-04"
        config = "samples/configs/material_process/stage15_f04_all_white.json"
    }
)

$fixtureResults = @()
foreach ($fixture in $fixtureDefinitions)
{
    $fixtureId = [string]$fixture.id
    $baselineConfig = Join-Path $configRoot "${fixtureId}_fail_closed.json"
    $candidateConfig = Join-Path $configRoot "${fixtureId}_white_underbase.json"
    Write-BenchmarkConfig `
        $repoRoot `
        ([string]$fixture.config) `
        (Join-Path $packageRoot "${fixtureId}_fail_closed") `
        $baselineConfig `
        "fail_closed" `
        $BenchmarkDpi `
        $BenchmarkScaleXY
    Write-BenchmarkConfig `
        $repoRoot `
        ([string]$fixture.config) `
        (Join-Path $packageRoot "${fixtureId}_white_underbase") `
        $candidateConfig `
        "white_underbase" `
        $BenchmarkDpi `
        $BenchmarkScaleXY

    for ($run = 0; $run -lt $WarmupRuns; ++$run)
    {
        Invoke-TimingRun $slicer $baselineConfig | Out-Null
        Invoke-TimingRun $slicer $candidateConfig | Out-Null
    }

    $baselineTimings = [System.Collections.Generic.List[object]]::new()
    $candidateTimings = [System.Collections.Generic.List[object]]::new()
    for ($run = 0; $run -lt $MeasurementRuns; ++$run)
    {
        if (($run % 2) -eq 0)
        {
            $baselineTimings.Add((Invoke-TimingRun $slicer $baselineConfig))
            $candidateTimings.Add((Invoke-TimingRun $slicer $candidateConfig))
        }
        else
        {
            $candidateTimings.Add((Invoke-TimingRun $slicer $candidateConfig))
            $baselineTimings.Add((Invoke-TimingRun $slicer $baselineConfig))
        }
    }

    $baseline = New-MeasurementSummary $baselineTimings
    $candidate = New-MeasurementSummary $candidateTimings
    $regressionPercent = if ($baseline.p50Ms -le 0.0)
    {
        0.0
    }
    else
    {
        (($candidate.p50Ms - $baseline.p50Ms) / $baseline.p50Ms) * 100.0
    }
    $fixtureResults += [ordered]@{
        id = $fixtureId
        metric = "sliceProcessingMs"
        baselinePolicy = "fail_closed"
        candidatePolicy = "white_underbase"
        baseline = $baseline
        candidate = $candidate
        regressionPercent = $regressionPercent
        pass = $regressionPercent -le $MaxRegressionPercent
    }
}

$failedFixtures = @($fixtureResults | Where-Object { -not $_.pass })
$result = [ordered]@{
    schema = "slicesoft.stage15.white_carrier_performance.1"
    generatedAt = [DateTimeOffset]::Now.ToString("o")
    build = [ordered]@{
        directory = $BuildDir.Replace('\', '/')
        configuration = $Config
        executable = $slicer.Substring($repoRoot.Length + 1).Replace('\', '/')
    }
    methodology = [ordered]@{
        metric = "sliceProcessingMs"
        includes = @("grid", "mask", "texture", "support", "layer_compute")
        excludes = @("tiff", "preview", "report", "package_publish")
        warmupRuns = $WarmupRuns
        measurementRuns = $MeasurementRuns
        order = "alternating_baseline_candidate"
        maxRegressionPercent = $MaxRegressionPercent
        benchmarkDpi = $BenchmarkDpi
        benchmarkScaleXY = $BenchmarkScaleXY
    }
    fixtures = $fixtureResults
    status = if ($failedFixtures.Count -eq 0) { "passed" } else { "failed" }
}

$resultPath = Join-Path $resolvedOutputRoot "performance_p50.json"
Write-Json $resultPath $result
Assert-True ($failedFixtures.Count -eq 0) `
    "Stage 15 性能退化超过 $MaxRegressionPercent%。证据：$resultPath"

Write-Host "Stage 15 white-carrier performance PASS."
foreach ($fixture in $fixtureResults)
{
    Write-Host (
        "{0}: baseline={1:F3} ms candidate={2:F3} ms regression={3:F2}%" -f
        $fixture.id,
        $fixture.baseline.p50Ms,
        $fixture.candidate.p50Ms,
        $fixture.regressionPercent)
}
Write-Host "Summary: $resultPath"
