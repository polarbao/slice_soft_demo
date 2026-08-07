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

std::optional<DiagnosticAnalysisRequest>
MainWindow::BuildDiagnosticAnalysisRequest(
    QString* errorMessage)
{
    if (!m_sceneDocument.Instance().has_value()
        || m_sceneDocument.SourceCacheKey().isEmpty())
    {
        *errorMessage =
            QStringLiteral("当前没有可诊断的模型实例。");
        return std::nullopt;
    }
    const std::optional<SceneModelRepositoryEntry> source =
        m_sceneModelRepository.Find(
            m_sceneDocument.SourceCacheKey());
    if (!source.has_value() || source->model == nullptr)
    {
        *errorMessage =
            QStringLiteral("当前实例的不可变源模型缓存不存在。");
        return std::nullopt;
    }

    QString profileId = m_currentProfileId.trimmed();
    if (profileId.isEmpty())
    {
        profileId = config_document_
            .value(
                {"materialProcessProfile", "name"})
            .toString()
            .trimmed();
    }
    if (profileId.isEmpty())
    {
        *errorMessage =
            QStringLiteral(
                "当前配置缺少 materialProcessProfile.name。");
        return std::nullopt;
    }

    const SceneTransformSnapshotResult snapshot =
        m_sceneTransformController.BuildCurrentScene(
            profileId.toStdString(),
            BuildFunctionalSceneVolume());
    if (!snapshot.IsValid())
    {
        *errorMessage = snapshot.error.has_value()
            ? snapshot.error->message
            : QStringLiteral("无法生成诊断场景快照。");
        return std::nullopt;
    }

    const int dpiX = config_document_
        .value({"output", "dpiX"})
        .toInt(slicer_core::kDefaultOutputDpiX);
    const int dpiY = config_document_
        .value({"output", "dpiY"})
        .toInt(slicer_core::kDefaultOutputDpiY);
    const double layerThicknessMm = config_document_
        .value({"output", "layerThicknessMm"})
        .toDouble(
            slicer_core::kDefaultLayerThicknessMm);
    const double classificationResolutionMm =
        std::max({
            25.4 / static_cast<double>(
                       std::max(1, dpiX)),
            25.4 / static_cast<double>(
                       std::max(1, dpiY)),
            layerThicknessMm,
        });
    const double minimumWidthMm =
        std::max(
            0.10,
            2.0 * classificationResolutionMm);

    const QString sessionId =
        QStringLiteral("diagnostic_%1_%2")
            .arg(
                QDateTime::currentDateTimeUtc()
                    .toString(
                        QStringLiteral(
                            "yyyyMMdd_HHmmss_zzz")),
                m_sceneDocument.CurrentInstanceId());
    const QString sessionDirectory =
        QDir(paths_.repo_root).filePath(
            QStringLiteral("output/ui_sessions/")
            + sessionId);
    const QString generatedConfigPath =
        QDir(sessionDirectory).filePath(
            QStringLiteral(
                "slice_config.diagnostic.effective.json"));
    const QString sourceConfigPath =
        absoluteFromRepo(config_edit_->text());

    slicer_core::DiagnosticEffectiveConfigRequest
        configRequest;
    configRequest.subjecttype =
        slicer_core::DiagnosticSubjectType::Scene;
    configRequest.sessionid = sessionId.toStdString();
    configRequest.sourceprofileid =
        profileId.toStdString();
    configRequest.generatedatutc =
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs)
            .toStdString();
    configRequest.sourceconfigpath =
        std::filesystem::path(
            sourceConfigPath.toStdWString());
    configRequest.generatedconfigpath =
        std::filesystem::path(
            generatedConfigPath.toStdWString());
    slicer_core::DiagnosticSceneSelection selection;
    selection.scene = snapshot.scene;
    selection.currentmodelid =
        m_sceneDocument.Instance()->modelid;
    selection.currentinstanceid =
        m_sceneDocument.Instance()->instanceid;
    configRequest.scene = std::move(selection);
    configRequest.requested.texturesurfacewidthmm =
        m_diagnosticTextureSurfaceWidthMm;
    configRequest.requested.modelfillmaterial =
        m_diagnosticModelFillMaterial.toStdString();
    configRequest.requested.diagnosticbackendrequest =
        "legacy_cpu";
    configRequest.derived.minimumwidthmm =
        minimumWidthMm;
    configRequest.derived.backendavailability =
        "legacy_cpu";
    configRequest.derived.derivationsource =
        "dpi_layer_resolution";
    configRequest.effective.texturesurfacewidthmm =
        m_diagnosticTextureSurfaceWidthMm;
    configRequest.effective.modelfillmaterial =
        m_diagnosticModelFillMaterial.toStdString();
    configRequest.effective.diagnosticbackend =
        "legacy_cpu";
    configRequest.effective.resolvedprofileid =
        profileId.toStdString();

    const slicer_core::DiagnosticEffectiveConfigResult written =
        slicer_core::WriteDiagnosticEffectiveConfig(
            configRequest);
    if (!written.IsValid())
    {
        *errorMessage =
            written.error.has_value()
            ? QString::fromStdString(
                  written.error->message)
            : QStringLiteral(
                  "诊断生效配置写入或回读失败。");
        return std::nullopt;
    }

    DiagnosticAnalysisRequest request;
    request.identity.sessionid = sessionId;
    request.identity.sceneid =
        m_sceneDocument.SceneId();
    request.identity.modelid =
        QString::fromStdString(
            m_sceneDocument.Instance()->modelid);
    request.identity.instanceid =
        m_sceneDocument.CurrentInstanceId();
    request.identity.confighash =
        QString::fromStdString(written.confighash);
    request.identity.scenerevision =
        m_sceneDocument.SceneRevision();
    request.identity.transformrevision =
        m_sceneDocument.Instance()->transformrevision;
    request.sourcemodel = source->model;
    request.instance =
        m_sceneDocument.Instance().value();
    request.textureoptions =
        source->textureoptions;
    request.modelpath = source->modelpath;
    request.modelfillmaterial =
        m_diagnosticModelFillMaterial;
    request.texturesurfacewidthmm =
        m_diagnosticTextureSurfaceWidthMm;
    request.classificationresolutionmm =
        classificationResolutionMm;
    return request;
}

