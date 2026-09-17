// 层掩膜栅格化的实现（F-09 第 3 步从 slicer.cpp 搬出）。
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。
// Segment2 与 RasterResult 实测只被本簇使用，随簇进匿名命名空间。

#include "slicer_core/geometry/SliceMaskRasterizer.h"

#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/materials/varnish_geometry/SurfaceVarnishMasks.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace slicer_core::masks {

namespace {

struct Segment2 {
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};
};

struct RasterResult {
    std::vector<std::uint8_t> mask;
    int odd_intersection_rows{0};
    int filled_spans{0};
};

std::vector<std::uint8_t> DilateMaskPhysical(
    const GridSpec& grid,
    const std::vector<std::uint8_t>& sourceMask,
    const OuterVarnishDiscretization& discretization)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> dilated(pixelCount, 0);
    if (!discretization.enabled)
    {
        return sourceMask;
    }

    for (int y{0}; y < grid.height_px; ++y)
    {
        for (int x{0}; x < grid.width_px; ++x)
        {
            const std::size_t index = mask_index(grid, x, y);
            if (sourceMask.at(index) == 0)
            {
                continue;
            }
            const int minX = std::max(0, x - discretization.radius_x_px);
            const int maxX = std::min(grid.width_px - 1, x + discretization.radius_x_px);
            const int minY = std::max(0, y - discretization.radius_y_px);
            const int maxY = std::min(grid.height_px - 1, y + discretization.radius_y_px);
            for (int ny{minY}; ny <= maxY; ++ny)
            {
                for (int nx{minX}; nx <= maxX; ++nx)
                {
                    if (!IsOuterVarnishOffsetWithinThickness(
                            discretization,
                            nx - x,
                            ny - y))
                    {
                        continue;
                    }
                    dilated.at(mask_index(grid, nx, ny)) = 1;
                }
            }
        }
    }

    return dilated;
}

bool edge_intersects_plane(const Vec3& a, const Vec3& b, const double z_mm) {
    return (a.z <= z_mm && b.z > z_mm) || (b.z <= z_mm && a.z > z_mm);
}

Vec3 interpolate_at_z(const Vec3& a, const Vec3& b, const double z_mm) {
    const double t{(z_mm - a.z) / (b.z - a.z)};
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, z_mm};
}

std::vector<Segment2> slice_triangles_to_segments(const std::vector<Triangle>& triangles, const double z_mm) {
    std::vector<Segment2> segments;
    segments.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        std::vector<Vec3> points;
        points.reserve(2);
        if (edge_intersects_plane(triangle.a, triangle.b, z_mm)) {
            points.push_back(interpolate_at_z(triangle.a, triangle.b, z_mm));
        }
        if (edge_intersects_plane(triangle.b, triangle.c, z_mm)) {
            points.push_back(interpolate_at_z(triangle.b, triangle.c, z_mm));
        }
        if (edge_intersects_plane(triangle.c, triangle.a, z_mm)) {
            points.push_back(interpolate_at_z(triangle.c, triangle.a, z_mm));
        }
        if (points.size() == 2) {
            const double dx{points.at(0).x - points.at(1).x};
            const double dy{points.at(0).y - points.at(1).y};
            if ((dx * dx + dy * dy) > 1.0e-18) {
                segments.push_back({points.at(0).x, points.at(0).y, points.at(1).x, points.at(1).y});
            }
        }
    }
    return segments;
}

void fill_span(std::vector<std::uint8_t>& mask, const GridSpec& grid, const double x0, const double x1, const int y) {
    const double left{std::min(x0, x1)};
    const double right{std::max(x0, x1)};
    int start_x{static_cast<int>(std::ceil((left - grid.origin_x_mm) / grid.pixel_size_x_mm - 0.5))};
    int end_x{static_cast<int>(std::floor((right - grid.origin_x_mm) / grid.pixel_size_x_mm - 0.5))};
    start_x = std::max(0, start_x);
    end_x = std::min(grid.width_px - 1, end_x);
    for (int x{start_x}; x <= end_x; ++x) {
        mask.at(mask_index(grid, x, y)) = 1;
    }
}

RasterResult rasterize_segments(const GridSpec& grid, const std::vector<Segment2>& segments) {
    RasterResult result;
    result.mask.resize(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
    std::vector<double> intersections;
    intersections.reserve(segments.size());

    for (int y{0}; y < grid.height_px; ++y) {
        const double y_mm{grid.origin_y_mm + (static_cast<double>(y) + 0.5) * grid.pixel_size_y_mm};
        intersections.clear();
        for (const Segment2& segment : segments) {
            const double min_y{std::min(segment.y0, segment.y1)};
            const double max_y{std::max(segment.y0, segment.y1)};
            if (y_mm < min_y || y_mm >= max_y || std::abs(segment.y1 - segment.y0) < 1.0e-12) {
                continue;
            }
            const double t{(y_mm - segment.y0) / (segment.y1 - segment.y0)};
            intersections.push_back(segment.x0 + (segment.x1 - segment.x0) * t);
        }
        std::sort(intersections.begin(), intersections.end());
        if ((intersections.size() % 2U) != 0U) {
            ++result.odd_intersection_rows;
        }
        for (std::size_t i{0}; i + 1U < intersections.size(); i += 2U) {
            fill_span(result.mask, grid, intersections.at(i), intersections.at(i + 1U), y);
            ++result.filled_spans;
        }
    }
    return result;
}

}  // namespace

