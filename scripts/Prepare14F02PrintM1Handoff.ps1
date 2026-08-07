[CmdletBinding()]
param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$DistributionRoot = "output/distribution",
    [string]$HandoffRoot = "output/handoff/stage14f02",
    [switch]$SkipBuild,
    [switch]$SkipPackageValidation
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

    $rootPrefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith(
            $rootPrefix,
            [System.StringComparison]::OrdinalIgnoreCase))
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

function InvokeScriptStep
{
    param(
        [string]$Name,
        [string]$Script,
        [string[]]$Arguments
    )

    Write-Host "== $Name"
    & powershell -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedBuildDir = ResolveRepoPath -RepoRoot $repoRoot -Path $BuildDir
$resolvedDistributionRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $DistributionRoot
$resolvedHandoffRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $HandoffRoot
AssertPathUnderRoot -Root $repoRoot -Path $resolvedDistributionRoot -Purpose "Distribution root"
AssertPathUnderRoot -Root $repoRoot -Path $resolvedHandoffRoot -Purpose "Handoff root"

$packageArguments = @(
    "-BuildDir", $resolvedBuildDir,
    "-OutputRoot", $resolvedDistributionRoot,
    "-Config", $Config)
if ($SkipBuild)
{
    $packageArguments += "-SkipBuild"
}
InvokeScriptStep `
    -Name "Prepare Stage 14F-01 module package" `
    -Script (Join-Path $PSScriptRoot "PackageSlicerModule.ps1") `
    -Arguments $packageArguments

$packageDir = Join-Path $resolvedDistributionRoot "$Config/modules/slicer"
if (-not $SkipPackageValidation)
{
    InvokeScriptStep `
        -Name "Validate Stage 14F-01 module package" `
        -Script (Join-Path $PSScriptRoot "TestSlicerModulePackage.ps1") `
        -Arguments @(
            "-PackageDir", $packageDir,
            "-BuildDir", $resolvedBuildDir,
            "-Config", $Config,
            "-EvidenceRoot", "output/evidence/stage14f02/package")
}

$hostExecutable = Join-Path $resolvedBuildDir "$Config/slicer_host_sim.exe"
if (-not (Test-Path -LiteralPath $hostExecutable -PathType Leaf))
{
    throw "Stage 14F-02 M1 intake probe is missing: $hostExecutable"
}

$portabilityManifestPath = Join-Path $repoRoot "contracts/slicer_ui_host_portability_manifest.json"
$portabilityManifest = Get-Content -LiteralPath $portabilityManifestPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
$contractPaths = @(
    @($portabilityManifest.contractInputs | ForEach-Object { [string]$_.path })
    "contracts/file_contract_v1.md"
    "contracts/file_contract_v1.contract_info.schema.json"
    "contracts/file_contract_v1.request.schema.json"
    "contracts/file_contract_v1.result.schema.json"
    "contracts/file_contract_v1.exit_codes.json"
    "contracts/p0.rgbwsv.2.schema.json"
) | Sort-Object -Unique

$parentRoot = Split-Path -Parent $resolvedHandoffRoot
$stagingRoot = Join-Path $parentRoot ((Split-Path -Leaf $resolvedHandoffRoot) + ".staging.$PID")
$backupRoot = Join-Path $parentRoot ((Split-Path -Leaf $resolvedHandoffRoot) + ".backup.$PID")
New-Item -ItemType Directory -Path $parentRoot -Force | Out-Null
RemoveSafeTree -Root $repoRoot -Path $stagingRoot
RemoveSafeTree -Root $repoRoot -Path $backupRoot
New-Item -ItemType Directory -Path $stagingRoot | Out-Null

$moduleDestination = Join-Path $stagingRoot "modules/slicer"
New-Item -ItemType Directory -Path (Split-Path -Parent $moduleDestination) -Force | Out-Null
Copy-Item -LiteralPath $packageDir -Destination $moduleDestination -Recurse

$contractEntries = @()
foreach ($relativePath in $contractPaths)
{
    $source = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf))
    {
        throw "Required Stage 14F-02 contract is missing: $relativePath"
    }
    $destination = Join-Path $stagingRoot $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
    $contractEntries += [ordered]@{
        path = $relativePath.Replace('\', '/')
        sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$toolsRoot = Join-Path $stagingRoot "tools"
New-Item -ItemType Directory -Path $toolsRoot | Out-Null
Copy-Item -LiteralPath $hostExecutable -Destination (Join-Path $toolsRoot "slicer_host_sim.exe")

$guideSource = Join-Path $repoRoot "docs/slice/DOC/DOC_GUIDE_14F_02_打印侧M1联调执行手册.md"
if (-not (Test-Path -LiteralPath $guideSource -PathType Leaf))
{
    throw "Stage 14F-02 integration guide is missing: $guideSource"
}
Copy-Item -LiteralPath $guideSource -Destination (Join-Path $stagingRoot "INTEGRATION_GUIDE.md")

$moduleManifest = Get-Content -LiteralPath (Join-Path $moduleDestination "module.json") -Raw -Encoding UTF8 |
    ConvertFrom-Json
$handoffManifest = [ordered]@{
    schema = "slicesoft.print_m1_handoff.14f02.1"
    status = "slicer_side_ready_print_side_ack_pending"
    module = [ordered]@{
        id = $moduleManifest.id
        version = $moduleManifest.version
        spi = $moduleManifest.spi
        buildConfig = $moduleManifest.buildConfig
        packagePath = "modules/slicer"
        dll = $moduleManifest.dll
        worker = $moduleManifest.subprocess.exe
        capabilities = @($moduleManifest.provides)
    }
    contracts = $contractEntries
    intakeProbe = "tools/slicer_host_sim.exe --m1-self-test modules/slicer/slicer_module.dll"
    requiredPrintSideEvidence = @(
        "manifest_validated_before_load",
        "spi_version_accepted",
        "module_info_runtime_and_build_config_accepted",
        "capability_count_15_accepted",
        "pm_self_test_passed",
        "pure_print_path_does_not_load_slicer_module",
        "runtime_loaded_module_path_recorded"
    )
}
$handoffManifest | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $stagingRoot "handoff_manifest.json") -Encoding UTF8

$checksumLines = @(
    Get-ChildItem -LiteralPath $stagingRoot -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($stagingRoot.Length + 1).Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $relative"
        })
$checksumLines |
    Set-Content -LiteralPath (Join-Path $stagingRoot "handoff_checksums.sha256") -Encoding ASCII

if (Test-Path -LiteralPath $resolvedHandoffRoot)
{
    Move-Item -LiteralPath $resolvedHandoffRoot -Destination $backupRoot
}
try
{
    Move-Item -LiteralPath $stagingRoot -Destination $resolvedHandoffRoot
}
catch
{
    if (Test-Path -LiteralPath $backupRoot)
    {
        Move-Item -LiteralPath $backupRoot -Destination $resolvedHandoffRoot
    }
    throw
}
RemoveSafeTree -Root $repoRoot -Path $backupRoot

Write-Host "STAGE14F02_HANDOFF_PREPARED root=$resolvedHandoffRoot"
