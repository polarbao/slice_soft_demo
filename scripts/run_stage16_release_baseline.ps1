[CmdletBinding()]
param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/stage16/16c_02",
    [ValidateRange(3, 20)]
    [int]$Iterations = 5,
    [ValidateRange(0, 5)]
    [int]$WarmupIterations = 1,
    [ValidateSet("full", "quick")]
    [string]$Mode = "full",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
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
    param([string]$Root, [string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
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
    throw "Executable $Name was not found under $BuildPath."
}

function Write-Json
{
    param([string]$Path, $Value)

    $parent = Split-Path -Parent $Path
    if ($parent)
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 100),
        [System.Text.UTF8Encoding]::new($false))
}

function Invoke-MeasuredProcess
{
    param([string]$Executable, [string[]]$Arguments)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]')
        {
            '"' + ($_ -replace '"', '\"') + '"'
        }
        else
        {
            $_
        }
    }) -join ' '
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Assert-True $process.Start() "Failed to start $Executable."
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    [uint64]$peakWorkingSetBytes = 0
    while (-not $process.HasExited)
    {
        try
        {
            $process.Refresh()
            $peakWorkingSetBytes = [math]::Max(
                $peakWorkingSetBytes,
                [uint64]$process.WorkingSet64)
        }
        catch
        {
            # Process exit may race the sampling loop.
        }
        Start-Sleep -Milliseconds 10
    }
    $process.WaitForExit()
    $stopwatch.Stop()
    try
    {
        $peakWorkingSetBytes = [math]::Max(
            $peakWorkingSetBytes,
            [uint64]$process.PeakWorkingSet64)
    }
    catch
    {
    }
    return [ordered]@{
        exitCode = $process.ExitCode
        wallClockMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
        peakWorkingSetBytes = $peakWorkingSetBytes
        stdout = $stdoutTask.Result
        stderr = $stderrTask.Result
    }
}

function Get-Percentile
{
    param([double[]]$Values, [double]$Percentile)

    $sorted = @($Values | Sort-Object)
    Assert-True ($sorted.Count -gt 0) "Cannot calculate an empty percentile."
    $rank = ($Percentile / 100.0) * ($sorted.Count - 1)
    $lower = [math]::Floor($rank)
    $upper = [math]::Ceiling($rank)
    if ($lower -eq $upper)
    {
        return [double]$sorted[$lower]
    }
    $weight = $rank - $lower
    return ([double]$sorted[$lower] * (1.0 - $weight)) +
        ([double]$sorted[$upper] * $weight)
}

