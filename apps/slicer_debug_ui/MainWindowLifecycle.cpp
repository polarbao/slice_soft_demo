#include "MainWindowInternal.h"

using slicer_debug_ui_internal::ApplyStoredGlobalTextureOverride;
using slicer_debug_ui_internal::BuildProductionSessionName;
using slicer_debug_ui_internal::ConfigureLongTextCombo;
using slicer_debug_ui_internal::IsSlicingAction;
using slicer_debug_ui_internal::MakeDefaultModelFillConfig;
using slicer_debug_ui_internal::MakeDefaultOuterVarnishConfig;
using slicer_debug_ui_internal::MakeDefaultPreviewPseudoColors;
using slicer_debug_ui_internal::MakeDefaultSupportConfig;
using slicer_debug_ui_internal::MakeDefaultSurfaceVarnishConfig;
using slicer_debug_ui_internal::MakeIntArray;
using slicer_debug_ui_internal::MakeNumberArray;
using slicer_debug_ui_internal::MakeScenarioDisplayLabel;
using slicer_debug_ui_internal::MakeScenarioToolTip;
using slicer_debug_ui_internal::MakeStringArray;
using slicer_debug_ui_internal::MaterialCapabilityLabel;
using slicer_debug_ui_internal::ParseSupportPlacement;
using slicer_debug_ui_internal::ProductionModeSessionTag;
using slicer_debug_ui_internal::ProductionSafetyLabel;
using slicer_debug_ui_internal::ResolveEffectiveProfileId;
using slicer_debug_ui_internal::ResolveModelPath;
using slicer_debug_ui_internal::SanitizeSessionName;
using slicer_debug_ui_internal::SessionIdFromConfigPath;
using slicer_debug_ui_internal::StoreGlobalTextureOverride;
using slicer_debug_ui_internal::UpdateComboPopupWidth;
using slicer_debug_ui_internal::addPathRow;
using slicer_debug_ui_internal::makeButton;
using slicer_debug_ui_internal::makePathEdit;

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_diagnosticAnalysisWorker.IsRunning())
    {
        m_diagnosticAnalysisWorker.Cancel();
    }
    if (WorkspaceLayoutState::PersistenceEnabled())
    {
        QSettings settings(
            WorkspaceLayoutState::OrganizationName(),
            WorkspaceLayoutState::ApplicationName());
        WorkspaceLayoutState::Save(
            settings,
            this,
            m_mainSplitter,
            m_contextInspector);
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::browseConfig() {
    const QString path = QFileDialog::getOpenFileName(this, "选择配置文件", paths_.repo_root, "JSON (*.json)");
    if (!path.isEmpty()) {
        m_currentProfileId.clear();
        if (m_scenarioSelector != nullptr)
        {
            m_scenarioSelector->setCurrentIndex(0);
        }
        config_edit_->setText(path);
        config_editor_panel_->loadConfig(path);
    }
}

void MainWindow::browsePackage() {
    const QString path = QFileDialog::getExistingDirectory(this, "选择输出包目录", paths_.repo_root);
    if (!path.isEmpty()) {
        package_edit_->setText(path);
        loadPackage(path);
    }
}

void MainWindow::browseProfileA() {
    const QString path = QFileDialog::getExistingDirectory(this, "选择对比包 A", paths_.repo_root);
    if (!path.isEmpty()) {
        profile_a_edit_->setText(path);
    }
}

void MainWindow::browseProfileB() {
    const QString path = QFileDialog::getExistingDirectory(this, "选择对比包 B", paths_.repo_root);
    if (!path.isEmpty()) {
        profile_b_edit_->setText(path);
    }
}

void MainWindow::buildDebug() {
    runCommand("构建调试版", "cmake", QStringList{"--build", "build", "--config", "Debug"});
}

void MainWindow::runSlicer() {
    const slicer_core::SlicePipelineMode selectedMode =
        config_editor_panel_->SelectedProductionMode();
    const EffectiveConfigResult result = GenerateEffectiveConfig(
        QString{},
        package_edit_->text(),
        SliceEngineRole::LegacyProduction,
        ProductionModeSessionTag(selectedMode));
    if (!result.IsValid())
    {
        status_label_->setText("生效配置校验失败，已停止切片。");
        log_panel_->appendError(result.errors.join("\n"));
        return;
    }

    SlicePreflightAction action;
    action.kind = selectedMode == slicer_core::SlicePipelineMode::Legacy
        ? SlicePreflightActionKind::Legacy
        : SlicePreflightActionKind::GlobalProduction;
    action.productionmode = selectedMode;
    action.productionprofileid =
        config_editor_panel_->SelectedProductionProfileId();
    action.configpath = result.generatedconfigpath;
    action.sessionid = SessionIdFromConfigPath(action.configpath);
    action.packagedir = absoluteFromRepo(
        result.document.object().value("output").toObject().value("packageDir").toString());
    action.capabilityprogram = paths_.openvdb_slicer_cli;
    RequestSlicePreflight(action);
}

void MainWindow::runRipSummary() {
    const QString package = absoluteFromRepo(package_edit_->text());
    runCommand("运行 RIP 摘要", paths_.rip_reader, QStringList{"--package", package, "--summary"});
}

void MainWindow::runQuickRegression() {
    runCommand("运行快速回归", paths_.powershell,
               QStringList{"-ExecutionPolicy", "Bypass", "-File", "scripts/run_regression.ps1", "-Mode", "quick"});
}

void MainWindow::compareProfiles() {
    const QString package_a = absoluteFromRepo(profile_a_edit_->text());
    const QString package_b = absoluteFromRepo(profile_b_edit_->text());
    compare_output_ = QDir(paths_.repo_root).filePath("output/MaterialProfileCompare_ui.json");
    runCommand("对比工艺配置", paths_.powershell,
               QStringList{"-ExecutionPolicy",
                           "Bypass",
                           "-File",
                           "scripts/compare_material_profiles.ps1",
                           "-PackageA",
                           package_a,
                           "-PackageB",
                           package_b,
                           "-Output",
                           compare_output_});
}

void MainWindow::openOutputFolder() {
    const QString package = absoluteFromRepo(package_edit_->text());
    QDesktopServices::openUrl(QUrl::fromLocalFile(package));
}

void MainWindow::loadPackageFromEdit() {
    loadPackage(package_edit_->text());
}

void MainWindow::OnImportModelPreview()
{
    if (m_sceneBatchImportController.IsRunning()
        || m_modelTopViewLoader.IsRunning()
        || (m_sceneDocument.InstanceCount() > 0U
            && m_sceneDocument.IsGeometryStale()))
    {
        status_label_->setText(
            QStringLiteral(
                "当前场景正在加载或重投影，"
                "请等待俯视更新完成后再添加模型。"));
        return;
    }
    if (m_sceneDocument.InstanceCount() >= 22U)
    {
        status_label_->setText(
            QStringLiteral(
                "SCENE_INSTANCE_LIMIT_EXCEEDED：场景最多允许 22 个模型实例。"));
        return;
    }
    const QStringList modelPaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择一个或多个模型"),
        paths_.repo_root,
        QStringLiteral("Model (*.obj *.stl *.3mf)"));
    if (modelPaths.isEmpty())
    {
        return;
    }

    if (m_transformedPreflightLoader.IsRunning())
    {
        m_transformedPreflightLoader.Cancel();
    }
    if (m_sceneDocument.InstanceCount() == 0U)
    {
        m_sceneSelectionModel.Clear();
    }
    m_mainWorkspaceTabs->setCurrentWidget(m_modelTopViewWorkspace);
    m_contextInspector->ShowScenePage();

    SceneBatchImportRequest request;
    request.batchid =
        QStringLiteral("ui-")
        + QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    request.configpath =
        absoluteFromRepo(config_edit_->text());
    request.files = modelPaths;
    request.autolayout = true;
    const SceneBatchImportStartResult result =
        m_sceneBatchImportController.Start(request);
    if (!result.IsValid())
    {
        status_label_->setText(
            QString::fromLatin1(
                SceneBatchImportStartErrorCodeName(
                    result.error->code)
                    .data())
            + QStringLiteral("：")
            + result.error->message);
    }
}

