#include "slicer_core/api/viewdata/SceneViewMeshBuilder.h"

#include "slicer_core/api/viewdata/MeshSimplifier.h"
#include "slicer_core/api/viewdata/SceneViewIdentity.h"
#include "slicer_core/api/viewdata/SceneViewMeshInputBuilder.h"

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

struct PreparedTriangle
{
    std::array<Vec3, 3> points{};
    std::array<TexCoord, 3> uvs{};
    Vec3 face_normal{};
    std::array<double, 3> corner_weights{};
    std::array<Vec3, 3> vertex_normals{};
};

double Length(const Vec3& vector)
{
    return std::sqrt(
        vector.x * vector.x
        + vector.y * vector.y
        + vector.z * vector.z);
}

std::optional<Vec3> Normalize(const Vec3& vector)
{
    const double length = Length(vector);
    if (!std::isfinite(length) || length <= 1.0e-12)
    {
        return std::nullopt;
    }
    return Vec3{
        vector.x / length,
        vector.y / length,
        vector.z / length};
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
    const Vec3 normal{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x};
    return Normalize(normal);
}

double CornerAngle(
    const Vec3& point,
    const Vec3& firstNeighbor,
    const Vec3& secondNeighbor)
{
    const std::optional<Vec3> first = Normalize({
        firstNeighbor.x - point.x,
        firstNeighbor.y - point.y,
        firstNeighbor.z - point.z});
    const std::optional<Vec3> second = Normalize({
        secondNeighbor.x - point.x,
        secondNeighbor.y - point.y,
        secondNeighbor.z - point.z});
    if (!first.has_value() || !second.has_value())
    {
        return 1.0;
    }
    const double cosine = std::clamp(
        first->x * second->x
            + first->y * second->y
            + first->z * second->z,
        -1.0,
        1.0);
    const double angle = std::acos(cosine);
    return std::isfinite(angle) && angle > 1.0e-12 ? angle : 1.0;
}

