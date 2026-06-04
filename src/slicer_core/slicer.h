#pragma once

#include "slicer_core/config.h"

#include <filesystem>

namespace slicer_core {

struct SliceRunResult {
    std::filesystem::path package_dir;
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    int model_pixel_count{0};
    int support_pixel_count{0};
};

struct SliceRunOptions {
    bool write_tiff_layers{true};
};

SliceRunResult run_slicer(const std::filesystem::path& config_path);
SliceRunResult run_slicer(const std::filesystem::path& config_path, const SliceRunOptions& options);

}  // namespace slicer_core
