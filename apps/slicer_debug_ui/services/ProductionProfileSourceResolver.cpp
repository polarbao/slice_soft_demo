#include "ProductionProfileSourceResolver.h"

#include "ProductionModeCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

namespace
{

void CopyAllowedSection(
    const QJsonObject& source,
    QJsonObject& destination,
    const QString& key)
{
    const QJsonValue value = source.value(key);
    if (!value.isUndefined())
    {
        destination.insert(key, value);
    }
}

}  // namespace

bool ProductionProfileSourceResult::IsValid() const
{
    return errors.isEmpty()
        && originaldocument.isObject()
        && overridedocument.isObject()
        && !templatepath.trimmed().isEmpty();
}

ProductionProfileSourceResult ProductionProfileSourceResolver::Resolve(
    const ProductionProfileSourceRequest& request) const
{
    ProductionProfileSourceResult result;
    if (request.mode == slicer_core::SlicePipelineMode::Legacy)
    {
        if (!request.legacyoriginaldocument.isObject()
            || !request.legacyoverridedocument.isObject())
        {
            result.errors.push_back(
                QStringLiteral("传统切片 Profile 模板不是有效 JSON object。"));
            return result;
        }
        result.profileid = request.requestedprofileid;
        result.templatepath = request.legacytemplatepath;
        result.originaldocument = request.legacyoriginaldocument;
        result.overridedocument = request.legacyoverridedocument;
        return result;
    }

    const ProductionProfileCapability* capability =
        ProductionModeCatalog::FindProfile(
            request.requestedprofileid.toStdString());
    if (capability == nullptr
        || capability->mode != request.mode
        || capability->sourceconfigrelativepath.empty())
    {
        result.errors.push_back(
            QStringLiteral("未找到获准的 Global Production Profile 源配置：")
            + request.requestedprofileid);
        return result;
    }

    result.profileid = request.requestedprofileid;
    result.templatepath = QDir(request.reporoot).filePath(
        QString::fromStdString(capability->sourceconfigrelativepath));
    QFile sourceFile(result.templatepath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        result.errors.push_back(
            QStringLiteral("无法读取 Global Production Profile 源配置：")
            + result.templatepath);
        return result;
    }

    QJsonParseError parseError{};
    result.originaldocument =
        QJsonDocument::fromJson(sourceFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !result.originaldocument.isObject())
    {
        result.errors.push_back(
            QStringLiteral("Global Production Profile 源配置不是有效 JSON：")
            + parseError.errorString());
        result.originaldocument = {};
        return result;
    }

    const QJsonObject originalRoot = result.originaldocument.object();
    const QString mode = originalRoot.value(QStringLiteral("slicePipeline"))
                             .toObject()
                             .value(QStringLiteral("mode"))
                             .toString();
    const QString profileId =
        originalRoot.value(QStringLiteral("materialProcessProfile"))
            .toObject()
            .value(QStringLiteral("target"))
            .toString();
    if (mode != QStringLiteral("global_surface_shell")
        || profileId != result.profileid)
    {
        result.errors.push_back(
            QStringLiteral("Global Production Profile 源配置身份与能力目录不匹配。"));
        result.originaldocument = {};
        return result;
    }

    QJsonObject overrideRoot = originalRoot;
    const QJsonObject legacyOverride =
        request.legacyoverridedocument.object();
    const QStringList allowedSections{
        QStringLiteral("output"),
        QStringLiteral("modelTransform"),
        QStringLiteral("autoOrient"),
        QStringLiteral("background"),
        QStringLiteral("preview"),
    };
    for (const QString& key : allowedSections)
    {
        CopyAllowedSection(legacyOverride, overrideRoot, key);
    }

    QJsonObject input = overrideRoot.value(QStringLiteral("input")).toObject();
    input.insert(QStringLiteral("format"), QStringLiteral("auto"));
    overrideRoot.insert(QStringLiteral("input"), input);
    result.overridedocument = QJsonDocument(overrideRoot);
    return result;
}