void MainWindow::OnStartDiagnosticAnalysis()
{
    QString error;
    const std::optional<DiagnosticAnalysisRequest> request =
        BuildDiagnosticAnalysisRequest(&error);
    if (!request.has_value())
    {
        m_diagnosticAnalysisMessage =
            QStringLiteral("启动失败：") + error;
        status_label_->setText(
            m_diagnosticAnalysisMessage);
        UpdateDiagnosticSettingsPresentation();
        return;
    }
    m_lastDiagnosticAnalysisResult.reset();
    m_previewWorkspace->ClearDiagnosticAnalysis(
        QStringLiteral(
            "新的诊断分析正在运行。"));
    m_activeDiagnosticAnalysisIdentity =
        request->identity;
    if (!m_diagnosticAnalysisWorker.Start(*request))
    {
        m_diagnosticAnalysisMessage =
            QStringLiteral(
                "启动失败：诊断请求身份或参数不完整。");
        UpdateDiagnosticSettingsPresentation();
    }
}

void MainWindow::OnCancelDiagnosticAnalysis()
{
    m_diagnosticAnalysisWorker.Cancel();
}

void MainWindow::OnDiagnosticAnalysisFinished(
    const DiagnosticAnalysisResult& result)
{
    m_lastDiagnosticAnalysisResult = result;
    m_activeDiagnosticAnalysisIdentity.reset();
    m_diagnosticAnalysisMessage = result.message;
    status_label_->setText(result.message);
    m_previewWorkspace->SetDiagnosticAnalysis(result);
    UpdateDiagnosticSettingsPresentation();
}

