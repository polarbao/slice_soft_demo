param(
  [string]$BuildDir = "build",
  [ValidateSet("Release")]
  [string]$Config = "Release",
  [double]$VoxelMm = 0.10,
  [double]$WidthMm = 0.20,
  [string]$BaselineSummary = "output/benchmarks/12e_08c_r1_pre_repair/baseline_summary.json",
  [string]$Expectations = "tests/golden/expected/12e_mesh_repair_release_expectations.json",
  [string]$Output = "output/benchmarks/12e_08c_r3_03_release/release_core_summary.json",
  [switch]$ReuseCoreEvidence,
  [switch]$ReuseQuickCiEvidence,
  [switch]$SkipQuickCi
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

function Invoke-LoggedPowerShell(
  [string]$ScriptPath,
  [string[]]$Arguments,
  [string]$LogPath)
{
  $previousErrorActionPreference = $ErrorActionPreference
  try
  {
    $ErrorActionPreference = "Continue"
    $lines = @(
      & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments 2>&1 |
        ForEach-Object { $_.ToString() }
    )
    $exitCode = $LASTEXITCODE
  }
  finally
  {
    $ErrorActionPreference = $previousErrorActionPreference
  }
  $lines | Set-Content -Encoding UTF8 -LiteralPath $LogPath
  foreach ($line in $lines)
  {
    Write-Host $line
  }
  return [pscustomobject][ordered]@{
    exitCode = $exitCode
    lines = $lines
  }
}

function Get-CaseById($Document, [string]$CaseId)
{
  $case = @($Document.cases) |
    Where-Object { $_.caseId -eq $CaseId } |
    Select-Object -First 1
  Assert-True ($null -ne $case) "missing case: $CaseId"
  return $case
}

function Get-KnownQuickCiDifference([string]$Text)
{
  if ($Text -match "material_process_top2\s+widthPx\s+expected=48\s+actual=226")
  {
    return "material_process_top2 widthPx expected=48 actual=226"
  }
  return ""
}

function Assert-GlobalCoreReport($Report, [string]$CaseId)
{
  Assert-Equal $Report.schema "slicesoft.texture_fill_partition.release_evidence.12e_08c.1" "$CaseId schema"
  Assert-Equal $Report.buildType "Release" "$CaseId build type"
  Assert-Equal $Report.diagnosticOnly $true "$CaseId diagnostic only"
  Assert-Equal $Report.productionOutputWritten $false "$CaseId production output"
  Assert-Equal $Report.productionAdmitted $false "$CaseId production admission"
  Assert-Equal $Report.build.useOpenVdb $false "$CaseId OpenVDB default OFF"
  Assert-Equal $Report.partition.partitionPass $true "$CaseId partition"
  Assert-Equal $Report.textureTransfer.available $true "$CaseId texture transfer"
  Assert-True ($Report.textureTransfer.sampledTextureCount -gt 0) "$CaseId sampled texture evidence"
  Assert-Equal $Report.textureTransfer.fallbackCount 0 "$CaseId texture fallback"
  Assert-Equal $Report.rasterMapping.available $true "$CaseId raster mapping"
  Assert-Equal $Report.rasterMapping.productionOutputWritten $false "$CaseId raster output"
  Assert-Equal $Report.fullClosure.available $true "$CaseId full closure available"
  Assert-Equal $Report.fullClosure.fullClosurePass $true "$CaseId full closure pass"
  Assert-Equal $Report.fullClosure.totalExpectedDomainGapPixels 0 "$CaseId expected-domain gaps"
  Assert-Equal $Report.fullClosure.totalSemanticChannelMismatchPixels 0 "$CaseId semantic mismatch"
  Assert-Equal $Report.fullClosure.productionOutputWritten $false "$CaseId closure output"
  Assert-Equal $Report.timingsMs.outputWriteMs 0 "$CaseId output write exclusion"
  Assert-True ($null -ne $Report.timingsMs.textureTransferMs) "$CaseId transfer timing"
  Assert-True ($null -ne $Report.timingsMs.rasterMappingMs) "$CaseId raster timing"
  Assert-True ($null -ne $Report.timingsMs.fullClosureMs) "$CaseId closure timing"
}

function New-SkippedGlobalCore([string]$BlockerCode)
{
  return [pscustomobject][ordered]@{
    status = "skipped_due_topology"
    blockerCodes = @($BlockerCode)
    partition = "skipped"
    textureTransfer = "skipped"
    rasterMapping = "skipped"
    fullClosure = "skipped"
    productionOutputWritten = $false
  }
}

function New-TimingSummary($RepairReport, $GlobalReport)
{
  $globalAvailable = $null -ne $GlobalReport
  $peakWorkingSetBytes = [UInt64]$RepairReport.performance.peakWorkingSetBytes
  if ($globalAvailable -and [UInt64]$GlobalReport.memory.peakWorkingSetBytes -gt $peakWorkingSetBytes)
  {
    $peakWorkingSetBytes = [UInt64]$GlobalReport.memory.peakWorkingSetBytes
  }
  return [pscustomobject][ordered]@{
    importTransformMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.configLoadMs +
        [double]$GlobalReport.timingsMs.modelLoadMs +
        [double]$GlobalReport.timingsMs.meshAdaptMs
    } else { $null }
    preDiagnosticsEligibilityMs =
      [double]$RepairReport.performance.diagnosticsMs +
      [double]$RepairReport.performance.eligibilityMs
    repairCoreMs = if ($globalAvailable) {
      [double]$RepairReport.performance.repairMs
    } else { $null }
    repairCoreStatus = if ($globalAvailable) {
      "executed_noop"
    } else { "skipped_due_topology" }
    attributeValidationPostStrictMs = if ($globalAvailable) {
      [double]$RepairReport.performance.attributeValidationMs +
        [double]$RepairReport.performance.postDiagnosticsMs +
        [double]$RepairReport.performance.hashMs
    } else { $null }
    attributeValidationPostStrictStatus = if ($globalAvailable) {
      "executed"
    } else { "skipped_due_topology" }
    partitionMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.totalCoreMs
    } else { $null }
    textureTransferMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.textureTransferMs
    } else { $null }
    rasterMappingMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.rasterMappingMs
    } else { $null }
    fullClosureMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.fullClosureMs
    } else { $null }
    globalCoreMs = if ($globalAvailable) {
      [double]$GlobalReport.timingsMs.globalCoreMs
    } else { $null }
    writeJsonMs = $null
    writeJsonStatus = "excluded_not_instrumented"
    writeTiffPreviewMs = $null
    writeTiffPreviewStatus = "not_executed"
    peakWorkingSetBytes = $peakWorkingSetBytes
  }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$benchmarkExe = Resolve-Executable $resolvedBuildDir $Config "texture_fill_partition_release_benchmark"
$baselinePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BaselineSummary))
$expectationsPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Expectations))
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Output))
$runRoot = Split-Path -Parent $outputPath
$repairMatrixPath = Join-Path $runRoot "repair_matrix/repair_matrix_summary.json"
$repairMatrixRelative = $repairMatrixPath.Substring($repoRoot.Length + 1).Replace("\", "/")
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $runRoot "repair_matrix") | Out-Null

$baseline = Read-Json $baselinePath
$expectationsDocument = Read-Json $expectationsPath
Assert-Equal $baseline.schema "slicesoft.mesh_repair_real_model_baseline.12e_08c_r1.1" "baseline schema"
Assert-Equal $expectationsDocument.schema "slicesoft.mesh_repair_release_expectations.12e_08c_r3_03.1" "expectation schema"
Assert-Equal @($baseline.cases).Count 4 "required baseline case count"
Assert-Equal @($expectationsDocument.cases).Count 4 "required expectation case count"

Push-Location $repoRoot
try
{
  Write-Host "== 12E-08C-R3-03 Release repair evidence"
  if (-not $ReuseCoreEvidence)
  {
    & (Join-Path $PSScriptRoot "run_12e_08c_r3_02_repair_matrix.ps1") `
      -BuildDir $BuildDir `
      -Config $Config `
      -VoxelMm $VoxelMm `
      -BaselineSummary $BaselineSummary `
      -Expectations "tests/golden/expected/12e_mesh_repair_matrix_expectations.json" `
      -Output $repairMatrixRelative
  }
  else
  {
    Assert-True (Test-Path -LiteralPath $repairMatrixPath) "reused Release repair matrix is missing"
  }

  $repairMatrix = Read-Json $repairMatrixPath
  Assert-Equal $repairMatrix.schema "slicesoft.mesh_repair_matrix.12e_08c_r3_02.1" "Release repair matrix schema"
  $caseResults = @()

  foreach ($expectation in $expectationsDocument.cases)
  {
    $caseId = $expectation.caseId
    $baselineCase = Get-CaseById $baseline $caseId
    $repairReportPath = Join-Path $runRoot "repair_matrix/$caseId/conservative_repair_run_1.json"
    $repairReport = Read-Json $repairReportPath
    $effectiveConfigPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $baselineCase.effectiveConfig))
    Assert-Equal `
      (Get-FileHash -Algorithm SHA256 -LiteralPath $effectiveConfigPath).Hash.ToLowerInvariant() `
      $baselineCase.effectiveConfigSha256 `
      "$caseId frozen effective config"
    Assert-Equal $repairReport.hashes.sourceHash $baselineCase.hashes.sourceHash "$caseId frozen source hash"
    Assert-Equal $repairReport.hashes.optionsHash $expectationsDocument.repairOptionsHash "$caseId frozen options hash"
    Assert-Equal $repairReport.status $expectation.repairStatus "$caseId Release repair status"
    Assert-Equal $repairReport.productionOutputWritten $false "$caseId repair production output"
    Assert-Equal $repairReport.admission.productionAllowed $false "$caseId repair production admission"

    $globalReport = $null
    $globalCore = $null
    if ($expectation.globalCoreStatus -eq "skipped_due_topology")
    {
      Assert-True (@($repairReport.admission.blockerCodes) -contains $expectation.requiredBlockerCode) "$caseId required blocker"
      $globalCore = New-SkippedGlobalCore $expectation.requiredBlockerCode
    }
    else
    {
      Assert-Equal $expectation.globalCoreStatus "completed" "$caseId global expectation"
      $globalRoot = Join-Path $runRoot "global_core/$caseId"
      New-Item -ItemType Directory -Force -Path $globalRoot | Out-Null
      $globalReportPath = Join-Path $globalRoot "release_evidence.json"
      $configPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $baselineCase.effectiveConfig))
      if (-not $ReuseCoreEvidence)
      {
        & $benchmarkExe `
          --config $configPath `
          --output $globalReportPath `
          --case-name $caseId `
          --voxel-mm $VoxelMm `
          --width-mm $WidthMm `
          --padding-voxels 1
        if ($LASTEXITCODE -ne 0)
        {
          throw "global Release core failed: case=$caseId exitCode=$LASTEXITCODE"
        }
      }
      Assert-True (Test-Path -LiteralPath $globalReportPath) "$caseId reused global report"
      $globalReport = Read-Json $globalReportPath
      Assert-GlobalCoreReport $globalReport $caseId
      $globalCore = [pscustomobject][ordered]@{
        status = "completed"
        blockerCodes = @()
        partition = $globalReport.partition.status
        textureTransfer = $globalReport.textureTransfer.status
        rasterMapping = $globalReport.rasterMapping.status
        fullClosure = $globalReport.fullClosure.status
        fullClosurePass = $globalReport.fullClosure.fullClosurePass
        productionOutputWritten = $false
        reportPath = $globalReportPath.Substring($repoRoot.Length + 1).Replace("\", "/")
      }
    }

    $caseResults += [pscustomobject][ordered]@{
      caseId = $caseId
      sourceHash = $repairReport.hashes.sourceHash
      optionsHash = $repairReport.hashes.optionsHash
      repair = [ordered]@{
        status = $repairReport.status
        repairAttempted = $repairReport.repairAttempted
        attributePreservationPass = $repairReport.attributePreservation.pass
        postStrictAvailable = $repairReport.postRepair.available
        blockerCodes = @($repairReport.admission.blockerCodes)
        productionOutputWritten = $false
      }
      globalCore = $globalCore
      timingsMs = New-TimingSummary $repairReport $globalReport
      productionOutputWritten = $false
    }
  }

  Write-Host "== 12E-08C-R3-03 repair-disabled TIFF/RIP regression"
  $materialLog = Join-Path $runRoot "material_closure_repair_disabled.log"
  if (-not $ReuseCoreEvidence)
  {
    $materialRun = Invoke-LoggedPowerShell `
      (Join-Path $PSScriptRoot "run_material_closure_tests.ps1") `
      @("-BuildDir", $BuildDir, "-Config", $Config, "-Mode", "RepairDisabled") `
      $materialLog
    Assert-Equal $materialRun.exitCode 0 "repair-disabled TIFF/RIP regression"
  }
  else
  {
    Assert-True (Test-Path -LiteralPath $materialLog) "reused material-closure log is missing"
    $materialText = Get-Content -Raw -Encoding UTF8 -LiteralPath $materialLog
    Assert-True ($materialText -match "TIFF SHA-256 invariant: PASS") "reused TIFF invariant evidence"
    Assert-True ($materialText -match "12D-06 Repair Disabled verification: PASS") "reused repair-disabled evidence"
    Assert-True (([regex]::Matches($materialText, "rip_reader_test: PASS")).Count -ge 2) "reused RIP strict evidence"
  }

  $quickCiStatus = "skipped_by_option"
  $quickCiDetail = ""
  if (-not $SkipQuickCi)
  {
    Write-Host "== 12E-08C-R3-03 Quick CI attribution"
    $quickLog = Join-Path $runRoot "quick_ci.log"
    if (-not $ReuseQuickCiEvidence)
    {
      $quickRun = Invoke-LoggedPowerShell `
        (Join-Path $PSScriptRoot "run_ci_quick.ps1") `
        @() `
        $quickLog
      $quickText = $quickRun.lines -join "`n"
      $quickExitCode = $quickRun.exitCode
    }
    else
    {
      Assert-True (Test-Path -LiteralPath $quickLog) "reused Quick CI log is missing"
      $quickText = Get-Content -Raw -Encoding UTF8 -LiteralPath $quickLog
      $quickExitCode = if ($quickText -match "CI quick complete\.") { 0 } else { 1 }
    }

    $knownDifference = Get-KnownQuickCiDifference $quickText
    if ($quickExitCode -eq 0)
    {
      $quickCiStatus = "passed"
    }
    elseif (-not [string]::IsNullOrWhiteSpace($knownDifference))
    {
      $quickCiStatus = "failed_known_baseline"
      $quickCiDetail = $knownDifference
    }
    else
    {
      $quickCiStatus = "failed_unexpected"
      $quickCiDetail = ($quickText -split "`r?`n" | Select-Object -Last 20) -join " | "
    }
  }

  $blockedCases = @($caseResults | Where-Object {
      $_.globalCore.status -eq "skipped_due_topology"
    })
  $completedCases = @($caseResults | Where-Object {
      $_.globalCore.status -eq "completed"
    })
  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.mesh_repair_release_evidence.12e_08c_r3_03.1"
    generatedAt = (Get-Date).ToString("o")
    stage = "12E-08C-R3-03"
    build = [ordered]@{
      buildType = $Config
      buildDir = $BuildDir.Replace("\", "/")
      useOpenVdb = $false
      backend = "legacy_cpu_global_distance"
    }
    contract = [ordered]@{
      diagnosticOnly = $true
      globalProductionPackageWritten = $false
      coreExcludesJsonTiffPngWrite = $true
      skippedStagesUseNullTiming = $true
      channelOrder = @("R", "G", "B", "W", "S", "V")
      bitDepth = 8
      polarity = "black_is_print"
    }
    cases = $caseResults
    legacyRegression = [ordered]@{
      repairEnabledDefault = $false
      repairDisabledTiffInvariant = "passed"
      ripStrict = "passed"
      protocol = "p0.rgbwsv.2/RGBWSV/uint8/black_is_print"
      quickCi = $quickCiStatus
      quickCiDetail = $quickCiDetail
    }
    result = [ordered]@{
      evidenceStatus = if ($quickCiStatus -eq "failed_unexpected") { "blocked" } else { "complete" }
      releaseBudget = "blocked"
      thresholdsFrozen = $false
      requiredCaseCount = $caseResults.Count
      globalCoreCompletedCases = $completedCases.Count
      globalCoreSkippedCases = $blockedCases.Count
      productionAdmission = "blocked"
      productionOutputWritten = $false
      nextTask = "12E-08C-R3-04"
      nextTaskReadiness = "ready_for_no_go_decision"
    }
  }
  Write-Utf8NoBom $outputPath ($summary | ConvertTo-Json -Depth 100)
  Write-Host "12E-08C-R3-03 Release evidence complete: $Output"
  Write-Host "globalCompleted=$($completedCases.Count) topologySkipped=$($blockedCases.Count) quickCi=$quickCiStatus"

  if ($quickCiStatus -eq "failed_unexpected")
  {
    throw "Quick CI failed outside the frozen known baseline; see $runRoot/quick_ci.log"
  }
}
finally
{
  Pop-Location
}
