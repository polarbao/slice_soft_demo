#include "slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.h"

#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/material/MaterialChannelComposer.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"
#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr const char* kReportSchema =
    "slicesoft.texture_fill_positive_matrix.12e_08c_r4.1";

ValidationIssue MakeMatrixIssue(
    const std::string& code,
    const std::string& message)
{
    return MakeValidationIssue(code, ValidationSeverity::Error, message);
}

Json BuildGridJson(const TextureFillPartitionGridSpec& grid)
{
    return Json::object({
        {"width", grid.width},
        {"height", grid.height},
        {"depth", grid.depth},
        {"originXMm", grid.originXMm},
        {"originYMm", grid.originYMm},
        {"originZMm", grid.originZMm},
        {"spacingXMm", grid.spacingXMm},
        {"spacingYMm", grid.spacingYMm},
        {"spacingZMm", grid.spacingZMm},
        {"classificationResolutionMm",
         std::max({grid.spacingXMm, grid.spacingYMm, grid.spacingZMm})},
    });
}

Json BuildStatsJson(const TextureFillPartitionStats& stats)
{
    return Json::object({
        {"modelVoxels", stats.modelVoxels},
        {"textureSurfaceVoxels", stats.textureSurfaceVoxels},
        {"modelFillVoxels", stats.modelFillVoxels},
        {"overlapTextureFillVoxels", stats.overlapTextureFillVoxels},
        {"unassignedModelVoxels", stats.unassignedModelVoxels},
        {"textureOutsideModelVoxels", stats.textureOutsideModelVoxels},
        {"modelFillOutsideModelVoxels", stats.modelFillOutsideModelVoxels},
    });
}

Json BuildWidthSamplesJson(
    const TextureFillPartitionWidthSweepResult& sweep)
{
    Json::Array samples;
    samples.reserve(sweep.samples.size());
    for (const TextureFillPartitionWidthSweepSample& sample : sweep.samples)
    {
        const bool invariantPass = sample.partitionPass
            && sample.stats.overlapTextureFillVoxels == 0U
            && sample.stats.unassignedModelVoxels == 0U
            && sample.stats.textureOutsideModelVoxels == 0U
            && sample.stats.modelFillOutsideModelVoxels == 0U
            && sample.stats.textureSurfaceVoxels
                + sample.stats.modelFillVoxels
                == sample.stats.modelVoxels;
        samples.push_back(Json::object({
            {"requestedWidthMm", sample.requestedWidthMm},
            {"effectiveWidthMm", sample.effectiveWidthMm},
            {"allTexture", sample.allTexture},
            {"partitionPass", sample.partitionPass},
            {"invariantPass", invariantPass},
            {"stats", BuildStatsJson(sample.stats)},
        }));
    }
    return Json{std::move(samples)};
}

Json BuildRequestedWidthAnchorsJson(const std::vector<double>& anchors)
{
    Json::Array values;
    values.reserve(anchors.size());
    for (const double anchor : anchors)
    {
        values.push_back(anchor);
    }
    return Json{std::move(values)};
}

Json BuildChannelsJson(const std::array<std::uint8_t, 6>& channels)
{
    return Json::object({
        {"R", static_cast<int>(channels.at(0U))},
        {"G", static_cast<int>(channels.at(1U))},
        {"B", static_cast<int>(channels.at(2U))},
        {"W", static_cast<int>(channels.at(3U))},
        {"S", static_cast<int>(channels.at(4U))},
        {"V", static_cast<int>(channels.at(5U))},
    });
}

Json BuildPrintVoxelsJson(const std::array<std::uint64_t, 6>& counts)
{
    return Json::object({
        {"R", counts.at(0U)},
        {"G", counts.at(1U)},
        {"B", counts.at(2U)},
        {"W", counts.at(3U)},
        {"S", counts.at(4U)},
        {"V", counts.at(5U)},
    });
}

Json BuildMaterialCasesJson(
    const std::vector<TextureFillPartitionPositiveMaterialCase>& cases)
{
    Json::Array items;
    items.reserve(cases.size());
    for (const TextureFillPartitionPositiveMaterialCase& item : cases)
    {
        items.push_back(Json::object({
            {"requestedMaterial", item.resolution.requestedMaterial},
            {"requestedRole", item.resolution.requestedRole},
            {"available", item.resolution.available},
            {"resolvedMaterial", item.resolution.resolvedMaterial},
            {"resolvedChannels",
             BuildChannelsJson(item.resolution.resolvedChannels)},
            {"profileId", item.resolution.profileId},
            {"reasonCode",
             item.reasonCode.empty()
                 ? item.resolution.reasonCode
                 : item.reasonCode},
            {"compositionEvaluated", item.compositionEvaluated},
            {"compositionPass", item.compositionPass},
            {"modelFillVoxels", item.modelFillVoxels},
            {"printVoxels", BuildPrintVoxelsJson(item.printVoxels)},
        }));
    }
    return Json{std::move(items)};
}

