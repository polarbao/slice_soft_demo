param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [string]$OutputRoot = "output/benchmarks/13c_04"
)

$ErrorActionPreference = "Stop"

function Assert-True
{
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition)
    {
        throw $Message
    }
}

function Resolve-Executable
{
    param(
        [string]$BuildRoot,
        [string]$BuildConfig,
        [string]$Name
    )

    $candidates = @(
        (Join-Path $BuildRoot "$BuildConfig/$Name.exe"),
        (Join-Path $BuildRoot "$Name.exe")
    )
    foreach ($candidate in $candidates)
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "missing executable $Name under $BuildRoot"
}

function Write-Utf8NoBom
{
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent))
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function Invoke-SliceCase
{
    param(
        [string]$CaseId,
        [string]$OutputPolicy,
        [bool]$Enabled,
        [string]$PackagePath,
        [string]$ConfigPath,
        [string]$SlicerPath,
        [string]$RipPath,
        [pscustomobject]$Template,
        [string]$ModelPath
    )

    $config = $Template | ConvertTo-Json -Depth 100 | ConvertFrom-Json
    $config.input.modelPath = $ModelPath
    $config.output.packageDir = $PackagePath
    $config.preview | Add-Member -NotePropertyName outputPolicy `
        -NotePropertyValue $OutputPolicy -Force
    $config.preview.enabled = $Enabled
    $config.preview.format = "png"
    $config.preview.interval = 1
    Write-Utf8NoBom $ConfigPath ($config | ConvertTo-Json -Depth 100)

    $outputLines = @(
        & $SlicerPath --config $ConfigPath 2>&1 |
            ForEach-Object { $_.ToString() }
    )
    Assert-True ($LASTEXITCODE -eq 0) "$CaseId slicer_cli failed"
    $outputLines | Set-Content -Encoding UTF8 -LiteralPath "$ConfigPath.log"

    $timingLine = @(
        $outputLines |
            Where-Object { $_ -like "SLICE_TIMING *" }
    ) | Select-Object -Last 1
    Assert-True (-not [string]::IsNullOrWhiteSpace($timingLine)) `
        "$CaseId missing SLICE_TIMING"
    $timing = [ordered]@{}
    foreach ($match in [regex]::Matches(
        $timingLine,
        "(?<key>[A-Za-z][A-Za-z0-9]*)=(?<value>[^\s]+)"))
    {
        $key = $match.Groups["key"].Value
        $value = $match.Groups["value"].Value
        if ($key -in @("engine", "profileLevel"))
        {
            $timing[$key] = $value
        }
        else
        {
            $timing[$key] = [double]::Parse(
                $value,
                [System.Globalization.CultureInfo]::InvariantCulture)
        }
    }

    & $RipPath --package $PackagePath --summary | Out-Host
    Assert-True ($LASTEXITCODE -eq 0) "$CaseId RIP strict failed"

    $manifest = Get-Content -Raw -Encoding UTF8 `
        -LiteralPath (Join-Path $PackagePath "manifest.json") |
        ConvertFrom-Json
    Assert-True ($manifest.schema -eq "p0.rgbwsv.2") `
        "$CaseId schema changed"
    Assert-True ($manifest.preview.outputPolicy -eq $OutputPolicy) `
        "$CaseId manifest outputPolicy mismatch"

    $layerFiles = @(
        Get-ChildItem -LiteralPath (Join-Path $PackagePath "layers") `
            -Filter "*.tiff" -File |
            Sort-Object Name
    )
    $previewPath = Join-Path $PackagePath "preview"
    $previewFiles = @()
    if (Test-Path -LiteralPath $previewPath)
    {
        $previewFiles = @(
            Get-ChildItem -LiteralPath $previewPath -File -Recurse
        )
    }
    $hashes = @(
        $layerFiles |
            ForEach-Object {
                [pscustomobject]@{
                    name = $_.Name
                    sha256 = (
                        Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName
                    ).Hash.ToLowerInvariant()
                }
            }
    )

    return [pscustomobject][ordered]@{
        caseId = $CaseId
        outputPolicy = $OutputPolicy
        packagePath = $PackagePath
        tiffFileCount = $layerFiles.Count
        tiffBytes = [uint64]((
            $layerFiles | Measure-Object -Property Length -Sum
        ).Sum)
        previewFileCount = $previewFiles.Count
        previewBytes = [uint64]((
            $previewFiles | Measure-Object -Property Length -Sum
        ).Sum)
        timing = [pscustomobject]$timing
        tiffHashes = $hashes
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$evidenceRoot =
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputRoot))
$allowedOutputRoot =
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot "output"))
Assert-True (
    $evidenceRoot.StartsWith(
        $allowedOutputRoot,
        [System.StringComparison]::OrdinalIgnoreCase)) `
    "OutputRoot must remain under repository output"

New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$slicerPath = Resolve-Executable $buildRoot $Config "slicer_cli"
$ripPath = Resolve-Executable $buildRoot $Config "rip_reader_test"
$templatePath = Join-Path $repoRoot `
    "samples/configs/golden/material_process_top2_fixture.json"
