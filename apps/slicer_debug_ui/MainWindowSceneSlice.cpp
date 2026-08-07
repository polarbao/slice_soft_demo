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

QString MainWindow::CurrentTextureWhitePreflightWarning() const
{
    if (!m_lastTextureWhitePreflightResult.has_value())
    {
        return {};
    }

    const TextureWhitePreflightResult& result =
        *m_lastTextureWhitePreflightResult;
    const bool identityMatches =
        result.sceneid == m_sceneDocument.SceneId()
        && result.scenerevision == m_sceneDocument.SceneRevision()
        && result.contenthash
            == BuildTextureWhitePreflightContentHash()
        && result.profileid == m_currentProfileId;
    if (!identityMatches
        || !result.containsstrictwhite
        || CurrentProfileCapabilities().contains(
            QStringLiteral("unprintable_white_underbase")))
    {
        return {};
    }

    return QStringLiteral("scene=%1，revision=%2，Profile=%3\n%4")
        .arg(
            result.sceneid,
            QString::number(result.scenerevision),
            result.profileid,
            result.warningmessage);
}

SceneSliceActionSceneState MainWindow::BuildSceneSliceState() const
{
    SceneSliceActionSceneState state;
    state.sceneid = m_sceneDocument.SceneId();
    state.scenerevision = m_sceneDocument.SceneRevision();
    state.importinprogress =
        m_sceneBatchImportController.IsRunning()
        || m_sceneDocument.State() == SceneDocumentState::Loading;
    state.allvisibleinstancesadmitted = true;
    for (const SceneDocumentItem& item : m_sceneDocument.Items())
    {
        if (!item.instance.visible)
        {
            continue;
        }
        ++state.visibleinstancecount;
        if (!item.geometry.has_value()
            || item.geometry->admissionstatus
                != slicer_core::SceneViewAdmissionStatus::Admitted)
        {
            state.allvisibleinstancesadmitted = false;
        }
    }
    if (state.visibleinstancecount == 0U)
    {
        state.allvisibleinstancesadmitted = false;
    }
    return state;
}

slicer_core::SceneBuildVolume
MainWindow::BuildFunctionalSceneVolume() const
{
    double maximumAbsX{0.0};
    double maximumAbsY{0.0};
    for (const SceneDocumentItem& item : m_sceneDocument.Items())
    {
        if (!item.instance.visible)
        {
            continue;
        }
        const slicer_core::BoundingBox& bounds =
            item.instance.effectivebboxmm;
        maximumAbsX = std::max(
            maximumAbsX,
            std::max(
                std::abs(bounds.min.x),
                std::abs(bounds.max.x)));
        maximumAbsY = std::max(
            maximumAbsY,
            std::max(
                std::abs(bounds.min.y),
                std::abs(bounds.max.y)));
    }

    constexpr double kFixtureMarginMm{1.0};
    slicer_core::SceneBuildVolume volume;
    volume.source = slicer_core::BuildVolumeSource::Fixture;
    volume.widthmm = std::max(
        1.0,
        2.0 * (maximumAbsX + kFixtureMarginMm));
    volume.heightmm = std::max(
        1.0,
        2.0 * (maximumAbsY + kFixtureMarginMm));
    volume.origin = slicer_core::BuildVolumeOrigin::Center;
    volume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.isfixture = true;
    return volume;
}

