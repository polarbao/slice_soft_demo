#include "HelpTextProvider.h"

namespace
{

const QString kMaterialDoc = QStringLiteral(
    "docs/slice/PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md");
const QString kWorkbenchDoc = QStringLiteral(
    "docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md");
const QString kOpenVdbDoc = QStringLiteral(
    "docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md");
const QString kDpiDoc = QStringLiteral(
    "docs/slice/PRD/PRD_12E_09C_XY_DPI配置与生产协议兼容.md");

SettingHelpMetadata MakeMetadata(
    const QString& key,
    const QString& title,
    const QString& description,
    const QStringList& affects,
    const QString& defaultValue,
    const QString& productionSafety,
    const QString& docPath)
{
    SettingHelpMetadata metadata;
    metadata.key = key;
    metadata.title = title;
    metadata.description = description;
    metadata.affects = affects;
    metadata.defaultvalue = defaultValue;
    metadata.productionsafety = productionSafety;
    metadata.docpath = docPath;
    return metadata;
}

const QVector<SettingHelpMetadata>& MetadataEntries()
{
    static const QVector<SettingHelpMetadata> entries{
        MakeMetadata(
            QStringLiteral("input.modelPath"),
            QStringLiteral("模型文件"),
            QStringLiteral("选择待切片的 OBJ、STL 或 3MF 模型；OBJ 的 MTL 与贴图通常位于模型同级目录。"),
            {QStringLiteral("输入几何"), QStringLiteral("RGB 纹理")},
            QStringLiteral("由用户选择"),
            QStringLiteral("生产可用；输入格式必须与所选 Profile 匹配"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("output.packageDir"),
            QStringLiteral("输出目录"),
            QStringLiteral("保存本次会话 generated config、RGBWSV TIFF、manifest、报告与调试预览。"),
            {QStringLiteral("输出包")},
            QStringLiteral("output/ui_sessions/<会话>/package"),
            QStringLiteral("生产可用；不得覆盖只读 Profile 模板"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("output.dpiX"),
            QStringLiteral("X 方向 DPI"),
            QStringLiteral("控制 X 方向栅格密度；它不改变模型物理缩放。物理像素宽度按 25.4 / dpiX 计算。"),
            {QStringLiteral("TIFF 宽度"), QStringLiteral("X 物理像素尺寸"), QStringLiteral("输出体积")},
            QStringLiteral("635 dpi（0.040000 mm/px）"),
            QStringLiteral("配置范围 72..2400；首批设备认证组合为 600/600 与 635/600"),
            kDpiDoc),
        MakeMetadata(
            QStringLiteral("output.dpiY"),
            QStringLiteral("Y 方向 DPI"),
            QStringLiteral("控制 Y 方向栅格密度；它不改变模型物理缩放。物理像素高度按 25.4 / dpiY 计算。"),
            {QStringLiteral("TIFF 高度"), QStringLiteral("Y 物理像素尺寸"), QStringLiteral("输出体积")},
            QStringLiteral("600 dpi（0.042333 mm/px）"),
            QStringLiteral("配置范围 72..2400；首批设备认证组合为 600/600 与 635/600"),
            kDpiDoc),
        MakeMetadata(
            QStringLiteral("output.layerThicknessMm"),
            QStringLiteral("切片层高"),
            QStringLiteral("控制 Z 方向每层厚度，直接影响层数、几何采样量、输出体积与耗时。"),
            {QStringLiteral("全部材料通道"), QStringLiteral("层数")},
            QStringLiteral("0.038 mm"),
            QStringLiteral("生产可用；必须大于 0"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("modelTransform.scale"),
            QStringLiteral("模型缩放"),
            QStringLiteral("分别控制模型 X/Y/Z 物理尺寸倍率；1.0 表示保持模型原始尺寸，不进行缩放。"),
            {QStringLiteral("模型物理尺寸"), QStringLiteral("输出像素尺寸"), QStringLiteral("切片层数")},
            QStringLiteral("X/Y/Z 均为 1.0"),
            QStringLiteral("生产可用；非 1.0 会直接改变成品尺寸，必须由用户明确设置"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("texture.applyMode"),
            QStringLiteral("纹理应用策略"),
            QStringLiteral("决定贴图 RGB 如何映射到模型表面；SDF 表面壳层仍属于 OpenVDB 实验路径。"),
            {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")},
            QStringLiteral("顶面纹理带"),
            QStringLiteral("生产安全性取决于选项；OpenVDB 壳层仅限候选/诊断"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("texture.nonSurfaceRgbPolicy"),
            QStringLiteral("非表面 RGB 策略"),
            QStringLiteral("控制纹理表层之外的模型区域是否继续写 RGB；模型内部填充材料由模型填充设置单独决定。"),
            {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")},
            QStringLiteral("使用模型材料"),
            QStringLiteral("生产可用；不得造成生产 Profile 内部材料为空"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("modelFill.material"),
            QStringLiteral("模型内部填充材料"),
            QStringLiteral("填充颜色表层之间的模型实体区域。白墨写 W，光油写 V；兼容模式可将贴图颜色投影到全实体 RGB。它不同于模型外部 S 通道支撑。"),
            {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"), QStringLiteral("W"), QStringLiteral("V")},
            QStringLiteral("生产 Profile 默认白墨，可显式选择光油或全实体 RGB 兼容模式"),
            QStringLiteral("生产 Profile 必须启用且不允许空填充"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("materialPolicy.white.enabled"),
            QStringLiteral("白墨叠加底层策略"),
            QStringLiteral("在模型区域额外叠加 W 通道白墨；它不同于“模型内部填充材料=白墨”，后者只填充颜色表层之间的模型实体区域。"),
            {QStringLiteral("W")},
            QStringLiteral("关闭；模型内部白墨填充由 Profile 单独决定"),
            QStringLiteral("生产可用；仅在确实需要全模型白墨底层时开启，避免与模型内部填充语义混淆"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("materialPolicy.varnish.enabled"),
            QStringLiteral("顶部光油材料策略"),
            QStringLiteral("按顶部层数或既有材料策略写 V 通道，不会在模型 XY 外轮廓之外扩张。"),
            {QStringLiteral("V")},
            QStringLiteral("关闭"),
            QStringLiteral("生产可用；与表面光油、外侧光油壳层语义分离"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("materialPolicy.varnish.topLayers"),
            QStringLiteral("顶部光油层数"),
            QStringLiteral("指定顶部光油材料策略覆盖的最上方层数；0 表示不按顶部层数生成。"),
            {QStringLiteral("V")},
            QStringLiteral("0 层"),
            QStringLiteral("生产可用；仅在顶部光油策略启用时生效"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.enabled"),
            QStringLiteral("支撑材料"),
            QStringLiteral("在模型外部或内部封闭镂空区域写入可剥离支撑材料；支撑只写 S 通道。"),
            {QStringLiteral("S")},
            QStringLiteral("生产 Profile 默认启用"),
            QStringLiteral("生产可用；冲突时保持 Model > OuterVarnishShell > Support > Empty"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.placement"),
            QStringLiteral("支撑位置"),
            QStringLiteral("选择下表面、上表面、双面、悬空区域或完整垂直投影；上表面支撑位于外侧光油壳层之外。"),
            {QStringLiteral("S")},
            QStringLiteral("下表面"),
            QStringLiteral("生产可用；应按模型摆放与工艺要求选择"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.internalVoid.enabled"),
            QStringLiteral("内部镂空支撑"),
            QStringLiteral("将每层模型轮廓内部的封闭空洞写入 S 通道，避免横截面出现无材料支撑区域。"),
            {QStringLiteral("S")},
            QStringLiteral("启用"),
            QStringLiteral("生产 Profile 默认启用；Model 材料优先于 Support"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.internalVoid.minAreaPx"),
            QStringLiteral("镂空最小面积"),
            QStringLiteral("忽略小于阈值的封闭空洞，单位为像素面积；0 表示不按面积过滤。"),
            {QStringLiteral("S")},
            QStringLiteral("16 px"),
            QStringLiteral("生产可用；过大的阈值可能漏掉需要支撑的小空洞"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.baseProjection.enabled"),
            QStringLiteral("支撑投影铺底"),
            QStringLiteral("在模型下方新增若干物理层，并将普通支撑跨层最大投影写入这些层的 S 通道；不会覆盖模型材料或外侧光油。"),
            {QStringLiteral("S"), QStringLiteral("低层支撑连续性")},
            QStringLiteral("生产 UI 默认启用"),
            QStringLiteral("生产可用；必须先保证模型正面朝上并落在 Z=0，不能用于掩盖错误摆放"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("support.baseProjection.layerCount"),
            QStringLiteral("支撑铺底层数"),
            QStringLiteral("控制模型下方新增的支撑铺底物理层数；30 表示新增 layerIndex 0..29，模型整体上移 30 层，因此 TIFF 总层数增加 30。"),
            {QStringLiteral("S"), QStringLiteral("铺底厚度")},
            QStringLiteral("30 层"),
            QStringLiteral("生产可用；范围 0..1000，0 表示不产生铺底层"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("surfaceVarnish.enabled"),
            QStringLiteral("表面光油"),
            QStringLiteral("在模型已有表面像素上写 V 通道，不扩张模型 XY 外轮廓；默认不应产生额外光油。"),
            {QStringLiteral("V")},
            QStringLiteral("关闭"),
            QStringLiteral("生产可用；必须由用户或 Profile 显式启用"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("surfaceVarnish.thicknessPx"),
            QStringLiteral("表面光油厚度"),
            QStringLiteral("控制模型表面内侧的光油像素厚度；0 表示不生成表面光油。"),
            {QStringLiteral("V")},
            QStringLiteral("0 px"),
            QStringLiteral("生产可用；与外侧光油厚度单位不同"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("outerVarnish.enabled"),
            QStringLiteral("外侧光油壳层"),
            QStringLiteral("在模型 XY 外轮廓之外扩张生成 V 通道光油壳层，允许改变最终打印外形尺寸。"),
            {QStringLiteral("V"), QStringLiteral("XY 外轮廓")},
            QStringLiteral("关闭"),
            QStringLiteral("生产可用；默认关闭且厚度为 0 mm"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("outerVarnish.thicknessMm"),
            QStringLiteral("外侧光油厚度"),
            QStringLiteral("按毫米配置外侧光油壳层厚度；X/Y 扩张像素分别依据 output.dpiX/dpiY 换算。pixelPitchUm 仅保留旧配置兼容。"),
            {QStringLiteral("V"), QStringLiteral("XY 外轮廓")},
            QStringLiteral("0.00 mm，步进 0.01 mm"),
            QStringLiteral("生产可用；启用后会扩张模型 XY 尺寸"),
            kMaterialDoc),
        MakeMetadata(
            QStringLiteral("preview.enabled"),
            QStringLiteral("自动生成诊断图"),
            QStringLiteral("开启后额外生成 preview PNG/PPM；关闭时 UI 仍直接读取生产 RGBWSV TIFF，不影响正式预览和 TIFF 输出。"),
            {QStringLiteral("诊断图"), QStringLiteral("保存耗时")},
            QStringLiteral("关闭"),
            QStringLiteral("仅用于诊断与兼容；默认关闭，不能替代 TIFF 真源"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("preview.outputPolicy"),
            QStringLiteral("预览输出策略"),
            QStringLiteral("tiff_native 只写生产 TIFF；tiff_native_with_diagnostics 额外自动生成诊断图。该字段优先于旧 enabled 字段。"),
            {QStringLiteral("生产 TIFF"), QStringLiteral("诊断图 I/O")},
            QStringLiteral("tiff_native"),
            QStringLiteral("生产预览始终以 RGBWSV TIFF 为真源"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("preview.interval"),
            QStringLiteral("诊断图保存间隔"),
            QStringLiteral("启用自动诊断图后，每隔指定层数保存一组图片；值越小，图片数量与保存耗时越高。"),
            {QStringLiteral("诊断图"), QStringLiteral("I/O 耗时")},
            QStringLiteral("10 层"),
            QStringLiteral("不改变生产 TIFF；仅影响调试图片密度"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("engine.legacy"),
            QStringLiteral("Legacy 生产切片引擎"),
            QStringLiteral("当前默认生产路径，负责生成正式 p0.rgbwsv.2 RGBWSV 输出包。"),
            {QStringLiteral("几何切片"), QStringLiteral("生产 RGBWSV")},
            QStringLiteral("启用"),
            QStringLiteral("当前生产默认；仍须通过配置、几何和输出校验"),
            kWorkbenchDoc),
        MakeMetadata(
            QStringLiteral("engine.openvdbCandidate"),
            QStringLiteral("OpenVDB 候选/诊断引擎"),
            QStringLiteral("仅用于 SDF utility、几何诊断与候选验证；关闭后继续使用 Legacy 生产引擎。"),
            {QStringLiteral("实验几何诊断")},
            QStringLiteral("关闭"),
            QStringLiteral("非生产；productionReplacementAllowed=false，不写生产 RGBWSV"),
            kOpenVdbDoc),
    };
    return entries;
}

}  // namespace

bool SettingHelpMetadata::IsComplete() const
{
    return !key.trimmed().isEmpty()
        && !title.trimmed().isEmpty()
        && !description.trimmed().isEmpty()
        && !affects.isEmpty()
        && !defaultvalue.trimmed().isEmpty()
        && !productionsafety.trimmed().isEmpty()
        && !docpath.trimmed().isEmpty();
}

QString SettingHelpMetadata::ToolTipText() const
{
    if (!IsComplete())
    {
        return {};
    }

    return QStringLiteral("%1\n%2\n影响：%3\n默认：%4\n生产安全：%5\n文档：%6")
        .arg(title, description, affects.join(QStringLiteral(" / ")),
             defaultvalue, productionsafety, docpath);
}

QString SettingHelpMetadata::DetailText() const
{
    if (!IsComplete())
    {
        return {};
    }

    return QStringLiteral(
               "%1\n\n%2\n\n设置键：%3\n影响范围：%4\n默认值：%5\n生产安全：%6\n相关文档：%7")
        .arg(title, description, key, affects.join(QStringLiteral(" / ")),
             defaultvalue, productionsafety, docpath);
}

const QVector<SettingHelpMetadata>& HelpTextProvider::All()
{
    return MetadataEntries();
}

const SettingHelpMetadata* HelpTextProvider::Find(const QString& key)
{
    const QVector<SettingHelpMetadata>& entries = MetadataEntries();
    for (const SettingHelpMetadata& entry : entries)
    {
        if (entry.key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

QString HelpTextProvider::ToolTip(const QString& key)
{
    const SettingHelpMetadata* metadata = Find(key);
    return metadata == nullptr ? QString{} : metadata->ToolTipText();
}