bool AllWidthSamplesPass(
    const TextureFillPartitionWidthSweepResult& sweep)
{
    return !sweep.samples.empty()
        && std::all_of(
            sweep.samples.begin(),
            sweep.samples.end(),
            [](const TextureFillPartitionWidthSweepSample& sample)
            {
                return sample.partitionPass
                    && sample.stats.overlapTextureFillVoxels == 0U
                    && sample.stats.unassignedModelVoxels == 0U
                    && sample.stats.textureOutsideModelVoxels == 0U
                    && sample.stats.modelFillOutsideModelVoxels == 0U
                    && sample.stats.textureSurfaceVoxels
                        + sample.stats.modelFillVoxels
                        == sample.stats.modelVoxels;
            });
}

Json BuildReport(
    const TextureFillPartitionPositiveMatrixRequest& request,
    const TextureFillPartitionPositiveMatrixResult& result)
{
    return Json::object({
        {"schema", kReportSchema},
        {"diagnosticOnly", true},
        {"productionOutputWritten", result.productionOutputWritten},
        {"requiredRepairPassCount", result.requiredRepairPassCount},
        {"input",
         Json::object({
             {"caseId", request.caseId},
             {"modelPath", request.modelPath},
             {"sourceHash", request.sourceHash},
             {"resourceHash", request.resourceHash},
             {"preflightStatus", request.preflightStatus},
         })},
        {"grid", BuildGridJson(result.grid)},
        {"widthSweep",
         Json::object({
             {"status", result.widthSweep.status},
             {"minimumWidthMm", result.widthSweep.minimumWidthMm},
             {"maximumWidthMm", result.widthSweep.maximumWidthMm},
             {"widthStepMm", result.widthSweep.widthStepMm},
             {"sampleCount",
              static_cast<std::uint64_t>(result.widthSweep.samples.size())},
             {"requestedRepresentativeCount", 3},
             {"representativeIntermediateCount", 1},
             {"requestedAnchorWidthsMm",
              BuildRequestedWidthAnchorsJson(
                  result.widthSweep.requestedAnchorWidthsMm)},
             {"deduplicated",
              result.widthSweep.samples.size()
                  < result.widthSweep.requestedAnchorWidthsMm.size()},
             {"monotonicPass", result.widthSweep.monotonicPass},
             {"endpointPass", result.widthSweep.endpointPass},
             {"samples", BuildWidthSamplesJson(result.widthSweep)},
         })},
        {"materialSampleWidthMm", result.materialSampleWidthMm},
        {"textureTransfer",
         Json::object({
             {"available", result.textureTransfer.available},
             {"status", result.textureTransfer.status},
             {"textureSurfaceVoxels",
              result.textureTransfer.stats.textureSurfaceVoxels},
             {"sampledTextureCount",
              result.textureTransfer.stats.sampledTextureCount},
             {"materialDiffuseCount",
              result.textureTransfer.stats.materialDiffuseCount},
             {"fallbackCount", result.textureTransfer.stats.fallbackCount},
         })},
        {"materialCases", BuildMaterialCasesJson(result.materialCases)},
        {"summary",
         Json::object({
             {"complementPass",
              AllWidthSamplesPass(result.widthSweep)},
             {"monotonicPass", result.widthSweep.monotonicPass},
             {"endpointPass", result.widthSweep.endpointPass},
             {"materialResolutionPass",
              std::all_of(
                  result.materialCases.begin(),
                  result.materialCases.end(),
                  [](const TextureFillPartitionPositiveMaterialCase& item)
                  {
                      return item.resolution.available
                          ? item.compositionPass
                          : !item.resolution.reasonCode.empty();
                  })},
             {"matrixPass", result.matrixPass},
         })},
        {"issues", ValidationIssuesToJson(result.issues)},
    });
}

std::optional<double> SelectMaterialWidth(
    const TextureFillPartitionWidthSweepResult& sweep)
{
    if (sweep.samples.size() > 1U)
    {
        for (std::size_t index{1U}; index < sweep.samples.size(); ++index)
        {
            const TextureFillPartitionWidthSweepSample& sample =
                sweep.samples.at(index);
            if (!sample.allTexture && sample.stats.modelFillVoxels > 0U)
            {
                return sample.requestedWidthMm;
            }
        }
    }
    for (const TextureFillPartitionWidthSweepSample& sample : sweep.samples)
    {
        if (!sample.allTexture && sample.stats.modelFillVoxels > 0U)
        {
            return sample.requestedWidthMm;
        }
    }
    if (!sweep.samples.empty())
    {
        return sweep.samples.front().requestedWidthMm;
    }
    return std::nullopt;
}

