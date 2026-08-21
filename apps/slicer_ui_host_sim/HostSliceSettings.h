#pragma once

#include <QJsonObject>
#include <QString>

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
     * @brief 按冻结的场景语义比较两个构建体积。
     * @param left 第一个宿主体积。
     * @param right 第二个宿主体积。
     * @return 尺寸、原点与坐标轴一致时返回 true。
     */
    static bool BuildVolumesEqual(
        const hostbuildvolume& left,
        const hostbuildvolume& right);
};
