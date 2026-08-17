[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HostExecutable,
    [Parameter(Mandatory = $true)]
    [string]$ModuleDirectory,
    [Parameter(Mandatory = $true)]
    [string]$FakeRipCli,
    [Parameter(Mandatory = $true)]
    [string]$SourcePackage,
    [string]$OutputRoot = "output/ripflow/lifecycle_gate"
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

function New-TestPackage
{
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $package = Join-Path $Root "$Name/package"
    New-Item -ItemType Directory -Path $package -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $Source "manifest.json") -Destination $package
    Copy-Item -LiteralPath (Join-Path $Source "layers") -Destination $package -Recurse
    return $package
}

function Assert-NoPartialOutput
{
    param([Parameter(Mandatory = $true)][string]$Package)
    $staging = @(
        Get-ChildItem -LiteralPath $Package -Directory `
            -Filter ".rip.staging.*" -Force
    )
    if ((Test-Path -LiteralPath (Join-Path $Package "rip")) -or
        $staging.Count -ne 0)
    {
        throw "RIP lifecycle case left a published or staging output: $Package"
    }
}

$hostPath = Resolve-AbsolutePath $HostExecutable
$modulePath = Resolve-AbsolutePath $ModuleDirectory
$fakePath = Resolve-AbsolutePath $FakeRipCli
$sourcePath = Resolve-AbsolutePath $SourcePackage
$outputPath = Resolve-AbsolutePath $OutputRoot
foreach ($required in @(
    $hostPath,
    (Join-Path $modulePath "rip_module.json"),
    $fakePath,
    (Join-Path $sourcePath "manifest.json"),
    (Join-Path $sourcePath "layers")))
{
    if (-not (Test-Path -LiteralPath $required))
    {
        throw "RIP lifecycle gate input is missing: $required"
    }
}

$caseRoot = Join-Path $outputPath (
    "run.{0}" -f [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
$cancelPackage = New-TestPackage `
    -Source $sourcePath -Root $caseRoot -Name "cancel"
& $hostPath `
    --rip-job-self-test `
    --package $cancelPackage `
    --rip-module $modulePath `
    --transparent-mode explicit_transparent `
    --gray-bits 2 `
    --cancel-after-ms 1 `
    --expect cancel
if ($LASTEXITCODE -ne 0)
{
    throw "RIP cancellation self-test failed with exit code $LASTEXITCODE."
}
Assert-NoPartialOutput -Package $cancelPackage

$fakeModule = Join-Path $caseRoot "fake_module"
Copy-Item -LiteralPath $modulePath -Destination $fakeModule -Recurse
Copy-Item -LiteralPath $fakePath `
    -Destination (Join-Path $fakeModule "rip_cli.exe") -Force
$manifestPath = Join-Path $fakeModule "rip_module.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$fakeExecutable = Get-Item -LiteralPath (Join-Path $fakeModule "rip_cli.exe")
$entry = @($manifest.files | Where-Object { $_.path -eq "rip_cli.exe" })
if ($entry.Count -ne 1)
{
    throw "RIP fake module manifest has no unique rip_cli.exe entry."
}
$entry[0].size = [long]$fakeExecutable.Length
$entry[0].sha256 = (Get-FileHash `
    -LiteralPath $fakeExecutable.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest | ConvertTo-Json -Depth 8 | Set-Content `
    -LiteralPath $manifestPath -Encoding UTF8

$timeoutPackage = New-TestPackage `
    -Source $sourcePath -Root $caseRoot -Name "timeout"
& $hostPath `
    --rip-job-self-test `
    --package $timeoutPackage `
    --rip-module $fakeModule `
    --transparent-mode explicit_transparent `
    --gray-bits 2 `
    --timeout-seconds 1 `
    --expect timeout
if ($LASTEXITCODE -ne 0)
{
    throw "RIP timeout self-test failed with exit code $LASTEXITCODE."
}
Assert-NoPartialOutput -Package $timeoutPackage

foreach ($exitCode in @(1, 2))
{
    $exitPackage = New-TestPackage `
        -Source $sourcePath -Root $caseRoot -Name "exit_$exitCode"
    $env:RIPFLOW_FAKE_EXIT_CODE = [string]$exitCode
    try
    {
        & $hostPath `
            --rip-job-self-test `
            --package $exitPackage `
            --rip-module $fakeModule `
            --transparent-mode explicit_transparent `
            --gray-bits 2 `
            --expect failure
        if ($LASTEXITCODE -ne 0)
        {
            throw "RIP exit $exitCode self-test failed with exit code $LASTEXITCODE."
        }
    }
    finally
    {
        Remove-Item Env:RIPFLOW_FAKE_EXIT_CODE -ErrorAction SilentlyContinue
    }
    Assert-NoPartialOutput -Package $exitPackage
}

Write-Host (
    "RIPFLOW_LIFECYCLE_GATE_PASS root={0} cancel=true timeout=true exit1=true exit2=true clean=true" -f `
        $caseRoot)
