param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere))
{
    throw "vswhere.exe was not found: $vswhere"
}
$installationPath = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath)
{
    throw "A Visual Studio C++ toolchain was not found."
}
$vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
$libraryDirectory = Join-Path $repoRoot "build-slicesoft\main\$Configuration"
$baseLibrary = Join-Path $libraryDirectory "slicer_base.lib"
$engineLibrary = Join-Path $libraryDirectory "slicer_engine.lib"
if (-not (Test-Path -LiteralPath $baseLibrary) `
    -or -not (Test-Path -LiteralPath $engineLibrary))
{
    throw "Build slicer_base and slicer_engine first: $libraryDirectory"
}

$outputDirectory = Join-Path $repoRoot "build-slicesoft\stage14b_02\$Configuration"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executable = Join-Path $outputDirectory "stage14b_02_tests.exe"
$programDatabase = Join-Path $outputDirectory "stage14b_02_tests.pdb"
$objectDirectory = $outputDirectory.Replace("\", "/") + "/"
$runtime = if ($Configuration -eq "Debug") { "/MDd /Od /Zi" } else { "/MD /O2" }
$sources = @(
    (Join-Path $repoRoot "tests\stage14b_02\Main.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\ModelFacadeImplementation.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadeImplementation.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadeCommon.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadePackage.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadePreview.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadePreviewEncoding.cpp"),
    (Join-Path $repoRoot "src\slicer_core\api\implementation\PackageQueryFacadeReport.cpp")
)
$quotedSources = ($sources | ForEach-Object { '"' + $_ + '"' }) -join " "
$includeRoot = Join-Path $repoRoot "src"
$includeMiniz = Join-Path $repoRoot "src\third_party\miniz"
$compileCommand = @"
call "$vsDevCmd" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++20 /EHsc /permissive- /W4 $runtime /Fd:"$programDatabase" /I"$includeRoot" /I"$includeMiniz" $quotedSources /Fe:"$executable" /Fo:"$objectDirectory" /link "$engineLibrary" "$baseLibrary" windowscodecs.lib ole32.lib psapi.lib
"@.Trim()

& cmd.exe /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0)
{
    throw "Stage 14B-02 $Configuration compilation failed with exit code $LASTEXITCODE."
}
& $executable
if ($LASTEXITCODE -ne 0)
{
    throw "Stage 14B-02 $Configuration tests failed with exit code $LASTEXITCODE."
}
