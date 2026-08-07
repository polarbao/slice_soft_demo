#include "MainWindowInternal.h"

namespace slicer_debug_ui_internal
{

constexpr int kPathEditMinimumWidth = 140;
constexpr int kPathLabelMinimumWidth = 62;
constexpr int kBrowseButtonWidth = 44;

constexpr auto kUiProductionSettingsKey =
    "uiProductionSettings";
constexpr auto kGlobalOverridesKey =
    "globalSurfaceShellOverrides";

QJsonObject ApplyStoredGlobalTextureOverride(
    const QJsonObject& sceneDraft,
    const QString& profileId,
    QJsonObject globalConfig)
{
    const QJsonObject settings = sceneDraft
        .value(QString::fromLatin1(kUiProductionSettingsKey))
        .toObject();
    const QJsonObject overrides = settings
        .value(QString::fromLatin1(kGlobalOverridesKey))
        .toObject();
    const QJsonObject profileOverride =
        overrides.value(profileId).toObject();
    if (profileOverride.isEmpty())
    {
        return globalConfig;
    }

    QJsonObject texture =
        globalConfig.value(QStringLiteral("texture")).toObject();
    QJsonObject surfaceShell =
        texture.value(QStringLiteral("surfaceShell")).toObject();
    if (profileOverride.contains(QStringLiteral("widthMm")))
    {
        surfaceShell.insert(
            QStringLiteral("widthMm"),
            profileOverride.value(QStringLiteral("widthMm")));
    }
    if (profileOverride.contains(QStringLiteral("mode")))
    {
        surfaceShell.insert(
            QStringLiteral("mode"),
            profileOverride.value(QStringLiteral("mode")));
    }
    texture.insert(QStringLiteral("surfaceShell"), surfaceShell);
    globalConfig.insert(QStringLiteral("texture"), texture);
    return globalConfig;
}
QJsonObject StoreGlobalTextureOverride(
    QJsonObject sceneDraft,
    const QString& profileId,
    const ProductionTextureControlState& state)
{
    QJsonObject settings = sceneDraft
        .value(QString::fromLatin1(kUiProductionSettingsKey))
        .toObject();
    settings.insert(
        QStringLiteral("schema"),
        QStringLiteral(
            "slicesoft.ui.production_settings.12e_09d.1"));
    QJsonObject overrides = settings
        .value(QString::fromLatin1(kGlobalOverridesKey))
        .toObject();
    overrides.insert(
        profileId,
        QJsonObject{
            {QStringLiteral("widthMm"), state.effectivewidthmm},
            {QStringLiteral("mode"),
             ProductionTextureSettingsContract::PartitionModeValue(
                 state.partitionmode)},
        });
    settings.insert(
        QString::fromLatin1(kGlobalOverridesKey),
        overrides);
    sceneDraft.insert(
        QString::fromLatin1(kUiProductionSettingsKey),
        settings);
    return sceneDraft;
}

QPushButton* makeButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
}

QLineEdit* makePathEdit(const QString& text, QWidget* parent)
{
    auto* edit = new QLineEdit(text, parent);
    edit->setMinimumWidth(kPathEditMinimumWidth);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setToolTip(text);
    QObject::connect(edit, &QLineEdit::textChanged, edit, [edit](const QString& value) {
        edit->setToolTip(value);
    });
    return edit;
}

void addPathRow(QVBoxLayout* layout, const QString& label, QLineEdit* edit, QPushButton* browse)
{
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    auto* labelWidget = new QLabel(label);
    labelWidget->setMinimumWidth(kPathLabelMinimumWidth);
    labelWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    browse->setFixedWidth(kBrowseButtonWidth);
    browse->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->addWidget(labelWidget);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    layout->addLayout(row);
}

QString MakeScenarioDisplayLabel(const ScenarioEntry& scenario)
{
    const QString displayName = scenario.displayname.isEmpty() ? scenario.name : scenario.displayname;
    QString label = scenario.category.isEmpty() ? displayName : scenario.category + " / " + displayName;
    if (scenario.experimental || scenario.requiresopenvdb)
    {
        label += "（实验）";
    }
    if (scenario.visibility == "fixture")
    {
        label += "（测试）";
    }
    else if (scenario.visibility == "advanced")
    {
        label += "（高级）";
    }
    return label;
}

QString ResolveEffectiveProfileId(
    const QJsonObject& profileRoot,
    const QString& requestedProfileId)
{
    const QJsonObject processProfile =
        profileRoot
            .value(QStringLiteral("materialProcessProfile"))
            .toObject();
    const QString configuredName =
        processProfile
            .value(QStringLiteral("name"))
            .toString()
            .trimmed();
    if (!configuredName.isEmpty())
    {
        return configuredName;
    }

    const QString auditedProfileId =
        profileRoot
            .value(QStringLiteral("uiAudit"))
            .toObject()
            .value(QStringLiteral("production"))
            .toObject()
            .value(
                QStringLiteral(
                    "effectiveProductionProfileId"))
            .toString()
            .trimmed();
    if (!auditedProfileId.isEmpty())
    {
        return auditedProfileId;
    }

    const QString configuredTarget =
        processProfile
            .value(QStringLiteral("target"))
            .toString()
            .trimmed();
    return configuredTarget.isEmpty()
        ? requestedProfileId.trimmed()
        : configuredTarget;
}

