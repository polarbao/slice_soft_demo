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
 * @brief Run slicing through the legacy pipeline facade.
 * @param configPath Path to the slice config.
 * @param options Runtime options for slicing.
 * @return Slice run result from the legacy implementation.
 */
SliceRunResult RunSlicePipelineLegacy(const std::filesystem::path& configPath, const SliceRunOptions& options);

}  // namespace slicer_core
