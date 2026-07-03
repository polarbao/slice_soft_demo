param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-External([string]$Name, [string]$File, [string[]]$Arguments) {
    Write-Host "== $Name"
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Invoke-External "build default OpenVDB-OFF targets" "cmake" @(
    "--build",
    $BuildDir,
    "--config",
    $Config,
    "--target",
    "slicer_cli",
    "rip_reader_test",
    "slicer_debug_ui"
)

Invoke-External "default ctest" "ctest" @(
    "--test-dir",
    $BuildDir,
    "-C",
    $Config,
    "--output-on-failure"
)

Write-Host "11A-R1 OpenVDB candidate off-lane safety smoke complete."
