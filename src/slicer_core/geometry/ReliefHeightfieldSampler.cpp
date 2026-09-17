// 浮雕高度场采样的实现（F-09 第 7 步从 slicer.cpp 搬出）。
//
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。
// kSupersample2x2Offsets 实测只被本簇使用，随簇进匿名命名空间。

#include "slicer_core/geometry/ReliefHeightfieldSampler.h"

#include "slicer_core/geometry/LayerOccupancyProvider.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/SliceMaskRasterizer.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/support/BoundedReliefSupportPlan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace slicer_core::relief {

namespace {

constexpr std::array<std::array<double, 2>, 4> kSupersample2x2Offsets{{
    {0.25, 0.25},
    {0.75, 0.25},
    {0.25, 0.75},
    {0.75, 0.75},
}};

bool IsProvenInteriorHeightfieldPixel(
    const std::vector<int>& hitCount,
    const GridSpec& grid,
    const int x,
    const int y)
{
    if (x <= 0 || y <= 0 || x >= grid.width_px - 1 || y >= grid.height_px - 1)
    {
        return false;
    }
    for (int yOffset{-1}; yOffset <= 1; ++yOffset)
    {
        for (int xOffset{-1}; xOffset <= 1; ++xOffset)
        {
            if (hitCount.at(mask_index(grid, x + xOffset, y + yOffset)) == 0)
            {
                return false;
            }
        }
    }
    return true;
}

void AccumulateReliefColumnReport(
    ReliefReportData& reliefReport,
    const ReliefColumnInfo& column)
{
    ++reliefReport.hit_columns;
    if (column.multi_hit)
    {
        ++reliefReport.multi_hit_columns;
    }
    const double thicknessMm{column.z_max_mm - column.z_min_mm};
    if (!reliefReport.has_hits)
    {
        reliefReport.z_min_mm = column.z_min_mm;
        reliefReport.z_max_mm = column.z_max_mm;
        reliefReport.thickness_min_mm = thicknessMm;
        reliefReport.thickness_max_mm = thicknessMm;
        reliefReport.has_hits = true;
        return;
    }
    reliefReport.z_min_mm = std::min(reliefReport.z_min_mm, column.z_min_mm);
    reliefReport.z_max_mm = std::max(reliefReport.z_max_mm, column.z_max_mm);
    reliefReport.thickness_min_mm = std::min(reliefReport.thickness_min_mm, thicknessMm);
    reliefReport.thickness_max_mm = std::max(reliefReport.thickness_max_mm, thicknessMm);
}

}  // namespace

