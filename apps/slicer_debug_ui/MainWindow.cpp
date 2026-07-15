#include "MainWindow.h"

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
#include <QSizePolicy>
#include <QSplitter>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

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
    : QMainWindow(parent), paths_(ToolPaths::fromRepoRoot(std::move(repo_root))) {
    setWindowTitle("SliceSoft 切片调试界面");
    resize(1440, 900);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    auto* main_splitter = new QSplitter(Qt::Horizontal, central);

    QWidget* left = createProjectPanel();
    auto* center_tabs = new QTabWidget(main_splitter);
    center_tabs->setObjectName(QStringLiteral("mainWorkspaceTabs"));
    center_tabs->setDocumentMode(true);
    center_tabs->setTabPosition(QTabWidget::North);
    m_previewWorkspace = new PreviewWorkspace(center_tabs);
    config_editor_panel_ = new ConfigEditorPanel(&config_document_, center_tabs);
    const int previewWorkspaceTab = center_tabs->addTab(m_previewWorkspace, "预览");
    const int configTab = center_tabs->addTab(config_editor_panel_, "配置");
    center_tabs->setTabToolTip(previewWorkspaceTab, "统一预览工作区：在生产层检查、材料叠加和原始调试预览之间切换并保持同一真实 layerIndex。");
    center_tabs->setTabToolTip(configTab, "编辑当前 JSON 配置；常用材料、支撑、预览和实验选项在这里。");
    center_tabs->setCurrentWidget(m_previewWorkspace);

    QWidget* right = createRightPanel();
    left->setMinimumWidth(320);
    left->setMaximumWidth(520);
    center_tabs->setMinimumWidth(620);
    right->setMinimumWidth(320);
    main_splitter->addWidget(left);
    main_splitter->addWidget(center_tabs);
    main_splitter->addWidget(right);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);
    main_splitter->setSizes(QList<int>{360, 840, 360});

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
    connect(&runner_, &ProcessRunner::output, log_panel_, &LogPanel::appendOutput);
    connect(&runner_, &ProcessRunner::errorOutput, log_panel_, &LogPanel::appendError);
    connect(&runner_, &ProcessRunner::finished, this, &MainWindow::handleProcessFinished);
    connect(&runner_, &ProcessRunner::failed, this, &MainWindow::handleProcessFailed);
    connect(report_panel_, &ReportPanel::warningsChanged, warnings_view_, &QPlainTextEdit::setPlainText);
    connect(config_editor_panel_, &ConfigEditorPanel::configPathChanged, config_edit_, &QLineEdit::setText);
    connect(config_editor_panel_, &ConfigEditorPanel::statusMessage, status_label_, &QLabel::setText);

    LoadScenarios();
    if (!config_document_.document().isObject())
    {
        config_editor_panel_->loadConfig(config_edit_->text());
    }
    loadPackage(package_edit_->text());
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
    const EffectiveConfigResult result = GenerateEffectiveConfig(
        QString{},
        package_edit_->text(),
        SliceEngineRole::LegacyProduction,
        QStringLiteral("legacy"));
    if (!result.IsValid())
    {
        status_label_->setText("生效配置校验失败，已停止切片。");
        log_panel_->appendError(result.errors.join("\n"));
        return;
    }

    pending_package_ = absoluteFromRepo(result.document.object().value("output").toObject().value("packageDir").toString());
    config_edit_->setText(result.generatedconfigpath);
    runCommand("运行切片", paths_.slicer_cli, QStringList{"--config", result.generatedconfigpath});
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

    RunGeneratedConfig(configPath, packageDir);
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

    RunOpenVdbDiagnostic(result.generatedconfigpath, CreateOpenVdbReportPath(modelPath));
}

