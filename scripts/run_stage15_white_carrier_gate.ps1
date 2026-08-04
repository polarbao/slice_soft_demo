param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$OutputRoot = "output/benchmarks/stage15",
    [switch]$SkipBuild,
    [switch]$PreparationOnly,
    [switch]$VerifyZeroDrift,
    [switch]$VerifyPerformance,
    [ValidateSet("pending", "passed", "failed")]
    [string]$PhysicalProof = "pending"
)

$ErrorActionPreference = "Stop"

function Assert-True
{
    param([bool]$Condition, [string]$Message)

    if (-not $Condition)
    {
        throw $Message
    }
}

function Resolve-RepositoryPath
{
    param([string]$RepositoryRoot, [string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $Path))
}

function Read-Json
{
    param([string]$Path)

    Assert-True (Test-Path -LiteralPath $Path) "JSON 不存在：$Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path |
        ConvertFrom-Json
}

function Write-Json
{
    param([string]$Path, $Value)

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory))
    {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($Path),
        ($Value | ConvertTo-Json -Depth 100),
        [System.Text.UTF8Encoding]::new($false))
}

function Get-FileIdentity
{
    param([string]$RepositoryRoot, [string]$RelativePath)

    $resolvedPath = Resolve-RepositoryPath $RepositoryRoot $RelativePath
    Assert-True (Test-Path -LiteralPath $resolvedPath) `
        "Stage 15 fixture 资产不存在：$RelativePath"
    $item = Get-Item -LiteralPath $resolvedPath
    if ($item.PSIsContainer)
    {
        $files = @(
            Get-ChildItem -LiteralPath $resolvedPath -File -Recurse |
                Sort-Object FullName |
                ForEach-Object {
                    [ordered]@{
                        path = $_.FullName.Substring(
                            $RepositoryRoot.TrimEnd('\').Length + 1).Replace('\', '/')
                        bytes = [uint64]$_.Length
                        sha256 = (Get-FileHash `
                                -Algorithm SHA256 `
                                -LiteralPath $_.FullName).Hash.ToLowerInvariant()
                    }
                }
        )
        return [ordered]@{
            path = $RelativePath.Replace('\', '/')
            type = "directory"
            fileCount = $files.Count
            files = $files
        }
    }
    return [ordered]@{
        path = $RelativePath.Replace('\', '/')
        type = "file"
        bytes = [uint64]$item.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedPath).Hash.ToLowerInvariant()
    }
}

function Get-FixturePaths
{
    param($Fixture)

    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($name in @(
        "configPath",
        "legacyConfigPath",
        "candidateConfigPath",
        "modelPath",
        "texturePath",
        "goldenRoot"))
    {
        $property = $Fixture.PSObject.Properties[$name]
        if ($null -ne $property -and
            -not [string]::IsNullOrWhiteSpace([string]$property.Value))
        {
            $paths.Add([string]$property.Value)
        }
    }
    return $paths
}

function Resolve-Executable
{
    param([string]$BuildPath, [string]$BuildConfig, [string]$Name)

    foreach ($candidate in @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath "$Name.exe")))
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "无法在 $BuildPath 下找到 $Name.exe"
}

function Invoke-CapturedTool
{
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$LogPath)

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try
    {
        $lines = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $directory = Split-Path -Parent $LogPath
    if (-not [string]::IsNullOrWhiteSpace($directory))
    {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    [System.IO.File]::WriteAllLines(
        [System.IO.Path]::GetFullPath($LogPath),
        $lines,
        [System.Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{
        exitCode = $exitCode
        lines = $lines
    }
}

function New-RunConfig
{
    param(
        [string]$RepositoryRoot,
        [string]$TemplatePath,
        [string]$PackagePath)

    $resolvedTemplatePath = Resolve-RepositoryPath $RepositoryRoot $TemplatePath
    $configObject = Read-Json $resolvedTemplatePath
    $templateDirectory = Split-Path -Parent $resolvedTemplatePath
    $configObject.input.modelPath = [System.IO.Path]::GetFullPath(
        (Join-Path $templateDirectory ([string]$configObject.input.modelPath)))
    $configObject.output.packageDir = [System.IO.Path]::GetFullPath($PackagePath)
    $configObject.preview.enabled = $false
    return $configObject
}

function Get-PackageTiffProjection
{
    param([string]$PackagePath)

    $manifest = Read-Json (Join-Path $PackagePath "manifest.json")
    $projection = @()
    foreach ($layer in @($manifest.layers | Sort-Object index))
    {
        $layerPath = [System.IO.Path]::GetFullPath(
            (Join-Path $PackagePath ([string]$layer.path)))
        Assert-True (Test-Path -LiteralPath $layerPath) `
            "manifest layer TIFF 不存在：$layerPath"
        $projection += [ordered]@{
            index = [int]$layer.index
            path = [string]$layer.path
            sha256 = (Get-FileHash `
                    -Algorithm SHA256 `
                    -LiteralPath $layerPath).Hash.ToLowerInvariant()
        }
    }
    return $projection
}

