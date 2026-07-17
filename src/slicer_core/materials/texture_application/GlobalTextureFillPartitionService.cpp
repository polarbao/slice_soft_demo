#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

double CeilToStep(const double value, const double step)
{
    return std::ceil(value / step - 1.0e-12) * step;
}

double RoundToStep(const double value, const double step)
{
    return std::floor(value / step + 0.5 + 1.0e-12) * step;
}

void AppendUniqueWidth(
    std::vector<double>& widths,
    const double width,
    const double epsilon)
{
    if (widths.empty() || std::abs(widths.back() - width) > epsilon)
    {
        widths.push_back(width);
    }
}

std::vector<double> BuildWidthSweepValues(
    const double minimumWidthMm,
    const double maximumWidthMm,
    const double widthStepMm,
    const TextureFillPartitionWidthSweepOptions& options,
    bool& sampleLimitExceeded)
{
    sampleLimitExceeded = false;
    std::vector<double> widths;
    if (options.maxSamples == 0U || options.representativeIntermediateCount < 0)
    {
        sampleLimitExceeded = true;
        return widths;
    }

    const double epsilon = std::max(widthStepMm * 1.0e-8, 1.0e-12);
    const double minimumRequest = CeilToStep(minimumWidthMm, widthStepMm);
    if (options.fullStepScan)
    {
        const double range = std::max(0.0, maximumWidthMm - minimumRequest);
        const std::size_t estimatedSamples = static_cast<std::size_t>(
            std::floor(range / widthStepMm + epsilon))
            + 1U;
        const bool needsEndpoint = minimumRequest
            + static_cast<double>(estimatedSamples - 1U) * widthStepMm
            < maximumWidthMm - epsilon;
        const std::size_t requiredSamples = estimatedSamples
            + (needsEndpoint ? 1U : 0U);
        if (requiredSamples > options.maxSamples)
        {
            sampleLimitExceeded = true;
            return widths;
        }
        for (std::size_t index{0U}; index < estimatedSamples; ++index)
        {
            AppendUniqueWidth(
                widths,
                minimumRequest + static_cast<double>(index) * widthStepMm,
                epsilon);
        }
        AppendUniqueWidth(widths, maximumWidthMm, epsilon);
        return widths;
    }

    const int intervalCount = options.representativeIntermediateCount + 1;
    for (int index{0}; index <= intervalCount; ++index)
    {
        double width{minimumRequest};
        if (index == intervalCount)
        {
            width = maximumWidthMm;
        }
        else if (index > 0)
        {
            const double ratio = static_cast<double>(index)
                / static_cast<double>(intervalCount);
            width = RoundToStep(
                minimumWidthMm
                    + (maximumWidthMm - minimumWidthMm) * ratio,
                widthStepMm);
            width = std::clamp(width, minimumRequest, maximumWidthMm);
        }
        AppendUniqueWidth(widths, width, epsilon);
    }
    if (widths.size() > options.maxSamples)
    {
        widths.clear();
        sampleLimitExceeded = true;
    }
    return widths;
}

void AppendSweepIssue(
    TextureFillPartitionWidthSweepResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.issues.push_back(MakePartitionIssue(code, message));
}

