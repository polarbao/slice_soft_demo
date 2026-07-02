param(
  [switch]$Build,
  [switch]$Output,
  [switch]$All,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Path) {
  return [System.IO.Path]::GetFullPath((Join-Path $PWD $Path))
}

function Assert-PathInsideRepo([string]$TargetPath) {
  $repoRoot = [System.IO.Path]::GetFullPath($PWD.Path).TrimEnd('\', '/')
  $resolved = [System.IO.Path]::GetFullPath($TargetPath).TrimEnd('\', '/')
  if (-not $resolved.StartsWith($repoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "refuse to clean path outside repo: $resolved"
  }
  return $resolved
}

function Remove-LocalPath([string]$Label, [string]$RelativePath) {
  $target = Assert-PathInsideRepo (Resolve-RepoPath $RelativePath)
  if (-not (Test-Path -LiteralPath $target)) {
    Write-Host "skip $Label, not found: $target"
    return
  }

  if ($DryRun) {
    Write-Host "[dry-run] remove ${Label}: $target"
    return
  }

  Write-Host "remove ${Label}: $target"
  Remove-Item -LiteralPath $target -Recurse -Force
}

if (-not ($Build -or $Output -or $All)) {
  throw "choose at least one of -Build, -Output, or -All"
}

if ($All -or $Build) {
  Remove-LocalPath "build directory" "build"
}

if ($All -or $Output) {
  Remove-LocalPath "output directory" "output"
}

Write-Host "Local environment clean complete."
