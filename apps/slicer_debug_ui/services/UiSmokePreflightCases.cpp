#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::SliceProgressTiming(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    SliceProgressProtocolParser parser;
    SliceProtocolUpdate update = parser.Append(
        QStringLiteral("ordinary log\nSLICE_PROGRESS phase=layer_processing current=7 total=20 percent=55 elap"));
    if (!update.progress.isEmpty() || !update.timings.isEmpty())
    {
        return fail(QStringLiteral("切片进度协议不应解析未完成的行。"));
    }

    update = parser.Append(QStringLiteral(
        "sedMs=1234.500\n"
        "SLICE_TIMING engine=legacy profileLevel=detailed configLoadMs=1.000 modelLoadMs=20.000 "
        "gridSetupMs=90.000 sliceProcessingMs=800.000 layerComputeMs=500.000 layerComposeMs=75.000 "
        "tiffWriteMs=200.000 previewWriteMs=100.000 "
        "reportBuildMs=30.000 reportWriteMs=40.000 packagePublishMs=0.000 outputWriteMs=340.000 totalMs=1191.000 "
        "memoryAvailable=1 workingSetBytes=50331648 peakWorkingSetBytes=100663296\n"));
    if (update.progress.size() != 1 || update.timings.size() != 1)
    {
        return fail(QStringLiteral("切片进度协议事件数量错误。"));
    }
    const SliceProgressEvent progress = update.progress.front();
    const SliceTimingEvent timing = update.timings.front();
    if (progress.phase != QStringLiteral("layer_processing")
        || progress.current != 7
        || progress.total != 20
        || progress.percent != 55)
    {
        return fail(QStringLiteral("切片进度字段解析错误。"));
    }
    if (timing.engine != QStringLiteral("legacy")
        || qAbs(timing.gridsetupms - 90.0) > 0.001
        || qAbs(timing.sliceprocessingms - 800.0) > 0.001
        || qAbs(timing.layercomposems - 75.0) > 0.001
        || qAbs(timing.outputwritems - 340.0) > 0.001
        || qAbs(timing.totalms - 1191.0) > 0.001
        || !timing.memoryavailable
        || timing.peakworkingsetbytes != 100663296U)
    {
        return fail(QStringLiteral("切片耗时字段解析错误。"));
    }

    SliceTimingPanel panel;
    panel.Reset(QStringLiteral("运行切片"));
    panel.UpdateProgress(progress);
    panel.ShowTiming(timing);
    panel.Finish(true, 1250);
    const QString summary = panel.SummaryText();
    if (!summary.contains(QStringLiteral("传统切片引擎"))
        || !summary.contains(QStringLiteral("800.0 ms"))
        || !summary.contains(QStringLiteral("340.0 ms"))
        || !summary.contains(QStringLiteral("准入=90.0 ms"))
        || !summary.contains(QStringLiteral("合成=75.0 ms")))
    {
        return fail(QStringLiteral("切片耗时面板未显示解析后的数据：") + summary);
    }

    SliceProgressEvent sceneProgress;
    sceneProgress.phase =
        QStringLiteral("scene_package_write");
    sceneProgress.current = 12;
    sceneProgress.total = 50;
    sceneProgress.percent = 84;
    sceneProgress.elapsedms = 2100.0;
    SliceTimingEvent sceneTiming = timing;
    sceneTiming.engine = QStringLiteral("legacy-scene");
    panel.Reset(QStringLiteral("切片当前场景"));
    panel.UpdateProgress(sceneProgress);
    panel.ShowTiming(sceneTiming);
    const QString sceneSummary = panel.SummaryText();
    if (!sceneSummary.contains(
            QStringLiteral("正在保存场景图层 12 / 50"))
        || !sceneSummary.contains(
            QStringLiteral("传统场景切片引擎")))
    {
        return fail(
            QStringLiteral("场景切片阶段与引擎中文显示错误：")
            + sceneSummary);
    }
    return pass(QStringLiteral("切片进度协议与耗时面板通过。"));
}

