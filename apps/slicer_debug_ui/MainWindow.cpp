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
    setWindowTitle("SliceSoft Debug UI");
    resize(1440, 900);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    auto* main_splitter = new QSplitter(Qt::Horizontal, central);

    QWidget* left = createProjectPanel();
    auto* center_tabs = new QTabWidget(main_splitter);
    preview_panel_ = new PreviewPanel(center_tabs);
    report_panel_ = new ReportPanel(center_tabs);
    center_tabs->addTab(preview_panel_, "Preview");
    center_tabs->addTab(report_panel_, "Reports");

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

    loadPackage(package_edit_->text());
}

void MainWindow::browseConfig() {
    const QString path = QFileDialog::getOpenFileName(this, "Select config", paths_.repo_root, "JSON (*.json)");
    if (!path.isEmpty()) {
        config_edit_->setText(path);
    }
}

void MainWindow::browsePackage() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select package", paths_.repo_root);
    if (!path.isEmpty()) {
        package_edit_->setText(path);
        loadPackage(path);
    }
}

void MainWindow::browseProfileA() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select Package A", paths_.repo_root);
    if (!path.isEmpty()) {
        profile_a_edit_->setText(path);
    }
}

void MainWindow::browseProfileB() {
    const QString path = QFileDialog::getExistingDirectory(this, "Select Package B", paths_.repo_root);
    if (!path.isEmpty()) {
        profile_b_edit_->setText(path);
    }
}

void MainWindow::buildDebug() {
    runCommand("build", "cmake", QStringList{"--build", "build", "--config", "Debug"});
}

void MainWindow::runSlicer() {
    const QString config = absoluteFromRepo(config_edit_->text());
    pending_package_ = inferPackageFromConfig(config);
    runCommand("slicer", paths_.slicer_cli, QStringList{"--config", config});
}

void MainWindow::runRipSummary() {
    const QString package = absoluteFromRepo(package_edit_->text());
    runCommand("rip", paths_.rip_reader, QStringList{"--package", package, "--summary"});
}

void MainWindow::runQuickRegression() {
    runCommand("regression", paths_.powershell,
               QStringList{"-ExecutionPolicy", "Bypass", "-File", "scripts/run_regression.ps1", "-Mode", "quick"});
}

void MainWindow::compareProfiles() {
    const QString package_a = absoluteFromRepo(profile_a_edit_->text());
    const QString package_b = absoluteFromRepo(profile_b_edit_->text());
    compare_output_ = QDir(paths_.repo_root).filePath("output/MaterialProfileCompare_ui.json");
    runCommand("compare", paths_.powershell,
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
    status_label_->setText("Running: " + current_action_);
    log_panel_->appendCommand(command);
}

void MainWindow::handleProcessFinished(const int exit_code, const qint64 elapsed_ms) {
    log_panel_->appendResult(exit_code, elapsed_ms);
    setBusy(false);
    status_label_->setText(exit_code == 0 ? "PASS: " + current_action_ : "FAIL: " + current_action_);
    if (exit_code != 0) {
        return;
    }
    if (current_action_ == "slicer" && !pending_package_.isEmpty()) {
        package_edit_->setText(pending_package_);
        loadPackage(pending_package_);
    } else if (current_action_ == "compare") {
        loadCompareResult(compare_output_);
    }
}

void MainWindow::handleProcessFailed(const QString& message) {
    log_panel_->appendError(message);
    setBusy(false);
    status_label_->setText("Process error: " + message);
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
    addPathRow(layout, "Config", config_edit_, config_browse);
    addPathRow(layout, "Package", package_edit_, package_browse);
    addPathRow(layout, "Profile A", profile_a_edit_, profile_a_browse);
    addPathRow(layout, "Profile B", profile_b_edit_, profile_b_browse);

    repo_label_ = new QLabel("Repo: " + paths_.repo_root, panel);
    slicer_label_ = new QLabel("slicer_cli: " + paths_.slicer_cli, panel);
    rip_label_ = new QLabel("rip_reader: " + paths_.rip_reader, panel);
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
    build_button_ = makeButton("Build Debug", panel);
    run_slicer_button_ = makeButton("Run Slicer", panel);
    run_rip_button_ = makeButton("Run RIP Summary", panel);
    regression_button_ = makeButton("Run Quick Regression", panel);
    compare_button_ = makeButton("Compare Profiles", panel);
    auto* load_button = makeButton("Load Package", panel);
    auto* open_button = makeButton("Open Output Folder", panel);
    status_label_ = new QLabel("Ready", panel);
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
    tabs->addTab(material_process_panel_, "Material");
    tabs->addTab(warnings_view_, "Warnings");
    tabs->addTab(compare_view_, "Compare");
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
    preview_panel_->loadPackage(package);
    material_process_panel_->loadPackage(package);
    warnings_view_->setPlainText(package.warnings.join('\n'));
}

void MainWindow::loadCompareResult(const QString& path) {
    const JsonReport report = report_loader_.load(path);
    if (!report.error.isEmpty()) {
        compare_view_->setPlainText("Failed to read compare output: " + report.error);
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

