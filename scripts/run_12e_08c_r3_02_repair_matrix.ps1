param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [UInt64]$MaxCandidatePairs = 5000000,
  [double]$WeldToleranceMm = 0.0001,
  [int]$MaxBoundaryLoopEdges = 64,
  [double]$MaxBoundaryLoopDiameterMm = 2.0,
  [double]$MaxBoundaryLoopPerimeterMm = 8.0,
  [double]$MaxBoundaryPlanarityErrorMm = 0.01,
  [double]$MaxHoleAreaMm2 = 4.0,
  [double]$MaxAffectedFaceRatio = 0.10,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$IntersectionSummaryPath = "output/benchmarks/12e_08c_r3_01a_self_intersection/self_intersection_summary.json",
  [string]$Expectations = "tests/golden/expected/12e_mesh_repair_matrix_expectations.json",
  [string]$Output = "output/benchmarks/12e_08c_r3_02_repair_matrix/repair_matrix_summary.json"
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

function Get-StableCompleteAnalysis($Analysis)
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
    operations = @($Report.operations)
    sourceMappings = @($Report.sourceMappings)
    vertexMappings = @($Report.vertexMappings)
    generatedTriangleMappings = @($Report.generatedTriangleMappings)
    attributePreservation = $Report.attributePreservation
    evidenceValidation = $Report.evidenceValidation
    nonManifoldAnalysis = $Report.nonManifoldAnalysis
    completeSelfIntersectionAnalysis = Get-StableCompleteAnalysis $Report.completeSelfIntersectionAnalysis
    postRepair = $Report.postRepair
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-MatrixLane(
  [string]$Executable,
  [string]$Lane,
  [string]$ConfigPath,
  [string]$ReportPath,
  [string]$SourceId)
{
  $arguments = @(
    "--config", $ConfigPath,
    "--output", $ReportPath,
    "--source-id", $SourceId,
    "--voxel-mm", $VoxelMm,
    "--require-openvdb-off",
    "--complete-self-intersection-max-candidates", $MaxCandidatePairs)
  if ($Lane -eq "strict_no_repair")
  {
    $arguments += "--analyze-r3-01a"
  }
  elseif ($Lane -eq "conservative_repair")
  {
    $arguments += @(
      "--execute-r3-02",
      "--weld-tolerance-mm", $WeldToleranceMm,
      "--max-boundary-loop-edges", $MaxBoundaryLoopEdges,
      "--max-boundary-loop-diameter-mm", $MaxBoundaryLoopDiameterMm,
      "--max-boundary-loop-perimeter-mm", $MaxBoundaryLoopPerimeterMm,
      "--max-boundary-planarity-error-mm", $MaxBoundaryPlanarityErrorMm,
      "--max-hole-area-mm2", $MaxHoleAreaMm2,
      "--max-affected-face-ratio", $MaxAffectedFaceRatio)
  }
  else
  {
    throw "unsupported matrix lane: $Lane"
  }

  $outputLines = @(
    & $Executable @arguments 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R3-02 failed: lane=$Lane source=$SourceId exitCode=$exitCode"
  }
}

function Assert-CompleteAnalysis(
  $Analysis,
  $IntersectionBaseline,
  $Expectation,
  [string]$MessagePrefix)
{
  Assert-Equal $Analysis.complete $true "$MessagePrefix complete analysis"
  Assert-Equal $Analysis.status $Expectation.analysisStatus "$MessagePrefix analysis status"
  Assert-Equal $Analysis.testedPairCount $Analysis.candidatePairCount "$MessagePrefix tested pair coverage"
  Assert-Equal $Analysis.candidatePairCount $IntersectionBaseline.candidatePairCount "$MessagePrefix candidate count"
  Assert-Equal $Analysis.confirmedIntersectionPairs $IntersectionBaseline.confirmedIntersectionPairs "$MessagePrefix confirmed count"
  Assert-Equal $Analysis.coplanarOverlapPairs $IntersectionBaseline.coplanarOverlapPairs "$MessagePrefix coplanar count"
  Assert-Equal $Analysis.touchingOnlyPairs $IntersectionBaseline.touchingOnlyPairs "$MessagePrefix touching count"
  Assert-Equal $Analysis.candidatePairHash $IntersectionBaseline.candidatePairHash "$MessagePrefix pair hash"
  Assert-True ([string]::IsNullOrWhiteSpace($Analysis.blockerCode)) "$MessagePrefix blocker must be empty"
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$executable = Resolve-Executable $resolvedBuildDir $Config "mesh_repair_preflight"
$baselinePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BaselineSummary))
$resolvedIntersectionPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $IntersectionSummaryPath))
$expectationsPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Expectations))
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Output))

