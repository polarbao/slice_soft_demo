param(
  [string]$BuildDir = "build",
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [double]$VoxelMm = 0.10,
  [string]$Output = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json"
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

function Get-RelativePath([string]$BasePath, [string]$TargetPath)
{
  $baseFullPath = [System.IO.Path]::GetFullPath($BasePath).TrimEnd("\", "/") +
    [System.IO.Path]::DirectorySeparatorChar
  $targetFullPath = [System.IO.Path]::GetFullPath($TargetPath)
  $baseUri = New-Object System.Uri($baseFullPath)
  $targetUri = New-Object System.Uri($targetFullPath)
  return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString())
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

function New-AssetEvidence([string]$RepoRoot, [string]$RelativePath, [string]$Role)
{
  $absolutePath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $RelativePath))
  Assert-True (Test-Path -LiteralPath $absolutePath) "missing $Role asset: $RelativePath"
  return [pscustomobject][ordered]@{
    role = $Role
    path = $RelativePath.Replace("\", "/")
    bytes = (Get-Item -LiteralPath $absolutePath).Length
    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $absolutePath).Hash.ToLowerInvariant()
  }
}

function New-StableProjection($Report)
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
    attributePreservation = $Report.attributePreservation
    postRepair = $Report.postRepair
    admission = $Report.admission
    issues = @($Report.issues)
  }
}

function Assert-Report($Report, [string]$CaseId)
{
  Assert-Equal $Report.schema "slicesoft.mesh_repair.12e_08c.1" "$CaseId schema"
  Assert-Equal $Report.mode "strict_closed" "$CaseId mode"
  Assert-Equal $Report.repairEnabled $false "$CaseId repairEnabled"
  Assert-Equal $Report.repairAttempted $false "$CaseId repairAttempted"
  Assert-Equal $Report.productionOutputWritten $false "$CaseId productionOutputWritten"
  Assert-Equal $Report.admission.productionAllowed $false "$CaseId productionAllowed"
  Assert-Equal $Report.preRepair.available $true "$CaseId preRepair.available"
  Assert-Equal $Report.postRepair.available $false "$CaseId postRepair.available"
  Assert-True ($Report.status -ne "not_evaluated") "$CaseId status must be evaluated"
  Assert-True ($Report.input.vertexCount -gt 0) "$CaseId vertexCount"
  Assert-True ($Report.input.triangleCount -gt 0) "$CaseId triangleCount"
  Assert-True ($Report.input.componentCount -gt 0) "$CaseId componentCount"
  Assert-True ($Report.hashes.sourceHash.Length -eq 64) "$CaseId sourceHash"
  Assert-True ($Report.hashes.preRepairGeometryHash.Length -eq 64) "$CaseId geometryHash"
  Assert-True ($Report.hashes.preRepairAttributeHash.Length -eq 64) "$CaseId attributeHash"
  Assert-Equal @($Report.operations).Count 0 "$CaseId operations"
}

