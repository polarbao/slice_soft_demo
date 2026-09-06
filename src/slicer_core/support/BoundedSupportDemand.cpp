#include "slicer_core/support/BoundedSupportDemand.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace slicer_core
{
namespace
{

void ValidateOptionalLayerFact(
    const int value,
    const int layerCount,
    const char* const field)
{
    if (value < -1 || value >= layerCount)
    {
        throw std::invalid_argument(field);
    }
}

void SetSupportPixel(
    const std::size_t index,
    const SupportType candidate,
    const std::span<std::uint8_t> supportMask,
    const std::span<SupportType> typeMap)
{
    supportMask[index] = 1U;
    if (SupportTypePriority(candidate)
        >= SupportTypePriority(typeMap[index]))
    {
        typeMap[index] = candidate;
    }
}

bool ByteRangesOverlap(
    const void* const leftData,
    const std::size_t leftBytes,
    const void* const rightData,
    const std::size_t rightBytes) noexcept
{
    if (leftBytes == 0U || rightBytes == 0U)
    {
        return false;
    }
    const auto leftBegin{reinterpret_cast<std::uintptr_t>(leftData)};
    const auto rightBegin{reinterpret_cast<std::uintptr_t>(rightData)};
    if (leftBytes > std::numeric_limits<std::uintptr_t>::max() - leftBegin
        || rightBytes
            > std::numeric_limits<std::uintptr_t>::max() - rightBegin)
    {
        return true;
    }
    const std::uintptr_t leftEnd{leftBegin + leftBytes};
    const std::uintptr_t rightEnd{rightBegin + rightBytes};
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

template <typename Left, typename Right>
bool SpansOverlap(
    const std::span<Left> left,
    const std::span<Right> right) noexcept
{
    return ByteRangesOverlap(
        left.data(),
        left.size_bytes(),
        right.data(),
        right.size_bytes());
}

}  // namespace

BoundedSupportDemandPlan BuildBoundedSupportDemandPlan(
    const BoundedSupportDemandRequest& request)
{
    if (request.layerCount <= 0)
    {
        throw std::invalid_argument(
            "BoundedSupportDemandRequest layerCount must be positive");
    }
    if (request.inputKind
        != GeometryOccupancyInputKind::SingleIntervalHeightfield)
    {
        throw std::invalid_argument(
            "Bounded support demand requires SingleIntervalHeightfield input");
    }

    const std::size_t columnCount{request.lowerSourceLayers.size()};
    if (columnCount == 0U
        || request.modelLastLayers.size() != columnCount
        || request.upperBoundaryLastLayers.size() != columnCount
        || request.unsupportedTopExclusiveLayers.size() != columnCount)
    {
        throw std::invalid_argument(
            "Bounded support demand fact dimensions are inconsistent");
    }

    for (std::size_t index{0U}; index < columnCount; ++index)
    {
        const int lower{request.lowerSourceLayers[index]};
        const int modelLast{request.modelLastLayers[index]};
        const int upper{request.upperBoundaryLastLayers[index]};
        const int unsupportedTop{
            request.unsupportedTopExclusiveLayers[index]};
        ValidateOptionalLayerFact(
            lower,
            request.layerCount,
            "Bounded support lower source layer is invalid");
        ValidateOptionalLayerFact(
            modelLast,
            request.layerCount,
            "Bounded support model last layer is invalid");
        ValidateOptionalLayerFact(
            upper,
            request.layerCount,
            "Bounded support upper boundary layer is invalid");
        if (unsupportedTop < 0 || unsupportedTop >= request.layerCount)
        {
            throw std::invalid_argument(
                "Bounded support unsupported top-exclusive layer is invalid");
        }
        if (lower >= 0 && modelLast >= 0 && lower > modelLast)
        {
            throw std::invalid_argument(
                "Bounded support lower source exceeds model last layer");
        }
        if ((lower >= 0) != (modelLast >= 0))
        {
            throw std::invalid_argument(
                "Bounded support model range is partially empty");
        }
        if (request.upperEnabled
            && modelLast >= 0
            && (upper < modelLast))
        {
            throw std::invalid_argument(
                "Bounded support upper boundary does not contain the model");
        }
        if (unsupportedTop > 0
            && (lower < 0
                || unsupportedTop < lower
                || unsupportedTop > modelLast))
        {
            throw std::invalid_argument(
                "Bounded support unsupported source is outside the model range");
        }
    }

    BoundedSupportDemandPlan plan;
    plan.layerCount_ = request.layerCount;
    plan.columnCount_ = columnCount;
    plan.inputKind_ = request.inputKind;
    plan.lowerSourceLayers_.assign(
        request.lowerSourceLayers.begin(),
        request.lowerSourceLayers.end());
    plan.modelLastLayers_.assign(
        request.modelLastLayers.begin(),
        request.modelLastLayers.end());
    plan.upperBoundaryLastLayers_.assign(
        request.upperBoundaryLastLayers.begin(),
        request.upperBoundaryLastLayers.end());
    plan.unsupportedTopExclusiveLayers_.assign(
        request.unsupportedTopExclusiveLayers.begin(),
        request.unsupportedTopExclusiveLayers.end());
    plan.lowerEnabled_ = request.lowerEnabled;
    plan.fullVerticalEnabled_ = request.fullVerticalEnabled;
    plan.upperEnabled_ = request.upperEnabled;
    plan.unsupportedEnabled_ = request.unsupportedEnabled;
    return plan;
}

BoundedSupportDemandPlan FinalizeBoundedUnsupportedDemand(
    BoundedSupportDemandPlan&& preliminaryPlan,
    std::vector<int>&& unsupportedTopExclusiveLayers)
{
    if (preliminaryPlan.unsupportedEnabled_)
    {
        throw std::invalid_argument(
            "bounded support demand plan is already finalized for unsupported demand");
    }
    if (unsupportedTopExclusiveLayers.size()
        != preliminaryPlan.columnCount_)
    {
        throw std::invalid_argument(
            "bounded unsupported demand dimensions are inconsistent");
    }
    for (std::size_t index{0U};
         index < unsupportedTopExclusiveLayers.size();
         ++index)
    {
        const int unsupportedTop{unsupportedTopExclusiveLayers[index]};
        if (unsupportedTop < 0
            || unsupportedTop >= preliminaryPlan.layerCount_)
        {
            throw std::invalid_argument(
                "bounded unsupported demand source layer is invalid");
        }
        if (unsupportedTop > 0
            && (preliminaryPlan.lowerSourceLayers_[index] < 0
                || unsupportedTop
                    < preliminaryPlan.lowerSourceLayers_[index]
                || unsupportedTop
                    > preliminaryPlan.modelLastLayers_[index]))
        {
            throw std::invalid_argument(
                "bounded unsupported demand source is outside the model range");
        }
    }
    preliminaryPlan.unsupportedTopExclusiveLayers_ =
        std::move(unsupportedTopExclusiveLayers);
    preliminaryPlan.unsupportedEnabled_ = true;
    return std::move(preliminaryPlan);
}

void MaterializePreShapeSupportLayer(
    const BoundedSupportDemandPlan& plan,
    const int layerIndex,
    const std::span<const std::uint8_t> modelOccupancyMask,
    const std::span<const std::uint8_t> upperBoundaryOccupancyMask,
    const std::span<std::uint8_t> outputSupportMask,
    const std::span<SupportType> outputTypeMap)
{
    if (layerIndex < 0 || layerIndex >= plan.layerCount_)
    {
        throw std::invalid_argument(
            "Bounded support materialization layer index is out of range");
    }
    if (modelOccupancyMask.size() != plan.columnCount_
        || upperBoundaryOccupancyMask.size() != plan.columnCount_
        || outputSupportMask.size() != plan.columnCount_
        || outputTypeMap.size() != plan.columnCount_)
    {
        throw std::invalid_argument(
            "Bounded support materialization buffer size is invalid");
    }
    if (SpansOverlap(modelOccupancyMask, outputSupportMask)
        || SpansOverlap(modelOccupancyMask, outputTypeMap)
        || SpansOverlap(upperBoundaryOccupancyMask, outputSupportMask)
        || SpansOverlap(upperBoundaryOccupancyMask, outputTypeMap)
        || SpansOverlap(outputSupportMask, outputTypeMap))
    {
        throw std::invalid_argument(
            "Bounded support inputs and outputs must not overlap");
    }

    for (std::size_t index{0U}; index < plan.columnCount_; ++index)
    {
        if (modelOccupancyMask[index] > 1U
            || (plan.upperEnabled_
                && upperBoundaryOccupancyMask[index] > 1U))
        {
            throw std::invalid_argument(
                "Bounded support occupancy masks must be binary");
        }
    }

    std::fill(outputSupportMask.begin(), outputSupportMask.end(), 0U);
    std::fill(
        outputTypeMap.begin(),
        outputTypeMap.end(),
        SupportType::None);
    for (std::size_t index{0U}; index < plan.columnCount_; ++index)
    {
        if (modelOccupancyMask[index] != 0U)
        {
            continue;
        }
        if (plan.lowerEnabled_
            && plan.lowerSourceLayers_[index] >= 0
            && layerIndex < plan.lowerSourceLayers_[index])
        {
            SetSupportPixel(
                index,
                SupportType::BottomProjection,
                outputSupportMask,
                outputTypeMap);
        }
        if (plan.fullVerticalEnabled_
            && plan.modelLastLayers_[index] >= 0
            && layerIndex < plan.modelLastLayers_[index])
        {
            SetSupportPixel(
                index,
                SupportType::FullVerticalProjection,
                outputSupportMask,
                outputTypeMap);
        }
        if (plan.upperEnabled_
            && plan.upperBoundaryLastLayers_[index] >= 0
            && layerIndex > plan.upperBoundaryLastLayers_[index])
        {
            if (upperBoundaryOccupancyMask[index] == 0U)
            {
                SetSupportPixel(
                    index,
                    SupportType::UpperProjection,
                    outputSupportMask,
                    outputTypeMap);
            }
        }
        if (plan.unsupportedEnabled_
            && layerIndex
                < plan.unsupportedTopExclusiveLayers_[index])
        {
            SetSupportPixel(
                index,
                SupportType::UnsupportedIsland,
                outputSupportMask,
                outputTypeMap);
        }
    }
}

}  // namespace slicer_core
