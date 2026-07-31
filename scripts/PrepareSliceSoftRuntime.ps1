[CmdletBinding()]
param(
    [string]$BuildDir = "build-slicesoft/main",
    [string]$RuntimeDir = "runtime/slicesoft",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [ValidateSet("VisualStudio", "NMake")]
    [string]$BuildSystem = "VisualStudio",
    [ValidateRange(1, 64)]
    [int]$Jobs = 8,
    [string]$Qt5Dir = $env:Qt5_DIR,
    [ValidateSet("handwritten", "libtiff")]
    [string]$TiffBackend = "handwritten",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$ConfigureOnly,
    [switch]$DeployOnly,
    [switch]$SkipDeploy,
    [switch]$ForceClean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function ResolveQt5CMakeDir
{
    param([string]$Candidate)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($Candidate))
    {
        $candidates += $Candidate
        $candidates += Join-Path $Candidate "lib/cmake/Qt5"
    }
    $candidates += "C:/Qt/Qt5.15.2/5.15.2/msvc2019_64/lib/cmake/Qt5"

    foreach ($path in $candidates)
    {
        if (Test-Path -LiteralPath (Join-Path $path "Qt5Config.cmake"))
        {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    throw "Qt5Config.cmake was not found. Set Qt5_DIR or pass -Qt5Dir."
}

function ResolveRepoPath
{
    param(
        [string]$RepoRoot,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function AssertPathUnderRepo
{
    param(
        [string]$RepoRoot,
        [string]$Path,
        [string]$Purpose
    )

    $rootPrefix = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "$Purpose must stay under the repository root: $resolved"
    }
}

function ResolveVsDevCmd
{
    if (-not [string]::IsNullOrWhiteSpace($env:VSINSTALLDIR))
    {
        $candidate = Join-Path $env:VSINSTALLDIR "Common7/Tools/VsDevCmd.bat"
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $vsWhere = "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    if (Test-Path -LiteralPath $vsWhere)
    {
        $installationPath = & $vsWhere `
            -latest `
            -products "*" `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if (-not [string]::IsNullOrWhiteSpace($installationPath))
        {
            $candidate = Join-Path $installationPath.Trim() "Common7/Tools/VsDevCmd.bat"
            if (Test-Path -LiteralPath $candidate)
            {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $verifiedCandidate = "D:/Program Files Tools/VS/vs_2026/Common7/Tools/VsDevCmd.bat"
    if (Test-Path -LiteralPath $verifiedCandidate)
    {
        return (Resolve-Path -LiteralPath $verifiedCandidate).Path
    }
    throw "VsDevCmd.bat was not found. Install the MSVC x64 build tools or run from a VS developer shell."
}

function ImportVisualStudioX64Environment
{
    $vsDevCmd = ResolveVsDevCmd
    $environmentLines = & $env:ComSpec /d /s /c "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
    if ($LASTEXITCODE -ne 0)
    {
        throw "VsDevCmd.bat failed with exit code $LASTEXITCODE."
    }
    foreach ($line in $environmentLines)
    {
        if ($line -match '^([^=]+)=(.*)$')
        {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function ResolveVcpkgRoot
{
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate))
    {
        throw "VCPKG_ROOT or -VcpkgRoot is required for the libtiff lane."
    }

    $resolved = [System.IO.Path]::GetFullPath($Candidate)
    $toolchain = Join-Path $resolved "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf))
    {
        throw "vcpkg toolchain was not found: $toolchain"
    }
    return $resolved
}

function SetVisualStudioRuntimeDiscoveryEnvironment
{
    if (-not [string]::IsNullOrWhiteSpace($env:VCINSTALLDIR))
    {
        return
    }

    $vsDevCmd = ResolveVsDevCmd
    $toolsDir = Split-Path -Parent $vsDevCmd
    $common7Dir = Split-Path -Parent $toolsDir
    $installDir = Split-Path -Parent $common7Dir
    $vcInstallDir = Join-Path $installDir "VC"
    if (-not (Test-Path -LiteralPath $vcInstallDir -PathType Container))
    {
        throw "Visual Studio VC directory was not found: $vcInstallDir"
    }

    $env:VSINSTALLDIR = "$installDir$([System.IO.Path]::DirectorySeparatorChar)"
    $env:VCINSTALLDIR = "$vcInstallDir$([System.IO.Path]::DirectorySeparatorChar)"
}

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

function GetRuntimeBuildInputFingerprint
{
    param(
        [string]$RepoRoot,
        [string]$Config,
        [string]$BuildSystem,
        [string]$Qt5Dir,
        [string]$TiffBackend,
        [string]$VcpkgRoot,
        [bool]$HashFileContents
    )

    $inputFiles = @(
        Get-ChildItem -LiteralPath (Join-Path $RepoRoot "src") -Recurse -File |
            Where-Object {
                $_.Extension -in @(
                    ".c", ".cc", ".cpp", ".cxx",
                    ".h", ".hh", ".hpp", ".hxx", ".inl")
            }
        Get-ChildItem -LiteralPath (Join-Path $RepoRoot "apps") -Recurse -File |
            Where-Object {
                $_.Extension -in @(
                    ".c", ".cc", ".cpp", ".cxx",
                    ".h", ".hh", ".hpp", ".hxx", ".inl")
            }
        Get-ChildItem -LiteralPath $RepoRoot -Recurse -Filter "CMakeLists.txt" -File |
            Where-Object {
                $_.FullName -notmatch
                    '[\\/](build|runtime|output)(-[^\\/]*)?[\\/]'
            }
        Get-Item -LiteralPath (Join-Path $RepoRoot "CMakeLists.txt")
        Get-Item -LiteralPath (Join-Path $RepoRoot "vcpkg.json")
        Get-Item -LiteralPath $PSCommandPath
    ) | Sort-Object FullName -Unique

    $manifest = [System.Text.StringBuilder]::new()
    [void]$manifest.AppendLine("config=$Config")
    [void]$manifest.AppendLine("buildSystem=$BuildSystem")
    [void]$manifest.AppendLine("qt5Dir=$Qt5Dir")
    [void]$manifest.AppendLine("tiffBackend=$TiffBackend")
    [void]$manifest.AppendLine("vcpkgRoot=$VcpkgRoot")
    $rootPrefix = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    foreach ($file in $inputFiles)
    {
        $fullPath = [System.IO.Path]::GetFullPath($file.FullName)
        if (-not $fullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
        {
            throw "Runtime build input must stay under the repository root: $fullPath"
        }
        $relativePath = $fullPath.Substring($rootPrefix.Length).Replace('\', '/')
        if ($HashFileContents)
        {
            $contentHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            [void]$manifest.AppendLine("$relativePath|$contentHash")
        }
        else
        {
            [void]$manifest.AppendLine(
                "$relativePath|$($file.Length)|$($file.LastWriteTimeUtc.Ticks)")
        }
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($manifest.ToString())
        return ([System.BitConverter]::ToString($sha256.ComputeHash($bytes))).Replace("-", "")
    }
    finally
    {
        $sha256.Dispose()
    }
}

function TestRuntimeCleanBuildRequired
{
    param(
        [string]$StampPath,
        [string]$CurrentFingerprint,
        [bool]$ForceClean
    )

    if ($ForceClean -or -not (Test-Path -LiteralPath $StampPath -PathType Leaf))
    {
        return $true
    }

    $previousFingerprint = (Get-Content -LiteralPath $StampPath -Raw).Trim()
    return $previousFingerprint -ne $CurrentFingerprint
}

function ResolveBuiltExecutable
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

function ReadTiffRuntimeMetadata
{
    param(
        [string]$BuildRoot,
        [string]$Config
    )

    $metadataPath = Join-Path $BuildRoot "slicesoft_tiff_runtime_$Config.txt"
    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf))
    {
        throw "TIFF runtime metadata was not generated: $metadataPath"
    }

    $values = @{}
    foreach ($line in Get-Content -LiteralPath $metadataPath)
    {
        if ($line -match '^([^=]+)=(.*)$')
        {
            $values[$Matches[1]] = $Matches[2]
        }
    }

    foreach ($requiredKey in @("backend", "runtime", "license"))
    {
        if (-not $values.ContainsKey($requiredKey))
        {
            throw "TIFF runtime metadata is missing '$requiredKey': $metadataPath"
        }
    }

    return [pscustomobject]@{
        backend = [string]$values["backend"]
        runtime = [string]$values["runtime"]
        license = [string]$values["license"]
        source = $metadataPath
    }
}

function AssertRuntimeDirectoryNotInUse
{
    param([string]$RuntimeDir)

    if (-not (Test-Path -LiteralPath $RuntimeDir -PathType Container))
    {
        return
    }

    $runtimePrefix =
        [System.IO.Path]::GetFullPath($RuntimeDir).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    foreach ($processName in @("slicer_debug_ui", "slicer_cli", "rip_reader_test"))
    {
        foreach ($process in @(Get-Process -Name $processName -ErrorAction SilentlyContinue))
        {
            try
            {
                $processPath = [System.IO.Path]::GetFullPath($process.Path)
                if ($processPath.StartsWith(
                    $runtimePrefix,
                    [System.StringComparison]::OrdinalIgnoreCase))
                {
                    throw "Runtime process is still running: $processPath (PID $($process.Id)). Close it before deploying '$RuntimeDir'."
                }
            }
            catch [System.ComponentModel.Win32Exception]
            {
                continue
            }
        }
    }
}

function ReadOptionalStringProperty
{
    param(
        [object]$Object,
        [string]$Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value)
    {
        return ""
    }
    return [string]$property.Value
}

function CopyProfileRuntimeResources
{
    param(
        [string]$RepoRoot,
        [string]$StagingDir
    )

    foreach ($relativeDirectory in @("samples", "model"))
    {
        $source = Join-Path $RepoRoot $relativeDirectory
        if (-not (Test-Path -LiteralPath $source -PathType Container))
        {
            throw "Profile runtime resource directory was not found: $source"
        }
        Copy-Item `
            -LiteralPath $source `
            -Destination (Join-Path $StagingDir $relativeDirectory) `
            -Recurse
    }

    $registryPath = Join-Path $StagingDir "samples/scenarios/slicer_scenarios.json"
    if (-not (Test-Path -LiteralPath $registryPath -PathType Leaf))
    {
        throw "Packaged scenario registry was not found: $registryPath"
    }
    $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
    $profileDocs = @(
        $registry.scenarios |
            ForEach-Object { ReadOptionalStringProperty -Object $_ -Name "docPath" } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    foreach ($relativeDocPath in $profileDocs)
    {
        $sourceDoc = ResolveRepoPath -RepoRoot $RepoRoot -Path $relativeDocPath
        AssertPathUnderRepo -RepoRoot $RepoRoot -Path $sourceDoc -Purpose "Profile document"
        if (-not (Test-Path -LiteralPath $sourceDoc -PathType Leaf))
        {
            throw "Profile document was not found: $sourceDoc"
        }
        $destinationDoc = Join-Path $StagingDir $relativeDocPath
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationDoc) -Force | Out-Null
        Copy-Item -LiteralPath $sourceDoc -Destination $destinationDoc
    }

    return [pscustomobject]@{
        scenarioCount = @($registry.scenarios).Count
        profileDocumentCount = $profileDocs.Count
    }
}

function TestPortableProfileResources
{
    param([string]$RuntimeRoot)

    $registryRelativePath = "samples/scenarios/slicer_scenarios.json"
    $registryPath = Join-Path $RuntimeRoot $registryRelativePath
    $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
    if ([string]$registry.schema -ne "slice_soft.scenarios.2")
    {
        throw "Packaged scenario registry schema is invalid: $($registry.schema)"
    }

    $scenarioIds = @($registry.scenarios | ForEach-Object { [string]$_.id })
    if ([string]::IsNullOrWhiteSpace([string]$registry.defaultScenarioId) `
        -or -not $scenarioIds.Contains([string]$registry.defaultScenarioId))
    {
        throw "Packaged scenario registry has an invalid defaultScenarioId."
    }

    foreach ($scenario in $registry.scenarios)
    {
        $scenarioId = [string]$scenario.id
        $configRelativePath = [string]$scenario.configPath
        if ([string]::IsNullOrWhiteSpace($scenarioId) `
            -or [string]::IsNullOrWhiteSpace($configRelativePath))
        {
            throw "Packaged scenario is missing id or configPath."
        }
        if ([System.IO.Path]::IsPathRooted($configRelativePath))
        {
            throw "Scenario configPath must be portable: $scenarioId -> $configRelativePath"
        }

        $configPath = [System.IO.Path]::GetFullPath((Join-Path $RuntimeRoot $configRelativePath))
        AssertPathUnderRepo -RepoRoot $RuntimeRoot -Path $configPath -Purpose "Scenario config"
        if (-not (Test-Path -LiteralPath $configPath -PathType Leaf))
        {
            throw "Scenario config was not packaged: $scenarioId -> $configRelativePath"
        }
        $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json

        $modelPath = [string]$config.input.modelPath
        if ([string]::IsNullOrWhiteSpace($modelPath) -or [System.IO.Path]::IsPathRooted($modelPath))
        {
            throw "Scenario modelPath must be a portable relative path: $scenarioId -> $modelPath"
        }
        $modelCandidates = @(
            [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $configPath) $modelPath)),
            [System.IO.Path]::GetFullPath((Join-Path $RuntimeRoot $modelPath))
        )
        $modelFound = $false
        foreach ($candidate in $modelCandidates)
        {
            AssertPathUnderRepo -RepoRoot $RuntimeRoot -Path $candidate -Purpose "Scenario model"
            if (Test-Path -LiteralPath $candidate -PathType Leaf)
            {
                $modelFound = $true
                break
            }
        }
        if (-not $modelFound)
        {
            throw "Scenario model was not packaged: $scenarioId -> $modelPath"
        }

        $packageDir = [string]$config.output.packageDir
        if ([string]::IsNullOrWhiteSpace($packageDir) -or [System.IO.Path]::IsPathRooted($packageDir))
        {
            throw "Scenario output.packageDir must be portable: $scenarioId -> $packageDir"
        }

        $docRelativePath = ReadOptionalStringProperty -Object $scenario -Name "docPath"
        if (-not [string]::IsNullOrWhiteSpace($docRelativePath))
        {
            if ([System.IO.Path]::IsPathRooted($docRelativePath))
            {
                throw "Scenario docPath must be portable: $scenarioId -> $docRelativePath"
            }
            $docPath = [System.IO.Path]::GetFullPath((Join-Path $RuntimeRoot $docRelativePath))
            AssertPathUnderRepo -RepoRoot $RuntimeRoot -Path $docPath -Purpose "Scenario document"
            if (-not (Test-Path -LiteralPath $docPath -PathType Leaf))
            {
                throw "Scenario document was not packaged: $scenarioId -> $docRelativePath"
            }
        }
    }

    return [pscustomobject]@{
        registry = $registryRelativePath
        scenarioCount = @($registry.scenarios).Count
        defaultScenarioId = [string]$registry.defaultScenarioId
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedQt5Dir = ResolveQt5CMakeDir -Candidate $Qt5Dir
$resolvedVcpkgRoot =
    if ($TiffBackend -eq "libtiff")
    {
        ResolveVcpkgRoot -Candidate $VcpkgRoot
    }
    else
    {
        ""
    }
$resolvedBuildDir = ResolveRepoPath -RepoRoot $repoRoot -Path $BuildDir
$isNMakeBuild = $BuildSystem -eq "NMake"
$resolvedConfigBuildDir =
    if ($isNMakeBuild)
    {
        Join-Path $resolvedBuildDir $Config
    }
    else
    {
        $resolvedBuildDir
    }
$resolvedRuntimeRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $RuntimeDir
$resolvedRuntimeDir = Join-Path $resolvedRuntimeRoot $Config

AssertPathUnderRepo -RepoRoot $repoRoot -Path $resolvedBuildDir -Purpose "BuildDir"
AssertPathUnderRepo -RepoRoot $repoRoot -Path $resolvedConfigBuildDir -Purpose "Config build directory"
AssertPathUnderRepo -RepoRoot $repoRoot -Path $resolvedRuntimeDir -Purpose "RuntimeDir"

if ($isNMakeBuild)
{
    ImportVisualStudioX64Environment
}

Push-Location $repoRoot
try
{
    if ($DeployOnly -and ($ConfigureOnly -or $SkipDeploy))
    {
        throw "DeployOnly cannot be combined with ConfigureOnly or SkipDeploy."
    }

    if (-not $DeployOnly)
    {
        $buildInputFingerprint = GetRuntimeBuildInputFingerprint `
            -RepoRoot $repoRoot `
            -Config $Config `
            -BuildSystem $BuildSystem `
            -Qt5Dir $resolvedQt5Dir `
            -TiffBackend $TiffBackend `
            -VcpkgRoot $resolvedVcpkgRoot `
            -HashFileContents $isNMakeBuild
        $buildInputStampPath =
            Join-Path $resolvedConfigBuildDir ".slicesoft_build_input.$Config.sha256"
        $cleanBuildRequired =
            if ($isNMakeBuild)
            {
                TestRuntimeCleanBuildRequired `
                    -StampPath $buildInputStampPath `
                    -CurrentFingerprint $buildInputFingerprint `
                    -ForceClean $ForceClean.IsPresent
            }
            else
            {
                $ForceClean.IsPresent
            }

        $configureArguments = @(
            "-S", $repoRoot,
            "-B", $resolvedConfigBuildDir
        )
        if ($isNMakeBuild)
        {
            $configureArguments += @(
                "-G", "NMake Makefiles",
                "-DCMAKE_BUILD_TYPE=$Config"
            )
        }
        else
        {
            $configureArguments += @(
                "-G", "Visual Studio 18 2026",
                "-A", "x64"
            )
        }
        $configureArguments += @(
            "-DQt5_DIR=$resolvedQt5Dir",
            "-DBUILD_SLICER_DEBUG_UI=ON",
            "-DUSE_OPENVDB=OFF",
            "-DSLICESOFT_TIFF_BACKEND=$TiffBackend"
        )
        if ($TiffBackend -eq "libtiff")
        {
            $configureArguments += @(
                "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $resolvedVcpkgRoot 'scripts/buildsystems/vcpkg.cmake')",
                "-DVCPKG_TARGET_TRIPLET=x64-windows"
            )
        }

        InvokeNativeStep `
            -Name "configure unified SliceSoft Qt build ($BuildSystem)" `
            -Executable "cmake" `
            -Arguments $configureArguments

        if ($ConfigureOnly)
        {
            Write-Host "SliceSoft configure passed: $resolvedConfigBuildDir"
            exit 0
        }

        $buildArguments = @("--build", $resolvedConfigBuildDir)
        if (-not $isNMakeBuild)
        {
            $buildArguments += @("--config", $Config)
        }
        if ($cleanBuildRequired)
        {
            if ($isNMakeBuild -and -not $ForceClean)
            {
                Write-Host "Runtime inputs changed; performing one clean NMake rebuild to prevent stale objects."
            }
            else
            {
                Write-Host "ForceClean requested; performing one clean rebuild."
            }
            $buildArguments += "--clean-first"
        }
        $buildArguments += @(
            "--parallel", [string]$Jobs,
            "--target", "slicesoft_runtime"
        )
        InvokeNativeStep `
            -Name "build SliceSoft runtime targets ($Config)" `
            -Executable "cmake" `
            -Arguments $buildArguments

        $postBuildInputFingerprint =
            GetRuntimeBuildInputFingerprint `
                -RepoRoot $repoRoot `
                -Config $Config `
                -BuildSystem $BuildSystem `
                -Qt5Dir $resolvedQt5Dir `
                -TiffBackend $TiffBackend `
                -VcpkgRoot $resolvedVcpkgRoot `
                -HashFileContents $isNMakeBuild
        if ($postBuildInputFingerprint -ne $buildInputFingerprint)
        {
            throw "Runtime build inputs changed while the build was running. The runtime was not deployed; rerun PrepareSliceSoftRuntime.ps1 after source edits have stopped."
        }

        [System.IO.File]::WriteAllText(
            $buildInputStampPath,
            "$buildInputFingerprint$([Environment]::NewLine)",
            [System.Text.UTF8Encoding]::new($false))

        if ($SkipDeploy)
        {
            Write-Host "SliceSoft build passed without deployment: $resolvedConfigBuildDir"
            exit 0
        }
    }
    else
    {
        Write-Host "Deploying existing SliceSoft build artifacts: $resolvedConfigBuildDir"
    }

    $slicerCli = ResolveBuiltExecutable `
        -BuildRoot $resolvedConfigBuildDir `
        -Candidates @("$Config/slicer_cli.exe", "slicer_cli.exe")
    $ripReader = ResolveBuiltExecutable `
        -BuildRoot $resolvedConfigBuildDir `
        -Candidates @("$Config/rip_reader_test.exe", "rip_reader_test.exe")
    $uiExecutable = ResolveBuiltExecutable `
        -BuildRoot $resolvedConfigBuildDir `
        -Candidates @(
            "apps/slicer_debug_ui/$Config/slicer_debug_ui.exe",
            "apps/slicer_debug_ui/slicer_debug_ui.exe"
        )
    $tiffRuntimeMetadata =
        ReadTiffRuntimeMetadata -BuildRoot $resolvedConfigBuildDir -Config $Config
    if ($tiffRuntimeMetadata.backend -ne $TiffBackend)
    {
        throw (
            "Requested TIFF backend '$TiffBackend' does not match the build " +
            "metadata backend '$($tiffRuntimeMetadata.backend)'.")
    }

    $qtRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedQt5Dir "../../.."))
    $winDeployQt = Join-Path $qtRoot "bin/windeployqt.exe"
    if (-not (Test-Path -LiteralPath $winDeployQt))
    {
        throw "windeployqt.exe was not found: $winDeployQt"
    }
    SetVisualStudioRuntimeDiscoveryEnvironment
    AssertRuntimeDirectoryNotInUse -RuntimeDir $resolvedRuntimeDir

    $runtimeParent = Split-Path -Parent $resolvedRuntimeDir
    New-Item -ItemType Directory -Path $runtimeParent -Force | Out-Null
    $stagingDir = Join-Path $runtimeParent (".{0}.staging.{1}.{2}" -f $Config, $PID, [DateTime]::UtcNow.Ticks)
    AssertPathUnderRepo -RepoRoot $repoRoot -Path $stagingDir -Purpose "Runtime staging directory"
    New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

    try
    {
        Copy-Item -LiteralPath $slicerCli -Destination (Join-Path $stagingDir "slicer_cli.exe")
        Copy-Item -LiteralPath $ripReader -Destination (Join-Path $stagingDir "rip_reader_test.exe")
        Copy-Item -LiteralPath $uiExecutable -Destination (Join-Path $stagingDir "slicer_debug_ui.exe")
        $tiffRuntimeLibraries = @()
        $tiffLicenseRelativePath = $null
        if ($TiffBackend -eq "libtiff")
        {
            if (-not (Test-Path -LiteralPath $tiffRuntimeMetadata.runtime -PathType Leaf))
            {
                throw "LibTIFF runtime file was not found: $($tiffRuntimeMetadata.runtime)"
            }
            if ([System.IO.Path]::GetExtension($tiffRuntimeMetadata.runtime) -ne ".dll")
            {
                throw (
                    "The 03D dynamic runtime lane expected a LibTIFF DLL but got: " +
                    $tiffRuntimeMetadata.runtime)
            }
            $runtimeFileName =
                [System.IO.Path]::GetFileName($tiffRuntimeMetadata.runtime)
            $runtimeDestination = Join-Path $stagingDir $runtimeFileName
            Copy-Item `
                -LiteralPath $tiffRuntimeMetadata.runtime `
                -Destination $runtimeDestination
            $tiffRuntimeLibraries += [ordered]@{
                file = $runtimeFileName
                sha256 = (Get-FileHash `
                    -LiteralPath $runtimeDestination `
                    -Algorithm SHA256).Hash.ToLowerInvariant()
            }

            if (-not (Test-Path -LiteralPath $tiffRuntimeMetadata.license -PathType Leaf))
            {
                throw "LibTIFF license file was not found: $($tiffRuntimeMetadata.license)"
            }
            $licenseDirectory = Join-Path $stagingDir "licenses"
            New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
            $tiffLicenseRelativePath = "licenses/libtiff.txt"
            Copy-Item `
                -LiteralPath $tiffRuntimeMetadata.license `
                -Destination (Join-Path $stagingDir $tiffLicenseRelativePath)
        }

        $backendInfoOutput =
            & (Join-Path $stagingDir "slicer_cli.exe") --tiff-backend-info-json
        if ($LASTEXITCODE -ne 0)
        {
            throw "slicer_cli --tiff-backend-info-json failed with exit code $LASTEXITCODE."
        }
        $tiffBackendInfo = ($backendInfoOutput -join [Environment]::NewLine) |
            ConvertFrom-Json
        if ([string]$tiffBackendInfo.configuredBackend -ne $TiffBackend)
        {
            throw "Deployed slicer_cli TIFF backend metadata does not match '$TiffBackend'."
        }
        $profileResourceCopy = CopyProfileRuntimeResources `
            -RepoRoot $repoRoot `
            -StagingDir $stagingDir

        $deployMode = if ($Config -eq "Debug") { "--debug" } else { "--release" }
        InvokeNativeStep `
            -Name "deploy Qt runtime ($Config)" `
            -Executable $winDeployQt `
            -Arguments @(
                $deployMode,
                "--compiler-runtime",
                "--no-translations",
                "--dir", $stagingDir,
                (Join-Path $stagingDir "slicer_debug_ui.exe")
            )

        @(
            "[Paths]",
            "Plugins=."
        ) | Set-Content -LiteralPath (Join-Path $stagingDir "qt.conf") -Encoding Ascii

        $profileResourceValidation = TestPortableProfileResources -RuntimeRoot $stagingDir
        $manifest = [ordered]@{
            schema = "slicesoft.runtime.1"
            generatedAt = [DateTimeOffset]::Now.ToString("o")
            config = $Config
            buildSystem = $BuildSystem
            parallelJobs = $Jobs
            buildDir = $resolvedConfigBuildDir
            qt5Dir = $resolvedQt5Dir
            useOpenVdb = $false
            tiffWriter = [ordered]@{
                configuredBackend = [string]$tiffBackendInfo.configuredBackend
                handwrittenAvailable = [bool]$tiffBackendInfo.handwrittenAvailable
                libtiffDependencyAvailable = [bool]$tiffBackendInfo.libtiffDependencyAvailable
                libtiffWriterImplemented = [bool]$tiffBackendInfo.libtiffWriterImplemented
                libtiffStrippedWriterImplemented = [bool]$tiffBackendInfo.libtiffStrippedWriterImplemented
                libtiffTiledWriterImplemented = [bool]$tiffBackendInfo.libtiffTiledWriterImplemented
                libtiffVersion = [string]$tiffBackendInfo.libtiffVersion
                runtimeLibraries = $tiffRuntimeLibraries
                license = $tiffLicenseRelativePath
            }
            tools = [ordered]@{
                ui = "slicer_debug_ui.exe"
                slicerCli = "slicer_cli.exe"
                ripReader = "rip_reader_test.exe"
            }
            resources = [ordered]@{
                profileRegistry = $profileResourceValidation.registry
                scenarioCount = $profileResourceValidation.scenarioCount
                defaultScenarioId = $profileResourceValidation.defaultScenarioId
                sampleTree = "samples"
                modelTree = "model"
                profileDocumentCount = $profileResourceCopy.profileDocumentCount
            }
        }
        $manifest | ConvertTo-Json -Depth 4 | Set-Content `
            -LiteralPath (Join-Path $stagingDir "runtime_manifest.json") `
            -Encoding UTF8

        $backupDir = $null
        $deployInPlace = $false
        if (Test-Path -LiteralPath $resolvedRuntimeDir)
        {
            $backupDir = "$resolvedRuntimeDir.previous.$([DateTime]::UtcNow.Ticks)"
            AssertPathUnderRepo -RepoRoot $repoRoot -Path $backupDir -Purpose "Runtime backup directory"
            try
            {
                [System.IO.Directory]::Move($resolvedRuntimeDir, $backupDir)
            }
            catch
            {
                Write-Warning (
                    "Runtime directory cannot be atomically replaced; " +
                    "deploying the immutable payload in place while preserving output. " +
                    "Original error: $($_.Exception.Message)")
                $backupDir = $null
                $deployInPlace = $true
            }
        }

        try
        {
            if ($deployInPlace)
            {
                Get-ChildItem -LiteralPath $stagingDir -Force |
                    Copy-Item `
                        -Destination $resolvedRuntimeDir `
                        -Recurse `
                        -Force
            }
            else
            {
                [System.IO.Directory]::Move($stagingDir, $resolvedRuntimeDir)

                if ($null -ne $backupDir)
                {
                    $previousOutputDir = Join-Path $backupDir "output"
                    if (Test-Path -LiteralPath $previousOutputDir -PathType Container)
                    {
                        $runtimeOutputDir = Join-Path $resolvedRuntimeDir "output"
                        if (Test-Path -LiteralPath $runtimeOutputDir)
                        {
                            throw "Fresh runtime unexpectedly contains an output directory: $runtimeOutputDir"
                        }
                        [System.IO.Directory]::Move(
                            $previousOutputDir,
                            $runtimeOutputDir)
                    }
                }
            }
        }
        catch
        {
            if ($null -ne $backupDir -and -not (Test-Path -LiteralPath $resolvedRuntimeDir))
            {
                [System.IO.Directory]::Move($backupDir, $resolvedRuntimeDir)
            }
            throw
        }

        if ($null -ne $backupDir -and (Test-Path -LiteralPath $backupDir))
        {
            AssertPathUnderRepo -RepoRoot $repoRoot -Path $backupDir -Purpose "Runtime backup cleanup"
            Remove-Item -LiteralPath $backupDir -Recurse -Force
        }
    }
    finally
    {
        if (Test-Path -LiteralPath $stagingDir)
        {
            AssertPathUnderRepo -RepoRoot $repoRoot -Path $stagingDir -Purpose "Runtime staging cleanup"
            Remove-Item -LiteralPath $stagingDir -Recurse -Force
        }
    }

    Write-Host "SliceSoft runtime prepared."
    Write-Host "  buildDir: $resolvedConfigBuildDir"
    Write-Host "  runtimeDir: $resolvedRuntimeDir"
    Write-Host "  config: $Config"
    Write-Host "  buildSystem: $BuildSystem"
    Write-Host "  tiffBackend: $TiffBackend"
    Write-Host "  parallelJobs: $Jobs"
    Write-Host "  scenarios: $($profileResourceValidation.scenarioCount)"
}
finally
{
    Pop-Location
}
