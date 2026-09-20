[CmdletBinding()]
param(
    [string]$SourceRoot = "",
    [string]$ResourceSourceRoot = "",
    [string]$Destination = "output/ripflow/modules/rip",
    [switch]$ReplaceOwnedDestination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath
{
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Assert-SafeDestination
{
    param([Parameter(Mandatory = $true)][string]$Path)
    $root = [System.IO.Path]::GetPathRoot($Path)
    if ([string]::IsNullOrWhiteSpace($root) -or
        $Path.TrimEnd('\', '/') -eq $root.TrimEnd('\', '/'))
    {
        throw "RIP module destination cannot be a filesystem root: $Path"
    }
}

function Get-Sha256Hex
{
    param([Parameter(Mandatory = $true)][string]$Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($Path)
    try
    {
        return (($algorithm.ComputeHash($stream) |
            ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally
    {
        $stream.Dispose()
        $algorithm.Dispose()
    }
}

function Get-SourceLabel
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$RepositoryRoot
    )
    $normalizedRoot = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\', '/')
    $normalizedPath = [System.IO.Path]::GetFullPath($Path)
    $rootPrefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
    if ($normalizedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        return $normalizedPath.Substring($rootPrefix.Length).Replace('\', '/')
    }
    return Split-Path -Leaf $normalizedPath
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$binarySource = if ([string]::IsNullOrWhiteSpace($SourceRoot))
{
    & (Join-Path $PSScriptRoot "ResolveRipModuleSource.ps1") -Component Binary
}
else
{
    Resolve-AbsolutePath $SourceRoot
}
$resourceSource = if (-not [string]::IsNullOrWhiteSpace($ResourceSourceRoot))
{
    Resolve-AbsolutePath $ResourceSourceRoot
}
elseif (-not [string]::IsNullOrWhiteSpace($SourceRoot))
{
    Join-Path $binarySource "CmykFiles"
}
else
{
    & (Join-Path $PSScriptRoot "ResolveRipModuleSource.ps1") -Component Resource
}
$destinationPath = Resolve-AbsolutePath $Destination
Assert-SafeDestination $destinationPath
if (-not (Test-Path -LiteralPath $binarySource -PathType Container))
{
    throw "RIP SDK binary source directory was not found: $binarySource"
}
if (-not (Test-Path -LiteralPath $resourceSource -PathType Container))
{
    throw "RIP SDK resource source directory was not found: $resourceSource"
}
if ((Test-Path -LiteralPath $destinationPath) -and -not $ReplaceOwnedDestination)
{
    throw "RIP module destination already exists; refusing to overwrite: $destinationPath"
}
if (Test-Path -LiteralPath $destinationPath)
{
    $ownedManifestPath = Join-Path $destinationPath "rip_module.json"
    if (-not (Test-Path -LiteralPath $ownedManifestPath -PathType Leaf))
    {
        throw "Existing RIP module destination has no ownership manifest: $destinationPath"
    }
    $ownedManifest = Get-Content -LiteralPath $ownedManifestPath -Raw | ConvertFrom-Json
    if ($ownedManifest.schema -ne "slicesoft.rip.module.1" -or
        $ownedManifest.moduleId -ne "slicesoft.external_rip")
    {
        throw "Existing RIP module destination is not owned by RIPFLOW: $destinationPath"
    }
}

$binaryPayload = @(
    "rip_cli.exe",
    "RipSlicer.dll",
    "tiff.dll"
)
$resourcePayload = @(
    "0.matrix",
    "1.matrix",
    "2.matrix",
    "3.matrix",
    "linear.csv",
    "CIERGB.icc",
    "CMYK.icc",
    "JapanColor2001Coated.icc"
)
foreach ($relativePath in $binaryPayload)
{
    $candidate = Join-Path $binarySource $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf))
    {
        throw "Required RIP SDK file is missing: $candidate"
    }
}
foreach ($relativePath in $resourcePayload)
{
    $candidate = Join-Path $resourceSource $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf))
    {
        throw "Required RIP SDK resource is missing: $candidate"
    }
}
$sourceCli = Join-Path $binarySource "rip_cli.exe"
$sourceHelp = & $sourceCli --help 2>&1
if ($LASTEXITCODE -ne 0 -or
    ($sourceHelp -join "`n") -notmatch '--transparent\s+<0-4>' -or
    ($sourceHelp -join "`n") -notmatch '--ripmode\s+<0\|1>' -or
    ($sourceHelp -join "`n") -notmatch '--verbose')
{
    throw "RIP SDK does not expose the required --transparent <0-4>, --ripmode <0|1> and --verbose contracts."
}

$metadataRoot = Join-Path $repoRoot "rip_module"
foreach ($metadataName in @("rip_settings.default.json", "runtime_dependencies.json"))
{
    $candidate = Join-Path $metadataRoot $metadataName
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf))
    {
        throw "Required RIP module metadata is missing: $candidate"
    }
}

$parent = Split-Path -Parent $destinationPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$staging = Join-Path $parent (".rip.staging.{0}" -f [Guid]::NewGuid().ToString("N"))
Assert-SafeDestination $staging
New-Item -ItemType Directory -Path $staging -Force | Out-Null

try
{
    foreach ($relativePath in $binaryPayload)
    {
        $target = Join-Path $staging $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $binarySource $relativePath) -Destination $target
    }
    foreach ($relativePath in $resourcePayload)
    {
        $target = Join-Path $staging (Join-Path "CmykFiles" $relativePath)
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $resourceSource $relativePath) -Destination $target
    }
    Copy-Item -LiteralPath (Join-Path $metadataRoot "rip_settings.default.json") -Destination $staging
    Copy-Item -LiteralPath (Join-Path $metadataRoot "runtime_dependencies.json") -Destination $staging
    [ordered]@{
        schema = "slicesoft.rip.source.provenance.2"
        binaryDirectory = Get-SourceLabel -Path $binarySource -RepositoryRoot $repoRoot
        resourceDirectory = Get-SourceLabel -Path $resourceSource -RepositoryRoot $repoRoot
        explicitBinarySourceOverride = -not [string]::IsNullOrWhiteSpace($SourceRoot)
        explicitResourceSourceOverride = `
            (-not [string]::IsNullOrWhiteSpace($ResourceSourceRoot)) -or `
            (-not [string]::IsNullOrWhiteSpace($SourceRoot))
    } | ConvertTo-Json | Set-Content `
        -LiteralPath (Join-Path $staging "source_provenance.json") -Encoding UTF8
    New-Item -ItemType Directory -Path (Join-Path $staging "licenses") -Force | Out-Null
    @(
        "LOCAL_ENGINEERING_ONLY",
        "External redistribution is blocked until RipSlicer, lcms2, ICC and private LibTIFF provenance and licenses are supplied."
    ) | Set-Content -LiteralPath (Join-Path $staging "licenses/REDISTRIBUTION_BLOCKED.txt") -Encoding UTF8

    $fileInventory = @()
    $payload = @($binaryPayload) + @($resourcePayload | ForEach-Object { "CmykFiles/$_" })
    foreach ($relativePath in $payload)
    {
        $file = Get-Item -LiteralPath (Join-Path $staging $relativePath)
        $fileInventory += [ordered]@{
            path = $relativePath.Replace('\', '/')
            size = [long]$file.Length
            sha256 = Get-Sha256Hex -Path $file.FullName
        }
    }
    $manifest = [ordered]@{
        schema = "slicesoft.rip.module.1"
        moduleId = "slicesoft.external_rip"
        version = "1.3.0"
        status = "LOCAL_ENGINEERING_ONLY"
        architecture = "x86_64-windows"
        entrypoint = "rip_cli.exe"
        library = "RipSlicer.dll"
        resourceDirectory = "CmykFiles"
        files = $fileInventory
        input = [ordered]@{
            schema = "p0.rgbwsv.2"
            bitDepth = 8
            samplesPerPixel = 6
            planar = "contiguous"
            storage = "stripped"
        }
        output = [ordered]@{
            samplesPerPixel = 7
            bitDepth = 8
            storage = "stripped"
            rawPattern = "slice.N.tiff"
            publishedPattern = "rip_%06d.tif"
        }
        externalValidation = "EXTERNAL_VALIDATION_DEFERRED"
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content `
        -LiteralPath (Join-Path $staging "rip_module.json") `
        -Encoding UTF8

    $backup = $null
    if (Test-Path -LiteralPath $destinationPath -PathType Container)
    {
        $backup = Join-Path $parent (".rip.backup.{0}" -f [Guid]::NewGuid().ToString("N"))
        Assert-SafeDestination $backup
        [System.IO.Directory]::Move($destinationPath, $backup)
    }
    try
    {
        [System.IO.Directory]::Move($staging, $destinationPath)
    }
    catch
    {
        if ($null -ne $backup -and
            (Test-Path -LiteralPath $backup -PathType Container) -and
            -not (Test-Path -LiteralPath $destinationPath))
        {
            [System.IO.Directory]::Move($backup, $destinationPath)
        }
        throw
    }
    if ($null -ne $backup -and (Test-Path -LiteralPath $backup -PathType Container))
    {
        Remove-Item -LiteralPath $backup -Recurse -Force
    }
}
catch
{
    if (Test-Path -LiteralPath $staging -PathType Container)
    {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    throw
}

Write-Host "RIP_MODULE_PACKAGE_PASS path=$destinationPath files=$($payload.Count)"
