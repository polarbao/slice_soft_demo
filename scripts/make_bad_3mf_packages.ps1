param(
  [string]$OutputRoot = "tests/packages/bad_3mf"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-ZipPackage([string]$Path, [hashtable]$Entries, [System.IO.Compression.CompressionLevel]$Level) {
  if (Test-Path $Path) {
    Remove-Item -LiteralPath $Path -Force
  }
  $archive = [System.IO.Compression.ZipFile]::Open($Path, [System.IO.Compression.ZipArchiveMode]::Create)
  try {
    foreach ($name in $Entries.Keys) {
      $entry = $archive.CreateEntry($name, $Level)
      $stream = $entry.Open()
      try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Entries[$name])
        $stream.Write($bytes, 0, $bytes.Length)
      } finally {
        $stream.Dispose()
      }
    }
  } finally {
    $archive.Dispose()
  }
}

function New-LargeZipPackage([string]$Path) {
  if (Test-Path $Path) {
    Remove-Item -LiteralPath $Path -Force
  }
  $archive = [System.IO.Compression.ZipFile]::Open($Path, [System.IO.Compression.ZipArchiveMode]::Create)
  try {
    $entry = $archive.CreateEntry("[Content_Types].xml", [System.IO.Compression.CompressionLevel]::Optimal)
    $stream = $entry.Open()
    try {
      $buffer = New-Object byte[] (1024 * 1024)
      for ($i = 0; $i -lt 129; ++$i) {
        $stream.Write($buffer, 0, $buffer.Length)
      }
    } finally {
      $stream.Dispose()
    }
  } finally {
    $archive.Dispose()
  }
}

function New-Config([string]$Dir, [string]$OutputName) {
  $config = @"
{
  "slicingMode": "relief_heightfield",
  "input": {
    "modelPath": "model.3mf",
    "format": "auto"
  },
  "output": {
    "packageDir": "output/$OutputName",
    "dpiX": 600,
    "dpiY": 600,
    "layerThicknessMm": 0.01,
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "bitDepth": 8,
    "planarConfig": "contiguous",
    "storageMode": "stripped",
    "rowsPerStrip": 64
  },
  "background": {
    "value": 255
  },
  "modelMaterial": {
    "materialChannel": "RGB",
    "applyMode": "solid_volume",
    "rgb": [0, 0, 0],
    "whiteValue": 255,
    "varnishValue": 255
  },
  "materialRoleMapping": {
    "enabled": true,
    "mode": "rules_then_default",
    "defaultRole": "rgb",
    "allowInputSupportMaterial": false,
    "rules": []
  },
  "support": {
    "enabled": false,
    "mode": "none",
    "value": 0,
    "offsetMm": 0.0,
    "minAreaPx": 0
  },
  "relief": {
    "fillMode": "intersection_range",
    "baseZMm": 0.0
  },
  "preview": {
    "enabled": false,
    "format": "png",
    "interval": 1,
    "channels": ["rgb"]
  }
}
"@
  Set-Content -LiteralPath (Join-Path $Dir "config.json") -Value $config -Encoding ASCII
}

$contentTypes = @"
<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
</Types>
"@

$rels = @"
<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Target="3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>
</Relationships>
"@

$modelGood = @"
<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">
  <resources>
    <basematerials id="1">
      <base name="color_body" displaycolor="#2266CC"/>
    </basematerials>
    <object id="2" type="model">
      <mesh>
        <vertices>
          <vertex x="0" y="0" z="0"/>
          <vertex x="1" y="0" z="0"/>
          <vertex x="0" y="1" z="0"/>
          <vertex x="0" y="0" z="1"/>
        </vertices>
        <triangles>
          <triangle v1="0" v2="1" v3="2" pid="1" p1="0"/>
          <triangle v1="0" v2="1" v3="3" pid="1" p1="0"/>
        </triangles>
      </mesh>
    </object>
  </resources>
  <build>
    <item objectid="2"/>
  </build>
</model>
"@

$cases = @(
  @{ Name = "bad_3mf_missing_content_types"; Entries = @{ "_rels/.rels" = $rels; "3D/3dmodel.model" = $modelGood } },
  @{ Name = "bad_3mf_missing_rels"; Entries = @{ "[Content_Types].xml" = $contentTypes; "3D/3dmodel.model" = $modelGood } },
  @{ Name = "bad_3mf_missing_model_part"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels } },
  @{ Name = "bad_3mf_xml_parse_failed"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = "<model><resources></model>" } },
  @{ Name = "bad_3mf_path_traversal"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = $modelGood; "../evil.txt" = "x" } },
  @{ Name = "bad_3mf_unknown_material_id"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = ($modelGood -replace 'pid="1" p1="0"', 'pid="99" p1="0"' -replace '<resources>', '<resources><colorgroup id="77"/>') } },
  @{ Name = "bad_3mf_invalid_component_reference"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = ($modelGood -replace '<object id="2" type="model">[\s\S]*?</object>', '<object id="3" type="model"><components><component objectid="404"/></components></object>' -replace '<item objectid="2"/>', '<item objectid="3"/>') } },
  @{ Name = "bad_3mf_invalid_triangle_indices"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = ($modelGood -replace 'v3="2"', 'v3="999"') } },
  @{ Name = "bad_3mf_unsupported_unit"; Entries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = ($modelGood -replace 'unit="millimeter"', 'unit="parsec"') } }
)

New-Item -ItemType Directory -Force $OutputRoot | Out-Null

foreach ($case in $cases) {
  $dir = Join-Path $OutputRoot $case.Name
  New-Item -ItemType Directory -Force $dir | Out-Null
  New-ZipPackage -Path (Join-Path $dir "model.3mf") -Entries $case.Entries -Level ([System.IO.Compression.CompressionLevel]::Optimal)
  New-Config -Dir $dir -OutputName $case.Name
}

$tooManyDir = Join-Path $OutputRoot "bad_3mf_too_many_entries"
New-Item -ItemType Directory -Force $tooManyDir | Out-Null
$tooManyEntries = @{ "[Content_Types].xml" = $contentTypes; "_rels/.rels" = $rels; "3D/3dmodel.model" = $modelGood }
for ($i = 0; $i -lt 257; ++$i) {
  $tooManyEntries["extra/file_$i.txt"] = "x"
}
New-ZipPackage -Path (Join-Path $tooManyDir "model.3mf") -Entries $tooManyEntries -Level ([System.IO.Compression.CompressionLevel]::Optimal)
New-Config -Dir $tooManyDir -OutputName "bad_3mf_too_many_entries"

$tooLargeDir = Join-Path $OutputRoot "bad_3mf_too_large_uncompressed"
New-Item -ItemType Directory -Force $tooLargeDir | Out-Null
New-LargeZipPackage -Path (Join-Path $tooLargeDir "model.3mf")
New-Config -Dir $tooLargeDir -OutputName "bad_3mf_too_large_uncompressed"

Write-Host "Generated bad 3MF packages in $OutputRoot"
