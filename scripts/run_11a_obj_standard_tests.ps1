param(
    [string]$BuildDir = "build",
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

function Find-Executable([string]$Name) {
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

function Read-Json([string]$Path) {
    Assert-True (Test-Path -LiteralPath $Path) "expected JSON file: $Path"
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

$configPath = "samples/configs/obj_standard/standard_obj_texture_legacy.json"
$packageDir = "output/ObjStandardTemplateLegacy"
$diagnosticReport = "output/ObjStandardTemplateOpenVdbDiagnostic/reports/experimental_openvdb_shell_report.json"

Write-Host "== build 11A standard OBJ targets"
& cmake --build $BuildDir --config $Config --target slicer_cli
if ($LASTEXITCODE -ne 0) { throw "slicer_cli build failed" }
& cmake --build $BuildDir --config $Config --target rip_reader_test
if ($LASTEXITCODE -ne 0) { throw "rip_reader_test build failed" }
& cmake --build $BuildDir --config $Config --target slicer_debug_ui
if ($LASTEXITCODE -ne 0) { throw "slicer_debug_ui build failed" }

$slicerCli = Find-Executable "slicer_cli"
$ripReader = Find-Executable "rip_reader_test"
$uiExe = Join-Path $BuildDir "apps/slicer_debug_ui/$Config/slicer_debug_ui.exe"
Assert-True (Test-Path -LiteralPath $uiExe) "slicer_debug_ui.exe was not found under $BuildDir"

Write-Host "== inspect standard OBJ template"
& $slicerCli --config $configPath --inspect-model
if ($LASTEXITCODE -ne 0) { throw "inspect-model failed" }

Write-Host "== slice standard OBJ legacy package"
& $slicerCli --config $configPath
if ($LASTEXITCODE -ne 0) { throw "standard OBJ legacy slicing failed" }

Write-Host "== RIP summary"
& $ripReader --package $packageDir --summary
if ($LASTEXITCODE -ne 0) { throw "rip_reader_test summary failed" }

Write-Host "== UI self-test"
& $uiExe --self-test
if ($LASTEXITCODE -ne 0) { throw "slicer_debug_ui self-test failed" }

Write-Host "== OpenVDB diagnostic"
& $slicerCli --config $configPath --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report $diagnosticReport
if ($LASTEXITCODE -ne 0) { throw "OpenVDB diagnostic failed" }

$manifest = Read-Json (Join-Path $packageDir "manifest.json")
Assert-Equal $manifest.schema "p0.rgbwsv.2" "manifest schema mismatch"
Assert-Equal $manifest.tiff.bitDepth 8 "manifest bitDepth mismatch"
Assert-Equal $manifest.tiff.polarity "black_is_print" "manifest polarity mismatch"
Assert-Equal $manifest.tiff.printValue 0 "manifest printValue mismatch"
Assert-Equal $manifest.tiff.emptyValue 255 "manifest emptyValue mismatch"
Assert-True ($manifest.layers.Count -gt 0) "expected at least one layer"

$textureReport = Read-Json (Join-Path $packageDir "reports/texture_report.json")
Assert-Equal $textureReport.enabled $true "texture report enabled mismatch"
Assert-True ($textureReport.sampledPixels -gt 0) "expected sampled texture pixels"
Assert-Equal $textureReport.missingTextures 0 "standard OBJ texture should exist"

$sliceReport = Read-Json (Join-Path $packageDir "reports/slice_report.json")
Assert-True ($sliceReport.totals.modelPixels -gt 0) "expected model pixels"
Assert-True ($sliceReport.totals.supportPixels -gt 0) "expected support pixels"

$diagnostic = Read-Json $diagnosticReport
Assert-Equal $diagnostic.schema "p0.experimental_openvdb_shell_cli_report.1" "diagnostic schema mismatch"
Assert-Equal $diagnostic.productionPackageWritten $false "diagnostic must not write production package"
Assert-Equal $diagnostic.productionAdmission.productionAllowed $false "diagnostic must not be production allowed"

Write-Host "11A standard OBJ legacy and diagnostic tests complete."
