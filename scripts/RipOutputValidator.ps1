[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ContractPath,
    [Parameter(Mandatory = $true)]
    [string]$DescriptorPath,
    [string]$ExpectCode = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RequiredProperty
{
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ErrorCode
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        throw "$ErrorCode missing property '$Name'"
    }
    return $property.Value
}

function Assert-Equal
{
    param(
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)][string]$ErrorCode,
        [Parameter(Mandatory = $true)][string]$Field
    )

    if ($Actual -ne $Expected)
    {
        throw "$ErrorCode $Field expected '$Expected' but was '$Actual'"
    }
}

function Assert-ArrayEqual
{
    param(
        [Parameter(Mandatory = $true)][object[]]$Actual,
        [Parameter(Mandatory = $true)][object[]]$Expected,
        [Parameter(Mandatory = $true)][string]$ErrorCode,
        [Parameter(Mandatory = $true)][string]$Field
    )

    if (($Actual -join ",") -ne ($Expected -join ","))
    {
        throw "$ErrorCode $Field expected '$($Expected -join ',')' but was '$($Actual -join ',')'"
    }
}

function Invoke-S2Validation
{
    param(
        [Parameter(Mandatory = $true)]$Contract,
        [Parameter(Mandatory = $true)]$Descriptor
    )

    $codes = Get-RequiredProperty $Contract "validationCodes" "S2_CONTRACT_INVALID"

    $c1 = $codes.C1
    $inputRule = Get-RequiredProperty $Contract "input" $c1
    $input = Get-RequiredProperty $Descriptor "input" $c1
    Assert-Equal $input.packageSchema $inputRule.packageSchema $c1 "input.packageSchema"
    Assert-ArrayEqual @($input.channelOrder) @($inputRule.channelOrder) $c1 "input.channelOrder"
    Assert-Equal $input.bitDepth $inputRule.bitDepth $c1 "input.bitDepth"
    Assert-Equal $input.polarity $inputRule.polarity $c1 "input.polarity"
    Assert-Equal $input.printValue $inputRule.printValue $c1 "input.printValue"
    Assert-Equal $input.emptyValue $inputRule.emptyValue $c1 "input.emptyValue"

    $c2 = $codes.C2
    $device = Get-RequiredProperty $Descriptor "device" $c2
    $grayBits = Get-RequiredProperty $device "grayBits" $c2
    if (@($Contract.device.allowedGrayBits) -notcontains [int]$grayBits)
    {
        throw "$c2 device.grayBits must be one of $($Contract.device.allowedGrayBits -join ',')"
    }

    $c3 = $codes.C3
    $quantizationRuleProperty = $Contract.quantization.PSObject.Properties[[string]$grayBits]
    if ($null -eq $quantizationRuleProperty)
    {
        throw "$c3 quantization rule is missing for grayBits=$grayBits"
    }
    $quantizationRule = $quantizationRuleProperty.Value
    $quantization = Get-RequiredProperty $Descriptor "quantization" $c3
    foreach ($channel in @("W", "S", "V"))
    {
        $actualMaximum = Get-RequiredProperty $quantization.maxDrops $channel $c3
        $expectedMaximum = Get-RequiredProperty $quantizationRule $channel $c3
        Assert-Equal $actualMaximum $expectedMaximum $c3 "quantization.maxDrops.$channel"
    }

    $c4 = $codes.C4
    $white = Get-RequiredProperty $Descriptor "whiteSemantics" $c4
    if (@($Contract.whiteSemantics.allowed) -notcontains [string]$white.manifest)
    {
        throw "$c4 whiteSemantics.manifest is unsupported"
    }
    Assert-Equal $white.authority $Contract.whiteSemantics.authority $c4 "whiteSemantics.authority"
    Assert-Equal $white.profile $white.manifest $c4 "whiteSemantics.profile"
    Assert-Equal $white.sentinelMode $Contract.whiteSemantics.sentinelMode $c4 "whiteSemantics.sentinelMode"
    if ($white.manifest -eq "opaque")
    {
        Assert-ArrayEqual @($white.opaqueRgbwsv) @($Contract.whiteSemantics.opaqueRgbwsv) $c4 "whiteSemantics.opaqueRgbwsv"
    }

    $c5 = $codes.C5
    $output = Get-RequiredProperty $Descriptor "output" $c5
    Assert-Equal $output.organization $Contract.output.organization $c5 "output.organization"
    Assert-Equal $output.filePattern $Contract.output.filePattern $c5 "output.filePattern"
    if ([int]$output.samplesPerPixel -lt [int]$Contract.output.minimumSamplesPerPixel)
    {
        throw "$c5 output.samplesPerPixel must be at least $($Contract.output.minimumSamplesPerPixel)"
    }
    Assert-Equal $output.storage $Contract.output.storage $c5 "output.storage"
    Assert-Equal ([bool]$output.tiled) ([bool]$Contract.output.tiled) $c5 "output.tiled"

    $c6 = $codes.C6
    $dropRange = Get-RequiredProperty $output "dropRange" $c6
    $sampleDrops = Get-RequiredProperty $output "sampleDrops" $c6
    foreach ($channel in @("W", "S", "V"))
    {
        $range = @(Get-RequiredProperty $dropRange $channel $c6)
        if ($range.Count -ne 2 -or [int]$range[0] -ne 0)
        {
            throw "$c6 output.dropRange.$channel must be [0,max]"
        }
        $expectedMaximum = [int](Get-RequiredProperty $quantizationRule $channel $c6)
        if ([int]$range[1] -ne $expectedMaximum)
        {
            throw "$c6 output.dropRange.$channel maximum must be $expectedMaximum"
        }
        foreach ($drop in @(Get-RequiredProperty $sampleDrops $channel $c6))
        {
            if ([int]$drop -lt 0 -or [int]$drop -gt $expectedMaximum)
            {
                throw "$c6 output.sampleDrops.$channel contains out-of-range value $drop"
            }
        }
    }

    $c7 = $codes.C7
    $mixing = Get-RequiredProperty $Descriptor "supportMixing" $c7
    Assert-Equal $mixing.supportParts $Contract.supportMixing.supportParts $c7 "supportMixing.supportParts"
    Assert-Equal $mixing.varnishParts $Contract.supportMixing.varnishParts $c7 "supportMixing.varnishParts"
    Assert-Equal $mixing.clampMax $Contract.supportMixing.clampMax $c7 "supportMixing.clampMax"
    Assert-Equal $mixing.enabledBy $Contract.supportMixing.enabledBy $c7 "supportMixing.enabledBy"
    $external = Get-RequiredProperty $Descriptor "externalPolarityMapping" $c7
    Assert-Equal $external.owner $Contract.externalPolarityMapping.owner $c7 "externalPolarityMapping.owner"
    Assert-Equal $external.status $Contract.externalPolarityMapping.allowedLocalGateStatus $c7 "externalPolarityMapping.status"
    if (@($Contract.externalPolarityMapping.mustNotClaim) -contains [string]$external.status)
    {
        throw "$c7 externalPolarityMapping.status makes a prohibited external claim"
    }
}

try
{
    $contract = Get-Content -LiteralPath $ContractPath -Raw | ConvertFrom-Json
    $descriptor = Get-Content -LiteralPath $DescriptorPath -Raw | ConvertFrom-Json
    Invoke-S2Validation -Contract $contract -Descriptor $descriptor

    if (-not [string]::IsNullOrWhiteSpace($ExpectCode))
    {
        Write-Error "Expected validation code '$ExpectCode', but descriptor passed"
        exit 1
    }

    Write-Host "RIP_OUTPUT_VALIDATOR_PASS descriptor=$DescriptorPath"
    exit 0
}
catch
{
    $message = $_.Exception.Message
    if (-not [string]::IsNullOrWhiteSpace($ExpectCode) -and
        $message.StartsWith($ExpectCode, [System.StringComparison]::Ordinal))
    {
        Write-Host "RIP_OUTPUT_VALIDATOR_EXPECTED_FAILURE code=$ExpectCode descriptor=$DescriptorPath"
        exit 0
    }

    Write-Error $message
    exit 1
}
