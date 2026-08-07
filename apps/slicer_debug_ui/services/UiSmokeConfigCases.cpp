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

int UiSmokeTestRunner::sliceSettingsModel(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    struct ProfileExpectation
    {
        QString id;
        ModelFillMaterial fillmaterial;
        int previewinterval;
    };
    const QVector<ProfileExpectation> expectations{
        {QStringLiteral("textured_nail_rgb_only_lower_support"), ModelFillMaterial::Rgb, 10},
        {QStringLiteral("textured_nail_rgb_white_lower_support"), ModelFillMaterial::White, 10},
        {QStringLiteral("textured_nail_rgb_varnish_lower_support"), ModelFillMaterial::Varnish, 10},
        {QStringLiteral("single_material_relief"), ModelFillMaterial::White, 10},
        {QStringLiteral("production_rgb_inspection"), ModelFillMaterial::White, 1},
    };

    SliceSettingsModel model;
    if (model.State().dpix != slicer_core::kDefaultOutputDpiX
        || model.State().dpiy != slicer_core::kDefaultOutputDpiY
        || std::abs(
               model.State().layerthicknessmm
               - slicer_core::kDefaultLayerThicknessMm)
            > 1.0e-9
        || model.State().modelfillmaterial != ModelFillMaterial::Rgb)
    {
        return fail(
            "slice-settings-model 系统默认值不是 "
            "635x600 DPI、0.038 mm 层高和全 RGB。");
    }
    for (const ProfileExpectation& expectation : expectations)
    {
        if (!model.ApplyProfileDefaults(expectation.id))
        {
            return fail("slice-settings-model 无法应用稳定 Profile：" + expectation.id);
        }
        const SliceSettingsState& defaults = model.State();
        if (defaults.profileid != expectation.id
            || defaults.modelfillmaterial != expectation.fillmaterial
            || defaults.preview.interval != expectation.previewinterval
            || defaults.dpix != slicer_core::kDefaultOutputDpiX
            || defaults.dpiy != slicer_core::kDefaultOutputDpiY
            || std::abs(
                   defaults.layerthicknessmm
                   - slicer_core::kDefaultLayerThicknessMm)
                > 1.0e-9
            || !defaults.support.enabled
            || defaults.support.placement != SupportPlacement::Lower
            || !defaults.support.internalvoidenabled
            || defaults.surfacevarnish.enabled
            || defaults.outervarnish.enabled
            || defaults.outervarnish.thicknessmm != 0.0
            || defaults.enginerole != SliceEngineRole::LegacyProduction)
        {
            return fail("slice-settings-model Profile 默认值错误：" + expectation.id);
        }
    }

    const SliceSettingsState previousState = model.State();
    if (model.ApplyProfileDefaults(QStringLiteral("unknown_profile"))
        || model.State().profileid != previousState.profileid)
    {
        return fail("slice-settings-model 未知 Profile 不应改变状态。");
    }

    SliceSettingsState validState = model.State();
    validState.modelpath = QStringLiteral("model/obj/sample.obj");
    validState.outputdirectory = QStringLiteral("output/ui_sessions/sample/package");
    model.SetState(validState);
    const SliceSettingsValidationResult validValidation =
        model.Validate();
    if (!validValidation.IsValid())
    {
        return fail("slice-settings-model 安全 legacy 设置未通过校验。");
    }
    SliceSettingsState rgbOnlyState = validState;
    rgbOnlyState.modelfillmaterial = ModelFillMaterial::Rgb;
    model.SetState(rgbOnlyState);
    const SliceSettingsValidationResult rgbOnlyValidation =
        model.Validate();
    if (!rgbOnlyValidation.IsValid()
        || !rgbOnlyValidation.warnings.join(QStringLiteral(" ")).contains(
            QStringLiteral("纯白")))
    {
        return fail(
            "slice-settings-model 全实体 RGB 兼容模式未提示纯白像素的 RGBWSV 协议限制。");
    }
    model.SetState(validState);

    SliceSettingsState invalidDpiState = validState;
    invalidDpiState.dpix = slicer_core::kMaximumOutputDpi + 1;
    model.SetState(invalidDpiState);
    if (model.Validate().IsValid())
    {
        return fail("slice-settings-model 非法 X/Y DPI 未被阻断。");
    }

    SliceSettingsState candidateState = validState;
    candidateState.enginerole = SliceEngineRole::OpenVdbUtilityCandidate;
    model.SetState(candidateState);
    const SliceSettingsValidationResult candidateValidation = model.Validate();
    if (!candidateValidation.IsValid()
        || !candidateValidation.warnings.join(" ").contains("productionReplacementAllowed=false"))
    {
        return fail("slice-settings-model OpenVDB candidate 边界未固化。");
    }

    SliceSettingsState invalidState = validState;
    invalidState.layerthicknessmm = 0.0;
    invalidState.support.enabled = false;
    invalidState.support.internalvoidenabled = true;
    invalidState.outervarnish.enabled = true;
    invalidState.outervarnish.thicknessmm = 0.0;
    model.SetState(invalidState);
    if (model.Validate().IsValid())
    {
        return fail("slice-settings-model 非法设置未被阻断。");
    }

    return pass(
        "slice-settings-model profiles=5 diagnostics-default=false openvdb=candidate-only");
}