function Invoke-BaselineCase(
  [string]$RepoRoot,
  [string]$PreflightExe,
  $Case,
  [string]$CaseRoot,
  [double]$CaseVoxelMm)
{
  New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null
  $templatePath = Join-Path $RepoRoot "samples/configs/texture_fill_partition/global_surface_shell_unavailable.json"
  $effectiveConfig = Read-Json $templatePath
  $modelAsset = $Case.assets | Where-Object { $_.role -in @("obj", "3mf") } | Select-Object -First 1
  $modelPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $modelAsset.path))
  $effectiveConfig.input.modelPath = $modelPath.Replace("\", "/")
  $effectiveConfig.input.format = "auto"
  $effectiveConfig.output.packageDir = "output/benchmarks/12e_08c_r1_pre_repair/no_production_package"
  if ($effectiveConfig.PSObject.Properties.Name -notcontains "autoOrient")
  {
    $effectiveConfig | Add-Member -NotePropertyName autoOrient -NotePropertyValue ([pscustomobject][ordered]@{
      enabled = $true
      maxHeightMm = 6.0
      strategy = "minimize_height_by_right_angle_rotation"
    })
  }

  $configPath = Join-Path $CaseRoot "slice_config.effective.json"
  $firstReportPath = Join-Path $CaseRoot "mesh_repair_preflight_run_1.json"
  $secondReportPath = Join-Path $CaseRoot "mesh_repair_preflight_run_2.json"
  Write-Utf8NoBom $configPath ($effectiveConfig | ConvertTo-Json -Depth 32)

  foreach ($reportPath in @($firstReportPath, $secondReportPath))
  {
    $outputLines = @(
      & $PreflightExe `
        --config $configPath `
        --output $reportPath `
        --source-id ($modelAsset.path.Replace("\", "/")) `
        --voxel-mm $CaseVoxelMm `
        --require-openvdb-off 2>&1 |
        ForEach-Object { $_.ToString() }
    )
    $exitCode = $LASTEXITCODE
    foreach ($line in $outputLines)
    {
      Write-Host $line
    }
    if ($exitCode -ne 0)
    {
      throw "12E-08C-R1 preflight failed: case=$($Case.id) exitCode=$exitCode"
    }
  }

  $firstReport = Read-Json $firstReportPath
  $secondReport = Read-Json $secondReportPath
  Assert-Report $firstReport $Case.id
  Assert-Report $secondReport $Case.id
  $firstStable = New-StableProjection $firstReport
  $secondStable = New-StableProjection $secondReport
  $firstStableJson = $firstStable | ConvertTo-Json -Depth 64 -Compress
  $secondStableJson = $secondStable | ConvertTo-Json -Depth 64 -Compress
  Assert-Equal $firstStableJson $secondStableJson "$($Case.id) stable projection repeatability"

  $stablePath = Join-Path $CaseRoot "mesh_repair_preflight_stable.json"
  Write-Utf8NoBom $stablePath ($firstStable | ConvertTo-Json -Depth 64)
  $assets = @(
    $Case.assets | ForEach-Object {
      New-AssetEvidence $RepoRoot $_.path $_.role
    }
  )
  return [pscustomobject][ordered]@{
    caseId = $Case.id
    inputKind = $Case.inputKind
    requiredRealModel = [bool]$Case.requiredRealModel
    assets = $assets
    effectiveConfig = (Get-RelativePath $RepoRoot $configPath).Replace("\", "/")
    effectiveConfigSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $configPath).Hash.ToLowerInvariant()
    reportPath = (Get-RelativePath $RepoRoot $firstReportPath).Replace("\", "/")
    stableEvidencePath = (Get-RelativePath $RepoRoot $stablePath).Replace("\", "/")
    stableEvidenceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stablePath).Hash.ToLowerInvariant()
    repeatability = "passed"
    status = $firstReport.status
    input = $firstReport.input
    hashes = $firstReport.hashes
    preRepair = $firstReport.preRepair
    eligibility = $firstReport.eligibility
    admission = $firstReport.admission
    performanceRun1 = $firstReport.performance
  }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$preflightExe = Resolve-Executable $resolvedBuildDir $Config "mesh_repair_preflight"
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Output))
$runRoot = Split-Path -Parent $outputPath
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$cases = @(
  [pscustomobject][ordered]@{
    id = "nai_you_new"
    inputKind = "obj_mtl_texture"
    requiredRealModel = $true
    assets = @(
      @{ role = "obj"; path = "model/obj/nai_you_new/MF_nai_you.obj" },
      @{ role = "mtl"; path = "model/obj/nai_you_new/MF_nai_you.mtl" },
      @{ role = "texture"; path = "model/obj/nai_you_new/T_Nai_you.png" }
    )
  },
  [pscustomobject][ordered]@{
    id = "aishen_fudiao"
    inputKind = "obj_mtl_texture"
    requiredRealModel = $true
    assets = @(
      @{ role = "obj"; path = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj" },
      @{ role = "mtl"; path = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.mtl" },
      @{ role = "texture"; path = "model/obj/aishen_fudiao/T_aishen_damuzhi_L_tx02.png" }
    )
  },
  [pscustomobject][ordered]@{
    id = "meigui_fudiao"
    inputKind = "obj_mtl_texture"
    requiredRealModel = $true
    assets = @(
      @{ role = "obj"; path = "model/obj/meigui_fudiao/04.obj" },
      @{ role = "mtl"; path = "model/obj/meigui_fudiao/04.mtl" },
      @{ role = "texture"; path = "model/obj/meigui_fudiao/zhongzhi1(4).png" }
    )
  },
  [pscustomobject][ordered]@{
    id = "three_mf_texture2d_checker"
    inputKind = "3mf_texture2d"
    requiredRealModel = $true
    assets = @(
      @{ role = "3mf"; path = "samples/models/3mf/texture2d_checker_cube.3mf" }
    )
  }
)

$caseResults = @()
Push-Location $repoRoot
try
{
  foreach ($case in $cases)
  {
    Write-Host "== 12E-08C-R1 pre-repair case: $($case.id)"
    $caseRoot = Join-Path $runRoot $case.id
    $caseResults += Invoke-BaselineCase `
      $repoRoot `
      $preflightExe `
      $case `
      $caseRoot `
      $VoxelMm
  }

  $threeMf = $caseResults | Where-Object { $_.caseId -eq "three_mf_texture2d_checker" }
  Assert-Equal $caseResults.Count 4 "required baseline case count"
  Assert-Equal $threeMf.status "strict_pass_no_repair" "closed 3MF no-op baseline"
  Assert-Equal $threeMf.preRepair.strictPass $true "closed 3MF strict baseline"

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_real_model_baseline.12e_08c_r1.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R1-04"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
    }
    contract = [ordered]@{
      mode = "strict_closed"
      repairEnabled = $false
      repairAttempted = $false
      productionOutputWritten = $false
      repeatRunsPerCase = 2
      timingExcludedFromRepeatabilityHash = $true
    }
    cases = $caseResults
    result = [ordered]@{
      evidenceStatus = "complete"
      caseCount = $caseResults.Count
      repeatabilityPassedCases = @($caseResults | Where-Object { $_.repeatability -eq "passed" }).Count
      productionAdmission = "not_evaluated_by_r1"
      nextTask = "12E-08C-R2-01"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C-R1 pre-repair baseline complete: $Output"
}
finally
{
  Pop-Location
}