std::optional<PreparedTriangle> PrepareTriangle(
    const MeshSimplificationInput& input,
    const MeshSimplificationResult& simplified,
    const std::size_t offset)
{
    PreparedTriangle prepared;
    for (std::size_t corner{0U}; corner < 3U; ++corner)
    {
        const std::uint32_t sourceIndex =
            simplified.indices.at(offset + corner);
        prepared.points.at(corner) = ReadSimplificationPoint(
            input,
            sourceIndex);
        prepared.uvs.at(corner) = ReadSimplificationUv(input, sourceIndex);
    }
    const Triangle triangle{
        prepared.points.at(0U),
        prepared.points.at(1U),
        prepared.points.at(2U)};
    const std::optional<Vec3> normal = ComputeNormal(triangle);
    if (!normal.has_value())
    {
        return std::nullopt;
    }
    prepared.face_normal = *normal;
    prepared.vertex_normals.fill(*normal);
    for (std::size_t corner{0U}; corner < 3U; ++corner)
    {
        prepared.corner_weights.at(corner) = CornerAngle(
            prepared.points.at(corner),
            prepared.points.at((corner + 1U) % 3U),
            prepared.points.at((corner + 2U) % 3U));
    }
    return prepared;
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

struct PositionKey
{
    std::array<std::uint32_t, 3> values{};

    [[nodiscard]] bool operator<(const PositionKey& other) const noexcept
    {
        return values < other.values;
    }

    [[nodiscard]] bool operator==(const PositionKey& other) const noexcept
    {
        return values == other.values;
    }
};

struct CornerReference
{
    std::size_t triangle_index{0U};
    std::size_t corner_index{0U};
};

std::uint32_t FloatBits(const float value)
{
    return value == 0.0F
        ? 0U
        : std::bit_cast<std::uint32_t>(value);
}

PositionKey MakePositionKey(const Vec3& point)
{
    return {{
        FloatBits(static_cast<float>(point.x)),
        FloatBits(static_cast<float>(point.y)),
        FloatBits(static_cast<float>(point.z))}};
}

double Dot(const Vec3& left, const Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

bool SharesEdgeAtPosition(
    const PreparedTriangle& leftTriangle,
    const std::size_t leftCorner,
    const PreparedTriangle& rightTriangle,
    const std::size_t rightCorner)
{
    const std::array<PositionKey, 2> leftNeighbors{
        MakePositionKey(leftTriangle.points.at((leftCorner + 1U) % 3U)),
        MakePositionKey(leftTriangle.points.at((leftCorner + 2U) % 3U))};
    const std::array<PositionKey, 2> rightNeighbors{
        MakePositionKey(rightTriangle.points.at((rightCorner + 1U) % 3U)),
        MakePositionKey(rightTriangle.points.at((rightCorner + 2U) % 3U))};
    return std::any_of(
        leftNeighbors.begin(),
        leftNeighbors.end(),
        [&rightNeighbors](const PositionKey& left)
        {
            return std::find(
                       rightNeighbors.begin(),
                       rightNeighbors.end(),
                       left)
                != rightNeighbors.end();
        });
}

std::size_t FindRoot(
    std::vector<std::size_t>& parents,
    const std::size_t index)
{
    if (parents.at(index) != index)
    {
        parents.at(index) = FindRoot(parents, parents.at(index));
    }
    return parents.at(index);
}

void MergeRoots(
    std::vector<std::size_t>& parents,
    const std::size_t left,
    const std::size_t right)
{
    const std::size_t leftRoot = FindRoot(parents, left);
    const std::size_t rightRoot = FindRoot(parents, right);
    if (leftRoot != rightRoot)
    {
        parents.at(rightRoot) = leftRoot;
    }
}

void ComputeSmoothNormals(
    std::vector<PreparedTriangle>& triangles,
    const double creaseAngleDegrees)
{
    constexpr double pi{3.14159265358979323846};
    const double creaseCosine = std::cos(creaseAngleDegrees * pi / 180.0);
    std::map<PositionKey, std::vector<CornerReference>> cornersByPosition;
    for (std::size_t triangleIndex{0U};
         triangleIndex < triangles.size();
         ++triangleIndex)
    {
        for (std::size_t cornerIndex{0U}; cornerIndex < 3U; ++cornerIndex)
        {
            cornersByPosition[MakePositionKey(
                triangles.at(triangleIndex).points.at(cornerIndex))]
                .push_back({triangleIndex, cornerIndex});
        }
    }

    for (const auto& [position, corners] : cornersByPosition)
    {
        static_cast<void>(position);
        std::vector<std::size_t> parents(corners.size());
        for (std::size_t index{0U}; index < parents.size(); ++index)
        {
            parents.at(index) = index;
        }
        for (std::size_t left{0U}; left < corners.size(); ++left)
        {
            const CornerReference& leftCorner = corners.at(left);
            const PreparedTriangle& leftTriangle =
                triangles.at(leftCorner.triangle_index);
            for (std::size_t right{left + 1U}; right < corners.size(); ++right)
            {
                const CornerReference& rightCorner = corners.at(right);
                const PreparedTriangle& rightTriangle =
                    triangles.at(rightCorner.triangle_index);
                if (SharesEdgeAtPosition(
                        leftTriangle,
                        leftCorner.corner_index,
                        rightTriangle,
                        rightCorner.corner_index)
                    && Dot(leftTriangle.face_normal, rightTriangle.face_normal)
                        + 1.0e-12 >= creaseCosine)
                {
                    MergeRoots(parents, left, right);
                }
            }
        }

        std::map<std::size_t, Vec3> weightedNormals;
        for (std::size_t index{0U}; index < corners.size(); ++index)
        {
            const CornerReference& corner = corners.at(index);
            const PreparedTriangle& triangle =
                triangles.at(corner.triangle_index);
            const double weight = triangle.corner_weights.at(
                corner.corner_index);
            Vec3& sum = weightedNormals[FindRoot(parents, index)];
            sum.x += triangle.face_normal.x * weight;
            sum.y += triangle.face_normal.y * weight;
            sum.z += triangle.face_normal.z * weight;
        }
        for (std::size_t index{0U}; index < corners.size(); ++index)
        {
            const CornerReference& corner = corners.at(index);
            const std::optional<Vec3> normal = Normalize(
                weightedNormals.at(FindRoot(parents, index)));
            if (normal.has_value())
            {
                triangles.at(corner.triangle_index)
                    .vertex_normals.at(corner.corner_index) = *normal;
            }
        }
    }
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
    const MeshAttributeFormat attributeFormat,
    const ICancelToken& cancelToken,
    const ViewMeshNormalOptions& normalOptions) noexcept
{
    try
    {
        if (!std::isfinite(normalOptions.crease_angle_degrees)
            || normalOptions.crease_angle_degrees < 0.0
            || normalOptions.crease_angle_degrees > 180.0)
        {
            return Failure<ViewMesh>(
                "PM-SLICER-PROFILE-0031",
                "ViewData crease angle must be within [0, 180] degrees",
                "viewMeshNormalOptions.creaseAngleDegrees");
        }
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
        mesh.attribute_format = attributeFormat;
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

            ApiResult<MeshSimplificationInput> input = BuildViewMeshGroupInput(
                model,
                triangleIndices,
                worldMatrix,
                meshTransform,
                cancelToken);
            if (!input.IsOk())
            {
                return Failure<ViewMesh>(
                    input.Error()->code,
                    input.Error()->message,
                    input.Error()->detail);
            }
            ApiResult<MeshSimplificationResult> simplified =
                SimplifyViewMesh(*input.Value(), groupBudget, cancelToken);
            if (!simplified.IsOk())
            {
                return Failure<ViewMesh>(
                    simplified.Error()->code,
                    simplified.Error()->message,
                    appearance.materials.at(materialIndex)
                        .material.material_id
                        + ":" + simplified.Error()->detail);
            }

            std::vector<PreparedTriangle> preparedTriangles;
            preparedTriangles.reserve(
                simplified.Value()->indices.size() / 3U);
            for (std::size_t offset{0U};
                 offset < simplified.Value()->indices.size();
                 offset += 3U)
            {
                const std::optional<PreparedTriangle> prepared =
                    PrepareTriangle(
                        *input.Value(),
                        *simplified.Value(),
                        offset);
                if (!prepared.has_value())
                {
                    return Failure<ViewMesh>(
                        "PM-SLICER-VIEWDATA-SIMPLIFICATION",
                        "ViewData simplification produced a degenerate triangle",
                        appearance.materials.at(materialIndex)
                            .material.material_id);
                }
                preparedTriangles.push_back(*prepared);
            }
            ComputeSmoothNormals(
                preparedTriangles,
                normalOptions.crease_angle_degrees);

            for (const PreparedTriangle& triangle : preparedTriangles)
            {
                for (std::size_t vertex{0U}; vertex < 3U; ++vertex)
                {
                    mesh.indices.push_back(ResolveVertex(
                        mesh,
                        vertexIndices,
                        triangle.points.at(vertex),
                        triangle.vertex_normals.at(vertex),
                        triangle.uvs.at(vertex)));
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
