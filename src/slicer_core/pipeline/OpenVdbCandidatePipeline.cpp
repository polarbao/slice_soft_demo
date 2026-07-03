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

std::string LayerFileName(const int layerIndex)
{
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layerIndex << ".tiff";
    return stream.str();
}

std::string PreviewFileName(const int layerIndex)
{
    std::ostringstream stream;
    stream << "preview/texture_rgb_" << std::setw(6) << std::setfill('0') << layerIndex << ".ppm";
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
    const std::filesystem::path& configPath)
{
    const std::filesystem::path configDir =
        configPath.parent_path().empty() ? std::filesystem::current_path() : configPath.parent_path();
    const SceneModel scene = load_model_report(config, configDir);

    SurfaceShellRealModelOptions options;
    options.voxel_size_mm = 0.05;
    options.shell_thickness_mm = 0.10;
    options.max_transfer_distance_mm = options.shell_thickness_mm + options.voxel_size_mm * 2.0;
    options.mesh_policy = MeshValidationPolicy::StrictClosed;
    options.fallback_rgb = config.texture.fallback_rgb;
    options.texture_sample.sampler = config.texture.sampler;
    options.texture_sample.uv_address_mode = config.texture.uv_address_mode;
    options.texture_sample.flip_v = config.texture.flip_v;

    SurfaceShellRealModelResult result = RunSurfaceShellRealModelPrototype(scene, config, options);
    result.config_path = configPath.generic_string();
    return result;
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

}  // namespace

OpenVdbCandidatePipelineResult RunOpenVdbCandidatePipeline(const std::filesystem::path& configPath)
{
    const SliceConfig config = load_slice_config(configPath);
    EnsureCandidateConfig(config);

    SurfaceShellRealModelResult prototype = RunCandidatePrototype(config, configPath);
    if (!prototype.errors.empty())
    {
        throw std::runtime_error("OpenVDB candidate prototype failed: " + prototype.errors.front());
    }

    OpenVdbCandidateLayerBufferOptions bufferOptions;
    bufferOptions.interior_rgb = config.texture.fallback_rgb;
    const OpenVdbCandidateLayerBufferBuildResult buffers =
        BuildOpenVdbCandidateLayerBuffers(prototype.shell, prototype.transfer, bufferOptions);
    if (!buffers.error.empty())
    {
        throw std::runtime_error("OpenVDB candidate layer buffers failed: " + buffers.error);
    }

    const std::filesystem::path packageDir = config.output.package_dir;
    const std::filesystem::path stagingDir = MakeSiblingDirectory(packageDir, "staging");
    std::filesystem::create_directories(stagingDir / "layers");
    std::filesystem::create_directories(stagingDir / "reports");
    std::filesystem::create_directories(stagingDir / "preview");

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
        const MaterialChannelComposerResult composed = ComposeMaterialChannels(layer.composer_input);
        if (!composed.error.empty())
        {
            throw std::runtime_error("OpenVDB candidate material composition failed: " + composed.error);
        }

        const std::string relativeLayerPath = LayerFileName(layer.layer_index);
        write_rgbwsv_tiff(stagingDir / relativeLayerPath, tiffSpec, composed.channels);
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

        const std::string relativePreviewPath = PreviewFileName(layer.layer_index);
        WritePpm(
            stagingDir / relativePreviewPath,
            buffers.width,
            buffers.height,
            BuildSurfaceShellRealModelPreviewPixels(prototype, layer.layer_index, "composite"));
        previewFiles.push_back(Json::object({
            {"layerIndex", layer.layer_index},
            {"channel", "texture_rgb"},
            {"type", "texture_rgb"},
            {"kind", "single"},
            {"format", "ppm"},
            {"path", relativePreviewPath},
            {"printPixels", layer.stats.model_pixels},
        }));
    }

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
             {"engine", "openvdb_candidate"},
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
    WriteReportJsonFile(stagingDir / "manifest.json", manifest);
    WriteReportJsonFile(
        stagingDir / "reports" / "openvdb_candidate_report.json",
        Json::object({
            {"schema", "p0.openvdb_candidate_report.1"},
            {"productionPackageWritten", true},
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
            {"channels", Json::array({"texture_rgb"})},
            {"files", Json{previewFiles}},
            {"generated", Json{previewFiles}},
        }));

    PublishStagedPackage(stagingDir, packageDir);

    return summary;
}

}  // namespace slicer_core
