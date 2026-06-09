#pragma once

#include <QJsonDocument>
#include <QString>

struct JsonReport {
    QString path;
    QString raw;
    QJsonDocument document;
    QString error;
};

class ReportLoader {
public:
    JsonReport load(const QString& path) const;
    static QString summarize(const JsonReport& report);
    static QString collectWarningsAndFailures(const QJsonValue& value, const QString& prefix = QString());
};

