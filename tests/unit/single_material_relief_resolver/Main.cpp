#include "SingleMaterialReliefResolver.h"

#include <QJsonArray>
#include <QJsonObject>

#include <iostream>

namespace
{

bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

QJsonObject WhiteReliefConfig()
{
    return QJsonObject{
        {QStringLiteral("slicingMode"),
         QStringLiteral("relief_heightfield")},
        {QStringLiteral("modelMaterial"),
         QJsonObject{
             {QStringLiteral("materialChannel"), QStringLiteral("W")},
             {QStringLiteral("applyMode"), QStringLiteral("solid_volume")},
             {QStringLiteral("rgb"), QJsonArray{255, 255, 255}},
             {QStringLiteral("whiteValue"), 0},
             {QStringLiteral("varnishValue"), 255}}},
        {QStringLiteral("materialProcessProfile"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("name"),
              QStringLiteral("single_material_relief")},
             {QStringLiteral("target"),
              QStringLiteral("single_material_relief")},
             {QStringLiteral("rgb"),
              QJsonObject{
                  {QStringLiteral("enabled"), false},
                  {QStringLiteral("source"),
                   QStringLiteral("modelMaterial")}}},
             {QStringLiteral("white"),
              QJsonObject{
                  {QStringLiteral("enabled"), true},
                  {QStringLiteral("mode"), QStringLiteral("all_model")},
                  {QStringLiteral("coverage"),
                   QStringLiteral("all_model")},
                  {QStringLiteral("value"), 0}}},
             {QStringLiteral("varnish"),
              QJsonObject{
                  {QStringLiteral("enabled"), false},
                  {QStringLiteral("mode"), QStringLiteral("disabled")},
                  {QStringLiteral("topLayers"), 1},
                  {QStringLiteral("coverage"),
                   QStringLiteral("model_surface")},
                  {QStringLiteral("value"), 0}}},
             {QStringLiteral("support"),
              QJsonObject{
                  {QStringLiteral("expected"), true},
                  {QStringLiteral("mode"),
                   QStringLiteral("existing_support_pipeline")}}},
             {QStringLiteral("validation"),
              QJsonObject{
                  {QStringLiteral("requireRgbPixels"), false},
                  {QStringLiteral("requireWhitePixels"), true},
                  {QStringLiteral("requireVarnishPixels"), false},
                  {QStringLiteral("requireSupportPixels"), true},
                  {QStringLiteral("maxUnexpectedOverlapPixels"), 0}}}}},
        {QStringLiteral("support"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("mode"),
              QStringLiteral("bottom_projection")},
             {QStringLiteral("sentinel"),
              QStringLiteral("preserve-support")}}},
        {QStringLiteral("preview"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("interval"), 10},
             {QStringLiteral("channels"),
              QJsonArray{QStringLiteral("white"),
                         QStringLiteral("support")}}}},
    };
}

bool TestReadsWhiteConfiguration()
{
    const SingleMaterialReliefState state =
        SingleMaterialReliefResolver::Read(
            WhiteReliefConfig(),
            QStringLiteral("single_material_relief"),
            true,
            false);

    return ExpectTrue(state.valid, "white configuration is valid")
        && ExpectTrue(state.editable, "white configuration is editable")
        && ExpectTrue(
            state.requestedmaterial
                == SingleMaterialReliefMaterial::White,
            "white material detected")
        && ExpectTrue(
            state.effectivechannel == QStringLiteral("W"),
            "white effective channel is W");
}

