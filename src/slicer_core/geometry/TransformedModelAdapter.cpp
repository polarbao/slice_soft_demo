#include "slicer_core/geometry/TransformedModelAdapter.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>
#include <utility>

namespace slicer_core
{
namespace
{

ModelTransformError MakeAdapterError(
    const ModelTransformErrorCode code,
    const ModelInstance& instance,
    const std::string_view field,
    const std::string_view message)
{
    ModelTransformError error;
    error.code = code;
    error.instanceid = instance.instanceid;
    error.modelid = instance.modelid;
    error.field = field;
    error.message = message;
    return error;
}

bool IsFinite(const Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

Vec3 TransformPoint(
    const Vec3& point,
    const Vec3& pivot,
    const ModelTransform& transform)
{
    const double mirrorX = transform.mirrorx ? -1.0 : 1.0;
    const double mirrorY = transform.mirrory ? -1.0 : 1.0;
    const double radiansX =
        transform.rotatexdeg * std::numbers::pi_v<double> / 180.0;
    const double radiansY =
        transform.rotateydeg * std::numbers::pi_v<double> / 180.0;
    const double radiansZ =
        transform.rotatezdeg * std::numbers::pi_v<double> / 180.0;

    Vec3 transformed{
        (point.x - pivot.x) * transform.uniformscale * mirrorX,
        (point.y - pivot.y) * transform.uniformscale * mirrorY,
        (point.z - pivot.z) * transform.uniformscale,
    };
    const double cosineX = std::cos(radiansX);
    const double sineX = std::sin(radiansX);
    transformed = {
        transformed.x,
        transformed.y * cosineX - transformed.z * sineX,
        transformed.y * sineX + transformed.z * cosineX,
    };
    const double cosineY = std::cos(radiansY);
    const double sineY = std::sin(radiansY);
    transformed = {
        transformed.x * cosineY + transformed.z * sineY,
        transformed.y,
        -transformed.x * sineY + transformed.z * cosineY,
    };
    const double cosineZ = std::cos(radiansZ);
    const double sineZ = std::sin(radiansZ);
    transformed = {
        transformed.x * cosineZ - transformed.y * sineZ,
        transformed.x * sineZ + transformed.y * cosineZ,
        transformed.z,
    };

    return {
        pivot.x + transformed.x + transform.translatexmm,
        pivot.y + transformed.y + transform.translateymm,
        pivot.z + transformed.z,
    };
}

void TranslateTriangleZ(Triangle& triangle, const double offset)
{
    triangle.a.z += offset;
    triangle.b.z += offset;
    triangle.c.z += offset;
}

void IncludePoint(BoundingBox& bbox, const Vec3& point)
{
    bbox.min.x = std::min(bbox.min.x, point.x);
    bbox.min.y = std::min(bbox.min.y, point.y);
    bbox.min.z = std::min(bbox.min.z, point.z);
    bbox.max.x = std::max(bbox.max.x, point.x);
    bbox.max.y = std::max(bbox.max.y, point.y);
    bbox.max.z = std::max(bbox.max.z, point.z);
}

}  // namespace

bool TransformedModelResult::IsValid() const
{
    return !error.has_value();
}

TransformedModelResult AdaptTransformedModel(
    const SceneModel& source,
    const ModelInstance& instance)
{
    if (const std::optional<ModelTransformError> instanceError =
            ValidateModelInstance(instance);
        instanceError.has_value())
    {
        return {{}, instanceError};
    }
    if (source.triangles.empty())
    {
        return {
            {},
            MakeAdapterError(
                ModelTransformErrorCode::SourceMissing,
                instance,
                "source.triangles",
                "source model geometry must not be empty")};
    }
    if (!IsFinite(source.bbox_mm.min) || !IsFinite(source.bbox_mm.max))
    {
        return {
            {},
            MakeAdapterError(
                ModelTransformErrorCode::NonFinite,
                instance,
                "source.bboxmm",
                "source model bounds must be finite")};
    }

    const ModelTransform transform =
        NormalizeModelTransform(instance.transform);
    TransformedModelGeometry geometry;
    geometry.pivotmm = {
        (source.bbox_mm.min.x + source.bbox_mm.max.x) * 0.5,
        (source.bbox_mm.min.y + source.bbox_mm.max.y) * 0.5,
        source.bbox_mm.min.z,
    };
    geometry.determinantsign =
        transform.mirrorx != transform.mirrory ? -1 : 1;
    geometry.mirrored = geometry.determinantsign < 0;
    geometry.transformrevision = instance.transformrevision;
    geometry.triangles.reserve(source.triangles.size());
    geometry.triangletextures = source.triangle_textures;

    bool hasBounds{false};
    for (std::size_t index{0U}; index < source.triangles.size(); ++index)
    {
        const Triangle& sourceTriangle = source.triangles.at(index);
        if (!IsFinite(sourceTriangle.a)
            || !IsFinite(sourceTriangle.b)
            || !IsFinite(sourceTriangle.c))
        {
            return {
                {},
                MakeAdapterError(
                    ModelTransformErrorCode::NonFinite,
                    instance,
                    "source.triangles",
                    "source model triangles must be finite")};
        }

        Triangle transformedTriangle{
            TransformPoint(sourceTriangle.a, geometry.pivotmm, transform),
            TransformPoint(sourceTriangle.b, geometry.pivotmm, transform),
            TransformPoint(sourceTriangle.c, geometry.pivotmm, transform),
        };
        if (!IsFinite(transformedTriangle.a)
            || !IsFinite(transformedTriangle.b)
            || !IsFinite(transformedTriangle.c))
        {
            return {
                {},
                MakeAdapterError(
                    ModelTransformErrorCode::NonFinite,
                    instance,
                    "transformed.triangles",
                    "transformed model triangles must be finite")};
        }
        if (geometry.mirrored)
        {
            std::swap(transformedTriangle.b, transformedTriangle.c);
            if (index < geometry.triangletextures.size())
            {
                std::swap(
                    geometry.triangletextures.at(index).uv.at(1U),
                    geometry.triangletextures.at(index).uv.at(2U));
            }
        }

        if (!hasBounds)
        {
            geometry.bboxmm.min = transformedTriangle.a;
            geometry.bboxmm.max = transformedTriangle.a;
            hasBounds = true;
        }
        IncludePoint(geometry.bboxmm, transformedTriangle.a);
        IncludePoint(geometry.bboxmm, transformedTriangle.b);
        IncludePoint(geometry.bboxmm, transformedTriangle.c);
        geometry.triangles.push_back(transformedTriangle);
    }

    const bool tilted = transform.rotatexdeg != 0.0
        || transform.rotateydeg != 0.0;
    if (tilted || transform.landonbuildplate)
    {
        const double targetMinZ = transform.landonbuildplate
            ? 0.0
            : source.bbox_mm.min.z;
        geometry.landingoffsetzmm = targetMinZ - geometry.bboxmm.min.z;
        if (geometry.landingoffsetzmm != 0.0)
        {
            for (Triangle& triangle : geometry.triangles)
            {
                TranslateTriangleZ(triangle, geometry.landingoffsetzmm);
            }
            geometry.bboxmm.min.z += geometry.landingoffsetzmm;
            geometry.bboxmm.max.z += geometry.landingoffsetzmm;
        }
    }

    return {std::move(geometry), std::nullopt};
}

}  // namespace slicer_core
