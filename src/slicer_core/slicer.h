#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/model.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"
#include "slicer_core/scene/ModelInstance.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace slicer_core {

namespace engine
{
class TransferProductionEntry;
}

/** @brief 仅可由生产 Facade 构造的 RGBWSVT Scene 准入令牌。 */
class TransferSceneProductionAdmission final
{
private:
    TransferSceneProductionAdmission() = default;
    TransferSceneProductionAdmission(
        const TransferSceneProductionAdmission&) = delete;
    TransferSceneProductionAdmission& operator=(
        const TransferSceneProductionAdmission&) = delete;

    friend class engine::TransferProductionEntry;
};

/**
 * @brief Summary returned by an admitted production slicing pipeline.
 */
struct SliceRunResult {
    std::filesystem::path package_dir;
    std::string effective_pipeline_mode{"legacy"};
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    int model_pixel_count{0};
    int support_pixel_count{0};
    SliceRunProfile profile;
};

/**
 * @brief Legacy producer raster geometry exposed before any file output.
 */
struct SliceRunRasterGrid
{
    int widthpx{0};
    int heightpx{0};
    int layercount{0};
    double pixelsizexmm{0.0};
    double pixelsizeymm{0.0};
    double layerthicknessmm{0.0};
    double originxmm{0.0};
    double originymm{0.0};
    double originzmm{0.0};
};

/**
 * @brief Callback receiving immutable Legacy raster geometry.
 */
using SliceRunGridCallback =
    std::function<void(const SliceRunRasterGrid&)>;

/**
 * @brief Callback receiving one final Legacy RGBWSV layer and ownership.
 */
using SliceRunLayerCallback = std::function<void(
    const RgbwsvProductionLayer&,
    const MaterialClosureSemanticLayerInput&)>;

/**
 * @brief Explicit model source override used by scene production adapters.
 */
struct SliceRunInputOverride
{
    std::filesystem::path modelpath;
    std::string format{"auto"};
};

/**
 * @brief Output switches for a slicer run.
 */
struct SliceRunOptions {
    bool write_tiff_layers{true};
    bool write_preview_files{true};
    bool write_reports{true};
    SliceRunProgressCallback progress_callback;
    std::function<bool()> cancellation_requested;
    SliceRunGridCallback gridcallback;
    SliceRunLayerCallback layercallback;
    std::optional<ModelInstance> instanceoverride;
    std::optional<SliceRunInputOverride> inputoverride;
    const TransferSceneProductionAdmission*
        transfer_scene_production_admission{nullptr};

    /**
     * @brief Optional already-imported model used by scene orchestration.
     *
     * The pointed model is copied before transforms are applied and must
     * remain alive for the synchronous run.
     */
    const ModelReport* modelreportoverride{nullptr};

    /**
     * @brief 同步取消点；返回 true 时切片立即失败且不产出半成品。
     *
     * 取用 std::function 而非 api::ICancelToken*，是为了与
     * MaterialVolumeBuildRequest、ModelPreflightRequest、TiffLayerSource 等既有形状一致，
     * 并可零适配直接转交。上层若持有 ICancelToken，在调用点包一层 lambda 即可，
     * 该做法在 ProductionRepairFacadeFactory 中已有先例。
     *
     * 未设置等同于「从不取消」，因此本字段对既有调用方完全无行为影响。
     */
    std::function<bool()> cancellationRequested;
};

/**
 * @brief Determine whether a material process white mode requires complete RGB underbase coverage.
 * @param whiteMode Stable materialProcessProfile.white.mode value.
 * @return True only for the exact legacy underbase mode.
 */
[[nodiscard]] bool RequiresCompleteWhiteUnderbase(std::string_view whiteMode) noexcept;

/**
 * @brief Run the legacy production slicer with default output options.
 * @param config_path Path to a slice configuration JSON file.
 * @return Summary of the generated package and coarse run profile.
 */
SliceRunResult run_slicer(const std::filesystem::path& config_path);

/**
 * @brief Run the legacy production slicer with explicit output options.
 * @param config_path Path to a slice configuration JSON file.
 * @param options Output switches used by tests and benchmarks.
 * @return Summary of the generated package and coarse run profile.
 */
SliceRunResult run_slicer(const std::filesystem::path& config_path, const SliceRunOptions& options);

}  // namespace slicer_core
