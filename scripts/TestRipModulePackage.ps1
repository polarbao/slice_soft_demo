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
    $manifest.version -ne "1.1.0" -or
    $manifest.status -ne "LOCAL_ENGINEERING_ONLY" -or
    $manifest.externalValidation -ne "EXTERNAL_VALIDATION_DEFERRED")
{
    throw "RIP module identity or safety status is invalid."
}

$rootPrefix = $moduleRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
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
    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
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
        ($output -join "`n") -notmatch '--transparent\s+<0-4>')
    {
        throw "RIP CLI help probe failed with exit code $LASTEXITCODE."
    }
}

Write-Host "RIP_MODULE_TEST_PASS path=$moduleRoot files=$verified"
