#pragma once

#include "slicer_core/geometry/repair/MeshRepairTypes.h"
#include "slicer_core/json_value.h"

namespace slicer_core
{

/**
 * @brief Build the not-evaluated 12E mesh-repair report skeleton.
 * @param input Stable input summary.
 * @param options Explicit repair options.
 * @param hashes Canonical pre-repair hashes.
 * @return JSON conforming to slicesoft.mesh_repair.12e_08c.1.
 */
Json BuildMeshRepairReportSkeleton(
    const MeshRepairInputSummary& input,
    const MeshRepairOptions& options,
    const MeshRepairHashes& hashes);

/**
 * @brief Serialize an in-memory mesh-repair result without writing files.
 * @param result Backend-neutral repair evidence.
 * @return JSON conforming to slicesoft.mesh_repair.12e_08c.1.
 */
Json BuildMeshRepairReport(const MeshRepairResult& result);

}  // namespace slicer_core
