param(
  [string]$BuildDir = "build",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug",
  [string]$RunId = ""
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message)
{
  if (-not $Condition)
  {
    throw $Message
  }
}

function Assert-Equal($Actual, $Expected, [string]$Message)
{
  if ($Actual -ne $Expected)
  {
    throw "$Message expected=$Expected actual=$Actual"
  }
}

function Assert-ArrayEqual($Actual, $Expected, [string]$Message)
{
  $actualItems = @($Actual)
  $expectedItems = @($Expected)
  Assert-Equal $actualItems.Count $expectedItems.Count "$Message count"
  for ($index = 0; $index -lt $expectedItems.Count; ++$index)
  {
    Assert-Equal $actualItems[$index] $expectedItems[$index] "$Message[$index]"
  }
}

function Read-Json([string]$Path)
{
  Assert-True (Test-Path -LiteralPath $Path) "missing JSON file: $Path"
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
}

function Write-Utf8NoBom([string]$Path, [string]$Content)
{
  $encoding = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Get-ObjectProperty($Object, [string]$Name, $DefaultValue)
{
  if ($null -ne $Object -and $Object.PSObject.Properties.Name -contains $Name)
  {
    return $Object.$Name
  }
  return $DefaultValue
}

function Get-RelativePath([string]$BasePath, [string]$TargetPath)
{
  $baseFullPath = [System.IO.Path]::GetFullPath($BasePath).TrimEnd("\", "/") +
    [System.IO.Path]::DirectorySeparatorChar
  $targetFullPath = [System.IO.Path]::GetFullPath($TargetPath)
  $baseUri = New-Object System.Uri($baseFullPath)
  $targetUri = New-Object System.Uri($targetFullPath)
  return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString())
}

function Resolve-Executable([string]$BuildRoot, [string]$BuildConfig, [string]$Name)
{
  $candidates = @(
    (Join-Path $BuildRoot "$BuildConfig/$Name.exe"),
    (Join-Path $BuildRoot "$Name.exe")
  )
  foreach ($candidate in $candidates)
  {
    if (Test-Path -LiteralPath $candidate)
    {
      return (Resolve-Path -LiteralPath $candidate).Path
    }
  }
  throw "missing executable $Name under build directory: $BuildRoot"
}

function Invoke-External(
  [string]$Name,
  [string]$Executable,
  [string[]]$Arguments,
  [string]$LogPath)
{
  $previousErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try
  {
    $outputLines = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
  }
  finally
  {
    $ErrorActionPreference = $previousErrorActionPreference
  }
  $outputLines | Set-Content -Encoding UTF8 -LiteralPath $LogPath
  foreach ($line in $outputLines)
  {
    Write-Host $line
  }
  if ($exitCode -ne 0)
  {
    throw "$Name failed with exit code $exitCode; log=$LogPath"
  }
  return $outputLines
}

function Convert-TimingLine([string[]]$OutputLines)
{
  $timingLine = $OutputLines | Where-Object { $_ -like "SLICE_TIMING *" } | Select-Object -Last 1
  Assert-True (-not [string]::IsNullOrWhiteSpace($timingLine)) "missing SLICE_TIMING output"

  $timings = [ordered]@{}
  foreach ($match in [regex]::Matches($timingLine, "(?<key>[A-Za-z][A-Za-z0-9]*)=(?<value>[^\s]+)"))
  {
    $key = $match.Groups["key"].Value
    $value = $match.Groups["value"].Value
    if ($key -in @("engine", "profileLevel"))
    {
      $timings[$key] = $value
    }
    else
    {
      $timings[$key] = [double]::Parse(
        $value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    }
  }
  return [pscustomobject]$timings
}

function Assert-Protocol($Manifest, [string]$CaseId)
{
  Assert-Equal $Manifest.schema "p0.rgbwsv.2" "$CaseId schema"
  Assert-Equal $Manifest.schemaVersion "p0.rgbwsv.2" "$CaseId schemaVersion"
  Assert-ArrayEqual $Manifest.tiff.channelOrder @("R", "G", "B", "W", "S", "V") "$CaseId channelOrder"
  Assert-Equal $Manifest.tiff.bitDepth 8 "$CaseId bitDepth"
  Assert-Equal $Manifest.tiff.polarity "black_is_print" "$CaseId polarity"
  Assert-Equal $Manifest.tiff.printValue 0 "$CaseId printValue"
  Assert-Equal $Manifest.tiff.emptyValue 255 "$CaseId emptyValue"
}

function Resolve-PackageFile([string]$PackageRoot, [string]$RelativePath, [string]$CaseId)
{
  $packagePath = [System.IO.Path]::GetFullPath($PackageRoot)
  $filePath = [System.IO.Path]::GetFullPath((Join-Path $packagePath $RelativePath))
  $allowedPrefix = $packagePath.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
  Assert-True ($filePath.StartsWith($allowedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) "$CaseId layer path escapes package: $RelativePath"
  Assert-True (Test-Path -LiteralPath $filePath) "$CaseId missing layer file: $RelativePath"
  return $filePath
}

function Write-TiffHashManifest(
  $Manifest,
  [string]$PackageRoot,
  [string]$CaseId,
  [string]$OutputPath)
{
  $layers = @($Manifest.layers | Sort-Object { [int]$_.index })
  Assert-Equal $layers.Count $Manifest.grid.layerCount "$CaseId manifest layerCount"
  Assert-Equal @($layers | Group-Object index | Where-Object { $_.Count -ne 1 }).Count 0 "$CaseId duplicate layer index"

  $hashRows = foreach ($layer in $layers)
  {
    $layerPath = Resolve-PackageFile $PackageRoot $layer.path $CaseId
    [pscustomobject][ordered]@{
      layerIndex = [int]$layer.index
      path = [string]$layer.path
      sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $layerPath).Hash.ToLowerInvariant()
    }
  }
  $hashRows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath
  return $hashRows.Count
}

function Assert-ClosureReport($Report, [string]$CaseId)
{
  Assert-Equal $Report.schema "p0.material_closure.1" "$CaseId closure schema"
  Assert-Equal $Report.source "semantic_masks" "$CaseId closure source"
  Assert-Equal $Report.confidence "exact" "$CaseId closure confidence"
  Assert-True ($Report.closureStatus -in @("pass", "warning", "fail")) "$CaseId invalid closureStatus: $($Report.closureStatus)"
  Assert-Equal $Report.repair.enabled $false "$CaseId repair.enabled"
  Assert-Equal $Report.repair.attempted $false "$CaseId repair.attempted"
  Assert-Equal $Report.repair.repairedPixels 0 "$CaseId repair.repairedPixels"

  if ($Report.closureStatus -eq "pass")
  {
    Assert-Equal $Report.productionAcceptance "passed" "$CaseId productionAcceptance"
    Assert-Equal $Report.totals.totalGapPixels 0 "$CaseId pass totalGapPixels"
  }
  elseif ($Report.closureStatus -eq "fail")
  {
    Assert-Equal $Report.productionAcceptance "failed" "$CaseId productionAcceptance"
    Assert-True (@($Report.worstLayers).Count -gt 0) "$CaseId fail report missing worstLayers"
    Assert-True ([int64]$Report.totals.totalGapPixels -gt 0) "$CaseId fail report missing gap pixels"
  }
}

function New-AssetHash([string]$RepoRoot, [string]$RelativePath, [string]$Role)
{
  $assetPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $RelativePath))
  Assert-True (Test-Path -LiteralPath $assetPath) "missing $Role asset: $RelativePath"
  return [pscustomobject][ordered]@{
    role = $Role
    path = $RelativePath.Replace("\", "/")
    bytes = (Get-Item -LiteralPath $assetPath).Length
    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $assetPath).Hash.ToLowerInvariant()
  }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($RunId))
{
  $RunId = Get-Date -Format "yyyyMMdd_HHmmss"
}

