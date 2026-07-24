param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot =
        "output/benchmarks/12e_09b_06_production_ui",
    [switch]$SkipBuild,
    [switch]$ReuseMatrix
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
        "$Description 不属于当前 package：$candidatePath"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Resolve-RepositoryPath $repoRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repoRoot $OutputRoot

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        slicer_cli `
        slicer_debug_ui `
        rip_reader_test `
        production_package_result_unit_tests `
        production_slice_route_process_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09B-06 Release build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(production_package_result_unit_tests|production_slice_route_process_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "12E-09B-06 生产 UI 定向单测失败，退出码=$LASTEXITCODE"
}

$ui = Resolve-Executable $resolvedBuildDir $Config "slicer_debug_ui"
foreach ($uiCase in @(
    [ordered]@{name = "self-test"; arguments = "--self-test"},
    [ordered]@{
        name = "production-mode-selector"
        arguments = "--ui-smoke-test --case production-mode-selector"
    },
    [ordered]@{
        name = "slice-progress-timing"
        arguments = "--ui-smoke-test --case slice-progress-timing"
    },
    [ordered]@{
        name = "model-preflight-one-click-gate"
        arguments = "--ui-smoke-test --case model-preflight-one-click-gate"
    }))
{
    & $ui ($uiCase.arguments -split " ")
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09B-06 UI gate 失败：$($uiCase.name)"
    }
}

if (-not $ReuseMatrix)
{
    & (Join-Path $PSScriptRoot "run_12e_08d_06_release_matrix.ps1") `
        -BuildDir $BuildDir `
        -Config $Config `
        -OutputRoot $OutputRoot `
        -SkipBuild
    if ($LASTEXITCODE -ne 0)
    {
        throw "12E-09B-06 真实模型矩阵失败，退出码=$LASTEXITCODE"
    }
}

$matrixSummaryPath =
    Join-Path $resolvedOutputRoot "release_matrix_summary.json"
$matrixSummary = Read-Json $matrixSummaryPath
Assert-True $matrixSummary.result.pass "真实模型矩阵未通过"
Assert-Equal @($matrixSummary.cases).Count 6 "真实模型矩阵 case 数量"
Assert-Equal $matrixSummary.result.fallbackApplied $false "矩阵 fallback"

$packageEvidence = @()
foreach ($case in @($matrixSummary.cases))
{
    $packagePath = [string]$case.packagePath
    if ([string]::IsNullOrWhiteSpace($packagePath))
    {
        $packagePath = Join-Path $resolvedOutputRoot "$($case.caseId)/package"
    }
    $packagePath = [System.IO.Path]::GetFullPath($packagePath)
    $manifestPath = Join-Path $packagePath "manifest.json"
    $sliceReportPath = Join-Path $packagePath "reports/slice_report.json"
    $previewReportPath = Join-Path $packagePath "reports/preview_report.json"
    $manifest = Read-Json $manifestPath
    $sliceReport = Read-Json $sliceReportPath
    $previewReport = Read-Json $previewReportPath

    Assert-Equal $manifest.schema "p0.rgbwsv.2" "$($case.caseId) schema"
    Assert-Equal $manifest.requestedPipelineMode $case.pipelineMode `
        "$($case.caseId) requested mode"
    Assert-Equal $manifest.effectivePipelineMode $case.pipelineMode `
        "$($case.caseId) effective mode"
    Assert-Equal $manifest.productionOutputWritten $true `
        "$($case.caseId) production output"
    Assert-Equal $manifest.fallbackApplied $false "$($case.caseId) fallback"
    Assert-Equal $sliceReport.requestedPipelineMode $case.pipelineMode `
        "$($case.caseId) slice report requested mode"
    Assert-Equal $sliceReport.effectivePipelineMode $case.pipelineMode `
        "$($case.caseId) slice report effective mode"
    Assert-Equal $sliceReport.productionOutputWritten $true `
        "$($case.caseId) slice report output"
    Assert-Equal $sliceReport.fallbackApplied $false `
        "$($case.caseId) slice report fallback"
    Assert-Equal $previewReport.schema "p0.preview_report.1" `
        "$($case.caseId) preview report schema"

    $previewCount = 0
    foreach ($previewEntry in @($previewReport.files))
    {
        $relativePath = [string]$previewEntry.path
        $previewPath = Join-Path $packagePath $relativePath
        Assert-PathUnderRoot $packagePath $previewPath `
            "$($case.caseId) preview"
        Assert-True (Test-Path -LiteralPath $previewPath) `
            "$($case.caseId) 预览文件不存在：$relativePath"
        $previewCount++
    }
    Assert-True ($previewCount -gt 0) `
        "$($case.caseId) 缺少当前 package 预览"

    $packageEvidence += [ordered]@{
        caseId = $case.caseId
        requestedPipelineMode = $manifest.requestedPipelineMode
        effectivePipelineMode = $manifest.effectivePipelineMode
        productionOutputWritten = $manifest.productionOutputWritten
        fallbackApplied = $manifest.fallbackApplied
        previewCount = $previewCount
        reports = @(
            "reports/slice_report.json",
            "reports/preview_report.json")
        ripReader = $case.package.ripReader
    }
}

$summary = [ordered]@{
    schema = "slicesoft.qt_production_entry.12e_09b.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-09B-06"
    buildType = $Config
    matrixSummary = $matrixSummaryPath
    packageIdentityValidated = $true
    previewReportSameSource = $true
    productionOutputWritten = $true
    fallbackApplied = $false
    packageEvidence = $packageEvidence
    result = "pass"
}
$summaryPath =
    Join-Path $resolvedOutputRoot "qt_production_entry_summary.json"
Write-Utf8NoBom $summaryPath ($summary | ConvertTo-Json -Depth 100)

Write-Host "12E-09B-06 Qt dual-mode production gate: PASS"
Write-Host "Package identity / manifest mode / preview-report provenance: PASS"
Write-Host "Summary: $summaryPath"
$global:LASTEXITCODE = 0
