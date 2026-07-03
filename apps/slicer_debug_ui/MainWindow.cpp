#include "MainWindow.h"

#include <QDesktopServices>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

QString SanitizeSessionName(const QString& name)
{
    QString normalized = name.trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    normalized = normalized.trimmed();
    return normalized.isEmpty() ? QStringLiteral("model") : normalized;
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
    center_tabs->setDocumentMode(true);
    center_tabs->setTabPosition(QTabWidget::North);
    m_layerPreviewPanel = new LayerPreviewPanel(center_tabs);
    preview_panel_ = new PreviewPanel(center_tabs);
    report_panel_ = new ReportPanel(center_tabs);
    config_editor_panel_ = new ConfigEditorPanel(&config_document_, center_tabs);
    channel_chart_panel_ = new ChannelChartPanel(center_tabs);
    preview_overlay_panel_ = new PreviewOverlayPanel(center_tabs);
    center_tabs->addTab(m_layerPreviewPanel, "层预览");
    center_tabs->addTab(report_panel_, "报告");
    center_tabs->addTab(channel_chart_panel_, "曲线");
    center_tabs->addTab(config_editor_panel_, "配置");
    center_tabs->addTab(preview_overlay_panel_, "叠加预览");
    center_tabs->addTab(preview_panel_, "原始预览");
    center_tabs->setCurrentWidget(m_layerPreviewPanel);

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

    log_panel_ = new LogPanel(central);
    log_panel_->setMinimumHeight(140);
    auto* vertical_splitter = new QSplitter(Qt::Vertical, central);
    vertical_splitter->addWidget(main_splitter);
    vertical_splitter->addWidget(log_panel_);
    vertical_splitter->setStretchFactor(0, 3);
    vertical_splitter->setStretchFactor(1, 1);
    vertical_splitter->setSizes(QList<int>{700, 180});
    root_layout->addWidget(vertical_splitter);
    setCentralWidget(central);

    connect(&runner_, &ProcessRunner::started, this, &MainWindow::handleProcessStarted);
    connect(&runner_, &ProcessRunner::output, log_panel_, &LogPanel::appendOutput);
    connect(&runner_, &ProcessRunner::errorOutput, log_panel_, &LogPanel::appendError);
    connect(&runner_, &ProcessRunner::finished, this, &MainWindow::handleProcessFinished);
    connect(&runner_, &ProcessRunner::failed, this, &MainWindow::handleProcessFailed);
    connect(report_panel_, &ReportPanel::warningsChanged, warnings_view_, &QPlainTextEdit::setPlainText);
    connect(config_editor_panel_, &ConfigEditorPanel::configPathChanged, config_edit_, &QLineEdit::setText);
    connect(config_editor_panel_, &ConfigEditorPanel::statusMessage, status_label_, &QLabel::setText);

    LoadScenarios();
    config_editor_panel_->loadConfig(config_edit_->text());
    loadPackage(package_edit_->text());
}

