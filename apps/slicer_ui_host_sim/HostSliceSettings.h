#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

/** @brief 宿主实体模型填充使用的材料通道。 */
enum class HostMaterialStrategy
{
    RgbSolid,
    RgbWhite,
    RgbVarnish,
    RgbWhiteVarnish,
    WhiteSolid,
    VarnishSolid
};

/** @brief 解析 OBJ/3MF 输入材料时使用的材料角色。 */
enum class HostMaterialRole
{
    Rgb,
    White,
    Varnish,
    Ignore,
    SupportCandidate
};

/** @brief 由宿主持有的材料策略与角色映射参数。 */
struct hostmaterialprocesssettings
{
    bool rolemappingenabled{false};
    HostMaterialRole defaultrole{HostMaterialRole::Rgb};
    bool mapwhitenames{true};
    bool mapvarnishnames{true};
    bool allowinputsupportmaterial{false};
    int whiteexpandpx{0};
    int whiteshrinkpx{0};
    int varnishtoplayers{1};
    int maxunexpectedoverlappixels{0};
};

/** @brief 参考宿主 Profile 暴露的支撑生成模式。 */
enum class HostSupportMode
{
    None,
    BottomProjection,
    UnsupportedOnly,
    BottomProjectionPlusUnsupported,
    FullVerticalProjection
};

/** @brief 由宿主持有的内部空洞支撑参数。 */
struct hostinternalvoidsettings
{
    bool enabled{true};
    int minareapx{16};
};

/** @brief 由宿主持有的最大足迹支撑基底参数。 */
struct hostbaseprojectionsettings
{
    bool enabled{false};
    int layercount{30};
};

/** @brief 由宿主持有的可编辑支撑参数。 */
struct hostsupportsettings
{
    bool enabled{true};
    HostSupportMode mode{HostSupportMode::BottomProjection};
    double offsetmm{0.0};
    int minareapx{0};
    hostinternalvoidsettings internalvoid;
    hostbaseprojectionsettings baseprojection;
};

/** @brief 宿主 Profile 暴露的生产纹理应用模式。 */
enum class HostTextureApplyMode
{
    SolidVolumeFromTopSurface,
    TopSurfaceOnly,
    TopSurfaceBand
};

/** @brief 采样 UV 坐标时使用的纹理过滤模式。 */
enum class HostTextureSampler
{
    Nearest,
    Bilinear
};

/** @brief 超出规范化 UV 范围时使用的纹理寻址模式。 */
enum class HostTextureUvAddressMode
{
    Clamp,
    Repeat
};

/** @brief 纹理资产缺失或无效时的失败即拒绝行为。 */
enum class HostTextureMissingPolicy
{
    WarnAndFallback,
    FailFast
};

/** @brief 有限纹理表面带下方的 RGB 行为。 */
enum class HostTextureNonSurfacePolicy
{
    ModelMaterial,
    Empty
};

/** @brief 所有通道均为空的纹理像素载体策略。 */
enum class HostTextureWhitePolicy
{
    FailClosed,
    WhiteUnderbase
};

/** @brief 宿主代码暴露的几何占用采样策略。 */
enum class HostGeometrySamplingStrategy
{
    LegacyCenterSample,
    LayerSlabSupersample2x2AtLeastTwoCandidate
};

/** @brief 宿主为生产层选择的 TIFF 压缩方式。 */
enum class HostTiffCompression
{
    None,
    PackBits
};

/** @brief 宿主显式选择的生产 Package 协议。 */
enum class HostPackageProtocol
{
    Rgbwsv,
    Rgbwsvt
};

/** @brief 外部工艺配置提供的单个材质漫反射 RGB 值。 */
struct hostrgbcolor
{
    int red{0};
    int green{0};
    int blue{0};
};

