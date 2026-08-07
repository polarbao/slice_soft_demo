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

QWidget* MainWindow::createProjectPanel() {
    auto* panel = new QScrollArea(this);
    panel->setObjectName(QStringLiteral("projectPanel"));
    panel->setWidgetResizable(true);
    auto* content = new QWidget(panel);
    content->setObjectName(QStringLiteral("projectPanelContent"));
    auto* layout = new QVBoxLayout(content);

    config_edit_ = makePathEdit(QDir(paths_.repo_root).filePath("samples/configs/material_process/nail_rgb_white_varnish_top2.json"), panel);
    package_edit_ = makePathEdit(QDir(paths_.repo_root).filePath("output/NailRgbWhiteVarnishTop2"), panel);
    profile_a_edit_ = makePathEdit(QDir(paths_.repo_root).filePath("output/NailRgbWhiteVarnishTop1"), panel);
    profile_b_edit_ = makePathEdit(QDir(paths_.repo_root).filePath("output/NailRgbWhiteVarnishTop3"), panel);

    auto* config_browse = makeButton("...", panel);
    auto* package_browse = makeButton("...", panel);
    auto* profile_a_browse = makeButton("...", panel);
    auto* profile_b_browse = makeButton("...", panel);

    m_scenarioSelector = new QComboBox(panel);
    ConfigureLongTextCombo(m_scenarioSelector, 22);
    m_scenarioSelector->setToolTip("选择常用切片场景。默认隐藏高级/测试夹具；勾选“显示全部场景”后可查看完整列表。");
    m_showAdvancedScenariosCheck = new QCheckBox("显示全部场景", panel);
    m_showAdvancedScenariosCheck->setToolTip("勾选后显示高级、测试夹具和实验场景；普通切片建议先使用默认列表。");
    auto* scenario_reload = makeButton("刷新", panel);
    scenario_reload->setToolTip("重新读取 samples/scenarios/slicer_scenarios.json。");
    auto* scenario_row = new QHBoxLayout();
    auto* scenarioLabel = new QLabel("场景/Profile", panel);
    scenarioLabel->setToolTip("Profile 是可复用的切片配置模板，选择后会填充配置文件和输出包路径。");
    scenario_row->addWidget(scenarioLabel);
    scenario_row->addWidget(m_scenarioSelector, 1);
    layout->addLayout(scenario_row);
    auto* scenarioActionsRow = new QHBoxLayout();
    scenarioActionsRow->addWidget(m_showAdvancedScenariosCheck);
    scenarioActionsRow->addStretch(1);
    scenarioActionsRow->addWidget(scenario_reload);
    layout->addLayout(scenarioActionsRow);
    m_scenarioCountLabel = new QLabel("场景：尚未加载", panel);
    m_scenarioCountLabel->setWordWrap(true);
    layout->addWidget(m_scenarioCountLabel);
    m_scenarioDescriptionLabel = new QLabel("场景索引用于替代大量 VSCode 专用调试项。", panel);
    m_scenarioDescriptionLabel->setWordWrap(true);
    layout->addWidget(m_scenarioDescriptionLabel);

    addPathRow(layout, "配置文件", config_edit_, config_browse);
    addPathRow(layout, "输出包", package_edit_, package_browse);
    addPathRow(layout, "对比包 A", profile_a_edit_, profile_a_browse);
    addPathRow(layout, "对比包 B", profile_b_edit_, profile_b_browse);

    repo_label_ = new QLabel("仓库根目录：" + paths_.repo_root, panel);
    slicer_label_ = new QLabel("切片工具：" + paths_.slicer_cli, panel);
    m_openVdbSlicerLabel = new QLabel("OpenVDB 候选工具：" + paths_.openvdb_slicer_cli, panel);
    rip_label_ = new QLabel("RIP 校验工具：" + paths_.rip_reader, panel);
    repo_label_->setWordWrap(true);
    slicer_label_->setWordWrap(true);
    m_openVdbSlicerLabel->setWordWrap(true);
    rip_label_->setWordWrap(true);
    layout->addWidget(repo_label_);
    layout->addWidget(slicer_label_);
    layout->addWidget(m_openVdbSlicerLabel);
    layout->addWidget(rip_label_);
    layout->addWidget(createRunPanel());
    layout->addStretch(1);

    connect(config_browse, &QPushButton::clicked, this, &MainWindow::browseConfig);
    connect(package_browse, &QPushButton::clicked, this, &MainWindow::browsePackage);
    connect(profile_a_browse, &QPushButton::clicked, this, &MainWindow::browseProfileA);
    connect(profile_b_browse, &QPushButton::clicked, this, &MainWindow::browseProfileB);
    connect(m_scenarioSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::OnScenarioChanged);
    connect(m_showAdvancedScenariosCheck, &QCheckBox::toggled, this, &MainWindow::OnScenarioVisibilityChanged);
    connect(scenario_reload, &QPushButton::clicked, this, &MainWindow::OnReloadScenarios);
    panel->setWidget(content);
    return panel;
}

