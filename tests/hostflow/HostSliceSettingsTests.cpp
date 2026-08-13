#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostProcessPresetCatalog.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/HostSliceSettingsPanel.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"
#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/json_value.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#include <sstream>

namespace
{
bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

hostslicesettings MakeSettings(
    const QString& modelPath,
    const QString& outputDirectory)
{
    hostslicesettings settings;
    settings.profileid = QStringLiteral("host-reference-default");
    settings.modelpath = modelPath;
    settings.modelformat = QStringLiteral("obj");
    settings.outputdirectory = outputDirectory;
    return settings;
}

QString ComputeWorkerProfileHash(const QJsonObject& profile)
{
    const QByteArray serialized = QJsonDocument(
        profile).toJson(QJsonDocument::Compact);
    std::istringstream input(serialized.toStdString());
    return QString::fromStdString(
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json::parse(input)));
}

bool VerifyPresetProfileHashClosure(
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    for (const hostprocesspreset& preset
         : HostProcessPresetCatalog::Presets())
    {
        hostslicesettings settings = MakeSettings(
            modelPath, outputDirectory);
        settings.materialstrategy = preset.materialstrategy;
        settings.materialprocess = preset.materialprocess;
        settings.texture = preset.texture;
        settings.support = preset.support;

        hosteffectiveprofile effective;
        QString error;
        if (!Check(
                HostEffectiveProfileBuilder::Build(
                    settings, &effective, &error),
                QStringLiteral("常用工艺 %1 的有效 Profile 构造失败：%2")
                    .arg(preset.id, error),
                errors))
        {
            return false;
        }

        const QString computed = ComputeWorkerProfileHash(
            effective.profile);
        if (!Check(
                computed == effective.profilehash,
                QStringLiteral(
                    "常用工艺 %1 的 Profile hash 未闭合：声明=%2，重算=%3")
                    .arg(preset.id, effective.profilehash, computed),
                errors))
        {
            return false;
        }
    }
    return true;
}

