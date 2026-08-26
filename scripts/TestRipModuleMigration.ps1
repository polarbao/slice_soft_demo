[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HostExecutable,
    [Parameter(Mandatory = $true)]
    [string]$ModuleDirectory,
    [Parameter(Mandatory = $true)]
    [string]$SourcePackage,
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$WinDeployQt =
        "C:/Qt/Qt5.15.2/5.15.2/msvc2019_64/bin/windeployqt.exe"
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

function Assert-NotRoot
{
    param([Parameter(Mandatory = $true)][string]$Path)
    $root = [System.IO.Path]::GetPathRoot($Path)
    if ([string]::IsNullOrWhiteSpace($root) -or
        $Path.TrimEnd('\', '/') -eq $root.TrimEnd('\', '/'))
    {
        throw "Migration destination cannot be a filesystem root: $Path"
    }
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
$destinationPath = Resolve-AbsolutePath $Destination
$deployQtPath = Resolve-AbsolutePath $WinDeployQt
Assert-NotRoot $destinationPath
foreach ($required in @(
    $hostPath,
    (Join-Path $modulePath "rip_module.json"),
    (Join-Path $sourcePath "manifest.json"),
    (Join-Path $sourcePath "layers"),
    $deployQtPath))
{
    if (-not (Test-Path -LiteralPath $required))
    {
        throw "RIP migration input is missing: $required"
    }
}
if (Test-Path -LiteralPath $destinationPath)
{
    throw "RIP migration destination already exists: $destinationPath"
}

$parent = Split-Path -Parent $destinationPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$staging = Join-Path $parent (
    ".rip.runtime.staging.{0}" -f [Guid]::NewGuid().ToString("N"))
Assert-NotRoot $staging
New-Item -ItemType Directory -Path $staging -Force | Out-Null
try
{
    $runtimeHost = Join-Path $staging "slicer_ui_host_sim.exe"
    Copy-Item -LiteralPath $hostPath -Destination $runtimeHost
    $hostRoot = Split-Path -Parent $hostPath
    foreach ($runtimeTiff in @(
        Get-ChildItem -LiteralPath $hostRoot -Filter "tiff*.dll" -File))
    {
        Copy-Item -LiteralPath $runtimeTiff.FullName -Destination $staging
    }
    $runtimeModule = Join-Path $staging "modules/rip"
    New-Item -ItemType Directory -Path (Split-Path -Parent $runtimeModule) `
        -Force | Out-Null
    Copy-Item -LiteralPath $modulePath -Destination $runtimeModule -Recurse

    $runtimePackage = Join-Path $staging "samples/package"
    New-Item -ItemType Directory -Path $runtimePackage -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourcePath "manifest.json") `
        -Destination $runtimePackage
    Copy-Item -LiteralPath (Join-Path $sourcePath "layers") `
        -Destination $runtimePackage -Recurse
    $before = Get-LayerIdentity -Package $runtimePackage

    & $deployQtPath `
        --debug `
        --compiler-runtime `
        --no-translations `
        --dir $staging `
        $runtimeHost | Out-Host
    if ($LASTEXITCODE -ne 0)
    {
        throw "windeployqt failed with exit code $LASTEXITCODE."
    }

    & $runtimeHost --rip-module-self-test
    if ($LASTEXITCODE -ne 0)
    {
        throw "Isolated relative RIP module self-test failed."
    }
    & $runtimeHost `
        --rip-job-self-test `
        --package $runtimePackage `
        --rip-module $runtimeModule `
        --transparent-mode 0 `
        --gray-bits 2
    if ($LASTEXITCODE -ne 0)
    {
        throw "Isolated real RIP job self-test failed."
    }
    $after = Get-LayerIdentity -Package $runtimePackage
    if (($before -join "`n") -cne ($after -join "`n"))
    {
        throw "Isolated RIP job modified source layers."
    }
    $resultExists = Test-Path -LiteralPath (
        Join-Path $runtimePackage "rip/rip_result.json")
    $privateTiffEscaped = Test-Path -LiteralPath (
        Join-Path $staging "tiff.dll")
    if (-not $resultExists -or $privateTiffEscaped)
    {
        throw "Isolated RIP result is missing or private tiff.dll escaped to app root."
    }
    [System.IO.Directory]::Move($staging, $destinationPath)
}
catch
{
    if (Test-Path -LiteralPath $staging -PathType Container)
    {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    throw
}

Write-Host (
    "RIPFLOW_MIGRATION_PASS runtime={0} relativeModule=true privateTiff=true realJob=true" -f `
        $destinationPath)
