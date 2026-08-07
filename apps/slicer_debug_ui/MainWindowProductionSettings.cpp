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

void MainWindow::SyncDiagnosticRequestedSettingsFromConfig()
{
    const double configuredWidth =
        config_document_
            .value(
                {"texture",
                 "surfaceShell",
                 "widthMm"})
            .toDouble(0.10);
    m_diagnosticTextureSurfaceWidthMm =
        std::clamp(configuredWidth, 0.10, 6.00);
    const QString configuredMaterial =
        config_document_
            .value({"modelFill", "material"})
            .toString(QStringLiteral("white"));
    m_diagnosticModelFillMaterial =
        configuredMaterial
                == QStringLiteral("varnish")
            || configuredMaterial
                == QStringLiteral("rgb")
        ? configuredMaterial
        : QStringLiteral("white");
    if (m_contextInspector != nullptr)
    {
        m_contextInspector
            ->SetDiagnosticRequestedSettings(
                m_diagnosticTextureSurfaceWidthMm,
                m_diagnosticModelFillMaterial);
    }
}

QJsonObject MainWindow::BuildSelectedGlobalProfileConfig(
    QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    if (config_editor_panel_ == nullptr
        || !config_document_.document().isObject())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage =
                QStringLiteral("当前没有可用的场景配置。");
        }
        return {};
    }

    const QString profileId =
        config_editor_panel_->SelectedProductionProfileId();
    ProductionProfileSourceRequest request;
    request.reporoot = paths_.repo_root;
    request.mode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    request.requestedprofileid = profileId;
    request.legacytemplatepath = config_document_.path();
    request.legacyoriginaldocument =
        config_document_.originalDocument();
    request.legacyoverridedocument =
        config_document_.document();
    const ProductionProfileSourceResult source =
        ProductionProfileSourceResolver().Resolve(request);
    if (!source.IsValid())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = source.errors.join(
                QStringLiteral("；"));
        }
        return {};
    }
    return ApplyStoredGlobalTextureOverride(
        config_document_.document().object(),
        profileId,
        source.overridedocument.object());
}

ProductionTextureSettingsPresentation
MainWindow::BuildProductionSettingsPresentation() const
{
    ProductionTextureSettingsPresentation presentation;
    presentation.profileid =
        m_currentProfileId.trimmed().isEmpty()
        ? QStringLiteral("custom")
        : m_currentProfileId;
    const bool stale = config_document_.isDirty();
    if (!config_document_.document().isObject())
    {
        presentation.texture.lockreason =
            QStringLiteral("当前没有可用的配置文档。");
        presentation.texture.issues.push_back(
            presentation.texture.lockreason);
        return presentation;
    }

    const slicer_core::SlicePipelineMode selectedMode =
        config_editor_panel_ == nullptr
        ? slicer_core::SlicePipelineMode::Legacy
        : config_editor_panel_->SelectedProductionMode();
    if (selectedMode
        == slicer_core::SlicePipelineMode::GlobalSurfaceShell)
    {
        presentation.profileid =
            config_editor_panel_->SelectedProductionProfileId();
        QString errorMessage;
        const QJsonObject globalConfig =
            BuildSelectedGlobalProfileConfig(&errorMessage);
        if (globalConfig.isEmpty())
        {
            presentation.texture.lockreason = errorMessage;
            presentation.texture.issues.push_back(errorMessage);
            return presentation;
        }
        const bool admittedProfile =
            ProductionModeCatalog::FindProfile(
                presentation.profileid.toStdString())
            != nullptr;
        presentation.texture =
            ProductionTextureSettingsModel::Read(
                globalConfig,
                admittedProfile,
                admittedProfile,
                stale);
        if (!admittedProfile)
        {
            presentation.texture.lockreason =
                QStringLiteral("当前 Global Profile 不在生产能力目录中。");
        }
        return presentation;
    }

    const QJsonObject root =
        config_document_.document().object();
    if (presentation.profileid
        == QStringLiteral("single_material_relief"))
    {
        presentation.singlematerialrelief =
            SingleMaterialReliefResolver::Read(
                root,
                presentation.profileid,
                true,
                stale);
        return presentation;
    }

    bool editable = true;
    const ScenarioEntry* scenario =
        m_scenarioRegistry.FindById(presentation.profileid);
    if (scenario != nullptr
        && scenario->productionsafety
            != QStringLiteral("production"))
    {
        editable = false;
    }
    presentation.texture =
        ProductionTextureSettingsModel::Read(
            root,
            editable,
            true,
            stale);
    if (!editable)
    {
        presentation.texture.lockreason =
            QStringLiteral("当前场景不是生产 Profile，生产纹理设置已锁定。");
    }
    return presentation;
}

