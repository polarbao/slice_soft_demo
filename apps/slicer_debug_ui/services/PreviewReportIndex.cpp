#include "PreviewReportIndex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace
{

QString ClassifyChannelFromPath(const QString& path)
{
    const QString base = QFileInfo(path).completeBaseName().toLower();
    if (base.contains("texture_rgb"))
    {
        return "texture_rgb";
    }
    if (base.contains("model_rgb") || base.contains("rgb"))
    {
        return "rgb";
    }
    if (base.contains("white") || base.contains("_w") || base.endsWith("w"))
    {
        return "white";
    }
    if (base.contains("varnish") || base.contains("_v") || base.endsWith("v"))
    {
        return "varnish";
    }
    if (base.contains("support") || base.contains("_s") || base.endsWith("s"))
    {
        return "support";
    }
    return "preview";
}

int ParseLayerIndexFromPath(const QString& path)
{
    const QString base = QFileInfo(path).completeBaseName();
    const QRegularExpression expression("(?:layer|z|_)(\\d+)");
    const QRegularExpressionMatch match = expression.match(base);
    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    const QRegularExpression digits("(\\d+)");
    const QRegularExpressionMatch fallback = digits.match(base);
    return fallback.hasMatch() ? fallback.captured(1).toInt() : -1;
}

}  // namespace

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
    QSet<QString> seen;
    QVector<PreviewReportEntry> uniqueEntries;
    for (const PreviewReportEntry& entry : entries_) {
        const QString dedupeKey =
            QString("%1|%2|%3").arg(entry.layer_index).arg(entry.channel, entry.path);
        if (seen.contains(dedupeKey)) {
            continue;
        }
        seen.insert(dedupeKey);
        uniqueEntries.push_back(entry);
    }
    entries_ = uniqueEntries;
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
        entry.channel = ClassifyChannelFromPath(entry.path);
        entry.layer_index = ParseLayerIndexFromPath(entry.path);
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QString path = object.value("path").toString(object.value("file").toString());
        if (path.isEmpty()) {
            return;
        }
        entry.path = QDir(package_dir).filePath(path);
        entry.channel = object.value("channel").toString(ClassifyChannelFromPath(entry.path));
        entry.kind = object.value("kind").toString();
        entry.layer_index = object.value("layerIndex").toInt(ParseLayerIndexFromPath(entry.path));
    }
    if (!entry.path.isEmpty()) {
        entries_.push_back(entry);
    }
}
