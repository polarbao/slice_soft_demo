param(
    [string]$BuildDir = "build-slicesoft/main",
    [ValidateSet("Release")]
    [string]$Config = "Release",
    [string]$Output =
        "output/benchmarks/03d_01/tiff_writer_handwritten_baseline.json",
    [int]$Iterations = 5,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Assert-True
{
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition)
    {
        throw $Message
    }
}

function Resolve-Executable
{
    param(
        [string]$ResolvedBuildDir,
        [string]$Configuration,
        [string]$Name
    )

    $candidates = @(
        (Join-Path $ResolvedBuildDir "$Configuration/$Name.exe"),
        (Join-Path $ResolvedBuildDir $Name),
        (Join-Path $ResolvedBuildDir "$Name.exe")
    )
    foreach ($candidate in $candidates)
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Executable not found: $Name under $ResolvedBuildDir"
}

if ($Iterations -lt 5)
{
    throw "03D-01 Release baseline requires at least 5 iterations."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot
try
{
    $resolvedBuildDir = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot $BuildDir))
    $resolvedOutput = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot $Output))

    if (-not $SkipBuild)
    {
        & cmake --build $resolvedBuildDir --config $Config --target `
            tiff_writer_contract_unit_tests `
            tiff_writer_benchmark `
            slicer_cli `
            rip_reader_test
        if ($LASTEXITCODE -ne 0)
        {
            throw "03D-01 Release build failed."
        }
    }

    & ctest --test-dir $resolvedBuildDir -C $Config `
        -R "^tiff_writer_contract_unit_tests$" `
        --output-on-failure
    if ($LASTEXITCODE -ne 0)
    {
        throw "03D-01 TIFF writer contract test failed."
    }

    $benchmark = Resolve-Executable `
        -ResolvedBuildDir $resolvedBuildDir `
        -Configuration $Config `
        -Name "tiff_writer_benchmark"
    $workDir = Join-Path $repoRoot "output/benchmarks/03d_01/files"
    & $benchmark `
        --output $resolvedOutput `
        --work-dir $workDir `
        --warmup 1 `
        --iterations $Iterations
    if ($LASTEXITCODE -ne 0)
    {
        throw "03D-01 Writer-only benchmark failed."
    }

    $report = Get-Content -Raw -LiteralPath $resolvedOutput |
        ConvertFrom-Json
    Assert-True `
        ($report.schema -eq "slicesoft.tiff_writer_benchmark.03d.1") `
        "03D-01 benchmark schema mismatch."
    Assert-True `
        ($report.backend -eq "handwritten") `
        "03D-01 benchmark backend must remain handwritten."
    Assert-True `
        ($report.buildType -eq "Release") `
        "03D-01 benchmark must use a Release executable."
    Assert-True `
        ($report.scope -eq "writer_only") `
        "03D-01 benchmark scope must be writer_only."
    Assert-True `
        ($report.cases.Count -eq 2) `
        "03D-01 benchmark must include stripped and tiled."
    foreach ($case in $report.cases)
    {
        Assert-True `
            ($case.measurementIterations -ge 5) `
            "$($case.storageMode) requires at least 5 measurements."
        Assert-True `
            ($case.decodedPixelsExact -eq $true) `
            "$($case.storageMode) decoded pixels are not exact."
        Assert-True `
            ($case.p50Ms -gt 0 -and $case.p95Ms -gt 0) `
            "$($case.storageMode) timing is invalid."
        Assert-True `
            ($case.bytesWritten -gt 0) `
            "$($case.storageMode) bytesWritten is invalid."
        Assert-True `
            ($case.writerStagingBytesEstimate -gt 0) `
            "$($case.storageMode) staging-memory estimate is invalid."
        Assert-True `
            ($case.memory.available -eq $true `
                -and $case.memory.workingSetBytes -gt 0 `
                -and $case.memory.peakWorkingSetBytes -gt 0 `
                -and $case.memory.samplePoint -eq `
                    "after_measurement_writes_before_decode" `
                -and $case.memory.peakScope -eq `
                    "process_cumulative") `
            "$($case.storageMode) process memory counters are unavailable."
    }

    $slicerCli = Resolve-Executable `
        -ResolvedBuildDir $resolvedBuildDir `
        -Configuration $Config `
        -Name "slicer_cli"
    $ripReader = Resolve-Executable `
        -ResolvedBuildDir $resolvedBuildDir `
        -Configuration $Config `
        -Name "rip_reader_test"
    & $slicerCli `
        --config "samples/configs/golden/material_process_top2_fixture.json"
    if ($LASTEXITCODE -ne 0)
    {
        throw "03D-01 RIP fixture package generation failed."
    }
    & $ripReader `
        --package "output/GoldenMaterialProcessTop2" `
        --quiet
    if ($LASTEXITCODE -ne 0)
    {
        throw "03D-01 rip_reader_test failed."
    }

    Write-Host "03D-01 handwritten TIFF Writer baseline PASS."
    Write-Host "  report: $resolvedOutput"
    foreach ($case in $report.cases)
    {
        $summary = (
            "  {0}: p50={1:N3} ms p95={2:N3} ms " +
            "working={3} bytes cumulativePeak={4} bytes " +
            "stagingEstimate={5} bytes") -f
            $case.storageMode,
            $case.p50Ms,
            $case.p95Ms,
            $case.memory.workingSetBytes,
            $case.memory.peakWorkingSetBytes,
            $case.writerStagingBytesEstimate
        Write-Host $summary
    }
}
finally
{
    Pop-Location
}