int UiSmokeTestRunner::ModelPreflightStates(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    slicer_core::ModelPreflightExecutionResult execution;
    execution.generation = 7U;
    execution.fastComplete = true;
    execution.fullComplete = true;
    execution.result.status = slicer_core::ModelPreflightStatus::Warning;
    execution.result.legacyAdmission.status =
        slicer_core::ModelPreflightAdmissionStatus::Warning;
    execution.result.legacyAdmission.warningCodes = {
        "MESH_BOUNDARY_EDGES"};
    slicer_core::ModelPreflightIssue unknownIssue;
    unknownIssue.code = "UNKNOWN_PREFLIGHT_CODE";
    unknownIssue.severity = slicer_core::ModelPreflightIssueSeverity::Error;
    unknownIssue.count = 2U;
    execution.result.issues.push_back(unknownIssue);

    const ModelPreflightPresentation warning =
        ModelPreflightPresenter::Present(
            execution,
            slicer_core::ModelPreflightPipelineMode::Legacy);
    if (warning.state != QStringLiteral("检测有警告")
        || warning.admission != QStringLiteral("需要确认风险")
        || warning.issues.size() != 2
        || !warning.issues.front().summary.contains(QStringLiteral("未识别问题")))
    {
        return fail(QStringLiteral("model-preflight-states 中文状态或未知码 fail-closed 映射错误。"));
    }

    ModelPreflightPanel panel;
    panel.ShowPresentation(warning);
    auto* state = panel.findChild<QLabel*>(
        QStringLiteral("modelPreflightState"));
    auto* issues = panel.findChild<QTableWidget*>(
        QStringLiteral("modelPreflightIssues"));
    if (state == nullptr || issues == nullptr
        || state->text() != QStringLiteral("检测有警告")
        || issues->rowCount() != 2)
    {
        return fail(QStringLiteral("model-preflight-states 面板未呈现完整状态和问题列表。"));
    }

    execution.result.status = slicer_core::ModelPreflightStatus::Running;
    execution.result.legacyAdmission.status =
        slicer_core::ModelPreflightAdmissionStatus::Blocked;
    const ModelPreflightPresentation running =
        ModelPreflightPresenter::Present(
            execution,
            slicer_core::ModelPreflightPipelineMode::Legacy);
    if (!running.running || !running.cancancel || running.canrecheck)
    {
        return fail(QStringLiteral("model-preflight-states 运行态按钮能力错误。"));
    }
    return pass(QStringLiteral("model-preflight-states 中文状态、未知码和面板展示通过。"));
}