void MainWindow::SyncProductionSettingsFromConfig()
{
    if (m_contextInspector == nullptr)
    {
        return;
    }
    m_contextInspector->SetProductionTexturePresentation(
        BuildProductionSettingsPresentation());
}

void MainWindow::OnProductionLegacyTopLayersChanged(
    const int layerCount)
{
    const ProductionTextureSettingsPresentation presentation =
        BuildProductionSettingsPresentation();
    const double layerThicknessMm =
        config_document_
            .value({"output", "layerThicknessMm"})
            .toDouble(slicer_core::kDefaultLayerThicknessMm);
    const ProductionTextureControlState updated =
        ProductionTextureSettingsModel::UpdateLegacyTopLayers(
            presentation.texture,
            layerCount,
            layerThicknessMm);
    const ProductionTextureSettingsApplyResult result =
        ProductionTextureSettingsModel::Apply(
            config_document_.document().object(),
            updated);
    if (!result.applied)
    {
        status_label_->setText(
            result.errorcode + QStringLiteral("：")
            + result.issues.join(QStringLiteral("；")));
        SyncProductionSettingsFromConfig();
        return;
    }
    config_document_.ReplaceObject(result.config);
    status_label_->setText(
        QStringLiteral(
            "生产顶面纹理已设为 %1 层（有效 Z 厚度 %2 mm）；"
            "已有输出已失效，请保存并重新切片。")
            .arg(updated.effectivetoplayers)
            .arg(updated.effectivetopthicknessmm, 0, 'f', 4));
}

void MainWindow::OnProductionGlobalTextureChanged(
    const double widthMm,
    const ProductionTexturePartitionMode mode)
{
    const ProductionTextureSettingsPresentation presentation =
        BuildProductionSettingsPresentation();
    const ProductionTextureControlState updated =
        ProductionTextureSettingsModel::UpdateGlobal(
            presentation.texture,
            widthMm,
            mode);
    QString errorMessage;
    const QJsonObject source =
        BuildSelectedGlobalProfileConfig(&errorMessage);
    const ProductionTextureSettingsApplyResult result =
        ProductionTextureSettingsModel::Apply(source, updated);
    if (!result.applied)
    {
        status_label_->setText(
            result.errorcode + QStringLiteral("：")
            + result.issues.join(QStringLiteral("；")));
        SyncProductionSettingsFromConfig();
        return;
    }

    QJsonObject draft = StoreGlobalTextureOverride(
        config_document_.document().object(),
        presentation.profileid,
        result.state);
    config_document_.ReplaceObject(draft);
    status_label_->setText(
        mode == ProductionTexturePartitionMode::AllTexture
            ? QStringLiteral(
                  "Global 生产纹理已设为全纹理；已有输出已失效。")
            : QStringLiteral(
                  "Global 生产纹理宽度已设为 %1 mm；已有输出已失效。")
                  .arg(result.state.effectivewidthmm, 0, 'f', 2));
}

void MainWindow::OnProductionSingleMaterialChanged(
    const SingleMaterialReliefMaterial material)
{
    const ProductionTextureSettingsPresentation presentation =
        BuildProductionSettingsPresentation();
    if (!presentation.singlematerialrelief.has_value())
    {
        status_label_->setText(
            QStringLiteral(
                "E_SINGLE_MATERIAL_RELIEF_UNSUPPORTED_PROFILE："
                "当前 Profile 不是单材料浮雕。"));
        return;
    }
    const SingleMaterialReliefState updated =
        SingleMaterialReliefResolver::Update(
            *presentation.singlematerialrelief,
            material);
    const SingleMaterialReliefApplyResult result =
        SingleMaterialReliefResolver::Apply(
            config_document_.document().object(),
            updated);
    if (!result.applied)
    {
        status_label_->setText(
            result.errorcode + QStringLiteral("：")
            + result.issues.join(QStringLiteral("；")));
        SyncProductionSettingsFromConfig();
        return;
    }
    config_document_.ReplaceObject(result.config);
    status_label_->setText(
        QStringLiteral(
            "单材料浮雕已切换为 %1 通道；已有输出已失效，请保存并重新切片。")
            .arg(result.state.effectivechannel));
}
