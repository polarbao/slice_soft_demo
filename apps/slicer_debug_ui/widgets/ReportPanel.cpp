#include "ReportPanel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

ReportPanel::ReportPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel("Report", this));
    report_selector_ = new QComboBox(this);
    row->addWidget(report_selector_, 1);
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

void ReportPanel::selectReport(const int index) {
    if (index < 0 || index >= report_paths_.size()) {
        summary_view_->setPlainText("No report selected.");
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

void ReportPanel::addReportPath(const QString& path) {
    report_paths_.push_back(path);
    report_selector_->addItem(QFileInfo(path).fileName());
}
