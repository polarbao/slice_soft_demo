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
            QStringLiteral("彩色纹理生产"),
            QStringLiteral("RGBWSV 生产切片与严格包校验。"),
            QStringLiteral("production"),
            {QStringLiteral("彩色纹理"), QStringLiteral("RGBWSV")},
            {QStringLiteral("model.import"),
             QStringLiteral("geometry.preflight"),
             QStringLiteral("slice.rgbwsv"),
             QStringLiteral("package.verify")}},
        hostprofiledescriptor{
            QStringLiteral("host-reference-material-parity"),
            QStringLiteral("材料一致性候选"),
            QStringLiteral("启用几何修复能力的受限材料验证流程。"),
            QStringLiteral("restricted"),
            {QStringLiteral("材料验证"), QStringLiteral("受限")},
            {QStringLiteral("geometry.preflight"),
             QStringLiteral("geometry.repair"),
             QStringLiteral("slice.rgbwsv"),
             QStringLiteral("package.verify")}},
        hostprofiledescriptor{
            QStringLiteral("host-reference-package-review"),
            QStringLiteral("包与通道诊断"),
            QStringLiteral("只读检查生产包摘要、报告和层预览。"),
            QStringLiteral("diagnostic"),
            {QStringLiteral("包检查"), QStringLiteral("只读")},
            {QStringLiteral("package.verify"),
             QStringLiteral("package.get_summary"),
             QStringLiteral("package.render_layer_preview"),
             QStringLiteral("package.read_report")}}};
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
