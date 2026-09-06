#include "slicer_core/support/SupportShapePipeline.h"

#include <utility>

namespace slicer_core
{

void SynchronizeSupportShapeTypesForLayer(
    const std::vector<std::uint8_t>& originalSupportMask,
    const std::vector<std::uint8_t>& optimizedSupportMask,
    std::vector<SupportType>& supportTypeMap)
{
    for (std::size_t index{0}; index < optimizedSupportMask.size(); ++index)
    {
        if (optimizedSupportMask.at(index) == 0)
        {
            supportTypeMap.at(index) = SupportType::None;
        }
        else if (originalSupportMask.at(index) == 0
                 && supportTypeMap.at(index) == SupportType::None)
        {
            supportTypeMap.at(index) = SupportType::BottomProjection;
        }
    }
}

void SynchronizeSupportShapeTypeMaps(
    const std::vector<std::vector<std::uint8_t>>& originalSupportMasks,
    const std::vector<std::vector<std::uint8_t>>& optimizedSupportMasks,
    std::vector<std::vector<SupportType>>& supportTypeMaps)
{
    for (std::size_t layerIndex{0};
         layerIndex < optimizedSupportMasks.size();
         ++layerIndex)
    {
        SynchronizeSupportShapeTypesForLayer(
            originalSupportMasks.at(layerIndex),
            optimizedSupportMasks.at(layerIndex),
            supportTypeMaps.at(layerIndex));
    }
}

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
