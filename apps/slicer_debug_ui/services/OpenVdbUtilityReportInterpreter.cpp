#include "OpenVdbUtilityReportInterpreter.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

namespace
{

const QString kSchema = QStringLiteral("slicesoft.openvdb_sdf_utility.12b_r2.1");
const QString kSchemaFamily = QStringLiteral("slicesoft.openvdb_sdf_utility");

void AddError(QStringList& errors, const QString& path, const QString& message)
{
    errors.push_back(path + QStringLiteral(": ") + message);
}

bool ReadRequiredBool(
    const QJsonObject& object,
    const QString& key,
    const QString& path,
    bool& value,
    QStringList& errors)
{
    const QJsonValue field = object.value(key);
    if (!field.isBool())
    {
        AddError(errors, path + QStringLiteral(".") + key, QStringLiteral("必须为布尔值"));
        return false;
    }
    value = field.toBool();
    return true;
}

QJsonObject ReadRequiredObject(
    const QJsonObject& object,
    const QString& key,
    const QString& path,
    QStringList& errors)
{
    const QJsonValue field = object.value(key);
    if (!field.isObject())
    {
        AddError(errors, path + QStringLiteral(".") + key, QStringLiteral("必须为对象"));
        return {};
    }
    return field.toObject();
}

QString ReadRequiredString(
    const QJsonObject& object,
    const QString& key,
    const QString& path,
    QStringList& errors)
{
    const QJsonValue field = object.value(key);
    if (!field.isString())
    {
        AddError(errors, path + QStringLiteral(".") + key, QStringLiteral("必须为字符串"));
        return {};
    }
    return field.toString();
}

void ValidateStringArray(
    const QJsonObject& object,
    const QString& key,
    const QString& path,
    QStringList& errors)
{
    const QJsonValue field = object.value(key);
    if (!field.isArray())
    {
        AddError(errors, path + QStringLiteral(".") + key, QStringLiteral("必须为字符串数组"));
        return;
    }
    const QJsonArray array = field.toArray();
    for (int index = 0; index < array.size(); ++index)
    {
        if (!array.at(index).isString())
        {
            AddError(
                errors,
                path + QStringLiteral(".") + key + QStringLiteral("[") + QString::number(index)
                    + QStringLiteral("]"),
                QStringLiteral("必须为字符串"));
        }
    }
}

QString ArrayText(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array)
    {
        if (value.isString())
        {
            values.push_back(value.toString());
        }
    }
    return values.isEmpty() ? QStringLiteral("无") : values.join(QStringLiteral(", "));
}

QString MapRole(const QString& role)
{
    if (role == QStringLiteral("sdf_utility_candidate"))
    {
        return QStringLiteral("OpenVDB SDF 辅助工具候选");
    }
    if (role == QStringLiteral("diagnostic_only"))
    {
        return QStringLiteral("仅诊断");
    }
    if (role == QStringLiteral("unavailable"))
    {
        return QStringLiteral("当前不可用");
    }
    if (role == QStringLiteral("rejected"))
    {
        return QStringLiteral("已拒绝");
    }
    return role;
}

QString MapStatus(const QString& status)
{
    if (status == QStringLiteral("pass"))
    {
        return QStringLiteral("Utility 验证通过（非生产）");
    }
    if (status == QStringLiteral("fail"))
    {
        return QStringLiteral("Utility 执行失败");
    }
    if (status == QStringLiteral("unavailable"))
    {
        return QStringLiteral("当前构建不可用");
    }
    if (status == QStringLiteral("blocked"))
    {
        return QStringLiteral("输入或准入阻断");
    }
    if (status == QStringLiteral("skipped"))
    {
        return QStringLiteral("已按任务边界跳过");
    }
    if (status == QStringLiteral("not_evaluated"))
    {
        return QStringLiteral("尚未评估");
    }
    return status;
}

