#include "ConfigValidator.h"
#include "EffectiveConfigGenerator.h"
#include "ProductionTextureSettingsModel.h"
#include "SingleMaterialReliefResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

QByteArray ReadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

QJsonDocument ReadJson(const QString& path)
{
    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(ReadFile(path), &error);
    return error.error == QJsonParseError::NoError
        ? document
        : QJsonDocument{};
}

QString FixturePath(const QString& fileName)
{
    return QStringLiteral(SLICESOFT_SOURCE_DIR)
        + QStringLiteral("/samples/configs/texture_fill_partition/")
        + fileName;
}

SliceSettingsState BuildSettings(
    const QString& outputDirectory,
    const bool supportEnabled,
    const bool varnishEnabled)
{
    SliceSettingsState settings;
    settings.profileid = QStringLiteral("ui_test_profile");
    settings.modelpath = QStringLiteral("model/test.obj");
    settings.outputdirectory = outputDirectory;
    settings.dpix = 635;
    settings.dpiy = 600;
    settings.layerthicknessmm = 0.2;
    settings.modelfillmaterial = ModelFillMaterial::White;
    settings.support.enabled = supportEnabled;
    settings.support.placement = SupportPlacement::Both;
    settings.support.internalvoidenabled = supportEnabled;
    settings.support.internalvoidminareapx = 7;
    settings.surfacevarnish.enabled = varnishEnabled;
    settings.surfacevarnish.thicknesspx = varnishEnabled ? 3 : 0;
    settings.outervarnish.enabled = varnishEnabled;
    settings.outervarnish.thicknessmm = varnishEnabled ? 0.12 : 0.0;
    settings.preview.outputpolicy =
        QStringLiteral("tiff_native_with_diagnostics");
    settings.preview.enabled = true;
    settings.preview.interval = 5;
    return settings;
}

EffectiveConfigRequest BuildGlobalRequest(
    const QJsonDocument& original,
    const QJsonDocument& overrideDocument,
    const QString& templatePath,
    const QString& generatedPath,
    const QString& profileId,
    const SliceSettingsState& settings)
{
    EffectiveConfigRequest request;
    request.profileid = QStringLiteral("source_fixture_profile");
    request.templatepath = templatePath;
    request.generatedconfigpath = generatedPath;
    request.originaldocument = original;
    request.overridedocument = overrideDocument;
    request.settings = settings;
    request.production.requestedmode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    request.production.requestedprofileid = profileId;
    request.production.sourceprofileid =
        QStringLiteral("source_fixture_profile");
    request.production.sessionid = QStringLiteral("session_09b_02");
    request.production.generatedatutc =
        QStringLiteral("2026-07-24T09:30:00.000Z");
    return request;
}

bool AuditContainsDisabledPath(
    const QJsonObject& root,
    const QString& path)
{
    const QJsonArray disabled = root.value(QStringLiteral("uiAudit"))
                                    .toObject()
                                    .value(QStringLiteral("production"))
                                    .toObject()
                                    .value(QStringLiteral("disabledOverrides"))
                                    .toArray();
    return disabled.contains(path);
}

