param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot =
        "output/benchmarks/12e_08d_06_release_matrix",
    [switch]$SkipBuild
)

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

function Assert-Equal
{
    param(
        $Actual,
        $Expected,
        [string]$Message
    )

    if ($Actual -ne $Expected)
    {
        throw "$Message expected=$Expected actual=$Actual"
    }
}

function Resolve-RepositoryPath
{
    param(
        [string]$BasePath,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

function Resolve-Executable
{
    param(
        [string]$BuildPath,
        [string]$BuildConfig,
        [string]$Name
    )

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

function Read-Json
{
    param([string]$Path)

    Assert-True (Test-Path -LiteralPath $Path) "JSON 文件不存在：$Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path |
        ConvertFrom-Json
}

function Write-Utf8NoBom
{
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if ($parent)
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function Assert-PathUnderRoot
{
    param(
        [string]$Root,
        [string]$Path
    )

    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
    $candidatePath = [System.IO.Path]::GetFullPath($Path)
    Assert-True (
        $candidatePath.StartsWith(
            $rootPath,
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "拒绝操作输出根目录之外的路径：$candidatePath"
}

function Parse-Timing
{
    param([string[]]$OutputLines)

    $line = @(
        $OutputLines |
            Where-Object { $_ -like "SLICE_TIMING *" }
    ) | Select-Object -Last 1
    Assert-True (-not [string]::IsNullOrWhiteSpace($line)) `
        "切片输出缺少 SLICE_TIMING"

    $values = [ordered]@{}
    foreach ($field in ($line -split " " | Select-Object -Skip 1))
    {
        $pair = $field -split "=", 2
        if ($pair.Count -eq 2)
        {
            $values[$pair[0]] = $pair[1]
        }
    }
    return $values
}

function Get-ChannelPrintPixels
{
    param(
        [pscustomobject]$SliceReport,
        [string]$Channel
    )

    if ($null -ne $SliceReport.totals.printPixels)
    {
        return [uint64]$SliceReport.totals.printPixels.$Channel
    }
    return [uint64]$SliceReport.totals.channelStats.$Channel.printPixels
}

function Invoke-MeasuredProcess
{
    param(
        [string]$Executable,
        [string]$Arguments
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Assert-True $process.Start() "无法启动：$Executable"
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $peakWorkingSetBytes = [uint64]0
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
            # The process may exit between HasExited and Refresh.
        }
        Start-Sleep -Milliseconds 25
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
        # Keep the sampled peak when the platform cannot query exited process data.
    }

    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $lines = @(
        (($stdout + [Environment]::NewLine + $stderr) -split "\r?\n") |
            Where-Object { $_ -ne "" }
    )
    return [ordered]@{
        exitCode = $process.ExitCode
        wallClockMs = [double]$stopwatch.Elapsed.TotalMilliseconds
        peakWorkingSetBytes = $peakWorkingSetBytes
        outputLines = $lines
    }
}

function New-CaseConfig
{
    param(
        [pscustomobject]$Case,
        [string]$RepositoryRoot,
        [string]$ResolvedOutputRoot
    )

    $sourceConfigPath = Resolve-RepositoryPath `
        $RepositoryRoot $Case.sourceConfig
    $sourceConfigDirectory = Split-Path -Parent $sourceConfigPath
    $configJson = Read-Json $sourceConfigPath
    $modelPath = if ($Case.modelPath)
    {
        Resolve-RepositoryPath $RepositoryRoot $Case.modelPath
    }
    else
    {
        Resolve-RepositoryPath `
            $sourceConfigDirectory $configJson.input.modelPath
    }
    $caseRoot = Join-Path $ResolvedOutputRoot $Case.caseId
    $packagePath = Join-Path $caseRoot "package"
    Assert-PathUnderRoot $ResolvedOutputRoot $packagePath
    if (Test-Path -LiteralPath $packagePath)
    {
        Remove-Item -LiteralPath $packagePath -Recurse -Force
    }

    $configJson.input.modelPath =
        [System.IO.Path]::GetFullPath($modelPath).Replace("\", "/")
    $configJson.output.packageDir =
        [System.IO.Path]::GetFullPath($packagePath).Replace("\", "/")
    $configJson.output.layerThicknessMm = 0.01
    $configJson.preview.interval = 50

    if ($Case.pipelineMode -eq "legacy")
    {
        $configJson | Add-Member -Force -NotePropertyName slicePipeline `
            -NotePropertyValue ([pscustomobject]@{mode = "legacy"})
        $configJson.modelTransform.scale = @(1.0, 1.0, 1.0)
        $configJson.materialProcessProfile.name =
            "release_matrix_$($Case.caseId)"
    }

    $configPath = Join-Path $caseRoot "config.json"
    Write-Utf8NoBom $configPath (
        $configJson | ConvertTo-Json -Depth 100)
    return [ordered]@{
        configPath = $configPath
        packagePath = $packagePath
        modelPath = $modelPath
    }
}

function Assert-ProductionPackage
{
    param(
        [pscustomobject]$Case,
        [string]$PackagePath,
        [string]$RipReader
    )

    $manifest = Read-Json (Join-Path $PackagePath "manifest.json")
    $sliceReport = Read-Json (
        Join-Path $PackagePath "reports/slice_report.json")
    Assert-Equal $manifest.schema "p0.rgbwsv.2" `
        "$($Case.caseId) schema"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$($Case.caseId) channel order"
    Assert-Equal $manifest.tiff.bitDepth 8 "$($Case.caseId) bit depth"
    Assert-Equal $manifest.tiff.polarity "black_is_print" `
        "$($Case.caseId) polarity"
    Assert-Equal $manifest.tiff.printValue 0 `
        "$($Case.caseId) print value"
    Assert-Equal $manifest.tiff.emptyValue 255 `
        "$($Case.caseId) empty value"
    if ($Case.pipelineMode -eq "global_surface_shell")
    {
        Assert-Equal $manifest.requestedPipelineMode $Case.pipelineMode `
            "$($Case.caseId) requested mode"
        Assert-Equal $manifest.effectivePipelineMode $Case.pipelineMode `
            "$($Case.caseId) effective mode"
        Assert-Equal $manifest.productionOutputWritten $true `
            "$($Case.caseId) production output"
        Assert-Equal $manifest.fallbackApplied $false `
            "$($Case.caseId) fallback"
    }
    Assert-Equal @($manifest.layers).Count $manifest.grid.layerCount `
        "$($Case.caseId) complete layer list"

    foreach ($layer in @($manifest.layers))
    {
        $relativeLayerPath = if ($layer.path)
        {
            $layer.path
        }
        else
        {
            $layer.file
        }
        $layerPath = Join-Path $PackagePath $relativeLayerPath
        Assert-True (Test-Path -LiteralPath $layerPath) `
            "$($Case.caseId) 缺少 TIFF：$relativeLayerPath"
    }

    foreach ($channel in $Case.requiredChannels)
    {
        Assert-True (
            (Get-ChannelPrintPixels $sliceReport $channel) -gt 0) `
            "$($Case.caseId) 缺少 $channel 打印像素"
    }
    foreach ($channel in $Case.emptyChannels)
    {
        Assert-Equal (
            Get-ChannelPrintPixels $sliceReport $channel) 0 `
            "$($Case.caseId) $channel 应为空"
    }

    $rip = Invoke-MeasuredProcess `
        $RipReader "--package `"$PackagePath`" --summary"
    Assert-Equal $rip.exitCode 0 `
        "$($Case.caseId) RIP Reader：$($rip.outputLines -join '; ')"

    $printPixels = [ordered]@{}
    foreach ($channel in @("R", "G", "B", "W", "S", "V"))
    {
        $printPixels[$channel] =
            Get-ChannelPrintPixels $sliceReport $channel
    }
    return [ordered]@{
        widthPx = [int]$manifest.grid.widthPx
        heightPx = [int]$manifest.grid.heightPx
        layerCount = [int]$manifest.grid.layerCount
        tiffCount = @($manifest.layers).Count
        printPixels = $printPixels
        ripReader = "pass"
        ripWallClockMs = [double]$rip.wallClockMs
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Resolve-RepositoryPath $repoRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repoRoot $OutputRoot
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        slicer_cli `
        rip_reader_test `
        global_surface_shell_material_evidence_unit_tests `
        global_surface_shell_production_pipeline_unit_tests `
        rgbwsv_production_package_writer_unit_tests `
        slice_pipeline_router_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-08D-06 Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(global_surface_shell_material_evidence_unit_tests|global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests|slice_pipeline_router_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-08D-06 定向单测失败，退出码=$LASTEXITCODE"
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$legacySource =
    "samples/configs/material_process/nail_rgb_white_varnish_top2.json"
$cases = @(
    [pscustomobject][ordered]@{
        caseId = "legacy_xiao_ma"
        modelFamily = "xiao_ma_wu_yu_new"
        sourceConfig = $legacySource
        modelPath =
            "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj"
        pipelineMode = "legacy"
        profile = "legacy_rgb_white_varnish_support"
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    },
    [pscustomobject][ordered]@{
        caseId = "legacy_yecan"
        modelFamily = "yecan"
        sourceConfig = $legacySource
        modelPath = "model/obj/yecan/3.obj"
        pipelineMode = "legacy"
        profile = "legacy_rgb_white_varnish_support"
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    },
    [pscustomobject][ordered]@{
        caseId = "global_restricted_xiao_ma"
        modelFamily = "xiao_ma_wu_yu_new"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_restricted_candidate"
        requiredChannels = @("R", "G", "B", "W")
        emptyChannels = @("S", "V")
    },
    [pscustomobject][ordered]@{
        caseId = "global_restricted_yecan"
        modelFamily = "yecan"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_yecan_white_fill.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_restricted_candidate"
        requiredChannels = @("R", "G", "B", "W")
        emptyChannels = @("S", "V")
    },
    [pscustomobject][ordered]@{
        caseId = "global_material_parity_xiao_ma"
        modelFamily = "xiao_ma_wu_yu_new"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_xiao_ma_material_parity.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_material_parity_candidate"
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    },
    [pscustomobject][ordered]@{
        caseId = "global_material_parity_yecan"
        modelFamily = "yecan"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_yecan_material_parity.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_material_parity_candidate"
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    }
)

$caseResults = @()
foreach ($case in $cases)
{
    Write-Host "运行 12E-08D-06 case：$($case.caseId)"
    $paths = New-CaseConfig $case $repoRoot $resolvedOutputRoot
    $run = Invoke-MeasuredProcess `
        $slicerCli "--config `"$($paths.configPath)`""
    if ($run.exitCode -ne 0)
    {
        throw "$($case.caseId) 切片失败：$($run.outputLines -join [Environment]::NewLine)"
    }
    $timing = Parse-Timing $run.outputLines
    Assert-Equal $timing.engine $case.pipelineMode `
        "$($case.caseId) timing engine"
    $package = Assert-ProductionPackage `
        $case $paths.packagePath $ripReader
    $caseResults += [ordered]@{
        caseId = $case.caseId
        modelFamily = $case.modelFamily
        modelPath = $paths.modelPath
        pipelineMode = $case.pipelineMode
        profile = $case.profile
        layerThicknessMm = 0.01
        timingsMs = [ordered]@{
            configLoad = [double]$timing.configLoadMs
            modelLoad = [double]$timing.modelLoadMs
            sliceProcessing = [double]$timing.sliceProcessingMs
            layerCompute = [double]$timing.layerComputeMs
            tiffWrite = [double]$timing.tiffWriteMs
            previewWrite = [double]$timing.previewWriteMs
            reportBuild = [double]$timing.reportBuildMs
            reportWrite = [double]$timing.reportWriteMs
            packagePublish = [double]$timing.packagePublishMs
            outputWrite = [double]$timing.outputWriteMs
            total = [double]$timing.totalMs
            observedWallClock = [double]$run.wallClockMs
        }
        memory = [ordered]@{
            processPeakWorkingSetBytes =
                [uint64]$run.peakWorkingSetBytes
        }
        package = $package
        pass = $true
    }
}

$globalCases = @(
    $caseResults |
        Where-Object { $_.pipelineMode -eq "global_surface_shell" }
)
$legacyCases = @(
    $caseResults |
        Where-Object { $_.pipelineMode -eq "legacy" }
)
$comparisons = @()
foreach ($modelFamily in @("xiao_ma_wu_yu_new", "yecan"))
{
    $legacyCase = @(
        $caseResults |
            Where-Object {
                $_.modelFamily -eq $modelFamily -and
                $_.pipelineMode -eq "legacy"
            }
    )[0]
    $restrictedCase = @(
        $caseResults |
            Where-Object {
                $_.modelFamily -eq $modelFamily -and
                $_.profile -eq
                    "global_surface_shell_restricted_candidate"
            }
    )[0]
    $parityCase = @(
        $caseResults |
            Where-Object {
                $_.modelFamily -eq $modelFamily -and
                $_.profile -eq
                    "global_surface_shell_material_parity_candidate"
            }
    )[0]
    $comparisons += [ordered]@{
        modelFamily = $modelFamily
        restrictedVsLegacyTotalRatio = [math]::Round(
            [double]$restrictedCase.timingsMs.total /
                [double]$legacyCase.timingsMs.total,
            3)
        parityVsLegacyTotalRatio = [math]::Round(
            [double]$parityCase.timingsMs.total /
                [double]$legacyCase.timingsMs.total,
            3)
        restrictedVsLegacyPeakMemoryRatio = [math]::Round(
            [double]$restrictedCase.memory.processPeakWorkingSetBytes /
                [double]$legacyCase.memory.processPeakWorkingSetBytes,
            3)
        parityVsLegacyPeakMemoryRatio = [math]::Round(
            [double]$parityCase.memory.processPeakWorkingSetBytes /
                [double]$legacyCase.memory.processPeakWorkingSetBytes,
            3)
    }
}
$summary = [ordered]@{
    schema = "slicesoft.global_release_matrix.12e_08d.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08D-06"
    buildType = $Config
    dpi = 600
    layerThicknessMm = 0.01
    cases = $caseResults
    comparisons = $comparisons
    result = [ordered]@{
        legacyProduction = if ($legacyCases.Count -eq 2)
        {
            "go"
        }
        else
        {
            "no_go"
        }
        globalRestrictedProduction = if (
            @($globalCases | Where-Object {
                $_.profile -eq
                    "global_surface_shell_restricted_candidate"
            }).Count -eq 2)
        {
            "go"
        }
        else
        {
            "no_go"
        }
        globalMaterialParityProduction = if (
            @($globalCases | Where-Object {
                $_.profile -eq
                    "global_surface_shell_material_parity_candidate"
            }).Count -eq 2)
        {
            "go"
        }
        else
        {
            "no_go"
        }
        globalDefaultReplacement = "no_go"
        globalDefaultReplacementReason =
            "candidate performance and peak memory are materially worse than legacy"
        fallbackApplied = $false
        pass = ($caseResults.Count -eq 6)
    }
    caveats = @(
        "Observed timings and memory are candidate reference-machine evidence, not a product SLA.",
        "Complex self-intersecting relief model coverage remains outside this matrix."
    )
}
$summaryPath = Join-Path $resolvedOutputRoot "release_matrix_summary.json"
Write-Utf8NoBom $summaryPath (
    $summary | ConvertTo-Json -Depth 100)

Write-Host "12E-08D-06 0.01 mm Release matrix: PASS"
Write-Host "Legacy production: GO"
Write-Host "Global restricted opt-in candidate: GO"
Write-Host "Global material parity opt-in candidate: GO"
Write-Host "Global default replacement: NO-GO (performance and memory)"
Write-Host "Summary: $summaryPath"
$global:LASTEXITCODE = 0
