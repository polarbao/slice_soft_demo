#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"

#include <cstddef>
#include <functional>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable failures produced by scene raster validation and composition.
 */
enum class SceneRasterErrorCode
{
    None,
    AdmissionRequired,
    GridInvalid,
    ProtocolMismatch,
    ResolutionMismatch,
    LayerSequenceMismatch,
    OffsetNotIntegral,
    LayerSizeInvalid,
    InstanceIdentityInvalid,
    InstanceOverlap,
    MaterialConflict,
    ClosureFailed,
    RevisionStale,
    PipelineModeMismatch,
    ProducerFailed,
    Cancelled,
};

/**
 * @brief Material ownership used to resolve one scene-raster pixel.
 */
enum class SceneRasterOwnership : std::uint8_t
{
    Empty = 0U,
    Support = 1U,
    OuterVarnish = 2U,
    Model = 3U,
};

/**
 * @brief Shared physical raster used by every visible scene instance.
 */
struct SceneRasterGrid
{
    int widthpx{0};
    int heightpx{0};
    int layercount{0};
    double originxmm{0.0};
    double originymm{0.0};
    double originzmm{0.0};
    double pitchxmm{0.0};
    double pitchymm{0.0};
    double layerthicknessmm{0.0};

    /**
     * @brief Report whether dimensions and physical sampling values are finite and positive.
     * @return True when the grid can be used for checked raster calculations.
     */
    bool IsValid() const;
};

/**
 * @brief One local writer-ready layer plus immutable material ownership masks.
 */
struct SceneInstanceRasterLayer
{
    int layerindex{0};
    double zmm{0.0};
    RgbwsvProductionLayer output;
    std::vector<std::uint8_t> modelownership;
    std::vector<std::uint8_t> modelvarnishownership;
    std::vector<std::uint8_t> outervarnishownership;
    std::vector<std::uint8_t> supportownership;
};

/**
 * @brief One admitted instance raster presented to the scene composer.
 */
struct SceneInstanceRaster
{
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::uint64_t scenerevision{0U};
    std::uint64_t transformrevision{0U};
    std::uint64_t admittedtransformrevision{0U};
    std::string transformhash;
    std::string admittedtransformhash;
    bool visible{true};
    bool admitted{false};
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
    SceneRasterGrid localgrid;
    RgbwsvProtocol protocol;
    std::vector<SceneInstanceRasterLayer> layers;
};

/**
 * @brief Complete immutable request for composing one admitted scene.
 */
struct SceneLayerComposeRequest
{
    std::string sceneid;
    std::uint64_t currentscenerevision{0U};
    std::uint64_t expectedscenerevision{0U};
    bool admissionpassed{false};
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
    SceneRasterGrid globalgrid;
    RgbwsvProtocol protocol;
    std::vector<SceneInstanceRaster> instances;
    double quantizationtolerance{1.0e-6};

    /**
     * @brief MF-05 步骤 3：逐层出口。设置后合成【不再累积】`result.layers`。
     *
     * 合成主循环本就是 layer-major（外层全局层、内层实例），逐层闭合校验也在
     * 循环内完成，故「全部层同时在场」只是末尾的计数校验，不是证据本身的需要。
     * 设置本回调后，每层合成完即交出并释放；`statistics.outputlayercount` 继续
     * 如实累加，`layerstatistics`（每层一条小结构）仍保留。
     *
     * 用户 0.2+0.3 @10um 场景下 `result.layers` 占约 62.2 GiB。
     * 未设置时行为【逐字节不变】，故可先落地、后接线。
     */
    std::function<void(int, RgbwsvProductionLayer&&, const RgbwsvProductionLayerStatistics&)>
        layersink;

    /**
     * @brief MF-05 步骤 2b：逐层入口。非空时按 (实例, 本地层号) 现取层，
     *        `instances` 里的 `layers` 允许为空。
     *
     * 返回空指针表示该实例在该层没有内容（层数不齐时的正常情形）。
     * 设置本回调后，实例校验由「先整实例走一遍层」改为「逐层校验」：
     * `ValidateLayer` 的调用点移进合成的层循环，层数齐备改为在循环结束时
     * 断言【已校验层数 == layercount】，`instanceStatistics` 逐层累积、收尾后移。
     * 语义不降级 —— 每层仍走同一个 `ValidateLayer`，只是时机不同。
     */
    std::function<const SceneInstanceRasterLayer*(const SceneInstanceRaster&, int)>
        layerprovider;