void MainWindow::browseConfig() {
    const QString path = QFileDialog::getOpenFileName(this, "选择配置文件", paths_.repo_root, "JSON (*.json)");
    if (!path.isEmpty()) {
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
    if (config_document_.isDirty()) {
        log_panel_->appendError("当前配置已修改但尚未保存，运行切片仍使用磁盘上的配置文件。");
    }
    const QString config = absoluteFromRepo(config_editor_panel_->configPath().isEmpty() ? config_edit_->text()
                                                                                        : config_editor_panel_->configPath());
    pending_package_ = inferPackageFromConfig(config);
    runCommand("运行切片", paths_.slicer_cli, QStringList{"--config", config});
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

    QString packageDir;
    const QString configPath = CreateOneClickConfig(modelPath, &packageDir);
    if (configPath.isEmpty())
    {
        return;
    }

    Q_UNUSED(packageDir);
    RunOpenVdbDiagnostic(configPath, CreateOpenVdbReportPath(modelPath));
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
    auto* scenario_reload = makeButton("刷新", panel);
    auto* scenario_row = new QHBoxLayout();
    scenario_row->addWidget(new QLabel("场景/Profile"));
    scenario_row->addWidget(m_scenarioSelector, 1);
    scenario_row->addWidget(scenario_reload);
    layout->addLayout(scenario_row);
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

QString MainWindow::CreateOneClickConfig(const QString& modelPath, QString* packageDir) const
{
    const QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists() || !modelInfo.isFile())
    {
        QMessageBox::warning(nullptr, "模型文件不存在", "无法找到模型文件：\n" + modelPath);
        return {};
    }

    const QString suffix = modelInfo.suffix().toLower();
    const bool textureEnabled = suffix == "obj" || suffix == "3mf";
    const QString sessionName = SanitizeSessionName(modelInfo.completeBaseName())
        + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
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
                QJsonObject{{"enabled", textureEnabled},
                            {"applyMode", "solid_volume_from_top_surface"},
                            {"sampler", "bilinear"},
                            {"uvAddressMode", "clamp"},
                            {"flipV", true},
                            {"fallbackRgb", MakeIntArray({0, 0, 0})},
                            {"missingTexturePolicy", "warn_and_fallback"}});
    root.insert("support",
                QJsonObject{{"enabled", true},
                            {"mode", "bottom_projection"},
                            {"value", 0},
                            {"offsetMm", 0.0},
                            {"minAreaPx", 0}});
    root.insert("relief", QJsonObject{{"fillMode", "intersection_range"}, {"baseZMm", 0.0}});
    root.insert("preview",
                QJsonObject{{"enabled", true},
                            {"format", "png"},
                            {"interval", 10},
                            {"channels", textureEnabled ? MakeStringArray({"texture_rgb", "support"})
                                                         : MakeStringArray({"rgb", "support"})},
                            {"onlyNonEmptyLayers", true}});
    root.insert("experimental",
                QJsonObject{{"openvdbPipeline",
                             QJsonObject{{"enabled", false},
                                         {"engine", "legacy"},
                                         {"admissionMode", "strict_closed"},
                                         {"failurePolicy", "fail_fast"},
                                         {"allowNonProductionOutput", false},
                                         {"writeProductionRgbwsv", false}}}});

    const QString configPath = repo.filePath(sessionRoot + "/slice_config.generated.json");
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
    root.insert("autoOrient",
                QJsonObject{{"enabled", false},
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
                            {"missingTexturePolicy", "fail_fast"}});
    root.insert("support",
                QJsonObject{{"enabled", false},
                            {"mode", "none"},
                            {"value", 0},
                            {"offsetMm", 0.0},
                            {"minAreaPx", 0}});
    root.insert("relief", QJsonObject{{"fillMode", "intersection_range"}, {"baseZMm", 0.0}});
    root.insert("preview",
                QJsonObject{{"enabled", true},
                            {"format", "ppm"},
                            {"interval", 1},
                            {"channels", MakeStringArray({"texture_rgb", "rgb", "support", "white", "varnish"})},
                            {"onlyNonEmptyLayers", false},
                            {"pseudoColors",
                             QJsonObject{{"empty", MakeIntArray({255, 255, 255})},
                                         {"support", MakeIntArray({0, 255, 0})},
                                         {"white", MakeIntArray({0, 170, 255})},
                                         {"varnish", MakeIntArray({127, 127, 127})}}}});
    root.insert("experimental",
                QJsonObject{{"openvdbPipeline",
                             QJsonObject{{"enabled", true},
                                         {"engine", "openvdb"},
                                         {"admissionMode", "strict_closed"},
                                         {"failurePolicy", "fail_fast"},
                                         {"allowNonProductionOutput", false},
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
    config_editor_panel_->loadConfig(configPath);
    pending_package_ = packageDir;
    runCommand("运行切片", paths_.slicer_cli, QStringList{"--config", configPath});
}

void MainWindow::RunOpenVdbDiagnostic(const QString& configPath, const QString& reportPath)
{
    config_edit_->setText(configPath);
    config_editor_panel_->loadConfig(configPath);
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

    for (const ScenarioEntry& scenario : m_scenarioRegistry.Entries())
    {
        if (!scenario.enabled)
        {
            continue;
        }

        QString label = scenario.category.isEmpty() ? scenario.name : scenario.category + " / " + scenario.name;
        if (scenario.experimental || scenario.requiresopenvdb)
        {
            label += "（实验）";
        }
        m_scenarioSelector->addItem(label, scenario.id);
    }

    m_scenarioSelector->blockSignals(false);

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
    if (m_scenarioDescriptionLabel != nullptr)
    {
        m_scenarioDescriptionLabel->setText(description);
    }

    config_editor_panel_->loadConfig(configPath);
    if (!packageDir.isEmpty())
    {
        loadPackage(packageDir);
    }
}

void MainWindow::loadPackage(const QString& package_dir) {
    const PackageSummary package = package_loader_.load(absoluteFromRepo(package_dir));
    package_edit_->setText(package.package_dir);
    report_panel_->loadPackage(package);
    m_layerPreviewPanel->LoadPackage(package);
    preview_panel_->loadPackage(package);
    material_process_panel_->loadPackage(package);
    channel_chart_panel_->loadPackage(package);
    preview_overlay_panel_->loadPackage(package);
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
