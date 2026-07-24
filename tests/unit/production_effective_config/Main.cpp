#include "EffectiveConfigGenerator.h"

#include <QFile>
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

}  // namespace

int main()
{
    const bool passed = TestRestrictedProfileClearsStaleOverrides()
        && TestMaterialParityRestoresProfileContract()
        && TestLegacyPreservesExistingProfile()
        && TestUnknownGlobalProfileFailsBeforeReplacingSessionFile()
        && TestGlobalProfileModeMismatchFailsClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS production_effective_config_unit_tests\n";
    return 0;
}
