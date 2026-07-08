param(
  [ValidateSet("legacy", "openvdb", "all")]
  [string]$Engine = "legacy",

  [ValidateSet("Debug", "Release")]
  [string]$BuildType = "Release",

  [string]$LegacyCli = "",
  [string]$OpenVdbCli = "",
  [string]$LegacyConfig = "samples\configs\slice_config.json",
  [string]$OpenVdbConfig = "samples\configs\openvdb_candidate\closed_textured_obj_candidate.json",
  [string]$CaseName = "default_same_pose",
  [string]$Output = "output\benchmarks\core_benchmark_12b.json",
  [switch]$NoImageWrite
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-RepoPath([string]$PathValue) {
  if ([string]::IsNullOrWhiteSpace($PathValue)) {
    return ""
  }
  if ([System.IO.Path]::IsPathRooted($PathValue)) {
    return $PathValue
  }
  return Join-Path $RepoRoot $PathValue
}

function Resolve-ConfigRelativePath([string]$ConfigPath, [string]$PathValue) {
  if ([string]::IsNullOrWhiteSpace($PathValue)) {
    return $null
  }
  if ([System.IO.Path]::IsPathRooted($PathValue)) {
    return [System.IO.Path]::GetFullPath($PathValue)
  }

  $resolvedConfig = Resolve-RepoPath $ConfigPath
  $configDirectory = Split-Path -Parent $resolvedConfig
  if ([string]::IsNullOrWhiteSpace($configDirectory)) {
    $configDirectory = $RepoRoot
  }
  return [System.IO.Path]::GetFullPath((Join-Path $configDirectory $PathValue))
}

function Get-JsonProperty($Object, [string]$Name, $DefaultValue = $null) {
  if ($null -eq $Object) {
    return $DefaultValue
  }
  $property = $Object.PSObject.Properties[$Name]
  if ($null -eq $property) {
    return $DefaultValue
  }
  return $property.Value
}

function ConvertTo-Hashtable($Value) {
  if ($null -eq $Value) {
    return $null
  }
  if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string]) -and -not ($Value -is [pscustomobject])) {
    $items = @()
    foreach ($item in $Value) {
      $items += ConvertTo-Hashtable $item
    }
    return $items
  }
  if ($Value -is [pscustomobject]) {
    $table = [ordered]@{}
    foreach ($property in $Value.PSObject.Properties) {
      $table[$property.Name] = ConvertTo-Hashtable $property.Value
    }
    return $table
  }
  return $Value
}

function ConvertFrom-BenchmarkStdout([string]$Text) {
  $trimmed = $Text.Trim()
  if ([string]::IsNullOrWhiteSpace($trimmed)) {
    throw "benchmark stdout is empty"
  }

  $firstBrace = $trimmed.IndexOf("{")
  $lastBrace = $trimmed.LastIndexOf("}")
  if ($firstBrace -lt 0 -or $lastBrace -lt $firstBrace) {
    throw "benchmark stdout does not contain a JSON object"
  }

  $jsonText = $trimmed.Substring($firstBrace, $lastBrace - $firstBrace + 1)
  return $jsonText | ConvertFrom-Json
}

function Read-ConfigSummary([string]$ConfigPath) {
  $resolvedPath = Resolve-RepoPath $ConfigPath
  if (-not (Test-Path $resolvedPath)) {
    return [ordered]@{
      configPath = $ConfigPath
      available = $false
      reason = "config_not_found"
    }
  }

  $config = Get-Content -Raw $resolvedPath | ConvertFrom-Json
  $inputSection = Get-JsonProperty $config "input"
  $outputSection = Get-JsonProperty $config "output"
  $transformSection = Get-JsonProperty $config "modelTransform"
  $autoOrientSection = Get-JsonProperty $config "autoOrient"

  $modelPath = Get-JsonProperty $inputSection "modelPath" $null

  return [ordered]@{
    configPath = $ConfigPath
    available = $true
    modelPath = $modelPath
    resolvedModelPath = Resolve-ConfigRelativePath $ConfigPath $modelPath
    layerThicknessMm = Get-JsonProperty $outputSection "layerThicknessMm" $null
    dpiX = Get-JsonProperty $outputSection "dpiX" $null
    dpiY = Get-JsonProperty $outputSection "dpiY" $null
    transform = [ordered]@{
      scale = Get-JsonProperty $transformSection "scale" @()
      rotationDeg = Get-JsonProperty $transformSection "rotationDeg" @()
      translationMm = Get-JsonProperty $transformSection "translationMm" @()
      autoOrientEnabled = [bool](Get-JsonProperty $autoOrientSection "enabled" $false)
      autoOrientApplied = $null
    }
  }
}

