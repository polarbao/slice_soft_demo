#include "slicer_core/geometry/TransformedModelAdapter.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

    const double translatedZ = transform.translatezmm == 0.0
        ? pivot.z + transformed.z
        : pivot.z + transformed.z + transform.translatezmm;
    return {
        pivot.x + transformed.x + transform.translatexmm,
        pivot.y + transformed.y + transform.translateymm,
        translatedZ,
    };
}

struct VertexKey
{
    std::uint64_t x{0U};
    std::uint64_t y{0U};
    std::uint64_t z{0U};

    bool operator==(const VertexKey&) const = default;
};

struct VertexKeyHash
{
    std::size_t operator()(const VertexKey& key) const noexcept
    {
        std::size_t seed = static_cast<std::size_t>(key.x);
        seed ^= static_cast<std::size_t>(key.y)
            + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= static_cast<std::size_t>(key.z)
            + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

VertexKey MakeVertexKey(const Vec3& point)
{
    const auto bits = [](const double value)
    {
        return std::bit_cast<std::uint64_t>(value == 0.0 ? 0.0 : value);
    };
    return {bits(point.x), bits(point.y), bits(point.z)};
}

class DisjointSet final
{
public:
    explicit DisjointSet(const std::size_t size)
        : m_parent(size), m_rank(size, 0U)
    {
        for (std::size_t index = 0U; index < size; ++index)
        {
            m_parent[index] = index;
        }
    }

    std::size_t Find(const std::size_t index)
    {
        if (m_parent[index] != index)
        {
            m_parent[index] = Find(m_parent[index]);
        }
        return m_parent[index];
    }

    void Unite(const std::size_t left, const std::size_t right)
    {
        std::size_t leftRoot = Find(left);
        std::size_t rightRoot = Find(right);
        if (leftRoot == rightRoot)
        {
            return;
        }
        if (m_rank[leftRoot] < m_rank[rightRoot])
        {
            std::swap(leftRoot, rightRoot);
        }
        m_parent[rightRoot] = leftRoot;
        if (m_rank[leftRoot] == m_rank[rightRoot])
        {
            ++m_rank[leftRoot];
        }
    }

private:
    std::vector<std::size_t> m_parent;
    std::vector<unsigned char> m_rank;
};

double TriangleArea(const Triangle& triangle)
{
    const Vec3 first{
        triangle.b.x - triangle.a.x,
        triangle.b.y - triangle.a.y,
        triangle.b.z - triangle.a.z};
    const Vec3 second{
        triangle.c.x - triangle.a.x,
        triangle.c.y - triangle.a.y,
        triangle.c.z - triangle.a.z};
    const Vec3 cross{
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x};
    return 0.5 * std::sqrt(
        cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
}

struct LandingReference
{
    double minimumzmm{0.0};
    std::size_t ignoredcomponentcount{0U};
};

LandingReference ResolveLandingReference(
    const std::vector<Triangle>& triangles,
    const double fallbackMinimumZ)
{
    if (triangles.empty())
    {
        return {fallbackMinimumZ, 0U};
    }

    DisjointSet components(triangles.size());
    std::unordered_map<VertexKey, std::size_t, VertexKeyHash> owners;
    owners.reserve(triangles.size() * 2U);
    for (std::size_t index = 0U; index < triangles.size(); ++index)
    {
        const Triangle& triangle = triangles[index];
        for (const Vec3& point : {triangle.a, triangle.b, triangle.c})
        {
            const auto [owner, inserted] = owners.emplace(
                MakeVertexKey(point), index);
            if (!inserted)
            {
                components.Unite(index, owner->second);
            }
        }
    }

    struct ComponentStats
    {
        std::size_t trianglecount{0U};
        double area{0.0};
        double minimumzmm{std::numeric_limits<double>::max()};
    };
    std::unordered_map<std::size_t, ComponentStats> statistics;
    statistics.reserve(triangles.size());
    for (std::size_t index = 0U; index < triangles.size(); ++index)
    {
        const Triangle& triangle = triangles[index];
        ComponentStats& stats = statistics[components.Find(index)];
        ++stats.trianglecount;
        stats.area += TriangleArea(triangle);
        stats.minimumzmm = std::min(
            stats.minimumzmm,
            std::min({triangle.a.z, triangle.b.z, triangle.c.z}));
    }

    std::size_t maximumTriangleCount{0U};
    double maximumArea{0.0};
    for (const auto& [unusedRoot, stats] : statistics)
    {
        static_cast<void>(unusedRoot);
        maximumTriangleCount = std::max(
            maximumTriangleCount, stats.trianglecount);
        maximumArea = std::max(maximumArea, stats.area);
    }

    constexpr double kTriangleShare{0.01};
    constexpr double kAreaShare{0.001};
    double referenceMinimumZ = std::numeric_limits<double>::max();
    std::size_t ignoredCount{0U};
    for (const auto& [unusedRoot, stats] : statistics)
    {
        static_cast<void>(unusedRoot);
        const bool significant =
            static_cast<double>(stats.trianglecount)
                    >= static_cast<double>(maximumTriangleCount)
                        * kTriangleShare
            || stats.area >= maximumArea * kAreaShare;
        if (significant)
        {
            referenceMinimumZ = std::min(
                referenceMinimumZ, stats.minimumzmm);
        }
        else
        {
            ++ignoredCount;
        }
    }
    if (!std::isfinite(referenceMinimumZ))
    {
        return {fallbackMinimumZ, 0U};
    }
    return {referenceMinimumZ, ignoredCount};
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

    if (transform.landonbuildplate)
    {
        const LandingReference reference = ResolveLandingReference(
            geometry.triangles, geometry.bboxmm.min.z);
        geometry.landingoffsetzmm = -reference.minimumzmm;
        geometry.landingignoredcomponentcount =
            reference.ignoredcomponentcount;
        if (geometry.landingoffsetzmm != 0.0)
        {
            for (Triangle& triangle : geometry.triangles)
            {
                TranslateTriangleZ(triangle, geometry.landingoffsetzmm);
            }
            geometry.bboxmm.min.z += geometry.landingoffsetzmm;
            geometry.bboxmm.max.z += geometry.landingoffsetzmm;
        }
        geometry.landingreferencezmm =
            reference.minimumzmm + geometry.landingoffsetzmm;
    }

    return {std::move(geometry), std::nullopt};
}

}  // namespace slicer_core
