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

void MainWindow::UpdateBatchImportPresentation()
{
    if (m_modelTopViewWidget == nullptr)
    {
        return;
    }

    if (m_sceneBatchImportController.IsRunning())
    {
        m_batchPresentationItemLimit =
            m_sceneDocument.InstanceCount();
        m_modelTopViewWidget->SetPresentationItemLimit(
            m_batchPresentationItemLimit);
        return;
    }

    m_batchPresentationItemLimit.reset();
    m_modelTopViewWidget->SetPresentationItemLimit(std::nullopt);
}

void MainWindow::LoadScenarios()
{
    if (m_scenarioSelector == nullptr)
    {
        return;
    }

    const bool loaded = m_scenarioRegistry.Load(paths_.repo_root);
    const QString currentId = m_scenarioSelector->currentData().toString();
    const QString defaultId = loaded ? m_scenarioRegistry.DefaultScenarioId() : QString{};

    m_scenarioSelector->blockSignals(true);
    m_scenarioSelector->clear();
    m_scenarioSelector->addItem("自定义路径", QString{});
    m_scenarioSelector->setItemData(
        0,
        "自定义路径：手动选择配置文件和输出包，不使用场景索引。",
        Qt::ToolTipRole);

    int enabledCount = 0;
    int visibleCount = 0;
    int hiddenCount = 0;
    for (const ScenarioEntry& scenario : m_scenarioRegistry.Entries())
    {
        if (!scenario.enabled)
        {
            continue;
        }
        ++enabledCount;
        if (!ShouldShowScenario(scenario))
        {
            ++hiddenCount;
            continue;
        }

        const int itemIndex = m_scenarioSelector->count();
        m_scenarioSelector->addItem(MakeScenarioDisplayLabel(scenario), scenario.id);
        m_scenarioSelector->setItemData(itemIndex, MakeScenarioToolTip(scenario), Qt::ToolTipRole);
        ++visibleCount;
    }

    m_scenarioSelector->blockSignals(false);
    UpdateComboPopupWidth(m_scenarioSelector);
    if (m_sceneActionBar != nullptr)
    {
        QList<SceneActionBarProfileOption> profileOptions;
        for (const ScenarioEntry& scenario :
             m_scenarioRegistry.Entries())
        {
            if (!scenario.enabled
                || !ShouldShowScenario(scenario))
            {
                continue;
            }
            SceneActionBarProfileOption option;
            option.id = scenario.id;
            option.label =
                MakeScenarioDisplayLabel(scenario);
            option.tooltip =
                MakeScenarioToolTip(scenario);
            profileOptions.push_back(option);
        }
        m_sceneActionBar->SetProfileOptions(
            profileOptions,
            currentId.isEmpty()
                ? defaultId
                : currentId);
    }
    if (m_scenarioCountLabel != nullptr)
    {
        QString countText = QString("场景：显示 %1 / 可用 %2").arg(visibleCount).arg(enabledCount);
        if (hiddenCount > 0)
        {
            countText += QString("，隐藏高级/测试 %1 个").arg(hiddenCount);
        }
        countText += "。完整说明可悬停下拉项查看。";
        m_scenarioCountLabel->setText(countText);
    }

    QString targetId = !currentId.isEmpty() ? currentId : defaultId;
    int targetIndex = targetId.isEmpty() ? 0 : m_scenarioSelector->findData(targetId);
    if (targetIndex < 0)
    {
        targetIndex = 0;
    }

    m_scenarioSelector->setCurrentIndex(targetIndex);
    OnScenarioChanged(targetIndex);

    for (const QString& warning : m_scenarioRegistry.Warnings())
    {
        log_panel_->appendError(warning);
    }
}

bool MainWindow::ShouldShowScenario(const ScenarioEntry& scenario) const
{
    if (scenario.visibility == "hidden")
    {
        return false;
    }
    if (m_showAdvancedScenariosCheck != nullptr && m_showAdvancedScenariosCheck->isChecked())
    {
        return true;
    }
    return scenario.visibility != "advanced" && scenario.visibility != "fixture";
}

