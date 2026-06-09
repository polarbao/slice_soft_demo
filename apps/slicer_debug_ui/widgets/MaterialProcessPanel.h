#pragma once

#include "../services/PackageLoader.h"
#include "../services/ReportLoader.h"

#include <QTextEdit>
#include <QWidget>

class MaterialProcessPanel final : public QWidget {
    Q_OBJECT

public:
    explicit MaterialProcessPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);

private:
    QString summarizeMaterialProcess(const QJsonObject& object) const;

    ReportLoader loader_;
    QTextEdit* summary_{nullptr};
};