SceneSliceSnapshotResult MainWindow::WriteCurrentSceneSnapshot(
    const SceneSliceActionRequest& request)
{
    SceneSliceSnapshotResult result;
    if (!m_sceneDocument.Instance().has_value())
    {
        result.errorcode =
            QStringLiteral("SCENE_SLICE_SCENE_UNAVAILABLE");
        result.message =
            QStringLiteral("当前场景没有可保存的模型实例。");
        return result;
    }
    if (request.mode
        != slicer_core::SlicePipelineMode::Legacy)
    {
        result.errorcode =
            QStringLiteral(
                "SCENE_SLICE_PIPELINE_MODE_NOT_ADMITTED");
        result.message =
            QStringLiteral(
                "Global 多模型生产尚未准入，且禁止回退到 Legacy。");
        return result;
    }

    const EffectiveConfigResult profile =
        GenerateEffectiveConfig(
            m_sceneDocument.ModelPath(),
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("scene_legacy"));
    if (!profile.IsValid())
    {
        result.errorcode =
            QStringLiteral("SCENE_SLICE_SNAPSHOT_FAILED");
        result.message =
            profile.errors.isEmpty()
            ? QStringLiteral("场景 Profile 生效配置生成失败。")
            : profile.errors.join(QStringLiteral("\n"));
        return result;
    }

    const QJsonObject profileRoot = profile.document.object();
    const QString profileId =
        ResolveEffectiveProfileId(
            profileRoot,
            m_currentProfileId);
    const QString packageDir = absoluteFromRepo(
        profileRoot
            .value(QStringLiteral("output"))
            .toObject()
            .value(QStringLiteral("packageDir"))
            .toString());
    if (profileId.trimmed().isEmpty()
        || packageDir.trimmed().isEmpty())
    {
        result.errorcode =
            QStringLiteral("SCENE_SLICE_SNAPSHOT_FAILED");
        result.message =
            QStringLiteral(
                "场景 Profile 缺少材料工艺身份或输出 Package。");
        return result;
    }

    const QFileInfo profileInfo(profile.generatedconfigpath);
    SceneTransformSaveRequest saveRequest;
    saveRequest.sessiondirectory =
        std::filesystem::path(
            profileInfo.absolutePath().toStdWString());
    saveRequest.sourceprofileconfigpath =
        std::filesystem::path(
            profileInfo.absoluteFilePath().toStdWString());
    saveRequest.outputpackagedir =
        std::filesystem::path(packageDir.toStdWString());
    saveRequest.sourceprofileid = profileId.toStdString();
    saveRequest.generatedatutc =
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs)
            .toStdString();
    saveRequest.buildvolume =
        BuildFunctionalSceneVolume();
    saveRequest.dpix =
        profileRoot.value(QStringLiteral("output"))
            .toObject()
            .value(QStringLiteral("dpiX"))
            .toInt(slicer_core::kDefaultOutputDpiX);
    saveRequest.dpiy =
        profileRoot.value(QStringLiteral("output"))
            .toObject()
            .value(QStringLiteral("dpiY"))
            .toInt(slicer_core::kDefaultOutputDpiY);
    saveRequest.layerheightmm =
        profileRoot.value(QStringLiteral("output"))
            .toObject()
            .value(QStringLiteral("layerThicknessMm"))
            .toDouble(
                slicer_core::kDefaultLayerThicknessMm);
    saveRequest.slicepipelinemode =
        slicer_core::SlicePipelineModeName(request.mode);
    saveRequest.expectedscenerevision =
        m_sceneDocument.SceneRevision();
    saveRequest.expectedtransformrevision =
        m_sceneDocument.Instance()->transformrevision;
    saveRequest.production = false;

    const SceneTransformSaveResult saved =
        m_sceneTransformController.SaveSceneEffectiveConfig(
            saveRequest);
    if (!saved.IsValid())
    {
        result.errorcode =
            QString::fromLatin1(
                SceneTransformErrorCodeName(
                    saved.error->code)
                    .data());
        result.message = saved.error->message;
        return result;
    }

    SceneSliceActionSnapshot snapshot;
    snapshot.sceneid =
        QString::fromStdString(saved.scene.sceneid);
    snapshot.scenerevision = saved.scene.scenerevision;
    snapshot.scenehash =
        QString::fromStdString(
            slicer_core::ComputeMultiModelSceneHash(
                saved.scene));
    snapshot.effectiveconfighash =
        QString::fromStdString(saved.confighash);
    snapshot.effectiveconfigpath =
        QString::fromStdWString(
            saved.effectiveconfigpath.wstring());
    snapshot.profileid = profileId;
    snapshot.sessionid =
        QFileInfo(snapshot.effectiveconfigpath)
            .absoluteDir()
            .dirName();
    snapshot.packagedir = packageDir;
    snapshot.mode = request.mode;
    result.snapshot = std::move(snapshot);
    return result;
}

SceneSlicePackageValidationResult
MainWindow::ValidateCurrentScenePackage(
    const SceneSliceActionSnapshot& snapshot,
    const qint64 elapsedMs)
{
    SceneSlicePackageValidationResult result;
    const PackageSummary package =
        package_loader_.load(snapshot.packagedir);
    ProductionPackageResultRequest request;
    request.runrequest.mode = snapshot.mode;
    request.runrequest.profileid = snapshot.profileid;
    request.runrequest.sessionid = snapshot.sessionid;
    request.runrequest.configpath =
        snapshot.effectiveconfigpath;
    request.runrequest.packagedir = snapshot.packagedir;
    request.package = package;
    request.measuredtotalms =
        m_lastSliceTimingEvent.has_value()
            && m_lastSliceTimingEvent->totalms > 0.0
        ? std::optional<double>{
              m_lastSliceTimingEvent->totalms}
        : std::optional<double>{
              static_cast<double>(elapsedMs)};
    if (m_lastSliceTimingEvent.has_value()
        && m_lastSliceTimingEvent->memoryavailable)
    {
        request.measuredpeakworkingsetbytes =
            m_lastSliceTimingEvent->peakworkingsetbytes;
    }
    request.expectedsceneid = snapshot.sceneid;
    request.expectedscenerevision =
        snapshot.scenerevision;
    request.expectedscenehash = snapshot.scenehash;
    const ProductionPackageResult validation =
        m_productionPackageResultValidator.Validate(request);
    config_editor_panel_->ShowProductionResult(
        validation.presentation);
    result.valid = validation.valid;
    result.packagedir = snapshot.packagedir;
    result.errors = validation.errors;
    return result;
}

