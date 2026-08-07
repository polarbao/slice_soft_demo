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

int UiSmokeTestRunner::WorkspaceLayoutSizes(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("workspace-layout-sizes 未找到 manifest：") + packagePath);
    }

    MainWindow window(options.repo_root);
    auto* splitter = window.findChild<QSplitter*>(QStringLiteral("mainSplitter"));
    auto* projectDock = window.findChild<ProjectToolsDock*>(
        QStringLiteral("projectToolsDock"));
    auto* projectPanel = window.findChild<QWidget*>(QStringLiteral("projectPanel"));
    auto* workspaceTabs = window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* rightPanel = window.findChild<ContextInspector*>(
        QStringLiteral("contextInspector"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    auto* configPanel = window.findChild<ConfigEditorPanel*>();
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* projectAction = window.findChild<QAction*>(
        QStringLiteral("projectToolsToggleAction"));
    auto* diagnosticsAction = window.findChild<QAction*>(QStringLiteral("diagnosticsToggleAction"));
    if (splitter == nullptr || projectDock == nullptr || projectPanel == nullptr
        || workspaceTabs == nullptr || rightPanel == nullptr
        || preview == nullptr || configPanel == nullptr || dock == nullptr
        || projectAction == nullptr || diagnosticsAction == nullptr)
    {
        return fail(QStringLiteral("workspace-layout-sizes 缺少稳定布局对象。"));
    }

    preview->LoadPackage(package);
    dock->LoadPackage(package);
    if (preview->LayerIndices().isEmpty())
    {
        return fail(QStringLiteral("workspace-layout-sizes 输出包没有生产层。"));
    }
    const int layerIndex = preview->LayerIndices().last();
    preview->SelectLayer(layerIndex);

    window.show();
    QApplication::processEvents();
    const QList<QSize> targetSizes{
        QSize(1440, 900),
        QSize(1280, 720),
        QSize(1024, 768),
    };
    QStringList verifiedSizes;
    for (const QSize& targetSize : targetSizes)
    {
        dock->SetExpanded(false);
        window.resize(targetSize);
        QApplication::processEvents();

        if (window.width() > targetSize.width() || window.height() > targetSize.height())
        {
            return fail(
                QStringLiteral(
                    "workspace-layout-sizes 窗口被 minimumSizeHint 强制放大：requested=%1x%2 actual=%3x%4 "
                    "windowHint=%5x%6 projectDockHint=%7x%8 workspaceHint=%9x%10 rightHint=%11x%12 "
                    "previewHint=%13x%14 configHint=%15x%16")
                    .arg(targetSize.width())
                    .arg(targetSize.height())
                    .arg(window.width())
                    .arg(window.height())
                    .arg(window.minimumSizeHint().width())
                    .arg(window.minimumSizeHint().height())
                    .arg(projectPanel->minimumSizeHint().width())
                    .arg(projectPanel->minimumSizeHint().height())
                    .arg(workspaceTabs->minimumSizeHint().width())
                    .arg(workspaceTabs->minimumSizeHint().height())
                    .arg(rightPanel->minimumSizeHint().width())
                    .arg(rightPanel->minimumSizeHint().height())
                    .arg(preview->minimumSizeHint().width())
                    .arg(preview->minimumSizeHint().height())
                    .arg(configPanel->minimumSizeHint().width())
                    .arg(configPanel->minimumSizeHint().height()));
        }
        if (projectDock->IsExpanded()
            || projectAction->isChecked()
            || !workspaceTabs->isVisible()
            || !rightPanel->isVisible())
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 默认工作区可见性不正确。"));
        }

        const QRect splitterRect = GlobalRect(splitter);
        const QRect workspaceRect = GlobalRect(workspaceTabs);
        const QRect rightRect = GlobalRect(rightPanel);
        if (workspaceRect.width() < 400
            || rightRect.width() < 240)
        {
            return fail(
                QStringLiteral(
                    "workspace-layout-sizes 主工作区宽度低于冻结边界：%1/%2")
                    .arg(workspaceRect.width())
                    .arg(rightRect.width()));
        }
        if (workspaceRect.intersects(rightRect))
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 主工作区发生重叠。"));
        }
        if (!splitterRect.contains(workspaceRect)
            || !splitterRect.contains(rightRect))
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 主工作区超出 mainSplitter。"));
        }
        if (dock->IsExpanded() || diagnosticsAction->isChecked())
        {
            return fail(QStringLiteral("workspace-layout-sizes 诊断区域未保持默认隐藏。"));
        }

        dock->SetExpanded(true);
        QApplication::processEvents();
        const QRect expandedWorkspaceRect = GlobalRect(workspaceTabs);
        const QRect dockRect = GlobalRect(dock);
        if (!dock->IsExpanded() || !diagnosticsAction->isChecked() || dockRect.height() <= 0
            || expandedWorkspaceRect.height() < 200 || expandedWorkspaceRect.intersects(dockRect))
        {
            return fail(QStringLiteral("workspace-layout-sizes 诊断区域展开后覆盖或压垮中央工作区。"));
        }
        dock->SetExpanded(false);
        QApplication::processEvents();
        if (preview->CurrentLayerIndex() != layerIndex)
        {
            return fail(QStringLiteral("workspace-layout-sizes resize/dock toggle 改变了真实 layerIndex。"));
        }

        verifiedSizes.push_back(
            QStringLiteral("%1x%2=%3/%4")
                .arg(targetSize.width())
                .arg(targetSize.height())
                .arg(workspaceRect.width())
                .arg(rightRect.width()));
    }

    window.hide();
    return pass(
        QStringLiteral("workspace-layout-sizes sizes=%1 layer=%2")
            .arg(verifiedSizes.join(QStringLiteral(",")))
            .arg(layerIndex));
}

