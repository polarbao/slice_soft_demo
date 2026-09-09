[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$pinPath = Join-Path $repoRoot "rip_module/source.json"
$pin = Get-Content -LiteralPath $pinPath -Raw | ConvertFrom-Json
if ($pin.schema -ne "slicesoft.rip.source.1" -or
    [string]$pin.sourceDirectory -notmatch '^rip_project/RIPDLL_[0-9]{8}$')
{
    throw "Invalid pinned RIP SDK source: $pinPath"
}

# Select only the reviewed SDK; adding a dated directory must not switch builds.
[System.IO.Path]::GetFullPath((Join-Path $repoRoot ([string]$pin.sourceDirectory)))
