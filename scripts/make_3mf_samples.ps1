param()

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Copy-ZipWithCompression([string]$Source, [string]$Destination, [System.IO.Compression.CompressionLevel]$Level) {
  if (Test-Path $Destination) {
    Remove-Item -LiteralPath $Destination -Force
  }
  $sourceArchive = [System.IO.Compression.ZipFile]::OpenRead($Source)
  try {
    $destArchive = [System.IO.Compression.ZipFile]::Open($Destination, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
      foreach ($entry in $sourceArchive.Entries) {
        $newEntry = $destArchive.CreateEntry($entry.FullName, $Level)
        $inputStream = $entry.Open()
        try {
          $outputStream = $newEntry.Open()
          try {
            $inputStream.CopyTo($outputStream)
          } finally {
            $outputStream.Dispose()
          }
        } finally {
          $inputStream.Dispose()
        }
      }
    } finally {
      $destArchive.Dispose()
    }
  } finally {
    $sourceArchive.Dispose()
  }
}

New-Item -ItemType Directory -Force "samples/models/3mf" | Out-Null

Copy-Item `
  -LiteralPath "samples/models/3mf/single_rgb_cube.3mf" `
  -Destination "samples/models/3mf/single_rgb_cube_stored.3mf" `
  -Force

Copy-ZipWithCompression `
  -Source "samples/models/3mf/single_rgb_cube.3mf" `
  -Destination "samples/models/3mf/single_rgb_cube_deflate.3mf" `
  -Level ([System.IO.Compression.CompressionLevel]::Optimal)

Copy-ZipWithCompression `
  -Source "samples/models/3mf/multi_material_rgb_white_varnish.3mf" `
  -Destination "samples/models/3mf/multi_material_rgb_white_varnish_deflate.3mf" `
  -Level ([System.IO.Compression.CompressionLevel]::Optimal)

Write-Host "Generated 06A 3MF samples."
