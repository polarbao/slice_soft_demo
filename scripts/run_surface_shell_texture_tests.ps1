param(
    [string]$BuildDir = "build-openvdb-09b",
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
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Get-DemoExe([string]$BuildDir, [string]$Config) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/surface_shell_texture_demo.exe"),
        (Join-Path $BuildDir "surface_shell_texture_demo.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "surface_shell_texture_demo.exe was not found under $BuildDir"
}

function Get-UnitTestExe([string]$BuildDir, [string]$Config) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/surface_shell_texture_unit_tests.exe"),
        (Join-Path $BuildDir "surface_shell_texture_unit_tests.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "surface_shell_texture_unit_tests.exe was not found under $BuildDir"
}

function Run-Demo([string]$DemoExe, [string]$OutputDir, [double]$ShellMm) {
    Write-Host "== surface shell texture shell-mm=$ShellMm"
    & $DemoExe --case generated-box --voxel-mm 0.05 --shell-mm $ShellMm --texture-source checker --output $OutputDir
    if ($LASTEXITCODE -ne 0) {
        throw "surface_shell_texture_demo failed for shell-mm=$ShellMm"
    }

    $reportPath = Join-Path $OutputDir "reports/surface_shell_texture_report.json"
    Assert-True (Test-Path -LiteralPath $reportPath) "expected surface shell report: $reportPath"

    $previewDir = Join-Path $OutputDir "preview"
    Assert-True ((Get-ChildItem -LiteralPath $previewDir -Filter "shell_layer_*.png").Count -gt 0) "expected shell preview"
    Assert-True ((Get-ChildItem -LiteralPath $previewDir -Filter "interior_layer_*.png").Count -gt 0) "expected interior preview"
    Assert-True ((Get-ChildItem -LiteralPath $previewDir -Filter "composite_layer_*.png").Count -gt 0) "expected composite preview"

    $report = Read-Json $reportPath
    Assert-Equal $report.schema "p0.surface_shell_texture_report.1" "report schema mismatch"
    Assert-Equal $report.caseName "generated-box" "caseName mismatch"
    Assert-Equal $report.openvdb.enabled $true "OpenVDB must be enabled"
    Assert-Equal $report.openvdb.available $true "OpenVDB must be available"
    Assert-True ($report.openvdb.activeVoxels -gt 0) "expected active voxels"
    Assert-True ($report.stats.insideVoxels -gt 0) "expected inside voxels"
    Assert-True ($report.stats.shellVoxels -gt 0) "expected shell voxels"
    Assert-True ($report.stats.interiorVoxels -gt 0) "expected interior voxels"
    Assert-Equal $report.stats.outsideColoredVoxels 0 "outsideColoredVoxels mismatch"
    Assert-Equal $report.stats.unclassifiedVoxels 0 "unclassifiedVoxels mismatch"
    Assert-Equal $report.stats.shellPlusInteriorEqualsInside $true "shell + interior invariant mismatch"
    return $report
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "OpenVDB 09B build directory is not configured: $BuildDir"
}

Write-Host "== build surface shell texture targets"
& cmake --build $BuildDir --config $Config --target surface_shell_texture_demo
if ($LASTEXITCODE -ne 0) {
    throw "surface_shell_texture_demo build failed"
}
& cmake --build $BuildDir --config $Config --target surface_shell_texture_unit_tests
if ($LASTEXITCODE -ne 0) {
    throw "surface_shell_texture_unit_tests build failed"
}

$demoExe = Get-DemoExe $BuildDir $Config
$unitTestExe = Get-UnitTestExe $BuildDir $Config

Write-Host "== surface shell texture unit tests"
& $unitTestExe
if ($LASTEXITCODE -ne 0) {
    throw "surface_shell_texture_unit_tests failed"
}

$report005 = Run-Demo $demoExe "output/SurfaceShellTexture005" 0.05
$report010 = Run-Demo $demoExe "output/SurfaceShellTexture010" 0.10
$report020 = Run-Demo $demoExe "output/SurfaceShellTexture020" 0.20

Assert-True ($report010.stats.shellVoxels -ge $report005.stats.shellVoxels) "shell 0.10 must be >= shell 0.05"
Assert-True ($report020.stats.shellVoxels -ge $report010.stats.shellVoxels) "shell 0.20 must be >= shell 0.10"
Assert-True ($report010.stats.interiorVoxels -le $report005.stats.interiorVoxels) "interior 0.10 must be <= interior 0.05"
Assert-True ($report020.stats.interiorVoxels -le $report010.stats.interiorVoxels) "interior 0.20 must be <= interior 0.10"

Write-Host "Surface shell texture tests complete."
