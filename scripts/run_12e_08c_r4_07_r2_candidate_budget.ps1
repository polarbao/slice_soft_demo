param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$R1SummaryPath =
        "output/benchmarks/12e_08c_r4_07_restricted_candidate/restricted_candidate_summary.json",
    [string]$PolicyPath =
        "tests/golden/expected/12e_r4_07_r2_candidate_budget_policy.json",
    [string]$MeasurementSummaryPath = "",
    [string]$OutputRoot =
        "output/benchmarks/12e_08c_r4_07_r2_candidate_budget",
    [int]$MeasurementCount = 5,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

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

function Assert-LessOrEqual
{
    param(
        [double]$Actual,
        [double]$ExpectedMaximum,
        [string]$Message
    )

    if ($Actual -gt $ExpectedMaximum)
    {
        throw "$Message maximum=$ExpectedMaximum actual=$Actual"
    }
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

function Read-Json
{
    param([string]$Path)

    Assert-True (Test-Path -LiteralPath $Path) "JSON 文件不存在：$Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
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

function Get-CMakeValue
{
    param(
        [string]$CachePath,
        [string]$Name
    )

    $line = Get-Content -LiteralPath $CachePath |
        Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=(.*)$" } |
        Select-Object -First 1
    Assert-True (-not [string]::IsNullOrWhiteSpace($line)) `
        "CMake cache 缺少 $Name"
    return ($line -replace "^[^=]+=", "")
}

function Get-CompilerIdentity
{
    param([string]$BuildPath)

    $compilerFile = Get-ChildItem -LiteralPath (Join-Path $BuildPath "CMakeFiles") `
        -Filter "CMakeCXXCompiler.cmake" -Recurse |
        Select-Object -First 1
    Assert-True ($null -ne $compilerFile) "找不到 CMakeCXXCompiler.cmake"
    $content = Get-Content -LiteralPath $compilerFile.FullName

    $idLine = $content |
        Where-Object { $_ -match '^set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)' } |
        Select-Object -First 1
    $versionLine = $content |
        Where-Object { $_ -match '^set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)' } |
        Select-Object -First 1
    $architectureLine = $content |
        Where-Object { $_ -match '^set\(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "([^"]+)"\)' } |
        Select-Object -First 1

    Assert-True (-not [string]::IsNullOrWhiteSpace($idLine)) "缺少 compiler id"
    Assert-True (-not [string]::IsNullOrWhiteSpace($versionLine)) "缺少 compiler version"
    Assert-True (-not [string]::IsNullOrWhiteSpace($architectureLine)) `
        "缺少 compiler architecture"

    $null = $idLine -match '^set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)'
    $compilerId = $Matches[1]
    $null = $versionLine -match '^set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)'
    $compilerVersion = $Matches[1]
    $null = $architectureLine -match `
        '^set\(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "([^"]+)"\)'
    $compilerArchitecture = $Matches[1]

    return [ordered]@{
        id = $compilerId
        version = $compilerVersion
        architecture = $compilerArchitecture
        identityFile = $compilerFile.FullName
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $BuildDir
$resolvedR1SummaryPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $R1SummaryPath
$resolvedPolicyPath =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $PolicyPath
$resolvedOutputRoot =
    Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $OutputRoot
$resolvedOutputPath = Join-Path $resolvedOutputRoot "candidate_budget_summary.json"
$cachePath = Join-Path $resolvedBuildDir "CMakeCache.txt"

$r1Summary = Read-Json $resolvedR1SummaryPath
$policy = Read-Json $resolvedPolicyPath

Assert-Equal `
    $r1Summary.schema `
    "slicesoft.r4_restricted_production_candidate.12e_08c_r4.1" `
    "R1 candidate schema"
Assert-Equal `
    $policy.schema `
    "slicesoft.r4_restricted_candidate_budget_policy.12e_08c_r4.1" `
    "budget policy schema"
Assert-Equal $r1Summary.result.candidateEvidencePass $true "R1 candidate evidence"
Assert-Equal $r1Summary.result.productionAdmission "not_evaluated" `
    "R1 production admission"
Assert-Equal $r1Summary.productionOutputWritten $false "R1 production output"
Assert-True (Test-Path -LiteralPath $cachePath) "CMake cache 不存在：$cachePath"

foreach ($expectedCandidate in $policy.candidates)
{
    $candidate = @(
        $r1Summary.admission.candidates |
            Where-Object { $_.candidateId -eq $expectedCandidate.candidateId }
    )
    Assert-Equal $candidate.Count 1 "$($expectedCandidate.candidateId) candidate"
    Assert-Equal $candidate[0].modelFamilyId $expectedCandidate.modelFamilyId `
        "$($expectedCandidate.candidateId) model family"
    Assert-Equal $candidate[0].sourceHash $expectedCandidate.sourceHash `
        "$($expectedCandidate.candidateId) source hash"
    Assert-Equal $candidate[0].resourceHash $expectedCandidate.resourceHash `
        "$($expectedCandidate.candidateId) resource hash"
    Assert-Equal $candidate[0].admitted $true `
        "$($expectedCandidate.candidateId) admission"
}

$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$system = Get-CimInstance Win32_ComputerSystem
$os = Get-CimInstance Win32_OperatingSystem
$compiler = Get-CompilerIdentity -BuildPath $resolvedBuildDir
$generator = Get-CMakeValue -CachePath $cachePath -Name "CMAKE_GENERATOR"
$useOpenVdb = Get-CMakeValue -CachePath $cachePath -Name "USE_OPENVDB"

Assert-Equal $cpu.Name.Trim() $policy.referenceEnvironment.processorName `
    "reference processor"
Assert-Equal $cpu.NumberOfCores $policy.referenceEnvironment.physicalCoreCount `
    "reference physical cores"
Assert-Equal `
    $cpu.NumberOfLogicalProcessors `
    $policy.referenceEnvironment.logicalProcessorCount `
    "reference logical processors"
Assert-Equal $system.Manufacturer $policy.referenceEnvironment.systemManufacturer `
    "reference manufacturer"
Assert-Equal $system.Model $policy.referenceEnvironment.systemModel `
    "reference model"
Assert-True `
    ([uint64]$system.TotalPhysicalMemory -ge `
        [uint64]$policy.referenceEnvironment.minimumPhysicalMemoryBytes) `
    "物理内存低于参考预算最低要求"
Assert-Equal $generator $policy.referenceEnvironment.cmakeGenerator `
    "reference CMake generator"
Assert-Equal $compiler.id $policy.referenceEnvironment.compilerId `
    "reference compiler id"
Assert-Equal $compiler.version $policy.referenceEnvironment.compilerVersion `
    "reference compiler version"
Assert-Equal `
    $compiler.architecture `
    $policy.referenceEnvironment.compilerArchitecture `
    "reference compiler architecture"
Assert-Equal $Config $policy.referenceEnvironment.buildType "reference build type"
Assert-Equal ($useOpenVdb -eq "ON") $policy.referenceEnvironment.useOpenVdb `
    "reference OpenVDB state"

$resolvedMeasurementSummaryPath = ""
if ([string]::IsNullOrWhiteSpace($MeasurementSummaryPath))
{
    Assert-True `
        ($MeasurementCount -ge [int]$policy.measurement.minimumSamplesPerCase) `
        "MeasurementCount 少于 policy 最低要求"
    $measurementRoot = Join-Path $resolvedOutputRoot "measurements"
    $repositoryPrefix = $repoRoot.TrimEnd("\", "/") +
        [System.IO.Path]::DirectorySeparatorChar
    Assert-True `
        $measurementRoot.StartsWith(
            $repositoryPrefix,
            [System.StringComparison]::OrdinalIgnoreCase) `
        "Measurement output 必须位于仓库目录内"
    $measurementRootArgument =
        $measurementRoot.Substring($repositoryPrefix.Length)
    $runnerArguments = @{
        BuildDir = $BuildDir
        Config = $Config
        OutputRoot = $measurementRootArgument
        VoxelMm = [double]$policy.referenceEnvironment.voxelMm
        MeasurementCount = $MeasurementCount
        ReuseIntakeEvidence = $true
        SkipLegacyRegression = $true
    }
    if ($SkipBuild)
    {
        $runnerArguments.SkipBuild = $true
    }

    & (Join-Path $repoRoot "scripts/run_12e_08c_r4_07_development_gate.ps1") `
        @runnerArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "R4-07-R2 measurement 失败，退出码=$LASTEXITCODE"
    }
    $resolvedMeasurementSummaryPath =
        Join-Path $measurementRoot "four_case_development_summary.json"
}
else
{
    $resolvedMeasurementSummaryPath =
        Resolve-RepositoryPath -RepositoryRoot $repoRoot -Path $MeasurementSummaryPath
}

$measurementSummary = Read-Json $resolvedMeasurementSummaryPath
Assert-Equal `
    $measurementSummary.schema `
    "slicesoft.r4_four_case_development_gate.12e_08c_r4.1" `
    "measurement summary schema"
Assert-Equal $measurementSummary.diagnosticOnly $true `
    "measurement diagnosticOnly"
Assert-Equal $measurementSummary.productionOutputWritten $false `
    "measurement productionOutputWritten"
Assert-Equal `
    $measurementSummary.build.buildType `
    $policy.referenceEnvironment.buildType `
    "measurement build type"
Assert-Equal `
    $measurementSummary.build.useOpenVdb `
    $policy.referenceEnvironment.useOpenVdb `
    "measurement OpenVDB state"
Assert-Equal `
    $measurementSummary.build.backend `
    $policy.referenceEnvironment.backend `
    "measurement backend"
Assert-Equal `
    ([double]$measurementSummary.build.voxelMm) `
    ([double]$policy.referenceEnvironment.voxelMm) `
    "measurement voxel"

$caseResults = @()
foreach ($caseBudget in $policy.caseBudgets)
{
    $case = @(
        $measurementSummary.cases |
            Where-Object { $_.caseId -eq $caseBudget.caseId }
    )
    Assert-Equal $case.Count 1 "$($caseBudget.caseId) measurement case"
    Assert-Equal $case[0].pass $true "$($caseBudget.caseId) case result"
    Assert-True `
        ([int]$case[0].sampleCount -ge `
            [int]$policy.measurement.minimumSamplesPerCase) `
        "$($caseBudget.caseId) 样本数量不足"
    Assert-LessOrEqual `
        ([double]$case[0].globalCoreMedianMs) `
        ([double]$caseBudget.medianCoreMsMax) `
        "$($caseBudget.caseId) median core 超预算"
    Assert-LessOrEqual `
        ([double]$case[0].globalCoreMaxMs) `
        ([double]$caseBudget.singleRunCoreMsMax) `
        "$($caseBudget.caseId) single-run core 超预算"
    Assert-LessOrEqual `
        ([double]$case[0].peakWorkingSetMaxBytes) `
        ([double]$caseBudget.peakWorkingSetBytesMax) `
        "$($caseBudget.caseId) peak working set 超预算"

    $caseResults += [ordered]@{
        caseId = $case[0].caseId
        sampleCount = [int]$case[0].sampleCount
        observed = [ordered]@{
            globalCoreMedianMs = [double]$case[0].globalCoreMedianMs
            globalCoreMaxMs = [double]$case[0].globalCoreMaxMs
            peakWorkingSetMaxBytes = [uint64]$case[0].peakWorkingSetMaxBytes
        }
        ceiling = [ordered]@{
            medianCoreMsMax = [double]$caseBudget.medianCoreMsMax
            singleRunCoreMsMax = [double]$caseBudget.singleRunCoreMsMax
            peakWorkingSetBytesMax = [uint64]$caseBudget.peakWorkingSetBytesMax
        }
        pass = $true
    }
}
Assert-Equal `
    @($measurementSummary.cases).Count `
    @($policy.caseBudgets).Count `
    "measurement case count"

$gitRevision = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitDirty = @(& git -C $repoRoot status --porcelain=v1).Count -gt 0
$summary = [ordered]@{
    schema = "slicesoft.r4_restricted_candidate_budget.12e_08c_r4.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R4-07-R2"
    scope = $policy.scope
    diagnosticOnly = $true
    productionOutputWritten = $false
    environment = [ordered]@{
        identityPass = $true
        processorName = $cpu.Name.Trim()
        physicalCoreCount = [int]$cpu.NumberOfCores
        logicalProcessorCount = [int]$cpu.NumberOfLogicalProcessors
        systemManufacturer = $system.Manufacturer
        systemModel = $system.Model
        totalPhysicalMemoryBytes = [uint64]$system.TotalPhysicalMemory
        osCaption = $os.Caption
        osVersion = $os.Version
        osBuildNumber = $os.BuildNumber
        cmakeGenerator = $generator
        compiler = $compiler
        buildType = $Config
        useOpenVdb = ($useOpenVdb -eq "ON")
        backend = $measurementSummary.build.backend
        voxelMm = [double]$measurementSummary.build.voxelMm
        gitRevision = $gitRevision
        gitDirty = $gitDirty
    }
    sourceEvidence = [ordered]@{
        r1SummaryPath = $resolvedR1SummaryPath
        r1SummarySha256 = Get-Sha256 $resolvedR1SummaryPath
        policyPath = $resolvedPolicyPath
        policySha256 = Get-Sha256 $resolvedPolicyPath
        measurementSummaryPath = $resolvedMeasurementSummaryPath
        measurementSummarySha256 = Get-Sha256 $resolvedMeasurementSummaryPath
    }
    budget = [ordered]@{
        policyVersion = $policy.policyVersion
        minimumSamplesPerCase = [int]$policy.measurement.minimumSamplesPerCase
        outputWriteExcluded = $policy.measurement.outputWriteExcluded
        cases = $caseResults
        status = "frozen_pass"
    }
    remainingBlockers = @(
        "quick_ci_baseline_unresolved",
        "explicit_08d_authorization_missing"
    )
    result = [ordered]@{
        budgetGatePass = $true
        productionAdmission = "not_evaluated"
        nextTask = "Quick-CI-R1"
    }
}

Write-Utf8NoBom `
    -Path $resolvedOutputPath `
    -Content ($summary | ConvertTo-Json -Depth 100)

Write-Host "R4-07-R2 candidate budget gate: PASS"
Write-Host "Policy version: $($policy.policyVersion)"
Write-Host "Cases: $($caseResults.Count)/$($policy.caseBudgets.Count) PASS"
Write-Host "Production admission: NOT EVALUATED"
Write-Host "Summary: $resolvedOutputPath"
