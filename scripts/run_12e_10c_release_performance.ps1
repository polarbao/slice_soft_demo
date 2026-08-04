param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/12e_10c",
    [ValidateRange(3, 20)]
    [int]$Iterations = 3,
    [ValidateRange(0, 5)]
    [int]$WarmupIterations = 1,
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

function Assert-Equal
{
    param($Actual, $Expected, [string]$Message)

    if ($Actual -ne $Expected)
    {
        throw "$Message expected=$Expected actual=$Actual"
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

function Resolve-Executable
{
    param([string]$BuildPath, [string]$BuildConfig, [string]$Name)

    foreach ($candidate in @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath "apps/slicer_debug_ui/$BuildConfig/$Name.exe"),
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

    Assert-True (Test-Path -LiteralPath $Path) "JSON 不存在：$Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path |
        ConvertFrom-Json
}

function Write-Json
{
    param([string]$Path, $Value)

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory))
    {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($Path),
        ($Value | ConvertTo-Json -Depth 100),
        [System.Text.UTF8Encoding]::new($false))
}

function Write-Text
{
    param([string]$Path, [string[]]$Lines)

    [System.IO.File]::WriteAllLines(
        [System.IO.Path]::GetFullPath($Path),
        $Lines,
        [System.Text.UTF8Encoding]::new($false))
}

function Set-Property
{
    param($Object, [string]$Name, $Value)

    $Object | Add-Member -Force -NotePropertyName $Name -NotePropertyValue $Value
}