void MainWindow::ApplyProfileDefaultsToDocument(const QString& profileId, const QString& packageDir)
{
    SliceSettingsModel settingsmodel;
    if (!settingsmodel.ApplyProfileDefaults(profileId))
    {
        return;
    }

    const SliceSettingsState& settings = settingsmodel.State();
    const auto SetValue = [this](const QStringList& path, const QJsonValue& value)
    {
        if (config_document_.value(path) != value)
        {
            config_document_.setValue(path, value);
        }
    };

    if (!packageDir.trimmed().isEmpty())
    {
        SetValue({"output", "packageDir"}, QDir::fromNativeSeparators(packageDir));
    }
    QString modelFillMaterial = QStringLiteral("white");
    if (settings.modelfillmaterial == ModelFillMaterial::Rgb)
    {
        modelFillMaterial = QStringLiteral("rgb");
    }
    else if (settings.modelfillmaterial == ModelFillMaterial::Varnish)
    {
        modelFillMaterial = QStringLiteral("varnish");
    }
    SetValue({"modelFill", "material"}, modelFillMaterial);
    SetValue({"modelFill", "enabled"}, true);
    SetValue(
        {"modelFill", "scope"},
        settings.modelfillmaterial == ModelFillMaterial::Rgb
            ? QStringLiteral("below_texture_surface")
            : QStringLiteral("all_model"));
    SetValue({"modelFill", "value"}, 0);
    SetValue({"modelFill", "emptyAllowedInProduction"}, false);
    SetValue({"modelFill", "legacyRgbFallback"}, false);
    const bool texturedFillProfile = profileId == QStringLiteral("textured_nail_rgb_white_lower_support")
        || profileId == QStringLiteral("textured_nail_rgb_varnish_lower_support");
    if (texturedFillProfile)
    {
        const bool whiteFill = settings.modelfillmaterial == ModelFillMaterial::White;
        SetValue({"texture", "enabled"}, true);
        SetValue({"texture", "applyMode"}, QStringLiteral("top_surface_band"));
        SetValue({"texture", "topSurfaceLayers"}, 1);
        SetValue({"materialPolicy", "enabled"}, false);
        SetValue({"materialPolicy", "white", "enabled"}, false);
        SetValue({"materialPolicy", "white", "mode"}, QStringLiteral("disabled"));
        SetValue({"materialPolicy", "varnish", "enabled"}, false);
        SetValue({"materialPolicy", "varnish", "mode"}, QStringLiteral("disabled"));
        SetValue({"materialProcessProfile", "white", "enabled"}, whiteFill);
        SetValue(
            {"materialProcessProfile", "white", "mode"},
            whiteFill ? QStringLiteral("all_model") : QStringLiteral("disabled"));
        SetValue({"materialProcessProfile", "varnish", "enabled"}, !whiteFill);
        SetValue(
            {"materialProcessProfile", "varnish", "mode"},
            whiteFill ? QStringLiteral("disabled") : QStringLiteral("all_model"));
        SetValue({"materialProcessProfile", "validation", "requireWhitePixels"}, whiteFill);
        SetValue({"materialProcessProfile", "validation", "requireVarnishPixels"}, !whiteFill);
    }
    SetValue({"support", "enabled"}, settings.support.enabled);
    SetValue({"support", "mode"}, QStringLiteral("bottom_projection"));
    SetValue({"support", "placement"}, QStringLiteral("lower"));
    SetValue({"support", "internalVoid", "enabled"}, settings.support.internalvoidenabled);
    SetValue({"support", "internalVoid", "minAreaPx"}, settings.support.internalvoidminareapx);
    SetValue({"support", "internalVoid", "fillRule"}, QStringLiteral("all_internal_voids"));
    SetValue({"support", "baseProjection", "enabled"}, settings.support.baseprojectionenabled);
    SetValue({"support", "baseProjection", "layerCount"}, settings.support.baseprojectionlayercount);
    SetValue(
        {"support", "baseProjection", "layerPlacement"},
        QStringLiteral("prepend_below_model"));
    SetValue(
        {"support", "baseProjection", "source"},
        QStringLiteral("max_support_footprint"));
    SetValue({"surfaceVarnish", "enabled"}, settings.surfacevarnish.enabled);
    SetValue({"surfaceVarnish", "thicknessPx"}, settings.surfacevarnish.thicknesspx);
    SetValue({"outerVarnish", "enabled"}, settings.outervarnish.enabled);
    SetValue({"outerVarnish", "thicknessMm"}, settings.outervarnish.thicknessmm);
    SetValue({"outerVarnish", "pixelPitchUm"}, settings.outervarnish.pixelpitchum);
    SetValue({"preview", "outputPolicy"}, settings.preview.outputpolicy);
    SetValue(
        {"preview", "enabled"},
        settings.preview.outputpolicy
            == QStringLiteral("tiff_native_with_diagnostics"));
    SetValue({"preview", "interval"}, settings.preview.interval);
    SetValue({"experimental", "openvdbPipeline", "enabled"}, false);
    SetValue({"experimental", "openvdbPipeline", "engine"}, QStringLiteral("legacy"));
    SetValue({"experimental", "openvdbPipeline", "admissionMode"}, QStringLiteral("strict_closed"));
    SetValue({"experimental", "openvdbPipeline", "failurePolicy"}, QStringLiteral("fail_fast"));
    SetValue({"experimental", "openvdbPipeline", "allowNonProductionOutput"}, false);
    SetValue({"experimental", "openvdbPipeline", "writeProductionRgbwsv"}, false);
}

