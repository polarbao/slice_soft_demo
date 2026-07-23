#include "slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h"

#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

constexpr const char* kRestrictedProfileTarget =
    "global_surface_shell_restricted_candidate";
#ifdef NDEBUG
constexpr const char* kBuildType{"Release"};
#else
constexpr const char* kBuildType{"Debug"};
#endif
constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kWhiteChannel{3U};

double ElapsedMilliseconds(const Clock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(
        Clock::now() - start).count();
}

GlobalSurfaceShellProductionProfileDecision BlockProfile(
    const std::string& detail)
{
    GlobalSurfaceShellProductionProfileDecision decision;
    decision.errorCode = SlicePipelineErrorCode::GlobalNotAdmitted;
    decision.detail = detail;
    return decision;
}

double PixelPitchMm(const int dpi)
{
    return 25.4 / static_cast<double>(dpi);
}

double ClassificationResolutionMm(const SliceConfig& config)
{
    return std::max({
        PixelPitchMm(config.output.dpi_x),
        PixelPitchMm(config.output.dpi_y),
        config.output.layer_thickness_mm});
}

int RasterAxisCount(
    const int sourceCount,
    const double sourceSpacing,
    const double destinationSpacing)
{
    const double extent =
        static_cast<double>(sourceCount) * sourceSpacing;
    const double count = std::ceil(extent / destinationSpacing - 1.0e-12);
    if (!std::isfinite(count)
        || count <= 0.0
        || count > static_cast<double>(std::numeric_limits<int>::max()))
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::GlobalNotAdmitted,
            "global_surface_shell destination raster dimensions are invalid");
    }
    return static_cast<int>(count);
}

TextureFillPartitionRasterGridSpec BuildProductionRasterGrid(
    const GlobalTextureFillPartitionResult& partition,
    const SliceConfig& config)
{
    TextureFillPartitionRasterGridSpec grid;
    grid.originXMm = partition.grid.originXMm;
    grid.originYMm = partition.grid.originYMm;
    grid.originZMm = partition.grid.originZMm;
    grid.pixelPitchXMm = PixelPitchMm(config.output.dpi_x);
    grid.pixelPitchYMm = PixelPitchMm(config.output.dpi_y);
    grid.layerThicknessMm = config.output.layer_thickness_mm;
    grid.width = RasterAxisCount(
        partition.grid.width,
        partition.grid.spacingXMm,
        grid.pixelPitchXMm);
    grid.height = RasterAxisCount(
        partition.grid.height,
        partition.grid.spacingYMm,
        grid.pixelPitchYMm);
    grid.depth = RasterAxisCount(
        partition.grid.depth,
        partition.grid.spacingZMm,
        grid.layerThicknessMm);
    return grid;
}

std::vector<TextureFillPartitionFullClosureLayerEvidence>
BuildClosureEvidence(
    const TextureFillPartitionRasterMappingResult& mapping,
    const std::uint8_t modelFillValue)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(mapping.grid.width)
        * static_cast<std::size_t>(mapping.grid.height);
    std::vector<TextureFillPartitionFullClosureLayerEvidence> evidenceLayers;
    evidenceLayers.reserve(mapping.layers.size());

    for (const TextureFillPartitionRasterLayer& layer : mapping.layers)
    {
        TextureFillPartitionFullClosureLayerEvidence evidence;
        evidence.layerIndex = layer.layerIndex;
        evidence.zMm = layer.zMm;
        evidence.widthPx = mapping.grid.width;
        evidence.heightPx = mapping.grid.height;
        evidence.supportFillMask.assign(pixelCount, 0U);
        evidence.internalVoidSupportMask.assign(pixelCount, 0U);
        evidence.surfaceVarnishMask.assign(pixelCount, 0U);
        evidence.outerVarnishShellMask.assign(pixelCount, 0U);
        evidence.modelEnvelopeMask = layer.modelMask;
        evidence.supportRequiredMask.assign(pixelCount, 0U);
        evidence.channels.assign(pixelCount * kChannelCount, 255U);

        for (std::size_t pixelIndex{0U};
             pixelIndex < pixelCount;
             ++pixelIndex)
        {
            const std::size_t channelOffset = pixelIndex * kChannelCount;
            if (layer.textureSurfaceMask.at(pixelIndex) != 0U)
            {
                const std::array<std::uint8_t, 3>& rgb =
                    layer.textureRgb.at(pixelIndex);
                evidence.channels.at(channelOffset) = rgb.at(0);
                evidence.channels.at(channelOffset + 1U) = rgb.at(1);
                evidence.channels.at(channelOffset + 2U) = rgb.at(2);
                if (rgb == std::array<std::uint8_t, 3>{255U, 255U, 255U})
                {
                    evidence.channels.at(channelOffset + kWhiteChannel) =
                        modelFillValue;
                }
            }
            else if (layer.modelFillMask.at(pixelIndex) != 0U)
            {
                evidence.channels.at(channelOffset + kWhiteChannel) =
                    modelFillValue;
            }
        }
        evidenceLayers.push_back(std::move(evidence));
    }
    return evidenceLayers;
}