function Parse-Timing
{
    param([string[]]$OutputLines)

    $line = @(
        $OutputLines | Where-Object { $_ -like "SLICE_TIMING *" }
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

function Get-TimingValue
{
    param($Timing, [string]$Name)

    if ($Timing.Contains($Name))
    {
        return [double]$Timing[$Name]
    }
    return [double]0.0
}

function Invoke-MeasuredProcess
{
    param([string]$Executable, [string]$Arguments)

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
        # Keep the sampled peak when the platform cannot query it.
    }

    $lines = @(
        (($stdoutTask.Result + [Environment]::NewLine +
            $stderrTask.Result) -split "\r?\n") |
            Where-Object { $_ -ne "" })
    return [ordered]@{
        exitCode = $process.ExitCode
        wallClockMs = [double]$stopwatch.Elapsed.TotalMilliseconds
        peakWorkingSetBytes = $peakWorkingSetBytes
        outputLines = $lines
    }
}

function Get-Median
{
    param([double[]]$Values)

    Assert-True ($Values.Count -gt 0) "中位数输入不能为空"
    $sorted = @($Values | Sort-Object)
    $middle = [int][math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1)
    {
        return [double]$sorted[$middle]
    }
    return ([double]$sorted[$middle - 1] +
        [double]$sorted[$middle]) / 2.0
}

function Get-Ratio
{
    param([double]$Numerator, [double]$Denominator)

    if ($Denominator -le 0.0)
    {
        return $null
    }
    return [math]::Round($Numerator / $Denominator, 3)
}

function Get-DirectoryBytes
{
    param([string]$Path)

    [uint64]$total = 0
    foreach ($file in Get-ChildItem -LiteralPath $Path -File -Recurse)
    {
        $total += [uint64]$file.Length
    }
    return $total
}

function Set-CommonConfig
{
    param(
        $ConfigJson,
        [string]$PipelineMode,
        [string]$ModelPath,
        [string]$PackagePath,
        [string]$ProfileName)

    Set-Property $ConfigJson "slicePipeline" ([pscustomobject]@{
        mode = $PipelineMode
    })
    $ConfigJson.input.modelPath = $ModelPath.Replace('\', '/')
    Set-Property $ConfigJson.output "packageDir" $PackagePath.Replace('\', '/')
    Set-Property $ConfigJson.output "dpiX" 600
    Set-Property $ConfigJson.output "dpiY" 600
    Set-Property $ConfigJson.output "layerThicknessMm" 0.2
    Set-Property $ConfigJson.output "channelOrder" @("R", "G", "B", "W", "S", "V")
    Set-Property $ConfigJson.output "bitDepth" 8
    Set-Property $ConfigJson.output "planarConfig" "contiguous"
    Set-Property $ConfigJson.output "storageMode" "stripped"
    Set-Property $ConfigJson.output "rowsPerStrip" 64
    Set-Property $ConfigJson.output "tiffCompression" ([pscustomobject]@{
        algorithm = "none"
    })
    Set-Property $ConfigJson "autoOrient" ([pscustomobject]@{
        enabled = $true
        maxHeightMm = 9.0
        strategy = "minimize_height_by_right_angle_rotation"
    })
    Set-Property $ConfigJson "preview" ([pscustomobject]@{
        enabled = $false
        format = "png"
        interval = 10
        channels = @("texture_rgb", "white", "varnish", "support")
        onlyNonEmptyLayers = $false
    })
    Set-Property $ConfigJson "support" ([pscustomobject]@{
        enabled = $false
        mode = "none"
        value = 0
    })
    Set-Property $ConfigJson "surfaceVarnish" ([pscustomobject]@{
        enabled = $false
    })
    Set-Property $ConfigJson "outerVarnish" ([pscustomobject]@{
        enabled = $false
        thicknessMm = 0.0
    })
    Set-Property $ConfigJson "materialClosure" ([pscustomobject]@{
        enabled = $true
        mode = "diagnostic"
        connectivity = 8
        maxGapPx = 1
        failOnGap = $true
        writeGapPreview = $false
        repair = [pscustomobject]@{
            enabled = $false
            colorFillGap = "model_fill"
            modelSupportGap = "contextual"
            internalVoidGap = "support"
            varnishSupportGap = "support"
        }
    })
    $ConfigJson.materialProcessProfile.name = $ProfileName
    $ConfigJson.materialProcessProfile.rgb.enabled = $true
    $ConfigJson.materialProcessProfile.rgb.source = "texture_or_color"
    $ConfigJson.materialProcessProfile.white.enabled = $true
    $ConfigJson.materialProcessProfile.white.mode = "underbase"
    $ConfigJson.materialProcessProfile.white.coverage = "all_model"
    $ConfigJson.materialProcessProfile.white.value = 0
    $ConfigJson.materialProcessProfile.varnish.enabled = $false
    $ConfigJson.materialProcessProfile.support.expected = $false
    $ConfigJson.materialProcessProfile.support.mode =
        "existing_support_pipeline"
    return $ConfigJson
}

function New-CaseConfig
{
    param(
        $Pair,
        [string]$PipelineMode,
        [string]$RepositoryRoot,
        [string]$CaseRoot)

    $sourcePath = Resolve-RepositoryPath $RepositoryRoot $Pair.sourceConfig
    $configJson = Read-Json $sourcePath
    $modelPath = Resolve-RepositoryPath $RepositoryRoot $Pair.modelPath
    $packagePath = Join-Path $CaseRoot "package"
    $profileName = "12e_10c_$($PipelineMode)_$($Pair.pairId)"
    $configJson = Set-CommonConfig $configJson $PipelineMode `
        $modelPath $packagePath $profileName
    Set-Property $configJson.texture "nonSurfaceRgbPolicy" "empty"

    if ($PipelineMode -eq "legacy")
    {
        Set-Property $configJson "slicingMode" "relief_heightfield"
        $configJson.materialProcessProfile.target = "legacy_performance_reference"
        if ($Pair.widthPoint -eq "all_texture")
        {
            $configJson.texture.applyMode = "solid_volume_from_top_surface"
            Set-Property $configJson "modelFill" ([pscustomobject]@{
                enabled = $false
                material = "white"
                scope = "below_texture_surface"
                value = 0
                emptyAllowedInProduction = $false
                legacyRgbFallback = $false
            })
        }
        else
        {
            $configJson.texture.applyMode = "top_surface_band"
            Set-Property $configJson.texture "topSurfaceLayers" `
                ([int][math]::Round(
                    [double]$Pair.requestedWidthMm / 0.2))
            Set-Property $configJson "modelFill" ([pscustomobject]@{
                enabled = $true
                material = "white"
                scope = "below_texture_surface"
                value = 0
                emptyAllowedInProduction = $false
                legacyRgbFallback = $false
            })
        }
    }
    else
    {
        $configJson.materialProcessProfile.target =
            "global_surface_shell_restricted_candidate"
        $configJson.texture.applyMode = "global_surface_shell"
        $partitionMode = if ($Pair.widthPoint -eq "all_texture")
        {
            "all_texture"
        }
        else
        {
            "partial_shell"
        }
        Set-Property $configJson.texture "surfaceShell" ([pscustomobject]@{
            geometryMode = "global_3d_distance"
            mode = $partitionMode
            widthMm = if ($null -eq $Pair.requestedWidthMm)
            {
                0.4
            }
            else
            {
                [double]$Pair.requestedWidthMm
            }
            widthStepMm = 0.01
            minimumWidthPolicy = "two_cells_floor_0_10_mm"
            surfaceScope = "all_closed_surfaces"
            fullTextureAtModelLimit = $true
        })
        Set-Property $configJson "modelFill" ([pscustomobject]@{
            enabled = $true
            material = "white"
            scope = "complement_of_global_texture_shell"
            value = 0
            emptyAllowedInProduction = $false
            legacyRgbFallback = $false
        })
    }

    New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null
    $configPath = Join-Path $CaseRoot "effective_config.json"
    Write-Json $configPath $configJson
    return [ordered]@{
        configPath = $configPath
        packagePath = $packagePath
        modelPath = $modelPath
    }
}

function Assert-ProductionPackage
{
    param(
        [string]$PackagePath,
        [string]$PipelineMode,
        [string]$RipReader)

    $manifest = Read-Json (Join-Path $PackagePath "manifest.json")
    Assert-Equal $manifest.schema "p0.rgbwsv.2" "package schema"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "channel order"
    Assert-Equal $manifest.tiff.bitDepth 8 "bit depth"
    Assert-Equal $manifest.tiff.polarity "black_is_print" "polarity"
    Assert-Equal $manifest.tiff.storageMode "stripped" "storage mode"
    Assert-Equal $manifest.tiff.compression "none" "compression"
    Assert-Equal $manifest.requestedPipelineMode $PipelineMode `
        "requested pipeline"
    Assert-Equal $manifest.effectivePipelineMode $PipelineMode `
        "effective pipeline"
    Assert-Equal $manifest.fallbackApplied $false "silent fallback"
    Assert-Equal $manifest.productionOutputWritten $true "production output"
    Assert-True (@($manifest.layers).Count -gt 0) "package 缺少 layer list"

    $rip = Invoke-MeasuredProcess $RipReader `
        "--package `"$PackagePath`" --summary"
    Assert-Equal $rip.exitCode 0 `
        "RIP strict 失败：$($rip.outputLines -join '; ')"
    return $manifest
}

function Invoke-PerformanceCase
{
    param(
        $Pair,
        [string]$PipelineMode,
        [int]$Iteration,
        [bool]$Measured,
        [string]$RepositoryRoot,
        [string]$RunRoot,
        [string]$SlicerCli,
        [string]$RipReader)

    $phase = if ($Measured) { "measure" } else { "warmup" }
    $caseId = "$PipelineMode`_$($Pair.pairId)"
    $caseRoot = Join-Path $RunRoot (
        "$phase-$('{0:D2}' -f $Iteration)/$caseId")
    Write-Host "== 12E-10C $phase $Iteration : $caseId"
    $paths = New-CaseConfig $Pair $PipelineMode `
        $RepositoryRoot $caseRoot
    $run = Invoke-MeasuredProcess $SlicerCli `
        "--config `"$($paths.configPath)`""
    Write-Text (Join-Path $caseRoot "slicer.log") $run.outputLines
    Assert-Equal $run.exitCode 0 `
        "$caseId 切片失败：$($run.outputLines -join [Environment]::NewLine)"

    $timing = Parse-Timing $run.outputLines
    Assert-Equal $timing.engine $PipelineMode "$caseId timing engine"
    $manifest = Assert-ProductionPackage $paths.packagePath `
        $PipelineMode $RipReader
    $sliceReport = Read-Json (
        Join-Path $paths.packagePath "reports/slice_report.json")
    $supportPixels = if ($null -ne $sliceReport.totals.printPixels)
    {
        [uint64]$sliceReport.totals.printPixels.S
    }
    else
    {
        [uint64]$sliceReport.totals.channelStats.S.printPixels
    }
    Assert-Equal $supportPixels 0 "$caseId support policy"

    $effectiveWidthMm = if ($PipelineMode -eq "global_surface_shell")
    {
        [double]$sliceReport.productionSettings.effectiveWidthMm
    }
    else
    {
        $Pair.requestedWidthMm
    }
    return [ordered]@{
        caseId = $caseId
        pairId = $Pair.pairId
        iteration = $Iteration
        measured = $Measured
        modelFamily = $Pair.modelFamily
        modelPath = $Pair.modelPath
        modelSha256 = (Get-FileHash -Algorithm SHA256 `
            -LiteralPath $paths.modelPath).Hash.ToLowerInvariant()
        pipelineMode = $PipelineMode
        widthPoint = $Pair.widthPoint
        requestedWidthMm = $Pair.requestedWidthMm
        effectiveWidthMm = $effectiveWidthMm
        dpiX = [int]$manifest.grid.dpiX
        dpiY = [int]$manifest.grid.dpiY
        layerThicknessMm = [double]$manifest.grid.layerThicknessMm
        storageMode = $manifest.tiff.storageMode
        compression = $manifest.tiff.compression
        previewEnabled = $false
        packagePath = $paths.packagePath.Replace('\', '/')
        layerCount = [int]$manifest.grid.layerCount
        packageBytes = Get-DirectoryBytes $paths.packagePath
        ripStrictPass = $true
        fallbackApplied = $false
        timingsMs = [ordered]@{
            core = Get-TimingValue $timing "layerComputeMs"
            compose = Get-TimingValue $timing "layerComposeMs"
            tiffSave = Get-TimingValue $timing "tiffWriteMs"
            previewReportSave =
                (Get-TimingValue $timing "previewWriteMs") +
                (Get-TimingValue $timing "reportBuildMs") +
                (Get-TimingValue $timing "reportWriteMs")
            total = Get-TimingValue $timing "totalMs"
            wallClock = [double]$run.wallClockMs
        }
        memory = [ordered]@{
            peakWorkingSetBytes = [uint64]$run.peakWorkingSetBytes
        }
        result = "PASS"
    }
}