    /** @brief Synchronous, non-owning cancellation source for long loops. */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief Stable scene/model/instance identity attached by engine adapters.
 */
struct SceneRasterIdentity
{
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::uint64_t scenerevision{0U};
    std::uint64_t transformrevision{0U};
    std::uint64_t admittedtransformrevision{0U};
    std::string transformhash;
    std::string admittedtransformhash;
    bool visible{true};
    bool admitted{false};
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
};

/**
 * @brief Structured fail-closed scene-raster error.
 */
struct SceneRasterError
{
    SceneRasterErrorCode code{SceneRasterErrorCode::None};
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::string otherinstanceid;
    int layerindex{-1};
    std::string field;
    std::string message;
};

/**
 * @brief Deterministic contribution counters for one visible instance.
 */
struct SceneInstanceComposeStatistics
{
    struct RasterStatistics
    {
        bool available{false};
        SceneRasterGrid grid;
        std::array<std::uint64_t, 6> printpixels{};
        std::array<std::uint64_t, 6> emptypixels{};
        int minimumx{0};
        int minimumy{0};
        int minimumlayer{0};
        int maximumx{-1};
        int maximumy{-1};
        int maximumlayer{-1};
    };

    std::string instanceid;
    std::size_t modelpixels{0U};
    std::size_t outervarnishpixels{0U};
    std::size_t supportpixels{0U};
    RasterStatistics raster;
};

/**
 * @brief Deterministic aggregate counters for one scene composition.
 */
struct SceneLayerComposeStatistics
{
    std::size_t totalinstancecount{0U};
    std::size_t visibleinstancecount{0U};
    std::size_t hiddeninstancecount{0U};
    std::size_t outputlayercount{0U};
    std::size_t modelpixels{0U};
    std::size_t outervarnishpixels{0U};
    std::size_t supportpixels{0U};
    std::size_t emptypixels{0U};
    std::vector<SceneInstanceComposeStatistics> instances;
};

/**
 * @brief Atomic writer-ready output of one scene composition.
 */
struct SceneLayerComposeResult
{
    bool available{false};
    std::string status{"blocked"};
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    SceneRasterGrid grid;
    RgbwsvProtocol protocol;
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
    std::vector<RgbwsvProductionLayer> layers;
    std::vector<RgbwsvProductionLayerStatistics> layerstatistics;
    SceneLayerComposeStatistics statistics;
    std::optional<SceneRasterError> error;
    double composems{0.0};
    std::size_t peakworkingbytes{0U};

    /**
     * @brief Report whether a complete writer-ready layer list is available.
     * @return True only after every input and output layer passed validation.
     */
    bool IsValid() const;

    /**
     * @brief Validate composed bytes while observing cancellation.
     * @param cancelToken Synchronous non-owning cancellation source.
     * @return False when invalid or cancellation was requested.
     */
    bool IsValid(const api::ICancelToken* cancelToken) const;
};

/**
 * @brief Result shared by Legacy and Global in-memory scene-layer adapters.
 */
struct SceneRasterAdapterResult
{
    bool available{false};
    std::string status{"blocked"};
    bool productionoutputwritten{false};
    SceneInstanceRaster raster;
    SliceRunProfile profile;
    std::optional<SceneRasterError> error;

    /**
     * @brief Report whether a complete in-memory instance raster is available.
     * @return True only when no package was written and no error exists.
     */
    bool IsValid() const;

    /**
     * @brief Validate adapter bytes while observing cancellation.
     * @param cancelToken Synchronous non-owning cancellation source.
     * @return False when invalid or cancellation was requested.
     */
    bool IsValid(const api::ICancelToken* cancelToken) const;

    /**
     * @brief 同上，但可声明「层已逐层交出」。
     * @param layersStreamed 为真时跳过层序列检查（`raster.layers` 恒为空）。
     *
     * 这是第三处藏在校验里的整栈假设 —— 另两处是合成侧 `ValidateInstance` 的
     * 层数前置检查、以及 adapter 返回前的 `layers.size()` 检查。流式化时三处都
     * 要显式放行，且都把「层数齐备」的责任交给合成侧的逐层计数断言。
     */
    bool IsValid(
        const api::ICancelToken* cancelToken,
        bool layersStreamed) const;
};

/**
 * @brief Return the stable protocol name for a scene-raster error.
 * @param code Scene-raster error code.
 * @return Stable ASCII error name.
 */
std::string_view SceneRasterErrorCodeName(SceneRasterErrorCode code);

/**
 * @brief Return the fixed current RGBWSV protocol without writing output.
 * @return p0.rgbwsv.2, RGBWSV, uint8, black_is_print protocol descriptor.
 */
RgbwsvProtocol FixedSceneRasterProtocol();

/**
 * @brief Derive model-owned V pixels without conflating outer varnish.
 * @param output Final local RGBWSV layer.
 * @param modelOwnership Binary model ownership mask.
 * @param protocol Fixed protocol values used to identify empty V bytes.
 * @return Binary mask where V is printed by a model-owned pixel.
 */
std::vector<std::uint8_t> BuildModelVarnishOwnership(
    const RgbwsvProductionLayer& output,
    const std::vector<std::uint8_t>& modelOwnership,
    const RgbwsvProtocol& protocol);

}  // namespace slicer_core
