[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-AbsoluteLocalPath([string]$Path)
{
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2).Contains(':'))
    {
        throw "An absolute local drive path without alternate streams is required: $Path"
    }
    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full.Length -gt 3) { $full = $full.TrimEnd([char[]]"\/") }
    return $full
}

function Test-WithinPath([string]$Path, [string]$Root)
{
    return $Path.Equals($Root, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Root.TrimEnd([char[]]"\/") + "\", [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparsePath([string]$Path, [bool]$AllowMissing)
{
    $root = [System.IO.Path]::GetPathRoot($Path)
    $current = $root
    $parts = @($Path.Substring($root.Length).Split([char[]]"\/", [StringSplitOptions]::RemoveEmptyEntries))
    $paths = @($root)
    foreach ($part in $parts)
    {
        $current = Join-Path $current $part
        $paths += $current
    }
    foreach ($candidate in $paths)
    {
        try { $item = Get-Item -LiteralPath $candidate -Force -ErrorAction Stop }
        catch [System.Management.Automation.ItemNotFoundException]
        {
            if ($AllowMissing) { return }
            throw
        }
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0)
        {
            throw "Reparse points are not accepted in source or destination paths: $candidate"
        }
        if ($candidate -ne $Path -and -not $item.PSIsContainer)
        {
            throw "A path ancestor is not a directory: $candidate"
        }
    }
}

function Get-Sha256([string]$Path)
{
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-SourceGit([string[]]$Arguments)
{
    $result = @(& git -C $sourceRoot @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "Unable to read source git identity: $($Arguments -join ' ')" }
    return $result
}

function Read-ApiVersion([string]$Path, [string]$Name)
{
    $content = [System.IO.File]::ReadAllText($Path)
    $match = [regex]::Match($content, '(?m)^\s*#\s*define\s+' + [regex]::Escape($Name) + '\s+(\d+)\s*$')
    if (-not $match.Success) { throw "Unable to read $Name from $Path" }
    return [int]$match.Groups[1].Value
}

$sourceRoot = Get-AbsoluteLocalPath ([System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..")))
$destination = Get-AbsoluteLocalPath $OutputDirectory
Assert-NoReparsePath $sourceRoot $false
Assert-NoReparsePath $destination $true
if (Test-Path -LiteralPath $destination) { throw "The output must be a new directory: $destination" }
if (Test-WithinPath $sourceRoot $destination) { throw "The output overlaps the source repository root" }
if (Test-WithinPath $destination $sourceRoot)
{
    $generatedRoots = @((Join-Path $sourceRoot "build-slicesoft"), (Join-Path $sourceRoot "output"))
    if (-not ($generatedRoots | Where-Object { Test-WithinPath $destination $_ }))
    {
        throw "An output inside the repository must be under build-slicesoft or output"
    }
}

$repoTop = Get-AbsoluteLocalPath ((Invoke-SourceGit @("rev-parse", "--show-toplevel")) -join "")
if ($repoTop -ne $sourceRoot) { throw "The exporter must belong to the source repository root" }
$revision = (Invoke-SourceGit @("rev-parse", "HEAD")) -join ""
$statusBefore = @(Invoke-SourceGit @("status", "--porcelain=v1", "--untracked-files=all"))

# Only these files are copied. Templates become package-root build/usage files.
$sourcePaths = @(
    "contracts/print_module_spi.h", "contracts/slicer_logging.h",
    "src/diagnostics/LogEvent.h", "src/diagnostics/LogEvent.cpp",
    "src/diagnostics/host/LogSession.h", "src/diagnostics/host/LogSession.cpp",
    "src/diagnostics/host/ModuleLogBinding.h", "src/diagnostics/host/ModuleLogBinding.cpp",
    "src/diagnostics/host/SessionRetention.h", "src/diagnostics/host/SessionRetention.cpp",
    "src/diagnostics/spdlog/FileLogSink.h", "src/diagnostics/spdlog/FileLogSink.cpp",
    "src/diagnostics/windows/CrashReporter.h", "src/diagnostics/windows/CrashReporter.cpp",
    "src/diagnostics/windows/CrashCapture.h", "src/diagnostics/windows/CrashCapture.cpp",
    "src/diagnostics/windows/CrashProtocol.h", "apps/slicer_crash_reporter/main.cpp",
    "licenses/spdlog.txt", "licenses/fmt.txt"
)
$mappings = @($sourcePaths | ForEach-Object { [pscustomobject]@{ source = $_; path = $_ } })
foreach ($name in @("CMakeLists.txt", "README.md", "DEPENDENCIES.md"))
{
    $mappings += [pscustomobject]@{ source = "sdk/diagnostics/$name"; path = $name }
}
$entries = @()
foreach ($mapping in $mappings)
{
    $source = Get-AbsoluteLocalPath (Join-Path $sourceRoot $mapping.source)
    Assert-NoReparsePath $source $false
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing SDK source file: $source" }
    $entries += [ordered]@{
        path = $mapping.path
        source = $mapping.source
        bytes = (Get-Item -LiteralPath $source).Length
        sha256 = Get-Sha256 $source
    }
}
$exporterRelative = "scripts/ExportSliceSoftDiagnosticsSdk.ps1"
$exporterPath = Join-Path $sourceRoot $exporterRelative
Assert-NoReparsePath $exporterPath $false
$exporterHash = Get-Sha256 $exporterPath
$spiVersion = Read-ApiVersion (Join-Path $sourceRoot "contracts/print_module_spi.h") "PM_SPI_VERSION"
$loggingVersion = Read-ApiVersion (Join-Path $sourceRoot "contracts/slicer_logging.h") "SLICER_LOG_API_VERSION"

# No deletion or overwrite: failed exports remain for inspection, without a manifest.
Assert-NoReparsePath $destination $true
New-Item -ItemType Directory -Path $destination -ErrorAction Stop | Out-Null
foreach ($entry in $entries)
{
    $source = Join-Path $sourceRoot $entry.source
    $target = Join-Path $destination $entry.path
    Assert-NoReparsePath $source $false
    Assert-NoReparsePath $target $true
    [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($target))
    [System.IO.File]::Copy($source, $target, $false)
    if ((Get-Sha256 $target) -ne $entry.sha256 -or (Get-Item -LiteralPath $target).Length -ne $entry.bytes)
    { throw "Source changed during SDK copy: $($entry.source)" }
}
foreach ($entry in $entries)
{
    $source = Join-Path $sourceRoot $entry.source
    Assert-NoReparsePath $source $false
    if ((Get-Sha256 $source) -ne $entry.sha256 -or (Get-Item -LiteralPath $source).Length -ne $entry.bytes)
    { throw "Source changed during SDK export: $($entry.source)" }
}
if (((Invoke-SourceGit @("rev-parse", "HEAD")) -join "") -ne $revision -or
    (Get-Sha256 $exporterPath) -ne $exporterHash)
{
    throw "The source revision or exporter changed during export"
}
$statusAfter = @(Invoke-SourceGit @("status", "--porcelain=v1", "--untracked-files=all"))
$manifest = [ordered]@{
    schema = "slicesoft.diagnostics_source_sdk.1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    source = [ordered]@{
        revision = $revision
        worktreeDirtyAtStart = ($statusBefore.Count -ne 0)
        worktreeDirtyAtEnd = ($statusAfter.Count -ne 0)
        identity = "HEAD plus the exact exported file hashes; no clean-source claim for dirty worktrees"
        exporter = $exporterRelative
        exporterSha256 = $exporterHash
    }
    contracts = [ordered]@{ spiVersion = $spiVersion; loggingApiVersion = $loggingVersion }
    build = [ordered]@{
        platform = "windows-x64-msvc"
        language = "c++20"
        releaseRuntime = "MD"
        debugRuntime = "MDd"
        crashOption = "SLICESOFT_DIAGNOSTICS_BUILD_CRASH"
        crashEnabledByDefault = $false
        dependenciesSuppliedByConsumer = @("spdlog", "fmt as selected by spdlog", "nlohmann_json")
    }
    binaryRuntimeIncluded = $false
    fileCount = $entries.Count
    files = $entries
}
$manifestPath = Join-Path $destination "sdk_manifest.json"
Assert-NoReparsePath $manifestPath $true
$manifestText = $manifest | ConvertTo-Json -Depth 8
$manifestBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($manifestText + "`n")
$stream = [System.IO.File]::Open($manifestPath, [System.IO.FileMode]::CreateNew)
try { $stream.Write($manifestBytes, 0, $manifestBytes.Length) }
finally { $stream.Dispose() }
Write-Output "SLICESOFT_DIAGNOSTICS_SDK_EXPORTED path=$destination files=$($entries.Count) spi=$spiVersion logApi=$loggingVersion"
