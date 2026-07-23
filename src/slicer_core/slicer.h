#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/config.h"

#include <filesystem>
#include <string>

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
 * @brief Output switches for a slicer run.
 */
struct SliceRunOptions {
    bool write_tiff_layers{true};
    bool write_preview_files{true};
    bool write_reports{true};
    SliceRunProgressCallback progress_callback;
};

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