bool TestRestrictedProfileClearsStaleOverrides()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "restricted temp directory"))
    {
        return false;
    }

    const QString templatePath = FixturePath(
        QStringLiteral("global_production_xiao_ma_white_fill.json"));
    const QByteArray templateBefore = ReadFile(templatePath);
    const QJsonDocument original =
        QJsonDocument::fromJson(templateBefore);
    QJsonObject overrideRoot = original.object();

    QJsonObject support = overrideRoot.value(QStringLiteral("support")).toObject();
    support.insert(QStringLiteral("enabled"), true);
    support.insert(QStringLiteral("placement"), QStringLiteral("both"));
    overrideRoot.insert(QStringLiteral("support"), support);

    QJsonObject surface =
        overrideRoot.value(QStringLiteral("surfaceVarnish")).toObject();
    surface.insert(QStringLiteral("enabled"), true);
    surface.insert(QStringLiteral("thicknessPx"), 4);
    overrideRoot.insert(QStringLiteral("surfaceVarnish"), surface);

    QJsonObject outer =
        overrideRoot.value(QStringLiteral("outerVarnish")).toObject();
    outer.insert(QStringLiteral("enabled"), true);
    outer.insert(QStringLiteral("thicknessMm"), 0.20);
    overrideRoot.insert(QStringLiteral("outerVarnish"), outer);

    const QString generatedPath =
        directory.filePath(QStringLiteral("slice_config.effective.json"));
    const EffectiveConfigRequest request = BuildGlobalRequest(
        original,
        QJsonDocument(overrideRoot),
        templatePath,
        generatedPath,
        QStringLiteral("global_surface_shell_restricted_candidate"),
        BuildSettings(directory.filePath(QStringLiteral("package")), true, true));

    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    if (!ExpectTrue(result.IsValid(), "restricted config generated"))
    {
        for (const QString& error : result.errors)
        {
            std::cerr << error.toStdString() << '\n';
        }
        return false;
    }

    const QJsonObject root = result.document.object();
    const QJsonObject output = root.value(QStringLiteral("output")).toObject();
    const QJsonObject preview =
        root.value(QStringLiteral("preview")).toObject();
    const QJsonObject productionAudit =
        root.value(QStringLiteral("uiAudit"))
            .toObject()
            .value(QStringLiteral("production"))
            .toObject();
    return ExpectTrue(
               root.value(QStringLiteral("slicePipeline"))
                       .toObject()
                       .value(QStringLiteral("mode"))
                       .toString()
                   == QStringLiteral("global_surface_shell"),
               "restricted effective mode")
        && ExpectTrue(
            root.value(QStringLiteral("materialProcessProfile"))
                    .toObject()
                    .value(QStringLiteral("target"))
                    .toString()
                == QStringLiteral(
                    "global_surface_shell_restricted_candidate"),
            "restricted effective Profile")
        && ExpectTrue(
            output.value(QStringLiteral("dpiX")).toInt() == 635
                && output.value(QStringLiteral("dpiY")).toInt() == 600,
            "restricted effective config preserves current X/Y DPI")
        && ExpectTrue(
            !root.value(QStringLiteral("support"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(true),
            "restricted support override cleared")
        && ExpectTrue(
            !root.value(QStringLiteral("surfaceVarnish"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(true),
            "restricted surface varnish override cleared")
        && ExpectTrue(
            !root.value(QStringLiteral("outerVarnish"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(true),
            "restricted outer varnish override cleared")
        && ExpectTrue(
            productionAudit.value(QStringLiteral("requestedPipelineMode"))
                    .toString()
                == QStringLiteral("global_surface_shell"),
            "requested mode audited")
        && ExpectTrue(
            productionAudit.value(QStringLiteral("effectivePipelineMode"))
                    .toString()
                == QStringLiteral("global_surface_shell"),
            "effective mode audited")
        && ExpectTrue(
            productionAudit.value(QStringLiteral("capabilityLockVersion"))
                    .toString()
                == QStringLiteral(
                    "slicesoft.ui.production_capability.12e_09b.1"),
            "capability lock audited")
        && ExpectTrue(
            AuditContainsDisabledPath(root, QStringLiteral("support.enabled")),
            "support stale override audited")
        && ExpectTrue(
            AuditContainsDisabledPath(
                root,
                QStringLiteral("surfaceVarnish.enabled")),
            "surface varnish stale override audited")
        && ExpectTrue(
            ReadJson(generatedPath) == result.document,
            "restricted atomic output is complete JSON")
        && ExpectTrue(
            preview.value(QStringLiteral("outputPolicy")).toString()
                == QStringLiteral("tiff_native_with_diagnostics")
                && preview.value(QStringLiteral("enabled")).toBool(),
            "effective config writes consistent preview policy")
        && ExpectTrue(
            ReadFile(templatePath) == templateBefore,
            "restricted source fixture remains read-only")
        && ExpectTrue(
            !result.differences.isEmpty(),
            "restricted effective diff is auditable")
        && ExpectTrue(
            result.warnings.join(QStringLiteral(" "))
                .contains(QStringLiteral("stale override")),
            "restricted stale override warning is visible");
}

bool TestMaterialParityRestoresProfileContract()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "parity temp directory"))
    {
        return false;
    }

    const QString templatePath = FixturePath(
        QStringLiteral("global_production_xiao_ma_material_parity.json"));
    const QJsonDocument original = ReadJson(templatePath);
    QJsonObject overrideRoot = original.object();
    QJsonObject support = overrideRoot.value(QStringLiteral("support")).toObject();
    support.insert(QStringLiteral("enabled"), false);
    support.insert(
        QStringLiteral("placement"),
        QStringLiteral("full_vertical_projection"));
    overrideRoot.insert(QStringLiteral("support"), support);
    QJsonObject outer =
        overrideRoot.value(QStringLiteral("outerVarnish")).toObject();
    outer.insert(QStringLiteral("enabled"), false);
    outer.insert(QStringLiteral("thicknessMm"), 0.0);
    overrideRoot.insert(QStringLiteral("outerVarnish"), outer);

    const EffectiveConfigRequest request = BuildGlobalRequest(
        original,
        QJsonDocument(overrideRoot),
        templatePath,
        directory.filePath(QStringLiteral("slice_config.effective.json")),
        QStringLiteral("global_surface_shell_material_parity_candidate"),
        BuildSettings(directory.filePath(QStringLiteral("package")), false, false));
    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    if (!ExpectTrue(result.IsValid(), "material parity config generated"))
    {
        return false;
    }

    const QJsonObject root = result.document.object();
    const QJsonObject effectiveSupport =
        root.value(QStringLiteral("support")).toObject();
    const QJsonObject effectiveOuter =
        root.value(QStringLiteral("outerVarnish")).toObject();
    return ExpectTrue(
               effectiveSupport.value(QStringLiteral("enabled")).toBool(false),
               "parity lower support restored")
        && ExpectTrue(
            effectiveSupport.value(QStringLiteral("placement")).toString()
                == QStringLiteral("lower"),
            "parity support placement restored")
        && ExpectTrue(
            effectiveSupport.value(QStringLiteral("internalVoid"))
                .toObject()
                .value(QStringLiteral("enabled"))
                .toBool(false),
            "parity internal void support restored")
        && ExpectTrue(
            effectiveOuter.value(QStringLiteral("enabled")).toBool(false),
            "parity outer varnish restored")
        && ExpectTrue(
            effectiveOuter.value(QStringLiteral("thicknessMm")).toDouble()
                > 0.0,
            "parity outer varnish thickness restored")
        && ExpectTrue(
            AuditContainsDisabledPath(root, QStringLiteral("support.enabled")),
            "parity support stale override audited")
        && ExpectTrue(
            AuditContainsDisabledPath(
                root,
                QStringLiteral("outerVarnish.enabled")),
            "parity varnish stale override audited");
}

bool TestLegacyPreservesExistingProfile()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "legacy temp directory"))
    {
        return false;
    }

    QJsonObject root{
        {QStringLiteral("input"),
         QJsonObject{{QStringLiteral("modelPath"), QStringLiteral("old.obj")}}},
        {QStringLiteral("output"),
         QJsonObject{
             {QStringLiteral("packageDir"), QStringLiteral("old_package")},
             {QStringLiteral("bitDepth"), 8},
             {QStringLiteral("channelOrder"),
              QJsonArray{
                  QStringLiteral("R"),
                  QStringLiteral("G"),
                  QStringLiteral("B"),
                  QStringLiteral("W"),
                  QStringLiteral("S"),
                  QStringLiteral("V")}}}},
        {QStringLiteral("background"),
         QJsonObject{{QStringLiteral("value"), 255}}},
        {QStringLiteral("materialProcessProfile"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("target"), QStringLiteral("legacy_profile")}}},
        {QStringLiteral("support"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("mode"), QStringLiteral("bottom_projection")},
             {QStringLiteral("placement"), QStringLiteral("lower")}}},
    };
    const QJsonDocument original(root);
    EffectiveConfigRequest request;
    request.profileid = QStringLiteral("legacy_profile_source");
    request.templatepath = directory.filePath(QStringLiteral("template.json"));
    request.generatedconfigpath =
        directory.filePath(QStringLiteral("slice_config.effective.json"));
    request.originaldocument = original;
    request.overridedocument = original;
    request.settings =
        BuildSettings(directory.filePath(QStringLiteral("package")), true, false);
    request.production.requestedmode =
        slicer_core::SlicePipelineMode::Legacy;
    request.production.sourceprofileid =
        QStringLiteral("legacy_profile_source");
    request.production.sessionid = QStringLiteral("legacy_session");
    request.production.generatedatutc =
        QStringLiteral("2026-07-24T09:30:00.000Z");

    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    if (!ExpectTrue(result.IsValid(), "legacy config generated"))
    {
        return false;
    }
    const QJsonObject effectiveRoot = result.document.object();
    const QJsonObject effectiveOutput =
        effectiveRoot.value(QStringLiteral("output")).toObject();
    const QJsonObject effectiveBaseProjection =
        effectiveRoot.value(QStringLiteral("support"))
            .toObject()
            .value(QStringLiteral("baseProjection"))
            .toObject();
    const QJsonObject audit = effectiveRoot.value(QStringLiteral("uiAudit"))
                                  .toObject()
                                  .value(QStringLiteral("production"))
                                  .toObject();
    return ExpectTrue(
               effectiveRoot.value(QStringLiteral("slicePipeline"))
                       .toObject()
                       .value(QStringLiteral("mode"))
                       .toString()
                   == QStringLiteral("legacy"),
               "legacy mode is explicit")
        && ExpectTrue(
            effectiveOutput.value(QStringLiteral("dpiX")).toInt() == 635
                && effectiveOutput.value(QStringLiteral("dpiY")).toInt() == 600,
            "legacy effective config uses current X/Y DPI")
        && ExpectTrue(
            effectiveBaseProjection.value(QStringLiteral("enabled"))
                    .toBool(false)
                && effectiveBaseProjection
                       .value(QStringLiteral("layerCount"))
                       .toInt()
                    == 30
                && effectiveBaseProjection
                       .value(QStringLiteral("source"))
                       .toString()
                    == QStringLiteral("max_support_footprint")
                && effectiveBaseProjection
                       .value(QStringLiteral("layerPlacement"))
                       .toString()
                    == QStringLiteral("prepend_below_model"),
            "legacy production settings prepend the 30-layer support base")
        && ExpectTrue(
            effectiveRoot.value(QStringLiteral("materialProcessProfile"))
                    .toObject()
                    .value(QStringLiteral("target"))
                    .toString()
                == QStringLiteral("legacy_profile"),
            "legacy Profile target is preserved")
        && ExpectTrue(
            audit.value(QStringLiteral("requestedProductionProfileId"))
                    .toString()
                == QStringLiteral("legacy_profile"),
            "legacy requested Profile audited")
        && ExpectTrue(
            audit.value(QStringLiteral("effectiveProductionProfileId"))
                    .toString()
                == QStringLiteral("legacy_profile"),
            "legacy effective Profile audited")
        && ExpectTrue(
            audit.value(QStringLiteral("disabledOverrides"))
                .toArray()
                .isEmpty(),
            "legacy does not clear Profile-owned overrides");
}

