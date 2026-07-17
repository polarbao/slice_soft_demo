#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace slicer_core
{

/**
 * @brief Options for deterministic point-in-closed-mesh classification.
 */
struct PointInClosedMeshQueryOptions
{
    double boundaryEpsilonMm{1.0e-7};
    double barycentricEpsilon{1.0e-10};
};

/**
 * @brief Result for one point-in-closed-mesh query.
 */
struct PointInClosedMeshResult
{
    bool inside{false};
    bool boundary{false};
    bool ambiguous{false};
    bool usedFallbackRay{false};
    std::uint64_t intersectionCount{0U};
};

/**
 * @brief Build and query statistics for the ray BVH.
 */
struct PointInClosedMeshQueryStats
{
    std::uint64_t queryCount{0U};
    std::uint64_t visitedNodes{0U};
    std::uint64_t testedTriangles{0U};
    std::uint64_t fallbackRayCount{0U};
    std::uint64_t ambiguousRayCount{0U};
    std::uint64_t boundaryPointCount{0U};
    std::size_t nodeCount{0U};
    std::size_t estimatedBytes{0U};
};

/**
 * @brief AABB BVH query for deterministic closed-mesh occupancy classification.
 */
class PointInClosedMeshQuery
{
public:
    /**
     * @brief Build a ray-query BVH for a closed indexed mesh.
     * @param mesh Indexed triangle mesh copied into the query object.
     */
    explicit PointInClosedMeshQuery(const TriangleMeshData& mesh);

    /** @brief Destroy the query object. */
    ~PointInClosedMeshQuery();

    /**
     * @brief Move a closed-mesh query and its owned acceleration data.
     * @param other Query object to move from.
     */
    PointInClosedMeshQuery(PointInClosedMeshQuery&& other) noexcept;

    /**
     * @brief Replace this query with another query's owned acceleration data.
     * @param other Query object to move from.
     * @return This query object.
     */
    PointInClosedMeshQuery& operator=(PointInClosedMeshQuery&& other) noexcept;

    /** @brief Copy construction is disabled because the query owns acceleration data. */
    PointInClosedMeshQuery(const PointInClosedMeshQuery&) = delete;

    /** @brief Copy assignment is disabled because the query owns acceleration data. */
    PointInClosedMeshQuery& operator=(const PointInClosedMeshQuery&) = delete;

    /**
     * @brief Classify one world-space point and accumulate query statistics.
     * @param pointMm Query point in millimeters.
     * @param options Boundary and tie tolerances.
     * @param stats Statistics accumulator.
     * @return Deterministic inside, boundary, or ambiguous result.
     */
    PointInClosedMeshResult Classify(
        const Vec3& pointMm,
        const PointInClosedMeshQueryOptions& options,
        PointInClosedMeshQueryStats& stats) const;

    /**
     * @brief Return static BVH build statistics.
     * @return Node count and estimated memory.
     */
    PointInClosedMeshQueryStats GetBuildStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Brute-force ray oracle used by generated fixture tests.
 * @param mesh Closed indexed mesh.
 * @param pointMm Query point in millimeters.
 * @param options Boundary and tie tolerances.
 * @return Deterministic inside, boundary, or ambiguous result.
 */
PointInClosedMeshResult ClassifyPointInClosedMeshBruteForce(
    const TriangleMeshData& mesh,
    const Vec3& pointMm,
    const PointInClosedMeshQueryOptions& options = {});

}  // namespace slicer_core
