#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Binary 2D mask used by experimental geometry kernel prototypes.
 */
struct BinaryMask2D
{
    int width{0};
    int height{0};
    double pixel_size_mm{0.01};
    std::vector<std::uint8_t> inside;
};

/**
 * @brief Signed distance field generated from a binary 2D mask.
 */
struct DistanceField2D
{
    int width{0};
    int height{0};
    double pixel_size_mm{0.01};
    std::vector<float> distance_mm;
};

/**
 * @brief Summary statistics for a distance field.
 */
struct DistanceFieldStats
{
    float min_distance_mm{0.0F};
    float max_distance_mm{0.0F};
    int negative_pixels{0};
    int positive_pixels{0};
    int zero_pixels{0};
};

/**
 * @brief Shell, interior, and boundary masks derived from a distance field.
 */
struct ShellMaskResult
{
    int width{0};
    int height{0};
    double pixel_size_mm{0.01};
    double shell_thickness_mm{0.05};
    std::vector<std::uint8_t> shell_mask;
    std::vector<std::uint8_t> interior_mask;
    std::vector<std::uint8_t> boundary_mask;
    int shell_pixels{0};
    int interior_pixels{0};
    int boundary_pixels{0};
};

/**
 * @brief OpenVDB dependency and runtime status.
 */
struct OpenVdbStatus
{
    bool compiled_with_openvdb{false};
    bool runtime_available{false};
    std::string version;
    std::vector<std::string> warnings;
};

/**
 * @brief Runtime result of the OpenVDB smoke case.
 */
struct OpenVdbSmokeResult
{
    OpenVdbStatus status;
    bool executed{false};
    bool skipped{false};
    int active_voxels{0};
    std::vector<std::string> warnings;
};

}  // namespace slicer_core
