#include "SingleMaterialReliefResolver.h"

#include <QJsonArray>

namespace
{

constexpr auto kSingleMaterialProfileId = "single_material_relief";

QString MaterialValue(const SingleMaterialReliefMaterial material)
{
    return material == SingleMaterialReliefMaterial::White
        ? QStringLiteral("white")
        : QStringLiteral("varnish");
}

QString ChannelValue(const SingleMaterialReliefMaterial material)
{
    return material == SingleMaterialReliefMaterial::White
        ? QStringLiteral("W")
        : QStringLiteral("V");
}

void SetFailure(
    SingleMaterialReliefState& state,
    const SingleMaterialReliefErrorCode errorCode,
    const QString& issue)
{
    state.valid = false;
    state.errorcode = errorCode;
    state.issues.clear();
    state.issues.push_back(issue);
}

void SetValid(SingleMaterialReliefState& state)
{
    state.valid = true;
    state.errorcode = SingleMaterialReliefErrorCode::None;
    state.issues.clear();
}

bool IsSingleMaterialProfile(
    const QJsonObject& config,
    const QString& profileId)
{
    return profileId == QString::fromLatin1(kSingleMaterialProfileId)
        && config.value(QStringLiteral("slicingMode")).toString()
            == QStringLiteral("relief_heightfield");
}

bool IsRgbEmpty(const QJsonObject& modelMaterial)
{
    return modelMaterial.value(QStringLiteral("rgb")).toArray()
        == QJsonArray{255, 255, 255};
}

bool IsProcessProfileConsistent(
    const QJsonObject& process,
    const SingleMaterialReliefMaterial material)
{
    if (!process.value(QStringLiteral("enabled")).toBool(false)
        || process.value(QStringLiteral("name")).toString()
            != QString::fromLatin1(kSingleMaterialProfileId)
        || process.value(QStringLiteral("target")).toString()
            != QString::fromLatin1(kSingleMaterialProfileId))
    {
        return false;
    }

    const QJsonObject rgb =
        process.value(QStringLiteral("rgb")).toObject();
    const QJsonObject white =
        process.value(QStringLiteral("white")).toObject();
    const QJsonObject varnish =
        process.value(QStringLiteral("varnish")).toObject();
    const QJsonObject validation =
        process.value(QStringLiteral("validation")).toObject();
    const bool whiteSelected =
        material == SingleMaterialReliefMaterial::White;

    return !rgb.value(QStringLiteral("enabled")).toBool(true)
        && rgb.value(QStringLiteral("source")).toString()
            == QStringLiteral("modelMaterial")
        && white.value(QStringLiteral("enabled")).toBool(false)
            == whiteSelected
        && white.value(QStringLiteral("mode")).toString()
            == (whiteSelected ? QStringLiteral("all_model")
                              : QStringLiteral("disabled"))
        && varnish.value(QStringLiteral("enabled")).toBool(false)
            == !whiteSelected
        && varnish.value(QStringLiteral("mode")).toString()
            == (whiteSelected ? QStringLiteral("disabled")
                              : QStringLiteral("all_model"))
        && validation.value(QStringLiteral("requireRgbPixels"))
               .toBool(true)
            == false
        && validation.value(QStringLiteral("requireWhitePixels"))
               .toBool(false)
            == whiteSelected
        && validation.value(QStringLiteral("requireVarnishPixels"))
               .toBool(false)
            == !whiteSelected;
}

bool IsModelFillConsistent(
    const QJsonObject& root,
    const SingleMaterialReliefMaterial material)
{
    if (!root.contains(QStringLiteral("modelFill")))
    {
        return true;
    }
    const QJsonObject modelFill =
        root.value(QStringLiteral("modelFill")).toObject();
    return modelFill.value(QStringLiteral("enabled")).toBool(false)
        && modelFill.value(QStringLiteral("material")).toString()
            == MaterialValue(material)
        && modelFill.value(QStringLiteral("scope")).toString()
            == QStringLiteral("all_model")
        && modelFill.value(QStringLiteral("value")).toInt(-1) == 0
        && !modelFill.value(QStringLiteral("emptyAllowedInProduction"))
                .toBool(true)
        && !modelFill.value(QStringLiteral("legacyRgbFallback"))
                .toBool(true);
}

bool IsConfigConsistent(
    const QJsonObject& config,
    const SingleMaterialReliefMaterial material)
{
    const QJsonObject modelMaterial =
        config.value(QStringLiteral("modelMaterial")).toObject();
    const bool whiteSelected =
        material == SingleMaterialReliefMaterial::White;
    return modelMaterial.value(QStringLiteral("materialChannel")).toString()
            == ChannelValue(material)
        && modelMaterial.value(QStringLiteral("applyMode")).toString()
            == QStringLiteral("solid_volume")
        && IsRgbEmpty(modelMaterial)
        && modelMaterial.value(QStringLiteral("whiteValue")).toInt(-1)
            == (whiteSelected ? 0 : 255)
        && modelMaterial.value(QStringLiteral("varnishValue")).toInt(-1)
            == (whiteSelected ? 255 : 0)
        && IsProcessProfileConsistent(
            config.value(QStringLiteral("materialProcessProfile"))
                .toObject(),
            material)
        && IsModelFillConsistent(config, material);
}

QJsonObject MakeWhiteProcess(const bool enabled)
{
    return QJsonObject{
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("mode"),
         enabled ? QStringLiteral("all_model")
                 : QStringLiteral("disabled")},
        {QStringLiteral("coverage"),
         enabled ? QStringLiteral("all_model")
                 : QStringLiteral("model_surface")},
        {QStringLiteral("value"), 0},
        {QStringLiteral("expandPx"), 0},
        {QStringLiteral("shrinkPx"), 0},
    };
}

