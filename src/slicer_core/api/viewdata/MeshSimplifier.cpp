#include "slicer_core/api/viewdata/MeshSimplifier.h"

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 文件职责：隔离第三方网格简化器并保持材质组拓扑边界；
// 边界：无效索引、取消或不可接受结果均返回稳定错误，不静默降级。
namespace slicer_core::api::viewdata_detail
{
namespace
{

constexpr float kMaximumRelativeError{0.02F};

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), detail});
}

struct DisjointSet
{
    explicit DisjointSet(const std::size_t size)
        : parent(size), rank(size, 0U)
    {
        std::iota(parent.begin(), parent.end(), 0U);
    }

    std::size_t Find(const std::size_t item)
    {
        if (parent.at(item) != item)
        {
            parent.at(item) = Find(parent.at(item));
        }
        return parent.at(item);
    }

    void Join(const std::size_t left, const std::size_t right)
    {
        std::size_t leftRoot = Find(left);
        std::size_t rightRoot = Find(right);
        if (leftRoot == rightRoot)
        {
            return;
        }
        if (rank.at(leftRoot) < rank.at(rightRoot))
        {
            std::swap(leftRoot, rightRoot);
        }
        parent.at(rightRoot) = leftRoot;
        if (rank.at(leftRoot) == rank.at(rightRoot))
        {
            ++rank.at(leftRoot);
        }
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

using Edge = std::pair<std::uint32_t, std::uint32_t>;

Edge MakeEdge(const std::uint32_t first, const std::uint32_t second)
{
    return first < second
        ? Edge{first, second}
        : Edge{second, first};
}

struct TopologySummary
{
    std::size_t component_count{0U};
    bool has_isolated_triangle{false};
};

TopologySummary SummarizeTopology(
    const std::vector<std::uint32_t>& indices)
{
    const std::size_t triangleCount = indices.size() / 3U;
    DisjointSet sets(triangleCount);
    std::map<Edge, std::vector<std::size_t>> edgeTriangles;
    for (std::size_t triangle{0U}; triangle < triangleCount; ++triangle)
    {
        const std::array<std::uint32_t, 3> vertices{
            indices.at(triangle * 3U),
            indices.at(triangle * 3U + 1U),
            indices.at(triangle * 3U + 2U)};
        for (std::size_t edge{0U}; edge < 3U; ++edge)
        {
            edgeTriangles[MakeEdge(
                vertices.at(edge),
                vertices.at((edge + 1U) % 3U))].push_back(triangle);
        }
    }

    std::vector<bool> hasNeighbor(triangleCount, false);
    for (const auto& [edge, triangles] : edgeTriangles)
    {
        static_cast<void>(edge);
        if (triangles.size() < 2U)
        {
            continue;
        }
        for (std::size_t index{1U}; index < triangles.size(); ++index)
        {
            sets.Join(triangles.front(), triangles.at(index));
        }
        for (const std::size_t triangle : triangles)
        {
            hasNeighbor.at(triangle) = true;
        }
    }

    std::vector<std::size_t> roots;
    roots.reserve(triangleCount);
    for (std::size_t triangle{0U}; triangle < triangleCount; ++triangle)
    {
        roots.push_back(sets.Find(triangle));
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
    return {
        roots.size(),
        std::find(hasNeighbor.begin(), hasNeighbor.end(), false)
            != hasNeighbor.end()};
}

ApiResult<void> ValidateInput(
    const MeshSimplificationInput& input,
    const std::size_t targetTriangleCount)
{
    if (input.positions.empty()
        || input.positions.size() % 3U != 0U
        || input.texcoord0.size() / 2U != input.positions.size() / 3U
        || input.texcoord0.size() % 2U != 0U
        || input.indices.empty()
        || input.indices.size() % 3U != 0U
        || targetTriangleCount == 0U)
    {
        return Failure<void>(
            "PM-SLICER-VIEWDATA-SIMPLIFICATION",
            "ViewData simplification input is invalid",
            "buffer_shape");
    }
    const std::size_t vertexCount = input.positions.size() / 3U;
    for (const std::uint32_t index : input.indices)
    {
        if (index >= vertexCount)
        {
            return Failure<void>(
                "PM-SLICER-VIEWDATA-SIMPLIFICATION",
                "ViewData simplification index is out of range",
                std::to_string(index));
        }
    }
    return ApiResult<void>::Success();
}

bool IsDegenerateTriangle(
    const MeshSimplificationInput& input,
    const std::uint32_t first,
    const std::uint32_t second,
    const std::uint32_t third)
{
    if (first == second || second == third || first == third)
    {
        return true;
    }
    const std::size_t firstOffset = static_cast<std::size_t>(first) * 3U;
    const std::size_t secondOffset = static_cast<std::size_t>(second) * 3U;
    const std::size_t thirdOffset = static_cast<std::size_t>(third) * 3U;
    const std::array<double, 3> ab{
        input.positions.at(secondOffset)
            - input.positions.at(firstOffset),
        input.positions.at(secondOffset + 1U)
            - input.positions.at(firstOffset + 1U),
        input.positions.at(secondOffset + 2U)
            - input.positions.at(firstOffset + 2U)};
    const std::array<double, 3> ac{
        input.positions.at(thirdOffset)
            - input.positions.at(firstOffset),
        input.positions.at(thirdOffset + 1U)
            - input.positions.at(firstOffset + 1U),
        input.positions.at(thirdOffset + 2U)
            - input.positions.at(firstOffset + 2U)};
    const std::array<double, 3> cross{
        ab.at(1U) * ac.at(2U) - ab.at(2U) * ac.at(1U),
        ab.at(2U) * ac.at(0U) - ab.at(0U) * ac.at(2U),
        ab.at(0U) * ac.at(1U) - ab.at(1U) * ac.at(0U)};
    const double squaredLength = cross.at(0U) * cross.at(0U)
        + cross.at(1U) * cross.at(1U)
        + cross.at(2U) * cross.at(2U);
    return squaredLength <= 1.0e-20;
}

void RemoveDegenerateTriangles(
    const MeshSimplificationInput& input,
    std::vector<std::uint32_t>& indices)
{
    std::size_t writeOffset{0U};
    for (std::size_t readOffset{0U}; readOffset < indices.size();
         readOffset += 3U)
    {
        const std::uint32_t first = indices.at(readOffset);
        const std::uint32_t second = indices.at(readOffset + 1U);
        const std::uint32_t third = indices.at(readOffset + 2U);
        if (IsDegenerateTriangle(input, first, second, third))
        {
            continue;
        }
        indices.at(writeOffset) = first;
        indices.at(writeOffset + 1U) = second;
        indices.at(writeOffset + 2U) = third;
        writeOffset += 3U;
    }
    indices.resize(writeOffset);
}

}  // namespace

ApiResult<MeshSimplificationResult> SimplifyViewMesh(
    const MeshSimplificationInput& input,
    const std::size_t targetTriangleCount,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        const ApiResult<void> validation = ValidateInput(
            input,
            targetTriangleCount);
        if (!validation.IsOk())
        {
            return Failure<MeshSimplificationResult>(
                validation.Error()->code,
                validation.Error()->message,
                validation.Error()->detail);
        }
        if (cancelToken.IsCancelRequested())
        {
            return Failure<MeshSimplificationResult>(
                "PM-SLICER-CANCELLED-0070",
                "ViewData simplification was cancelled",
                "meshoptimizer");
        }

        const std::size_t sourceTriangleCount = input.indices.size() / 3U;
        if (sourceTriangleCount <= targetTriangleCount)
        {
            return ApiResult<MeshSimplificationResult>::Success(
                {input.indices, 0.0F});
        }

        MeshSimplificationResult result;
        result.indices.resize(input.indices.size());
        constexpr std::array<float, 2> attributeWeights{1.0F, 1.0F};
        const std::size_t outputIndexCount = meshopt_simplifyWithAttributes(
            result.indices.data(),
            input.indices.data(),
            input.indices.size(),
            input.positions.data(),
            input.positions.size() / 3U,
            sizeof(float) * 3U,
            input.texcoord0.data(),
            sizeof(float) * 2U,
            attributeWeights.data(),
            attributeWeights.size(),
            nullptr,
            targetTriangleCount * 3U,
            kMaximumRelativeError,
            meshopt_SimplifyLockBorder,
            &result.relative_error);
        result.indices.resize(outputIndexCount);
        RemoveDegenerateTriangles(input, result.indices);

        if (cancelToken.IsCancelRequested())
        {
            return Failure<MeshSimplificationResult>(
                "PM-SLICER-CANCELLED-0070",
                "ViewData simplification was cancelled",
                "meshoptimizer");
        }
        if (result.indices.empty()
            || result.indices.size() % 3U != 0U
            || result.indices.size() / 3U > targetTriangleCount
            || !std::isfinite(result.relative_error)
            || result.relative_error > kMaximumRelativeError + 1.0e-6F)
        {
            return Failure<MeshSimplificationResult>(
                "PM-SLICER-VIEWDATA-SIMPLIFICATION",
                "ViewData mesh cannot meet the safe simplification budget",
                std::to_string(result.indices.size() / 3U)
                    + "/" + std::to_string(targetTriangleCount));
        }

        const TopologySummary sourceTopology = SummarizeTopology(
            input.indices);
        const TopologySummary resultTopology = SummarizeTopology(
            result.indices);
        if (resultTopology.component_count > sourceTopology.component_count
            || (!sourceTopology.has_isolated_triangle
                && resultTopology.has_isolated_triangle))
        {
            return Failure<MeshSimplificationResult>(
                "PM-SLICER-VIEWDATA-SIMPLIFICATION",
                "ViewData simplification would fragment mesh topology",
                std::to_string(sourceTopology.component_count)
                    + "->" + std::to_string(resultTopology.component_count));
        }
        return ApiResult<MeshSimplificationResult>::Success(
            std::move(result));
    }
    catch (const std::exception& error)
    {
        return Failure<MeshSimplificationResult>(
            "PM-SLICER-INTERNAL-0099",
            "ViewData mesh simplification failed",
            error.what());
    }
    catch (...)
    {
        return Failure<MeshSimplificationResult>(
            "PM-SLICER-INTERNAL-0099",
            "ViewData mesh simplification failed",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::viewdata_detail
