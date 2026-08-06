#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Inputs for writing one deterministic, job-owned OBJ repair asset.
 */
struct DeterministicObjAssetWriteRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    std::filesystem::path outputObjPath;
    std::function<bool()> cancellationRequested;
};

/**
 * @brief Files and attribute evidence produced by the deterministic OBJ writer.
 */
struct DeterministicObjAssetWriteResult
{
    std::filesystem::path objPath;
    std::filesystem::path mtlPath;
    std::vector<std::filesystem::path> texturePaths;
    bool uvPreserved{false};
    bool materialsPreserved{false};
    bool textureBytesPreserved{false};
};

/**
 * @brief Write an indexed repair candidate as deterministic OBJ/MTL resources.
 * @param request Candidate mesh, staging OBJ path and optional cancellation callback.
 * @return Paths and attribute-preservation evidence for the staged asset.
 * @throws std::runtime_error when input, resources, output ownership or writing is invalid.
 */
DeterministicObjAssetWriteResult WriteDeterministicObjAsset(
    const DeterministicObjAssetWriteRequest& request);

}  // namespace slicer_core