RgbwsvProductionPackageWriteRequest BuildPackageRequest(
    const SliceConfig& config,
    const std::filesystem::path& configPath,
    const SceneModel& scene,
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceRunOptions& options)
{
    RgbwsvProductionPackageWriteRequest request;
    request.packageDir = config.output.package_dir;
    request.sourceConfigPath = configPath;
    request.sourceModelPath = scene.model_path;
    request.sourceFormat = scene.format;
    request.requestedPipelineMode = "global_surface_shell";
    request.effectivePipelineMode = "global_surface_shell";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = mapping.grid.width;
    request.grid.heightPx = mapping.grid.height;
    request.grid.layerCount = mapping.grid.depth;
    request.grid.dpiX = config.output.dpi_x;
    request.grid.dpiY = config.output.dpi_y;
    request.grid.pixelSizeXmm = mapping.grid.pixelPitchXMm;
    request.grid.pixelSizeYmm = mapping.grid.pixelPitchYMm;
    request.grid.layerThicknessMm = mapping.grid.layerThicknessMm;
    request.grid.originXmm = mapping.grid.originXMm;
    request.grid.originYmm = mapping.grid.originYMm;
    request.grid.originZmm = mapping.grid.originZMm;
    request.storage.storageMode = config.output.storage_mode;
    request.storage.rowsPerStrip = config.output.rows_per_strip;
    request.storage.tileWidth = config.output.tile_size.at(0);
    request.storage.tileHeight = config.output.tile_size.at(1);
    request.preview.enabled =
        config.preview.enabled && options.write_preview_files;
    request.preview.format = config.preview.format;
    request.preview.interval = config.preview.interval;
    return request;
}

void RequireBenchmarkEvidence(
    const TextureFillPartitionReleaseBenchmarkResult& benchmark)
{
    if (!benchmark.partition.partitionPass
        || !benchmark.textureTransfer.available)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::GlobalNotAdmitted,
            "global_surface_shell partition or texture transfer failed");
    }
}

}  // namespace

