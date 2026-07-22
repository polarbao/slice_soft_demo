param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$OutputRoot = "output/benchmarks/12e_08c_r4_06_repaired_asset_intake",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Write-Utf8NoBom
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if ($parent)
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function Resolve-Executable
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildPath,
        [Parameter(Mandatory = $true)]
        [string]$BuildConfig,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $candidates = @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath "$Name.exe")
    )
    foreach ($candidate in $candidates)
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Unable to locate $Name.exe under $BuildPath"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$resolvedOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputRoot))
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target repaired_asset_intake repaired_asset_intake_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "R4-06 build failed with exit code $LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config -R "^repaired_asset_intake_unit_tests$" --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "R4-06 unit test failed with exit code $LASTEXITCODE"
}

$executable = Resolve-Executable -BuildPath $resolvedBuildDir -BuildConfig $Config -Name "repaired_asset_intake"
$cases = @(
    [ordered]@{
        familyId = "required_aishen_family"
        candidateId = "current_aishen_damuzhi"
        modelPath = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj"
    },
    [ordered]@{
        familyId = "required_meigui_family"
        candidateId = "current_meigui_04"
        modelPath = "model/obj/meigui_fudiao/04.obj"
    },
    [ordered]@{
        familyId = "required_titian_family"
        candidateId = "current_titian_dmz"
        modelPath = "model/obj/titian_fudiao/dmz.obj"
    }
)

$summaryCases = @()
foreach ($case in $cases)
{
    $caseDir = Join-Path $resolvedOutputRoot $case.candidateId
    New-Item -ItemType Directory -Path $caseDir -Force | Out-Null
    $modelPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.modelPath))
    $configPath = Join-Path $caseDir "slice_config.json"
    $manifestPath = Join-Path $caseDir "intake_manifest.json"
    $reportPath = Join-Path $caseDir "intake_report.json"

    $configDocument = [ordered]@{
        input = [ordered]@{
            modelPath = $modelPath
            format = "obj"
        }
        texture = [ordered]@{
            missingTexturePolicy = "fail_fast"
        }
        autoOrient = [ordered]@{
            enabled = $true
            maxHeightMm = 6.0
            strategy = "minimize_height_by_right_angle_rotation"
        }
    }
    Write-Utf8NoBom -Path $configPath -Content ($configDocument | ConvertTo-Json -Depth 8)

    $sourceHash = (Get-FileHash -LiteralPath $modelPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestDocument = [ordered]@{
        schema = "slicesoft.repaired_asset_intake_manifest.12e_08c_r4.1"
        familyId = $case.familyId
        candidateId = $case.candidateId
        candidateKind = "strict_pass_original"
        originalConfig = $configPath
        candidateConfig = $configPath
        expectedOriginalSourceHash = $sourceHash
        expectedCandidateSourceHash = $sourceHash
        approval = [ordered]@{
            maxDimensionDeltaMm = 0.10
            allowAttributeChanges = $false
            attributeChangeReason = ""
        }
        preflight = [ordered]@{
            voxelMm = 0.10
            maxCompleteSelfIntersectionCandidatePairs = 5000000
        }
    }
    Write-Utf8NoBom -Path $manifestPath -Content ($manifestDocument | ConvertTo-Json -Depth 8)

    & $executable --manifest $manifestPath --output $reportPath
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 2)
    {
        throw "$($case.candidateId) expected blocked exit code 2, actual $exitCode"
    }
    $report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
    if ($report.status -ne "blocked" -or $report.requiredFamilyPassCount -ne 0)
    {
        throw "$($case.candidateId) did not preserve the current blocked family baseline"
    }
    if ($report.reasonCodes -notcontains "E_12E_INTAKE_POST_STRICT_FAILED")
    {
        throw "$($case.candidateId) is missing the post-strict blocker"
    }
    if ($report.productionOutputWritten)
    {
        throw "$($case.candidateId) unexpectedly wrote production output"
    }
    $summaryCases += [ordered]@{
        familyId = $case.familyId
        candidateId = $case.candidateId
        status = $report.status
        requiredFamilyPassCount = $report.requiredFamilyPassCount
        reasonCodes = @($report.reasonCodes)
    }
}

$summary = [ordered]@{
    schema = "slicesoft.repaired_asset_intake_matrix.12e_08c_r4.1"
    diagnosticOnly = $true
    productionOutputWritten = $false
    requiredFamilyCount = 3
    admittedFamilyCount = 0
    matrixStatus = "blocked"
    cases = $summaryCases
}
$summaryPath = Join-Path $resolvedOutputRoot "required_family_matrix.json"
Write-Utf8NoBom -Path $summaryPath -Content ($summary | ConvertTo-Json -Depth 10)

Write-Host "R4-06 required-family intake contract: PASS"
Write-Host "Current real family matrix: 0/3 BLOCKED"
Write-Host "Summary: $summaryPath"
