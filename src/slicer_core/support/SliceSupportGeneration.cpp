// 支撑掩膜生成的实现（F-09 第 4 步从 slicer.cpp 搬出）。
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。
// IslandComponent 实测只被本簇使用，随簇进匿名命名空间。

#include "slicer_core/support/SliceSupportGeneration.h"

#include "slicer_core/support/BoundedReliefSupportPlan.h"
#include "slicer_core/support/InternalVoidSupport.h"
#include "slicer_core/support/SupportConnectivityAnalysis.h"
#include "slicer_core/support/SupportShapePipeline.h"
#include "slicer_core/support/SupportShapePolicy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace slicer_core::support {

namespace {

struct IslandComponent {
    int layer_index{0};
    int component_id{0};
    int area_px{0};
    int overlap_px{0};
    double overlap_ratio{0.0};
    bool filtered{false};
    std::vector<int> pixels;
};

bool support_mode_includes_bottom_projection(const std::string& mode) {
    return mode == "bottom_projection" || mode == "bottom_projection_plus_unsupported";
}

bool support_mode_includes_unsupported(const std::string& mode) {
    return mode == "unsupported_only" || mode == "bottom_projection_plus_unsupported";
}

std::string support_type_name(const SupportType type) {
    switch (type) {
        case SupportType::BottomProjection:
            return "bottom_projection";
        case SupportType::UnsupportedIsland:
            return "unsupported_island";
        case SupportType::FullVerticalProjection:
            return "full_vertical_projection";
        case SupportType::InternalVoid:
            return "internal_void";
        case SupportType::UpperProjection:
            return "upper_projection";
        case SupportType::ProjectionBase:
            return "projection_base";
        case SupportType::None:
            return "none";
    }
    return "none";
}

void AddUpperProjectionSupport(
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<ColumnLayerRange>& columnRanges,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    std::vector<std::vector<SupportType>>& supportTypeMaps)
{
    for (std::size_t index{0}; index < columnRanges.size(); ++index)
    {
        const ColumnLayerRange& range = columnRanges.at(index);
        if (!range.hasModel || range.upperLayer < 0)
        {
            continue;
        }
        for (int layerIndex{range.upperLayer + 1}; layerIndex < grid.layer_count; ++layerIndex)
        {
            if (modelMasks.at(layerIndex).at(index) != 0)
            {
                continue;
            }
            set_support_pixel(
                supportMasks.at(layerIndex),
                supportTypeMaps.at(layerIndex),
                index,
                SupportType::UpperProjection);
        }
    }
}

std::vector<std::uint8_t> make_supported_base_mask(
    const std::vector<std::uint8_t>& previous_model_mask,
    const std::vector<std::uint8_t>& previous_support_mask,
    const GridSpec& grid,
    const int dilation_px) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> base(pixel_count, 0);
    for (std::size_t i{0}; i < pixel_count; ++i) {
        if (previous_model_mask.at(i) != 0 || previous_support_mask.at(i) != 0) {
            base.at(i) = 1;
        }
    }
    for (int iteration{0}; iteration < dilation_px; ++iteration) {
        std::vector<std::uint8_t> dilated = base;
        for (int y{0}; y < grid.height_px; ++y) {
            for (int x{0}; x < grid.width_px; ++x) {
                const std::size_t index = mask_index(grid, x, y);
                if (base.at(index) == 0) {
                    continue;
                }
                for (int dy{-1}; dy <= 1; ++dy) {
                    for (int dx{-1}; dx <= 1; ++dx) {
                        const int nx{x + dx};
                        const int ny{y + dy};
                        if (nx >= 0 && nx < grid.width_px && ny >= 0 && ny < grid.height_px) {
                            dilated.at(mask_index(grid, nx, ny)) = 1;
                        }
                    }
                }
            }
        }
        base = std::move(dilated);
    }
    return base;
}

