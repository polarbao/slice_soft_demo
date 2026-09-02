#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostProcessPresetCatalog.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/HostSliceSettingsPanel.h"
#include "apps/slicer_ui_host_sim/HostPackageReviewPanel.h"
#include "apps/slicer_ui_host_sim/HostSliceJobPanel.h"
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
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <sstream>

namespace
{
// 断言随规则而非索引：凡叠加了非 RGB 通道的条目，其 tooltip 必须写明是伪彩色。
//
// 原断言把「索引 4 的 tooltip 含伪彩色」写死。索引 4 当时恰是六通道组合，
// 但那是下拉排布的实现细节：并集项由两个（六通道、七通道）合并为一个随包改写的项后，
// 索引 4 变成了单通道，断言随即失败——而它要守的性质（伪彩色必须有说明，
// 免得被当成生产 TIFF 的像素值）其实一条都没被破坏。
// 钉索引会让每次合理的重排都误报，钉规则才守得住意图。
bool AllPseudoColourItemsExplainThemselves(const QComboBox* combo)
{
    if (combo == nullptr)
    {
        return false;
    }
    for (int index = 0; index < combo->count(); ++index)
    {
        const QStringList channels = combo->itemData(index).toStringList();
        const bool hasPseudoColourChannel =
            channels.contains(QStringLiteral("W"))
            || channels.contains(QStringLiteral("S"))
            || channels.contains(QStringLiteral("V"))
            || channels.contains(QStringLiteral("T"));
        if (!hasPseudoColourChannel)
        {
            continue;
        }
        if (!combo->itemData(index, Qt::ToolTipRole)
                 .toString()
                 .contains(QStringLiteral("伪彩色")))
        {
            return false;
        }
    }
    return true;
}


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
        if (preset.packageprotocol == HostPackageProtocol::Rgbwsvt) { continue; }
        hostslicesettings settings = MakeSettings(
            modelPath, outputDirectory);
        settings.materialstrategy = preset.materialstrategy;
        settings.materialprocess = preset.materialprocess;
        settings.texture = preset.texture;
        settings.support = preset.support;
        settings.materialvolume = preset.materialvolume;

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

/// @brief MV-07A：materialVolumePolicy 必须条件产出 —— 旧六类预设的 Profile 里
///        不得出现该块（保证其 profileHash 不变），新预设里必须出现且字段正确。
bool VerifyMaterialVolumeConditionalEmission(
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    bool sawLegacy = false;
    bool sawVolumetric = false;
    bool sawVolumetricAuto = false;
    for (const hostprocesspreset& preset : HostProcessPresetCatalog::Presets())
    {
        if (preset.packageprotocol == HostPackageProtocol::Rgbwsvt) { continue; }
        hostslicesettings settings = MakeSettings(modelPath, outputDirectory);
        settings.materialstrategy = preset.materialstrategy;
        settings.materialprocess = preset.materialprocess;
        settings.texture = preset.texture;
        settings.support = preset.support;
        settings.materialvolume = preset.materialvolume;

        hosteffectiveprofile effective;
        QString error;
        if (!Check(
                HostEffectiveProfileBuilder::Build(settings, &effective, &error),
                QStringLiteral("预设 %1 的 Profile 构造失败：%2").arg(preset.id, error),
                errors))
        {
            return false;
        }
        const bool hasBlock = effective.profile.contains(
            QStringLiteral("materialVolumePolicy"));
        if (!preset.materialvolume.enabled)
        {
            sawLegacy = true;
            if (!Check(
                    !hasBlock,
                    QStringLiteral("旧预设 %1 的 Profile 不得出现 materialVolumePolicy 块。")
                        .arg(preset.id),
                    errors))
            {
                return false;
            }
            if (!Check(
                    effective.profile.value(QStringLiteral("slicingMode")).toString()
                        != QStringLiteral("relief_heightfield")
                        || preset.texture.enabled
                        || preset.materialstrategy == HostMaterialStrategy::WhiteSolid
                        || preset.materialstrategy == HostMaterialStrategy::VarnishSolid,
                    QStringLiteral("旧预设 %1 的 slicingMode 推导被意外改变。").arg(preset.id),
                    errors))
            {
                return false;
            }
            continue;
        }
        sawVolumetric = true;
        if (!Check(
                hasBlock,
                QStringLiteral("MATVOL 预设 %1 必须产出 materialVolumePolicy 块。")
                    .arg(preset.id),
                errors))
        {
            return false;
        }
        const QJsonObject block = effective.profile.value(
            QStringLiteral("materialVolumePolicy")).toObject();
        const QJsonObject openSurface = block.value(
            QStringLiteral("openSurface")).toObject();
        const QJsonObject overlap = block.value(QStringLiteral("overlap")).toObject();
        const QJsonArray rules = overlap.value(QStringLiteral("rules")).toArray();
        // MATVOL 预设有两种 overlap 形态，各自逐项校验；共有字段先统一检查。
        // 首版此处只写了显式优先级一种形态并硬编码 01/02 与 200/100，
        // 于是 MATOPQ 新增自动模式预设时四个 hostflow 用例一并失败——
        // 断言过窄而非实现出错，故按形态分支而不是放宽任何一项检查。
        if (!Check(
                block.value(QStringLiteral("enabled")).toBool()
                    && block.value(QStringLiteral("mode")).toString()
                        == QStringLiteral("closed_intervals")
                    && block.value(QStringLiteral("missingMaterial")).toString()
                        == QStringLiteral("fail_closed")
                    && openSurface.value(QStringLiteral("mode")).toString()
                        == QStringLiteral("reject"),
                QStringLiteral("MATVOL 预设 %1 的 materialVolumePolicy 公共字段不正确。")
                    .arg(preset.id),
                errors))
        {
            return false;
        }
        if (preset.materialvolume.overlapautobyname)
        {
            sawVolumetricAuto = true;
            // 自动模式：优先级由材质命名推导，故 rules 必须为空——
            // 留着手写规则会让「谁生效」变成隐式行为。
            if (!Check(
                    overlap.value(QStringLiteral("mode")).toString()
                            == QStringLiteral("auto_by_material_name")
                        && rules.isEmpty(),
                    QStringLiteral("MATVOL 自动模式预设 %1 的 overlap 不正确。")
                        .arg(preset.id),
                    errors))
            {
                return false;
            }
            const QJsonObject opacityVarnish = block.value(
                QStringLiteral("opacityVarnish")).toObject();
            if (!Check(
                    preset.materialvolume.opacityvarnishenabled
                        == opacityVarnish.value(
                               QStringLiteral("enabled")).toBool()
                        && (!preset.materialvolume.opacityvarnishenabled
                            || (opacityVarnish.value(
                                    QStringLiteral("semiTransparentRole")).toString()
                                    == QStringLiteral("rgb")
                                && opacityVarnish.value(
                                       QStringLiteral("opacityMax")).toDouble() > 0.0
                                && opacityVarnish.value(
                                       QStringLiteral("opacityMax")).toDouble() < 1.0)),
                    QStringLiteral("MATVOL 自动模式预设 %1 的 opacityVarnish 不正确。")
                        .arg(preset.id),
                    errors))
            {
                return false;
            }
        }
        else if (!Check(
                overlap.value(QStringLiteral("mode")).toString()
                        == QStringLiteral("explicit_priority")
                    && rules.size() == 2
                    && rules.at(0).toObject().value(
                           QStringLiteral("matchMaterialName")).toString()
                        == QStringLiteral("01")
                    && rules.at(0).toObject().value(
                           QStringLiteral("priority")).toInt() == 200
                    && rules.at(1).toObject().value(
                           QStringLiteral("matchMaterialName")).toString()
                        == QStringLiteral("02")
                    && rules.at(1).toObject().value(
                           QStringLiteral("priority")).toInt() == 100,
                QStringLiteral("MATVOL 显式优先级预设 %1 的 overlap 不正确。")
                    .arg(preset.id),
                errors))
        {
            return false;
        }
        if (!Check(
                effective.profile.value(QStringLiteral("slicingMode")).toString()
                    == QStringLiteral("relief_heightfield"),
                QStringLiteral("MATVOL 预设 %1 必须发出 relief_heightfield。")
                    .arg(preset.id),
                errors))
        {
            return false;
        }
    }
    // 自动模式一并纳入收口：否则将来若把该预设删掉，上面的分支会整段失效而无人察觉。
    return Check(
        sawLegacy && sawVolumetric && sawVolumetricAuto,
        QStringLiteral(
            "预设目录必须同时包含旧工艺、MATVOL 显式优先级候选与 MATVOL 自动模式候选。"),
        errors);
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
    const QJsonObject baseProjection = support.value(
        QStringLiteral("baseProjection")).toObject();
    if (!Check(
            output.value(QStringLiteral("dpiX")).toInt() == 635
                && output.value(QStringLiteral("dpiY")).toInt() == 600
                && output.value(
                    QStringLiteral("layerThicknessMm")).toDouble() == 0.038
                && geometrySampling.value(
                    QStringLiteral("strategy")).toString()
                    == QStringLiteral("legacy_center_sample")
                && !first.profile.contains(QStringLiteral("relief")),
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
                && baseProjection.value(
                    QStringLiteral("layerPlacement")).toString()
                    == QStringLiteral("prepend_below_model")
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
                        QStringLiteral("layerCount")).toInt() == 30
                && changedSupport.value(
                    QStringLiteral("baseProjection")).toObject().value(
                        QStringLiteral("layerPlacement")).toString()
                    == QStringLiteral("prepend_below_model"),
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
    const QJsonObject relief = textured.profile.value(
        QStringLiteral("relief")).toObject();
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
                    == QJsonArray{12, 34, 56}
                && relief.value(QStringLiteral("fillMode")).toString()
                    == QStringLiteral("intersection_range")
                && relief.value(QStringLiteral("baseZMm")).toDouble() == 0.0,
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
            QStringLiteral("S3 与普通非 relief Profile 组合必须 fail-closed。"),
            errors))
    {
        return false;
    }

