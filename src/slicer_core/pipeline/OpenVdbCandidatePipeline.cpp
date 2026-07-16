#include "slicer_core/pipeline/OpenVdbCandidatePipeline.h"

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/material/MaterialChannelComposer.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelReport.h"
#include "slicer_core/model.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"
#include "slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{
namespace
{

using PipelineClock = std::chrono::steady_clock;

double ElapsedMsSince(const PipelineClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(PipelineClock::now() - start).count();
}

void NotifyProgress(
    const OpenVdbCandidatePipelineOptions& options,
    const PipelineClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent)
{
    if (!options.progress_callback)
    {
        return;
    }

    options.progress_callback(SliceRunProgress{
        phase,
        current,
        total,
        percent,
        ElapsedMsSince(runStart)});
}

bool ShouldNotifyLayerProgress(const int completedLayers, const int layerCount)
{
    if (completedLayers <= 1 || completedLayers >= layerCount)
    {
        return true;
    }

    const int interval = std::max(1, (layerCount + 99) / 100);
    return completedLayers % interval == 0;
}

std::string LayerFileName(const int layerIndex)
{
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layerIndex << ".tiff";
    return stream.str();
}

std::string PreviewFileName(const int layerIndex, const std::string& channel)
{
    std::ostringstream stream;
    stream << "preview/" << channel << "_" << std::setw(6) << std::setfill('0') << layerIndex << ".ppm";
    return stream.str();
}

std::string UniqueDirectorySuffix()
{
    const auto tickCount = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::to_string(tickCount);
}

std::filesystem::path MakeSiblingDirectory(
    const std::filesystem::path& packageDir,
    const std::string& marker)
{
    const std::filesystem::path parentDir = packageDir.parent_path();
    const std::string baseName = packageDir.filename().string();
    return parentDir / (baseName + "." + marker + "." + UniqueDirectorySuffix());
}

void PublishStagedPackage(
    const std::filesystem::path& stagingDir,
    const std::filesystem::path& packageDir)
{
    const std::filesystem::path parentDir = packageDir.parent_path();
    if (!parentDir.empty())
    {
        std::filesystem::create_directories(parentDir);
    }

    std::filesystem::path backupDir;
    if (std::filesystem::exists(packageDir))
    {
        backupDir = MakeSiblingDirectory(packageDir, "previous");
        std::filesystem::rename(packageDir, backupDir);
    }

    try
    {
        std::filesystem::rename(stagingDir, packageDir);
    }
    catch (...)
    {
        if (!backupDir.empty() && !std::filesystem::exists(packageDir) && std::filesystem::exists(backupDir))
        {
            std::filesystem::rename(backupDir, packageDir);
        }
        throw;
    }
}

Json StringArrayToJson(const std::array<std::string, 6>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return Json{array};
}

Json ColorToJson(const std::array<std::uint8_t, 3>& color)
{
    return Json::array({
        static_cast<int>(color.at(0)),
        static_cast<int>(color.at(1)),
        static_cast<int>(color.at(2)),
    });
}

Json::Array StringsToJsonArray(const std::vector<std::string>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return array;
}

Json LayerStatsToJson(const OpenVdbCandidateLayerBufferStats& stats)
{
    return Json::object({
        {"layerIndex", stats.layer_index},
        {"supportPixels", stats.support_pixels},
        {"modelPixels", stats.model_pixels},
        {"shellPixels", stats.shell_pixels},
        {"interiorPixels", stats.interior_pixels},
        {"whitePixels", stats.white_pixels},
        {"varnishPixels", stats.varnish_pixels},
        {"clearedSupportConflictPixels", stats.cleared_support_conflict_pixels},
    });
}

Json ComposerStatsToJson(const MaterialChannelComposerStats& stats)
{
    return Json::object({
        {"emptyPixels", stats.empty_pixels},
        {"supportPixels", stats.support_pixels},
        {"modelPixels", stats.model_pixels},
        {"surfaceRgbPixels", stats.surface_rgb_pixels},
        {"whitePixels", stats.white_pixels},
        {"varnishPixels", stats.varnish_pixels},
        {"modelSupportConflictPixels", stats.model_support_conflict_pixels},
    });
}

void WritePpm(
    const std::filesystem::path& path,
    const int width,
    const int height,
    const std::vector<std::uint8_t>& pixels)
{
    if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U)
    {
        throw std::runtime_error("candidate preview pixel buffer size mismatch");
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error("failed to write candidate preview: " + path.string());
    }
    output << "P6\n" << width << ' ' << height << "\n255\n";
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

std::size_t ChannelIndex(const std::size_t pixelIndex, const MaterialChannelOffset offset)
{
    return pixelIndex * static_cast<std::size_t>(rgbwsv_channel_count) + static_cast<std::size_t>(offset);
}

std::vector<std::uint8_t> BuildRgbPreviewPixels(
    const MaterialChannelComposerResult& composed)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(composed.width) * static_cast<std::size_t>(composed.height);
    std::vector<std::uint8_t> pixels(pixelCount * 3U, 255);
    for (std::size_t pixelIndex{0}; pixelIndex < pixelCount; ++pixelIndex)
    {
        const std::size_t targetIndex = pixelIndex * 3U;
        pixels.at(targetIndex + 0U) = composed.channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::R));
        pixels.at(targetIndex + 1U) = composed.channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::G));
        pixels.at(targetIndex + 2U) = composed.channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::B));
    }
    return pixels;
}

