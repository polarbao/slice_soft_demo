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

    // MATOPQ：多图层透明→光油。相对上一条候选的差别有四处，缺一不可：
    //   1) overlap 改为按材质命名自动推导优先级——多图层素材数超过
    //      primary/secondary 两个名字槽，手填不可行；
    //   2) 由 MTL 的 d 值把全透明素材判入 V 通道，不依赖材质名；
    //   3) 退化面阈值收紧到 1e-24——CAD/NURBS 导出含 nm^2 级合法薄面，
    //      默认门 1e-6 mm^2 会误杀它们，使材质被判为开放表面而直接拒绝；
    //   4) texture 必须【开启】——MATOPQ-RGB 的逐材质贴图采样以它为前置，
    //      build_per_material_texture_columns 在 texture 关闭时直接返回空表，
    //      各图层就只剩自身 Kd 单色、拿不到 map_Kd。
    //      本项在 MO-11 建这条预设时曾设为 false：当时 MATOPQ-RGB 尚未落地，
    //      多图层贴图不可用，开启 texture 只会让逐列顶面取色写入错误颜色
    //      （下层区域被填成上层材质的 Kd）。M2 落地后该理由已不成立。
    // 资产须遵循 DOC_SPEC_MATERIAL_NAMING 的 <素材名>-L<层号> 命名，
    // 且各图层需在 MTL 中各自声明 map_Kd 才能取到自己的贴图。
    hostprocesspreset multiLayerVarnish = MakeTexturedPreset(
        QStringLiteral("multilayer_transparent_varnish_lower_support"),
        QStringLiteral("多图层透明→光油｜逐层材质 RGB + 按需补白墨｜下表面支撑（候选）"),
        QStringLiteral(
            "按封闭材质子网格逐层解析材质所有权；"
            "材质优先级由 <素材名>-L<层号> 命名自动推导，无需手填；"
            "MTL 中 d 判为全透明的素材写入光油 V 通道且不补白墨底；"
            "各图层按自身顶面 UV 采样各自的 map_Kd，被上层遮住的下层贴图亦可取到。"
            "退化面阈值收紧以容纳 CAD 导出的极小薄面。"
            "要求资产遵循 <素材名>-L<层号> 命名，内嵌关系须分层。"),
        HostMaterialStrategy::RgbSolid,
        HostTextureApplyMode::SolidVolumeFromTopSurface);
    // 见上方第 4 点：逐材质贴图采样以 texture 开启为前置，不得改回 false。
    multiLayerVarnish.texture.enabled = true;
    multiLayerVarnish.texture.whitepolicy =
        HostTextureWhitePolicy::WhiteUnderbase;
    multiLayerVarnish.materialvolume.enabled = true;
    multiLayerVarnish.materialvolume.overlapautobyname = true;
    multiLayerVarnish.materialvolume.opacityvarnishenabled = true;
    multiLayerVarnish.materialvolume.opacityvarnishmax = 0.001;
    multiLayerVarnish.materialvolume.degenerateareaepsilonmm2 = 1e-24;
    presets.push_back(multiLayerVarnish);

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
