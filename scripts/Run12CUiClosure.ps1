[CmdletBinding()]
param(
    [string]$BuildDir = "build-12c-ui",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function InvokeNativeStep
{
    param(
        [string]$Name,
        [string]$Executable,
        [string[]]$Arguments
    )

    Write-Host "== $Name"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function ResolveExecutable
{
    param(
        [string]$BuildRoot,
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates)
    {
        $path = Join-Path $BuildRoot $candidate
        if (Test-Path -LiteralPath $path)
        {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    throw "Executable was not found under $BuildRoot. Candidates: $($Candidates -join ', ')"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir))
{
    $BuildDir
}
else
{
    Join-Path $repoRoot $BuildDir
}

Push-Location $repoRoot
try
{
    InvokeNativeStep `
        -Name "build slicer_cli, slicer_debug_ui, and CTest targets" `
        -Executable "cmake" `
        -Arguments @(
            "--build", $resolvedBuildDir,
            "--config", $Config,
            "--target",
            "slicer_cli",
            "slicer_debug_ui",
            "production_admission_policy_unit_tests",
            "experimental_config_unit_tests",
            "geometry_kernel_service_unit_tests",
            "openvdb_sdf_utility_report_unit_tests",
            "surface_shell_texture_service_unit_tests",
            "material_channel_composer_unit_tests",
            "--", "/m"
        )

    $slicerCli = ResolveExecutable `
        -BuildRoot $resolvedBuildDir `
        -Candidates @("$Config/slicer_cli.exe", "slicer_cli.exe")
    $uiExecutable = ResolveExecutable `
        -BuildRoot $resolvedBuildDir `
        -Candidates @(
            "apps/slicer_debug_ui/$Config/slicer_debug_ui.exe",
            "$Config/slicer_debug_ui.exe",
            "apps/slicer_debug_ui/slicer_debug_ui.exe"
        )

    InvokeNativeStep `
        -Name "generate UiSmokeLayerPreview" `
        -Executable $slicerCli `
        -Arguments @("--config", "samples/configs/ui_smoke/ui_layer_preview.json")
    InvokeNativeStep `
        -Name "generate UiSmokeOverlayRgbwv" `
        -Executable $slicerCli `
        -Arguments @("--config", "samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json")

    $smokeCases = @(
        @{ Name = "self-test"; Arguments = @("--self-test") },
        @{ Name = "scenario-registry"; Arguments = @("--ui-smoke-test", "--case", "scenario-registry") },
        @{ Name = "slice-settings-model"; Arguments = @("--ui-smoke-test", "--case", "slice-settings-model") },
        @{ Name = "generated-effective-config"; Arguments = @("--ui-smoke-test", "--case", "generated-effective-config") },
        @{ Name = "slice-progress-timing"; Arguments = @("--ui-smoke-test", "--case", "slice-progress-timing") },
        @{ Name = "setting-help-metadata"; Arguments = @("--ui-smoke-test", "--case", "setting-help-metadata") },
        @{
            Name = "preview-workspace-shared-layer"
            Arguments = @(
                "--ui-smoke-test", "--case", "preview-workspace-shared-layer",
                "--package", "output/UiSmokeOverlayRgbwv"
            )
        },
        @{
            Name = "preview-legend-probe-context"
            Arguments = @(
                "--ui-smoke-test", "--case", "preview-legend-probe-context",
                "--package", "output/UiSmokeLayerPreview"
            )
        },
        @{
            Name = "diagnostics-collapse"
            Arguments = @(
                "--ui-smoke-test", "--case", "diagnostics-collapse",
                "--package", "output/UiSmokeLayerPreview"
            )
        },
        @{
            Name = "material-closure-diagnostics"
            Arguments = @(
                "--ui-smoke-test", "--case", "material-closure-diagnostics"
            )
        },
        @{ Name = "openvdb-utility-summary"; Arguments = @("--ui-smoke-test", "--case", "openvdb-utility-summary") },
        @{
            Name = "workspace-layout-sizes"
            Arguments = @(
                "--ui-smoke-test", "--case", "workspace-layout-sizes",
                "--package", "output/UiSmokeLayerPreview"
            )
        },
        @{
            Name = "layer-preview-load"
            Arguments = @(
                "--ui-smoke-test", "--case", "layer-preview-load",
                "--package", "output/UiSmokeLayerPreview"
            )
        },
        @{
            Name = "overlay-load-real"
            Arguments = @(
                "--ui-smoke-test", "--case", "overlay-load-real",
                "--package", "output/UiSmokeOverlayRgbwv"
            )
        }
    )

    foreach ($smokeCase in $smokeCases)
    {
        InvokeNativeStep `
            -Name "UI smoke $($smokeCase.Name)" `
            -Executable $uiExecutable `
            -Arguments $smokeCase.Arguments
    }

    Write-Host "12C Qt UI closure passed."
    Write-Host "  buildDir: $resolvedBuildDir"
    Write-Host "  config: $Config"
}
finally
{
    Pop-Location
}