$template = Get-Content -Raw -Encoding UTF8 -LiteralPath $templatePath |
    ConvertFrom-Json
$modelPath = [System.IO.Path]::GetFullPath(
    (Join-Path (Split-Path -Parent $templatePath) $template.input.modelPath))

$nativePackage = Join-Path $evidenceRoot "tiff_native/package"
$diagnosticPackage = Join-Path $evidenceRoot "diagnostics/package"
$nativeConfig = Join-Path $evidenceRoot "tiff_native/config.json"
$diagnosticConfig = Join-Path $evidenceRoot "diagnostics/config.json"

foreach ($package in @($nativePackage, $diagnosticPackage))
{
    if (Test-Path -LiteralPath $package)
    {
        Remove-Item -LiteralPath $package -Recurse -Force
    }
}

$native = Invoke-SliceCase `
    "tiff_native" `
    "tiff_native" `
    $false `
    $nativePackage `
    $nativeConfig `
    $slicerPath `
    $ripPath `
    $template `
    $modelPath
$diagnostics = Invoke-SliceCase `
    "tiff_native_with_diagnostics" `
    "tiff_native_with_diagnostics" `
    $true `
    $diagnosticPackage `
    $diagnosticConfig `
    $slicerPath `
    $ripPath `
    $template `
    $modelPath

Assert-True ($native.previewFileCount -eq 0) `
    "tiff_native generated duplicate diagnostic images"
Assert-True (
    -not (Test-Path -LiteralPath (Join-Path $nativePackage "preview"))) `
    "tiff_native created preview directory"
Assert-True ($diagnostics.previewFileCount -gt 0) `
    "diagnostic policy generated no images"
Assert-True ($native.tiffFileCount -eq $diagnostics.tiffFileCount) `
    "TIFF layer counts differ"
Assert-True ($native.tiffBytes -eq $diagnostics.tiffBytes) `
    "TIFF byte counts differ"
$nativeHashesJson =
    $native.tiffHashes | ConvertTo-Json -Compress -Depth 10
$diagnosticHashesJson =
    $diagnostics.tiffHashes | ConvertTo-Json -Compress -Depth 10
Assert-True ($nativeHashesJson -eq $diagnosticHashesJson) `
    "production TIFF hashes differ between preview policies"

$savedPreviewFileCount =
    $diagnostics.previewFileCount - $native.previewFileCount
$savedPreviewBytes =
    $diagnostics.previewBytes - $native.previewBytes
$savedPreviewWriteMs =
    [double]$diagnostics.timing.previewWriteMs `
    - [double]$native.timing.previewWriteMs
$result = [pscustomobject][ordered]@{
    schema = "slicesoft.preview_io_comparison.1"
    generatedAt = [DateTime]::UtcNow.ToString("o")
    buildConfig = $Config
    fixture = $templatePath
    productionTiffIdentical = $true
    native = $native
    diagnostics = $diagnostics
    savings = [pscustomobject][ordered]@{
        previewFileCount = $savedPreviewFileCount
        previewBytes = $savedPreviewBytes
        previewWriteMs = $savedPreviewWriteMs
    }
}
$resultPath = Join-Path $evidenceRoot "preview_io_comparison.json"
Write-Utf8NoBom $resultPath ($result | ConvertTo-Json -Depth 100)

Write-Host "PASS 13C-04 Preview IO comparison"
Write-Host "evidence=$resultPath"
Write-Host (
    "native tiff={0} preview={1} previewWriteMs={2}" -f
        $native.tiffFileCount,
        $native.previewFileCount,
        $native.timing.previewWriteMs)
Write-Host (
    "diagnostics tiff={0} preview={1} previewWriteMs={2}" -f
        $diagnostics.tiffFileCount,
        $diagnostics.previewFileCount,
        $diagnostics.timing.previewWriteMs)
