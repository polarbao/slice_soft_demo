#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{

struct TriangleMeshData;

/**
 * @brief Stable errors for the Stage 12E texture/fill partition contract.
 */
enum class TextureFillPartitionErrorCode
{
    SurfaceShellWidthInvalid,
    SurfaceShellStepUnsupported,
    SurfaceShellGeometryModeUnsupported,
    SurfaceShellMinimumPolicyUnsupported,
    SurfaceScopeUnsupported,
    FullTextureAtModelLimitRequired,
    TextureFillScopeMismatch,
    ModelFillRequired,
    PartitionBackendUnavailable,
    PartitionBackendFailed,
    PartitionGridInvalid,
    PartitionMaskSizeMismatch,
    PartitionMaskNonBinary,
    TextureOutsideModel,
    ModelFillOutsideModel,
    TextureFillOverlap,
    ModelVoxelUnassigned,
};

/**
 * @brief Convert a Stage 12E error code to its stable machine-readable name.
 * @param code Error code.
 * @return Stable error name.
 */
std::string TextureFillPartitionErrorCodeName(TextureFillPartitionErrorCode code);

/**
 * @brief Exception carrying a stable Stage 12E error code.
 */
class TextureFillPartitionError : public std::runtime_error
{
public:
    /**
     * @brief Construct a Stage 12E contract error.
     * @param code Stable error code.
     * @param message Human-readable diagnostic message.
     */
    TextureFillPartitionError(TextureFillPartitionErrorCode code, const std::string& message);

    /**
     * @brief Return the stable Stage 12E error code.
     * @return Error code associated with this exception.
     */
    TextureFillPartitionErrorCode Code() const noexcept;

private:
    TextureFillPartitionErrorCode m_code;
};

/**
 * @brief Backend-neutral options for a future global 3D texture/fill partition.
 */
struct GlobalTextureFillPartitionOptions
{
    double requestedWidthMm{0.10};
    double widthStepMm{0.01};
    double baseMinimumWidthMm{0.10};
    std::string surfaceScope{"all_closed_surfaces"};
};

/**
 * @brief Grid shared by all backend-neutral Stage 12E three-dimensional masks.
 */
struct TextureFillPartitionGridSpec
{
    int width{0};
    int height{0};
    int depth{0};
    double originXMm{0.0};
    double originYMm{0.0};
    double originZMm{0.0};
    double spacingXMm{0.0};
    double spacingYMm{0.0};
    double spacingZMm{0.0};
};

/**
 * @brief Binary mask aligned to a Stage 12E three-dimensional grid.
 */
struct TextureFillPartitionMask3D
{
    TextureFillPartitionGridSpec grid;
    std::vector<std::uint8_t> values;
};

/**
 * @brief Statistics recomputed by the partition service rather than trusted from a backend.
 */
struct TextureFillPartitionStats
{
    std::uint64_t modelVoxels{0U};
    std::uint64_t textureSurfaceVoxels{0U};
    std::uint64_t modelFillVoxels{0U};
    std::uint64_t overlapTextureFillVoxels{0U};
    std::uint64_t unassignedModelVoxels{0U};
    std::uint64_t textureOutsideModelVoxels{0U};
    std::uint64_t modelFillOutsideModelVoxels{0U};
};

/**
 * @brief Request passed to a backend-neutral global texture/fill partition backend.
 */
struct GlobalTextureFillPartitionRequest
{
    const TriangleMeshData* mesh{nullptr};
    TextureFillPartitionGridSpec grid;
    GlobalTextureFillPartitionOptions options;
};

/**
 * @brief Candidate masks returned by a diagnostic Stage 12E backend.
 */
struct GlobalTextureFillPartitionCandidate
{
    bool available{false};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    TextureFillPartitionMask3D modelMask;
    TextureFillPartitionMask3D textureSurfaceMask;
    TextureFillPartitionMask3D modelFillMask;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Validated backend-neutral result from the Stage 12E partition service.
 */
struct GlobalTextureFillPartitionResult
{
    bool available{false};
    bool partitionPass{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    GlobalTextureFillPartitionOptions options;
    TextureFillPartitionGridSpec grid;
    TextureFillPartitionMask3D modelMask;
    TextureFillPartitionMask3D textureSurfaceMask;
    TextureFillPartitionMask3D modelFillMask;
    TextureFillPartitionStats stats;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Backend-neutral report state used before partition evidence exists.
 */
struct TextureFillPartitionReportData
{
    bool enabled{false};
    std::string strategy{"global_surface_shell"};
    std::string availability{"unavailable"};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    GlobalTextureFillPartitionOptions options;
    std::vector<ValidationIssue> issues;
};

}  // namespace slicer_core
