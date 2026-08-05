param(
    [string]$BuildDir = "build-slicesoft/14b03a-independent"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$outputDir = Join-Path $repoRoot $BuildDir
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$compiler = (Get-Command g++.exe -ErrorAction SilentlyContinue).Source
if (-not $compiler) {
    $compiler = "D:/Program Files Tools/w64devkit/bin/g++.exe"
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Stage 14B-03A independent compiler was not found."
}

$sources = @(
    "tests/stage14b_03a/Main.cpp",
    "tests/stage14b_03a/PositiveCases.cpp",
    "tests/stage14b_03a/FailureCases.cpp",
    "src/slicer_core/api/viewdata/TexturedSceneViewDataProvider.cpp",
    "src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp",
    "src/slicer_core/api/viewdata/SceneViewAppearanceBudget.cpp",
    "src/slicer_core/api/viewdata/SceneViewCandidateBuilder.cpp",
    "src/slicer_core/api/viewdata/SceneViewMeshBuilder.cpp",
    "src/slicer_core/api/viewdata/SceneViewOutlineBuilder.cpp",
    "src/slicer_core/api/viewdata/SceneSurfacePreviewBuilder.cpp",
    "src/slicer_core/api/viewdata/SceneViewIdentity.cpp",
    "src/slicer_core/api/viewdata/SceneViewBudget.cpp",
    "src/slicer_core/api/viewdata/SceneViewClosureValidator.cpp",
    "src/slicer_core/system/Sha256.cpp"
)
$executable = Join-Path $outputDir "stage14b03a_tests.exe"
$arguments = @(
    "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
    "-Isrc", "-Itests/stage14b_03a"
)
$arguments += $sources
$arguments += @("-o", $executable)

Push-Location $repoRoot
try {
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Stage 14B-03A independent compilation failed."
    }
    & $executable
    if ($LASTEXITCODE -ne 0) {
        throw "Stage 14B-03A behavior tests failed."
    }
    python tests/stage14b_03a/ValidateRealFixtures.py
    if ($LASTEXITCODE -ne 0) {
        throw "Stage 14B-03A real fixture guard failed."
    }
}
finally {
    Pop-Location
}