bool TestUnknownGlobalProfileFailsBeforeReplacingSessionFile()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "negative temp directory"))
    {
        return false;
    }

    const QString templatePath = FixturePath(
        QStringLiteral("global_production_xiao_ma_white_fill.json"));
    const QJsonDocument original = ReadJson(templatePath);
    const QString generatedPath =
        directory.filePath(QStringLiteral("slice_config.effective.json"));
    QFile sentinel(generatedPath);
    if (!sentinel.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return ExpectTrue(false, "create negative sentinel");
    }
    const QByteArray sentinelBytes{"{\"sentinel\":true}\n"};
    sentinel.write(sentinelBytes);
    sentinel.close();
    const QByteArray sentinelBefore = ReadFile(generatedPath);

    EffectiveConfigRequest request = BuildGlobalRequest(
        original,
        original,
        templatePath,
        generatedPath,
        QStringLiteral("unknown_global_profile"),
        BuildSettings(directory.filePath(QStringLiteral("package")), true, true));
    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    return ExpectTrue(!result.IsValid(), "unknown Global Profile rejected")
        && ExpectTrue(!result.errors.isEmpty(), "unknown Profile error reported")
        && ExpectTrue(
            ReadFile(generatedPath) == sentinelBefore,
            "negative validation does not replace session config");
}

