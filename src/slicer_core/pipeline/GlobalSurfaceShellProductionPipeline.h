#pragma once

#include "slicer_core/config.h"
#include "slicer_core/slicer.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Decision for the explicitly restricted Global production Profile.
 */
struct GlobalSurfaceShellProductionProfileDecision
{
    bool allowed{false};
    std::string productionAcceptance{"not_evaluated"};
    SlicePipelineErrorCode errorCode{SlicePipelineErrorCode::None};
    std::string detail;
};

/**
 * @brief Evaluate an explicitly admitted Global production Profile.
 * @param config Parsed slice configuration.
 * @return Auditable decision. Unsupported material combinations remain fail-closed.
 */
GlobalSurfaceShellProductionProfileDecision
EvaluateGlobalSurfaceShellProductionProfile(const SliceConfig& config);

/**
 * @brief Run an already preflight-admitted restricted Global production Profile.
 * @param configPath Slice configuration path.
 * @param options Runtime output and progress options.
 * @return Production package summary.
 * @throws SlicePipelineError when the Profile or Global evidence is not admitted.
 */
SliceRunResult RunGlobalSurfaceShellProductionPipeline(
    const std::filesystem::path& configPath,
    const SliceRunOptions& options);

}  // namespace slicer_core
