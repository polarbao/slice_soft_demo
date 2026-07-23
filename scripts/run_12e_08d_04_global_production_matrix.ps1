param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot =
        "output/benchmarks/12e_08d_04_global_production",
    [switch]$SkipBuild,
    [switch]$SkipLegacyRegression
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
        [string]$RepositoryRoot,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $Path))
}

function Resolve-Executable
{
    param(
        [string]$BuildPath,
        [string]$BuildConfig,
        [string]$Name
    )

    $candidates = @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath "$Name.exe")
    )
    foreach ($candidate in $candidates)
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

function Assert-ProductionPackage
{
    param(
        [string]$CaseId,
        [string]$PackagePath,
        [string]$RipReader
    )

    $manifestPath = Join-Path $PackagePath "manifest.json"
    $sliceReportPath = Join-Path $PackagePath "reports/slice_report.json"
    Assert-True (Test-Path -LiteralPath $manifestPath) `
        "$CaseId manifest 不存在"
    Assert-True (Test-Path -LiteralPath $sliceReportPath) `
        "$CaseId slice_report 不存在"

    $manifest = Read-Json $manifestPath
    $sliceReport = Read-Json $sliceReportPath
    Assert-Equal $manifest.schema "p0.rgbwsv.2" "$CaseId manifest schema"
    Assert-Equal $manifest.requestedPipelineMode `
        "global_surface_shell" "$CaseId requested mode"
    Assert-Equal $manifest.effectivePipelineMode `
        "global_surface_shell" "$CaseId effective mode"
    Assert-Equal $manifest.productionAcceptance "admitted" `
        "$CaseId production acceptance"
    Assert-Equal $manifest.productionOutputWritten $true `
        "$CaseId production output"
    Assert-Equal $manifest.fallbackApplied $false "$CaseId fallback"
    Assert-Equal $manifest.tiff.bitDepth 8 "$CaseId bit depth"
    Assert-Equal $manifest.tiff.channelCount 6 "$CaseId channel count"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$CaseId channel order"
    Assert-Equal $manifest.tiff.polarity "black_is_print" `
        "$CaseId polarity"
    Assert-Equal $manifest.tiff.printValue 0 "$CaseId print value"
    Assert-Equal $manifest.tiff.emptyValue 255 "$CaseId empty value"
    Assert-Equal @($manifest.layers).Count $manifest.grid.layerCount `
        "$CaseId layer list"
    Assert-Equal $sliceReport.productionTiffLayerCount `
        $manifest.grid.layerCount "$CaseId production TIFF count"
    Assert-True ([uint64]$sliceReport.totals.printPixels.R -gt 0) `
        "$CaseId 缺少 R 打印像素"
    Assert-True ([uint64]$sliceReport.totals.printPixels.G -gt 0) `
        "$CaseId 缺少 G 打印像素"
    Assert-True ([uint64]$sliceReport.totals.printPixels.B -gt 0) `
        "$CaseId 缺少 B 打印像素"
    Assert-True ([uint64]$sliceReport.totals.printPixels.W -gt 0) `
        "$CaseId 缺少 W Model Fill 像素"
    Assert-Equal ([uint64]$sliceReport.totals.printPixels.S) 0 `
        "$CaseId 受限 Profile 不应输出支撑"
    Assert-Equal ([uint64]$sliceReport.totals.printPixels.V) 0 `
        "$CaseId 受限 Profile 不应输出光油"

    foreach ($layer in $manifest.layers)
    {
        Assert-True `
            (Test-Path -LiteralPath (Join-Path $PackagePath $layer.path)) `
            "$CaseId 缺少 TIFF：$($layer.path)"
    }

    $ripOutput = @(
        & $RipReader --package $PackagePath --summary 2>&1 |
            ForEach-Object { $_.ToString() }
    )
    if ($LASTEXITCODE -ne 0)
    {
        throw "$CaseId RIP Reader 失败：$($ripOutput -join [Environment]::NewLine)"
    }

    return [ordered]@{
        packagePath = $PackagePath.Replace("\", "/")
        widthPx = [int]$manifest.grid.widthPx
        heightPx = [int]$manifest.grid.heightPx
        layerCount = [int]$manifest.grid.layerCount
        printPixels = [ordered]@{
            R = [uint64]$sliceReport.totals.printPixels.R
            G = [uint64]$sliceReport.totals.printPixels.G
            B = [uint64]$sliceReport.totals.printPixels.B
            W = [uint64]$sliceReport.totals.printPixels.W
            S = [uint64]$sliceReport.totals.printPixels.S
            V = [uint64]$sliceReport.totals.printPixels.V
        }
        ripReader = "pass"
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $BuildDir
$resolvedOutputRoot =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $OutputRoot
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        slicer_cli `
        rip_reader_test `
        global_surface_shell_production_pipeline_unit_tests `
        rgbwsv_production_package_writer_unit_tests `
        slice_pipeline_router_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-08D-04 Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests|slice_pipeline_router_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-08D-04 定向单测失败，退出码=$LASTEXITCODE"
}

$slicerCli = Resolve-Executable `
    -BuildPath $resolvedBuildDir -BuildConfig $Config -Name "slicer_cli"
$ripReader = Resolve-Executable `
    -BuildPath $resolvedBuildDir -BuildConfig $Config -Name "rip_reader_test"

$cases = @(
    [ordered]@{
        caseId = "xiao_ma"
        modelFamily = "xiao_ma_wu_yu_new"
        config = "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"
    },
    [ordered]@{
        caseId = "yecan"
        modelFamily = "yecan"
        config = "samples/configs/texture_fill_partition/global_production_yecan_white_fill.json"
    }
)

$caseResults = @()
foreach ($case in $cases)
{
    $configPath =
        Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $case.config
    $configJson = Read-Json $configPath
    $packagePath = Resolve-RepositoryPath `
        -RepositoryRoot $repoRoot -Path $configJson.output.packageDir

    $outputLines = @(
        & $slicerCli --config $configPath 2>&1 |
            ForEach-Object { $_.ToString() }
    )
    if ($LASTEXITCODE -ne 0)
    {
        throw "$($case.caseId) Global production 失败：$($outputLines -join [Environment]::NewLine)"
    }
    $timing = Parse-Timing $outputLines
    Assert-Equal $timing.engine "global_surface_shell" `
        "$($case.caseId) timing engine"
    $package = Assert-ProductionPackage `
        -CaseId $case.caseId `
        -PackagePath $packagePath `
        -RipReader $ripReader
    $caseResults += [ordered]@{
        caseId = $case.caseId
        modelFamily = $case.modelFamily
        configPath = $case.config
        profileTarget = $configJson.materialProcessProfile.target
        effectivePipelineMode = $timing.engine
        timingsMs = [ordered]@{
            modelLoad = [double]$timing.modelLoadMs
            sliceProcessing = [double]$timing.sliceProcessingMs
            outputWrite = [double]$timing.outputWriteMs
            total = [double]$timing.totalMs
        }
        package = $package
        pass = $true
    }
}

