param(
    [string]$IntakeSummaryPath =
        "output/benchmarks/12e_08c_r4_06_repaired_asset_intake/development_gate_matrix.json",
    [string]$FourCaseSummaryPath =
        "output/benchmarks/12e_08c_r4_07_development_gate/four_case_development_summary.json",
    [string]$ExpectationsPath =
        "tests/golden/expected/12e_r4_07_restricted_candidate_expectations.json",
    [string]$OutputPath =
        "output/benchmarks/12e_08c_r4_07_restricted_candidate/restricted_candidate_summary.json"
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

function Assert-Equal
{
    param(
        $Actual,
        $Expected,
        [string]$Message
    )

    if ($Actual -ne $Expected)
    {
        throw "$Message expected=$Expected actual=$Actual"
    }
}

function Read-Json
{
    param([string]$Path)

    Assert-True (Test-Path -LiteralPath $Path) "JSON 文件不存在：$Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Resolve-RepositoryPath
{
    param(
        [string]$RepositoryRoot,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $Path))
}

function Write-Utf8NoBom
{
    param(
        [string]$Path,
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

function Get-Sha256
{
    param([string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedIntakeSummaryPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $IntakeSummaryPath
$resolvedFourCaseSummaryPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $FourCaseSummaryPath
$resolvedExpectationsPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $ExpectationsPath
$resolvedOutputPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $OutputPath

$intakeSummary = Read-Json $resolvedIntakeSummaryPath
$fourCaseSummary = Read-Json $resolvedFourCaseSummaryPath
$expectations = Read-Json $resolvedExpectationsPath

Assert-Equal `
    $intakeSummary.schema `
    "slicesoft.r4_07_development_gate.12e_08c_r4.1" `
    "intake summary schema"
Assert-Equal `
    $fourCaseSummary.schema `
    "slicesoft.r4_four_case_development_gate.12e_08c_r4.1" `
    "four-case summary schema"
Assert-Equal `
    $expectations.schema `
    "slicesoft.r4_restricted_production_candidate_expectations.12e_08c_r4.1" `
    "expectations schema"

Assert-Equal $intakeSummary.diagnosticOnly $true "intake diagnosticOnly"
Assert-Equal $intakeSummary.productionOutputWritten $false "intake productionOutputWritten"
Assert-Equal $fourCaseSummary.diagnosticOnly $true "four-case diagnosticOnly"
Assert-Equal $fourCaseSummary.productionOutputWritten $false "four-case productionOutputWritten"
Assert-Equal $fourCaseSummary.build.buildType "Release" "four-case build type"
Assert-Equal $fourCaseSummary.build.useOpenVdb $false "four-case OpenVDB default"
Assert-Equal `
    $fourCaseSummary.legacyRegression `
    $expectations.requiredLegacyRegression `
    "legacy TIFF/RIP regression"

$admittedCandidates = @($intakeSummary.cases | Where-Object { $_.admitted })
$admittedFamilies = @(
    $admittedCandidates |
        Select-Object -ExpandProperty modelFamilyId -Unique
)
Assert-True `
    ($admittedFamilies.Count -ge $expectations.minimumIndependentModelFamilies) `
    "独立 admitted 模型族数量不足"
Assert-Equal `
    $intakeSummary.admittedIndependentModelFamilyCount `
    $admittedFamilies.Count `
    "intake independent family count"
Assert-Equal `
    $intakeSummary.restrictedCandidateIdentityGatePass `
    $true `
    "restricted candidate identity gate"

$candidateResults = @()
foreach ($expectedCandidate in $expectations.candidates)
{
    $candidate = @(
        $admittedCandidates |
            Where-Object { $_.candidateId -eq $expectedCandidate.candidateId }
    )
    Assert-Equal $candidate.Count 1 "$($expectedCandidate.candidateId) candidate identity"
    Assert-Equal `
        $candidate[0].modelFamilyId `
        $expectedCandidate.modelFamilyId `
        "$($expectedCandidate.candidateId) model family"
    Assert-Equal `
        $candidate[0].modelPath `
        $expectedCandidate.modelPath `
        "$($expectedCandidate.candidateId) model path"
    Assert-True (-not [string]::IsNullOrWhiteSpace($candidate[0].sourceHash)) `
        "$($expectedCandidate.candidateId) sourceHash 为空"
    Assert-True (-not [string]::IsNullOrWhiteSpace($candidate[0].resourceHash)) `
        "$($expectedCandidate.candidateId) resourceHash 为空"
    Assert-True (-not [string]::IsNullOrWhiteSpace($candidate[0].geometryHash)) `
        "$($expectedCandidate.candidateId) geometryHash 为空"
    Assert-True (-not [string]::IsNullOrWhiteSpace($candidate[0].attributeHash)) `
        "$($expectedCandidate.candidateId) attributeHash 为空"
    Assert-True (-not [string]::IsNullOrWhiteSpace($candidate[0].auditHash)) `
        "$($expectedCandidate.candidateId) auditHash 为空"

    $candidateResults += [ordered]@{
        candidateId = $candidate[0].candidateId
        modelFamilyId = $candidate[0].modelFamilyId
        modelPath = $candidate[0].modelPath
        sourceHash = $candidate[0].sourceHash
        resourceHash = $candidate[0].resourceHash
        geometryHash = $candidate[0].geometryHash
        attributeHash = $candidate[0].attributeHash
        auditHash = $candidate[0].auditHash
        admitted = $true
    }
}

Assert-Equal `
    @($fourCaseSummary.cases).Count `
    @($expectations.fourCases).Count `
    "four-case count"
$caseResults = @()
$globalCoreMaxMs = 0.0
$peakWorkingSetMaxBytes = [uint64]0
foreach ($expectedCase in $expectations.fourCases)
{
    $case = @(
        $fourCaseSummary.cases |
            Where-Object { $_.caseId -eq $expectedCase.caseId }
    )
    Assert-Equal $case.Count 1 "$($expectedCase.caseId) case identity"
    Assert-Equal `
        $case[0].intakeCandidateId `
        $expectedCase.candidateId `
        "$($expectedCase.caseId) candidate identity"
    Assert-Equal $case[0].pass $true "$($expectedCase.caseId) result"
    Assert-True `
        ($case[0].sampleCount -ge $expectations.minimumReleaseMeasurementsPerCase) `
        "$($expectedCase.caseId) Release 样本不足"
    Assert-Equal `
        @($case[0].measurements).Count `
        $case[0].sampleCount `
        "$($expectedCase.caseId) measurement count"

    foreach ($measurement in $case[0].measurements)
    {
        Assert-True `
            ([double]$measurement.globalCoreMs -gt 0.0) `
            "$($expectedCase.caseId) globalCoreMs 必须大于 0"
        Assert-True `
            ([uint64]$measurement.peakWorkingSetBytes -gt 0) `
            "$($expectedCase.caseId) peakWorkingSetBytes 必须大于 0"
    }

    $globalCoreMaxMs = [math]::Max(
        $globalCoreMaxMs,
        [double]$case[0].globalCoreMaxMs)
    $peakWorkingSetMaxBytes = [math]::Max(
        $peakWorkingSetMaxBytes,
        [uint64]$case[0].peakWorkingSetMaxBytes)

    $caseResults += [ordered]@{
        caseId = $case[0].caseId
        candidateId = $case[0].intakeCandidateId
        widthSelector = $case[0].widthSelector
        widthMm = [double]$case[0].widthMm
        sampleCount = [int]$case[0].sampleCount
        globalCoreMedianMs = [double]$case[0].globalCoreMedianMs
        globalCoreMaxMs = [double]$case[0].globalCoreMaxMs
        peakWorkingSetMaxBytes = [uint64]$case[0].peakWorkingSetMaxBytes
        pass = $true
    }
}

$summary = [ordered]@{
    schema = "slicesoft.r4_restricted_production_candidate.12e_08c_r4.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R4-07-R1"
    scope = "restricted_production_candidate"
    diagnosticOnly = $true
    productionOutputWritten = $false
    sourceEvidence = [ordered]@{
        intakeSummaryPath = $resolvedIntakeSummaryPath
        intakeSummarySha256 = Get-Sha256 $resolvedIntakeSummaryPath
        fourCaseSummaryPath = $resolvedFourCaseSummaryPath
        fourCaseSummarySha256 = Get-Sha256 $resolvedFourCaseSummaryPath
        expectationsPath = $resolvedExpectationsPath
        expectationsSha256 = Get-Sha256 $resolvedExpectationsPath
    }
    admission = [ordered]@{
        minimumIndependentModelFamilies =
            [int]$expectations.minimumIndependentModelFamilies
        admittedIndependentModelFamilies = $admittedFamilies.Count
        modelFamilies = @($admittedFamilies)
        candidates = $candidateResults
        identityGatePass = $true
    }
    fourCase = [ordered]@{
        requiredCount = @($expectations.fourCases).Count
        passedCount = @($caseResults | Where-Object { $_.pass }).Count
        pass = $true
        cases = $caseResults
    }
    releaseMeasurement = [ordered]@{
        buildType = $fourCaseSummary.build.buildType
        backend = $fourCaseSummary.build.backend
        voxelMm = [double]$fourCaseSummary.build.voxelMm
        minimumSamplesPerCase =
            [int]$expectations.minimumReleaseMeasurementsPerCase
        observedGlobalCoreMaxMs = $globalCoreMaxMs
        observedPeakWorkingSetMaxBytes = $peakWorkingSetMaxBytes
        budgetStatus = "measured_not_frozen"
    }
    legacyRegression = [ordered]@{
        status = $fourCaseSummary.legacyRegression
        pass = $true
    }
    remainingBlockers = @(
        "release_budget_not_frozen",
        "quick_ci_baseline_unresolved",
        "explicit_08d_authorization_missing"
    )
    result = [ordered]@{
        candidateEvidencePass = $true
        productionAdmission = "not_evaluated"
        nextTask = "12E-08C-R4-07-R2"
    }
}

Write-Utf8NoBom `
    -Path $resolvedOutputPath `
    -Content ($summary | ConvertTo-Json -Depth 100)

Write-Host "R4-07-R1 restricted production candidate evidence: PASS"
Write-Host "Independent model families: $($admittedFamilies.Count)"
Write-Host "Observed global core max: $globalCoreMaxMs ms"
Write-Host "Observed peak working set max: $peakWorkingSetMaxBytes bytes"
Write-Host "Production admission: NOT EVALUATED"
Write-Host "Summary: $resolvedOutputPath"
