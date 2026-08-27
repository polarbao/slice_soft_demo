#include "HostProcessPresetCatalog.h"
#include "HostTransferProcessPresetLoader.h"

namespace
{
hostprocesspreset MakeTexturedPreset(
    const QString& id,
    const QString& displayName,
    const QString& description,
    const HostMaterialStrategy strategy,
    const HostTextureApplyMode applyMode)
{
    hostprocesspreset preset;
    preset.id = id;
    preset.displayname = displayName;
    preset.description = description;
    preset.materialstrategy = strategy;
    preset.texture.enabled = true;
    preset.texture.applymode = applyMode;
    return preset;
}

hostprocesspreset MakeSingleMaterialPreset(
    const QString& id,
    const QString& displayName,
    const QString& description,
    const HostMaterialStrategy strategy)
{
    hostprocesspreset preset;
    preset.id = id;
    preset.displayname = displayName;
    preset.description = description;
    preset.materialstrategy = strategy;
    preset.texture.enabled = false;
    return preset;
}

void AppendTransferPreset(
    const hostprocesspreset& legacyPreset,
    const QString& configFileName,
    QVector<hostprocesspreset>* presets)
{
    hosttransferchannelsettings transfer;
    if (!HostTransferProcessPresetLoader::Load(
            configFileName, &transfer, nullptr))
    {
        return;
    }
    hostprocesspreset preset = legacyPreset;
    preset.id += QStringLiteral("_rgbwsvt");
    preset.displayname += QStringLiteral("｜缩裹 T 通道");
    preset.description += QStringLiteral(
        " 新版工艺从外部 JSON 读取缩裹材质 RGB/Kd 与拓扑策略，"
        "显式输出 p0.rgbwsvt.1。");
    preset.packageprotocol = HostPackageProtocol::Rgbwsvt;
    preset.transferchannel = transfer;
    presets->push_back(preset);
}
}

QString HostProcessPresetCatalog::DefaultPresetId()
{
    return QStringLiteral(
        "textured_nail_rgb_white_ondemand_lower_support");
}

