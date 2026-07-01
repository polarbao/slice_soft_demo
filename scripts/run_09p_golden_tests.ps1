param(
    [string]$BuildDir = "build",
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
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Test-GoldenReport($Report, $Contract, [string]$Mode) {
    Assert-Equal $Report.schema $Contract.reportSchema "$Mode schema"
    Assert-Equal $Report.experimentalOpenvdbShell $Contract.safetyInvariants.experimentalOpenvdbShell "$Mode experimental flag"
    Assert-Equal $Report.legacyPathExecuted $Contract.safetyInvariants.legacyPathExecuted "$Mode legacyPathExecuted"
    Assert-Equal $Report.productionPackageWritten $Contract.safetyInvariants.productionPackageWritten "$Mode package written"
    Assert-Equal $Report.writeProductionRgbwsv $Contract.safetyInvariants.writeProductionRgbwsv "$Mode writeProductionRgbwsv"
    Assert-Equal $Report.productionAdmission.mode $Mode "$Mode admission mode"
    Assert-Equal $Report.productionAdmission.productionAllowed $Contract.safetyInvariants.productionAllowed "$Mode productionAllowed"
    Assert-Equal $Report.productionAdmission.nonProduction $Contract.safetyInvariants.nonProduction "$Mode nonProduction"
    Assert-Contains $Report.productionAdmission.reasonCodes $Contract.diagnosticReasonCode "$Mode reasonCodes"

    Assert-Equal $Report.outputContract.packageSchema $Contract.safetyInvariants.packageSchema "$Mode packageSchema"
    Assert-ArrayEqual $Report.outputContract.channelOrder $Contract.safetyInvariants.channelOrder "$Mode channelOrder"
    Assert-Equal $Report.outputContract.bitDepth $Contract.safetyInvariants.bitDepth "$Mode bitDepth"
    Assert-Equal $Report.outputContract.polarity $Contract.safetyInvariants.polarity "$Mode polarity"
    Assert-Equal $Report.outputContract.printValue $Contract.safetyInvariants.printValue "$Mode printValue"
    Assert-Equal $Report.outputContract.emptyValue $Contract.safetyInvariants.emptyValue "$Mode emptyValue"

    Assert-Equal $Report.outputContract.perLayerStats.available $false "$Mode perLayerStats unavailable"
    Assert-True (-not [string]::IsNullOrWhiteSpace($Report.outputContract.perLayerStats.reason)) "$Mode perLayerStats reason"
    Assert-Equal $Report.outputContract.textureFidelity.available $false "$Mode textureFidelity unavailable"
    Assert-True (-not [string]::IsNullOrWhiteSpace($Report.outputContract.textureFidelity.reason)) "$Mode textureFidelity reason"
    Assert-ArrayEqual $Report.outputContract.textureFidelity.fallbackCodes @() "$Mode textureFidelity fallbackCodes"
    Assert-ArrayEqual $Report.outputContract.fallbackCodes @() "$Mode output fallbackCodes"

    Assert-Equal $Report.surfaceShell.generated $false "$Mode surface shell generated"
    Assert-Equal $Report.textureTransfer.executed $false "$Mode texture transfer executed"
    Assert-Equal $Report.materialComposer.executed $false "$Mode material composer executed"
    Assert-Equal $Report.legacyPath.executed $false "$Mode legacy path executed"
    Assert-Equal $Report.legacyPath.productionPackageWritten $false "$Mode legacy package written"
    Assert-True ($Report.stats.issueCount -ge 1) "$Mode issueCount"

    if ($Report.openvdb.available -eq $false) {
        $codes = @($Report.diagnostics | ForEach-Object { $_.code })
        Assert-Contains $codes "OPENVDB_UNAVAILABLE" "$Mode OpenVDB unavailable diagnostic"
    }
}

$contractPath = "tests/golden/expected/09p_experimental_output_contract.json"
$contract = Read-Json $contractPath

Invoke-External "09P schema tests" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_schema_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

Invoke-External "09P CLI experimental tests" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_cli_experimental_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

$schemaOutputDir = "output/09PSchemaTests"
foreach ($mode in @($contract.requiredModes)) {
    $reportPath = Join-Path $schemaOutputDir "${mode}_report.json"
    Assert-True (Test-Path -LiteralPath $reportPath) "missing generated report: $reportPath"
    $report = Read-Json $reportPath
    Test-GoldenReport $report $contract $mode
}

Write-Host "09P experimental golden tests complete."
