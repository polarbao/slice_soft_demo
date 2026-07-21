param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [UInt64]$MaxCandidatePairs = 5000000,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Output = "output/benchmarks/12e_08c_r3_01a_self_intersection/self_intersection_summary.json"
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message)
{
  if (-not $Condition)
  {
    throw $Message
  }
}

function Assert-Equal($Actual, $Expected, [string]$Message)
{
  if ($Actual -ne $Expected)
  {
    throw "$Message expected=$Expected actual=$Actual"
  }
}

function Read-Json([string]$Path)
{
  Assert-True (Test-Path -LiteralPath $Path) "missing JSON file: $Path"
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Write-Utf8NoBom([string]$Path, [string]$Content)
{
  $directory = Split-Path -Parent $Path
  if (-not [string]::IsNullOrWhiteSpace($directory))
  {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
  }
  [System.IO.File]::WriteAllText(
    $Path,
    $Content,
    [System.Text.UTF8Encoding]::new($false))
}

function Resolve-Executable([string]$BuildRoot, [string]$BuildConfig, [string]$Name)
{
  foreach ($candidate in @(
      (Join-Path $BuildRoot "$BuildConfig/$Name.exe"),
      (Join-Path $BuildRoot "$Name.exe")))
  {
    if (Test-Path -LiteralPath $candidate)
    {
      return (Resolve-Path -LiteralPath $candidate).Path
    }
  }
  throw "missing executable $Name under build directory: $BuildRoot"
}

function Get-StableAnalysis($Analysis)
{
  return [pscustomobject][ordered]@{
    status = $Analysis.status
    complete = $Analysis.complete
    triangleCount = $Analysis.triangleCount
    bvhNodeCount = $Analysis.bvhNodeCount
    candidatePairCount = $Analysis.candidatePairCount
    testedPairCount = $Analysis.testedPairCount
    confirmedIntersectionPairs = $Analysis.confirmedIntersectionPairs
    coplanarOverlapPairs = $Analysis.coplanarOverlapPairs
    touchingOnlyPairs = $Analysis.touchingOnlyPairs
    aabbOnlyPairs = $Analysis.aabbOnlyPairs
    candidatePairHash = $Analysis.candidatePairHash
    blockerCode = $Analysis.blockerCode
    issues = @($Analysis.issues)
  }
}

function Get-StableProjection($Report)
{
  return [pscustomobject][ordered]@{
    schema = $Report.schema
    status = $Report.status
    mode = $Report.mode
    repairEnabled = $Report.repairEnabled
    repairAttempted = $Report.repairAttempted
    productionOutputWritten = $Report.productionOutputWritten
    input = $Report.input
    options = $Report.options
    hashes = $Report.hashes
    preRepair = $Report.preRepair
    eligibility = $Report.eligibility
    completeSelfIntersectionAnalysis = Get-StableAnalysis $Report.completeSelfIntersectionAnalysis
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-CompleteAudit(
  [string]$Executable,
  [string]$ConfigPath,
  [string]$ReportPath,
  [string]$SourceId)
{
  $outputLines = @(
    & $Executable `
      --config $ConfigPath `
      --output $ReportPath `
      --source-id $SourceId `
      --voxel-mm $VoxelMm `
      --require-openvdb-off `
      --analyze-r3-01a `
      --complete-self-intersection-max-candidates $MaxCandidatePairs 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R3-01A failed: source=$SourceId exitCode=$exitCode"
  }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$executable = Resolve-Executable $resolvedBuildDir $Config "mesh_repair_preflight"
$baselinePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BaselineSummary))
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Output))

if (-not (Test-Path -LiteralPath $baselinePath))
{
  & (Join-Path $PSScriptRoot "run_12e_08c_r1_pre_repair_baseline.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -VoxelMm $VoxelMm `
    -Output $BaselineSummary
}

$baseline = Read-Json $baselinePath
Assert-Equal $baseline.schema "slicesoft.mesh_repair_real_model_baseline.12e_08c_r1.1" "baseline schema"
Assert-Equal @($baseline.cases).Count 4 "required baseline case count"
$caseResults = @()

Push-Location $repoRoot
try
{
  foreach ($case in $baseline.cases)
  {
    $caseId = $case.caseId
    Write-Host "== 12E-08C-R3-01A complete self-intersection case: $caseId"
    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"
    $firstReportPath = Join-Path $caseRoot "complete_self_intersection_run_1.json"
    $secondReportPath = Join-Path $caseRoot "complete_self_intersection_run_2.json"
    Invoke-CompleteAudit $executable $configPath $firstReportPath $sourceAsset.path
    Invoke-CompleteAudit $executable $configPath $secondReportPath $sourceAsset.path

    $first = Read-Json $firstReportPath
    $second = Read-Json $secondReportPath
    $analysis = $first.completeSelfIntersectionAnalysis
    Assert-Equal $first.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId report schema"
    Assert-Equal $first.options.analyzeCompleteSelfIntersections $true "$caseId complete audit option"
    Assert-Equal $first.options.maxCompleteSelfIntersectionCandidatePairs $MaxCandidatePairs "$caseId candidate budget"
    Assert-Equal $first.productionOutputWritten $false "$caseId production output"
    Assert-Equal $first.repairAttempted $false "$caseId repair attempt"
    Assert-True ($analysis.status -in @(
        "complete_no_intersection",
        "confirmed_intersection",
        "coplanar_overlap",
        "touching_only",
        "budget_or_resource_blocked")) "$caseId analysis status"
    Assert-True (-not (@($first.eligibility.decisions).issueCode -contains "MESH_SELF_INTERSECTION_SAMPLED")) "$caseId sampled decision removed"

    if ($analysis.complete)
    {
      Assert-Equal $analysis.testedPairCount $analysis.candidatePairCount "$caseId complete pair coverage"
      Assert-True (-not [string]::IsNullOrWhiteSpace($analysis.candidatePairHash)) "$caseId pair hash"
      Assert-Equal $analysis.candidatePairHash.Length 64 "$caseId pair hash length"
      Assert-True ([string]::IsNullOrWhiteSpace($analysis.blockerCode)) "$caseId no blocker"
      if (($analysis.confirmedIntersectionPairs + $analysis.coplanarOverlapPairs) -gt 0)
      {
        Assert-Equal $first.status "rejected_self_intersection" "$caseId confirmed fail-fast"
      }
    }
    else
    {
      Assert-Equal $analysis.status "budget_or_resource_blocked" "$caseId blocked status"
      Assert-True (-not [string]::IsNullOrWhiteSpace($analysis.blockerCode)) "$caseId blocker code"
      Assert-True ([string]::IsNullOrWhiteSpace($analysis.candidatePairHash)) "$caseId partial hash absent"
      Assert-True (@($first.eligibility.decisions).issueCode -contains "MESH_SELF_INTERSECTION_BUDGET_BLOCKED") "$caseId budget eligibility"
    }

    $firstStable = Get-StableProjection $first
    $secondStable = Get-StableProjection $second
    $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
    $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
    Assert-Equal $firstStableJson $secondStableJson "$caseId complete audit repeatability"
    $stablePath = Join-Path $caseRoot "complete_self_intersection_stable.json"
    Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      status = $first.status
      analysisStatus = $analysis.status
      complete = $analysis.complete
      triangleCount = $analysis.triangleCount
      bvhNodeCount = $analysis.bvhNodeCount
      candidatePairCount = $analysis.candidatePairCount
      testedPairCount = $analysis.testedPairCount
      confirmedIntersectionPairs = $analysis.confirmedIntersectionPairs
      coplanarOverlapPairs = $analysis.coplanarOverlapPairs
      touchingOnlyPairs = $analysis.touchingOnlyPairs
      aabbOnlyPairs = $analysis.aabbOnlyPairs
      candidatePairHash = $analysis.candidatePairHash
      blockerCode = $analysis.blockerCode
      durationMsRun1 = $analysis.durationMs
      peakWorkingSetBytesRun1 = $analysis.peakWorkingSetBytes
      stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
      repeatability = "passed"
      repairAttempted = $first.repairAttempted
      productionOutputWritten = $first.productionOutputWritten
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_complete_self_intersection.12e_08c_r3_01a.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R3-01A"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      analyzerOnly = $true
      deterministicBroadPhase = "aabb_bvh"
      maxCandidatePairs = $MaxCandidatePairs
      repairAttempted = $false
      productionOutputWritten = $false
      repeatRunsPerCase = 2
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      completeCases = @($caseResults | Where-Object { $_.complete }).Count
      budgetBlockedCases = @($caseResults | Where-Object { -not $_.complete }).Count
      confirmedIntersectionCases = @($caseResults | Where-Object {
          ($_.confirmedIntersectionPairs + $_.coplanarOverlapPairs) -gt 0
        }).Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      nextTask = "12E-08C-R3-02"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R3-01A complete self-intersection evidence complete: $Output"
}
finally
{
  Pop-Location
}
