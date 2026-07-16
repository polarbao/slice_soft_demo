#pragma once

#include "slicer_core/SliceRunTelemetry.h"

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
    bool non_production{false};
    SliceRunProfile profile;
};

/**
 * @brief Output controls for the explicit OpenVDB candidate package writer path.
 */
struct OpenVdbCandidatePipelineOptions
{
    bool write_tiff_layers{true};
    bool write_preview_files{true};
    bool write_reports{true};
    bool publish_package{true};
    SliceRunProgressCallback progress_callback;
};

/**
 * @brief Run the explicit OpenVDB candidate package writer path.
 * @param configPath Slice config path.
 * @return Candidate package summary.
 */
OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(const std::filesystem::path& configPath);

/**
 * @brief Run the explicit OpenVDB candidate path with configurable output controls.
 * @param configPath Slice config path.
 * @param options Output controls. Use disabled outputs only for benchmark/diagnostic paths.
 * @return Candidate package summary.
 */
OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(
    const std::filesystem::path& configPath,
    const OpenVdbCandidatePipelineOptions& options);

}  // namespace slicer_core
