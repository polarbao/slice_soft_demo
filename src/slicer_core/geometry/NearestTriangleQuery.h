#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace slicer_core
{

/**
 * @brief Closest source triangle result for one world-space query point.
 */
struct NearestTriangleHit
{
    bool found{false};
    std::size_t triangle_index{0};
    Vec3 closest_point_mm;
    std::array<double, 3> barycentric{0.0, 0.0, 0.0};
    double distance_mm{0.0};
};

/**
 * @brief Options for stable nearest-triangle selection.
 */
struct NearestTriangleQueryOptions
{
    double tie_epsilon_mm{1.0e-7};
};

/**
 * @brief Query/build statistics for BVH instrumentation.
 */
struct NearestTriangleQueryStats
{
    std::uint64_t query_count{0};
    std::uint64_t visited_nodes{0};
    std::uint64_t tested_triangles{0};
    std::uint64_t max_visited_nodes{0};
    std::size_t node_count{0};
    std::size_t estimated_bytes{0};
};

/**
 * @brief AABB BVH nearest-triangle query for experimental texture transfer.
 */
class NearestTriangleQuery
{
public:
    /**
     * @brief Build a nearest-triangle BVH.
     * @param mesh Indexed triangle mesh copied into the query object.
     */
    explicit NearestTriangleQuery(const TriangleMeshData& mesh);

    /** @brief Destroy the BVH query object. */
    ~NearestTriangleQuery();

    NearestTriangleQuery(NearestTriangleQuery&&) noexcept;
    NearestTriangleQuery& operator=(NearestTriangleQuery&&) noexcept;
    NearestTriangleQuery(const NearestTriangleQuery&) = delete;
    NearestTriangleQuery& operator=(const NearestTriangleQuery&) = delete;

    /**
     * @brief Find the nearest source triangle.
     * @param pointMm Query point in world millimeters.
     * @return Nearest hit and barycentric coordinates.
     */
    NearestTriangleHit FindNearest(const Vec3& pointMm) const;

    /**
     * @brief Find the nearest source triangle and update query statistics.
     * @param pointMm Query point in world millimeters.
     * @param options Stable selection options.
     * @param stats Statistics accumulator.
     * @return Nearest hit and barycentric coordinates.
     */
    NearestTriangleHit FindNearestWithStats(
        const Vec3& pointMm,
        const NearestTriangleQueryOptions& options,
        NearestTriangleQueryStats& stats) const;

    /**
     * @brief Return static build statistics for this BVH.
     * @return Node count and estimated memory.
     */
    NearestTriangleQueryStats GetBuildStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Brute-force nearest query used only by unit-test comparison.
 * @param mesh Indexed triangle mesh.
 * @param pointMm Query point in world millimeters.
 * @return Nearest hit.
 */
NearestTriangleHit FindNearestTriangleBruteForce(const TriangleMeshData& mesh, const Vec3& pointMm);

/**
 * @brief Compare two hits using distance, barycentric interior margin, then triangle index.
 * @param candidate Candidate hit.
 * @param best Current best hit.
 * @param tieEpsilonMm Distance tie epsilon in millimeters.
 * @return True when candidate is a better stable hit.
 */
bool IsBetterNearestTriangleHit(
    const NearestTriangleHit& candidate,
    const NearestTriangleHit& best,
    double tieEpsilonMm);

}  // namespace slicer_core