bool TestGlobalProfileModeMismatchFailsClosed()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "mismatch temp directory"))
    {
        return false;
    }

    const QString templatePath = FixturePath(
        QStringLiteral("global_production_xiao_ma_white_fill.json"));
    QJsonObject mismatchedRoot = ReadJson(templatePath).object();
    QJsonObject profile =
        mismatchedRoot.value(QStringLiteral("materialProcessProfile")).toObject();
    profile.insert(
        QStringLiteral("target"),
        QStringLiteral("global_surface_shell_material_parity_candidate"));
    mismatchedRoot.insert(QStringLiteral("materialProcessProfile"), profile);
    const QJsonDocument mismatched(mismatchedRoot);
    const EffectiveConfigRequest request = BuildGlobalRequest(
        mismatched,
        mismatched,
        templatePath,
        directory.filePath(QStringLiteral("slice_config.effective.json")),
        QStringLiteral("global_surface_shell_restricted_candidate"),
        BuildSettings(directory.filePath(QStringLiteral("package")), true, true));

    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    return ExpectTrue(!result.IsValid(), "mode/Profile mismatch rejected")
        && ExpectTrue(
            !QFile::exists(request.generatedconfigpath),
            "mismatch does not write session config");
}

QString ReliefFixturePath(const QString& fileName)
{
    return QStringLiteral(SLICESOFT_SOURCE_DIR)
        + QStringLiteral("/samples/configs/relief/")
        + fileName;
}