std::vector<std::uint8_t> BuildMaskPreviewPixels(
    const MaterialChannelComposerResult& composed,
    const MaterialChannelOffset offset,
    const std::uint8_t emptyValue,
    const std::array<std::uint8_t, 3>& emptyColor,
    const std::array<std::uint8_t, 3>& printColor)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(composed.width) * static_cast<std::size_t>(composed.height);
    std::vector<std::uint8_t> pixels(pixelCount * 3U, 255);
    for (std::size_t pixelIndex{0}; pixelIndex < pixelCount; ++pixelIndex)
    {
        const bool isPrint =
            composed.channels.at(ChannelIndex(pixelIndex, offset)) != emptyValue;
        const std::array<std::uint8_t, 3>& color = isPrint ? printColor : emptyColor;
        const std::size_t targetIndex = pixelIndex * 3U;
        pixels.at(targetIndex + 0U) = color.at(0);
        pixels.at(targetIndex + 1U) = color.at(1);
        pixels.at(targetIndex + 2U) = color.at(2);
    }
    return pixels;
}

void WritePreviewFrame(
    const std::filesystem::path& packageDir,
    Json::Array& previewFiles,
    const int width,
    const int height,
    const int layerIndex,
    const std::string& filePrefix,
    const std::string& channel,
    const std::string& type,
    const std::vector<std::uint8_t>& pixels,
    const int printPixels)
{
    const std::string relativePreviewPath = PreviewFileName(layerIndex, filePrefix);
    WritePpm(packageDir / relativePreviewPath, width, height, pixels);
    previewFiles.push_back(Json::object({
        {"layerIndex", layerIndex},
        {"channel", channel},
        {"type", type},
        {"kind", "single"},
        {"format", "ppm"},
        {"path", relativePreviewPath},
        {"printPixels", printPixels},
    }));
}

void EnsureCandidateConfig(const SliceConfig& config)
{
    if (!config.experimental.openvdb_pipeline.enabled
        || config.experimental.openvdb_pipeline.engine != "openvdb"
        || config.texture.apply_mode != "surface_shell_from_sdf"
        || !config.experimental.openvdb_pipeline.write_production_rgbwsv)
    {
        throw std::runtime_error(
            "OpenVDB candidate package requires enabled=true, engine=openvdb, "
            "texture.applyMode=surface_shell_from_sdf and writeProductionRgbwsv=true");
    }
    if (config.experimental.openvdb_pipeline.admission_mode != "strict_closed")
    {
        throw std::runtime_error("OpenVDB candidate package requires admissionMode=strict_closed");
    }
}

TiffImageSpec MakeTiffSpec(const SliceConfig& config, const int width, const int height)
{
    TiffImageSpec spec;
    spec.width = static_cast<std::uint32_t>(width);
    spec.height = static_cast<std::uint32_t>(height);
    spec.tile_width = static_cast<std::uint32_t>(config.output.tile_size.at(0));
    spec.tile_height = static_cast<std::uint32_t>(config.output.tile_size.at(1));
    spec.rows_per_strip = static_cast<std::uint32_t>(config.output.rows_per_strip);
    spec.storage_mode =
        config.output.storage_mode == "tiled" ? TiffStorageMode::Tiled : TiffStorageMode::Stripped;
    return spec;
}

