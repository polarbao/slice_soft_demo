param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/12e_09d",
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
    New-Item -ItemType Directory -Force -Path (Split-Path $Path -Parent) |
        Out-Null
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

function Get-ChannelPrintPixels
{
    param($Report, [string]$Channel)
    if ($null -ne $Report.totals.printPixels)
    {
        return [uint64]$Report.totals.printPixels.$Channel
    }
    return [uint64]$Report.totals.channelStats.$Channel.printPixels
}

function Get-LayerChannelPrintPixels
{
    param($Layer, [string]$Channel)
    if ($null -ne $Layer.printPixels)
    {
        return [uint64]$Layer.printPixels.$Channel
    }
    return [uint64]$Layer.channelStats.$Channel.printPixels
}

function Get-ActiveRgbLayerCount
{
    param($Report)
    $layers = if ($null -ne $Report.layerStats)
    {
        @($Report.layerStats)
    }
    else
    {
        @($Report.layers)
    }
    return @($layers | Where-Object {
        (Get-LayerChannelPrintPixels $_ "R") -gt 0 `
            -or (Get-LayerChannelPrintPixels $_ "G") -gt 0 `
            -or (Get-LayerChannelPrintPixels $_ "B") -gt 0
    }).Count
}

function Assert-ProtocolAndRip
{
    param([string]$CaseId, [string]$PackagePath, [string]$RipReader)
    $manifest = Read-Json (Join-Path $PackagePath "manifest.json")
    Assert-Equal $manifest.schema "p0.rgbwsv.2" "$CaseId schema"
    Assert-Equal $manifest.tiff.bitDepth 8 "$CaseId bit depth"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$CaseId channel order"
    Assert-Equal $manifest.tiff.polarity "black_is_print" "$CaseId polarity"
    Assert-Equal $manifest.tiff.printValue 0 "$CaseId print value"
    Assert-Equal $manifest.tiff.emptyValue 255 "$CaseId empty value"
    $ripOutput = @(
        & $RipReader --package $PackagePath --summary 2>&1 |
            ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0)
    {
        throw "$CaseId RIP strict 失败：$($ripOutput -join [Environment]::NewLine)"
    }
    return $manifest
}