std::vector<IslandComponent> find_island_components(
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<std::uint8_t>& base_mask,
    const GridSpec& grid,
    const SliceConfig& config,
    const int layer_index) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> visited(pixel_count, 0);
    std::vector<IslandComponent> islands;
    int component_id{0};
    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    for (int y{0}; y < grid.height_px; ++y) {
        for (int x{0}; x < grid.width_px; ++x) {
            const std::size_t start = mask_index(grid, x, y);
            if (model_mask.at(start) == 0 || visited.at(start) != 0) {
                continue;
            }

            IslandComponent component;
            component.layer_index = layer_index;
            component.component_id = component_id++;
            std::vector<int> stack{static_cast<int>(start)};
            visited.at(start) = 1;
            while (!stack.empty()) {
                const int current = stack.back();
                stack.pop_back();
                component.pixels.push_back(current);
                const int cx = current % grid.width_px;
                const int cy = current / grid.width_px;
                if (base_mask.at(static_cast<std::size_t>(current)) != 0) {
                    ++component.overlap_px;
                }

                if (config.support.connectivity == 8) {
                    for (const auto& neighbor : neighbors8) {
                        const int nx{cx + neighbor.at(0)};
                        const int ny{cy + neighbor.at(1)};
                        if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                            continue;
                        }
                        const std::size_t next = mask_index(grid, nx, ny);
                        if (model_mask.at(next) != 0 && visited.at(next) == 0) {
                            visited.at(next) = 1;
                            stack.push_back(static_cast<int>(next));
                        }
                    }
                } else {
                    for (const auto& neighbor : neighbors4) {
                        const int nx{cx + neighbor.at(0)};
                        const int ny{cy + neighbor.at(1)};
                        if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                            continue;
                        }
                        const std::size_t next = mask_index(grid, nx, ny);
                        if (model_mask.at(next) != 0 && visited.at(next) == 0) {
                            visited.at(next) = 1;
                            stack.push_back(static_cast<int>(next));
                        }
                    }
                }
            }

            component.area_px = static_cast<int>(component.pixels.size());
            component.overlap_ratio = component.area_px > 0
                ? static_cast<double>(component.overlap_px) / static_cast<double>(component.area_px)
                : 0.0;
            if (component.overlap_ratio < config.support.min_overlap_ratio) {
                component.filtered = component.area_px < config.support.min_island_area_px;
                islands.push_back(std::move(component));
            }
        }
    }
    return islands;
}

}  // namespace

void LiftModelForSupportBase(
    ModelReport& modelReport,
    const double modelLiftMm)
{
    if (modelLiftMm <= 0.0)
    {
        return;
    }

    for (Triangle& triangle : modelReport.triangles)
    {
        triangle.a.z += modelLiftMm;
        triangle.b.z += modelLiftMm;
        triangle.c.z += modelLiftMm;
    }
    modelReport.bbox_mm.min.z += modelLiftMm;
    modelReport.bbox_mm.max.z += modelLiftMm;
}

int ApplyOuterVarnishSupportPriority(
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    SupportGenerationResult& supportGeneration,
    std::vector<std::vector<std::size_t>>* clearedSupportIndices)
{
    if (clearedSupportIndices != nullptr)
    {
        clearedSupportIndices->assign(supportGeneration.support_masks.size(), {});
    }

    if (outerVarnishMasks.empty())
    {
        return 0;
    }

    int clearedSupportPixels{0};
    for (std::size_t layerIndex{0}; layerIndex < supportGeneration.support_masks.size(); ++layerIndex)
    {
        std::vector<std::uint8_t>& supportMask = supportGeneration.support_masks.at(layerIndex);
        std::vector<SupportType>& supportTypeMap = supportGeneration.support_type_maps.at(layerIndex);
        const std::vector<std::uint8_t>& varnishMask = outerVarnishMasks.at(layerIndex);
        for (std::size_t index{0}; index < supportMask.size(); ++index)
        {
            if (varnishMask.at(index) == 0 || supportMask.at(index) == 0)
            {
                continue;
            }
            supportMask.at(index) = 0;
            supportTypeMap.at(index) = SupportType::None;
            if (clearedSupportIndices != nullptr)
            {
                clearedSupportIndices->at(layerIndex).push_back(index);
            }
            ++clearedSupportPixels;
        }
    }
    return clearedSupportPixels;
}

SupportPlacementPolicy ResolveSupportPlacementPolicy(const SliceConfig& config)
{
    SupportPlacementPolicy policy;
    policy.requested_placement = config.support.placement;
    policy.effective_placement = config.support.placement;
    policy.placement_explicit = config.support.placement_explicit;

    if (config.support.placement_explicit)
    {
        policy.lower_enabled = config.support.placement == "lower" || config.support.placement == "both";
        policy.upper_enabled = config.support.placement == "upper" || config.support.placement == "both";
        policy.unsupported_only_enabled = config.support.placement == "unsupported_only";
        policy.full_vertical_projection_enabled = config.support.placement == "full_vertical_projection";
        policy.advanced_debug = policy.full_vertical_projection_enabled;
        return policy;
    }

    policy.requested_placement = "legacy_mode";
    policy.effective_placement = config.support.mode;
    policy.lower_enabled = support_mode_includes_bottom_projection(config.support.mode);
    policy.upper_enabled = false;
    policy.unsupported_only_enabled = support_mode_includes_unsupported(config.support.mode);
    policy.full_vertical_projection_enabled = config.support.mode == "full_vertical_projection";
    policy.advanced_debug = policy.full_vertical_projection_enabled;
    return policy;
}

