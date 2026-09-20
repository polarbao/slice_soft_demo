#include "slicer_core/slicer.h"
#include "slicer_core/system/Utf8Path.h"
#include "slicer_core/diagnostics/MaterialClosureCandidateDetector.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/LayerOccupancyProvider.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/geometry/SliceMaskRasterizer.h"
#include "slicer_core/geometry/ReliefHeightfieldSampler.h"
#include "slicer_core/geometry/ReliefColumnInfo.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/json_value.h"
#include "slicer_core/material/MaterialClosureExactLayerPass.h"
#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/materials/volume/MaterialLayerRgbComposer.h"
#include "slicer_core/materials/volume/MaterialLayerNameResolver.h"
#include "slicer_core/materials/volume/MaterialOpacityVarnishResolver.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/materials/volume/MaterialVolumeWhiteCarrier.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/materials/SliceMaterialTexture.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"
#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"
#include "slicer_core/materials/transfer/LegacyTransferChannelSession.h"
#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/materials/varnish_geometry/SurfaceVarnishMasks.h"
#include "slicer_core/model.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/pipeline/SliceProgressNotifier.h"
#include "slicer_core/output/preview/LayerPreviewWriter.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.h"
#include "slicer_core/output/rgbwsvt/LegacyTransferCanvas.h"
#include "slicer_core/reports/CrossSectionStackReport.h"
#include "slicer_core/reports/MaterialProcessReport.h"
#include "slicer_core/reports/MaterialVolumeReport.h"
#include "slicer_core/reports/MaterialClosureReport.h"
#include "slicer_core/reports/ReportBase.h"
#include "slicer_core/support/BoundedReliefSupportPlan.h"
#include "slicer_core/support/InternalVoidSupport.h"
#include "slicer_core/support/SliceSupportGeneration.h"
#include "slicer_core/support/SupportBaseProjection.h"
#include "slicer_core/support/SupportConnectivityAnalysis.h"
#include "slicer_core/support/SupportShapePipeline.h"
#include "slicer_core/support/SupportShapePolicy.h"
#include "slicer_core/support/SupportShapeReport.h"
#include "slicer_core/support/SupportType.h"
#include "slicer_core/texture_image.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slicer_core {

bool RequiresCompleteWhiteUnderbase(const std::string_view whiteMode) noexcept
{
    return whiteMode == "underbase";
}

namespace {


GridSpec make_grid_spec(const SliceConfig& config, const BoundingBox& bbox) {
    GridSpec grid;
    grid.pixel_size_x_mm =
        kMillimetersPerInch / static_cast<double>(config.output.dpi_x);
    grid.pixel_size_y_mm =
        kMillimetersPerInch / static_cast<double>(config.output.dpi_y);
    const OuterVarnishDiscretization outerVarnish =
        ComputeOuterVarnishDiscretization(
            config.outer_varnish,
            grid.pixel_size_x_mm,
            grid.pixel_size_y_mm);
    const int outerVarnishPaddingXPx =
        config.outer_varnish.allow_xy_expansion
        ? outerVarnish.radius_x_px
        : 0;
    const int outerVarnishPaddingYPx =
        config.outer_varnish.allow_xy_expansion
        ? outerVarnish.radius_y_px
        : 0;
    grid.origin_x_mm = bbox.min.x
        - static_cast<double>(outerVarnishPaddingXPx) * grid.pixel_size_x_mm;
    grid.origin_y_mm = bbox.min.y
        - static_cast<double>(outerVarnishPaddingYPx) * grid.pixel_size_y_mm;
    const double width_mm{std::max(0.001, bbox.max.x - bbox.min.x)
                          + 2.0 * static_cast<double>(outerVarnishPaddingXPx) * grid.pixel_size_x_mm};
    const double height_mm{std::max(0.001, bbox.max.y - bbox.min.y)
                           + 2.0 * static_cast<double>(outerVarnishPaddingYPx) * grid.pixel_size_y_mm};
    const double z_max{std::max(config.output.layer_thickness_mm, bbox.max.z + config.support.offset_mm)};
    grid.width_px = std::max(1, static_cast<int>(std::ceil(width_mm / grid.pixel_size_x_mm)));
    grid.height_px = std::max(1, static_cast<int>(std::ceil(height_mm / grid.pixel_size_y_mm)));
    grid.layer_count = std::max(1, static_cast<int>(std::ceil(z_max / config.output.layer_thickness_mm)));
    return grid;
}


bool IsOpenVdbCandidateConfig(const SliceConfig& config)
{
    return config.texture.apply_mode == "surface_shell_from_sdf"
        || config.experimental.openvdb_pipeline.write_production_rgbwsv;
}

bool IsTransferSceneProductionOptIn(
    const SliceConfig& config,
    const SliceRunOptions& options)
{
    return options.transfer_scene_production_admission != nullptr
        && config.transfer_channel_policy.enabled
        && options.instanceoverride.has_value()
        && options.inputoverride.has_value()
        && !options.gridcallback
        && !options.layercallback
        && options.modelreportoverride == nullptr;
}

void EnsureLegacyPipelineAcceptsConfig(const SliceConfig& config, const SliceRunOptions& options)
{
    const bool usesAdapter = options.gridcallback || options.layercallback
        || options.instanceoverride || options.inputoverride
        || options.modelreportoverride != nullptr;
    const bool transferSceneProductionOptIn =
        IsTransferSceneProductionOptIn(config, options);
    ValidateLegacyTransferChannelRunBoundary(
        config.transfer_channel_policy,
        !options.write_tiff_layers && !options.write_preview_files
            && !options.write_reports && !usesAdapter,
        options.write_tiff_layers && options.write_reports
            && (!usesAdapter || transferSceneProductionOptIn));
    if (!IsOpenVdbCandidateConfig(config))
    {
        return;
    }

    throw std::runtime_error(
        "OpenVDB candidate slicing requires the explicit --openvdb-candidate-slice path; "
        "the legacy production path must not run surface_shell_from_sdf or writeProductionRgbwsv configs");
}


bool MatchesSourceIdentity(
    const std::filesystem::path& loadedModelPath,
    const std::string& sourceIdentity)
{
    if (loadedModelPath.empty() || sourceIdentity.empty())
    {
        return false;
    }
    const std::filesystem::path identityPath = PathFromUtf8(sourceIdentity);
    std::error_code error;
    const bool equivalent = std::filesystem::equivalent(
        loadedModelPath,
        identityPath,
        error);
    if (!error)
    {
        return equivalent;
    }

    error.clear();
    const std::filesystem::path loadedCanonical =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(loadedModelPath),
            error);
    if (error)
    {
        return false;
    }
    const std::filesystem::path identityCanonical =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(identityPath),
            error);
    return !error && loadedCanonical == identityCanonical;
}

}  // namespace

SliceRunLayerConsumerError::SliceRunLayerConsumerError(
    const SliceRunLayerConsumeStatus status,
    const int layerIndex,
    std::string detail)
    : std::runtime_error(
          "owned layer consumer "
          + std::string(
              status == SliceRunLayerConsumeStatus::Cancelled
                  ? "cancelled"
                  : "failed")
          + " at layer " + std::to_string(layerIndex)
          + (detail.empty() ? std::string{} : ": " + detail)),
      status_(status),
      layerIndex_(layerIndex)
{
}

SliceRunLayerConsumeStatus SliceRunLayerConsumerError::Status() const noexcept
{
    return status_;
}

int SliceRunLayerConsumerError::LayerIndex() const noexcept
{
    return layerIndex_;
}

SliceRunResult run_slicer(const std::filesystem::path& config_path) {
    return run_slicer(config_path, SliceRunOptions{});
}

