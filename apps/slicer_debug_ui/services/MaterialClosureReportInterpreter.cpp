#include "MaterialClosureReportInterpreter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

QString DisplayClosureStatus(const QString& value)
{
    if (value == QStringLiteral("pass"))
    {
        return QStringLiteral("通过");
    }
    if (value == QStringLiteral("warning"))
    {
        return QStringLiteral("警告");
    }
    if (value == QStringLiteral("fail"))
    {
        return QStringLiteral("失败");
    }
    if (value == QStringLiteral("not_available"))
    {
        return QStringLiteral("不可用");
    }
    return QStringLiteral("未知");
}

QString DisplayConfidence(const QString& value)
{
    if (value == QStringLiteral("exact"))
    {
        return QStringLiteral("精确语义证据");
    }
    if (value == QStringLiteral("candidate"))
    {
        return QStringLiteral("候选推断");
    }
    if (value == QStringLiteral("unavailable"))
    {
        return QStringLiteral("不可用");
    }
    return QStringLiteral("未知");
}

QString DisplayProductionAcceptance(const QString& value)
{
    if (value == QStringLiteral("passed"))
    {
        return QStringLiteral("通过");
    }
    if (value == QStringLiteral("failed"))
    {
        return QStringLiteral("未通过");
    }
    if (value == QStringLiteral("not_evaluated"))
    {
        return QStringLiteral("未评估");
    }
    return QStringLiteral("未知");
}

QString ResolveGapPreviewPath(const QString& value, const QString& packageDir)
{
    if (value.trimmed().isEmpty())
    {
        return {};
    }

    const QFileInfo info(value);
    const QString resolved = info.isAbsolute()
        ? info.absoluteFilePath()
        : QDir(packageDir).filePath(value);
    return QFileInfo::exists(resolved) ? QFileInfo(resolved).absoluteFilePath() : QString{};
}

QStringList ReadStringArray(const QJsonArray& values)
{
    QStringList result;
    for (const QJsonValue& value : values)
    {
        const QString text = value.toString();
        if (!text.isEmpty())
        {
            result.push_back(text);
        }
    }
    return result;
}

bool IsSupportedValue(const QString& value, const QStringList& supported)
{
    return supported.contains(value);
}

}  // namespace

MaterialClosureDiagnosticsSummary MaterialClosureReportInterpreter::Interpret(
    const QString& reportPath,
    const QString& packageDir)
{
    MaterialClosureDiagnosticsSummary summary;
    summary.reportpath = reportPath;
    if (reportPath.isEmpty() || !QFileInfo::exists(reportPath))
    {
        summary.error = QStringLiteral("当前输出包未生成 reports/material_closure_report.json。");
        return summary;
    }

    summary.reportavailable = true;
    QFile file(reportPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        summary.error = QStringLiteral("无法读取材料闭环报告：") + reportPath;
        return summary;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        summary.error = QStringLiteral("材料闭环报告 JSON 无效：") + parseError.errorString();
        return summary;
    }

    const QJsonObject root = document.object();
    const QString schema = root.value(QStringLiteral("schema")).toString();
    if (schema != QStringLiteral("p0.material_closure.1"))
    {
        summary.error = QStringLiteral("不支持的材料闭环报告协议：")
            + (schema.isEmpty() ? QStringLiteral("<empty>") : schema);
        return summary;
    }

    summary.closurestatus = root.value(QStringLiteral("closureStatus")).toString();
    summary.confidence = root.value(QStringLiteral("confidence")).toString();
    summary.productionacceptance =
        root.value(QStringLiteral("productionAcceptance")).toString();
    summary.candidateonly =
        summary.confidence == QStringLiteral("candidate")
        || root.value(QStringLiteral("source")).toString()
            == QStringLiteral("rgbwsv_tiff_inferred");
    if (!IsSupportedValue(
            summary.closurestatus,
            QStringList{
                QStringLiteral("pass"),
                QStringLiteral("warning"),
                QStringLiteral("fail"),
                QStringLiteral("not_available")})
        || !IsSupportedValue(
            summary.confidence,
            QStringList{
                QStringLiteral("exact"),
                QStringLiteral("candidate"),
                QStringLiteral("unavailable")})
        || !IsSupportedValue(
            summary.productionacceptance,
            QStringList{
                QStringLiteral("passed"),
                QStringLiteral("failed"),
                QStringLiteral("not_evaluated")}))
    {
        summary.error = QStringLiteral("材料闭环报告包含不受支持的状态枚举。");
        return summary;
    }
    if (summary.candidateonly
        && (summary.closurestatus == QStringLiteral("pass")
            || summary.productionacceptance != QStringLiteral("not_evaluated")))
    {
        summary.error =
            QStringLiteral("候选材料闭环报告违反非生产约束，已拒绝显示为有效结论。");
        return summary;
    }
    summary.schemavalid = true;

    const QJsonObject repair = root.value(QStringLiteral("repair")).toObject();
    summary.repairenabled = repair.value(QStringLiteral("enabled")).toBool(false);
    summary.repairattempted = repair.value(QStringLiteral("attempted")).toBool(false);
    summary.repairedpixels = repair.value(QStringLiteral("repairedPixels")).toInt(0);

    const QJsonObject totals = root.value(QStringLiteral("totals")).toObject();
    summary.remaininggappixels =
        totals.value(QStringLiteral("remainingGapPixels")).toInt(
            totals.value(QStringLiteral("totalGapPixels")).toInt(0));
    summary.colorfillgappixels =
        totals.value(QStringLiteral("colorFillGapPixels")).toInt(0);
    summary.modelsupportgappixels =
        totals.value(QStringLiteral("modelSupportGapPixels")).toInt(0);
    summary.colorsupportgappixels =
        totals.value(QStringLiteral("colorSupportGapPixels")).toInt(0);
    summary.internalvoidgappixels =
        totals.value(QStringLiteral("internalVoidGapPixels")).toInt(0);
    summary.varnishsupportgappixels =
        totals.value(QStringLiteral("varnishSupportGapPixels")).toInt(0);
    summary.externalbackgroundprotectedpixels =
        totals.value(QStringLiteral("externalBackgroundProtectedPixels")).toInt(0);

    QHash<int, QString> gapPreviewPaths;
    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    for (const QJsonValue& value : layers)
    {
        const QJsonObject layer = value.toObject();
        const int layerIndex = layer.value(QStringLiteral("layerIndex")).toInt(-1);
        if (layerIndex < 0)
        {
            continue;
        }
        const QString path = ResolveGapPreviewPath(
            layer.value(QStringLiteral("gapPreviewPath")).toString(),
            packageDir);
        if (!path.isEmpty())
        {
            gapPreviewPaths.insert(layerIndex, path);
        }
    }

    const QJsonArray worstLayers = root.value(QStringLiteral("worstLayers")).toArray();
    for (const QJsonValue& value : worstLayers)
    {
        const QJsonObject object = value.toObject();
        MaterialClosureWorstLayerUi layer;
        layer.layerindex = object.value(QStringLiteral("layerIndex")).toInt(-1);
        if (layer.layerindex < 0)
        {
            continue;
        }
        layer.zmm = object.value(QStringLiteral("zMm")).toDouble(0.0);
        layer.gappixels = object.value(QStringLiteral("gapPixels")).toInt(0);
        layer.types = ReadStringArray(object.value(QStringLiteral("types")).toArray());
        layer.gappreviewpath = gapPreviewPaths.value(layer.layerindex);
        summary.worstlayers.push_back(layer);
    }

    const QJsonArray diagnostics = root.value(QStringLiteral("diagnostics")).toArray();
    for (const QJsonValue& value : diagnostics)
    {
        const QString code = value.toObject().value(QStringLiteral("code")).toString();
        if (!code.isEmpty())
        {
            summary.diagnosticcodes.push_back(code);
        }
    }
    summary.diagnosticcodes.removeDuplicates();
    return summary;
}

