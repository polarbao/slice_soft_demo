#include "slicer_core/support/SupportShapePipeline.h"

#include <utility>

namespace slicer_core
{

SupportShapeOptimizationResult ApplySupportShapePolicy(
    const SupportShapePolicy& policy,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    const int width,
    const int height,
    const int connectivity)
{
    return OptimizeSupportShape(policy, modelMasks, supportMasks, width, height, connectivity);
}

SupportShapeOptimizationResult OptimizeSupportShapeForLayer(
    const SupportShapePolicy& policy,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    const int width,
    const int height,
    const int connectivity)
{
    const std::vector<std::vector<std::uint8_t>> modelMasks{modelMask};
    std::vector<std::vector<std::uint8_t>> supportMasks{supportMask};
    SupportShapeOptimizationResult result =
        ApplySupportShapePolicy(policy, modelMasks, supportMasks, width, height, connectivity);
    supportMask = std::move(supportMasks.at(0));
    return result;
}

}  // namespace slicer_core