void MainWindow::ApplyScenario(const ScenarioEntry& scenario)
{
    const QString configPath = absoluteFromRepo(scenario.configpath);
    const QString packageDir = scenario.packagedir.isEmpty() ? inferPackageFromConfig(configPath) : absoluteFromRepo(scenario.packagedir);

    config_edit_->setText(configPath);
    if (!packageDir.isEmpty())
    {
        package_edit_->setText(packageDir);
    }

    QString description = scenario.description;
    if (scenario.experimental || scenario.requiresopenvdb)
    {
        description += "\n实验场景：不会默认作为生产路径验收。";
    }
    if (scenario.visibility == "fixture")
    {
        description += "\n测试夹具：默认隐藏，只用于回归或专项验证。";
    }
    else if (scenario.visibility == "advanced")
    {
        description += "\n高级场景：默认隐藏，适合专项调试。";
    }
    if (!scenario.audience.isEmpty())
    {
        description += "\n受众：" + scenario.audience;
    }
    if (m_scenarioDescriptionLabel != nullptr)
    {
        m_scenarioDescriptionLabel->setText(description);
    }

    m_currentProfileId = scenario.id;
    if (!config_editor_panel_->loadConfig(configPath))
    {
        return;
    }
    ApplyProfileDefaultsToDocument(scenario.id, packageDir);
    SyncDiagnosticRequestedSettingsFromConfig();
    SyncProductionSettingsFromConfig();
    if (!packageDir.isEmpty())
    {
        loadPackage(packageDir);
    }
    RequestTextureWhitePreflight();
}

void MainWindow::loadPackage(const QString& package_dir) {
    const PackageSummary package = package_loader_.load(absoluteFromRepo(package_dir));
    LoadPackageSummary(package);
}

void MainWindow::LoadPackageSummary(const PackageSummary& package)
{
    package_edit_->setText(package.package_dir);
    m_diagnosticsDock->LoadPackage(package);
    m_previewWorkspace->LoadPackage(package);
    material_process_panel_->loadPackage(package);
    warnings_view_->setPlainText(package.warnings.join('\n'));
}

void MainWindow::OnMaterialClosureLayerRequested(
    const int layerIndex,
    const QString& gapPreviewPath)
{
    const bool selected =
        m_previewWorkspace->ShowMaterialClosureLayer(layerIndex, gapPreviewPath);
    status_label_->setText(
        selected
            ? QStringLiteral("已定位材料闭环诊断 layer=%1%2")
                  .arg(layerIndex)
                  .arg(
                      gapPreviewPath.isEmpty()
                          ? QStringLiteral("；当前报告未提供 Gap 预览。")
                          : QStringLiteral("；显示诊断 Gap 伪彩图。"))
            : QStringLiteral("材料闭环报告中的 layer=%1 不在当前输出包层列表中。")
                  .arg(layerIndex));
}

void MainWindow::loadCompareResult(const QString& path) {
    const JsonReport report = report_loader_.load(path);
    if (!report.error.isEmpty()) {
        compare_view_->setPlainText("读取对比结果失败：" + report.error);
        return;
    }
    compare_view_->setPlainText(ReportLoader::summarize(report) + "\n\n" + QString::fromUtf8(report.document.toJson(QJsonDocument::Indented)));
}

void MainWindow::runCommand(const QString& action, const QString& program, const QStringList& args) {
    current_action_ = action;
    runner_.Run(program, args, paths_.repo_root);
}

void MainWindow::setBusy(const bool busy) {
    m_processBusy = busy;
    UpdateActionAvailability();
    UpdateModelPreflightUi();
}
