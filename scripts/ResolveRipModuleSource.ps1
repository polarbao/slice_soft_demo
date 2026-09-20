[CmdletBinding()]
param(
    [ValidateSet("Binary", "Resource")]
    [string]$Component = "Binary",
    [string]$PinPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedPinPath = if ([string]::IsNullOrWhiteSpace($PinPath))
{
    Join-Path $repoRoot "rip_module/source.json"
}
else
{
    [System.IO.Path]::GetFullPath($PinPath)
}
$pin = Get-Content -LiteralPath $resolvedPinPath -Raw | ConvertFrom-Json

$binaryDirectory = ""
$resourceDirectory = ""
if ($pin.schema -eq "slicesoft.rip.source.1" -and
    [string]$pin.sourceDirectory -match '^rip_project/RIPDLL_[0-9]{8}$')
{
    $binaryDirectory = [string]$pin.sourceDirectory
    $resourceDirectory = "$binaryDirectory/CmykFiles"
}
elseif ($pin.schema -eq "slicesoft.rip.source.2" -and
    [string]$pin.binaryDirectory -match '^rip_project/RIPDLL_[0-9]{8}$' -and
    [string]$pin.resourceDirectory -match '^rip_project/RIPDLL_[0-9]{8}/CmykFiles$')
{
    $binaryDirectory = [string]$pin.binaryDirectory
    $resourceDirectory = [string]$pin.resourceDirectory
}
else
{
    throw "Invalid pinned RIP SDK source: $resolvedPinPath"
}

$selectedDirectory = if ($Component -eq "Resource")
{
    $resourceDirectory
}
else
{
    $binaryDirectory
}

# Select only the reviewed SDK; adding a dated directory must not switch builds.
[System.IO.Path]::GetFullPath((Join-Path $repoRoot $selectedDirectory))
