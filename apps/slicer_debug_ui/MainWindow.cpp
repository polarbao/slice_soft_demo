#include "MainWindow.h"

#include "services/ProductionProfileSourceResolver.h"
#include "widgets/MaterialClosurePanel.h"
#include "slicer_core/config.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDesktopServices>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kPathEditMinimumWidth = 140;
constexpr int kPathLabelMinimumWidth = 62;
constexpr int kBrowseButtonWidth = 44;

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
        || action == QStringLiteral("OpenVDB 候选切片");
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
                       {"material", "white"},
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

}  // namespace

MainWindow::MainWindow(QString repo_root, QWidget* parent)
    : QMainWindow(parent),
      paths_(ToolPaths::FromRepoRoot(std::move(repo_root))),
      m_modelPreflightController(this),
      m_slicePreflightCoordinator(&m_modelPreflightController, this),
      m_sceneDocument(this),
      m_sceneSelectionModel(this),
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
      m_transformedPreflightLoader(
          &m_sceneDocument,
          &m_sceneModelRepository,
          this)
{
    setWindowTitle("SliceSoft 切片调试界面");
    resize(1440, 900);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    auto* main_splitter = new QSplitter(Qt::Horizontal, central);
    main_splitter->setObjectName(QStringLiteral("mainSplitter"));

    QWidget* left = createProjectPanel();
    m_mainWorkspaceTabs = new QTabWidget(main_splitter);
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
    auto* modelWorkspaceSplitter = new QSplitter(
        Qt::Horizontal,
        m_modelTopViewWorkspace);
    modelWorkspaceSplitter->setObjectName(
        QStringLiteral("modelTransformWorkspaceSplitter"));
    auto* modelSideTabs = new QTabWidget(m_modelTopViewWorkspace);
    modelSideTabs->setObjectName(QStringLiteral("modelSceneSideTabs"));
    modelSideTabs->setDocumentMode(true);
    modelSideTabs->setMinimumWidth(250);
    modelSideTabs->setMaximumWidth(330);
    modelSideTabs->addTab(m_modelListPanel, QStringLiteral("模型列表"));
    modelSideTabs->addTab(m_modelTransformPanel, QStringLiteral("变换"));
    modelSideTabs->addTab(m_sceneLayoutPanel, QStringLiteral("排版"));
    modelWorkspaceSplitter->addWidget(m_modelTopViewWidget);
    modelWorkspaceSplitter->addWidget(modelSideTabs);
    modelWorkspaceSplitter->setStretchFactor(0, 1);
    modelWorkspaceSplitter->setStretchFactor(1, 0);
    modelWorkspaceSplitter->setCollapsible(0, false);
    modelWorkspaceSplitter->setCollapsible(1, false);
    modelWorkspaceSplitter->setSizes(QList<int>{720, 280});
    modelTopViewLayout->addWidget(modelWorkspaceSplitter, 1);
    m_previewWorkspace = new PreviewWorkspace(m_mainWorkspaceTabs);
    auto* configScrollArea = new QScrollArea(m_mainWorkspaceTabs);
    configScrollArea->setObjectName(QStringLiteral("configEditorScrollArea"));
    configScrollArea->setWidgetResizable(true);
    config_editor_panel_ = new ConfigEditorPanel(&config_document_, configScrollArea);
    configScrollArea->setWidget(config_editor_panel_);
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
    left->setMinimumWidth(280);
    left->setMaximumWidth(440);
    m_mainWorkspaceTabs->setMinimumWidth(400);
    right->setMinimumWidth(240);
    right->setMaximumWidth(420);
    main_splitter->addWidget(left);
    main_splitter->addWidget(m_mainWorkspaceTabs);
    main_splitter->addWidget(right);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);
    main_splitter->setSizes(QList<int>{320, 800, 300});

    root_layout->addWidget(main_splitter);
    setCentralWidget(central);

    m_diagnosticsDock = new DiagnosticsDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsDock);
    m_diagnosticsDock->SetExpanded(false);
    report_panel_ = m_diagnosticsDock->ReportView();
    channel_chart_panel_ = m_diagnosticsDock->ChartView();
    log_panel_ = m_diagnosticsDock->LogView();

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    QAction* diagnosticsAction = m_diagnosticsDock->toggleViewAction();
    diagnosticsAction->setObjectName(QStringLiteral("diagnosticsToggleAction"));
    diagnosticsAction->setText(QStringLiteral("诊断区域"));
    viewMenu->addAction(diagnosticsAction);

    connect(&runner_, &ProcessRunner::started, this, &MainWindow::handleProcessStarted);
    connect(&runner_, &ProcessRunner::output, this, &MainWindow::OnProcessOutput);
    connect(&runner_, &ProcessRunner::errorOutput, log_panel_, &LogPanel::appendError);
    connect(&runner_, &ProcessRunner::finished, this, &MainWindow::handleProcessFinished);
    connect(&runner_, &ProcessRunner::failed, this, &MainWindow::handleProcessFailed);
    connect(report_panel_, &ReportPanel::warningsChanged, warnings_view_, &QPlainTextEdit::setPlainText);
    connect(
        m_diagnosticsDock->MaterialClosureView(),
        &MaterialClosurePanel::SigLayerRequested,
        this,
        &MainWindow::OnMaterialClosureLayerRequested);
    connect(config_editor_panel_, &ConfigEditorPanel::configPathChanged, config_edit_, &QLineEdit::setText);
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
            else if (m_sceneDocument.Geometry().has_value())
            {
                m_sceneDocument.Reset();
            }
            if (!m_suppressPreflightStale)
            {
                m_modelPreflightController.MarkStale();
                m_productionRunSession.Invalidate();
            }
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
    loadPackage(package_edit_->text());
    UpdateModelPreflightUi();
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
    UpdateActionAvailability();
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

