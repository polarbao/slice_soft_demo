#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <filesystem>
#include <functional>
#include <string>

namespace slicer_core
{

/**
 * @brief Inputs that bind one staged repair asset to its effective Profile.
 */
struct StrictRepairAssetRecheckRequest
{
    std::filesystem::path stagedObjPath;
    std::filesystem::path profileConfigPath;
    std::string profileHash;
    const AdaptedTriangleMesh* expectedMesh{nullptr};
    std::function<bool()> cancellationRequested;
};

/**
 * @brief Complete reimport, attribute and strict-topology evidence.
 */
struct StrictRepairAssetRecheckResult
{
    bool assetReimported{false};
    bool strictComplete{false};
    bool strictPass{false};
    bool attributesPreserved{false};
    std::string geometryHash;
    std::string attributeHash;
    MeshRepairDiagnosticsSummary diagnostics;
};

/**
 * @brief Reimport one staged OBJ with effective Profile resource rules and run strict audit.
 * @param request Staged asset, Profile identity, expected in-memory candidate and cancellation.
 * @return Complete reimport and strict topology evidence.
 * @throws std::runtime_error on stale identity, import failure, cancellation or incomplete audit.
 */
StrictRepairAssetRecheckResult RecheckStrictRepairAsset(
    const StrictRepairAssetRecheckRequest& request);

}  // namespace slicer_core