function New-PerformanceSummary
{
    param([object[]]$Samples, [string]$PairId, [string]$PipelineMode)

    $selected = @($Samples | Where-Object {
        $_.measured -and $_.pairId -eq $PairId -and
        $_.pipelineMode -eq $PipelineMode
    })
    Assert-Equal $selected.Count $Iterations `
        "$PairId/$PipelineMode sample count"
    $layerCount = @($selected.layerCount | Sort-Object -Unique)
    Assert-Equal $layerCount.Count 1 "$PairId/$PipelineMode layer count stability"
    $core = [double[]]@($selected | ForEach-Object { $_.timingsMs.core })
    $compose = [double[]]@($selected | ForEach-Object { $_.timingsMs.compose })
    $tiff = [double[]]@($selected | ForEach-Object { $_.timingsMs.tiffSave })
    $previewReport = [double[]]@($selected | ForEach-Object {
        $_.timingsMs.previewReportSave
    })
    $total = [double[]]@($selected | ForEach-Object { $_.timingsMs.total })
    $wall = [double[]]@($selected | ForEach-Object { $_.timingsMs.wallClock })
    $memory = [double[]]@($selected | ForEach-Object {
        $_.memory.peakWorkingSetBytes
    })
    $medianCore = Get-Median $core
    return [ordered]@{
        pairId = $PairId
        pipelineMode = $PipelineMode
        sampleCount = $selected.Count
        layerCount = [int]$layerCount[0]
        medianCoreMs = [math]::Round($medianCore, 3)
        medianCorePerLayerMs = [math]::Round(
            $medianCore / [double]$layerCount[0], 6)
        medianComposeMs = [math]::Round((Get-Median $compose), 3)
        medianTiffSaveMs = [math]::Round((Get-Median $tiff), 3)
        medianPreviewReportSaveMs = [math]::Round(
            (Get-Median $previewReport), 3)
        medianTotalMs = [math]::Round((Get-Median $total), 3)
        medianWallClockMs = [math]::Round((Get-Median $wall), 3)
        medianPeakWorkingSetBytes = [uint64](Get-Median $memory)
        maxPeakWorkingSetBytes = [uint64](
            $memory | Measure-Object -Maximum).Maximum
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Resolve-RepositoryPath $repositoryRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repositoryRoot $OutputRoot
$runId = (Get-Date).ToString("yyyyMMdd_HHmmss_fff")
$runRoot = Join-Path $resolvedOutputRoot "runs/$runId"
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        slicer_cli `
        rip_reader_test `
        global_surface_shell_production_pipeline_unit_tests `
        rgbwsv_production_package_writer_unit_tests `
        material_closure_report_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-10C Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests|material_closure_report_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-10C 定向单测失败，退出码=$LASTEXITCODE"
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$families = @(
    [pscustomobject]@{
        id = "xiao_ma"
        modelPath =
            "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
        sha256 =
            "4F2012E7D584C7D8F4E3A4467D0AF112216F93C222046F61A987880AF8820DDC"
    },
    [pscustomobject]@{
        id = "yecan"
        modelPath = "model/obj/yecan/3.obj"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_yecan_white_fill.json"
        sha256 =
            "A3A421005112292A71F49BED5734CE186C2B97A1379AA50E6DF8BE1A6914363D"
    })
foreach ($family in $families)
{
    $modelPath = Resolve-RepositoryPath $repositoryRoot $family.modelPath
    Assert-True (Test-Path -LiteralPath $modelPath) `
        "固定模型不存在：$($family.modelPath)"
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $modelPath).Hash
    Assert-Equal $actualHash.ToUpperInvariant() $family.sha256 `
        "固定模型 hash 漂移：$($family.modelPath)"
}

$pairs = @()
foreach ($family in $families)
{
    foreach ($width in @(
        [pscustomobject]@{id = "minimum"; mm = 0.4},
        [pscustomobject]@{id = "intermediate"; mm = 0.8},
        [pscustomobject]@{id = "all_texture"; mm = $null}))
    {
        $pairs += [pscustomobject]@{
            pairId = "$($family.id)_$($width.id)"
            modelFamily = $family.id
            modelPath = $family.modelPath
            sourceConfig = $family.sourceConfig
            widthPoint = $width.id
            requestedWidthMm = $width.mm
        }
    }
}

$samples = @()
for ($warmup = 1; $warmup -le $WarmupIterations; ++$warmup)
{
    foreach ($pair in $pairs)
    {
        foreach ($pipeline in @("legacy", "global_surface_shell"))
        {
            $samples += Invoke-PerformanceCase $pair $pipeline `
                $warmup $false $repositoryRoot $runRoot $slicerCli $ripReader
        }
    }
}
for ($iteration = 1; $iteration -le $Iterations; ++$iteration)
{
    $pipelines = if (($iteration % 2) -eq 1)
    {
        @("legacy", "global_surface_shell")
    }
    else
    {
        @("global_surface_shell", "legacy")
    }
    foreach ($pair in $pairs)
    {
        foreach ($pipeline in $pipelines)
        {
            $samples += Invoke-PerformanceCase $pair $pipeline `
                $iteration $true $repositoryRoot $runRoot $slicerCli $ripReader
        }
    }
}

$summaries = @()
$comparisons = @()
foreach ($pair in $pairs)
{
    $legacy = New-PerformanceSummary $samples $pair.pairId "legacy"
    $global = New-PerformanceSummary $samples $pair.pairId `
        "global_surface_shell"
    $summaries += $legacy
    $summaries += $global
    $comparisons += [ordered]@{
        pairId = $pair.pairId
        modelFamily = $pair.modelFamily
        widthPoint = $pair.widthPoint
        requestedWidthMm = $pair.requestedWidthMm
        legacyLayerCount = $legacy.layerCount
        globalLayerCount = $global.layerCount
        layerCountEqual = $legacy.layerCount -eq $global.layerCount
        globalVsLegacyCoreRatio = Get-Ratio `
            $global.medianCoreMs $legacy.medianCoreMs
        globalVsLegacyCorePerLayerRatio = Get-Ratio `
            $global.medianCorePerLayerMs $legacy.medianCorePerLayerMs
        globalVsLegacyTotalRatio = Get-Ratio `
            $global.medianTotalMs $legacy.medianTotalMs
        globalVsLegacyPeakMemoryRatio = Get-Ratio `
            $global.maxPeakWorkingSetBytes $legacy.maxPeakWorkingSetBytes
    }
}

$measuredSamples = @($samples | Where-Object { $_.measured })
Assert-Equal $pairs.Count 6 "performance pair count"
Assert-Equal $measuredSamples.Count (6 * 2 * $Iterations) `
    "measured sample count"
Assert-Equal @($measuredSamples | Where-Object {
    -not $_.ripStrictPass -or $_.fallbackApplied -or $_.result -ne "PASS"
}).Count 0 "invalid measured sample count"

$requiredSampleFields = @(
    "caseId", "pairId", "iteration", "measured", "modelFamily",
    "modelPath", "modelSha256", "pipelineMode", "widthPoint",
    "requestedWidthMm", "effectiveWidthMm", "dpiX", "dpiY",
    "layerThicknessMm", "storageMode", "compression",
    "previewEnabled", "packagePath", "layerCount", "packageBytes",
    "ripStrictPass", "fallbackApplied", "timingsMs", "memory", "result")
foreach ($sample in $samples)
{
    foreach ($field in $requiredSampleFields)
    {
        Assert-True ($sample.Contains($field)) `
            "$($sample.caseId) 缺少 schema 字段：$field"
    }
}