SupportGenerationResult generate_support_masks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const std::vector<std::vector<std::uint8_t>>& upper_boundary_masks,
    const std::vector<int>& support_source_layers,
    const std::vector<ColumnLayerRange>& column_ranges,
    const std::vector<ColumnLayerRange>& upper_boundary_column_ranges,
    std::vector<LayerDiagnostics>& diagnostics) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    const SupportPlacementPolicy placement_policy = ResolveSupportPlacementPolicy(config);
    SupportGenerationResult result;
    result.support_masks.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixel_count, 0));
    result.support_type_maps.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<SupportType>(pixel_count, SupportType::None));

    if (!config.support.enabled) {
        return result;
    }

    if (placement_policy.lower_enabled) {
        for (std::size_t index{0}; index < support_source_layers.size(); ++index) {
            const int lower_layer = support_source_layers.at(index);
            for (int layer_index{0}; layer_index < lower_layer; ++layer_index) {
                if (model_masks.at(layer_index).at(index) == 0) {
                    set_support_pixel(
                        result.support_masks.at(layer_index),
                        result.support_type_maps.at(layer_index),
                        index,
                        SupportType::BottomProjection);
                }
            }
        }
    }

    if (placement_policy.full_vertical_projection_enabled) {
        const std::vector<int> last_model_layers =
            ComputeRetainedLastModelLayers(model_masks, grid);
        for (std::size_t index{0}; index < last_model_layers.size(); ++index) {
            const int last_layer = last_model_layers.at(index);
            for (int layer_index{0}; layer_index < last_layer; ++layer_index) {
                if (model_masks.at(layer_index).at(index) == 0) {
                    set_support_pixel(
                        result.support_masks.at(layer_index),
                        result.support_type_maps.at(layer_index),
                        index,
                        SupportType::FullVerticalProjection);
                }
            }
        }
    }

    if (placement_policy.upper_enabled) {
        AddUpperProjectionSupport(
            grid,
            upper_boundary_masks,
            upper_boundary_column_ranges,
            result.support_masks,
            result.support_type_maps);
    }

    if (placement_policy.unsupported_only_enabled) {
        for (int layer_index{1}; layer_index < grid.layer_count; ++layer_index) {
            const std::vector<std::uint8_t> base_mask = make_supported_base_mask(
                model_masks.at(layer_index - 1),
                result.support_masks.at(layer_index - 1),
                grid,
                config.support.xy_dilation_px);
            std::vector<IslandComponent> islands = find_island_components(
                model_masks.at(layer_index),
                base_mask,
                grid,
                config,
                layer_index);

            for (const IslandComponent& island : islands) {
                if (island.filtered) {
                    ++diagnostics.at(layer_index).filtered_island_count;
                    diagnostics.at(layer_index).filtered_island_pixels += island.area_px;
                    continue;
                }
                ++diagnostics.at(layer_index).island_count;
                diagnostics.at(layer_index).island_pixels += island.area_px;
                diagnostics.at(layer_index).unsupported_pixels += island.area_px;
                for (int target_layer{0}; target_layer < layer_index; ++target_layer) {
                    for (const int pixel : island.pixels) {
                        const std::size_t index = static_cast<std::size_t>(pixel);
                        if (model_masks.at(target_layer).at(index) == 0) {
                            set_support_pixel(
                                result.support_masks.at(target_layer),
                                result.support_type_maps.at(target_layer),
                                index,
                                SupportType::UnsupportedIsland);
                        }
                    }
                }
            }
        }
    }

    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index)
    {
        AddInternalVoidSupportForLayer(
            config,
            grid,
            model_masks.at(layer_index),
            result.support_masks.at(layer_index),
            result.support_type_maps.at(layer_index));
    }

    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        if (diagnostics.at(layer_index).island_count > 0 || diagnostics.at(layer_index).filtered_island_count > 0) {
            ++result.layers_with_islands;
            result.island_count += diagnostics.at(layer_index).island_count;
            result.island_pixels += diagnostics.at(layer_index).island_pixels;
            result.unsupported_pixels += diagnostics.at(layer_index).unsupported_pixels;
            result.filtered_island_count += diagnostics.at(layer_index).filtered_island_count;
            result.filtered_island_pixels += diagnostics.at(layer_index).filtered_island_pixels;
        }
    }

    return result;
}