std::vector<std::vector<std::uint8_t>> BuildOuterVarnishMasks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks)
{
    const OuterVarnishDiscretization discretization =
        ComputeOuterVarnishDiscretization(
            config.outer_varnish,
            grid.pixel_size_x_mm,
            grid.pixel_size_y_mm);
    if (!discretization.enabled)
    {
        return {};
    }

    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::vector<std::uint8_t>> outerVarnishMasks(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));

    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        const std::vector<std::uint8_t>& modelMask = modelMasks.at(layerIndex);
        const bool hasModel = std::any_of(modelMask.begin(), modelMask.end(), [](const std::uint8_t value)
        {
            return value != 0;
        });
        if (!hasModel)
        {
            continue;
        }

        const std::vector<std::uint8_t> externalEmpty = BuildExternalEmptyMask(grid, modelMask);
        const std::vector<std::uint8_t> dilatedModel =
            DilateMaskPhysical(grid, modelMask, discretization);
        std::vector<std::uint8_t>& varnishMask = outerVarnishMasks.at(layerIndex);
        for (std::size_t index{0}; index < pixelCount; ++index)
        {
            if (modelMask.at(index) == 0 && externalEmpty.at(index) != 0 && dilatedModel.at(index) != 0)
            {
                varnishMask.at(index) = 1;
            }
        }
    }

    return outerVarnishMasks;
}

UpperSupportBoundaryInfo ResolveUpperSupportBoundaryInfo(const SliceConfig& config)
{
    UpperSupportBoundaryInfo info;
    if (config.support.upper.outside == "outer_varnish_shell"
        && config.outer_varnish.enabled
        && config.outer_varnish.thickness_mm > 0.0)
    {
        info.includes_outer_varnish_shell = true;
        info.source = "model_envelope_plus_outer_varnish_shell";
    }
    return info;
}

std::vector<std::vector<std::uint8_t>> BuildUpperSupportBoundaryMasks(
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    const UpperSupportBoundaryInfo& boundaryInfo)
{
    if (!boundaryInfo.includes_outer_varnish_shell)
    {
        return {};
    }

    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::vector<std::uint8_t>> boundaryMasks(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));
    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        for (std::size_t index{0}; index < pixelCount; ++index)
        {
            if (modelMasks.at(layerIndex).at(index) != 0 || outerVarnishMasks.at(layerIndex).at(index) != 0)
            {
                boundaryMasks.at(layerIndex).at(index) = 1;
            }
        }
    }
    return boundaryMasks;
}

std::vector<std::vector<std::uint8_t>> sample_model_masks(
    const ModelReport& model_report,
    const GridSpec& grid,
    const double layer_thickness_mm,
    std::vector<LayerDiagnostics>& diagnostics) {
    std::vector<std::vector<std::uint8_t>> masks;
    masks.reserve(grid.layer_count);
    diagnostics.clear();
    diagnostics.reserve(grid.layer_count);
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        const double z_mm{(static_cast<double>(layer_index) + 0.5) * layer_thickness_mm};
        LayerDiagnostics layer_diagnostics;
        layer_diagnostics.layer_index = layer_index;
        layer_diagnostics.z_mm = z_mm;
        if (z_mm < model_report.bbox_mm.min.z || z_mm > model_report.bbox_mm.max.z) {
            masks.emplace_back(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
            diagnostics.push_back(layer_diagnostics);
            continue;
        }
        const std::vector<Segment2> segments = slice_triangles_to_segments(model_report.triangles, z_mm);
        RasterResult raster = rasterize_segments(grid, segments);
        layer_diagnostics.segment_count = static_cast<int>(segments.size());
        layer_diagnostics.odd_intersection_rows = raster.odd_intersection_rows;
        layer_diagnostics.filled_spans = raster.filled_spans;
        masks.push_back(std::move(raster.mask));
        diagnostics.push_back(layer_diagnostics);
    }
    return masks;
}

bool point_in_triangle_xy(
    const Vec3& p,
    const Triangle& triangle,
    double& w0,
    double& w1,
    double& w2) {
    const double denominator =
        (triangle.b.y - triangle.c.y) * (triangle.a.x - triangle.c.x)
        + (triangle.c.x - triangle.b.x) * (triangle.a.y - triangle.c.y);
    if (std::abs(denominator) < 1.0e-12) {
        return false;
    }
    w0 = ((triangle.b.y - triangle.c.y) * (p.x - triangle.c.x)
          + (triangle.c.x - triangle.b.x) * (p.y - triangle.c.y))
        / denominator;
    w1 = ((triangle.c.y - triangle.a.y) * (p.x - triangle.c.x)
          + (triangle.a.x - triangle.c.x) * (p.y - triangle.c.y))
        / denominator;
    w2 = 1.0 - w0 - w1;
    constexpr double epsilon{-1.0e-9};
    return w0 >= epsilon && w1 >= epsilon && w2 >= epsilon;
}

int first_layer_at_or_above_z(const double z_mm, const double layer_thickness_mm) {
    return static_cast<int>(std::ceil(z_mm / layer_thickness_mm - 0.5));
}

int last_layer_at_or_below_z(const double z_mm, const double layer_thickness_mm) {
    return static_cast<int>(std::floor(z_mm / layer_thickness_mm - 0.5));
}

}  // namespace slicer_core::masks
