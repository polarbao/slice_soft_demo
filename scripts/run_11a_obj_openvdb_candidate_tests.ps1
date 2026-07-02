param(
    [string]$OpenVdbBuildDir = "build-openvdb-09b-r1",
    [string]$Config = "Debug"
)

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

function Read-Json([string]$Path) {
    Assert-True (Test-Path -LiteralPath $Path) "expected JSON file: $Path"
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Contains-Code($Codes, [string]$Expected) {
    foreach ($code in $Codes) {
        if ($code -eq $Expected) {
            return $true
        }
    }
    return $false
}

$configPath = "samples/configs/obj_standard/standard_obj_texture_legacy.json"
$outputDir = "output/ObjStandardTemplateOpenVdbProbe"
$reportPath = Join-Path $outputDir "reports/surface_shell_texture_report.json"
$demoExe = Join-Path $OpenVdbBuildDir "$Config/surface_shell_real_model_demo.exe"

if (-not (Test-Path -LiteralPath (Join-Path $OpenVdbBuildDir "CMakeCache.txt"))) {
    throw "OpenVDB build directory is not configured: $OpenVdbBuildDir"
}

Write-Host "== build OpenVDB real-model demo"
& cmake --build $OpenVdbBuildDir --config $Config --target surface_shell_real_model_demo
if ($LASTEXITCODE -ne 0) {
    throw "surface_shell_real_model_demo build failed"
}
Assert-True (Test-Path -LiteralPath $demoExe) "surface_shell_real_model_demo.exe was not found"

Write-Host "== probe standard OBJ OpenVDB candidate admission"
& $demoExe --config $configPath --voxel-mm 0.10 --shell-mm 0.15 --mesh-policy strict_closed --output $outputDir
$exitCode = $LASTEXITCODE
if ($exitCode -eq 0) {
    throw "standard OBJ unexpectedly passed strict_closed OpenVDB candidate probe"
}

$report = Read-Json $reportPath
Assert-Equal $report.schema "p0.surface_shell_texture_report.2" "probe report schema mismatch"
Assert-Equal $report.productionAdmission.productionAllowed $false "standard OBJ should not be production allowed"
Assert-True (Contains-Code $report.productionAdmission.blockerCodes "MESH_BOUNDARY_EDGES") "expected boundary edge blocker"
Assert-True (Contains-Code $report.productionAdmission.blockerCodes "MESH_NON_MANIFOLD_EDGES") "expected non-manifold blocker"

$candidateManifest = Join-Path "output/ObjStandardTemplateOpenVdbCandidate" "manifest.json"
Assert-True (-not (Test-Path -LiteralPath $candidateManifest)) "blocked candidate must not write manifest"

Write-Host "11A OpenVDB candidate gate test complete: standard OBJ is correctly blocked."
