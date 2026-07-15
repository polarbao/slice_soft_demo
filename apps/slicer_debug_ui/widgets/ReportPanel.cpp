#include "ReportPanel.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

ReportPanel::ReportPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel("报告", this));
    report_selector_ = new QComboBox(this);
    row->addWidget(report_selector_, 1);
    m_loadReportButton = new QPushButton(QStringLiteral("加载诊断报告..."), this);
    m_loadReportButton->setObjectName(QStringLiteral("loadDiagnosticReportButton"));
    m_loadReportButton->setToolTip(
        QStringLiteral("加载独立 JSON 诊断报告；只加入当前界面，不复制或修改源文件。"));
    row->addWidget(m_loadReportButton);
    layout->addLayout(row);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    summary_view_ = new QTextEdit(splitter);
    summary_view_->setReadOnly(true);
    raw_view_ = new QTextEdit(splitter);
    raw_view_->setReadOnly(true);
    raw_view_->setLineWrapMode(QTextEdit::NoWrap);
    splitter->addWidget(summary_view_);
    splitter->addWidget(raw_view_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(report_selector_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ReportPanel::selectReport);
    connect(m_loadReportButton, &QPushButton::clicked, this, &ReportPanel::OnLoadDiagnosticReport);
}

void ReportPanel::loadPackage(const PackageSummary& package) {
    report_selector_->blockSignals(true);
    report_selector_->clear();
    report_paths_.clear();
    if (!package.manifest_path.isEmpty()) {
        addReportPath(package.manifest_path);
    }
    for (const QString& path : package.report_paths) {
        addReportPath(path);
    }
    report_selector_->blockSignals(false);
    selectReport(report_selector_->count() > 0 ? 0 : -1);
}

void ReportPanel::LoadReportPath(const QString& path)
{
    if (path.isEmpty())
    {
        return;
    }
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    int index = report_paths_.indexOf(absolutePath);
    if (index < 0)
    {
        addReportPath(absolutePath);
        index = report_paths_.size() - 1;
    }
    if (report_selector_->currentIndex() == index)
    {
        selectReport(index);
    }
    else
    {
        report_selector_->setCurrentIndex(index);
    }
}

QString ReportPanel::CurrentSummary() const
{
    return summary_view_->toPlainText();
}

int ReportPanel::ReportCount() const
{
    return report_paths_.size();
}

void ReportPanel::selectReport(const int index) {
    if (index < 0 || index >= report_paths_.size()) {
        summary_view_->setPlainText("未选择报告。");
        raw_view_->clear();
        emit warningsChanged({});
        return;
    }
    const JsonReport report = loader_.load(report_paths_.at(index));
    summary_view_->setPlainText(ReportLoader::summarize(report));
    raw_view_->setPlainText(report.error.isEmpty() ? QString::fromUtf8(report.document.toJson(QJsonDocument::Indented))
                                                   : report.raw);
    emit warningsChanged(ReportLoader::collectWarningsAndFailures(report.document.object()));
}

void ReportPanel::OnLoadDiagnosticReport()
{
    const QString initialDirectory = report_paths_.isEmpty()
        ? QString()
        : QFileInfo(report_paths_.constLast()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("加载诊断报告"),
        initialDirectory,
        QStringLiteral("JSON 报告 (*.json);;所有文件 (*.*)"));
    if (!path.isEmpty())
    {
        LoadReportPath(path);
    }
}

void ReportPanel::addReportPath(const QString& path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    if (report_paths_.contains(absolutePath))
    {
        return;
    }
    report_paths_.push_back(absolutePath);
    report_selector_->addItem(QFileInfo(absolutePath).fileName());
}
