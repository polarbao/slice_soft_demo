#include "slicer_core/api/viewdata/SceneViewMeshBuilder.h"

#include "slicer_core/api/viewdata/SceneViewIdentity.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace slicer_core::api::viewdata_detail
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), detail});
}

bool IsFinite(const Vec3& point)
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

Vec3 TransformPoint(const Vec3& point, const Matrix4d& matrix)
{
    return {
        matrix.values.at(0U) * point.x
            + matrix.values.at(1U) * point.y
            + matrix.values.at(2U) * point.z
            + matrix.values.at(3U),
        matrix.values.at(4U) * point.x
            + matrix.values.at(5U) * point.y
            + matrix.values.at(6U) * point.z
            + matrix.values.at(7U),
        matrix.values.at(8U) * point.x
            + matrix.values.at(9U) * point.y
            + matrix.values.at(10U) * point.z
            + matrix.values.at(11U),
    };
}

std::optional<Vec3> ComputeNormal(const Triangle& triangle)
{
    const Vec3 ab{
        triangle.b.x - triangle.a.x,
        triangle.b.y - triangle.a.y,
        triangle.b.z - triangle.a.z};
    const Vec3 ac{
        triangle.c.x - triangle.a.x,
        triangle.c.y - triangle.a.y,
        triangle.c.z - triangle.a.z};
    Vec3 normal{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x};
    const double length = std::sqrt(
        normal.x * normal.x
        + normal.y * normal.y
        + normal.z * normal.z);
    if (!std::isfinite(length) || length <= 1.0e-12)
    {
        return std::nullopt;
    }
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    return normal;
}

std::size_t TriangleLimit(const ViewLod lod)
{
    switch (lod)
    {
    case ViewLod::Lod0:
        return std::numeric_limits<std::size_t>::max();
    case ViewLod::Lod1:
        return 50000U;
    case ViewLod::Lod2:
        return 10000U;
    case ViewLod::Auto:
    case ViewLod::OutlineOnly:
        break;
    }
    return 0U;
}

std::vector<std::vector<std::size_t>> GroupTriangles(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance)
{
    std::vector<std::vector<std::size_t>> groups(
        appearance.materials.size());
    for (std::size_t index{0U}; index < model.triangles.size(); ++index)
    {
        const auto resolved = ResolveTriangleMaterialIndex(
            model,
            appearance,
            index);
        if (resolved.IsOk())
        {
            groups.at(*resolved.Value()).push_back(index);
        }
    }
    return groups;
}

struct GroupBudgetRemainder
{
    std::size_t group_index{0U};
    std::uint64_t remainder{0U};
};

std::vector<std::size_t> AllocateTriangleBudgets(
    const std::vector<std::vector<std::size_t>>& groups,
    const std::size_t triangleLimit)
{
    std::vector<std::size_t> budgets(groups.size(), 0U);
    std::size_t totalTriangles{0U};
    std::vector<std::size_t> activeGroups;
    for (std::size_t groupIndex{0U}; groupIndex < groups.size(); ++groupIndex)
    {
        const std::size_t groupSize = groups.at(groupIndex).size();
        totalTriangles += groupSize;
        if (groupSize > 0U)
        {
            activeGroups.push_back(groupIndex);
        }
    }
    if (totalTriangles <= triangleLimit)
    {
        for (std::size_t groupIndex{0U}; groupIndex < groups.size();
             ++groupIndex)
        {
            budgets.at(groupIndex) = groups.at(groupIndex).size();
        }
        return budgets;
    }

    const std::size_t targetTriangles = triangleLimit;
    if (targetTriangles < activeGroups.size())
    {
        std::stable_sort(
            activeGroups.begin(),
            activeGroups.end(),
            [&groups](const std::size_t left, const std::size_t right)
            {
                return groups.at(left).size() > groups.at(right).size();
            });
        for (std::size_t index{0U}; index < targetTriangles; ++index)
        {
            budgets.at(activeGroups.at(index)) = 1U;
        }
        return budgets;
    }

    for (const std::size_t groupIndex : activeGroups)
    {
        budgets.at(groupIndex) = 1U;
    }
    const std::size_t remainingTarget =
        targetTriangles - activeGroups.size();
    const std::size_t remainingCapacity =
        totalTriangles - activeGroups.size();
    if (remainingTarget == 0U || remainingCapacity == 0U)
    {
        return budgets;
    }

    std::size_t assigned{0U};
    std::vector<GroupBudgetRemainder> remainders;
    remainders.reserve(activeGroups.size());
    for (const std::size_t groupIndex : activeGroups)
    {
        const std::size_t capacity = groups.at(groupIndex).size() - 1U;
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(capacity)
            * static_cast<std::uint64_t>(remainingTarget);
        const std::size_t allocation = static_cast<std::size_t>(
            numerator / remainingCapacity);
        budgets.at(groupIndex) += allocation;
        assigned += allocation;
        remainders.push_back({
            groupIndex,
            numerator % remainingCapacity});
    }

    std::stable_sort(
        remainders.begin(),
        remainders.end(),
        [](const GroupBudgetRemainder& left,
           const GroupBudgetRemainder& right)
        {
            return left.remainder > right.remainder;
        });
    const std::size_t remainderCount = remainingTarget - assigned;
    for (std::size_t index{0U}; index < remainderCount; ++index)
    {
        ++budgets.at(remainders.at(index).group_index);
    }
    return budgets;
}