QString MapPromoteDecision(const QString& decision)
{
    if (decision == QStringLiteral("promote"))
    {
        return QStringLiteral("建议推进为辅助 Utility");
    }
    if (decision == QStringLiteral("keep_experimental"))
    {
        return QStringLiteral("保持实验能力");
    }
    if (decision == QStringLiteral("reject"))
    {
        return QStringLiteral("不建议继续推进");
    }
    if (decision == QStringLiteral("not_evaluated"))
    {
        return QStringLiteral("尚未评估");
    }
    return decision;
}

QString UtilityLabel(const QString& key)
{
    if (key == QStringLiteral("outerVarnishShell"))
    {
        return QStringLiteral("外侧光油壳层");
    }
    if (key == QStringLiteral("clearanceDistance"))
    {
        return QStringLiteral("间隙距离");
    }
    if (key == QStringLiteral("topologyDiagnostic"))
    {
        return QStringLiteral("拓扑诊断");
    }
    if (key == QStringLiteral("materialClosureAssist"))
    {
        return QStringLiteral("材料闭环辅助");
    }
    return key;
}

void ValidateUtility(
    const QString& key,
    const QJsonObject& utilities,
    const bool openVdbAvailable,
    QStringList& errors)
{
    const QString path = QStringLiteral("utilities.") + key;
    const QJsonValue value = utilities.value(key);
    if (!value.isObject())
    {
        AddError(errors, path, QStringLiteral("缺少固定 utility 对象"));
        return;
    }

    const QJsonObject utility = value.toObject();
    bool available = false;
    bool executed = false;
    const bool hasAvailable = ReadRequiredBool(utility, QStringLiteral("available"), path, available, errors);
    const bool hasExecuted = ReadRequiredBool(utility, QStringLiteral("executed"), path, executed, errors);
    const QString status = ReadRequiredString(utility, QStringLiteral("status"), path, errors);
    ReadRequiredString(utility, QStringLiteral("source"), path, errors);
    const QString promoteDecision =
        ReadRequiredString(utility, QStringLiteral("promoteDecision"), path, errors);
    ReadRequiredObject(utility, QStringLiteral("metrics"), path, errors);
    ReadRequiredObject(utility, QStringLiteral("timingsMs"), path, errors);
    ValidateStringArray(utility, QStringLiteral("blockers"), path, errors);
    ValidateStringArray(utility, QStringLiteral("warnings"), path, errors);
    ValidateStringArray(utility, QStringLiteral("notes"), path, errors);

    const QSet<QString> statuses{
        QStringLiteral("pass"),
        QStringLiteral("fail"),
        QStringLiteral("unavailable"),
        QStringLiteral("blocked"),
        QStringLiteral("skipped"),
        QStringLiteral("not_evaluated")};
    if (!status.isEmpty() && !statuses.contains(status))
    {
        AddError(errors, path + QStringLiteral(".status"), QStringLiteral("包含未知状态 ") + status);
    }

    const QSet<QString> decisions{
        QStringLiteral("promote"),
        QStringLiteral("keep_experimental"),
        QStringLiteral("reject"),
        QStringLiteral("not_evaluated")};
    if (!promoteDecision.isEmpty() && !decisions.contains(promoteDecision))
    {
        AddError(
            errors,
            path + QStringLiteral(".promoteDecision"),
            QStringLiteral("包含未知推进决策 ") + promoteDecision);
    }
    if (hasAvailable && hasExecuted && !available && executed)
    {
        AddError(errors, path, QStringLiteral("available=false 时 executed 不能为 true"));
    }
    if (!openVdbAvailable && status == QStringLiteral("pass"))
    {
        AddError(errors, path + QStringLiteral(".status"), QStringLiteral("OpenVDB 不可用时不能为 pass"));
    }
}

