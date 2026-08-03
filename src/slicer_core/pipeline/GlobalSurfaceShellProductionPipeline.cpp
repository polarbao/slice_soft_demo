#include "slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h"

#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
constexpr const char* kMaterialParityProfileTarget =
    "global_surface_shell_material_parity_candidate";
#ifdef NDEBUG
constexpr const char* kBuildType{"Release"};
#else
constexpr const char* kBuildType{"Debug"};
#endif
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
    if (config.outer_varnish.allow_xy_expansion
        && config.outer_varnish.enabled
        && config.outer_varnish.thickness_mm > 0.0)
    {
        const OuterVarnishDiscretization discretization =
            ComputeOuterVarnishDiscretization(
                config.outer_varnish,
                grid.pixelPitchXMm,
                grid.pixelPitchYMm);
        grid.originXMm -= static_cast<double>(
            discretization.radius_x_px) * grid.pixelPitchXMm;
        grid.originYMm -= static_cast<double>(
            discretization.radius_y_px) * grid.pixelPitchYMm;
        grid.width += 2 * discretization.radius_x_px;
        grid.height += 2 * discretization.radius_y_px;
    }
    return grid;
}

bool HasCommonGlobalMaterialContract(const SliceConfig& config)
{
    return config.texture.enabled
        && config.texture.apply_mode == "global_surface_shell"
        && config.texture.surface_shell.geometry_mode == "global_3d_distance"
        && config.texture.surface_shell.surface_scope == "all_closed_surfaces"
        && config.texture.surface_shell.full_texture_at_model_limit
        && config.model_fill.enabled
        && config.model_fill.material == "white"
        && config.model_fill.scope == "complement_of_global_texture_shell"
        && config.model_fill.value == 0U
        && !config.model_fill.empty_allowed_in_production
        && !config.model_fill.legacy_rgb_fallback
        && config.material_process_profile.rgb.enabled
        && config.material_process_profile.rgb.source == "texture_or_color"
        && config.material_process_profile.white.enabled
        && config.material_process_profile.white.value == 0U
        && config.material_process_profile.white.expand_px == 0
        && config.material_process_profile.white.shrink_px == 0;
}

bool HasRestrictedMaterialContract(const SliceConfig& config)
{
    return !config.support.enabled
        && !config.material_process_profile.support.expected
        && !config.material_process_profile.validation.require_support_pixels
        && !config.surface_varnish.enabled
        && !config.outer_varnish.enabled
        && config.outer_varnish.thickness_mm <= 0.0
        && !config.material_process_profile.varnish.enabled
        && !config.material_process_profile.validation.require_varnish_pixels;
}

bool HasMaterialParityContract(const SliceConfig& config)
{
    const bool supportContract = config.support.enabled
        && config.support.mode == "bottom_projection"
        && config.support.placement == "lower"
        && config.support.placement_explicit
        && config.support.value == 0U
        && config.support.offset_mm == 0.0
        && config.support.xy_dilation_px == 0
        && !config.support.shape_enabled
        && config.support.internal_void.enabled
        && !config.support.upper.enabled
        && config.material_process_profile.support.expected
        && config.material_process_profile.validation.require_support_pixels;
    const bool varnishRequested =
        config.surface_varnish.enabled || config.outer_varnish.enabled;
    const bool varnishContract = varnishRequested
        && config.material_process_profile.varnish.enabled
        && config.material_process_profile.varnish.value == 0U
        && config.material_process_profile.validation.require_varnish_pixels
        && (!config.surface_varnish.enabled
            || (config.surface_varnish.thickness_px > 0
                && config.surface_varnish.value == 0U
                && config.surface_varnish.source == "explicit"))
        && (!config.outer_varnish.enabled
            || (config.outer_varnish.thickness_mm > 0.0
                && config.outer_varnish.allow_xy_expansion
                && config.outer_varnish.value == 0U));
    return supportContract && varnishContract;
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
    request.outerVarnish = ComputeOuterVarnishDiscretization(
        config.outer_varnish,
        mapping.grid.pixelPitchXMm,
        mapping.grid.pixelPitchYMm);
    request.storage.storageMode = config.output.storage_mode;
    request.storage.compression = config.output.tiff_compression;
    request.storage.rowsPerStrip = config.output.rows_per_strip;
    request.storage.tileWidth = config.output.tile_size.at(0);
    request.storage.tileHeight = config.output.tile_size.at(1);
    request.preview.enabled =
        config.preview.enabled && options.write_preview_files;
    request.preview.outputpolicy =
        request.preview.enabled
            ? "tiff_native_with_diagnostics"
            : "tiff_native";
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
    if (!config.material_process_profile.enabled)
    {
        return BlockProfile(
            "global_surface_shell production requires an explicit admitted Profile target");
    }
    if (!HasCommonGlobalMaterialContract(config))
    {
        return BlockProfile(
            "global_surface_shell production requires the closed-surface RGB and non-empty white Model Fill contract");
    }

    const bool restrictedProfile =
        config.material_process_profile.target == kRestrictedProfileTarget;
    const bool materialParityProfile =
        config.material_process_profile.target == kMaterialParityProfileTarget;
    if (!restrictedProfile && !materialParityProfile)
    {
        return BlockProfile(
            "global_surface_shell production Profile target is not admitted");
    }
    if (restrictedProfile && !HasRestrictedMaterialContract(config))
    {
        return BlockProfile(
            "global_surface_shell support generation is not admitted by the restricted production Profile");
    }
    if (materialParityProfile && !HasMaterialParityContract(config))
    {
        return BlockProfile(
            "global_surface_shell material parity Profile requires lower support and explicit zero-value varnish");
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

    GlobalSurfaceShellMaterialEvidenceResult materialEvidence =
        ComposeGlobalSurfaceShellMaterialEvidence(mapping, config);
    if (!materialEvidence.available)
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::GlobalAdapterInputInvalid,
            materialEvidence.detail);
    }
    TextureFillPartitionFullClosureAdapterRequest closureRequest;
    closureRequest.rasterMapping = &mapping;
    closureRequest.layers = &materialEvidence.layers;
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
    adapterRequest.closureEvidence = &materialEvidence.layers;
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
    result.support_pixel_count = static_cast<int>(
        std::min<std::uint64_t>(
            materialEvidence.supportPixels,
            static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
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
