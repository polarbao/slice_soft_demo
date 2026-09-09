[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$HostExecutable,
    [Parameter(Mandatory = $true)][string]$ModuleDirectory,
    [Parameter(Mandatory = $true)][string]$SourcePackage,
    [string]$OutputRoot = 'output/ripflow/ink_mode_matrix'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$hostPath = (Resolve-Path -LiteralPath $HostExecutable).Path
$modulePath = (Resolve-Path -LiteralPath $ModuleDirectory).Path
$sourcePath = (Resolve-Path -LiteralPath $SourcePackage).Path
$root = [IO.Path]::GetFullPath((Join-Path $OutputRoot ([guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Path $root | Out-Null
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $sourcePath 'layers') -File)
$sourceHashes = @($inputs | Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash)
$expectedCount = $inputs.Count
if ($expectedCount -eq 0) { throw 'Source package contains no layers.' }
$cases = @()
foreach ($mode in 0, 1)
{
    foreach ($color in 0, 1, 2, 3, 4)
    {
        $package = Join-Path $root "mode${mode}_color${color}/package"
        New-Item -ItemType Directory -Path $package -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $sourcePath 'manifest.json') -Destination $package
        Copy-Item -LiteralPath (Join-Path $sourcePath 'layers') -Destination $package -Recurse
        $log = & $hostPath --rip-job-self-test --package $package `
            --rip-module $modulePath --ripmode $mode --transparent-mode $color `
            --gray-bits 2 --timeout-seconds 3600 `
            --output-validation-mode diagnostic_unvalidated --expect diagnostic 2>&1
        $exitCode = $LASTEXITCODE
        $log | Set-Content -LiteralPath (Join-Path (Split-Path $package) 'host.log') -Encoding UTF8
        if ($exitCode -ne 0) { throw "Host RIP failed for $mode/$color : $log" }
        $resultPath = Join-Path $package 'rip_diagnostic/rip_diagnostic_result.json'
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($result.schema -ne 'slicesoft.rip.diagnostic.3' -or
            $result.settings.ripMode -ne $mode -or
            $result.settings.transparentMode -ne $color -or
            $result.module.version -ne '1.2.0' -or
            $result.output.layerCount -ne $expectedCount -or
            $result.output.s2PublicationEligible -ne $false -or
            (Test-Path -LiteralPath (Join-Path $package 'rip')))
        {
            throw "Host RIP contract mismatch for $mode/$color"
        }
        $outputs = @(Get-ChildItem -LiteralPath (Join-Path $package 'rip_diagnostic') -Filter '*.tif' -File)
        if ($outputs.Count -ne $expectedCount) { throw 'Missing published layers.' }
        $cases += [ordered]@{
            ripMode = $mode
            transparentMode = $color
            layerCount = $result.output.layerCount
            s2DropLimitsPassed = $result.output.s2DropLimitsPassed
            minimum = $result.output.minimum
            maximum = $result.output.maximum
            elapsedMs = $result.process.elapsedMs
            resultPath = $resultPath
        }
        Write-Host "RIP_INK_MODE_CASE_PASS mode=$mode color=$color layers=$expectedCount"
    }
}
foreach ($file in $sourceHashes)
{
    if ((Get-FileHash -LiteralPath $file.Path -Algorithm SHA256).Hash -ne $file.Hash)
    {
        throw "RIP modified source input: $($file.Path)"
    }
}
[ordered]@{
    schema = 'slicesoft.rip.ink_mode.matrix.1'
    status = 'PASS'
    sourcePackage = $sourcePath
    moduleDirectory = $modulePath
    sourceUnchanged = $true
    physicalPrintValidated = $false
    cases = $cases
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $root 'matrix.json') -Encoding UTF8
Write-Host "RIP_INK_MODE_MATRIX_PASS cases=$($cases.Count) root=$root"
