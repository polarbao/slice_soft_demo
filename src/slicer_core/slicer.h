#pragma once

#include "slicer_core/config.h"

#include <filesystem>
#include <string>

namespace slicer_core {

/**
 * @brief Coarse timing profile for one slicer run.
 *
 * The profile is diagnostic-only. It is not part of the RGBWSV package protocol and must not drive
 * material or support decisions.
 */
struct SliceRunProfile {
    bool available{false};
    std::string profile_level{"none"};
    double config_load_ms{0.0};
    double model_load_ms{0.0};
    double grid_setup_ms{0.0};
    double mask_sampling_ms{0.0};
    double texture_prepare_ms{0.0};
    double support_generation_ms{0.0};
    double layer_compose_ms{0.0};
    double report_build_ms{0.0};
    double report_write_ms{0.0};
    double total_ms{0.0};
};

/**
 * @brief Summary returned by the production legacy slicer path.
 */
struct SliceRunResult {
    std::filesystem::path package_dir;
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
