#include "HostProfileCatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

namespace
{
const QSet<QString>& SupportedSafetyLevels()
{
    static const QSet<QString> levels{
        QStringLiteral("production"),
        QStringLiteral("restricted"),
        QStringLiteral("diagnostic")};
    return levels;
}

bool ParseModuleCapabilities(
    const QByteArray& moduleInfo,
    QStringList* capabilities,
    QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        moduleInfo, &parseError);
    const QJsonObject root = document.object();
    if (!document.isObject()
        || root.value(QStringLiteral("schema")).toString()
            != QStringLiteral("slicesoft.module_info.1"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块信息不是有效的 slicesoft.module_info.1：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonArray provides = root.value(QStringLiteral("provides")).toArray();
    QSet<QString> uniqueCapabilities;
    for (const QJsonValue& value : provides)
    {
        const QString capability = value.toString().trimmed();
        if (!value.isString() || capability.isEmpty()
            || uniqueCapabilities.contains(capability))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral(
                    "pm_module_info.provides 包含空值、非字符串或重复能力。");
            }
            return false;
        }
        uniqueCapabilities.insert(capability);
        capabilities->append(capability);
    }
    if (capabilities->isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("pm_module_info.provides 不能为空。");
        }
        return false;
    }
    capabilities->sort();
    return true;
}

bool ValidateProfile(
    const hostprofiledescriptor& profile,
    QSet<QString>* knownProfileIds,
    QString* error)
{
    if (profile.profileid.trimmed().isEmpty()
        || profile.displayname.trimmed().isEmpty()
        || profile.requiredcapabilities.isEmpty()
        || !SupportedSafetyLevels().contains(profile.productionsafety))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "Profile 必须包含 id、名称、已知安全级别和至少一项能力要求：%1")
                         .arg(profile.profileid);
        }
        return false;
    }
    if (knownProfileIds->contains(profile.profileid))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("宿主 Profile id 重复：%1")
                         .arg(profile.profileid);
        }
        return false;
    }
    knownProfileIds->insert(profile.profileid);

    QSet<QString> requirements;
    for (const QString& requirement : profile.requiredcapabilities)
    {
        const QString capability = requirement.trimmed();
        if (capability.isEmpty() || requirements.contains(capability))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("Profile 能力要求为空或重复：%1")
                             .arg(profile.profileid);
            }
            return false;
        }
        requirements.insert(capability);
    }
    return true;
}
}

QList<hostprofiledescriptor> ReferenceHostProfileCatalog::Profiles() const
{
    return {
        hostprofiledescriptor{
            QStringLiteral("host-reference-default"),
            QStringLiteral("彩色纹理生产 · 材料与支撑可编辑"),
            QStringLiteral(
                "面向 OBJ、3MF、STL 的常规生产切片，支持彩色纹理、"
                "单材料和宿主侧工艺参数编辑。"),
            QStringLiteral("production"),
            {QStringLiteral("彩色纹理"), QStringLiteral("RGBWSV")},
            {QStringLiteral("model.import"),
             QStringLiteral("geometry.preflight"),
             QStringLiteral("slice.rgbwsv"),
             QStringLiteral("package.verify")},
            QStringLiteral(
                "日常模型导入、排版、参数调整和生产包生成。"),
            QStringLiteral(
                "默认 635×600 DPI、0.038 mm 层厚、全实体 RGB、"
                "下表面投影支撑；材料、支撑和纹理策略可在切片设置中修改。"),
            QStringLiteral(
                "输出 p0.rgbwsv.2、uint8、black_is_print、"
                "R/G/B/W/S/V 六通道 TIFF，并执行严格包校验。"),
            QStringLiteral(
                "纹理不会因选择 Profile 自动开启；请在切片设置中确认"
                "纹理应用模式、白区载体和输出目录。")},
        hostprofiledescriptor{
            QStringLiteral("host-reference-material-parity"),
            QStringLiteral("材料一致性验证 · 受限候选"),
            QStringLiteral(
                "用于验证修复后几何与材料通道的一致性，不作为日常默认工艺。"),
            QStringLiteral("restricted"),
            {QStringLiteral("材料验证"), QStringLiteral("受限")},
            {QStringLiteral("geometry.preflight"),
             QStringLiteral("geometry.repair"),
             QStringLiteral("slice.rgbwsv"),
             QStringLiteral("package.verify")},
            QStringLiteral(
                "几何修复候选、材料闭合和通道一致性专项验证。"),
            QStringLiteral(
                "沿用宿主当前材料、支撑和纹理参数，但要求模块提供"
                "geometry.repair 能力。"),
            QStringLiteral(
                "输出与生产 Profile 相同的 RGBWSV 协议包并严格校验。"),
            QStringLiteral(
                "受限候选不等于生产准入；修复失败、严格预检失败或"
                "材料不闭合时必须停止。")},
        hostprofiledescriptor{
            QStringLiteral("host-reference-package-review"),
            QStringLiteral("生产包与六通道检查 · 只读"),
            QStringLiteral("只读检查生产包摘要、报告和层预览。"),
            QStringLiteral("diagnostic"),
            {QStringLiteral("包检查"), QStringLiteral("只读")},
            {QStringLiteral("package.verify"),
             QStringLiteral("package.get_summary"),
             QStringLiteral("package.render_layer_preview"),
             QStringLiteral("package.read_report")},
            QStringLiteral(
                "加载既有生产包，检查 manifest、报告和逐层通道。"),
            QStringLiteral("不提交模型切片，不修改任何生产参数。"),
            QStringLiteral(
                "读取并校验 p0.rgbwsv.2 包，提供层预览和报告摘要。"),
            QStringLiteral(
                "该 Profile 仅用于结果检查，不能作为模型切片入口。")}};
}

bool HostProfileCapabilityResolver::Resolve(
    const IHostProfileCatalog& catalog,
    const QByteArray& moduleInfo,
    hostprofilecatalogresolution* resolution,
    QString* error)
{
    if (resolution == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Profile 解析结果目标不能为空。");
        }
        return false;
    }
    *resolution = {};
    if (!ParseModuleCapabilities(
            moduleInfo, &resolution->modulecapabilities, error))
    {
        return false;
    }

    const QList<hostprofiledescriptor> profiles = catalog.Profiles();
    if (profiles.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("宿主 Profile 目录不能为空。");
        }
        return false;
    }

    const QSet<QString> moduleCapabilities(
        resolution->modulecapabilities.cbegin(),
        resolution->modulecapabilities.cend());
    QSet<QString> knownProfileIds;
    for (const hostprofiledescriptor& profile : profiles)
    {
        if (!ValidateProfile(profile, &knownProfileIds, error))
        {
            *resolution = {};
            return false;
        }
        hostprofileavailability availability;
        availability.profile = profile;
        for (const QString& requirement : profile.requiredcapabilities)
        {
            if (!moduleCapabilities.contains(requirement))
            {
                availability.missingcapabilities.append(requirement);
            }
        }
        availability.available = availability.missingcapabilities.isEmpty();
        resolution->profiles.append(availability);
    }
    return true;
}
