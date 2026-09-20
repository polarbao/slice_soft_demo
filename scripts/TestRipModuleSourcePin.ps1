[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolver = Join-Path $PSScriptRoot "ResolveRipModuleSource.ps1"
$packager = Join-Path $PSScriptRoot "PackageRipModule.ps1"

function Assert-Equal
{
    param(
        [Parameter(Mandatory = $true)][string]$Expected,
        [Parameter(Mandatory = $true)][string]$Actual,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if ($Expected -cne $Actual)
    {
        throw "$Message expected=$Expected actual=$Actual"
    }
}

function Assert-Throws
{
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$Message
    )
    try
    {
        & $Action
    }
    catch
    {
        return
    }
    throw $Message
}

$expectedBinary = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot "rip_project/RIPDLL_20260920"))
$expectedResource = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot "rip_project/RIPDLL_20260909/CmykFiles"))
Assert-Equal $expectedBinary (& $resolver -Component Binary) `
    "Default RIP binary pin mismatch."
Assert-Equal $expectedResource (& $resolver -Component Resource) `
    "Default RIP resource pin mismatch."

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "slicesoft-rip-source-pin-{0}" -f [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try
{
    $legacyPin = Join-Path $temporaryRoot "legacy.json"
    [ordered]@{
        schema = "slicesoft.rip.source.1"
        sourceDirectory = "rip_project/RIPDLL_20260909"
    } | ConvertTo-Json | Set-Content -LiteralPath $legacyPin -Encoding UTF8
    $legacyBinary = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot "rip_project/RIPDLL_20260909"))
    $legacyResource = Join-Path $legacyBinary "CmykFiles"
    Assert-Equal $legacyBinary (& $resolver -PinPath $legacyPin -Component Binary) `
        "Legacy RIP binary pin mismatch."
    Assert-Equal $legacyResource (& $resolver -PinPath $legacyPin -Component Resource) `
        "Legacy RIP resource pin mismatch."

    $invalidPins = @(
        [ordered]@{
            schema = "slicesoft.rip.source.3"
            binaryDirectory = "rip_project/RIPDLL_20260920"
            resourceDirectory = "rip_project/RIPDLL_20260909/CmykFiles"
        },
        [ordered]@{
            schema = "slicesoft.rip.source.2"
            binaryDirectory = "../RIPDLL_20260920"
            resourceDirectory = "rip_project/RIPDLL_20260909/CmykFiles"
        },
        [ordered]@{
            schema = "slicesoft.rip.source.2"
            binaryDirectory = "rip_project/RIPDLL_20260920"
            resourceDirectory = "rip_project/RIPDLL_20260909"
        }
    )
    for ($index = 0; $index -lt $invalidPins.Count; $index++)
    {
        $invalidPin = Join-Path $temporaryRoot "invalid-$index.json"
        $invalidPins[$index] | ConvertTo-Json | Set-Content `
            -LiteralPath $invalidPin -Encoding UTF8
        Assert-Throws { & $resolver -PinPath $invalidPin -Component Binary } `
            "Invalid RIP source pin was accepted: $index"
    }

    $missingBinary = Join-Path $temporaryRoot "RIPDLL_20990101"
    Assert-Throws {
        & $packager -SourceRoot $missingBinary `
            -Destination (Join-Path $temporaryRoot "missing-binary-output")
    } "Missing RIP binary source was accepted."

    $binaryOnly = Join-Path $temporaryRoot "RIPDLL_20990102"
    New-Item -ItemType Directory -Path $binaryOnly | Out-Null
    Assert-Throws {
        & $packager -SourceRoot $binaryOnly `
            -Destination (Join-Path $temporaryRoot "missing-resource-output")
    } "Missing RIP resource source was accepted."
}
finally
{
    if (Test-Path -LiteralPath $temporaryRoot)
    {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

Write-Host "RIP_MODULE_SOURCE_PIN_TEST_PASS"