GlobalSurfaceShellProductionProfileDecision
EvaluateGlobalSurfaceShellProductionProfile(const SliceConfig& config)
{
    if (config.slice_pipeline.mode != SlicePipelineMode::GlobalSurfaceShell
        || !config.slice_pipeline.explicitly_configured)
    {
        return BlockProfile(
            "global_surface_shell production requires an explicit slicePipeline.mode");
    }
    if (!config.material_process_profile.enabled
        || config.material_process_profile.target != kRestrictedProfileTarget)
    {
        return BlockProfile(
            "global_surface_shell production requires the explicit restricted candidate Profile target");
    }
    if (!config.texture.enabled
        || config.texture.apply_mode != "global_surface_shell"
        || config.texture.surface_shell.geometry_mode != "global_3d_distance"
        || config.texture.surface_shell.surface_scope != "all_closed_surfaces"
        || !config.texture.surface_shell.full_texture_at_model_limit)
    {
        return BlockProfile(
            "global_surface_shell restricted production requires the global closed-surface texture contract");
    }
    if (!config.model_fill.enabled
        || config.model_fill.material != "white"
        || config.model_fill.scope != "complement_of_global_texture_shell"
        || config.model_fill.value != 0U
        || config.model_fill.empty_allowed_in_production
        || config.model_fill.legacy_rgb_fallback
        || !config.material_process_profile.rgb.enabled
        || config.material_process_profile.rgb.source != "texture_or_color"
        || !config.material_process_profile.white.enabled
        || config.material_process_profile.white.value != 0U
        || config.material_process_profile.white.expand_px != 0
        || config.material_process_profile.white.shrink_px != 0)
    {
        return BlockProfile(
            "global_surface_shell restricted production currently supports non-empty white Model Fill only");
    }
    if (config.support.enabled
        || config.material_process_profile.support.expected
        || config.material_process_profile.validation.require_support_pixels)
    {
        return BlockProfile(
            "global_surface_shell support generation is not admitted by the restricted production Profile");
    }
    if (config.surface_varnish.enabled
        || config.outer_varnish.enabled
        || config.outer_varnish.thickness_mm > 0.0
        || config.material_process_profile.varnish.enabled
        || config.material_process_profile.validation.require_varnish_pixels)
    {
        return BlockProfile(
            "global_surface_shell varnish generation is not admitted by the restricted production Profile");
    }
    if (config.material_policy.enabled
        || config.material_role_mapping.enabled)
    {
        return BlockProfile(
            "global_surface_shell restricted production does not admit legacy material policy or role mapping");
    }
    if (config.material_closure.repair.enabled)
    {
        return BlockProfile(
            "global_surface_shell restricted production requires material closure repair disabled");
    }

    const double resolutionMm = ClassificationResolutionMm(config);
    const double minimumWidthMm = std::max(0.10, 2.0 * resolutionMm);
    if (config.texture.surface_shell.width_mm + 1.0e-9
        < minimumWidthMm)
    {
        return BlockProfile(
            "global_surface_shell widthMm is below the effective two-cell minimum");
    }

    GlobalSurfaceShellProductionProfileDecision decision;
    decision.allowed = true;
    decision.productionAcceptance = "admitted";
    return decision;
}

