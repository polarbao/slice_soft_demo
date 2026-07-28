param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$OutputDir = "output/benchmarks/13b_07",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Join-Path $repoRoot $BuildDir
$resolvedOutputDir = Join-Path $repoRoot $OutputDir

if (-not $SkipBuild) {
    cmake --build $resolvedBuildDir --config $Config --target multi_model_scene_matrix rip_reader_test
    if ($LASTEXITCODE -ne 0) {
        throw "13B-07 targets failed to build."
    }
}

$matrixExe = Join-Path $resolvedBuildDir "$Config/multi_model_scene_matrix.exe"
$ripExe = Join-Path $resolvedBuildDir "$Config/rip_reader_test.exe"
if (-not (Test-Path -LiteralPath $matrixExe)) {
    throw "Missing matrix executable: $matrixExe"
}
if (-not (Test-Path -LiteralPath $ripExe)) {
    throw "Missing RIP reader executable: $ripExe"
}

& $matrixExe --source-root $repoRoot --output $resolvedOutputDir
if ($LASTEXITCODE -ne 0) {
    throw "13B-07 functional matrix failed."
}

$reportPath = Join-Path $resolvedOutputDir "real_model_matrix.json"
if (-not (Test-Path -LiteralPath $reportPath)) {
    throw "Missing matrix report: $reportPath"
}
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.schema -ne "slicesoft.multimodel_scene_matrix.13b.1") {
    throw "Unexpected 13B-07 matrix schema."
}
if (-not $report.functionalMatrixPass) {
    throw "13B-07 report did not pass the functional matrix."
}
if ($report.productionGo -or $report.productionStatus -ne "INPUT_OPEN") {
    throw "13B-07 fixture report must keep production INPUT_OPEN."
}

foreach ($case in $report.cases) {
    if ($case.category -ne "positive" -or -not $case.passed) {
        continue
    }
    & $ripExe --package $case.package.path --summary
    if ($LASTEXITCODE -ne 0) {
        throw "RIP strict failed for $($case.caseId)."
    }
}

Write-Host "13B-07 functional matrix PASS"
Write-Host "Report: $reportPath"
Write-Host "Production: INPUT_OPEN"