void MainWindow::OnImportModelOpenVdbCandidate()
{
    if (!QFileInfo::exists(paths_.openvdb_slicer_cli))
    {
        QMessageBox::warning(this,
                             "OpenVDB 构建不存在",
                             "未找到 OpenVDB ON 版本 slicer_cli：\n" + paths_.openvdb_slicer_cli
                                 + "\n\n请先构建 build-openvdb-09p。");
        return;
    }

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

    RunOpenVdbCandidate(configPath, packageDir);
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

void MainWindow::handleProcessStarted(const QString& command) {
    setBusy(true);
    status_label_->setText("正在执行：" + current_action_);
    log_panel_->appendCommand(command);
}

void MainWindow::handleProcessFinished(const int exit_code, const qint64 elapsed_ms) {
    log_panel_->appendResult(exit_code, elapsed_ms);
    setBusy(false);
    status_label_->setText(exit_code == 0 ? "通过：" + current_action_ : "失败：" + current_action_);
    if (exit_code != 0) {
        if (current_action_ == "OpenVDB 候选切片" && !pending_package_.isEmpty())
        {
            package_edit_->setText(pending_package_);
            loadPackage(pending_package_);
            status_label_->setText("失败：OpenVDB 候选切片（已加载诊断报告）");
        }
        return;
    }
    if ((current_action_ == "运行切片" || current_action_ == "OpenVDB 候选切片") && !pending_package_.isEmpty()) {
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
}

QWidget* MainWindow::createProjectPanel() {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);

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
    scenario_row->addWidget(m_showAdvancedScenariosCheck);
    scenario_row->addWidget(scenario_reload);
    layout->addLayout(scenario_row);
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
    return panel;
}

QWidget* MainWindow::createRunPanel() {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    build_button_ = makeButton("构建调试版", panel);
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
    layout->addWidget(build_button_);
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

    connect(build_button_, &QPushButton::clicked, this, &MainWindow::buildDebug);
    connect(m_importSliceButton, &QPushButton::clicked, this, &MainWindow::OnImportModelAndSlice);
    connect(m_importOpenVdbButton, &QPushButton::clicked, this, &MainWindow::OnImportModelOpenVdbDiagnostic);
    connect(m_importOpenVdbCandidateButton, &QPushButton::clicked, this, &MainWindow::OnImportModelOpenVdbCandidate);
    connect(run_slicer_button_, &QPushButton::clicked, this, &MainWindow::runSlicer);
    connect(run_rip_button_, &QPushButton::clicked, this, &MainWindow::runRipSummary);
    connect(regression_button_, &QPushButton::clicked, this, &MainWindow::runQuickRegression);
    connect(compare_button_, &QPushButton::clicked, this, &MainWindow::compareProfiles);
    connect(load_button, &QPushButton::clicked, this, &MainWindow::loadPackageFromEdit);
    connect(open_button, &QPushButton::clicked, this, &MainWindow::openOutputFolder);
    return panel;
}

QWidget* MainWindow::createRightPanel() {
    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    material_process_panel_ = new MaterialProcessPanel(tabs);
    warnings_view_ = new QPlainTextEdit(tabs);
    warnings_view_->setReadOnly(true);
    compare_view_ = new QPlainTextEdit(tabs);
    compare_view_->setReadOnly(true);
    tabs->addTab(material_process_panel_, "参数");
    tabs->addTab(warnings_view_, "诊断");
    tabs->addTab(compare_view_, "工艺对比");
    tabs->setMinimumWidth(360);
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
    settings.layerthicknessmm = config_document_.value({"output", "layerThicknessMm"})
                                    .toDouble(settings.layerthicknessmm);

    const QString modelFill = config_document_.value({"modelFill", "material"}).toString();
    if (modelFill == QStringLiteral("varnish"))
    {
        settings.modelfillmaterial = ModelFillMaterial::Varnish;
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

    const QString profilePart = SanitizeSessionName(
        m_currentProfileId.isEmpty() ? QStringLiteral("custom") : m_currentProfileId);
    const QString modelPart = SanitizeSessionName(modelInfo.completeBaseName());
    const QString sessionName = modelPart + "_" + profilePart + "_" + sessionTag + "_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString relativeSessionRoot = QStringLiteral("output/ui_sessions/") + sessionName;
    const QString generatedPath = QDir(paths_.repo_root).filePath(
        relativeSessionRoot + QStringLiteral("/slice_config.generated.json"));
    const QString effectivePackage = packageDirOverride.trimmed().isEmpty()
        ? QDir(paths_.repo_root).filePath(relativeSessionRoot + QStringLiteral("/package"))
        : absoluteFromRepo(packageDirOverride);

    if (config_document_.value({"input", "modelPath"}).toString() != resolvedModel)
    {
        config_document_.setValue({"input", "modelPath"}, resolvedModel);
    }
    if (config_document_.value({"output", "packageDir"}).toString() != effectivePackage)
    {
        config_document_.setValue({"output", "packageDir"}, effectivePackage);
    }

    EffectiveConfigRequest request;
    request.profileid = m_currentProfileId.isEmpty() ? QStringLiteral("custom") : m_currentProfileId;
    request.templatepath = config_document_.path();
    request.generatedconfigpath = generatedPath;
    request.originaldocument = config_document_.originalDocument();
    request.overridedocument = config_document_.document();
    request.settings = BuildCurrentSettings(resolvedModel, effectivePackage, engineRole);

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
    const EffectiveConfigResult result = GenerateEffectiveConfig(
        modelPath,
        QString{},
        SliceEngineRole::LegacyProduction,
        QStringLiteral("legacy"));
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
                            {"dpiX", 600},
                            {"dpiY", 600},
                            {"layerThicknessMm", 0.01},
                            {"channelOrder", MakeStringArray({"R", "G", "B", "W", "S", "V"})},
                            {"bitDepth", 8},
                            {"planarConfig", "contiguous"},
                            {"storageMode", "stripped"},
                            {"rowsPerStrip", 64}});
    root.insert("modelTransform",
                QJsonObject{{"unit", "mm"},
                            {"scale", MakeNumberArray({0.8, 0.8, 0.8})},
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

void MainWindow::RunGeneratedConfig(const QString& configPath, const QString& packageDir)
{
    config_edit_->setText(configPath);
    package_edit_->setText(packageDir);
    pending_package_ = packageDir;
    runCommand("运行切片", paths_.slicer_cli, QStringList{"--config", configPath});
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
    config_editor_panel_->loadConfig(configPath);
    pending_package_ = packageDir;
    runCommand("OpenVDB 候选切片",
               paths_.openvdb_slicer_cli,
               QStringList{"--config", configPath, "--openvdb-candidate-slice"});
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
    SetValue(
        {"modelFill", "material"},
        settings.modelfillmaterial == ModelFillMaterial::Varnish ? QStringLiteral("varnish")
                                                                  : QStringLiteral("white"));
    SetValue({"modelFill", "enabled"}, true);
    SetValue({"modelFill", "emptyAllowedInProduction"}, false);
    SetValue({"modelFill", "legacyRgbFallback"}, false);
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
    package_edit_->setText(package.package_dir);
    m_diagnosticsDock->LoadPackage(package);
    m_previewWorkspace->LoadPackage(package);
    material_process_panel_->loadPackage(package);
    warnings_view_->setPlainText(package.warnings.join('\n'));
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
    build_button_->setEnabled(!busy);
    m_importSliceButton->setEnabled(!busy);
    m_importOpenVdbButton->setEnabled(!busy);
    m_importOpenVdbCandidateButton->setEnabled(!busy);
    run_slicer_button_->setEnabled(!busy);
    run_rip_button_->setEnabled(!busy);
    regression_button_->setEnabled(!busy);
    compare_button_->setEnabled(!busy);
}
