[CmdletBinding()]
param(
    [string]$EvidenceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$exporter = Join-Path $sourceRoot "scripts/ExportSliceSoftDiagnosticsSdk.ps1"
if ([string]::IsNullOrWhiteSpace($EvidenceRoot))
{
    $EvidenceRoot = Join-Path $sourceRoot ("build-slicesoft/main/diagnostic-evidence/sdk-export-test-" + [Guid]::NewGuid().ToString("N"))
}
if (-not [System.IO.Path]::IsPathRooted($EvidenceRoot)) { throw "EvidenceRoot must be absolute" }
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw "EvidenceRoot must be new" }
$sdkRoot = Join-Path $EvidenceRoot ([string][char]0x65E5 + [char]0x5FD7 + "_SDK")

function Require([bool]$Condition, [string]$Message)
{
    if (-not $Condition) { throw $Message }
}

function ExpectFailure([scriptblock]$Action, [string]$Expected)
{
    $errorMessage = ""
    try { & $Action | Out-Null } catch { $errorMessage = $_.Exception.Message }
    Require ($errorMessage.Contains($Expected)) "Expected '$Expected', got '$errorMessage'"
}

& $exporter -OutputDirectory $sdkRoot
$manifestPath = Join-Path $sdkRoot "sdk_manifest.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
Require ($manifest.schema -eq "slicesoft.diagnostics_source_sdk.1") "Wrong SDK manifest schema"
Require ($manifest.fileCount -eq 23 -and @($manifest.files).Count -eq 23) "Incomplete source inventory"
Require (-not $manifest.binaryRuntimeIncluded -and -not $manifest.build.crashEnabledByDefault) "SDK scope changed"
Require ($manifest.contracts.spiVersion -eq 1 -and $manifest.contracts.loggingApiVersion -eq 1) "Unexpected contract versions"
$gitHead = (& git -C $sourceRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0) { throw "Unable to query source HEAD" }
Require ($manifest.source.revision -eq $gitHead) "HEAD identity mismatch"
$gitStatus = @(& git -C $sourceRoot status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw "Unable to query source status" }
Require ($manifest.source.worktreeDirtyAtEnd -eq ($gitStatus.Count -ne 0)) "Dirty source identity mismatch"
Require (-not ((Get-Content -LiteralPath $manifestPath -Raw) -match '[A-Za-z]:[\\/]')) "Manifest contains machine paths"

$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $manifest.files)
{
    Require ($seen.Add($entry.path) -and $entry.path -notmatch '(^/|\\|:|\.\.)') "Unsafe or duplicate package path"
    $file = Join-Path $sdkRoot $entry.path
    $source = Join-Path $sourceRoot $entry.source
    Require ((Get-Item -LiteralPath $file).Length -eq $entry.bytes) "Size mismatch: $($entry.path)"
    Require ((Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant() -eq $entry.sha256) "Payload hash mismatch"
    Require ((Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -eq $entry.sha256) "Source hash mismatch"
}
$actual = @(Get-ChildItem -LiteralPath $sdkRoot -Recurse -File)
Require ($actual.Count -eq $manifest.fileCount + 1) "Unexpected unlisted SDK files"
Require (@($actual | Where-Object { $_.Extension -in @('.dll', '.exe', '.pdb', '.dmp', '.log') }).Count -eq 0) "Runtime or private data leaked"
foreach ($required in @('src/diagnostics/host/SessionRetention.h', 'src/diagnostics/windows/CrashReporter.h', 'apps/slicer_crash_reporter/main.cpp'))
{
    Require ($seen.Contains($required)) "Required reusable component missing: $required"
}

$originalManifestHash = (Get-FileHash -LiteralPath $manifestPath).Hash
ExpectFailure { & $exporter -OutputDirectory $sdkRoot } "new directory"
Require ((Get-FileHash -LiteralPath $manifestPath).Hash -eq $originalManifestHash) "Rejected overwrite changed the manifest"
ExpectFailure { & $exporter -OutputDirectory "relative-sdk-output" } "absolute local drive path"
$overlap = Join-Path $sourceRoot ("src/diagnostics/sdk-overlap-test-" + [Guid]::NewGuid().ToString("N"))
ExpectFailure { & $exporter -OutputDirectory $overlap } "must be under build-slicesoft or output"
Require (-not (Test-Path -LiteralPath $overlap)) "Overlap rejection created an output"

# Junctions belong only to this new evidence directory; leave all evidence intact.
$target = Join-Path $EvidenceRoot "junction-target"
New-Item -ItemType Directory -Path $target | Out-Null
$destinationAlias = Join-Path $EvidenceRoot "destination-alias"
New-Item -ItemType Junction -Path $destinationAlias -Target $target | Out-Null
ExpectFailure { & $exporter -OutputDirectory (Join-Path $destinationAlias "sdk") } "Reparse points"
Require (-not (Test-Path -LiteralPath (Join-Path $target "sdk"))) "Destination junction was followed"
$sourceAlias = Join-Path $EvidenceRoot "source-alias"
New-Item -ItemType Junction -Path $sourceAlias -Target $sourceRoot | Out-Null
ExpectFailure {
    & (Join-Path $sourceAlias 'scripts/ExportSliceSoftDiagnosticsSdk.ps1') -OutputDirectory (Join-Path $EvidenceRoot "rejected-source")
} "Reparse points"
Require (-not (Test-Path -LiteralPath (Join-Path $EvidenceRoot "rejected-source"))) "Source junction was followed"

Write-Output "DIAGNOSTICS_SDK_EXPORT_PASS: exact files/hashes, Chinese path, source identity, no overwrite, overlap and source/destination reparse rejection."
Write-Output "SDK: $sdkRoot"
Write-Output "Evidence: $EvidenceRoot"