QString MaterialProcessFixturePath(const QString& fileName)
{
    return QStringLiteral(SLICESOFT_SOURCE_DIR)
        + QStringLiteral("/samples/configs/material_process/")
        + fileName;
}

bool TestProductionTextureOverrideReachesSessionConfig()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "09D texture temp directory"))
    {
        return false;
    }

    const QString templatePath = FixturePath(
        QStringLiteral("global_production_xiao_ma_white_fill.json"));
    const QJsonDocument original = ReadJson(templatePath);
    SliceSettingsState settings = BuildSettings(
        directory.filePath(QStringLiteral("package")),
        false,
        false);
    settings.productiontextureoverrideenabled = true;
    settings.productiontexture = ProductionTextureSettingsModel::UpdateGlobal(
        ProductionTextureSettingsModel::Read(
            original.object(),
            true,
            true,
            false),
        0.35,
        ProductionTexturePartitionMode::AllTexture);

    const EffectiveConfigRequest request = BuildGlobalRequest(
        original,
        original,
        templatePath,
        directory.filePath(QStringLiteral("slice_config.effective.json")),
        QStringLiteral("global_surface_shell_restricted_candidate"),
        settings);
    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    if (!ExpectTrue(result.IsValid(), "09D Global texture config generated"))
    {
        return false;
    }

    const QJsonObject root = result.document.object();
    const QJsonObject surfaceShell =
        root.value(QStringLiteral("texture"))
            .toObject()
            .value(QStringLiteral("surfaceShell"))
            .toObject();
    const QJsonObject textureAudit =
        root.value(QStringLiteral("uiAudit"))
            .toObject()
            .value(QStringLiteral("production"))
            .toObject()
            .value(QStringLiteral("texture"))
            .toObject();
    return ExpectTrue(
               std::abs(surfaceShell.value(QStringLiteral("widthMm")).toDouble()
                        - 0.35)
                   < 1.0e-9,
               "09D requested Global width reaches effective config")
        && ExpectTrue(
            surfaceShell.value(QStringLiteral("mode")).toString()
                == QStringLiteral("all_texture"),
            "09D explicit mode reaches effective config")
        && ExpectTrue(
            textureAudit.value(QStringLiteral("strategy")).toString()
                == QStringLiteral("global_surface_shell"),
            "09D strategy is audited")
        && ExpectTrue(
            std::abs(textureAudit.value(QStringLiteral("effectiveWidthMm")).toDouble()
                        - 0.35)
                < 1.0e-9,
            "09D effective width is audited")
        && ExpectTrue(
            ReadJson(request.generatedconfigpath) == result.document,
            "09D session config persists the production texture state")
        && ExpectTrue(
            result.summary.contains(
                QStringLiteral(
                    "生产纹理：global_surface_shell / all_texture")),
            "09D Global state reaches the run summary");
}

