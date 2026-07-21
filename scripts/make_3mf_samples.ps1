param()

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$fixedEntryTime = [DateTimeOffset]::Parse("2026-06-29T16:53:58+08:00")

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
        $newEntry.LastWriteTime = $fixedEntryTime
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

function New-ZipPackage([string]$Path, [hashtable]$Entries, [System.IO.Compression.CompressionLevel]$Level) {
  if (Test-Path $Path) {
    Remove-Item -LiteralPath $Path -Force
  }
  $archive = [System.IO.Compression.ZipFile]::Open($Path, [System.IO.Compression.ZipArchiveMode]::Create)
  try {
    foreach ($name in $Entries.Keys) {
      $entry = $archive.CreateEntry($name, $Level)
      $entry.LastWriteTime = $fixedEntryTime
      $stream = $entry.Open()
      try {
        if ($Entries[$name] -is [byte[]]) {
          $bytes = $Entries[$name]
        } else {
          $bytes = [System.Text.Encoding]::UTF8.GetBytes($Entries[$name])
        }
        $stream.Write($bytes, 0, $bytes.Length)
      } finally {
        $stream.Dispose()
      }
    }
  } finally {
    $archive.Dispose()
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

$contentTypes = @"
<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
  <Default Extension="png" ContentType="image/png"/>
</Types>
"@

$rels = @"
<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Target="3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>
</Relationships>
"@

$vertices = @"
          <vertex x="0" y="0" z="0"/>
          <vertex x="3" y="0" z="0"/>
          <vertex x="3" y="3" z="0"/>
          <vertex x="0" y="3" z="0"/>
          <vertex x="0" y="0" z="0.5"/>
          <vertex x="3" y="0" z="0.5"/>
          <vertex x="3" y="3" z="0.5"/>
          <vertex x="0" y="3" z="0.5"/>
"@

$colorTriangles = @"
          <triangle v1="0" v2="2" v3="1" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="0" v2="3" v3="2" pid="20" p1="1" p2="1" p3="1"/>
          <triangle v1="4" v2="5" v3="6" pid="20" p1="0" p2="1" p3="2"/>
          <triangle v1="4" v2="6" v3="7" pid="20" p1="2" p2="2" p3="2"/>
          <triangle v1="0" v2="1" v3="5" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="0" v2="5" v3="4" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="1" v2="2" v3="6" pid="20" p1="1" p2="1" p3="1"/>
          <triangle v1="1" v2="6" v3="5" pid="20" p1="1" p2="1" p3="1"/>
          <triangle v1="2" v2="3" v3="7" pid="20" p1="2" p2="2" p3="2"/>
          <triangle v1="2" v2="7" v3="6" pid="20" p1="2" p2="2" p3="2"/>
          <triangle v1="3" v2="0" v3="4" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="3" v2="4" v3="7" pid="20" p1="0" p2="0" p3="0"/>
"@

$textureTriangles = @"
          <triangle v1="0" v2="2" v3="1" pid="31" p1="0" p2="2" p3="1"/>
          <triangle v1="0" v2="3" v3="2" pid="31" p1="0" p2="3" p3="2"/>
          <triangle v1="4" v2="5" v3="6" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="4" v2="6" v3="7" pid="31" p1="0" p2="2" p3="3"/>
          <triangle v1="0" v2="1" v3="5" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="0" v2="5" v3="4" pid="31" p1="0" p2="2" p3="3"/>
          <triangle v1="1" v2="2" v3="6" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="1" v2="6" v3="5" pid="31" p1="0" p2="2" p3="3"/>
          <triangle v1="2" v2="3" v3="7" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="2" v2="7" v3="6" pid="31" p1="0" p2="2" p3="3"/>
          <triangle v1="3" v2="0" v3="4" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="3" v2="4" v3="7" pid="31" p1="0" p2="2" p3="3"/>
"@

$mixedTriangles = @"
          <triangle v1="0" v2="2" v3="1" pid="1" p1="0"/>
          <triangle v1="0" v2="3" v3="2" pid="1" p1="0"/>
          <triangle v1="4" v2="5" v3="6" pid="31" p1="0" p2="1" p3="2"/>
          <triangle v1="4" v2="6" v3="7" pid="31" p1="0" p2="2" p3="3"/>
          <triangle v1="0" v2="1" v3="5" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="0" v2="5" v3="4" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="1" v2="2" v3="6" pid="20" p1="1" p2="1" p3="1"/>
          <triangle v1="1" v2="6" v3="5" pid="20" p1="1" p2="1" p3="1"/>
          <triangle v1="2" v2="3" v3="7" pid="20" p1="2" p2="2" p3="2"/>
          <triangle v1="2" v2="7" v3="6" pid="20" p1="2" p2="2" p3="2"/>
          <triangle v1="3" v2="0" v3="4" pid="20" p1="0" p2="0" p3="0"/>
          <triangle v1="3" v2="4" v3="7" pid="20" p1="0" p2="0" p3="0"/>
"@

function New-ModelXml([string]$Resources, [string]$Triangles) {
  return @"
<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">
  <resources>
$Resources
    <object id="2" type="model">
      <mesh>
        <vertices>
$vertices
        </vertices>
        <triangles>
$Triangles
        </triangles>
      </mesh>
    </object>
  </resources>
  <build>
    <item objectid="2"/>
  </build>
</model>
"@
}

$colorResources = @"
    <colorgroup id="20">
      <color color="#D82525"/>
      <color color="#25A647"/>
      <color color="#2557D8"/>
    </colorgroup>
"@

$textureResources = @"
    <texture2d id="30" path="/3D/Textures/checker.png" contenttype="image/png"/>
    <texture2dgroup id="31" texid="30">
      <tex2coord u="0" v="0"/>
      <tex2coord u="1" v="0"/>
      <tex2coord u="1" v="1"/>
      <tex2coord u="0" v="1"/>
    </texture2dgroup>
"@

$baseResources = @"
    <basematerials id="1">
      <base name="base_rgb" displaycolor="#4466AA"/>
    </basematerials>
"@

$checkerBytes = [byte[]](Get-Content -LiteralPath "samples/models/textured/textures/checker.png" -Encoding Byte)

New-ZipPackage `
  -Path "samples/models/3mf/color_group_cube.3mf" `
  -Level ([System.IO.Compression.CompressionLevel]::Optimal) `
  -Entries @{
    "[Content_Types].xml" = $contentTypes
    "_rels/.rels" = $rels
    "3D/3dmodel.model" = (New-ModelXml -Resources $colorResources -Triangles $colorTriangles)
  }

New-ZipPackage `
  -Path "samples/models/3mf/texture2d_checker_cube.3mf" `
  -Level ([System.IO.Compression.CompressionLevel]::Optimal) `
  -Entries @{
    "[Content_Types].xml" = $contentTypes
    "_rels/.rels" = $rels
    "3D/3dmodel.model" = (New-ModelXml -Resources $textureResources -Triangles $textureTriangles)
    "3D/Textures/checker.png" = $checkerBytes
  }

New-ZipPackage `
  -Path "samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf" `
  -Level ([System.IO.Compression.CompressionLevel]::Optimal) `
  -Entries @{
    "[Content_Types].xml" = $contentTypes
    "_rels/.rels" = $rels
    "3D/3dmodel.model" = (New-ModelXml -Resources ($baseResources + $colorResources + $textureResources) -Triangles $mixedTriangles)
    "3D/Textures/checker.png" = $checkerBytes
  }

Write-Host "Generated 06A/06B 3MF samples."
