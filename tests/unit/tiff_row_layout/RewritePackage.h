#pragma once

#include "slicer_core/rip_reader.h"
#include "slicer_core/system/Sha256.h"
#include <fstream>

// Test-only evidence tool. No re-slicing, in-place updates, or stale RIP copies.
inline void RewriteLegacyPackageForAudit(
    const std::filesystem::path& source, const std::filesystem::path& destination)
{
    using namespace slicer_core;
    if (std::filesystem::exists(destination))
        throw std::runtime_error("audit destination must not exist");
    (void)validate_slice_package(source);
    std::ifstream input(source / "manifest.json", std::ios::binary);
    Json manifest = Json::parse(input);
    if (manifest.at("schema").as_string() != "p0.rgbwsv.2")
        throw std::runtime_error("audit rewrite requires a six-channel legacy package");
    const auto tiff = manifest.at("tiff");
    if (ReadManifestTiffRowOrder(tiff) != TiffRowOrder::MinYFirst)
        throw std::runtime_error("audit source is not legacy row order");
    std::filesystem::create_directories(destination / "layers");
    for (const auto& layer : manifest.at("layers").as_array())
    {
        const auto relative = std::filesystem::path(layer.at("path").as_string());
        if (relative.is_absolute() || relative.parent_path() != "layers")
            throw std::runtime_error("audit only accepts immediate layers paths");
        const auto decoded = read_rgbwsv_tiff(source / relative);
        RgbwsvProductionStorageSpec storage;
        storage.storageMode = tiff_storage_mode_string(decoded.spec.storage_mode);
        storage.compression = TiffCompressionModeString(decoded.spec.compression_mode);
        storage.rowsPerStrip = static_cast<int>(decoded.spec.rows_per_strip);
        storage.tileWidth = static_cast<int>(decoded.spec.tile_width);
        storage.tileHeight = static_cast<int>(decoded.spec.tile_height);
        WriteRgbwsvProductionLayerTiff(destination / relative, storage,
            {static_cast<int>(decoded.spec.width), static_cast<int>(decoded.spec.height), decoded.pixels});
        const auto reread = read_rgbwsv_tiff(destination / relative);
        if (reread.pixels != decoded.pixels || reread.channel_checksums != decoded.channel_checksums)
            throw std::runtime_error("real package canonical pixels changed");
    }
    // Only reports are carried forward; original diagnostic RIP output is not valid for new files.
    std::filesystem::copy(source / "reports", destination / "reports", std::filesystem::copy_options::recursive);
    auto tiffObject = tiff.as_object();
    tiffObject["rowOrder"] = "max_y_first";
    auto root = manifest.as_object();
    root["tiff"] = Json{std::move(tiffObject)};
    root["orientationAudit"] = Json::object({{"sourcePackage", source.generic_string()},
        {"reSliced", false}, {"physicalPrintValidated", false}});
    { std::ofstream output(destination / "manifest.json", std::ios::binary);
      output << Json{std::move(root)}.dump(2); }
    (void)validate_slice_package(destination);
    std::cout << "PASS: real package all layers canonical equality and strict validation\n";
}
