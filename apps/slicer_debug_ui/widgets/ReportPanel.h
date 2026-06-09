#pragma once

#include "../services/PackageLoader.h"
#include "../services/ReportLoader.h"

#include <QComboBox>
#include <QTextEdit>
#include <QWidget>

class ReportPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ReportPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);

signals:
    void warningsChanged(const QString& warnings);

private slots:
    void selectReport(int index);

private:
    void addReportPath(const QString& path);

    ReportLoader loader_;
    QStringList report_paths_;
    QComboBox* report_selector_{nullptr};
    QTextEdit* summary_view_{nullptr};
    QTextEdit* raw_view_{nullptr};
};