if (-not (Test-Path -LiteralPath $baselinePath))
{
  & (Join-Path $PSScriptRoot "run_12e_08c_r1_pre_repair_baseline.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -VoxelMm $VoxelMm `
    -Output $BaselineSummary
}
if (-not (Test-Path -LiteralPath $resolvedIntersectionPath))
{
  & (Join-Path $PSScriptRoot "run_12e_08c_r3_01a_complete_self_intersection.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -VoxelMm $VoxelMm `
    -MaxCandidatePairs $MaxCandidatePairs `
    -BaselineSummary $BaselineSummary `
    -Output $IntersectionSummaryPath
}

$baseline = Read-Json $baselinePath
$intersectionDocument = Read-Json $resolvedIntersectionPath
$expectationsDocument = Read-Json $expectationsPath
Assert-Equal $baseline.schema "slicesoft.mesh_repair_real_model_baseline.12e_08c_r1.1" "baseline schema"
Assert-Equal $intersectionDocument.schema "slicesoft.mesh_complete_self_intersection.12e_08c_r3_01a.1" "intersection schema"
Assert-Equal $expectationsDocument.schema "slicesoft.mesh_repair_matrix_expectations.12e_08c_r3_02.1" "expectation schema"
Assert-Equal @($baseline.cases).Count 4 "required baseline case count"
Assert-Equal @($intersectionDocument.cases).Count 4 "required intersection case count"
Assert-Equal @($expectationsDocument.cases).Count 4 "required expectation case count"
$caseResults = @()

Push-Location $repoRoot
try
{
  foreach ($case in $baseline.cases)
  {
    $caseId = $case.caseId
    $intersectionBaseline = @($intersectionDocument.cases) |
      Where-Object { $_.caseId -eq $caseId } |
      Select-Object -First 1
    $expectation = @($expectationsDocument.cases) |
      Where-Object { $_.caseId -eq $caseId } |
      Select-Object -First 1
    Assert-True ($null -ne $intersectionBaseline) "$caseId intersection baseline"
    Assert-True ($null -ne $expectation) "$caseId matrix expectation"
    Write-Host "== 12E-08C-R3-02 matrix case: $caseId"

    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"

    $strictRun1Path = Join-Path $caseRoot "strict_no_repair_run_1.json"
    $strictRun2Path = Join-Path $caseRoot "strict_no_repair_run_2.json"
    $conservativeRun1Path = Join-Path $caseRoot "conservative_repair_run_1.json"
    $conservativeRun2Path = Join-Path $caseRoot "conservative_repair_run_2.json"
    Invoke-MatrixLane $executable "strict_no_repair" $configPath $strictRun1Path $sourceAsset.path
    Invoke-MatrixLane $executable "strict_no_repair" $configPath $strictRun2Path $sourceAsset.path
    Invoke-MatrixLane $executable "conservative_repair" $configPath $conservativeRun1Path $sourceAsset.path
    Invoke-MatrixLane $executable "conservative_repair" $configPath $conservativeRun2Path $sourceAsset.path

    $strictRun1 = Read-Json $strictRun1Path
    $strictRun2 = Read-Json $strictRun2Path
    $conservativeRun1 = Read-Json $conservativeRun1Path
    $conservativeRun2 = Read-Json $conservativeRun2Path
    Assert-Equal $strictRun1.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId strict schema"
    Assert-Equal $conservativeRun1.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId conservative schema"
    Assert-Equal $strictRun1.status $expectation.strictStatus "$caseId strict status"
    Assert-Equal $conservativeRun1.status $expectation.conservativeStatus "$caseId conservative status"
    Assert-Equal $strictRun1.mode "strict_closed" "$caseId strict mode"
    Assert-Equal $conservativeRun1.mode "repair_then_strict" "$caseId conservative mode"
    Assert-Equal $strictRun1.repairEnabled $false "$caseId strict repair disabled"
    Assert-Equal $conservativeRun1.repairEnabled $true "$caseId conservative repair enabled"
    Assert-Equal $strictRun1.productionOutputWritten $false "$caseId strict production output"
    Assert-Equal $conservativeRun1.productionOutputWritten $false "$caseId conservative production output"
    Assert-Equal $strictRun1.admission.productionAllowed $false "$caseId strict production admission"
    Assert-Equal $conservativeRun1.admission.productionAllowed $false "$caseId conservative production admission"
    Assert-Equal $conservativeRun1.options.analyzeCompleteSelfIntersections $true "$caseId conservative complete analysis option"
    Assert-Equal $conservativeRun1.options.classifyNonManifoldPatterns $true "$caseId conservative classifier option"
    Assert-Equal $conservativeRun1.options.validatePostRepairEvidence $true "$caseId conservative evidence validator option"
    Assert-Equal $conservativeRun1.options.allowVertexWeld $true "$caseId conservative weld option"
    Assert-Equal $conservativeRun1.options.allowWindingRepair $true "$caseId conservative winding option"
    Assert-Equal $conservativeRun1.options.allowBoundaryFill $true "$caseId conservative boundary option"
    Assert-Equal $conservativeRun1.options.allowNewFaces $true "$caseId conservative new-face option"
    Assert-Equal $conservativeRun1.options.maxCompleteSelfIntersectionCandidatePairs $MaxCandidatePairs "$caseId conservative candidate budget"

    Assert-CompleteAnalysis $strictRun1.completeSelfIntersectionAnalysis $intersectionBaseline $expectation "$caseId strict"
    Assert-CompleteAnalysis $conservativeRun1.completeSelfIntersectionAnalysis $intersectionBaseline $expectation "$caseId conservative"
    Assert-Equal $strictRun1.hashes.preRepairGeometryHash $conservativeRun1.hashes.preRepairGeometryHash "$caseId pre geometry hash"
    Assert-Equal $strictRun1.hashes.preRepairAttributeHash $conservativeRun1.hashes.preRepairAttributeHash "$caseId pre attribute hash"
    Assert-Equal $conservativeRun1.nonManifoldAnalysis.status $expectation.nonManifoldStatus "$caseId non-manifold status"
    Assert-Equal $conservativeRun1.nonManifoldAnalysis.nonManifoldEdgeCount $expectation.nonManifoldEdgeCount "$caseId non-manifold edge count"
    Assert-Equal $conservativeRun1.nonManifoldAnalysis.complete $true "$caseId non-manifold classification complete"
    Assert-Equal $conservativeRun1.repairAttempted $false "$caseId no mutation"
    Assert-Equal @($conservativeRun1.operations).Count 0 "$caseId no operation"

    if ($expectation.productionGateStatus -eq "blocked_confirmed_self_intersection")
    {
      Assert-Equal $conservativeRun1.evidenceValidation.status "not_evaluated" "$caseId fail-fast validator status"
      Assert-Equal $conservativeRun1.evidenceValidation.candidateAccepted $false "$caseId rejected candidate"
      Assert-Equal $conservativeRun1.postRepair.available $false "$caseId no post-repair candidate"
    }
    else
    {
      Assert-Equal $conservativeRun1.evidenceValidation.status "passed" "$caseId validator status"
      Assert-Equal $conservativeRun1.evidenceValidation.candidateAccepted $true "$caseId candidate acceptance"
      Assert-Equal $conservativeRun1.evidenceValidation.postStrictComplete $true "$caseId post-strict complete"
      Assert-Equal $conservativeRun1.evidenceValidation.postStrictPass $true "$caseId post-strict pass"
      Assert-Equal $conservativeRun1.attributePreservation.pass $true "$caseId attribute preservation"
      Assert-Equal $conservativeRun1.hashes.preRepairGeometryHash $conservativeRun1.hashes.postRepairGeometryHash "$caseId no-op geometry hash"
      Assert-Equal $conservativeRun1.hashes.preRepairAttributeHash $conservativeRun1.hashes.postRepairAttributeHash "$caseId no-op attribute hash"
    }

    $strictStable = Get-StableProjection $strictRun1
    $strictStableRepeat = Get-StableProjection $strictRun2
    $conservativeStable = Get-StableProjection $conservativeRun1
    $conservativeStableRepeat = Get-StableProjection $conservativeRun2
    Assert-Equal `
      ($strictStable | ConvertTo-Json -Depth 100 -Compress) `
      ($strictStableRepeat | ConvertTo-Json -Depth 100 -Compress) `
      "$caseId strict repeatability"
    Assert-Equal `
      ($conservativeStable | ConvertTo-Json -Depth 100 -Compress) `
      ($conservativeStableRepeat | ConvertTo-Json -Depth 100 -Compress) `
      "$caseId conservative repeatability"
    $strictStablePath = Join-Path $caseRoot "strict_no_repair_stable.json"
    $conservativeStablePath = Join-Path $caseRoot "conservative_repair_stable.json"
    Write-Utf8NoBom $strictStablePath ($strictStable | ConvertTo-Json -Depth 100)
    Write-Utf8NoBom $conservativeStablePath ($conservativeStable | ConvertTo-Json -Depth 100)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      strictStatus = $strictRun1.status
      conservativeStatus = $conservativeRun1.status
      analysisStatus = $conservativeRun1.completeSelfIntersectionAnalysis.status
      candidatePairCount = $conservativeRun1.completeSelfIntersectionAnalysis.candidatePairCount
      confirmedIntersectionPairs = $conservativeRun1.completeSelfIntersectionAnalysis.confirmedIntersectionPairs
      coplanarOverlapPairs = $conservativeRun1.completeSelfIntersectionAnalysis.coplanarOverlapPairs
      touchingOnlyPairs = $conservativeRun1.completeSelfIntersectionAnalysis.touchingOnlyPairs
      candidatePairHash = $conservativeRun1.completeSelfIntersectionAnalysis.candidatePairHash
      nonManifoldStatus = $conservativeRun1.nonManifoldAnalysis.status
      nonManifoldEdgeCount = $conservativeRun1.nonManifoldAnalysis.nonManifoldEdgeCount
      allUniqueFanSplitsFeasible = $conservativeRun1.nonManifoldAnalysis.allUniqueFanSplitsFeasible
      repairAttempted = $conservativeRun1.repairAttempted
      operationCount = @($conservativeRun1.operations).Count
      evidenceValidationStatus = $conservativeRun1.evidenceValidation.status
      candidateAccepted = $conservativeRun1.evidenceValidation.candidateAccepted
      attributePreservationPass = $conservativeRun1.attributePreservation.pass
      taskEvidenceStatus = $expectation.taskEvidenceStatus
      productionGateStatus = $expectation.productionGateStatus
      strictStableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $strictStablePath).Hash.ToLowerInvariant()
      conservativeStableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $conservativeStablePath).Hash.ToLowerInvariant()
      strictRepeatability = "passed"
      conservativeRepeatability = "passed"
      productionOutputWritten = $false
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_matrix.12e_08c_r3_02.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R3-02"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      lanes = @("strict_no_repair", "conservative_repair")
      completeSelfIntersectionRequired = $true
      conservativeOperations = @(
        "remove_degenerate_triangle",
        "remove_exact_duplicate_face",
        "weld_vertex",
        "flip_triangle_winding",
        "fill_boundary_loop")
      confirmedIntersectionFailFast = $true
      repeatRunsPerLane = 2
      productionOutputWritten = $false
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      taskEvidenceCompleteCases = @($caseResults | Where-Object {
          $_.taskEvidenceStatus -like "complete_*"
        }).Count
      confirmedIntersectionCases = @($caseResults | Where-Object {
          $_.productionGateStatus -eq "blocked_confirmed_self_intersection"
        }).Count
      noOpStrictPassCases = @($caseResults | Where-Object {
          $_.taskEvidenceStatus -eq "complete_no_op_pass"
        }).Count
      productionGatePassedCases = 0
      nextTask = "12E-08C-R3-03"
      nextTaskReadiness = "ready_non_production_release_evidence"
      productionAdmission = "blocked"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 100)
  Write-Host "12E-08C-R3-02 repair matrix complete: $Output"
}
finally
{
  Pop-Location
}
