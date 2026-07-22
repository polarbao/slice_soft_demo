param(
    [string]$BuildDir = "build",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/12e_08c_r4_07_development_gate",
    [double]$VoxelMm = 0.20,
    [int]$MeasurementCount = 3,
    [switch]$SkipBuild,
    [switch]$ReuseIntakeEvidence,
    [switch]$SkipLegacyRegression
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

function Resolve-Executable
{
    param(
        [string]$BuildPath,
        [string]$BuildConfig,
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
    throw "无法在 $BuildPath 下找到 $Name.exe"
}

function Get-Median
{
    param([double[]]$Values)
    $sorted = @($Values | Sort-Object)
    Assert-True ($sorted.Count -gt 0) "中位数样本不能为空"
    $middle = [int][math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1)
    {
        return [double]$sorted[$middle]
    }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

function Resolve-Width
{
    param(
        $WidthSweep,
        [string]$Selector
    )
    if ($Selector -eq "minimum")
    {
        return [double]$WidthSweep.minimumWidthMm
    }
    if ($Selector -eq "maximum")
    {
        return [double]$WidthSweep.maximumWidthMm
    }
    if ($Selector -eq "intermediate")
    {
        $samples = @($WidthSweep.samples)
        if ($samples.Count -ge 3)
        {
            return [double]$samples[1].effectiveWidthMm
        }
        return [double]$WidthSweep.minimumWidthMm
    }
    throw "未知 widthSelector：$Selector"
}

function Assert-ReleaseReport
{
    param(
        $Report,
        [string]$CaseId
    )
    Assert-Equal $Report.schema "slicesoft.texture_fill_partition.release_evidence.12e_08c.1" "$CaseId schema"
    Assert-Equal $Report.buildType "Release" "$CaseId build type"
    Assert-Equal $Report.diagnosticOnly $true "$CaseId diagnosticOnly"
    Assert-Equal $Report.productionOutputWritten $false "$CaseId productionOutputWritten"
    Assert-Equal $Report.productionAdmitted $false "$CaseId productionAdmitted"
    Assert-Equal $Report.partition.partitionPass $true "$CaseId partition"
    Assert-Equal $Report.partition.overlapTextureFillVoxels 0 "$CaseId overlap"
    Assert-Equal $Report.partition.unassignedModelVoxels 0 "$CaseId unassigned"
    Assert-Equal $Report.textureTransfer.available $true "$CaseId texture transfer"
    Assert-True ($Report.textureTransfer.sampledTextureCount -gt 0) "$CaseId 未采样到纹理"
    Assert-Equal $Report.rasterMapping.available $true "$CaseId raster mapping"
    Assert-Equal $Report.rasterMapping.partitionPass $true "$CaseId raster partition"
    Assert-Equal $Report.fullClosure.available $true "$CaseId full closure available"
    Assert-Equal $Report.fullClosure.fullClosurePass $true "$CaseId full closure"
    Assert-Equal $Report.fullClosure.totalExpectedDomainGapPixels 0 "$CaseId closure gap"
    Assert-Equal $Report.fullClosure.totalSemanticChannelMismatchPixels 0 "$CaseId semantic mismatch"
    Assert-Equal $Report.timingsMs.outputWriteMs 0 "$CaseId output time exclusion"
}

Assert-True ($MeasurementCount -ge 1) "MeasurementCount 必须大于等于 1"
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$resolvedOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputRoot))
$intakeOutputRoot = Join-Path $repoRoot "output/benchmarks/12e_08c_r4_06_repaired_asset_intake"
$expectationsPath = Join-Path $repoRoot "tests/golden/expected/12e_r4_07_development_gate_expectations.json"
$positiveRoot = Join-Path $resolvedOutputRoot "positive_matrix"
New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        repaired_asset_intake `
        repaired_asset_intake_unit_tests `
        texture_fill_partition_positive_matrix `
        texture_fill_partition_positive_matrix_unit_tests `
        texture_fill_partition_release_benchmark `
        texture_fill_partition_release_benchmark_unit_tests
    if ($LASTEXITCODE -ne 0)
    {
        throw "R4-07 development build 失败，退出码=$LASTEXITCODE"
    }
}

& ctest --test-dir $resolvedBuildDir -C $Config `
    -R "^(repaired_asset_intake_unit_tests|texture_fill_partition_positive_matrix_unit_tests|texture_fill_partition_release_benchmark_unit_tests)$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0)
{
    throw "R4-07 development 定向单测失败，退出码=$LASTEXITCODE"
}

if (-not $ReuseIntakeEvidence)
{
    & (Join-Path $repoRoot "scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1") `
        -BuildDir $BuildDir `
        -Config $Config `
        -OutputRoot "output/benchmarks/12e_08c_r4_06_repaired_asset_intake" `
        -SkipBuild
    if ($LASTEXITCODE -ne 0)
    {
        throw "R4-06 development intake 失败，退出码=$LASTEXITCODE"
    }
}

$developmentGatePath = Join-Path $intakeOutputRoot "development_gate_matrix.json"
$developmentGate = Read-Json $developmentGatePath
Assert-Equal $developmentGate.schema "slicesoft.r4_07_development_gate.12e_08c_r4.1" "development gate schema"
Assert-True $developmentGate.r4_07DevelopmentAllowed "R4-07 development gate 未放行"
Assert-True ($developmentGate.admittedDevelopmentCandidateCount -ge 1) "缺少 admitted development candidate"
Assert-Equal $developmentGate.finalRequiredFamilyGatePass $false "final required family gate"

& (Join-Path $repoRoot "scripts/run_12e_08c_r4_05_clean_positive_matrix.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -OutputRoot $positiveRoot `
    -VoxelMm $VoxelMm
if ($LASTEXITCODE -ne 0)
{
    throw "R4-07 width 正向矩阵失败，退出码=$LASTEXITCODE"
}

$expectations = Read-Json $expectationsPath
Assert-Equal $expectations.schema "slicesoft.r4_07_development_gate_expectations.12e_08c_r4.1" "expectations schema"
Assert-True (
    $developmentGate.admittedDevelopmentCandidateCount `
        -ge $expectations.minimumAdmittedDevelopmentCandidates) `
    "admitted development candidate 数量不足"

$benchmark = Resolve-Executable `
    -BuildPath $resolvedBuildDir `
    -BuildConfig $Config `
    -Name "texture_fill_partition_release_benchmark"
$caseResults = @()
foreach ($case in $expectations.cases)
{
    $positiveReportPath = Join-Path $positiveRoot "$($case.positiveCaseId).json"
    $positiveReport = Read-Json $positiveReportPath
    Assert-True $positiveReport.summary.matrixPass "$($case.caseId) positive matrix 未通过"
    $widthMm = Resolve-Width $positiveReport.widthSweep $case.widthSelector

    if (-not [string]::IsNullOrWhiteSpace($case.intakeCandidateId))
    {
        $intakeCase = @(
            $developmentGate.cases |
                Where-Object { $_.candidateId -eq $case.intakeCandidateId }
        )
        Assert-Equal $intakeCase.Count 1 "$($case.caseId) intake identity"
        Assert-True $intakeCase[0].admitted "$($case.caseId) intake 未准入"
        Assert-Equal $intakeCase[0].sourceHash $positiveReport.input.sourceHash "$($case.caseId) source hash"
        Assert-Equal $intakeCase[0].resourceHash $positiveReport.input.resourceHash "$($case.caseId) resource hash"
    }

    $caseRoot = Join-Path $resolvedOutputRoot $case.caseId
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
    $configPath = Join-Path $repoRoot $case.config
    $warmupPath = Join-Path $caseRoot "warmup.json"
    & $benchmark `
        --config $configPath `
        --output $warmupPath `
        --case-name "$($case.caseId)_warmup" `
        --voxel-mm $VoxelMm `
        --width-mm $widthMm `
        --padding-voxels 1
    if ($LASTEXITCODE -ne 0)
    {
        throw "$($case.caseId) warm-up 失败，退出码=$LASTEXITCODE"
    }
    Assert-ReleaseReport (Read-Json $warmupPath) "$($case.caseId) warm-up"

    $measurements = @()
    for ($index = 1; $index -le $MeasurementCount; ++$index)
    {
        $reportPath = Join-Path $caseRoot ("measurement_{0}.json" -f $index)
        & $benchmark `
            --config $configPath `
            --output $reportPath `
            --case-name $case.caseId `
            --voxel-mm $VoxelMm `
            --width-mm $widthMm `
            --padding-voxels 1
        if ($LASTEXITCODE -ne 0)
        {
            throw "$($case.caseId) measurement $index 失败，退出码=$LASTEXITCODE"
        }
        $report = Read-Json $reportPath
        Assert-ReleaseReport $report "$($case.caseId) measurement $index"
        $measurements += [ordered]@{
            index = $index
            globalCoreMs = [double]$report.timingsMs.globalCoreMs
            partitionCoreMs = [double]$report.timingsMs.totalCoreMs
            textureTransferMs = [double]$report.timingsMs.textureTransferMs
            rasterMappingMs = [double]$report.timingsMs.rasterMappingMs
            fullClosureMs = [double]$report.timingsMs.fullClosureMs
            peakWorkingSetBytes = [uint64]$report.memory.peakWorkingSetBytes
            reportPath = $reportPath
        }
    }

    $globalTimes = [double[]]@($measurements | ForEach-Object { $_.globalCoreMs })
    $peakValues = [uint64[]]@($measurements | ForEach-Object { $_.peakWorkingSetBytes })
    $caseResults += [ordered]@{
        caseId = $case.caseId
        intakeCandidateId = $case.intakeCandidateId
        modelPath = $positiveReport.input.modelPath
        sourceHash = $positiveReport.input.sourceHash
        resourceHash = $positiveReport.input.resourceHash
        widthSelector = $case.widthSelector
        widthMm = $widthMm
        sampleCount = $MeasurementCount
        globalCoreMedianMs = Get-Median $globalTimes
        globalCoreMaxMs = ($globalTimes | Measure-Object -Maximum).Maximum
        peakWorkingSetMaxBytes = ($peakValues | Measure-Object -Maximum).Maximum
        measurements = $measurements
        pass = $true
    }
}

$legacyStatus = "skipped"
if (-not $SkipLegacyRegression)
{
    & (Join-Path $repoRoot "scripts/run_material_closure_tests.ps1") `
        -BuildDir $BuildDir `
        -Config $Config `
        -Mode RepairDisabled
    if ($LASTEXITCODE -ne 0)
    {
        throw "R4-07 legacy TIFF/RIP 回归失败，退出码=$LASTEXITCODE"
    }
    $legacyStatus = "passed"
}

$summary = [ordered]@{
    schema = "slicesoft.r4_four_case_development_gate.12e_08c_r4.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R4-07-development"
    diagnosticOnly = $true
    productionOutputWritten = $false
    developmentGate = [ordered]@{
        admittedCandidateCount = $developmentGate.admittedDevelopmentCandidateCount
        r4_07DevelopmentAllowed = $true
        fourCaseDevelopmentPass = @($caseResults | Where-Object { $_.pass }).Count -eq 4
    }
    finalGate = [ordered]@{
        requiredFamilyMatrix = "0/3"
        requiredFamilyPass = $false
        releaseBudgetFrozen = $false
        productionAdmission = "blocked"
    }
    build = [ordered]@{
        buildType = $Config
        buildDir = $BuildDir.Replace("\", "/")
        useOpenVdb = $false
        backend = "legacy_cpu_global_distance"
        voxelMm = $VoxelMm
    }
    cases = $caseResults
    legacyRegression = $legacyStatus
}
Assert-True $summary.developmentGate.fourCaseDevelopmentPass "R4-07 四 case development matrix 未通过"
$summaryPath = Join-Path $resolvedOutputRoot "four_case_development_summary.json"
Write-Utf8NoBom -Path $summaryPath -Content ($summary | ConvertTo-Json -Depth 100)

Write-Host "R4-07 development four-case gate: PASS"
Write-Host "Final required family gate: 0/3 BLOCKED"
Write-Host "Summary: $summaryPath"