function Join-ComparableArray($Value) {
  if ($null -eq $Value) {
    return ""
  }
  if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string])) {
    $items = @()
    foreach ($item in $Value) {
      $items += [string]$item
    }
    return ($items -join ",")
  }
  return [string]$Value
}

function Test-ConfigCompatibility($LegacySummary, $OpenVdbSummary, [string]$SelectedEngine) {
  if ($SelectedEngine -eq "legacy" -or $SelectedEngine -eq "openvdb") {
    return [ordered]@{
      samePose = $true
      samePoseReason = "single_engine_benchmark"
      sameResolution = $true
      sameResolutionReason = "single_engine_benchmark"
    }
  }

  $samePose = $false
  $samePoseReason = "config_unavailable"
  $sameResolution = $false
  $sameResolutionReason = "config_unavailable"

  if ($LegacySummary.available -and $OpenVdbSummary.available) {
    $legacyTransform = $LegacySummary.transform
    $openVdbTransform = $OpenVdbSummary.transform
    $sameModel =
      -not [string]::IsNullOrWhiteSpace($LegacySummary.resolvedModelPath) -and
      -not [string]::IsNullOrWhiteSpace($OpenVdbSummary.resolvedModelPath) -and
      ([string]::Equals($LegacySummary.resolvedModelPath, $OpenVdbSummary.resolvedModelPath, [System.StringComparison]::OrdinalIgnoreCase))
    $sameScale = (Join-ComparableArray $legacyTransform.scale) -eq (Join-ComparableArray $openVdbTransform.scale)
    $sameRotation = (Join-ComparableArray $legacyTransform.rotationDeg) -eq (Join-ComparableArray $openVdbTransform.rotationDeg)
    $sameTranslation = (Join-ComparableArray $legacyTransform.translationMm) -eq (Join-ComparableArray $openVdbTransform.translationMm)
    $sameAutoOrient = [bool]$legacyTransform.autoOrientEnabled -eq [bool]$openVdbTransform.autoOrientEnabled

    $samePose = $sameModel -and $sameScale -and $sameRotation -and $sameTranslation -and $sameAutoOrient
    if ($samePose) {
      $samePoseReason = "same_model_transform_and_auto_orient"
    }
    else {
      $poseReasons = @()
      if (-not $sameModel) {
        $poseReasons += "model_path_differs"
      }
      if (-not $sameScale) {
        $poseReasons += "scale_differs"
      }
      if (-not $sameRotation) {
        $poseReasons += "rotation_differs"
      }
      if (-not $sameTranslation) {
        $poseReasons += "translation_differs"
      }
      if (-not $sameAutoOrient) {
        $poseReasons += "auto_orient_differs"
      }
      $samePoseReason = $poseReasons -join ";"
    }

    $sameDpiX = [int]$LegacySummary.dpiX -eq [int]$OpenVdbSummary.dpiX
    $sameDpiY = [int]$LegacySummary.dpiY -eq [int]$OpenVdbSummary.dpiY
    $sameLayerThickness = [double]$LegacySummary.layerThicknessMm -eq [double]$OpenVdbSummary.layerThicknessMm
    $sameResolution = $sameDpiX -and $sameDpiY -and $sameLayerThickness
    if ($sameResolution) {
      $sameResolutionReason = "same_dpi_and_layer_thickness"
    }
    else {
      $resolutionReasons = @()
      if (-not $sameDpiX) {
        $resolutionReasons += "dpi_x_differs"
      }
      if (-not $sameDpiY) {
        $resolutionReasons += "dpi_y_differs"
      }
      if (-not $sameLayerThickness) {
        $resolutionReasons += "layer_thickness_differs"
      }
      $sameResolutionReason = $resolutionReasons -join ";"
    }
  }

  return [ordered]@{
    samePose = $samePose
    samePoseReason = $samePoseReason
    sameResolution = $sameResolution
    sameResolutionReason = $sameResolutionReason
  }
}