void MainWindow::handleProcessStarted(const QString& command) {
    setBusy(true);
    status_label_->setText("正在执行：" + current_action_);
    log_panel_->appendCommand(command);
    m_sliceProgressParser.Reset();
    m_lastSliceTimingEvent.reset();
    if (m_sliceTimingPanel != nullptr && IsSlicingAction(current_action_))
    {
        m_sliceTimingPanel->Reset(current_action_);
    }
}

void MainWindow::handleProcessFinished(const int exit_code, const qint64 elapsed_ms) {
    log_panel_->appendResult(exit_code, elapsed_ms);
    setBusy(false);
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
    status_label_->setText("进程错误：" + message);
    m_productionRunSession.Invalidate();
    if (m_sliceTimingPanel != nullptr && IsSlicingAction(current_action_))
    {
        m_sliceTimingPanel->Finish(false, 0);
    }
}

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
    m_sliceCurrentSceneButton =
        makeButton("切片当前场景", panel);
    m_sliceCurrentSceneButton->setObjectName(
        QStringLiteral("sliceCurrentSceneButton"));
    m_sliceCurrentSceneButton->setMinimumHeight(36);
    m_sliceCurrentSceneButton->setEnabled(false);
    m_sliceCurrentSceneButton->setToolTip(
        QStringLiteral(
            "13B-08-02/03 将接通场景生产服务；"
            "当前禁止使用旧单模型配置绕过 SceneDocument"));
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
    layout->addWidget(m_sliceCurrentSceneButton);
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
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("rightDiagnosticsPanel"));
    tabs->setDocumentMode(true);
    material_process_panel_ = new MaterialProcessPanel(tabs);
    warnings_view_ = new QPlainTextEdit(tabs);
    warnings_view_->setReadOnly(true);
    compare_view_ = new QPlainTextEdit(tabs);
    compare_view_->setReadOnly(true);
    m_modelPreflightPanel = new ModelPreflightPanel(tabs);
    tabs->addTab(material_process_panel_, "参数");
    tabs->addTab(m_modelPreflightPanel, QStringLiteral("模型预检"));
    tabs->addTab(warnings_view_, "诊断");
    tabs->addTab(compare_view_, "工艺对比");
    tabs->setMinimumWidth(240);
    return tabs;
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
    settings.preview.enabled = config_document_.value({"preview", "enabled"})
                                   .toBool(settings.preview.enabled);
    settings.preview.interval = config_document_.value({"preview", "interval"})
                                    .toInt(settings.preview.interval);
    settings.enginerole = engineRole;
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

    const QString profilePart = SanitizeSessionName(source.profileid);
    const QString modelPart = SanitizeSessionName(modelInfo.completeBaseName());
    const QString sessionName = modelPart + "_" + profilePart + "_" + sessionTag + "_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
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

    EffectiveConfigResult result = EffectiveConfigGenerator().Generate(request);
    config_editor_panel_->ShowEffectiveConfig(result);
    if (result.IsValid())
    {
        status_label_->setText(QStringLiteral("已生成并校验生效配置：") + result.generatedconfigpath);
    }
    return result;
}