QString ProductionSafetyLabel(const QString& value)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("production"), QStringLiteral("生产模板")},
        {QStringLiteral("diagnostic"), QStringLiteral("仅诊断")},
        {QStringLiteral("development_only"), QStringLiteral("仅开发")},
        {QStringLiteral("fixture_only"), QStringLiteral("仅测试夹具")},
        {QStringLiteral("experimental_only"), QStringLiteral("仅实验")},
    };
    return labels.value(value, value);
}

QString MaterialCapabilityLabel(const QString& value)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("rgb_surface"), QStringLiteral("RGB 表层")},
        {QStringLiteral("white_model_fill"), QStringLiteral("白墨模型填充")},
        {QStringLiteral("varnish_model_fill"), QStringLiteral("光油模型填充")},
        {QStringLiteral("lower_support"), QStringLiteral("下表面支撑")},
        {QStringLiteral("internal_void_support"), QStringLiteral("内部镂空支撑")},
        {QStringLiteral("optional_outer_varnish"), QStringLiteral("可选外侧光油")},
        {QStringLiteral("single_material"), QStringLiteral("单材料")},
        {QStringLiteral("selectable_white_or_varnish_fill"), QStringLiteral("白墨或光油填充")},
        {QStringLiteral("production_rgb_preview"), QStringLiteral("生产 RGB 预览")},
        {QStringLiteral("rgbwsv_pixel_probe"), QStringLiteral("RGBWSV 像素探针")},
    };
    return labels.value(value, value);
}

bool IsSlicingAction(const QString& action)
{
    return action == QStringLiteral("运行切片")
        || action == QStringLiteral("OpenVDB 候选切片")
        || action == QStringLiteral("切片当前场景");
}

QString ProductionModeSessionTag(
    const slicer_core::SlicePipelineMode mode)
{
    return mode == slicer_core::SlicePipelineMode::GlobalSurfaceShell
        ? QStringLiteral("global_surface_shell")
        : QStringLiteral("legacy");
}

QString SessionIdFromConfigPath(const QString& configPath)
{
    return QFileInfo(configPath).absoluteDir().dirName();
}

QString MakeScenarioToolTip(const ScenarioEntry& scenario)
{
    QStringList lines;
    lines.push_back(MakeScenarioDisplayLabel(scenario));
    if (!scenario.description.isEmpty())
    {
        lines.push_back(scenario.description);
    }
    lines.push_back("配置：" + scenario.configpath);
    if (!scenario.packagedir.isEmpty())
    {
        lines.push_back("输出包：" + scenario.packagedir);
    }
    lines.push_back("可见性：" + scenario.visibility);
    if (!scenario.inputformats.isEmpty())
    {
        lines.push_back("输入格式：" + scenario.inputformats.join(" / ").toUpper());
    }
    if (!scenario.materialcapabilities.isEmpty())
    {
        QStringList capabilities;
        for (const QString& capability : scenario.materialcapabilities)
        {
            capabilities.push_back(MaterialCapabilityLabel(capability));
        }
        lines.push_back("材料能力：" + capabilities.join("、"));
    }
    if (!scenario.productionsafety.isEmpty())
    {
        lines.push_back("生产安全：" + ProductionSafetyLabel(scenario.productionsafety));
    }
    if (!scenario.docpath.isEmpty())
    {
        lines.push_back("说明文档：" + scenario.docpath);
    }
    if (scenario.experimental || scenario.requiresopenvdb)
    {
        lines.push_back("实验场景：用于专项验证，不默认作为生产切片路径。");
    }
    return lines.join('\n');
}

void ConfigureLongTextCombo(QComboBox* combo, const int minimumContentsLength)
{
    combo->setMinimumContentsLength(minimumContentsLength);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMaxVisibleItems(24);
    if (combo->view() != nullptr)
    {
        combo->view()->setMinimumWidth(420);
    }
}

void UpdateComboPopupWidth(QComboBox* combo)
{
    if (combo == nullptr || combo->view() == nullptr)
    {
        return;
    }

    const QFontMetrics metrics(combo->font());
    int popupWidth = combo->width();
    for (int index = 0; index < combo->count(); ++index)
    {
        popupWidth = qMax(popupWidth, metrics.horizontalAdvance(combo->itemText(index)) + 72);
    }
    combo->view()->setMinimumWidth(qMin(popupWidth, 860));
}

QJsonArray MakeNumberArray(const std::initializer_list<double> values)
{
    QJsonArray array;
    for (const double value : values)
    {
        array.push_back(value);
    }
    return array;
}

QJsonArray MakeIntArray(const std::initializer_list<int> values)
{
    QJsonArray array;
    for (const int value : values)
    {
        array.push_back(value);
    }
    return array;
}

