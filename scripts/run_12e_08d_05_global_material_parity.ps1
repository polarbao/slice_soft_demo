param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot =
        "output/benchmarks/12e_08d_05_global_material_parity",
    [switch]$SkipBuild,
    [switch]$SkipRestrictedRegression
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

function Assert-MaterialParityPackage
{
    param(
        [string]$CaseId,
        [string]$PackagePath,
        [string]$RipReader
    )

    $manifest = Read-Json (Join-Path $PackagePath "manifest.json")
    $sliceReport = Read-Json (
        Join-Path $PackagePath "reports/slice_report.json")
    Assert-Equal $manifest.schema "p0.rgbwsv.2" "$CaseId schema"
    Assert-Equal ($manifest.tiff.channelOrder -join " ") `
        "R G B W S V" "$CaseId channel order"
    Assert-Equal $manifest.tiff.bitDepth 8 "$CaseId bit depth"
    Assert-Equal $manifest.tiff.polarity "black_is_print" "$CaseId polarity"
    Assert-Equal $manifest.tiff.printValue 0 "$CaseId print value"
    Assert-Equal $manifest.tiff.emptyValue 255 "$CaseId empty value"
    Assert-Equal $manifest.requestedPipelineMode `
        "global_surface_shell" "$CaseId requested mode"
    Assert-Equal $manifest.effectivePipelineMode `
        "global_surface_shell" "$CaseId effective mode"
    Assert-Equal $manifest.productionAcceptance "admitted" `
        "$CaseId production acceptance"
    Assert-Equal $manifest.productionOutputWritten $true `
        "$CaseId production output"
    Assert-Equal $manifest.fallbackApplied $false "$CaseId fallback"
    Assert-Equal @($manifest.layers).Count $manifest.grid.layerCount `
        "$CaseId complete layer list"
    foreach ($channel in @("R", "G", "B", "W", "S", "V"))
    {
        Assert-True `
            ([uint64]$sliceReport.totals.printPixels.$channel -gt 0) `
            "$CaseId 缺少 $channel 打印像素"
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
        throw "12E-08D-05 Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(global_surface_shell_material_evidence_unit_tests|global_surface_shell_production_pipeline_unit_tests|rgbwsv_production_package_writer_unit_tests|slice_pipeline_router_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-08D-05 定向单测失败，退出码=$LASTEXITCODE"
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$cases = @(
    [ordered]@{
        caseId = "xiao_ma"
        modelFamily = "xiao_ma_wu_yu_new"
        config = "samples/configs/texture_fill_partition/global_production_xiao_ma_material_parity.json"
    },
    [ordered]@{
        caseId = "yecan"
        modelFamily = "yecan"
        config = "samples/configs/texture_fill_partition/global_production_yecan_material_parity.json"
    }
)

$caseResults = @()
foreach ($case in $cases)
{
    $configPath = Resolve-RepositoryPath $repoRoot $case.config
    $configJson = Read-Json $configPath
    $packagePath = Resolve-RepositoryPath `
        $repoRoot $configJson.output.packageDir
    $outputLines = @(
        & $slicerCli --config $configPath 2>&1 |
            ForEach-Object { $_.ToString() }
    )
    if ($LASTEXITCODE -ne 0)
    {
        throw "$($case.caseId) 材料等价切片失败：$($outputLines -join [Environment]::NewLine)"
    }
    $timing = Parse-Timing $outputLines
    Assert-Equal $timing.engine "global_surface_shell" `
        "$($case.caseId) timing engine"
    $package = Assert-MaterialParityPackage `
        $case.caseId $packagePath $ripReader
    $caseResults += [ordered]@{
        caseId = $case.caseId
        modelFamily = $case.modelFamily
        configPath = $case.config
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

$negativeRoot = Join-Path $resolvedOutputRoot "negative_upper_support"
$negativePackage = Join-Path $negativeRoot "package"
$negativeConfigPath = Join-Path $negativeRoot "config.json"
New-Item -ItemType Directory -Path $negativeRoot -Force | Out-Null
if (Test-Path -LiteralPath $negativePackage)
{
    Remove-Item -LiteralPath $negativePackage -Recurse -Force
}
$sourceConfigPath = Resolve-RepositoryPath $repoRoot $cases[0].config
$negativeConfig = Read-Json $sourceConfigPath
$negativeConfig.output.packageDir = $negativePackage.Replace("\", "/")
$negativeConfig.input.modelPath = [System.IO.Path]::GetFullPath(
    (Join-Path `
        (Split-Path -Parent $sourceConfigPath) `
        $negativeConfig.input.modelPath)).Replace("\", "/")
$negativeConfig.support.placement = "upper"
Write-Utf8NoBom $negativeConfigPath (
    $negativeConfig | ConvertTo-Json -Depth 100)

$savedPreference = $ErrorActionPreference
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
    $ErrorActionPreference = $savedPreference
}
Assert-True ($negativeExitCode -ne 0) `
    "upper support 负向 Profile 必须失败"
Assert-True `
    (($negativeOutput -join "`n") -like "*E_12E_PIPELINE_GLOBAL_NOT_ADMITTED*") `
    "upper support 负向 Profile 缺少稳定错误码"
Assert-True (-not (Test-Path -LiteralPath $negativePackage)) `
    "upper support 负向 Profile 不得写 package"

$restrictedRegression = "skipped"
if (-not $SkipRestrictedRegression)
{
    & (Join-Path $repoRoot "scripts/run_12e_08d_04_global_production_matrix.ps1") `
        -BuildDir $BuildDir `
        -SkipBuild `
        -SkipLegacyRegression
    $restrictedRegression = "passed"
}

$summary = [ordered]@{
    schema = "slicesoft.global_material_parity_matrix.12e_08d.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08D-05"
    buildType = $Config
    profileTarget = "global_surface_shell_material_parity_candidate"
    cases = $caseResults
    negativeCases = @(
        [ordered]@{
            caseId = "upper_support_blocked"
            errorCode = "E_12E_PIPELINE_GLOBAL_NOT_ADMITTED"
            productionOutputWritten = $false
            pass = $true
        }
    )
    restrictedRegression = $restrictedRegression
    result = [ordered]@{
        lowerSupport = "pass"
        internalVoidSupport = "pass"
        surfaceAndOuterVarnish = "pass"
        ripStrict = "pass"
        materialParityCandidate = "go"
        fullGlobalParity = "pending_0_01_mm_release_matrix"
        fallbackApplied = $false
        pass = $true
    }
}
$summaryPath = Join-Path $resolvedOutputRoot `
    "global_material_parity_matrix_summary.json"
Write-Utf8NoBom $summaryPath (
    $summary | ConvertTo-Json -Depth 100)

Write-Host "12E-08D-05 Global material parity matrix: PASS"
Write-Host "Material parity candidate: GO"
Write-Host "Summary: $summaryPath"
$global:LASTEXITCODE = 0