struct VertexKey
{
    std::array<std::uint32_t, 8> values{};

    [[nodiscard]] bool operator<(const VertexKey& other) const noexcept
    {
        return values < other.values;
    }
};

std::uint32_t FloatBits(const float value)
{
    return value == 0.0F
        ? 0U
        : std::bit_cast<std::uint32_t>(value);
}

VertexKey MakeVertexKey(
    const Vec3& point,
    const Vec3& normal,
    const TexCoord& uv)
{
    const std::array<float, 8> values{
        static_cast<float>(point.x),
        static_cast<float>(point.y),
        static_cast<float>(point.z),
        static_cast<float>(normal.x),
        static_cast<float>(normal.y),
        static_cast<float>(normal.z),
        static_cast<float>(uv.u),
        static_cast<float>(uv.v)};
    VertexKey key;
    std::transform(
        values.begin(),
        values.end(),
        key.values.begin(),
        FloatBits);
    return key;
}

std::uint32_t ResolveVertex(
    ViewMesh& mesh,
    std::map<VertexKey, std::uint32_t>& vertexIndices,
    const Vec3& point,
    const Vec3& normal,
    const TexCoord& uv)
{
    const VertexKey key = MakeVertexKey(point, normal, uv);
    const auto existing = vertexIndices.find(key);
    if (existing != vertexIndices.end())
    {
        return existing->second;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(
        mesh.positions.size() / 3U);
    mesh.positions.insert(
        mesh.positions.end(),
        {static_cast<float>(point.x),
         static_cast<float>(point.y),
         static_cast<float>(point.z)});
    mesh.normals.insert(
        mesh.normals.end(),
        {static_cast<float>(normal.x),
         static_cast<float>(normal.y),
         static_cast<float>(normal.z)});
    mesh.texcoord0.insert(
        mesh.texcoord0.end(),
        {static_cast<float>(uv.u),
         static_cast<float>(uv.v)});
    vertexIndices.emplace(key, index);
    return index;
}

}  // namespace

