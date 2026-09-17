[CmdletBinding()]
param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Debug", "Release")][string]$Config = "Debug"
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
. (Join-Path $repoRoot "scripts/SliceSoftDiagnosticsPackaging.ps1")
$buildRoot = if ([System.IO.Path]::IsPathRooted($BuildDir)) { [System.IO.Path]::GetFullPath($BuildDir) }
    else { [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir)) }
$prefix = $repoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $buildRoot.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase))
{ throw "Test build directory must remain in this worktree." }
$testRoot = Join-Path $buildRoot ("diagnostic-evidence/packaging/{0}_{1}_{2}" -f $Config, $PID, [DateTime]::UtcNow.Ticks)
New-Item -ItemType Directory -Path $testRoot | Out-Null
$binary = Join-Path $buildRoot "$Config/diagnostics_crash_child.exe"
$helper = Join-Path $buildRoot "$Config/slicer_crash_reporter.exe"
$metadata = Join-Path $buildRoot "slicesoft_diagnostics_runtime_$Config.txt"
$buildManifestFixture = Join-Path $repoRoot "version-manifest.json"

function Require([bool]$Value, [string]$Message)
{ if (-not $Value) { throw $Message } }
function ExpectFailure([scriptblock]$Action, [string]$Expected)
{
    $observed = $null
    try { & $Action | Out-Null } catch { $observed = $_.Exception.Message }
    Require ($null -ne $observed -and $observed.Contains($Expected)) "Expected failure '$Expected', got '$observed'."
}

$pe = Read-SliceSoftPeCodeView $binary
$identity = Read-SliceSoftPdbIdentity ([System.IO.Path]::ChangeExtension($binary, ".pdb"))
Require ($pe.guid -eq $identity.guid -and $pe.age -eq $identity.age) "Real child PE/PDB identities differ."
$staging = Join-Path $testRoot "runtime"
New-Item -ItemType Directory -Path $staging | Out-Null
Copy-Item -LiteralPath $binary, $helper -Destination $staging
$archive = Export-SliceSoftSymbolArchive -BinaryPaths @($binary, $helper) `
    -ArchiveRoot (Join-Path $testRoot "symbols") -BuildManifest $buildManifestFixture -Config $Config `
    -PackagedBinaryDirectory $staging
Require ($archive.binaryCount -eq 2 -and -not $archive.includedInRuntime) "Private symbol archive count or placement is wrong."
$symbolManifest = Get-Content -LiteralPath (Join-Path $testRoot "symbols/$($archive.archiveId)/symbols_manifest.json") -Raw | ConvertFrom-Json
Require ($symbolManifest.symbols.Count -eq 2) "Symbol archive manifest lacks both binaries."
Require (-not (($archive | ConvertTo-Json) -match '[A-Za-z]:[\\/]')) "Runtime symbol reference contains a machine path."
$dependencies = Copy-SliceSoftDiagnosticsDependencies -MetadataPath $metadata -StagingDir $staging
Require ($dependencies.Count -eq 2) "Diagnostic dependency inventory is incomplete."
foreach ($dependency in $dependencies)
{
    Require (Test-Path -LiteralPath (Join-Path $staging $dependency.license)) "Dependency license is missing."
    if ($dependency.linkage -eq "shared")
    {
        $actual = (Get-FileHash -LiteralPath (Join-Path $staging $dependency.file)).Hash.ToLowerInvariant()
        Require ($actual -eq $dependency.librarySha256) "Copied dependency hash differs."
    }
}
Require (@(Get-ChildItem -LiteralPath $staging -Filter *.pdb).Count -eq 0) "Runtime unexpectedly contains private symbols."
ExpectFailure { Copy-SliceSoftDiagnosticsDependencies -MetadataPath (Join-Path $testRoot "absent.txt") -StagingDir $staging } "metadata was not generated"
ExpectFailure { Export-SliceSoftSymbolArchive -BinaryPaths @($binary, $binary) -ArchiveRoot (Join-Path $testRoot "duplicates") -BuildManifest $buildManifestFixture -Config $Config } "duplicate filenames"
$wrongPayload = Join-Path $testRoot "wrong-payload"
New-Item -ItemType Directory -Path $wrongPayload | Out-Null
Copy-Item -LiteralPath $helper -Destination (Join-Path $wrongPayload ([System.IO.Path]::GetFileName($binary)))
ExpectFailure { Export-SliceSoftSymbolArchive -BinaryPaths @($binary) -ArchiveRoot (Join-Path $testRoot "mismatched-payload-symbols") -BuildManifest $buildManifestFixture -Config $Config -PackagedBinaryDirectory $wrongPayload } "does not match packaged binary"
Require (@(Get-ChildItem -LiteralPath (Join-Path $testRoot "mismatched-payload-symbols") -Recurse -Filter symbols_manifest.json).Count -eq 0) "Mismatched runtime published a symbol manifest."
$wrong = Join-Path $testRoot "wrong-pdb"
New-Item -ItemType Directory -Path $wrong | Out-Null
$wrongBinary = Join-Path $wrong ([System.IO.Path]::GetFileName($binary))
Copy-Item -LiteralPath $binary -Destination $wrongBinary
Copy-Item -LiteralPath ([System.IO.Path]::ChangeExtension($helper, ".pdb")) -Destination (Join-Path $wrong $pe.pdbFile)
ExpectFailure { Export-SliceSoftSymbolArchive -BinaryPaths @($wrongBinary) -ArchiveRoot (Join-Path $testRoot "rejected") -BuildManifest $buildManifestFixture -Config $Config } "PDB GUID/age does not match"
Require (-not (Test-Path -LiteralPath (Join-Path $testRoot "rejected"))) "Rejected PDB published an archive."
$truncated = Join-Path $testRoot "truncated.pdb"
[System.IO.File]::WriteAllBytes($truncated, [byte[]](1, 2, 3))
ExpectFailure { Read-SliceSoftPdbIdentity $truncated } "beyond the file"
Write-Output "Diagnostics packaging PASS ($Config): real PE/PDB GUID+age, separate archive, exact dependencies/licenses, wrong PDB and truncated-file rejection."
Write-Output "Evidence: $testRoot"
