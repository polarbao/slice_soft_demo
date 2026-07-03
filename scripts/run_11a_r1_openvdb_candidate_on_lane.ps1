param(
    [string]$OpenVdbBuildDir = "build-openvdb-09p",
    [string]$UiBuildDir = "build",
    [string]$Config = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Equal($Actual, $Expected, [string]$Message) {
    if ($Actual -ne $Expected) {
        throw "$Message expected=$Expected actual=$Actual"
    }
}

function Assert-ArrayEqual($Actual, $Expected, [string]$Message) {
    $actualValues = @($Actual)
    $expectedValues = @($Expected)
    Assert-Equal $actualValues.Count $expectedValues.Count "$Message count"
    for ($index = 0; $index -lt $expectedValues.Count; ++$index) {
        Assert-Equal $actualValues[$index] $expectedValues[$index] "$Message[$index]"
    }
}

function Assert-Contains($Actual, [string]$Expected, [string]$Message) {
    $actualValues = @($Actual)
    Assert-True ($actualValues -contains $Expected) "$Message expected $Expected"
}

function Invoke-External([string]$Name, [string]$File, [string[]]$Arguments) {
    Write-Host "== $Name"
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Read-Json([string]$Path) {
    Assert-True (Test-Path -LiteralPath $Path) "missing JSON: $Path"
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Find-Executable([string]$BuildDir, [string]$Name) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "$Name.exe was not found under $BuildDir"
}

$contractPath = "tests/golden/expected/11a_r1_openvdb_candidate_contract.json"
$contract = Read-Json $contractPath
$configPath = $contract.config
$packageDir = $contract.package

Invoke-External "build OpenVDB candidate CLI" "cmake" @(
    "--build",
    $OpenVdbBuildDir,
    "--config",
    $Config,
    "--target",
    "slicer_cli",
    "rip_reader_test"
)

Invoke-External "build default UI smoke target" "cmake" @(
    "--build",
    $UiBuildDir,
    "--config",
    $Config,
    "--target",
    "slicer_debug_ui"
)

$slicerCli = Find-Executable $OpenVdbBuildDir "slicer_cli"
$ripReader = Find-Executable $OpenVdbBuildDir "rip_reader_test"
$uiExe = Join-Path $UiBuildDir "apps/slicer_debug_ui/$Config/slicer_debug_ui.exe"
Assert-True (Test-Path -LiteralPath $uiExe) "missing slicer_debug_ui: $uiExe"

Invoke-External "slice OpenVDB candidate package" $slicerCli @(
    "--config",
    $configPath,
    "--openvdb-candidate-slice"
)

Invoke-External "RIP reader summary" $ripReader @(
    "--package",
    $packageDir,
    "--summary"
)

$manifest = Read-Json (Join-Path $packageDir "manifest.json")
Assert-Equal $manifest.schema $contract.packageSchema "manifest schema"
Assert-Equal $manifest.tiff.bitDepth $contract.bitDepth "TIFF bitDepth"
Assert-Equal $manifest.tiff.polarity $contract.polarity "TIFF polarity"
Assert-Equal $manifest.tiff.printValue $contract.printValue "TIFF printValue"
Assert-Equal $manifest.tiff.emptyValue $contract.emptyValue "TIFF emptyValue"
Assert-Equal $manifest.tiff.storageMode $contract.storageMode "TIFF storageMode"
Assert-ArrayEqual $manifest.tiff.channelOrder $contract.channelOrder "TIFF channelOrder"
Assert-Equal $manifest.grid.widthPx $contract.grid.widthPx "grid width"
Assert-Equal $manifest.grid.heightPx $contract.grid.heightPx "grid height"
Assert-Equal $manifest.grid.layerCount $contract.grid.layerCount "grid layerCount"

$candidateReport = Read-Json (Join-Path $packageDir "reports/openvdb_candidate_report.json")
Assert-Equal $candidateReport.productionPackageWritten $true "candidate productionPackageWritten"
Assert-Equal $candidateReport.totals.modelPixels $contract.totals.modelPixels "candidate modelPixels"
Assert-Equal $candidateReport.totals.supportPixels $contract.totals.supportPixels "candidate supportPixels"
Assert-Equal $candidateReport.totals.shellPixels $contract.totals.shellPixels "candidate shellPixels"

$previewReport = Read-Json (Join-Path $packageDir "reports/preview_report.json")
Assert-Equal $previewReport.schema $contract.preview.schema "preview schema"
Assert-Equal $previewReport.format $contract.preview.format "preview format"
Assert-Equal @($previewReport.files).Count $contract.preview.fileCount "preview file count"
foreach ($channel in @($contract.preview.channels)) {
    Assert-Contains $previewReport.channels $channel "preview channels"
}
foreach ($relativeReportPath in @($contract.requiredReports)) {
    Assert-True (Test-Path -LiteralPath (Join-Path $packageDir $relativeReportPath)) "missing report: $relativeReportPath"
}

Invoke-External "UI LayerPreview smoke" $uiExe @(
    "--ui-smoke-test",
    "--case",
    "layer-preview-load",
    "--package",
    $packageDir
)

Invoke-External "UI OverlayPreview smoke" $uiExe @(
    "--ui-smoke-test",
    "--case",
    "overlay-load-real",
    "--package",
    $packageDir
)

Write-Host "11A-R1 OpenVDB candidate on-lane smoke complete."
