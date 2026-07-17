#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"

#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <string>

namespace slicer_core
{
namespace
{

ValidationIssue MakePartitionIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        ValidationSeverity::Error,
        message);
}

ValidationIssue MakeCountIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message,
    const std::uint64_t count)
{
    ValidationIssue issue = MakePartitionIssue(code, message);
    issue.context = Json::object({{"count", count}});
    return issue;
}

bool IsFiniteGrid(const TextureFillPartitionGridSpec& grid)
{
    return grid.width > 0
        && grid.height > 0
        && grid.depth > 0
        && std::isfinite(grid.originXMm)
        && std::isfinite(grid.originYMm)
        && std::isfinite(grid.originZMm)
        && std::isfinite(grid.spacingXMm)
        && std::isfinite(grid.spacingYMm)
        && std::isfinite(grid.spacingZMm)
        && grid.spacingXMm > 0.0
        && grid.spacingYMm > 0.0
        && grid.spacingZMm > 0.0;
}

bool GridsMatch(
    const TextureFillPartitionGridSpec& first,
    const TextureFillPartitionGridSpec& second)
{
    return first.width == second.width
        && first.height == second.height
        && first.depth == second.depth
        && first.originXMm == second.originXMm
        && first.originYMm == second.originYMm
        && first.originZMm == second.originZMm
        && first.spacingXMm == second.spacingXMm
        && first.spacingYMm == second.spacingYMm
        && first.spacingZMm == second.spacingZMm;
}

bool IsGridSpecified(const TextureFillPartitionGridSpec& grid)
{
    return grid.width != 0 || grid.height != 0 || grid.depth != 0
        || grid.originXMm != 0.0 || grid.originYMm != 0.0
        || grid.originZMm != 0.0
        || grid.spacingXMm != 0.0 || grid.spacingYMm != 0.0
        || grid.spacingZMm != 0.0;
}

std::optional<std::size_t> VoxelCount(const TextureFillPartitionGridSpec& grid)
{
    if (!IsFiniteGrid(grid))
    {
        return std::nullopt;
    }

    const std::size_t width = static_cast<std::size_t>(grid.width);
    const std::size_t height = static_cast<std::size_t>(grid.height);
    const std::size_t depth = static_cast<std::size_t>(grid.depth);
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    if (width > maximum / height)
    {
        return std::nullopt;
    }
    const std::size_t area = width * height;
    if (area > maximum / depth)
    {
        return std::nullopt;
    }
    return area * depth;
}

std::uint64_t CountNonBinaryValues(const TextureFillPartitionMask3D& mask)
{
    std::uint64_t count{0U};
    for (const std::uint8_t value : mask.values)
    {
        count += value > 1U ? 1U : 0U;
    }
    return count;
}

bool HasErrorIssue(const std::vector<ValidationIssue>& issues)
{
    for (const ValidationIssue& issue : issues)
    {
        if (issue.severity == ValidationSeverity::Error)
        {
            return true;
        }
    }
    return false;
}

bool ValidateMaskShapes(GlobalTextureFillPartitionResult& result)
{
    const std::optional<std::size_t> expectedVoxelCount = VoxelCount(result.grid);
    if (!expectedVoxelCount.has_value())
    {
        result.issues.push_back(MakePartitionIssue(
            TextureFillPartitionErrorCode::PartitionGridInvalid,
            "global texture/fill partition grid must have finite positive dimensions and spacing"));
        return false;
    }

    if (!GridsMatch(result.grid, result.textureSurfaceMask.grid)
        || !GridsMatch(result.grid, result.modelFillMask.grid)
        || result.modelMask.values.size() != expectedVoxelCount.value()
        || result.textureSurfaceMask.values.size() != expectedVoxelCount.value()
        || result.modelFillMask.values.size() != expectedVoxelCount.value())
    {
        result.issues.push_back(MakePartitionIssue(
            TextureFillPartitionErrorCode::PartitionMaskSizeMismatch,
            "model, texture-surface, and model-fill masks must share one grid and voxel count"));
        return false;
    }
    return true;
}

void ValidateBinaryValues(GlobalTextureFillPartitionResult& result)
{
    const std::uint64_t nonBinaryCount = CountNonBinaryValues(result.modelMask)
        + CountNonBinaryValues(result.textureSurfaceMask)
        + CountNonBinaryValues(result.modelFillMask);
    if (nonBinaryCount > 0U)
    {
        result.issues.push_back(MakeCountIssue(
            TextureFillPartitionErrorCode::PartitionMaskNonBinary,
            "global texture/fill partition masks must contain only 0 or 1",
            nonBinaryCount));
    }
}

