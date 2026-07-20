param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [double]$WeldToleranceMm = 0.0001,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Output = "output/benchmarks/12e_08c_r2_02_topology/topology_summary.json"
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
    attributePreservation = $Report.attributePreservation
    postRepair = $Report.postRepair
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-R2Topology(
  [string]$Executable,
  [string]$ConfigPath,
  [string]$ReportPath,
  [string]$SourceId,
  [double]$CaseVoxelMm,
  [double]$CaseWeldToleranceMm)
{
  $outputLines = @(
    & $Executable `
      --config $ConfigPath `
      --output $ReportPath `
      --source-id $SourceId `
      --voxel-mm $CaseVoxelMm `
      --require-openvdb-off `
      --execute-r2-02 `
      --weld-tolerance-mm $CaseWeldToleranceMm 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R2-02 failed: source=$SourceId exitCode=$exitCode"
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
  "flip_triangle_winding")
$caseResults = @()

Push-Location $repoRoot
try
{
  foreach ($case in $baseline.cases)
  {
    $caseId = $case.caseId
    Write-Host "== 12E-08C-R2-02 topology case: $caseId"
    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"
    $firstReportPath = Join-Path $caseRoot "mesh_repair_topology_run_1.json"
    $secondReportPath = Join-Path $caseRoot "mesh_repair_topology_run_2.json"
    Invoke-R2Topology $executable $configPath $firstReportPath $sourceAsset.path $VoxelMm $WeldToleranceMm
    Invoke-R2Topology $executable $configPath $secondReportPath $sourceAsset.path $VoxelMm $WeldToleranceMm

    $first = Read-Json $firstReportPath
    $second = Read-Json $secondReportPath
    Assert-Equal $first.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId report schema"
    Assert-Equal $first.mode "repair_then_strict" "$caseId mode"
    Assert-Equal $first.options.allowVertexWeld $true "$caseId vertex weld option"
    Assert-Equal $first.options.allowWindingRepair $true "$caseId winding option"
    Assert-Equal $first.productionOutputWritten $false "$caseId production output"
    Assert-Equal $first.postRepair.connectedComponents $first.input.componentCount "$caseId component guard"
    Assert-True (
      @($first.vertexMappings).Count -le $first.input.vertexCount) `
      "$caseId output vertex mapping count must not exceed input"
    $mappedSourceVertices = 0
    foreach ($mapping in $first.vertexMappings)
    {
      $mappedSourceVertices += @($mapping.sourceVertexIndices).Count
    }
    Assert-Equal $mappedSourceVertices $first.input.vertexCount "$caseId source vertex coverage"
    foreach ($operation in $first.operations)
    {
      Assert-True ($operation.type -in $allowedOperations) "$caseId unexpected operation: $($operation.type)"
    }

    $firstStable = Get-StableProjection $first
    $secondStable = Get-StableProjection $second
    $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
    $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
    Assert-Equal $firstStableJson $secondStableJson "$caseId topology repeatability"
    $stablePath = Join-Path $caseRoot "mesh_repair_topology_stable.json"
    Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      status = $first.status
      operationCount = @($first.operations).Count
      vertexWeldOperations = @($first.operations | Where-Object { $_.type -eq "weld_vertex" }).Count
      windingOperations = @($first.operations | Where-Object { $_.type -eq "flip_triangle_winding" }).Count
      inputVertices = $first.input.vertexCount
      candidateVertices = @($first.vertexMappings).Count
      inputComponents = $first.input.componentCount
      postComponents = $first.postRepair.connectedComponents
      postBoundaryEdges = $first.postRepair.boundaryEdges
      postNonManifoldEdges = $first.postRepair.nonManifoldEdges
      postLocalWindingIssues = $first.postRepair.localWindingIssues
      operationHash = $first.hashes.repairOperationHash
      postGeometryHash = $first.hashes.postRepairGeometryHash
      postAttributeHash = $first.hashes.postRepairAttributeHash
      stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
      repeatability = "passed"
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_topology_evidence.12e_08c_r2_02.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R2-02"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      weldToleranceMm = $WeldToleranceMm
      componentMergeAllowed = $false
      perCornerUvSwapRequired = $true
      productionOutputWritten = $false
      repeatRunsPerCase = 2
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      nextTask = "12E-08C-R2-03"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R2-02 topology evidence complete: $Output"
}
finally
{
  Pop-Location
}
