#include "MainWindow.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QPushButton* makeButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
}

QLineEdit* makePathEdit(const QString& text, QWidget* parent) {
    auto* edit = new QLineEdit(text, parent);
    edit->setMinimumWidth(320);
    return edit;
}

void addPathRow(QVBoxLayout* layout, const QString& label, QLineEdit* edit, QPushButton* browse) {
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(label));
    row->addWidget(edit, 1);
    row->addWidget(browse);
    layout->addLayout(row);
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
    m_layerPreviewPanel = new LayerPreviewPanel(center_tabs);
    preview_panel_ = new PreviewPanel(center_tabs);
    report_panel_ = new ReportPanel(center_tabs);
    config_editor_panel_ = new ConfigEditorPanel(&config_document_, center_tabs);
    channel_chart_panel_ = new ChannelChartPanel(center_tabs);
    preview_overlay_panel_ = new PreviewOverlayPanel(center_tabs);
    center_tabs->addTab(m_layerPreviewPanel, "层预览");
    center_tabs->addTab(preview_panel_, "预览");
    center_tabs->addTab(report_panel_, "报告");
    center_tabs->addTab(config_editor_panel_, "配置");
    center_tabs->addTab(channel_chart_panel_, "曲线");
    center_tabs->addTab(preview_overlay_panel_, "叠加预览");

    QWidget* right = createRightPanel();
    main_splitter->addWidget(left);
    main_splitter->addWidget(center_tabs);
    main_splitter->addWidget(right);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);

    log_panel_ = new LogPanel(central);
    auto* vertical_splitter = new QSplitter(Qt::Vertical, central);
    vertical_splitter->addWidget(main_splitter);
    vertical_splitter->addWidget(log_panel_);
    vertical_splitter->setStretchFactor(0, 3);
    vertical_splitter->setStretchFactor(1, 1);
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
        return;
    }
    if (current_action_ == "运行切片" && !pending_package_.isEmpty()) {
        package_edit_->setText(pending_package_);
        loadPackage(pending_package_);
    } else if (current_action_ == "对比工艺配置") {
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
    addPathRow(layout, "配置文件", config_edit_, config_browse);
    addPathRow(layout, "输出包", package_edit_, package_browse);
    addPathRow(layout, "对比包 A", profile_a_edit_, profile_a_browse);
    addPathRow(layout, "对比包 B", profile_b_edit_, profile_b_browse);

    repo_label_ = new QLabel("仓库根目录：" + paths_.repo_root, panel);
    slicer_label_ = new QLabel("切片工具：" + paths_.slicer_cli, panel);
    rip_label_ = new QLabel("RIP 校验工具：" + paths_.rip_reader, panel);
    repo_label_->setWordWrap(true);
    slicer_label_->setWordWrap(true);
    rip_label_->setWordWrap(true);
    layout->addWidget(repo_label_);
    layout->addWidget(slicer_label_);
    layout->addWidget(rip_label_);
    layout->addWidget(createRunPanel());
    layout->addStretch(1);

    connect(config_browse, &QPushButton::clicked, this, &MainWindow::browseConfig);
    connect(package_browse, &QPushButton::clicked, this, &MainWindow::browsePackage);
    connect(profile_a_browse, &QPushButton::clicked, this, &MainWindow::browseProfileA);
    connect(profile_b_browse, &QPushButton::clicked, this, &MainWindow::browseProfileB);
    return panel;
}

QWidget* MainWindow::createRunPanel() {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    build_button_ = makeButton("构建调试版", panel);
    run_slicer_button_ = makeButton("运行切片", panel);
    run_rip_button_ = makeButton("运行 RIP 摘要", panel);
    regression_button_ = makeButton("运行快速回归", panel);
    compare_button_ = makeButton("对比工艺配置", panel);
    auto* load_button = makeButton("加载输出包", panel);
    auto* open_button = makeButton("打开输出目录", panel);
    status_label_ = new QLabel("就绪", panel);
    status_label_->setWordWrap(true);
    layout->addWidget(build_button_);
    layout->addWidget(run_slicer_button_);
    layout->addWidget(run_rip_button_);
    layout->addWidget(regression_button_);
    layout->addWidget(compare_button_);
    layout->addWidget(load_button);
    layout->addWidget(open_button);
    layout->addWidget(status_label_);

    connect(build_button_, &QPushButton::clicked, this, &MainWindow::buildDebug);
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
    material_process_panel_ = new MaterialProcessPanel(tabs);
    warnings_view_ = new QPlainTextEdit(tabs);
    warnings_view_->setReadOnly(true);
    compare_view_ = new QPlainTextEdit(tabs);
    compare_view_->setReadOnly(true);
    tabs->addTab(material_process_panel_, "材料工艺");
    tabs->addTab(warnings_view_, "警告/失败");
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
    run_slicer_button_->setEnabled(!busy);
    run_rip_button_->setEnabled(!busy);
    regression_button_->setEnabled(!busy);
    compare_button_->setEnabled(!busy);
}