QJsonArray MakeStringArray(const std::initializer_list<QString> values)
{
    QJsonArray array;
    for (const QString& value : values)
    {
        array.push_back(value);
    }
    return array;
}

QJsonObject MakeDefaultModelFillConfig()
{
    return QJsonObject{{"enabled", true},
                       {"material", "rgb"},
                       {"scope", "below_texture_surface"},
                       {"value", 0},
                       {"emptyAllowedInProduction", false},
                       {"legacyRgbFallback", false}};
}

QJsonObject MakeDefaultSupportConfig()
{
    return QJsonObject{{"enabled", true},
                       {"mode", "bottom_projection"},
                       {"placement", "lower"},
                       {"value", 0},
                       {"offsetMm", 0.0},
                       {"minAreaPx", 0},
                       {"connectivity", 8},
                       {"writeSupportTypeDebug", true},
                       {"internalVoid",
                        QJsonObject{{"enabled", true},
                                    {"minAreaPx", 16},
                                    {"fillRule", "all_internal_voids"}}},
                       {"baseProjection",
                        QJsonObject{{"enabled", true},
                                    {"layerCount", 30},
                                    {"layerPlacement", "prepend_below_model"},
                                    {"source", "max_support_footprint"}}},
                       {"upper",
                        QJsonObject{{"enabled", false},
                                    {"outside", "outer_varnish_shell"},
                                    {"reason", "optional_detachable_surface_support"}}}};
}

QJsonObject MakeDefaultOuterVarnishConfig()
{
    return QJsonObject{{"enabled", false},
                       {"thicknessMm", 0.0},
                       {"thicknessStepMm", 0.01},
                       {"pixelPitchUm", 42.3},
                       {"allowXYExpansion", true},
                       {"conflictPolicy", "varnish_shell_wins"},
                       {"value", 0}};
}

QJsonObject MakeDefaultSurfaceVarnishConfig()
{
    return QJsonObject{{"enabled", false},
                       {"outerSurface", true},
                       {"innerSurface", true},
                       {"thicknessPx", 0},
                       {"value", 0},
                       {"source", "explicit"}};
}

QJsonObject MakeDefaultPreviewPseudoColors()
{
    return QJsonObject{{"empty", MakeIntArray({255, 255, 255})},
                       {"support", MakeIntArray({0, 255, 0})},
                       {"white", MakeIntArray({0, 170, 255})},
                       {"varnish", MakeIntArray({127, 127, 127})}};
}

QString SanitizeSessionName(const QString& name)
{
    QString normalized = name.trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    normalized = normalized.trimmed();
    return normalized.isEmpty() ? QStringLiteral("model") : normalized;
}

QString BuildProductionSessionName(
    const QString& modelName,
    const QString& profileId,
    const QString& sessionTag,
    const QString& timestamp)
{
    constexpr int kMaximumSessionNameLength{72};
    constexpr int kMaximumTagLength{16};
    constexpr int kHashLength{8};

    const QString modelPart = SanitizeSessionName(modelName);
    const QString profilePart = SanitizeSessionName(profileId);
    const QString tagPart =
        SanitizeSessionName(sessionTag).left(kMaximumTagLength);
    const QString timestampPart = SanitizeSessionName(timestamp);
    const QString candidate =
        modelPart + QStringLiteral("_")
        + profilePart + QStringLiteral("_")
        + tagPart + QStringLiteral("_")
        + timestampPart;
    if (candidate.size() <= kMaximumSessionNameLength)
    {
        return candidate;
    }

    const QString identityHash = QString::fromLatin1(
        QCryptographicHash::hash(
            candidate.toUtf8(),
            QCryptographicHash::Sha256)
            .toHex()
            .left(kHashLength));
    const QString suffix =
        QStringLiteral("_") + tagPart
        + QStringLiteral("_") + timestampPart
        + QStringLiteral("_") + identityHash;
    const int readableBudget = std::max(
        3,
        kMaximumSessionNameLength - suffix.size());
    const int modelBudget = std::max(1, (readableBudget - 1) / 2);
    const int profileBudget = std::max(
        1,
        readableBudget - modelBudget - 1);
    return modelPart.left(modelBudget)
        + QStringLiteral("_")
        + profilePart.left(profileBudget)
        + suffix;
}

SupportPlacement ParseSupportPlacement(const QString& value)
{
    if (value == QStringLiteral("upper"))
    {
        return SupportPlacement::Upper;
    }
    if (value == QStringLiteral("both"))
    {
        return SupportPlacement::Both;
    }
    if (value == QStringLiteral("unsupported_only"))
    {
        return SupportPlacement::UnsupportedOnly;
    }
    if (value == QStringLiteral("full_vertical_projection"))
    {
        return SupportPlacement::FullVerticalProjection;
    }
    return SupportPlacement::Lower;
}