$negativeRoot = Join-Path $resolvedOutputRoot "negative_support_enabled"
$negativePackage = Join-Path $negativeRoot "package"
$negativeConfigPath = Join-Path $negativeRoot "config.json"
New-Item -ItemType Directory -Path $negativeRoot -Force | Out-Null
if (Test-Path -LiteralPath $negativePackage)
{
    Remove-Item -LiteralPath $negativePackage -Recurse -Force
}
$negativeSourceConfigPath = Resolve-RepositoryPath `
    -RepositoryRoot $repoRoot `
    -Path $cases[0].config
$negativeConfig = Read-Json $negativeSourceConfigPath
$negativeConfig.output.packageDir = $negativePackage.Replace("\", "/")
$negativeConfig.input.modelPath = [System.IO.Path]::GetFullPath(
    (Join-Path `
        (Split-Path -Parent $negativeSourceConfigPath) `
        $negativeConfig.input.modelPath)).Replace("\", "/")
$negativeConfig.support.enabled = $true
Write-Utf8NoBom `
    -Path $negativeConfigPath `
    -Content ($negativeConfig | ConvertTo-Json -Depth 100)
$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try
{
    $negativeOutput = @(
        & $slicerCli --config $negativeConfigPath 2>&1 |
            ForEach-Object { $_.ToString() }
    )
    $negativeExitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $savedErrorActionPreference
}
Assert-True ($negativeExitCode -ne 0) `
    "support-enabled 负向 Profile 必须失败"
Assert-True `
    (($negativeOutput -join "`n") -like "*E_12E_PIPELINE_GLOBAL_NOT_ADMITTED*") `
    "support-enabled 负向 Profile 缺少稳定错误码"
Assert-True (-not (Test-Path -LiteralPath $negativePackage)) `
    "support-enabled 负向 Profile 不得写 production package"

$legacyRegression = "skipped"
if (-not $SkipLegacyRegression)
{
    & (Join-Path $repoRoot "scripts/run_material_closure_tests.ps1") `
        -BuildDir $BuildDir `
        -Config $Config `
        -Mode RepairDisabled
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-08D-04 legacy TIFF/RIP 回归失败，退出码=$LASTEXITCODE"
    }
    $legacyRegression = "passed"
}

$summary = [ordered]@{
    schema = "slicesoft.global_production_matrix.12e_08d.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08D-04"
    buildType = $Config
    profile = [ordered]@{
        target = "global_surface_shell_restricted_candidate"
        modelFillMaterial = "white"
        support = "disabled"
        varnish = "disabled"
        layerThicknessMm = 0.2
    }
    cases = $caseResults
    negativeCases = @(
        [ordered]@{
            caseId = "support_enabled_blocked"
            errorCode = "E_12E_PIPELINE_GLOBAL_NOT_ADMITTED"
            productionOutputWritten = $false
            pass = $true
        }
    )
    legacyRegression = $legacyRegression
    result = [ordered]@{
        restrictedProfileProductionAdmission = "go"
        ordinaryGlobalProductionParity = "no_go"
        ordinaryGlobalBlockers = @(
            "support_generation_not_integrated",
            "surface_and_outer_varnish_not_integrated",
            "final_0_01_mm_release_matrix_not_executed"
        )
        fallbackApplied = $false
        pass = $true
    }
}
$summaryPath = Join-Path $resolvedOutputRoot "global_production_matrix_summary.json"
Write-Utf8NoBom `
    -Path $summaryPath `
    -Content ($summary | ConvertTo-Json -Depth 100)

Write-Host "12E-08D-04 restricted Global production matrix: PASS"
Write-Host "Restricted Profile production admission: GO"
Write-Host "Ordinary Global feature parity: NO-GO"
Write-Host "Summary: $summaryPath"
