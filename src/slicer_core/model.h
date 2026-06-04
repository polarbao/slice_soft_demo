#pragma once

#include "slicer_core/config.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace slicer_core {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct ModelReport {
    std::filesystem::path model_path;
    std::string format;
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    BoundingBox bbox_mm;
};

ModelReport load_model_report(const SliceConfig& config, const std::filesystem::path& config_dir);

}  // namespace slicer_core

