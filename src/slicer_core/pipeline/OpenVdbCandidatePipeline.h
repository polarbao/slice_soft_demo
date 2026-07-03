#pragma once

#include <filesystem>

namespace slicer_core
{

/**
 * @brief Result summary for an OpenVDB candidate package run.
 */
struct OpenVdbCandidatePipelineResult
{
    std::filesystem::path package_dir;
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    int model_pixels{0};
    int support_pixels{0};
    int shell_pixels{0};
};

/**
 * @brief Run the explicit OpenVDB candidate package writer path.
 * @param configPath Slice config path.
 * @return Candidate package summary.
 */
OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(const std::filesystem::path& configPath);

}  // namespace slicer_core
