#pragma once

#include "slicer_core/api/Cancellation.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"
#include "slicer_core/output/rgbwsv/RgbwsvSceneExtension.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Observer invoked after one production layer is persisted.
 *
 * The callback runs synchronously on the writer thread and must return quickly.
 */
using RgbwsvProductionLayerWriteCallback =
    std::function<void(int, int)>;

/**
 * @brief Diagnostic-only timing profile for one RGBWSV package publication.
 */
struct RgbwsvProductionPackageWriteProfile
{
    double tiffwritems{0.0};
    double previewwritems{0.0};
    double reportbuildms{0.0};
    double reportwritems{0.0};
    double packagepublishms{0.0};
    double totalms{0.0};
};

/**
 * @brief Grid metadata written into an RGBWSV production package.
 */
struct RgbwsvProductionGridSpec
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    int dpiX{kDefaultOutputDpiX};
    int dpiY{kDefaultOutputDpiY};
    double pixelSizeXmm{
        kMillimetersPerInch / static_cast<double>(kDefaultOutputDpiX)};
    double pixelSizeYmm{
        kMillimetersPerInch / static_cast<double>(kDefaultOutputDpiY)};
    double layerThicknessMm{kDefaultLayerThicknessMm};
    double originXmm{0.0};
    double originYmm{0.0};
    double originZmm{0.0};
};

/**
 * @brief TIFF storage settings shared by Legacy and Global Surface Shell output.
 */
struct RgbwsvProductionStorageSpec
{
    std::string storageMode{"stripped"};
    std::string compression{"none"};
    int rowsPerStrip{64};
    int tileWidth{256};
    int tileHeight{256};
};

/**
 * @brief Production package preview settings.
 */
struct RgbwsvProductionPreviewSpec
{
    std::string outputpolicy{"tiff_native"};
    bool enabled{false};
    std::string format{"ppm"};
    int interval{10};
};

/**
 * @brief Non-owning view used by the shared per-layer TIFF writer.
 */
struct RgbwsvProductionLayerView
{
    int widthPx{0};
    int heightPx{0};
    std::span<const std::uint8_t> channels;
};

/**
 * @brief Complete request for publishing an admitted RGBWSV production package.
 */
struct RgbwsvProductionPackageWriteRequest
{
    std::filesystem::path packageDir;
    std::string jobId;
    std::string attemptId;
    std::filesystem::path sourceConfigPath;
    std::filesystem::path sourceModelPath;
    std::string sourceFormat;
    std::string requestedPipelineMode;
    std::string effectivePipelineMode;
    std::string productionAcceptance{"not_evaluated"};
    std::optional<std::string> manifestWhiteSemantics;
    std::optional<std::string> profileWhiteSemantics;
    RgbwsvProductionGridSpec grid;
    OuterVarnishDiscretization outerVarnish;
    RgbwsvProductionStorageSpec storage;
    RgbwsvProductionPreviewSpec preview;
    std::vector<RgbwsvProductionLayer> layers;
    std::vector<RgbwsvProductionLayerStatistics> layerStatistics;
    std::optional<MultiModelSceneReportDocument> scene;
    std::optional<Json> perinstance;
    std::optional<Json> profileecho;
    std::optional<Json> productionSettings;
    RgbwsvProductionLayerWriteCallback layerwritecallback;

    /**
     * @brief Synchronous, non-owning cancellation source for staged writing.
     *
     * Cancellation is checked before and after each TIFF plus report and
     * publication boundaries. The caller must keep the token alive until
     * this function returns.
     */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief Result returned after an RGBWSV package is validated and published.
 */
struct RgbwsvProductionPackageWriteResult
{
    bool productionOutputWritten{false};
    bool fallbackApplied{false};
    bool strictProtocolValidated{false};
    int layerCount{0};
    std::string jobId;
    std::string attemptId;
    std::filesystem::path packageDir;
    std::filesystem::path replacedPackageBackupDir;
    bool stagingRemoved{false};
    bool backupRemoved{false};
    bool leaseReleased{false};
    RgbwsvProductionPackageWriteProfile profile;
};

/**
 * @brief Write one RGBWSV TIFF through the shared fixed-protocol writer.
 * @param path Destination TIFF path.
 * @param storage TIFF storage settings.
 * @param layer Final interleaved RGBWSV bytes and dimensions.
 */
void WriteRgbwsvProductionLayerTiff(
    const std::filesystem::path& path,
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvProductionLayerView& layer);

/**
 * @brief Validate, stage, RIP-check, and atomically publish a production package.
 * @param request Admitted final RGBWSV layers and package metadata.
 * @return Published package summary.
 */
RgbwsvProductionPackageWriteResult WriteRgbwsvProductionPackage(
    const RgbwsvProductionPackageWriteRequest& request);

/**
 * @brief 逐层发布会话（MF-05 步骤 4b）。
 *
 * `WriteRgbwsvProductionPackage` 要求全部层先在内存里；场景路径按此每实例持有
 * 整栈，用户 0.2+0.3 @10um 实测斜率 189 MB/层。本类把同一套发布流程拆成三段，
 * 使上游可以「合成一层就交出一层」，层写完即释放。
 *
 * **本类不是新写的发布逻辑** —— `WriteRgbwsvProductionPackage` 已改写为本类的
 * 薄封装（构造、逐层 Append、Finish），故两条路径共用同一份实现，不会漂移。
 *
 * **RAII 是这里的关键。** 原实现用 `try/catch(...)` 包住 staging 到发布的全程，
 * 失败时 `RecoverPackageArtifacts` 释放租约并清理 staging。拆成三段后，调用方
 * 可能在 Append 中途抛出而永远不调 Finish —— 那样租约会泄漏、staging 会残留。
 * 故未成功 `Finish()` 时由析构函数执行同一套回滚。析构不抛：回滚失败只吞掉，
 * 因为此时通常已在栈展开中。
 *
 * 生命周期：持有 `request` 的引用，调用方须保证其存活到 `Finish()` 之后。
 */
class RgbwsvProductionPackageSession final
{
public:
    explicit RgbwsvProductionPackageSession(
        const RgbwsvProductionPackageWriteRequest& request);
    ~RgbwsvProductionPackageSession();

    RgbwsvProductionPackageSession(const RgbwsvProductionPackageSession&) = delete;
    RgbwsvProductionPackageSession& operator=(
        const RgbwsvProductionPackageSession&) = delete;
    RgbwsvProductionPackageSession(RgbwsvProductionPackageSession&&) = delete;
    RgbwsvProductionPackageSession& operator=(
        RgbwsvProductionPackageSession&&) = delete;

    /// 写一层 TIFF 并累积 manifest 条目与通道统计。
    void AppendLayer(const RgbwsvProductionLayer& layer);

    /// 写 manifest/report 并原子发布。成功后析构不再回滚。
    RgbwsvProductionPackageWriteResult Finish();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

}  // namespace slicer_core