QJsonObject MakeVarnishProcess(const bool enabled)
{
    return QJsonObject{
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("mode"),
         enabled ? QStringLiteral("all_model")
                 : QStringLiteral("disabled")},
        {QStringLiteral("topLayers"), 1},
        {QStringLiteral("value"), 0},
        {QStringLiteral("coverage"),
         enabled ? QStringLiteral("all_model")
                 : QStringLiteral("model_surface")},
    };
}

SingleMaterialReliefApplyResult FailureResult(
    const QJsonObject& config,
    const SingleMaterialReliefState& state,
    const SingleMaterialReliefErrorCode errorCode,
    const QString& issue)
{
    SingleMaterialReliefApplyResult result;
    result.config = config;
    result.state = state;
    result.state.valid = false;
    result.state.errorcode = errorCode;
    result.errorcode =
        SingleMaterialReliefResolver::ErrorCodeValue(errorCode);
    result.issues.push_back(issue);
    return result;
}

}  // namespace

SingleMaterialReliefState SingleMaterialReliefResolver::Read(
    const QJsonObject& config,
    const QString& profileId,
    const bool editable,
    const bool stale)
{
    SingleMaterialReliefState state;
    state.profileid = profileId;
    state.editable = editable;
    state.stale = stale;

    if (!IsSingleMaterialProfile(config, profileId))
    {
        SetFailure(
            state,
            SingleMaterialReliefErrorCode::UnsupportedProfile,
            QStringLiteral("当前 Profile 不是单材料浮雕生产 Profile。"));
        state.editable = false;
        return state;
    }

    const QString channel =
        config.value(QStringLiteral("modelMaterial"))
            .toObject()
            .value(QStringLiteral("materialChannel"))
            .toString();
    if (channel == QStringLiteral("W"))
    {
        state.requestedmaterial = SingleMaterialReliefMaterial::White;
    }
    else if (channel == QStringLiteral("V"))
    {
        state.requestedmaterial = SingleMaterialReliefMaterial::Varnish;
    }
    else
    {
        SetFailure(
            state,
            SingleMaterialReliefErrorCode::InvalidMaterial,
            QStringLiteral("单材料浮雕只能选择 W 白墨或 V 光油。"));
        return state;
    }
    state.effectivechannel = ChannelValue(state.requestedmaterial);

    if (!IsConfigConsistent(config, state.requestedmaterial))
    {
        SetFailure(
            state,
            SingleMaterialReliefErrorCode::ConfigConflict,
            QStringLiteral("单材料浮雕 W/V 配置字段组不一致。"));
        return state;
    }

    SetValid(state);
    return state;
}

SingleMaterialReliefState SingleMaterialReliefResolver::Update(
    const SingleMaterialReliefState& current,
    const SingleMaterialReliefMaterial requestedMaterial)
{
    SingleMaterialReliefState state = current;
    if (!current.valid)
    {
        return state;
    }
    state.requestedmaterial = requestedMaterial;
    state.effectivechannel = ChannelValue(requestedMaterial);
    state.stale = current.stale
        || current.requestedmaterial != requestedMaterial;
    SetValid(state);
    return state;
}

