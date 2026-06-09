#include "PreviewReportIndex.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool PreviewReportIndex::load(const QString& package_dir) {
    entries_.clear();
    error_.clear();
    QFile file(QDir(package_dir).filePath("reports/preview_report.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error_ = "preview_report.json 不存在。";
        return false;
    }
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        error_ = "preview_report.json 解析失败：" + parse_error.errorString();
        return false;
    }
    const QJsonObject object = document.object();
    const QString schema = object.value("schema").toString();
    if (!schema.isEmpty() && schema != "p0.preview_report.1") {
        error_ = "未知 preview_report schema：" + schema;
    }
    for (const QString& key : QStringList{"files", "generated", "previewFiles"}) {
        const QJsonArray array = object.value(key).toArray();
        for (const QJsonValue& value : array) {
            addEntry(package_dir, value);
        }
    }
    if (entries_.isEmpty() && error_.isEmpty()) {
        error_ = "preview_report.json 未列出 preview 文件。";
    }
    return !entries_.isEmpty();
}

QVector<PreviewReportEntry> PreviewReportIndex::entries() const {
    return entries_;
}

QString PreviewReportIndex::errorString() const {
    return error_;
}

void PreviewReportIndex::addEntry(const QString& package_dir, const QJsonValue& value) {
    PreviewReportEntry entry;
    if (value.isString()) {
        entry.path = QDir(package_dir).filePath(value.toString());
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QString path = object.value("path").toString(object.value("file").toString());
        if (path.isEmpty()) {
            return;
        }
        entry.path = QDir(package_dir).filePath(path);
        entry.channel = object.value("channel").toString();
        entry.kind = object.value("kind").toString();
        entry.layer_index = object.value("layerIndex").toInt(-1);
    }
    if (!entry.path.isEmpty()) {
        entries_.push_back(entry);
    }
}
