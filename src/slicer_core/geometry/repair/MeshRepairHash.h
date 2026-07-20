#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Compute SHA-256 for an already canonical byte string.
 * @param canonicalPayload Stable canonical bytes.
 * @return Lowercase 64-character SHA-256 hex string.
 */
std::string ComputeMeshRepairSha256(std::string_view canonicalPayload);

/**
 * @brief Hash indexed geometry using mesh_repair_canonical.1 encoding.
 * @param mesh Indexed mesh to hash.
 * @return Stable geometry SHA-256 hex string.
 */
std::string ComputeMeshRepairGeometryHash(const TriangleMeshData& mesh);

/**
 * @brief Hash per-triangle and material attributes independently from geometry.
 * @param mesh Adapted mesh containing source attributes.
 * @return Stable attribute SHA-256 hex string.
 */
std::string ComputeMeshRepairAttributeHash(const AdaptedTriangleMesh& mesh);

/**
 * @brief Hash explicit mesh-repair options.
 * @param options Repair options.
 * @return Stable options SHA-256 hex string.
 */
std::string ComputeMeshRepairOptionsHash(const MeshRepairOptions& options);

/**
 * @brief Hash an ordered repair operation list.
 * @param operations Ordered operation records.
 * @return Stable operation SHA-256 hex string.
 */
std::string ComputeMeshRepairOperationsHash(
    const std::vector<MeshRepairOperation>& operations);

/**
 * @brief Build the pre-repair hash set without mutating the mesh.
 * @param mesh Adapted mesh and attributes.
 * @param options Explicit repair options.
 * @param operations Ordered operations, normally empty in R1.
 * @return Hash DTO conforming to mesh_repair_canonical.1.
 */
MeshRepairHashes ComputeMeshRepairPreHashes(
    const AdaptedTriangleMesh& mesh,
    const MeshRepairOptions& options,
    const std::vector<MeshRepairOperation>& operations = {});

}  // namespace slicer_core
