param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$Output = "output/benchmarks/12b_r2_openvdb_sdf_utility_report.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cachePath))
{
    throw "Build directory is not configured: $BuildDir"
}

$openVdbSetting = Select-String -LiteralPath $cachePath -Pattern '^USE_OPENVDB:BOOL=(ON|OFF)$'
if ($null -eq $openVdbSetting)
{
    throw "USE_OPENVDB was not found in $cachePath"
}
$expectOpenVdb = $openVdbSetting.Matches[0].Groups[1].Value -eq "ON"

& cmake --build $BuildDir --config $Config --target openvdb_sdf_utility_probe
if ($LASTEXITCODE -ne 0)
{
    throw "openvdb_sdf_utility_probe build failed with exit code $LASTEXITCODE"
}

$executableCandidates = @(
    (Join-Path $BuildDir "$Config/openvdb_sdf_utility_probe.exe"),
    (Join-Path $BuildDir "openvdb_sdf_utility_probe.exe")
)
$probeExe = $null
foreach ($candidate in $executableCandidates)
{
    if (Test-Path -LiteralPath $candidate)
    {
        $probeExe = $candidate
        break
    }
}
if ($null -eq $probeExe)
{
    throw "openvdb_sdf_utility_probe.exe was not found under $BuildDir"
}

& $probeExe --output $Output --build-dir $BuildDir --build-type $Config
if ($LASTEXITCODE -ne 0)
{
    throw "openvdb_sdf_utility_probe failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path -LiteralPath $Output))
{
    throw "Utility report was not generated: $Output"
}
$report = Get-Content -Raw -LiteralPath $Output | ConvertFrom-Json

if ($report.schema -ne "slicesoft.openvdb_sdf_utility.12b_r2.1")
{
    throw "Unexpected utility report schema: $($report.schema)"
}
if ($report.outputPolicy.writesProductionPackage -ne $false -or
    $report.outputPolicy.writesProductionTiff -ne $false -or
    $report.outputPolicy.writesPreview -ne $false -or
    $report.outputPolicy.writesUtilityReport -ne $true -or
    $report.outputPolicy.modifiesLegacyOutput -ne $false -or
    $report.outputPolicy.protocolSchemaTouched -ne $false)
{
    throw "Utility report violates the diagnostic-only output policy"
}
if ($report.decision.productionReplacementAllowed -ne $false)
{
    throw "productionReplacementAllowed must remain false"
}
$allowedStatuses = @("pass", "fail", "unavailable", "blocked", "skipped", "not_evaluated")
$allowedPromoteDecisions = @("promote", "keep_experimental", "reject", "not_evaluated")
foreach ($utilityName in @("outerVarnishShell", "clearanceDistance", "topologyDiagnostic", "materialClosureAssist"))
{
    if ($report.utilities.PSObject.Properties.Name -notcontains $utilityName)
    {
        throw "Missing utility report item: $utilityName"
    }
    $utility = $report.utilities.$utilityName
    if ($allowedStatuses -notcontains [string]$utility.status)
    {
        throw "Invalid utility status for ${utilityName}: $($utility.status)"
    }
    if ($allowedPromoteDecisions -notcontains [string]$utility.promoteDecision)
    {
        throw "Invalid promoteDecision for ${utilityName}: $($utility.promoteDecision)"
    }
    if ($utility.available -eq $false -and $utility.executed -eq $true)
    {
        throw "Unavailable utility cannot be marked executed: $utilityName"
    }
    if ($report.build.openVdbAvailable -eq $false -and $utility.status -eq "pass")
    {
        throw "OpenVDB unavailable report cannot contain a passing utility: $utilityName"
    }
}
$allowedSeverities = @("info", "warning", "blocker", "error")
foreach ($issue in $report.issues)
{
    if ($allowedSeverities -notcontains [string]$issue.severity)
    {
        throw "Invalid issue severity: $($issue.severity)"
    }
}

if ($expectOpenVdb)
{
    if ($report.build.useOpenVdb -ne $true -or $report.build.openVdbAvailable -ne $true)
    {
        throw "OpenVDB ON build did not report an available runtime"
    }
    if ($report.utilities.outerVarnishShell.status -ne "pass" -or
        [int]$report.utilities.outerVarnishShell.metrics.activeVoxels -le 0)
    {
        throw "OpenVDB ON outer varnish utility did not produce valid shell metrics"
    }
    if ($report.utilities.topologyDiagnostic.status -ne "pass")
    {
        throw "Closed fixture topology diagnostic did not pass"
    }
}
else
{
    if ($report.build.useOpenVdb -ne $false -or $report.build.openVdbAvailable -ne $false)
    {
        throw "OpenVDB OFF build did not report unavailable"
    }
    if ($report.utilities.outerVarnishShell.status -ne "unavailable" -or
        $report.utilities.outerVarnishShell.executed -ne $false)
    {
        throw "OpenVDB OFF utility must be unavailable and not executed"
    }
}

Write-Host "12B-R2 OpenVDB SDF utility report validation passed."
Write-Host "  buildDir: $BuildDir"
Write-Host "  useOpenVdb: $($report.build.useOpenVdb)"
Write-Host "  report: $Output"
