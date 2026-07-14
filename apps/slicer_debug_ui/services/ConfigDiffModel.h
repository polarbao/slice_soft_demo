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

    /**
     * @brief Compare every root field in two configuration documents.
     * @param original Original read-only Profile template document.
     * @param current Generated effective configuration document.
     * @return Sorted leaf-level configuration differences.
     */
    static QVector<ConfigDiffEntry> DiffAll(const QJsonDocument& original, const QJsonDocument& current);
};