/** @brief 由外部工艺文件提供、由宿主持久化的 T 通道识别策略。 */
struct hosttransferchannelsettings
{
    bool enabled{false};
    QString matchsource{QStringLiteral("material_diffuse_rgb")};
    QVector<hostrgbcolor> materialdiffusergbvalues;
    QString missingregion{QStringLiteral("allow_empty")};
    QString multiplematches{QStringLiteral("fail_closed")};
    int value{0};
    QString selfintersectionpolicy{QStringLiteral("reject")};
    int maxselfintersectionpairs{64};
    // MQ-06 有界开边上限。默认 0 与切片库侧一致（保持现状、须显式开启）。
    // 此前该字段【不存在】，于是工艺文件里的 maxBoundaryEdges 在宿主这一环被静默丢弃：
    // JSON 有 8，发射出的 Profile 里没有，切片库侧取默认 0，
    // 带 3 条真开边的资产（08/09/08-03/08-04）随即被拒，
    // 错误消息里那句「configured limit of 0」正是这样来的。
    // 注意：本目录的源文件【不得出现切片库内部目标名】，14E-02 边界门禁按子串匹配，
    // 注释中提及同样会判违规——本注释即为此改写措辞。
    int maxboundaryedges{0};
};

/** @brief 由宿主持有的生产纹理与 Stage 15 白色载体设置。 */
struct hosttexturesettings
{
    bool enabled{false};
    HostTextureApplyMode applymode{
        HostTextureApplyMode::SolidVolumeFromTopSurface};
    int topsurfacelayers{1};
    HostTextureSampler sampler{HostTextureSampler::Bilinear};
    HostTextureUvAddressMode uvaddressmode{
        HostTextureUvAddressMode::Clamp};
    bool flipv{true};
    int fallbackred{0};
    int fallbackgreen{0};
    int fallbackblue{0};
    HostTextureMissingPolicy missingpolicy{
        HostTextureMissingPolicy::WarnAndFallback};
    HostTextureNonSurfacePolicy nonsurfacepolicy{
        HostTextureNonSurfacePolicy::ModelMaterial};
    HostTextureWhitePolicy whitepolicy{
        HostTextureWhitePolicy::FailClosed};
    int whiteinkthreshold{0};
    int whitevalue{0};
};

/** @brief 由设备持有并注入首次场景 Commit 的构建体积。 */
struct hostbuildvolume
{
    double widthmm{230.0};
    double heightmm{100.0};
    double zlimitmm{60.0};
    QString origin{QStringLiteral("lower_left")};
    QString xdirection{QStringLiteral("positive")};
    QString ydirection{QStringLiteral("positive")};
};

/**
 * @brief 由宿主持有的多材质纵深体积候选参数。
 *
 * 本结构体是宿主侧镜像，刻意不引用切片内核类型；序列化后的
 * materialVolumePolicy 块由 Worker 侧解析回内核配置。
 */
struct hostmaterialvolumesettings
{
    bool enabled{false};
    QString primarymaterialname;
    int primarypriority{200};
    QString secondarymaterialname;
    int secondarypriority{100};
    /**
     * @brief 由材质命名自动推导逐材质优先级，取代 primary/secondary 手填。
     *
     * 开启后 `overlap.mode` 取 `auto_by_material_name`，`rules` 留空，
     * 材质数量不再受本结构两个名字槽的限制——多图层资产正需要这一点。
     * 命名规范见 DOC_SPEC_MATERIAL_NAMING。
     */
    bool overlapautobyname{false};
    /// @brief 由不透明度判为光油（V 通道体积填充）。
    bool opacityvarnishenabled{false};
    /// @brief 判为光油的不透明度上限，须在 (0, 1)。
    double opacityvarnishmax{0.001};
    /**
     * @brief 退化面判定阈值（面积平方 mm^4）；<=0 表示沿用内建默认。
     *
     * 语义上属 geometrySampling 而非 materialVolume，此处随本结构携带
     * 只是为了让工艺预设能一并带上——该阈值仅在多材质纵深场景才需要收紧。
     * CAD/NURBS 导出含 nm^2 级合法薄面，默认门 1e-6 mm^2 会误杀它们，
     * 使材质被判为开放表面而直接拒绝；推荐值 1e-24。
     */
    double degenerateareaepsilonmm2{0.0};
};

