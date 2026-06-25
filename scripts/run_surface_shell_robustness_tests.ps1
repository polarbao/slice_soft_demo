param(
    [string]$BuildDir = "build-openvdb-09b-r2",
    [string]$Config = "Debug",
    [switch]$RunMatrix,
    [switch]$RunRealModels
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Assert-Equal($Actual, $Expected, [string]$Message) {
    if ($Actual -ne $Expected) { throw "$Message expected=$Expected actual=$Actual" }
}

function Read-Json([string]$Path) {
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Find-Executable([string]$Name) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "$Name.exe was not found under $BuildDir"
}

function Run-RobustnessCase(
    [string]$DemoExe,
    [string]$FixtureId,
    [string]$ConfigPath,
    [string]$OutputDir,
    [bool]$ExpectSuccess = $true,
    [double]$VoxelMm = 0.05,
    [double]$ShellMm = 0.10,
    [string]$MeshPolicy = "strict_closed",
    [bool]$RequireInterior = $true
) {
    Write-Host "== surface shell robustness $FixtureId"
    & $DemoExe `
        --config $ConfigPath `
        --fixture-id $FixtureId `
        --output $OutputDir `
        --voxel-mm $VoxelMm `
        --shell-mm $ShellMm `
        --mesh-policy $MeshPolicy `
        --build-config $Config
    $exitCode = $LASTEXITCODE
    if ($ExpectSuccess -and $exitCode -ne 0) {
        throw "surface_shell_robustness_demo failed: $FixtureId exit=$exitCode"
    }
    if (-not $ExpectSuccess -and $exitCode -eq 0) {
        throw "surface_shell_robustness_demo unexpectedly passed: $FixtureId"
    }

    $reportPath = Join-Path $OutputDir "reports/surface_shell_texture_report.json"
    $benchmarkPath = Join-Path $OutputDir "reports/surface_shell_benchmark_report.json"
    Assert-True (Test-Path -LiteralPath $reportPath) "expected report: $reportPath"
    Assert-True (Test-Path -LiteralPath $benchmarkPath) "expected benchmark report: $benchmarkPath"
    $report = Read-Json $reportPath
    Assert-Equal $report.schema "p0.surface_shell_texture_report.2" "report schema mismatch"
    Assert-True ($report.epsilon.positionEpsilonMm -gt 0) "expected position epsilon"
    Assert-True ($report.transferStats.nearestQueryStats.nodeCount -ge 0) "expected nearest query stats"
    Assert-True ($report.memory.peakEstimatedBytes -ge 0) "expected memory fields"
    if ($ExpectSuccess) {
        Assert-True ($report.openvdb.activeVoxels -gt 0) "expected active voxels"
        Assert-True ($report.stats.shellVoxels -gt 0) "expected shell voxels"
        if ($RequireInterior) {
            Assert-True ($report.stats.interiorVoxels -gt 0) "expected interior voxels"
        }
        Assert-Equal $report.stats.outsideColoredVoxels 0 "outside colored voxels mismatch"
        Assert-Equal $report.stats.shellPlusInteriorEqualsInside $true "shell invariant mismatch"
        Assert-True ((Get-ChildItem -LiteralPath (Join-Path $OutputDir "preview") -Filter "composite_layer_*.png").Count -gt 0) "expected composite preview"
    }
    return $report
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "OpenVDB 09B-R2 build directory is not configured: $BuildDir"
}

Write-Host "== build surface shell robustness targets"
& cmake --build $BuildDir --config $Config --target surface_shell_robustness_demo
if ($LASTEXITCODE -ne 0) { throw "surface_shell_robustness_demo build failed" }
& cmake --build $BuildDir --config $Config --target surface_shell_robustness_unit_tests
if ($LASTEXITCODE -ne 0) { throw "surface_shell_robustness_unit_tests build failed" }

$demoExe = Find-Executable "surface_shell_robustness_demo"
$unitExe = Find-Executable "surface_shell_robustness_unit_tests"

Write-Host "== surface shell robustness unit tests"
& $unitExe
if ($LASTEXITCODE -ne 0) { throw "surface_shell_robustness_unit_tests failed" }

$golden = Read-Json "tests/golden/expected/surface_shell_real_model_r2.json"

$multi = Run-RobustnessCase $demoExe "multimaterial_seam" "samples/configs/openvdb/surface_shell_multimaterial_seam.json" "output/SurfaceShellR2MultiMaterialSeam"
$thin = Run-RobustnessCase $demoExe "thin_wall" "samples/configs/openvdb/surface_shell_thin_wall.json" "output/SurfaceShellR2ThinWall" $true 0.05 0.10 "strict_closed" $false
$duplicate = Run-RobustnessCase $demoExe "duplicate_face" "samples/configs/openvdb/surface_shell_duplicate_face.json" "output/SurfaceShellR2DuplicateFace" $false
$reversed = Run-RobustnessCase $demoExe "local_reversed" "samples/configs/openvdb/surface_shell_local_reversed.json" "output/SurfaceShellR2LocalReversed" $false
$self = Run-RobustnessCase $demoExe "self_intersect" "samples/configs/openvdb/surface_shell_self_intersect.json" "output/SurfaceShellR2SelfIntersect" $false

Assert-Equal $multi.meshDiagnostics.acceptedTriangles $golden.fixtures.multimaterial_seam.acceptedTriangles "multimaterial triangle count"
Assert-Equal $multi.robustnessDiagnostics.duplicateFaces $golden.fixtures.multimaterial_seam.duplicateFaces "multimaterial duplicate count"
Assert-Equal $multi.stats.outsideColoredVoxels $golden.fixtures.multimaterial_seam.outsideColoredVoxels "multimaterial outside color"
Assert-True ($multi.transferStats.sampledTextureVoxels -ge $golden.fixtures.multimaterial_seam.sampledTextureVoxelsMin) "multimaterial sampled texture"
Assert-True ($multi.transferStats.loadedTextureCount -ge $golden.fixtures.multimaterial_seam.loadedTextureCountMin) "multimaterial texture count"
Assert-True ($duplicate.robustnessDiagnostics.duplicateFaces -gt 0) "duplicate face expected"
Assert-True ($reversed.robustnessDiagnostics.inconsistentOrientedEdges -gt 0) "local reversed expected"
Assert-True ($self.robustnessDiagnostics.selfIntersectionPairs -gt 0) "self-intersection expected"

if ($RunMatrix) {
    $voxels = @(0.10, 0.05, 0.025)
    $shells = @(0.05, 0.10, 0.20)
    $previousShell = -1
    foreach ($voxel in $voxels) {
        foreach ($shell in $shells) {
            $name = "matrix_v$($voxel)_s$($shell)" -replace '\.', '_'
            $report = Run-RobustnessCase `
                $demoExe `
                $name `
                "samples/configs/openvdb/surface_shell_multimaterial_seam.json" `
                "output/SurfaceShellR2Matrix/$name" `
                $true `
                $voxel `
                $shell
            Assert-True ($report.stats.shellVoxels -gt 0) "matrix shell voxels"
            if ($voxel -eq 0.05 -and $previousShell -ge 0) {
                Assert-True ($report.stats.shellVoxels -ge $previousShell) "shell monotonic for voxel 0.05"
            }
            if ($voxel -eq 0.05) {
                $previousShell = [int]$report.stats.shellVoxels
            }
        }
    }
}

if ($RunRealModels) {
    Run-RobustnessCase $demoExe "nail_obj_golden" "samples/configs/openvdb/surface_shell_nail_obj_golden.json" "output/SurfaceShellR2NailObjGolden" $true 0.10 0.10 "warn_and_attempt" | Out-Null
    Run-RobustnessCase $demoExe "nail_3mf_golden" "samples/configs/openvdb/surface_shell_nail_3mf_golden.json" "output/SurfaceShellR2Nail3MfGolden" $true 0.10 0.10 "warn_and_attempt" | Out-Null
}

Write-Host "Surface shell robustness tests complete."
