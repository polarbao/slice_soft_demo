#include "slicer_core/api/viewdata/SceneSurfacePreviewBuilder.h"

#include "slicer_core/api/viewdata/SceneViewIdentity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace slicer_core::api::viewdata_detail
{
namespace
{

constexpr double kRasterEpsilon{1.0e-9};

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), detail});
}

double Edge(
    const double ax,
    const double ay,
    const double bx,
    const double by,
    const double px,
    const double py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

std::array<std::uint8_t, 4> SampleTexture(
    const ViewTexture& texture,
    const double sourceU,
    const double sourceV)
{
    const double u = std::clamp(sourceU, 0.0, 1.0);
    const double v = std::clamp(1.0 - sourceV, 0.0, 1.0);
    const int x = std::clamp(
        static_cast<int>(std::lround(u * (texture.width_px - 1))),
        0,
        texture.width_px - 1);
    const int y = std::clamp(
        static_cast<int>(std::lround(v * (texture.height_px - 1))),
        0,
        texture.height_px - 1);
    const std::size_t offset =
        (static_cast<std::size_t>(y)
            * static_cast<std::size_t>(texture.width_px)
         + static_cast<std::size_t>(x)) * 4U;
    return {
        texture.rgba8.at(offset + 0U),
        texture.rgba8.at(offset + 1U),
        texture.rgba8.at(offset + 2U),
        texture.rgba8.at(offset + 3U)};
}

std::array<std::uint8_t, 4> BaseColor(const ViewMaterial& material)
{
    std::array<std::uint8_t, 4> result{};
    for (std::size_t channel{0U}; channel < result.size(); ++channel)
    {
        result.at(channel) = static_cast<std::uint8_t>(std::lround(
            std::clamp(material.base_color.at(channel), 0.0F, 1.0F)
            * 255.0F));
    }
    return result;
}

Bounds3d LocalBounds(const SceneModel& model)
{
    Bounds3d bounds;
    bounds.min_mm = {
        model.bbox_mm.min.x,
        model.bbox_mm.min.y,
        model.bbox_mm.min.z};
    bounds.max_mm = {
        model.bbox_mm.max.x,
        model.bbox_mm.max.y,
        model.bbox_mm.max.z};
    return bounds;
}

}  // namespace

