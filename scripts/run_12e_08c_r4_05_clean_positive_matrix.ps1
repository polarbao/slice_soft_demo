param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$OutputRoot = "output/benchmarks/12e_08c_r4_05_clean_positive_matrix",
    [double]$VoxelMm = 0.20
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $repoRoot $BuildDir
}
$resolvedOutputRoot = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot
} else {
    Join-Path $repoRoot $OutputRoot
}
$candidateExecutables = @(
    (Join-Path $resolvedBuildDir "$Config/texture_fill_partition_positive_matrix.exe"),
    (Join-Path $resolvedBuildDir "texture_fill_partition_positive_matrix.exe")
)
$executable = $candidateExecutables |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $executable) {
    throw "未找到 texture_fill_partition_positive_matrix，请先构建目标。候选路径：$($candidateExecutables -join ', ')"
}

$cases = @(
    @{
        id = "clean_obj_primary"
        config = "samples/configs/texture_fill_partition/r4_05_clean_obj_primary.json"
    },
    @{
        id = "clean_obj_independent"
        config = "samples/configs/texture_fill_partition/r4_05_clean_obj_independent.json"
    },
    @{
        id = "clean_3mf_texture2d"
        config = "samples/configs/texture_fill_partition/r4_05_clean_3mf_texture2d.json"
    }
)

New-Item -ItemType Directory -Force -Path $resolvedOutputRoot | Out-Null
$summaryCases = @()
Push-Location $repoRoot
try {
    foreach ($case in $cases) {
        $configPath = Join-Path $repoRoot $case.config
        if (-not (Test-Path -LiteralPath $configPath)) {
            throw "R4-05 必跑配置缺失：$configPath"
        }
        $reportPath = Join-Path $resolvedOutputRoot "$($case.id).json"
        & $executable `
            --config $configPath `
            --output $reportPath `
            --case-id $case.id `
            --voxel-mm $VoxelMm `
            --padding-voxels 1
        if ($LASTEXITCODE -ne 0) {
            throw "R4-05 正向矩阵失败：$($case.id)，退出码=$LASTEXITCODE"
        }

        $report = Get-Content -LiteralPath $reportPath -Raw |
            ConvertFrom-Json
        if ($report.schema -ne "slicesoft.texture_fill_positive_matrix.12e_08c_r4.1") {
            throw "R4-05 schema 不匹配：$($case.id)"
        }
        if (-not $report.diagnosticOnly -or $report.productionOutputWritten) {
            throw "R4-05 生产边界被破坏：$($case.id)"
        }
        if ([int64]$report.requiredRepairPassCount -ne 0) {
            throw "正常模型不得计入 required repair PASS：$($case.id)"
        }
        if ($report.input.preflightStatus -ne "passed") {
            throw "R4-05 模型预检未通过：$($case.id)"
        }
        if (-not $report.summary.matrixPass `
            -or -not $report.summary.complementPass `
            -or -not $report.summary.monotonicPass `
            -or -not $report.summary.endpointPass `
            -or -not $report.summary.materialResolutionPass) {
            throw "R4-05 汇总不变量失败：$($case.id)"
        }
        if ($report.widthSweep.sampleCount -lt 1 `
            -or $report.widthSweep.sampleCount -gt 3) {
            throw "R4-05 width sample 数量异常：$($case.id)"
        }
        if (@($report.widthSweep.requestedAnchorWidthsMm).Count -ne 3) {
            throw "R4-05 原始 width 三点未完整保留：$($case.id)"
        }
        foreach ($sample in $report.widthSweep.samples) {
            if (-not $sample.partitionPass -or -not $sample.invariantPass) {
                throw "R4-05 width sample 互补不变量失败：$($case.id)"
            }
        }
        $endpoint = $report.widthSweep.samples[-1]
        if (-not $endpoint.allTexture `
            -or [int64]$endpoint.stats.modelFillVoxels -ne 0 `
            -or [int64]$endpoint.stats.textureSurfaceVoxels -ne [int64]$endpoint.stats.modelVoxels) {
            throw "R4-05 allTexture 终点失败：$($case.id)"
        }
        foreach ($material in $report.materialCases) {
            if ($material.available -and -not $material.compositionPass) {
                throw "R4-05 材料合成失败：$($case.id)/$($material.requestedMaterial)/$($material.requestedRole)"
            }
            if (-not $material.available -and [string]::IsNullOrWhiteSpace($material.reasonCode)) {
                throw "R4-05 不可用材料缺少稳定原因：$($case.id)/$($material.requestedRole)"
            }
            if ([int64]$material.printVoxels.S -ne 0) {
                throw "Model Fill 不得占用 S 通道：$($case.id)/$($material.requestedMaterial)"
            }
        }

        $summaryCases += [ordered]@{
            caseId = $case.id
            modelPath = $report.input.modelPath
            sourceHash = $report.input.sourceHash
            resourceHash = $report.input.resourceHash
            preflightStatus = $report.input.preflightStatus
            sampleCount = $report.widthSweep.sampleCount
            requestedAnchorCount = @($report.widthSweep.requestedAnchorWidthsMm).Count
            deduplicated = $report.widthSweep.deduplicated
            minimumWidthMm = $report.widthSweep.minimumWidthMm
            maximumWidthMm = $report.widthSweep.maximumWidthMm
            materialCaseCount = @($report.materialCases).Count
            matrixPass = $report.summary.matrixPass
            report = $reportPath
        }
    }
}
finally {
    Pop-Location
}

$summary = [ordered]@{
    schema = "slicesoft.texture_fill_positive_matrix_summary.12e_08c_r4.1"
    diagnosticOnly = $true
    productionOutputWritten = $false
    requiredRepairPassCount = 0
    buildConfig = $Config
    voxelMm = $VoxelMm
    caseCount = $summaryCases.Count
    allPassed = $true
    cases = $summaryCases
}
$summaryPath = Join-Path $resolvedOutputRoot "summary.json"
$summary | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Host "R4-05 clean positive matrix PASS"
Write-Host "summary: $summaryPath"
