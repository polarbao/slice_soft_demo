[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HostExecutable,
    [Parameter(Mandatory = $true)]
    [string]$ModuleDirectory,
    [Parameter(Mandatory = $true)]
    [string]$SourcePackage,
    [string]$OutputRoot = "output/ripflow/local_gate",
    [ValidateSet("explicit_transparent", "explicit_opaque")]
    [string]$TransparentMode = "explicit_transparent",
    [ValidateSet(1, 2)]
    [int]$GrayBits = 2
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

function Get-LayerIdentity
{
    param([Parameter(Mandatory = $true)][string]$Package)
    return @(
        Get-ChildItem -LiteralPath (Join-Path $Package "layers") -File |
            Sort-Object Name |
            ForEach-Object {
                "{0}:{1}" -f $_.Name,
                    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
    )
}

$hostPath = Resolve-AbsolutePath $HostExecutable
$modulePath = Resolve-AbsolutePath $ModuleDirectory
$sourcePath = Resolve-AbsolutePath $SourcePackage
$outputPath = Resolve-AbsolutePath $OutputRoot
foreach ($required in @(
    $hostPath,
    (Join-Path $modulePath "rip_module.json"),
    (Join-Path $sourcePath "manifest.json"),
    (Join-Path $sourcePath "layers")))
{
    if (-not (Test-Path -LiteralPath $required))
    {
        throw "RIPFLOW local gate input is missing: $required"
    }
}
if (Test-Path -LiteralPath (Join-Path $sourcePath "rip"))
{
    throw "Source package already contains rip; choose an unprocessed package."
}

New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$caseRoot = Join-Path $outputPath (
    "case.{0}" -f [Guid]::NewGuid().ToString("N"))
$package = Join-Path $caseRoot "package"
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $sourcePath "manifest.json") -Destination $package
Copy-Item -LiteralPath (Join-Path $sourcePath "layers") -Destination $package -Recurse
$before = Get-LayerIdentity -Package $package

& $hostPath `
    --rip-job-self-test `
    --package $package `
    --rip-module $modulePath `
    --transparent-mode $TransparentMode `
    --gray-bits $GrayBits
if ($LASTEXITCODE -ne 0)
{
    throw "RIPFLOW job self-test failed with exit code $LASTEXITCODE."
}

$after = Get-LayerIdentity -Package $package
if (($before -join "`n") -cne ($after -join "`n"))
{
    throw "RIPFLOW modified one or more source layer files."
}
$resultPath = Join-Path $package "rip/rip_result.json"
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf))
{
    throw "RIPFLOW result report was not published."
}
$result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
$publishedLayers = @(
    Get-ChildItem -LiteralPath (Join-Path $package "rip") `
        -Filter "rip_*.tif" -File
)
if ($result.schema -ne "slicesoft.rip.result.1" -or
    $result.status -ne "succeeded" -or
    $result.externalValidation -ne "EXTERNAL_VALIDATION_DEFERRED" -or
    $publishedLayers.Count -ne [int]$result.output.layerCount)
{
    throw "RIPFLOW result contract or published layer count is invalid."
}

Write-Host (
    "RIPFLOW_LOCAL_GATE_PASS package={0} layers={1} mode={2} grayBits={3}" -f `
        $package,
        $publishedLayers.Count,
        $TransparentMode,
        $GrayBits)