ApiResult<SurfacePreview> BuildSurfacePreview(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const int maximumDimension,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        if (maximumDimension < 16 || maximumDimension > 768)
        {
            return Failure<SurfacePreview>(
                "PM-SLICER-PROFILE-0031",
                "ViewData preview dimension is outside the supported range",
                std::to_string(maximumDimension));
        }
        const double widthMm = model.bbox_mm.max.x - model.bbox_mm.min.x;
        const double heightMm = model.bbox_mm.max.y - model.bbox_mm.min.y;
        if (!std::isfinite(widthMm) || !std::isfinite(heightMm)
            || widthMm <= 0.0 || heightMm <= 0.0)
        {
            return Failure<SurfacePreview>(
                "PM-SLICER-INPUT-0002",
                "ViewData top preview requires finite positive XY bounds",
                model.model_path.generic_string());
        }

        SurfacePreview preview;
        if (widthMm >= heightMm)
        {
            preview.width_px = maximumDimension;
            preview.height_px = std::max(
                2,
                static_cast<int>(std::lround(
                    maximumDimension * heightMm / widthMm)));
        }
        else
        {
            preview.height_px = maximumDimension;
            preview.width_px = std::max(
                2,
                static_cast<int>(std::lround(
                    maximumDimension * widthMm / heightMm)));
        }
        const std::size_t pixelCount =
            static_cast<std::size_t>(preview.width_px)
            * static_cast<std::size_t>(preview.height_px);
        preview.rgba8.assign(pixelCount * 4U, 0U);
        std::vector<double> depth(
            pixelCount,
            std::numeric_limits<double>::lowest());
        bool painted{false};

        for (std::size_t triangleIndex{0U};
             triangleIndex < model.triangles.size();
             ++triangleIndex)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Failure<SurfacePreview>(
                    "PM-SLICER-CANCELLED-0070",
                    "ViewData top preview generation was cancelled",
                    model.model_path.generic_string());
            }
            const Triangle& triangle = model.triangles.at(triangleIndex);
            const std::array<double, 3> x{
                (triangle.a.x - model.bbox_mm.min.x) / widthMm
                    * (preview.width_px - 1),
                (triangle.b.x - model.bbox_mm.min.x) / widthMm
                    * (preview.width_px - 1),
                (triangle.c.x - model.bbox_mm.min.x) / widthMm
                    * (preview.width_px - 1)};
            const std::array<double, 3> y{
                (model.bbox_mm.max.y - triangle.a.y) / heightMm
                    * (preview.height_px - 1),
                (model.bbox_mm.max.y - triangle.b.y) / heightMm
                    * (preview.height_px - 1),
                (model.bbox_mm.max.y - triangle.c.y) / heightMm
                    * (preview.height_px - 1)};
            const double area = Edge(
                x.at(0U), y.at(0U),
                x.at(1U), y.at(1U),
                x.at(2U), y.at(2U));
            if (std::abs(area) <= kRasterEpsilon)
            {
                continue;
            }

            const auto materialResult = ResolveTriangleMaterialIndex(
                model,
                appearance,
                triangleIndex);
            if (!materialResult.IsOk())
            {
                return Failure<SurfacePreview>(
                    materialResult.Error()->code,
                    materialResult.Error()->message,
                    materialResult.Error()->detail);
            }
            const ResolvedViewMaterial& resolvedMaterial =
                appearance.materials.at(*materialResult.Value());
            const TriangleTextureInfo* binding =
                model.triangle_textures.empty()
                ? nullptr
                : &model.triangle_textures.at(triangleIndex);

            const int minX = std::clamp(
                static_cast<int>(std::floor(*std::min_element(
                    x.begin(), x.end()))),
                0,
                preview.width_px - 1);
            const int maxX = std::clamp(
                static_cast<int>(std::ceil(*std::max_element(
                    x.begin(), x.end()))),
                0,
                preview.width_px - 1);
            const int minY = std::clamp(
                static_cast<int>(std::floor(*std::min_element(
                    y.begin(), y.end()))),
                0,
                preview.height_px - 1);
            const int maxY = std::clamp(
                static_cast<int>(std::ceil(*std::max_element(
                    y.begin(), y.end()))),
                0,
                preview.height_px - 1);

            for (int py = minY; py <= maxY; ++py)
            {
                for (int px = minX; px <= maxX; ++px)
                {
                    const double sampleX = static_cast<double>(px) + 0.5;
                    const double sampleY = static_cast<double>(py) + 0.5;
                    const double w0 = Edge(
                        x.at(1U), y.at(1U),
                        x.at(2U), y.at(2U),
                        sampleX, sampleY) / area;
                    const double w1 = Edge(
                        x.at(2U), y.at(2U),
                        x.at(0U), y.at(0U),
                        sampleX, sampleY) / area;
                    const double w2 = 1.0 - w0 - w1;
                    if (w0 < -kRasterEpsilon
                        || w1 < -kRasterEpsilon
                        || w2 < -kRasterEpsilon)
                    {
                        continue;
                    }
                    const double z = triangle.a.z * w0
                        + triangle.b.z * w1
                        + triangle.c.z * w2;
                    const std::size_t pixel =
                        static_cast<std::size_t>(py)
                            * static_cast<std::size_t>(preview.width_px)
                        + static_cast<std::size_t>(px);
                    if (z <= depth.at(pixel))
                    {
                        continue;
                    }

                    std::array<std::uint8_t, 4> color = BaseColor(
                        resolvedMaterial.material);
                    if (resolvedMaterial.has_texture)
                    {
                        const double u = binding->uv.at(0U).u * w0
                            + binding->uv.at(1U).u * w1
                            + binding->uv.at(2U).u * w2;
                        const double v = binding->uv.at(0U).v * w0
                            + binding->uv.at(1U).v * w1
                            + binding->uv.at(2U).v * w2;
                        color = SampleTexture(
                            appearance.appearance.textures.at(
                                resolvedMaterial.texture_index),
                            u,
                            v);
                    }
                    if (color.at(3U) == 0U)
                    {
                        continue;
                    }
                    const std::size_t offset = pixel * 4U;
                    std::copy(
                        color.begin(), color.end(),
                        preview.rgba8.begin()
                            + static_cast<std::ptrdiff_t>(offset));
                    depth.at(pixel) = z;
                    painted = true;
                }
            }
        }

        if (!painted)
        {
            return Failure<SurfacePreview>(
                "PM-SLICER-INPUT-0002",
                "ViewData top preview contains no visible +Z surface",
                model.model_path.generic_string());
        }
        preview.local_bounds_mm = LocalBounds(model);
        preview.appearance_identity =
            appearance.appearance.appearance_identity;
        preview.preview_identity = ComputePreviewIdentity(preview);
        return ApiResult<SurfacePreview>::Success(std::move(preview));
    }
    catch (const std::exception& error)
    {
        return Failure<SurfacePreview>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build ViewData top preview",
            error.what());
    }
    catch (...)
    {
        return Failure<SurfacePreview>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build ViewData top preview",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::viewdata_detail
