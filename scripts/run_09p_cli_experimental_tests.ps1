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

function Run-ExperimentalCase(
    [string]$CliExe,
    [string]$Mode,
    [string]$ReportPath
) {
    $reportDirectory = Split-Path -Parent $ReportPath
    New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
    if (Test-Path -LiteralPath $ReportPath) {
        Remove-Item -LiteralPath $ReportPath -Force
    }

    Write-Host "== slicer_cli experimental mode=$Mode"
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
    $report = Read-Json $ReportPath
    Assert-Equal $report.schema "p0.experimental_openvdb_shell_cli_report.1" "experimental schema mismatch"
    Assert-Equal $report.experimentalOpenvdbShell $true "experimental flag mismatch"
    Assert-Equal $report.legacyPathExecuted $false "legacy path should not execute"
    Assert-Equal $report.productionPackageWritten $false "production package must not be written"
    Assert-Equal $report.noProductionRgbwsv $true "noProductionRgbwsv mismatch"
    Assert-Equal $report.writeProductionRgbwsv $false "writeProductionRgbwsv mismatch"
    Assert-Equal $report.productionAdmission.productionAllowed $false "experimental CLI must not be production allowed"
    Assert-Equal $report.productionAdmission.nonProduction $true "experimental CLI must be nonProduction"
    return $report
}

$cliExe = Find-SlicerCli $BuildDir $Config

Write-Host "== slicer_cli legacy smoke"
& $cliExe --config samples/configs/slice_config.json --preview-only
if ($LASTEXITCODE -ne 0) {
    throw "legacy slicer_cli preview-only smoke failed"
}

$outputDir = "output/09PExperimentalCliSmoke"
$strictReport = Run-ExperimentalCase $cliExe "strict_closed" (Join-Path $outputDir "strict_closed_report.json")
$diagnosticReport = Run-ExperimentalCase $cliExe "diagnostic_only" (Join-Path $outputDir "diagnostic_only_report.json")
$warnReport = Run-ExperimentalCase $cliExe "warn_and_attempt" (Join-Path $outputDir "warn_and_attempt_report.json")

if ($strictReport.openvdb.available -eq $false) {
    $strictCodes = @($strictReport.diagnostics | ForEach-Object { $_.code })
    Assert-True ($strictCodes -contains "OPENVDB_UNAVAILABLE") "USE_OPENVDB=OFF strict report must include OPENVDB_UNAVAILABLE"
    Assert-True (@($strictReport.productionAdmission.blockerCodes) -contains "OPENVDB_UNAVAILABLE") `
        "strict admission must block on OPENVDB_UNAVAILABLE"
}

Assert-Equal $diagnosticReport.productionAdmission.status "diagnostic_only" "diagnostic_only status mismatch"
Assert-Equal $warnReport.productionAdmission.status "non_production_only" "warn_and_attempt status mismatch"

Write-Host "09P experimental CLI tests complete."