QWidget* MainWindow::createRunPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    build_button_ = makeButton("构建调试版", panel);
    m_importModelPreviewButton =
        makeButton("导入模型（可多选）", panel);
    m_importModelPreviewButton->setObjectName(
        QStringLiteral("importModelPreviewButton"));
    m_importModelPreviewButton->setToolTip(
        QStringLiteral(
            "一次选择一个或多个 OBJ、STL、3MF；"
            "串行加载并在批次完成后统一排版"));
    m_cancelModelImportButton =
        makeButton("取消模型导入", panel);
    m_cancelModelImportButton->setObjectName(
        QStringLiteral("cancelModelImportButton"));
    m_cancelModelImportButton->setToolTip(
        QStringLiteral(
            "取消当前和尚未开始的导入项；"
            "已成功导入的模型保留"));
    m_cancelModelImportButton->setEnabled(false);
    m_importSliceButton = makeButton("导入模型并切片", panel);
    m_importOpenVdbButton = makeButton("导入模型并 OpenVDB 诊断", panel);
    m_importOpenVdbCandidateButton = makeButton("导入模型并 OpenVDB 候选切片", panel);
    run_slicer_button_ = makeButton("运行切片", panel);
    run_rip_button_ = makeButton("运行 RIP 摘要", panel);
    regression_button_ = makeButton("运行快速回归", panel);
    compare_button_ = makeButton("对比工艺配置", panel);
    auto* load_button = makeButton("加载输出包", panel);
    auto* open_button = makeButton("打开输出目录", panel);
    status_label_ = new QLabel("就绪", panel);
    status_label_->setWordWrap(true);
    auto* preflightRow = new QHBoxLayout();
    m_modelPreflightCompactState = new QLabel(QStringLiteral("预检：待检测"), panel);
    m_modelPreflightCompactState->setObjectName(
        QStringLiteral("modelPreflightCompactState"));
    m_modelPreflightCompactState->setWordWrap(true);
    m_modelPreflightCompactMode = new QLabel(QStringLiteral("传统切片"), panel);
    m_modelPreflightCompactMode->setObjectName(
        QStringLiteral("modelPreflightCompactMode"));
    m_modelPreflightRecheckButton = new QPushButton(panel);
    m_modelPreflightRecheckButton->setObjectName(
        QStringLiteral("modelPreflightCompactRecheck"));
    m_modelPreflightRecheckButton->setIcon(
        style()->standardIcon(QStyle::SP_BrowserReload));
    m_modelPreflightRecheckButton->setToolTip(QStringLiteral("重新执行模型预检"));
    m_modelPreflightRecheckButton->setFixedSize(28, 28);
    m_modelPreflightCancelButton = new QPushButton(panel);
    m_modelPreflightCancelButton->setObjectName(
        QStringLiteral("modelPreflightCompactCancel"));
    m_modelPreflightCancelButton->setIcon(
        style()->standardIcon(QStyle::SP_BrowserStop));
    m_modelPreflightCancelButton->setToolTip(QStringLiteral("取消当前模型预检"));
    m_modelPreflightCancelButton->setFixedSize(28, 28);
    m_modelPreflightCancelButton->setEnabled(false);
    preflightRow->addWidget(m_modelPreflightCompactState, 1);
    preflightRow->addWidget(m_modelPreflightCompactMode);
    preflightRow->addWidget(m_modelPreflightRecheckButton);
    preflightRow->addWidget(m_modelPreflightCancelButton);
    m_sliceTimingPanel = new SliceTimingPanel(panel);
    layout->addWidget(build_button_);
    layout->addWidget(m_importModelPreviewButton);
    layout->addWidget(m_cancelModelImportButton);
    layout->addWidget(m_importSliceButton);
    layout->addWidget(m_importOpenVdbButton);
    layout->addWidget(m_importOpenVdbCandidateButton);
    layout->addWidget(run_slicer_button_);
    layout->addWidget(run_rip_button_);
    layout->addWidget(regression_button_);
    layout->addWidget(compare_button_);
    layout->addWidget(load_button);
    layout->addWidget(open_button);
    layout->addWidget(status_label_);
    layout->addLayout(preflightRow);
    layout->addWidget(m_sliceTimingPanel);

    connect(build_button_, &QPushButton::clicked, this, &MainWindow::buildDebug);
    connect(
        m_importModelPreviewButton,
        &QPushButton::clicked,
        this,
        &MainWindow::OnImportModelPreview);
    connect(
        m_cancelModelImportButton,
        &QPushButton::clicked,
        &m_sceneBatchImportController,
        &SceneBatchImportController::Cancel);
    connect(m_importSliceButton, &QPushButton::clicked, this, &MainWindow::OnImportModelAndSlice);
    connect(m_importOpenVdbButton, &QPushButton::clicked, this, &MainWindow::OnImportModelOpenVdbDiagnostic);
    connect(m_importOpenVdbCandidateButton, &QPushButton::clicked, this, &MainWindow::OnImportModelOpenVdbCandidate);
    connect(run_slicer_button_, &QPushButton::clicked, this, &MainWindow::runSlicer);
    connect(run_rip_button_, &QPushButton::clicked, this, &MainWindow::runRipSummary);
    connect(regression_button_, &QPushButton::clicked, this, &MainWindow::runQuickRegression);
    connect(compare_button_, &QPushButton::clicked, this, &MainWindow::compareProfiles);
    connect(load_button, &QPushButton::clicked, this, &MainWindow::loadPackageFromEdit);
    connect(open_button, &QPushButton::clicked, this, &MainWindow::openOutputFolder);
    connect(
        m_modelPreflightRecheckButton,
        &QPushButton::clicked,
        this,
        &MainWindow::OnRecheckModelPreflight);
    connect(
        m_modelPreflightCancelButton,
        &QPushButton::clicked,
        this,
        &MainWindow::OnCancelModelPreflight);
    return panel;
}