SingleMaterialReliefApplyResult SingleMaterialReliefResolver::Apply(
    const QJsonObject& config,
    const SingleMaterialReliefState& state)
{
    if (!state.valid)
    {
        return FailureResult(
            config,
            state,
            state.errorcode,
            state.issues.isEmpty()
                ? QStringLiteral("单材料浮雕生产设置无效。")
                : state.issues.front());
    }
    if (!state.editable)
    {
        return FailureResult(
            config,
            state,
            SingleMaterialReliefErrorCode::ProfileLocked,
            state.lockreason.isEmpty()
                ? QStringLiteral("当前单材料浮雕 Profile 已锁定。")
                : state.lockreason);
    }
    if (!IsSingleMaterialProfile(config, state.profileid))
    {
        return FailureResult(
            config,
            state,
            SingleMaterialReliefErrorCode::UnsupportedProfile,
            QStringLiteral("当前 Profile 不是单材料浮雕生产 Profile。"));
    }

    const bool whiteSelected =
        state.requestedmaterial == SingleMaterialReliefMaterial::White;
    QJsonObject root = config;
    root.insert(
        QStringLiteral("modelMaterial"),
        QJsonObject{
            {QStringLiteral("materialChannel"),
             ChannelValue(state.requestedmaterial)},
            {QStringLiteral("applyMode"), QStringLiteral("solid_volume")},
            {QStringLiteral("rgb"), QJsonArray{255, 255, 255}},
            {QStringLiteral("whiteValue"), whiteSelected ? 0 : 255},
            {QStringLiteral("varnishValue"), whiteSelected ? 255 : 0},
        });

    QJsonObject modelFill =
        root.value(QStringLiteral("modelFill")).toObject();
    modelFill.insert(QStringLiteral("enabled"), true);
    modelFill.insert(
        QStringLiteral("material"),
        MaterialValue(state.requestedmaterial));
    modelFill.insert(QStringLiteral("scope"), QStringLiteral("all_model"));
    modelFill.insert(QStringLiteral("value"), 0);
    modelFill.insert(QStringLiteral("emptyAllowedInProduction"), false);
    modelFill.insert(QStringLiteral("legacyRgbFallback"), false);
    root.insert(QStringLiteral("modelFill"), modelFill);

    QJsonObject process =
        root.value(QStringLiteral("materialProcessProfile")).toObject();
    process.insert(QStringLiteral("enabled"), true);
    process.insert(
        QStringLiteral("name"),
        QString::fromLatin1(kSingleMaterialProfileId));
    process.insert(
        QStringLiteral("target"),
        QString::fromLatin1(kSingleMaterialProfileId));
    process.insert(
        QStringLiteral("rgb"),
        QJsonObject{
            {QStringLiteral("enabled"), false},
            {QStringLiteral("source"), QStringLiteral("modelMaterial")},
        });
    process.insert(QStringLiteral("white"), MakeWhiteProcess(whiteSelected));
    process.insert(
        QStringLiteral("varnish"),
        MakeVarnishProcess(!whiteSelected));
    process.insert(
        QStringLiteral("support"),
        QJsonObject{
            {QStringLiteral("expected"),
             root.value(QStringLiteral("support"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(false)},
            {QStringLiteral("mode"),
             QStringLiteral("existing_support_pipeline")},
        });
    process.insert(
        QStringLiteral("validation"),
        QJsonObject{
            {QStringLiteral("requireRgbPixels"), false},
            {QStringLiteral("requireWhitePixels"), whiteSelected},
            {QStringLiteral("requireVarnishPixels"), !whiteSelected},
            {QStringLiteral("requireSupportPixels"),
             root.value(QStringLiteral("support"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(false)},
            {QStringLiteral("maxUnexpectedOverlapPixels"), 0},
        });
    root.insert(QStringLiteral("materialProcessProfile"), process);

    QJsonObject preview =
        root.value(QStringLiteral("preview")).toObject();
    preview.insert(
        QStringLiteral("channels"),
        QJsonArray{
            whiteSelected ? QStringLiteral("white")
                          : QStringLiteral("varnish"),
            QStringLiteral("support"),
        });
    root.insert(QStringLiteral("preview"), preview);

    if (!IsConfigConsistent(root, state.requestedmaterial))
    {
        return FailureResult(
            config,
            state,
            SingleMaterialReliefErrorCode::ConfigConflict,
            QStringLiteral("生成的单材料浮雕字段组未通过一致性复核。"));
    }

    SingleMaterialReliefApplyResult result;
    result.applied = true;
    result.config = root;
    result.state = state;
    return result;
}

QString SingleMaterialReliefResolver::ErrorCodeValue(
    const SingleMaterialReliefErrorCode errorCode)
{
    switch (errorCode)
    {
    case SingleMaterialReliefErrorCode::None:
        return QString{};
    case SingleMaterialReliefErrorCode::UnsupportedProfile:
        return QStringLiteral(
            "E_SINGLE_MATERIAL_RELIEF_UNSUPPORTED_PROFILE");
    case SingleMaterialReliefErrorCode::InvalidMaterial:
        return QStringLiteral(
            "E_SINGLE_MATERIAL_RELIEF_INVALID_MATERIAL");
    case SingleMaterialReliefErrorCode::ConfigConflict:
        return QStringLiteral(
            "E_SINGLE_MATERIAL_RELIEF_CONFIG_CONFLICT");
    case SingleMaterialReliefErrorCode::ProfileLocked:
        return QStringLiteral(
            "E_SINGLE_MATERIAL_RELIEF_PROFILE_LOCKED");
    }
    return QStringLiteral("E_SINGLE_MATERIAL_RELIEF_UNKNOWN");
}