function New-UnavailableEngineResult([string]$EngineName, [string]$ConfigPath, [string[]]$FailureReasons) {
  return [ordered]@{
    engine = $EngineName
    available = $false
    sourceSchema = $null
    configPath = $ConfigPath
    buildType = $BuildType
    modelPath = $null
    grid = [ordered]@{
      widthPx = 0
      heightPx = 0
      layerCount = 0
    }
    stats = [ordered]@{
      modelPixels = 0
      supportPixels = 0
      rgbPrintPixels = $null
      whitePrintPixels = $null
      varnishPrintPixels = $null
      shellPixels = 0
    }
    timingsMs = [ordered]@{
      import = $null
      coreCompute = $null
      materialCompose = $null
      ioWrite = 0
      previewWrite = 0
      reportWrite = 0
      endToEnd = $null
    }
    profile = [ordered]@{
      available = $false
      profileLevel = "unavailable"
      reason = "engine_unavailable"
      notes = @()
    }
    memory = [ordered]@{
      processPeakWorkingSetAvailable = $false
      processPeakWorkingSetBytes = $null
      processWorkingSetBytes = $null
    }
    replacementGate = [ordered]@{
      outputSemanticsComparable = $false
      performanceComparable = $false
      replacementPass = $false
      failureReasons = $FailureReasons
    }
  }
}

function Normalize-EngineResult($RawResult, [string]$EngineName, [string]$ConfigPath) {
  $grid = Get-JsonProperty $RawResult "grid"
  $stats = Get-JsonProperty $RawResult "stats"
  $timings = Get-JsonProperty $RawResult "timingsMs"
  $memory = Get-JsonProperty $RawResult "memory"
  $gate = Get-JsonProperty $RawResult "replacementGate"
  $profile = Get-JsonProperty $RawResult "profile"

  $outputSemanticsComparable = [bool](Get-JsonProperty $gate "outputSemanticsComparable" $false)
  $performanceComparable = [bool](Get-JsonProperty $gate "performanceComparable" $false)
  $replacementPass = [bool](Get-JsonProperty $gate "replacementPass" $false)
  $reason = Get-JsonProperty $gate "reason" $null
  $failureReasons = @()
  if (-not $outputSemanticsComparable) {
    $failureReasons += "$($EngineName)_output_semantics_not_comparable"
  }
  if (-not [string]::IsNullOrWhiteSpace($reason) -and $reason -ne "single-engine benchmark; compare pair externally") {
    $failureReasons += $reason
  }

  return [ordered]@{
    engine = $EngineName
    available = $true
    sourceSchema = Get-JsonProperty $RawResult "schema" $null
    configPath = $ConfigPath
    buildType = Get-JsonProperty $RawResult "buildType" $BuildType
    modelPath = Get-JsonProperty $RawResult "modelPath" $null
    grid = [ordered]@{
      widthPx = [int](Get-JsonProperty $grid "widthPx" 0)
      heightPx = [int](Get-JsonProperty $grid "heightPx" 0)
      layerCount = [int](Get-JsonProperty $grid "layerCount" 0)
    }
    stats = [ordered]@{
      modelPixels = [int](Get-JsonProperty $stats "modelPixels" 0)
      supportPixels = [int](Get-JsonProperty $stats "supportPixels" 0)
      rgbPrintPixels = $null
      whitePrintPixels = $null
      varnishPrintPixels = $null
      shellPixels = [int](Get-JsonProperty $stats "shellPixels" 0)
    }
    timingsMs = [ordered]@{
      import = $null
      coreCompute = [double](Get-JsonProperty $timings "coreCompute" 0)
      materialCompose = $null
      ioWrite = 0
      previewWrite = 0
      reportWrite = 0
      endToEnd = [double](Get-JsonProperty $timings "endToEnd" 0)
    }
    profile = if ($null -ne $profile) {
      ConvertTo-Hashtable $profile
    }
    else {
      [ordered]@{
        available = $false
        profileLevel = "unavailable"
        reason = "source_benchmark_did_not_emit_profile"
        notes = @()
      }
    }
    memory = [ordered]@{
      processPeakWorkingSetAvailable = [bool](Get-JsonProperty $memory "available" $false)
      processPeakWorkingSetBytes = Get-JsonProperty $memory "peakWorkingSetBytes" $null
      processWorkingSetBytes = Get-JsonProperty $memory "workingSetBytes" $null
    }
    replacementGate = [ordered]@{
      outputSemanticsComparable = $outputSemanticsComparable
      performanceComparable = $performanceComparable
      replacementPass = $replacementPass
      failureReasons = $failureReasons
    }
  }
}