function Get-DirectoryHash
{
    param([string]$Path)

    $rootUri = [System.Uri]::new(
        ([System.IO.Path]::GetFullPath($Path).TrimEnd('\') + '\'))
    $lines = foreach ($file in Get-ChildItem -LiteralPath $Path -File -Recurse |
        Sort-Object FullName)
    {
        $relative = [System.Uri]::UnescapeDataString(
            $rootUri.MakeRelativeUri(
                [System.Uri]::new($file.FullName)).ToString())
        if ($relative -eq "reports/multimodel_scene_report.json")
        {
            continue
        }
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).
            Hash.ToLowerInvariant()
        "$relative`t$hash"
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        return (($sha.ComputeHash($bytes) | ForEach-Object {
            $_.ToString("x2")
        }) -join '')
    }
    finally
    {
        $sha.Dispose()
    }
}

function Get-BuildIdentity
{
    param([string]$Root, [string]$BuildPath, [string]$Executable)

    $cachePath = Join-Path $BuildPath "CMakeCache.txt"
    $cache = if (Test-Path -LiteralPath $cachePath)
    {
        Get-Content -LiteralPath $cachePath
    }
    else
    {
        @()
    }
    $compiler = @($cache | Where-Object {
        $_ -match "^CMAKE_CXX_COMPILER(:FILEPATH)?="
    } | Select-Object -First 1)
    $tiffBackend = @($cache | Where-Object {
        $_ -like "SLICESOFT_TIFF_BACKEND:STRING=*"
    } | Select-Object -First 1)
    $commit = (& git -C $Root rev-parse HEAD).Trim()
    $dirty = -not [string]::IsNullOrWhiteSpace(
        (& git -C $Root status --short | Out-String).Trim())
    return [ordered]@{
        gitCommit = $commit
        worktreeDirty = $dirty
        buildConfig = $Config
        executablePath = $Executable.Replace("\", "/")
        executableSha256 = (Get-FileHash -LiteralPath $Executable `
            -Algorithm SHA256).Hash.ToLowerInvariant()
        compiler = if ($compiler.Count -gt 0) {
            ($compiler[0] -split "=", 2)[1]
        } else {
            $generator = @($cache | Where-Object {
                $_ -like "CMAKE_GENERATOR:INTERNAL=*"
            } | Select-Object -First 1)
            if ($generator.Count -gt 0) {
                "MSVC via " + ($generator[0] -split "=", 2)[1]
            } else { "unknown" }
        }
        tiffBackend = if ($tiffBackend.Count -gt 0) {
            ($tiffBackend[0] -split "=", 2)[1]
        } else { "unknown" }
        powershell = $PSVersionTable.PSVersion.ToString()
        os = [System.Environment]::OSVersion.VersionString
    }
}

function Invoke-BaselineCase
{
    param(
        [string]$Temperature,
        [int]$Iteration,
        $Strategy,
        $Case,
        [string]$RunRoot,
        [string]$RepositoryRoot,
        [string]$Executable)

    New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
    $run = Invoke-MeasuredProcess -Executable $Executable -Arguments @(
        "--source-root", $RepositoryRoot,
        "--output", $RunRoot,
        "--case-id", $Case.id,
        "--sampling-strategy", $Strategy.value,
        "--positive-only")
    Assert-True ($run.exitCode -eq 0) (
        "Measurement failed for $($Strategy.id)/$($Case.id): " +
        $run.stderr + $run.stdout)
    $matrixPath = Join-Path $RunRoot "real_model_matrix.json"
    $matrix = Get-Content -LiteralPath $matrixPath -Raw `
        -Encoding UTF8 | ConvertFrom-Json
    Assert-True ([bool]$matrix.functionalMatrixPass) `
        "Functional matrix failed: $matrixPath"
    $item = @($matrix.cases)[0]
    $packagePath = [string]$item.package.path
    return [ordered]@{
        strategyId = $Strategy.id
        strategy = $Strategy.value
        caseId = $Case.id
        instanceCount = [int]$Case.instances
        iteration = $Iteration
        temperature = $Temperature
        wallClockMs = [double]$run.wallClockMs
        peakWorkingSetBytes = [uint64]$run.peakWorkingSetBytes
        coreOnlyMs = [double]$item.timing.sliceMs
        composeMs = [double]$item.timing.composeMs
        writeMs = [double]$item.timing.tiffAndReportWriteMs
        ripStrictMs = [double]$item.timing.ripValidationMs
        endToEndMs = [double]$item.timing.totalMs
        deterministicOutputHash = Get-DirectoryHash -Path $packagePath
        packageBytes = [uint64]$item.package.bytes
        grid = $item.grid
        ripStrictPass = [bool]$item.package.ripStrictPass
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = Resolve-RepositoryPath -Root $repoRoot -Path $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath -Root $repoRoot -Path $OutputRoot
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config `
        --target multi_model_scene_matrix -- /m
    if ($LASTEXITCODE -ne 0)
    {
        throw "16C-02 Release target failed to build."
    }
}
$matrixExe = Resolve-Executable -BuildPath $resolvedBuildDir `
    -BuildConfig $Config -Name "multi_model_scene_matrix"

$strategies = @(
    [ordered]@{
        id = "S0"
        value = "legacy_center_sample"
    },
    [ordered]@{
        id = "S3"
        value = "layer_slab_supersample_2x2_at_least_two_candidate"
    },
    [ordered]@{
        id = "S4"
        value = "layer_slab_supersample_2x2_any_hit_candidate"
    }
)
$cases = @(
    [ordered]@{ id = "13B-M01"; instances = 1 },
    [ordered]@{ id = "13B-M11"; instances = 11 },
    [ordered]@{ id = "13B-M12"; instances = 12 },
    [ordered]@{ id = "13B-M22"; instances = 22 }
)
if ($Mode -eq "quick")
{
    $cases = @($cases[0])
}
elseif ($Iterations -lt 5)
{
    throw "Full 16C-02 evidence requires at least five warm samples."
}

$samples = [System.Collections.Generic.List[object]]::new()
foreach ($strategy in $strategies)
{
    foreach ($case in $cases)
    {
        $coldRoot = Join-Path $resolvedOutputRoot (
            "measure/{0}/{1}/cold-01" -f $strategy.id, $case.id)
        $samples.Add((Invoke-BaselineCase `
            -Temperature "cold" `
            -Iteration 1 `
            -Strategy $strategy `
            -Case $case `
            -RunRoot $coldRoot `
            -RepositoryRoot $repoRoot `
            -Executable $matrixExe))

        for ($warmup = 1; $warmup -le $WarmupIterations; ++$warmup)
        {
            $runRoot = Join-Path $resolvedOutputRoot (
                "warmup/{0}/{1}/{2:D2}" -f $strategy.id, $case.id, $warmup)
            [void](Invoke-BaselineCase `
                -Temperature "warmup" `
                -Iteration $warmup `
                -Strategy $strategy `
                -Case $case `
                -RunRoot $runRoot `
                -RepositoryRoot $repoRoot `
                -Executable $matrixExe)
        }

        for ($iteration = 1; $iteration -le $Iterations; ++$iteration)
        {
            $runRoot = Join-Path $resolvedOutputRoot (
                "measure/{0}/{1}/warm-{2:D2}" -f `
                    $strategy.id, $case.id, $iteration)
            $samples.Add((Invoke-BaselineCase `
                -Temperature "warm" `
                -Iteration $iteration `
                -Strategy $strategy `
                -Case $case `
                -RunRoot $runRoot `
                -RepositoryRoot $repoRoot `
                -Executable $matrixExe))
        }
    }
}

$summaries = [System.Collections.Generic.List[object]]::new()
foreach ($strategy in $strategies)
{
    foreach ($case in $cases)
    {
        $selected = @($samples | Where-Object {
            $_.strategyId -eq $strategy.id -and $_.caseId -eq $case.id
        })
        $warm = @($selected | Where-Object { $_.temperature -eq "warm" })
        $cold = @($selected | Where-Object { $_.temperature -eq "cold" })
        Assert-True ($cold.Count -eq 1) "Cold sample count drifted."
        Assert-True ($warm.Count -ge 2) "At least two warm samples are required."
        $outputHashes = @(
            $selected.deterministicOutputHash | Sort-Object -Unique)
        Assert-True ($outputHashes.Count -eq 1) (
            "Output hash drifted for $($strategy.id)/$($case.id).")
        $summaries.Add([ordered]@{
            strategyId = $strategy.id
            caseId = $case.id
            instanceCount = [int]$case.instances
            cold = [ordered]@{
                coreOnlyMs = [math]::Round($cold[0].coreOnlyMs, 3)
                endToEndMs = [math]::Round($cold[0].endToEndMs, 3)
                wallClockMs = [math]::Round($cold[0].wallClockMs, 3)
                peakWorkingSetBytes = [uint64]$cold[0].peakWorkingSetBytes
            }
            warm = [ordered]@{
                sampleCount = $warm.Count
                coreOnlyP50Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.coreOnlyMs) 50), 3)
                coreOnlyP95Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.coreOnlyMs) 95), 3)
                endToEndP50Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.endToEndMs) 50), 3)
                endToEndP95Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.endToEndMs) 95), 3)
                wallClockP50Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.wallClockMs) 50), 3)
                wallClockP95Ms = [math]::Round((Get-Percentile `
                    ([double[]]$warm.wallClockMs) 95), 3)
                peakWorkingSetBytes = [uint64](
                    ($warm.peakWorkingSetBytes | Measure-Object -Maximum).Maximum)
            }
            deterministicOutputHash = $outputHashes[0]
            packageBytes = [uint64]$selected[0].packageBytes
            ripStrictPass = -not (@($selected | Where-Object {
                -not $_.ripStrictPass
            }).Count -gt 0)
        })
    }
}