QString MainWindow::CreateOneClickConfig(const QString& modelPath, QString* packageDir)
{
    const slicer_core::SlicePipelineMode selectedMode =
        config_editor_panel_->SelectedProductionMode();
    const EffectiveConfigResult result = GenerateEffectiveConfig(
        modelPath,
        QString{},
        SliceEngineRole::LegacyProduction,
        ProductionModeSessionTag(selectedMode));
    if (!result.IsValid())
    {
        status_label_->setText("一键切片配置校验失败。");
        log_panel_->appendError(result.errors.join("\n"));
        return {};
    }

    if (packageDir != nullptr)
    {
        *packageDir = absoluteFromRepo(
            result.document.object().value("output").toObject().value("packageDir").toString());
    }
    return result.generatedconfigpath;
}

QString MainWindow::CreateOpenVdbCandidateConfig(const QString& modelPath, QString* packageDir) const
{
    const QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists() || !modelInfo.isFile())
    {
        QMessageBox::warning(nullptr, "模型文件不存在", "无法找到模型文件：\n" + modelPath);
        return {};
    }

    const QString suffix = modelInfo.suffix().toLower();
    if (suffix != "obj" && suffix != "3mf")
    {
        QMessageBox::warning(nullptr, "模型格式不支持", "OpenVDB 候选切片当前只接受 OBJ / 3MF 模型。");
        return {};
    }

    const QString sessionName = SanitizeSessionName(modelInfo.completeBaseName())
        + "_openvdb_candidate_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString sessionRoot = "output/ui_sessions/" + sessionName;
    const QString relativePackageDir = sessionRoot + "/package";
    const int dpiX = config_document_.value({"output", "dpiX"})
                         .toInt(slicer_core::kDefaultOutputDpiX);
    const int dpiY = config_document_.value({"output", "dpiY"})
                         .toInt(slicer_core::kDefaultOutputDpiY);

    QDir repo(paths_.repo_root);
    if (!repo.mkpath(sessionRoot))
    {
        QMessageBox::warning(nullptr, "无法创建会话目录", "无法创建目录：\n" + repo.filePath(sessionRoot));
        return {};
    }

    QJsonObject root;
    root.insert("slicingMode", "relief_heightfield");
    root.insert("input",
                QJsonObject{{"modelPath", QDir::fromNativeSeparators(modelInfo.absoluteFilePath())}, {"format", "auto"}});
    root.insert("output",
                QJsonObject{{"packageDir", relativePackageDir},
                            {"dpiX", dpiX},
                            {"dpiY", dpiY},
                            {"layerThicknessMm", 0.01},
                            {"channelOrder", MakeStringArray({"R", "G", "B", "W", "S", "V"})},
                            {"bitDepth", 8},
                            {"planarConfig", "contiguous"},
                            {"storageMode", "stripped"},
                            {"rowsPerStrip", 64}});
    root.insert("modelTransform",
                QJsonObject{{"unit", "mm"},
                            {"scale", MakeNumberArray({1.0, 1.0, 1.0})},
                            {"rotationDeg", MakeNumberArray({0.0, 0.0, 0.0})},
                            {"translationMm", MakeNumberArray({0.0, 0.0, 0.0})}});
    root.insert("autoOrient",
                QJsonObject{{"enabled", true},
                            {"maxHeightMm", 6.0},
                            {"strategy", "minimize_height_by_right_angle_rotation"}});
    root.insert("background", QJsonObject{{"value", 255}});
    root.insert("modelMaterial",
                QJsonObject{{"materialChannel", "RGB"},
                            {"applyMode", "solid_volume"},
                            {"rgb", MakeIntArray({0, 0, 0})},
                            {"whiteValue", 255},
                            {"varnishValue", 255}});
    root.insert("texture",
                QJsonObject{{"enabled", true},
                            {"applyMode", "surface_shell_from_sdf"},
                            {"sampler", "nearest"},
                            {"uvAddressMode", "clamp"},
                            {"flipV", true},
                            {"fallbackRgb", MakeIntArray({255, 0, 255})},
                            {"missingTexturePolicy", "fail_fast"},
                            {"nonSurfaceRgbPolicy", "empty"}});
    root.insert("modelFill", MakeDefaultModelFillConfig());
    root.insert("support", MakeDefaultSupportConfig());
    root.insert("surfaceVarnish", MakeDefaultSurfaceVarnishConfig());
    root.insert("outerVarnish", MakeDefaultOuterVarnishConfig());
    root.insert("relief", QJsonObject{{"fillMode", "intersection_range"}, {"baseZMm", 0.0}});
    root.insert("preview",
                QJsonObject{{"enabled", true},
                            {"format", "ppm"},
                            {"interval", 1},
                            {"channels", MakeStringArray({"texture_rgb", "rgb", "support", "white", "varnish"})},
                            {"onlyNonEmptyLayers", false},
                            {"pseudoColors", MakeDefaultPreviewPseudoColors()}});
    root.insert("experimental",
                QJsonObject{{"openvdbPipeline",
                             QJsonObject{{"enabled", true},
                                         {"engine", "openvdb"},
                                         {"admissionMode", "strict_closed"},
                                         {"failurePolicy", "non_production_only"},
                                         {"allowNonProductionOutput", true},
                                         {"writeProductionRgbwsv", true}}}});

    const QString configPath = repo.filePath(sessionRoot + "/slice_config.openvdb_candidate.json");
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QMessageBox::warning(nullptr, "无法写入配置", "无法写入配置文件：\n" + configPath);
        return {};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    if (packageDir != nullptr)
    {
        *packageDir = repo.filePath(relativePackageDir);
    }
    return configPath;
}

