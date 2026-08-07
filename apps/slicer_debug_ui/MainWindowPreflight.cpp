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

void MainWindow::OnModelPreflightStateChanged()
{
    UpdateModelPreflightUi();
    UpdateActionAvailability();
}

void MainWindow::OnPreflightActionAdmitted()
{
    const SlicePreflightAction action =
        m_slicePreflightCoordinator.AdmittedAction();
    if (action.kind == SlicePreflightActionKind::Legacy
        || action.kind == SlicePreflightActionKind::GlobalProduction)
    {
        config_editor_panel_->ShowProductionAdmissionState(
            ProductionAdmissionState::Admitted,
            QStringLiteral("当前模型与生效配置已通过所选产品模式准入。"));
        RunGeneratedConfig(action);
        return;
    }
    if (action.kind == SlicePreflightActionKind::OpenVdbDiagnostic)
    {
        RunOpenVdbDiagnostic(action.configpath, action.reportpath);
        return;
    }
    RunOpenVdbCandidate(action.configpath, action.packagedir);
}

void MainWindow::OnPreflightActionBlocked()
{
    m_productionRunSession.Invalidate();
    QStringList blockers;
    const slicer_core::ModelPreflightResult& result =
        m_modelPreflightController.CurrentExecution().result;
    const slicer_core::ModeAdmissionResult& admission =
        config_editor_panel_->SelectedProductionMode()
                == slicer_core::SlicePipelineMode::GlobalSurfaceShell
            ? result.globalAdmission
            : result.legacyAdmission;
    for (const std::string& code : admission.blockerCodes)
    {
        blockers.push_back(QString::fromStdString(code));
    }
    config_editor_panel_->ShowProductionAdmissionState(
        ProductionAdmissionState::Blocked,
        blockers.isEmpty()
            ? QStringLiteral("模型预检未放行。")
            : blockers.join(QStringLiteral("、")));
    status_label_->setText(QStringLiteral("模型预检未放行，切片进程未启动。"));
    UpdateActionAvailability();
}

void MainWindow::OnLegacyPreflightConfirmationRequired()
{
    const QString codes =
        m_slicePreflightCoordinator.PendingWarningCodes().join(QStringLiteral("、"));
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this,
        QStringLiteral("传统切片拓扑风险确认"),
        QStringLiteral(
            "模型预检发现拓扑警告。传统切片可兼容尝试，但结果不代表全局模式准入。\n\n"
            "警告码：%1\n\n是否继续传统切片？")
            .arg(codes),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    m_slicePreflightCoordinator.ConfirmLegacyWarning(
        answer == QMessageBox::Yes);
}

void MainWindow::OnRecheckModelPreflight()
{
    m_modelPreflightController.Recheck();
}

void MainWindow::OnCancelModelPreflight()
{
    m_slicePreflightCoordinator.CancelPending();
}

void MainWindow::OnSceneDocumentChanged()
{
    if (m_diagnosticAnalysisWorker.IsRunning()
        && m_activeDiagnosticAnalysisIdentity.has_value())
    {
        const std::uint64_t transformRevision =
            m_sceneDocument.Instance().has_value()
            ? m_sceneDocument.Instance()->transformrevision
            : 0U;
        if (m_activeDiagnosticAnalysisIdentity->sceneid
                != m_sceneDocument.SceneId()
            || m_activeDiagnosticAnalysisIdentity->instanceid
                != m_sceneDocument.CurrentInstanceId()
            || m_activeDiagnosticAnalysisIdentity->scenerevision
                != m_sceneDocument.SceneRevision()
            || m_activeDiagnosticAnalysisIdentity
                   ->transformrevision
                != transformRevision)
        {
            m_diagnosticAnalysisWorker.Cancel();
            m_lastDiagnosticAnalysisResult.reset();
            m_previewWorkspace->ClearDiagnosticAnalysis(
                QStringLiteral(
                    "当前场景、实例或变换已变化。"));
            m_diagnosticAnalysisMessage =
                QStringLiteral(
                    "当前场景、实例或变换已变化；"
                    "旧诊断结果已取消并标记为不可复用。");
        }
    }
    switch (m_sceneDocument.State())
    {
    case SceneDocumentState::Unloaded:
        status_label_->setText(QStringLiteral("模型俯视尚未加载。"));
        break;
    case SceneDocumentState::Loading:
        status_label_->setText(QStringLiteral("正在异步加载模型俯视：")
                               + m_sceneDocument.ModelPath());
        break;
    case SceneDocumentState::Ready:
        if (m_sceneSelectionModel.SelectedInstance().isEmpty()
            && m_sceneDocument.Instance().has_value())
        {
            m_sceneSelectionModel.SetSelectedInstance(
                QString::fromStdString(
                    m_sceneDocument.Instance()->instanceid));
        }
        status_label_->setText(
            !m_sceneDocument.Error().isEmpty()
                ? QStringLiteral("添加模型失败，原场景保持不变：")
                    + m_sceneDocument.Error()
                : m_sceneDocument.IsDirty()
                ? QStringLiteral(
                      "模型俯视已就绪；变换尚未保存，未启动切片。")
                : QStringLiteral("模型俯视已就绪；未启动切片。"));
        break;
    case SceneDocumentState::Blocked:
        if (m_sceneSelectionModel.SelectedInstance().isEmpty()
            && m_sceneDocument.Instance().has_value())
        {
            m_sceneSelectionModel.SetSelectedInstance(
                QString::fromStdString(
                    m_sceneDocument.Instance()->instanceid));
        }
        status_label_->setText(
            QStringLiteral("模型可查看，但预检状态为 blocked。"));
        break;
    case SceneDocumentState::Failed:
        status_label_->setText(
            QStringLiteral("模型俯视加载失败：")
            + m_sceneDocument.Error());
        log_panel_->appendError(m_sceneDocument.Error());
        break;
    case SceneDocumentState::Cancelled:
        status_label_->setText(QStringLiteral("模型俯视加载已取消。"));
        break;
    }
    RequestTextureWhitePreflight();
    UpdateActionAvailability();
}