/** @brief 由宿主持有的可编辑切片参数。 */
struct hostslicesettings
{
    QString profileid;
    QString processpresetid{QStringLiteral("custom")};
    QString modelpath;
    QString modelformat;
    QString outputdirectory;
    int dpix{635};
    int dpiy{600};
    double layerthicknessmm{0.038};
    HostMaterialStrategy materialstrategy{HostMaterialStrategy::RgbSolid};
    hostmaterialprocesssettings materialprocess;
    hostbuildvolume buildvolume;
    hostsupportsettings support;
    hosttexturesettings texture;
    HostTiffCompression tiffcompression{HostTiffCompression::None};
    HostGeometrySamplingStrategy geometrysamplingstrategy{
        HostGeometrySamplingStrategy::LegacyCenterSample};
    hostmaterialvolumesettings materialvolume;
    HostPackageProtocol packageprotocol{HostPackageProtocol::Rgbwsv};
    hosttransferchannelsettings transferchannel;
};

/** @brief 已校验且可用于后续切片请求的 Profile 预览。 */
struct hosteffectiveprofile
{
    QJsonObject profile;
    QString profilehash;
};

/** @brief 构建并校验由宿主持有的有效切片 Profile。 */
class HostEffectiveProfileBuilder final
{
public:
    /**
     * @brief 不调用模块，仅校验可编辑宿主设置。
     * @param settings 待检查的宿主侧设置。
     * @param error 接收用户可读的校验原因。
     * @return 设置能够生成有效 Profile 时返回 true。
     */
    static bool Validate(
        const hostslicesettings& settings,
        QString* error);

    /**
     * @brief 根据宿主设置构建包含自身哈希的有效 Profile。
     * @param settings 有效的宿主侧设置。
     * @param effectiveProfile 接收解析后的 Profile JSON 与哈希。
     * @param error 接收失败即拒绝原因。
     * @return 成功构建后续提交所用的精确 Profile 时返回 true。
     */
    static bool Build(
        const hostslicesettings& settings,
        hosteffectiveprofile* effectiveProfile,
        QString* error);

    /**
     * @brief 将材料策略转换为稳定宿主标识。
     * @param strategy 操作员选择的策略。
     * @return UI 测试与持久化使用的稳定小写标识。
     */
    static QString MaterialStrategyId(HostMaterialStrategy strategy);

    /**
     * @brief 将材料角色转换为稳定 Profile 值。
     * @param role 操作员选择的材料角色。
     * @return 写入 `materialRoleMapping` 的稳定标识。
     */
    static QString MaterialRoleId(HostMaterialRole role);

    /**
     * @brief 将支撑模式转换为稳定生产 Profile 值。
     * @param mode 操作员选择的支撑模式。
     * @return 写入 `support.mode` 的稳定标识。
     */
    static QString SupportModeId(HostSupportMode mode);

    /**
     * @brief 将几何采样策略转换为稳定 Profile 值。
     * @param strategy 宿主代码选择的几何采样策略。
     * @return 已批准的 Profile 标识；无法识别时返回 `unknown`。
     */
    static QString GeometrySamplingStrategyId(
        HostGeometrySamplingStrategy strategy);

    /**
     * @brief 将 TIFF 压缩方式转换为 Profile 算法值。
     * @param compression 宿主选择的无损压缩模式。
     * @return `none`、`packbits` 或 `unknown`。
     */
    static QString TiffCompressionId(HostTiffCompression compression);

    /**
     * @brief 将 Package 协议转换为稳定 Profile 值。
     * @param protocol 宿主显式选择的协议。
     * @return 冻结协议标识；无法识别时返回 `unknown`。
     */
    static QString PackageProtocolId(HostPackageProtocol protocol);

    /**
     * @brief 按冻结的场景语义比较两个构建体积。
     * @param left 第一个宿主体积。
     * @param right 第二个宿主体积。
     * @return 尺寸、原点与坐标轴一致时返回 true。
     */
    static bool BuildVolumesEqual(
        const hostbuildvolume& left,
        const hostbuildvolume& right);
};
