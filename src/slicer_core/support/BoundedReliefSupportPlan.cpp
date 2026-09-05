#include "slicer_core/support/BoundedReliefSupportPlan.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace slicer_core
{

namespace
{

/// 与 slicer.cpp `support_mode_includes_bottom_projection` 保持同一判据。
[[nodiscard]] bool ModeIncludesBottomProjection(const std::string& mode)
{
    return mode == "bottom_projection" || mode == "bottom_projection_plus_unsupported";
}

/// 与 slicer.cpp `support_mode_includes_unsupported` 保持同一判据。
[[nodiscard]] bool ModeIncludesUnsupported(const std::string& mode)
{
    return mode == "unsupported_only" || mode == "bottom_projection_plus_unsupported";
}

[[nodiscard]] BoundedReliefSupportEligibility Reject(std::string reason)
{
    BoundedReliefSupportEligibility result;
    result.eligible = false;
    result.reason = std::move(reason);
    return result;
}

}  // namespace

BoundedReliefSupportEligibility EvaluateBoundedReliefSupportPath(
    const SliceConfig& config)
{
    if (config.slicing_mode != "relief_heightfield")
    {
        return Reject("slicing_mode_not_relief_heightfield");
    }
    if (config.geometry_sampling.strategy != "legacy_center_sample")
    {
        return Reject("geometry_sampling_strategy_not_legacy_center_sample");
    }

    // 放置策略：显式 placement 与 legacy mode 两条解析路径都要覆盖，
    // 判据与 slicer.cpp ResolveSupportPlacementPolicy 一一对应。
    if (config.support.placement_explicit)
    {
        if (config.support.placement != "lower")
        {
            return Reject("support_placement_not_lower");
        }
    }
    else
    {
        if (!ModeIncludesBottomProjection(config.support.mode))
        {
            return Reject("support_mode_without_bottom_projection");
        }
        if (ModeIncludesUnsupported(config.support.mode))
        {
            return Reject("support_mode_includes_unsupported");
        }
        if (config.support.mode == "full_vertical_projection")
        {
            return Reject("support_mode_full_vertical_projection");
        }
    }

    if (config.support.shape_enabled)
    {
        return Reject("support_shape_enabled");
    }
    if (config.support.base_projection.enabled)
    {
        return Reject("support_base_projection_enabled");
    }
    if (config.outer_varnish.enabled)
    {
        return Reject("outer_varnish_enabled");
    }

    BoundedReliefSupportEligibility result;
    result.eligible = true;
    result.reason = "relief_bottom_projection_bounded";
    return result;
}

std::vector<std::uint32_t> BuildBoundedActiveColumns(
    const std::vector<BoundedReliefColumnSpan>& spans,
    const int widthPx,
    const int heightPx)
{
    const std::size_t columnCount =
        static_cast<std::size_t>(widthPx) * static_cast<std::size_t>(heightPx);
    if (widthPx <= 0 || heightPx <= 0 || spans.size() != columnCount)
    {
        throw std::runtime_error(
            "BuildBoundedActiveColumns received spans that do not match the grid");
    }

    const auto neverHasModel = [&spans](const std::size_t index) {
        const BoundedReliefColumnSpan& span = spans[index];
        return !span.hasModel || span.lowerLayer < 0;
    };

    // 从幅面边界出发，在【所有层都无模型】的列上做 4 邻接洪泛。
    std::vector<std::uint8_t> alwaysExternal(columnCount, 0U);
    std::vector<std::uint32_t> stack;
    const auto push = [&](const int x, const int y) {
        const std::size_t index =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(widthPx)
            + static_cast<std::size_t>(x);
        if (alwaysExternal[index] != 0U || !neverHasModel(index))
        {
            return;
        }
        alwaysExternal[index] = 1U;
        stack.push_back(static_cast<std::uint32_t>(index));
    };
    for (int x{0}; x < widthPx; ++x)
    {
        push(x, 0);
        push(x, heightPx - 1);
    }
    for (int y{0}; y < heightPx; ++y)
    {
        push(0, y);
        push(widthPx - 1, y);
    }
    while (!stack.empty())
    {
        const std::size_t index = stack.back();
        stack.pop_back();
        const int x = static_cast<int>(index % static_cast<std::size_t>(widthPx));
        const int y = static_cast<int>(index / static_cast<std::size_t>(widthPx));
        if (x > 0) { push(x - 1, y); }
        if (x + 1 < widthPx) { push(x + 1, y); }
        if (y > 0) { push(x, y - 1); }
        if (y + 1 < heightPx) { push(x, y + 1); }
    }

    std::vector<std::uint32_t> active;
    for (std::size_t index{0}; index < columnCount; ++index)
    {
        if (alwaysExternal[index] == 0U)
        {
            active.push_back(static_cast<std::uint32_t>(index));
        }
    }
    return active;
}

void MaterializeReliefModelLayer(
    const std::vector<BoundedReliefColumnSpan>& spans,
    const int layerIndex,
    std::vector<std::uint8_t>& outModelMask,
    const std::vector<std::uint32_t>* activeColumns)
{
    if (outModelMask.size() != spans.size())
    {
        throw std::runtime_error(
            "MaterializeReliefModelLayer received a mask buffer whose size does not"
            " match the column span count");
    }
    if (activeColumns == nullptr)
    {
        std::fill(outModelMask.begin(), outModelMask.end(), static_cast<std::uint8_t>(0));
    }
    const std::size_t iterationCount =
        activeColumns != nullptr ? activeColumns->size() : spans.size();
    for (std::size_t iteration{0}; iteration < iterationCount; ++iteration)
    {
        const std::size_t index = activeColumns != nullptr
            ? static_cast<std::size_t>(activeColumns->at(iteration))
            : iteration;
        if (activeColumns != nullptr)
        {
            outModelMask.at(index) = 0;
        }
        const BoundedReliefColumnSpan& span = spans.at(index);
        if (!span.hasModel || span.lowerLayer < 0)
        {
            continue;
        }
        if (layerIndex >= span.lowerLayer && layerIndex <= span.upperLayer)
        {
            outModelMask.at(index) = 1;
        }
    }
}

void MaterializeBottomProjectionSupportLayer(
    const std::vector<int>& supportSourceLayers,
    const std::vector<std::uint8_t>& modelMask,
    const bool supportEnabled,
    const int layerIndex,
    std::vector<std::uint8_t>& outSupportMask,
    std::vector<SupportType>& outSupportTypeMap,
    const std::vector<std::uint32_t>* activeColumns)
{
    if (outSupportMask.size() != modelMask.size()
        || outSupportTypeMap.size() != modelMask.size()
        || supportSourceLayers.size() != modelMask.size())
    {
        throw std::runtime_error(
            "MaterializeBottomProjectionSupportLayer received buffers whose sizes do"
            " not agree");
    }
    const std::size_t iterationCount =
        activeColumns != nullptr ? activeColumns->size() : modelMask.size();
    if (activeColumns == nullptr)
    {
        std::fill(outSupportMask.begin(), outSupportMask.end(), static_cast<std::uint8_t>(0));
        std::fill(outSupportTypeMap.begin(), outSupportTypeMap.end(), SupportType::None);
    }
    else
    {
        for (const std::uint32_t column : *activeColumns)
        {
            outSupportMask.at(column) = 0;
            outSupportTypeMap.at(column) = SupportType::None;
        }
    }
    if (!supportEnabled)
    {
        return;
    }
    for (std::size_t iteration{0}; iteration < iterationCount; ++iteration)
    {
        const std::size_t index = activeColumns != nullptr
            ? static_cast<std::size_t>(activeColumns->at(iteration))
            : iteration;
        if (layerIndex >= supportSourceLayers.at(index))
        {
            continue;
        }
        if (modelMask.at(index) != 0)
        {
            continue;
        }
        // 与 set_support_pixel 同构：type map 每层重置为 None，
        // 且本路径只写 BottomProjection，故优先级判据恒成立，仍保留以示同源。
        outSupportMask.at(index) = 1;
        if (SupportTypePriority(SupportType::BottomProjection)
            >= SupportTypePriority(outSupportTypeMap.at(index)))
        {
            outSupportTypeMap.at(index) = SupportType::BottomProjection;
        }
    }
}

}  // namespace slicer_core