$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$slicerExe = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripExe = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$templatePath = Join-Path $repoRoot "samples/configs/material_closure/real_model_diagnostic_template.json"
$validationRoot = Join-Path $repoRoot "output/MaterialClosureRealModelValidation/$RunId"
New-Item -ItemType Directory -Force -Path $validationRoot | Out-Null

$cases = @(
  [pscustomobject][ordered]@{
    id = "nai_you_new"
    assets = @(
      @{ role = "obj"; path = "model/obj/nai_you_new/MF_nai_you.obj" },
      @{ role = "mtl"; path = "model/obj/nai_you_new/MF_nai_you.mtl" },
      @{ role = "texture"; path = "model/obj/nai_you_new/T_Nai_you.png" }
    )
  },
  [pscustomobject][ordered]@{
    id = "aishen_fudiao"
    assets = @(
      @{ role = "obj"; path = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj" },
      @{ role = "mtl"; path = "model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.mtl" },
      @{ role = "texture"; path = "model/obj/aishen_fudiao/T_aishen_damuzhi_L_tx02.png" }
    )
  },
  [pscustomobject][ordered]@{
    id = "meigui_fudiao"
    assets = @(
      @{ role = "obj"; path = "model/obj/meigui_fudiao/04.obj" },
      @{ role = "mtl"; path = "model/obj/meigui_fudiao/04.mtl" },
      @{ role = "texture"; path = "model/obj/meigui_fudiao/zhongzhi1(4).png" }
    )
  }
)

$caseSummaries = @()
Push-Location $repoRoot
try
{
  foreach ($case in $cases)
  {
    Write-Host "== 12D-10 real model: $($case.id)"
    $caseRoot = Join-Path $validationRoot $case.id
    $packageRoot = Join-Path $caseRoot "package"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null

    $effectiveConfig = Read-Json $templatePath
    $objAsset = $case.assets | Where-Object { $_.role -eq "obj" } | Select-Object -First 1
    $modelPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $objAsset.path))
    $effectiveConfig.input.modelPath = $modelPath.Replace("\", "/")
    $effectiveConfig.output.packageDir = $packageRoot.Replace("\", "/")
    $effectiveConfig.materialProcessProfile.name = "material_closure_$($case.id)_diagnostic"
    $effectiveConfigPath = Join-Path $caseRoot "slice_config.effective.json"
    Write-Utf8NoBom $effectiveConfigPath ($effectiveConfig | ConvertTo-Json -Depth 64)

    $sliceLogPath = Join-Path $caseRoot "slicer_cli.log"
    $sliceInvocation = @{
      Name = "slicer $($case.id)"
      Executable = $slicerExe
      Arguments = @("--config", $effectiveConfigPath)
      LogPath = $sliceLogPath
    }
    $sliceOutput = Invoke-External @sliceInvocation
    $timings = Convert-TimingLine $sliceOutput

    $ripLogPath = Join-Path $caseRoot "rip_reader.log"
    $ripInvocation = @{
      Name = "RIP reader $($case.id)"
      Executable = $ripExe
      Arguments = @("--package", $packageRoot, "--summary")
      LogPath = $ripLogPath
    }
    Invoke-External @ripInvocation | Out-Null

    $manifest = Read-Json (Join-Path $packageRoot "manifest.json")
    $closureReport = Read-Json (Join-Path $packageRoot "reports/material_closure_report.json")
    Assert-Protocol $manifest $case.id
    Assert-ClosureReport $closureReport $case.id
    Assert-Equal $closureReport.totals.layerCount $manifest.grid.layerCount "$($case.id) closure layerCount"

    $hashManifestPath = Join-Path $caseRoot "tiff_sha256.csv"
    $hashInvocation = @{
      Manifest = $manifest
      PackageRoot = $packageRoot
      CaseId = $case.id
      OutputPath = $hashManifestPath
    }
    $hashedLayerCount = Write-TiffHashManifest @hashInvocation
    $assetHashes = @(
      $case.assets | ForEach-Object { New-AssetHash $repoRoot $_.path $_.role }
    )

    $caseSummaries += [pscustomobject][ordered]@{
      caseId = $case.id
      effectiveConfig = Get-RelativePath $repoRoot $effectiveConfigPath
      effectiveConfigSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $effectiveConfigPath).Hash.ToLowerInvariant()
      assets = $assetHashes
      grid = [ordered]@{
        widthPx = [int]$manifest.grid.widthPx
        heightPx = [int]$manifest.grid.heightPx
        layerCount = [int]$manifest.grid.layerCount
        layerThicknessMm = [double]$manifest.grid.layerThicknessMm
      }
      closure = [ordered]@{
        source = $closureReport.source
        confidence = $closureReport.confidence
        status = $closureReport.closureStatus
        productionAcceptance = $closureReport.productionAcceptance
        colorFillGapPixels = [int64]$closureReport.totals.colorFillGapPixels
        modelSupportGapPixels = [int64]$closureReport.totals.modelSupportGapPixels
        colorSupportGapPixels = [int64]$closureReport.totals.colorSupportGapPixels
        internalVoidGapPixels = [int64]$closureReport.totals.internalVoidGapPixels
        varnishSupportGapPixels = [int64]$closureReport.totals.varnishSupportGapPixels
        totalGapPixels = [int64]$closureReport.totals.totalGapPixels
        repairedPixels = [int64]$closureReport.totals.repairedPixels
        remainingGapPixels = [int64](Get-ObjectProperty $closureReport.totals "remainingGapPixels" $closureReport.totals.totalGapPixels)
        externalBackgroundProtectedPixels = [int64]$closureReport.totals.externalBackgroundProtectedPixels
        worstLayers = @($closureReport.worstLayers)
        diagnostics = @($closureReport.diagnostics)
      }
      protocol = [ordered]@{
        schema = $manifest.schema
        channelOrder = @($manifest.tiff.channelOrder)
        bitDepth = [int]$manifest.tiff.bitDepth
        polarity = $manifest.tiff.polarity
        printValue = [int]$manifest.tiff.printValue
        emptyValue = [int]$manifest.tiff.emptyValue
      }
      timingMs = $timings
      tiffHashes = [ordered]@{
        layerCount = $hashedLayerCount
        path = Get-RelativePath $repoRoot $hashManifestPath
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $hashManifestPath).Hash.ToLowerInvariant()
      }
      ripReader = "passed"
    }
  }

  $summary = [pscustomobject][ordered]@{
    schema = "slicesoft.material_closure_real_model_validation.12d.1"
    runId = $RunId
    buildConfig = $Config
    repairEnabled = $false
    repairDecision = "diagnostic pass does not require production TIFF repair"
    protocolInvariant = "p0.rgbwsv.2/RGBWSV/uint8/black_is_print"
    cases = $caseSummaries
  }
  $summaryPath = Join-Path $validationRoot "validation_summary.json"
  Write-Utf8NoBom $summaryPath ($summary | ConvertTo-Json -Depth 64)
  Write-Host "12D-10 real-model validation: PASS"
  Write-Host "summary=$(Get-RelativePath $repoRoot $summaryPath)"
}
finally
{
  Pop-Location
}