void MainWindow::OnImportModelAndSlice()
{
    const QString modelPath =
        QFileDialog::getOpenFileName(this, "选择要一键切片的模型", paths_.repo_root, "Model (*.obj *.stl *.3mf)");
    if (modelPath.isEmpty())
    {
        return;
    }

    QString packageDir;
    const QString configPath = CreateOneClickConfig(modelPath, &packageDir);
    if (configPath.isEmpty())
    {
        return;
    }

    SlicePreflightAction action;
    const slicer_core::SlicePipelineMode selectedMode =
        config_editor_panel_->SelectedProductionMode();
    action.kind = selectedMode == slicer_core::SlicePipelineMode::Legacy
        ? SlicePreflightActionKind::Legacy
        : SlicePreflightActionKind::GlobalProduction;
    action.productionmode = selectedMode;
    action.productionprofileid =
        config_editor_panel_->SelectedProductionProfileId();
    action.sessionid = SessionIdFromConfigPath(configPath);
    action.configpath = configPath;
    action.packagedir = packageDir;
    action.capabilityprogram = paths_.openvdb_slicer_cli;
    RequestSlicePreflight(action);
}

void MainWindow::OnImportModelOpenVdbDiagnostic()
{
    const QString modelPath =
        QFileDialog::getOpenFileName(this, "选择要执行 OpenVDB 实验诊断的模型", paths_.repo_root, "Model (*.obj *.stl *.3mf)");
    if (modelPath.isEmpty())
    {
        return;
    }

    const EffectiveConfigResult result = GenerateEffectiveConfig(
        modelPath,
        QString{},
        SliceEngineRole::OpenVdbUtilityCandidate,
        QStringLiteral("openvdb_diagnostic"));
    if (!result.IsValid())
    {
        status_label_->setText("OpenVDB 诊断配置校验失败。");
        log_panel_->appendError(result.errors.join("\n"));
        return;
    }

    SlicePreflightAction action;
    action.kind = SlicePreflightActionKind::OpenVdbDiagnostic;
    action.configpath = result.generatedconfigpath;
    action.reportpath = CreateOpenVdbReportPath(modelPath);
    action.capabilityprogram = paths_.openvdb_slicer_cli;
    RequestSlicePreflight(action);
}