QWidget* MainWindow::createRightPanel() {
    material_process_panel_ = new MaterialProcessPanel(this);
    warnings_view_ = new QPlainTextEdit(this);
    warnings_view_->setObjectName(
        QStringLiteral("warningsDiagnosticView"));
    warnings_view_->setReadOnly(true);
    compare_view_ = new QPlainTextEdit(this);
    compare_view_->setObjectName(
        QStringLiteral("processComparisonView"));
    compare_view_->setReadOnly(true);
    m_modelPreflightPanel = new ModelPreflightPanel(this);
    m_contextInspector = new ContextInspector(
        m_modelListPanel,
        m_modelTransformPanel,
        m_sceneLayoutPanel,
        m_modelPreflightPanel,
        warnings_view_,
        this);
    return m_contextInspector;
}

QString MainWindow::absoluteFromRepo(const QString& path) const {
    const QFileInfo info(path);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    return QDir(paths_.repo_root).filePath(path);
}

QString MainWindow::inferPackageFromConfig(const QString& config_path) const {
    QFile file(config_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        return {};
    }
    const QString package = document.object().value("output").toObject().value("packageDir").toString();
    return package.isEmpty() ? QString{} : absoluteFromRepo(package);
}

SliceSettingsState MainWindow::BuildCurrentSettings(
    const QString& modelPathOverride,
    const QString& packageDirOverride,
    const SliceEngineRole engineRole) const
{
    SliceSettingsModel settingsmodel;
    const QString profileId = m_currentProfileId.trimmed().isEmpty()
        ? QStringLiteral("custom")
        : m_currentProfileId;
    if (!settingsmodel.ApplyProfileDefaults(profileId))
    {
        SliceSettingsState customstate;
        customstate.profileid = profileId;
        settingsmodel.SetState(customstate);
    }

    SliceSettingsState settings = settingsmodel.State();
    const QString configuredModel = config_document_.value({"input", "modelPath"}).toString();
    const QString selectedModel = modelPathOverride.trimmed().isEmpty()
        ? configuredModel
        : modelPathOverride;
    settings.modelpath = ResolveModelPath(selectedModel, config_document_.path());

    const QString configuredPackage = config_document_.value({"output", "packageDir"}).toString();
    settings.outputdirectory = packageDirOverride.trimmed().isEmpty()
        ? configuredPackage
        : packageDirOverride;
    settings.dpix = config_document_.value({"output", "dpiX"})
                       .toInt(slicer_core::kDefaultOutputDpiX);
    settings.dpiy = config_document_.value({"output", "dpiY"})
                       .toInt(slicer_core::kDefaultOutputDpiY);
    settings.layerthicknessmm = config_document_.value({"output", "layerThicknessMm"})
                                    .toDouble(settings.layerthicknessmm);

    const QString modelFill = config_document_.value({"modelFill", "material"}).toString();
    if (modelFill == QStringLiteral("varnish"))
    {
        settings.modelfillmaterial = ModelFillMaterial::Varnish;
    }
    else if (modelFill == QStringLiteral("rgb"))
    {
        settings.modelfillmaterial = ModelFillMaterial::Rgb;
    }
    else if (modelFill == QStringLiteral("white"))
    {
        settings.modelfillmaterial = ModelFillMaterial::White;
    }

    settings.support.enabled = config_document_.value({"support", "enabled"})
                                   .toBool(settings.support.enabled);
    settings.support.placement = ParseSupportPlacement(
        config_document_.value({"support", "placement"}).toString(QStringLiteral("lower")));
    settings.support.internalvoidenabled = config_document_.value({"support", "internalVoid", "enabled"})
                                               .toBool(settings.support.internalvoidenabled);
    settings.support.internalvoidminareapx = config_document_.value({"support", "internalVoid", "minAreaPx"})
                                                .toInt(settings.support.internalvoidminareapx);
    settings.support.baseprojectionenabled =
        config_document_.value({"support", "baseProjection", "enabled"})
            .toBool(settings.support.baseprojectionenabled);
    settings.support.baseprojectionlayercount =
        config_document_.value({"support", "baseProjection", "layerCount"})
            .toInt(settings.support.baseprojectionlayercount);

    settings.surfacevarnish.enabled = config_document_.value({"surfaceVarnish", "enabled"})
                                          .toBool(settings.surfacevarnish.enabled);
    settings.surfacevarnish.thicknesspx = config_document_.value({"surfaceVarnish", "thicknessPx"})
                                              .toInt(settings.surfacevarnish.thicknesspx);
    settings.outervarnish.enabled = config_document_.value({"outerVarnish", "enabled"})
                                        .toBool(settings.outervarnish.enabled);
    settings.outervarnish.thicknessmm = config_document_.value({"outerVarnish", "thicknessMm"})
                                            .toDouble(settings.outervarnish.thicknessmm);
    settings.outervarnish.pixelpitchum = config_document_.value({"outerVarnish", "pixelPitchUm"})
                                             .toDouble(settings.outervarnish.pixelpitchum);
    const QString previewOutputPolicy =
        config_document_.value({"preview", "outputPolicy"}).toString();
    if (!previewOutputPolicy.isEmpty())
    {
        settings.preview.outputpolicy = previewOutputPolicy;
        settings.preview.enabled =
            previewOutputPolicy
            == QStringLiteral("tiff_native_with_diagnostics");
    }
    else
    {
        settings.preview.enabled =
            config_document_.value({"preview", "enabled"})
                .toBool(settings.preview.enabled);
        settings.preview.outputpolicy =
            settings.preview.enabled
                ? QStringLiteral("tiff_native_with_diagnostics")
                : QStringLiteral("tiff_native");
    }
    settings.preview.interval = config_document_.value({"preview", "interval"})
                                    .toInt(settings.preview.interval);
    settings.enginerole = engineRole;
    const ProductionTextureSettingsPresentation presentation =
        BuildProductionSettingsPresentation();
    if (presentation.singlematerialrelief.has_value()
        && presentation.singlematerialrelief->valid
        && presentation.singlematerialrelief->editable)
    {
        settings.singlematerialreliefoverrideenabled = true;
        settings.singlematerialrelief =
            *presentation.singlematerialrelief;
    }
    else if (presentation.texture.valid
             && presentation.texture.editable)
    {
        settings.productiontextureoverrideenabled = true;
        settings.productiontexture = presentation.texture;
    }
    return settings;
}

