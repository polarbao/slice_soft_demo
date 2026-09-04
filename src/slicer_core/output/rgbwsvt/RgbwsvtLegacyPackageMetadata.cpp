#include "slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

[[noreturn]] void ThrowProtocolError(const std::string& detail)
{
    throw TransferChannelError(
        TransferChannelErrorCode::ProtocolInvalid,
        "Legacy RGBWSVT output failed: " + detail);
}

TiffImageSpec MakeTiffSpec(
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvtProductionLayer& layer)
{
    if (layer.widthPx <= 0 || layer.heightPx <= 0)
    {
        ThrowProtocolError("layer dimensions must be positive");
    }
    TiffImageSpec spec;
    spec.width = static_cast<std::uint32_t>(layer.widthPx);
    spec.height = static_cast<std::uint32_t>(layer.heightPx);
    spec.samples_per_pixel = static_cast<std::uint16_t>(kRgbwsvtChannelCount);
    if (storage.storageMode == "stripped")
    {
        if (storage.rowsPerStrip <= 0)
        {
            ThrowProtocolError("rowsPerStrip must be positive");
        }
        spec.storage_mode = TiffStorageMode::Stripped;
        spec.rows_per_strip = static_cast<std::uint32_t>(storage.rowsPerStrip);
    }
    else if (storage.storageMode == "tiled")
    {
        if (storage.tileWidth <= 0 || storage.tileHeight <= 0)
        {
            ThrowProtocolError("tile dimensions must be positive");
        }
        spec.storage_mode = TiffStorageMode::Tiled;
        spec.tile_width = static_cast<std::uint32_t>(storage.tileWidth);
        spec.tile_height = static_cast<std::uint32_t>(storage.tileHeight);
    }
    else
    {
        ThrowProtocolError("storage mode must be stripped or tiled");
    }
    if (storage.compression == "none")
    {
        spec.compression_mode = TiffCompressionMode::None;
    }
    else if (storage.compression == "packbits")
    {
        spec.compression_mode = TiffCompressionMode::PackBits;
    }
    else
    {
        ThrowProtocolError("compression must be none or packbits");
    }
    return spec;
}

double CoverageRatio(const std::uint64_t count, const std::uint64_t total)
{
    return total == 0U ? 0.0 : static_cast<double>(count) / static_cast<double>(total);
}

RgbwsvtMaterialStatistics CountMaterials(const std::vector<std::uint8_t>& pixels)
{
    RgbwsvtMaterialStatistics result;
    for (std::size_t offset = 0U; offset < pixels.size(); offset += kRgbwsvtChannelCount)
    {
        const bool rgb = pixels[offset] != 255U || pixels[offset + 1U] != 255U
            || pixels[offset + 2U] != 255U;
        const bool white = pixels[offset + 3U] != 255U;
        const bool support = pixels[offset + 4U] != 255U;
        const bool varnish = pixels[offset + 5U] != 255U;
        const bool transfer = pixels[offset + 6U] != 255U;
        result.rgbPrintPixels += rgb;
        result.whitePrintPixels += white;
        result.supportPrintPixels += support;
        result.varnishPrintPixels += varnish;
        result.transferPrintPixels += transfer;
        result.unexpectedOverlapPixels +=
            static_cast<int>(rgb) + static_cast<int>(white) + static_cast<int>(support)
                + static_cast<int>(varnish) + static_cast<int>(transfer) > 1;
    }
    return result;
}

Json ChannelOrderJson(const bool transferEnabled)
{
    return transferEnabled
        ? Json::array({"R", "G", "B", "W", "S", "V", "T"})
        : Json::array({"R", "G", "B", "W", "S", "V"});
}

}  // namespace

RgbwsvtLegacyLayerWriteResult WriteRgbwsvtLegacyProductionLayerTiff(
    const std::filesystem::path& path,
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvtProductionLayer& layer)
{
    const TiffImageSpec spec = MakeTiffSpec(storage, layer);
    const std::size_t expectedBytes = static_cast<std::size_t>(spec.width)
        * static_cast<std::size_t>(spec.height) * kRgbwsvtChannelCount;
    if (layer.channelOrder != CurrentRgbwsvtProtocol().channelOrder
        || layer.channels.size() != expectedBytes)
    {
        ThrowProtocolError("layer channel order or byte count is invalid");
    }
    write_rgbwsvt_tiff(path, spec, layer.channels);
    const RgbwsvtTiffReadResult persisted = ReadRgbwsvtTiff(path);
    if (persisted.spec.width != spec.width || persisted.spec.height != spec.height
        || persisted.spec.storage_mode != spec.storage_mode
        || persisted.spec.compression_mode != spec.compression_mode
        || persisted.pixels != layer.channels)
    {
        ThrowProtocolError("persisted TIFF does not match the final layer bytes");
    }
    return RgbwsvtLegacyLayerWriteResult{
        persisted.channelStats, CountMaterials(persisted.pixels)};
}