std::vector<ModelFillMaterialResolveRequest> BuildMaterialRequests(
    const TextureFillPartitionPositiveMatrixRequest& request)
{
    std::vector<ModelFillMaterialResolveRequest> requests;
    const auto append = [&](const std::string& material, const std::string& role)
    {
        ModelFillMaterialResolveRequest item;
        item.requestedMaterial = material;
        item.requestedRole = role;
        item.customRgb = request.modelFillRgb;
        item.value = request.modelFillValue;
        item.profile = request.profile;
        item.roleRegistry = request.roleRegistry;
        requests.push_back(std::move(item));
    };
    append("white", {});
    append("varnish", {});
    append("rgb", {});
    append("profile_default", {});
    for (const std::string& role : request.requestedRoles)
    {
        append("material_role", role);
    }
    return requests;
}

std::array<std::uint8_t, 6> ReadChannels(
    const MaterialChannelComposerResult& composed,
    const std::size_t pixelIndex)
{
    std::array<std::uint8_t, 6> channels{};
    const std::size_t base = pixelIndex
        * static_cast<std::size_t>(MaterialChannelCount());
    for (std::size_t channel{0U}; channel < channels.size(); ++channel)
    {
        channels.at(channel) = composed.channels.at(base + channel);
    }
    return channels;
}

TextureFillPartitionPositiveMaterialCase EvaluateMaterialCase(
    const ModelFillMaterialResolveRequest& materialRequest,
    const GlobalTextureFillPartitionResult& partition,
    const TextureFillPartitionTextureTransferResult& transfer)
{
    TextureFillPartitionPositiveMaterialCase evidence;
    evidence.resolution = ResolveModelFillMaterial(materialRequest);
    if (!evidence.resolution.available)
    {
        evidence.compositionPass = !evidence.resolution.reasonCode.empty();
        return evidence;
    }

    TextureFillPartitionDiagnosticComposerRequest composerRequest;
    composerRequest.partition = &partition;
    composerRequest.transfer = &transfer;
    composerRequest.modelFillMaterial = evidence.resolution.resolvedMaterial;
    composerRequest.modelFillValue = evidence.resolution.resolvedValue;
    composerRequest.modelFillRgb = evidence.resolution.resolvedRgb;
    const TextureFillPartitionDiagnosticComposerResult composed =
        ComposeTextureFillPartitionDiagnostic(composerRequest);
    evidence.compositionEvaluated = true;
    if (!composed.available || composed.status != "diagnostic")
    {
        evidence.reasonCode = "E_12E_MODEL_FILL_COMPOSITION_BLOCKED";
        return evidence;
    }

    bool channelsMatch{true};
    for (const TextureFillPartitionDiagnosticLayer& layer : composed.layers)
    {
        for (std::size_t pixelIndex{0U};
             pixelIndex < layer.modelFillMask.size();
             ++pixelIndex)
        {
            if (layer.modelFillMask.at(pixelIndex) == 0U)
            {
                continue;
            }
            ++evidence.modelFillVoxels;
            const std::array<std::uint8_t, 6> channels =
                ReadChannels(layer.composed, pixelIndex);
            channelsMatch = channelsMatch
                && channels == evidence.resolution.resolvedChannels;
            for (std::size_t channel{0U}; channel < channels.size(); ++channel)
            {
                if (channels.at(channel) != 255U)
                {
                    ++evidence.printVoxels.at(channel);
                }
            }
        }
    }
    evidence.compositionPass = channelsMatch
        && evidence.modelFillVoxels == partition.stats.modelFillVoxels
        && evidence.printVoxels.at(
               static_cast<std::size_t>(MaterialChannelOffset::S))
            == 0U;
    if (!evidence.compositionPass)
    {
        evidence.reasonCode = "E_12E_MODEL_FILL_CHANNEL_MISMATCH";
    }
    return evidence;
}

}  // namespace

