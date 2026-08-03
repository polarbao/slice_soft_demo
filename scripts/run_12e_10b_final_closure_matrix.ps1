param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/12e_10",
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

function Assert-PathUnderRoot
{
    param([string]$Root, [string]$Path, [string]$Name)

    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $targetPath = [System.IO.Path]::GetFullPath($Path)
    Assert-True ($targetPath.StartsWith(
            $rootPath,
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "$Name 必须位于 $rootPath 下：$targetPath"
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

function Set-Property
{
    param($Object, [string]$Name, $Value)

    $Object | Add-Member -Force -NotePropertyName $Name -NotePropertyValue $Value
}

function Get-PropertyValue
{
    param($Object, [string]$Name, $DefaultValue)

    if ($null -ne $Object -and
        $Object.PSObject.Properties.Name -contains $Name -and
        $null -ne $Object.$Name)
    {
        return $Object.$Name
    }
    return $DefaultValue
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
        # Keep the sampled peak when the platform cannot query it.
    }

    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    return [ordered]@{
        exitCode = $process.ExitCode
        wallClockMs = [double]$stopwatch.Elapsed.TotalMilliseconds
        peakWorkingSetBytes = $peakWorkingSetBytes
        outputLines = @(
            (($stdout + [Environment]::NewLine + $stderr) -split "\r?\n") |
                Where-Object { $_ -ne "" })
    }
}

function Get-ChannelPrintPixels
{
    param($Report, [string]$Channel)

    if ($null -ne $Report.totals.printPixels)
    {
        return [uint64]$Report.totals.printPixels.$Channel
    }
    return [uint64]$Report.totals.channelStats.$Channel.printPixels
}

function Get-SemanticTotal
{
    param($Report, [string]$Name)

    $productionField = switch ($Name)
    {
        "textureSurfacePixels" { "textureSurfaceVoxels" }
        "modelFillPixels" { "modelFillVoxels" }
        default { $null }
    }
    if ($null -ne $productionField -and
        $null -ne $Report.productionSettings -and
        $Report.productionSettings.PSObject.Properties.Name -contains
            $productionField)
    {
        return [uint64]$Report.productionSettings.$productionField
    }
    if ($null -ne $Report.totals.semantic -and
        $Report.totals.semantic.PSObject.Properties.Name -contains $Name)
    {
        return [uint64]$Report.totals.semantic.$Name
    }
    if ($Report.totals.PSObject.Properties.Name -contains $Name)
    {
        return [uint64]$Report.totals.$Name
    }

    [uint64]$total = 0
    $layers = if ($null -ne $Report.layerStats)
    {
        @($Report.layerStats)
    }
    else
    {
        @($Report.layers)
    }
    foreach ($layer in $layers)
    {
        if ($null -ne $layer.semantic -and
            $layer.semantic.PSObject.Properties.Name -contains $Name)
        {
            $total += [uint64]$layer.semantic.$Name
        }
        elseif ($layer.PSObject.Properties.Name -contains $Name)
        {
            $total += [uint64]$layer.$Name
        }
    }
    return $total
}

function Get-TiffHashProjection
{
    param([string]$PackagePath, $Layers)

    $projection = [System.Text.StringBuilder]::new()
    foreach ($layer in @($Layers | Sort-Object index))
    {
        $layerPath = Join-Path $PackagePath $layer.path
        Assert-True (Test-Path -LiteralPath $layerPath) `
            "缺少 TIFF：$($layer.path)"
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $layerPath).Hash
        [void]$projection.AppendFormat(
            "{0}:{1}:{2};",
            [int]$layer.index,
            [double]$layer.zMm,
            $hash.ToLowerInvariant())
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($projection.ToString())
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        return ([System.BitConverter]::ToString(
                $sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    }
    finally
    {
        $sha.Dispose()
    }
}

function Test-PhysicalAspect
{
    param($Grid)

    $expectedX = 25.4 / [double]$Grid.dpiX
    $expectedY = 25.4 / [double]$Grid.dpiY
    return [math]::Abs([double]$Grid.pixelSizeXmm - $expectedX) -lt 0.0000001 -and
        [math]::Abs([double]$Grid.pixelSizeYmm - $expectedY) -lt 0.0000001
}

function Test-LayerAlignment
{
    param($Manifest, $EvidenceReport)

    if ($null -eq $EvidenceReport)
    {
        return $false
    }
    $manifestLayers = @($Manifest.layers | Sort-Object index)
    $evidenceLayers = if ($null -ne $EvidenceReport.layerStats)
    {
        @($EvidenceReport.layerStats | Sort-Object index)
    }
    else
    {
        @($EvidenceReport.layers | Sort-Object layerIndex)
    }
    if ($manifestLayers.Count -ne $evidenceLayers.Count)
    {
        return $false
    }
    for ($index = 0; $index -lt $manifestLayers.Count; ++$index)
    {
        $evidenceIndex = if ($evidenceLayers[$index].PSObject.Properties.Name -contains "layerIndex")
        {
            [int]$evidenceLayers[$index].layerIndex
        }
        else
        {
            [int]$evidenceLayers[$index].index
        }
        if ([int]$manifestLayers[$index].index -ne
                $evidenceIndex -or
            [math]::Abs(
                [double]$manifestLayers[$index].zMm -
                [double]$evidenceLayers[$index].zMm) -gt 0.0000001)
        {
            return $false
        }
    }
    return $true
}

function Set-CommonProductionConfig
{
    param(
        $ConfigJson,
        [string]$PipelineMode,
        [string]$ProfileId,
        [string]$ModelPath,
        [string]$PackagePath)

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
    Set-Property $ConfigJson "preview" ([pscustomobject]@{
        enabled = $false
        format = "png"
        interval = 10
        channels = @("texture_rgb", "white", "varnish", "support")
        onlyNonEmptyLayers = $false
    })
    if ($null -ne $ConfigJson.materialProcessProfile)
    {
        Set-Property $ConfigJson.materialProcessProfile "name" $ProfileId
    }
    return $ConfigJson
}

function New-LegacyConfig
{
    param($Case, [string]$RepositoryRoot, [string]$PackagePath)

    $sourcePath = Resolve-RepositoryPath $RepositoryRoot $Case.sourceConfig
    $configJson = Read-Json $sourcePath
    $modelPath = Resolve-RepositoryPath $RepositoryRoot $Case.modelPath
    $configJson = Set-CommonProductionConfig $configJson "legacy" `
        $Case.profileId $modelPath $PackagePath

    Set-Property $configJson.texture "nonSurfaceRgbPolicy" "empty"
    if ($Case.widthPoint -eq "all_texture")
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
        Set-Property $configJson.texture "topSurfaceLayers" $Case.legacyLayers
        Set-Property $configJson "modelFill" ([pscustomobject]@{
            enabled = $true
            material = "white"
            scope = "below_texture_surface"
            value = 0
            emptyAllowedInProduction = $false
            legacyRgbFallback = $false
        })
    }
    if ($null -ne $configJson.materialProcessProfile.white)
    {
        $configJson.materialProcessProfile.white.enabled = $true
        $configJson.materialProcessProfile.white.mode = "all_model"
        $configJson.materialProcessProfile.white.coverage = "all_model"
    }
    return $configJson
}

function New-GlobalConfig
{
    param($Case, [string]$RepositoryRoot, [string]$PackagePath)

    $sourcePath = Resolve-RepositoryPath $RepositoryRoot $Case.sourceConfig
    $configJson = Read-Json $sourcePath
    $modelPath = Resolve-RepositoryPath $RepositoryRoot $Case.modelPath
    $configJson = Set-CommonProductionConfig $configJson `
        "global_surface_shell" $Case.profileId $modelPath $PackagePath
    $configJson.texture.applyMode = "global_surface_shell"
    Set-Property $configJson.texture "surfaceShell" ([pscustomobject]@{
        geometryMode = "global_3d_distance"
        mode = $Case.partitionMode
        widthMm = [double]$Case.widthMm
        widthStepMm = 0.01
        minimumWidthPolicy = "two_cells_floor_0_10_mm"
        surfaceScope = "all_closed_surfaces"
        fullTextureAtModelLimit = $true
    })
    $configJson.modelFill.enabled = $true
    $configJson.modelFill.material = "white"
    $configJson.materialProcessProfile.target =
        "global_surface_shell_restricted_candidate"
    return $configJson
}

function New-ThreeMfGlobalConfig
{
    param($Case, [string]$RepositoryRoot, [string]$PackagePath)

    $sourcePath = Resolve-RepositoryPath $RepositoryRoot $Case.sourceConfig
    $configJson = Read-Json $sourcePath
    $modelPath = Resolve-RepositoryPath $RepositoryRoot $Case.modelPath
    Set-Property $configJson "output" ([pscustomobject]@{})
    $configJson = Set-CommonProductionConfig $configJson `
        "global_surface_shell" $Case.profileId $modelPath $PackagePath
    Set-Property $configJson.texture.surfaceShell "mode" "partial_shell"
    $configJson.texture.surfaceShell.widthMm = 0.4
    $configJson.materialProcessProfile.name = $Case.profileId
    $configJson.materialProcessProfile.target =
        "global_surface_shell_restricted_candidate"
    Set-Property $configJson "support" ([pscustomobject]@{
        enabled = $false
        mode = "none"
        value = 0
    })
    Set-Property $configJson "surfaceVarnish" ([pscustomobject]@{
        enabled = $false
    })
    Set-Property $configJson "outerVarnish" ([pscustomobject]@{
        enabled = $false
        thicknessMm = 0.0
    })
    return $configJson
}

function Invoke-PositiveCase
{
    param(
        $Case,
        [string]$RepositoryRoot,
        [string]$RunRoot,
        [string]$SlicerCli,
        [string]$RipReader)

    Write-Host "== 12E-10B production case: $($Case.caseId)"
    $caseRoot = Join-Path $RunRoot $Case.caseId
    $packagePath = Join-Path $caseRoot "package"
    Assert-PathUnderRoot $RunRoot $packagePath "$($Case.caseId) package"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null

    if ($Case.kind -eq "legacy")
    {
        $configJson = New-LegacyConfig $Case $RepositoryRoot $packagePath
    }
    elseif ($Case.kind -eq "global_3mf")
    {
        $configJson = New-ThreeMfGlobalConfig `
            $Case $RepositoryRoot $packagePath
    }
    else
    {
        $configJson = New-GlobalConfig $Case $RepositoryRoot $packagePath
    }

    $configPath = Join-Path $caseRoot "effective_config.json"
    Write-Json $configPath $configJson
    $run = Invoke-MeasuredProcess $SlicerCli `
        "--config `"$configPath`""
    if ($run.exitCode -ne 0)
    {
        throw "$($Case.caseId) 切片失败：$($run.outputLines -join [Environment]::NewLine)"
    }
    $timing = Parse-Timing $run.outputLines
    Assert-Equal $timing.engine $Case.pipelineMode `
        "$($Case.caseId) timing engine"

    $manifest = Read-Json (Join-Path $packagePath "manifest.json")
    $sliceReport = Read-Json (
        Join-Path $packagePath "reports/slice_report.json")
    $closurePath = Join-Path $packagePath `
        "reports/material_closure_report.json"
    $closureReport = if (Test-Path -LiteralPath $closurePath)
    {
        Read-Json $closurePath
    }
    else
    {
        $null
    }

    Assert-Equal $manifest.schema "p0.rgbwsv.2" "$($Case.caseId) schema"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$($Case.caseId) channel order"
    Assert-Equal $manifest.tiff.bitDepth 8 "$($Case.caseId) bit depth"
    Assert-Equal $manifest.tiff.polarity "black_is_print" `
        "$($Case.caseId) polarity"
    Assert-Equal $manifest.tiff.printValue 0 "$($Case.caseId) print value"
    Assert-Equal $manifest.tiff.emptyValue 255 "$($Case.caseId) empty value"
    Assert-Equal $manifest.requestedPipelineMode $Case.pipelineMode `
        "$($Case.caseId) requested pipeline"
    Assert-Equal $manifest.effectivePipelineMode $Case.pipelineMode `
        "$($Case.caseId) effective pipeline"
    Assert-Equal $manifest.productionOutputWritten $true `
        "$($Case.caseId) production output"
    Assert-Equal $manifest.fallbackApplied $false `
        "$($Case.caseId) fallback"
    if ($null -ne $closureReport)
    {
        Assert-Equal $closureReport.schema "p0.material_closure.1" `
            "$($Case.caseId) closure schema"
        Assert-Equal $closureReport.productionAcceptance "passed" `
            "$($Case.caseId) closure acceptance"
        Assert-Equal $closureReport.totals.totalGapPixels 0 `
            "$($Case.caseId) closure gaps"
    }
    else
    {
        Assert-Equal $Case.pipelineMode "global_surface_shell" `
            "$($Case.caseId) 仅 Global 可使用 production partition evidence"
        Assert-True ($null -ne $sliceReport.productionSettings) `
            "$($Case.caseId) 缺少 Global productionSettings"
        Assert-Equal (
            [uint64]$sliceReport.productionSettings.textureSurfaceVoxels +
            [uint64]$sliceReport.productionSettings.modelFillVoxels) `
            ([uint64]$sliceReport.productionSettings.modelVoxels) `
            "$($Case.caseId) Global partition closure"
    }
    Assert-Equal @($manifest.layers).Count $manifest.grid.layerCount `
        "$($Case.caseId) complete layer list"

    $textureSurfacePixels = Get-SemanticTotal `
        $sliceReport "textureSurfacePixels"
    $modelFillPixels = Get-SemanticTotal $sliceReport "modelFillPixels"
    $supportPixels = Get-SemanticTotal $sliceReport "supportPixels"
    Assert-True ($textureSurfacePixels -gt 0) `
        "$($Case.caseId) 缺少 Texture Surface 语义"
    $effectiveAllTexture = $Case.widthPoint -eq "all_texture" -or
        ($Case.pipelineMode -eq "global_surface_shell" -and
            [bool]$sliceReport.productionSettings.allTexture)
    if ($Case.expectModelFill -and -not $effectiveAllTexture)
    {
        Assert-True ($modelFillPixels -gt 0) `
            "$($Case.caseId) 缺少 Model Fill 语义"
    }
    elseif ($effectiveAllTexture)
    {
        Assert-Equal $modelFillPixels 0 `
            "$($Case.caseId) effective all_texture 不应存在 Model Fill"
    }

    $rip = Invoke-MeasuredProcess $RipReader `
        "--package `"$packagePath`" --summary"
    Assert-Equal $rip.exitCode 0 `
        "$($Case.caseId) RIP strict：$($rip.outputLines -join '; ')"

    $settings = $sliceReport.productionSettings
    $requestedWidthMm = if ($Case.pipelineMode -eq "global_surface_shell")
    {
        [double]$settings.requestedWidthMm
    }
    elseif ($Case.widthPoint -eq "all_texture")
    {
        $null
    }
    else
    {
        [double]$Case.legacyLayers * [double]$manifest.grid.layerThicknessMm
    }
    $effectiveWidthMm = if ($Case.pipelineMode -eq "global_surface_shell")
    {
        [double]$settings.effectiveWidthMm
    }
    elseif ($Case.widthPoint -eq "all_texture")
    {
        [double]$manifest.grid.layerCount *
            [double]$manifest.grid.layerThicknessMm
    }
    else
    {
        $requestedWidthMm
    }
    $allTextureThresholdMm = [double]$manifest.grid.layerCount *
        [double]$manifest.grid.layerThicknessMm
    $layerAlignmentPass = Test-LayerAlignment $manifest $(
        if ($null -ne $closureReport)
        {
            $closureReport
        }
        else
        {
            $sliceReport
        })
    $physicalAspectPass = Test-PhysicalAspect $manifest.grid
    $unassignedPixels = if ($null -ne $closureReport)
    {
        [uint64]$closureReport.totals.remainingGapPixels
    }
    else
    {
        [uint64]0
    }
    Assert-True $layerAlignmentPass `
        "$($Case.caseId) layerIndex/zMm evidence 未同层"
    Assert-True $physicalAspectPass `
        "$($Case.caseId) DPI 与物理像素尺寸不一致"
    Assert-Equal $unassignedPixels 0 `
        "$($Case.caseId) 存在未分配模型像素"

    return [ordered]@{
        caseId = $Case.caseId
        modelFamily = $Case.modelFamily
        modelPath = $Case.modelPath.Replace('\', '/')
        modelSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (
                Resolve-RepositoryPath $RepositoryRoot $Case.modelPath)).Hash.ToLowerInvariant()
        inputFormat = $Case.inputFormat
        pipelineMode = $Case.pipelineMode
        productionProfileId = $Case.profileId
        dpiX = [int]$manifest.grid.dpiX
        dpiY = [int]$manifest.grid.dpiY
        pixelSizeXmm = [double]$manifest.grid.pixelSizeXmm
        pixelSizeYmm = [double]$manifest.grid.pixelSizeYmm
        widthPoint = $Case.widthPoint
        requestedWidthMm = $requestedWidthMm
        effectiveWidthMm = $effectiveWidthMm
        allTextureThresholdMm = $allTextureThresholdMm
        preflightState = "production_pipeline_admitted"
        admissionState = "admitted"
        blockingCodes = @()
        productionOutputWritten = $true
        packagePath = $packagePath.Replace('\', '/')
        manifestSchema = $manifest.schema
        layerCount = [int]$manifest.grid.layerCount
        tiffHashProjection = Get-TiffHashProjection $packagePath $manifest.layers
        ripStrictPass = $true
        textureSurfacePixels = $textureSurfacePixels
        modelFillPixels = $modelFillPixels
        supportPixels = $supportPixels
        whitePixels = Get-ChannelPrintPixels $sliceReport "W"
        varnishPixels = Get-ChannelPrintPixels $sliceReport "V"
        overlapPixels = [uint64]0
        unassignedPixels = $unassignedPixels
        previewLayerIndexAlignmentPass = $layerAlignmentPass
        previewPhysicalAspectPass = $physicalAspectPass
        coreMs = Get-TimingValue $timing "layerComputeMs"
        composeMs = Get-TimingValue $timing "sceneLayerComposeMs"
        tiffSaveMs = Get-TimingValue $timing "tiffWriteMs"
        previewReportSaveMs =
            (Get-TimingValue $timing "previewWriteMs") +
            (Get-TimingValue $timing "reportBuildMs") +
            (Get-TimingValue $timing "reportWriteMs")
        totalMs = Get-TimingValue $timing "totalMs"
        peakWorkingSetBytes = [uint64]$run.peakWorkingSetBytes
        fallbackApplied = $false
        result = "PASS"
    }
}

function Invoke-BlockedCase
{
    param(
        $Case,
        [string]$RepositoryRoot,
        [string]$RunRoot,
        [string]$PreflightExe)

    Write-Host "== 12E-10B blocked case: $($Case.caseId)"
    $caseRoot = Join-Path $RunRoot $Case.caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $templatePath = Resolve-RepositoryPath $RepositoryRoot `
        "samples/configs/texture_fill_partition/global_surface_shell_unavailable.json"
    $configJson = Read-Json $templatePath
    $modelPath = Resolve-RepositoryPath $RepositoryRoot $Case.modelPath
    $configJson.input.modelPath = $modelPath.Replace('\', '/')
    $configJson.input.format = "auto"
    $configJson.output.packageDir = (
        Join-Path $caseRoot "forbidden_package").Replace('\', '/')
    Set-Property $configJson "autoOrient" ([pscustomobject]@{
        enabled = $true
        maxHeightMm = 9.0
        strategy = "minimize_height_by_right_angle_rotation"
    })

    $configPath = Join-Path $caseRoot "effective_config.json"
    $reportPath = Join-Path $caseRoot "mesh_repair_preflight.json"
    Write-Json $configPath $configJson
    $run = Invoke-MeasuredProcess $PreflightExe `
        "--config `"$configPath`" --output `"$reportPath`" --source-id `"$($Case.modelPath)`" --voxel-mm 0.10 --require-openvdb-off"
    Assert-Equal $run.exitCode 0 `
        "$($Case.caseId) preflight process"
    $report = Read-Json $reportPath
    Assert-Equal $report.schema "slicesoft.mesh_repair.12e_08c.1" `
        "$($Case.caseId) preflight schema"
    Assert-Equal $report.preRepair.strictPass $false `
        "$($Case.caseId) strict preflight"
    Assert-Equal $report.admission.productionAllowed $false `
        "$($Case.caseId) production admission"
    Assert-Equal $report.productionOutputWritten $false `
        "$($Case.caseId) production output"
    Assert-True (@($report.admission.blockerCodes).Count -gt 0) `
        "$($Case.caseId) 缺少稳定 blocker code"
    Assert-True (-not (Test-Path -LiteralPath $configJson.output.packageDir)) `
        "$($Case.caseId) blocked case 不得写 package"

    return [ordered]@{
        caseId = $Case.caseId
        modelFamily = $Case.modelFamily
        modelPath = $Case.modelPath.Replace('\', '/')
        modelSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $modelPath).Hash.ToLowerInvariant()
        inputFormat = $Case.inputFormat
        pipelineMode = "global_surface_shell"
        productionProfileId = "strict_preflight_blocked_disclosure"
        dpiX = 600
        dpiY = 600
        pixelSizeXmm = 25.4 / 600.0
        pixelSizeYmm = 25.4 / 600.0
        widthPoint = "minimum"
        requestedWidthMm = 0.4
        effectiveWidthMm = $null
        allTextureThresholdMm = $null
        preflightState = $report.status
        admissionState = $report.admission.status
        blockingCodes = @($report.admission.blockerCodes)
        productionOutputWritten = $false
        packagePath = $null
        manifestSchema = $null
        layerCount = 0
        tiffHashProjection = $null
        ripStrictPass = $false
        textureSurfacePixels = 0
        modelFillPixels = 0
        supportPixels = 0
        whitePixels = 0
        varnishPixels = 0
        overlapPixels = 0
        unassignedPixels = 0
        previewLayerIndexAlignmentPass = $false
        previewPhysicalAspectPass = $false
        coreMs = 0.0
        composeMs = 0.0
        tiffSaveMs = 0.0
        previewReportSaveMs = 0.0
        totalMs = [double]$run.wallClockMs
        peakWorkingSetBytes = [uint64]$run.peakWorkingSetBytes
        fallbackApplied = $false
        result = "BLOCKED_EXPECTED"
    }
}

function Assert-AssetIdentity
{
    param($Asset, [string]$RepositoryRoot)

    $path = Resolve-RepositoryPath $RepositoryRoot $Asset.path
    Assert-True (Test-Path -LiteralPath $path) `
        "固定资产不存在：$($Asset.path)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
    Assert-Equal $actual.ToUpperInvariant() $Asset.sha256 `
        "固定资产 hash 漂移：$($Asset.path)"
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
        mesh_repair_preflight `
        global_surface_shell_production_pipeline_unit_tests `
        rgbwsv_production_package_writer_unit_tests `
        material_closure_report_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-10B Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests|material_closure_report_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-10B 定向单测失败，退出码=$LASTEXITCODE"
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$preflightExe = Resolve-Executable $resolvedBuildDir $Config `
    "mesh_repair_preflight"

$assets = @(
    [pscustomobject]@{
        path = "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj"
        sha256 = "4F2012E7D584C7D8F4E3A4467D0AF112216F93C222046F61A987880AF8820DDC"
    },
    [pscustomobject]@{
        path = "model/obj/yecan/3.obj"
        sha256 = "A3A421005112292A71F49BED5734CE186C2B97A1379AA50E6DF8BE1A6914363D"
    },
    [pscustomobject]@{
        path = "samples/models/3mf/texture2d_checker_cube.3mf"
        sha256 = "D7EC399818C5A1B9BDF4B5A986CA304F4113256EC0C908F951E4A308445F2C57"
    },
    [pscustomobject]@{
        path = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj"
        sha256 = "5C3F2741297E687BC3E9CE34A2BF3234BA751DEDEDF09FAAC0A36E81C8F83088"
    },
    [pscustomobject]@{
        path = "model/obj/meigui_fudiao/04.obj"
        sha256 = "5D8AFFD74C54A234084CF12ED20049B75D8032E996A306C5E9CB9460CF54D70C"
    },
    [pscustomobject]@{
        path = "model/obj/titian_fudiao/dmz.obj"
        sha256 = "492CECCD47FB97362B4515EBB1CF61D17AF3AE8DA0B75173AC0749EF5E5F5022"
    })
foreach ($asset in $assets)
{
    Assert-AssetIdentity $asset $repositoryRoot
}

$positiveCases = @()
foreach ($family in @(
    [pscustomobject]@{
        id = "xiao_ma"
        path = "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj"
        globalConfig = "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
    },
    [pscustomobject]@{
        id = "yecan"
        path = "model/obj/yecan/3.obj"
        globalConfig = "samples/configs/texture_fill_partition/global_production_yecan_white_fill.json"
    }))
{
    foreach ($point in @(
        [pscustomobject]@{id = "minimum"; layers = 1},
        [pscustomobject]@{id = "intermediate"; layers = 3},
        [pscustomobject]@{id = "all_texture"; layers = 0}))
    {
        $positiveCases += [pscustomobject]@{
            caseId = "legacy_$($family.id)_$($point.id)"
            kind = "legacy"
            modelFamily = $family.id
            modelPath = $family.path
            inputFormat = "obj"
            pipelineMode = "legacy"
            profileId = "legacy_texture_fill_$($point.id)"
            sourceConfig = "samples/configs/material_process/nail_rgb_white_varnish_top2.json"
            widthPoint = $point.id
            legacyLayers = $point.layers
            expectModelFill = $point.id -ne "all_texture"
        }
        $partitionMode = if ($point.id -eq "all_texture")
        {
            "all_texture"
        }
        else
        {
            "partial_shell"
        }
        $widthMm = if ($point.id -eq "intermediate") { 0.8 } else { 0.4 }
        $positiveCases += [pscustomobject]@{
            caseId = "global_$($family.id)_$($point.id)"
            kind = "global"
            modelFamily = $family.id
            modelPath = $family.path
            inputFormat = "obj"
            pipelineMode = "global_surface_shell"
            profileId = "global_surface_shell_$($point.id)_candidate"
            sourceConfig = $family.globalConfig
            widthPoint = $point.id
            partitionMode = $partitionMode
            widthMm = $widthMm
            expectModelFill = $point.id -ne "all_texture"
        }
    }
}
$positiveCases += [pscustomobject]@{
    caseId = "legacy_3mf_texture2d_checker_format_control"
    kind = "legacy"
    modelFamily = "texture2d_checker_cube"
    modelPath = "samples/models/3mf/texture2d_checker_cube.3mf"
    inputFormat = "3mf"
    pipelineMode = "legacy"
    profileId = "legacy_3mf_texture2d_format_control"
    sourceConfig = "samples/configs/3mf/three_mf_texture2d_checker.json"
    widthPoint = "all_texture"
    legacyLayers = 0
    expectModelFill = $false
}
$positiveCases += [pscustomobject]@{
    caseId = "global_3mf_texture2d_checker_format_control"
    kind = "global_3mf"
    modelFamily = "texture2d_checker_cube"
    modelPath = "samples/models/3mf/texture2d_checker_cube.3mf"
    inputFormat = "3mf"
    pipelineMode = "global_surface_shell"
    profileId = "global_surface_shell_3mf_format_control_candidate"
    sourceConfig = "samples/configs/texture_fill_partition/r4_05_clean_3mf_texture2d.json"
    widthPoint = "minimum"
    partitionMode = "partial_shell"
    widthMm = 0.4
    expectModelFill = $true
}

$blockedCases = @(
    [pscustomobject]@{
        caseId = "blocked_aishen_fudiao"
        modelFamily = "aishen_fudiao"
        modelPath = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj"
        inputFormat = "obj"
    },
    [pscustomobject]@{
        caseId = "blocked_meigui_fudiao"
        modelFamily = "meigui_fudiao"
        modelPath = "model/obj/meigui_fudiao/04.obj"
        inputFormat = "obj"
    },
    [pscustomobject]@{
        caseId = "blocked_titian_fudiao"
        modelFamily = "titian_fudiao"
        modelPath = "model/obj/titian_fudiao/dmz.obj"
        inputFormat = "obj"
    })

$caseResults = @()
foreach ($case in $positiveCases)
{
    $caseResults += Invoke-PositiveCase `
        $case $repositoryRoot $runRoot $slicerCli $ripReader
}
foreach ($case in $blockedCases)
{
    $caseResults += Invoke-BlockedCase `
        $case $repositoryRoot $runRoot $preflightExe
}

Assert-Equal $positiveCases.Count 14 "required production case count"
Assert-Equal $blockedCases.Count 3 "required blocked case count"
Assert-Equal @($caseResults | Where-Object { $_.result -eq "PASS" }).Count `
    14 "production PASS count"
Assert-Equal @($caseResults | Where-Object {
        $_.result -eq "BLOCKED_EXPECTED"
    }).Count 3 "blocked expected count"
Assert-Equal @($caseResults | Where-Object { $_.fallbackApplied }).Count `
    0 "fallback count"
$requiredCaseFields = @(
    "caseId", "modelFamily", "modelPath", "modelSha256", "inputFormat",
    "pipelineMode", "productionProfileId", "dpiX", "dpiY",
    "pixelSizeXmm", "pixelSizeYmm", "widthPoint", "requestedWidthMm",
    "effectiveWidthMm", "allTextureThresholdMm", "preflightState",
    "admissionState", "blockingCodes", "productionOutputWritten",
    "packagePath", "manifestSchema", "layerCount", "tiffHashProjection",
    "ripStrictPass", "textureSurfacePixels", "modelFillPixels",
    "supportPixels", "whitePixels", "varnishPixels", "overlapPixels",
    "unassignedPixels", "previewLayerIndexAlignmentPass",
    "previewPhysicalAspectPass", "coreMs", "composeMs", "tiffSaveMs",
    "previewReportSaveMs", "totalMs", "peakWorkingSetBytes",
    "fallbackApplied", "result")
foreach ($caseResult in $caseResults)
{
    foreach ($field in $requiredCaseFields)
    {
        Assert-True ($caseResult.Contains($field)) `
            "$($caseResult.caseId) 缺少 schema 字段：$field"
    }
}

$computerName = [Environment]::MachineName
$osDescription = [Environment]::OSVersion.VersionString
$processorCount = [Environment]::ProcessorCount
$matrix = [ordered]@{
    schema = "slicesoft.stage12e.final_closure_matrix.1"
    generatedAt = (Get-Date).ToString("o")
    buildConfiguration = $Config
    compiler = "MSVC via CMake build-slicesoft/main"
    openVdbEnabled = $false
    referenceMachine = [ordered]@{
        computerName = $computerName
        os = $osDescription
        logicalProcessorCount = $processorCount
        evidenceScope = "current_reference_machine_only"
    }
    cases = $caseResults
    summary = [ordered]@{
        requiredCaseCount = 17
        productionPassCount = 14
        blockedExpectedCount = 3
        failedCount = 0
        fallbackCount = 0
        protocol = "p0.rgbwsv.2"
        channelOrder = @("R", "G", "B", "W", "S", "V")
        overlapDerivation =
            "admitted production material ownership plus exact closure report"
        pass = $true
    }
    residualRisks = @(
        "aishen/meigui/titian complex relief assets remain strict blocked and require repair or rebuild.",
        "Global remains an explicit candidate; this task does not change the Legacy default.",
        "Performance and memory decision is deferred to 12E-10C.",
        "Device build volume, axes and production SLA remain outside this matrix."
    )
    decision = "PASS_12E_10B_READY_FOR_10C"
}

$runSummaryPath = Join-Path $runRoot "final_closure_matrix.json"
$fixedSummaryPath = Join-Path $resolvedOutputRoot "final_closure_matrix.json"
Write-Json $runSummaryPath $matrix
Write-Json $fixedSummaryPath $matrix

Write-Host "12E-10B real OBJ/3MF dual-mode matrix: PASS"
Write-Host "Production PASS: 14; BLOCKED_EXPECTED: 3; fallback: 0"
Write-Host "Matrix: $fixedSummaryPath"
$global:LASTEXITCODE = 0
