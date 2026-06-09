param(
  [string]$Root = "tests/packages/bad_3mf"
)

$ErrorActionPreference = "Stop"

function Run-ExpectError([string]$Name, [string]$Expected) {
  Write-Host "== bad 3MF $Name"
  $config = Join-Path $Root "$Name/config.json"
  $process = New-Object System.Diagnostics.Process
  $process.StartInfo.FileName = ".\build\Debug\slicer_cli.exe"
  $process.StartInfo.Arguments = "--config `"$config`""
  $process.StartInfo.RedirectStandardOutput = $true
  $process.StartInfo.RedirectStandardError = $true
  $process.StartInfo.UseShellExecute = $false
  $process.StartInfo.WorkingDirectory = (Get-Location).Path
  [void]$process.Start()
  $stdout = $process.StandardOutput.ReadToEnd()
  $stderr = $process.StandardError.ReadToEnd()
  $process.WaitForExit()
  $output = $stdout + $stderr
  if ($process.ExitCode -eq 0) {
    throw "$Name expected failure but slicer succeeded"
  }
  if ($output -notlike "*$Expected*") {
    throw "$Name expected error containing $Expected but got: $output"
  }
  Write-Host "PASS expected-error $Expected"
}

function Run-ExpectWarning([string]$Name, [scriptblock]$Verify) {
  Write-Host "== bad 3MF warning/fallback $Name"
  $config = Join-Path $Root "$Name/config.json"
  & .\build\Debug\slicer_cli.exe --config $config
  if ($LASTEXITCODE -ne 0) {
    throw "$Name expected warning/fallback success"
  }
  & $Verify
  Write-Host "PASS warning/fallback $Name"
}

Run-ExpectError "bad_3mf_missing_content_types" "E_3MF_CONTENT_TYPES_MISSING"
Run-ExpectWarning "bad_3mf_missing_rels" {
  $report = Get-Content -Raw "output/bad_3mf_missing_rels/reports/three_mf_report.json" | ConvertFrom-Json
  if (($report.warnings -join " ") -notlike "*E_3MF_RELS_MISSING*") { throw "missing rels warning not recorded" }
}
Run-ExpectError "bad_3mf_missing_model_part" "E_3MF_MODEL_PART_MISSING"
Run-ExpectError "bad_3mf_xml_parse_failed" "E_3MF_XML_PARSE_FAILED"
Run-ExpectError "bad_3mf_path_traversal" "E_3MF_ZIP_PATH_TRAVERSAL"
Run-ExpectError "bad_3mf_too_many_entries" "E_3MF_ZIP_TOO_MANY_ENTRIES"
Run-ExpectError "bad_3mf_too_large_uncompressed" "E_3MF_ZIP_TOO_LARGE"
Run-ExpectWarning "bad_3mf_unknown_material_id" {
  $report = Get-Content -Raw "output/bad_3mf_unknown_material_id/reports/three_mf_report.json" | ConvertFrom-Json
  if ($report.validation.unknownMaterialCount -le 0) { throw "unknown material count not recorded" }
  if (($report.warnings -join " ") -notlike "*E_3MF_UNKNOWN_MATERIAL_ID*") { throw "unknown material warning not recorded" }
}
Run-ExpectError "bad_3mf_invalid_component_reference" "E_3MF_INVALID_COMPONENT_REFERENCE"
Run-ExpectError "bad_3mf_invalid_triangle_indices" "E_3MF_INVALID_TRIANGLE_INDEX"
Run-ExpectError "bad_3mf_unsupported_unit" "E_3MF_UNSUPPORTED_UNIT"
Run-ExpectWarning "bad_3mf_texture_path_missing" {
  $report = Get-Content -Raw "output/bad_3mf_texture_path_missing/reports/three_mf_report.json" | ConvertFrom-Json
  $texture = Get-Content -Raw "output/bad_3mf_texture_path_missing/reports/texture_report.json" | ConvertFrom-Json
  if ($report.textures.missingCount -le 0) { throw "missing texture path not recorded in three_mf_report" }
  if (($report.warnings -join " ") -notlike "*E_3MF_TEXTURE_PATH_MISSING*") { throw "missing texture path warning not recorded" }
  if ($texture.missingTextures -le 0) { throw "missing texture path not recorded in texture_report" }
}
Run-ExpectWarning "bad_3mf_texture_decode_failed" {
  $texture = Get-Content -Raw "output/bad_3mf_texture_decode_failed/reports/texture_report.json" | ConvertFrom-Json
  if ($texture.missingTextures -le 0) { throw "texture decode failure not recorded" }
  if (($texture.warnings -join " ") -notlike "*texture decode failed*") { throw "texture decode warning not recorded" }
}
Run-ExpectError "bad_3mf_texture2dgroup_missing_texid" "E_3MF_TEXTURE2DGROUP_MISSING_TEXID"
Run-ExpectError "bad_3mf_tex2coord_index_out_of_range" "E_3MF_TEX2COORD_INDEX_OUT_OF_RANGE"
Run-ExpectError "bad_3mf_colorgroup_index_out_of_range" "E_3MF_COLORGROUP_INDEX_OUT_OF_RANGE"
Run-ExpectWarning "bad_3mf_unsupported_compositematerials" {
  $report = Get-Content -Raw "output/bad_3mf_unsupported_compositematerials/reports/three_mf_report.json" | ConvertFrom-Json
  if (($report.unsupportedResources -join " ") -notlike "*compositematerials*") { throw "unsupported compositematerials not recorded" }
}
Run-ExpectWarning "bad_3mf_unsupported_multiproperties" {
  $report = Get-Content -Raw "output/bad_3mf_unsupported_multiproperties/reports/three_mf_report.json" | ConvertFrom-Json
  if (($report.unsupportedResources -join " ") -notlike "*multiproperties*") { throw "unsupported multiproperties not recorded" }
}

Write-Host "3MF negative tests complete."