function Invoke-SliceCase
{
    param(
        [string]$CaseId,
        $ConfigJson,
        [string]$SourceConfigPath,
        [string]$ResolvedOutputRoot,
        [string]$SlicerCli,
        [string]$RipReader)

    $caseRoot = Join-Path $ResolvedOutputRoot $CaseId
    $packagePath = Join-Path $caseRoot "package"
    Assert-PathUnderRoot $ResolvedOutputRoot $packagePath "$CaseId package"
    if (Test-Path -LiteralPath $packagePath)
    {
        Remove-Item -LiteralPath $packagePath -Recurse -Force
    }

    $sourceDirectory = Split-Path -Parent $SourceConfigPath
    $modelPath = if ([System.IO.Path]::IsPathRooted($ConfigJson.input.modelPath))
    {
        [System.IO.Path]::GetFullPath($ConfigJson.input.modelPath)
    }
    else
    {
        [System.IO.Path]::GetFullPath(
            (Join-Path $sourceDirectory $ConfigJson.input.modelPath))
    }
    Assert-True (Test-Path -LiteralPath $modelPath) `
        "$CaseId 模型不存在：$modelPath"
    $ConfigJson.input.modelPath = $modelPath.Replace('\', '/')
    $ConfigJson.output.packageDir =
        ([System.IO.Path]::GetFullPath($packagePath)).Replace('\', '/')
    $ConfigJson.preview.enabled = $false

    $configPath = Join-Path $caseRoot "config.json"
    Write-Json $configPath $ConfigJson
    $output = @(
        & $SlicerCli --config $configPath 2>&1 |
            ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0)
    {
        throw "$CaseId 切片失败：$($output -join [Environment]::NewLine)"
    }
    $manifest = Assert-ProtocolAndRip $CaseId $packagePath $RipReader
    $report = Read-Json (Join-Path $packagePath "reports/slice_report.json")
    return [ordered]@{
        caseId = $CaseId
        configPath = $configPath.Replace('\', '/')
        packagePath = $packagePath.Replace('\', '/')
        manifest = $manifest
        report = $report
        printPixels = [ordered]@{
            R = Get-ChannelPrintPixels $report "R"
            G = Get-ChannelPrintPixels $report "G"
            B = Get-ChannelPrintPixels $report "B"
            W = Get-ChannelPrintPixels $report "W"
            S = Get-ChannelPrintPixels $report "S"
            V = Get-ChannelPrintPixels $report "V"
        }
        activeRgbLayers = Get-ActiveRgbLayerCount $report
        ripStrict = "pass"
    }
}

function New-SingleMaterialConfig
{
    param($Source, [ValidateSet("white", "varnish")] [string]$Material)
    $white = $Material -eq "white"
    Set-Property $Source.modelMaterial "materialChannel" $(if ($white) { "W" } else { "V" })
    Set-Property $Source.modelMaterial "applyMode" "solid_volume"
    Set-Property $Source.modelMaterial "rgb" @(255, 255, 255)
    Set-Property $Source.modelMaterial "whiteValue" $(if ($white) { 0 } else { 255 })
    Set-Property $Source.modelMaterial "varnishValue" $(if ($white) { 255 } else { 0 })

    Set-Property $Source "modelFill" ([pscustomobject][ordered]@{
        enabled = $true
        material = $Material
        scope = "all_model"
        value = 0
        emptyAllowedInProduction = $false
        legacyRgbFallback = $false
    })
    $Source.materialProcessProfile.name = "single_material_relief"
    $Source.materialProcessProfile.target = "single_material_relief"
    $Source.materialProcessProfile.rgb.enabled = $false
    $Source.materialProcessProfile.rgb.source = "modelMaterial"
    $Source.materialProcessProfile.white = [pscustomobject][ordered]@{
        enabled = $white
        mode = $(if ($white) { "all_model" } else { "disabled" })
        coverage = $(if ($white) { "all_model" } else { "model_surface" })
        value = 0
        expandPx = 0
        shrinkPx = 0
    }
    $Source.materialProcessProfile.varnish = [pscustomobject][ordered]@{
        enabled = (-not $white)
        mode = $(if ($white) { "disabled" } else { "all_model" })
        topLayers = 1
        value = 0
        coverage = $(if ($white) { "model_surface" } else { "all_model" })
    }
    $Source.materialProcessProfile.validation.requireRgbPixels = $false
    $Source.materialProcessProfile.validation.requireWhitePixels = $white
    $Source.materialProcessProfile.validation.requireVarnishPixels = (-not $white)
    $Source.materialProcessProfile.validation.requireSupportPixels = $true
    $Source.preview.channels = @($(if ($white) { "white" } else { "varnish" }), "support")
    return $Source
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Resolve-RepositoryPath $repositoryRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repositoryRoot $OutputRoot
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        slicer_cli `
        rip_reader_test `
        slicer_debug_ui `
        production_texture_settings_contract_unit_tests `
        production_texture_settings_model_unit_tests `
        single_material_relief_resolver_unit_tests `
        production_effective_config_unit_tests `
        global_surface_shell_production_pipeline_unit_tests `
        rgbwsv_production_package_writer_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09D Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(production_texture_settings_(contract|model)_unit_tests|single_material_relief_resolver_unit_tests|production_effective_config_unit_tests|global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-09D 定向单测失败，退出码=$LASTEXITCODE"
}

$ui = Resolve-Executable $resolvedBuildDir $Config "slicer_debug_ui"
foreach ($uiArguments in @(
    @("--self-test"),
    @("--ui-smoke-test", "--case", "production-texture-controls", "--repo-root", $repositoryRoot),
    @("--ui-smoke-test", "--case", "diagnostic-settings-controls", "--repo-root", $repositoryRoot)))
{
    & $ui @uiArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09D UI Smoke 失败：$($uiArguments -join ' ')"
    }
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"

$legacySourcePath = Resolve-RepositoryPath $repositoryRoot `
    "samples/configs/golden/material_process_top2_fixture.json"
$legacyResults = @()
foreach ($layers in @(1, 3, 10))
{
    $configJson = Read-Json $legacySourcePath
    $configJson.texture.applyMode = "top_surface_band"
    Set-Property $configJson.texture "topSurfaceLayers" $layers
    Set-Property $configJson.texture "nonSurfaceRgbPolicy" "empty"
    $result = Invoke-SliceCase `
        "legacy_top_$layers" $configJson $legacySourcePath `
        $resolvedOutputRoot $slicerCli $ripReader
    $result.effectiveThicknessMm =
        $layers * [double]$configJson.output.layerThicknessMm
    $legacyResults += $result
}
Assert-True ($legacyResults[0].activeRgbLayers -le $legacyResults[1].activeRgbLayers) `
    "Legacy 1 -> 3 RGB 生效层数必须单调"
Assert-True ($legacyResults[1].activeRgbLayers -le $legacyResults[2].activeRgbLayers) `
    "Legacy 3 -> 10 RGB 生效层数必须单调"
Assert-Equal $legacyResults[0].printPixels.W $legacyResults[1].printPixels.W `
    "Legacy W 语义不得随纹理层数改变"
Assert-Equal $legacyResults[1].printPixels.W $legacyResults[2].printPixels.W `
    "Legacy W 语义不得随纹理层数改变"
Assert-Equal $legacyResults[0].printPixels.S $legacyResults[1].printPixels.S `
    "Legacy S 语义不得随纹理层数改变"
Assert-Equal $legacyResults[1].printPixels.S $legacyResults[2].printPixels.S `
    "Legacy S 语义不得随纹理层数改变"

$globalSourcePath = Resolve-RepositoryPath $repositoryRoot `
    "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
$globalResults = @()
foreach ($case in @(
    [pscustomobject]@{id = "global_min"; mode = "partial_shell"; width = 0.40},
    [pscustomobject]@{id = "global_mid"; mode = "partial_shell"; width = 0.80},
    [pscustomobject]@{id = "global_all"; mode = "all_texture"; width = 0.40}))
{
    $configJson = Read-Json $globalSourcePath
    Set-Property $configJson.texture.surfaceShell "mode" $case.mode
    $configJson.texture.surfaceShell.widthMm = $case.width
    $result = Invoke-SliceCase `
        $case.id $configJson $globalSourcePath $resolvedOutputRoot `
        $slicerCli $ripReader
    $settings = $result.report.productionSettings
    Assert-Equal $settings.partitionMode $case.mode "$($case.id) partition mode"
    Assert-Equal $settings.backend "legacy_cpu_global_distance" `
        "$($case.id) backend"
    Assert-True ([double]$settings.effectiveWidthMm -gt 0.0) `
        "$($case.id) effective width"
    $globalResults += $result
}
Assert-True (
    [uint64]$globalResults[0].report.productionSettings.modelFillVoxels -ge
        [uint64]$globalResults[1].report.productionSettings.modelFillVoxels) `
    "Global 宽度增加时语义 Model Fill 不得增加"
Assert-Equal $globalResults[2].report.productionSettings.modelFillVoxels 0 `
    "Global all_texture 必须移除语义 Model Fill"
Assert-True (($globalResults[2].printPixels.R + $globalResults[2].printPixels.G + $globalResults[2].printPixels.B) -gt 0) `
    "Global all_texture 必须保留 RGB"

$singleSourcePath = Resolve-RepositoryPath $repositoryRoot `
    "samples/configs/relief/relief_nail_white_support.json"
$singleResults = @()
foreach ($material in @("white", "varnish"))
{
    $configJson = New-SingleMaterialConfig `
        (Read-Json $singleSourcePath) $material
    $singleResults += Invoke-SliceCase `
        "single_$material" $configJson $singleSourcePath `
        $resolvedOutputRoot $slicerCli $ripReader
}
Assert-True ($singleResults[0].printPixels.W -gt 0) "single W 缺少白墨"
Assert-Equal $singleResults[0].printPixels.V 0 "single W 不应输出光油"
Assert-True ($singleResults[1].printPixels.V -gt 0) "single V 缺少光油"
Assert-Equal $singleResults[1].printPixels.W 0 "single V 不应输出白墨"
Assert-True ($singleResults[0].printPixels.S -gt 0) "single W 缺少支撑"
Assert-Equal $singleResults[0].printPixels.S $singleResults[1].printPixels.S `
    "single W/V 支撑必须一致"
Assert-Equal $singleResults[0].manifest.grid.layerCount `
    $singleResults[1].manifest.grid.layerCount "single W/V 层数必须一致"

