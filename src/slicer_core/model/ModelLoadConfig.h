#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace slicer_core {

/** @brief Model source path and explicit format hint. */
struct InputConfig
{
    std::filesystem::path model_path;
    std::string format{"auto"};
};

/** @brief Unit, scale, rotation and translation applied during import. */
struct TransformConfig
{
    std::string unit{"mm"};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
    std::array<double, 3> rotation_deg{0.0, 0.0, 0.0};
    std::array<double, 3> translation_mm{0.0, 0.0, 0.0};
};

/** @brief Deterministic right-angle auto-orientation options. */
struct AutoOrientConfig
{
    bool enabled{true};
    double max_height_mm{9.0};
    std::string strategy{"minimize_height_by_right_angle_rotation"};
};

/** @brief Narrow model-loader configuration independent from SliceConfig. */
struct ModelLoadConfig
{
    InputConfig input;
    std::filesystem::path output_package_dir;
    TransformConfig transform;
    AutoOrientConfig auto_orient;
};

}  // namespace slicer_core
