param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [double]$WeldToleranceMm = 0.0001,
  [int]$MaxBoundaryLoopEdges = 64,
  [double]$MaxBoundaryLoopDiameterMm = 2.0,
  [double]$MaxBoundaryLoopPerimeterMm = 8.0,
  [double]$MaxBoundaryPlanarityErrorMm = 0.01,
  [double]$MaxHoleAreaMm2 = 4.0,
  [double]$MaxAffectedFaceRatio = 0.10,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Output = "output/benchmarks/12e_08c_r2_03_boundary/boundary_summary.json"
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
    postRepair = $Report.postRepair
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-R2Boundary(
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
      --execute-r2-03 `
      --weld-tolerance-mm $WeldToleranceMm `
      --max-boundary-loop-edges $MaxBoundaryLoopEdges `
      --max-boundary-loop-diameter-mm $MaxBoundaryLoopDiameterMm `
      --max-boundary-loop-perimeter-mm $MaxBoundaryLoopPerimeterMm `
      --max-boundary-planarity-error-mm $MaxBoundaryPlanarityErrorMm `
      --max-hole-area-mm2 $MaxHoleAreaMm2 `
      --max-affected-face-ratio $MaxAffectedFaceRatio 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R2-03 failed: source=$SourceId exitCode=$exitCode"
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
$allowedOperations = @(
  "remove_degenerate_triangle",
  "remove_exact_duplicate_face",
  "weld_vertex",
  "flip_triangle_winding",
  "fill_boundary_loop")
$caseResults = @()

Push-Location $repoRoot
try
{
  foreach ($case in $baseline.cases)
  {
    $caseId = $case.caseId
    Write-Host "== 12E-08C-R2-03 boundary case: $caseId"
    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"
    $firstReportPath = Join-Path $caseRoot "mesh_repair_boundary_run_1.json"
    $secondReportPath = Join-Path $caseRoot "mesh_repair_boundary_run_2.json"
    Invoke-R2Boundary $executable $configPath $firstReportPath $sourceAsset.path
    Invoke-R2Boundary $executable $configPath $secondReportPath $sourceAsset.path

    $first = Read-Json $firstReportPath
    $second = Read-Json $secondReportPath
    Assert-Equal $first.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId report schema"
    Assert-Equal $first.mode "repair_then_strict" "$caseId mode"
    Assert-Equal $first.options.allowBoundaryFill $true "$caseId boundary option"
    Assert-Equal $first.options.allowNewFaces $true "$caseId new-face option"
    Assert-Equal $first.options.newFaceAttributePolicy "inherit_uniform_material_no_uv" "$caseId attribute policy"
    Assert-Equal $first.productionOutputWritten $false "$caseId production output"
    Assert-Equal $first.postRepair.connectedComponents $first.input.componentCount "$caseId component guard"
    foreach ($operation in $first.operations)
    {
      Assert-True ($operation.type -in $allowedOperations) "$caseId unexpected operation: $($operation.type)"
    }
    foreach ($mapping in $first.generatedTriangleMappings)
    {
      Assert-Equal $mapping.attributePolicy "inherit_uniform_material_no_uv" "$caseId generated policy"
      Assert-Equal $mapping.hasUv $false "$caseId generated UV guard"
      Assert-Equal @($mapping.generatingBoundaryVertexIndices).Count 3 "$caseId generated provenance"
    }
    Assert-Equal `
      @($first.generatedTriangleMappings).Count `
      $first.attributePreservation.newTriangles `
      "$caseId generated face count"

    $firstStable = Get-StableProjection $first
    $secondStable = Get-StableProjection $second
    $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
    $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
    Assert-Equal $firstStableJson $secondStableJson "$caseId boundary repeatability"
    $stablePath = Join-Path $caseRoot "mesh_repair_boundary_stable.json"
    Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      status = $first.status
      operationCount = @($first.operations).Count
      boundaryFillOperations = @($first.operations | Where-Object { $_.type -eq "fill_boundary_loop" }).Count
      generatedTriangles = @($first.generatedTriangleMappings).Count
      inputComponents = $first.input.componentCount
      postComponents = $first.postRepair.connectedComponents
      preBoundaryEdges = $first.preRepair.boundaryEdges
      postBoundaryEdges = $first.postRepair.boundaryEdges
      attributeStatus = $first.attributePreservation.status
      operationHash = $first.hashes.repairOperationHash
      postGeometryHash = $first.hashes.postRepairGeometryHash
      postAttributeHash = $first.hashes.postRepairAttributeHash
      stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
      repeatability = "passed"
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_boundary_evidence.12e_08c_r2_03.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R2-03"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      weldToleranceMm = $WeldToleranceMm
      maxBoundaryLoopEdges = $MaxBoundaryLoopEdges
      maxBoundaryLoopDiameterMm = $MaxBoundaryLoopDiameterMm
      maxBoundaryLoopPerimeterMm = $MaxBoundaryLoopPerimeterMm
      maxBoundaryPlanarityErrorMm = $MaxBoundaryPlanarityErrorMm
      maxHoleAreaMm2 = $MaxHoleAreaMm2
      maxAffectedFaceRatio = $MaxAffectedFaceRatio
      attributePolicy = "inherit_uniform_material_no_uv"
      productionOutputWritten = $false
      repeatRunsPerCase = 2
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      nextTask = "12E-08C-R2-04"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R2-03 boundary evidence complete: $Output"
}
finally
{
  Pop-Location
}