function Convert-ResultForSummary
{
    param($Result)
    return [ordered]@{
        caseId = $Result.caseId
        configPath = $Result.configPath
        packagePath = $Result.packagePath
        layerCount = [int]$Result.manifest.grid.layerCount
        activeRgbLayers = [int]$Result.activeRgbLayers
        printPixels = $Result.printPixels
        effectiveThicknessMm = $Result.effectiveThicknessMm
        productionSettings = $Result.report.productionSettings
        ripStrict = $Result.ripStrict
        pass = $true
    }
}

$summary = [ordered]@{
    schema = "slicesoft.production_texture_material_matrix.12e_09d.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-09D-06"
    buildType = $Config
    protocol = [ordered]@{
        schema = "p0.rgbwsv.2"
        channelOrder = @("R", "G", "B", "W", "S", "V")
        bitDepth = 8
        polarity = "black_is_print"
    }
    legacy = @($legacyResults | ForEach-Object { Convert-ResultForSummary $_ })
    global = @($globalResults | ForEach-Object { Convert-ResultForSummary $_ })
    singleMaterial = @($singleResults | ForEach-Object { Convert-ResultForSummary $_ })
    result = [ordered]@{
        legacyMonotonic = $true
        globalAllTexture = $true
        singleMaterialWv = $true
        ripStrict = $true
        pass = $true
    }
    boundaries = @(
        "Production parameters and diagnostic parameters remain separate.",
        "Legacy topSurfaceLayers and Global widthMm/mode are not mixed.",
        "12G-TCWS remains frozen and is not implemented by this matrix.")
}
$summaryPath = Join-Path $resolvedOutputRoot `
    "production_texture_material_matrix_summary.json"
Write-Json $summaryPath $summary

Write-Host "12E-09D production texture/material matrix: PASS"
Write-Host "Legacy 1/3/10, Global min/mid/all_texture, single W/V: PASS"
Write-Host "RIP strict and fixed RGBWSV protocol: PASS"
Write-Host "Summary: $summaryPath"
$global:LASTEXITCODE = 0
