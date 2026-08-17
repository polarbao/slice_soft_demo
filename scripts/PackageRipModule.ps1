[CmdletBinding()]
param(
    [string]$SourceRoot = "rip_project",
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

$source = Resolve-AbsolutePath $SourceRoot
$destinationPath = Resolve-AbsolutePath $Destination
Assert-SafeDestination $destinationPath
if (-not (Test-Path -LiteralPath $source -PathType Container))
{
    throw "RIP SDK source directory was not found: $source"
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

$payload = @(
    "rip_cli.exe",
    "RipSlicer.dll",
    "tiff.dll",
    "CmykFiles/0.matrix",
    "CmykFiles/1.matrix",
    "CmykFiles/2.matrix",
    "CmykFiles/3.matrix",
    "CmykFiles/linear.csv",
    "CmykFiles/CIERGB.icc",
    "CmykFiles/CMYK.icc",
    "CmykFiles/JapanColor2001Coated.icc"
)
foreach ($relativePath in $payload)
{
    $candidate = Join-Path $source $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf))
    {
        throw "Required RIP SDK file is missing: $candidate"
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
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
    foreach ($relativePath in $payload)
    {
        $target = Join-Path $staging $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $source $relativePath) -Destination $target
    }
    Copy-Item -LiteralPath (Join-Path $metadataRoot "rip_settings.default.json") -Destination $staging
    Copy-Item -LiteralPath (Join-Path $metadataRoot "runtime_dependencies.json") -Destination $staging
    New-Item -ItemType Directory -Path (Join-Path $staging "licenses") -Force | Out-Null
    @(
        "LOCAL_ENGINEERING_ONLY",
        "External redistribution is blocked until RipSlicer, lcms2, ICC and private LibTIFF provenance and licenses are supplied."
    ) | Set-Content -LiteralPath (Join-Path $staging "licenses/REDISTRIBUTION_BLOCKED.txt") -Encoding UTF8

    $fileInventory = @()
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
        version = "1.0.0"
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