SliceRunResult run_slicer(const std::filesystem::path& config_path, const SliceRunOptions& options) {
    if (options.layercallback && options.ownedlayercallback)
    {
        throw std::invalid_argument(
            "legacy const and owned layer callbacks are mutually exclusive");
    }
    if (options.ownedlayercallback
        && (options.write_tiff_layers
            || options.write_preview_files
            || options.write_reports))
    {
        throw std::invalid_argument(
            "owned layer callback requires producer file output to be disabled");
    }

    SliceRunProfile profile;
    profile.available = true;
    profile.profile_level = "coarse";
    const auto run_start = SlicerClock::now();
    auto phase_start = run_start;

    progress::NotifyProgress(options, run_start, "config_load", 0, 1, 0);
    SliceConfig config = load_slice_config(config_path);
    if (options.inputoverride.has_value())
    {
        if (options.inputoverride->modelpath.empty())
        {
            throw std::runtime_error(
                "slice input override model path must not be empty");
        }
        config.input.model_path =
            options.inputoverride->modelpath;
        config.input.format =
            options.inputoverride->format.empty()
            ? "auto"
            : options.inputoverride->format;
        validate_slice_config(config);
    }
    profile.config_load_ms = progress::ElapsedMsSince(phase_start);
    EnsureGlobalTextureFillPartitionBackendAvailable(config);
    progress::NotifyProgress(options, run_start, "model_load", 0, 1, 3);
    phase_start = SlicerClock::now();

    EnsureLegacyPipelineAcceptsConfig(config, options);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    ModelReport model_report;
    if (options.modelreportoverride != nullptr)
    {
        if (options.modelreportoverride->triangles.empty())
        {
            throw std::runtime_error(
                "slice model report override must contain triangles");
        }
        model_report = *options.modelreportoverride;
    }
    else
    {
        model_report = load_model_report(config, config_dir);
    }
    if (options.instanceoverride.has_value())
    {
        if (!MatchesSourceIdentity(
                model_report.model_path,
                options.instanceoverride->sourcetransformidentity))
        {
            throw std::runtime_error(
                "Legacy instance source identity does not match the configured model");
        }
        TransformedModelResult transformed =
            AdaptTransformedModel(
                model_report,
                *options.instanceoverride);
        if (!transformed.IsValid())
        {
            throw std::runtime_error(
                "Legacy instance transform failed: "
                + transformed.error->message);
        }
        model_report.triangles = std::move(transformed.geometry.triangles);
        model_report.frame_vertices = std::move(transformed.geometry.framevertices);
        model_report.triangle_textures =
            std::move(transformed.geometry.triangletextures);
        model_report.bbox_mm = transformed.geometry.bboxmm;
    }
    profile.model_load_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "grid_setup", 0, 1, 10);
    phase_start = SlicerClock::now();

    SupportBaseProjectionPreparation supportBaseProjectionPreparation;
    if (config.support.enabled)
    {
        supportBaseProjectionPreparation =
            ResolveSupportBaseProjectionPreparation(
                config.support.base_projection,
                config.output.layer_thickness_mm);
        support::LiftModelForSupportBase(model_report, supportBaseProjectionPreparation.model_lift_mm);
    }
    const GridSpec grid = make_grid_spec(config, model_report.bbox_mm);
    const LegacyTransferCanvas transferCanvas(grid, config.output.scene_pad_to_origin_x, config.output.scene_pad_to_origin_y,
        config.transfer_channel_policy.enabled && options.instanceoverride.has_value());
    const GridSpec outputGrid = transferCanvas.OutputGrid(grid);
    const MaterialVolumeGrid materialVolumeGrid{
        grid.width_px, grid.height_px, grid.origin_x_mm, grid.origin_y_mm,
        grid.pixel_size_x_mm, grid.pixel_size_y_mm, config.output.layer_thickness_mm, grid.layer_count};

    // 退化面阈值：默认沿用适配器内建值；工艺文件显式收紧时才覆盖。
    // CAD/NURBS 导出的多材质资产常含 nm^2 级合法薄面，默认门会误杀并制造边界边。
    SceneModelTriangleMeshAdapterOptions geometryAdapterOptions;
    if (config.geometry_sampling.degenerate_area_epsilon_mm2 > 0.0)
    {
        geometryAdapterOptions.degenerate_area_epsilon_mm2 =
            config.geometry_sampling.degenerate_area_epsilon_mm2;
    }

    std::optional<LegacyTransferChannelSession> transferSession;
    if (config.transfer_channel_policy.enabled)
    {
        const AdaptedTriangleMesh transferMesh =
            AdaptSceneModelToTriangleMesh(model_report, geometryAdapterOptions);
        transferSession = BuildLegacyTransferChannelSession(
            config.transfer_channel_policy,
            transferMesh,
            materialVolumeGrid,
            options.cancellation_requested);
    }

    // MATVOL 生产接线（MV-08B）。必须在此处构建：model_report 已完成自动摆正、
    // 实例变换与 LiftModelForSupportBase 的 Z 抬升，grid 亦由同一 bbox 推导，
    // 因此 plan 的列序与层号与后续栅格化逐项对齐，无需任何补偿。
    std::optional<MaterialVolumePlan> materialVolumePlan;
    std::optional<MaterialRgbTable> materialVolumeRgbTable;
    MaterialOpacityVarnishResolution opacityVarnishResolution;
    std::vector<std::uint8_t> opacityVarnishByIndex;
    if (config.material_volume_policy.enabled)
    {
        // SceneModel 即 ModelReport 的别名，既有适配器可直接消费，无需适配层。
        const AdaptedTriangleMesh matvolMesh =
            AdaptSceneModelToTriangleMesh(model_report, geometryAdapterOptions);
        // overlap.mode=auto_by_material_name：按命名规范推导逐材质 priority，
        // 替代人工填写的 rules。违规命名与撞号都必须 fail-closed——
        // MATVOL 本身对「缺声明」和「同级」均拒绝，此处提前给出可读原因。
        MaterialVolumePolicyConfig effectiveVolumePolicy = config.material_volume_policy;
        if (effectiveVolumePolicy.overlap.mode == "auto_by_material_name")
        {
            const MaterialLayerNaming naming =
                ResolveMaterialLayerNaming(model_report.material_infos);
            if (!naming.violations.empty())
            {
                throw std::runtime_error(
                    "E_MATOPQ_LAYER_NAME_INVALID: " + naming.violations.front());
            }
            if (!naming.collisions.empty())
            {
                throw std::runtime_error(
                    "E_MATOPQ_LAYER_PRIORITY_COLLISION: " + naming.collisions.front());
            }
            effectiveVolumePolicy.overlap.rules.clear();
            for (std::size_t index{0}; index < naming.names.size(); ++index)
            {
                effectiveVolumePolicy.overlap.rules.push_back(
                    MaterialVolumeOverlapRuleConfig{
                        naming.names.at(index).material_name,
                        naming.priorities.at(index)});
            }
        }

        MaterialVolumeBuildRequest matvolRequest;
        matvolRequest.mesh = &matvolMesh;
        matvolRequest.policy = &effectiveVolumePolicy;
        matvolRequest.grid = materialVolumeGrid;
        // plan 构建是本路径上最长的不可中断窗口（逐列遍历全部三角面），
        // 且发生在 gridcallback 之前，故必须显式透传取消点，
        // 否则该窗口在生产路径上完全无法取消。
        matvolRequest.cancellationRequested = options.cancellationRequested;
        materialVolumePlan = BuildMaterialVolumePlan(matvolRequest);
        MaterialRgbTableRequest tableRequest;
        tableRequest.plan = &materialVolumePlan.value();
        tableRequest.materialInfos = model_report.material_infos;
        materialVolumeRgbTable = BuildMaterialRgbTable(tableRequest);
        // MO-04：按不透明度判据解出哪些材质归属光油，并折成按 materialIndex 的查表，
        // 使逐层内循环只做 O(1) 索引，不做字符串比较。
        opacityVarnishResolution = ResolveMaterialOpacityVarnish(
            config.material_volume_policy, model_report.material_infos);
        const std::span<const std::string> planMaterialNames =
            materialVolumePlan.value().MaterialNames();
        opacityVarnishByIndex.assign(planMaterialNames.size(), 0U);
        for (std::size_t index{0}; index < planMaterialNames.size(); ++index)
        {
            if (opacityVarnishResolution.varnish_materials.count(
                    std::string(planMaterialNames[index])) != 0U)
            {
                opacityVarnishByIndex.at(index) = 1U;
            }
        }
    }
    if (options.gridcallback)
    {
        options.gridcallback(
            SliceRunRasterGrid{
                grid.width_px,
                grid.height_px,
                grid.layer_count,
                grid.pixel_size_x_mm,
                grid.pixel_size_y_mm,
                config.output.layer_thickness_mm,
                grid.origin_x_mm,
                grid.origin_y_mm,
                0.0});
    }

    const std::filesystem::path package_dir = config.output.package_dir;
    std::optional<RgbwsvtCandidatePackageGuard> transferPackageGuard;
    if (transferSession.has_value()
        && (options.write_tiff_layers || options.write_preview_files || options.write_reports))
    {
        transferPackageGuard.emplace(package_dir);
    }
    const bool automatic_diagnostic_images =
        config.preview.enabled && options.write_preview_files;
    const std::string effective_preview_output_policy =
        automatic_diagnostic_images
            ? "tiff_native_with_diagnostics"
            : "tiff_native";
    if (options.write_tiff_layers || options.write_preview_files || options.write_reports) {
        std::filesystem::create_directories(package_dir);
    }
    if (options.write_tiff_layers) {
        std::filesystem::create_directories(package_dir / "layers");
    }
    if (options.write_reports) {
        std::filesystem::create_directories(package_dir / "reports");
    }
    if (automatic_diagnostic_images) {
        std::filesystem::create_directories(package_dir / "preview");
    }

    RgbwsvProductionStorageSpec productionStorage;
    productionStorage.storageMode = config.output.storage_mode;
    productionStorage.compression = config.output.tiff_compression;
    productionStorage.tileWidth = config.output.tile_size.at(0);
    productionStorage.tileHeight = config.output.tile_size.at(1);
    productionStorage.rowsPerStrip = config.output.rows_per_strip;
    profile.grid_setup_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "mask_sampling", 0, 1, 12);
    phase_start = SlicerClock::now();

    std::vector<LayerDiagnostics> layer_diagnostics;
    ReliefReportData relief_report;
    std::vector<ReliefColumnInfo> relief_columns;
    ReliefPerMaterialTopSurface relief_per_material_top;
    std::vector<std::vector<std::uint8_t>> model_masks;
    // MF-03X2a：判定本次配置能否走有界路径。判定只看配置，故可在采样【之前】求出，
    // 从而连采样阶段的整栈分配（10um 场景约 10.4 GB）一起省掉。
    const BoundedReliefSupportEligibility boundedReliefSupport =
        EvaluateBoundedReliefSupportPath(config);
    std::vector<BoundedReliefColumnSpan> boundedReliefSpans;
    // MF-03X2b 稀疏遍历的活动列表。实测 a-2/0.2.obj 只有 186,103 / 7,369,346 列
    // （2.53%）有模型，其余 97.47% 在任何一层都是空白 —— compose_layer 的 else 链
    // 没有末尾 else，那些列本来就不写任何字节，故跳过它们是【精确等价】。
    // 判据不能简单取「有模型的列」：内部空腔支撑会写到面内被围住的空列（环形件
    // 孔心在所有层都没有模型）。见 BuildBoundedActiveColumns。
    std::vector<std::uint32_t> boundedActiveColumns;
    InternalVoidScratch boundedInternalVoidScratch;
    // MF-03X4：compose 的输出缓冲跨层复用，避免每层重新触碰 w*h*6 字节的新页。
    std::vector<std::uint8_t> layer;
    if (config.slicing_mode == "relief_heightfield") {
        // M2：MATVOL 启用时把材质名表传入采样，使其一并解出逐材质顶面。
        // plan 在本调用之前建成（见上方 BuildMaterialVolumePlan），故此处可用。
        std::optional<std::span<const std::string>> planMaterialNamesForSampling;
        if (materialVolumePlan.has_value()) {
            planMaterialNamesForSampling = materialVolumePlan.value().MaterialNames();
        }
        ReliefSamplingResult relief_sampling = relief::sample_relief_heightfield_masks(
            config,
            model_report,
            grid,
            layer_diagnostics,
            planMaterialNamesForSampling.has_value()
                ? &planMaterialNamesForSampling.value() : nullptr,
            !boundedReliefSupport.eligible);
        model_masks = std::move(relief_sampling.model_masks);
        relief_columns = std::move(relief_sampling.columns);
        if (boundedReliefSupport.eligible)
        {
            // 整栈未物化，改为留下逐列闭区间；主循环按层重建。
            boundedReliefSpans.resize(relief_columns.size());
            for (std::size_t column{0}; column < relief_columns.size(); ++column)
            {
                const ReliefColumnInfo& info = relief_columns.at(column);
                boundedReliefSpans.at(column) = BoundedReliefColumnSpan{
                    info.has_model, info.lower_layer, info.upper_layer};
            }
            boundedActiveColumns = BuildBoundedActiveColumns(
                boundedReliefSpans, grid.width_px, grid.height_px);
        }
        relief_per_material_top = std::move(relief_sampling.per_material_top);
        relief_report = std::move(relief_sampling.report);
    } else {
        model_masks = masks::sample_model_masks(model_report, grid, config.output.layer_thickness_mm, layer_diagnostics);
        relief_report.total_columns = grid.width_px * grid.height_px;
        relief_report.empty_columns = relief_report.total_columns;
    }
    const std::vector<int> support_source_layers = config.slicing_mode == "relief_heightfield"
        ? relief::compute_relief_lower_layers(relief_columns)
        : relief::compute_first_model_layers(model_masks, grid);
    profile.mask_sampling_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "texture_prepare", 0, 1, 28);
    phase_start = SlicerClock::now();

    TextureRuntime texture_runtime = materials::prepare_texture_runtime(config, model_report);
    if (model_report.format == "3mf") {
        int internal_loaded{0};
        int internal_missing{0};
        for (const auto& [unused_name, material] : texture_runtime.materials) {
            (void)unused_name;
            if (!material.material.has_texture || material.material.texture_source != "3mf_internal") {
                continue;
            }
            if (material.loaded) {
                ++internal_loaded;
            } else {
                ++internal_missing;
            }
        }
        if (internal_loaded + internal_missing > 0) {
            model_report.three_mf.texture_loaded_count = internal_loaded;
            model_report.three_mf.texture_missing_count = std::max(model_report.three_mf.texture_missing_count, internal_missing);
        }
    }
    std::vector<TextureColumnColor> texture_columns;
    if (config.texture.enabled && config.slicing_mode == "relief_heightfield") {
        texture_columns = materials::build_relief_texture_columns(config, model_report, relief_columns, texture_runtime);
    }
    // M2：逐材质贴图色。仅在 MATVOL 启用（relief_per_material_top 非空）且
    // texture 启用时才构建；否则为空，MATVOL 分支沿用 Kd 表。
    const std::vector<TextureColumnColor> per_material_texture_columns =
        materials::build_per_material_texture_columns(
            config, model_report, relief_per_material_top, texture_runtime);
    const MaterialRoleMappingReportData material_role_mapping_report =
        materials::build_material_role_mapping_report(config, model_report);
    std::vector<MaterialRoleColumn> material_role_columns;
    if (config.material_role_mapping.enabled && config.slicing_mode == "relief_heightfield") {
        material_role_columns = materials::build_material_role_columns(
            config,
            model_report,
            relief_columns,
            config.texture.enabled ? &texture_columns : nullptr);
    }
    profile.texture_prepare_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "support_generation", 0, 1, 32);
    phase_start = SlicerClock::now();

    const std::vector<ColumnLayerRange> column_ranges = config.slicing_mode == "relief_heightfield"
        ? relief::compute_relief_column_ranges(relief_columns)
        : relief::compute_mask_column_ranges(model_masks, grid);
    const std::vector<std::vector<std::uint8_t>> outer_varnish_masks =
        masks::BuildOuterVarnishMasks(config, grid, model_masks);
    const UpperSupportBoundaryInfo upper_support_boundary_info =
        masks::ResolveUpperSupportBoundaryInfo(config);
    const std::vector<std::vector<std::uint8_t>> owned_upper_support_boundary_masks =
        masks::BuildUpperSupportBoundaryMasks(grid, model_masks, outer_varnish_masks, upper_support_boundary_info);
    const std::vector<std::vector<std::uint8_t>>& upper_support_boundary_masks =
        owned_upper_support_boundary_masks.empty()
        ? model_masks
        : owned_upper_support_boundary_masks;
    const std::vector<ColumnLayerRange> upper_support_boundary_column_ranges =
        relief::compute_mask_column_ranges(upper_support_boundary_masks, grid);
    // MF-03X：表面光油 mask 由全层构建改为按层物化。原 BuildSurfaceVarnishMasks
    // 一次分配 列数 x 层数 x 2 字节，10um 大幅面场景下两个容器各约 10.4 GB。
    // 该算法逐层独立（每层只读本层 model mask），故改为下方层循环内按需物化，
    // 缓冲跨层复用、只覆写不重分配。全层版本已无调用者，随本次改动删除。
    const std::size_t surfaceVarnishPixelCount =
        static_cast<std::size_t>(grid.width_px) * grid.height_px;
    const bool surfaceVarnishRequired = SurfaceVarnishMasksRequired(config);
    std::vector<std::uint8_t> outerSurfaceVarnishLayer(
        surfaceVarnishRequired ? surfaceVarnishPixelCount : 0U, 0U);
    std::vector<std::uint8_t> innerSurfaceVarnishLayer(
        surfaceVarnishRequired ? surfaceVarnishPixelCount : 0U, 0U);
    // MF-03X2a：有界路径的三个单层缓冲，跨层复用、只覆写不重分配。
    // 峰值由 O(列数 x 层数) x 3 降为 O(列数) x 3 —— 10um 场景 31.11 GB -> 约 23 MB。
    std::vector<std::uint8_t> boundedModelLayer(
        boundedReliefSupport.eligible ? surfaceVarnishPixelCount : 0U, 0U);
    std::vector<std::uint8_t> boundedSupportLayer(
        boundedReliefSupport.eligible ? surfaceVarnishPixelCount : 0U, 0U);
    std::vector<SupportType> boundedSupportTypeLayer(
        boundedReliefSupport.eligible ? surfaceVarnishPixelCount : 0U,
        SupportType::None);
    const OuterVarnishDiscretization outerVarnishDiscretization =
        ComputeOuterVarnishDiscretization(
            config.outer_varnish,
            grid.pixel_size_x_mm,
            grid.pixel_size_y_mm);
    const int columns_with_support = config.support.enabled
        ? static_cast<int>(std::count_if(support_source_layers.begin(), support_source_layers.end(), [](const int layer) {
              return layer > 0;
          }))
        : 0;
    // MF-03X2a：有界路径下 support_masks / support_type_maps 保持为空，
    // 由层循环内按 bottom projection 谓词逐层物化（该谓词只依赖本层 model mask
    // 与按列标量 support_source_layers，无任何跨层数据）。
    SupportGenerationResult support_generation;
    if (!boundedReliefSupport.eligible)
    {
        support_generation =
            support::generate_support_masks(
                config,
                grid,
                model_masks,
                upper_support_boundary_masks,
                support_source_layers,
                column_ranges,
                upper_support_boundary_column_ranges,
                layer_diagnostics);
    }
    const SupportShapePolicy support_shape_policy = MakeSupportShapePolicy(config.support);
    SupportShapeOptimizationResult support_shape_result;
    if (support_shape_policy.enabled)
    {
        // 有界路径下 support_masks 整栈为空（generate_support_masks 根本没被调用），
        // 于是下面的整栈形状优化会跑 0 层，却仍报 enabled=true、added/removed 全 0
        // —— 不崩、不报错、结果静默错误。准入当前拒绝 shape 档（见
        // EvaluateBoundedReliefSupportPath），故此处不可能触发；它存在是为了让
        // 【将来放开那道准入】时立刻失败，而不是悄悄产出一份没做过形状优化的包。
        if (boundedReliefSupport.eligible)
        {
            throw std::runtime_error(
                "support shape optimization requires the retained support stack; "
                "the bounded path must materialize shape per layer before the "
                "support_shape_enabled admission is relaxed");
        }
        const std::vector<std::vector<std::uint8_t>> originalSupportMasks = support_generation.support_masks;
        support_shape_result = ApplySupportShapePolicy(
            support_shape_policy,
            model_masks,
            support_generation.support_masks,
            grid.width_px,
            grid.height_px,
            config.support.connectivity);
        if (support_shape_result.enabled)
        {
            SynchronizeSupportShapeTypeMaps(
                originalSupportMasks,
                support_generation.support_masks,
                support_generation.support_type_maps);
        }
    }
    SupportBaseProjectionResult supportBaseProjectionResult;
    if (config.support.enabled)
    {
        supportBaseProjectionResult = ApplySupportBaseProjection(
            config.support.base_projection,
            model_masks,
            support_generation.support_masks);
        supportBaseProjectionResult.added_layer_count =
            supportBaseProjectionPreparation.prepended_layer_count;
        supportBaseProjectionResult.model_lift_mm =
            supportBaseProjectionPreparation.model_lift_mm;
        for (int layerIndex{0};
             layerIndex < supportBaseProjectionResult.effective_layer_count;
             ++layerIndex)
        {
            const std::vector<std::uint8_t>& supportMask =
                support_generation.support_masks.at(
                    static_cast<std::size_t>(layerIndex));
            std::vector<SupportType>& supportTypeMap =
                support_generation.support_type_maps.at(
                    static_cast<std::size_t>(layerIndex));
            for (std::size_t index{0U};
                 index < supportMask.size();
                 ++index)
            {
                if (supportMask.at(index) != 0U
                    && (supportBaseProjectionResult.layer_placement
                            == "prepend_below_model"
                        || supportTypeMap.at(index)
                            == SupportType::None))
                {
                    supportTypeMap.at(index) = SupportType::ProjectionBase;
                }
            }
        }
    }
    const SupportPlacementPolicy support_placement_policy = support::ResolveSupportPlacementPolicy(config);
    const bool repairMaterialClosure = config.material_closure.enabled
        && config.material_closure.mode == "repair_then_report"
        && config.material_closure.repair.enabled;
    const bool collectMaterialClosureExact = config.material_closure.enabled
        && (options.write_reports || repairMaterialClosure);
    // MF-09：闭合分析的 workspace 与入参都与层无关，持有在循环外跨层复用。
    MaterialClosureExactLayerWorkspace materialClosureWorkspace;
    const MaterialClosureExactLayerRequest materialClosureRequest{
        .connectivity = config.material_closure.connectivity,
        .maxGapPx = config.material_closure.max_gap_px,
        .repair = repairMaterialClosure,
        .repairValues = materials::ResolveMaterialClosureRepairValues(config)};
    const bool collectMaterialClosureSemantic =
        collectMaterialClosureExact
        || static_cast<bool>(options.layercallback)
        || static_cast<bool>(options.ownedlayercallback);
    std::vector<std::vector<std::size_t>> clearedOuterVarnishSupportIndices;
    const int cleared_outer_varnish_support_pixels =
        support::ApplyOuterVarnishSupportPriority(
            outer_varnish_masks,
            support_generation,
            collectMaterialClosureSemantic
                ? &clearedOuterVarnishSupportIndices
                : nullptr);
    if (boundedReliefSupport.eligible)
    {
        // ApplyOuterVarnishSupportPriority 按 support_masks.size() 定尺，
        // 有界路径下整栈为空会得到 0 长度，使层循环内 .at(layer_index) 抛异常。
        // 此处按层数补尺；有界路径已排除外光油，故内容必为空集。
        if (collectMaterialClosureSemantic)
        {
            clearedOuterVarnishSupportIndices.assign(
                static_cast<std::size_t>(grid.layer_count), {});
        }
        // 统计改为层循环内逐层累积，此处只清零。
        support::ResetSupportGenerationStats(support_generation);
    }
    else
    {
        support::CalculateSupportGenerationStats(
            support_generation,
            layer_diagnostics,
            grid,
            config);
    }
    profile.support_statistics_scan_count = 1;
    profile.support_generation_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "layer_processing", 0, grid.layer_count, 36);
    phase_start = SlicerClock::now();

    int total_model_pixels{0};
    int total_support_pixels{0};
    int total_rgb_non_zero_pixels{0};
    int total_white_non_zero_pixels{0};
    int total_support_non_zero_pixels{0};
    int total_varnish_non_zero_pixels{0};
    LayerSemanticStats total_semantic_stats;
    std::array<ChannelStats, rgbwsv_channel_count> total_channel_stats{};
    std::optional<RgbwsvtChannelStatistics> totalTransferChannelStatistics;
    RgbwsvtMaterialStatistics totalTransferMaterialStatistics;
    std::vector<RgbwsvtLegacyLayerStatistics> transferLayerStatistics;
    if (transferSession.has_value() && options.write_tiff_layers)
    {
        totalTransferChannelStatistics.emplace();
        transferLayerStatistics.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    Json::Array layers;
    Json::Array slice_layers;
    Json::Array contour_layers;
    Json::Array preview_files;
    std::vector<MaterialClosureSemanticLayerResult> materialClosureExactLayers;
    if (collectMaterialClosureExact)
    {
        materialClosureExactLayers.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    const bool collectMaterialClosureCandidate = config.material_closure.enabled
        && options.write_tiff_layers
        && !collectMaterialClosureExact;
    std::vector<MaterialClosureCandidateLayer> materialClosureCandidateLayers;
    if (collectMaterialClosureCandidate)
    {
        materialClosureCandidateLayers.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    MaterialPolicyReportData material_policy_report;
    material_policy_report.enabled = config.material_policy.enabled;
    const std::size_t layerPixelCount =
        static_cast<std::size_t>(grid.width_px)
        * static_cast<std::size_t>(grid.height_px);
    const BoundedMaterialReplayPolicy retainedMaterialPolicy =
        MakeBoundedMaterialReplayPolicy(config);
    const std::vector<std::uint8_t> emptyOptionalMask(layerPixelCount, 0U);
    // 未归属模型像素的填补（用户 2026-08-24 裁定「确有间隙则填补为下层材料」）。
    // 该逻辑曾在 03.obj 上恒不触发而被当作死代码撤除；08/09 的 3 条真开边给出了
    // 真实触发资产（实测 46 万格中 2 格），故按同一裁定重新引入并附计数。
    // lastOwnedMaterial 随层循环自下而上推进，天然给出「下方最近的已归属材质」。
    std::vector<std::uint32_t> lastOwnedMaterial;
    // 次级规则所需：每列最低区间的材质。位于最低区间【之下】的格子没有「下方材质」，
    // 例如薄于一个层厚的区间会被整段丢弃（firstLayer > lastLayer），
    // 使该列底部出现掩码为模型却无任何区间覆盖的格子。
    std::vector<std::uint32_t> bottomMaterial;
    std::uint64_t unownedFilledCells{0};
    if (materialVolumePlan.has_value())
    {
        lastOwnedMaterial.assign(layerPixelCount, kNoMaterialOwner);
        bottomMaterial.assign(layerPixelCount, kNoMaterialOwner);
        const std::span<const std::uint32_t> offsets =
            materialVolumePlan.value().ColumnIntervalOffsets();
        const std::span<const MaterialLayerInterval> intervals =
            materialVolumePlan.value().Intervals();
        for (std::size_t column{0}; column < layerPixelCount; ++column)
        {
            int lowestFirstLayer{std::numeric_limits<int>::max()};
            for (std::uint32_t index{offsets[column]};
                 index < offsets[column + 1U];
                 ++index)
            {
                const MaterialLayerInterval& interval = intervals[index];
                if (interval.firstLayerInclusive < lowestFirstLayer)
                {
                    lowestFirstLayer = interval.firstLayerInclusive;
                    bottomMaterial[column] = interval.materialIndex;
                }
            }
        }
    }
    // MV-08C：逐层 owner 覆盖统计。逐层记录而非只记总量，
    // 是为了让「只有部分层用了 owner」这类层间突变在报告里可见。
    std::vector<MaterialVolumeLayerStat> materialVolumeLayerStats;
    if (materialVolumePlan.has_value())
    {
        materialVolumeLayerStats.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    // MATVOL 逐层复用缓冲：owner 为每列一个材质下标，rgb 为紧凑三通道，
    // 与 compose_layer 返回的六通道交错布局不同，需在写回时按列取用。
    // M1：逐列顶面材质在 plan 材质表中的下标。O(列数) 且只构建一次，
    // 不进层循环——故不触碰 MV-03 禁止的 O(材质数 x 层数 x 像素数) 稠密栈。
    // 构建点必须同时晚于 materialVolumePlan 建成与 relief_columns 填充：
    // 前者提供材质名表，后者提供每列顶面三角。非 relief 模式下 relief_columns
    // 为空，本表随之为空，判据恒真即退回既有行为。
    std::vector<std::uint32_t> topMaterialIndexByColumn;
    if (materialVolumePlan.has_value() && !relief_columns.empty()) {
        const std::span<const std::string> planMaterialNames =
            materialVolumePlan.value().MaterialNames();
        topMaterialIndexByColumn.assign(relief_columns.size(), kNoMaterialOwner);
        for (std::size_t column{0}; column < relief_columns.size(); ++column) {
            const ReliefColumnInfo& columnInfo = relief_columns.at(column);
            if (!columnInfo.has_model || columnInfo.top_triangle_index < 0
                || columnInfo.top_triangle_index
                       >= static_cast<int>(model_report.triangle_textures.size())) {
                continue;
            }
            const std::string& topMaterialName =
                model_report.triangle_textures
                    .at(static_cast<std::size_t>(columnInfo.top_triangle_index))
                    .material_name;
            for (std::size_t index{0}; index < planMaterialNames.size(); ++index) {
                if (planMaterialNames[index] == topMaterialName) {
                    topMaterialIndexByColumn.at(column) =
                        static_cast<std::uint32_t>(index);
                    break;
                }
            }
        }
    }
    // MF-03X4：relief_columns 是 64 B/列的结构，10um 大幅面场景约 450 MB ——
    // 单模型峰值 1.18 GiB 里最大的一块。它的全部消费者（贴图列、角色列、列区间、
    // 支撑起始层、逐列闭区间、MATVOL 顶面索引）都在层循环【之前】跑完，
    // 层循环只用 12 B/列的 boundedReliefSpans，故此处即可归还。
    relief_columns.clear();
    relief_columns.shrink_to_fit();

    std::vector<std::uint32_t> materialVolumeOwner;
    std::vector<std::uint8_t> materialVolumeRgb;
    std::vector<std::uint8_t> materialVolumeVarnishMask;
    if (materialVolumePlan.has_value())
    {
        materialVolumeOwner.assign(layerPixelCount, kNoMaterialOwner);
        materialVolumeRgb.assign(layerPixelCount * 3U, 0U);
    }

    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        const auto layerComputeStart = SlicerClock::now();
        int layer_model_pixels{0};
        int layer_support_pixels{0};
        LayerDiagnostics& diagnostics = layer_diagnostics.at(layer_index);
        // MF-03X2a：有界路径下三个整栈均未物化，此处按层重建。
        // model mask 由逐列闭区间重建；support 由 bottom projection 谓词重建
        // （只依赖本层 model mask 与按列标量）；统计逐层累积，语义与 retained 一致。
        if (boundedReliefSupport.eligible)
        {
            MaterializeReliefModelLayer(
                boundedReliefSpans, layer_index, boundedModelLayer,
                &boundedActiveColumns);
            MaterializeBoundedSupportLayer(
                boundedReliefSupport.placement,
                boundedReliefSpans,
                support_source_layers,
                boundedModelLayer,
                config.support.enabled,
                layer_index,
                boundedSupportLayer,
                boundedSupportTypeLayer,
                &boundedActiveColumns);
            if (config.support.enabled)
            {
                // 与 retained 的 generate_support_masks 同序：放置分支之后逐层补
                // 内部空腔支撑。该函数【逐层独立】（只读本层 model mask、只改本层
                // support），故有界路径可原样复用；`internal_void.enabled` 默认为
                // true，漏掉它会把本应 InternalVoid 的像素误标成 BottomProjection。
                // retained 在 support.enabled 为 false 时提前返回、不跑本段，
                // 故此处同样以该开关为守卫。
                AddInternalVoidSupportForLayer(
                    config,
                    grid,
                    boundedModelLayer,
                    boundedSupportLayer,
                    boundedSupportTypeLayer,
                    &boundedActiveColumns,
                    &boundedInternalVoidScratch);
            }
            support::AccumulateSupportLayerStats(
                support_generation,
                diagnostics,
                boundedSupportLayer,
                boundedSupportTypeLayer,
                grid,
                config);
        }
        const std::vector<std::uint8_t>& current_model_mask =
            boundedReliefSupport.eligible
            ? boundedModelLayer
            : model_masks.at(static_cast<std::size_t>(layer_index));
        const std::vector<std::uint8_t>& current_support_mask =
            boundedReliefSupport.eligible
            ? boundedSupportLayer
            : support_generation.support_masks.at(
                static_cast<std::size_t>(layer_index));
        const std::vector<SupportType>& current_support_type_map =
            boundedReliefSupport.eligible
            ? boundedSupportTypeLayer
            : support_generation.support_type_maps.at(
                static_cast<std::size_t>(layer_index));
        const std::vector<std::uint8_t>& outerVarnishMask =
            outer_varnish_masks.empty()
            ? emptyOptionalMask
            : outer_varnish_masks.at(layer_index);
        // MF-03X：本层表面光油 mask 就地物化，替代原来的全层索引。
        if (surfaceVarnishRequired)
        {
            MaterializeSurfaceVarnishLayer(
                config,
                grid,
                current_model_mask,
                layer_index,
                outerSurfaceVarnishLayer,
                innerSurfaceVarnishLayer);
        }
        const std::vector<std::uint8_t>& outerSurfaceVarnishMask =
            surfaceVarnishRequired ? outerSurfaceVarnishLayer : emptyOptionalMask;
        const std::vector<std::uint8_t>& innerSurfaceVarnishMask =
            surfaceVarnishRequired ? innerSurfaceVarnishLayer : emptyOptionalMask;
        MaterialClosureSemanticLayerInput materialClosureInput;
        MaterialClosureSemanticLayerInput* materialClosureInputPointer{nullptr};
        if (collectMaterialClosureSemantic)
        {
            materialClosureInput =
                InitializeRetainedMaterialClosureSemanticInputFromIndices(
                layer_index,
                diagnostics.z_mm,
                grid.width_px,
                grid.height_px,
                current_model_mask,
                current_support_mask,
                clearedOuterVarnishSupportIndices.at(layer_index),
                outerVarnishMask);
            materialClosureInputPointer = &materialClosureInput;
        }
        if (materialVolumePlan.has_value())
        {
            MaterializeMaterialOwnershipLayer(
                materialVolumePlan.value(),
                layer_index,
                current_model_mask,
                materialVolumeOwner);
            {
                const std::vector<std::uint8_t>& matvolMask =
                    current_model_mask;
                for (std::size_t column{0}; column < layerPixelCount; ++column)
                {
                    if (materialVolumeOwner[column] != kNoMaterialOwner)
                    {
                        lastOwnedMaterial[column] = materialVolumeOwner[column];
                        continue;
                    }
                    if (matvolMask[column] == 0U)
                    {
                        continue;
                    }
                    // 主规则：下方最近的已归属材质。
                    std::uint32_t filler = lastOwnedMaterial[column];
                    if (filler == kNoMaterialOwner)
                    {
                        // 次级规则：位于该列最低区间之下，不存在「下方材质」，
                        // 退而取该列最低区间的材质。这是一条【独立的次级规则】，
                        // 不是主规则的一部分。整列无任何区间时仍保持未归属，
                        // 由 ComposeMaterialLayerRgb 按既有语义 fail closed，不静默填充。
                        filler = bottomMaterial[column];
                    }
                    if (filler == kNoMaterialOwner)
                    {
                        continue;
                    }
                    materialVolumeOwner[column] = filler;
                    ++unownedFilledCells;
                }
            }
            ComposeMaterialLayerRgb(
                materialVolumeRgbTable.value(),
                materialVolumeOwner,
                current_model_mask,
                materialVolumeRgb);
            if (config.material_volume_policy.opacity_varnish.enabled)
            {
                materialVolumeVarnishMask.assign(materialVolumeOwner.size(), 0U);
                for (std::size_t column{0}; column < materialVolumeOwner.size(); ++column)
                {
                    const std::uint32_t owner = materialVolumeOwner[column];
                    if (owner == kNoMaterialOwner
                        || owner >= opacityVarnishByIndex.size())
                    {
                        continue;
                    }
                    materialVolumeVarnishMask[column] =
                        opacityVarnishByIndex.at(owner);
                }
            }
            materialVolumeLayerStats.push_back(CountMaterialVolumeLayerOwners(
                materialVolumePlan.value(),
                layer_index,
                materialVolumeOwner,
                current_model_mask));
        }
                materials::compose_layer(
            layer,
            config,
            grid,
            current_model_mask,
            outerVarnishMask,
            outerSurfaceVarnishMask,
            innerSurfaceVarnishMask,
            current_support_mask,
            current_support_type_map,
            config.texture.enabled ? &texture_columns : nullptr,
            config.material_role_mapping.enabled ? &material_role_columns : nullptr,
            &column_ranges,
            materialVolumePlan.has_value() ? &materialVolumeRgb : nullptr,
            config.material_volume_policy.opacity_varnish.enabled
                ? &materialVolumeVarnishMask : nullptr,
            // M1：两者同时可用才启用 owner-vs-顶面判据；owner buffer 是层循环内
            // 复用的同一块内存，其生命周期覆盖本调用点。
            topMaterialIndexByColumn.empty() ? nullptr : &topMaterialIndexByColumn,
            materialVolumeOwner.empty() ? nullptr : &materialVolumeOwner,
            per_material_texture_columns.empty()
                ? nullptr : &per_material_texture_columns,
            boundedReliefSupport.eligible ? &boundedActiveColumns : nullptr,
            layer_index,
            config.texture.enabled ? &texture_runtime.report : nullptr,
            config.material_policy.enabled ? &material_policy_report : nullptr,
            materialClosureInputPointer,
            diagnostics.semantic,
            layer_model_pixels,
            layer_support_pixels);
        diagnostics.model_pixels = layer_model_pixels;
        diagnostics.support_pixels = layer_support_pixels;
        if (collectMaterialClosureSemantic)
        {
            PopulateRetainedMaterialClosureEmptyMask(layer, materialClosureInput);
        }
        if (collectMaterialClosureExact)
        {
            // 未修复时三个 repaired 计数恒为 0，故不必再按 repairMaterialClosure 分支。
            MaterialClosureExactLayerOutcome closureOutcome =
                RunMaterialClosureExactLayerPass(
                    materialClosureInput, materialClosureRequest, layer,
                    materialClosureWorkspace);
            layer_model_pixels += closureOutcome.repairedModelFillPixels;
            layer_support_pixels += closureOutcome.repairedSupportPixels;
            diagnostics.model_pixels = layer_model_pixels;
            diagnostics.support_pixels = layer_support_pixels;
            diagnostics.semantic.model_fill_pixels += closureOutcome.repairedModelFillPixels;
            diagnostics.semantic.support_pixels += closureOutcome.repairedSupportPixels;
            diagnostics.semantic.internal_void_support_pixels +=
                closureOutcome.repairedInternalVoidPixels;
            materialClosureExactLayers.push_back(std::move(closureOutcome.result));
        }
        std::optional<RgbwsvtProductionLayer> transferLayer;
        if (transferSession.has_value())
        {
            transferLayer = ComposeLegacyTransferChannelLayer(
                transferSession.value(),
                RgbwsvProductionLayer{
                    .layerIndex = layer_index, .zMm = diagnostics.z_mm,
                    .widthPx = grid.width_px, .heightPx = grid.height_px,
                    .channels = layer},
                current_model_mask);
            transferCanvas.Apply(transferLayer.value());
            // 光油（V）与弹性材料（T）不得占用同一像素：一个体素不可能同时是两种材料。
            //
            // ComposeRgbwsvtLayer 对缩裹像素【丢弃全部六通道只写 T】，
            // 因此 V 与 T 重叠时 T 会静默胜出且不留痕——这与 K3 裁定
            // （邻域多数表决 + V 优先兜底）相反。此处先 fail-closed 堵住静默错误。
            //
            // 未实施 K3 表决的理由：当前无任何 V/T 重叠资产可验证该表决逻辑，
            // 上线跑不到的裁决不如显式拒绝。K3 七项参数已备（见策略总表 §1.6），
            // 待出现重叠资产后再落地并用其验证。
            if (config.material_volume_policy.opacity_varnish.enabled
                && materialVolumeVarnishMask.size()
                    == transferSession.value().transferMask.size())
            {
                for (std::size_t column{0};
                     column < materialVolumeVarnishMask.size();
                     ++column)
                {
                    if (materialVolumeVarnishMask[column] != 0U
                        && transferSession.value().transferMask[column] != 0U)
                    {
                        throw std::runtime_error(
                            "E_MATOPQ_VARNISH_TRANSFER_OVERLAP: a pixel is claimed by both "
                            "the opacity-derived varnish (V) and the transfer material (T) "
                            "at layer " + std::to_string(layer_index)
                            + "; K3 neighbourhood arbitration is not implemented yet");
                    }
                }
            }
        }
        if (options.layercallback)
        {
            RgbwsvProductionLayer outputLayer;
            outputLayer.layerIndex = layer_index;
            outputLayer.zMm = diagnostics.z_mm;
            outputLayer.widthPx = grid.width_px;
            outputLayer.heightPx = grid.height_px;
            outputLayer.channels = layer;
            options.layercallback(
                outputLayer,
                materialClosureInput);
        }
        if (!materialVolumeLayerStats.empty()
            && materialVolumeLayerStats.back().layerIndex == layer_index)
        {
            // 补白计数由 compose_layer 累加进本层 semantic，此处回填到该层统计。
            materialVolumeLayerStats.back().unprintableWhiteCarrierPixels =
                diagnostics.semantic.unprintable_white_carrier_pixels;
        }
        reports::update_layer_channel_stats(
            layer,
            diagnostics,
            boundedReliefSupport.eligible ? &boundedActiveColumns : nullptr,
            config.background.value);
        total_model_pixels += layer_model_pixels;
        total_support_pixels += layer_support_pixels;
        total_rgb_non_zero_pixels += diagnostics.rgb_non_zero_pixels;
        total_white_non_zero_pixels += diagnostics.white_non_zero_pixels;
        total_support_non_zero_pixels += diagnostics.support_non_zero_pixels;
        total_varnish_non_zero_pixels += diagnostics.varnish_non_zero_pixels;
        reports::merge_channel_stats(total_channel_stats, diagnostics);
        reports::merge_semantic_stats(total_semantic_stats, diagnostics.semantic);
        profile.layer_compute_ms += progress::ElapsedMsSince(layerComputeStart);
        const std::string relative_path = preview::layer_file_name(layer_index);
        if (options.write_tiff_layers) {
            const auto tiffWriteStart = SlicerClock::now();
            if (transferLayer.has_value())
            {
                const RgbwsvtLegacyLayerWriteResult writeResult =
                    WriteRgbwsvtLegacyProductionLayerTiff(
                        package_dir / relative_path, productionStorage,
                        transferLayer.value());
                MergeRgbwsvtChannelStatistics(
                    totalTransferChannelStatistics.value(),
                    writeResult.channelStatistics);
                MergeRgbwsvtMaterialStatistics(
                    totalTransferMaterialStatistics, writeResult.materialStatistics);
                transferLayerStatistics.push_back(RgbwsvtLegacyLayerStatistics{
                    layer_index, writeResult.channelStatistics,
                    writeResult.materialStatistics});
            }
            else
            {
                WriteRgbwsvProductionLayerTiff(
                    package_dir / relative_path, productionStorage,
                    RgbwsvProductionLayerView{
                        grid.width_px, grid.height_px, layer});
            }
            profile.tiff_write_ms += progress::ElapsedMsSince(tiffWriteStart);
            if (collectMaterialClosureCandidate)
            {
                const auto candidateDetectionStart = SlicerClock::now();
                materialClosureCandidateLayers.push_back(DetectMaterialClosureCandidateLayer(
                    layer,
                    grid.width_px,
                    grid.height_px,
                    layer_index,
                    diagnostics.z_mm,
                    config.material_closure.connectivity,
                    config.material_closure.max_gap_px));
                profile.layer_compute_ms += progress::ElapsedMsSince(candidateDetectionStart);
            }
        }
        if (options.write_preview_files && preview::should_write_preview(config.preview, layer_index, grid.layer_count)) {
            const auto previewWriteStart = SlicerClock::now();
            std::vector<std::uint8_t> texture_preview_mask = materials::build_texture_preview_mask(
                config,
                grid,
                current_model_mask,
                config.material_role_mapping.enabled ? &material_role_columns : nullptr,
                &column_ranges,
                layer_index);
            transferCanvas.PadPreviewMask(texture_preview_mask);
            Json::Array written = preview::write_layer_previews(
                config.preview,
                package_dir,
                outputGrid,
                layer_index,
                transferLayer.has_value() ? transferLayer->channels : layer,
                config.texture.enabled ? &texture_preview_mask : nullptr,
                transferLayer.has_value());
            preview_files.insert(preview_files.end(), written.begin(), written.end());
            profile.preview_write_ms += progress::ElapsedMsSince(previewWriteStart);
        }
        const auto layerMetadataStart = SlicerClock::now();
        layers.push_back(Json::object({
            {"index", layer_index},
            {"zMm", diagnostics.z_mm},
            {"path", relative_path},
            {"widthPx", outputGrid.width_px},
            {"heightPx", outputGrid.height_px},
            {"modelPixels", layer_model_pixels},
            {"supportPixels", layer_support_pixels},
            {"textureSurfacePixels", diagnostics.semantic.texture_surface_pixels},
            {"unprintableWhiteCarrierPixels",
             diagnostics.semantic.unprintable_white_carrier_pixels},
            {"modelFillPixels", diagnostics.semantic.model_fill_pixels},
            {"internalVoidSupportPixels", diagnostics.semantic.internal_void_support_pixels},
            {"upperSurfaceSupportPixels", diagnostics.upper_projection_support_pixels},
            {"projectionBaseSupportPixels", diagnostics.projection_base_support_pixels},
            {"outerVarnishPixels", diagnostics.semantic.outer_varnish_pixels},
            {"outerSurfaceVarnishPixels", diagnostics.semantic.outer_surface_varnish_pixels},
            {"innerSurfaceVarnishPixels", diagnostics.semantic.inner_surface_varnish_pixels},
            {"semantic", reports::semantic_stats_to_json(diagnostics.semantic)},
        }));
        slice_layers.push_back(reports::layer_diagnostics_to_json(diagnostics));
        contour_layers.push_back(reports::layer_diagnostics_to_json(diagnostics));
        profile.layer_compute_ms += progress::ElapsedMsSince(layerMetadataStart);
        if (options.ownedlayercallback)
        {
            SliceRunOwnedLayer ownedLayer;
            ownedLayer.output.layerIndex = layer_index;
            ownedLayer.output.zMm = diagnostics.z_mm;
            ownedLayer.output.widthPx = grid.width_px;
            ownedLayer.output.heightPx = grid.height_px;
            ownedLayer.output.channels = std::move(layer);
            ownedLayer.semantic = std::move(materialClosureInput);
            // MW3-05：启用 T 时把本层缩裹占位一并交出。
            // transferMask 是会话里逐层复用的暂存缓冲，此处必须【复制】，
            // 下一层会把它覆盖掉。
            if (transferSession.has_value())
            {
                ownedLayer.transfermask =
                    transferSession.value().transferMask;
            }
            const SliceRunLayerConsumeResult consumeResult =
                options.ownedlayercallback(std::move(ownedLayer));
            switch (consumeResult.status)
            {
            case SliceRunLayerConsumeStatus::Accepted:
                break;
            case SliceRunLayerConsumeStatus::Cancelled:
            case SliceRunLayerConsumeStatus::Failed:
                throw SliceRunLayerConsumerError(
                    consumeResult.status,
                    layer_index,
                    consumeResult.detail);
            default:
                throw SliceRunLayerConsumerError(
                    SliceRunLayerConsumeStatus::Failed,
                    layer_index,
                    "owned layer consumer returned an invalid status");
            }
        }
        const int completedLayers = layer_index + 1;
        if (progress::ShouldNotifyLayerProgress(completedLayers, grid.layer_count))
        {
            const int percent = 36 + (completedLayers * 56 / std::max(1, grid.layer_count));
            progress::NotifyProgress(
                options,
                run_start,
                "layer_processing",
                completedLayers,
                grid.layer_count,
                percent);
        }
    }
    profile.layer_compose_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "report_build", 0, 1, 92);
    phase_start = SlicerClock::now();

    model_report.three_mf.texture_sampled_pixels = texture_runtime.report.sampled_pixels;
    const char* productionAcceptance = transferSession.has_value()
        ? (IsTransferSceneProductionOptIn(config, options)
            ? "admitted"
            : "rgbwsvt_candidate_unvalidated")
        : "legacy_production";

    std::vector<MaterialProcessLayerStatistics> materialProcessLayers;
    materialProcessLayers.reserve(layer_diagnostics.size());
    for (const LayerDiagnostics& layer : layer_diagnostics)
    {
        materialProcessLayers.push_back(MaterialProcessLayerStatistics{
            layer.layer_index, static_cast<std::uint64_t>(layer.rgb_non_zero_pixels),
            layer.channel_stats[3U].print_pixels, layer.channel_stats[4U].print_pixels,
            layer.channel_stats[5U].print_pixels,
            layer.semantic.unprintable_white_carrier_pixels});
    }
    // MF-03B4B：bounded 统计不依赖 TIFF；此处逐字段转换，不能按相同布局 reinterpret。
    std::array<TiffChannelStats, rgbwsv_channel_count> reportChannelTotals{};
    for (std::size_t channel{0}; channel < rgbwsv_channel_count; ++channel)
    {
        const ChannelStats& src = total_channel_stats.at(channel);
        TiffChannelStats& dst = reportChannelTotals.at(channel);
        dst.print_pixels = src.print_pixels;
        dst.full_print_pixels = src.full_print_pixels;
        dst.partial_print_pixels = src.partial_print_pixels;
        dst.empty_pixels = src.empty_pixels;
        dst.min_value = src.min_value;
        dst.max_value = src.max_value;
    }
    Json material_process_report = BuildMaterialProcessReport(MaterialProcessReportRequest{
        &config, model_report.format, model_report.model_path,
        outputGrid.width_px, outputGrid.height_px, grid.layer_count,
        grid.pixel_size_x_mm, grid.pixel_size_y_mm,
        materialProcessLayers, reportChannelTotals});
    Json transfer_channel_report;
    if (transferSession && options.write_tiff_layers)
    {
        const std::uint64_t totalPixels = static_cast<std::uint64_t>(outputGrid.width_px)
            * static_cast<std::uint64_t>(outputGrid.height_px) * static_cast<std::uint64_t>(grid.layer_count);
        material_process_report = BuildRgbwsvtMaterialProcessReport(
            material_process_report, config.material_process_profile,
            transferLayerStatistics, totalTransferMaterialStatistics, totalPixels);
        transfer_channel_report = BuildLegacyTransferChannelReport(
            config.transfer_channel_policy, transferSession->plan,
            transferLayerStatistics, totalTransferChannelStatistics.value(),
            totalTransferMaterialStatistics);
    }
    const Json cross_section_material_stack_report =
        reports::BuildCrossSectionMaterialStackReport(
            config,
            total_semantic_stats,
            support_generation,
            support_placement_policy);
    // MV-08C：体积报告。未启用时同样产出骨架，与既有报告一致——
    // 「报告存在但为空」与「报告缺失」在下游是两种完全不同的信号。
    Json material_volume_report;
    if (materialVolumePlan.has_value())
    {
        MaterialVolumeReportInput volumeInput;
        volumeInput.plan = &materialVolumePlan.value();
        volumeInput.rgbTable = &materialVolumeRgbTable.value();
        volumeInput.policy = &config.material_volume_policy;
        volumeInput.topologyFacts = materialVolumePlan.value().TopologyFacts();
        volumeInput.layers = materialVolumeLayerStats;
        material_volume_report = BuildMaterialVolumeReport(volumeInput);
    }
    else
    {
        material_volume_report = BuildDisabledMaterialVolumeReport();
    }
    if (transferSession.has_value())
    {
        Json::Object volumeFields = material_volume_report.as_object();
        volumeFields["packageProtocol"] = "p0.rgbwsvt.1";
        material_volume_report = Json{std::move(volumeFields)};
    }
    Json material_closure_report;
    if (collectMaterialClosureExact
        && materialClosureExactLayers.size() == static_cast<std::size_t>(grid.layer_count))
    {
        material_closure_report = BuildMaterialClosureExactReport(
            config.material_closure,
            materialClosureExactLayers);
    }
    else if (collectMaterialClosureCandidate
             && materialClosureCandidateLayers.size() == static_cast<std::size_t>(grid.layer_count))
    {
        material_closure_report = BuildMaterialClosureCandidateReport(
            config.material_closure,
            materialClosureCandidateLayers);
    }
    else
    {
        material_closure_report = BuildMaterialClosureReportSkeleton(
            config.material_closure,
            grid.layer_count);
    }

    Json slice_report = Json::object({
        {"requestedPipelineMode", "legacy"},
        {"effectivePipelineMode", "legacy"},
        {"productionAcceptance", productionAcceptance},
        {"productionOutputWritten", options.write_tiff_layers},
        {"fallbackApplied", false},
        {"slicingMode", config.slicing_mode},
        {"grid",
         Json::object({
             {"widthPx", outputGrid.width_px},
             {"heightPx", outputGrid.height_px},
             {"layerCount", grid.layer_count},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
         })},
        {"totals",
         Json::object({
             {"modelPixels", total_model_pixels},
             {"supportPixels", total_support_pixels},
             {"rgbNonZeroPixels", total_rgb_non_zero_pixels},
             {"whiteNonZeroPixels", total_white_non_zero_pixels},
             {"supportNonZeroPixels", total_support_non_zero_pixels},
             {"varnishNonZeroPixels", total_varnish_non_zero_pixels},
             {"modelPrintPixels", total_model_pixels},
             {"supportPrintPixels", total_support_non_zero_pixels},
             {"textureSurfacePixels", total_semantic_stats.texture_surface_pixels},
             {"unprintableWhiteCarrierPixels",
              total_semantic_stats.unprintable_white_carrier_pixels},
             {"modelFillPixels", total_semantic_stats.model_fill_pixels},
             {"internalVoidSupportPixels", total_semantic_stats.internal_void_support_pixels},
             {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
             {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
             {"outerVarnishPixels", total_semantic_stats.outer_varnish_pixels},
             {"outerSurfaceVarnishPixels", total_semantic_stats.outer_surface_varnish_pixels},
             {"innerSurfaceVarnishPixels", total_semantic_stats.inner_surface_varnish_pixels},
             {"rgbPrintPixels", total_rgb_non_zero_pixels},
             {"whitePrintPixels", total_white_non_zero_pixels},
             {"varnishPrintPixels", total_varnish_non_zero_pixels},
             {"islandCount", support_generation.island_count},
             {"islandPixels", support_generation.island_pixels},
             {"unsupportedPixels", support_generation.unsupported_pixels},
             {"filteredIslandCount", support_generation.filtered_island_count},
             {"filteredIslandPixels", support_generation.filtered_island_pixels},
             {"supportTypeStats",
              Json::object({
                  {"bottom_projection", support_generation.bottom_projection_support_pixels},
                  {"unsupported_island", support_generation.unsupported_island_support_pixels},
                  {"full_vertical_projection", support_generation.full_vertical_projection_support_pixels},
                  {"internal_void", support_generation.internal_void_support_pixels},
                  {"upper_projection", support_generation.upper_projection_support_pixels},
                  {"projection_base", support_generation.projection_base_support_pixels},
              })},
             {"supportConnectivity", reports::support_connectivity_summary_to_json(layer_diagnostics)},
             {"channelStats", reports::channel_stats_array_to_json(total_channel_stats)},
             {"texture",
              Json::object({
                  {"enabled", texture_runtime.report.enabled},
                  {"sampledPixels", texture_runtime.report.sampled_pixels},
                  {"fallbackPixels", texture_runtime.report.fallback_pixels},
                  {"uvOutOfRangePixels", texture_runtime.report.uv_out_of_range_pixels},
              })},
             {"materialPolicyApplied", config.material_policy.enabled},
             {"materialPolicy",
              Json::object({
                  {"enabled", config.material_policy.enabled},
                  {"rgbPrintPixels", material_policy_report.rgb_print_pixels},
                  {"whitePrintPixels", material_policy_report.white_print_pixels},
                  {"varnishPrintPixels", material_policy_report.varnish_print_pixels},
              })},
             {"semantic", reports::semantic_stats_to_json(total_semantic_stats)},
             {"materialSemantics",
              Json::object({
                  {"modelFill",
                   Json::object({
                       {"enabled", config.model_fill.enabled},
                       {"material", config.model_fill.material},
                       {"scope", config.model_fill.scope},
                       {"value", static_cast<int>(config.model_fill.value)},
                       {"emptyAllowedInProduction", config.model_fill.empty_allowed_in_production},
                       {"legacyRgbFallback", config.model_fill.legacy_rgb_fallback},
                       {"resolvedProfileDefaultMaterial",
                        materials::ModelFillMaterialToString(materials::ResolveProfileDefaultModelFillMaterial(config))},
                   })},
                  {"supportPlacement", config.support.placement},
                  {"supportPlacementPolicy",
                   Json::object({
                       {"requested", support_placement_policy.requested_placement},
                       {"effective", support_placement_policy.effective_placement},
                       {"placementExplicit", support_placement_policy.placement_explicit},
                       {"lowerEnabled", support_placement_policy.lower_enabled},
                       {"upperEnabled", support_placement_policy.upper_enabled},
                       {"unsupportedOnlyEnabled", support_placement_policy.unsupported_only_enabled},
                       {"fullVerticalProjectionEnabled",
                        support_placement_policy.full_vertical_projection_enabled},
                       {"advancedDebug", support_placement_policy.advanced_debug},
                       {"upperBoundarySource", upper_support_boundary_info.source},
                       {"upperBoundaryIncludesOuterVarnishShell",
                        upper_support_boundary_info.includes_outer_varnish_shell},
                       {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
                   })},
                  {"internalVoidSupport",
                   Json::object({
                       {"enabled", config.support.internal_void.enabled},
                       {"fillRule", config.support.internal_void.fill_rule},
                       {"minAreaPx", config.support.internal_void.min_area_px},
                       {"printPixels", total_semantic_stats.internal_void_support_pixels},
                   })},
                  {"supportBaseProjection",
                   support::BuildSupportBaseProjectionReport(
                       supportBaseProjectionResult,
                       support_generation)},
                  {"outerVarnish",
                   Json::object({
                       {"enabled", config.outer_varnish.enabled},
                       {"thicknessMm", config.outer_varnish.thickness_mm},
                       {"requestedThicknessMm",
                        config.outer_varnish.thickness_mm},
                       {"thicknessPx",
                        std::max(
                            outerVarnishDiscretization.radius_x_px,
                            outerVarnishDiscretization.radius_y_px)},
                       {"radiusXPx", outerVarnishDiscretization.radius_x_px},
                       {"radiusYPx", outerVarnishDiscretization.radius_y_px},
                       {"effectiveThicknessXmm",
                        outerVarnishDiscretization.effective_thickness_x_mm},
                       {"effectiveThicknessYmm",
                        outerVarnishDiscretization.effective_thickness_y_mm},
                       {"effectiveThicknessMm",
                        std::max(
                            outerVarnishDiscretization.effective_thickness_x_mm,
                            outerVarnishDiscretization.effective_thickness_y_mm)},
                       {"pixelSizeXmm",
                        outerVarnishDiscretization.pixel_size_x_mm},
                       {"pixelSizeYmm",
                        outerVarnishDiscretization.pixel_size_y_mm},
                       {"pixelPitchSource", "output_dpi"},
                       {"legacyConfigPixelPitchUm",
                        config.outer_varnish.pixel_pitch_um},
                       {"pixelPitchUm",
                        config.outer_varnish.pixel_pitch_um},
                       {"allowXYExpansion", config.outer_varnish.allow_xy_expansion},
                       {"conflictPolicy", config.outer_varnish.conflict_policy},
                       {"value", static_cast<int>(config.outer_varnish.value)},
                       {"printPixels", total_semantic_stats.outer_varnish_pixels},
                   })},
                  {"surfaceVarnish",
                   Json::object({
                       {"enabled", config.surface_varnish.enabled},
                       {"outerSurface", config.surface_varnish.outer_surface},
                       {"innerSurface", config.surface_varnish.inner_surface},
                       {"thicknessPx", config.surface_varnish.thickness_px},
                       {"source", config.surface_varnish.source},
                       {"value", static_cast<int>(materials::ResolveSurfaceVarnishValue(config))},
                       {"outerSurfacePrintPixels", total_semantic_stats.outer_surface_varnish_pixels},
                       {"innerSurfacePrintPixels", total_semantic_stats.inner_surface_varnish_pixels},
                       {"note", "SurfaceVarnishLayer writes V on model pixels and is distinct from OuterVarnishShell"},
                   })},
                  {"semanticPriority", "Model>OuterVarnishShell>Support>Empty"},
              })},
             {"crossSectionMaterialStack", cross_section_material_stack_report},
             {"materialClosure", BuildMaterialClosureSliceSummary(material_closure_report)},
             {"singleMaterialConsistency", reports::BuildSingleMaterialConsistencyHint(config)},
         })},
        {"layers", Json{slice_layers}},
    });
    if (transferSession.has_value() && options.write_tiff_layers)
    {
        slice_report = BuildRgbwsvtSliceReport(
            slice_report, transferLayerStatistics,
            totalTransferChannelStatistics.value(), totalTransferMaterialStatistics);
    }

    const Json repair_report = Json::object({
        {"status", "not_required_p0_lite"},
        {"operations", Json::Array{}},
        {"note", "P0 performs lightweight load validation and bbox extraction only"},
    });

    const Json support_report = Json::object({
        {"enabled", config.support.enabled},
        {"mode", config.support.mode},
        {"supportMode", config.support.mode},
        {"placement", config.support.placement},
        {"placementPolicy",
         Json::object({
             {"requested", support_placement_policy.requested_placement},
             {"effective", support_placement_policy.effective_placement},
             {"placementExplicit", support_placement_policy.placement_explicit},
             {"lowerEnabled", support_placement_policy.lower_enabled},
             {"upperEnabled", support_placement_policy.upper_enabled},
             {"unsupportedOnlyEnabled", support_placement_policy.unsupported_only_enabled},
             {"fullVerticalProjectionEnabled", support_placement_policy.full_vertical_projection_enabled},
             {"advancedDebug", support_placement_policy.advanced_debug},
             {"upperBoundarySource", upper_support_boundary_info.source},
             {"upperBoundaryIncludesOuterVarnishShell",
              upper_support_boundary_info.includes_outer_varnish_shell},
             {"upperOutsideBoundary",
              config.outer_varnish.enabled ? "outer_varnish_shell" : config.support.upper.outside},
         })},
        {"supportReasons",
         Json::object({
             {"bottom_projection", "lower_surface_support"},
             {"unsupported_island", "unsupported_island_projection"},
             {"full_vertical_projection", "advanced_debug_full_vertical_projection"},
             {"internal_void", "internal_void"},
             {"upper_projection", "upper_detachable_surface_support"},
             {"projection_base", "max_support_footprint_base"},
         })},
        {"value", config.support.value},
        {"minOverlapRatio", config.support.min_overlap_ratio},
        {"minIslandAreaPx", config.support.min_island_area_px},
        {"connectivity", config.support.connectivity},
        {"unsupportedProjection", config.support.unsupported_projection},
        {"xyDilationPx", config.support.xy_dilation_px},
        {"internalVoid",
         Json::object({
             {"enabled", config.support.internal_void.enabled},
             {"fillRule", config.support.internal_void.fill_rule},
             {"minAreaPx", config.support.internal_void.min_area_px},
             {"reason", "internal_void"},
             {"printPixels", support_generation.internal_void_support_pixels},
         })},
        {"baseProjection",
         support::BuildSupportBaseProjectionReport(
             supportBaseProjectionResult,
             support_generation)},
        {"shape",
         Json::object({
             {"enabled", support_shape_policy.enabled},
             {"minComponentAreaPx", support_shape_policy.min_component_area_px},
             {"xyDilationPx", support_shape_policy.xy_dilation_px},
             {"closingRadiusPx", support_shape_policy.closing_radius_px},
             {"bridgeGapPx", support_shape_policy.bridge_gap_px},
             {"preserveModelPriority", support_shape_policy.preserve_model_priority},
             {"maxAddedSupportRatio", support_shape_policy.max_added_support_ratio},
             {"addedSupportPixels", support_shape_result.added_support_pixels},
             {"removedSupportPixels", support_shape_result.removed_support_pixels},
         })},
        {"slicingMode", config.slicing_mode},
        {"supportSource",
         config.slicing_mode == "relief_heightfield" ? "relief_lower_surface" : "first_model_layer"},
        {"modelPriority", "Model > Support"},
        {"supportPixels", total_support_pixels},
        {"supportPrintPixels", total_support_non_zero_pixels},
        {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
        {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
        {"columnsWithSupport", columns_with_support},
        {"islandCount", support_generation.island_count},
        {"islandPixels", support_generation.island_pixels},
        {"unsupportedPixels", support_generation.unsupported_pixels},
        {"filteredIslandCount", support_generation.filtered_island_count},
        {"filteredIslandPixels", support_generation.filtered_island_pixels},
        {"layersWithIslands", support_generation.layers_with_islands},
        {"layersWithSupport", support_generation.layers_with_support},
        {"supportConnectivity", reports::support_connectivity_summary_to_json(layer_diagnostics)},
        {"totals",
         Json::object({
             {"supportPixels", total_support_pixels},
             {"supportPrintPixels", total_support_non_zero_pixels},
             {"islandCount", support_generation.island_count},
             {"islandPixels", support_generation.island_pixels},
             {"unsupportedPixels", support_generation.unsupported_pixels},
             {"filteredIslandCount", support_generation.filtered_island_count},
             {"filteredIslandPixels", support_generation.filtered_island_pixels},
             {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
             {"upperProjectionPixels", support_generation.upper_projection_support_pixels},
             {"projectionBaseSupportPixels", support_generation.projection_base_support_pixels},
         })},
        {"supportTypeStats",
         Json::object({
             {"bottom_projection", support_generation.bottom_projection_support_pixels},
             {"unsupported_island", support_generation.unsupported_island_support_pixels},
             {"full_vertical_projection", support_generation.full_vertical_projection_support_pixels},
             {"internal_void", support_generation.internal_void_support_pixels},
             {"upper_projection", support_generation.upper_projection_support_pixels},
             {"projection_base", support_generation.projection_base_support_pixels},
         })},
        {"layers", Json{contour_layers}},
    });
    const Json support_shape_report = MakeSupportShapeReport(support_shape_policy, support_shape_result);

    Json::Array preview_channels;
    for (const std::string& channel : config.preview.channels) {
        preview_channels.push_back(preview::canonical_preview_channel(channel));
    }
    const auto color_json = [](const std::array<std::uint8_t, 3>& color) {
        return Json::array({static_cast<int>(color.at(0)), static_cast<int>(color.at(1)), static_cast<int>(color.at(2))});
    };

    Json::Object previewPseudoColors = Json::object({
        {"empty", color_json(config.preview.empty_color)},
        {"support", color_json(config.preview.support_color)},
        {"white", color_json(config.preview.white_color)},
        {"varnish", color_json(config.preview.varnish_color)}}).as_object();
    if (transferSession.has_value())
    {
        previewPseudoColors["transfer"] = color_json(config.preview.transfer_color);
    }
    const Json preview_report = Json::object({
        {"schema", "p0.preview_report.1"},
        {"outputPolicy", effective_preview_output_policy},
        {"productionSource", transferSession.has_value() ? "rgbwsvt_tiff" : "rgbwsv_tiff"},
        {"automaticDiagnosticImages", automatic_diagnostic_images},
        {"enabled", automatic_diagnostic_images},
        {"format", config.preview.format},
        {"interval", config.preview.interval},
        {"channels", Json{preview_channels}},
        {"pseudoColors", Json{std::move(previewPseudoColors)}},
        {"layerRange",
         config.preview.has_layer_range ? Json::array({config.preview.layer_range.at(0), config.preview.layer_range.at(1)})
                                        : Json{}},
        {"onlyNonEmptyLayers", config.preview.only_non_empty_layers},
        {"files", Json{preview_files}},
        {"generated", Json{preview_files}},
    });

    Json::Array material_libraries;
    for (const std::string& library : model_report.material_libraries) {
        material_libraries.push_back(library);
    }
    Json::Array materials;
    for (const MaterialStat& material : model_report.materials) {
        materials.push_back(Json::object({
            {"name", material.name},
            {"faceCount", static_cast<std::uint64_t>(material.face_count)},
            {"triangleCount", static_cast<std::uint64_t>(material.triangle_count)},
        }));
    }
    Json::Array material_infos;
    for (const MaterialInfo& material : model_report.material_infos) {
        material_infos.push_back(Json::object({
            {"name", material.name},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", reports::rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", slicer_core::PathToUtf8(material.diffuse_texture_path)},
            {"textureExists", material.texture_exists},
        }));
    }

    const Json model_json = Json::object({
        {"modelPath", slicer_core::PathToUtf8(model_report.model_path)},
        {"format", model_report.format},
        {"stlEncoding", model_report.stl_encoding},
        {"vertexCount", static_cast<std::uint64_t>(model_report.vertex_count)},
        {"faceCount", static_cast<std::uint64_t>(model_report.face_count)},
        {"triangleCount", static_cast<std::uint64_t>(model_report.triangle_count)},
        {"degenerateTriangleCount", static_cast<std::uint64_t>(model_report.degenerate_triangle_count)},
        {"materialCount", static_cast<std::uint64_t>(model_report.materials.size())},
        {"texcoordCount", static_cast<std::uint64_t>(model_report.texcoord_count)},
        {"facesWithUv", static_cast<std::uint64_t>(model_report.faces_with_uv)},
        {"facesWithoutUv", static_cast<std::uint64_t>(model_report.faces_without_uv)},
        {"materialLibraries", Json{material_libraries}},
        {"materials", Json{materials}},
        {"materialInfos", Json{material_infos}},
        {"autoOrient",
         Json::object({
             {"enabled", model_report.auto_orient.enabled},
             {"applied", model_report.auto_orient.applied},
             {"maxHeightMm", model_report.auto_orient.max_height_mm},
             {"selectedOrientation", model_report.auto_orient.selected_orientation},
             {"rotationDeg",
              Json::array({
                  model_report.auto_orient.rotation_deg.at(0U),
                  model_report.auto_orient.rotation_deg.at(1U),
                  model_report.auto_orient.rotation_deg.at(2U)})},
             {"originalBboxMm", reports::bbox_to_json(model_report.auto_orient.original_bbox_mm)},
         })},
        {"bboxMm", reports::bbox_to_json(model_report.bbox_mm)},
    });
    const Json package_report = MakeReportBase(
        "p0.report.package.1",
        Json::object({
            {"component", "slicer_core"},
            {"packageDir", slicer_core::PathToUtf8(package_dir)},
        }),
        Json::object({
            {"configPath", slicer_core::PathToUtf8(config_path)},
            {"modelPath", slicer_core::PathToUtf8(model_report.model_path)},
            {"schema", config.output.package_protocol},
        }));

    const RgbwsvtChannelStatistics* persistedTransferStatistics =
        options.write_tiff_layers && totalTransferChannelStatistics.has_value()
        ? &totalTransferChannelStatistics.value() : nullptr;
    const Json tiff_json = BuildLegacyTiffManifestMetadata(
        config.output, layers, options.write_tiff_layers, persistedTransferStatistics);

    Json::Object reportPaths = Json::object({
        {"package", "reports/package_report.json"},
        {"model", "reports/model_report.json"},
        {"slice", "reports/slice_report.json"},
        {"repair", "reports/repair_report.json"},
        {"support", "reports/support_report.json"},
        {"supportShape", "reports/support_shape_report.json"},
        {"preview", "reports/preview_report.json"},
        {"texture", "reports/texture_report.json"},
        {"materialPolicy", "reports/material_policy_report.json"},
        {"materialProcess", "reports/material_process_report.json"},
        {"crossSectionMaterialStack", "reports/cross_section_material_stack_report.json"},
        {"materialClosure", "reports/material_closure_report.json"},
        {"materialVolume", "reports/material_volume_report.json"},
        {"materialRoleMapping", "reports/material_role_mapping_report.json"},
        {"objMtlMaterial", "reports/obj_mtl_material_report.json"},
        {"threeMf", "reports/three_mf_report.json"},
        {"contour", "reports/contour_report.json"},
        {"relief", "reports/relief_report.json"}}).as_object();
    if (transferSession.has_value())
    {
        reportPaths["transferChannel"] = "reports/transfer_channel_report.json";
    }
    Json::Object manifest_fields = Json::object({
        {"schema", config.output.package_protocol},
        {"schemaVersion", config.output.package_protocol},
        {"requestedPipelineMode", "legacy"},
        {"effectivePipelineMode", "legacy"},
        {"productionAcceptance", productionAcceptance},
        {"productionOutputWritten", options.write_tiff_layers},
        {"fallbackApplied", false},
        {"source",
         Json::object({
             {"configPath", slicer_core::PathToUtf8(config_path)},
             {"modelPath", slicer_core::PathToUtf8(model_report.model_path)},
             {"format", model_report.format},
             {"engine", "legacy"},
         })},
        {"grid",
         Json::object({
             {"widthPx", outputGrid.width_px},
             {"heightPx", outputGrid.height_px},
             {"layerCount", grid.layer_count},
             {"dpiX", config.output.dpi_x},
             {"dpiY", config.output.dpi_y},
             {"dpi", Json::array({config.output.dpi_x, config.output.dpi_y})},
             {"pixelSizeXmm", grid.pixel_size_x_mm},
             {"pixelSizeYmm", grid.pixel_size_y_mm},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
             {"originMm", Json::array({outputGrid.origin_x_mm, outputGrid.origin_y_mm, 0.0})},
         })},
        {"slicing",
         Json::object({
             {"mode", config.slicing_mode},
             {"reliefFillMode", config.relief.fill_mode},
         })},
        {"tiff", tiff_json},
        {"layers", Json{layers}},
        {"reports", Json{std::move(reportPaths)}},
        {"preview",
         Json::object({
             {"outputPolicy", effective_preview_output_policy},
             {"productionSource", transferSession.has_value() ? "rgbwsvt_tiff" : "rgbwsv_tiff"},
             {"automaticDiagnosticImages", automatic_diagnostic_images},
             {"enabled", automatic_diagnostic_images},
             {"format", config.preview.format},
             {"files", Json{preview_files}},
         })},
    }).as_object();
    const std::optional<std::string> white_semantics =
        ResolveWhiteSemantics(config);
    if (white_semantics.has_value())
    {
        manifest_fields["whiteSemantics"] = *white_semantics;
    }
    // Profile 溯源回声。包摘要与宿主结果页都以它承载「这一包由哪份 Profile 切出」；
    // 此前 legacy 发布路径从不写，于是七通道包（该路径是其唯一出口）始终缺失该字段。
    //
    // 仅在两者【都】非空时发射：CLI 直接喂配置文件的场景本就没有 Profile 溯源，
    // 此时不写比写一个占位值诚实；同时既有 golden 配置均无 profileVersion，
    // 故此举不改变它们的包字节，MV-09 建立的零漂移结论不受影响。
    //
    // 注意本处【不发射】perInstance：该字段要求 instanceId/modelId/layerRange 等
    // 场景实例信息，而宿主有效 Profile 并不携带（HostRequestBuilder 无 instanceId），
    // legacy 发布路径也无从得知。编一个 "instance-0" 只会把「缺失」变成「伪造的溯源」，
    // 后者更难发现。该缺口需由场景侧传入实例上下文才能真正补上。
    if (!config.profile_version.empty() && !config.profile_hash.empty())
    {
        manifest_fields["profileEcho"] = Json::object({
            {"profileVersion", config.profile_version},
            {"profileHash", config.profile_hash},
        });
    }
    const Json manifest{std::move(manifest_fields)};
    profile.report_build_ms = progress::ElapsedMsSince(phase_start);
    progress::NotifyProgress(options, run_start, "report_write", 0, 1, 95);
    phase_start = SlicerClock::now();

    if (options.write_reports) {
        reports::write_json_file(package_dir / "reports/model_report.json", model_json);
        reports::write_json_file(package_dir / "reports/package_report.json", package_report);
        reports::write_json_file(package_dir / "reports/slice_report.json", slice_report);
        reports::write_json_file(package_dir / "reports/repair_report.json", repair_report);
        reports::write_json_file(package_dir / "reports/support_report.json", support_report);
        reports::write_json_file(package_dir / "reports/support_shape_report.json", support_shape_report);
        reports::write_json_file(package_dir / "reports/preview_report.json", preview_report);
        reports::write_json_file(package_dir / "reports/texture_report.json", reports::texture_report_to_json(texture_runtime.report));
        reports::write_json_file(
            package_dir / "reports/material_policy_report.json",
            reports::material_policy_report_to_json(config, material_policy_report));
        reports::write_json_file(package_dir / "reports/material_process_report.json", material_process_report);
        if (transferSession.has_value())
        {
            reports::write_json_file(
                package_dir / "reports/transfer_channel_report.json",
                transfer_channel_report);
        }
        reports::write_json_file(package_dir / "reports/cross_section_material_stack_report.json", cross_section_material_stack_report);
        reports::write_json_file(package_dir / "reports/material_closure_report.json", material_closure_report);
        reports::write_json_file(
            package_dir / "reports/material_volume_report.json", material_volume_report);
        reports::write_json_file(
            package_dir / "reports/material_role_mapping_report.json",
            reports::material_role_mapping_report_to_json(material_role_mapping_report));
        reports::write_json_file(
            package_dir / "reports/obj_mtl_material_report.json",
            reports::obj_mtl_material_report_to_json(model_report));
        reports::write_json_file(
            package_dir / "reports/three_mf_report.json",
            reports::three_mf_report_to_json(model_report));
        reports::write_json_file(package_dir / "reports/contour_report.json", Json::object({{"layers", Json{contour_layers}}}));
        reports::write_json_file(
            package_dir / "reports/relief_report.json",
            reports::relief_report_to_json(config, relief_report, total_support_pixels, columns_with_support));
        reports::write_json_file(package_dir / "manifest.json", manifest);
    }
    if (transferPackageGuard.has_value())
    {
        transferPackageGuard->Commit();
    }
    profile.report_write_ms = progress::ElapsedMsSince(phase_start);
    profile.slice_processing_ms =
        profile.grid_setup_ms
        + profile.mask_sampling_ms
        + profile.texture_prepare_ms
        + profile.support_generation_ms
        + profile.layer_compute_ms;
    profile.output_write_ms =
        profile.tiff_write_ms
        + profile.preview_write_ms
        + profile.report_write_ms
        + profile.package_publish_ms;
    profile.total_ms = progress::ElapsedMsSince(run_start);
    progress::NotifyProgress(options, run_start, "completed", 1, 1, 100);

    SliceRunResult result;
    result.package_dir = package_dir;
    result.effective_pipeline_mode = "legacy";
    result.width_px = outputGrid.width_px;
    result.height_px = outputGrid.height_px;
    result.layer_count = grid.layer_count;
    result.model_pixel_count = total_model_pixels;
    result.support_pixel_count = total_support_pixels;
    result.profile = profile;
    return result;
}

}  // namespace slicer_core
