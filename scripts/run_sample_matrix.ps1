param(
  [string]$Registry = "samples/scenarios/slicer_scenarios.json",
  [string]$ScenarioId = "",
  [string]$Category = "",
  [switch]$IncludeExperimental,
  [switch]$SkipBuild,
  [switch]$SkipRip,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Path) {
  if ([System.IO.Path]::IsPathRooted($Path)) {
    return [System.IO.Path]::GetFullPath($Path)
  }
  return [System.IO.Path]::GetFullPath((Join-Path $PWD $Path))
}

function Run-Step([string]$Name, [scriptblock]$Block) {
  Write-Host "== $Name"
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  try {
    & $Block
    $timer.Stop()
    Write-Host ("PASS {0:N2}s" -f $timer.Elapsed.TotalSeconds)
  } catch {
    $timer.Stop()
    Write-Host ("FAIL {0:N2}s: {1}" -f $timer.Elapsed.TotalSeconds, $_.Exception.Message)
    throw
  }
}

$registryPath = Resolve-RepoPath $Registry
if (-not (Test-Path -LiteralPath $registryPath)) {
  throw "scenario registry not found: $registryPath"
}

$registryJson = Get-Content -Raw -LiteralPath $registryPath | ConvertFrom-Json
if ($registryJson.schema -ne "slice_soft.scenarios.1") {
  throw "unsupported scenario registry schema: $($registryJson.schema)"
}

$scenarios = @($registryJson.scenarios | Where-Object {
  $_.enabled -eq $true `
    -and ($ScenarioId -eq "" -or $_.id -eq $ScenarioId) `
    -and ($Category -eq "" -or $_.category -eq $Category) `
    -and ($IncludeExperimental -or $_.experimental -ne $true)
})

if ($scenarios.Count -eq 0) {
  throw "no scenarios selected"
}

Write-Host "Selected scenarios: $($scenarios.Count)"

if (-not $SkipBuild) {
  Run-Step "build Debug" {
    cmake --build build --config Debug
    if ($LASTEXITCODE -ne 0) {
      throw "build failed"
    }
  }
}

foreach ($scenario in $scenarios) {
  $configPath = Resolve-RepoPath $scenario.configPath
  $packageDir = Resolve-RepoPath $scenario.packageDir
  $name = "$($scenario.category) / $($scenario.name)"

  if (-not (Test-Path -LiteralPath $configPath)) {
    throw "config not found for scenario $($scenario.id): $configPath"
  }

  if ($DryRun) {
    Write-Host "[dry-run] slicer $name -> $configPath"
  } else {
    Run-Step "slicer $name" {
      & .\build\Debug\slicer_cli.exe --config $configPath
      if ($LASTEXITCODE -ne 0) {
        throw "slicer failed: $configPath"
      }
    }
  }

  $shouldRunRip = (-not $SkipRip) -and ($scenario.ripSummary -ne $false)
  if ($shouldRunRip) {
    if ($DryRun) {
      Write-Host "[dry-run] rip $name -> $packageDir"
    } else {
      Run-Step "rip $name" {
        & .\build\Debug\rip_reader_test.exe --package $packageDir --summary
        if ($LASTEXITCODE -ne 0) {
          throw "rip_reader_test failed: $packageDir"
        }
      }
    }
  }
}

Write-Host "Sample matrix complete."
