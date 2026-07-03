param(
  [string]$LegacyCli = ".\build\Debug\slicer_cli.exe",
  [string]$OpenVdbCli = ".\build-openvdb-09p\Debug\slicer_cli.exe",
  [string]$LegacyConfig = "samples\configs\slice_config.json",
  [string]$OpenVdbConfig = "samples\configs\openvdb_candidate\closed_textured_obj_candidate.json",
  [string]$Output = "output\benchmarks\openvdb_legacy_core_benchmark_11b.json"
)

$ErrorActionPreference = "Stop"

function Run-CoreBenchmark([string]$Cli, [string]$Config, [string]$Engine) {
  if (-not (Test-Path $Cli)) {
    throw "CLI not found: $Cli"
  }
  if (-not (Test-Path $Config)) {
    throw "Config not found: $Config"
  }

  $jsonText = (& $Cli --config $Config --benchmark-core-only --engine $Engine | Out-String)
  if ($LASTEXITCODE -ne 0) {
    throw "benchmark failed: engine=$Engine cli=$Cli config=$Config"
  }
  return $jsonText | ConvertFrom-Json
}

function To-Hashtable($Value) {
  if ($null -eq $Value) {
    return $null
  }
  if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string]) -and -not ($Value -is [pscustomobject])) {
    $items = @()
    foreach ($item in $Value) {
      $items += To-Hashtable $item
    }
    return $items
  }
  if ($Value -is [pscustomobject]) {
    $table = [ordered]@{}
    foreach ($property in $Value.PSObject.Properties) {
      $table[$property.Name] = To-Hashtable $property.Value
    }
    return $table
  }
  return $Value
}

$legacy = Run-CoreBenchmark $LegacyCli $LegacyConfig "legacy"
$openvdb = Run-CoreBenchmark $OpenVdbCli $OpenVdbConfig "openvdb-candidate"

$legacyMs = [double]$legacy.timingsMs.coreCompute
$openvdbMs = [double]$openvdb.timingsMs.coreCompute
$ratio = if ($legacyMs -gt 0.0) { $openvdbMs / $legacyMs } else { $null }
$semanticsComparable = [bool]$legacy.replacementGate.outputSemanticsComparable -and [bool]$openvdb.replacementGate.outputSemanticsComparable

$report = [ordered]@{
  schema = "p0.openvdb_legacy_core_benchmark.1"
  generatedAt = (Get-Date).ToString("o")
  outputPolicy = [ordered]@{
    writeTiff = $false
    writePreview = $false
    writeReports = "benchmark_stdout_only"
    publishPackage = $false
  }
  inputs = [ordered]@{
    legacyCli = $LegacyCli
    openvdbCli = $OpenVdbCli
    legacyConfig = $LegacyConfig
    openvdbConfig = $OpenVdbConfig
  }
  engines = [ordered]@{
    legacy = To-Hashtable $legacy
    openvdbCandidate = To-Hashtable $openvdb
  }
  comparison = [ordered]@{
    legacyCoreComputeMs = $legacyMs
    openvdbCoreComputeMs = $openvdbMs
    openvdbToLegacyCoreRatio = $ratio
    outputSemanticsComparable = $semanticsComparable
    replacementPass = $false
    reason = if ($semanticsComparable) { "performance gate is informational in 11B" } else { "output semantics are not comparable" }
  }
}

$outputPath = if ([System.IO.Path]::IsPathRooted($Output)) { $Output } else { Join-Path (Get-Location) $Output }
$outputDirectory = Split-Path -Parent $outputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
  New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$text = $report | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($outputPath, $text, [System.Text.UTF8Encoding]::new($false))
Write-Host "benchmark report: $Output"
Write-Host ("legacy coreComputeMs={0:N3}" -f $legacyMs)
Write-Host ("openvdb coreComputeMs={0:N3}" -f $openvdbMs)
if ($null -ne $ratio) {
  Write-Host ("openvdb/legacy ratio={0:N3}" -f $ratio)
}