$report = [ordered]@{
    schema = "slicesoft.stage16.release_baseline.1"
    stage = "16C-02"
    status = "complete"
    mode = $Mode
    measurementContract = [ordered]@{
        warmupIterations = $WarmupIterations
        measuredIterations = $Iterations
        coldDefinition = "first isolated process after case warmup set"
        warmDefinition = "subsequent isolated processes with warm OS file cache"
        coreOnlyBoundary = "scene instance raster production timing; no TIFF/package write"
        endToEndBoundary = "import, layout, admission, raster, compose, package write, RIP strict"
        percentileMethod = "linear interpolation over sorted samples"
    }
    buildIdentity = Get-BuildIdentity -Root $repoRoot `
        -BuildPath $resolvedBuildDir -Executable $matrixExe
    strategies = $strategies
    cases = $cases
    summaries = $summaries
    samples = $samples
    productionStatus = "INPUT_OPEN"
    productionBlockers = @(
        "device_build_volume_open",
        "device_origin_and_xy_axes_open",
        "22_instance_performance_budget_open")
}
$reportPath = Join-Path $resolvedOutputRoot "release_baseline.json"
Write-Json -Path $reportPath -Value $report
Write-Host "16C-02 Release baseline PASS"
Write-Host "Report: $reportPath"
Write-Host "Production: INPUT_OPEN"
