param(
  [string]$BuildDir = "build",
  [ValidateSet("Release")]
  [string]$Config = "Release",
  [double]$VoxelMm = 0.10,
  [double]$WidthMm = 0.20,
  [string]$Output = "output/benchmarks/12e_08c/release_evidence_summary.json",
  [switch]$SkipQuickRegression
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

function Assert-CaseReport($Report, [string]$CaseId)
{
  Assert-Equal $Report.schema "slicesoft.texture_fill_partition.release_evidence.12e_08c.1" "$CaseId schema"
  Assert-Equal $Report.buildType "Release" "$CaseId buildType"
  Assert-Equal $Report.diagnosticOnly $true "$CaseId diagnosticOnly"
  Assert-Equal $Report.productionOutputWritten $false "$CaseId productionOutputWritten"
  Assert-Equal $Report.productionAdmitted $false "$CaseId productionAdmitted"
  Assert-Equal $Report.build.useOpenVdb $false "$CaseId default OpenVDB OFF"
  Assert-Equal $Report.timingsMs.outputWriteMs 0 "$CaseId outputWriteMs"
  Assert-True ($Report.memory.gridVoxelCount -gt 0) "$CaseId gridVoxelCount"
  Assert-True ($Report.timingsMs.totalCoreMs -ge 0) "$CaseId totalCoreMs"

  if ($Report.partition.partitionPass)
  {
    Assert-Equal $Report.partition.status "diagnostic" "$CaseId diagnostic status"
    Assert-Equal $Report.partition.overlapTextureFillVoxels 0 "$CaseId overlap"
    Assert-Equal $Report.partition.unassignedModelVoxels 0 "$CaseId unassigned"
    Assert-Equal $Report.partition.textureOutsideModelVoxels 0 "$CaseId texture outside model"
    Assert-Equal $Report.partition.modelFillOutsideModelVoxels 0 "$CaseId fill outside model"
  }
  else
  {
    Assert-Equal $Report.partition.status "blocked" "$CaseId blocked status"
    Assert-True (@($Report.issues).Count -gt 0) "$CaseId blocked report requires issues"
  }
}

function Invoke-ReleaseCase(
  [string]$RepoRoot,
  [string]$BenchmarkExe,
  $Case,
  [string]$CaseRoot,
  [double]$CaseVoxelMm,
  [double]$CaseWidthMm)
{
  New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null
  $templatePath = Join-Path $RepoRoot "samples/configs/texture_fill_partition/global_surface_shell_unavailable.json"
  $effectiveConfig = Read-Json $templatePath
  $modelAsset = $Case.assets | Where-Object { $_.role -in @("obj", "3mf") } | Select-Object -First 1
  $modelPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $modelAsset.path))
  $effectiveConfig.input.modelPath = $modelPath.Replace("\", "/")
  $effectiveConfig.input.format = "auto"
  $effectiveConfig.output.packageDir = "output/benchmarks/12e_08c/no_production_package"
  $effectiveConfig.texture.surfaceShell.widthMm = $CaseWidthMm
  if ($effectiveConfig.PSObject.Properties.Name -notcontains "autoOrient")
  {
    $effectiveConfig | Add-Member -NotePropertyName autoOrient -NotePropertyValue ([pscustomobject][ordered]@{
      enabled = $true
      maxHeightMm = 6.0
      strategy = "minimize_height_by_right_angle_rotation"
    })
  }

  $configPath = Join-Path $CaseRoot "slice_config.effective.json"
  $reportPath = Join-Path $CaseRoot "release_evidence.json"
  $logPath = Join-Path $CaseRoot "benchmark.log"
  Write-Utf8NoBom $configPath ($effectiveConfig | ConvertTo-Json -Depth 32)

  $outputLines = @(
    & $BenchmarkExe `
      --config $configPath `
      --output $reportPath `
      --case-name $Case.id `
      --voxel-mm $CaseVoxelMm `
      --width-mm $CaseWidthMm `
      --padding-voxels 1 2>&1 |
      ForEach-Object { $_.ToString() }
  )
  $exitCode = $LASTEXITCODE
  $outputLines | Set-Content -Encoding UTF8 -LiteralPath $logPath
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "12E-08C benchmark failed: case=$($Case.id) exitCode=$exitCode"
  }

  $report = Read-Json $reportPath
  Assert-CaseReport $report $Case.id
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
    reportPath = (Get-RelativePath $RepoRoot $reportPath).Replace("\", "/")
    reportSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $reportPath).Hash.ToLowerInvariant()
    partitionPass = [bool]$report.partition.partitionPass
    status = $report.partition.status
    topology = $report.topology
    grid = $report.grid
    partition = $report.partition
    timingsMs = $report.timingsMs
    memory = $report.memory
    issues = @($report.issues)
  }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$benchmarkExe = Resolve-Executable $resolvedBuildDir $Config "texture_fill_partition_release_benchmark"
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
    Write-Host "== 12E-08C Release case: $($case.id)"
    $caseRoot = Join-Path $runRoot $case.id
    $caseResults += Invoke-ReleaseCase `
      $repoRoot `
      $benchmarkExe `
      $case `
      $caseRoot `
      $VoxelMm `
      $WidthMm
  }

  Write-Host "== 12E-08C legacy repair-disabled TIFF invariant"
  & (Join-Path $repoRoot "scripts/run_material_closure_tests.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -Mode RepairDisabled
  if ($LASTEXITCODE -ne 0)
  {
    throw "Release repair-disabled legacy regression failed"
  }

  $quickRegressionStatus = "skipped"
  if (-not $SkipQuickRegression)
  {
    Write-Host "== 12E-08C legacy quick regression"
    & (Join-Path $repoRoot "scripts/run_regression.ps1") `
      -Mode quick `
      -BuildDir $BuildDir `
      -Config $Config `
      -SkipBuild
    if ($LASTEXITCODE -ne 0)
    {
      throw "Release quick regression failed"
    }
    $quickRegressionStatus = "passed"
  }

  $blockedCases = @($caseResults | Where-Object { -not $_.partitionPass })
  $requiredCases = @($caseResults | Where-Object { $_.requiredRealModel })
  $requiredPassCases = @($requiredCases | Where-Object { $_.partitionPass })
  $releaseBudgetStatus = if ($requiredPassCases.Count -eq $requiredCases.Count) { "measured" } else { "blocked" }
  $decisionReasons = @()
  if ($blockedCases.Count -gt 0)
  {
    $decisionReasons += "strict_topology_blocked_real_models"
  }
  if ($quickRegressionStatus -eq "skipped")
  {
    $decisionReasons += "quick_regression_skipped"
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.texture_fill_partition.release_matrix.12e_08c.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
      backend = "legacy_cpu_global_distance"
    }
    benchmarkContract = [ordered]@{
      voxelMm = $VoxelMm
      widthMm = $WidthMm
      coreExcludesTiffPngJsonWrite = $true
      productionOutputWritten = $false
      productionAdmitted = $false
    }
    cases = $caseResults
    releaseBudget = [ordered]@{
      status = $releaseBudgetStatus
      thresholdsFrozen = $false
      requiredCaseCount = $requiredCases.Count
      passedRequiredCaseCount = $requiredPassCases.Count
      blockedCaseIds = @($blockedCases | ForEach-Object { $_.caseId })
      reason = if ($releaseBudgetStatus -eq "measured") {
        "all required cases measured; a production threshold still requires an admission decision"
      } else {
        "strict topology blockers prevent a representative real-model budget from being frozen"
      }
    }
    legacyRegression = [ordered]@{
      repairDisabledTiffInvariant = "passed"
      ripStrict = "passed"
      quickRegression = $quickRegressionStatus
      protocol = "p0.rgbwsv.2/RGBWSV/uint8/black_is_print"
    }
    decision = [ordered]@{
      evidenceStatus = "complete"
      productionAdmission = "blocked"
      nextAllowedTask = "12E-09A_diagnostic_ui"
      blockedTask = "12E-08D_production_admission"
      reasons = $decisionReasons
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12E-08C evidence complete: $Output"
  Write-Host "releaseBudget=$releaseBudgetStatus blockedCases=$($blockedCases.Count)"
}
finally
{
  Pop-Location
}