bool VerifyEffectiveProfiles(
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    hostslicesettings settings = MakeSettings(modelPath, outputDirectory);
    hosteffectiveprofile first;
    QString error;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &first, &error),
            QStringLiteral("默认有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialstrategy = HostMaterialStrategy::RgbWhiteVarnish;
    settings.materialprocess.rolemappingenabled = true;
    settings.materialprocess.defaultrole = HostMaterialRole::Ignore;
    settings.materialprocess.allowinputsupportmaterial = true;
    settings.materialprocess.whiteexpandpx = 2;
    settings.materialprocess.whiteshrinkpx = 1;
    settings.materialprocess.varnishtoplayers = 3;
    settings.materialprocess.maxunexpectedoverlappixels = 4;
    hosteffectiveprofile combined;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &combined, &error)
                && combined.profilehash != first.profilehash,
            QStringLiteral("组合材料有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    settings.materialprocess.mapwhitenames = false;
    settings.materialprocess.mapvarnishnames = false;
    hosteffectiveprofile emptyRules;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &emptyRules, &error)
                && ComputeWorkerProfileHash(emptyRules.profile)
                    == emptyRules.profilehash,
            QStringLiteral(
                "空材料映射规则的 Profile hash 必须与 Worker 重算一致。"),
            errors))
    {
        return false;
    }
    const QJsonObject combinedPolicy = combined.profile.value(
        QStringLiteral("materialPolicy")).toObject();
    const QJsonObject combinedProcess = combined.profile.value(
        QStringLiteral("materialProcessProfile")).toObject();
    const QJsonObject roleMapping = combined.profile.value(
        QStringLiteral("materialRoleMapping")).toObject();
    const QJsonObject modelFill = combined.profile.value(
        QStringLiteral("modelFill")).toObject();
    if (!Check(
            combinedPolicy.value(QStringLiteral("rgb")).toObject().value(
                QStringLiteral("enabled")).toBool()
                && combinedPolicy.value(
                    QStringLiteral("white")).toObject().value(
                        QStringLiteral("enabled")).toBool()
                && combinedPolicy.value(
                    QStringLiteral("varnish")).toObject().value(
                        QStringLiteral("enabled")).toBool()
                && combinedProcess.value(
                    QStringLiteral("white")).toObject().value(
                        QStringLiteral("expandPx")).toInt() == 2
                && combinedProcess.value(
                    QStringLiteral("varnish")).toObject().value(
                        QStringLiteral("topLayers")).toInt() == 3
                && combinedProcess.value(
                    QStringLiteral("validation")).toObject().value(
                        QStringLiteral("maxUnexpectedOverlapPixels"))
                        .toInt() == 4
                && roleMapping.value(QStringLiteral("enabled")).toBool()
                && roleMapping.value(
                    QStringLiteral("defaultRole")).toString()
                    == QStringLiteral("ignore")
                && roleMapping.value(QStringLiteral("rules")).toArray()
                    .size() == 2
                && modelFill.value(QStringLiteral("enabled")).toBool()
                && modelFill.value(QStringLiteral("material")).toString()
                    == QStringLiteral("white"),
            QStringLiteral("材料策略、角色映射或模型填充未进入有效 Profile。"),
            errors))
    {
        return false;
    }
    const QJsonObject output = first.profile.value(
        QStringLiteral("output")).toObject();
    const QJsonObject geometrySampling = first.profile.value(
        QStringLiteral("geometrySampling")).toObject();
    const QJsonObject material = first.profile.value(
        QStringLiteral("modelMaterial")).toObject();
    const QJsonObject support = first.profile.value(
        QStringLiteral("support")).toObject();
    const QJsonObject processSupport = first.profile.value(
        QStringLiteral("materialProcessProfile")).toObject().value(
            QStringLiteral("support")).toObject();
    if (!Check(
            output.value(QStringLiteral("dpiX")).toInt() == 635
                && output.value(QStringLiteral("dpiY")).toInt() == 600
                && output.value(
                    QStringLiteral("layerThicknessMm")).toDouble() == 0.038
                && geometrySampling.value(
                    QStringLiteral("strategy")).toString()
                    == QStringLiteral("legacy_center_sample"),
            QStringLiteral("参考默认 DPI/层厚/Legacy 采样未进入有效 Profile。"),
            errors)
        || !Check(
            material.value(QStringLiteral("materialChannel")).toString()
                == QStringLiteral("RGB"),
            QStringLiteral("默认材料策略应映射到 RGB。"),
            errors)
        || !Check(
            support.value(QStringLiteral("enabled")).toBool()
                && support.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("bottom_projection")
                && support.value(QStringLiteral("placement")).toString()
                    == QStringLiteral("lower")
                && support.value(QStringLiteral("value")).toInt(-1) == 0
                && support.value(QStringLiteral("internalVoid")).toObject()
                    .value(QStringLiteral("enabled")).toBool()
                && processSupport.value(
                    QStringLiteral("expected")).toBool(),
            QStringLiteral("默认 lower support Profile 段不完整。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    hosteffectiveprofile repeated;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &repeated, &error)
                && repeated.profilehash == first.profilehash,
            QStringLiteral("相同输入的 Profile hash 必须确定。"),
            errors))
    {
        return false;
    }
    settings.dpix = 636;
    hosteffectiveprofile changed;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &changed, &error)
                && changed.profilehash != first.profilehash,
            QStringLiteral("DPI 变化必须使 Profile hash 变化。"),
            errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.mode = HostSupportMode::FullVerticalProjection;
    settings.support.offsetmm = 0.25;
    settings.support.minareapx = 32;
    settings.support.baseprojection.enabled = true;
    settings.support.baseprojection.layercount = 30;
    hosteffectiveprofile supportChanged;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &supportChanged, &error)
                && supportChanged.profilehash != first.profilehash,
            QStringLiteral("支撑参数变化必须使 Profile hash 变化。"),
            errors))
    {
        return false;
    }
    const QJsonObject changedSupport = supportChanged.profile.value(
        QStringLiteral("support")).toObject();
    if (!Check(
            changedSupport.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("full_vertical_projection")
                && !changedSupport.contains(QStringLiteral("placement"))
                && changedSupport.value(
                    QStringLiteral("baseProjection")).toObject().value(
                        QStringLiteral("layerCount")).toInt() == 30,
            QStringLiteral("高级支撑参数未进入有效 Profile。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.enabled = false;
    settings.support.mode = HostSupportMode::None;
    settings.support.internalvoid.enabled = false;
    settings.support.baseprojection.enabled = false;
    hosteffectiveprofile disabledSupport;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &disabledSupport, &error),
            QStringLiteral("关闭支撑的有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject disabledRoot = disabledSupport.profile.value(
        QStringLiteral("support")).toObject();
    const QJsonObject disabledProcess = disabledSupport.profile.value(
        QStringLiteral("materialProcessProfile")).toObject().value(
            QStringLiteral("support")).toObject();
    if (!Check(
            !disabledRoot.value(QStringLiteral("enabled")).toBool()
                && disabledRoot.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("none")
                && !disabledProcess.value(
                    QStringLiteral("expected")).toBool(),
            QStringLiteral("关闭支撑时 expected/mode 未同步。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialstrategy = HostMaterialStrategy::WhiteSolid;
    hosteffectiveprofile white;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &white, &error),
            QStringLiteral("白墨有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject whiteMaterial = white.profile.value(
        QStringLiteral("modelMaterial")).toObject();
    if (!Check(
            whiteMaterial.value(
                QStringLiteral("materialChannel")).toString()
                    == QStringLiteral("W")
                && whiteMaterial.value(
                    QStringLiteral("whiteValue")).toInt() == 0
                && whiteMaterial.value(
                    QStringLiteral("varnishValue")).toInt() == 255,
            QStringLiteral("白墨实体映射必须只写 W 通道。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.texture.enabled = true;
    settings.texture.applymode = HostTextureApplyMode::TopSurfaceBand;
    settings.texture.topsurfacelayers = 5;
    settings.texture.uvaddressmode = HostTextureUvAddressMode::Repeat;
    settings.texture.missingpolicy = HostTextureMissingPolicy::FailFast;
    settings.texture.nonsurfacepolicy = HostTextureNonSurfacePolicy::Empty;
    settings.texture.fallbackred = 12;
    settings.texture.fallbackgreen = 34;
    settings.texture.fallbackblue = 56;
    hosteffectiveprofile textured;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &textured, &error)
                && textured.profilehash != first.profilehash,
            QStringLiteral("生产纹理有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject texture = textured.profile.value(
        QStringLiteral("texture")).toObject();
    if (!Check(
            textured.profile.value(QStringLiteral("slicingMode")).toString()
                    == QStringLiteral("relief_heightfield")
                && texture.value(QStringLiteral("enabled")).toBool()
                && texture.value(QStringLiteral("applyMode")).toString()
                    == QStringLiteral("top_surface_band")
                && texture.value(QStringLiteral("topSurfaceLayers")).toInt()
                    == 5
                && texture.value(QStringLiteral("uvAddressMode")).toString()
                    == QStringLiteral("repeat")
                && texture.value(
                    QStringLiteral("missingTexturePolicy")).toString()
                    == QStringLiteral("fail_fast")
                && texture.value(
                    QStringLiteral("nonSurfaceRgbPolicy")).toString()
                    == QStringLiteral("empty")
                && texture.value(QStringLiteral("fallbackRgb")).toArray()
                    == QJsonArray{12, 34, 56},
            QStringLiteral("纹理应用、UV 或回退策略未进入有效 Profile。"),
            errors))
    {
        return false;
    }

    settings.geometrysamplingstrategy =
        HostGeometrySamplingStrategy::
            LayerSlabSupersample2x2AtLeastTwoCandidate;
    hosteffectiveprofile sampled;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &sampled, &error)
                && sampled.profilehash != textured.profilehash
                && sampled.profile.value(
                    QStringLiteral("geometrySampling")).toObject().value(
                        QStringLiteral("strategy")).toString()
                    == QStringLiteral(
                        "layer_slab_supersample_2x2_at_least_two_candidate")
                && ComputeWorkerProfileHash(sampled.profile)
                    == sampled.profilehash,
            QStringLiteral("S3 显式候选未进入自哈希 relief Profile。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.geometrysamplingstrategy =
        HostGeometrySamplingStrategy::
            LayerSlabSupersample2x2AtLeastTwoCandidate;
    error.clear();
    if (!Check(
            !HostEffectiveProfileBuilder::Validate(settings, &error),
            QStringLiteral("S3 与非 relief Profile 组合必须 fail-closed。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.texture.enabled = true;
    settings.texture.whitepolicy = HostTextureWhitePolicy::WhiteUnderbase;
    settings.texture.whiteinkthreshold = 4;
    settings.texture.whitevalue = 0;
    hosteffectiveprofile whiteCarrier;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &whiteCarrier, &error),
            QStringLiteral("Stage 15 按需补白 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject carrierTexture = whiteCarrier.profile.value(
        QStringLiteral("texture")).toObject();
    const QJsonObject carrierPolicy = whiteCarrier.profile.value(
        QStringLiteral("materialPolicy")).toObject();
    const QJsonObject carrierProcess = whiteCarrier.profile.value(
        QStringLiteral("materialProcessProfile")).toObject();
    if (!Check(
            carrierTexture.value(
                QStringLiteral("unprintableWhitePolicy")).toString()
                    == QStringLiteral("white_underbase")
                && carrierTexture.value(
                    QStringLiteral("unprintableWhiteInkThreshold")).toInt()
                    == 4
                && !carrierPolicy.value(QStringLiteral("enabled")).toBool()
                && carrierProcess.value(
                    QStringLiteral("white")).toObject().value(
                        QStringLiteral("mode")).toString()
                    == QStringLiteral("unprintable_white_underbase")
                && !whiteCarrier.profile.value(
                    QStringLiteral("materialRoleMapping")).toObject().value(
                        QStringLiteral("enabled")).toBool(),
            QStringLiteral("Stage 15 按需补白与材料合同未协同。"),
            errors))
    {
        return false;
    }
    settings.materialstrategy = HostMaterialStrategy::RgbWhite;
    error.clear();
    if (!Check(
            !HostEffectiveProfileBuilder::Validate(settings, &error),
            QStringLiteral("按需补白与普通 RGB+W 策略组合必须 fail-closed。"),
            errors))
    {
        return false;
    }

    settings.outputdirectory = QStringLiteral("relative/package");
    error.clear();
    if (!Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error)
            && !error.isEmpty(),
        QStringLiteral("相对输出目录必须 fail-closed。"),
        errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialstrategy = static_cast<HostMaterialStrategy>(99);
    error.clear();
    if (!Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error),
        QStringLiteral("未知材料策略必须 fail-closed。"),
        errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.baseprojection.layercount = 1001;
    error.clear();
    if (!Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error),
        QStringLiteral("越界铺底层数必须 fail-closed。"),
        errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialprocess.varnishtoplayers = 0;
    error.clear();
    return Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error),
        QStringLiteral("越界光油顶层数必须 fail-closed。"),
        errors);
}

bool VerifyPanelIsLocal(
    ModuleClient& client,
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    HostSliceSettingsPanel panel;
    panel.SetSelectedProfileId(QStringLiteral("host-reference-default"));
    auto* outputEdit = panel.findChild<QLineEdit*>(
        QStringLiteral("hostSliceOutputEdit"));
    auto* processPreset = panel.findChild<QComboBox*>(
        QStringLiteral("hostProcessPresetCombo"));
    auto* dpiXSpin = panel.findChild<QSpinBox*>(
        QStringLiteral("hostSliceDpiXSpin"));
    auto* materialCombo = panel.findChild<QComboBox*>(
        QStringLiteral("hostSliceMaterialCombo"));
    auto* supportEnabled = panel.findChild<QCheckBox*>(
        QStringLiteral("hostSupportEnabledCheck"));
    auto* supportMode = panel.findChild<QComboBox*>(
        QStringLiteral("hostSupportModeCombo"));
    auto* supportOffset = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostSupportOffsetSpin"));
    auto* roleMapping = panel.findChild<QCheckBox*>(
        QStringLiteral("hostMaterialRoleMappingCheck"));
    auto* whiteExpand = panel.findChild<QSpinBox*>(
        QStringLiteral("hostMaterialWhiteExpandSpin"));
    auto* textureEnabled = panel.findChild<QCheckBox*>(
        QStringLiteral("hostTextureEnabledCheck"));
    auto* textureApplyMode = panel.findChild<QComboBox*>(
        QStringLiteral("hostTextureApplyModeCombo"));
    auto* textureWhitePolicy = panel.findChild<QComboBox*>(
        QStringLiteral("hostTextureWhitePolicyCombo"));
    if (!Check(
            outputEdit != nullptr && processPreset != nullptr
                && dpiXSpin != nullptr
                && materialCombo != nullptr && supportEnabled != nullptr
                && supportMode != nullptr && supportOffset != nullptr
                && roleMapping != nullptr && whiteExpand != nullptr
                && textureEnabled != nullptr
                && textureApplyMode != nullptr
                && textureWhitePolicy != nullptr,
            QStringLiteral("切片设置面板控件不完整。"),
            errors))
    {
        return false;
    }
    const QString defaultOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            QFileInfo(defaultOutput).isAbsolute()
                && defaultOutput.contains(
                    QStringLiteral("/output/ui_sessions/"))
                && defaultOutput.endsWith(QStringLiteral("/package")),
            QStringLiteral(
                "参考宿主默认输出目录未对齐旧版 output/ui_sessions/<session>/package。"),
            errors))
    {
        return false;
    }
    outputEdit->setText(outputDirectory);
    panel.SetModelPath(modelPath);
    client.ResetCallCount();

    const int onDemandIndex = processPreset->findData(
        QStringLiteral("textured_nail_rgb_white_ondemand_lower_support"));
    processPreset->setCurrentIndex(onDemandIndex);
    QCoreApplication::processEvents();
    hostslicesettings presetSettings = panel.Settings();
    if (!Check(
            onDemandIndex > 0
                && processPreset->itemText(onDemandIndex)
                    == QStringLiteral(
                        "彩色纹理｜全实体 RGB + 纯白按需补 W｜下表面支撑")
                && !processPreset->itemData(
                    onDemandIndex, Qt::ToolTipRole).toString().isEmpty()
                && presetSettings.materialstrategy
                    == HostMaterialStrategy::RgbSolid
                && presetSettings.texture.enabled
                && presetSettings.texture.applymode
                    == HostTextureApplyMode::SolidVolumeFromTopSurface
                && presetSettings.texture.whitepolicy
                    == HostTextureWhitePolicy::WhiteUnderbase
                && presetSettings.support.enabled
                && presetSettings.support.mode
                    == HostSupportMode::BottomProjection,
            QStringLiteral("常用按需补白工艺方案未完整应用。"),
            errors))
    {
        return false;
    }
    const int varnishIndex = processPreset->findData(
        QStringLiteral("single_material_relief_varnish"));
    processPreset->setCurrentIndex(varnishIndex);
    QCoreApplication::processEvents();
    presetSettings = panel.Settings();
    if (!Check(
            varnishIndex > 0
                && presetSettings.materialstrategy
                    == HostMaterialStrategy::VarnishSolid
                && !presetSettings.texture.enabled,
            QStringLiteral("单材料光油工艺方案未完整应用。"),
            errors))
    {
        return false;
    }

    dpiXSpin->setValue(700);
    materialCombo->setCurrentIndex(1);
    roleMapping->setChecked(true);
    whiteExpand->setValue(2);
    supportMode->setCurrentIndex(2);
    supportOffset->setValue(0.2);
    materialCombo->setCurrentIndex(0);
    roleMapping->setChecked(false);
    textureEnabled->setChecked(true);
    textureApplyMode->setCurrentIndex(0);
    textureWhitePolicy->setCurrentIndex(1);
    QCoreApplication::processEvents();
    if (!Check(
        panel.IsReady(),
        QStringLiteral("有效设置未生成 Profile 预览。"),
        errors)
        || !Check(
            client.CallCount() == 0U,
            QStringLiteral("切片参数编辑不得调用 DLL。"),
            errors)
        || !Check(
            processPreset->currentData().toString()
                == QStringLiteral("custom"),
            QStringLiteral("手工细调材料参数后应切换为自定义工艺。"),
            errors))
    {
        return false;
    }
    panel.SetSelectedProfileId(
        QStringLiteral("host-reference-package-review"), false);
    return Check(
        !panel.IsReady(),
        QStringLiteral("不含 slice.rgbwsv 的诊断 Profile 不得生成切片配置。"),
        errors);
}

bool VerifySceneAuthority(
    ModuleClient& client,
    const QString& modelPath,
    QTextStream& errors)
{
    HostModelImportWorkflow workflow(client);
    hostbuildvolume volume;
    volume.widthmm = 210.0;
    volume.heightmm = 90.0;
    volume.zlimitmm = 50.0;
    QString error;
    if (!Check(
            workflow.SetPendingSceneContext(
                QStringLiteral("host-reference-material-parity"),
                volume,
                &error),
            QStringLiteral("宿主场景上下文设置失败：%1").arg(error),
            errors))
    {
        return false;
    }
    hostmodelimportresult imported;
    if (!Check(
            workflow.ImportModel(modelPath, &imported, &error),
            QStringLiteral("场景上下文验证模型导入失败：%1").arg(error),
            errors))
    {
        return false;
    }

    const QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_snapshot")},
        {QStringLiteral("sceneHandle"),
         static_cast<qint64>(workflow.SceneHandle())}};
    QByteArray responseBytes;
    if (!Check(
            client.Execute(
                QJsonDocument(request).toJson(QJsonDocument::Compact),
                &responseBytes,
                &error),
            QStringLiteral("场景快照读取失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject response = QJsonDocument::fromJson(
        responseBytes).object();
    const QJsonObject scene = response.value(QStringLiteral("scene")).toObject();
    const QJsonObject snapshotVolume = scene.value(
        QStringLiteral("buildVolume")).toObject();
    if (!Check(
            scene.value(QStringLiteral("resolvedProfileId")).toString()
                == QStringLiteral("host-reference-material-parity")
                && snapshotVolume.value(
                    QStringLiteral("widthMm")).toDouble() == 210.0
                && snapshotVolume.value(
                    QStringLiteral("heightMm")).toDouble() == 90.0,
            QStringLiteral("首次 Commit 未采用宿主 Profile/buildVolume。"),
            errors))
    {
        return false;
    }

    const quint64 revision = workflow.SceneRevision();
    volume.widthmm = 220.0;
    error.clear();
    return Check(
        !workflow.SetPendingSceneContext(
            QStringLiteral("host-reference-material-parity"),
            volume,
            &error)
            && workflow.SceneRevision() == revision,
        QStringLiteral("已绑定场景必须拒绝 buildVolume 静默变更。"),
        errors);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTextStream errors(stderr);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    QTemporaryDir outputRoot;
    if (!Check(QFileInfo(modulePath).isFile(),
               QStringLiteral("slicer_module.dll 不存在。"), errors)
        || !Check(QFileInfo(modelPath).isFile(),
                  QStringLiteral("H-B-05 模型 fixture 不存在。"), errors)
        || !Check(outputRoot.isValid(),
                  QStringLiteral("临时输出根目录不可用。"), errors))
    {
        return 2;
    }
    const QString outputDirectory = QDir(outputRoot.path()).filePath(
        QStringLiteral("package"));

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "模块加载失败：" << error << Qt::endl;
        return 3;
    }
    if (!VerifyPresetProfileHashClosure(
            modelPath, outputDirectory, errors)
        || !VerifyEffectiveProfiles(modelPath, outputDirectory, errors)
        || !VerifyPanelIsLocal(
            client, modelPath, outputDirectory, errors)
        || !VerifySceneAuthority(client, modelPath, errors))
    {
        return 4;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HE04_PASS profile=host-reference-default"
        << " dpi=635x600 layer=0.038 support=editable"
        << " material=editable" << Qt::endl;
    return 0;
}