int UiSmokeTestRunner::SettingHelpMetadataCase(const UiSmokeTestOptions& options)
{
    const QStringList requiredKeys{
        QStringLiteral("output.dpiX"),
        QStringLiteral("output.dpiY"),
        QStringLiteral("modelTransform.scale"),
        QStringLiteral("modelFill.material"),
        QStringLiteral("support.enabled"),
        QStringLiteral("support.placement"),
        QStringLiteral("support.internalVoid.enabled"),
        QStringLiteral("surfaceVarnish.enabled"),
        QStringLiteral("outerVarnish.enabled"),
        QStringLiteral("preview.enabled"),
        QStringLiteral("preview.outputPolicy"),
        QStringLiteral("preview.interval"),
        QStringLiteral("engine.legacy"),
        QStringLiteral("engine.openvdbCandidate"),
    };

    const QVector<SettingHelpMetadata>& entries = HelpTextProvider::All();
    QSet<QString> seenKeys;
    for (const SettingHelpMetadata& entry : entries)
    {
        if (!entry.IsComplete())
        {
            return fail(QStringLiteral("setting-help-metadata 字段不完整：") + entry.key);
        }
        if (seenKeys.contains(entry.key))
        {
            return fail(QStringLiteral("setting-help-metadata 重复 key：") + entry.key);
        }
        seenKeys.insert(entry.key);

        if (!QFileInfo(QDir(options.repo_root).filePath(entry.docpath)).isFile())
        {
            return fail(QStringLiteral("setting-help-metadata 文档不存在：") + entry.docpath);
        }
        if (!ContainsAll(
                entry.ToolTipText(),
                {entry.title, entry.description, QStringLiteral("影响："),
                 QStringLiteral("默认："), QStringLiteral("生产安全："), entry.docpath}))
        {
            return fail(QStringLiteral("setting-help-metadata tooltip 字段缺失：") + entry.key);
        }
    }

    SettingHelpPanel helpPanel;
    for (const QString& key : requiredKeys)
    {
        const SettingHelpMetadata* metadata = HelpTextProvider::Find(key);
        if (metadata == nullptr || !helpPanel.SelectKey(key))
        {
            return fail(QStringLiteral("setting-help-metadata 缺少必需设置：") + key);
        }
        if (!ContainsAll(
                helpPanel.CurrentText(),
                {metadata->title, metadata->description, metadata->defaultvalue,
                 metadata->productionsafety, metadata->docpath}))
        {
            return fail(QStringLiteral("setting-help-metadata 说明面板字段缺失：") + key);
        }
    }

    QTemporaryDir configTempDir;
    if (!configTempDir.isValid())
    {
        return fail(QStringLiteral("setting-help-metadata 无法创建配置测试目录。"));
    }
    const QString configPath = configTempDir.filePath(QStringLiteral("quick-config.json"));
    const QJsonObject quickConfigFixture{
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("modelPath"), QStringLiteral("fixture.obj")}}},
        {QStringLiteral("output"), QJsonObject{{QStringLiteral("packageDir"), QStringLiteral("fixture-package")}}},
        {QStringLiteral("modelTransform"),
         QJsonObject{
             {QStringLiteral("scale"), QJsonArray{0.8, 0.8, 0.8}}}},
        {QStringLiteral("materialPolicy"),
         QJsonObject{
             {QStringLiteral("enabled"), false},
             {QStringLiteral("white"),
              QJsonObject{
                  {QStringLiteral("enabled"), false},
                  {QStringLiteral("mode"), QStringLiteral("disabled")}}}}},
        {QStringLiteral("materialProcessProfile"), QJsonObject{}},
    };
    if (!WriteJsonFixture(configPath, quickConfigFixture))
    {
        return fail(QStringLiteral("setting-help-metadata 无法写入配置测试夹具。"));
    }

    ConfigDocument document;
    if (!document.load(configPath))
    {
        return fail(QStringLiteral("setting-help-metadata 无法加载配置测试夹具。"));
    }
    QuickConfigPanel quickPanel(&document);
    quickPanel.LoadFromDocument();
    const QVector<QPair<QString, QString>> tooltipBindings{
        {QStringLiteral("modelScaleXSpin"), QStringLiteral("modelTransform.scale")},
        {QStringLiteral("outputDpiXSpin"), QStringLiteral("output.dpiX")},
        {QStringLiteral("outputDpiYSpin"), QStringLiteral("output.dpiY")},
        {QStringLiteral("modelFillMaterialCombo"), QStringLiteral("modelFill.material")},
        {QStringLiteral("whitePolicyEnabledCheck"), QStringLiteral("materialPolicy.white.enabled")},
        {QStringLiteral("supportEnabledCheck"), QStringLiteral("support.enabled")},
        {QStringLiteral("supportPlacementCombo"), QStringLiteral("support.placement")},
        {QStringLiteral("baseProjectionEnabledCheck"), QStringLiteral("support.baseProjection.enabled")},
        {QStringLiteral("baseProjectionLayerCountSpin"), QStringLiteral("support.baseProjection.layerCount")},
        {QStringLiteral("surfaceVarnishEnabledCheck"), QStringLiteral("surfaceVarnish.enabled")},
        {QStringLiteral("outerVarnishEnabledCheck"), QStringLiteral("outerVarnish.enabled")},
        {QStringLiteral("previewDiagnosticImagesCheck"), QStringLiteral("preview.enabled")},
        {QStringLiteral("previewIntervalSpin"), QStringLiteral("preview.interval")},
        {QStringLiteral("openVdbCandidateCheck"), QStringLiteral("engine.openvdbCandidate")},
    };
    for (const QPair<QString, QString>& binding : tooltipBindings)
    {
        QWidget* widget = quickPanel.findChild<QWidget*>(binding.first);
        if (widget == nullptr || widget->toolTip() != HelpTextProvider::ToolTip(binding.second))
        {
            return fail(QStringLiteral("setting-help-metadata tooltip 未复用集中元数据：") + binding.first);
        }
    }

    QDoubleSpinBox* modelScaleXSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleXSpin"));
    QDoubleSpinBox* modelScaleYSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleYSpin"));
    QDoubleSpinBox* modelScaleZSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleZSpin"));
    QPushButton* resetModelScaleButton = quickPanel.findChild<QPushButton*>(
        QStringLiteral("resetModelScaleButton"));
    QSpinBox* outputDpiXSpin = quickPanel.findChild<QSpinBox*>(
        QStringLiteral("outputDpiXSpin"));
    QSpinBox* outputDpiYSpin = quickPanel.findChild<QSpinBox*>(
        QStringLiteral("outputDpiYSpin"));
    QDoubleSpinBox* layerHeightSpin =
        quickPanel.findChild<QDoubleSpinBox*>(
            QStringLiteral("layerHeightSpin"));
    QLabel* outputPixelSizeLabel = quickPanel.findChild<QLabel*>(
        QStringLiteral("outputPixelSizeLabel"));
    if (modelScaleXSpin == nullptr
        || modelScaleYSpin == nullptr
        || modelScaleZSpin == nullptr
        || resetModelScaleButton == nullptr
        || modelScaleXSpin->value() != 0.8
        || modelScaleYSpin->value() != 0.8
        || modelScaleZSpin->value() != 0.8)
    {
        return fail(QStringLiteral("setting-help-metadata 模型缩放控件未正确加载配置值。"));
    }
    if (outputDpiXSpin == nullptr
        || outputDpiYSpin == nullptr
        || layerHeightSpin == nullptr
        || outputPixelSizeLabel == nullptr
        || outputDpiXSpin->minimum() != 72
        || outputDpiXSpin->maximum() != 2400
        || outputDpiYSpin->minimum() != 72
        || outputDpiYSpin->maximum() != 2400
        || outputDpiXSpin->value() != 635
        || outputDpiYSpin->value() != 600
        || std::abs(
               layerHeightSpin->value()
               - slicer_core::kDefaultLayerThicknessMm)
            > 1.0e-9
        || !ContainsAll(
            outputPixelSizeLabel->text(),
            {QStringLiteral("X 0.040000 mm/px"),
             QStringLiteral("Y 0.042333 mm/px")}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 默认值或物理像素提示错误。"));
    }
    const QList<QAbstractSpinBox*> numericInputs =
        quickPanel.findChildren<QAbstractSpinBox*>();
    if (numericInputs.isEmpty())
    {
        return fail(
            QStringLiteral(
                "setting-help-metadata 未找到可编辑数值输入框。"));
    }
    for (const QAbstractSpinBox* input : numericInputs)
    {
        if (input->keyboardTracking())
        {
            return fail(
                QStringLiteral(
                    "setting-help-metadata 数值输入仍会逐字符提交：")
                + input->objectName());
        }
    }
    outputDpiXSpin->setValue(600);
    outputDpiYSpin->setValue(1200);
    if (document.value({QStringLiteral("output"), QStringLiteral("dpiX")}).toInt() != 600
        || document.value({QStringLiteral("output"), QStringLiteral("dpiY")}).toInt() != 1200
        || !ContainsAll(
            outputPixelSizeLabel->text(),
            {QStringLiteral("X 0.042333 mm/px"),
             QStringLiteral("Y 0.021167 mm/px")}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 修改未写入配置或刷新物理像素。"));
    }
    if (!document.save(nullptr, SaveOptions{true}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 配置保存失败。"));
    }
    ConfigDocument reloadedDocument;
    if (!reloadedDocument.load(configPath)
        || reloadedDocument.value(
               {QStringLiteral("output"), QStringLiteral("dpiX")}).toInt()
            != 600
        || reloadedDocument.value(
               {QStringLiteral("output"), QStringLiteral("dpiY")}).toInt()
            != 1200)
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 未能独立保存并重新加载。"));
    }
    document.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiX")},
        slicer_core::kMaximumOutputDpi + 1);
    if (document.validate().isValid())
    {
        return fail(QStringLiteral("setting-help-metadata 超范围 DPI 未被配置校验阻断。"));
    }
    document.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiX")},
        600);
    resetModelScaleButton->click();
    const QJsonArray resetScale = document.value(
        {QStringLiteral("modelTransform"), QStringLiteral("scale")}).toArray();
    if (resetScale != QJsonArray{1.0, 1.0, 1.0})
    {
        return fail(QStringLiteral("setting-help-metadata 模型缩放未恢复为 1:1。"));
    }

    QCheckBox* whitePolicyCheck = quickPanel.findChild<QCheckBox*>(
        QStringLiteral("whitePolicyEnabledCheck"));
    if (whitePolicyCheck == nullptr)
    {
        return fail(QStringLiteral("setting-help-metadata 未找到白墨叠加策略控件。"));
    }
    whitePolicyCheck->setChecked(true);
    if (!document.value({QStringLiteral("materialPolicy"), QStringLiteral("enabled")}).toBool()
        || !document.value(
                {QStringLiteral("materialPolicy"), QStringLiteral("white"), QStringLiteral("enabled")})
                .toBool()
        || document.value(
               {QStringLiteral("materialPolicy"), QStringLiteral("white"), QStringLiteral("mode")})
               .toString()
            != QStringLiteral("all_model")
        || document.value(
               {QStringLiteral("materialProcessProfile"), QStringLiteral("white"), QStringLiteral("mode")})
               .toString()
            != QStringLiteral("all_model")
        || !document.value(
                {QStringLiteral("materialProcessProfile"), QStringLiteral("validation"),
                 QStringLiteral("requireWhitePixels")})
                .toBool())
    {
        return fail(QStringLiteral("setting-help-metadata 白墨叠加开关未写入完整材料策略。"));
    }

    const SettingHelpMetadata* openVdb = HelpTextProvider::Find(
        QStringLiteral("engine.openvdbCandidate"));
    if (openVdb == nullptr
        || !ContainsAll(
            openVdb->DetailText(),
            {QStringLiteral("关闭"), QStringLiteral("非生产"),
             QStringLiteral("productionReplacementAllowed=false")}))
    {
        return fail(QStringLiteral("setting-help-metadata OpenVDB 安全边界不完整。"));
    }

    return pass(QStringLiteral("setting-help-metadata entries=%1 required=%2 tooltips=%3")
                    .arg(entries.size())
                    .arg(requiredKeys.size())
                    .arg(tooltipBindings.size()));
}