bool TestSingleMaterialReliefOverrideReachesSessionConfig()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "09D relief temp directory"))
    {
        return false;
    }

    const QString templatePath = ReliefFixturePath(
        QStringLiteral("relief_nail_white_support.json"));
    const QJsonDocument original = ReadJson(templatePath);
    SliceSettingsState settings = BuildSettings(
        directory.filePath(QStringLiteral("package")),
        true,
        false);
    settings.profileid = QStringLiteral("single_material_relief");
    settings.singlematerialreliefoverrideenabled = true;
    settings.singlematerialrelief = SingleMaterialReliefResolver::Update(
        SingleMaterialReliefResolver::Read(
            original.object(),
            settings.profileid,
            true,
            false),
        SingleMaterialReliefMaterial::Varnish);

    EffectiveConfigRequest request;
    request.profileid = settings.profileid;
    request.templatepath = templatePath;
    request.generatedconfigpath =
        directory.filePath(QStringLiteral("slice_config.effective.json"));
    request.originaldocument = original;
    request.overridedocument = original;
    request.settings = settings;
    request.production.requestedmode =
        slicer_core::SlicePipelineMode::Legacy;
    request.production.requestedprofileid = settings.profileid;
    request.production.sourceprofileid = settings.profileid;
    request.production.sessionid = QStringLiteral("session_09d_relief");
    request.production.generatedatutc =
        QStringLiteral("2026-08-03T12:00:00.000Z");

    const EffectiveConfigResult result =
        EffectiveConfigGenerator{}.Generate(request);
    if (!ExpectTrue(result.IsValid(), "09D relief config generated"))
    {
        for (const QString& error : result.errors)
        {
            std::cerr << error.toStdString() << '\n';
        }
        return false;
    }

    const QJsonObject root = result.document.object();
    const QJsonObject modelMaterial =
        root.value(QStringLiteral("modelMaterial")).toObject();
    const QJsonObject validation =
        root.value(QStringLiteral("materialProcessProfile"))
            .toObject()
            .value(QStringLiteral("validation"))
            .toObject();
    const QJsonObject materialAudit =
        root.value(QStringLiteral("uiAudit"))
            .toObject()
            .value(QStringLiteral("production"))
            .toObject()
            .value(QStringLiteral("singleMaterialRelief"))
            .toObject();
    return ExpectTrue(
               modelMaterial.value(QStringLiteral("materialChannel")).toString()
                   == QStringLiteral("V"),
               "09D V channel reaches effective config")
        && ExpectTrue(
            modelMaterial.value(QStringLiteral("whiteValue")).toInt() == 255
                && modelMaterial.value(QStringLiteral("varnishValue")).toInt()
                    == 0,
            "09D V values reach effective config")
        && ExpectTrue(
            !validation.value(QStringLiteral("requireWhitePixels"))
                 .toBool(true)
                && validation.value(QStringLiteral("requireVarnishPixels"))
                       .toBool(false),
            "09D material validation reaches effective config")
        && ExpectTrue(
            materialAudit.value(QStringLiteral("effectiveChannel")).toString()
                == QStringLiteral("V"),
            "09D material channel is audited")
        && ExpectTrue(
            materialAudit.value(QStringLiteral("stale")).toBool(false),
            "09D material stale state is audited")
        && ExpectTrue(
            ReadJson(request.generatedconfigpath) == result.document,
            "09D material session config is persisted")
        && ExpectTrue(
            result.summary.contains(
                QStringLiteral("单材料浮雕：varnish / channel=V")),
            "09D material state reaches the run summary");
}

