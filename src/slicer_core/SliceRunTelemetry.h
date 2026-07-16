#pragma once

#include <functional>
#include <string>

namespace slicer_core
{

/**
 * @brief Diagnostic timing profile for one slicer run.
 *
 * The profile is diagnostic-only. It is not part of the RGBWSV package protocol and must not drive
 * material or support decisions. All values use milliseconds.
 */
struct SliceRunProfile
{
    bool available{false};
    std::string profile_level{"none"};
    double config_load_ms{0.0};
    double model_load_ms{0.0};
    double grid_setup_ms{0.0};
    double mask_sampling_ms{0.0};
    double texture_prepare_ms{0.0};
    double support_generation_ms{0.0};
    double layer_compute_ms{0.0};
    double tiff_write_ms{0.0};
    double preview_write_ms{0.0};
    double layer_compose_ms{0.0};
    double report_build_ms{0.0};
    double report_write_ms{0.0};
    double package_publish_ms{0.0};
    double slice_processing_ms{0.0};
    double output_write_ms{0.0};
    double total_ms{0.0};
};

/**
 * @brief Progress snapshot emitted by a slicer run.
 */
struct SliceRunProgress
{
    std::string phase;
    int current{0};
    int total{0};
    int percent{0};
    double elapsed_ms{0.0};
};

/**
 * @brief Observer invoked for diagnostic slicing progress.
 *
 * The callback runs synchronously on the slicing thread and must return quickly.
 */
using SliceRunProgressCallback = std::function<void(const SliceRunProgress&)>;

}  // namespace slicer_core