$coreRatios = [double[]]@($comparisons | ForEach-Object {
    $_.globalVsLegacyCoreRatio
})
$totalRatios = [double[]]@($comparisons | ForEach-Object {
    $_.globalVsLegacyTotalRatio
})
$memoryRatios = [double[]]@($comparisons | ForEach-Object {
    $_.globalVsLegacyPeakMemoryRatio
})
$matrix = [ordered]@{
    schema = "slicesoft.stage12e.release_performance.1"
    generatedAt = (Get-Date).ToString("o")
    buildConfiguration = $Config
    referenceMachine = [ordered]@{
        computerName = [Environment]::MachineName
        os = [Environment]::OSVersion.VersionString
        logicalProcessorCount = [Environment]::ProcessorCount
        evidenceScope = "current_reference_machine_only"
    }
    measurementContract = [ordered]@{
        warmupIterations = $WarmupIterations
        measurementIterations = $Iterations
        dpiX = 600
        dpiY = 600
        layerThicknessMm = 0.2
        storageMode = "stripped"
        compression = "none"
        previewEnabled = $false
        supportEnabled = $false
        varnishEnabled = $false
        modelFillMaterial = "white"
        ordering = "alternate_pipeline_order_by_iteration"
        primaryMetrics = @("coreMs", "peakWorkingSetBytes")
    }
    samples = $samples
    summaries = $summaries
    comparisons = $comparisons
    summary = [ordered]@{
        pairCount = 6
        measuredSampleCount = $measuredSamples.Count
        ripStrictPassCount = $measuredSamples.Count
        fallbackCount = 0
        failedCount = 0
        globalVsLegacyCoreRatioRange = @(
            ($coreRatios | Measure-Object -Minimum).Minimum,
            ($coreRatios | Measure-Object -Maximum).Maximum)
        globalVsLegacyTotalRatioRange = @(
            ($totalRatios | Measure-Object -Minimum).Minimum,
            ($totalRatios | Measure-Object -Maximum).Maximum)
        globalVsLegacyPeakMemoryRatioRange = @(
            ($memoryRatios | Measure-Object -Minimum).Minimum,
            ($memoryRatios | Measure-Object -Maximum).Maximum)
        legacyDefaultConfirmed = $true
        globalStatus = "explicit_candidate"
        pass = $true
    }
    decision = "PASS_LEGACY_DEFAULT_GLOBAL_EXPLICIT_CANDIDATE"
    residualRisks = @(
        "The measurements are a current-reference-machine baseline, not a device SLA.",
        "Legacy top-surface depth and Global 3D surface distance share requested width but are not voxel-equivalent semantics.",
        "Global effective width may clamp at the model limit and is recorded per sample.",
        "Complex relief assets remain strict blocked and are excluded from performance comparisons.",
        "No automatic default-pipeline switch is authorized by this matrix."
    )
}

$runSummaryPath = Join-Path $runRoot "release_performance_matrix.json"
$fixedSummaryPath = Join-Path $resolvedOutputRoot `
    "release_performance_matrix.json"
Write-Json $runSummaryPath $matrix
Write-Json $fixedSummaryPath $matrix

Write-Host "12E-10C Release performance matrix: PASS"
Write-Host "Measured samples: $($measuredSamples.Count); RIP strict: $($measuredSamples.Count)/$($measuredSamples.Count)"
Write-Host "Global/Legacy core ratio range: $($matrix.summary.globalVsLegacyCoreRatioRange -join ' - ')"
Write-Host "Global/Legacy total ratio range: $($matrix.summary.globalVsLegacyTotalRatioRange -join ' - ')"
Write-Host "Global/Legacy peak memory ratio range: $($matrix.summary.globalVsLegacyPeakMemoryRatioRange -join ' - ')"
Write-Host "Matrix: $fixedSummaryPath"
$global:LASTEXITCODE = 0