bool TestSingleMaterialReliefConflictFailsConfigValidation()
{
    const QJsonDocument document = ReadJson(
        ReliefFixturePath(
            QStringLiteral("relief_nail_white_support.json")));
    QJsonObject root = document.object();
    QJsonObject process =
        root.value(QStringLiteral("materialProcessProfile")).toObject();
    QJsonObject varnish =
        process.value(QStringLiteral("varnish")).toObject();
    varnish.insert(QStringLiteral("enabled"), true);
    varnish.insert(QStringLiteral("mode"), QStringLiteral("all_model"));
    process.insert(QStringLiteral("varnish"), varnish);
    root.insert(QStringLiteral("materialProcessProfile"), process);

    const ConfigValidationResult validation =
        ConfigValidator::validate(root);
    return ExpectTrue(
               !validation.isValid(),
               "09D conflicting W/V fields fail config validation")
        && ExpectTrue(
            validation.errors.join(QStringLiteral("\n"))
                .contains(QStringLiteral(
                    "E_SINGLE_MATERIAL_RELIEF_CONFIG_CONFLICT")),
            "09D config validation exposes stable conflict code");
}

EffectiveConfigRequest BuildTextureWhiteWarningRequest(
    const QString& generatedPath,
    const QStringList& capabilities,
    const quint64 preflightRevision)
{
    const QString templatePath = MaterialProcessFixturePath(
        QStringLiteral("obj_mtl_texture_rgb_only.json"));
    const QJsonDocument original = ReadJson(templatePath);
    EffectiveConfigRequest request;
    request.profileid =
        QStringLiteral("textured_nail_rgb_only_lower_support");
    request.templatepath = templatePath;
    request.generatedconfigpath = generatedPath;
    request.originaldocument = original;
    request.overridedocument = original;
    request.settings = BuildSettings(
        QFileInfo(generatedPath).dir().filePath(
            QStringLiteral("package")),
        true,
        false);
    request.settings.modelfillmaterial = ModelFillMaterial::Rgb;
    request.production.requestedmode =
        slicer_core::SlicePipelineMode::Legacy;
    request.production.requestedprofileid = request.profileid;
    request.production.sourceprofileid = request.profileid;
    request.production.sessionid =
        QStringLiteral("stage15-white-warning-session");
    request.production.generatedatutc =
        QStringLiteral("2026-08-04T10:00:00.000Z");
    request.sceneid = QStringLiteral("stage15-white-warning-scene");
    request.scenerevision = 7U;
    request.scenecontenthash =
        QStringLiteral("stage15-white-warning-content");
    request.profilecapabilities = capabilities;

    TextureWhitePreflightResult preflight;
    preflight.sceneid = request.sceneid;
    preflight.scenerevision = preflightRevision;
    preflight.contenthash = request.scenecontenthash;
    preflight.profileid = request.profileid;
    preflight.containsstrictwhite = true;
    preflight.replacementprofileid = QStringLiteral(
        "textured_nail_rgb_white_ondemand_lower_support");
    preflight.replacementprofiledisplayname = QStringLiteral(
        "彩色纹理甲片 - 全实体 RGB + 按需补白 + 下表面支撑");
    request.texturewhitepreflight = preflight;
    return request;
}