/**
 * @brief 把逐层支撑统计累加进 result 与该层 diagnostics。
 *
 * MF-03X2a 从 CalculateSupportGenerationStats 原样抽出，使有界路径能在层循环内
 * 逐层累积，而不必先物化整栈。retained 路径由下方包装函数逐层调用本函数，
 * 累加顺序与原实现一致，故统计值逐字不变。
 */
void AccumulateSupportLayerStats(
    SupportGenerationResult& result,
    LayerDiagnostics& layerDiagnostics,
    const std::vector<std::uint8_t>& support_mask,
    const std::vector<SupportType>& support_type_map,
    const GridSpec& grid,
    const SliceConfig& config)
{
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    bool layer_has_support{false};
    layerDiagnostics.bottom_projection_support_pixels = 0;
    layerDiagnostics.unsupported_island_support_pixels = 0;
    layerDiagnostics.full_vertical_projection_support_pixels = 0;
    layerDiagnostics.internal_void_support_pixels = 0;
    layerDiagnostics.upper_projection_support_pixels = 0;
    layerDiagnostics.projection_base_support_pixels = 0;
    for (std::size_t index{0}; index < pixel_count; ++index)
    {
        if (support_mask.at(index) == 0)
        {
            continue;
        }
        layer_has_support = true;
        ++result.support_pixels;
        switch (support_type_map.at(index))
        {
            case SupportType::BottomProjection:
                ++result.bottom_projection_support_pixels;
                ++layerDiagnostics.bottom_projection_support_pixels;
                break;
            case SupportType::UnsupportedIsland:
                ++result.unsupported_island_support_pixels;
                ++layerDiagnostics.unsupported_island_support_pixels;
                break;
            case SupportType::FullVerticalProjection:
                ++result.full_vertical_projection_support_pixels;
                ++layerDiagnostics.full_vertical_projection_support_pixels;
                break;
            case SupportType::InternalVoid:
                ++result.internal_void_support_pixels;
                ++layerDiagnostics.internal_void_support_pixels;
                break;
            case SupportType::UpperProjection:
                ++result.upper_projection_support_pixels;
                ++layerDiagnostics.upper_projection_support_pixels;
                break;
            case SupportType::ProjectionBase:
                ++result.projection_base_support_pixels;
                ++layerDiagnostics.projection_base_support_pixels;
                break;
            case SupportType::None:
                break;
        }
    }
    if (layer_has_support)
    {
        ++result.layers_with_support;
    }
    layerDiagnostics.support_connectivity =
        analyze_support_connectivity(support_mask, grid, config.support.connectivity);
}

/// 统计累加前的清零。retained 与有界两条路径共用，避免两处漂移。
void ResetSupportGenerationStats(SupportGenerationResult& result)
{
    result.support_pixels = 0;
    result.bottom_projection_support_pixels = 0;
    result.unsupported_island_support_pixels = 0;
    result.full_vertical_projection_support_pixels = 0;
    result.internal_void_support_pixels = 0;
    result.upper_projection_support_pixels = 0;
    result.projection_base_support_pixels = 0;
    result.layers_with_support = 0;
}

void CalculateSupportGenerationStats(
    SupportGenerationResult& result,
    std::vector<LayerDiagnostics>& diagnostics,
    const GridSpec& grid,
    const SliceConfig& config)
{
    ResetSupportGenerationStats(result);
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index)
    {
        AccumulateSupportLayerStats(
            result,
            diagnostics.at(layer_index),
            result.support_masks.at(layer_index),
            result.support_type_maps.at(layer_index),
            grid,
            config);
    }
}

Json BuildSupportBaseProjectionReport(
    const SupportBaseProjectionResult& result,
    const SupportGenerationResult& supportGeneration)
{
    Json::Array effectiveLayerRange;
    if (result.effective_layer_count > 0)
    {
        effectiveLayerRange.push_back(0);
        effectiveLayerRange.push_back(result.effective_layer_count - 1);
    }
    return Json::object({
        {"configuredEnabled", result.enabled},
        {"effectiveEnabled",
         result.enabled
             && result.effective_layer_count > 0
             && result.footprint_pixels > 0},
        {"configuredLayerCount", result.configured_layer_count},
        {"effectiveLayerCount", result.effective_layer_count},
        {"effectiveLayerRange", Json{effectiveLayerRange}},
        {"layerPlacement", result.layer_placement},
        {"addedLayerCount", result.added_layer_count},
        {"modelLiftMm", result.model_lift_mm},
        {"source", "max_support_footprint"},
        {"footprintPixels", result.footprint_pixels},
        {"addedSupportPixelsBeforeMaterialPriority",
         result.added_support_pixels},
        {"printPixels",
         supportGeneration.projection_base_support_pixels},
    });
}

}  // namespace slicer_core::support
