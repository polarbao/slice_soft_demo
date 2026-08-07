#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::GeneratedEffectiveConfig(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempdir;
    if (!tempdir.isValid())
    {
        return fail("generated-effective-config 无法创建临时目录。");
    }

    const QJsonArray channelorder{
        QStringLiteral("R"),
        QStringLiteral("G"),
        QStringLiteral("B"),
        QStringLiteral("W"),
        QStringLiteral("S"),
        QStringLiteral("V"),
    };
    QJsonObject root{
        {"input", QJsonObject{{"modelPath", "template.obj"}, {"format", "auto"}}},
        {"output",
         QJsonObject{{"packageDir", "output/template"},
                     {"layerThicknessMm", 0.01},
                     {"channelOrder", channelorder},
                     {"bitDepth", 8},
                     {"storageMode", "stripped"}}},
        {"background", QJsonObject{{"value", 255}}},
        {"modelFill",
         QJsonObject{{"enabled", true},
                     {"material", "white"},
                     {"scope", "below_texture_surface"},
                     {"value", 0},
                     {"emptyAllowedInProduction", false},
                     {"legacyRgbFallback", false}}},
        {"support",
         QJsonObject{{"enabled", true},
                     {"mode", "bottom_projection"},
                     {"placement", "lower"},
                     {"internalVoid",
                      QJsonObject{{"enabled", true},
                                  {"minAreaPx", 16},
                                  {"fillRule", "all_internal_voids"}}}}},
        {"surfaceVarnish",
         QJsonObject{{"enabled", false},
                     {"thicknessPx", 0},
                     {"outerSurface", true},
                     {"innerSurface", true},
                     {"value", 0},
                     {"source", "explicit"}}},
        {"outerVarnish",
         QJsonObject{{"enabled", false},
                     {"thicknessMm", 0.0},
                     {"thicknessStepMm", 0.01},
                     {"pixelPitchUm", 42.3},
                     {"allowXYExpansion", true},
                     {"conflictPolicy", "varnish_shell_wins"},
                     {"value", 0}}},
        {"preview", QJsonObject{{"enabled", true}, {"interval", 10}}},
        {"texture",
         QJsonObject{{"enabled", true},
                     {"applyMode", "solid_volume_from_top_surface"},
                     {"topSurfaceLayers", 8},
                     {"nonSurfaceRgbPolicy", "model_material"}}},
    };

    const QString templatepath = tempdir.filePath("profile.template.json");
    QFile templatefile(templatepath);
    if (!templatefile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        return fail("generated-effective-config 无法写入模板 fixture。");
    }
    const QByteArray templatebytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    templatefile.write(templatebytes);
    templatefile.close();

    ConfigDocument document;
    if (!document.load(templatepath))
    {
        return fail("generated-effective-config 无法加载模板 fixture。");
    }
    document.setValue({"texture", "nonSurfaceRgbPolicy"}, "empty");

    SliceSettingsModel settingsmodel;
    if (!settingsmodel.ApplyProfileDefaults(QStringLiteral("textured_nail_rgb_varnish_lower_support")))
    {
        return fail("generated-effective-config 无法应用 Profile 默认值。");
    }
    SliceSettingsState settings = settingsmodel.State();
    settings.modelpath = QStringLiteral("model/obj/nail.obj");
    settings.outputdirectory = tempdir.filePath("session/package");
    settings.dpix = 635;
    settings.dpiy = 600;
    settings.layerthicknessmm = 0.02;
    settings.support.placement = SupportPlacement::Both;
    settings.support.internalvoidminareapx = 24;
    settings.surfacevarnish.enabled = true;
    settings.surfacevarnish.thicknesspx = 2;
    settings.outervarnish.enabled = true;
    settings.outervarnish.thicknessmm = 0.05;
    settings.preview.interval = 3;

    const QString generatedpath = tempdir.filePath("session/slice_config.effective.json");
    EffectiveConfigRequest request;
    request.profileid = settings.profileid;
    request.templatepath = templatepath;
    request.generatedconfigpath = generatedpath;
    request.originaldocument = document.originalDocument();
    request.overridedocument = document.document();
    request.settings = settings;

    const EffectiveConfigResult result = EffectiveConfigGenerator().Generate(request);
    if (!result.IsValid())
    {
        return fail("generated-effective-config 生成失败：" + result.errors.join("; "));
    }
    if (!QFileInfo::exists(generatedpath))
    {
        return fail("generated-effective-config 未写入 session 配置。");
    }

    QFile unchangedtemplate(templatepath);
    if (!unchangedtemplate.open(QIODevice::ReadOnly | QIODevice::Text)
        || unchangedtemplate.readAll() != templatebytes)
    {
        return fail("generated-effective-config 修改了原始模板。");
    }

    const QJsonObject generated = result.document.object();
    const QJsonObject output = generated.value("output").toObject();
    const QJsonObject support = generated.value("support").toObject();
    const QJsonObject baseProjection =
        support.value("baseProjection").toObject();
    const QJsonObject texture = generated.value("texture").toObject();
    const QJsonObject openvdb = generated.value("experimental").toObject().value("openvdbPipeline").toObject();
    if (generated.value("input").toObject().value("modelPath").toString() != settings.modelpath
        || output.value("packageDir").toString() != settings.outputdirectory
        || output.value("dpiX").toInt() != settings.dpix
        || output.value("dpiY").toInt() != settings.dpiy
        || output.value("layerThicknessMm").toDouble() != settings.layerthicknessmm
        || output.value("channelOrder").toArray() != channelorder
        || output.value("bitDepth").toInt() != 8
        || generated.value("background").toObject().value("value").toInt() != 255
        || generated.value("modelFill").toObject().value("material").toString() != "varnish"
        || generated.value("modelFill").toObject().value("scope").toString() != "all_model"
        || support.value("placement").toString() != "both"
        || support.value("internalVoid").toObject().value("minAreaPx").toInt() != 24
        || !baseProjection.value("enabled").toBool()
        || baseProjection.value("layerCount").toInt() != 30
        || baseProjection.value("layerPlacement").toString()
            != "prepend_below_model"
        || baseProjection.value("source").toString()
            != "max_support_footprint"
        || !generated.value("surfaceVarnish").toObject().value("enabled").toBool()
        || generated.value("surfaceVarnish").toObject().value("thicknessPx").toInt() != 2
        || !generated.value("outerVarnish").toObject().value("enabled").toBool()
        || generated.value("outerVarnish").toObject().value("thicknessMm").toDouble() != 0.05
        || generated.value("preview").toObject().value("interval").toInt() != 3
        || texture.value("applyMode").toString() != "top_surface_band"
        || texture.value("topSurfaceLayers").toInt() != 1
        || texture.value("nonSurfaceRgbPolicy").toString() != "empty"
        || openvdb.value("enabled").toBool(true)
        || openvdb.value("writeProductionRgbwsv").toBool(true))
    {
        return fail("generated-effective-config 未完整合成 Profile、dirty UI override 或安全边界。");
    }
    if (!result.warnings.join(QStringLiteral(" ")).contains(QStringLiteral("模型内部填充")))
    {
        return fail("generated-effective-config 未说明实体纹理投影与模型内部填充的自动纠正。");
    }
    if (!result.summary.contains(settings.profileid)
        || result.differences.isEmpty())
    {
        return fail("generated-effective-config 未提供生效摘要或差异。");
    }
    ConfigDocument viewdocument;
    if (!viewdocument.load(templatepath))
    {
        return fail("generated-effective-config 无法加载 UI 展示文档。");
    }
    ConfigEditorPanel configpanel(&viewdocument);
    if (!configpanel.loadConfig(templatepath))
    {
        return fail("generated-effective-config UI 无法加载压缩设置 fixture。");
    }
    auto* compressionCombo =
        configpanel.findChild<QComboBox*>("tiffCompressionCombo");
    if (compressionCombo == nullptr
        || compressionCombo->findData(QStringLiteral("none")) < 0
        || compressionCombo->findData(QStringLiteral("packbits")) < 0)
    {
        return fail("generated-effective-config UI 未提供 none/PackBits TIFF 压缩选项。");
    }
    compressionCombo->setCurrentIndex(
        compressionCombo->findData(QStringLiteral("packbits")));
    if (viewdocument
            .value({"output", "tiffCompression", "algorithm"})
            .toString()
        != QStringLiteral("packbits"))
    {
        return fail("generated-effective-config UI 未写回 output.tiffCompression.algorithm。");
    }
    EffectiveConfigRequest uiCompressionRequest = request;
    uiCompressionRequest.generatedconfigpath =
        tempdir.filePath("ui_compression/slice_config.effective.json");
    uiCompressionRequest.overridedocument = viewdocument.document();
    const EffectiveConfigResult uiCompressionResult =
        EffectiveConfigGenerator().Generate(uiCompressionRequest);
    if (!uiCompressionResult.IsValid()
        || uiCompressionResult.document.object()
                   .value("output")
                   .toObject()
                   .value("tiffCompression")
                   .toObject()
                   .value("algorithm")
                   .toString()
            != QStringLiteral("packbits"))
    {
        return fail("generated-effective-config 未把 UI PackBits 选择传入生效配置。");
    }
    configpanel.ShowEffectiveConfig(result);
    if (!configpanel.EffectiveConfigText().contains(settings.profileid)
        || !configpanel.EffectiveConfigText().contains(QStringLiteral("modelFill.material")))
    {
        return fail("generated-effective-config UI 未显示生效摘要和差异。");
    }

    EffectiveConfigRequest rgbOnlyRequest = request;
    rgbOnlyRequest.profileid = QStringLiteral("textured_nail_rgb_only_lower_support");
    rgbOnlyRequest.generatedconfigpath = tempdir.filePath("rgb-only/slice_config.effective.json");
    rgbOnlyRequest.settings.profileid = rgbOnlyRequest.profileid;
    rgbOnlyRequest.settings.outputdirectory = tempdir.filePath("rgb-only/package");
    rgbOnlyRequest.settings.modelfillmaterial = ModelFillMaterial::Rgb;
    const EffectiveConfigResult rgbOnlyResult = EffectiveConfigGenerator().Generate(rgbOnlyRequest);
    const QJsonObject rgbOnlyRoot = rgbOnlyResult.document.object();
    if (!rgbOnlyResult.IsValid()
        || rgbOnlyRoot.value("modelFill").toObject().value("material").toString() != "rgb"
        || rgbOnlyRoot.value("modelFill").toObject().value("scope").toString()
            != "below_texture_surface"
        || rgbOnlyRoot.value("texture").toObject().value("applyMode").toString()
            != "solid_volume_from_top_surface"
        || rgbOnlyResult.warnings.join(QStringLiteral(" ")).contains(QStringLiteral("已改为 1 层顶面纹理带")))
    {
        return fail("generated-effective-config 全实体 RGB Profile 被错误改写为白墨/光油填充或顶面纹理带。");
    }

    EffectiveConfigRequest invalidrequest = request;
    invalidrequest.generatedconfigpath = tempdir.filePath("invalid/slice_config.effective.json");
    invalidrequest.settings.outervarnish.enabled = true;
    invalidrequest.settings.outervarnish.thicknessmm = 0.0;
    const EffectiveConfigResult invalidresult = EffectiveConfigGenerator().Generate(invalidrequest);
    if (invalidresult.IsValid() || QFileInfo::exists(invalidrequest.generatedconfigpath))
    {
        return fail("generated-effective-config 非法设置未在写文件前阻断。");
    }

    EffectiveConfigRequest protocolrequest = request;
    protocolrequest.generatedconfigpath = tempdir.filePath("bad_protocol/slice_config.effective.json");
    QJsonObject badprotocolroot = protocolrequest.overridedocument.object();
    QJsonObject badprotocoloutput = badprotocolroot.value("output").toObject();
    badprotocoloutput.insert("bitDepth", 16);
    badprotocolroot.insert("output", badprotocoloutput);
    protocolrequest.overridedocument = QJsonDocument(badprotocolroot);
    const EffectiveConfigResult protocolresult = EffectiveConfigGenerator().Generate(protocolrequest);
    if (protocolresult.IsValid() || QFileInfo::exists(protocolrequest.generatedconfigpath))
    {
        return fail("generated-effective-config 未阻断 RGBWSV 固定协议偏差。");
    }

    EffectiveConfigRequest compressionRequest = request;
    compressionRequest.generatedconfigpath =
        tempdir.filePath("bad_compression/slice_config.effective.json");
    QJsonObject badCompressionRoot =
        compressionRequest.overridedocument.object();
    QJsonObject badCompressionOutput =
        badCompressionRoot.value("output").toObject();
    badCompressionOutput.insert(
        "tiffCompression",
        QJsonObject{{"algorithm", "deflate"}});
    badCompressionRoot.insert("output", badCompressionOutput);
    compressionRequest.overridedocument =
        QJsonDocument(badCompressionRoot);
    const EffectiveConfigResult compressionResult =
        EffectiveConfigGenerator().Generate(compressionRequest);
    if (compressionResult.IsValid()
        || QFileInfo::exists(compressionRequest.generatedconfigpath))
    {
        return fail("generated-effective-config 未阻断不支持的 TIFF 压缩算法。");
    }

    return pass(QString("generated-effective-config differences=%1 template-readonly=true compression=packbits")
                    .arg(result.differences.size()));
}