void MainWindow::OnImportModelOpenVdbCandidate()
{
    const QString modelPath =
        QFileDialog::getOpenFileName(this, "选择要执行 OpenVDB 候选切片的模型", paths_.repo_root, "Model (*.obj *.3mf)");
    if (modelPath.isEmpty())
    {
        return;
    }

    QString packageDir;
    const QString configPath = CreateOpenVdbCandidateConfig(modelPath, &packageDir);
    if (configPath.isEmpty())
    {
        return;
    }

    SlicePreflightAction action;
    action.kind = SlicePreflightActionKind::OpenVdbCandidate;
    action.configpath = configPath;
    action.packagedir = packageDir;
    action.capabilityprogram = paths_.openvdb_slicer_cli;
    RequestSlicePreflight(action);
}

void MainWindow::OnScenarioChanged(const int index)
{
    if (index < 0 || m_scenarioSelector == nullptr)
    {
        return;
    }

    const QString scenarioId = m_scenarioSelector->itemData(index).toString();
    if (m_sceneActionBar != nullptr)
    {
        m_sceneActionBar->SetSelectedProfileId(
            scenarioId);
    }
    const QString itemToolTip = m_scenarioSelector->itemData(index, Qt::ToolTipRole).toString();
    if (!itemToolTip.isEmpty())
    {
        m_scenarioSelector->setToolTip(itemToolTip);
    }
    if (scenarioId.isEmpty())
    {
        if (m_scenarioDescriptionLabel != nullptr)
        {
            m_scenarioDescriptionLabel->setText("自定义路径：手动选择配置文件和输出包。");
        }
        return;
    }

    const ScenarioEntry* scenario = m_scenarioRegistry.FindById(scenarioId);
    if (scenario == nullptr)
    {
        return;
    }

    ApplyScenario(*scenario);
}

void MainWindow::OnQuickProfileRequested(
    const QString& profileId)
{
    if (profileId.trimmed().isEmpty()
        || m_scenarioSelector == nullptr)
    {
        return;
    }
    const int index =
        m_scenarioSelector->findData(profileId);
    if (index < 0)
    {
        status_label_->setText(
            QStringLiteral("未找到生产 Profile：")
            + profileId);
        return;
    }
    if (m_scenarioSelector->currentIndex() == index)
    {
        OnScenarioChanged(index);
        return;
    }
    m_scenarioSelector->setCurrentIndex(index);
}

void MainWindow::OnReloadScenarios()
{
    LoadScenarios();
}

void MainWindow::OnScenarioVisibilityChanged(const bool checked)
{
    Q_UNUSED(checked);
    LoadScenarios();
}

void MainWindow::OnProcessOutput(const QString& text)
{
    log_panel_->appendOutput(text);
    const SliceProtocolUpdate update = m_sliceProgressParser.Append(text);
    if (m_sliceTimingPanel == nullptr)
    {
        return;
    }
    for (const SliceProgressEvent& event : update.progress)
    {
        m_sliceTimingPanel->UpdateProgress(event);
    }
    for (const SliceTimingEvent& event : update.timings)
    {
        m_lastSliceTimingEvent = event;
        m_sliceTimingPanel->ShowTiming(event);
    }
}