function Assert-TiffProjectionsEqual
{
    param($Actual, $Expected, [string]$Message)

    Assert-True ($Actual.Count -eq $Expected.Count) `
        "$Message layer count 不一致"
    for ($index = 0; $index -lt $Actual.Count; ++$index)
    {
        Assert-True ($Actual[$index].index -eq $Expected[$index].index) `
            "$Message layerIndex 不一致：$index"
        Assert-True ($Actual[$index].sha256 -eq $Expected[$index].sha256) `
            "$Message TIFF SHA-256 不一致：layer=$($Actual[$index].index)"
    }
}

function Get-Fixture
{
    param($Manifest, [string]$Id)

    $fixture = @($Manifest.fixtures | Where-Object { $_.id -eq $Id })
    Assert-True ($fixture.Count -eq 1) "Stage 15 fixture $Id 必须唯一"
    return $fixture[0]
}

function Get-CarrierEvidence
{
    param([string]$PackagePath)

    $sliceReport = Read-Json (Join-Path $PackagePath "reports/slice_report.json")
    $materialReport = Read-Json (
        Join-Path $PackagePath "reports/material_process_report.json")
    $layerCarrierSum = [uint64](@(
            $sliceReport.layers |
                Measure-Object -Property unprintableWhiteCarrierPixels -Sum
        ).Sum)
    $validationFailures = @($materialReport.validation.failures)
    return [ordered]@{
        carrierPixels = [uint64]$sliceReport.totals.unprintableWhiteCarrierPixels
        layerCarrierSum = $layerCarrierSum
        whitePrintPixels = [uint64]$sliceReport.totals.channelStats.W.printPixels
        materialCarrierPixels = [uint64]$materialReport.white.unprintableWhiteCarrierPixels
        validationFailureCount = $validationFailures.Count
    }
}

function Write-RunConfig
{
    param(
        [string]$RepositoryRoot,
        [string]$TemplatePath,
        [string]$PackagePath,
        [string]$ConfigPath)

    $configObject = New-RunConfig `
        $RepositoryRoot `
        $TemplatePath `
        $PackagePath
    Write-Json $ConfigPath $configObject
    return $configObject
}