void MergeRgbwsvtChannelStatistics(
    RgbwsvtChannelStatistics& totals,
    const RgbwsvtChannelStatistics& layer)
{
    for (std::size_t index = 0U; index < totals.size(); ++index)
    {
        TiffChannelStats& total = totals[index];
        const TiffChannelStats& current = layer[index];
        total.print_pixels += current.print_pixels;
        total.full_print_pixels += current.full_print_pixels;
        total.partial_print_pixels += current.partial_print_pixels;
        total.empty_pixels += current.empty_pixels;
        total.min_value = std::min(total.min_value, current.min_value);
        total.max_value = std::max(total.max_value, current.max_value);
    }
}

void MergeRgbwsvtMaterialStatistics(
    RgbwsvtMaterialStatistics& totals,
    const RgbwsvtMaterialStatistics& layer)
{
    totals.rgbPrintPixels += layer.rgbPrintPixels;
    totals.whitePrintPixels += layer.whitePrintPixels;
    totals.supportPrintPixels += layer.supportPrintPixels;
    totals.varnishPrintPixels += layer.varnishPrintPixels;
    totals.transferPrintPixels += layer.transferPrintPixels;
    totals.unexpectedOverlapPixels += layer.unexpectedOverlapPixels;
}

Json ChannelStatisticsToJson(
    const std::span<const TiffChannelStats> statistics,
    const std::span<const std::string> channelOrder)
{
    if (statistics.size() != channelOrder.size())
    {
        ThrowProtocolError("channel statistics do not match channel order");
    }
    Json::Object result;
    for (std::size_t index = 0U; index < statistics.size(); ++index)
    {
        const TiffChannelStats& channel = statistics[index];
        result.emplace(channelOrder[index], Json::object({
            {"printPixels", channel.print_pixels},
            {"fullPrintPixels", channel.full_print_pixels},
            {"partialPrintPixels", channel.partial_print_pixels},
            {"emptyPixels", channel.empty_pixels},
            {"minValue", channel.min_value},
            {"maxValue", channel.max_value}}));
    }
    return Json{std::move(result)};
}

