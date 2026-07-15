#include "ReportLoader.h"

#include "OpenVdbUtilityReportInterpreter.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace {

QString scalarToString(const QJsonValue& value) {
    if (value.isBool()) {
        return value.toBool() ? "是" : "否";
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isNull()) {
        return "空";
    }
    return {};
}

void appendIfPresent(QStringList& lines, const QJsonObject& object, const QString& key, const QString& label) {
    if (object.contains(key)) {
        lines.push_back(label + ": " + scalarToString(object.value(key)));
    }
}

void appendChannel(QStringList& lines, const QJsonObject& object, const QString& key, const QString& label) {
    const QJsonObject channel = object.value(key).toObject();
    if (channel.isEmpty()) {
        return;
    }
    QString line = label;
    if (channel.contains("printPixels")) {
        line += " 打印像素=" + scalarToString(channel.value("printPixels"));
    }
    if (channel.contains("coverageRatio")) {
        line += " 覆盖率=" + scalarToString(channel.value("coverageRatio"));
    }
    if (channel.contains("missingUnderbasePixels")) {
        line += " 缺失白墨底层像素=" + scalarToString(channel.value("missingUnderbasePixels"));
    }
    if (channel.contains("activeLayerIndices")) {
        line += " 生效层数=" + QString::number(channel.value("activeLayerIndices").toArray().size());
    }
    lines.push_back(line);
}

QString StringArrayToText(const QJsonValue& value)
{
    const QJsonArray array = value.toArray();
    QStringList values;
    for (const QJsonValue& item : array)
    {
        values.push_back(scalarToString(item));
    }
    return values.isEmpty() ? "无" : values.join(", ");
}

void AppendExperimentalOpenVdbSummary(QStringList& lines, const QJsonObject& object)
{
    appendIfPresent(lines, object, "experimentalOpenvdbShell", "Experimental OpenVDB");
    appendIfPresent(lines, object, "legacyPathExecuted", "legacyPathExecuted");
    appendIfPresent(lines, object, "productionPackageWritten", "productionPackageWritten");
    appendIfPresent(lines, object, "writeProductionRgbwsv", "writeProductionRgbwsv");

    const QJsonObject openvdb = object.value("openvdb").toObject();
    if (!openvdb.isEmpty())
    {
        appendIfPresent(lines, openvdb, "enabled", "OpenVDB 编译启用");
        appendIfPresent(lines, openvdb, "available", "OpenVDB 可用");
        appendIfPresent(lines, openvdb, "version", "OpenVDB 版本");
    }

    const QJsonObject admission = object.value("productionAdmission").toObject();
    if (!admission.isEmpty())
    {
        appendIfPresent(lines, admission, "mode", "准入模式");
        appendIfPresent(lines, admission, "status", "准入状态");
        appendIfPresent(lines, admission, "productionAllowed", "允许生产");
        appendIfPresent(lines, admission, "nonProduction", "仅非生产");
        lines.push_back("阻断码: " + StringArrayToText(admission.value("blockerCodes")));
        lines.push_back("警告码: " + StringArrayToText(admission.value("warningCodes")));
    }

    const QJsonObject legacyPath = object.value("legacyPath").toObject();
    if (!legacyPath.isEmpty())
    {
        appendIfPresent(lines, legacyPath, "executed", "Legacy path executed");
        appendIfPresent(lines, legacyPath, "productionPackageWritten", "Legacy package written");
    }
}

void AppendProductionAdmissionSummary(QStringList& lines, const QJsonObject& admission)
{
    if (admission.isEmpty())
    {
        return;
    }

    appendIfPresent(lines, admission, "mode", "准入模式");
    appendIfPresent(lines, admission, "status", "准入状态");
    appendIfPresent(lines, admission, "productionAllowed", "允许生产");
    appendIfPresent(lines, admission, "nonProduction", "仅非生产");
    lines.push_back("阻断码: " + StringArrayToText(admission.value("blockerCodes")));
    lines.push_back("警告码: " + StringArrayToText(admission.value("warningCodes")));
}

