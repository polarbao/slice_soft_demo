[CmdletBinding()]
param(
    [string]$BuildDir = "build-slicesoft/main",
    [string]$OutputRoot = "output/distribution",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string[]]$DependencySearchPath = @(),
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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

function AssertPathUnderRoot
{
    param(
        [string]$Root,
        [string]$Path,
        [string]$Purpose
    )

    $rootPrefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "$Purpose must stay under $Root`: $resolved"
    }
}

function RemoveSafeTree
{
    param(
        [string]$Root,
        [string]$Path
    )

    AssertPathUnderRoot -Root $Root -Path $Path -Purpose "Recursive delete target"
    if (Test-Path -LiteralPath $Path)
    {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
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

function ResolveVisualStudioInstallation
{
    if (-not [string]::IsNullOrWhiteSpace($env:VSINSTALLDIR))
    {
        $candidate = $env:VSINSTALLDIR.TrimEnd('\', '/')
        if (Test-Path -LiteralPath (Join-Path $candidate "VC/Tools/MSVC"))
        {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $vsWhere = "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    if (Test-Path -LiteralPath $vsWhere)
    {
        $candidate = & $vsWhere `
            -latest `
            -products "*" `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            return [System.IO.Path]::GetFullPath($candidate.Trim())
        }
    }

    $verifiedCandidate = "D:/Program Files Tools/VS/vs_2026"
    if (Test-Path -LiteralPath (Join-Path $verifiedCandidate "VC/Tools/MSVC"))
    {
        return [System.IO.Path]::GetFullPath($verifiedCandidate)
    }
    throw "Visual Studio with the MSVC x64 tools was not found."
}

function ResolveVisualStudioTools
{
    param([string]$InstallationRoot)

    $toolsets = Get-ChildItem -LiteralPath (Join-Path $InstallationRoot "VC/Tools/MSVC") -Directory |
        Sort-Object { [version]$_.Name } -Descending
    foreach ($toolset in $toolsets)
    {
        $dumpbin = Join-Path $toolset.FullName "bin/Hostx64/x64/dumpbin.exe"
        if (Test-Path -LiteralPath $dumpbin -PathType Leaf)
        {
            return [ordered]@{
                toolsetVersion = $toolset.Name
                dumpbin = [System.IO.Path]::GetFullPath($dumpbin)
            }
        }
    }
    throw "dumpbin.exe was not found in the Visual Studio installation."
}

function ResolveVisualStudioRuntimeDirectory
{
    param([string]$InstallationRoot)

    $redistRoot = Join-Path $InstallationRoot "VC/Redist/MSVC"
    $redistVersions = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending
    foreach ($redistVersion in $redistVersions)
    {
        $x64Root = Join-Path $redistVersion.FullName "x64"
        if (-not (Test-Path -LiteralPath $x64Root -PathType Container))
        {
            continue
        }
        $crt = Get-ChildItem -LiteralPath $x64Root -Directory |
            Where-Object { $_.Name -match '^Microsoft\.VC\d+\.CRT$' } |
            Select-Object -First 1
        if ($null -ne $crt)
        {
            return [ordered]@{
                redistVersion = $redistVersion.Name
                directory = $crt.FullName
            }
        }
    }
    throw "The x64 Microsoft Visual C++ redistributable directory was not found."
}

function GetPeDependencies
{
    param(
        [string]$Dumpbin,
        [string]$Binary
    )

    $lines = & $Dumpbin /dependents $Binary 2>&1
    if ($LASTEXITCODE -ne 0)
    {
        throw "dumpbin failed for $Binary with exit code $LASTEXITCODE."
    }
    return @(
        $lines |
            ForEach-Object {
                if ($_ -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$')
                {
                    $matches[1].ToLowerInvariant()
                }
            } |
            Sort-Object -Unique)
}

function IsVisualCppRuntime
{
    param([string]$Name)

    return $Name -match '^(msvcp|vcruntime|concrt)\d.*\.dll$'
}

function IsWindowsSystemDependency
{
    param([string]$Name)

    if ($Name -match '^(api-ms-win-|ext-ms-win-)')
    {
        return $true
    }
    return Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$Name") -PathType Leaf
}

function FindDependencyFile
{
    param(
        [string]$Name,
        [string]$VisualCppRuntimeDirectory,
        [string[]]$SearchDirectories
    )

    if (IsVisualCppRuntime -Name $Name)
    {
        $candidate = Join-Path $VisualCppRuntimeDirectory $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return [System.IO.Path]::GetFullPath($candidate)
        }
        throw "MSVC runtime dependency was not found in the redistributable directory: $Name"
    }

    foreach ($directory in $SearchDirectories)
    {
        $candidate = Join-Path $directory $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedBuildDir = ResolveRepoPath -RepoRoot $repoRoot -Path $BuildDir
$resolvedOutputRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $OutputRoot
AssertPathUnderRoot -Root $repoRoot -Path $resolvedOutputRoot -Purpose "Distribution output"

if (-not $SkipBuild)
{
    InvokeNativeStep `
        -Name "Build Stage 14F runtime artifacts" `
        -Executable "cmake" `
        -Arguments @(
            "--build", $resolvedBuildDir,
            "--config", $Config,
            "--target", "slicer_module", "slicer_worker", "slicer_host_sim",
            "--parallel")
}

$binaryRoot = Join-Path $resolvedBuildDir $Config
$sourceArtifacts = [ordered]@{
    "slicer_module.dll" = Join-Path $binaryRoot "slicer_module.dll"
    "slicer_worker.exe" = Join-Path $binaryRoot "slicer_worker.exe"
    "module.json" = Join-Path $binaryRoot "module.json"
    "version-manifest.json" = Join-Path $binaryRoot "version-manifest.json"
    "slicesoft_build_manifest.json" =
        Join-Path $binaryRoot "slicesoft_build_manifest.json"
}
foreach ($entry in $sourceArtifacts.GetEnumerator())
{
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf))
    {
        throw "Required Stage 14F artifact is missing: $($entry.Value)"
    }
}

$moduleManifest = Get-Content -LiteralPath $sourceArtifacts["module.json"] -Raw -Encoding UTF8 |
    ConvertFrom-Json
$moduleManifestValid = $moduleManifest.schema -eq "slicesoft.module_manifest.1" -and
    $moduleManifest.spi -eq 1 -and
    $moduleManifest.dll -eq "slicer_module.dll" -and
    $moduleManifest.subprocess.exe -eq "slicer_worker.exe" -and
    $moduleManifest.buildConfig -eq $Config
if (-not $moduleManifestValid)
{
    throw "module.json does not match the frozen Stage 14 deployment contract."
}
$sourceVersionManifest =
    Get-Content -LiteralPath $sourceArtifacts["version-manifest.json"] -Raw -Encoding UTF8 |
        ConvertFrom-Json
$buildVersionManifest =
    Get-Content -LiteralPath $sourceArtifacts["slicesoft_build_manifest.json"] -Raw -Encoding UTF8 |
        ConvertFrom-Json
$sourceApplicationVersion = [string]$sourceVersionManifest.components.application.version
if (-not [string]::IsNullOrWhiteSpace(
        [string]$sourceVersionManifest.components.application.preRelease))
{
    $sourceApplicationVersion +=
        "-" + [string]$sourceVersionManifest.components.application.preRelease
}
$sourceSlicerVersion = [string]$sourceVersionManifest.components.slicer.version
if (-not [string]::IsNullOrWhiteSpace(
        [string]$sourceVersionManifest.components.slicer.preRelease))
{
    $sourceSlicerVersion +=
        "-" + [string]$sourceVersionManifest.components.slicer.preRelease
}
$versionManifestValid =
    $sourceVersionManifest.schemaVersion -eq 1 -and
    $sourceVersionManifest.releasePolicy -eq "lockstep" -and
    $buildVersionManifest.schema -eq "slicesoft.build.1" -and
    $buildVersionManifest.build.config -eq $Config -and
    $buildVersionManifest.components.application.version -eq $sourceApplicationVersion -and
    $buildVersionManifest.components.slicer.id -eq "slicer" -and
    $buildVersionManifest.components.slicer.version -eq $sourceSlicerVersion -and
    $sourceApplicationVersion -eq $sourceSlicerVersion -and
    $sourceSlicerVersion -eq $moduleManifest.version
if (-not $versionManifestValid)
{
    throw "SliceSoft source/build/module version manifests are inconsistent."
}

if ([string]$sourceVersionManifest.release.status -eq "stable")
{
    if (-not [string]::IsNullOrWhiteSpace(
            [string]$sourceVersionManifest.release.preRelease) -or
        [string]$buildVersionManifest.source.state -ne "clean" -or
        [string]$buildVersionManifest.source.revision -eq "unknown")
    {
        throw "A stable SliceSoft package requires an unqualified version and known clean source."
    }
    $expectedTag = "v" + [string]$sourceVersionManifest.release.version
    $tagObject = & git -C $repoRoot rev-parse --verify "refs/tags/$expectedTag^{tag}" 2>$null
    if ($LASTEXITCODE -ne 0)
    {
        throw "Stable SliceSoft package requires annotated tag $expectedTag."
    }
    $tagRevision = & git -C $repoRoot rev-parse --short=12 "$expectedTag^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0 -or
        [string]$tagRevision -ne [string]$buildVersionManifest.source.revision)
    {
        throw "Stable SliceSoft tag $expectedTag does not match the build revision."
    }
}

$expectedSlicerFullVersion =
    [string]$buildVersionManifest.components.slicer.fullBuildVersion
foreach ($binaryName in @("slicer_module.dll", "slicer_worker.exe"))
{
    $versionInfo = (Get-Item -LiteralPath $sourceArtifacts[$binaryName]).VersionInfo
    if ($versionInfo.FileVersion -ne $sourceSlicerVersion -or
        $versionInfo.ProductVersion -ne $sourceSlicerVersion -or
        $versionInfo.PrivateBuild -ne $expectedSlicerFullVersion)
    {
        throw "Built slicer binary version identity drifted: $binaryName"
    }
}

$vsInstallation = ResolveVisualStudioInstallation
$vsTools = ResolveVisualStudioTools -InstallationRoot $vsInstallation
$vsRuntime = ResolveVisualStudioRuntimeDirectory -InstallationRoot $vsInstallation

$modulesRoot = Join-Path (Join-Path $resolvedOutputRoot $Config) "modules"
$packageDir = Join-Path $modulesRoot "slicer"
$stagingDir = Join-Path $modulesRoot ("slicer.staging." + $PID)
$backupDir = Join-Path $modulesRoot ("slicer.backup." + $PID)
New-Item -ItemType Directory -Path $modulesRoot -Force | Out-Null
RemoveSafeTree -Root $resolvedOutputRoot -Path $stagingDir
RemoveSafeTree -Root $resolvedOutputRoot -Path $backupDir
New-Item -ItemType Directory -Path $stagingDir | Out-Null

foreach ($entry in $sourceArtifacts.GetEnumerator())
{
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $stagingDir $entry.Key)
}
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD_PARTY_NOTICES.txt") -Destination $stagingDir
Copy-Item -LiteralPath (Join-Path $repoRoot "licenses") -Destination $stagingDir -Recurse
Copy-Item `
    -LiteralPath (Join-Path $repoRoot "contracts/third_party_distribution_manifest.json") `
    -Destination $stagingDir

$searchDirectories = @($binaryRoot)
foreach ($path in $DependencySearchPath)
{
    $resolved = ResolveRepoPath -RepoRoot $repoRoot -Path $path
    if (-not (Test-Path -LiteralPath $resolved -PathType Container))
    {
        throw "Dependency search directory does not exist: $resolved"
    }
    $searchDirectories += $resolved
}

$pending = [System.Collections.Queue]::new()
$pending.Enqueue([pscustomobject]@{ path = $sourceArtifacts["slicer_module.dll"]; relative = "slicer_module.dll" })
$pending.Enqueue([pscustomobject]@{ path = $sourceArtifacts["slicer_worker.exe"]; relative = "slicer_worker.exe" })
$processed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$copiedDependencies = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$imports = [System.Collections.Generic.List[object]]::new()

while ($pending.Count -gt 0)
{
    $source = $pending.Dequeue()
    if (-not $processed.Add([System.IO.Path]::GetFullPath($source.path)))
    {
        continue
    }
    foreach ($dependency in GetPeDependencies -Dumpbin $vsTools.dumpbin -Binary $source.path)
    {
        if ((IsWindowsSystemDependency -Name $dependency) -and -not (IsVisualCppRuntime -Name $dependency))
        {
            $imports.Add([ordered]@{
                source = $source.relative
                dependency = $dependency
                disposition = "windows_system"
                packagedPath = $null
            })
            continue
        }

        $dependencyPath = FindDependencyFile `
            -Name $dependency `
            -VisualCppRuntimeDirectory $vsRuntime.directory `
            -SearchDirectories $searchDirectories
        if ([string]::IsNullOrWhiteSpace($dependencyPath))
        {
            throw "Non-system runtime dependency could not be resolved: $dependency (required by $($source.relative))"
        }
        if ($copiedDependencies.Add($dependency))
        {
            Copy-Item -LiteralPath $dependencyPath -Destination (Join-Path $stagingDir $dependency)
            $pending.Enqueue([pscustomobject]@{ path = $dependencyPath; relative = $dependency })
        }
        $imports.Add([ordered]@{
            source = $source.relative
            dependency = $dependency
            disposition = if (IsVisualCppRuntime -Name $dependency) { "microsoft_vc_runtime" } else { "app_local" }
            packagedPath = $dependency
        })
    }
}

$artifacts = @(
    Get-ChildItem -LiteralPath $stagingDir -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            [ordered]@{
                path = $_.FullName.Substring($stagingDir.Length + 1).Replace('\', '/')
                bytes = $_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        })
$runtimeInventory = [ordered]@{
    schema = "slicesoft.runtime_dependency_inventory.1"
    moduleId = "slicer"
    config = $Config
    architecture = "x64"
    toolsetVersion = $vsTools.toolsetVersion
    redistributableVersion = $vsRuntime.redistVersion
    artifacts = $artifacts
    imports = @($imports | Sort-Object source, dependency)
}
$runtimeInventory | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $stagingDir "runtime_dependencies.json") -Encoding UTF8

$checksumLines = @(
    Get-ChildItem -LiteralPath $stagingDir -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($stagingDir.Length + 1).Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $relative"
        })
$checksumLines | Set-Content -LiteralPath (Join-Path $stagingDir "checksums.sha256") -Encoding ASCII

if (Test-Path -LiteralPath $packageDir)
{
    Move-Item -LiteralPath $packageDir -Destination $backupDir
}
try
{
    Move-Item -LiteralPath $stagingDir -Destination $packageDir
}
catch
{
    if (Test-Path -LiteralPath $backupDir)
    {
        Move-Item -LiteralPath $backupDir -Destination $packageDir
    }
    throw
}
RemoveSafeTree -Root $resolvedOutputRoot -Path $backupDir

Write-Host "STAGE14F01_PACKAGE_PASS package=$packageDir"