QString MainWindow::CreateOpenVdbReportPath(const QString& modelPath) const
{
    const QFileInfo modelInfo(modelPath);
    const QString sessionName = SanitizeSessionName(modelInfo.completeBaseName())
        + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString reportDir = "output/ui_sessions/" + sessionName + "_openvdb/reports";
    QDir repo(paths_.repo_root);
    repo.mkpath(reportDir);
    return repo.filePath(reportDir + "/experimental_openvdb_shell_report.json");
}

void MainWindow::RunGeneratedConfig(const SlicePreflightAction& action)
{
    ProductionSliceRunRequest request;
    request.mode = action.productionmode;
    request.profileid = action.productionprofileid;
    request.sessionid = action.sessionid;
    request.configpath = action.configpath;
    request.packagedir = action.packagedir;
    const QStringList errors = m_productionRunSession.Begin(request);
    if (!errors.isEmpty())
    {
        status_label_->setText(QStringLiteral("生产切片会话身份无效，进程未启动。"));
        log_panel_->appendError(errors.join(QStringLiteral("\n")));
        return;
    }

    config_editor_panel_->ClearProductionResult();
    config_edit_->setText(action.configpath);
    package_edit_->setText(action.packagedir);
    pending_package_.clear();
    runCommand(
        "运行切片",
        paths_.slicer_cli,
        QStringList{"--config", action.configpath});
}

void MainWindow::RunOpenVdbDiagnostic(const QString& configPath, const QString& reportPath)
{
    config_edit_->setText(configPath);
    compare_output_ = reportPath;
    pending_package_.clear();
    runCommand("OpenVDB 实验诊断",
               paths_.slicer_cli,
               QStringList{"--config",
                           configPath,
                           "--experimental-openvdb-shell",
                           "--admission-mode",
                           "diagnostic_only",
                           "--experimental-report",
                           reportPath});
}