QString MaterialClosureReportInterpreter::BuildSummaryText(
    const MaterialClosureDiagnosticsSummary& summary)
{
    if (!summary.reportavailable || !summary.schemavalid)
    {
        return summary.error.isEmpty()
            ? QStringLiteral("材料闭环报告不可用。")
            : summary.error;
    }

    QStringList lines;
    lines.push_back(
        QStringLiteral("闭环状态：%1 (%2)")
            .arg(DisplayClosureStatus(summary.closurestatus), summary.closurestatus));
    lines.push_back(
        QStringLiteral("证据置信度：%1 (%2)")
            .arg(DisplayConfidence(summary.confidence), summary.confidence));
    lines.push_back(
        QStringLiteral("生产验收：%1 (%2)")
            .arg(
                DisplayProductionAcceptance(summary.productionacceptance),
                summary.productionacceptance));
    if (summary.candidateonly)
    {
        lines.push_back(
            QStringLiteral("安全提示：候选诊断，不能作为生产通过依据。"));
    }
    lines.push_back(
        QStringLiteral("修复：启用=%1，已尝试=%2，已修复=%3，剩余 Gap=%4")
            .arg(summary.repairenabled ? QStringLiteral("是") : QStringLiteral("否"))
            .arg(summary.repairattempted ? QStringLiteral("是") : QStringLiteral("否"))
            .arg(summary.repairedpixels)
            .arg(summary.remaininggappixels));
    lines.push_back(
        QStringLiteral(
            "Gap 分类：颜色/填充=%1，模型/支撑=%2，颜色/支撑=%3，内部镂空=%4，光油/支撑=%5")
            .arg(summary.colorfillgappixels)
            .arg(summary.modelsupportgappixels)
            .arg(summary.colorsupportgappixels)
            .arg(summary.internalvoidgappixels)
            .arg(summary.varnishsupportgappixels));
    lines.push_back(
        QStringLiteral("外部背景保护像素：%1")
            .arg(summary.externalbackgroundprotectedpixels));
    lines.push_back(
        QStringLiteral("Worst Layers：%1")
            .arg(summary.worstlayers.size()));
    lines.push_back(
        QStringLiteral("诊断码：%1")
            .arg(
                summary.diagnosticcodes.isEmpty()
                    ? QStringLiteral("无")
                    : summary.diagnosticcodes.join(QStringLiteral(", "))));
    return lines.join(QLatin1Char('\n'));
}

QString MaterialClosureReportInterpreter::DisplayGapType(const QString& type)
{
    if (type == QStringLiteral("COLOR_FILL_GAP"))
    {
        return QStringLiteral("颜色/填充 (%1)").arg(type);
    }
    if (type == QStringLiteral("MODEL_SUPPORT_GAP"))
    {
        return QStringLiteral("模型/支撑 (%1)").arg(type);
    }
    if (type == QStringLiteral("COLOR_SUPPORT_GAP"))
    {
        return QStringLiteral("颜色/支撑 (%1)").arg(type);
    }
    if (type == QStringLiteral("INTERNAL_VOID_GAP"))
    {
        return QStringLiteral("内部镂空 (%1)").arg(type);
    }
    if (type == QStringLiteral("VARNISH_SUPPORT_GAP"))
    {
        return QStringLiteral("光油/支撑 (%1)").arg(type);
    }
    return type;
}
