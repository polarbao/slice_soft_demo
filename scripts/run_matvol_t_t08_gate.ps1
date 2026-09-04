param(
    [string]$BuildDir = "build-slicesoft/main",
    [string]$Config = "Release",
    [switch]$SkipBuild,
    [string]$ExpectedProductionAcceptance = "rgbwsvt_candidate_unvalidated",
    [string]$OutputRoot = "output/benchmarks/matvol_t_t08"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Write-Utf8NoBom([string]$Path, [string]$Content) {
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $Path, $Content + [Environment]::NewLine, $encoding)
}

function Resolve-Executable([string]$Name) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe"),
        (Join-Path $BuildDir "apps/slicer_cli/$Config/$Name.exe"),
        (Join-Path $BuildDir "apps/rip_reader_test/$Config/$Name.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "找不到 $Name.exe，已检查：$($candidates -join ', ')"
}

function Write-ConfigVariant(
    [string]$Path,
    [string]$Package,
    [bool]$TransferEnabled,
    [bool]$TransferPresent
) {
    $profile = Get-Content -LiteralPath `
        "samples/configs/matvol_t/process_profiles/obj_mtl_texture_rgb_only_rgbwsvt.json" `
        -Raw | ConvertFrom-Json
    $profile.input.modelPath = (Resolve-Path `
        "model/obj/reality/finger_suoguo/03.obj").Path
    $profile.output.packageDir = $Package
    $profile.output.dpiX = 80
    $profile.output.dpiY = 80
    $profile.output.layerThicknessMm = 0.30
    $profile.preview.enabled = $false
    if ($TransferEnabled) {
        $profile.output.packageProtocol = "p0.rgbwsvt.1"
        $profile.output.channelOrder = @("R", "G", "B", "W", "S", "V", "T")
        if (-not $TransferPresent) {
            $profile.transferChannelPolicy.materialDiffuseRgbValues = @(@(1, 2, 3))
        }
    } else {
        $profile.output.packageProtocol = "p0.rgbwsv.2"
        $profile.output.channelOrder = @("R", "G", "B", "W", "S", "V")
        $profile.PSObject.Properties.Remove("transferChannelPolicy")
    }
    Write-Utf8NoBom $Path ($profile | ConvertTo-Json -Depth 100)
}

function Invoke-Slice([string]$ConfigPath) {
    $lines = @(& $script:SlicerExe --config $ConfigPath 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "slicer_cli 失败 ($exitCode)：$($lines -join [Environment]::NewLine)"
    }
    $timing = $lines | Where-Object { $_ -match '^SLICE_TIMING ' } | Select-Object -Last 1
    Assert-True ($null -ne $timing) "slicer_cli 未输出 SLICE_TIMING"
    $match = [regex]::Match(
        [string]$timing,
        'totalMs=(?<total>[0-9.]+).*memoryAvailable=(?<available>[01]).*peakWorkingSetBytes=(?<peak>[0-9]+)')
    Assert-True $match.Success "无法解析 SLICE_TIMING：$timing"
    Assert-True ($match.Groups['available'].Value -eq '1') "进程内存观测不可用"
    return [ordered]@{
        totalMs = [double]$match.Groups['total'].Value
        peakWorkingSetBytes = [uint64]$match.Groups['peak'].Value
    }
}

function Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    return [double]$sorted[[int][math]::Floor($sorted.Count / 2)]
}

function LayerHashes([string]$Package) {
    $manifest = Get-Content -LiteralPath (Join-Path $Package "manifest.json") `
        -Raw | ConvertFrom-Json
    return @($manifest.layers | Sort-Object index | ForEach-Object {
        (Get-FileHash -Algorithm SHA256 -LiteralPath `
            (Join-Path $Package $_.path)).Hash.ToLowerInvariant()
    })
}

if (-not $SkipBuild) {
    cmake --build $BuildDir --config $Config --target `
        slicer_cli rip_reader_test matvol_t_production_matrix_tests `
        matvol_rgbwsvt_legacy_package_tests matvol_legacy_transfer_session_tests
    if ($LASTEXITCODE -ne 0) { throw "T-08 定向构建失败" }
}

$script:SlicerExe = Resolve-Executable "slicer_cli"
$ripExe = Resolve-Executable "rip_reader_test"
$absoluteOutput = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $absoluteOutput) {
    Remove-Item -LiteralPath $absoluteOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $absoluteOutput | Out-Null

$ctestRegex = '^(matvol_(legacy_transfer_session|rgbwsvt_(legacy_package|cli_candidate)|t_production_matrix)_tests)$'
ctest --test-dir $BuildDir -C $Config -R $ctestRegex --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "T-08 CTest 矩阵失败" }

$legacyRuns = @()
$transferRuns = @()
$transferPackages = @()
for ($index = 1; $index -le 3; ++$index) {
    $legacyPackage = Join-Path $absoluteOutput "legacy_$index"
    $legacyConfig = Join-Path $absoluteOutput "legacy_$index.json"
    Write-ConfigVariant $legacyConfig $legacyPackage $false $false
    $legacyRuns += Invoke-Slice $legacyConfig

    $transferPackage = Join-Path $absoluteOutput "rgbwsvt_$index"
    $transferConfig = Join-Path $absoluteOutput "rgbwsvt_$index.json"
    Write-ConfigVariant $transferConfig $transferPackage $true $true
    $transferRuns += Invoke-Slice $transferConfig
    $transferPackages += $transferPackage
    & $ripExe --package $transferPackage --summary | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "RGBWSVT strict RIP 失败：run $index" }
    $manifest = Get-Content -LiteralPath (Join-Path $transferPackage "manifest.json") `
        -Raw | ConvertFrom-Json
    Assert-True ($manifest.schema -eq "p0.rgbwsvt.1") "RGBWSVT manifest schema 不正确"
    Assert-True ($manifest.productionAcceptance -eq $ExpectedProductionAcceptance) `
        "productionAcceptance 不符合阶段预期"
    Assert-True ([uint64]$manifest.tiff.channelStats.T.printPixels -gt 0) `
        "03 RGBWSVT 包必须包含 T 像素"
}

$firstHashes = LayerHashes $transferPackages[0]
foreach ($package in $transferPackages[1..2]) {
    $hashes = LayerHashes $package
    Assert-True ($hashes.Count -eq $firstHashes.Count) "重复运行层数不一致"
    Assert-True (-not (Compare-Object $firstHashes $hashes)) "重复运行 TIFF 字节不一致"
}

$badPackage = Join-Path $absoluteOutput "bad_t_statistics"
Copy-Item -LiteralPath $transferPackages[0] -Destination $badPackage -Recurse
$badManifestPath = Join-Path $badPackage "manifest.json"
$badManifest = Get-Content -LiteralPath $badManifestPath -Raw | ConvertFrom-Json
$badManifest.tiff.channelStats.T.printPixels =
    [uint64]$badManifest.tiff.channelStats.T.printPixels + 1
Write-Utf8NoBom $badManifestPath ($badManifest | ConvertTo-Json -Depth 100)
& $ripExe --package $badPackage --expect-error `
    --expect-code E_LAYER_STATISTICS_MISMATCH --quiet | Out-Host
if ($LASTEXITCODE -ne 0) { throw "坏 T Package 未被 strict RIP 拒绝" }

$legacyMedianMs = Median ([double[]]@($legacyRuns | ForEach-Object { $_.totalMs }))
$transferMedianMs = Median ([double[]]@($transferRuns | ForEach-Object { $_.totalMs }))
$legacyPeak = [uint64](($legacyRuns | ForEach-Object { $_.peakWorkingSetBytes } |
    Measure-Object -Maximum).Maximum)
$transferPeak = [uint64](($transferRuns | ForEach-Object { $_.peakWorkingSetBytes } |
    Measure-Object -Maximum).Maximum)
$timeLimitMs = [math]::Max($legacyMedianMs * 2.0, $legacyMedianMs + 5000.0)
$memoryLimitBytes = [uint64][math]::Max(
    [double]$legacyPeak * 1.25,
    [double]$legacyPeak + 64MB)
Assert-True ($transferMedianMs -le $timeLimitMs) "RGBWSVT 耗时超过 T-08 相对门"
Assert-True ($transferPeak -le $memoryLimitBytes) "RGBWSVT 峰值内存超过 T-08 相对门"

$report = [ordered]@{
    schema = "slicesoft.matvol_t_t08_gate.1"
    generatedAt = (Get-Date).ToString("o")
    config = $Config
    expectedProductionAcceptance = $ExpectedProductionAcceptance
    cases = [ordered]@{
        reality03 = "pass"
        reality08 = "expected_topology_rejection"
        reality09 = "expected_topology_rejection"
        noTransfer = "pass_rgbwsv_projection_exact"
        badTransfer = "pass_strict_rejection"
        cancellation = "pass_no_partial_package"
        deterministicTiff = "pass"
        strictRip = "pass"
    }
    performance = [ordered]@{
        legacyRuns = $legacyRuns
        rgbwsvtRuns = $transferRuns
        legacyMedianMs = $legacyMedianMs
        rgbwsvtMedianMs = $transferMedianMs
        timeLimitMs = $timeLimitMs
        legacyPeakWorkingSetBytes = $legacyPeak
        rgbwsvtPeakWorkingSetBytes = $transferPeak
        memoryLimitBytes = $memoryLimitBytes
        classification = "same_machine_regression_gate_not_device_sla"
    }
    externalRip = "accepted_user_input_not_locally_retested"
    physicalPrint = "not_tested"
    status = "PASS"
}
$reportPath = Join-Path $absoluteOutput "gate_report.json"
Write-Utf8NoBom $reportPath ($report | ConvertTo-Json -Depth 20)
Write-Host "MATVOL-T T-08 GATE PASS"
Write-Host "Report: $reportPath"
