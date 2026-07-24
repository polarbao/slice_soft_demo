#include "EffectiveConfigGenerator.h"

#include "ConfigValidator.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

namespace
{

QString ModelFillMaterialValue(const ModelFillMaterial material)
{
    switch (material)
    {
    case ModelFillMaterial::Rgb:
        return QStringLiteral("rgb");
    case ModelFillMaterial::Varnish:
        return QStringLiteral("varnish");
    case ModelFillMaterial::White:
    default:
        return QStringLiteral("white");
    }
}

QString SupportPlacementValue(const SupportPlacement placement)
{
    switch (placement)
    {
    case SupportPlacement::Upper:
        return QStringLiteral("upper");
    case SupportPlacement::Both:
        return QStringLiteral("both");
    case SupportPlacement::UnsupportedOnly:
        return QStringLiteral("unsupported_only");
    case SupportPlacement::FullVerticalProjection:
        return QStringLiteral("full_vertical_projection");
    case SupportPlacement::Lower:
    default:
        return QStringLiteral("lower");
    }
}

QString SupportModeValue(const SupportPlacement placement)
{
    if (placement == SupportPlacement::UnsupportedOnly)
    {
        return QStringLiteral("unsupported_only");
    }
    if (placement == SupportPlacement::FullVerticalProjection)
    {
        return QStringLiteral("full_vertical_projection");
    }
    return QStringLiteral("bottom_projection");
}

QString EngineRoleValue(const SliceEngineRole role)
{
    return role == SliceEngineRole::OpenVdbUtilityCandidate
        ? QStringLiteral("OpenVDB utility/candidate")
        : QStringLiteral("legacy production");
}

QString PipelineModeValue(const slicer_core::SlicePipelineMode mode)
{
    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(mode);
    if (capability == nullptr)
    {
        return {};
    }
    return QString::fromStdString(capability->stablevalue);
}

QJsonObject ObjectWithValues(QJsonObject object, const std::initializer_list<std::pair<QString, QJsonValue>>& values)
{
    for (const auto& value : values)
    {
        object.insert(value.first, value.second);
    }
    return object;
}

void ApplySettings(QJsonObject& root, const SliceSettingsState& settings)
{
    QJsonObject input = root.value(QStringLiteral("input")).toObject();
    input.insert(QStringLiteral("modelPath"), QDir::fromNativeSeparators(settings.modelpath));
    input.insert(QStringLiteral("format"), QStringLiteral("auto"));
    root.insert(QStringLiteral("input"), input);

    QJsonObject output = root.value(QStringLiteral("output")).toObject();
    output.insert(QStringLiteral("packageDir"), QDir::fromNativeSeparators(settings.outputdirectory));
    output.insert(QStringLiteral("layerThicknessMm"), settings.layerthicknessmm);
    root.insert(QStringLiteral("output"), output);

    QJsonObject modelfill = root.value(QStringLiteral("modelFill")).toObject();
    modelfill = ObjectWithValues(
        modelfill,
        {
            {QStringLiteral("enabled"), true},
            {QStringLiteral("material"), ModelFillMaterialValue(settings.modelfillmaterial)},
            {QStringLiteral("scope"), modelfill.value(QStringLiteral("scope")).toString(QStringLiteral("below_texture_surface"))},
            {QStringLiteral("value"), 0},
            {QStringLiteral("emptyAllowedInProduction"), false},
            {QStringLiteral("legacyRgbFallback"), false},
        });
    root.insert(QStringLiteral("modelFill"), modelfill);

    QJsonObject support = root.value(QStringLiteral("support")).toObject();
    support.insert(QStringLiteral("enabled"), settings.support.enabled);
    support.insert(
        QStringLiteral("mode"),
        settings.support.enabled ? SupportModeValue(settings.support.placement) : QStringLiteral("none"));
    support.insert(QStringLiteral("placement"), SupportPlacementValue(settings.support.placement));
    QJsonObject internalvoid = support.value(QStringLiteral("internalVoid")).toObject();
    internalvoid.insert(
        QStringLiteral("enabled"),
        settings.support.enabled && settings.support.internalvoidenabled);
    internalvoid.insert(QStringLiteral("minAreaPx"), settings.support.internalvoidminareapx);
    internalvoid.insert(QStringLiteral("fillRule"), QStringLiteral("all_internal_voids"));
    support.insert(QStringLiteral("internalVoid"), internalvoid);
    QJsonObject upper = support.value(QStringLiteral("upper")).toObject();
    upper.insert(
        QStringLiteral("enabled"),
        settings.support.enabled
            && (settings.support.placement == SupportPlacement::Upper
                || settings.support.placement == SupportPlacement::Both));
    upper.insert(QStringLiteral("outside"), QStringLiteral("outer_varnish_shell"));
    upper.insert(QStringLiteral("reason"), QStringLiteral("optional_detachable_surface_support"));
    support.insert(QStringLiteral("upper"), upper);
    root.insert(QStringLiteral("support"), support);

    QJsonObject surfacevarnish = root.value(QStringLiteral("surfaceVarnish")).toObject();
    surfacevarnish = ObjectWithValues(
        surfacevarnish,
        {
            {QStringLiteral("enabled"), settings.surfacevarnish.enabled},
            {QStringLiteral("outerSurface"), true},
            {QStringLiteral("innerSurface"), true},
            {QStringLiteral("thicknessPx"), settings.surfacevarnish.thicknesspx},
            {QStringLiteral("value"), 0},
            {QStringLiteral("source"), QStringLiteral("explicit")},
        });
    root.insert(QStringLiteral("surfaceVarnish"), surfacevarnish);

    QJsonObject outervarnish = root.value(QStringLiteral("outerVarnish")).toObject();
    outervarnish = ObjectWithValues(
        outervarnish,
        {
            {QStringLiteral("enabled"), settings.outervarnish.enabled},
            {QStringLiteral("thicknessMm"), settings.outervarnish.thicknessmm},
            {QStringLiteral("thicknessStepMm"), 0.01},
            {QStringLiteral("pixelPitchUm"), settings.outervarnish.pixelpitchum},
            {QStringLiteral("allowXYExpansion"), true},
            {QStringLiteral("conflictPolicy"), QStringLiteral("varnish_shell_wins")},
            {QStringLiteral("value"), 0},
        });
    root.insert(QStringLiteral("outerVarnish"), outervarnish);

    QJsonObject preview = root.value(QStringLiteral("preview")).toObject();
    preview.insert(QStringLiteral("enabled"), settings.preview.enabled);
    preview.insert(QStringLiteral("interval"), settings.preview.interval);
    root.insert(QStringLiteral("preview"), preview);

    QJsonObject experimental = root.value(QStringLiteral("experimental")).toObject();
    QJsonObject openvdb = experimental.value(QStringLiteral("openvdbPipeline")).toObject();
    const bool candidate = settings.enginerole == SliceEngineRole::OpenVdbUtilityCandidate;
    openvdb.insert(QStringLiteral("enabled"), candidate);
    openvdb.insert(QStringLiteral("engine"), candidate ? QStringLiteral("openvdb") : QStringLiteral("legacy"));
    openvdb.insert(
        QStringLiteral("admissionMode"),
        candidate ? QStringLiteral("diagnostic_only") : QStringLiteral("strict_closed"));
    openvdb.insert(
        QStringLiteral("failurePolicy"),
        candidate ? QStringLiteral("diagnostic_only") : QStringLiteral("fail_fast"));
    openvdb.insert(QStringLiteral("allowNonProductionOutput"), candidate);
    openvdb.insert(QStringLiteral("writeProductionRgbwsv"), false);
    experimental.insert(QStringLiteral("openvdbPipeline"), openvdb);
    root.insert(QStringLiteral("experimental"), experimental);
}

void CollectChangedLeafPaths(
    const QString& path,
    const QJsonValue& currentValue,
    const QJsonValue& lockedValue,
    QStringList& paths)
{
    if (currentValue == lockedValue)
    {
        return;
    }
    if (currentValue.isObject() || lockedValue.isObject())
    {
        const QJsonObject currentObject = currentValue.toObject();
        const QJsonObject lockedObject = lockedValue.toObject();
        QSet<QString> keys;
        for (auto iterator = currentObject.begin();
             iterator != currentObject.end();
             ++iterator)
        {
            keys.insert(iterator.key());
        }
        for (auto iterator = lockedObject.begin();
             iterator != lockedObject.end();
             ++iterator)
        {
            keys.insert(iterator.key());
        }
        QStringList sortedKeys = keys.values();
        sortedKeys.sort();
        for (const QString& key : sortedKeys)
        {
            CollectChangedLeafPaths(
                path.isEmpty() ? key : path + QStringLiteral(".") + key,
                currentObject.value(key),
                lockedObject.value(key),
                paths);
        }
        return;
    }
    paths.push_back(path);
}

void RestoreLockedSection(
    QJsonObject& root,
    const QJsonObject& baseline,
    const QString& key,
    QStringList& disabledOverrides)
{
    const QJsonValue currentValue = root.value(key);
    const QJsonValue lockedValue = baseline.value(key);
    CollectChangedLeafPaths(
        key,
        currentValue,
        lockedValue,
        disabledOverrides);
    if (lockedValue.isUndefined())
    {
        root.remove(key);
    }
    else
    {
        root.insert(key, lockedValue);
    }
}

bool ApplyProductionCapabilityLock(
    QJsonObject& root,
    const QJsonObject& baseline,
    const EffectiveConfigRequest& request,
    QStringList& disabledOverrides,
    QString& effectiveProfileId,
    QStringList& errors)
{
    const ProductionModeCapability* modeCapability =
        ProductionModeCatalog::FindMode(request.production.requestedmode);
    if (modeCapability == nullptr)
    {
        errors.push_back(
            QStringLiteral("请求的生产切片模式不在能力目录中。"));
        return false;
    }

    const QString requestedMode =
        QString::fromStdString(modeCapability->stablevalue);
    if (request.production.requestedmode
        == slicer_core::SlicePipelineMode::Legacy)
    {
        QJsonObject slicePipeline =
            root.value(QStringLiteral("slicePipeline")).toObject();
        slicePipeline.insert(QStringLiteral("mode"), requestedMode);
        root.insert(QStringLiteral("slicePipeline"), slicePipeline);

        QJsonObject processProfile =
            root.value(QStringLiteral("materialProcessProfile")).toObject();
        effectiveProfileId =
            request.production.requestedprofileid.trimmed();
        if (effectiveProfileId.isEmpty())
        {
            effectiveProfileId =
                processProfile.value(QStringLiteral("target"))
                    .toString()
                    .trimmed();
        }
        else
        {
            processProfile.insert(
                QStringLiteral("target"),
                effectiveProfileId);
            root.insert(
                QStringLiteral("materialProcessProfile"),
                processProfile);
        }
        return true;
    }

    const QString requestedProfileId =
        request.production.requestedprofileid.trimmed();
    if (requestedProfileId.isEmpty())
    {
        errors.push_back(
            QStringLiteral("全局纹理壳层必须显式选择获准的 Production Profile。"));
        return false;
    }
    const ProductionProfileCapability* profileCapability =
        ProductionModeCatalog::FindProfile(
            requestedProfileId.toStdString());
    if (profileCapability == nullptr)
    {
        errors.push_back(
            QStringLiteral("请求的 Global Production Profile 未获准：")
            + requestedProfileId);
        return false;
    }
    if (profileCapability->mode != request.production.requestedmode)
    {
        errors.push_back(
            QStringLiteral("生产模式与 Production Profile 能力目录不匹配。"));
        return false;
    }

    const QString baselineMode =
        baseline.value(QStringLiteral("slicePipeline"))
            .toObject()
            .value(QStringLiteral("mode"))
            .toString();
    const QString baselineProfileId =
        baseline.value(QStringLiteral("materialProcessProfile"))
            .toObject()
            .value(QStringLiteral("target"))
            .toString();
    if (baselineMode != requestedMode
        || baselineProfileId != requestedProfileId)
    {
        errors.push_back(
            QStringLiteral(
                "原始只读 Profile 与请求的 Global 模式/Profile 不匹配，拒绝生成生效配置。"));
        return false;
    }

    const QStringList lockedSections{
        QStringLiteral("slicePipeline"),
        QStringLiteral("texture"),
        QStringLiteral("modelFill"),
        QStringLiteral("materialProcessProfile"),
        QStringLiteral("support"),
        QStringLiteral("surfaceVarnish"),
        QStringLiteral("outerVarnish"),
        QStringLiteral("materialPolicy"),
        QStringLiteral("materialRoleMapping"),
        QStringLiteral("materialClosure"),
        QStringLiteral("experimental"),
    };
    for (const QString& key : lockedSections)
    {
        RestoreLockedSection(
            root,
            baseline,
            key,
            disabledOverrides);
    }
    disabledOverrides.removeDuplicates();
    disabledOverrides.sort();

    QJsonObject slicePipeline =
        root.value(QStringLiteral("slicePipeline")).toObject();
    slicePipeline.insert(QStringLiteral("mode"), requestedMode);
    root.insert(QStringLiteral("slicePipeline"), slicePipeline);
    QJsonObject processProfile =
        root.value(QStringLiteral("materialProcessProfile")).toObject();
    processProfile.insert(QStringLiteral("target"), requestedProfileId);
    root.insert(QStringLiteral("materialProcessProfile"), processProfile);
    effectiveProfileId = requestedProfileId;
    return true;
}

QString EffectiveSessionId(const EffectiveConfigRequest& request)
{
    const QString configured = request.production.sessionid.trimmed();
    if (!configured.isEmpty())
    {
        return configured;
    }
    return QFileInfo(request.generatedconfigpath)
        .absoluteDir()
        .dirName();
}

QString EffectiveGeneratedAtUtc(
    const ProductionEffectiveConfigSelection& production)
{
    const QString configured = production.generatedatutc.trimmed();
    return configured.isEmpty()
        ? QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
        : configured;
}

void ApplyProductionAudit(
    QJsonObject& root,
    const EffectiveConfigRequest& request,
    const QString& effectiveProfileId,
    const QStringList& disabledOverrides)
{
    const QString requestedMode =
        PipelineModeValue(request.production.requestedmode);
    QString requestedProfileId =
        request.production.requestedprofileid.trimmed();
    if (requestedProfileId.isEmpty())
    {
        requestedProfileId = effectiveProfileId;
    }

    QJsonObject productionAudit{
        {QStringLiteral("schema"),
         QStringLiteral(
             "slicesoft.ui.production_effective_config.12e_09b.1")},
        {QStringLiteral("sourceProfileId"),
         request.production.sourceprofileid.trimmed().isEmpty()
             ? request.profileid
             : request.production.sourceprofileid},
        {QStringLiteral("requestedPipelineMode"), requestedMode},
        {QStringLiteral("effectivePipelineMode"), requestedMode},
        {QStringLiteral("requestedProductionProfileId"),
         requestedProfileId},
        {QStringLiteral("effectiveProductionProfileId"),
         effectiveProfileId},
        {QStringLiteral("capabilityLockVersion"),
         QString::fromUtf8(
             ProductionModeCatalog::CapabilityLockVersion().data(),
             static_cast<int>(
                 ProductionModeCatalog::CapabilityLockVersion().size()))},
        {QStringLiteral("disabledOverrides"),
         QJsonArray::fromStringList(disabledOverrides)},
        {QStringLiteral("sourceModelPath"),
         QDir::fromNativeSeparators(request.settings.modelpath)},
        {QStringLiteral("sourceTemplatePath"),
         QDir::fromNativeSeparators(request.templatepath)},
        {QStringLiteral("sessionId"), EffectiveSessionId(request)},
        {QStringLiteral("generatedAtUtc"),
         EffectiveGeneratedAtUtc(request.production)},
    };
    QJsonObject uiAudit =
        root.value(QStringLiteral("uiAudit")).toObject();
    uiAudit.insert(QStringLiteral("production"), productionAudit);
    root.insert(QStringLiteral("uiAudit"), uiAudit);
}

bool NormalizeModelFillTextureContract(QJsonObject& root)
{
    const QJsonObject modelFill = root.value(QStringLiteral("modelFill")).toObject();
    QJsonObject texture = root.value(QStringLiteral("texture")).toObject();
    const QString fillMaterial = modelFill.value(QStringLiteral("material")).toString();
    const bool requiresSeparateFill = fillMaterial != QStringLiteral("rgb");
    if (!modelFill.value(QStringLiteral("enabled")).toBool(false)
        || modelFill.value(QStringLiteral("scope")).toString() != QStringLiteral("below_texture_surface")
        || !requiresSeparateFill
        || !texture.value(QStringLiteral("enabled")).toBool(false)
        || texture.value(QStringLiteral("applyMode")).toString()
            != QStringLiteral("solid_volume_from_top_surface"))
    {
        return false;
    }

    texture.insert(QStringLiteral("applyMode"), QStringLiteral("top_surface_band"));
    texture.insert(QStringLiteral("topSurfaceLayers"), 1);
    root.insert(QStringLiteral("texture"), texture);
    return true;
}

QString BuildSummary(
    const EffectiveConfigRequest& request,
    const EffectiveConfigResult& result)
{
    const SliceSettingsState& settings = request.settings;
    return QStringLiteral(
               "Profile：%1\n模板：%2\n生效配置：%3\n模型：%4\n输出：%5\n层高：%6 mm\n"
               "模型填充：%7\n支撑：%8，内部镂空=%9\n表面光油：%10 / %11 px\n"
               "外侧光油：%12 / %13 mm\n预览：%14 / 间隔 %15\n引擎：%16\n差异：%17 项")
        .arg(request.profileid,
             request.templatepath,
             request.generatedconfigpath,
             settings.modelpath,
             settings.outputdirectory)
        .arg(settings.layerthicknessmm, 0, 'f', 4)
        .arg(ModelFillMaterialValue(settings.modelfillmaterial),
             SupportPlacementValue(settings.support.placement),
             settings.support.internalvoidenabled ? QStringLiteral("开") : QStringLiteral("关"),
             settings.surfacevarnish.enabled ? QStringLiteral("开") : QStringLiteral("关"))
        .arg(settings.surfacevarnish.thicknesspx)
        .arg(settings.outervarnish.enabled ? QStringLiteral("开") : QStringLiteral("关"))
        .arg(settings.outervarnish.thicknessmm, 0, 'f', 2)
        .arg(settings.preview.enabled ? QStringLiteral("开") : QStringLiteral("关"))
        .arg(settings.preview.interval)
        .arg(EngineRoleValue(settings.enginerole))
        .arg(result.differences.size());
}

bool SameFilePath(const QString& left, const QString& right)
{
    if (left.trimmed().isEmpty() || right.trimmed().isEmpty())
    {
        return false;
    }
    return QFileInfo(left).absoluteFilePath().compare(
               QFileInfo(right).absoluteFilePath(),
               Qt::CaseInsensitive)
        == 0;
}

}  // namespace

