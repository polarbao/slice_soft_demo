[CmdletBinding()]
param(
    [string]$PackageDir = "output/distribution/Release/modules/slicer",
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$EvidenceRoot = "output/evidence/stage14f01"
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

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedPackageDir = ResolveRepoPath -RepoRoot $repoRoot -Path $PackageDir
$resolvedBuildDir = ResolveRepoPath -RepoRoot $repoRoot -Path $BuildDir
$resolvedEvidenceRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $EvidenceRoot
AssertPathUnderRoot -Root $repoRoot -Path $resolvedPackageDir -Purpose "Package directory"
AssertPathUnderRoot -Root $repoRoot -Path $resolvedEvidenceRoot -Purpose "Evidence directory"

$requiredFiles = @(
    "slicer_module.dll",
    "slicer_worker.exe",
    "module.json",
    "runtime_dependencies.json",
    "checksums.sha256",
    "THIRD_PARTY_NOTICES.txt",
    "third_party_distribution_manifest.json",
    "licenses/miniz.txt",
    "licenses/libtiff.txt",
    "licenses/assimp.txt",
    "licenses/meshoptimizer.txt")
foreach ($relativePath in $requiredFiles)
{
    $path = Join-Path $resolvedPackageDir $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Package file is missing: $relativePath"
    }
}

$moduleManifest = Get-Content -LiteralPath (Join-Path $resolvedPackageDir "module.json") -Raw -Encoding UTF8 |
    ConvertFrom-Json
$moduleManifestValid = $moduleManifest.schema -eq "slicesoft.module_manifest.1" -and
    $moduleManifest.id -eq "slicer" -and
    $moduleManifest.spi -eq 1 -and
    $moduleManifest.buildConfig -eq $Config -and
    @($moduleManifest.provides).Count -eq 15
if (-not $moduleManifestValid)
{
    throw "module.json failed the frozen Stage 14 deployment contract."
}

$runtimeInventory = Get-Content -LiteralPath (Join-Path $resolvedPackageDir "runtime_dependencies.json") -Raw -Encoding UTF8 |
    ConvertFrom-Json
$runtimeInventoryValid = $runtimeInventory.schema -eq "slicesoft.runtime_dependency_inventory.1" -and
    $runtimeInventory.config -eq $Config
if (-not $runtimeInventoryValid)
{
    throw "runtime_dependencies.json is invalid."
}
$appLocalImports = @(
    $runtimeInventory.imports |
        Where-Object { $_.disposition -in @("microsoft_vc_runtime", "app_local") })
foreach ($import in $appLocalImports)
{
    $dependencyPackaged = -not [string]::IsNullOrWhiteSpace($import.packagedPath) -and
        (Test-Path -LiteralPath (Join-Path $resolvedPackageDir $import.packagedPath) -PathType Leaf)
    if (-not $dependencyPackaged)
    {
        throw "App-local dependency is missing: $($import.dependency)"
    }
}

$checksumPath = Join-Path $resolvedPackageDir "checksums.sha256"
$checksumEntries = @{}
foreach ($line in Get-Content -LiteralPath $checksumPath -Encoding ASCII)
{
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$')
    {
        throw "Malformed checksum line: $line"
    }
    $checksumEntries[$matches[2]] = $matches[1]
}
foreach ($entry in $checksumEntries.GetEnumerator())
{
    $file = Join-Path $resolvedPackageDir $entry.Key
    if (-not (Test-Path -LiteralPath $file -PathType Leaf))
    {
        throw "Checksummed package file is missing: $($entry.Key)"
    }
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $entry.Value)
    {
        throw "Checksum mismatch: $($entry.Key)"
    }
}
$uncheckedFiles = @(
    Get-ChildItem -LiteralPath $resolvedPackageDir -File -Recurse |
        Where-Object { $_.FullName -ne $checksumPath } |
        ForEach-Object { $_.FullName.Substring($resolvedPackageDir.Length + 1).Replace('\', '/') } |
        Where-Object { -not $checksumEntries.ContainsKey($_) })
if ($uncheckedFiles.Count -gt 0)
{
    throw "Package contains unchecked files: $($uncheckedFiles -join ', ')"
}

$worker = Join-Path $resolvedPackageDir "slicer_worker.exe"
$workerContractText = & $worker --contract-info
if ($LASTEXITCODE -ne 0)
{
    throw "Packaged slicer_worker --contract-info failed with exit code $LASTEXITCODE."
}
$workerContract = $workerContractText | ConvertFrom-Json
if ($workerContract.contract -ne "file_contract" -or $workerContract.major -ne 1)
{
    throw "Packaged worker returned an incompatible file contract."
}

$hostExecutable = Join-Path (Join-Path $resolvedBuildDir $Config) "slicer_host_sim.exe"
if (-not (Test-Path -LiteralPath $hostExecutable -PathType Leaf))
{
    throw "Pure C host simulator is missing: $hostExecutable"
}
if (Test-Path -LiteralPath $resolvedEvidenceRoot)
{
    AssertPathUnderRoot -Root $repoRoot -Path $resolvedEvidenceRoot -Purpose "Evidence cleanup"
    Remove-Item -LiteralPath $resolvedEvidenceRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedEvidenceRoot | Out-Null

$originalPath = $env:PATH
try
{
    $env:PATH = "$resolvedPackageDir;$env:SystemRoot;$env:SystemRoot\System32"
    InvokeNativeStep `
        -Name "Run packaged module through the pure C host" `
        -Executable $hostExecutable `
        -Arguments @(
            (Join-Path $resolvedPackageDir "slicer_module.dll"),
            $repoRoot,
            $resolvedEvidenceRoot)
}
finally
{
    $env:PATH = $originalPath
}

$packageManifests = @(
    Get-ChildItem -LiteralPath $resolvedEvidenceRoot -Filter "manifest.json" -File -Recurse)
if ($packageManifests.Count -ne 1)
{
    throw "The packaged module must produce exactly one reference RGBWSV package; found $($packageManifests.Count)."
}

Write-Host "STAGE14F01_PACKAGE_VALIDATION_PASS package=$resolvedPackageDir evidence=$resolvedEvidenceRoot"