ReliefSamplingResult sample_relief_heightfield_masks(
    const SliceConfig& config,
    const ModelReport& model_report,
    const GridSpec& grid,
    std::vector<LayerDiagnostics>& diagnostics,
    // M2：MATVOL 的材质名表。非空时按 (材质, 列) 记录逐材质顶面，使被遮住的
    // 下层材质也能采到自己的贴图；为空（MATVOL 未启用）时完全不建表，零开销。
    const std::span<const std::string>* planMaterialNames,
    // MF-03X2a：false 时【不分配也不填充】整栈 model mask，只产出 columns。
    // 10um 大幅面场景该整栈约 10.4 GB，是三个无条件分配之一。
    // 列区间仍照常写入 columns，主循环据此按层重建（见 BoundedReliefSupportPlan）。
    const bool materializeModelMaskStack)
{
    const std::size_t pixelCount{static_cast<std::size_t>(grid.width_px) * grid.height_px};
    std::vector<double> zMin(pixelCount, std::numeric_limits<double>::max());
    std::vector<double> zMax(pixelCount, std::numeric_limits<double>::lowest());
    std::vector<int> hitCount(pixelCount, 0);
    // M2：triangleIndex -> plan 材质下标。预算一次 O(三角数)，
    // 使采样热路径内只做 O(1) 查表，不做字符串比较。
    std::vector<std::uint32_t> planMaterialByTriangle;
    std::vector<double> zMaxByMaterial;
    const std::size_t planMaterialCount{
        planMaterialNames != nullptr ? planMaterialNames->size() : 0U};
    if (planMaterialCount > 0U && pixelCount > 0U) {
        planMaterialByTriangle.assign(
            model_report.triangle_textures.size(), kNoMaterialOwner);
        for (std::size_t triangle{0}; triangle < model_report.triangle_textures.size();
             ++triangle) {
            const std::string& name =
                model_report.triangle_textures.at(triangle).material_name;
            for (std::size_t index{0}; index < planMaterialCount; ++index) {
                if ((*planMaterialNames)[index] == name) {
                    planMaterialByTriangle.at(triangle) =
                        static_cast<std::uint32_t>(index);
                    break;
                }
            }
        }
        zMaxByMaterial.assign(
            planMaterialCount * pixelCount, std::numeric_limits<double>::lowest());
    }
    const bool useSupersampleAtLeastTwo = config.geometry_sampling.strategy
        == "layer_slab_supersample_2x2_at_least_two_candidate";
    const bool useSupersampleAnyHit = config.geometry_sampling.strategy
        == "layer_slab_supersample_2x2_any_hit_candidate";
    const bool useSupersampleCandidate{useSupersampleAtLeastTwo || useSupersampleAnyHit};
    const bool useLayerSlabCandidate = useSupersampleCandidate
        || config.geometry_sampling.strategy == "layer_slab_pixel_center_candidate";
    const GeometryOccupancyPolicy occupancyPolicy = useSupersampleCandidate
        ? MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(useSupersampleAtLeastTwo ? 2U : 1U)
        : (useLayerSlabCandidate
            ? MakeLayerSlabGeometryOccupancyPolicy()
            : MakeLegacyGeometryOccupancyPolicy());
    ValidateLayerOccupancyPolicy(occupancyPolicy);

    ReliefSamplingResult result;
    result.columns.resize(pixelCount);
    if (!zMaxByMaterial.empty()) {
        result.per_material_top.materialCount = planMaterialCount;
        result.per_material_top.columnCount = pixelCount;
        result.per_material_top.topTriangle.assign(
            planMaterialCount * pixelCount, -1);
        result.per_material_top.topBarycentric.assign(
            planMaterialCount * pixelCount, std::array<double, 3>{0.0, 0.0, 0.0});
    }
    ReliefReportData& reliefReport{result.report};
    reliefReport.total_columns = static_cast<int>(pixelCount);
    std::vector<std::uint8_t> projectedCandidateMask;
    if (useSupersampleCandidate)
    {
        projectedCandidateMask.assign(pixelCount, 0);
    }

    for (std::size_t triangleIndex{0}; triangleIndex < model_report.triangles.size(); ++triangleIndex)
    {
        const Triangle& triangle{model_report.triangles.at(triangleIndex)};
        const double minX{std::min({triangle.a.x, triangle.b.x, triangle.c.x})};
        const double maxX{std::max({triangle.a.x, triangle.b.x, triangle.c.x})};
        const double minY{std::min({triangle.a.y, triangle.b.y, triangle.c.y})};
        const double maxY{std::max({triangle.a.y, triangle.b.y, triangle.c.y})};
        int startX{static_cast<int>(std::floor((minX - grid.origin_x_mm) / grid.pixel_size_x_mm)) - 1};
        int endX{static_cast<int>(std::ceil((maxX - grid.origin_x_mm) / grid.pixel_size_x_mm)) + 1};
        int startY{static_cast<int>(std::floor((minY - grid.origin_y_mm) / grid.pixel_size_y_mm)) - 1};
        int endY{static_cast<int>(std::ceil((maxY - grid.origin_y_mm) / grid.pixel_size_y_mm)) + 1};
        startX = std::max(0, startX);
        startY = std::max(0, startY);
        endX = std::min(grid.width_px - 1, endX);
        endY = std::min(grid.height_px - 1, endY);
        if (startX > endX || startY > endY)
        {
            continue;
        }

        for (int y{startY}; y <= endY; ++y)
        {
            const double yMm{grid.origin_y_mm + (static_cast<double>(y) + 0.5) * grid.pixel_size_y_mm};
            for (int x{startX}; x <= endX; ++x)
            {
                const std::size_t pixelIndex{mask_index(grid, x, y)};
                if (useSupersampleCandidate)
                {
                    projectedCandidateMask.at(pixelIndex) = 1;
                }
                const double xMm{grid.origin_x_mm + (static_cast<double>(x) + 0.5) * grid.pixel_size_x_mm};
                double w0{0.0};
                double w1{0.0};
                double w2{0.0};
                if (!masks::point_in_triangle_xy({xMm, yMm, 0.0}, triangle, w0, w1, w2))
                {
                    continue;
                }
                const double zMm{w0 * triangle.a.z + w1 * triangle.b.z + w2 * triangle.c.z};
                zMin.at(pixelIndex) = std::min(zMin.at(pixelIndex), zMm);
                if (zMm >= zMax.at(pixelIndex))
                {
                    zMax.at(pixelIndex) = zMm;
                    ReliefColumnInfo& column{result.columns.at(pixelIndex)};
                    column.top_triangle_index = static_cast<int>(triangleIndex);
                    column.top_barycentric = {w0, w1, w2};
                }
                // M2：与上面的全列顶面判定并列——同一遍遍历内再记一份逐材质顶面，
                // 不引入额外的几何遍历开销。
                if (!zMaxByMaterial.empty()
                    && triangleIndex < planMaterialByTriangle.size())
                {
                    const std::uint32_t planMaterial =
                        planMaterialByTriangle.at(triangleIndex);
                    if (planMaterial != kNoMaterialOwner)
                    {
                        const std::size_t slot =
                            result.per_material_top.Index(planMaterial, pixelIndex);
                        if (zMm >= zMaxByMaterial.at(slot))
                        {
                            zMaxByMaterial.at(slot) = zMm;
                            result.per_material_top.topTriangle.at(slot) =
                                static_cast<int>(triangleIndex);
                            result.per_material_top.topBarycentric.at(slot) =
                                {w0, w1, w2};
                        }
                    }
                }
                ++hitCount.at(pixelIndex);
            }
        }
    }

    std::vector<GeometryOccupancyColumn> coverageSubsampleColumns;
    if (useSupersampleCandidate)
    {
        coverageSubsampleColumns.resize(pixelCount * kSupersample2x2Offsets.size());
        std::vector<std::uint8_t> boundaryCandidateMask(pixelCount, 0);
        std::vector<double> subsampleZMin(
            coverageSubsampleColumns.size(),
            std::numeric_limits<double>::max());
        std::vector<double> subsampleZMax(
            coverageSubsampleColumns.size(),
            std::numeric_limits<double>::lowest());
        std::vector<int> subsampleHitCount(coverageSubsampleColumns.size(), 0);
        std::vector<double> representativeTopZ{zMax};

        for (int y{0}; y < grid.height_px; ++y)
        {
            for (int x{0}; x < grid.width_px; ++x)
            {
                const std::size_t pixelIndex{mask_index(grid, x, y)};
                if (IsProvenInteriorHeightfieldPixel(hitCount, grid, x, y))
                {
                    const double startZ = config.relief.fill_mode == "surface_to_base"
                        ? config.relief.base_z_mm
                        : zMin.at(pixelIndex);
                    for (std::size_t sampleIndex{0}; sampleIndex < kSupersample2x2Offsets.size(); ++sampleIndex)
                    {
                        coverageSubsampleColumns.at(
                            pixelIndex * kSupersample2x2Offsets.size() + sampleIndex) = {
                                true,
                                startZ,
                                zMax.at(pixelIndex)};
                    }
                }
                else if (projectedCandidateMask.at(pixelIndex) != 0)
                {
                    boundaryCandidateMask.at(pixelIndex) = 1;
                }
            }
        }

        for (std::size_t triangleIndex{0}; triangleIndex < model_report.triangles.size(); ++triangleIndex)
        {
            const Triangle& triangle{model_report.triangles.at(triangleIndex)};
            const double minX{std::min({triangle.a.x, triangle.b.x, triangle.c.x})};
            const double maxX{std::max({triangle.a.x, triangle.b.x, triangle.c.x})};
            const double minY{std::min({triangle.a.y, triangle.b.y, triangle.c.y})};
            const double maxY{std::max({triangle.a.y, triangle.b.y, triangle.c.y})};
            int startX{static_cast<int>(std::floor((minX - grid.origin_x_mm) / grid.pixel_size_x_mm)) - 1};
            int endX{static_cast<int>(std::ceil((maxX - grid.origin_x_mm) / grid.pixel_size_x_mm)) + 1};
            int startY{static_cast<int>(std::floor((minY - grid.origin_y_mm) / grid.pixel_size_y_mm)) - 1};
            int endY{static_cast<int>(std::ceil((maxY - grid.origin_y_mm) / grid.pixel_size_y_mm)) + 1};
            startX = std::max(0, startX);
            startY = std::max(0, startY);
            endX = std::min(grid.width_px - 1, endX);
            endY = std::min(grid.height_px - 1, endY);

            for (int y{startY}; y <= endY; ++y)
            {
                for (int x{startX}; x <= endX; ++x)
                {
                    const std::size_t pixelIndex{mask_index(grid, x, y)};
                    if (boundaryCandidateMask.at(pixelIndex) == 0)
                    {
                        continue;
                    }
                    for (std::size_t sampleIndex{0}; sampleIndex < kSupersample2x2Offsets.size(); ++sampleIndex)
                    {
                        const double xMm = grid.origin_x_mm
                            + (static_cast<double>(x) + kSupersample2x2Offsets.at(sampleIndex).at(0))
                                * grid.pixel_size_x_mm;
                        const double yMm = grid.origin_y_mm
                            + (static_cast<double>(y) + kSupersample2x2Offsets.at(sampleIndex).at(1))
                                * grid.pixel_size_y_mm;
                        double w0{0.0};
                        double w1{0.0};
                        double w2{0.0};
                        if (!masks::point_in_triangle_xy({xMm, yMm, 0.0}, triangle, w0, w1, w2))
                        {
                            continue;
                        }
                        const double zMm{w0 * triangle.a.z + w1 * triangle.b.z + w2 * triangle.c.z};
                        const std::size_t flatSampleIndex{
                            pixelIndex * kSupersample2x2Offsets.size() + sampleIndex};
                        subsampleZMin.at(flatSampleIndex) = std::min(subsampleZMin.at(flatSampleIndex), zMm);
                        subsampleZMax.at(flatSampleIndex) = std::max(subsampleZMax.at(flatSampleIndex), zMm);
                        ++subsampleHitCount.at(flatSampleIndex);
                        zMin.at(pixelIndex) = std::min(zMin.at(pixelIndex), zMm);
                        zMax.at(pixelIndex) = std::max(zMax.at(pixelIndex), zMm);
                        hitCount.at(pixelIndex) = std::max(
                            hitCount.at(pixelIndex),
                            subsampleHitCount.at(flatSampleIndex));
                        if (zMm >= representativeTopZ.at(pixelIndex))
                        {
                            representativeTopZ.at(pixelIndex) = zMm;
                            ReliefColumnInfo& column{result.columns.at(pixelIndex)};
                            column.top_triangle_index = static_cast<int>(triangleIndex);
                            column.top_barycentric = {w0, w1, w2};
                        }
                        // M2：超采样档的逐材质顶面。与标准档同口径——
                        // 用与该档代表 Z 相同的比较量，避免两档产生不同的顶面选择。
                        if (!zMaxByMaterial.empty()
                            && triangleIndex < planMaterialByTriangle.size())
                        {
                            const std::uint32_t planMaterial =
                                planMaterialByTriangle.at(triangleIndex);
                            if (planMaterial != kNoMaterialOwner)
                            {
                                const std::size_t slot =
                                    result.per_material_top.Index(
                                        planMaterial, pixelIndex);
                                if (zMm >= zMaxByMaterial.at(slot))
                                {
                                    zMaxByMaterial.at(slot) = zMm;
                                    result.per_material_top.topTriangle.at(slot) =
                                        static_cast<int>(triangleIndex);
                                    result.per_material_top.topBarycentric.at(slot) =
                                        {w0, w1, w2};
                                }
                            }
                        }
                    }
                }
            }
        }

        for (std::size_t flatSampleIndex{0}; flatSampleIndex < coverageSubsampleColumns.size(); ++flatSampleIndex)
        {
            if (subsampleHitCount.at(flatSampleIndex) == 0)
            {
                continue;
            }
            const double startZ = config.relief.fill_mode == "surface_to_base"
                ? config.relief.base_z_mm
                : subsampleZMin.at(flatSampleIndex);
            coverageSubsampleColumns.at(flatSampleIndex) = {
                true,
                startZ,
                subsampleZMax.at(flatSampleIndex)};
        }
    }

    if (materializeModelMaskStack)
    {
        result.model_masks.resize(
            static_cast<std::size_t>(grid.layer_count),
            std::vector<std::uint8_t>(pixelCount, 0));
    }
    diagnostics.clear();
    diagnostics.reserve(grid.layer_count);
    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        LayerDiagnostics layerDiagnostics;
        layerDiagnostics.layer_index = layerIndex;
        layerDiagnostics.z_mm = (static_cast<double>(layerIndex) + 0.5)
            * config.output.layer_thickness_mm;
        diagnostics.push_back(layerDiagnostics);
    }

    std::vector<GeometryOccupancyColumn> occupancyColumns;
    if (useLayerSlabCandidate)
    {
        occupancyColumns.resize(pixelCount);
    }

    for (std::size_t pixelIndex{0}; pixelIndex < pixelCount; ++pixelIndex)
    {
        if (hitCount.at(pixelIndex) == 0)
        {
            continue;
        }
        ReliefColumnInfo& column{result.columns.at(pixelIndex)};
        column.has_model = true;
        column.z_min_mm = zMin.at(pixelIndex);
        column.z_max_mm = zMax.at(pixelIndex);
        column.hit_count = hitCount.at(pixelIndex);
        column.multi_hit = hitCount.at(pixelIndex) > 1;

        if (!useSupersampleCandidate)
        {
            AccumulateReliefColumnReport(reliefReport, column);
        }

        const double startZ = config.relief.fill_mode == "surface_to_base"
            ? config.relief.base_z_mm
            : zMin.at(pixelIndex);
        const double endZ{zMax.at(pixelIndex)};
        if (useLayerSlabCandidate)
        {
            occupancyColumns.at(pixelIndex) = {true, startZ, endZ};
            continue;
        }
        int startLayer{masks::first_layer_at_or_above_z(startZ, config.output.layer_thickness_mm)};
        int endLayer{masks::last_layer_at_or_below_z(endZ, config.output.layer_thickness_mm)};
        startLayer = std::max(0, startLayer);
        endLayer = std::min(grid.layer_count - 1, endLayer);
        if (startLayer > endLayer)
        {
            continue;
        }
        column.lower_layer = startLayer;
        column.upper_layer = endLayer;
        if (!materializeModelMaskStack)
        {
            // MF-03X2a：列区间已写入 column，整栈由主循环按层重建。
            continue;
        }
        for (int layerIndex{startLayer}; layerIndex <= endLayer; ++layerIndex)
        {
            result.model_masks.at(layerIndex).at(pixelIndex) = 1;
        }
    }

    if (useLayerSlabCandidate)
    {
        LayerOccupancyRequest occupancyRequest;
        occupancyRequest.columns = occupancyColumns;
        occupancyRequest.coverageSubsampleColumns = coverageSubsampleColumns;
        occupancyRequest.layerCount = grid.layer_count;
        occupancyRequest.layerThicknessMm = config.output.layer_thickness_mm;
        occupancyRequest.inputKind = GeometryOccupancyInputKind::SingleIntervalHeightfield;
        occupancyRequest.policy = occupancyPolicy;
        LayerOccupancyResult occupancyResult{BuildLayerOccupancy(occupancyRequest)};
        result.model_masks = std::move(occupancyResult.masks);

        for (std::size_t pixelIndex{0}; pixelIndex < pixelCount; ++pixelIndex)
        {
            ReliefColumnInfo& column{result.columns.at(pixelIndex)};
            if (!column.has_model)
            {
                continue;
            }
            column.lower_layer = occupancyResult.firstOccupiedLayers.at(pixelIndex);
            column.upper_layer = occupancyResult.lastOccupiedLayers.at(pixelIndex);
            if (useSupersampleCandidate && column.lower_layer < 0)
            {
                column.has_model = false;
                continue;
            }
            if (useSupersampleCandidate)
            {
                AccumulateReliefColumnReport(reliefReport, column);
            }
        }
    }
    reliefReport.empty_columns = reliefReport.total_columns - reliefReport.hit_columns;
    return result;
}

