[CmdletBinding()]
param(
    [string]$HandoffRoot = "output/handoff/stage14f02",
    [string]$EvidenceRoot = "output/evidence/stage14f02/intake"
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

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedHandoffRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $HandoffRoot
$resolvedEvidenceRoot = ResolveRepoPath -RepoRoot $repoRoot -Path $EvidenceRoot
AssertPathUnderRoot -Root $repoRoot -Path $resolvedHandoffRoot -Purpose "Handoff root"
AssertPathUnderRoot -Root $repoRoot -Path $resolvedEvidenceRoot -Purpose "Evidence root"

$requiredFiles = @(
    "handoff_manifest.json",
    "handoff_checksums.sha256",
    "INTEGRATION_GUIDE.md",
    "tools/slicer_host_sim.exe",
    "modules/slicer/slicer_module.dll",
    "modules/slicer/slicer_worker.exe",
    "modules/slicer/module.json",
    "contracts/print_module_spi.h",
    "contracts/slicer_capability_dtos.json",
    "contracts/slicer_three_lane_contract.json")
foreach ($relativePath in $requiredFiles)
{
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedHandoffRoot $relativePath) -PathType Leaf))
    {
        throw "Stage 14F-02 handoff file is missing: $relativePath"
    }
}

$manifest = Get-Content -LiteralPath (Join-Path $resolvedHandoffRoot "handoff_manifest.json") -Raw -Encoding UTF8 |
    ConvertFrom-Json
$manifestValid = $manifest.schema -eq "slicesoft.print_m1_handoff.14f02.1" -and
    $manifest.status -eq "slicer_side_ready_print_side_ack_pending" -and
    $manifest.module.id -eq "slicer" -and
    $manifest.module.spi -eq 1 -and
    $manifest.module.buildConfig -eq "Release" -and
    @($manifest.module.capabilities).Count -eq 15 -and
    @($manifest.requiredPrintSideEvidence).Count -eq 7
if (-not $manifestValid)
{
    throw "Stage 14F-02 handoff manifest failed the frozen M1 intake contract."
}

$checksumPath = Join-Path $resolvedHandoffRoot "handoff_checksums.sha256"
$checksumEntries = @{}
foreach ($line in Get-Content -LiteralPath $checksumPath -Encoding ASCII)
{
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$')
    {
        throw "Malformed handoff checksum line: $line"
    }
    $checksumEntries[$matches[2]] = $matches[1]
}
foreach ($entry in $checksumEntries.GetEnumerator())
{
    $file = Join-Path $resolvedHandoffRoot $entry.Key
    if (-not (Test-Path -LiteralPath $file -PathType Leaf))
    {
        throw "Checksummed handoff file is missing: $($entry.Key)"
    }
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $entry.Value)
    {
        throw "Handoff checksum mismatch: $($entry.Key)"
    }
}
$uncheckedFiles = @(
    Get-ChildItem -LiteralPath $resolvedHandoffRoot -File -Recurse |
        Where-Object { $_.FullName -ne $checksumPath } |
        ForEach-Object {
            $_.FullName.Substring($resolvedHandoffRoot.Length + 1).Replace('\', '/')
        } |
        Where-Object { -not $checksumEntries.ContainsKey($_) })
if ($uncheckedFiles.Count -gt 0)
{
    throw "Handoff contains unchecked files: $($uncheckedFiles -join ', ')"
}

if (Test-Path -LiteralPath $resolvedEvidenceRoot)
{
    Remove-Item -LiteralPath $resolvedEvidenceRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedEvidenceRoot | Out-Null

$probe = Join-Path $resolvedHandoffRoot "tools/slicer_host_sim.exe"
$module = Join-Path $resolvedHandoffRoot "modules/slicer/slicer_module.dll"
$moduleRoot = Split-Path -Parent $module
$originalPath = $env:PATH
try
{
    $env:PATH = "$moduleRoot;$env:SystemRoot;$env:SystemRoot\System32"
    $probeOutput = & $probe --m1-self-test $module 2>&1 | Out-String
    $probeExitCode = $LASTEXITCODE
    if ($probeExitCode -ne 0 -or $probeOutput -notmatch "STAGE14F02_M1_INTAKE_PASS")
    {
        throw "Stage 14F-02 M1 intake probe failed with exit code $probeExitCode`: $probeOutput"
    }

    $missingModule = Join-Path $resolvedEvidenceRoot "missing/slicer_module.dll"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try
    {
        $negativeOutput = & $probe --m1-self-test $missingModule 2>&1 | Out-String
        $negativeExitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($negativeExitCode -eq 0 -or $negativeOutput -notmatch "load failed")
    {
        throw "Stage 14F-02 missing-module negative case did not fail closed."
    }
}
finally
{
    $env:PATH = $originalPath
}

$evidence = [ordered]@{
    schema = "slicesoft.print_m1_intake_evidence.14f02.1"
    status = "slicer_side_pass_print_side_ack_pending"
    handoffManifestSha256 = (Get-FileHash `
        -LiteralPath (Join-Path $resolvedHandoffRoot "handoff_manifest.json") `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    checks = [ordered]@{
        handoffChecksums = "pass"
        manifestAndCapabilityList = "pass"
        runtimeLoadAndSelfTest = "pass"
        missingModuleFailClosed = "pass"
        printHostPurePathModuleEnumeration = "not_run_external"
    }
    probeOutput = $probeOutput.Trim()
    negativeExitCode = $negativeExitCode
}
$evidencePath = Join-Path $resolvedEvidenceRoot "m1_intake_evidence.json"
$evidence | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $evidencePath -Encoding UTF8

Write-Host "STAGE14F02_SLICER_HANDOFF_PASS evidence=$evidencePath PRINT_SIDE_M1_ACK_PENDING"
