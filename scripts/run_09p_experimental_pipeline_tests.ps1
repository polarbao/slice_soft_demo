param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$OpenVdbBuildDir = "",
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

function Find-Executable([string]$BuildDir, [string]$Config, [string]$Name) {
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

function Invoke-UnitTest([string]$BuildDir, [string]$Config, [string]$Name) {
    $exe = Find-Executable $BuildDir $Config $Name
    Invoke-External $Name $exe @()
}

Write-Host "09P experimental pipeline validation"
Write-Host "  BuildDir:        $BuildDir"
Write-Host "  Config:          $Config"
Write-Host "  OpenVdbBuildDir: $OpenVdbBuildDir"

Invoke-External "build $Config" "cmake" @("--build", $BuildDir, "--config", $Config)

Invoke-UnitTest $BuildDir $Config "production_admission_policy_unit_tests"
Invoke-UnitTest $BuildDir $Config "experimental_config_unit_tests"
Invoke-UnitTest $BuildDir $Config "geometry_kernel_service_unit_tests"
Invoke-UnitTest $BuildDir $Config "surface_shell_texture_service_unit_tests"
Invoke-UnitTest $BuildDir $Config "material_channel_composer_unit_tests"

Invoke-External "slicer_cli experimental smoke" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_09p_cli_experimental_tests.ps1",
    "-BuildDir",
    $BuildDir,
    "-Config",
    $Config
)

Invoke-External "run_ci_quick" "powershell" @(
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    ".\scripts\run_ci_quick.ps1"
)

if ([string]::IsNullOrWhiteSpace($OpenVdbBuildDir)) {
    Write-Host "== OpenVDB-specific tests skipped; -OpenVdbBuildDir was not provided."
}
else {
    if (-not (Test-Path -LiteralPath (Join-Path $OpenVdbBuildDir "CMakeCache.txt"))) {
        throw "OpenVDB build directory is not configured: $OpenVdbBuildDir"
    }

    Invoke-External "OpenVDB smoke" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_openvdb_smoke.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )
    Invoke-External "OpenVDB robustness tests" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_surface_shell_robustness_tests.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )
    Invoke-External "OpenVDB real-model tests" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_surface_shell_real_model_tests.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )
    Invoke-External "OpenVDB surface shell texture tests" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_surface_shell_texture_tests.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )

    Invoke-External "OpenVDB slicer_cli build" "cmake" @(
        "--build",
        $OpenVdbBuildDir,
        "--config",
        $Config,
        "--target",
        "slicer_cli"
    )
    Invoke-External "OpenVDB slicer_cli experimental smoke" "powershell" @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        ".\scripts\run_09p_cli_experimental_tests.ps1",
        "-BuildDir",
        $OpenVdbBuildDir,
        "-Config",
        $Config
    )
}

if ($RunBenchmarks) {
    if ([string]::IsNullOrWhiteSpace($OpenVdbBuildDir)) {
        throw "-RunBenchmarks requires -OpenVdbBuildDir."
    }
    Invoke-External "OpenVDB Release benchmarks" "powershell" @(
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

Write-Host "09P experimental pipeline validation complete."
