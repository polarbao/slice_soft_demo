param(
    [string]$BuildDirectory = "build-slicesoft/main",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDirectory = Join-Path $repositoryRoot $BuildDirectory
$mainUiExecutable = Join-Path $resolvedBuildDirectory "apps/slicer_debug_ui/$Configuration/slicer_debug_ui.exe"
$hostUiExecutable = Join-Path $resolvedBuildDirectory "apps/slicer_ui_host_sim/$Configuration/slicer_ui_host_sim.exe"
$modulePath = Join-Path $resolvedBuildDirectory "$Configuration/slicer_module.dll"

function Invoke-CheckedProcess
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    $output = & $FilePath @ArgumentList 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0)
    {
        Write-Error "$Label failed with exit code $LASTEXITCODE.`n$output"
    }
    Write-Host "PASS $Label"
}

foreach ($path in @($mainUiExecutable, $hostUiExecutable, $modulePath))
{
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "H-C-03 required binary is missing: $path"
    }
}

Push-Location $repositoryRoot
try
{
    $env:QT_QPA_PLATFORM = "offscreen"
    $mainCases = @(
        "multi-model-list",
        "scene-grid-layout",
        "production-mode-selector",
        "slice-settings-model",
        "generated-effective-config"
    )
    foreach ($caseName in $mainCases)
    {
        $processParameters = @{
            Label = "main:$caseName"
            FilePath = $mainUiExecutable
            ArgumentList = @("--ui-smoke-test", "--case", $caseName, "--yes")
        }
        Invoke-CheckedProcess @processParameters
    }

    $hostCases = @(
        "--hostflow-import-ui-self-test",
        "--hostflow-profile-ui-self-test",
        "--hostflow-settings-ui-self-test",
        "--hostflow-job-ui-self-test",
        "--hostflow-result-ui-self-test",
        "--hostflow-workspace-ui-self-test"
    )
    foreach ($caseName in $hostCases)
    {
        $processParameters = @{
            Label = "host:$caseName"
            FilePath = $hostUiExecutable
            ArgumentList = @($caseName, "--module", $modulePath)
        }
        Invoke-CheckedProcess @processParameters
    }

    $testParameters = @{
        Label = "ctest:^hostflow_h[ab]"
        FilePath = "ctest"
        ArgumentList = @(
            "--test-dir", $resolvedBuildDirectory,
            "-C", $Configuration,
            "-R", "^hostflow_h[ab]",
            "--output-on-failure"
        )
    }
    Invoke-CheckedProcess @testParameters

    $matrixParameters = @{
        Label = "matrix"
        FilePath = "python"
        ArgumentList = @("scripts/ValidateHostflowAbMatrix.py")
    }
    Invoke-CheckedProcess @matrixParameters

    Write-Host "HOSTFLOW_HC03_PASS config=$Configuration main=5 host=6"
}
finally
{
    Pop-Location
}
