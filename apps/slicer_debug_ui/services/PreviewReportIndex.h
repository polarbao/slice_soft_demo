#pragma once

#include <QJsonValue>
#include <QVector>

struct PreviewReportEntry {
    QString path;
    QString channel;
    QString kind;
    int layer_index{-1};
};

class PreviewReportIndex {
public:
    bool load(const QString& package_dir);
    QVector<PreviewReportEntry> entries() const;
    QString errorString() const;

private:
    void addEntry(const QString& package_dir, const QJsonValue& value);

    QVector<PreviewReportEntry> entries_;
    QString error_;
};