QVector<hostprocesspreset> HostProcessPresetCatalog::Presets()
{
    QVector<hostprocesspreset> presets;
    presets.reserve(10);
    const hostprocesspreset rgbOnly = MakeTexturedPreset(
        QStringLiteral("textured_nail_rgb_only_lower_support"),
        QStringLiteral(
            "彩色纹理｜全实体 RGB｜下表面支撑（纯白阻断）"),
        QStringLiteral(
            "对应旧版全 RGB 兼容工艺：模型实体只写 RGB，保留下表面支撑；"
            "严格纯白纹理没有可打印通道时会阻断切片。"),
        HostMaterialStrategy::RgbSolid,
        HostTextureApplyMode::SolidVolumeFromTopSurface);
    presets.push_back(rgbOnly);

    hostprocesspreset rgbWhite = MakeTexturedPreset(
        QStringLiteral("textured_nail_rgb_white_lower_support"),
        QStringLiteral(
            "彩色纹理｜RGB 表层 + 白墨实体填充｜下表面支撑"),
        QStringLiteral(
            "对应旧版 RGB+白墨工艺：顶面纹理写 RGB，模型内部写白墨 W，"
            "并生成下表面及内部镂空支撑。"),
        HostMaterialStrategy::RgbWhite,
        HostTextureApplyMode::TopSurfaceBand);
    rgbWhite.materialprocess.rolemappingenabled = true;
    presets.push_back(rgbWhite);

    hostprocesspreset onDemandWhite = MakeTexturedPreset(
        QStringLiteral("textured_nail_rgb_white_ondemand_lower_support"),
        QStringLiteral(
            "彩色纹理｜全实体 RGB + 按需补白墨｜下表面支撑"),
        QStringLiteral(
            "Stage 15 工艺：保持全实体 RGB，仅在不可打印纯白纹理像素的"
            "同层写入 W 载体；不写 V。"),
        HostMaterialStrategy::RgbSolid,
        HostTextureApplyMode::SolidVolumeFromTopSurface);
    onDemandWhite.texture.whitepolicy =
        HostTextureWhitePolicy::WhiteUnderbase;
    presets.push_back(onDemandWhite);

    hostprocesspreset rgbVarnish = MakeTexturedPreset(
        QStringLiteral("textured_nail_rgb_varnish_lower_support"),
        QStringLiteral(
            "彩色纹理｜RGB 表层 + 光油实体填充｜下表面支撑"),
        QStringLiteral(
            "顶面纹理写 RGB，模型内部写光油 V，并生成下表面及内部镂空支撑。"),
        HostMaterialStrategy::RgbVarnish,
        HostTextureApplyMode::TopSurfaceBand);
    rgbVarnish.materialprocess.rolemappingenabled = true;
    presets.push_back(rgbVarnish);

    const hostprocesspreset whiteOnly = MakeSingleMaterialPreset(
        QStringLiteral("single_material_relief_white"),
        QStringLiteral("单材料浮雕｜白墨 W 实体｜下表面支撑"),
        QStringLiteral(
            "不采样彩色纹理，模型实体只写白墨 W，并保留下表面支撑。"),
        HostMaterialStrategy::WhiteSolid);
    presets.push_back(whiteOnly);
    const hostprocesspreset varnishOnly = MakeSingleMaterialPreset(
        QStringLiteral("single_material_relief_varnish"),
        QStringLiteral("单材料浮雕｜光油 V 实体｜下表面支撑"),
        QStringLiteral(
            "不采样彩色纹理，模型实体只写光油 V，并保留下表面支撑。"),
        HostMaterialStrategy::VarnishSolid);
    presets.push_back(varnishOnly);
    hostprocesspreset volumetricRgb = MakeTexturedPreset(
        QStringLiteral("volumetric_nail_rgb_white_ondemand_lower_support"),
        QStringLiteral("多材质纵深｜逐层材质 RGB + 按需补白墨｜下表面支撑（候选）"),
        QStringLiteral(
            "候选工艺：按封闭材质子网格逐层解析材质所有权并合成 RGB，"
            "最终 RGB 之后复用按需补白。开放材质表面默认拒绝，"
            "材质重叠需显式优先级。生产接线未完成，仅供候选评估。"),
        HostMaterialStrategy::RgbSolid,
        HostTextureApplyMode::SolidVolumeFromTopSurface);
    volumetricRgb.texture.enabled = false;
    volumetricRgb.texture.whitepolicy =
        HostTextureWhitePolicy::WhiteUnderbase;
    volumetricRgb.materialvolume.enabled = true;
    volumetricRgb.materialvolume.primarymaterialname = QStringLiteral("01");
    volumetricRgb.materialvolume.primarypriority = 200;
    volumetricRgb.materialvolume.secondarymaterialname = QStringLiteral("02");
    volumetricRgb.materialvolume.secondarypriority = 100;
    presets.push_back(volumetricRgb);

    AppendTransferPreset(
        rgbOnly,
        QStringLiteral("obj_mtl_texture_rgb_only_rgbwsvt.json"),
        &presets);
    AppendTransferPreset(
        whiteOnly,
        QStringLiteral("nail_white_underbase_only_rgbwsvt.json"),
        &presets);
    AppendTransferPreset(
        varnishOnly,
        QStringLiteral("nail_varnish_only_rgbwsvt.json"),
        &presets);
    // 按需补白墨 + 缩裹 T 通道。工艺文件此前已随部署包发布，但宿主漏了这次派生，
    // 于是「甲片贴图走 RGB、纯白像素补 W、缩裹走 T」这一组合在 UI 上无从选择——
    // 而它正是带贴图甲片（如 08-03.obj，材质 01 为纯白 Kd + map_Kd）的目标工艺。
    AppendTransferPreset(
        onDemandWhite,
        QStringLiteral("obj_mtl_texture_rgb_white_ondemand_rgbwsvt.json"),
        &presets);

    return presets;
}

bool HostProcessPresetCatalog::Resolve(
    const QString& presetId,
    hostprocesspreset* preset)
{
    if (preset == nullptr)
    {
        return false;
    }
    for (const hostprocesspreset& candidate : Presets())
    {
        if (candidate.id == presetId)
        {
            *preset = candidate;
            return true;
        }
    }
    return false;
}