int UiSmokeTestRunner::ModelPreflightOneClickGate(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("model-preflight-one-click-gate 无法创建临时目录。"));
    }
    const QString cleanConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("clean"),
        ClosedBoxObjFixture());
    const QString openConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("open"),
        OpenTriangleObjFixture());
    const QString missingConfig = tempDir.filePath(QStringLiteral("missing.json"));
    const QJsonObject missingRoot{
        {QStringLiteral("input"),
         QJsonObject{{QStringLiteral("modelPath"),
                      tempDir.filePath(QStringLiteral("absent.obj"))},
                     {QStringLiteral("format"), QStringLiteral("obj")}}},
    };
    if (cleanConfig.isEmpty() || openConfig.isEmpty()
        || !WriteJsonFixture(missingConfig, missingRoot))
    {
        return fail(QStringLiteral("model-preflight-one-click-gate 无法创建 fixture。"));
    }

    ModelPreflightController productionGlobalController;
    SlicePreflightCoordinator productionGlobalCoordinator(
        &productionGlobalController);
    int productionGlobalAdmittedCount{0};
    QObject::connect(
        &productionGlobalCoordinator,
        &SlicePreflightCoordinator::SigActionAdmitted,
        &productionGlobalCoordinator,
        [&productionGlobalAdmittedCount]()
        {
            ++productionGlobalAdmittedCount;
        });
    SlicePreflightAction cleanGlobalProductionAction;
    cleanGlobalProductionAction.kind =
        SlicePreflightActionKind::GlobalProduction;
    cleanGlobalProductionAction.configpath = cleanConfig;
    productionGlobalCoordinator.RequestAction(
        cleanGlobalProductionAction);
    if (!WaitForCondition(
            [&productionGlobalAdmittedCount]()
            {
                return productionGlobalAdmittedCount == 1;
            })
        || !productionGlobalController.LastCapabilityDiagnostic().contains(
            QStringLiteral("request-override=true")))
    {
        return fail(
            QStringLiteral(
                "model-preflight-one-click-gate Global production 未通过普通 slicer_cli 能力路径。"));
    }

    ModelPreflightController controller;
    controller.SetCapabilityOverrideForTests(true);
    SlicePreflightCoordinator coordinator(&controller);
    int admittedCount{0};
    int blockedCount{0};
    int confirmationCount{0};
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigActionAdmitted,
        &coordinator,
        [&admittedCount]()
        {
            ++admittedCount;
        });
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigActionBlocked,
        &coordinator,
        [&blockedCount]()
        {
            ++blockedCount;
        });
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigLegacyConfirmationRequired,
        &coordinator,
        [&confirmationCount]()
        {
            ++confirmationCount;
        });

    SlicePreflightAction cleanAction;
    cleanAction.kind = SlicePreflightActionKind::Legacy;
    cleanAction.configpath = cleanConfig;
    coordinator.RequestAction(cleanAction);
    if (!WaitForCondition([&admittedCount]() { return admittedCount == 1; }))
    {
        return fail(QStringLiteral("model-preflight-one-click-gate clean legacy 未放行。"));
    }

    SlicePreflightAction missingAction;
    missingAction.kind = SlicePreflightActionKind::Legacy;
    missingAction.configpath = missingConfig;
    coordinator.RequestAction(missingAction);
    if (!WaitForCondition([&blockedCount]() { return blockedCount == 1; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate fatal 输入启动了动作。"));
    }

    SlicePreflightAction globalAction;
    globalAction.kind = SlicePreflightActionKind::GlobalProduction;
    globalAction.configpath = openConfig;
    coordinator.RequestAction(globalAction);
    if (!WaitForCondition([&blockedCount]() { return blockedCount == 2; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate global topology blocker 未阻断。"));
    }

    SlicePreflightAction legacyWarningAction;
    legacyWarningAction.kind = SlicePreflightActionKind::Legacy;
    legacyWarningAction.configpath = openConfig;
    coordinator.RequestAction(legacyWarningAction);
    if (!WaitForCondition(
            [&confirmationCount]() { return confirmationCount == 1; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate legacy warning 未等待确认。"));
    }
    coordinator.ConfirmLegacyWarning(true);
    if (admittedCount != 2)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate legacy 明确确认后未放行。"));
    }

    bool realCapabilityVerified{false};
    const QString candidateProgram =
        ToolPaths::FromRepoRoot(options.repo_root).openvdb_slicer_cli;
    if (QFileInfo::exists(candidateProgram))
    {
        ModelPreflightController realController;
        SlicePreflightCoordinator realCoordinator(&realController);
        int realAdmittedCount{0};
        QObject::connect(
            &realCoordinator,
            &SlicePreflightCoordinator::SigActionAdmitted,
            &realCoordinator,
            [&realAdmittedCount]()
            {
                ++realAdmittedCount;
            });
        SlicePreflightAction realGlobalAction;
        realGlobalAction.kind = SlicePreflightActionKind::OpenVdbCandidate;
        realGlobalAction.configpath = cleanConfig;
        realGlobalAction.capabilityprogram = candidateProgram;
        realCoordinator.RequestAction(realGlobalAction);
        if (!WaitForCondition(
                [&realAdmittedCount, &realController]()
                {
                    return realAdmittedCount == 1 || !realController.IsRunning();
                },
                90000)
            || realAdmittedCount != 1)
        {
            const slicer_core::ModelPreflightExecutionResult& realExecution =
                realController.CurrentExecution();
            QStringList blockerCodes;
            for (const std::string& code : realExecution.result.globalAdmission.blockerCodes)
            {
                blockerCodes.push_back(QString::fromStdString(code));
            }
            return fail(QStringLiteral(
                            "model-preflight-one-click-gate 真实 OpenVDB capability 探针未放行 clean global："
                            "status=%1 generation=%2 blockers=%3 program=%4")
                            .arg(QString::fromStdString(
                                slicer_core::ModelPreflightStatusName(
                                    realExecution.result.status)))
                            .arg(realExecution.generation)
                            .arg(blockerCodes.join(QStringLiteral(",")))
                            .arg(candidateProgram)
                        + QStringLiteral(" diagnostic=")
                        + realController.LastCapabilityDiagnostic());
        }
        realCapabilityVerified = true;
    }

    return pass(
        QStringLiteral(
            "model-preflight-one-click-gate admitted=2 blocked=2 process-start-before-admission=0 "
            "global-production=admitted real-capability=%1")
            .arg(realCapabilityVerified ? QStringLiteral("verified")
                                        : QStringLiteral("skipped")));
}

int UiSmokeTestRunner::ModelPreflightLifecycle(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("model-preflight-lifecycle 无法创建临时目录。"));
    }
    const QString cleanConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("clean"),
        ClosedBoxObjFixture());
    const QString openConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("open"),
        OpenTriangleObjFixture());
    if (cleanConfig.isEmpty() || openConfig.isEmpty())
    {
        return fail(QStringLiteral("model-preflight-lifecycle 无法创建 fixture。"));
    }

    ModelPreflightController controller;
    controller.SetCapabilityOverrideForTests(false);
    UiModelPreflightRequest first;
    first.configpath = openConfig;
    UiModelPreflightRequest second;
    second.configpath = cleanConfig;
    UiModelPreflightRequest third = second;
    controller.RequestPreflight(first);
    controller.RequestPreflight(second);
    controller.RequestPreflight(third);
    if (!WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning()
                    && controller.CurrentExecution().generation == 3U;
            }))
    {
        return fail(QStringLiteral("model-preflight-lifecycle 最新 generation 未完成。"));
    }
    if (controller.CurrentExecution().result.status
            != slicer_core::ModelPreflightStatus::Passed
        || controller.CurrentExecution().generation != 3U)
    {
        return fail(QStringLiteral("model-preflight-lifecycle 旧 generation 覆盖了最新结果。"));
    }

    controller.RequestPreflight(first);
    controller.Cancel();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (controller.CurrentExecution().result.status
        != slicer_core::ModelPreflightStatus::Cancelled)
    {
        return fail(QStringLiteral("model-preflight-lifecycle 取消状态未保持。"));
    }

    auto* disposable = new ModelPreflightController();
    disposable->SetCapabilityOverrideForTests(false);
    disposable->RequestPreflight(first);
    delete disposable;
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    return pass(QStringLiteral(
        "model-preflight-lifecycle latest-generation=3 cancel=stable close=no-crash"));
}
