#pragma once

#include "slicer_core/slicer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Return the canonical R1 pipeline step names.
 * @return Ordered list of pipeline step names.
 */
std::vector<std::string> DefaultSlicePipelineSteps();

/**
 * @brief Run the configured end-to-end production slice pipeline.
 * @param configPath Path to the slice config.
 * @param options Runtime options for slicing.
 * @return Slice run result from the selected production implementation.
 * @throws SlicePipelineError when the selected mode is not admitted.
 */
SliceRunResult RunSlicePipeline(
    const std::filesystem::path& configPath,
    const SliceRunOptions& options);

/**
 * @brief Run slicing through the fail-closed legacy preflight facade.
 * @param configPath Path to the slice config.
 * @param options Runtime options for slicing.
 * @return Slice run result from the legacy implementation.
 */
SliceRunResult RunSlicePipelineLegacy(const std::filesystem::path& configPath, const SliceRunOptions& options);

}  // namespace slicer_core