function Invoke-SlicerSuccess
{
    param(
        [string]$Slicer,
        [string]$ConfigPath,
        [string]$LogPath)

    $result = Invoke-CapturedTool `
        -Executable $Slicer `
        -Arguments @("--config", $ConfigPath) `
        -LogPath $LogPath
    Assert-True ($result.exitCode -eq 0) `
        "切片失败：$ConfigPath。日志：$LogPath"
    return $result
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedBuildDir = Resolve-RepositoryPath $repoRoot $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath $repoRoot $OutputRoot
$fixtureManifestPath = Join-Path `
    $repoRoot `
    "samples/configs/material_process/stage15_fixture_manifest.json"
$fixtureManifest = Read-Json $fixtureManifestPath

Assert-True `
    ($fixtureManifest.schema -eq "slicesoft.stage15.fixture_manifest.1") `
    "Stage 15 fixture manifest schema 不匹配"

$fixtureIds = @($fixtureManifest.fixtures | ForEach-Object { [string]$_.id })
foreach ($requiredId in @("F-01", "F-02", "F-03", "F-04", "F-05"))
{
    Assert-True ($fixtureIds -contains $requiredId) `
        "Stage 15 fixture manifest 缺少 $requiredId"
}

$fixtureEvidence = @()
foreach ($fixture in @($fixtureManifest.fixtures))
{
    $assets = @()
    foreach ($path in @(Get-FixturePaths $fixture))
    {
        $assets += Get-FileIdentity $repoRoot $path
    }
    $fixtureEvidence += [ordered]@{
        id = [string]$fixture.id
        purpose = [string]$fixture.purpose
        assets = $assets
    }
}

$baselineIdentityPath = Join-Path $resolvedOutputRoot "baseline_identity.json"
Assert-True (Test-Path -LiteralPath $baselineIdentityPath) `
    "缺少 15A-01 基线身份：$baselineIdentityPath"

if (-not $SkipBuild)
{
    & cmake --build $resolvedBuildDir --config $Config --target `
        texture_white_carrier_policy_unit_tests `
        experimental_config_unit_tests `
        slicer_cli `
        rip_reader_test
    Assert-True ($LASTEXITCODE -eq 0) "Stage 15 Release target 构建失败"
}

$gitHead = (& git -C $repoRoot rev-parse HEAD).Trim()
Assert-True ($LASTEXITCODE -eq 0) "无法读取 Stage 15 git HEAD"
$summary = [ordered]@{
    schema = "slicesoft.stage15.white_carrier_gate.1"
    generatedAt = [DateTimeOffset]::Now.ToString("o")
    mode = if ($PreparationOnly) { "preparation_only" } else { "full_gate" }
    status = if ($PreparationOnly) { "prepared" } else { "not_implemented" }
    build = [ordered]@{
        gitHead = $gitHead
        buildDir = $BuildDir.Replace('\', '/')
        configuration = $Config
    }
    fixtureManifest = Get-FileIdentity `
        $repoRoot `
        "samples/configs/material_process/stage15_fixture_manifest.json"
    fixtures = $fixtureEvidence
    gates = [ordered]@{
        G1 = "pending"
        G2 = "pending"
        G3 = "pending"
        G4 = "pending"
        G5 = "pending"
        G6 = "pending"
    }
    channelDiffCounts = [ordered]@{
        R = $null
        G = $null
        B = $null
        W = $null
        S = $null
        V = $null
    }
    tiffSha256 = @()
    reader = [ordered]@{
        status = "pending"
        exitCode = $null
    }
    performance = [ordered]@{
        requested = [bool]$VerifyPerformance
        status = if ($VerifyPerformance) { "pending" } else { "not_run" }
        measurementRuns = 7
        warmupRuns = 1
        p50 = $null
        evidence = $null
    }
    zeroDrift = [ordered]@{
        requested = [bool]$VerifyZeroDrift
        status = if ($VerifyZeroDrift) { "pending" } else { "not_run" }
        goldenFileCount = 0
        quickCiExitCode = $null
        quickCiLog = $null
    }
    physicalProof = $PhysicalProof
}

$summaryPath = Join-Path $resolvedOutputRoot "stage15_white_carrier_summary.json"
Write-Json $summaryPath $summary

if ($PreparationOnly)
{
    Write-Host "Stage 15 fixture discovery and Gate schema preparation PASS."
    Write-Host "Summary: $summaryPath"
    exit 0
}

$slicerCli = Resolve-Executable $resolvedBuildDir $Config "slicer_cli"
$ripReader = Resolve-Executable $resolvedBuildDir $Config "rip_reader_test"
$policyTests = Resolve-Executable `
    $resolvedBuildDir `
    $Config `
    "texture_white_carrier_policy_unit_tests"
$configRoot = Join-Path $resolvedOutputRoot "configs"
$packageRoot = Join-Path $resolvedOutputRoot "packages"
$logRoot = Join-Path $resolvedOutputRoot "logs"
New-Item -ItemType Directory -Force -Path $configRoot | Out-Null
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

if ($VerifyZeroDrift)
{
    $baselineIdentity = Read-Json $baselineIdentityPath
    $baselineGolden = @(
        $baselineIdentity.inputs |
            Where-Object {
                ([string]$_.path).Replace('\', '/').StartsWith(
                    "tests/golden/expected/",
                    [System.StringComparison]::OrdinalIgnoreCase)
            } |
            Sort-Object path
    )
    Assert-True ($baselineGolden.Count -gt 0) `
        "15A-01 基线未包含 golden SHA-256"

    $beforeLines = [System.Collections.Generic.List[string]]::new()
    $afterLines = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in $baselineGolden)
    {
        $relativePath = ([string]$entry.path).Replace('\', '/')
        $resolvedPath = Resolve-RepositoryPath $repoRoot $relativePath
        Assert-True (Test-Path -LiteralPath $resolvedPath) `
            "golden 文件不存在：$relativePath"
        $actualHash = (Get-FileHash `
                -Algorithm SHA256 `
                -LiteralPath $resolvedPath).Hash.ToLowerInvariant()
        $expectedHash = ([string]$entry.sha256).ToLowerInvariant()
        $beforeLines.Add("$expectedHash  $relativePath")
        $afterLines.Add("$actualHash  $relativePath")
        Assert-True ($actualHash -eq $expectedHash) `
            "G3 golden SHA-256 漂移：$relativePath"
    }

    $beforePath = Join-Path $resolvedOutputRoot "sha256_baseline_before.txt"
    $afterPath = Join-Path $resolvedOutputRoot "sha256_baseline_after.txt"
    [System.IO.File]::WriteAllLines(
        $beforePath,
        $beforeLines,
        [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllLines(
        $afterPath,
        $afterLines,
        [System.Text.UTF8Encoding]::new($false))

    $quickCiLog = Join-Path $logRoot "quick_ci_zero_drift.log"
    Push-Location $repoRoot
    try
    {
        $quickCiResult = Invoke-CapturedTool `
            -Executable (Join-Path $PSHOME "powershell.exe") `
            -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                (Join-Path $repoRoot "scripts/run_ci_quick.ps1")) `
            -LogPath $quickCiLog
    }
    finally
    {
        Pop-Location
    }
    Assert-True ($quickCiResult.exitCode -eq 0) `
        "G3 Quick CI 失败，日志：$quickCiLog"

    $summary.gates.G3 = "passed"
    $summary.zeroDrift.status = "passed"
    $summary.zeroDrift.goldenFileCount = $baselineGolden.Count
    $summary.zeroDrift.quickCiExitCode = $quickCiResult.exitCode
    $summary.zeroDrift.quickCiLog = `
        $quickCiLog.Substring($repoRoot.Length + 1).Replace('\', '/')
}

if ($VerifyPerformance)
{
    $performanceLog = Join-Path $logRoot "white_carrier_performance.log"
    $performanceResult = Invoke-CapturedTool `
        -Executable (Join-Path $PSHOME "powershell.exe") `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $repoRoot "scripts/run_stage15_white_carrier_performance.ps1"),
            "-BuildDir",
            $resolvedBuildDir,
            "-Config",
            $Config,
            "-OutputRoot",
            $resolvedOutputRoot,
            "-SkipBuild") `
        -LogPath $performanceLog
    Assert-True ($performanceResult.exitCode -eq 0) `
        "Stage 15 性能 Gate 失败，日志：$performanceLog"

    $performancePath = Join-Path $resolvedOutputRoot "performance_p50.json"
    $performanceEvidence = Read-Json $performancePath
    Assert-True ($performanceEvidence.status -eq "passed") `
        "Stage 15 性能证据未通过：$performancePath"
    $summary.performance.status = "passed"
    $summary.performance.p50 = $performanceEvidence.fixtures
    $summary.performance.evidence = `
        $performancePath.Substring($repoRoot.Length + 1).Replace('\', '/')
}

$pixelDiffPath = Join-Path $resolvedOutputRoot "pixel_diff_F04.csv"
$policyResult = Invoke-CapturedTool `
    -Executable $policyTests `
    -Arguments @("--evidence-csv", $pixelDiffPath) `
    -LogPath (Join-Path $logRoot "policy_unit.log")
Assert-True ($policyResult.exitCode -eq 0) "Stage 15 policy unit evidence failed"
$pixelDiff = @(Import-Csv -LiteralPath $pixelDiffPath)
Assert-True ($pixelDiff.Count -gt 0) "F-04 pixel diff evidence 为空"
foreach ($row in $pixelDiff)
{
    Assert-True ($row.channel -eq "W") "F-04 出现非 W 通道差异"
    Assert-True ([int]$row.before -eq 255 -and [int]$row.after -eq 0) `
        "F-04 W 差异不是 255 -> 0"
    Assert-True (
        [int]$row.r -eq 255 -and
        [int]$row.g -eq 255 -and
        [int]$row.b -eq 255) `
        "F-04 差异像素不是严格 RGB 255/255/255"
}
$summary.channelDiffCounts.R = 0
$summary.channelDiffCounts.G = 0
$summary.channelDiffCounts.B = 0
$summary.channelDiffCounts.W = $pixelDiff.Count
$summary.channelDiffCounts.S = 0
$summary.channelDiffCounts.V = 0

$f01 = Get-Fixture $fixtureManifest "F-01"
$f01Package = Join-Path $packageRoot "F01_new"
$f01ConfigPath = Join-Path $configRoot "F01_new.json"
Write-RunConfig `
    $repoRoot `
    ([string]$f01.configPath) `
    $f01Package `
    $f01ConfigPath | Out-Null
Invoke-SlicerSuccess `
    $slicerCli `
    $f01ConfigPath `
    (Join-Path $logRoot "slicer_F01_new.log") | Out-Null
$f01Evidence = Get-CarrierEvidence $f01Package
Assert-True ($f01Evidence.carrierPixels -gt 0) "F-01 未产出纯白补白像素"
Assert-True ($f01Evidence.whitePrintPixels -gt 0) "F-01 未产出 W 打印像素"
Assert-True (
    $f01Evidence.carrierPixels -eq $f01Evidence.layerCarrierSum -and
    $f01Evidence.carrierPixels -eq $f01Evidence.materialCarrierPixels) `
    "F-01 slice/material/layer 纯白补白统计不一致"
Assert-True ($f01Evidence.validationFailureCount -eq 0) `
    "F-01 material_process_report 存在校验失败"
$summary.gates.G1 = "passed"

$ripResult = Invoke-CapturedTool `
    -Executable $ripReader `
    -Arguments @("--package", $f01Package, "--quiet") `
    -LogPath (Join-Path $logRoot "rip_strict_F01.log")
Assert-True ($ripResult.exitCode -eq 0) "F-01 RIP strict Reader failed"
$summary.reader.status = "passed"
$summary.reader.exitCode = $ripResult.exitCode
$summary.gates.G5 = "passed"

$f04 = Get-Fixture $fixtureManifest "F-04"
$f04Package = Join-Path $packageRoot "F04_new"
$f04ConfigPath = Join-Path $configRoot "F04_new.json"
$f04Config = Write-RunConfig `
    $repoRoot `
    ([string]$f04.configPath) `
    $f04Package `
    $f04ConfigPath
Invoke-SlicerSuccess `
    $slicerCli `
    $f04ConfigPath `
    (Join-Path $logRoot "slicer_F04_new.log") | Out-Null
$f04Evidence = Get-CarrierEvidence $f04Package
Assert-True ($f04Evidence.carrierPixels -gt 0) "F-04 候选策略未产出 W"

$f04FailPackage = Join-Path $packageRoot "F04_fail_closed"
$f04FailConfigPath = Join-Path $configRoot "F04_fail_closed.json"
$f04Config.output.packageDir = [System.IO.Path]::GetFullPath($f04FailPackage)
$f04Config.texture.unprintableWhitePolicy = "fail_closed"
Write-Json $f04FailConfigPath $f04Config
$f04FailResult = Invoke-CapturedTool `
    -Executable $slicerCli `
    -Arguments @("--config", $f04FailConfigPath) `
    -LogPath (Join-Path $logRoot "slicer_F04_fail_closed.log")
Assert-True ($f04FailResult.exitCode -eq 0) `
    "F-04 fail_closed 诊断运行未能产出材料校验报告"
$f04FailReport = Read-Json (
    Join-Path $f04FailPackage "reports/material_process_report.json")
$f04FailCodes = @($f04FailReport.validation.failures)
Assert-True (-not [bool]$f04FailReport.validation.pass) `
    "F-04 fail_closed 负向基线意外通过材料校验"
Assert-True ($f04FailCodes -contains "E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE") `
    "F-04 fail_closed 未报告缺失 W 材料"
$summary.gates.G2 = "passed"

$f03 = Get-Fixture $fixtureManifest "F-03"
$f03T0Package = Join-Path $packageRoot "F03_threshold_0"
$f03T0ConfigPath = Join-Path $configRoot "F03_threshold_0.json"
$f03T0Config = Write-RunConfig `
    $repoRoot `
    ([string]$f03.configPath) `
    $f03T0Package `
    $f03T0ConfigPath
Invoke-SlicerSuccess `
    $slicerCli `
    $f03T0ConfigPath `
    (Join-Path $logRoot "slicer_F03_threshold_0.log") | Out-Null
$f03T0Evidence = Get-CarrierEvidence $f03T0Package

$f03T1Package = Join-Path $packageRoot "F03_threshold_1"
$f03T1ConfigPath = Join-Path $configRoot "F03_threshold_1.json"
$f03T0Config.output.packageDir = [System.IO.Path]::GetFullPath($f03T1Package)
$f03T0Config.texture.unprintableWhiteInkThreshold = 1
Write-Json $f03T1ConfigPath $f03T0Config
Invoke-SlicerSuccess `
    $slicerCli `
    $f03T1ConfigPath `
    (Join-Path $logRoot "slicer_F03_threshold_1.log") | Out-Null
$f03T1Evidence = Get-CarrierEvidence $f03T1Package
Assert-True ($f03T1Evidence.carrierPixels -gt $f03T0Evidence.carrierPixels) `
    "F-03 threshold=1 未覆盖更多近白像素"

$f02 = Get-Fixture $fixtureManifest "F-02"
$f02OldPackage = Join-Path $packageRoot "F02_old"
$f02OldConfigPath = Join-Path $configRoot "F02_old.json"
Write-RunConfig `
    $repoRoot `
    ([string]$f02.legacyConfigPath) `
    $f02OldPackage `
    $f02OldConfigPath | Out-Null
Invoke-SlicerSuccess `
    $slicerCli `
    $f02OldConfigPath `
    (Join-Path $logRoot "slicer_F02_old.log") | Out-Null

$f02NewPackage = Join-Path $packageRoot "F02_new"
$f02NewConfigPath = Join-Path $configRoot "F02_new.json"
Write-RunConfig `
    $repoRoot `
    ([string]$f02.candidateConfigPath) `
    $f02NewPackage `
    $f02NewConfigPath | Out-Null
Invoke-SlicerSuccess `
    $slicerCli `
    $f02NewConfigPath `
    (Join-Path $logRoot "slicer_F02_new.log") | Out-Null
$f02Evidence = Get-CarrierEvidence $f02NewPackage
Assert-True ($f02Evidence.carrierPixels -eq 0) `
    "F-02 被错误识别为含严格纯白纹理"
$f02OldProjection = @(Get-PackageTiffProjection $f02OldPackage)
$f02NewProjection = @(Get-PackageTiffProjection $f02NewPackage)
Assert-TiffProjectionsEqual `
    $f02NewProjection `
    $f02OldProjection `
    "F-02 新旧 Profile"
$tiffEvidence = [ordered]@{
    schema = "slicesoft.stage15.tiff_hash_projection.1"
    fixture = "F-02"
    legacy = $f02OldProjection
    candidate = $f02NewProjection
}
$tiffEvidencePath = Join-Path $resolvedOutputRoot "tiff_hash_F02.json"
Write-Json $tiffEvidencePath $tiffEvidence
$summary.tiffSha256 = $f02NewProjection
$summary.gates.G4 = "passed"

$summary.status = "partial_pass"
$summary.evidence = [ordered]@{
    F01 = $f01Evidence
    F03 = [ordered]@{
        threshold0CarrierPixels = $f03T0Evidence.carrierPixels
        threshold1CarrierPixels = $f03T1Evidence.carrierPixels
    }
    F04 = [ordered]@{
        carrierPixels = $f04Evidence.carrierPixels
        failClosedExitCode = $f04FailResult.exitCode
        failClosedValidation = $f04FailCodes
        pixelDiffCsv = $pixelDiffPath.Substring($repoRoot.Length + 1).Replace('\', '/')
    }
    F02 = [ordered]@{
        carrierPixels = $f02Evidence.carrierPixels
        tiffHashProjection = $tiffEvidencePath.Substring($repoRoot.Length + 1).Replace('\', '/')
    }
}
Write-Json $summaryPath $summary

$passedGates = if ($VerifyZeroDrift) { "G1/G2/G3/G4/G5" } else { "G1/G2/G4/G5" }
Write-Host "Stage 15 automatic white-carrier Gate PASS ($passedGates)."
if ($VerifyPerformance)
{
    Write-Host "Stage 15 performance Gate PASS (NFR-02)."
}
Write-Host "Summary: $summaryPath"
