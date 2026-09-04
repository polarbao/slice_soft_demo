[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-slicesoft/main",
    # 接受 Debug 只是为了能【显式跳过】，不是为了在 Debug 上验收。
    # 原先这里是 ValidateSet("Release")，而 CTest 用 $<CONFIG> 无条件注册本门禁，
    # 于是 Debug 回归里传进来的 Debug 被参数校验直接拒绝 —— 这条门禁在任何
    # Debug 回归里都必然失败，且失败原因与被测对象无关，纯属注册与脚本契约不一致。
    # 它长期红着，让 Debug 失败集恒定多一条噪音，掩盖真实问题。
    # 现改为：非 Release 时以 SKIP_RETURN_CODE 退出，CTest 报「Skipped」而非 Failed。
    # 之所以不用 CTest 的 CONFIGURATIONS 属性：CTest 4.3.1 下实测它确实解析到了
    # CONFIGURATIONS=Release 却仍然执行该条，滤不掉；且「显式跳过」比「静默缺席」更好 ——
    # 后者会让一条门禁悄悄从回归里消失，正是本仓库反复踩的那类隐性债。
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$RepositoryRoot = ".",
    [string]$OutputRoot = "",
    [switch]$SkipPackagePreparation
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 与 CMakeLists 里的 SKIP_RETURN_CODE 对应，改动时必须两处同步。
$script:SkipReturnCode = 111

if ($Config -ne "Release")
{
    Write-Host ("14F05 SKIP: 阶段封口门禁只验 Release 分发包 " +
        "output/distribution/<Config>/modules/slicer，当前 Config=$Config，跳过。")
    exit $script:SkipReturnCode
}

function Resolve-AbsolutePath
{
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Invoke-ScriptStep
{
    param(
        [Parameter(Mandatory = $true)][string]$PowerShellPath,
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    & $PowerShellPath `
        -NoProfile `
        -ExecutionPolicy Bypass `
        -File $ScriptPath `
        @Arguments | ForEach-Object { Write-Host $_ }
    $exitCode = $LASTEXITCODE
    $stopwatch.Stop()
    if ($exitCode -ne 0)
    {
        throw "$Name failed with exit code $exitCode"
    }

    return [pscustomobject][ordered]@{
        name = $Name
        result = "PASS"
        elapsedMs = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
    }
}

$repository = Resolve-AbsolutePath $RepositoryRoot
$build = Resolve-AbsolutePath $BuildDirectory
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $build "stage14f05_evidence/$Config"
}
$evidenceRoot = Resolve-AbsolutePath $OutputRoot
$repositoryPrefix = $repository.TrimEnd('\') + '\'
if (-not $evidenceRoot.StartsWith($repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase))
{
    throw "OutputRoot must remain inside the repository: $evidenceRoot"
}

$powerShellPath = Join-Path $PSHOME "powershell.exe"
$packageDirectory = Join-Path $repository "output/distribution/$Config/modules/slicer"
$handoffDirectory = Join-Path $repository "output/handoff/stage14f02"
$steps = @()

if (-not $SkipPackagePreparation)
{
    $steps += Invoke-ScriptStep `
        -PowerShellPath $powerShellPath `
        -ScriptPath (Join-Path $repository "scripts/PackageSlicerModule.ps1") `
        -Arguments @(
            "-BuildDir", $build,
            "-OutputRoot", (Join-Path $repository "output/distribution"),
            "-Config", $Config,
            "-SkipBuild"
        ) `
        -Name "14F-01 package preparation"

    $steps += Invoke-ScriptStep `
        -PowerShellPath $powerShellPath `
        -ScriptPath (Join-Path $repository "scripts/Prepare14F02PrintM1Handoff.ps1") `
        -Arguments @(
            "-BuildDir", $build,
            "-Config", $Config,
            "-DistributionRoot", (Join-Path $repository "output/distribution"),
            "-HandoffRoot", $handoffDirectory,
            "-SkipBuild"
        ) `
        -Name "14F-02 handoff preparation"
}

$steps += Invoke-ScriptStep `
    -PowerShellPath $powerShellPath `
    -ScriptPath (Join-Path $repository "scripts/TestSlicerModulePackage.ps1") `
    -Arguments @(
        "-PackageDir", $packageDirectory,
        "-BuildDir", $build,
        "-Config", $Config,
        "-EvidenceRoot", (Join-Path $evidenceRoot "14f01")
    ) `
    -Name "14F-01 isolated package gate"

$steps += Invoke-ScriptStep `
    -PowerShellPath $powerShellPath `
    -ScriptPath (Join-Path $repository "scripts/Test14F02PrintM1Handoff.ps1") `
    -Arguments @(
        "-HandoffRoot", $handoffDirectory,
        "-EvidenceRoot", (Join-Path $evidenceRoot "14f02")
    ) `
    -Name "14F-02 M1 local intake gate"

$steps += Invoke-ScriptStep `
    -PowerShellPath $powerShellPath `
    -ScriptPath (Join-Path $repository "scripts/Run14F03SingleModelS1Gate.ps1") `
    -Arguments @(
        "-BuildDirectory", $build,
        "-Config", $Config,
        "-RepositoryRoot", $repository,
        "-OutputRoot", (Join-Path $evidenceRoot "14f03")
    ) `
    -Name "14F-03 single model S1 gate"

$steps += Invoke-ScriptStep `
    -PowerShellPath $powerShellPath `
    -ScriptPath (Join-Path $repository "scripts/Run14F04S2ContractGate.ps1") `
    -Arguments @(
        "-RepositoryRoot", $repository,
        "-OutputRoot", (Join-Path $evidenceRoot "14f04")
    ) `
    -Name "14F-04 S2 local contract gate"

$frozenContractFiles = @(
    "contracts/print_module_spi.h",
    "contracts/slicer_capability_dtos.json",
    "contracts/slicer_three_lane_contract.json",
    "contracts/file_contract_v1.request.schema.json",
    "contracts/file_contract_v1.result.schema.json",
    "contracts/file_contract_v1.contract_info.schema.json",
    "contracts/file_contract_v1.exit_codes.json",
    "contracts/p0.rgbwsv.2.schema.json",
    "contracts/slicer_rip_s2_contract.json",
    "contracts/slicer_ui_view_spec.json",
    "docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md",
    "docs/slice/DOC/DOC_DECISION_14F_外部验证延期与接口冻结.md"
)
$frozenContractHashes = @()
foreach ($relativePath in $frozenContractFiles)
{
    $path = Join-Path $repository $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Frozen contract file is missing: $relativePath"
    }
    $frozenContractHashes += [ordered]@{
        path = $relativePath
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

if (-not (Test-Path -LiteralPath $evidenceRoot -PathType Container))
{
    New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
}
$evidence = [ordered]@{
    schema = "slicesoft.stage14f05.closure.1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    branch = (& git -C $repository branch --show-current).Trim()
    config = $Config
    stageStatus = "SLICER_PACKAGE_READY_INTERFACES_FROZEN"
    externalAcceptance = "EXTERNAL_VALIDATION_DEFERRED"
    steps = @($steps)
    frozenContractHashes = @($frozenContractHashes)
    prohibitedClaims = @(
        "EXTERNAL_ACCEPTED",
        "PRODUCTION_READY",
        "PRINT_SIDE_VALIDATED",
        "TARGET_RIP_VALIDATED"
    )
}
$evidencePath = Join-Path $evidenceRoot "stage14f05_closure.json"
$evidence | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $evidencePath -Encoding UTF8

Write-Host "STAGE14F05_LOCAL_CLOSURE_PASS steps=$($steps.Count) frozenContracts=$($frozenContractHashes.Count)"
Write-Host "status=SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED"
Write-Host "evidence=$evidencePath"
