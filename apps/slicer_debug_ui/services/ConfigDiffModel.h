#pragma once

#include <QJsonDocument>
#include <QVector>

struct ConfigDiffEntry {
    QString path;
    QString old_value;
    QString new_value;
};

class ConfigDiffModel {
public:
    static QVector<ConfigDiffEntry> diff(const QJsonDocument& original, const QJsonDocument& current);
};
