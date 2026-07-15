#pragma once

#include "../services/PackageLoader.h"
#include "../services/ReportLoader.h"

#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

class ReportPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ReportPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);

    /**
     * @brief Load an independent JSON report into the current report session.
     * @param path Absolute or relative JSON report path.
     */
    void LoadReportPath(const QString& path);

    /**
     * @brief Return the currently displayed report summary for smoke tests.
     * @return Current summary text.
     */
    QString CurrentSummary() const;

    /**
     * @brief Return the number of reports currently registered by the panel.
     * @return Report count.
     */
    int ReportCount() const;

signals:
    void warningsChanged(const QString& warnings);

private slots:
    void selectReport(int index);
    void OnLoadDiagnosticReport();

private:
    void addReportPath(const QString& path);

    ReportLoader loader_;
    QStringList report_paths_;
    QComboBox* report_selector_{nullptr};
    QPushButton* m_loadReportButton{nullptr};
    QTextEdit* summary_view_{nullptr};
    QTextEdit* raw_view_{nullptr};
};
