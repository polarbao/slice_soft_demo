#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace slicer_core {

/** @brief 模型源路径和显式格式提示。 */
struct InputConfig
{
    std::filesystem::path model_path;
    std::string format{"auto"};
};

/** @brief 导入时应用的单位、缩放、旋转和平移。 */
struct TransformConfig
{
    std::string unit{"mm"};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
    std::array<double, 3> rotation_deg{0.0, 0.0, 0.0};
    std::array<double, 3> translation_mm{0.0, 0.0, 0.0};
};

/** @brief 确定性的直角自动定向选项。 */
struct AutoOrientConfig
{
    bool enabled{true};
    double max_height_mm{9.0};
    std::string strategy{"minimize_height_by_right_angle_rotation"};
};

/** @brief 独立于 SliceConfig、仅包含模型加载所需字段的配置。 */
struct ModelLoadConfig
{
    InputConfig input;
    std::filesystem::path output_package_dir;
    TransformConfig transform;
    AutoOrientConfig auto_orient;
};

}  // namespace slicer_core
