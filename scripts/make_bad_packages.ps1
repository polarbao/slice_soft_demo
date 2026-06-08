param(
  [string]$GoodPackage = "output/SlicePackage",
  [string]$OutputRoot = "tests/packages/bad"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Path) {
  if ([System.IO.Path]::IsPathRooted($Path)) {
    return [System.IO.Path]::GetFullPath($Path)
  }
  return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Copy-Package([string]$Name) {
  $source = Resolve-RepoPath $GoodPackage
  if (-not (Test-Path $source)) {
    throw "Good package does not exist: $source"
  }
  $root = Resolve-RepoPath $OutputRoot
  $target = Join-Path $root $Name
  if (-not $target.StartsWith($root)) {
    throw "Refusing to write outside output root: $target"
  }
  if (Test-Path $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path $root | Out-Null
  Copy-Item -LiteralPath $source -Destination $target -Recurse
  return $target
}

function Read-Manifest([string]$Package) {
  return Get-Content -Raw -Path (Join-Path $Package "manifest.json") | ConvertFrom-Json
}

function Write-Manifest([string]$Package, $Manifest) {
  $json = $Manifest | ConvertTo-Json -Depth 100
  $path = Join-Path $Package "manifest.json"
  $encoding = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($path, $json, $encoding)
}

function Get-FirstLayerPath($Manifest) {
  $layers = $Manifest.layers
  if ($null -eq $layers) {
    $layers = $Manifest.tiff.layers
  }
  if ($null -eq $layers -or $layers.Count -eq 0) {
    throw "Manifest does not contain layers"
  }
  return $layers[0].path
}

function Set-TiffShortTagValue([string]$TiffPath, [int]$Tag, [int]$Value) {
  $bytes = [System.IO.File]::ReadAllBytes($TiffPath)
  if ($bytes.Length -lt 8 -or $bytes[0] -ne 0x49 -or $bytes[1] -ne 0x49) {
    throw "Only little-endian TIFF is supported for bad package patching: $TiffPath"
  }
  $ifdOffset = [BitConverter]::ToUInt32($bytes, 4)
  $entryCount = [BitConverter]::ToUInt16($bytes, $ifdOffset)
  for ($i = 0; $i -lt $entryCount; $i++) {
    $entryOffset = $ifdOffset + 2 + ($i * 12)
    $entryTag = [BitConverter]::ToUInt16($bytes, $entryOffset)
    if ($entryTag -eq $Tag) {
      $valueBytes = [BitConverter]::GetBytes([UInt16]$Value)
      $bytes[$entryOffset + 8] = $valueBytes[0]
      $bytes[$entryOffset + 9] = $valueBytes[1]
      [System.IO.File]::WriteAllBytes($TiffPath, $bytes)
      return
    }
  }
  throw "TIFF tag not found: $Tag in $TiffPath"
}

$cases = @()

$pkg = Copy-Package "bad_missing_manifest"
Remove-Item -LiteralPath (Join-Path $pkg "manifest.json") -Force
$cases += "bad_missing_manifest"

$pkg = Copy-Package "bad_manifest_parse"
Set-Content -Path (Join-Path $pkg "manifest.json") -Value "{ bad json" -Encoding UTF8
$cases += "bad_manifest_parse"

$pkg = Copy-Package "bad_schema"
$m = Read-Manifest $pkg
$m.schema = "bad.schema"
Write-Manifest $pkg $m
$cases += "bad_schema"

$pkg = Copy-Package "bad_bit_depth"
$m = Read-Manifest $pkg
$m.tiff.bitDepth = 16
Write-Manifest $pkg $m
$cases += "bad_bit_depth"

$pkg = Copy-Package "bad_channel_order"
$m = Read-Manifest $pkg
$m.tiff.channelOrder[0] = "B"
Write-Manifest $pkg $m
$cases += "bad_channel_order"

$pkg = Copy-Package "bad_channel_count"
$m = Read-Manifest $pkg
$m.tiff.channelCount = 5
Write-Manifest $pkg $m
$cases += "bad_channel_count"

$pkg = Copy-Package "bad_polarity"
$m = Read-Manifest $pkg
$m.tiff.polarity = "white_is_print"
Write-Manifest $pkg $m
$cases += "bad_polarity"

$pkg = Copy-Package "bad_print_value"
$m = Read-Manifest $pkg
$m.tiff.printValue = 1
Write-Manifest $pkg $m
$cases += "bad_print_value"

$pkg = Copy-Package "bad_empty_value"
$m = Read-Manifest $pkg
$m.tiff.emptyValue = 0
Write-Manifest $pkg $m
$cases += "bad_empty_value"

$pkg = Copy-Package "bad_grid"
$m = Read-Manifest $pkg
$m.grid.widthPx = 0
Write-Manifest $pkg $m
$cases += "bad_grid"

$pkg = Copy-Package "bad_missing_layer"
$m = Read-Manifest $pkg
$layerPath = Get-FirstLayerPath $m
Remove-Item -LiteralPath (Join-Path $pkg $layerPath) -Force
$cases += "bad_missing_layer"

$pkg = Copy-Package "bad_layer_size"
$m = Read-Manifest $pkg
if ($null -ne $m.layers) {
  $m.layers[0].widthPx = $m.grid.widthPx + 1
}
if ($null -ne $m.tiff.layers) {
  $m.tiff.layers[0].widthPx = $m.grid.widthPx + 1
}
Write-Manifest $pkg $m
$cases += "bad_layer_size"

$pkg = Copy-Package "bad_samples_per_pixel"
$m = Read-Manifest $pkg
$layerPath = Get-FirstLayerPath $m
Set-TiffShortTagValue (Join-Path $pkg $layerPath) 277 5
$cases += "bad_samples_per_pixel"

$pkg = Copy-Package "bad_planar_config"
$m = Read-Manifest $pkg
$layerPath = Get-FirstLayerPath $m
Set-TiffShortTagValue (Join-Path $pkg $layerPath) 284 2
$cases += "bad_planar_config"

$pkg = Copy-Package "bad_storage_mode"
$m = Read-Manifest $pkg
$m.tiff.storageMode = "chunked"
$m.tiff.storage = "chunked"
Write-Manifest $pkg $m
$cases += "bad_storage_mode"

$pkg = Copy-Package "bad_rows_per_strip"
$m = Read-Manifest $pkg
$m.tiff.storageMode = "stripped"
$m.tiff.storage = "stripped"
$m.tiff.tiled = $false
$m.tiff.rowsPerStrip = 0
Write-Manifest $pkg $m
$cases += "bad_rows_per_strip"

$pkg = Copy-Package "bad_tiff_storage_mismatch"
$m = Read-Manifest $pkg
$m.tiff.storageMode = "tiled"
$m.tiff.storage = "tiled"
$m.tiff.tiled = $true
$m.tiff | Add-Member -NotePropertyName tileSize -NotePropertyValue @(256, 256) -Force
Write-Manifest $pkg $m
$cases += "bad_tiff_storage_mismatch"

$pkg = Copy-Package "bad_tile_size"
$m = Read-Manifest $pkg
$m.tiff.storageMode = "tiled"
$m.tiff.storage = "tiled"
$m.tiff.tiled = $true
$m.tiff | Add-Member -NotePropertyName tileSize -NotePropertyValue @(0, 256) -Force
Write-Manifest $pkg $m
$cases += "bad_tile_size"

Write-Host "Generated bad packages:"
$cases | ForEach-Object { Write-Host "  $_" }