QString ResolveModelPath(const QString& modelPath, const QString& configPath)
{
    const QFileInfo modelInfo(modelPath);
    if (modelInfo.isAbsolute() || configPath.trimmed().isEmpty())
    {
        return QDir::fromNativeSeparators(modelInfo.absoluteFilePath());
    }
    const QDir configDir(QFileInfo(configPath).absolutePath());
    return QDir::fromNativeSeparators(QFileInfo(configDir.filePath(modelPath)).absoluteFilePath());
}

}  // namespace slicer_debug_ui_internal

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

MainWindow::MainWindow(QString repo_root, QWidget* parent)
    : QMainWindow(parent),
      paths_(ToolPaths::FromRepoRoot(std::move(repo_root))),
      m_modelPreflightController(this),
      m_slicePreflightCoordinator(&m_modelPreflightController, this),
      m_sceneDocument(this),
      m_sceneSelectionModel(this),
      m_textureWhitePreflightService(this),
      m_modelTopViewLoader(
          &m_sceneDocument,
          &m_sceneModelRepository,
          this),
      m_sceneBatchImportController(
          &m_sceneDocument,
          this),
      m_sceneTransformController(
          &m_sceneDocument,
          &m_sceneSelectionModel,
          &m_sceneModelRepository,
          this),
      m_sceneSliceActionController(this),
      m_transformedPreflightLoader(
          &m_sceneDocument,
          &m_sceneModelRepository,
          this),
      m_diagnosticAnalysisWorker(this)
{
    setWindowTitle("SliceSoft 切片调试界面");
    resize(1440, 900);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    m_sceneActionBar = new SceneActionBar(central);
    m_sliceCurrentSceneButton =
        m_sceneActionBar->SliceButton();
    root_layout->addWidget(m_sceneActionBar);
    m_mainSplitter = new QSplitter(
        Qt::Horizontal,
        central);
    m_mainSplitter->setObjectName(
        QStringLiteral("mainSplitter"));

    QWidget* projectTools = createProjectPanel();
    m_mainWorkspaceTabs = new QTabWidget(m_mainSplitter);
    m_mainWorkspaceTabs->setObjectName(QStringLiteral("mainWorkspaceTabs"));
    m_mainWorkspaceTabs->setDocumentMode(true);
    m_mainWorkspaceTabs->setTabPosition(QTabWidget::North);
    m_modelTopViewWorkspace = new QWidget(m_mainWorkspaceTabs);
    m_modelTopViewWorkspace->setObjectName(
        QStringLiteral("modelTopViewWorkspace"));
    auto* modelTopViewLayout =
        new QVBoxLayout(m_modelTopViewWorkspace);
    modelTopViewLayout->setContentsMargins(0, 0, 0, 0);
    auto* modelTopViewToolbar = new QHBoxLayout();
    auto* modelTopViewHint = new QLabel(
        QStringLiteral(
            "场景 +Z 俯视，统一显示全部可见模型及其纹理；"
            "导入后按当前规则自动排版，不会启动切片。"),
        m_modelTopViewWorkspace);
    auto* modelTopViewFitButton = new QPushButton(
        QStringLiteral("适应视图"),
        m_modelTopViewWorkspace);
    modelTopViewFitButton->setObjectName(
        QStringLiteral("modelTopViewFitButton"));
    modelTopViewFitButton->setToolTip(
        QStringLiteral("将当前场景全部可见模型完整显示在画布内"));
    modelTopViewToolbar->addWidget(modelTopViewHint, 1);
    modelTopViewToolbar->addWidget(modelTopViewFitButton);
    modelTopViewLayout->addLayout(modelTopViewToolbar);
    m_modelTopViewWidget = new ModelTopViewWidget(
        &m_sceneDocument,
        &m_sceneSelectionModel,
        m_modelTopViewWorkspace);
    m_modelListPanel = new ModelListPanel(
        &m_sceneDocument,
        &m_sceneSelectionModel,
        m_modelTopViewWorkspace);
    m_sceneLayoutPanel = new SceneLayoutPanel(
        &m_sceneDocument,
        m_modelTopViewWorkspace);
    m_modelTransformPanel = new ModelTransformPanel(
        &m_sceneDocument,
        &m_sceneSelectionModel,
        &m_sceneTransformController,
        m_modelTopViewWorkspace);
    modelTopViewLayout->addWidget(m_modelTopViewWidget, 1);
    m_previewWorkspace = new PreviewWorkspace(m_mainWorkspaceTabs);
    auto* configScrollArea = new QScrollArea(m_mainWorkspaceTabs);
    configScrollArea->setObjectName(QStringLiteral("configEditorScrollArea"));
    configScrollArea->setWidgetResizable(true);
    config_editor_panel_ = new ConfigEditorPanel(&config_document_, configScrollArea);
    configScrollArea->setWidget(config_editor_panel_);
    m_configWorkspace = configScrollArea;
    const int modelTopViewTab = m_mainWorkspaceTabs->addTab(
        m_modelTopViewWorkspace,
        QStringLiteral("模型"));
    const int previewWorkspaceTab =
        m_mainWorkspaceTabs->addTab(m_previewWorkspace, "预览");
    const int configTab =
        m_mainWorkspaceTabs->addTab(configScrollArea, "配置");
    m_mainWorkspaceTabs->setTabToolTip(
        modelTopViewTab,
        QStringLiteral("切片前 +Z 俯视工作区：显示模型 XY 占地、坐标和准入状态。"));
    m_mainWorkspaceTabs->setTabToolTip(previewWorkspaceTab, "统一预览工作区：在生产层检查、材料叠加和原始调试预览之间切换并保持同一真实 layerIndex。");
    m_mainWorkspaceTabs->setTabToolTip(configTab, "编辑当前 JSON 配置；常用材料、支撑、预览和实验选项在这里。");
    m_mainWorkspaceTabs->setCurrentWidget(m_modelTopViewWorkspace);

    QWidget* right = createRightPanel();
    m_mainWorkspaceTabs->setMinimumWidth(400);
    right->setMinimumWidth(240);
    right->setMaximumWidth(420);
    m_mainSplitter->addWidget(m_mainWorkspaceTabs);
    m_mainSplitter->addWidget(right);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 0);
    m_mainSplitter->setSizes(QList<int>{1040, 320});

    root_layout->addWidget(m_mainSplitter);
    setCentralWidget(central);

    m_projectToolsDock = new ProjectToolsDock(
        projectTools,
        this);
    addDockWidget(
        Qt::LeftDockWidgetArea,
        m_projectToolsDock);
    m_projectToolsDock->SetExpanded(false);

    m_diagnosticsDock = new DiagnosticsDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_diagnosticsDock);
    m_diagnosticsDock->AddView(
        material_process_panel_,
        QStringLiteral("材料参数"));
    m_diagnosticsDock->AddView(
        compare_view_,
        QStringLiteral("工艺对比"));
    m_diagnosticsDock->AddView(
        m_sliceTimingPanel,
        QStringLiteral("切片耗时"));
    m_diagnosticsDock->SetExpanded(false);
    report_panel_ = m_diagnosticsDock->ReportView();
    channel_chart_panel_ = m_diagnosticsDock->ChartView();
    log_panel_ = m_diagnosticsDock->LogView();

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    QAction* projectToolsAction =
        m_projectToolsDock->toggleViewAction();
    projectToolsAction->setObjectName(
        QStringLiteral("projectToolsToggleAction"));
    projectToolsAction->setText(
        QStringLiteral("项目与高级工具"));
    viewMenu->addAction(projectToolsAction);
    projectToolsAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Alt+P")));
    m_contextInspectorToggleAction =
        viewMenu->addAction(
            QStringLiteral("上下文检查器"));
    m_contextInspectorToggleAction->setObjectName(
        QStringLiteral(
            "contextInspectorToggleAction"));
    m_contextInspectorToggleAction->setCheckable(true);
    m_contextInspectorToggleAction->setChecked(true);
    m_contextInspectorToggleAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Alt+I")));
    connect(
        m_contextInspectorToggleAction,
        &QAction::toggled,
        this,
        [this](const bool visible)
        {
            m_contextInspector->setVisible(visible);
        });
    QAction* diagnosticsAction = m_diagnosticsDock->toggleViewAction();
    diagnosticsAction->setObjectName(QStringLiteral("diagnosticsToggleAction"));
    diagnosticsAction->setText(QStringLiteral("任务详情"));
    diagnosticsAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Alt+D")));
    viewMenu->addAction(diagnosticsAction);

    connect(
        &runner_,
        &ProcessRunner::SigStarted,
        this,
        &MainWindow::handleProcessStarted);
    connect(
        &runner_,
        &ProcessRunner::SigOutput,
        this,
        &MainWindow::OnProcessOutput);
    connect(
        &runner_,
        &ProcessRunner::SigErrorOutput,
        log_panel_,
        &LogPanel::appendError);
    connect(
        &runner_,
        &ProcessRunner::SigFinished,
        this,
        &MainWindow::handleProcessFinished);
    connect(
        &runner_,
        &ProcessRunner::SigFailed,
        this,
        &MainWindow::handleProcessFailed);
    connect(report_panel_, &ReportPanel::warningsChanged, warnings_view_, &QPlainTextEdit::setPlainText);
    connect(
        m_diagnosticsDock->MaterialClosureView(),
        &MaterialClosurePanel::SigLayerRequested,
        this,
        &MainWindow::OnMaterialClosureLayerRequested);
    connect(config_editor_panel_, &ConfigEditorPanel::configPathChanged, config_edit_, &QLineEdit::setText);
    connect(
        config_editor_panel_,
        &ConfigEditorPanel::configPathChanged,
        this,
        [this](const QString&)
        {
            SyncDiagnosticRequestedSettingsFromConfig();
            SyncProductionSettingsFromConfig();
            UpdateDiagnosticSettingsPresentation();
        });
    connect(config_editor_panel_, &ConfigEditorPanel::statusMessage, status_label_, &QLabel::setText);
    connect(
        config_editor_panel_,
        &ConfigEditorPanel::SigProductionSelectionChanged,
        this,
        [this]()
        {
            m_modelPreflightController.MarkStale();
            m_productionRunSession.Invalidate();
            config_editor_panel_->ShowProductionAdmissionState(
                ProductionAdmissionState::Stale,
                QStringLiteral("生产模式或 Profile 已改变，需要重新执行预检。"));
            SyncProductionSettingsFromConfig();
            UpdateActionAvailability();
        });
    connect(
        &config_document_,
        &ConfigDocument::changed,
        this,
        [this]()
        {
            if (m_transformedPreflightLoader.IsRunning())
            {
                m_transformedPreflightLoader.Cancel();
            }
            if (m_modelTopViewLoader.IsRunning())
            {
                m_modelTopViewLoader.Cancel();
            }
            if (!m_suppressPreflightStale)
            {
                m_modelPreflightController.MarkStale();
                m_productionRunSession.Invalidate();
            }
            SyncProductionSettingsFromConfig();
        });
    connect(
        &m_modelPreflightController,
        &ModelPreflightController::SigStateChanged,
        this,
        &MainWindow::OnModelPreflightStateChanged);
    connect(
        &m_sceneDocument,
        &SceneDocument::SigChanged,
        this,
        &MainWindow::OnSceneDocumentChanged);
    connect(
        &m_textureWhitePreflightService,
        &TextureWhitePreflightService::SigPreflightFinished,
        this,
        &MainWindow::OnTextureWhitePreflightFinished);
    connect(
        &m_sceneSelectionModel,
        &SceneSelectionModel::SigSelectionChanged,
        this,
        [this](const QString& instanceId)
        {
            if (instanceId.isEmpty()
                || !m_sceneDocument.SetCurrentInstance(instanceId))
            {
                return;
            }
            if (m_transformedPreflightLoader.IsRunning())
            {
                m_transformedPreflightLoader.Cancel();
            }
            if (m_sceneDocument.Geometry().has_value()
                && !m_sceneDocument.IsGeometryStale())
            {
                m_transformedPreflightLoader.RequestCurrent();
            }
        });
    m_sceneTransformController.SetProjectionRequester(
        [this](const SceneProjectionRequest& request)
        {
            m_modelTopViewLoader.RequestProjection(request);
        });
    connect(
        &m_sceneTransformController,
        &SceneTransformController::SigTransformChanged,
        this,
        [this]()
        {
            if (m_transformedPreflightLoader.IsRunning())
            {
                m_transformedPreflightLoader.Cancel();
            }
        });
    m_sceneBatchImportController.SetLoadHandlers(
        [this](const ModelTopViewLoadRequest& request)
        {
            m_modelTopViewLoader.RequestLoad(request);
            return m_modelTopViewLoader.Generation();
        },
        [this]()
        {
            m_modelTopViewLoader.Cancel();
        });
    connect(
        &m_modelTopViewLoader,
        &ModelTopViewLoader::SigLoadingFinished,
        &m_sceneBatchImportController,
        [this]()
        {
            m_sceneBatchImportController.OnLoadFinished(
                m_modelTopViewLoader.Generation());
        });
    connect(
        &m_sceneBatchImportController,
        &SceneBatchImportController::SigStateChanged,
        this,
        [this]()
        {
            UpdateBatchImportPresentation();
            status_label_->setText(
                m_sceneBatchImportController.StatusText());
            UpdateActionAvailability();
        });
    connect(
        &m_sceneBatchImportController,
        &SceneBatchImportController::SigFinished,
        this,
        [this]()
        {
            const SceneBatchImportSummary& summary =
                m_sceneBatchImportController.Summary();
            for (const SceneBatchImportItemResult& item :
                 summary.items)
            {
                if (item.status
                    == SceneBatchImportItemStatus::Failed)
                {
                    log_panel_->appendError(
                        item.errorcode
                        + QStringLiteral("：")
                        + item.path
                        + QStringLiteral("：")
                        + item.message);
                }
            }
            if (!summary.layouterror.isEmpty())
            {
                log_panel_->appendError(
                    summary.layouterrorcode
                    + QStringLiteral("：")
                    + summary.layouterror);
            }
        });
    m_sceneSliceActionController.Configure(
        [this]()
        {
            return BuildSceneSliceState();
        },
        [this](const SceneSliceActionRequest& request)
        {
            return WriteCurrentSceneSnapshot(request);
        },
        [this](const SceneSliceActionSnapshot& snapshot)
        {
            if (paths_.slicer_cli.trimmed().isEmpty()
                || runner_.IsRunning())
            {
                return false;
            }
            runCommand(
                QStringLiteral("切片当前场景"),
                paths_.slicer_cli,
                QStringList{
                    QStringLiteral("--scene-config"),
                    snapshot.effectiveconfigpath});
            return runner_.IsRunning();
        },
        [this]()
        {
            runner_.Stop();
        },
        [this](
            const SceneSliceActionSnapshot& snapshot,
            const qint64 elapsedMs)
        {
            return ValidateCurrentScenePackage(
                snapshot,
                elapsedMs);
        });
    connect(
        &m_sceneSliceActionController,
        &SceneSliceActionController::SigStateChanged,
        this,
        [this](
            const SceneSliceActionState,
            const QString& message)
        {
            status_label_->setText(message);
            UpdateActionAvailability();
        });
    connect(
        &m_sceneSliceActionController,
        &SceneSliceActionController::SigFailed,
        this,
        [this](const QString& code, const QString& message)
        {
            log_panel_->appendError(
                code + QStringLiteral("：") + message);
        });
    connect(
        &m_sceneSliceActionController,
        &SceneSliceActionController::SigPackageReady,
        this,
        [this](const QString& packageDir)
        {
            package_edit_->setText(packageDir);
            loadPackage(packageDir);
            m_mainWorkspaceTabs->setCurrentWidget(
                m_previewWorkspace);
        });
    connect(
        &m_modelTopViewLoader,
        &ModelTopViewLoader::SigLoadingFinished,
        this,
        [this]()
        {
            const bool selectionWillChange =
                !m_sceneDocument.CurrentInstanceId().isEmpty()
                && m_sceneSelectionModel.SelectedInstance()
                    != m_sceneDocument.CurrentInstanceId();
            if (!m_sceneDocument.CurrentInstanceId().isEmpty())
            {
                m_sceneSelectionModel.SetSelectedInstance(
                    m_sceneDocument.CurrentInstanceId());
            }
            if (selectionWillChange)
            {
                return;
            }
            if ((m_sceneDocument.State()
                     == SceneDocumentState::Ready
                 || m_sceneDocument.State()
                     == SceneDocumentState::Blocked)
                && !m_sceneDocument.IsGeometryStale()
                && m_sceneDocument.Instance().has_value())
            {
                m_transformedPreflightLoader.RequestCurrent();
            }
        });
    connect(
        &m_modelTopViewLoader,
        &ModelTopViewLoader::SigAutoLayoutFailed,
        this,
        [this](const QString& error)
        {
            status_label_->setText(
                QStringLiteral(
                    "模型已导入，但自动排版失败，可在“排版”页手动处理：")
                + error);
            log_panel_->appendError(error);
        });
    connect(
        m_modelTransformPanel,
        &ModelTransformPanel::SigSaveRequested,
        this,
        &MainWindow::OnSaveSceneTransform);
    connect(
        m_sceneActionBar,
        &SceneActionBar::SigImportRequested,
        this,
        &MainWindow::OnImportModelPreview);
    connect(
        m_sceneActionBar,
        &SceneActionBar::SigSaveRequested,
        this,
        &MainWindow::OnSaveSceneTransform);
    connect(
        m_sceneActionBar,
        &SceneActionBar::SigSliceRequested,
        this,
        &MainWindow::OnSliceCurrentScene);
    connect(
        m_sceneActionBar,
        &SceneActionBar::SigCancelRequested,
        &m_sceneSliceActionController,
        &SceneSliceActionController::Cancel);
    connect(
        m_sceneActionBar,
        &SceneActionBar::SigProfileRequested,
        this,
        &MainWindow::OnQuickProfileRequested);
    connect(
        m_contextInspector,
        &ContextInspector::SigOpenConfigRequested,
        this,
        [this]()
        {
            m_mainWorkspaceTabs->setCurrentWidget(
                m_configWorkspace);
        });
    connect(
        m_contextInspector,
        &ContextInspector::
            SigDiagnosticTextureSurfaceWidthChanged,
        this,
        [this](const double widthMm)
        {
            if (m_diagnosticAnalysisWorker.IsRunning())
            {
                m_diagnosticAnalysisWorker.Cancel();
            }
            m_lastDiagnosticAnalysisResult.reset();
            m_previewWorkspace->ClearDiagnosticAnalysis(
                QStringLiteral(
                    "纹理表面层宽度已变化。"));
            m_diagnosticTextureSurfaceWidthMm =
                widthMm;
            status_label_->setText(
                QStringLiteral(
                    "诊断纹理表面层宽度已设为 %1 mm；"
                    "尚未启动分析，不会修改生产 Profile。")
                    .arg(widthMm, 0, 'f', 2));
        });
    connect(
        m_contextInspector,
        &ContextInspector::
            SigProductionLegacyTopLayersChanged,
        this,
        &MainWindow::OnProductionLegacyTopLayersChanged);
    connect(
        m_contextInspector,
        &ContextInspector::
            SigProductionGlobalTextureChanged,
        this,
        &MainWindow::OnProductionGlobalTextureChanged);
    connect(
        m_contextInspector,
        &ContextInspector::
            SigProductionSingleMaterialChanged,
        this,
        &MainWindow::OnProductionSingleMaterialChanged);
    connect(
        m_contextInspector,
        &ContextInspector::
            SigDiagnosticModelFillMaterialChanged,
        this,
        [this](const QString& material)
        {
            if (m_diagnosticAnalysisWorker.IsRunning())
            {
                m_diagnosticAnalysisWorker.Cancel();
            }
            m_lastDiagnosticAnalysisResult.reset();
            m_previewWorkspace->ClearDiagnosticAnalysis(
                QStringLiteral(
                    "模型填充材料已变化。"));
            m_diagnosticModelFillMaterial =
                material;
            status_label_->setText(
                QStringLiteral(
                    "诊断模型填充材料已更新；"
                    "尚未启动分析，不会修改生产 Profile。"));
        });
    connect(
        m_contextInspector,
        &ContextInspector::SigDiagnosticStartRequested,
        this,
        &MainWindow::OnStartDiagnosticAnalysis);
    connect(
        m_contextInspector,
        &ContextInspector::SigDiagnosticCancelRequested,
        this,
        &MainWindow::OnCancelDiagnosticAnalysis);
    connect(
        &m_diagnosticAnalysisWorker,
        &DiagnosticAnalysisWorker::SigStarted,
        this,
        [this](const DiagnosticAnalysisIdentity&)
        {
            m_diagnosticAnalysisMessage =
                QStringLiteral(
                    "运行中（running）：后台正在执行拓扑、"
                    "距离、纹理分区和栅格映射。");
            UpdateDiagnosticSettingsPresentation();
        });
    connect(
        &m_diagnosticAnalysisWorker,
        &DiagnosticAnalysisWorker::SigFinished,
        this,
        &MainWindow::OnDiagnosticAnalysisFinished);
    connect(
        m_modelTransformPanel,
        &ModelTransformPanel::SigStatusMessage,
        status_label_,
        &QLabel::setText);
    connect(
        m_modelListPanel,
        &ModelListPanel::SigAddRequested,
        this,
        &MainWindow::OnImportModelPreview);
    connect(
        m_modelListPanel,
        &ModelListPanel::SigStatusMessage,
        status_label_,
        &QLabel::setText);
    connect(
        m_sceneLayoutPanel,
        &SceneLayoutPanel::SigStatusMessage,
        status_label_,
        &QLabel::setText);
    connect(
        modelTopViewFitButton,
        &QPushButton::clicked,
        m_modelTopViewWidget,
        &ModelTopViewWidget::FitToView);
    connect(
        &m_slicePreflightCoordinator,
        &SlicePreflightCoordinator::SigActionAdmitted,
        this,
        &MainWindow::OnPreflightActionAdmitted);
    connect(
        &m_slicePreflightCoordinator,
        &SlicePreflightCoordinator::SigActionBlocked,
        this,
        &MainWindow::OnPreflightActionBlocked);
    connect(
        &m_slicePreflightCoordinator,
        &SlicePreflightCoordinator::SigLegacyConfirmationRequired,
        this,
        &MainWindow::OnLegacyPreflightConfirmationRequired);
    connect(
        m_modelPreflightPanel,
        &ModelPreflightPanel::SigRecheckRequested,
        this,
        &MainWindow::OnRecheckModelPreflight);
    connect(
        m_modelPreflightPanel,
        &ModelPreflightPanel::SigCancelRequested,
        this,
        &MainWindow::OnCancelModelPreflight);

    LoadScenarios();
    if (!config_document_.document().isObject())
    {
        config_editor_panel_->loadConfig(config_edit_->text());
    }
    SyncDiagnosticRequestedSettingsFromConfig();
    SyncProductionSettingsFromConfig();
    loadPackage(package_edit_->text());
    UpdateModelPreflightUi();
    UpdateActionAvailability();
    QPushButton* jobImportButton =
        m_sceneActionBar->findChild<QPushButton*>(
            QStringLiteral("jobImportModelsButton"));
    QPushButton* jobSaveButton =
        m_sceneActionBar->findChild<QPushButton*>(
            QStringLiteral("jobSaveSceneButton"));
    QPushButton* jobCancelButton =
        m_sceneActionBar->findChild<QPushButton*>(
            QStringLiteral(
                "cancelCurrentSceneSliceButton"));
    if (jobImportButton != nullptr
        && jobSaveButton != nullptr
        && m_sliceCurrentSceneButton != nullptr
        && jobCancelButton != nullptr)
    {
        QWidget::setTabOrder(
            jobImportButton,
            jobSaveButton);
        QWidget::setTabOrder(
            jobSaveButton,
            m_sliceCurrentSceneButton);
        QWidget::setTabOrder(
            m_sliceCurrentSceneButton,
            jobCancelButton);
        QWidget::setTabOrder(
            jobCancelButton,
            m_mainWorkspaceTabs);
    }
    if (WorkspaceLayoutState::PersistenceEnabled())
    {
        QSettings settings(
            WorkspaceLayoutState::OrganizationName(),
            WorkspaceLayoutState::ApplicationName());
        WorkspaceLayoutState::Restore(
            settings,
            this,
            m_mainSplitter,
            m_contextInspector);
    }
}
