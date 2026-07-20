param(
  [string]$BuildDir = "build",
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Output = "output/benchmarks/12e_08c_r2_cleanup/cleanup_summary.json"
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
  $candidates = @(
    (Join-Path $BuildRoot "$BuildConfig/$Name.exe"),
    (Join-Path $BuildRoot "$Name.exe")
  )
  foreach ($candidate in $candidates)
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
    attributePreservation = $Report.attributePreservation
    postRepair = $Report.postRepair
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-Cleanup(
  [string]$Executable,
  [string]$ConfigPath,
  [string]$ReportPath,
  [string]$SourceId,
  [double]$CaseVoxelMm)
{
  $outputLines = @(
    & $Executable `
      --config $ConfigPath `
      --output $ReportPath `
      --source-id $SourceId `
      --voxel-mm $CaseVoxelMm `
      --require-openvdb-off `
      --execute-cleanup 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R2 cleanup failed: source=$SourceId exitCode=$exitCode"
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
  if ($LASTEXITCODE -ne 0)
  {
    throw "failed to generate prerequisite R1 baseline"
  }
}

$baseline = Read-Json $baselinePath
Assert-Equal $baseline.schema "slicesoft.mesh_repair_real_model_baseline.12e_08c_r1.1" "baseline schema"
Assert-Equal @($baseline.cases).Count 4 "required baseline case count"

$expectedOperationCounts = @{
  nai_you_new = 1
  aishen_fudiao = 1
  meigui_fudiao = 0
  three_mf_texture2d_checker = 0
}
$caseResults = @()

Push-Location $repoRoot
try
{
  foreach ($case in $baseline.cases)
  {
    $caseId = $case.caseId
    Write-Host "== 12E-08C-R2 cleanup case: $caseId"
    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"
    $firstReportPath = Join-Path $caseRoot "mesh_repair_cleanup_run_1.json"
    $secondReportPath = Join-Path $caseRoot "mesh_repair_cleanup_run_2.json"
    Invoke-Cleanup $executable $configPath $firstReportPath $sourceAsset.path $VoxelMm
    Invoke-Cleanup $executable $configPath $secondReportPath $sourceAsset.path $VoxelMm

    $first = Read-Json $firstReportPath
    $second = Read-Json $secondReportPath
    Assert-Equal $first.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId report schema"
    Assert-Equal $first.mode "repair_then_strict" "$caseId mode"
    Assert-Equal $first.repairEnabled $true "$caseId repair enabled"
    Assert-Equal $first.productionOutputWritten $false "$caseId production output"
    Assert-Equal @($first.operations).Count $expectedOperationCounts[$caseId] "$caseId operation count"
    Assert-Equal @($first.operations | Where-Object { $_.type -eq "remove_exact_duplicate_face" }).Count 0 "$caseId exact duplicate removal"
    Assert-Equal @($first.sourceMappings).Count ($first.preRepair.degenerateTriangles + $first.input.triangleCount) "$caseId source mapping count"
    Assert-Equal $first.attributePreservation.unknownSourceTriangles 0 "$caseId unknown source triangles"

    $firstStable = Get-StableProjection $first
    $secondStable = Get-StableProjection $second
    $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
    $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
    Assert-Equal $firstStableJson $secondStableJson "$caseId cleanup repeatability"
    $stablePath = Join-Path $caseRoot "mesh_repair_cleanup_stable.json"
    Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      status = $first.status
      operationCount = @($first.operations).Count
      operationTypes = @($first.operations | ForEach-Object { $_.type })
      inputTriangles = $first.input.triangleCount
      candidateTriangles = $first.attributePreservation.sourceMappedTriangles
      postStrictPass = $first.postRepair.strictPass
      postBoundaryEdges = $first.postRepair.boundaryEdges
      postNonManifoldEdges = $first.postRepair.nonManifoldEdges
      postDuplicateFaces = $first.postRepair.duplicateFaces
      postOppositeDuplicateFaces = $first.postRepair.oppositeDuplicateFaces
      sourceMappingCount = @($first.sourceMappings).Count
      operationHash = $first.hashes.repairOperationHash
      postGeometryHash = $first.hashes.postRepairGeometryHash
      postAttributeHash = $first.hashes.postRepairAttributeHash
      stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
      repeatability = "passed"
    }
  }

  $threeMf = $caseResults | Where-Object { $_.caseId -eq "three_mf_texture2d_checker" }
  Assert-Equal $threeMf.status "strict_pass_no_repair" "closed 3MF no-op status"
  Assert-Equal $threeMf.postStrictPass $true "closed 3MF post strict"
  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_cleanup_evidence.12e_08c_r2_01.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R2-01"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      mode = "repair_then_strict"
      allowedOperations = @("remove_degenerate_triangle", "remove_exact_duplicate_face")
      oppositeDuplicateRemovalAllowed = $false
      productionOutputWritten = $false
      repeatRunsPerCase = 2
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      nextTask = "12E-08C-R2-02"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R2 cleanup evidence complete: $Output"
}
finally
{
  Pop-Location
}
