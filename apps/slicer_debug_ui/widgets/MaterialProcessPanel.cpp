#include "MaterialProcessPanel.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>

namespace {

QString valueString(const QJsonValue& value) {
    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isArray()) {
        QStringList values;
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            values.push_back(valueString(item));
        }
        return values.join(", ");
    }
    return {};
}

QString channelLine(const QJsonObject& root, const QString& key, const QString& label) {
    const QJsonObject channel = root.value(key).toObject();
    if (channel.isEmpty()) {
        return label + ": n/a";
    }
    QStringList parts;
    parts.push_back(label);
    if (channel.contains("printPixels")) {
        parts.push_back("printPixels=" + valueString(channel.value("printPixels")));
    }
    if (channel.contains("coverageRatio")) {
        parts.push_back("coverageRatio=" + valueString(channel.value("coverageRatio")));
    }
    if (channel.contains("missingUnderbasePixels")) {
        parts.push_back("missingUnderbasePixels=" + valueString(channel.value("missingUnderbasePixels")));
    }
    if (channel.contains("activeLayerIndices")) {
        parts.push_back("activeLayers=[" + valueString(channel.value("activeLayerIndices")) + "]");
    }
    return parts.join("  ");
}

}  // namespace

MaterialProcessPanel::MaterialProcessPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    summary_ = new QTextEdit(this);
    summary_->setReadOnly(true);
    layout->addWidget(summary_);
}

void MaterialProcessPanel::loadPackage(const PackageSummary& package) {
    QString path;
    for (const QString& candidate : package.report_paths) {
        if (QFileInfo(candidate).fileName() == "material_process_report.json") {
            path = candidate;
            break;
        }
    }
    if (path.isEmpty()) {
        summary_->setPlainText("material_process_report.json is not available.");
        return;
    }
    const JsonReport report = loader_.load(path);
    if (!report.error.isEmpty()) {
        summary_->setPlainText("Failed to read material process report: " + report.error);
        return;
    }
    summary_->setPlainText(summarizeMaterialProcess(report.document.object()));
}

QString MaterialProcessPanel::summarizeMaterialProcess(const QJsonObject& object) const {
    QStringList lines;
    lines.push_back("Material Process");
    lines.push_back("profileName: " + valueString(object.value("profileName")));
    lines.push_back("target: " + valueString(object.value("target")));
    lines.push_back("inputFormat: " + valueString(object.value("inputFormat")));
    lines.push_back(channelLine(object, "rgb", "RGB"));
    lines.push_back(channelLine(object, "white", "W"));
    lines.push_back(channelLine(object, "varnish", "V"));
    lines.push_back(channelLine(object, "support", "S"));
    lines.push_back("unexpectedOverlapPixels: " + valueString(object.value("unexpectedOverlapPixels")));
    const QJsonObject validation = object.value("validation").toObject();
    lines.push_back("validation.pass: " + valueString(validation.value("pass")));
    lines.push_back("validation.failures: " + valueString(validation.value("failures")));
    lines.push_back("warnings: " + valueString(object.value("warnings")));
    return lines.join('\n');
}

