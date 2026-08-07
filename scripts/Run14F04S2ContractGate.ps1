[CmdletBinding()]
param(
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

function Invoke-Validator
{
    param(
        [Parameter(Mandatory = $true)][string]$PowerShellPath,
        [Parameter(Mandatory = $true)][string]$ValidatorPath,
        [Parameter(Mandatory = $true)][string]$ContractPath,
        [Parameter(Mandatory = $true)][string]$DescriptorPath,
        [string]$ExpectedCode = ""
    )

    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $ValidatorPath,
        "-ContractPath", $ContractPath,
        "-DescriptorPath", $DescriptorPath
    )
    if (-not [string]::IsNullOrWhiteSpace($ExpectedCode))
    {
        $arguments += @("-ExpectCode", $ExpectedCode)
    }

    & $PowerShellPath @arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "rip_output_validator failed for '$DescriptorPath' with exit code $LASTEXITCODE"
    }
}

$repository = Resolve-AbsolutePath $RepositoryRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repository "build-slicesoft/main/stage14f04_evidence/Release"
}
$evidenceRoot = Resolve-AbsolutePath $OutputRoot
$repositoryPrefix = $repository.TrimEnd('\') + '\'
if (-not $evidenceRoot.StartsWith($repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase))
{
    throw "OutputRoot must remain inside the repository: $evidenceRoot"
}

$contractPath = Join-Path $repository "contracts/slicer_rip_s2_contract.json"
$validatorPath = Join-Path $repository "scripts/RipOutputValidator.ps1"
$fixtureRoot = Join-Path $repository "tests/contracts/stage14f04"
$positiveFixtures = @(
    (Join-Path $fixtureRoot "s2_valid_graybits1.json"),
    (Join-Path $fixtureRoot "s2_valid_graybits2.json")
)
foreach ($requiredPath in @($contractPath, $validatorPath) + $positiveFixtures)
{
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf))
    {
        throw "Required Stage 14F-04 input is missing: $requiredPath"
    }
}

$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($contract.schema -ne "slicesoft.rip.s2.contract.1" -or
    $contract.contractId -ne "s2.rip_output.1" -or
    $contract.status -ne "INTERFACE_FROZEN")
{
    throw "S2 contract identity or freeze status is invalid"
}
$codeValues = @(
    $contract.validationCodes.C1,
    $contract.validationCodes.C2,
    $contract.validationCodes.C3,
    $contract.validationCodes.C4,
    $contract.validationCodes.C5,
    $contract.validationCodes.C6,
    $contract.validationCodes.C7
)
if (($codeValues | Select-Object -Unique).Count -ne 7)
{
    throw "S2 C1-C7 validation codes must be unique"
}

if (Test-Path -LiteralPath $evidenceRoot)
{
    Remove-Item -LiteralPath $evidenceRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$negativeRoot = Join-Path $evidenceRoot "negative_descriptors"
New-Item -ItemType Directory -Path $negativeRoot -Force | Out-Null

$powerShellPath = Join-Path $PSHOME "powershell.exe"
foreach ($fixture in $positiveFixtures)
{
    Invoke-Validator `
        -PowerShellPath $powerShellPath `
        -ValidatorPath $validatorPath `
        -ContractPath $contractPath `
        -DescriptorPath $fixture
}

$negativeCases = @(
    [ordered]@{ name = "bad_c1_input_contract"; code = $contract.validationCodes.C1 },
    [ordered]@{ name = "bad_c2_gray_bits"; code = $contract.validationCodes.C2 },
    [ordered]@{ name = "bad_c3_quantization"; code = $contract.validationCodes.C3 },
    [ordered]@{ name = "bad_c4_white_semantics"; code = $contract.validationCodes.C4 },
    [ordered]@{ name = "bad_c5_output_layout"; code = $contract.validationCodes.C5 },
    [ordered]@{ name = "bad_c6_drop_range"; code = $contract.validationCodes.C6 },
    [ordered]@{ name = "bad_c7_external_state"; code = $contract.validationCodes.C7 }
)

$baseJson = Get-Content -LiteralPath $positiveFixtures[1] -Raw
foreach ($case in $negativeCases)
{
    $descriptor = $baseJson | ConvertFrom-Json
    switch ($case.name)
    {
        "bad_c1_input_contract" { $descriptor.input.bitDepth = 16 }
        "bad_c2_gray_bits" { $descriptor.device.grayBits = 3 }
        "bad_c3_quantization" { $descriptor.quantization.maxDrops.W = 7 }
        "bad_c4_white_semantics" { $descriptor.whiteSemantics.profile = "transparent" }
        "bad_c5_output_layout" { $descriptor.output.tiled = $true }
        "bad_c6_drop_range" { $descriptor.output.sampleDrops.S = @(0, 9, 10) }
        "bad_c7_external_state" { $descriptor.externalPolarityMapping.status = "PASS" }
        default { throw "Unknown Stage 14F-04 negative case: $($case.name)" }
    }

    $descriptorPath = Join-Path $negativeRoot "$($case.name).json"
    $descriptor | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $descriptorPath -Encoding UTF8
    Invoke-Validator `
        -PowerShellPath $powerShellPath `
        -ValidatorPath $validatorPath `
        -ContractPath $contractPath `
        -DescriptorPath $descriptorPath `
        -ExpectedCode $case.code
}

$evidence = [ordered]@{
    schema = "slicesoft.stage14f04.s2_gate.1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    contract = [ordered]@{
        path = $contractPath
        contractId = $contract.contractId
        status = $contract.status
    }
    positiveCases = @(
        [ordered]@{ name = "grayBits1"; result = "PASS" },
        [ordered]@{ name = "grayBits2"; result = "PASS" }
    )
    negativeCases = @($negativeCases | ForEach-Object {
        [ordered]@{ name = $_.name; expectedCode = $_.code; result = "PASS" }
    })
    localGate = "PASS"
    externalRipValidation = "EXTERNAL_VALIDATION_DEFERRED"
    externalPolarityMapping = "EXTERNAL_VALIDATION_DEFERRED"
}
$evidencePath = Join-Path $evidenceRoot "stage14f04_s2_gate.json"
$evidence | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $evidencePath -Encoding UTF8

Write-Host "STAGE14F04_S2_CONTRACT_GATE_PASS positive=2 negative=$($negativeCases.Count)"
Write-Host "evidence=$evidencePath"
