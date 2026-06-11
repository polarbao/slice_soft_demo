$ErrorActionPreference = "Stop"

function Assert-Equal($Actual, $Expected, [string]$Message) {
  if ($Actual -ne $Expected) {
    throw "$Message expected=$Expected actual=$Actual"
  }
}

function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) {
    throw $Message
  }
}

function Read-Json([string]$Path) {
  return Get-Content -Raw $Path | ConvertFrom-Json
}

function Run-GeometryCase([string]$CaseName, [string]$OutputDir, [string[]]$ExtraArgs = @()) {
  Write-Host "== geometry kernel $CaseName"
  $args = @("--case", $CaseName, "--output", $OutputDir) + $ExtraArgs
  & .\build\Debug\geometry_kernel_demo.exe @args
  if ($LASTEXITCODE -ne 0) {
    throw "geometry_kernel_demo failed: $CaseName"
  }

  $reportPath = Join-Path $OutputDir "reports/geometry_kernel_report.json"
  $previewPath = Join-Path $OutputDir "preview/$CaseName.png"
  Assert-True (Test-Path $reportPath) "$CaseName expected geometry_kernel_report.json"
  Assert-True (Test-Path $previewPath) "$CaseName expected preview PNG"

  $report = Read-Json $reportPath
  Assert-Equal $report.schema "p0.geometry_kernel_report.1" "$CaseName report schema mismatch"
  Assert-Equal $report.caseName $CaseName "$CaseName report caseName mismatch"
  Assert-True ($report.grid.widthPx -gt 0) "$CaseName expected widthPx"
  Assert-True ($report.grid.heightPx -gt 0) "$CaseName expected heightPx"
  Assert-True ($report.shellStats.shellPixels -gt 0) "$CaseName expected shell pixels"
  return $report
}

$null = Run-GeometryCase "heightfield-sdf" "output\GeometryKernelDemo"
$null = Run-GeometryCase "surface-shell" "output\GeometryKernelShell" @("--shell-mm", "0.05")
$openVdbReport = Run-GeometryCase "openvdb-smoke" "output\GeometryKernelOpenVdbStub"
Assert-Equal $openVdbReport.openvdb.enabled $false "openvdb-smoke default OFF expected disabled"

Write-Host "Geometry kernel tests complete."