SurfaceShellRealModelResult RunCandidatePrototype(
    const SliceConfig& config,
    const std::filesystem::path& configPath,
    const MeshValidationPolicy meshPolicy)
{
    const std::filesystem::path configDir =
        configPath.parent_path().empty() ? std::filesystem::current_path() : configPath.parent_path();
    const SceneModel scene = load_model_report(config, configDir);

    SurfaceShellRealModelOptions options;
    options.voxel_size_mm = 0.05;
    options.shell_thickness_mm = 0.10;
    options.max_transfer_distance_mm = options.shell_thickness_mm + options.voxel_size_mm * 2.0;
    options.mesh_policy = meshPolicy;
    options.fallback_rgb = config.texture.fallback_rgb;
    options.texture_sample.sampler = config.texture.sampler;
    options.texture_sample.uv_address_mode = config.texture.uv_address_mode;
    options.texture_sample.flip_v = config.texture.flip_v;

    SurfaceShellRealModelResult result = RunSurfaceShellRealModelPrototype(scene, config, options);
    result.config_path = configPath.generic_string();
    return result;
}

bool ShouldAttemptNonProductionFallback(const SliceConfig& config)
{
    return config.experimental.openvdb_pipeline.allow_non_production_output
        && config.experimental.openvdb_pipeline.failure_policy == "non_production_only";
}

Json MakeTextureFidelityReport(const SurfaceShellRealModelResult& result)
{
    const SurfaceTextureTransferStats& stats = result.transfer.stats;
    return Json::object({
        {"schema", "p0.openvdb_candidate.texture_fidelity.1"},
        {"sampledTextureVoxels", stats.sampled_texture_voxels},
        {"fallbackVoxels", stats.fallback_voxels},
        {"missingUvVoxels", stats.missing_uv_voxels},
        {"missingTextureVoxels", stats.missing_texture_voxels},
        {"uvOutOfRangeVoxels", stats.uv_out_of_range_voxels},
        {"uniqueColorCount", stats.unique_color_count},
        {"loadedTextureCount", stats.loaded_texture_count},
    });
}

void WriteCandidateFailureReports(
    const SliceConfig& config,
    const SurfaceShellRealModelResult& prototype,
    const std::string& stage)
{
    const std::filesystem::path reportDir = config.output.package_dir / "reports";
    std::filesystem::create_directories(reportDir);

    const Json realModelReport = MakeSurfaceShellRealModelReport(prototype);
    WriteReportJsonFile(reportDir / "surface_shell_texture_report.json", realModelReport);
    WriteReportJsonFile(
        reportDir / "production_admission_report.json",
        realModelReport.at("productionAdmission"));
    WriteReportJsonFile(
        reportDir / "openvdb_candidate_report.json",
        Json::object({
            {"schema", "p0.openvdb_candidate_report.1"},
            {"productionPackageWritten", false},
            {"status", "blocked"},
            {"stage", stage},
            {"message", prototype.errors.empty() ? "OpenVDB candidate package was blocked" : prototype.errors.front()},
            {"errors", Json{StringsToJsonArray(prototype.errors)}},
            {"warnings", Json{StringsToJsonArray(prototype.warnings)}},
            {"productionAdmission", realModelReport.at("productionAdmission")},
            {"reports",
             Json::object({
                 {"surfaceShellTexture", "reports/surface_shell_texture_report.json"},
                 {"productionAdmission", "reports/production_admission_report.json"},
             })},
        }));
}

}  // namespace

OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(const std::filesystem::path& configPath)
{
    return RunOpenVdbCandidatePipeline(configPath, OpenVdbCandidatePipelineOptions{});
}

OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(
    const std::filesystem::path& configPath,
    const OpenVdbCandidatePipelineOptions& options)
{
    SliceRunProfile profile;
    profile.available = true;
    profile.profile_level = "detailed";
    const auto runStart = PipelineClock::now();
    auto phaseStart = runStart;

    NotifyProgress(options, runStart, "config_load", 0, 1, 0);
    const SliceConfig config = load_slice_config(configPath);
    EnsureCandidateConfig(config);
    profile.config_load_ms = ElapsedMsSince(phaseStart);
    NotifyProgress(options, runStart, "openvdb_prepare", 0, 1, 3);
    phaseStart = PipelineClock::now();

    SurfaceShellRealModelResult prototype =
        RunCandidatePrototype(config, configPath, MeshValidationPolicy::StrictClosed);
    bool nonProductionPackage{false};
    std::string strictFailureMessage;
    if (!prototype.errors.empty())
    {
        strictFailureMessage = prototype.errors.front();
        if (options.write_reports)
        {
            WriteCandidateFailureReports(config, prototype, "surface_shell_prototype");
        }
        if (!ShouldAttemptNonProductionFallback(config))
        {
            throw std::runtime_error("OpenVDB candidate prototype failed: " + prototype.errors.front());
        }

        prototype = RunCandidatePrototype(config, configPath, MeshValidationPolicy::WarnAndAttempt);
        nonProductionPackage = true;
        prototype.warnings.push_back("strict_closed blocked production output: " + strictFailureMessage);
        prototype.warnings.push_back("non-production OpenVDB candidate fallback was enabled by config");
        if (!prototype.errors.empty())
        {
            if (options.write_reports)
            {
                WriteCandidateFailureReports(config, prototype, "surface_shell_non_production_fallback");
            }
            throw std::runtime_error("OpenVDB non-production fallback failed: " + prototype.errors.front());
        }
    }
    profile.mask_sampling_ms = ElapsedMsSince(phaseStart);
    NotifyProgress(options, runStart, "layer_buffer_prepare", 0, 1, 30);
    phaseStart = PipelineClock::now();

    OpenVdbCandidateLayerBufferOptions bufferOptions;
    bufferOptions.interior_rgb = config.texture.fallback_rgb;
    const OpenVdbCandidateLayerBufferBuildResult buffers =
        BuildOpenVdbCandidateLayerBuffers(prototype.shell, prototype.transfer, bufferOptions);
    if (!buffers.error.empty())
    {
        throw std::runtime_error("OpenVDB candidate layer buffers failed: " + buffers.error);
    }
    profile.grid_setup_ms = ElapsedMsSince(phaseStart);
    NotifyProgress(options, runStart, "layer_processing", 0, buffers.depth, 36);
    phaseStart = PipelineClock::now();

    const std::filesystem::path packageDir = config.output.package_dir;
    const std::filesystem::path stagingDir = MakeSiblingDirectory(packageDir, "staging");
    if (options.write_tiff_layers || options.write_preview_files || options.write_reports)
    {
        std::filesystem::create_directories(stagingDir);
    }
    if (options.write_tiff_layers)
    {
        std::filesystem::create_directories(stagingDir / "layers");
    }
    if (options.write_reports)
    {
        std::filesystem::create_directories(stagingDir / "reports");
    }
    if (options.write_preview_files)
    {
        std::filesystem::create_directories(stagingDir / "preview");
    }

    const RgbwsvProtocol protocol = CurrentRgbwsvProtocol();
    const TiffImageSpec tiffSpec = MakeTiffSpec(config, buffers.width, buffers.height);

    Json::Array layers;
    Json::Array layerStats;
    Json::Array composerStats;
    Json::Array previewFiles;
    OpenVdbCandidatePipelineResult summary;
    summary.package_dir = packageDir;
    summary.width_px = buffers.width;
    summary.height_px = buffers.height;
    summary.layer_count = buffers.depth;

    for (const OpenVdbCandidateLayerBuffer& layer : buffers.layers)
    {
        const auto layerComputeStart = PipelineClock::now();
        const MaterialChannelComposerResult composed = ComposeMaterialChannels(layer.composer_input);
        if (!composed.error.empty())
        {
            throw std::runtime_error("OpenVDB candidate material composition failed: " + composed.error);
        }
        profile.layer_compute_ms += ElapsedMsSince(layerComputeStart);

        const std::string relativeLayerPath = LayerFileName(layer.layer_index);
        if (options.write_tiff_layers)
        {
            const auto tiffWriteStart = PipelineClock::now();
            write_rgbwsv_tiff(stagingDir / relativeLayerPath, tiffSpec, composed.channels);
            profile.tiff_write_ms += ElapsedMsSince(tiffWriteStart);
        }
        const auto layerMetadataStart = PipelineClock::now();
        layers.push_back(Json::object({
            {"index", layer.layer_index},
            {"path", relativeLayerPath},
            {"widthPx", buffers.width},
            {"heightPx", buffers.height},
            {"modelPixels", composed.stats.model_pixels},
            {"supportPixels", composed.stats.support_pixels},
        }));
        layerStats.push_back(LayerStatsToJson(layer.stats));
        composerStats.push_back(ComposerStatsToJson(composed.stats));
        summary.model_pixels += composed.stats.model_pixels;
        summary.support_pixels += composed.stats.support_pixels;
        summary.shell_pixels += layer.stats.shell_pixels;
        profile.layer_compute_ms += ElapsedMsSince(layerMetadataStart);

        if (options.write_preview_files)
        {
            const auto previewWriteStart = PipelineClock::now();
            WritePreviewFrame(
                stagingDir,
                previewFiles,
                buffers.width,
                buffers.height,
                layer.layer_index,
                "texture_rgb",
                "texture_rgb",
                "texture_rgb",
                BuildSurfaceShellRealModelPreviewPixels(prototype, layer.layer_index, "composite"),
                layer.stats.shell_pixels);
            WritePreviewFrame(
                stagingDir,
                previewFiles,
                buffers.width,
                buffers.height,
                layer.layer_index,
                "model_rgb",
                "rgb",
                "model_rgb",
                BuildRgbPreviewPixels(composed),
                composed.stats.model_pixels);
            WritePreviewFrame(
                stagingDir,
                previewFiles,
                buffers.width,
                buffers.height,
                layer.layer_index,
                "support",
                "support",
                "support_s",
                BuildMaskPreviewPixels(
                    composed,
                    MaterialChannelOffset::S,
                    protocol.empty_value,
                    config.preview.empty_color,
                    config.preview.support_color),
                composed.stats.support_pixels);
            WritePreviewFrame(
                stagingDir,
                previewFiles,
                buffers.width,
                buffers.height,
                layer.layer_index,
                "white",
                "white",
                "white_w",
                BuildMaskPreviewPixels(
                    composed,
                    MaterialChannelOffset::W,
                    protocol.empty_value,
                    config.preview.empty_color,
                    config.preview.white_color),
                composed.stats.white_pixels);
            WritePreviewFrame(
                stagingDir,
                previewFiles,
                buffers.width,
                buffers.height,
                layer.layer_index,
                "varnish",
                "varnish",
                "varnish_v",
                BuildMaskPreviewPixels(
                    composed,
                    MaterialChannelOffset::V,
                    protocol.empty_value,
                    config.preview.empty_color,
                    config.preview.varnish_color),
                composed.stats.varnish_pixels);
            profile.preview_write_ms += ElapsedMsSince(previewWriteStart);
        }

        const int completedLayers = layer.layer_index + 1;
        if (ShouldNotifyLayerProgress(completedLayers, buffers.depth))
        {
            const int percent = 36 + (completedLayers * 56 / std::max(1, buffers.depth));
            NotifyProgress(
                options,
                runStart,
                "layer_processing",
                completedLayers,
                buffers.depth,
                percent);
        }
    }
    profile.layer_compose_ms = ElapsedMsSince(phaseStart);
    NotifyProgress(options, runStart, "report_build", 0, 1, 92);
    phaseStart = PipelineClock::now();

    Json::Object tiffJson;
    tiffJson["channelOrder"] = StringArrayToJson(protocol.channel_order);
    tiffJson["channelCount"] = rgbwsv_channel_count;
    tiffJson["bitDepth"] = protocol.bit_depth;
    tiffJson["sampleFormat"] = "uint";
    tiffJson["planarConfig"] = "contiguous";
    tiffJson["tiled"] = config.output.storage_mode == "tiled";
    tiffJson["storage"] = config.output.storage_mode;
    tiffJson["storageMode"] = config.output.storage_mode;
    tiffJson["polarity"] = protocol.polarity;
    tiffJson["printValue"] = static_cast<int>(protocol.print_value);
    tiffJson["emptyValue"] = static_cast<int>(protocol.empty_value);
    tiffJson["layers"] = Json{layers};
    if (config.output.storage_mode == "tiled")
    {
        tiffJson["tileSize"] = Json::array({config.output.tile_size.at(0), config.output.tile_size.at(1)});
    }
    else
    {
        tiffJson["rowsPerStrip"] = config.output.rows_per_strip;
    }

    const Json manifest = Json::object({
        {"schema", protocol.schema},
        {"schemaVersion", protocol.schema},
        {"source",
         Json::object({
             {"configPath", configPath.generic_string()},
             {"modelPath", prototype.model_path},
             {"engine", nonProductionPackage ? "openvdb_candidate_non_production" : "openvdb_candidate"},
             {"nonProduction", nonProductionPackage},
             {"strictClosedFailure", strictFailureMessage},
         })},
        {"grid",
         Json::object({
             {"widthPx", buffers.width},
             {"heightPx", buffers.height},
             {"layerCount", buffers.depth},
             {"dpiX", config.output.dpi_x},
             {"dpiY", config.output.dpi_y},
             {"dpi", Json::array({config.output.dpi_x, config.output.dpi_y})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
             {"originMm", Json::array({0.0, 0.0, 0.0})},
         })},
        {"tiff", Json{tiffJson}},
        {"layers", Json{layers}},
        {"reports",
         Json::object({
             {"openvdbCandidate", "reports/openvdb_candidate_report.json"},
             {"productionAdmission", "reports/production_admission_report.json"},
             {"textureFidelity", "reports/texture_fidelity_report.json"},
             {"preview", "reports/preview_report.json"},
         })},
        {"preview", Json::object({{"format", "ppm"}, {"files", Json{previewFiles}}})},
    });

    const Json realModelReport = MakeSurfaceShellRealModelReport(prototype);
    profile.report_build_ms = ElapsedMsSince(phaseStart);
    NotifyProgress(options, runStart, "report_write", 0, 1, 95);
    phaseStart = PipelineClock::now();
    if (options.write_reports)
    {
        WriteReportJsonFile(stagingDir / "manifest.json", manifest);
        WriteReportJsonFile(
            stagingDir / "reports" / "openvdb_candidate_report.json",
            Json::object({
            {"schema", "p0.openvdb_candidate_report.1"},
            {"status", nonProductionPackage ? "non_production_written" : "production_written"},
            {"packageWritten", true},
            {"productionPackageWritten", !nonProductionPackage},
            {"nonProductionPackageWritten", nonProductionPackage},
            {"strictClosedFailure", strictFailureMessage},
            {"productionAdmission", realModelReport.at("productionAdmission")},
            {"grid", Json::object({{"width", buffers.width}, {"height", buffers.height}, {"depth", buffers.depth}})},
            {"totals",
             Json::object({
                 {"modelPixels", summary.model_pixels},
                 {"supportPixels", summary.support_pixels},
                 {"shellPixels", summary.shell_pixels},
             })},
            {"layerStats", Json{layerStats}},
            {"composerStats", Json{composerStats}},
        }));
        WriteReportJsonFile(
            stagingDir / "reports" / "production_admission_report.json",
            realModelReport.at("productionAdmission"));
        WriteReportJsonFile(stagingDir / "reports" / "surface_shell_texture_report.json", realModelReport);
        WriteReportJsonFile(stagingDir / "reports" / "texture_fidelity_report.json", MakeTextureFidelityReport(prototype));
        WriteReportJsonFile(
            stagingDir / "reports" / "preview_report.json",
            Json::object({
            {"schema", "p0.preview_report.1"},
            {"enabled", true},
            {"format", "ppm"},
            {"channels", Json::array({"texture_rgb", "rgb", "support", "white", "varnish"})},
            {"pseudoColors",
             Json::object({
                 {"empty", ColorToJson(config.preview.empty_color)},
                 {"support", ColorToJson(config.preview.support_color)},
                 {"white", ColorToJson(config.preview.white_color)},
                 {"varnish", ColorToJson(config.preview.varnish_color)},
             })},
            {"files", Json{previewFiles}},
            {"generated", Json{previewFiles}},
        }));
    }
    profile.report_write_ms = ElapsedMsSince(phaseStart);

    if (options.publish_package)
    {
        NotifyProgress(options, runStart, "package_publish", 0, 1, 98);
        phaseStart = PipelineClock::now();
        PublishStagedPackage(stagingDir, packageDir);
        profile.package_publish_ms = ElapsedMsSince(phaseStart);
    }
    summary.non_production = nonProductionPackage;
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
    profile.total_ms = ElapsedMsSince(runStart);
    summary.profile = profile;
    NotifyProgress(options, runStart, "completed", 1, 1, 100);

    return summary;
}

}  // namespace slicer_core