void AppendUtilitySummary(QStringList& lines, const QString& key, const QJsonObject& utilities)
{
    const QJsonObject utility = utilities.value(key).toObject();
    lines.push_back(QString());
    lines.push_back(QStringLiteral("[") + UtilityLabel(key) + QStringLiteral("]"));
    lines.push_back(QStringLiteral("状态: ") + MapStatus(utility.value(QStringLiteral("status")).toString()));
    lines.push_back(
        QStringLiteral("可用/已执行: ")
        + (utility.value(QStringLiteral("available")).toBool() ? QStringLiteral("是") : QStringLiteral("否"))
        + QStringLiteral(" / ")
        + (utility.value(QStringLiteral("executed")).toBool() ? QStringLiteral("是") : QStringLiteral("否")));
    lines.push_back(
        QStringLiteral("推进建议: ")
        + MapPromoteDecision(utility.value(QStringLiteral("promoteDecision")).toString()));
    lines.push_back(
        QStringLiteral("阻断项: ") + ArrayText(utility.value(QStringLiteral("blockers")).toArray()));
    const QJsonArray warnings = utility.value(QStringLiteral("warnings")).toArray();
    if (!warnings.isEmpty())
    {
        lines.push_back(QStringLiteral("警告: ") + ArrayText(warnings));
    }
}

QString BuildValidSummary(const QJsonObject& object)
{
    const QJsonObject build = object.value(QStringLiteral("build")).toObject();
    const QJsonObject decision = object.value(QStringLiteral("decision")).toObject();
    const QJsonObject outputPolicy = object.value(QStringLiteral("outputPolicy")).toObject();
    const QJsonObject utilities = object.value(QStringLiteral("utilities")).toObject();
    const QJsonObject validation = object.value(QStringLiteral("validation")).toObject();
    const QJsonObject legacyGuard = validation.value(QStringLiteral("legacyGuard")).toObject();

    QStringList lines;
    lines.push_back(QStringLiteral("报告状态: 有效的 OpenVDB Utility 诊断报告"));
    lines.push_back(
        QStringLiteral("报告角色: ") + MapRole(decision.value(QStringLiteral("openVdbRole")).toString()));
    lines.push_back(
        QStringLiteral("OpenVDB 编译/运行可用: ")
        + (build.value(QStringLiteral("useOpenVdb")).toBool() ? QStringLiteral("是") : QStringLiteral("否"))
        + QStringLiteral(" / ")
        + (build.value(QStringLiteral("openVdbAvailable")).toBool() ? QStringLiteral("是")
                                                                     : QStringLiteral("否")));
    if (build.value(QStringLiteral("openVdbVersion")).isString())
    {
        lines.push_back(
            QStringLiteral("OpenVDB 版本: ") + build.value(QStringLiteral("openVdbVersion")).toString());
    }
    if (build.value(QStringLiteral("openVdbUnavailableReason")).isString())
    {
        lines.push_back(
            QStringLiteral("不可用原因: ")
            + build.value(QStringLiteral("openVdbUnavailableReason")).toString());
    }
    lines.push_back(QStringLiteral("生产替代许可: 否 (productionReplacementAllowed=false)"));
    lines.push_back(QStringLiteral("生产结论: 仅 Utility 诊断，不形成生产验收结论"));
    lines.push_back(QStringLiteral("默认生产路径: Legacy"));
    lines.push_back(
        QStringLiteral("Legacy 输出保护: ")
        + (!outputPolicy.value(QStringLiteral("modifiesLegacyOutput")).toBool()
               ? QStringLiteral("未修改")
               : QStringLiteral("无效状态")));
    const bool guardRan = legacyGuard.value(QStringLiteral("ran")).toBool();
    QString guardText = guardRan ? QStringLiteral("已运行") : QStringLiteral("未运行");
    if (legacyGuard.value(QStringLiteral("reason")).isString())
    {
        guardText += QStringLiteral(" (") + legacyGuard.value(QStringLiteral("reason")).toString()
            + QStringLiteral(")");
    }
    lines.push_back(QStringLiteral("Legacy guard: ") + guardText);

    const QStringList utilityKeys{
        QStringLiteral("outerVarnishShell"),
        QStringLiteral("clearanceDistance"),
        QStringLiteral("topologyDiagnostic"),
        QStringLiteral("materialClosureAssist")};
    for (const QString& key : utilityKeys)
    {
        AppendUtilitySummary(lines, key, utilities);
    }

    const QJsonArray issues = object.value(QStringLiteral("issues")).toArray();
    lines.push_back(QString());
    lines.push_back(QStringLiteral("报告级问题: ") + (issues.isEmpty() ? QStringLiteral("无") : QString()));
    for (const QJsonValue& value : issues)
    {
        const QJsonObject issue = value.toObject();
        lines.push_back(
            QStringLiteral("- [") + issue.value(QStringLiteral("severity")).toString() + QStringLiteral("] ")
            + issue.value(QStringLiteral("code")).toString() + QStringLiteral(": ")
            + issue.value(QStringLiteral("message")).toString());
    }
    return lines.join(QChar('\n'));
}

}  // namespace