bool TestSwitchesToVarnishAtomicallyAndPreservesSupport()
{
    const QJsonObject original = WhiteReliefConfig();
    const SingleMaterialReliefState current =
        SingleMaterialReliefResolver::Read(
            original,
            QStringLiteral("single_material_relief"),
            true,
            false);
    const SingleMaterialReliefState updated =
        SingleMaterialReliefResolver::Update(
            current,
            SingleMaterialReliefMaterial::Varnish);
    const SingleMaterialReliefApplyResult result =
        SingleMaterialReliefResolver::Apply(original, updated);

    const QJsonObject modelMaterial =
        result.config.value(QStringLiteral("modelMaterial")).toObject();
    const QJsonObject modelFill =
        result.config.value(QStringLiteral("modelFill")).toObject();
    const QJsonObject process =
        result.config.value(QStringLiteral("materialProcessProfile"))
            .toObject();
    const QJsonObject validation =
        process.value(QStringLiteral("validation")).toObject();
    const QJsonArray previewChannels =
        result.config.value(QStringLiteral("preview"))
            .toObject()
            .value(QStringLiteral("channels"))
            .toArray();

    return ExpectTrue(result.applied, "varnish switch applied")
        && ExpectTrue(updated.stale, "material switch marks output stale")
        && ExpectTrue(
            modelMaterial.value(QStringLiteral("materialChannel")).toString()
                == QStringLiteral("V"),
            "model material channel switched to V")
        && ExpectTrue(
            modelMaterial.value(QStringLiteral("whiteValue")).toInt()
                    == 255
                && modelMaterial.value(QStringLiteral("varnishValue")).toInt()
                    == 0,
            "W and V production values switched atomically")
        && ExpectTrue(
            modelFill.value(QStringLiteral("material")).toString()
                == QStringLiteral("varnish"),
            "compatibility model fill summary switched")
        && ExpectTrue(
            !process.value(QStringLiteral("white"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(true)
                && process.value(QStringLiteral("varnish"))
                       .toObject()
                       .value(QStringLiteral("enabled"))
                       .toBool(false),
            "process W and V toggles are mutually exclusive")
        && ExpectTrue(
            !validation.value(QStringLiteral("requireWhitePixels"))
                 .toBool(true)
                && validation.value(QStringLiteral("requireVarnishPixels"))
                       .toBool(false),
            "validation follows effective material")
        && ExpectTrue(
            previewChannels
                == QJsonArray{QStringLiteral("varnish"),
                              QStringLiteral("support")},
            "preview follows V and S channels")
        && ExpectTrue(
            result.config.value(QStringLiteral("support"))
                    .toObject()
                    .value(QStringLiteral("sentinel"))
                    .toString()
                == QStringLiteral("preserve-support"),
            "support configuration is preserved");
}

bool TestRejectsConflictingMaterialFields()
{
    QJsonObject config = WhiteReliefConfig();
    QJsonObject process =
        config.value(QStringLiteral("materialProcessProfile")).toObject();
    QJsonObject varnish =
        process.value(QStringLiteral("varnish")).toObject();
    varnish.insert(QStringLiteral("enabled"), true);
    varnish.insert(QStringLiteral("mode"), QStringLiteral("all_model"));
    process.insert(QStringLiteral("varnish"), varnish);
    config.insert(QStringLiteral("materialProcessProfile"), process);

    const SingleMaterialReliefState state =
        SingleMaterialReliefResolver::Read(
            config,
            QStringLiteral("single_material_relief"),
            true,
            false);
    const SingleMaterialReliefApplyResult result =
        SingleMaterialReliefResolver::Apply(config, state);

    return ExpectTrue(!state.valid, "conflicting configuration rejected")
        && ExpectTrue(!result.applied, "conflict not applied")
        && ExpectTrue(result.config == config, "conflict preserves source JSON")
        && ExpectTrue(
            result.errorcode
                == QStringLiteral("E_SINGLE_MATERIAL_RELIEF_CONFIG_CONFLICT"),
            "conflict error code is stable");
}

bool TestRejectsUnsupportedProfile()
{
    const QJsonObject original = WhiteReliefConfig();
    const SingleMaterialReliefState state =
        SingleMaterialReliefResolver::Read(
            original,
            QStringLiteral("textured_nail_rgb_white_lower_support"),
            true,
            false);
    const SingleMaterialReliefApplyResult result =
        SingleMaterialReliefResolver::Apply(original, state);

    return ExpectTrue(!state.valid, "unsupported Profile rejected")
        && ExpectTrue(!result.applied, "unsupported Profile not applied")
        && ExpectTrue(result.config == original, "unsupported preserves source")
        && ExpectTrue(
            result.errorcode
                == QStringLiteral("E_SINGLE_MATERIAL_RELIEF_UNSUPPORTED_PROFILE"),
            "unsupported Profile error code is stable");
}

bool TestLockedStateFailsClosed()
{
    const QJsonObject original = WhiteReliefConfig();
    const SingleMaterialReliefState state =
        SingleMaterialReliefResolver::Read(
            original,
            QStringLiteral("single_material_relief"),
            false,
            false);
    const SingleMaterialReliefApplyResult result =
        SingleMaterialReliefResolver::Apply(original, state);

    return ExpectTrue(!result.applied, "locked Profile not applied")
        && ExpectTrue(result.config == original, "locked Profile preserves source")
        && ExpectTrue(
            result.errorcode
                == QStringLiteral("E_SINGLE_MATERIAL_RELIEF_PROFILE_LOCKED"),
            "locked Profile error code is stable");
}

}  // namespace

int main()
{
    const bool passed = TestReadsWhiteConfiguration()
        && TestSwitchesToVarnishAtomicallyAndPreservesSupport()
        && TestRejectsConflictingMaterialFields()
        && TestRejectsUnsupportedProfile()
        && TestLockedStateFailsClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS single_material_relief_resolver_unit_tests\n";
    return 0;
}