void MainWindow::UpdateActionAvailability()
{
    const bool preflightRunning = m_modelPreflightController.IsRunning();
    const bool batchImportRunning =
        m_sceneBatchImportController.IsRunning();
    const bool enabled =
        !m_processBusy
        && !preflightRunning
        && !batchImportRunning;
    build_button_->setEnabled(enabled);
    m_importModelPreviewButton->setEnabled(enabled);
    m_cancelModelImportButton->setEnabled(batchImportRunning);
    const SceneSliceActionSceneState sceneSliceState =
        BuildSceneSliceState();
    const bool sceneModeAdmitted =
        config_editor_panel_->SelectedProductionMode()
        == slicer_core::SlicePipelineMode::Legacy;
    const bool canSliceScene =
        enabled
        && sceneSliceState.visibleinstancecount > 0U
        && sceneSliceState.allvisibleinstancesadmitted
        && sceneModeAdmitted;
    const QString sceneSliceReason =
        batchImportRunning
        ? QStringLiteral("批量导入完成后才能切片当前场景。")
        : sceneSliceState.visibleinstancecount == 0U
        ? QStringLiteral("请先导入至少一个可见模型。")
        : !sceneSliceState.allvisibleinstancesadmitted
        ? QStringLiteral(
              "当前场景存在未通过预检的可见模型。")
        : !sceneModeAdmitted
        ? QStringLiteral(
              "Global 多模型生产尚未准入，禁止回退到 Legacy。")
        : QStringLiteral(
              "冻结当前 SceneDocument，并通过显式 "
              "--scene-config 生成一个 RGBWSV Package。");
    SceneActionBarPresentation actionPresentation;
    actionPresentation.canimport = enabled;
    actionPresentation.cansave =
        enabled && m_sceneDocument.InstanceCount() > 0U;
    actionPresentation.canslice = canSliceScene;
    actionPresentation.cancancel =
        m_sceneSliceActionController.IsRunning();
    actionPresentation.canselectprofile = enabled;
    actionPresentation.modelabel =
        config_editor_panel_->SelectedProductionMode()
            == slicer_core::SlicePipelineMode::Legacy
        ? QStringLiteral("传统切片")
        : QStringLiteral("Global Surface Shell");
    const QString selectedProductionProfile =
        config_editor_panel_->SelectedProductionProfileId();
    actionPresentation.profilelabel =
        !selectedProductionProfile.trimmed().isEmpty()
        ? selectedProductionProfile
        : m_currentProfileId.trimmed().isEmpty()
        ? QStringLiteral("自定义")
        : m_currentProfileId;
    actionPresentation.statustext =
        m_sceneSliceActionController.IsRunning()
        ? m_sceneSliceActionController.Message()
        : QStringLiteral("场景 %1 / 可见 %2")
              .arg(m_sceneDocument.SceneRevision())
              .arg(sceneSliceState.visibleinstancecount);
    actionPresentation.savereason =
        actionPresentation.cansave
        ? QStringLiteral("保存当前 SceneDocument 的场景和变换配置。")
        : QStringLiteral("请先导入至少一个模型。");
    actionPresentation.slicereason = sceneSliceReason;
    m_sceneActionBar->SetPresentation(actionPresentation);
    m_contextInspector->SetSliceSettingsSummary(
        actionPresentation.modelabel,
        actionPresentation.profilelabel,
        sceneSliceReason);
    UpdateDiagnosticSettingsPresentation();
    m_importSliceButton->setEnabled(enabled);
    m_importOpenVdbButton->setEnabled(enabled);
    m_importOpenVdbCandidateButton->setEnabled(enabled);
    const bool sceneProductionBlocked =
        m_sceneDocument.Instance().has_value();
    run_slicer_button_->setEnabled(
        enabled && !sceneProductionBlocked);
    run_slicer_button_->setToolTip(
        sceneProductionBlocked
            ? [&]()
              {
                  if (m_sceneDocument.TransformedPreflightState()
                          != SceneTransformedPreflightState::Ready
                      || !m_sceneDocument.TransformedPreflight()
                              .has_value())
                  {
                      return QStringLiteral(
                          "变换后预检尚未完成，禁止使用旧配置启动生产切片。");
                  }
                  const auto& result =
                      m_sceneDocument.TransformedPreflight()
                          ->transformed.result;
                  const bool global =
                      config_editor_panel_->SelectedProductionMode()
                      == slicer_core::SlicePipelineMode::
                          GlobalSurfaceShell;
                  const auto admission =
                      global ? result.globalAdmission.status
                             : result.legacyAdmission.status;
                  if (admission
                      == slicer_core::
                          ModelPreflightAdmissionStatus::Blocked)
                  {
                      return QStringLiteral(
                          "变换后模型未通过当前模式预检，禁止生产切片。");
                  }
                  return QStringLiteral(
                      "变换后预检已完成；场景生产消费将在 13B "
                      "联合切片入口接通，当前仍禁止旧配置绕行。");
              }()
            : QString());
    run_rip_button_->setEnabled(enabled);
    regression_button_->setEnabled(enabled);
    compare_button_->setEnabled(enabled);
    if (m_modelPreflightRecheckButton != nullptr)
    {
        m_modelPreflightRecheckButton->setEnabled(
            enabled && m_modelPreflightController.CurrentExecution().result.status
                != slicer_core::ModelPreflightStatus::NotRun);
    }
}