SliceRunResult RunGlobalSurfaceShellProductionPipeline(
    const std::filesystem::path& configPath,
    const SliceRunOptions& options)
{
    const Clock::time_point runStart = Clock::now();
    const Clock::time_point configStart = Clock::now();
    const SliceConfig config = load_slice_config(configPath);
    const double configLoadMs = ElapsedMilliseconds(configStart);
    const GlobalSurfaceShellProductionProfileDecision profileDecision =
        EvaluateGlobalSurfaceShellProductionProfile(config);
    if (!profileDecision.allowed)
    {
        throw SlicePipelineError(
            profileDecision.errorCode,
            profileDecision.detail);
    }
    if (!options.write_tiff_layers)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::ProductionTiffRequired,
            "global_surface_shell production cannot disable TIFF output");
    }

    const std::filesystem::path configDirectory =
        configPath.parent_path().empty()
        ? std::filesystem::current_path()
        : configPath.parent_path();
    const Clock::time_point modelStart = Clock::now();
    const SceneModel scene = load_model_report(config, configDirectory);
    const AdaptedTriangleMesh adapted =
        AdaptSceneModelToTriangleMesh(scene);
    const double modelLoadMs = ElapsedMilliseconds(modelStart);

    TextureFillPartitionReleaseBenchmarkRequest benchmarkRequest;
    benchmarkRequest.mesh = &adapted.mesh;
    benchmarkRequest.adaptedMesh = &adapted;
    benchmarkRequest.caseName =
        config.material_process_profile.name;
    benchmarkRequest.configPath = configPath.generic_string();
    benchmarkRequest.modelPath = scene.model_path.generic_string();
    benchmarkRequest.buildType = kBuildType;
    benchmarkRequest.voxelMm = ClassificationResolutionMm(config);
    benchmarkRequest.widthMm = config.texture.surface_shell.width_mm;
    benchmarkRequest.paddingVoxels = 1;
    benchmarkRequest.configLoadMs = configLoadMs;
    benchmarkRequest.modelLoadMs = modelLoadMs;
    benchmarkRequest.sourceTriangles = adapted.topology.source_triangles;
    benchmarkRequest.acceptedTriangles = adapted.topology.accepted_triangles;
    benchmarkRequest.degenerateTriangles = adapted.topology.degenerate_triangles;
    benchmarkRequest.boundaryEdges = adapted.topology.boundary_edges;
    benchmarkRequest.nonManifoldEdges = adapted.topology.non_manifold_edges;
    benchmarkRequest.textureSample.sampler = config.texture.sampler;
    benchmarkRequest.textureSample.uv_address_mode =
        config.texture.uv_address_mode;
    benchmarkRequest.textureSample.flip_v = config.texture.flip_v;
    benchmarkRequest.fallbackRgb = config.texture.fallback_rgb;
    benchmarkRequest.missingTexturePolicy =
        config.texture.missing_texture_policy;

    const Clock::time_point sliceStart = Clock::now();
    const TextureFillPartitionReleaseBenchmarkResult benchmark =
        RunTextureFillPartitionReleaseBenchmark(benchmarkRequest);
    RequireBenchmarkEvidence(benchmark);

    TextureFillPartitionRasterMappingRequest rasterRequest;
    rasterRequest.partition = &benchmark.partition;
    rasterRequest.transfer = &benchmark.textureTransfer;
    rasterRequest.rasterGrid =
        BuildProductionRasterGrid(benchmark.partition, config);
    const TextureFillPartitionRasterMappingResult mapping =
        MapTextureFillPartitionToRaster(rasterRequest);
    if (!mapping.available || !mapping.stats.partitionPass)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::GlobalNotAdmitted,
            "global_surface_shell production raster mapping failed");
    }

    std::vector<TextureFillPartitionFullClosureLayerEvidence>
        closureEvidence = BuildClosureEvidence(
            mapping,
            config.model_fill.value);
    TextureFillPartitionFullClosureAdapterRequest closureRequest;
    closureRequest.rasterMapping = &mapping;
    closureRequest.layers = &closureEvidence;
    const TextureFillPartitionFullClosureAdapterResult closure =
        AdaptTextureFillPartitionFullClosure(closureRequest);
    if (!closure.fullClosurePass)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::GlobalAdapterClosureRequired,
            "global_surface_shell full material closure did not pass");
    }

    GlobalSurfaceShellProductionLayerAdapterRequest adapterRequest;
    adapterRequest.rasterMapping = &mapping;
    adapterRequest.fullClosure = &closure;
    adapterRequest.closureEvidence = &closureEvidence;
    GlobalSurfaceShellProductionLayerAdapterResult adapter =
        AdaptGlobalSurfaceShellProductionLayers(adapterRequest);
    if (!adapter.available)
    {
        throw SlicePipelineError(adapter.errorCode, adapter.detail);
    }
    const double sliceProcessingMs = ElapsedMilliseconds(sliceStart);

    const Clock::time_point outputStart = Clock::now();
    const RgbwsvProductionPackageWriteResult package =
        WriteGlobalSurfaceShellProductionPackage(
            BuildPackageRequest(
                config,
                configPath,
                scene,
                mapping,
                options),
            std::move(adapter));
    const double outputWriteMs = ElapsedMilliseconds(outputStart);
    if (!package.productionOutputWritten)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::ProductionTiffRequired,
            "global_surface_shell writer returned no production TIFF");
    }

    SliceRunResult result;
    result.package_dir = package.packageDir;
    result.effective_pipeline_mode = "global_surface_shell";
    result.width_px = mapping.grid.width;
    result.height_px = mapping.grid.height;
    result.layer_count = mapping.grid.depth;
    result.model_pixel_count = static_cast<int>(std::min<std::uint64_t>(
        mapping.stats.modelRasterVoxels,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
    result.support_pixel_count = 0;
    result.profile.available = true;
    result.profile.profile_level = "coarse";
    result.profile.config_load_ms = configLoadMs;
    result.profile.model_load_ms = modelLoadMs;
    result.profile.slice_processing_ms = sliceProcessingMs;
    result.profile.layer_compute_ms = sliceProcessingMs;
    result.profile.output_write_ms = outputWriteMs;
    result.profile.total_ms = ElapsedMilliseconds(runStart);
    return result;
}

}  // namespace slicer_core
