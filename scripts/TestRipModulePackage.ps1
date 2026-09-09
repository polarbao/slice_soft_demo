[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ModuleDirectory,
    [switch]$SkipExecutableProbe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$moduleRoot = [System.IO.Path]::GetFullPath($ModuleDirectory)
if (-not (Test-Path -LiteralPath $moduleRoot -PathType Container))
{
    throw "RIP module directory was not found: $moduleRoot"
}
$manifestPath = Join-Path $moduleRoot "rip_module.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf))
{
    throw "RIP module manifest was not found: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne "slicesoft.rip.module.1" -or
    $manifest.moduleId -ne "slicesoft.external_rip" -or
    $manifest.version -ne "1.2.0" -or
    $manifest.status -ne "LOCAL_ENGINEERING_ONLY" -or
    $manifest.externalValidation -ne "EXTERNAL_VALIDATION_DEFERRED")
{
    throw "RIP module identity or safety status is invalid."
}

$rootPrefix = $moduleRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$requiredPayload = @(
    "rip_cli.exe", "RipSlicer.dll", "tiff.dll",
    "CmykFiles/0.matrix", "CmykFiles/1.matrix", "CmykFiles/2.matrix", "CmykFiles/3.matrix",
    "CmykFiles/linear.csv", "CmykFiles/CIERGB.icc", "CmykFiles/CMYK.icc",
    "CmykFiles/JapanColor2001Coated.icc"
)
foreach ($requiredPath in $requiredPayload)
{
    $entries = @($manifest.files | Where-Object { $_.path -eq $requiredPath })
    if ($entries.Count -ne 1)
    {
        throw "RIP module inventory must contain exactly one entry for: $requiredPath"
    }
}
$verified = 0
foreach ($entry in @($manifest.files))
{
    $path = [System.IO.Path]::GetFullPath((Join-Path $moduleRoot ([string]$entry.path)))
    if (-not $path.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "RIP module file escapes the module root: $($entry.path)"
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "RIP module file is missing: $path"
    }
    $file = Get-Item -LiteralPath $path
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($path)
    try
    {
        $actualHash = ([System.BitConverter]::ToString(
            $algorithm.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
    }
    finally
    {
        $stream.Dispose()
        $algorithm.Dispose()
    }
    if ([long]$entry.size -ne [long]$file.Length -or [string]$entry.sha256 -ne $actualHash)
    {
        throw "RIP module file identity mismatch: $($entry.path)"
    }
    $verified++
}
if ($verified -lt 11)
{
    throw "RIP module inventory is incomplete: $verified"
}

$privateTiff = Join-Path $moduleRoot "tiff.dll"
if (-not (Test-Path -LiteralPath $privateTiff -PathType Leaf))
{
    throw "RIP private tiff.dll must remain inside the module root."
}
if (-not $SkipExecutableProbe)
{
    $entrypoint = Join-Path $moduleRoot ([string]$manifest.entrypoint)
    $output = & $entrypoint --help 2>&1
    if ($LASTEXITCODE -ne 0 -or
        ($output -join "`n") -notmatch "RipSlicer" -or
        ($output -join "`n") -notmatch '--transparent\s+<0-4>' -or
        ($output -join "`n") -notmatch '--ripmode\s+<0\|1>')
    {
        throw "RIP CLI help probe failed with exit code $LASTEXITCODE."
    }
}

Write-Host "RIP_MODULE_TEST_PASS path=$moduleRoot files=$verified"
