param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$OutputRoot =
        "output/benchmarks/12e_09c_06_dpi_matrix",
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

function Assert-Near
{
    param(
        [double]$Actual,
        [double]$Expected,
        [double]$Tolerance,
        [string]$Message
    )

    if ([math]::Abs($Actual - $Expected) -gt $Tolerance)
    {
        throw (
            "$Message expected=$Expected actual=$Actual " +
            "tolerance=$Tolerance")
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
        [string]$Path,
        [string]$Description
    )

    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
    $candidatePath = [System.IO.Path]::GetFullPath($Path)
    Assert-True (
        $candidatePath.StartsWith(
            $rootPath,
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "$Description 不属于预期输出根目录：$candidatePath"
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

function Invoke-MeasuredProcess
{
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = (
        $Arguments |
            ForEach-Object {
                if ($_ -match "[\s`"]")
                {
                    '"' + ($_ -replace '"', '\"') + '"'
                }
                else
                {
                    $_
                }
            }) -join " "
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
        # Keep the sampled peak when the platform cannot query exited data.
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

function Get-OuterVarnish
{
    param([pscustomobject]$SliceReport)

    if ($null -ne $SliceReport.materialSemantics.outerVarnish)
    {
        return $SliceReport.materialSemantics.outerVarnish
    }
    return $SliceReport.totals.materialSemantics.outerVarnish
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
    Assert-True (Test-Path -LiteralPath $modelPath) `
        "$($Case.caseId) 模型不存在：$modelPath"

    $caseRoot = Join-Path $ResolvedOutputRoot $Case.caseId
    $packagePath = Join-Path $caseRoot "package"
    Assert-PathUnderRoot $ResolvedOutputRoot $packagePath `
        "$($Case.caseId) package"
    if (Test-Path -LiteralPath $packagePath)
    {
        Remove-Item -LiteralPath $packagePath -Recurse -Force
    }

    $configJson.input.modelPath =
        [System.IO.Path]::GetFullPath($modelPath).Replace("\", "/")
    $configJson.output.packageDir =
        [System.IO.Path]::GetFullPath($packagePath).Replace("\", "/")
    $configJson.output.dpiX = $Case.dpiX
    $configJson.output.dpiY = $Case.dpiY
    $configJson.output.layerThicknessMm = 0.01
    $configJson.preview.enabled = $true
    $configJson.preview.interval = 50

    if ($Case.pipelineMode -eq "legacy")
    {
        $configJson | Add-Member -Force -NotePropertyName slicePipeline `
            -NotePropertyValue ([pscustomobject]@{mode = "legacy"})
        $configJson.modelTransform.scale = @(1.0, 1.0, 1.0)
        $configJson.materialProcessProfile.name =
            "dpi_matrix_$($Case.caseId)"
    }

    $configPath = Join-Path $caseRoot "config.json"
    Write-Utf8NoBom $configPath (
        $configJson | ConvertTo-Json -Depth 100)
    return [ordered]@{
        configPath = $configPath
        packagePath = $packagePath
        modelPath = $modelPath
        modelSha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $modelPath).Hash
    }
}

function Assert-ProductionPackage
{
    param(
        [pscustomobject]$Case,
        [hashtable]$Paths,
        [string]$RipReader
    )

    $manifest = Read-Json (Join-Path $Paths.packagePath "manifest.json")
    $sliceReport = Read-Json (
        Join-Path $Paths.packagePath "reports/slice_report.json")
    $previewReport = Read-Json (
        Join-Path $Paths.packagePath "reports/preview_report.json")

    Assert-Equal $manifest.schema "p0.rgbwsv.2" `
        "$($Case.caseId) schema"
    Assert-Equal $manifest.schemaVersion "p0.rgbwsv.2" `
        "$($Case.caseId) schema version"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$($Case.caseId) channel order"
    Assert-Equal $manifest.tiff.bitDepth 8 "$($Case.caseId) bit depth"
    Assert-Equal $manifest.tiff.polarity "black_is_print" `
        "$($Case.caseId) polarity"
    Assert-Equal $manifest.tiff.printValue 0 `
        "$($Case.caseId) print value"
    Assert-Equal $manifest.tiff.emptyValue 255 `
        "$($Case.caseId) empty value"
    Assert-Equal $manifest.requestedPipelineMode $Case.pipelineMode `
        "$($Case.caseId) requested mode"
    Assert-Equal $manifest.effectivePipelineMode $Case.pipelineMode `
        "$($Case.caseId) effective mode"
    Assert-Equal $manifest.productionOutputWritten $true `
        "$($Case.caseId) production output"
    Assert-Equal $manifest.fallbackApplied $false `
        "$($Case.caseId) fallback"

    Assert-Equal $manifest.grid.dpiX $Case.dpiX `
        "$($Case.caseId) dpiX"
    Assert-Equal $manifest.grid.dpiY $Case.dpiY `
        "$($Case.caseId) dpiY"
    Assert-Equal @($manifest.grid.dpi).Count 2 `
        "$($Case.caseId) dpi tuple length"
    Assert-Equal $manifest.grid.dpi[0] $Case.dpiX `
        "$($Case.caseId) dpi[0]"
    Assert-Equal $manifest.grid.dpi[1] $Case.dpiY `
        "$($Case.caseId) dpi[1]"
    $pixelSizeXmm = 25.4 / [double]$Case.dpiX
    $pixelSizeYmm = 25.4 / [double]$Case.dpiY
    Assert-Near $manifest.grid.pixelSizeXmm $pixelSizeXmm 0.0000001 `
        "$($Case.caseId) pixelSizeXmm"
    Assert-Near $manifest.grid.pixelSizeYmm $pixelSizeYmm 0.0000001 `
        "$($Case.caseId) pixelSizeYmm"
    Assert-Near $manifest.grid.pixelSizeMm[0] $pixelSizeXmm 0.0000001 `
        "$($Case.caseId) pixelSizeMm[0]"
    Assert-Near $manifest.grid.pixelSizeMm[1] $pixelSizeYmm 0.0000001 `
        "$($Case.caseId) pixelSizeMm[1]"

    Assert-Equal $sliceReport.requestedPipelineMode $Case.pipelineMode `
        "$($Case.caseId) report requested mode"
    Assert-Equal $sliceReport.effectivePipelineMode $Case.pipelineMode `
        "$($Case.caseId) report effective mode"
    Assert-Equal $sliceReport.productionOutputWritten $true `
        "$($Case.caseId) report production output"
    Assert-Equal $sliceReport.fallbackApplied $false `
        "$($Case.caseId) report fallback"
    if ($null -ne $sliceReport.grid)
    {
        Assert-Equal $sliceReport.grid.widthPx $manifest.grid.widthPx `
            "$($Case.caseId) report width"
        Assert-Equal $sliceReport.grid.heightPx $manifest.grid.heightPx `
            "$($Case.caseId) report height"
        Assert-Near $sliceReport.grid.pixelSizeMm[0] $pixelSizeXmm `
            0.0000001 "$($Case.caseId) report pixel X"
        Assert-Near $sliceReport.grid.pixelSizeMm[1] $pixelSizeYmm `
            0.0000001 "$($Case.caseId) report pixel Y"
    }

    Assert-Equal $previewReport.schema "p0.preview_report.1" `
        "$($Case.caseId) preview schema"
    Assert-Equal $previewReport.enabled $true `
        "$($Case.caseId) preview enabled"
    Assert-Equal $previewReport.format "png" `
        "$($Case.caseId) preview format"
    $previewCount = 0
    foreach ($previewEntry in @($previewReport.files))
    {
        $relativePath = [string]$previewEntry.path
        Assert-True (-not [string]::IsNullOrWhiteSpace($relativePath)) `
            "$($Case.caseId) preview path 为空"
        $previewPath = Join-Path $Paths.packagePath $relativePath
        Assert-PathUnderRoot $Paths.packagePath $previewPath `
            "$($Case.caseId) preview"
        Assert-True (Test-Path -LiteralPath $previewPath) `
            "$($Case.caseId) 预览不存在：$relativePath"
        $previewCount++
    }
    Assert-True ($previewCount -gt 0) `
        "$($Case.caseId) 缺少当前 package 预览"

    Assert-Equal @($manifest.layers).Count $manifest.grid.layerCount `
        "$($Case.caseId) complete layer list"
    Assert-Equal @($manifest.tiff.layers).Count $manifest.grid.layerCount `
        "$($Case.caseId) TIFF layer list"
    foreach ($layer in @($manifest.layers))
    {
        Assert-Equal $layer.widthPx $manifest.grid.widthPx `
            "$($Case.caseId) layer width"
        Assert-Equal $layer.heightPx $manifest.grid.heightPx `
            "$($Case.caseId) layer height"
        $layerPath = Join-Path $Paths.packagePath ([string]$layer.path)
        Assert-PathUnderRoot $Paths.packagePath $layerPath `
            "$($Case.caseId) TIFF"
        Assert-True (Test-Path -LiteralPath $layerPath) `
            "$($Case.caseId) 缺少 TIFF：$($layer.path)"
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
        $RipReader @("--package", $Paths.packagePath, "--summary")
    Assert-Equal $rip.exitCode 0 `
        "$($Case.caseId) RIP Reader：$($rip.outputLines -join '; ')"

    $printPixels = [ordered]@{}
    foreach ($channel in @("R", "G", "B", "W", "S", "V"))
    {
        $printPixels[$channel] =
            Get-ChannelPrintPixels $sliceReport $channel
    }
    $outerVarnish = Get-OuterVarnish $sliceReport
    return [ordered]@{
        widthPx = [int]$manifest.grid.widthPx
        heightPx = [int]$manifest.grid.heightPx
        layerCount = [int]$manifest.grid.layerCount
        dpiX = [int]$manifest.grid.dpiX
        dpiY = [int]$manifest.grid.dpiY
        pixelSizeXmm = [double]$manifest.grid.pixelSizeXmm
        pixelSizeYmm = [double]$manifest.grid.pixelSizeYmm
        physicalWidthMm = [double]$manifest.grid.widthPx * $pixelSizeXmm
        physicalHeightMm = [double]$manifest.grid.heightPx * $pixelSizeYmm
        tiffCount = @($manifest.layers).Count
        previewCount = $previewCount
        printPixels = $printPixels
        outerVarnish = $outerVarnish
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
        slicer_debug_ui `
        rip_reader_test `
        output_resolution_config_unit_tests `
        rip_reader_resolution_unit_tests `
        outer_varnish_discretization_unit_tests `
        non_square_raster_pipeline_unit_tests `
        global_surface_shell_production_pipeline_unit_tests `
        production_effective_config_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09C-06 $Config build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(output_resolution_config_unit_tests|rip_reader_resolution_unit_tests|outer_varnish_discretization_unit_tests|non_square_raster_pipeline_unit_tests|global_surface_shell_production_pipeline_unit_tests|production_effective_config_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-09C-06 DPI 定向单测失败，退出码=$LASTEXITCODE"
}

$ui = Resolve-Executable $resolvedBuildDir $Config "slicer_debug_ui"
foreach ($uiCase in @(
    [ordered]@{name = "self-test"; arguments = @("--self-test")},
    [ordered]@{
        name = "slice-settings-model"
        arguments = @("--ui-smoke-test", "--case", "slice-settings-model")
    },
    [ordered]@{
        name = "generated-effective-config"
        arguments = @(
            "--ui-smoke-test",
            "--case",
            "generated-effective-config")
    },
    [ordered]@{
        name = "preview-physical-aspect"
        arguments = @(
            "--ui-smoke-test",
            "--case",
            "preview-physical-aspect")
    },
    [ordered]@{
        name = "production-mode-selector"
        arguments = @(
            "--ui-smoke-test",
            "--case",
            "production-mode-selector")
    }))
{
    $uiRun = Invoke-MeasuredProcess $ui $uiCase.arguments
    Assert-Equal $uiRun.exitCode 0 `
        "12E-09C-06 UI smoke 失败：$($uiCase.name)；$($uiRun.outputLines -join '; ')"
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$legacySource =
    "samples/configs/material_process/nail_rgb_white_varnish_top2.json"
$modelPath =
    "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj"
$cases = @(
    [pscustomobject][ordered]@{
        caseId = "legacy_600_compat"
        sourceConfig = $legacySource
        modelPath = $modelPath
        pipelineMode = "legacy"
        profile = "legacy_explicit_600_compat"
        dpiX = 600
        dpiY = 600
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    },
    [pscustomobject][ordered]@{
        caseId = "legacy_635x600"
        sourceConfig = $legacySource
        modelPath = $modelPath
        pipelineMode = "legacy"
        profile = "legacy_non_square"
        dpiX = 635
        dpiY = 600
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    },
    [pscustomobject][ordered]@{
        caseId = "global_restricted_635x600"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_restricted_candidate"
        dpiX = 635
        dpiY = 600
        requiredChannels = @("R", "G", "B", "W")
        emptyChannels = @("S", "V")
    },
    [pscustomobject][ordered]@{
        caseId = "global_material_parity_635x600"
        sourceConfig =
            "samples/configs/texture_fill_partition/global_production_xiao_ma_material_parity.json"
        modelPath = $null
        pipelineMode = "global_surface_shell"
        profile = "global_surface_shell_material_parity_candidate"
        dpiX = 635
        dpiY = 600
        requiredChannels = @("R", "G", "B", "W", "S", "V")
        emptyChannels = @()
    }
)

$caseResults = @()
foreach ($case in $cases)
{
    Write-Host "运行 12E-09C-06 case：$($case.caseId)"
    $paths = New-CaseConfig $case $repoRoot $resolvedOutputRoot
    $run = Invoke-MeasuredProcess `
        $slicerCli @("--config", $paths.configPath)
    if ($run.exitCode -ne 0)
    {
        throw (
            "$($case.caseId) 切片失败：" +
            ($run.outputLines -join [Environment]::NewLine))
    }
    $timing = Parse-Timing $run.outputLines
    Assert-Equal $timing.engine $case.pipelineMode `
        "$($case.caseId) timing engine"
    $package = Assert-ProductionPackage $case $paths $ripReader
    $caseResults += [ordered]@{
        caseId = $case.caseId
        modelFamily = "xiao_ma_wu_yu_new"
        modelPath = $paths.modelPath
        modelSha256 = $paths.modelSha256
        configPath = $paths.configPath
        packagePath = $paths.packagePath
        pipelineMode = $case.pipelineMode
        profile = $case.profile
        dpiX = $case.dpiX
        dpiY = $case.dpiY
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

$legacy600 = @(
    $caseResults |
        Where-Object { $_.caseId -eq "legacy_600_compat" })[0]
$legacy635 = @(
    $caseResults |
        Where-Object { $_.caseId -eq "legacy_635x600" })[0]
$globalRestricted = @(
    $caseResults |
        Where-Object { $_.caseId -eq "global_restricted_635x600" })[0]
$globalParity = @(
    $caseResults |
        Where-Object {
            $_.caseId -eq "global_material_parity_635x600"
        })[0]

foreach ($case in $caseResults)
{
    Assert-Equal $case.modelSha256 $legacy600.modelSha256 `
        "$($case.caseId) 必须使用同一模型资产"
}
Assert-Near $legacy600.package.physicalWidthMm `
    $legacy635.package.physicalWidthMm 0.05 `
    "Legacy 600/600 与 635/600 物理宽度"
Assert-Near $legacy600.package.physicalHeightMm `
    $legacy635.package.physicalHeightMm 0.05 `
    "Legacy 600/600 与 635/600 物理高度"
Assert-Near $legacy635.package.physicalWidthMm `
    $globalRestricted.package.physicalWidthMm 0.25 `
    "Legacy 与 Global restricted 635/600 物理宽度"
Assert-Near $legacy635.package.physicalHeightMm `
    $globalRestricted.package.physicalHeightMm 0.25 `
    "Legacy 与 Global restricted 635/600 物理高度"

$outerVarnish = $globalParity.package.outerVarnish
Assert-Equal $outerVarnish.enabled $true `
    "Global material parity 外侧光油"
Assert-Near $outerVarnish.pixelSizeXmm (25.4 / 635.0) 0.0000001 `
    "外侧光油 pixelSizeXmm"
Assert-Near $outerVarnish.pixelSizeYmm (25.4 / 600.0) 0.0000001 `
    "外侧光油 pixelSizeYmm"
Assert-Equal $outerVarnish.pixelPitchSource "output_dpi" `
    "外侧光油物理像素来源"
Assert-True (
    $outerVarnish.effectiveThicknessXmm -ge
        $outerVarnish.requestedThicknessMm) `
    "外侧光油 X 有效厚度不足"
Assert-True (
    $outerVarnish.effectiveThicknessYmm -ge
        $outerVarnish.requestedThicknessMm) `
    "外侧光油 Y 有效厚度不足"

$comparisons = [ordered]@{
    explicit600Vs635Legacy = [ordered]@{
        physicalWidthDeltaMm = [math]::Abs(
            $legacy600.package.physicalWidthMm -
                $legacy635.package.physicalWidthMm)
        physicalHeightDeltaMm = [math]::Abs(
            $legacy600.package.physicalHeightMm -
                $legacy635.package.physicalHeightMm)
        toleranceMm = 0.05
        pass = $true
    }
    sameDpiLegacyVsGlobalRestricted = [ordered]@{
        dpiX = 635
        dpiY = 600
        physicalWidthDeltaMm = [math]::Abs(
            $legacy635.package.physicalWidthMm -
                $globalRestricted.package.physicalWidthMm)
        physicalHeightDeltaMm = [math]::Abs(
            $legacy635.package.physicalHeightMm -
                $globalRestricted.package.physicalHeightMm)
        toleranceMm = 0.25
        pass = $true
    }
    outerVarnish = [ordered]@{
        requestedThicknessMm =
            [double]$outerVarnish.requestedThicknessMm
        radiusXPx = [int]$outerVarnish.radiusXPx
        radiusYPx = [int]$outerVarnish.radiusYPx
        effectiveThicknessXmm =
            [double]$outerVarnish.effectiveThicknessXmm
        effectiveThicknessYmm =
            [double]$outerVarnish.effectiveThicknessYmm
        pixelPitchSource = [string]$outerVarnish.pixelPitchSource
        pass = $true
    }
    previewPhysicalAspect = [ordered]@{
        uiSmoke = "pass"
        correctedFixture = "635x600"
        pass = $true
    }
}

$summary = [ordered]@{
    schema = "slicesoft.xy_dpi_matrix.12e_09c.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-09C-06"
    buildType = $Config
    modelFamily = "xiao_ma_wu_yu_new"
    modelSha256 = $legacy600.modelSha256
    layerThicknessMm = 0.01
    certifiedDpiCombinations = @(
        [ordered]@{dpiX = 600; dpiY = 600},
        [ordered]@{dpiX = 635; dpiY = 600})
    cases = $caseResults
    comparisons = $comparisons
    result = [ordered]@{
        explicit600Compatibility = "pass"
        legacy635x600Production = "pass"
        globalRestricted635x600Production = "pass"
        globalMaterialParity635x600Production = "pass"
        manifestPreviewReportConsistency = "pass"
        ripStrict = "pass"
        fallbackApplied = $false
        pass = ($caseResults.Count -eq 4)
    }
    boundaries = @(
        "p0.rgbwsv.2, R G B W S V, uint8 and black_is_print remain unchanged.",
        "Legacy remains the default; Global remains explicit opt-in with no silent fallback.",
        "This matrix certifies package compatibility, not printer hardware calibration."
    )
}
$summaryPath = Join-Path $resolvedOutputRoot "dpi_matrix_summary.json"
Write-Utf8NoBom $summaryPath (
    $summary | ConvertTo-Json -Depth 100)

Write-Host "12E-09C-06 X/Y DPI production matrix: PASS"
Write-Host "Explicit 600/600 compatibility: PASS"
Write-Host "Legacy + Global 635/600 package and RIP strict: PASS"
Write-Host "Physical extent / outer varnish / preview aspect: PASS"
Write-Host "Summary: $summaryPath"
$global:LASTEXITCODE = 0
