param(
    [string]$BuildDir = "build-openvdb-09b-r1",
    [string]$Config = "Debug"
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

function Run-RealModelCase(
    [string]$DemoExe,
    [string]$ConfigPath,
    [string]$OutputDir,
    [bool]$ExpectSuccess = $true
) {
    Write-Host "== surface shell real model $ConfigPath"
    & $DemoExe --config $ConfigPath --voxel-mm 0.05 --shell-mm 0.10 --mesh-policy strict_closed --output $OutputDir
    $exitCode = $LASTEXITCODE
    if ($ExpectSuccess -and $exitCode -ne 0) {
        throw "surface_shell_real_model_demo failed: $ConfigPath exit=$exitCode"
    }
    if (-not $ExpectSuccess -and $exitCode -eq 0) {
        throw "surface_shell_real_model_demo unexpectedly passed: $ConfigPath"
    }

    $reportPath = Join-Path $OutputDir "reports/surface_shell_texture_report.json"
    Assert-True (Test-Path -LiteralPath $reportPath) "expected report: $reportPath"
    $report = Read-Json $reportPath
    Assert-Equal $report.schema "p0.surface_shell_texture_report.2" "report schema mismatch"
    if ($ExpectSuccess) {
        Assert-True ($report.openvdb.activeVoxels -gt 0) "expected active voxels"
        Assert-True ($report.stats.shellVoxels -gt 0) "expected shell voxels"
        Assert-True ($report.stats.interiorVoxels -gt 0) "expected interior voxels"
        Assert-Equal $report.stats.outsideColoredVoxels 0 "outside colored voxels mismatch"
        Assert-Equal $report.stats.unclassifiedVoxels 0 "unclassified voxels mismatch"
        Assert-Equal $report.stats.shellPlusInteriorEqualsInside $true "shell invariant mismatch"
        Assert-True ((Get-ChildItem -LiteralPath (Join-Path $OutputDir "preview") -Filter "composite_layer_*.png").Count -gt 0) "expected composite preview"
    }
    return $report
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "OpenVDB 09B-R1 build directory is not configured: $BuildDir"
}

Write-Host "== build surface shell real-model targets"
& cmake --build $BuildDir --config $Config --target surface_shell_real_model_demo
if ($LASTEXITCODE -ne 0) { throw "surface_shell_real_model_demo build failed" }
& cmake --build $BuildDir --config $Config --target surface_shell_real_model_unit_tests
if ($LASTEXITCODE -ne 0) { throw "surface_shell_real_model_unit_tests build failed" }

$demoExe = Find-Executable "surface_shell_real_model_demo"
$unitExe = Find-Executable "surface_shell_real_model_unit_tests"

Write-Host "== surface shell real-model unit tests"
& $unitExe
if ($LASTEXITCODE -ne 0) { throw "surface_shell_real_model_unit_tests failed" }

$objReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_obj_real.json" "output/SurfaceShellObjReal"
$threeMfReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_3mf_real.json" "output/SurfaceShell3MfReal"
$missingTextureReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_obj_missing_texture.json" "output/SurfaceShellObjMissingTexture"
$noUvReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_obj_no_uv.json" "output/SurfaceShellObjNoUv"
$openMeshReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_open_mesh.json" "output/SurfaceShellOpenMesh" $false
$nonManifoldReport = Run-RealModelCase $demoExe "samples/configs/openvdb/surface_shell_non_manifold.json" "output/SurfaceShellNonManifold" $false

Assert-True ($objReport.transferStats.sampledTextureVoxels -gt 0) "OBJ expected sampled texture voxels"
Assert-True ($threeMfReport.transferStats.sampledTextureVoxels -gt 0) "3MF expected sampled texture voxels"
Assert-True ($objReport.transferStats.uniqueColorCount -gt 1) "OBJ expected multiple texture colors"
Assert-True ($threeMfReport.transferStats.uniqueColorCount -gt 1) "3MF expected multiple texture colors"
Assert-True ($missingTextureReport.transferStats.materialDiffuseVoxels -gt 0) "missing texture expected diffuse fallback"
Assert-True ($missingTextureReport.transferStats.missingTextureVoxels -gt 0) "missing texture expected missing count"
Assert-True ($noUvReport.transferStats.fallbackVoxels -gt 0) "no UV expected fallback voxels"
Assert-True ($noUvReport.transferStats.missingUvVoxels -gt 0) "no UV expected missing UV count"
Assert-True ($openMeshReport.meshDiagnostics.boundaryEdges -gt 0) "open mesh expected boundary edges"
Assert-True ($openMeshReport.errors.Count -gt 0) "open mesh expected report errors"
Assert-True ($nonManifoldReport.meshDiagnostics.nonManifoldEdges -gt 0) "non-manifold mesh expected non-manifold edges"
Assert-True ($nonManifoldReport.errors.Count -gt 0) "non-manifold mesh expected report errors"

$shellDifference = [math]::Abs([int]$objReport.stats.shellVoxels - [int]$threeMfReport.stats.shellVoxels)
$shellTolerance = [math]::Max(1, [math]::Ceiling([int]$objReport.stats.shellVoxels * 0.01))
Assert-True ($shellDifference -le $shellTolerance) "OBJ/3MF shell voxel difference exceeds 1% tolerance"

Write-Host "Surface shell real-model tests complete."
