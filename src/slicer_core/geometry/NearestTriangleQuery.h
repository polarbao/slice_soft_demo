#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"

#include <array>
#include <cstddef>
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

}  // namespace slicer_core
