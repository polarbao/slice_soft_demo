#include "HostProcessPresetCatalog.h"

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
}

QVector<hostprocesspreset> HostProcessPresetCatalog::Presets()
{
    QVector<hostprocesspreset> presets;
    presets.reserve(6);
    presets.push_back(MakeTexturedPreset(
        QStringLiteral("textured_nail_rgb_only_lower_support"),
        QStringLiteral(
            "彩色纹理｜全实体 RGB｜下表面支撑（纯白阻断）"),
        QStringLiteral(
            "对应旧版全 RGB 兼容工艺：模型实体只写 RGB，保留下表面支撑；"
            "严格纯白纹理没有可打印通道时会阻断切片。"),
        HostMaterialStrategy::RgbSolid,
        HostTextureApplyMode::SolidVolumeFromTopSurface));

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
            "彩色纹理｜全实体 RGB + 纯白按需补 W｜下表面支撑"),
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

    presets.push_back(MakeSingleMaterialPreset(
        QStringLiteral("single_material_relief_white"),
        QStringLiteral("单材料浮雕｜白墨 W 实体｜下表面支撑"),
        QStringLiteral(
            "不采样彩色纹理，模型实体只写白墨 W，并保留下表面支撑。"),
        HostMaterialStrategy::WhiteSolid));
    presets.push_back(MakeSingleMaterialPreset(
        QStringLiteral("single_material_relief_varnish"),
        QStringLiteral("单材料浮雕｜光油 V 实体｜下表面支撑"),
        QStringLiteral(
            "不采样彩色纹理，模型实体只写光油 V，并保留下表面支撑。"),
        HostMaterialStrategy::VarnishSolid));
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
