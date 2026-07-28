[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$OutputDir = "output/benchmarks/13b_08",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
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

function Invoke-NativeStep
{
    param(
        [string]$Name,
        [string]$Executable,
        [string[]]$Arguments
    )

    Write-Host "== $Name"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Resolve-RepositoryPath
{
    param(
        [string]$Root,
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath(
        (Join-Path $Root $Path))
}

function Resolve-Executable
{
    param(
        [string]$BuildPath,
        [string]$BuildConfig,
        [string]$Name
    )

    foreach ($candidate in @(
        (Join-Path $BuildPath "$BuildConfig/$Name.exe"),
        (Join-Path $BuildPath (
            "apps/slicer_debug_ui/$BuildConfig/$Name.exe")),
        (Join-Path $BuildPath "$Name.exe")))
    {
        if (Test-Path -LiteralPath $candidate)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "Executable $Name was not found under $BuildPath."
}

function Read-Json
{
    param([string]$Path)

    Assert-True (
        Test-Path -LiteralPath $Path) `
        "JSON file does not exist: $Path"
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path |
        ConvertFrom-Json
}

function Write-Utf8NoBom
{
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if ($parent)
    {
        New-Item -ItemType Directory -Path $parent -Force |
            Out-Null
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function New-AssetEvidence
{
    param(
        [string]$Root,
        [string]$Path,
        [string]$Role
    )

    $absolutePath = Resolve-RepositoryPath -Root $Root -Path $Path
    Assert-True (
        Test-Path -LiteralPath $absolutePath) `
        "Required asset does not exist: $absolutePath"
    $item = Get-Item -LiteralPath $absolutePath
    return [ordered]@{
        path = $absolutePath.Replace("\", "/")
        repositoryPath = $Path.Replace("\", "/")
        format = $item.Extension.TrimStart(".").ToLowerInvariant()
        role = $Role
        bytes = $item.Length
        sha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $absolutePath
        ).Hash.ToLowerInvariant()
    }
}

$repoRoot = (
    Resolve-Path (Join-Path $PSScriptRoot "..")
).Path
$resolvedBuildDir = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $BuildDir
$resolvedOutputRoot = Resolve-RepositoryPath `
    -Root $repoRoot `
    -Path $OutputDir
$configOutput = Join-Path `
    $resolvedOutputRoot `
    $Config.ToLowerInvariant()
$matrixOutput = Join-Path $configOutput "real_model_matrix"
$uiEvidencePath = Join-Path `
    $configOutput `
    "qt_real_assets_workflow.json"
$summaryPath = Join-Path `
    $configOutput `
    "scene_workflow_summary.json"

New-Item -ItemType Directory -Path $configOutput -Force |
    Out-Null

Push-Location $repoRoot
$previousQpaPlatform = $env:QT_QPA_PLATFORM
try
{
    if (-not $SkipBuild)
    {
        Invoke-NativeStep `
            -Name "build 13B-08 workflow targets" `
            -Executable "cmake" `
            -Arguments @(
                "--build", $resolvedBuildDir,
                "--config", $Config,
                "--target",
                "slicer_cli",
                "slicer_debug_ui",
                "multi_model_scene_matrix",
                "rip_reader_test",
                "multimodel_scene_contract_unit_tests",
                "grid_layout_policy_unit_tests",
                "scene_collision_admission_unit_tests",
                "multi_model_layer_composer_unit_tests",
                "scene_layer_adapters_unit_tests",
                "multi_model_package_writer_unit_tests",
                "multi_model_production_service_unit_tests",
                "translated_scene_raster_reuse_unit_tests",
                "multi_model_scene_matrix_report_unit_tests",
                "production_package_result_unit_tests",
                "scene_transform_controller_unit_tests",
                "scene_batch_import_controller_unit_tests",
                "scene_slice_action_controller_unit_tests",
                "--", "/m")
    }

    $matrixExe = Resolve-Executable `
        -BuildPath $resolvedBuildDir `
        -BuildConfig $Config `
        -Name "multi_model_scene_matrix"
    $ripExe = Resolve-Executable `
        -BuildPath $resolvedBuildDir `
        -BuildConfig $Config `
        -Name "rip_reader_test"
    $uiExe = Resolve-Executable `
        -BuildPath $resolvedBuildDir `
        -BuildConfig $Config `
        -Name "slicer_debug_ui"

    $targetedRegex = (
        "^(multimodel_scene_contract_unit_tests|" +
        "grid_layout_policy_unit_tests|" +
        "scene_collision_admission_unit_tests|" +
        "multi_model_layer_composer_unit_tests|" +
        "scene_layer_adapters_unit_tests|" +
        "multi_model_package_writer_unit_tests|" +
        "multi_model_production_service_unit_tests|" +
        "scene_slice_route_positive|" +
        "scene_slice_route_negative|" +
        "translated_scene_raster_reuse_unit_tests|" +
        "multi_model_scene_matrix_report_unit_tests|" +
        "production_package_result_unit_tests|" +
        "scene_transform_controller_unit_tests|" +
        "scene_batch_import_controller_unit_tests|" +
        "scene_slice_action_controller_unit_tests)$")
    Invoke-NativeStep `
        -Name "13B-08 targeted CTest" `
        -Executable "ctest" `
        -Arguments @(
            "--test-dir", $resolvedBuildDir,
            "-C", $Config,
            "-R", $targetedRegex,
            "--output-on-failure")

    Invoke-NativeStep `
        -Name "real-model 1/11/12/22 and 3MF matrix" `
        -Executable $matrixExe `
        -Arguments @(
            "--source-root", $repoRoot,
            "--output", $matrixOutput)

    $matrixReportPath = Join-Path `
        $matrixOutput `
        "real_model_matrix.json"
    $matrixReport = Read-Json -Path $matrixReportPath
    Assert-True (
        $matrixReport.schema -eq
            "slicesoft.multimodel_scene_matrix.13b.1") `
        "Unexpected real-model matrix schema."
    Assert-True (
        [bool]$matrixReport.functionalMatrixPass) `
        "Real-model functional matrix did not pass."
    Assert-True (
        -not [bool]$matrixReport.productionGo -and
        $matrixReport.productionStatus -eq "INPUT_OPEN") `
        "Functional fixture must keep production INPUT_OPEN."

    $positiveCounts = @(
        $matrixReport.cases |
            Where-Object {
                $_.category -eq "positive" -and $_.passed
            } |
            ForEach-Object { [int]$_.instanceCount }
    )
    foreach ($requiredCount in @(1, 11, 12, 22))
    {
        Assert-True (
            $positiveCounts -contains $requiredCount) `
            "Positive matrix is missing $requiredCount instances."
    }

    foreach ($case in $matrixReport.cases)
    {
        if ($case.category -ne "positive" -or -not $case.passed)
        {
            continue
        }
        Invoke-NativeStep `
            -Name "RIP strict $($case.caseId)" `
            -Executable $ripExe `
            -Arguments @(
                "--package", [string]$case.package.path,
                "--summary")
    }

    $env:QT_QPA_PLATFORM = "offscreen"
    $uiCases = @(
        "scene-batch-import-three",
        "scene-batch-import-partial-failure",
        "scene-slice-current",
        "scene-slice-stale",
        "scene-slice-cancel",
        "scene-slice-no-fallback")
    foreach ($uiCase in $uiCases)
    {
        Invoke-NativeStep `
            -Name "UI smoke $uiCase" `
            -Executable $uiExe `
            -Arguments @(
                "--ui-smoke-test",
                "--case", $uiCase)
    }
    Invoke-NativeStep `
        -Name "UI smoke scene-slice-real-assets" `
        -Executable $uiExe `
        -Arguments @(
            "--ui-smoke-test",
            "--case", "scene-slice-real-assets",
            "--output", $uiEvidencePath)

    $uiEvidence = Read-Json -Path $uiEvidencePath
    Assert-True (
        $uiEvidence.schema -eq
            "slicesoft.scene_workflow_ui_smoke.13b08.1") `
        "Unexpected Qt scene workflow evidence schema."
    Assert-True (
        $uiEvidence.status -eq "passed" -and
        [bool]$uiEvidence.singlePackage -and
        [int]$uiEvidence.instanceCount -eq 3) `
        "Qt real-assets workflow contract did not pass."
    Assert-True (
        -not [bool]$uiEvidence.productionGo -and
        $uiEvidence.productionStatus -eq "INPUT_OPEN") `
        "Qt functional workflow must keep production INPUT_OPEN."

    Invoke-NativeStep `
        -Name "RIP strict Qt real-assets package" `
        -Executable $ripExe `
        -Arguments @(
            "--package", [string]$uiEvidence.packageDir,
            "--summary")

    $sceneReport = Read-Json -Path (
        [string]$uiEvidence.sceneReportPath)
    $assets = @()
    $assets += New-AssetEvidence `
        -Root $repoRoot `
        -Path (
            "model/obj/xiao_ma_wu_yu_new/" +
            "MF_Xiao_ma_Damuzhi_ty02.obj") `
        -Role "real_textured_obj"
    $assets += New-AssetEvidence `
        -Root $repoRoot `
        -Path "model/obj/yecan/3.obj" `
        -Role "independent_real_obj"
    $assets += New-AssetEvidence `
        -Root $repoRoot `
        -Path (
            "samples/models/3mf/" +
            "texture2d_checker_cube.3mf") `
        -Role "texture2d_3mf_control"

    $negativeCases = @(
        $matrixReport.cases |
            Where-Object { $_.category -eq "negative" }
    )
    $summary = [ordered]@{
        schema = "slicesoft.scene_workflow_matrix.13b08.1"
        generatedAtUtc = (
            [DateTime]::UtcNow.ToString("o"))
        status = "functional_pass"
        buildConfig = $Config
        productionGo = $false
        productionStatus = "INPUT_OPEN"
        productionBlockers = @(
            $matrixReport.productionBlockers)
        assets = $assets
        profile = [ordered]@{
            id = $sceneReport.resolvedProfileId
            dpiX = $uiEvidence.grid.dpiX
            dpiY = $uiEvidence.grid.dpiY
            layerThicknessMm =
                $uiEvidence.grid.layerThicknessMm
            pipelineMode =
                $sceneReport.effectivePipelineMode
            buildVolume = $sceneReport.buildVolume
            productionReady =
                [bool]$sceneReport.productionReady
        }
        scaleMatrix = [ordered]@{
            required = @(1, 3, 11, 12, 22)
            corePositiveInstanceCounts = $positiveCounts
            qtCurrentSceneInstanceCount =
                [int]$uiEvidence.instanceCount
            passed = $true
        }
        formatMatrix = [ordered]@{
            required = @("obj", "obj_mtl_texture", "3mf")
            passed = $true
        }
        negativeMatrix = [ordered]@{
            coreCases = @(
                $negativeCases |
                    ForEach-Object {
                        [ordered]@{
                            caseId = $_.caseId
                            errorCode = $_.errorCode
                            passed = -not [bool]$_.passed
                        }
                    })
            partialImport = "scene-batch-import-partial-failure"
            capacity = "scene_batch_import_controller_unit_tests"
            cancel = "scene-slice-cancel"
            stale = "scene-slice-stale"
            globalNoFallback = "scene-slice-no-fallback"
        }
        outputContract = [ordered]@{
            schema = "p0.rgbwsv.2"
            channelOrder = @("R", "G", "B", "W", "S", "V")
            bitDepth = 8
            polarity = "black_is_print"
            printValue = 0
            emptyValue = 255
            oneSceneOnePackage = $true
            ripStrict = $true
            qtRoute = $uiEvidence.route
            qtPackage = $uiEvidence.packageDir
            qtSceneId = $uiEvidence.sceneId
            qtSceneRevision = $uiEvidence.sceneRevision
            qtSceneHash = $uiEvidence.sceneHash
        }
        evidence = [ordered]@{
            matrixReport = $matrixReportPath.Replace("\", "/")
            qtWorkflow = $uiEvidencePath.Replace("\", "/")
            targetedCtest = "passed"
        }
    }
    Write-Utf8NoBom `
        -Path $summaryPath `
        -Content (
            $summary |
                ConvertTo-Json -Depth 20)

    Write-Host "13B-08 scene workflow FUNCTIONAL PASS"
    Write-Host "Summary: $summaryPath"
    Write-Host "Production: INPUT_OPEN"
}
finally
{
    $env:QT_QPA_PLATFORM = $previousQpaPlatform
    Pop-Location
}