void MainWindow::OnTextureWhitePreflightFinished(
    const TextureWhitePreflightResult& result)
{
    m_lastTextureWhitePreflightResult = result;
    if (!result.HasWarning())
    {
        return;
    }

    warnings_view_->setPlainText(result.warningmessage);
    status_label_->setText(
        QStringLiteral(
            "切片前预检发现纯白纹理风险；当前告警为保守判断，"
            "生产材料闭合校验仍是最终真源。"));
    log_panel_->appendOutput(
        QStringLiteral("纹理纯白预检警告：")
        + result.warningmessage);
}

void MainWindow::OnSaveSceneTransform()
{
    if (!m_sceneDocument.Instance().has_value())
    {
        status_label_->setText(QStringLiteral("没有可保存的模型场景。"));
        return;
    }

    const SliceSettingsState settings = BuildCurrentSettings(
        m_sceneDocument.ModelPath(),
        {},
        SliceEngineRole::LegacyProduction);
    const QString sessionName =
        QStringLiteral("model_scene_%1")
            .arg(QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    SceneTransformSaveRequest request;
    request.sessiondirectory =
        std::filesystem::path(
            QDir(paths_.repo_root)
                .filePath(QStringLiteral("output/ui_sessions/")
                          + sessionName)
                .toStdWString());
    request.sourceprofileid =
        settings.profileid.trimmed().isEmpty()
        ? std::string("custom")
        : settings.profileid.toStdString();
    request.generatedatutc =
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs)
            .toStdString();
    request.dpix = settings.dpix;
    request.dpiy = settings.dpiy;
    request.layerheightmm = settings.layerthicknessmm;
    request.slicepipelinemode =
        slicer_core::SlicePipelineModeName(
            config_editor_panel_->SelectedProductionMode());
    request.expectedscenerevision =
        m_sceneDocument.SceneRevision();
    request.expectedtransformrevision =
        m_sceneDocument.Instance()->transformrevision;

    const SceneTransformSaveResult saved =
        m_sceneTransformController.SaveSceneEffectiveConfig(request);
    if (!saved.IsValid())
    {
        status_label_->setText(
            QString::fromLatin1(
                SceneTransformErrorCodeName(saved.error->code).data())
            + QStringLiteral("：")
            + saved.error->message);
        return;
    }
    status_label_->setText(
        QStringLiteral("场景配置已保存并回读：")
        + QString::fromStdWString(
            saved.effectiveconfigpath.wstring()));
}

void MainWindow::OnSliceCurrentScene()
{
    const QString textureWhiteWarning =
        CurrentTextureWhitePreflightWarning();
    if (!textureWhiteWarning.isEmpty())
    {
        QMessageBox::warning(
            this,
            QStringLiteral("纹理纯白预检"),
            textureWhiteWarning
                + QStringLiteral(
                    "\n\n该告警不会阻断切片；继续后将由生产材料闭合校验"
                    "判定最终结果。"));
    }
    SceneSliceActionRequest request;
    request.mode =
        config_editor_panel_->SelectedProductionMode();
    m_sceneSliceActionController.Start(request);
}

void MainWindow::handleProcessStarted(const QString& command) {
    setBusy(true);
    status_label_->setText("正在执行：" + current_action_);
    log_panel_->appendCommand(command);
    m_sliceProgressParser.Reset();
    m_lastSliceTimingEvent.reset();
    if (m_sliceTimingPanel != nullptr && IsSlicingAction(current_action_))
    {
        m_sliceTimingPanel->Reset(current_action_);
        if (m_diagnosticsDock != nullptr)
        {
            m_diagnosticsDock->ShowView(
                m_sliceTimingPanel,
                false);
        }
    }
}