Json BuildRgbwsvtMaterialProcessReport(
    const Json& legacyReport,
    const MaterialProcessProfileConfig& profile,
    const std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtMaterialStatistics& totals,
    const std::uint64_t totalPixels)
{
    Json::Object result = legacyReport.as_object();
    result["packageProtocol"] = "p0.rgbwsvt.1";
    result["statisticsSource"] = "persisted_tiff_bytes";
    const auto replaceCount = [totalPixels](const Json& source, const std::uint64_t count)
    {
        Json::Object object = source.as_object();
        object["printPixels"] = count;
        object["coverageRatio"] = CoverageRatio(count, totalPixels);
        return Json{std::move(object)};
    };
    result["rgb"] = replaceCount(legacyReport.at("rgb"), totals.rgbPrintPixels);
    result["white"] = replaceCount(legacyReport.at("white"), totals.whitePrintPixels);
    result["support"] = replaceCount(legacyReport.at("support"), totals.supportPrintPixels);
    result["varnish"] = replaceCount(legacyReport.at("varnish"), totals.varnishPrintPixels);
    result["transfer"] = Json::object({
        {"enabled", true}, {"value", 0},
        {"printPixels", totals.transferPrintPixels},
        {"coverageRatio", CoverageRatio(totals.transferPrintPixels, totalPixels)}});
    result["unexpectedOverlapPixels"] = totals.unexpectedOverlapPixels;

    Json::Array outputLayers;
    if (legacyReport.at("layers").size() != layers.size())
    {
        ThrowProtocolError("material process layer statistics are incomplete");
    }
    for (std::size_t index = 0U; index < layers.size(); ++index)
    {
        Json::Object entry = legacyReport.at("layers").at(index).as_object();
        const RgbwsvtMaterialStatistics& material = layers[index].materials;
        entry["rgbPrintPixels"] = material.rgbPrintPixels;
        entry["whitePrintPixels"] = material.whitePrintPixels;
        entry["supportPrintPixels"] = material.supportPrintPixels;
        entry["varnishPrintPixels"] = material.varnishPrintPixels;
        entry["transferPrintPixels"] = material.transferPrintPixels;
        entry["unexpectedOverlapPixels"] = material.unexpectedOverlapPixels;
        outputLayers.emplace_back(Json{std::move(entry)});
    }
    result["layers"] = Json{std::move(outputLayers)};

    const std::uint64_t missingUnderbase = totals.whitePrintPixels < totals.rgbPrintPixels
        ? totals.rgbPrintPixels - totals.whitePrintPixels : 0U;
    Json::Array failures;
    if (profile.enabled)
    {
        if (profile.validation.require_rgb_pixels && totals.rgbPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB");
        if (profile.validation.require_white_pixels && totals.whitePrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE");
        if (profile.validation.require_varnish_pixels && totals.varnishPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_VARNISH");
        if (profile.validation.require_support_pixels && totals.supportPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_SUPPORT");
        if (totals.unexpectedOverlapPixels
            > static_cast<std::uint64_t>(profile.validation.max_unexpected_overlap_pixels))
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP");
        if (profile.white.enabled && profile.white.mode == "underbase" && missingUnderbase > 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW");
    }
    Json::Object white = result.at("white").as_object();
    white["missingUnderbasePixels"] = missingUnderbase;
    result["white"] = Json{std::move(white)};
    result["validation"] = Json::object({
        {"pass", failures.empty()}, {"failures", Json{std::move(failures)}}});
    return Json{std::move(result)};
}

Json BuildRgbwsvtSliceReport(
    const Json& legacyReport,
    const std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtChannelStatistics& channelTotals,
    const RgbwsvtMaterialStatistics& materialTotals)
{
    Json::Object result = legacyReport.as_object();
    result["packageProtocol"] = "p0.rgbwsvt.1";
    result["statisticsSource"] = "persisted_tiff_bytes";

    Json::Object totals = legacyReport.at("totals").as_object();
    totals["rgbNonZeroPixels"] = materialTotals.rgbPrintPixels;
    totals["whiteNonZeroPixels"] = materialTotals.whitePrintPixels;
    totals["supportNonZeroPixels"] = materialTotals.supportPrintPixels;
    totals["varnishNonZeroPixels"] = materialTotals.varnishPrintPixels;
    totals["transferNonZeroPixels"] = materialTotals.transferPrintPixels;
    totals["rgbPrintPixels"] = materialTotals.rgbPrintPixels;
    totals["whitePrintPixels"] = materialTotals.whitePrintPixels;
    totals["supportPrintPixels"] = materialTotals.supportPrintPixels;
    totals["varnishPrintPixels"] = materialTotals.varnishPrintPixels;
    totals["transferPrintPixels"] = materialTotals.transferPrintPixels;
    totals["unexpectedOverlapPixels"] = materialTotals.unexpectedOverlapPixels;
    const RgbwsvtProtocol protocol = CurrentRgbwsvtProtocol();
    totals["channelStats"] = ChannelStatisticsToJson(channelTotals, protocol.channelOrder);
    result["totals"] = Json{std::move(totals)};

    if (legacyReport.at("layers").size() != layers.size())
    {
        ThrowProtocolError("slice report layer statistics are incomplete");
    }
    Json::Array outputLayers;
    outputLayers.reserve(layers.size());
    for (std::size_t index = 0U; index < layers.size(); ++index)
    {
        Json::Object entry = legacyReport.at("layers").at(index).as_object();
        const RgbwsvtLegacyLayerStatistics& layer = layers[index];
        entry["rgbNonZeroPixels"] = layer.materials.rgbPrintPixels;
        entry["whiteNonZeroPixels"] = layer.materials.whitePrintPixels;
        entry["supportNonZeroPixels"] = layer.materials.supportPrintPixels;
        entry["varnishNonZeroPixels"] = layer.materials.varnishPrintPixels;
        entry["transferNonZeroPixels"] = layer.materials.transferPrintPixels;
        entry["rgbPrintPixels"] = layer.materials.rgbPrintPixels;
        entry["whitePrintPixels"] = layer.materials.whitePrintPixels;
        entry["supportPrintPixels"] = layer.materials.supportPrintPixels;
        entry["varnishPrintPixels"] = layer.materials.varnishPrintPixels;
        entry["transferPrintPixels"] = layer.materials.transferPrintPixels;
        entry["unexpectedOverlapPixels"] = layer.materials.unexpectedOverlapPixels;
        entry["channelStats"] = ChannelStatisticsToJson(layer.channels, protocol.channelOrder);
        outputLayers.emplace_back(Json{std::move(entry)});
    }
    result["layers"] = Json{std::move(outputLayers)};
    return Json{std::move(result)};
}

Json BuildLegacyTransferChannelReport(
    const TransferChannelPolicyConfig& policy,
    const TransferMaterialVolumePlan& plan,
    const std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtChannelStatistics& channelTotals,
    const RgbwsvtMaterialStatistics& materialTotals)
{
    Json::Array configuredColours;
    for (const auto& colour : policy.material_diffuse_rgb_values)
    {
        configuredColours.emplace_back(Json::array({
            static_cast<int>(colour[0]), static_cast<int>(colour[1]),
            static_cast<int>(colour[2])}));
    }
    Json::Array layerEntries;
    for (const RgbwsvtLegacyLayerStatistics& layer : layers)
    {
        layerEntries.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"transferPrintPixels", layer.materials.transferPrintPixels}}));
    }
    return Json::object({
        {"schema", "slicesoft.transfer_channel_report.1"},
        {"packageProtocol", "p0.rgbwsvt.1"},
        {"statisticsSource", "persisted_tiff_bytes"},
        {"enabled", policy.enabled},
        {"matchSource", policy.match_source},
        {"configuredMaterialDiffuseRgbValues", Json{std::move(configuredColours)}},
        {"regionPresent", plan.HasRegion()},
        {"materialName", plan.material.materialName},
        {"matchedDiffuseRgb", Json::array({
            static_cast<int>(plan.material.diffuseRgb[0]),
            static_cast<int>(plan.material.diffuseRgb[1]),
            static_cast<int>(plan.material.diffuseRgb[2])})},
        {"value", static_cast<int>(policy.value)},
        {"channelStats", ChannelStatisticsToJson(
            channelTotals, CurrentRgbwsvtProtocol().channelOrder)},
        {"totals", Json::object({
            {"layerCount", static_cast<int>(layers.size())},
            {"transferPrintPixels", materialTotals.transferPrintPixels},
            {"unexpectedOverlapPixels", materialTotals.unexpectedOverlapPixels}})},
        {"layers", Json{std::move(layerEntries)}},
        {"warnings", Json::array({})}, {"errors", Json::array({})}});
}

RgbwsvtCandidatePackageGuard::RgbwsvtCandidatePackageGuard(
    std::filesystem::path packageDirectory)
    : m_packageDirectory(std::move(packageDirectory))
{
    if (m_packageDirectory.empty() || std::filesystem::exists(m_packageDirectory))
    {
        ThrowProtocolError("candidate package directory must not already exist");
    }
    std::filesystem::create_directories(m_packageDirectory.parent_path());
    if (!std::filesystem::create_directory(m_packageDirectory))
    {
        ThrowProtocolError("candidate package directory could not be reserved");
    }
}

RgbwsvtCandidatePackageGuard::~RgbwsvtCandidatePackageGuard()
{
    if (m_cleanupRequired)
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_packageDirectory, ignored);
    }
}