TextureFillPartitionPositiveMatrixResult RunTextureFillPartitionPositiveMatrix(
    const TextureFillPartitionPositiveMatrixRequest& request)
{
    TextureFillPartitionPositiveMatrixResult result;
    result.evidenceCollected = true;
    if (request.preflight == nullptr
        || request.preflight->status != ModelPreflightStatus::Passed
        || request.preflightStatus != "passed"
        || request.sourceHash.empty()
        || request.resourceHash.empty()
        || request.preflight->identity.sourceHash != request.sourceHash
        || request.preflight->identity.resourceHash != request.resourceHash)
    {
        result.issues.push_back(MakeMatrixIssue(
            "E_12E_POSITIVE_MATRIX_PREFLIGHT_REQUIRED",
            "positive matrix requires matching complete passed preflight evidence"));
        result.report = BuildReport(request, result);
        return result;
    }
    if (request.adaptedMesh == nullptr)
    {
        result.issues.push_back(MakeMatrixIssue(
            "E_12E_POSITIVE_MATRIX_INPUT_INVALID",
            "positive matrix requires an adapted mesh"));
        result.report = BuildReport(request, result);
        return result;
    }

    try
    {
        result.grid = BuildTextureFillPartitionBenchmarkGrid(
            request.adaptedMesh->mesh.bbox_mm,
            request.voxelMm,
            request.paddingVoxels);

        LegacyCpuGlobalDistanceBackend backend;
        const GlobalTextureFillPartitionService service(&backend);
        GlobalTextureFillPartitionRequest partitionRequest;
        partitionRequest.mesh = &request.adaptedMesh->mesh;
        partitionRequest.grid = result.grid;
        partitionRequest.options.requestedWidthMm = 0.10;
        partitionRequest.options.widthStepMm = 0.01;
        partitionRequest.options.baseMinimumWidthMm = 0.10;
        partitionRequest.options.surfaceScope = "all_closed_surfaces";

        TextureFillPartitionWidthSweepOptions sweepOptions;
        sweepOptions.representativeIntermediateCount = 1;
        result.widthSweep = service.EvaluateWidthSweep(
            partitionRequest,
            sweepOptions);
        result.issues.insert(
            result.issues.end(),
            result.widthSweep.issues.begin(),
            result.widthSweep.issues.end());
        if (!result.widthSweep.monotonicPass
            || !result.widthSweep.endpointPass)
        {
            result.report = BuildReport(request, result);
            return result;
        }

        const std::optional<double> materialWidth =
            SelectMaterialWidth(result.widthSweep);
        if (!materialWidth.has_value())
        {
            result.issues.push_back(MakeMatrixIssue(
                "E_12E_POSITIVE_MATRIX_FILL_SAMPLE_MISSING",
                "positive matrix has no valid width sample"));
            result.report = BuildReport(request, result);
            return result;
        }
        result.materialSampleWidthMm = *materialWidth;
        partitionRequest.options.requestedWidthMm = *materialWidth;
        const GlobalTextureFillPartitionResult partition =
            service.Evaluate(partitionRequest);
        if (!partition.partitionPass)
        {
            result.issues.insert(
                result.issues.end(),
                partition.issues.begin(),
                partition.issues.end());
            result.report = BuildReport(request, result);
            return result;
        }

        TextureFillPartitionTextureTransferRequest transferRequest;
        transferRequest.adaptedMesh = request.adaptedMesh;
        transferRequest.partition = &partition;
        transferRequest.textureSample = request.textureSample;
        transferRequest.fallbackRgb = request.fallbackRgb;
        transferRequest.missingTexturePolicy =
            request.missingTexturePolicy;
        result.textureTransfer = TransferTextureFillPartition(
            transferRequest);
        result.issues.insert(
            result.issues.end(),
            result.textureTransfer.issues.begin(),
            result.textureTransfer.issues.end());
        if (!result.textureTransfer.available)
        {
            result.report = BuildReport(request, result);
            return result;
        }

        const std::vector<ModelFillMaterialResolveRequest> materialRequests =
            BuildMaterialRequests(request);
        result.materialCases.reserve(materialRequests.size());
        for (const ModelFillMaterialResolveRequest& materialRequest :
             materialRequests)
        {
            result.materialCases.push_back(EvaluateMaterialCase(
                materialRequest,
                partition,
                result.textureTransfer));
        }
        const bool materialPass = std::all_of(
            result.materialCases.begin(),
            result.materialCases.end(),
            [](const TextureFillPartitionPositiveMaterialCase& item)
            {
                return item.resolution.available
                    ? item.compositionPass
                    : !item.resolution.reasonCode.empty();
            });
        result.matrixPass = result.widthSweep.monotonicPass
            && result.widthSweep.endpointPass
            && AllWidthSamplesPass(result.widthSweep)
            && materialPass
            && result.textureTransfer.available;
    }
    catch (const std::exception& error)
    {
        result.issues.push_back(MakeMatrixIssue(
            "E_12E_POSITIVE_MATRIX_EXECUTION_FAILED",
            error.what()));
    }

    result.report = BuildReport(request, result);
    return result;
}

}  // namespace slicer_core