    settings.materialstrategy = HostMaterialStrategy::WhiteSolid;
    hosteffectiveprofile whiteReliefSampled;
    error.clear();
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &whiteReliefSampled, &error)
                && whiteReliefSampled.profile.value(
                    QStringLiteral("slicingMode")).toString()
                    == QStringLiteral("relief_heightfield")
                && !whiteReliefSampled.profile.value(
                    QStringLiteral("texture")).toObject().value(
                    QStringLiteral("enabled")).toBool()
                && whiteReliefSampled.profile.value(
                    QStringLiteral("modelMaterial")).toObject().value(
                    QStringLiteral("materialChannel")).toString()
                    == QStringLiteral("W")
                && ComputeWorkerProfileHash(whiteReliefSampled.profile)
                    == whiteReliefSampled.profilehash,
            QStringLiteral("单材料白墨 S3 浮雕 Profile 构造失败：%1")
                .arg(error),
            errors))
    {
        return false;
    }

    settings.materialstrategy = HostMaterialStrategy::VarnishSolid;
    hosteffectiveprofile varnishReliefSampled;
    error.clear();
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &varnishReliefSampled, &error)
                && varnishReliefSampled.profile.value(
                    QStringLiteral("slicingMode")).toString()
                    == QStringLiteral("relief_heightfield")
                && !varnishReliefSampled.profile.value(
                    QStringLiteral("texture")).toObject().value(
                    QStringLiteral("enabled")).toBool()
                && varnishReliefSampled.profile.value(
                    QStringLiteral("modelMaterial")).toObject().value(
                    QStringLiteral("materialChannel")).toString()
                    == QStringLiteral("V")
                && ComputeWorkerProfileHash(varnishReliefSampled.profile)
                    == varnishReliefSampled.profilehash,
            QStringLiteral("单材料光油 S3 浮雕 Profile 构造失败：%1")
                .arg(error),
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
    settings = MakeSettings(
        modelPath,
        QDir(QFileInfo(outputDirectory).absolutePath()).filePath(
            QStringLiteral("output/ui_sessions")));
    error.clear();
    if (!Check(
            !HostEffectiveProfileBuilder::Validate(settings, &error)
                && error.contains(QStringLiteral("共享会话根目录")),
            QStringLiteral("共享 output/ui_sessions 根目录必须 fail-closed。"),
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
    auto* geometrySampling = panel.findChild<QComboBox*>(
        QStringLiteral("hostGeometrySamplingCombo"));
    auto* tiffCompressionCheck = panel.findChild<QCheckBox*>(
        QStringLiteral("hostTiffCompressionCheck"));
    auto* tiffCompressionCombo = panel.findChild<QComboBox*>(
        QStringLiteral("hostTiffCompressionCombo"));
    auto* materialCombo = panel.findChild<QComboBox*>(
        QStringLiteral("hostSliceMaterialCombo"));
    auto* supportEnabled = panel.findChild<QCheckBox*>(
        QStringLiteral("hostSupportEnabledCheck"));
    auto* supportMode = panel.findChild<QComboBox*>(
        QStringLiteral("hostSupportModeCombo"));
    auto* supportOffset = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostSupportOffsetSpin"));
    auto* supportBaseProjection = panel.findChild<QCheckBox*>(
        QStringLiteral("hostSupportBaseProjectionCheck"));
    auto* supportBaseProjectionLayers = panel.findChild<QSpinBox*>(
        QStringLiteral("hostSupportBaseProjectionLayersSpin"));
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
                && geometrySampling != nullptr
                && tiffCompressionCheck != nullptr
                && tiffCompressionCombo != nullptr
                && materialCombo != nullptr && supportEnabled != nullptr
                && supportMode != nullptr && supportOffset != nullptr
                && supportBaseProjection != nullptr
                && supportBaseProjectionLayers != nullptr
                && roleMapping != nullptr && whiteExpand != nullptr
                && textureEnabled != nullptr
                && textureApplyMode != nullptr
                && textureWhitePolicy != nullptr,
            QStringLiteral("切片设置面板控件不完整。"),
            errors))
    {
        return false;
    }
    if (!Check(
            processPreset->currentData().toString()
                    == HostProcessPresetCatalog::DefaultPresetId()
                && processPreset->currentText()
                    == QStringLiteral(
                        "彩色纹理｜全实体 RGB + 按需补白墨｜下表面支撑"),
            QStringLiteral("新版宿主默认常用工艺未指向按需补白生产方案。"),
            errors)
        || !Check(
            !tiffCompressionCheck->isChecked()
                && !tiffCompressionCombo->isEnabled(),
            QStringLiteral("TIFF 压缩必须默认关闭以保持 RIP 兼容基线。"),
            errors))
    {
        return false;
    }

    const int rgbWhiteIndex = materialCombo->findData(
        QStringLiteral("rgb_white"));
    if (!Check(
            rgbWhiteIndex >= 0
                && materialCombo->itemText(rgbWhiteIndex)
                    == QStringLiteral("RGB 表层 + 白墨实体填充")
                && materialCombo->itemData(
                    rgbWhiteIndex, Qt::ToolTipRole).toString().contains(
                        QStringLiteral("不是‘全实体 RGB + 按需补白墨’")),
            QStringLiteral("普通 RGB+W 与按需补白的材料入口仍存在歧义。"),
            errors))
    {
        return false;
    }
    const QString defaultOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            QFileInfo(defaultOutput).isAbsolute()
                && defaultOutput.contains(
                    QStringLiteral("/output/h"))
                && !defaultOutput.contains(QStringLiteral("/out/"))
                && defaultOutput.endsWith(QStringLiteral("/package")),
            QStringLiteral(
                "参考宿主默认输出目录未使用 output/h<session>/package。"),
            errors))
    {
        return false;
    }
    const QDir outputRoot(
        QFileInfo(QFileInfo(defaultOutput).absolutePath()).absolutePath());
    const QDir sharedSessionRoot(
        outputRoot.filePath(QStringLiteral("ui_sessions")));
    hostslicesettings persistedSettings;
    persistedSettings.outputdirectory = sharedSessionRoot.absolutePath();
    panel.SetPersistentSettings(persistedSettings);
    const QString migratedOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            migratedOutput != QDir::fromNativeSeparators(
                sharedSessionRoot.absolutePath())
                && migratedOutput.contains(
                    QStringLiteral("/output/h"))
                && !migratedOutput.contains(QStringLiteral("/out/"))
                && migratedOutput.endsWith(QStringLiteral("/package")),
            QStringLiteral(
                "持久化的共享 ui_sessions 根目录未迁移到短会话包目录。"),
            errors))
    {
        return false;
    }
    persistedSettings.outputdirectory = QDir(sharedSessionRoot).filePath(
        QStringLiteral("host_20260814_113641_837/package"));
    panel.SetPersistentSettings(persistedSettings);
    const QString migratedLegacyOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            migratedLegacyOutput.contains(QStringLiteral("/output/h"))
                && !migratedLegacyOutput.contains(
                    QStringLiteral("/output/ui_sessions/"))
                && migratedLegacyOutput.endsWith(QStringLiteral("/package")),
            QStringLiteral("旧版自动会话目录未迁移到缩短后的默认目录。"),
            errors))
    {
        return false;
    }
    panel.SetModelPath(modelPath);
    const QString completedAutomaticOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            panel.IsReady()
                && panel.PrepareNextAutomaticOutputDirectory(
                    completedAutomaticOutput),
            QStringLiteral("成功作业后自动输出目录未进入下一会话。"),
            errors))
    {
        return false;
    }
    const QString nextAutomaticOutput = QDir::fromNativeSeparators(
        outputEdit->text());
    if (!Check(
            panel.IsReady()
                && nextAutomaticOutput != completedAutomaticOutput
                && nextAutomaticOutput.contains(QStringLiteral("/output/h"))
                && !nextAutomaticOutput.contains(QStringLiteral("/out/"))
                && nextAutomaticOutput.endsWith(QStringLiteral("/package")),
            QStringLiteral(
                "下一切片会话未保留 output 根或未获得独立短路径。"),
            errors))
    {
        return false;
    }
    outputEdit->setText(outputDirectory);
    if (!Check(
            !panel.PrepareNextAutomaticOutputDirectory(outputDirectory)
                && outputEdit->text() == outputDirectory,
            QStringLiteral("用户自定义输出目录不应被自动轮换。"),
            errors))
    {
        return false;
    }
    client.ResetCallCount();

    tiffCompressionCheck->setChecked(true);
    QCoreApplication::processEvents();
    const hostslicesettings compressedSettings = panel.Settings();
    hosteffectiveprofile compressedProfile;
    QString compressedError;
    if (!Check(
            tiffCompressionCombo->isEnabled()
                && compressedSettings.tiffcompression
                    == HostTiffCompression::PackBits
                && HostEffectiveProfileBuilder::Build(
                    compressedSettings,
                    &compressedProfile,
                    &compressedError)
                && compressedProfile.profile.value(
                    QStringLiteral("output")).toObject().value(
                        QStringLiteral("tiffCompression")).toObject().value(
                            QStringLiteral("algorithm")).toString()
                    == QStringLiteral("packbits"),
            QStringLiteral("TIFF PackBits UI 选项未进入有效 Profile：%1")
                .arg(compressedError),
            errors))
    {
        return false;
    }
    tiffCompressionCheck->setChecked(false);
    QCoreApplication::processEvents();

    if (!Check(
            geometrySampling->currentData().toString()
                == QStringLiteral("legacy_center_sample"),
            QStringLiteral("Stage 16 几何采样必须默认显示 S0。"),
            errors))
    {
        return false;
    }

    const int onDemandIndex = processPreset->findData(
        QStringLiteral("textured_nail_rgb_white_ondemand_lower_support"));
    processPreset->setCurrentIndex(onDemandIndex);
    QCoreApplication::processEvents();
    hostslicesettings presetSettings = panel.Settings();
    if (!Check(
            onDemandIndex > 0
                && processPreset->itemText(onDemandIndex)
                    == QStringLiteral(
                        "彩色纹理｜全实体 RGB + 按需补白墨｜下表面支撑")
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

    const int rgbOnlyIndex = processPreset->findData(
        QStringLiteral("textured_nail_rgb_only_lower_support"));
    processPreset->setCurrentIndex(rgbOnlyIndex);
    supportEnabled->setChecked(false);
    QCoreApplication::processEvents();
    supportEnabled->setChecked(true);
    supportBaseProjection->setChecked(true);
    QCoreApplication::processEvents();
    const hostslicesettings baseProjectionSettings = panel.Settings();
    hosteffectiveprofile baseProjectionProfile;
    QString baseProjectionError;
    if (!Check(
            rgbOnlyIndex > 0
                && processPreset->currentData().toString()
                    == QStringLiteral(
                        "textured_nail_rgb_only_lower_support")
                && baseProjectionSettings.support.enabled
                && baseProjectionSettings.support.baseprojection.enabled
                && baseProjectionSettings.support.baseprojection.layercount
                    == supportBaseProjectionLayers->value()
                && panel.BuildSubmissionProfile(
                    &baseProjectionProfile, &baseProjectionError)
                && [&baseProjectionProfile]()
                {
                    const QJsonObject submittedBase =
                        baseProjectionProfile.profile.value(
                            QStringLiteral("support")).toObject().value(
                            QStringLiteral("baseProjection")).toObject();
                    return submittedBase.value(
                               QStringLiteral("enabled")).toBool()
                        && submittedBase.value(
                               QStringLiteral("layerPlacement")).toString()
                            == QStringLiteral("prepend_below_model");
                }(),
            QStringLiteral(
                "全实体 RGB 工艺重新开启支撑和投影铺底后不应切换为自定义，且必须保持可提交：%1")
                .arg(baseProjectionError),
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

    const int s3Index = geometrySampling->findData(QStringLiteral(
        "layer_slab_supersample_2x2_at_least_two_candidate"));
    geometrySampling->setCurrentIndex(s3Index);
    QCoreApplication::processEvents();
    const hostslicesettings singleMaterialSamplingSettings = panel.Settings();
    hosteffectiveprofile singleMaterialSamplingProfile;
    QString singleMaterialSamplingError;
    if (!Check(
            s3Index >= 0
                && HostEffectiveProfileBuilder::Build(
                    singleMaterialSamplingSettings,
                    &singleMaterialSamplingProfile,
                    &singleMaterialSamplingError)
                && singleMaterialSamplingProfile.profile.value(
                    QStringLiteral("slicingMode")).toString()
                    == QStringLiteral("relief_heightfield")
                && !singleMaterialSamplingProfile.profile.value(
                    QStringLiteral("texture")).toObject().value(
                    QStringLiteral("enabled")).toBool()
                && singleMaterialSamplingProfile.profile.value(
                    QStringLiteral("modelMaterial")).toObject().value(
                    QStringLiteral("materialChannel")).toString()
                    == QStringLiteral("V"),
            QStringLiteral("UI 单材料光油 S3 Profile 构造失败：%1")
                .arg(singleMaterialSamplingError),
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
    geometrySampling->setCurrentIndex(s3Index);
    QCoreApplication::processEvents();
    const hostslicesettings samplingSettings = panel.Settings();
    hosteffectiveprofile samplingProfile;
    QString samplingError;
    const bool samplingBuilt = HostEffectiveProfileBuilder::Build(
        samplingSettings, &samplingProfile, &samplingError);
    if (!Check(
        s3Index >= 0 && samplingBuilt
            && samplingProfile.profile.value(
                QStringLiteral("geometrySampling")).toObject().value(
                    QStringLiteral("strategy")).toString()
                == QStringLiteral(
                    "layer_slab_supersample_2x2_at_least_two_candidate"),
        QStringLiteral("S3 选择未进入有效 Profile/hash：%1")
            .arg(samplingError),
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

    panel.SetSingleMaterialRestriction(
        true,
        QStringLiteral("测试模型缺少 MTL 定义"));
    auto* processModel = qobject_cast<QStandardItemModel*>(
        processPreset->model());
    auto* materialModel = qobject_cast<QStandardItemModel*>(
        materialCombo->model());
    const int rgbPresetIndex = processPreset->findData(
        QStringLiteral("textured_nail_rgb_only_lower_support"));
    const int whitePresetIndex = processPreset->findData(
        QStringLiteral("single_material_relief_white"));
    const int rgbMaterialIndex = materialCombo->findData(
        QStringLiteral("rgb_solid"));
    const int whiteMaterialIndex = materialCombo->findData(
        QStringLiteral("white_solid"));
    if (!Check(
            processModel != nullptr && materialModel != nullptr
                && rgbPresetIndex >= 0 && whitePresetIndex >= 0
                && rgbMaterialIndex >= 0 && whiteMaterialIndex >= 0
                && !processModel->item(rgbPresetIndex)->isEnabled()
                && processModel->item(whitePresetIndex)->isEnabled()
                && !materialModel->item(rgbMaterialIndex)->isEnabled()
                && materialModel->item(whiteMaterialIndex)->isEnabled()
                && panel.Settings().materialstrategy
                    == HostMaterialStrategy::WhiteSolid,
            QStringLiteral(
                "外观资源不完整时未把工艺限定为单材料白墨/光油。"),
            errors))
    {
        return false;
    }
    /* MV-07B：MATVOL 候选在能力不足时必须【禁用而非静默回退】。 */
    const int matvolPresetIndex = processPreset->findData(
        QStringLiteral("volumetric_nail_rgb_white_ondemand_lower_support"));
    auto* matvolEnabled = panel.findChild<QCheckBox*>(
        QStringLiteral("hostMatvolEnabledCheck"));
    auto* matvolPrimaryName = panel.findChild<QLineEdit*>(
        QStringLiteral("hostMatvolPrimaryNameEdit"));
    auto* matvolHint = panel.findChild<QLabel*>(
        QStringLiteral("hostMatvolCapabilityHint"));
    if (!Check(
            matvolPresetIndex > 0,
            QStringLiteral(
                "MV-07B：MATVOL 候选工艺未出现在工艺目录中。"),
            errors))
    {
        return false;
    }
    {
        processPreset->setCurrentIndex(matvolPresetIndex);
        QCoreApplication::processEvents();
        const hostslicesettings beforeRestriction = panel.Settings();
        panel.SetSingleMaterialRestriction(
            true,
            QStringLiteral("测试模型缺少 MTL 定义"));
        QCoreApplication::processEvents();
        const hostslicesettings afterRestriction = panel.Settings();
        hosteffectiveprofile blockedProfile;
        QString blockedError;
        if (!Check(
                matvolEnabled != nullptr && matvolPrimaryName != nullptr
                    && matvolHint != nullptr
                    && beforeRestriction.materialvolume.enabled
                    && processPreset->currentData().toString()
                        == QStringLiteral(
                            "volumetric_nail_rgb_white_ondemand_lower_support")
                    && afterRestriction.materialvolume.enabled
                    && !matvolPrimaryName->isEnabled()
                    && !matvolEnabled->isEnabled()
                    && matvolHint->text().contains(
                           QStringLiteral("多材质纵深不可用"))
                    && !panel.BuildSubmissionProfile(
                        &blockedProfile, &blockedError)
                    && !blockedError.isEmpty(),
                QStringLiteral(
                    "MV-07B：能力不足时 MATVOL 应禁用编辑并拒绝提交，而不是静默改工艺。"),
                errors))
        {
            return false;
        }
        panel.SetSingleMaterialRestriction(false, QString{});
        QCoreApplication::processEvents();
        if (!Check(
                matvolEnabled->isEnabled()
                    && matvolPrimaryName->isEnabled(),
                QStringLiteral(
                    "MV-07B：清除限制后 MATVOL 编辑未恢复。"),
                errors))
        {
            return false;
        }
        processPreset->setCurrentIndex(rgbPresetIndex);
        QCoreApplication::processEvents();
    }
    panel.SetSingleMaterialRestriction(false, QString{});
    if (!Check(
            processModel->item(rgbPresetIndex)->isEnabled()
                && materialModel->item(rgbMaterialIndex)->isEnabled(),
            QStringLiteral("清除单材料限制后 RGB 工艺未恢复可选。"),
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

bool VerifyStage16Diagnostics(QTextStream& errors)
{
    HostSliceJobPanel jobPanel;
    jobPanel.SetStage16Context(QStringLiteral(
        "layer_slab_supersample_2x2_at_least_two_candidate"));
    auto* jobContext = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostStage16JobContextLabel"));
    auto* progressBar = jobPanel.findChild<QProgressBar*>(
        QStringLiteral("hostSliceJobProgressBar"));
    auto* pendingConfigLoadValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingConfigLoadValue"));
    auto* liveWorkerTotalValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingWorkerTotalValue"));
    auto* liveHostTotalValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingHostTotalValue"));
    jobPanel.SetActive();
    jobPanel.UpdateProgress(
        QStringLiteral("running"),
        QStringLiteral("completed"),
        1,
        1,
        100,
        18);
    jobPanel.UpdateLiveTiming(QJsonObject{
        {QStringLiteral("approximate"), true},
        {QStringLiteral("pollResolutionMs"), 100},
        {QStringLiteral("configLoadMs"), 1.5},
        {QStringLiteral("modelLoadMs"), 2.5},
        {QStringLiteral("totalMs"), 7.0},
        {QStringLiteral("workerElapsedMs"), 7.0},
        {QStringLiteral("hostElapsedMs"), 18.0}});
    const bool runningStateIsHonest = progressBar != nullptr
        && progressBar->value() == 99
        && pendingConfigLoadValue != nullptr
        && pendingConfigLoadValue->text()
            .contains(QStringLiteral("估算"))
        && liveWorkerTotalValue != nullptr
        && liveWorkerTotalValue->text().contains(QStringLiteral("7.0 ms"))
        && liveHostTotalValue != nullptr
        && liveHostTotalValue->text().contains(QStringLiteral("18.0 ms"));
    QJsonObject timing{
        {QStringLiteral("available"), true},
        {QStringLiteral("approximate"), true},
        {QStringLiteral("engine"), QStringLiteral("legacy-scene")},
        {QStringLiteral("configLoadMs"), 1.5},
        {QStringLiteral("modelLoadMs"), 2.5},
        {QStringLiteral("sliceProcessingMs"), 12.5},
        {QStringLiteral("supportStatisticsScanCount"), 3},
        {QStringLiteral("totalMs"), 18.0}};
    jobPanel.ShowCompletion(
        true,
        false,
        QString{},
        QString{},
        QString{},
        QStringLiteral("package"),
        timing,
        20,
        -1);
    auto* scanValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSupportStatisticsScanValue"));
    auto* engineValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingEngineValue"));
    auto* configLoadValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingConfigLoadValue"));
    auto* modelLoadValue = jobPanel.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingModelLoadValue"));

    HostPackageReviewPanel reviewPanel;
    hostpackagereview review;
    review.valid = true;
    review.layercount = 2;
    hostlayerdescriptor first;
    first.layerindex = 0;
    first.printpixels.values = {10U, 20U, 30U, 40U, 50U, 60U};
    hostlayerdescriptor current = first;
    current.layerindex = 1;
    current.printpixels.values = {15U, 18U, 30U, 44U, 45U, 70U};
    review.layers = {first, current};
    reviewPanel.SetStage16Context(
        QStringLiteral("legacy_center_sample"), timing);
    reviewPanel.SetPackage(review);
    auto* layerSpin = reviewPanel.findChild<QSpinBox*>(
        QStringLiteral("hostPackageLayerSpin"));
    auto* summary = reviewPanel.findChild<QLabel*>(
        QStringLiteral("hostPackageStage16SummaryLabel"));
    auto* previewImage = reviewPanel.findChild<QLabel*>(
        QStringLiteral("hostPackagePreviewImage"));
    auto* referencePreview = reviewPanel.findChild<QLabel*>(
        QStringLiteral("hostPackageReferencePreviewImage"));
    auto* previewMode = reviewPanel.findChild<QComboBox*>(
        QStringLiteral("hostPackagePreviewModeCombo"));
    auto* referenceCaption = reviewPanel.findChild<QLabel*>(
        QStringLiteral("hostPackageReferencePreviewCaption"));
    auto* currentCaption = reviewPanel.findChild<QLabel*>(
        QStringLiteral("hostPackageCurrentPreviewCaption"));
    reviewPanel.SetStage16Context(
        QStringLiteral("legacy_center_sample"), timing);
    layerSpin->setValue(1);
    QCoreApplication::processEvents();

    return Check(
               runningStateIsHonest,
               QStringLiteral(
                   "非终结态不得显示 100%，已观测阶段应显示实时估算。"),
               errors)
        && Check(
               jobContext != nullptr
                   && jobContext->text().contains(QStringLiteral("S3"))
                   && jobContext->text().contains(QStringLiteral("P3")),
               QStringLiteral("Stage 16 作业策略/姿态摘要缺失。"),
               errors)
        && Check(
               scanValue != nullptr
                   && scanValue->text().contains(QStringLiteral("3 次")),
               QStringLiteral("支撑统计扫描 telemetry 未显示。"),
               errors)
        && Check(
               engineValue != nullptr && configLoadValue != nullptr
                   && modelLoadValue != nullptr
                   && engineValue->text().contains(
                       QStringLiteral("失败前阶段进度估算"))
                   && configLoadValue->text() != QStringLiteral("未提供")
                   && modelLoadValue->text() != QStringLiteral("未提供"),
               QStringLiteral("失败作业的阶段耗时估算未显示。"),
               errors)
        && Check(
               previewMode != nullptr
                    && previewMode->currentData().toStringList()
                        == QStringList({
                            QStringLiteral("R"),
                            QStringLiteral("G"),
                            QStringLiteral("B"),
                            QStringLiteral("W"),
                            QStringLiteral("S"),
                            QStringLiteral("V")})
                    && previewImage != nullptr
                    && referencePreview == nullptr
                    && referenceCaption == nullptr
                    && currentCaption == nullptr,
               QStringLiteral(
                    "结果预览应恢复单视图并默认显示全通道组合，使支撑等非 RGB 通道默认可见。"),
               errors)
        && Check(
               summary != nullptr
                    && summary->text().contains(
                        QStringLiteral("当前生产层 layer=1"))
                    && summary->text().contains(QStringLiteral("R=15"))
                    && summary->text().contains(QStringLiteral("S=45")),
               QStringLiteral("当前生产层通道摘要缺失：%1")
                   .arg(summary != nullptr
                       ? summary->text()
                       : QStringLiteral("summary=null")),
               errors)
        && Check(
               previewMode != nullptr
                   && !previewMode->itemData(0, Qt::ToolTipRole)
                           .toString()
                           .isEmpty()
                   && AllPseudoColourItemsExplainThemselves(previewMode),
               QStringLiteral(
                   "MV-07C：预览模式条目必须带伪彩色说明的 tooltip。"),
               errors)
        && Check(
               summary != nullptr
                   && summary->text().contains(
                          QStringLiteral("通道显示："))
                   && summary->text().contains(
                          QStringLiteral("W/S/V 为显示用伪彩色"))
                   && summary->text().contains(
                          QStringLiteral("不代表生产 TIFF 像素值")),
               QStringLiteral(
                   "MV-07C：Stage 16 摘要必须标注 W/S/V 为伪彩色。"),
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
    if (!VerifyMaterialVolumeConditionalEmission(
            modelPath, outputDirectory, errors)
        || !VerifyPresetProfileHashClosure(
            modelPath, outputDirectory, errors)
        || !VerifyEffectiveProfiles(modelPath, outputDirectory, errors)
        || !VerifyPanelIsLocal(
            client, modelPath, outputDirectory, errors)
        || !VerifyStage16Diagnostics(errors)
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