bool TestTextureWhitePreflightWarningIsProfileAndSceneBound()
{
    QTemporaryDir directory;
    if (!ExpectTrue(directory.isValid(), "15C-02 temp directory"))
    {
        return false;
    }

    const EffectiveConfigRequest unsupported =
        BuildTextureWhiteWarningRequest(
            directory.filePath(
                QStringLiteral("unsupported.effective.json")),
            {QStringLiteral("rgb_full_volume_texture")},
            7U);
    const EffectiveConfigResult warned =
        EffectiveConfigGenerator{}.Generate(unsupported);
    const QString warnedText =
        warned.warnings.join(QStringLiteral("\n"));
    if (!ExpectTrue(warned.IsValid(), "15C-02 RGB config generated")
        || !ExpectTrue(
            warnedText.contains(unsupported.sceneid)
                && warnedText.contains(unsupported.profileid)
                && warnedText.contains(QStringLiteral("按需补白")),
            "15C-02 RGB path exposes scene/Profile-bound warning"))
    {
        return false;
    }

    const EffectiveConfigRequest capable =
        BuildTextureWhiteWarningRequest(
            directory.filePath(
                QStringLiteral("capable.effective.json")),
            {QStringLiteral("unprintable_white_underbase")},
            7U);
    const EffectiveConfigResult capableResult =
        EffectiveConfigGenerator{}.Generate(capable);
    if (!ExpectTrue(capableResult.IsValid(), "15C-02 capable config generated")
        || !ExpectTrue(
            !capableResult.warnings.join(QStringLiteral("\n"))
                 .contains(QStringLiteral("纹理纯白预检")),
            "15C-02 capable Profile suppresses warning"))
    {
        return false;
    }

    const EffectiveConfigRequest stale =
        BuildTextureWhiteWarningRequest(
            directory.filePath(
                QStringLiteral("stale.effective.json")),
            {QStringLiteral("rgb_full_volume_texture")},
            6U);
    const EffectiveConfigResult staleResult =
        EffectiveConfigGenerator{}.Generate(stale);
    return ExpectTrue(staleResult.IsValid(), "15C-02 stale config generated")
        && ExpectTrue(
            !staleResult.warnings.join(QStringLiteral("\n"))
                 .contains(QStringLiteral("纹理纯白预检")),
            "15C-02 stale scene identity is discarded");
}

}  // namespace

int main()
{
    const bool passed = TestRestrictedProfileClearsStaleOverrides()
        && TestMaterialParityRestoresProfileContract()
        && TestLegacyPreservesExistingProfile()
        && TestUnknownGlobalProfileFailsBeforeReplacingSessionFile()
        && TestGlobalProfileModeMismatchFailsClosed()
        && TestProductionTextureOverrideReachesSessionConfig()
        && TestSingleMaterialReliefOverrideReachesSessionConfig()
        && TestSingleMaterialReliefConflictFailsConfigValidation()
        && TestTextureWhitePreflightWarningIsProfileAndSceneBound();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS production_effective_config_unit_tests\n";
    return 0;
}