bool EffectiveConfigResult::IsValid() const
{
    return errors.isEmpty() && !document.isNull() && !generatedconfigpath.isEmpty();
}

EffectiveConfigResult EffectiveConfigGenerator::Generate(const EffectiveConfigRequest& request) const
{
    EffectiveConfigResult result;
    result.generatedconfigpath = request.generatedconfigpath;

    if (!request.overridedocument.isObject() || !request.originaldocument.isObject())
    {
        result.errors.push_back(QStringLiteral("Profile 模板或 UI override 不是有效 JSON object。"));
        return result;
    }
    if (request.generatedconfigpath.trimmed().isEmpty())
    {
        result.errors.push_back(QStringLiteral("session generated config 路径不能为空。"));
        return result;
    }
    if (SameFilePath(request.templatepath, request.generatedconfigpath))
    {
        result.errors.push_back(QStringLiteral("generated config 禁止覆盖原始 Profile 模板。"));
        return result;
    }

    SliceSettingsModel settingsmodel;
    settingsmodel.SetState(request.settings);
    const SliceSettingsValidationResult settingsvalidation = settingsmodel.Validate();
    result.warnings.append(settingsvalidation.warnings);
    result.errors.append(settingsvalidation.errors);
    if (!settingsvalidation.IsValid())
    {
        return result;
    }

    QJsonObject root = request.overridedocument.object();
    ApplySettings(root, request.settings);
    if (NormalizeModelFillTextureContract(root))
    {
        result.warnings.push_back(
            QStringLiteral(
                "纹理投影到整个实体会占满模型内部区域；生效配置已改为 1 层顶面纹理带，以保留白墨/光油模型内部填充。原始 Profile 模板未修改。"));
    }

    QStringList disabledOverrides;
    QString effectiveProfileId;
    if (!ApplyProductionCapabilityLock(
            root,
            request.originaldocument.object(),
            request,
            disabledOverrides,
            effectiveProfileId,
            result.errors))
    {
        return result;
    }
    if (!disabledOverrides.isEmpty())
    {
        result.warnings.push_back(
            QStringLiteral("Production Profile 能力锁定已清除 %1 个不受支持的 stale override：%2")
                .arg(disabledOverrides.size())
                .arg(disabledOverrides.join(QStringLiteral(", "))));
    }
    ApplyProductionAudit(
        root,
        request,
        effectiveProfileId,
        disabledOverrides);
    result.document = QJsonDocument(root);

    const ConfigValidationResult configvalidation = ConfigValidator::validate(root);
    result.warnings.append(configvalidation.warnings);
    result.errors.append(configvalidation.errors);
    if (!configvalidation.isValid())
    {
        result.document = QJsonDocument{};
        return result;
    }

    result.differences = ConfigDiffModel::DiffAll(request.originaldocument, result.document);
    result.summary = BuildSummary(request, result);

    const QFileInfo generatedinfo(request.generatedconfigpath);
    if (!QDir().mkpath(generatedinfo.absolutePath()))
    {
        result.errors.push_back(QStringLiteral("无法创建 generated config 会话目录：") + generatedinfo.absolutePath());
        result.document = QJsonDocument{};
        return result;
    }

    QSaveFile generatedfile(request.generatedconfigpath);
    if (!generatedfile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        result.errors.push_back(QStringLiteral("无法写入 generated config：") + request.generatedconfigpath);
        result.document = QJsonDocument{};
        return result;
    }
    if (generatedfile.write(result.document.toJson(QJsonDocument::Indented)) < 0
        || !generatedfile.commit())
    {
        result.errors.push_back(QStringLiteral("提交 generated config 失败：") + request.generatedconfigpath);
        result.document = QJsonDocument{};
    }
    return result;
}
