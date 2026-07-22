#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/process_profile/ModelFillMaterialResolver.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/preflight/ModelPreflightTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Input for one R4-05 clean-model texture/fill positive matrix.
 */
struct TextureFillPartitionPositiveMatrixRequest
{
    const AdaptedTriangleMesh* adaptedMesh{nullptr};
    const ModelPreflightResult* preflight{nullptr};
    std::string caseId;
    std::string modelPath;
    std::string sourceHash;
    std::string resourceHash;
    std::string preflightStatus;
    double voxelMm{0.20};
    int paddingVoxels{1};
    TextureSampleOptions textureSample;
    std::array<std::uint8_t, 3> fallbackRgb{0U, 0U, 0U};
    std::string missingTexturePolicy{"warn_and_fallback"};
    std::array<std::uint8_t, 3> modelFillRgb{0U, 0U, 0U};
    std::uint8_t modelFillValue{0U};
    MaterialProcessProfileBoundary profile;
    std::vector<ModelFillMaterialRoleRegistration> roleRegistry;
    std::vector<std::string> requestedRoles{"c", "m", "y", "k"};
};

/**
 * @brief Report evidence for one Model Fill material selection.
 */
struct TextureFillPartitionPositiveMaterialCase
{
    ModelFillMaterialResolution resolution;
    bool compositionEvaluated{false};
    bool compositionPass{false};
    std::uint64_t modelFillVoxels{0U};
    std::array<std::uint64_t, 6> printVoxels{0U, 0U, 0U, 0U, 0U, 0U};
    std::string reasonCode;
};

/**
 * @brief Diagnostic-only R4-05 positive matrix result.
 */
struct TextureFillPartitionPositiveMatrixResult
{
    bool evidenceCollected{false};
    bool matrixPass{false};
    bool productionOutputWritten{false};
    std::uint64_t requiredRepairPassCount{0U};
    TextureFillPartitionGridSpec grid;
    TextureFillPartitionWidthSweepResult widthSweep;
    double materialSampleWidthMm{0.0};
    TextureFillPartitionTextureTransferResult textureTransfer;
    std::vector<TextureFillPartitionPositiveMaterialCase> materialCases;
    std::vector<ValidationIssue> issues;
    Json report{Json::object({})};
};

/**
 * @brief Run minimum/intermediate/all-texture and Model Fill material diagnostics.
 * @param request Preflight-approved adapted mesh, input identity, grid, and material options.
 * @return Diagnostic report and in-memory evidence; production files are never written.
 */
TextureFillPartitionPositiveMatrixResult RunTextureFillPartitionPositiveMatrix(
    const TextureFillPartitionPositiveMatrixRequest& request);

}  // namespace slicer_core