void ValidateWidthSweep(TextureFillPartitionWidthSweepResult& result)
{
    if (result.samples.empty())
    {
        AppendSweepIssue(
            result,
            TextureFillPartitionErrorCode::WidthSweepEmpty,
            "texture/fill width sweep contains no validated samples");
        return;
    }

    bool modelStable{true};
    bool textureMonotonic{true};
    bool fillMonotonic{true};
    bool partitionStable{true};
    const std::uint64_t expectedModel = result.samples.front().stats.modelVoxels;
    for (std::size_t index{0U}; index < result.samples.size(); ++index)
    {
        const TextureFillPartitionWidthSweepSample& sample =
            result.samples.at(index);
        modelStable = modelStable
            && sample.stats.modelVoxels == expectedModel;
        partitionStable = partitionStable
            && sample.partitionPass
            && sample.stats.overlapTextureFillVoxels == 0U
            && sample.stats.unassignedModelVoxels == 0U
            && sample.stats.textureSurfaceVoxels
                    + sample.stats.modelFillVoxels
                == sample.stats.modelVoxels;
        if (index > 0U)
        {
            const TextureFillPartitionWidthSweepSample& previous =
                result.samples.at(index - 1U);
            textureMonotonic = textureMonotonic
                && sample.stats.textureSurfaceVoxels
                    >= previous.stats.textureSurfaceVoxels;
            fillMonotonic = fillMonotonic
                && sample.stats.modelFillVoxels
                    <= previous.stats.modelFillVoxels;
        }
    }

    if (!modelStable)
    {
        AppendSweepIssue(
            result,
            TextureFillPartitionErrorCode::WidthSweepModelChanged,
            "model occupancy changed while scanning texture-shell width");
    }
    if (!textureMonotonic)
    {
        AppendSweepIssue(
            result,
            TextureFillPartitionErrorCode::WidthSweepTextureNonMonotonic,
            "texture-surface voxel count decreased while width increased");
    }
    if (!fillMonotonic)
    {
        AppendSweepIssue(
            result,
            TextureFillPartitionErrorCode::WidthSweepFillNonMonotonic,
            "model-fill voxel count increased while width increased");
    }

    const TextureFillPartitionWidthSweepSample& endpoint =
        result.samples.back();
    result.endpointPass = endpoint.allTexture
        && endpoint.partitionPass
        && endpoint.stats.textureSurfaceVoxels == endpoint.stats.modelVoxels
        && endpoint.stats.modelFillVoxels == 0U;
    if (!result.endpointPass)
    {
        AppendSweepIssue(
            result,
            TextureFillPartitionErrorCode::WidthSweepEndpointInvalid,
            "final width-sweep sample is not the all-texture endpoint");
    }
    result.monotonicPass = modelStable
        && textureMonotonic
        && fillMonotonic
        && partitionStable;
    result.status = result.monotonicPass && result.endpointPass
        ? "diagnostic"
        : "fail";
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

TextureFillPartitionWidthSweepResult
GlobalTextureFillPartitionService::EvaluateWidthSweep(
    const GlobalTextureFillPartitionRequest& request,
    const TextureFillPartitionWidthSweepOptions& options) const
{
    TextureFillPartitionWidthSweepResult sweep;
    const double classificationResolutionMm = std::max({
        request.grid.spacingXMm,
        request.grid.spacingYMm,
        request.grid.spacingZMm});
    GlobalTextureFillPartitionRequest discoveryRequest = request;
    discoveryRequest.options.requestedWidthMm = std::max({
        request.options.requestedWidthMm,
        request.options.baseMinimumWidthMm,
        2.0 * classificationResolutionMm});
    const GlobalTextureFillPartitionResult discovery = Evaluate(
        discoveryRequest);
    sweep.available = discovery.available;
    sweep.backend = discovery.backend;
    sweep.backendRole = discovery.backendRole;
    sweep.widthStepMm = request.options.widthStepMm;
    if (!discovery.available || !discovery.partitionPass)
    {
        sweep.status = discovery.available ? "fail" : "unavailable";
        sweep.issues = discovery.issues;
        AppendSweepIssue(
            sweep,
            TextureFillPartitionErrorCode::WidthSweepSampleFailed,
            "width-sweep discovery candidate is unavailable or invalid");
        return sweep;
    }

    sweep.minimumWidthMm = discovery.widthMetrics.effectiveMinimumWidthMm;
    sweep.maximumWidthMm = discovery.widthMetrics.allTextureThresholdMm;
    if (!std::isfinite(sweep.minimumWidthMm)
        || !std::isfinite(sweep.maximumWidthMm)
        || !std::isfinite(sweep.widthStepMm)
        || sweep.minimumWidthMm <= 0.0
        || sweep.maximumWidthMm < sweep.minimumWidthMm
        || sweep.widthStepMm <= 0.0)
    {
        sweep.status = "fail";
        AppendSweepIssue(
            sweep,
            TextureFillPartitionErrorCode::WidthSweepEndpointInvalid,
            "width-sweep discovery returned invalid dynamic bounds");
        return sweep;
    }

    bool sampleLimitExceeded{false};
    const std::vector<double> widths = BuildWidthSweepValues(
        sweep.minimumWidthMm,
        sweep.maximumWidthMm,
        sweep.widthStepMm,
        options,
        sampleLimitExceeded);
    if (sampleLimitExceeded)
    {
        sweep.status = "fail";
        AppendSweepIssue(
            sweep,
            TextureFillPartitionErrorCode::WidthSweepSampleFailed,
            "width-sweep sample count exceeds the configured safety limit");
        return sweep;
    }
    if (widths.empty())
    {
        sweep.status = "fail";
        AppendSweepIssue(
            sweep,
            TextureFillPartitionErrorCode::WidthSweepEmpty,
            "width-sweep quantization produced no representative widths");
        return sweep;
    }

    sweep.samples.reserve(widths.size());
    for (const double width : widths)
    {
        GlobalTextureFillPartitionRequest sampleRequest = request;
        sampleRequest.options.requestedWidthMm = width;
        const GlobalTextureFillPartitionResult sampleResult = Evaluate(
            sampleRequest);
        if (!sampleResult.available || !sampleResult.partitionPass)
        {
            sweep.status = "fail";
            sweep.issues.insert(
                sweep.issues.end(),
                sampleResult.issues.begin(),
                sampleResult.issues.end());
            AppendSweepIssue(
                sweep,
                TextureFillPartitionErrorCode::WidthSweepSampleFailed,
                "one width-sweep sample is unavailable or invalid");
            return sweep;
        }
        TextureFillPartitionWidthSweepSample sample;
        sample.requestedWidthMm = width;
        sample.effectiveWidthMm = sampleResult.widthMetrics.effectiveWidthMm;
        sample.allTexture = sampleResult.widthMetrics.allTexture;
        sample.partitionPass = sampleResult.partitionPass;
        sample.status = sampleResult.status;
        sample.stats = sampleResult.stats;
        sample.performance = sampleResult.performance;
        sweep.totalCandidateCoreMs += sample.performance.totalCoreMs;
        sweep.samples.push_back(sample);
    }
    ValidateWidthSweep(sweep);
    return sweep;
}

TextureFillPartitionConformanceResult CompareTextureFillPartitionResults(
    const GlobalTextureFillPartitionResult& cpuResult,
    const GlobalTextureFillPartitionResult& openVdbResult)
{
    TextureFillPartitionConformanceResult result;
    result.cpuAvailable = cpuResult.available;
    result.openVdbAvailable = openVdbResult.available;
    result.cpuPartitionInvariantPass = cpuResult.partitionPass;
    result.openVdbPartitionInvariantPass = openVdbResult.partitionPass;
    result.cpuStatus = cpuResult.status;
    result.openVdbStatus = openVdbResult.status;
    result.cpuBackendRole = cpuResult.backendRole;
    result.openVdbBackendRole = openVdbResult.backendRole;
    result.sameGrid = GridsMatch(cpuResult.grid, openVdbResult.grid);

    if (!cpuResult.available || !openVdbResult.available)
    {
        result.issues.insert(
            result.issues.end(),
            cpuResult.issues.begin(),
            cpuResult.issues.end());
        result.issues.insert(
            result.issues.end(),
            openVdbResult.issues.begin(),
            openVdbResult.issues.end());
        return result;
    }
    if (!result.sameGrid
        || !cpuResult.partitionPass
        || !openVdbResult.partitionPass
        || cpuResult.modelMask.values.size()
            != openVdbResult.modelMask.values.size()
        || cpuResult.textureSurfaceMask.values.size()
            != openVdbResult.textureSurfaceMask.values.size()
        || cpuResult.modelFillMask.values.size()
            != openVdbResult.modelFillMask.values.size())
    {
        result.conformanceStatus = "fail";
        result.issues.push_back(MakePartitionIssue(
            TextureFillPartitionErrorCode::BackendConformanceFailed,
            "CPU and OpenVDB partition results do not share valid structural invariants"));
        return result;
    }

    double totalDistanceDeltaMm{0.0};
    for (std::size_t index{0U};
         index < cpuResult.modelMask.values.size();
         ++index)
    {
        const bool cpuModel = cpuResult.modelMask.values.at(index) != 0U;
        const bool openVdbModel =
            openVdbResult.modelMask.values.at(index) != 0U;
        const bool cpuTexture =
            cpuResult.textureSurfaceMask.values.at(index) != 0U;
        const bool openVdbTexture =
            openVdbResult.textureSurfaceMask.values.at(index) != 0U;
        const bool cpuFill = cpuResult.modelFillMask.values.at(index) != 0U;
        const bool openVdbFill =
            openVdbResult.modelFillMask.values.at(index) != 0U;
        result.modelOnlyCpuVoxels += cpuModel && !openVdbModel ? 1U : 0U;
        result.modelOnlyOpenVdbVoxels += openVdbModel && !cpuModel ? 1U : 0U;
        result.textureOnlyCpuVoxels +=
            cpuTexture && !openVdbTexture ? 1U : 0U;
        result.textureOnlyOpenVdbVoxels +=
            openVdbTexture && !cpuTexture ? 1U : 0U;
        result.fillOnlyCpuVoxels += cpuFill && !openVdbFill ? 1U : 0U;
        result.fillOnlyOpenVdbVoxels += openVdbFill && !cpuFill ? 1U : 0U;

        if (index < cpuResult.closestSurfaceReferences.size()
            && index < openVdbResult.closestSurfaceReferences.size())
        {
            const TextureFillClosestSurfaceReference& cpuReference =
                cpuResult.closestSurfaceReferences.at(index);
            const TextureFillClosestSurfaceReference& openVdbReference =
                openVdbResult.closestSurfaceReferences.at(index);
            if (cpuReference.valid && openVdbReference.valid)
            {
                const double distanceDeltaMm = std::abs(
                    cpuReference.distanceMm - openVdbReference.distanceMm);
                result.maxDistanceDeltaMm = std::max(
                    result.maxDistanceDeltaMm,
                    distanceDeltaMm);
                totalDistanceDeltaMm += distanceDeltaMm;
                ++result.commonDistanceSamples;
            }
        }
    }
    if (result.commonDistanceSamples > 0U)
    {
        result.meanDistanceDeltaMm = totalDistanceDeltaMm
            / static_cast<double>(result.commonDistanceSamples);
    }
    result.allTextureThresholdDeltaMm = std::abs(
        cpuResult.widthMetrics.allTextureThresholdMm
        - openVdbResult.widthMetrics.allTextureThresholdMm);
    if (cpuResult.performance.totalCoreMs > 0.0)
    {
        result.openVdbToCpuCoreTimeRatio =
            openVdbResult.performance.totalCoreMs
            / cpuResult.performance.totalCoreMs;
    }
    if (cpuResult.performance.processPeakWorkingSetBytes > 0U)
    {
        result.openVdbToCpuPeakMemoryRatio = static_cast<double>(
            openVdbResult.performance.processPeakWorkingSetBytes)
            / static_cast<double>(
                cpuResult.performance.processPeakWorkingSetBytes);
    }
    result.conformanceStatus = "diagnostic";
    return result;
}

}  // namespace slicer_core
