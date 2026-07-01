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

function Assert-HasProperty($Object, [string]$Name, [string]$Context) {
    $properties = @($Object.PSObject.Properties.Name)
    Assert-True ($properties -contains $Name) "$Context missing property: $Name"
}

function Assert-Properties($Object, $Names, [string]$Context) {
    foreach ($name in @($Names)) {
        Assert-HasProperty $Object $name $Context
    }
}

function Assert-StringArrayContains($ArrayValue, [string]$Expected, [string]$Context) {
    $values = @($ArrayValue)
    Assert-True ($values -contains $Expected) "$Context expected array to contain $Expected"
}

function Assert-ArrayEqual($Actual, $Expected, [string]$Context) {
    $actualValues = @($Actual)
    $expectedValues = @($Expected)
    Assert-Equal $actualValues.Count $expectedValues.Count "$Context count"
    for ($i = 0; $i -lt $expectedValues.Count; ++$i) {
        Assert-Equal $actualValues[$i] $expectedValues[$i] "$Context[$i]"
    }
}

function Find-SlicerCli([string]$BuildDir, [string]$Config) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/slicer_cli.exe"),
        (Join-Path $BuildDir "slicer_cli.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "slicer_cli.exe was not found under $BuildDir"
}

function Read-Json([string]$Path) {
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Invoke-ExperimentalReport(
    [string]$CliExe,
    [string]$Mode,
    [string]$ReportPath
) {
    $reportDirectory = Split-Path -Parent $ReportPath
    New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
    if (Test-Path -LiteralPath $ReportPath) {
        Remove-Item -LiteralPath $ReportPath -Force
    }

    Write-Host "== 09P schema report mode=$Mode"
    $cliOutput = & $CliExe `
        --config samples/configs/slice_config.json `
        --experimental-openvdb-shell `
        --admission-mode $Mode `
        --no-production-rgbwsv `
        --experimental-report $ReportPath 2>&1
    $cliOutput | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "experimental slicer_cli failed mode=$Mode exit=$LASTEXITCODE"
    }

    Assert-True (Test-Path -LiteralPath $ReportPath) "expected experimental report: $ReportPath"
    return Read-Json $ReportPath
}

function Test-ExperimentalReportSchema($Report, $Contract, [string]$Mode) {
    Assert-Properties $Report $Contract.requiredTopLevelFields "top-level report"
    Assert-Properties $Report.productionAdmission $Contract.productionAdmissionRequiredFields "productionAdmission"
    Assert-Properties $Report.outputContract $Contract.outputContractRequiredFields "outputContract"

    Assert-Equal $Report.schema $Contract.schema "schema"
    Assert-Equal $Report.experimentalOpenvdbShell $true "experimentalOpenvdbShell"
    Assert-Equal $Report.legacyPathExecuted $false "legacyPathExecuted"
    Assert-Equal $Report.productionPackageWritten $false "productionPackageWritten"
    Assert-Equal $Report.writeProductionRgbwsv $false "writeProductionRgbwsv"
    Assert-Equal $Report.productionAdmission.mode $Mode "productionAdmission.mode"
    Assert-Equal $Report.productionAdmission.productionAllowed $false "productionAdmission.productionAllowed"
    Assert-Equal $Report.productionAdmission.allowed $false "productionAdmission.allowed"
    Assert-Equal $Report.productionAdmission.nonProduction $true "productionAdmission.nonProduction"
    Assert-StringArrayContains $Report.productionAdmission.reasonCodes "EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY" "reasonCodes"

    Assert-Equal $Report.outputContract.packageSchema $Contract.safetyInvariants.packageSchema "outputContract.packageSchema"
    Assert-ArrayEqual $Report.outputContract.channelOrder $Contract.safetyInvariants.channelOrder "outputContract.channelOrder"
    Assert-Equal $Report.outputContract.bitDepth $Contract.safetyInvariants.bitDepth "outputContract.bitDepth"
    Assert-Equal $Report.outputContract.polarity $Contract.safetyInvariants.polarity "outputContract.polarity"
    Assert-Equal $Report.outputContract.printValue 0 "outputContract.printValue"
    Assert-Equal $Report.outputContract.emptyValue 255 "outputContract.emptyValue"

    Assert-Equal $Report.legacyPath.executed $false "legacyPath.executed"
    Assert-Equal $Report.legacyPath.productionPackageWritten $false "legacyPath.productionPackageWritten"
    Assert-Equal $Report.surfaceShell.generated $false "surfaceShell.generated"
    Assert-Equal $Report.textureTransfer.executed $false "textureTransfer.executed"
    Assert-Equal $Report.materialComposer.executed $false "materialComposer.executed"
    Assert-True ($Report.stats.issueCount -ge 1) "stats.issueCount expected diagnostic issue"
}

$contractPath = "tests/golden/expected/09p_experimental_report_schema.json"
$contract = Read-Json $contractPath
$cliExe = Find-SlicerCli $BuildDir $Config
$outputDir = "output/09PSchemaTests"

$strictReport = Invoke-ExperimentalReport $cliExe "strict_closed" (Join-Path $outputDir "strict_closed_report.json")
$diagnosticReport = Invoke-ExperimentalReport $cliExe "diagnostic_only" (Join-Path $outputDir "diagnostic_only_report.json")
$warnReport = Invoke-ExperimentalReport $cliExe "warn_and_attempt" (Join-Path $outputDir "warn_and_attempt_report.json")

Test-ExperimentalReportSchema $strictReport $contract "strict_closed"
Test-ExperimentalReportSchema $diagnosticReport $contract "diagnostic_only"
Test-ExperimentalReportSchema $warnReport $contract "warn_and_attempt"

Assert-Equal $diagnosticReport.productionAdmission.status "diagnostic_only" "diagnostic_only status"
Assert-Equal $warnReport.productionAdmission.status "non_production_only" "warn_and_attempt status"

Write-Host "09P experimental report schema tests complete."