ApiResult<ViewMesh> BuildViewMesh(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const Matrix4d& worldMatrix,
    const ViewLod lod,
    const MeshTransform meshTransform,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        const std::size_t triangleLimit = TriangleLimit(lod);
        if (triangleLimit == 0U)
        {
            return Failure<ViewMesh>(
                "PM-SLICER-PROFILE-0031",
                "ViewData mesh requires lod0, lod1, or lod2",
                "lod");
        }

        const auto groups = GroupTriangles(model, appearance);
        const std::vector<std::size_t> groupBudgets =
            AllocateTriangleBudgets(groups, triangleLimit);

        ViewMesh mesh;
        mesh.lod = lod;
        mesh.mesh_transform = meshTransform;
        std::size_t estimatedTriangles{0U};
        for (const std::size_t groupBudget : groupBudgets)
        {
            estimatedTriangles += groupBudget;
        }
        mesh.positions.reserve(estimatedTriangles * 9U);
        mesh.normals.reserve(estimatedTriangles * 9U);
        mesh.texcoord0.reserve(estimatedTriangles * 6U);
        mesh.indices.reserve(estimatedTriangles * 3U);
        std::map<VertexKey, std::uint32_t> vertexIndices;

        for (std::size_t materialIndex{0U};
             materialIndex < groups.size();
             ++materialIndex)
        {
            const std::vector<std::size_t>& triangleIndices =
                groups.at(materialIndex);
            const std::size_t groupBudget = groupBudgets.at(materialIndex);
            if (triangleIndices.empty() || groupBudget == 0U)
            {
                continue;
            }
            ViewSubmesh submesh;
            submesh.first_index = static_cast<std::uint32_t>(
                mesh.indices.size());
            submesh.material_id =
                appearance.materials.at(materialIndex).material.material_id;

            for (std::size_t sampleIndex{0U};
                 sampleIndex < groupBudget;
                 ++sampleIndex)
            {
                if (cancelToken.IsCancelRequested())
                {
                    return Failure<ViewMesh>(
                        "PM-SLICER-CANCELLED-0070",
                        "ViewData mesh generation was cancelled",
                        model.model_path.generic_string());
                }
                const std::size_t groupIndex =
                    sampleIndex * triangleIndices.size() / groupBudget;
                const std::size_t triangleIndex = triangleIndices.at(
                    groupIndex);
                Triangle triangle = model.triangles.at(triangleIndex);
                if (meshTransform == MeshTransform::World)
                {
                    triangle.a = TransformPoint(triangle.a, worldMatrix);
                    triangle.b = TransformPoint(triangle.b, worldMatrix);
                    triangle.c = TransformPoint(triangle.c, worldMatrix);
                }
                if (!IsFinite(triangle.a)
                    || !IsFinite(triangle.b)
                    || !IsFinite(triangle.c))
                {
                    return Failure<ViewMesh>(
                        "PM-SLICER-INPUT-0002",
                        "ViewData mesh contains non-finite geometry",
                        std::to_string(triangleIndex));
                }
                const std::optional<Vec3> normal = ComputeNormal(triangle);
                if (!normal.has_value())
                {
                    return Failure<ViewMesh>(
                        "PM-SLICER-INPUT-0002",
                        "ViewData mesh contains a degenerate triangle",
                        std::to_string(triangleIndex));
                }

                const std::array<Vec3, 3> points{
                    triangle.a, triangle.b, triangle.c};
                const TriangleTextureInfo* binding =
                    model.triangle_textures.empty()
                    ? nullptr
                    : &model.triangle_textures.at(triangleIndex);
                for (std::size_t vertex{0U}; vertex < 3U; ++vertex)
                {
                    const Vec3& point = points.at(vertex);
                    const TexCoord uv = binding != nullptr && binding->has_uv
                        ? binding->uv.at(vertex)
                        : TexCoord{};
                    mesh.indices.push_back(ResolveVertex(
                        mesh,
                        vertexIndices,
                        point,
                        *normal,
                        uv));
                }
            }
            submesh.index_count = static_cast<std::uint32_t>(
                mesh.indices.size() - submesh.first_index);
            if (submesh.index_count > 0U)
            {
                mesh.submeshes.push_back(std::move(submesh));
            }
        }

        if (mesh.indices.empty())
        {
            return Failure<ViewMesh>(
                "PM-SLICER-INPUT-0002",
                "ViewData mesh generation produced no triangles",
                model.model_path.generic_string());
        }
        mesh.mesh_identity = ComputeMeshIdentity(mesh);
        return ApiResult<ViewMesh>::Success(std::move(mesh));
    }
    catch (const std::exception& error)
    {
        return Failure<ViewMesh>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build ViewData mesh",
            error.what());
    }
    catch (...)
    {
        return Failure<ViewMesh>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build ViewData mesh",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::viewdata_detail
