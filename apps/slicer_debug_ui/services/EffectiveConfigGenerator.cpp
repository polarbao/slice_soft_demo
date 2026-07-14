#include "EffectiveConfigGenerator.h"

#include "ConfigValidator.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

namespace
{

QString ModelFillMaterialValue(const ModelFillMaterial material)
{
    return material == ModelFillMaterial::Varnish
        ? QStringLiteral("varnish")
        : QStringLiteral("white");
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
    if (!input.contains(QStringLiteral("format")))
    {
        input.insert(QStringLiteral("format"), QStringLiteral("auto"));
    }
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