function Invoke-CoreBenchmark([string]$CliPath, [string]$ConfigPath, [string]$EngineName, [bool]$Required) {
  $resolvedCli = Resolve-RepoPath $CliPath
  $resolvedConfig = Resolve-RepoPath $ConfigPath

  if (-not (Test-Path $resolvedCli)) {
    $reason = "cli_not_found:$CliPath"
    if ($Required) {
      throw $reason
    }
    return New-UnavailableEngineResult $EngineName $ConfigPath @($reason)
  }
  if (-not (Test-Path $resolvedConfig)) {
    $reason = "config_not_found:$ConfigPath"
    if ($Required) {
      throw $reason
    }
    return New-UnavailableEngineResult $EngineName $ConfigPath @($reason)
  }

  Push-Location $RepoRoot
  try {
    $stdout = (& $resolvedCli --config $resolvedConfig --benchmark-core-only --engine $EngineName 2>&1 | Out-String)
    $exitCode = $LASTEXITCODE
  }
  finally {
    Pop-Location
  }

  if ($exitCode -ne 0) {
    $reason = "benchmark_failed:$EngineName exitCode=$exitCode"
    if ($Required) {
      throw "$reason`n$stdout"
    }
    return New-UnavailableEngineResult $EngineName $ConfigPath @($reason, $stdout.Trim())
  }

  try {
    $rawResult = ConvertFrom-BenchmarkStdout $stdout
    return Normalize-EngineResult $rawResult $EngineName $ConfigPath
  }
  catch {
    $reason = "benchmark_json_parse_failed:$($_.Exception.Message)"
    if ($Required) {
      throw "$reason`n$stdout"
    }
    return New-UnavailableEngineResult $EngineName $ConfigPath @($reason)
  }
}

