[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-slicesoft/main",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$RepositoryRoot = ".",
    [string]$OutputRoot = ""
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

function Assert-Executable
{
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "Required executable is missing: $Path"
    }
}

function Invoke-Checked
{
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Label
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

$repository = Resolve-AbsolutePath $RepositoryRoot
$build = Resolve-AbsolutePath $BuildDirectory
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $build "stage14f03_evidence/$Config"
}
$evidenceRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$allowedRoots = @(
    ([System.IO.Path]::GetFullPath($repository).TrimEnd('\') + '\'),
    ([System.IO.Path]::GetFullPath($build).TrimEnd('\') + '\')
)
$isAllowedOutput = $false
foreach ($allowedRoot in $allowedRoots)
{
    if ($evidenceRoot.StartsWith(
            $allowedRoot,
            [System.StringComparison]::OrdinalIgnoreCase))
    {
        $isAllowedOutput = $true
        break
    }
}
if (-not $isAllowedOutput)
{
    throw "OutputRoot must remain inside the repository or build directory: $evidenceRoot"
}

$binaryRoot = Join-Path $build $Config
$hostExecutable = Join-Path $binaryRoot "slicer_host_sim.exe"
$module = Join-Path $binaryRoot "slicer_module.dll"
$reader = Join-Path $binaryRoot "rip_reader_test.exe"
Assert-Executable $hostExecutable
Assert-Executable $module
Assert-Executable $reader

if (Test-Path -LiteralPath $evidenceRoot)
{
    Remove-Item -LiteralPath $evidenceRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null

Invoke-Checked `
    -Executable $hostExecutable `
    -Arguments @($module, $repository, $evidenceRoot) `
    -Label "single-model module flow"

$packageDirectory = Join-Path $evidenceRoot "stage14e01_package"
$manifestPath = Join-Path $packageDirectory "manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf))
{
    throw "Single-model flow did not publish manifest.json"
}

Invoke-Checked `
    -Executable $reader `
    -Arguments @("--package", $packageDirectory, "--quiet") `
    -Label "S1 positive package"

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne "p0.rgbwsv.2" -or
    $manifest.tiff.bitDepth -ne 8 -or
    $manifest.tiff.channelCount -ne 6 -or
    (($manifest.tiff.channelOrder -join ",") -ne "R,G,B,W,S,V") -or
    $manifest.tiff.polarity -ne "black_is_print" -or
    $manifest.tiff.printValue -ne 0 -or
    $manifest.tiff.emptyValue -ne 255)
{
    throw "S1 positive package does not match the frozen RGBWSV contract"
}

$negativeCases = @(
    [ordered]@{ name = "bad_schema"; code = "E_SCHEMA_UNSUPPORTED" },
    [ordered]@{ name = "bad_bit_depth"; code = "E_BIT_DEPTH_INVALID" },
    [ordered]@{ name = "bad_channel_order"; code = "E_CHANNEL_ORDER_INVALID" },
    [ordered]@{ name = "bad_channel_count"; code = "E_CHANNEL_COUNT_INVALID" },
    [ordered]@{ name = "bad_polarity"; code = "E_POLARITY_INVALID" },
    [ordered]@{ name = "bad_missing_layer"; code = "E_LAYER_MISSING" },
    [ordered]@{ name = "bad_layer_size"; code = "E_LAYER_SIZE_MISMATCH" }
)

foreach ($case in $negativeCases)
{
    $badPackage = Join-Path $repository "tests/packages/bad/$($case.name)"
    Invoke-Checked `
        -Executable $reader `
        -Arguments @(
            "--package", $badPackage,
            "--expect-error",
            "--expect-code", $case.code,
            "--quiet"
        ) `
        -Label "S1 negative package $($case.name)"
}

$evidence = [ordered]@{
    schema = "slicesoft.stage14f03.s1_gate.1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    config = $Config
    module = $module
    packageDirectory = $packageDirectory
    positive = [ordered]@{
        schema = $manifest.schema
        layerCount = $manifest.grid.layerCount
        channelOrder = @($manifest.tiff.channelOrder)
        bitDepth = $manifest.tiff.bitDepth
        polarity = $manifest.tiff.polarity
        printValue = $manifest.tiff.printValue
        emptyValue = $manifest.tiff.emptyValue
        result = "PASS"
    }
    negativeCases = @($negativeCases | ForEach-Object {
        [ordered]@{ name = $_.name; expectedCode = $_.code; result = "PASS" }
    })
    externalPrintValidation = "DEFERRED_BY_USER"
}
$evidencePath = Join-Path $evidenceRoot "stage14f03_s1_gate.json"
$evidence | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $evidencePath -Encoding UTF8

Write-Host "STAGE14F03_S1_GATE_PASS positive=1 negative=$($negativeCases.Count)"
Write-Host "evidence=$evidencePath"