void AppendOpenVdbCandidateSummary(QStringList& lines, const QJsonObject& object)
{
    appendIfPresent(lines, object, "stage", "阶段");
    appendIfPresent(lines, object, "status", "状态");
    appendIfPresent(lines, object, "productionPackageWritten", "已写生产包");
    appendIfPresent(lines, object, "message", "说明");
    AppendProductionAdmissionSummary(lines, object.value("productionAdmission").toObject());
}

void collectNamedArrays(QStringList& lines, const QJsonValue& value, const QString& prefix) {
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const QString key = it.key();
            const QString next_prefix = prefix.isEmpty() ? key : prefix + "." + key;
            if (key == "issues" && it.value().isArray())
            {
                const QJsonArray array = it.value().toArray();
                for (const QJsonValue& item : array)
                {
                    const QJsonObject issue = item.toObject();
                    lines.push_back(
                        next_prefix + "." + issue.value("code").toString() + ": "
                        + issue.value("message").toString());
                }
            }
            else if ((key == "warnings" || key == "errors" || key == "failures" || key == "blockers"
                      || key == "blockerCodes" || key == "warningCodes" || key == "reasonCodes")
                && it.value().isArray()) {
                const QJsonArray array = it.value().toArray();
                for (const QJsonValue& item : array) {
                    lines.push_back(next_prefix + ": " + scalarToString(item));
                }
            } else {
                collectNamedArrays(lines, it.value(), next_prefix);
            }
        }
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            collectNamedArrays(lines, array.at(i), prefix + "[" + QString::number(i) + "]");
        }
    }
}

}  // namespace

JsonReport ReportLoader::load(const QString& path) const {
    JsonReport report;
    report.path = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.error = "failed to open: " + path;
        return report;
    }
    report.raw = QString::fromUtf8(file.readAll());
    QJsonParseError parse_error{};
    report.document = QJsonDocument::fromJson(report.raw.toUtf8(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        report.error = parse_error.errorString();
    }
    return report;
}

QString ReportLoader::summarize(const JsonReport& report) {
    if (!report.error.isEmpty()) {
        return "错误：" + report.error;
    }
    const QJsonObject object = report.document.object();
    QStringList lines;
    lines.push_back(QFileInfo(report.path).fileName());
    appendIfPresent(lines, object, "schema", "协议");
    const OpenVdbUtilityReportInterpretation utilityInterpretation =
        OpenVdbUtilityReportInterpreter::Interpret(object);
    if (utilityInterpretation.recognized)
    {
        lines.push_back(utilityInterpretation.summary);
        return lines.join('\n');
    }
    if (object.value("schema").toString() == "p0.experimental_openvdb_shell_cli_report.1") {
        AppendExperimentalOpenVdbSummary(lines, object);
    }
    if (object.value("schema").toString() == "p0.openvdb_candidate_report.1") {
        AppendOpenVdbCandidateSummary(lines, object);
    }
    if (object.contains("blockerCodes") && object.contains("productionAllowed")) {
        AppendProductionAdmissionSummary(lines, object);
    }
    appendIfPresent(lines, object, "slicingMode", "切片模式");
    appendIfPresent(lines, object, "profileName", "工艺配置名称");
    appendIfPresent(lines, object, "target", "目标");
    appendIfPresent(lines, object, "inputFormat", "输入格式");
    appendChannel(lines, object, "rgb", "RGB");
    appendChannel(lines, object, "white", "W");
    appendChannel(lines, object, "varnish", "V");
    appendChannel(lines, object, "support", "S");
    const QJsonObject validation = object.value("validation").toObject();
    if (!validation.isEmpty()) {
        appendIfPresent(lines, validation, "pass", "校验通过");
    }
    const QString warnings = collectWarningsAndFailures(report.document.object());
    if (!warnings.isEmpty()) {
        lines.push_back("");
        lines.push_back(warnings);
    }
    return lines.join('\n');
}

QString ReportLoader::collectWarningsAndFailures(const QJsonValue& value, const QString& prefix) {
    QStringList lines;
    collectNamedArrays(lines, value, prefix);
    return lines.join('\n');
}