void RgbwsvtCandidatePackageGuard::Commit() noexcept
{
    m_cleanupRequired = false;
}

Json BuildLegacyTiffManifestMetadata(
    const OutputConfig& output,
    const Json::Array& layers,
    const bool writeTiffLayers,
    const RgbwsvtChannelStatistics* rgbwsvtStatistics)
{
    const bool transferEnabled = output.package_protocol == "p0.rgbwsvt.1";
    if (transferEnabled && writeTiffLayers && rgbwsvtStatistics == nullptr)
    {
        ThrowProtocolError("persisted channel statistics are required");
    }
    Json::Object result{
        {"channelOrder", ChannelOrderJson(transferEnabled)},
        {"channelCount", transferEnabled ? 7 : 6},
        {"bitDepth", 8},
        {"sampleFormat", "uint"},
        {"planarConfig", "contiguous"},
        {"tiled", output.storage_mode == "tiled"},
        {"storage", output.storage_mode},
        {"storageMode", output.storage_mode},
        {"compression", output.tiff_compression},
        {"polarity", "black_is_print"},
        {"printValue", 0},
        {"emptyValue", 255},
        {"writeTiffLayers", writeTiffLayers},
        {"layers", Json{layers}},
    };
    if (output.storage_mode == "tiled")
    {
        result["tileSize"] = Json::array(
            {output.tile_size[0U], output.tile_size[1U]});
    }
    else
    {
        result["rowsPerStrip"] = output.rows_per_strip;
    }
    if (transferEnabled && rgbwsvtStatistics != nullptr)
    {
        result["channelStats"] = ChannelStatisticsToJson(
            *rgbwsvtStatistics, CurrentRgbwsvtProtocol().channelOrder);
        result["statisticsSource"] = "persisted_tiff_bytes";
    }
    return Json{std::move(result)};
}

}  // namespace slicer_core
