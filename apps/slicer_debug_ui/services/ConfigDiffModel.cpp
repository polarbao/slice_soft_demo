#include "ConfigDiffModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace {

QString valueText(const QJsonValue& value) {
    if (value.isUndefined()) {
        return "<missing>";
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isNull()) {
        return "null";
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

void appendDiffs(QVector<ConfigDiffEntry>& entries, const QString& path, const QJsonValue& old_value, const QJsonValue& new_value) {
    if (old_value == new_value) {
        return;
    }
    if (old_value.isObject() || new_value.isObject()) {
        const QJsonObject old_object = old_value.toObject();
        const QJsonObject new_object = new_value.toObject();
        QSet<QString> keys;
        for (auto it = old_object.begin(); it != old_object.end(); ++it) {
            keys.insert(it.key());
        }
        for (auto it = new_object.begin(); it != new_object.end(); ++it) {
            keys.insert(it.key());
        }
        QStringList sorted = keys.values();
        sorted.sort();
        for (const QString& key : sorted) {
            appendDiffs(entries,
                        path.isEmpty() ? key : path + "." + key,
                        old_object.value(key),
                        new_object.value(key));
        }
        return;
    }
    entries.push_back(ConfigDiffEntry{path, valueText(old_value), valueText(new_value)});
}

}  // namespace

QVector<ConfigDiffEntry> ConfigDiffModel::diff(const QJsonDocument& original, const QJsonDocument& current) {
    QVector<ConfigDiffEntry> entries;
    const QStringList roots{"materialProcessProfile", "materialPolicy", "materialRoleMapping", "support", "preview"};
    const QJsonObject old_root = original.object();
    const QJsonObject new_root = current.object();
    for (const QString& root : roots) {
        appendDiffs(entries, root, old_root.value(root), new_root.value(root));
    }
    return entries;
}