EffectiveConfigResult MainWindow::GenerateEffectiveConfig(
    const QString& modelPathOverride,
    const QString& packageDirOverride,
    const SliceEngineRole engineRole,
    const QString& sessionTag)
{
    EffectiveConfigResult errorresult;
    if (!config_document_.document().isObject())
    {
        errorresult.errors.push_back(QStringLiteral("当前没有可用的 Profile 模板配置。"));
        config_editor_panel_->ShowEffectiveConfig(errorresult);
        return errorresult;
    }

    const QString configuredModel = modelPathOverride.trimmed().isEmpty()
        ? config_document_.value({"input", "modelPath"}).toString()
        : modelPathOverride;
    const QString resolvedModel = ResolveModelPath(configuredModel, config_document_.path());
    const QFileInfo modelInfo(resolvedModel);
    if (!modelInfo.exists() || !modelInfo.isFile())
    {
        errorresult.errors.push_back(QStringLiteral("模型文件不存在：") + resolvedModel);
        config_editor_panel_->ShowEffectiveConfig(errorresult);
        return errorresult;
    }

    const slicer_core::SlicePipelineMode requestedMode =
        engineRole == SliceEngineRole::LegacyProduction
        ? config_editor_panel_->SelectedProductionMode()
        : slicer_core::SlicePipelineMode::Legacy;
    const QString requestedProfileId =
        requestedMode == slicer_core::SlicePipelineMode::GlobalSurfaceShell
        ? config_editor_panel_->SelectedProductionProfileId()
        : (m_currentProfileId.isEmpty()
               ? QStringLiteral("custom")
               : m_currentProfileId);
    ProductionProfileSourceRequest sourceRequest;
    sourceRequest.reporoot = paths_.repo_root;
    sourceRequest.mode = requestedMode;
    sourceRequest.requestedprofileid = requestedProfileId;
    sourceRequest.legacytemplatepath = config_document_.path();
    sourceRequest.legacyoriginaldocument = config_document_.originalDocument();
    sourceRequest.legacyoverridedocument = config_document_.document();
    const ProductionProfileSourceResult source =
        ProductionProfileSourceResolver().Resolve(sourceRequest);
    if (!source.IsValid())
    {
        errorresult.errors = source.errors;
        config_editor_panel_->ShowEffectiveConfig(errorresult);
        return errorresult;
    }

    const QString sessionName = BuildProductionSessionName(
        modelInfo.completeBaseName(),
        source.profileid,
        sessionTag,
        QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    const QString relativeSessionRoot = QStringLiteral("output/ui_sessions/") + sessionName;
    const QString generatedPath = QDir(paths_.repo_root).filePath(
        relativeSessionRoot + QStringLiteral("/slice_config.effective.json"));
    const QString effectivePackage = packageDirOverride.trimmed().isEmpty()
        ? QDir(paths_.repo_root).filePath(relativeSessionRoot + QStringLiteral("/package"))
        : absoluteFromRepo(packageDirOverride);

    EffectiveConfigRequest request;
    request.profileid = source.profileid;
    request.templatepath = source.templatepath;
    request.generatedconfigpath = generatedPath;
    request.originaldocument = source.originaldocument;
    request.overridedocument = source.overridedocument;
    request.settings = BuildCurrentSettings(resolvedModel, effectivePackage, engineRole);
    request.production.requestedmode = requestedMode;
    request.production.requestedprofileid = requestedProfileId;
    request.production.sourceprofileid = source.profileid;
    request.production.sessionid = sessionName;
    request.sceneid = m_sceneDocument.SceneId();
    request.scenerevision = m_sceneDocument.SceneRevision();
    request.scenecontenthash =
        BuildTextureWhitePreflightContentHash();
    request.profilecapabilities =
        CurrentProfileCapabilities();
    request.texturewhitepreflight =
        m_lastTextureWhitePreflightResult;

    EffectiveConfigResult result = EffectiveConfigGenerator().Generate(request);
    config_editor_panel_->ShowEffectiveConfig(result);
    if (result.IsValid())
    {
        status_label_->setText(QStringLiteral("已生成并校验生效配置：") + result.generatedconfigpath);
    }
    return result;
}