std::vector<int> compute_first_model_layers(const std::vector<std::vector<std::uint8_t>>& model_masks, const GridSpec& grid) {
    std::vector<int> first_model_layer(static_cast<std::size_t>(grid.width_px) * grid.height_px, -1);
    for (int layer_index{0}; layer_index < static_cast<int>(model_masks.size()); ++layer_index) {
        const auto& mask = model_masks.at(layer_index);
        for (std::size_t i{0}; i < mask.size(); ++i) {
            if (mask.at(i) != 0 && first_model_layer.at(i) < 0) {
                first_model_layer.at(i) = layer_index;
            }
        }
    }
    return first_model_layer;
}

std::vector<int> compute_relief_lower_layers(const std::vector<ReliefColumnInfo>& columns) {
    std::vector<int> lower_layers(columns.size(), -1);
    for (std::size_t i{0}; i < columns.size(); ++i) {
        if (columns.at(i).has_model && columns.at(i).lower_layer >= 0) {
            lower_layers.at(i) = columns.at(i).lower_layer;
        }
    }
    return lower_layers;
}

std::vector<ColumnLayerRange> compute_relief_column_ranges(const std::vector<ReliefColumnInfo>& columns) {
    std::vector<ColumnLayerRange> ranges(columns.size());
    for (std::size_t i{0}; i < columns.size(); ++i) {
        const ReliefColumnInfo& column = columns.at(i);
        if (column.has_model && column.lower_layer >= 0 && column.upper_layer >= column.lower_layer) {
            ranges.at(i) = {true, column.lower_layer, column.upper_layer};
        }
    }
    return ranges;
}

std::vector<ColumnLayerRange> compute_mask_column_ranges(
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const GridSpec& grid) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<ColumnLayerRange> ranges(pixel_count);
    for (int layer_index{0}; layer_index < static_cast<int>(model_masks.size()); ++layer_index) {
        const auto& mask = model_masks.at(layer_index);
        for (std::size_t i{0}; i < pixel_count; ++i) {
            if (mask.at(i) == 0) {
                continue;
            }
            ColumnLayerRange& range = ranges.at(i);
            if (!range.hasModel) {
                range.hasModel = true;
                range.lowerLayer = layer_index;
            }
            range.upperLayer = layer_index;
        }
    }
    return ranges;
}

}  // namespace slicer_core::relief
