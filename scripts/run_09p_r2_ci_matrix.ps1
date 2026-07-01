param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$OpenVdbBuildDir = "",
    [switch]$RunOpenVdbOn,
    [switch]$RunBenchmarks
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

function Assert-BuildDir([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath (Join-Path $Path "CMakeCache.txt"))) {
        throw "$Label build directory is not configured: $Path"
    }
}

Write-Host "09P-R2 CI matrix"
Write-Host "  BuildDir:        $BuildDir"
Write-Host "  Config:          $Config"
Write-Host "  OpenVdbBuildDir: $OpenVdbBuildDir"
Write-Host "  RunOpenVdbOn:    $RunOpenVdbOn"
Write-Host "  RunBenchmarks:   $RunBenchmarks"

Assert-BuildDir $BuildDir "OpenVDB OFF/default"

Invoke-External "OFF build" "cmake" @("--build", $BuildDir, "--config", $Config)

Invoke-External "OFF ctest" "ctest" @(
    "--test-dir",
    $BuildDir,
    "-C",
    $Config,
    "--output-on-failure"
)

Invoke-External "OFF run_ci_quick" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_ci_quick.ps1"
)

Invoke-External "OFF experimental CLI smoke" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_cli_experimental_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

Invoke-External "OFF schema contract" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_schema_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

Invoke-External "OFF golden contract" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_golden_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

if ($RunOpenVdbOn) {
    if ([string]::IsNullOrWhiteSpace($OpenVdbBuildDir)) {
        throw "-RunOpenVdbOn requires -OpenVdbBuildDir."
    }
    Assert-BuildDir $OpenVdbBuildDir "OpenVDB ON"

    Invoke-External "ON build" "cmake" @("--build", $OpenVdbBuildDir, "--config", $Config)

    Invoke-External "ON OpenVDB smoke" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_openvdb_smoke.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )

    Invoke-External "ON experimental pipeline" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_09p_experimental_pipeline_tests.ps1",
        "-BuildDir",
        $BuildDir,
        "-Config",
        $Config,
        "-OpenVdbBuildDir",
        $OpenVdbBuildDir
    )
}
else {
    Write-Host "== OpenVDB ON lane skipped; pass -RunOpenVdbOn -OpenVdbBuildDir <dir> to enable it."
}

if ($RunBenchmarks) {
    if ([string]::IsNullOrWhiteSpace($OpenVdbBuildDir)) {
        throw "-RunBenchmarks requires -OpenVdbBuildDir."
    }
    Assert-BuildDir $OpenVdbBuildDir "OpenVDB benchmark"
    Invoke-External "Benchmark Release" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_surface_shell_benchmarks.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        "Release"
    )
}
else {
    Write-Host "== Benchmark lane skipped; pass -RunBenchmarks -OpenVdbBuildDir <dir> to enable it."
}

Write-Host "09P-R2 CI matrix complete."
