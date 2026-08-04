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
    SliceRunGridCallback gridcallback;
    SliceRunLayerCallback layercallback;
    std::optional<ModelInstance> instanceoverride;
    std::optional<SliceRunInputOverride> inputoverride;

    /**
     * @brief Optional already-imported model used by scene orchestration.
     *
     * The pointed model is copied before transforms are applied and must
     * remain alive for the synchronous run.
     */
    const ModelReport* modelreportoverride{nullptr};
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