void MainWindow::handleProcessFinished(const int exit_code, const qint64 elapsed_ms) {
    log_panel_->appendResult(exit_code, elapsed_ms);
    setBusy(false);
    if (current_action_ == QStringLiteral("切片当前场景"))
    {
        m_sceneSliceActionController.OnProcessFinished(
            exit_code,
            elapsed_ms);
        if (m_sliceTimingPanel != nullptr)
        {
            m_sliceTimingPanel->Finish(
                m_sceneSliceActionController.State()
                    == SceneSliceActionState::Completed,
                elapsed_ms);
        }
        return;
    }
    ProductionSliceRunCompletion productionCompletion;
    if (current_action_ == QStringLiteral("运行切片"))
    {
        productionCompletion = m_productionRunSession.Complete(exit_code);
    }
    bool resultAccepted = exit_code == 0;
    std::optional<PackageSummary> productionPackage;
    if (resultAccepted
        && current_action_ == QStringLiteral("运行切片")
        && !productionCompletion.request.has_value())
    {
        resultAccepted = false;
        log_panel_->appendError(
            QStringLiteral("生产切片进程缺少当前 session 身份，拒绝加载输出包。"));
    }
    if (resultAccepted
        && current_action_ == QStringLiteral("运行切片")
        && productionCompletion.request.has_value())
    {
        productionPackage =
            package_loader_.load(productionCompletion.packagedirtoload);
        ProductionPackageResultRequest validationRequest;
        validationRequest.runrequest = *productionCompletion.request;
        validationRequest.package = *productionPackage;
        validationRequest.measuredtotalms =
            m_lastSliceTimingEvent.has_value()
                && m_lastSliceTimingEvent->totalms > 0.0
            ? std::optional<double>{m_lastSliceTimingEvent->totalms}
            : std::optional<double>{static_cast<double>(elapsed_ms)};
        if (m_lastSliceTimingEvent.has_value()
            && m_lastSliceTimingEvent->memoryavailable)
        {
            validationRequest.measuredpeakworkingsetbytes =
                m_lastSliceTimingEvent->peakworkingsetbytes;
        }
        const ProductionPackageResult validation =
            m_productionPackageResultValidator.Validate(validationRequest);
        config_editor_panel_->ShowProductionResult(validation.presentation);
        if (!validation.valid)
        {
            resultAccepted = false;
            log_panel_->appendError(
                QStringLiteral("生产结果校验失败：\n")
                + validation.errors.join(QStringLiteral("\n")));
        }
    }
    status_label_->setText(
        resultAccepted
            ? QStringLiteral("通过：") + current_action_
            : QStringLiteral("失败：") + current_action_);
    if (m_sliceTimingPanel != nullptr && IsSlicingAction(current_action_))
    {
        m_sliceTimingPanel->Finish(resultAccepted, elapsed_ms);
    }
    if (exit_code != 0) {
        if (current_action_ == "OpenVDB 候选切片" && !pending_package_.isEmpty())
        {
            package_edit_->setText(pending_package_);
            loadPackage(pending_package_);
            status_label_->setText("失败：OpenVDB 候选切片（已加载诊断报告）");
        }
        return;
    }
    if (!resultAccepted)
    {
        status_label_->setText(
            QStringLiteral("失败：生产包身份或协议校验未通过，未加载预览与报告"));
        return;
    }
    if (current_action_ == QStringLiteral("运行切片")
        && productionPackage.has_value())
    {
        package_edit_->setText(productionCompletion.packagedirtoload);
        LoadPackageSummary(*productionPackage);
    }
    else if (current_action_ == "OpenVDB 候选切片" && !pending_package_.isEmpty()) {
        package_edit_->setText(pending_package_);
        loadPackage(pending_package_);
    } else if (current_action_ == "对比工艺配置") {
        loadCompareResult(compare_output_);
    } else if (current_action_ == "OpenVDB 实验诊断") {
        loadCompareResult(compare_output_);
    }
}

void MainWindow::handleProcessFailed(const QString& message) {
    log_panel_->appendError(message);
    setBusy(false);
    if (current_action_ == QStringLiteral("切片当前场景"))
    {
        m_sceneSliceActionController.OnProcessFailed(message);
        if (m_sliceTimingPanel != nullptr)
        {
            m_sliceTimingPanel->Finish(false, 0);
        }
        return;
    }
    status_label_->setText("进程错误：" + message);
    m_productionRunSession.Invalidate();
    if (m_sliceTimingPanel != nullptr && IsSlicingAction(current_action_))
    {
        m_sliceTimingPanel->Finish(false, 0);
    }
}