int UiSmokeTestRunner::ProductionModeSelector(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    auto* workspaceTabs =
        window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* configWorkspace =
        window.findChild<QWidget*>(QStringLiteral("configEditorScrollArea"));
    auto* modePanel =
        window.findChild<ProductionModePanel*>(QStringLiteral("productionModePanel"));
    auto* modeCombo =
        window.findChild<QComboBox*>(QStringLiteral("productionModeCombo"));
    auto* profileCombo =
        window.findChild<QComboBox*>(QStringLiteral("productionProfileCombo"));
    auto* capabilityLabel =
        window.findChild<QLabel*>(QStringLiteral("productionCapabilityLabel"));
    auto* admissionLabel =
        window.findChild<QLabel*>(QStringLiteral("productionAdmissionLabel"));
    auto* resourceLabel =
        window.findChild<QLabel*>(QStringLiteral("productionResourceLabel"));
    auto* resultIdentityLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultIdentityLabel"));
    auto* resultOutputLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultOutputLabel"));
    auto* resultResourceLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultResourceLabel"));
    auto* supportCheck =
        window.findChild<QCheckBox*>(QStringLiteral("supportEnabledCheck"));
    auto* surfaceVarnishCheck =
        window.findChild<QCheckBox*>(QStringLiteral("surfaceVarnishEnabledCheck"));
    auto* openVdbCheck =
        window.findChild<QCheckBox*>(QStringLiteral("openVdbCandidateCheck"));
    if (workspaceTabs == nullptr || configWorkspace == nullptr
        || modePanel == nullptr || modeCombo == nullptr
        || profileCombo == nullptr || capabilityLabel == nullptr
        || admissionLabel == nullptr || resourceLabel == nullptr
        || resultIdentityLabel == nullptr || resultOutputLabel == nullptr
        || resultResourceLabel == nullptr
        || supportCheck == nullptr || surfaceVarnishCheck == nullptr
        || openVdbCheck == nullptr)
    {
        return fail(QStringLiteral("production-mode-selector 缺少稳定 UI 对象。"));
    }

    if (modeCombo->currentData().toString() != QStringLiteral("legacy")
        || modeCombo->currentText() != QStringLiteral("传统切片")
        || profileCombo->isEnabled()
        || !supportCheck->isEnabled()
        || !surfaceVarnishCheck->isEnabled())
    {
        return fail(QStringLiteral("production-mode-selector 未保持传统切片默认和能力透传。"));
    }
    if (!openVdbCheck->isHidden())
    {
        return fail(QStringLiteral("production-mode-selector 普通配置页仍暴露 OpenVDB backend 开关。"));
    }

    const int globalIndex =
        modeCombo->findData(QStringLiteral("global_surface_shell"));
    if (globalIndex < 0)
    {
        return fail(QStringLiteral("production-mode-selector 缺少全局纹理壳层模式。"));
    }
    modeCombo->setCurrentIndex(globalIndex);
    QApplication::processEvents();
    if (!profileCombo->isEnabled()
        || profileCombo->count() != 2
        || profileCombo->currentData().toString()
            != QStringLiteral("global_surface_shell_restricted_candidate")
        || supportCheck->isEnabled()
        || surfaceVarnishCheck->isEnabled()
        || !supportCheck->toolTip().contains(QStringLiteral("不支持 S 支撑"))
        || !surfaceVarnishCheck->toolTip().contains(QStringLiteral("不支持 V 光油"))
        || !resourceLabel->text().contains(QStringLiteral("高资源开销"))
        || !admissionLabel->text().contains(QStringLiteral("需要重新执行")))
    {
        return fail(QStringLiteral("production-mode-selector restricted Profile 能力锁定或状态提示错误。"));
    }

    const int parityIndex = profileCombo->findData(
        QStringLiteral("global_surface_shell_material_parity_candidate"));
    profileCombo->setCurrentIndex(parityIndex);
    QApplication::processEvents();
    if (parityIndex < 0
        || !capabilityLabel->text().contains(QStringLiteral("内部镂空支撑"))
        || supportCheck->isEnabled()
        || surfaceVarnishCheck->isEnabled()
        || !supportCheck->toolTip().contains(QStringLiteral("Profile 已锁定"))
        || !surfaceVarnishCheck->toolTip().contains(QStringLiteral("Profile 已锁定")))
    {
        return fail(QStringLiteral("production-mode-selector material-parity Profile 能力锁定错误。"));
    }

    ProductionModeUiDto result;
    result.requestedmode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    result.effectivemode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    result.productionoutputwritten = true;
    result.fallbackapplied = false;
    result.resourcecost = ProductionResourceCostLevel::High;
    result.measuredtotalms = 1525.0;
    result.measuredpeakworkingsetbytes = 96U * 1024U * 1024U;
    result.sessionid = "ui-smoke-session";
    result.packagepath = "output/ui-smoke/package";
    modePanel->ShowProductionResult(result);
    if (!resultIdentityLabel->text().contains(QStringLiteral("全局纹理壳层"))
        || !resultIdentityLabel->text().contains(QStringLiteral("ui-smoke-session"))
        || !resultOutputLabel->text().contains(QStringLiteral("TIFF=已写入"))
        || !resultOutputLabel->text().contains(QStringLiteral("fallback=否"))
        || !resultResourceLabel->text().contains(QStringLiteral("1.52 s"))
        || !resultResourceLabel->text().contains(QStringLiteral("96.0 MiB"))
        || !resultResourceLabel->text().contains(QStringLiteral("高开销")))
    {
        return fail(QStringLiteral("production-mode-selector 未显示当前生产结果与实际资源。"));
    }

    window.show();
    workspaceTabs->setCurrentWidget(configWorkspace);
    QApplication::processEvents();
    const QList<QSize> targetSizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    QStringList verifiedSizes;
    for (const QSize& targetSize : targetSizes)
    {
        window.resize(targetSize);
        QApplication::processEvents();
        if (window.width() > targetSize.width()
            || window.height() > targetSize.height()
            || !modePanel->isVisible()
            || modeCombo->width() < modeCombo->minimumSizeHint().width()
            || profileCombo->width() < profileCombo->minimumSizeHint().width())
        {
            return fail(
                QStringLiteral(
                    "production-mode-selector 中文文本在 %1x%2 被截断或模式面板不可见："
                    "window=%3x%4 panelVisible=%5 mode=%6/%7 profile=%8/%9。")
                    .arg(targetSize.width())
                    .arg(targetSize.height())
                    .arg(window.width())
                    .arg(window.height())
                    .arg(modePanel->isVisible())
                    .arg(modeCombo->width())
                    .arg(modeCombo->minimumSizeHint().width())
                    .arg(profileCombo->width())
                    .arg(profileCombo->minimumSizeHint().width()));
        }
        verifiedSizes.push_back(
            QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height()));
    }
    window.hide();

    return pass(
        QStringLiteral(
            "production-mode-selector default=legacy profiles=2 backendHidden=true sizes=%1")
            .arg(verifiedSizes.join(QStringLiteral(","))));
}