void RecomputePartitionStats(GlobalTextureFillPartitionResult& result)
{
    const std::size_t voxelCount = result.modelMask.values.size();
    for (std::size_t index{0U}; index < voxelCount; ++index)
    {
        const bool model = result.modelMask.values.at(index) != 0U;
        const bool texture = result.textureSurfaceMask.values.at(index) != 0U;
        const bool fill = result.modelFillMask.values.at(index) != 0U;

        result.stats.modelVoxels += model ? 1U : 0U;
        result.stats.textureSurfaceVoxels += texture ? 1U : 0U;
        result.stats.modelFillVoxels += fill ? 1U : 0U;
        result.stats.textureOutsideModelVoxels += texture && !model ? 1U : 0U;
        result.stats.modelFillOutsideModelVoxels += fill && !model ? 1U : 0U;
        result.stats.overlapTextureFillVoxels += texture && fill ? 1U : 0U;
        result.stats.unassignedModelVoxels += model && !texture && !fill ? 1U : 0U;
    }
}

void AppendPartitionInvariantIssues(GlobalTextureFillPartitionResult& result)
{
    if (result.stats.textureOutsideModelVoxels > 0U)
    {
        result.issues.push_back(MakeCountIssue(
            TextureFillPartitionErrorCode::TextureOutsideModel,
            "texture-surface mask contains voxels outside the model mask",
            result.stats.textureOutsideModelVoxels));
    }
    if (result.stats.modelFillOutsideModelVoxels > 0U)
    {
        result.issues.push_back(MakeCountIssue(
            TextureFillPartitionErrorCode::ModelFillOutsideModel,
            "model-fill mask contains voxels outside the model mask",
            result.stats.modelFillOutsideModelVoxels));
    }
    if (result.stats.overlapTextureFillVoxels > 0U)
    {
        result.issues.push_back(MakeCountIssue(
            TextureFillPartitionErrorCode::TextureFillOverlap,
            "texture-surface and model-fill masks overlap",
            result.stats.overlapTextureFillVoxels));
    }
    if (result.stats.unassignedModelVoxels > 0U)
    {
        result.issues.push_back(MakeCountIssue(
            TextureFillPartitionErrorCode::ModelVoxelUnassigned,
            "one or more model voxels are not assigned to texture surface or model fill",
            result.stats.unassignedModelVoxels));
    }
}

}  // namespace

GlobalTextureFillPartitionService::GlobalTextureFillPartitionService(
    const IGlobalTextureFillPartitionBackend* backend)
    : m_backend(backend)
{
}

GlobalTextureFillPartitionResult GlobalTextureFillPartitionService::Evaluate(
    const GlobalTextureFillPartitionRequest& request) const
{
    GlobalTextureFillPartitionResult result;
    result.options = request.options;
    if (m_backend == nullptr)
    {
        result.issues.push_back(MakePartitionIssue(
            TextureFillPartitionErrorCode::PartitionBackendUnavailable,
            "global texture/fill partition backend is unavailable"));
        return result;
    }

    GlobalTextureFillPartitionCandidate candidate;
    try
    {
        candidate = m_backend->Evaluate(request);
    }
    catch (const std::exception& error)
    {
        result.issues.push_back(MakePartitionIssue(
            TextureFillPartitionErrorCode::PartitionBackendFailed,
            std::string{"global texture/fill partition backend failed: "} + error.what()));
        return result;
    }
    result.available = candidate.available;
    result.backend = candidate.backend;
    result.backendRole = candidate.backendRole;
    result.modelMask = candidate.modelMask;
    result.textureSurfaceMask = candidate.textureSurfaceMask;
    result.modelFillMask = candidate.modelFillMask;
    result.widthMetrics = candidate.widthMetrics;
    result.queryStats = candidate.queryStats;
    result.performance = candidate.performance;
    result.closestSurfaceReferences = candidate.closestSurfaceReferences;
    result.grid = candidate.modelMask.grid;
    result.issues = candidate.issues;

    if (!candidate.available)
    {
        if (result.issues.empty())
        {
            result.issues.push_back(MakePartitionIssue(
                TextureFillPartitionErrorCode::PartitionBackendUnavailable,
                "global texture/fill partition backend returned unavailable"));
        }
        return result;
    }

    if (candidate.blocked)
    {
        result.status = "blocked";
        return result;
    }

    if (IsGridSpecified(request.grid))
    {
        if (!IsFiniteGrid(request.grid))
        {
            result.issues.push_back(MakePartitionIssue(
                TextureFillPartitionErrorCode::PartitionGridInvalid,
                "requested global texture/fill partition grid is invalid"));
            result.status = "fail";
            return result;
        }
        if (!GridsMatch(request.grid, result.grid))
        {
            result.issues.push_back(MakePartitionIssue(
                TextureFillPartitionErrorCode::PartitionMaskSizeMismatch,
                "backend partition grid does not match the requested grid"));
            result.status = "fail";
            return result;
        }
    }

    if (!ValidateMaskShapes(result))
    {
        result.status = "fail";
        return result;
    }

    ValidateBinaryValues(result);
    RecomputePartitionStats(result);
    AppendPartitionInvariantIssues(result);
    result.partitionPass = !HasErrorIssue(result.issues);
    result.status = result.partitionPass ? "diagnostic" : "fail";
    return result;
}

}  // namespace slicer_core