void MainWindow::RunOpenVdbCandidate(const QString& configPath, const QString& packageDir)
{
    config_edit_->setText(configPath);
    package_edit_->setText(packageDir);
    m_suppressPreflightStale = true;
    config_editor_panel_->loadConfig(configPath);
    m_suppressPreflightStale = false;
    pending_package_ = packageDir;
    runCommand("OpenVDB 候选切片",
               paths_.openvdb_slicer_cli,
               QStringList{"--config", configPath, "--openvdb-candidate-slice"});
}

void MainWindow::RequestSlicePreflight(const SlicePreflightAction& action)
{
    config_edit_->setText(action.configpath);
    if (!action.packagedir.isEmpty())
    {
        package_edit_->setText(action.packagedir);
    }
    pending_package_.clear();
    m_productionRunSession.Invalidate();
    if (action.kind == SlicePreflightActionKind::Legacy
        || action.kind == SlicePreflightActionKind::GlobalProduction)
    {
        config_editor_panel_->ShowProductionAdmissionState(
            ProductionAdmissionState::Running,
            QStringLiteral("正在执行当前模型和所选生产模式的预检。"));
    }
    status_label_->setText(QStringLiteral("正在执行模型预检，尚未启动切片进程。"));
    m_slicePreflightCoordinator.RequestAction(action);
}

void MainWindow::UpdateModelPreflightUi()
{
    ModelPreflightPresentation presentation =
        ModelPreflightPresenter::Present(
            m_modelPreflightController.CurrentExecution(),
            m_modelPreflightController.CurrentMode());
    if (!m_modelPreflightController.LastCapabilityDiagnostic().isEmpty())
    {
        presentation.detail += QStringLiteral("；capability=")
            + m_modelPreflightController.LastCapabilityDiagnostic();
    }
    if (m_modelPreflightPanel != nullptr)
    {
        m_modelPreflightPanel->ShowPresentation(presentation);
    }
    if (m_modelPreflightCompactState != nullptr)
    {
        m_modelPreflightCompactState->setText(
            QStringLiteral("预检：") + presentation.state);
        m_modelPreflightCompactState->setToolTip(
            presentation.admission + QStringLiteral("\n") + presentation.detail);
    }
    if (m_modelPreflightCompactMode != nullptr)
    {
        m_modelPreflightCompactMode->setText(presentation.mode);
    }
    if (m_modelPreflightRecheckButton != nullptr)
    {
        m_modelPreflightRecheckButton->setEnabled(
            presentation.canrecheck && !m_processBusy);
    }
    if (m_modelPreflightCancelButton != nullptr)
    {
        m_modelPreflightCancelButton->setEnabled(presentation.cancancel);
    }
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
    m_sliceCurrentSceneButton->setEnabled(false);
    m_sliceCurrentSceneButton->setToolTip(
        batchImportRunning
            ? QStringLiteral("批量导入完成后才能切片当前场景。")
            : m_sceneDocument.InstanceCount() == 0U
            ? QStringLiteral("请先导入模型。")
            : QStringLiteral(
                  "场景生产服务将在 13B-08-02/03 接通；"
                  "当前禁止旧单模型配置绕行。"));
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
    SetValue({"modelFill", "scope"}, QStringLiteral("below_texture_surface"));
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
    SetValue({"surfaceVarnish", "enabled"}, settings.surfacevarnish.enabled);
    SetValue({"surfaceVarnish", "thicknessPx"}, settings.surfacevarnish.thicknesspx);
    SetValue({"outerVarnish", "enabled"}, settings.outervarnish.enabled);
    SetValue({"outerVarnish", "thicknessMm"}, settings.outervarnish.thicknessmm);
    SetValue({"outerVarnish", "pixelPitchUm"}, settings.outervarnish.pixelpitchum);
    SetValue({"preview", "enabled"}, settings.preview.enabled);
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
    if (!packageDir.isEmpty())
    {
        loadPackage(packageDir);
    }
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
    runner_.run(program, args, paths_.repo_root);
}

void MainWindow::setBusy(const bool busy) {
    m_processBusy = busy;
    UpdateActionAvailability();
    UpdateModelPreflightUi();
}