function New-Comparison($EngineResults, $Compatibility) {
  $legacy = $EngineResults | Where-Object { $_.engine -eq "legacy" } | Select-Object -First 1
  $openvdb = $EngineResults | Where-Object { $_.engine -eq "openvdb-candidate" } | Select-Object -First 1
  $failureReasons = @()

  $legacyMs = $null
  if ($null -ne $legacy -and $legacy.available) {
    $legacyMs = [double]$legacy.timingsMs.coreCompute
  }

  $openvdbMs = $null
  if ($null -ne $openvdb -and $openvdb.available) {
    $openvdbMs = [double]$openvdb.timingsMs.coreCompute
  }

  $ratio = $null
  if ($null -ne $legacyMs -and $legacyMs -gt 0.0 -and $null -ne $openvdbMs) {
    $ratio = $openvdbMs / $legacyMs
  }

  if ($null -eq $legacy -or -not $legacy.available) {
    $failureReasons += "legacy_unavailable"
  }
  if ($null -eq $openvdb) {
    $failureReasons += "openvdb_not_requested"
  }
  elseif (-not $openvdb.available) {
    $failureReasons += "openvdb_unavailable"
    $failureReasons += $openvdb.replacementGate.failureReasons
  }
  elseif (-not [bool]$openvdb.replacementGate.outputSemanticsComparable) {
    $failureReasons += "openvdb_output_semantics_not_comparable"
    $failureReasons += $openvdb.replacementGate.failureReasons
  }
  if (-not [bool]$Compatibility.samePose) {
    $failureReasons += "same_pose_false:$($Compatibility.samePoseReason)"
  }
  if (-not [bool]$Compatibility.sameResolution) {
    $failureReasons += "same_resolution_false:$($Compatibility.sameResolutionReason)"
  }

  $outputSemanticsComparable =
    ($null -ne $legacy -and $legacy.available -and [bool]$legacy.replacementGate.outputSemanticsComparable) -and
    ($null -ne $openvdb -and $openvdb.available -and [bool]$openvdb.replacementGate.outputSemanticsComparable)
  $performanceComparable =
    $outputSemanticsComparable -and
    [bool]$Compatibility.samePose -and
    [bool]$Compatibility.sameResolution -and
    ($null -ne $ratio)
  $replacementPass = $outputSemanticsComparable -and $performanceComparable -and ($ratio -le 1.0)

  return [ordered]@{
    legacyCoreComputeMs = $legacyMs
    openVdbCoreComputeMs = $openvdbMs
    openVdbToLegacyCoreRatio = $ratio
    outputSemanticsComparable = $outputSemanticsComparable
    performanceComparable = $performanceComparable
    replacementPass = $replacementPass
    failureReasons = @($failureReasons | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
  }
}

if ([string]::IsNullOrWhiteSpace($LegacyCli)) {
  $LegacyCli = "build\$BuildType\slicer_cli.exe"
}
if ([string]::IsNullOrWhiteSpace($OpenVdbCli)) {
  $OpenVdbCli = "build-openvdb-09p\$BuildType\slicer_cli.exe"
}

$engineResults = @()
if ($Engine -eq "legacy" -or $Engine -eq "all") {
  $engineResults += Invoke-CoreBenchmark $LegacyCli $LegacyConfig "legacy" $true
}
if ($Engine -eq "openvdb" -or $Engine -eq "all") {
  $engineResults += Invoke-CoreBenchmark $OpenVdbCli $OpenVdbConfig "openvdb-candidate" $false
}

$legacyConfigSummary = Read-ConfigSummary $LegacyConfig
$openvdbConfigSummary = Read-ConfigSummary $OpenVdbConfig
$compatibility = Test-ConfigCompatibility $legacyConfigSummary $openvdbConfigSummary $Engine
$comparison = New-Comparison $engineResults $compatibility

$report = [ordered]@{
  schema = "slicesoft.benchmark.12b.1"
  generatedAt = (Get-Date).ToString("o")
  caseName = $CaseName
  buildType = $BuildType
  samePose = $compatibility.samePose
  samePoseReason = $compatibility.samePoseReason
  sameResolution = $compatibility.sameResolution
  sameResolutionReason = $compatibility.sameResolutionReason
  sameSemanticsRequested = $true
  outputPolicy = [ordered]@{
    writeTiff = $false
    writePreview = $false
    writeReports = "benchmark_stdout_only"
    publishPackage = $false
    noImageWrite = [bool]$NoImageWrite
  }
  environment = [ordered]@{
    os = [System.Environment]::OSVersion.Platform.ToString()
    compiler = "MSVC"
    cpu = $env:PROCESSOR_IDENTIFIER
    buildDir = if ($Engine -eq "openvdb") { "build-openvdb-09p" } else { "build" }
    openVdbEnabled = ($Engine -eq "openvdb" -or $Engine -eq "all")
    vcpkgRoot = $env:VCPKG_ROOT
  }
  inputs = [ordered]@{
    modelPath = if ($legacyConfigSummary.available) { $legacyConfigSummary.modelPath } else { $openvdbConfigSummary.modelPath }
    legacyConfig = $LegacyConfig
    openVdbConfig = $OpenVdbConfig
    legacyCli = $LegacyCli
    openVdbCli = $OpenVdbCli
    layerThicknessMm = if ($legacyConfigSummary.available) { $legacyConfigSummary.layerThicknessMm } else { $openvdbConfigSummary.layerThicknessMm }
    dpiX = if ($legacyConfigSummary.available) { $legacyConfigSummary.dpiX } else { $openvdbConfigSummary.dpiX }
    dpiY = if ($legacyConfigSummary.available) { $legacyConfigSummary.dpiY } else { $openvdbConfigSummary.dpiY }
    transform = if ($legacyConfigSummary.available) { $legacyConfigSummary.transform } else { $openvdbConfigSummary.transform }
  }
  engines = $engineResults
  comparison = $comparison
  decision = [ordered]@{
    recommendedProductionEngine = "legacy"
    openVdbRole = if ($comparison.outputSemanticsComparable) { "candidate_requires_performance_gate" } else { "sdf_utility_candidate" }
    nextStage = "12B-R0 release baseline"
    notes = @(
      "Core-only benchmark disables TIFF, preview, reports, and package publishing.",
      "OpenVDB replacement remains false unless output semantics and performance are both comparable."
    )
  }
}

$outputPath = Resolve-RepoPath $Output
$outputDirectory = Split-Path -Parent $outputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
  New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$jsonText = $report | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($outputPath, $jsonText, [System.Text.UTF8Encoding]::new($false))

Write-Host "12B benchmark report: $Output"
foreach ($result in $engineResults) {
  if ($result.available) {
    Write-Host ("{0} coreComputeMs={1:N3}" -f $result.engine, [double]$result.timingsMs.coreCompute)
  }
  else {
    Write-Host ("{0} unavailable: {1}" -f $result.engine, ($result.replacementGate.failureReasons -join "; "))
  }
}