OpenVdbUtilityReportInterpretation OpenVdbUtilityReportInterpreter::Interpret(const QJsonObject& object)
{
    OpenVdbUtilityReportInterpretation result;
    const QString schema = object.value(QStringLiteral("schema")).toString();
    result.recognized = schema.startsWith(kSchemaFamily);
    if (!result.recognized)
    {
        return result;
    }

    if (schema != kSchema)
    {
        AddError(result.errors, QStringLiteral("schema"), QStringLiteral("不支持的 OpenVDB Utility schema ") + schema);
    }

    const QJsonObject build = ReadRequiredObject(object, QStringLiteral("build"), QStringLiteral("root"), result.errors);
    const QJsonObject outputPolicy =
        ReadRequiredObject(object, QStringLiteral("outputPolicy"), QStringLiteral("root"), result.errors);
    const QJsonObject utilities =
        ReadRequiredObject(object, QStringLiteral("utilities"), QStringLiteral("root"), result.errors);
    const QJsonObject decision =
        ReadRequiredObject(object, QStringLiteral("decision"), QStringLiteral("root"), result.errors);
    const QJsonObject validation =
        ReadRequiredObject(object, QStringLiteral("validation"), QStringLiteral("root"), result.errors);
    if (!object.value(QStringLiteral("issues")).isArray())
    {
        AddError(result.errors, QStringLiteral("issues"), QStringLiteral("必须为数组"));
    }

    bool useOpenVdb = false;
    bool openVdbAvailable = false;
    ReadRequiredBool(build, QStringLiteral("useOpenVdb"), QStringLiteral("build"), useOpenVdb, result.errors);
    ReadRequiredBool(
        build,
        QStringLiteral("openVdbAvailable"),
        QStringLiteral("build"),
        openVdbAvailable,
        result.errors);
    if (!useOpenVdb && openVdbAvailable)
    {
        AddError(
            result.errors,
            QStringLiteral("build.openVdbAvailable"),
            QStringLiteral("USE_OPENVDB 关闭时不能为 true"));
    }

    struct RequiredPolicy
    {
        const char* key;
        bool expected;
    };
    const RequiredPolicy policies[] = {
        {"writesProductionPackage", false},
        {"writesProductionTiff", false},
        {"writesPreview", false},
        {"writesUtilityReport", true},
        {"modifiesLegacyOutput", false},
        {"protocolSchemaTouched", false}};
    for (const RequiredPolicy& policy : policies)
    {
        bool actual = false;
        const QString key = QString::fromLatin1(policy.key);
        if (ReadRequiredBool(outputPolicy, key, QStringLiteral("outputPolicy"), actual, result.errors)
            && actual != policy.expected)
        {
            AddError(
                result.errors,
                QStringLiteral("outputPolicy.") + key,
                QStringLiteral("必须为 ") + (policy.expected ? QStringLiteral("true") : QStringLiteral("false")));
        }
    }

    bool replacementAllowed = true;
    if (ReadRequiredBool(
            decision,
            QStringLiteral("productionReplacementAllowed"),
            QStringLiteral("decision"),
            replacementAllowed,
            result.errors)
        && replacementAllowed)
    {
        AddError(
            result.errors,
            QStringLiteral("decision.productionReplacementAllowed"),
            QStringLiteral("必须为 false"));
    }
    const QString openVdbRole =
        ReadRequiredString(decision, QStringLiteral("openVdbRole"), QStringLiteral("decision"), result.errors);
    const QSet<QString> roles{
        QStringLiteral("sdf_utility_candidate"),
        QStringLiteral("diagnostic_only"),
        QStringLiteral("unavailable"),
        QStringLiteral("rejected")};
    if (!openVdbRole.isEmpty() && !roles.contains(openVdbRole))
    {
        AddError(result.errors, QStringLiteral("decision.openVdbRole"), QStringLiteral("包含未知角色 ") + openVdbRole);
    }
    ReadRequiredString(
        decision,
        QStringLiteral("recommendedNextStep"),
        QStringLiteral("decision"),
        result.errors);
    ReadRequiredObject(
        decision,
        QStringLiteral("capabilitySummary"),
        QStringLiteral("decision"),
        result.errors);

    const QStringList utilityKeys{
        QStringLiteral("outerVarnishShell"),
        QStringLiteral("clearanceDistance"),
        QStringLiteral("topologyDiagnostic"),
        QStringLiteral("materialClosureAssist")};
    for (const QString& key : utilityKeys)
    {
        ValidateUtility(key, utilities, openVdbAvailable, result.errors);
    }

    bool schemaValid = false;
    if (ReadRequiredBool(
            validation,
            QStringLiteral("schemaValid"),
            QStringLiteral("validation"),
            schemaValid,
            result.errors)
        && !schemaValid)
    {
        AddError(result.errors, QStringLiteral("validation.schemaValid"), QStringLiteral("报告声明自身无效"));
    }
    const QJsonObject legacyGuard = ReadRequiredObject(
        validation,
        QStringLiteral("legacyGuard"),
        QStringLiteral("validation"),
        result.errors);
    bool legacyGuardRan = false;
    if (ReadRequiredBool(
            legacyGuard,
            QStringLiteral("ran"),
            QStringLiteral("validation.legacyGuard"),
            legacyGuardRan,
            result.errors)
        && !legacyGuardRan
        && !legacyGuard.value(QStringLiteral("reason")).isString())
    {
        AddError(
            result.errors,
            QStringLiteral("validation.legacyGuard.reason"),
            QStringLiteral("ran=false 时必须提供原因"));
    }

    const QSet<QString> severities{
        QStringLiteral("info"),
        QStringLiteral("warning"),
        QStringLiteral("blocker"),
        QStringLiteral("error")};
    const QJsonArray issues = object.value(QStringLiteral("issues")).toArray();
    for (int index = 0; index < issues.size(); ++index)
    {
        if (!issues.at(index).isObject())
        {
            AddError(
                result.errors,
                QStringLiteral("issues[") + QString::number(index) + QStringLiteral("]"),
                QStringLiteral("必须为对象"));
            continue;
        }
        const QJsonObject issue = issues.at(index).toObject();
        const QString issuePath = QStringLiteral("issues[") + QString::number(index) + QStringLiteral("]");
        const QString severity =
            ReadRequiredString(issue, QStringLiteral("severity"), issuePath, result.errors);
        ReadRequiredString(issue, QStringLiteral("code"), issuePath, result.errors);
        if (!severity.isEmpty() && !severities.contains(severity))
        {
            AddError(
                result.errors,
                issuePath + QStringLiteral(".severity"),
                QStringLiteral("包含未知严重级别 ") + severity);
        }
    }

    result.valid = result.errors.isEmpty();
    if (result.valid)
    {
        result.summary = BuildValidSummary(object);
        return result;
    }

    QStringList lines;
    lines.push_back(QStringLiteral("报告状态: 无效，禁止作为生产证据"));
    lines.push_back(QStringLiteral("安全要求: productionReplacementAllowed=false"));
    lines.push_back(QStringLiteral("校验错误:"));
    for (const QString& error : result.errors)
    {
        lines.push_back(QStringLiteral("- ") + error);
    }
    result.summary = lines.join(QChar('\n'));
    return result;
}