void MainWindow::UpdateDiagnosticSettingsPresentation()
{
    if (m_contextInspector == nullptr)
    {
        return;
    }

    DiagnosticSettingsPresentation presentation;
    const int dpiX =
        config_document_
            .value({"output", "dpiX"})
            .toInt(slicer_core::kDefaultOutputDpiX);
    const int dpiY =
        config_document_
            .value({"output", "dpiY"})
            .toInt(slicer_core::kDefaultOutputDpiY);
    const double layerThicknessMm =
        config_document_
            .value(
                {"output", "layerThicknessMm"})
            .toDouble(
                slicer_core::kDefaultLayerThicknessMm);
    const double classificationResolutionMm =
        std::max({
            25.4 / static_cast<double>(
                       std::max(1, dpiX)),
            25.4 / static_cast<double>(
                       std::max(1, dpiY)),
            layerThicknessMm,
        });
    const double minimumWidthMm =
        std::max(
            0.10,
            2.0 * classificationResolutionMm);
    presentation.minimumwidthmm = minimumWidthMm;
    if (m_diagnosticTextureSurfaceWidthMm
        < minimumWidthMm)
    {
        m_diagnosticTextureSurfaceWidthMm =
            minimumWidthMm;
        m_contextInspector
            ->SetDiagnosticRequestedSettings(
                m_diagnosticTextureSurfaceWidthMm,
                m_diagnosticModelFillMaterial);
    }
    const bool hasCurrentInstance =
        !m_sceneDocument.CurrentInstanceId().isEmpty();
    const bool importRunning =
        m_sceneBatchImportController.IsRunning()
        || m_sceneDocument.State()
            == SceneDocumentState::Loading;
    presentation.controlsenabled =
        hasCurrentInstance && !importRunning;
    presentation.analysisrunning =
        m_diagnosticAnalysisWorker.IsRunning();

    if (hasCurrentInstance)
    {
        presentation.subjectsummary =
            QStringLiteral(
                "场景 %1 / revision %2 / 当前实例 %3")
                .arg(
                    m_sceneDocument.SceneId().isEmpty()
                        ? QStringLiteral("未保存")
                        : m_sceneDocument.SceneId())
                .arg(m_sceneDocument.SceneRevision())
                .arg(
                    m_sceneDocument.CurrentInstanceId());
    }
    else
    {
        presentation.subjectsummary =
            QStringLiteral("未选择模型。");
    }

    const bool openVdbAvailable =
        QFileInfo::exists(
            paths_.openvdb_slicer_cli);
    presentation.backendavailability =
        QStringLiteral(
            "Legacy CPU 可用；OpenVDB 候选工具%1，能力尚未探测")
            .arg(
                openVdbAvailable
                    ? QStringLiteral("已发现")
                    : QStringLiteral("未发现"));

    if (importRunning)
    {
        presentation.status =
            QStringLiteral(
                "等待（pending）：模型导入中，"
                "诊断参数暂不可编辑。");
    }
    else if (!hasCurrentInstance)
    {
        presentation.status =
            QStringLiteral(
                "不可用（unavailable）："
                "等待导入或选择模型。");
        presentation.blockingreasons.push_back(
            QStringLiteral("当前没有诊断对象。"));
    }
    else if (m_sceneDocument.State()
             == SceneDocumentState::Blocked)
    {
        presentation.status =
            QStringLiteral(
                "已阻断（blocked）：模型预检未通过；"
                "参数可编辑，"
                "但诊断结论不得冒充生产准入。");
        presentation.blockingreasons.push_back(
            m_sceneDocument.Error().isEmpty()
                ? QStringLiteral(
                      "当前模型未通过几何预检。")
                : m_sceneDocument.Error());
    }
    else if (m_sceneDocument.State()
             == SceneDocumentState::Failed)
    {
        presentation.status =
            QStringLiteral(
                "不可用（unavailable）：模型加载失败。");
        presentation.blockingreasons.push_back(
            m_sceneDocument.Error().isEmpty()
                ? QStringLiteral(
                      "模型加载失败，未提供详细原因。")
                : m_sceneDocument.Error());
        presentation.controlsenabled = false;
    }
    else
    {
        if (presentation.analysisrunning)
        {
            presentation.status =
                m_diagnosticAnalysisMessage;
        }
        else if (m_lastDiagnosticAnalysisResult.has_value())
        {
            const DiagnosticAnalysisResult& result =
                *m_lastDiagnosticAnalysisResult;
            presentation.maximumwidthmm =
                result.maximumwidthmm;
            presentation.alltexturethresholdmm =
                result.alltexturethresholdmm;
            if (result.state
                == DiagnosticAnalysisState::Succeeded)
            {
                presentation.status =
                    QStringLiteral(
                        "%1 Texture Surface=%2；"
                        "Model Fill=%3；核心耗时=%4 ms。")
                        .arg(
                            result.message,
                            result.texturesurfacevoxels
                                    .has_value()
                                ? QString::number(
                                      *result
                                           .texturesurfacevoxels)
                                : QStringLiteral("未评估"),
                            result.modelfillvoxels
                                    .has_value()
                                ? QString::number(
                                      *result.modelfillvoxels)
                                : QStringLiteral("未评估"),
                            result.totalcorems.has_value()
                                ? QString::number(
                                      *result.totalcorems,
                                      'f',
                                      2)
                                : QStringLiteral("未评估"));
            }
            else
            {
                presentation.status = result.message;
                if (result.state
                    == DiagnosticAnalysisState::Failed
                    || result.state
                        == DiagnosticAnalysisState::Stale)
                {
                    presentation.blockingreasons.push_back(
                        result.message);
                }
            }
        }
        else if (!m_diagnosticAnalysisMessage.isEmpty())
        {
            presentation.status =
                m_diagnosticAnalysisMessage;
        }
        else
        {
            presentation.status =
                QStringLiteral(
                    "等待（pending）：参数已就绪，"
                    "可开始后台诊断。");
        }
    }

    m_contextInspector->SetDiagnosticPresentation(
        presentation);
}
