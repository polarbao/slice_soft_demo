#pragma once

#include "slicer_core/config.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

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

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct AutoOrientReport {
    bool enabled{true};
    bool applied{false};
    double max_height_mm{6.0};
    std::string selected_orientation{"identity"};
    BoundingBox original_bbox_mm;
};

struct ModelReport {
    std::filesystem::path model_path;
    std::string format;
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    AutoOrientReport auto_orient;
    BoundingBox bbox_mm;
    std::vector<Triangle> triangles;
};

ModelReport load_model_report(const SliceConfig& config, const std::filesystem::path& config_dir);

}  // namespace slicer_core
