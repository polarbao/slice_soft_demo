param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Output = "output/benchmarks/12e_08c_r3_01_non_manifold/non_manifold_summary.json"
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
    nonManifoldAnalysis = $Report.nonManifoldAnalysis
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Invoke-Classifier(
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
      --classify-r3-01 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C-R3-01 failed: source=$SourceId exitCode=$exitCode"
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
    Write-Host "== 12E-08C-R3-01 non-manifold case: $caseId"
    $caseRoot = Join-Path (Split-Path -Parent $outputPath) $caseId
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $case.effectiveConfig))
    $sourceAsset = $case.assets |
      Where-Object { $_.role -in @("obj", "3mf") } |
      Select-Object -First 1
    Assert-True ($null -ne $sourceAsset) "$caseId source asset"
    $firstReportPath = Join-Path $caseRoot "mesh_non_manifold_run_1.json"
    $secondReportPath = Join-Path $caseRoot "mesh_non_manifold_run_2.json"
    Invoke-Classifier $executable $configPath $firstReportPath $sourceAsset.path
    Invoke-Classifier $executable $configPath $secondReportPath $sourceAsset.path

    $first = Read-Json $firstReportPath
    $second = Read-Json $secondReportPath
    Assert-Equal $first.schema "slicesoft.mesh_repair.12e_08c.1" "$caseId report schema"
    Assert-Equal $first.options.classifyNonManifoldPatterns $true "$caseId classifier option"
    Assert-Equal $first.productionOutputWritten $false "$caseId production output"
    Assert-Equal $first.repairAttempted $false "$caseId repair attempt"
    Assert-Equal $first.nonManifoldAnalysis.complete $true "$caseId classification completeness"
    Assert-Equal `
      @($first.nonManifoldAnalysis.edges).Count `
      $first.nonManifoldAnalysis.nonManifoldEdgeCount `
      "$caseId edge record count"
    $patternTotal = $first.nonManifoldAnalysis.duplicateShellOrExporterDuplicateEdges `
      + $first.nonManifoldAnalysis.separableLocalEdgeFanEdges `
      + $first.nonManifoldAnalysis.overlappingComponentEdges `
      + $first.nonManifoldAnalysis.mixedWindingFanEdges `
      + $first.nonManifoldAnalysis.attributeConflictingFanEdges `
      + $first.nonManifoldAnalysis.unclassifiedEdges
    Assert-Equal $patternTotal $first.nonManifoldAnalysis.nonManifoldEdgeCount "$caseId pattern coverage"

    if ($caseId -in @("nai_you_new", "three_mf_texture2d_checker"))
    {
      Assert-Equal $first.nonManifoldAnalysis.status "not_present" "$caseId no non-manifold status"
      Assert-Equal $first.nonManifoldAnalysis.nonManifoldEdgeCount 0 "$caseId no non-manifold count"
    }
    else
    {
      Assert-Equal $first.nonManifoldAnalysis.nonManifoldEdgeCount $case.preRepair.nonManifoldEdges "$caseId baseline count"
      Assert-True ($first.nonManifoldAnalysis.status -in @("classified", "classified_with_unknown")) "$caseId classified status"
      Assert-Equal $first.status "manual_repair_required" "$caseId remains manual"
    }

    $firstStable = Get-StableProjection $first
    $secondStable = Get-StableProjection $second
    $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
    $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
    Assert-Equal $firstStableJson $secondStableJson "$caseId classifier repeatability"
    $stablePath = Join-Path $caseRoot "mesh_non_manifold_stable.json"
    Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      status = $first.status
      classificationStatus = $first.nonManifoldAnalysis.status
      complete = $first.nonManifoldAnalysis.complete
      nonManifoldEdges = $first.nonManifoldAnalysis.nonManifoldEdgeCount
      duplicateShellOrExporterDuplicateEdges = $first.nonManifoldAnalysis.duplicateShellOrExporterDuplicateEdges
      separableLocalEdgeFanEdges = $first.nonManifoldAnalysis.separableLocalEdgeFanEdges
      overlappingComponentEdges = $first.nonManifoldAnalysis.overlappingComponentEdges
      mixedWindingFanEdges = $first.nonManifoldAnalysis.mixedWindingFanEdges
      attributeConflictingFanEdges = $first.nonManifoldAnalysis.attributeConflictingFanEdges
      unclassifiedEdges = $first.nonManifoldAnalysis.unclassifiedEdges
      allUniqueFanSplitsFeasible = $first.nonManifoldAnalysis.allUniqueFanSplitsFeasible
      stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
      repeatability = "passed"
      productionOutputWritten = $first.productionOutputWritten
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_non_manifold_patterns.12e_08c_r3_01.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R3-01"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      classifierOnly = $true
      repairAttempted = $false
      productionOutputWritten = $false
      repeatRunsPerCase = 2
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      classifiedNonManifoldCases = @($caseResults | Where-Object { $_.nonManifoldEdges -gt 0 }).Count
      allUniqueFanSplitCases = @($caseResults | Where-Object { $_.allUniqueFanSplitsFeasible }).Count
      nextTask = "12E-08C-R3-01A"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R3-01 non-manifold evidence complete: $Output"
}
finally
{
  Pop-Location
}
