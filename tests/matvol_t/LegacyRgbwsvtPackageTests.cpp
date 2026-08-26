#include "slicer_core/TiffReadApi.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/slicer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path SourceRoot()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path TestRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("slicesoft_rgbwsvt_legacy_package_" + std::to_string(stamp));
}

slicer_core::Json MakeConfig(
    const std::filesystem::path& packageDirectory,
    const std::optional<std::array<int, 3>>& transferColour)
{
    const bool transferEnabled = transferColour.has_value();
    slicer_core::Json::Object root = slicer_core::Json::object({
        {"input", slicer_core::Json::object({
            {"format", "obj"},
            {"modelPath", (SourceRoot() / "model/obj/reality/finger_suoguo/03.obj").generic_string()},
        })},
        {"output", slicer_core::Json::object({
            {"packageDir", packageDirectory.generic_string()},
            {"packageProtocol", transferEnabled ? "p0.rgbwsvt.1" : "p0.rgbwsv.2"},
            {"channelOrder", transferEnabled
                ? slicer_core::Json::array({"R", "G", "B", "W", "S", "V", "T"})
                : slicer_core::Json::array({"R", "G", "B", "W", "S", "V"})},
            {"dpiX", 100},
            {"dpiY", 100},
            {"layerThicknessMm", 0.20},
            {"storageMode", "stripped"},
            {"rowsPerStrip", 16},
        })},
    }).as_object();
    if (transferColour.has_value())
    {
        root["transferChannelPolicy"] = slicer_core::Json::object({
            {"enabled", true},
            {"matchSource", "material_diffuse_rgb"},
            {"materialDiffuseRgbValues", slicer_core::Json::array({
                slicer_core::Json::array({
                    transferColour->at(0U), transferColour->at(1U), transferColour->at(2U)})})},
            {"missingRegion", "allow_empty"},
            {"multipleMatches", "fail_closed"},
            {"value", 0},
            {"topology", slicer_core::Json::object({
                {"selfIntersectionPolicy", "tolerate_closed_self_intersection"},
                {"maxSelfIntersectionPairs", 64},
            })},
        });
        root["preview"] = slicer_core::Json::object({
            {"enabled", true},
            {"format", "ppm"},
            {"interval", 1000},
            {"channels", slicer_core::Json::array({"transfer"})},
            {"pseudoColors", slicer_core::Json::object({
                {"transfer", slicer_core::Json::array({255, 0, 255})}})},
        });
    }
    return slicer_core::Json{std::move(root)};
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& root,
    const std::string& name,
    const slicer_core::Json& config)
{
    const std::filesystem::path path = root / (name + ".json");
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << config.dump(2) << '\n';
    if (!stream.good())
    {
        throw std::runtime_error("failed to write test config");
    }
    return path;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return slicer_core::Json::parse(stream);
}

slicer_core::SliceRunOptions CandidateOptions()
{
    slicer_core::SliceRunOptions options;
    options.write_tiff_layers = true;
    options.write_reports = true;
    options.write_preview_files = false;
    return options;
}

bool Reality03ManifestMatchesPersistedBytes(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "reality_03";
    const std::filesystem::path config = WriteConfig(
        root, "reality_03", MakeConfig(package, std::array<int, 3>{255, 220, 198}));
    const slicer_core::SliceRunResult result =
        slicer_core::run_slicer(config, CandidateOptions());
    const slicer_core::Json manifest = ReadJson(package / "manifest.json");
    slicer_core::RgbwsvtChannelStatistics totals{};
    bool exclusive = true;
    for (const slicer_core::Json& entry : manifest.at("layers").as_array())
    {
        const slicer_core::RgbwsvtTiffReadResult layer =
            slicer_core::ReadRgbwsvtTiff(package / entry.at("path").as_string());
        slicer_core::MergeRgbwsvtChannelStatistics(totals, layer.channelStats);
        for (std::size_t offset = 0U; offset < layer.pixels.size(); offset += 7U)
        {
            if (layer.pixels[offset + 6U] == 0U)
            {
                exclusive = exclusive
                    && std::all_of(
                        layer.pixels.begin() + static_cast<std::ptrdiff_t>(offset),
                        layer.pixels.begin() + static_cast<std::ptrdiff_t>(offset + 6U),
                        [](const std::uint8_t value) { return value == 255U; });
            }
        }
    }
    const slicer_core::Json& stats = manifest.at("tiff").at("channelStats");
    return ExpectTrue(result.layer_count == manifest.at("layers").size(), "layer count matches manifest")
        && ExpectTrue(manifest.at("schema").as_string() == "p0.rgbwsvt.1", "manifest schema is RGBWSVT")
        && ExpectTrue(manifest.at("tiff").at("channelCount").as_int() == 7, "manifest has seven channels")
        && ExpectTrue(totals[6U].print_pixels > 0U, "03 persisted TIFF contains T pixels")
        && ExpectTrue(
            stats.at("T").at("printPixels").as_int()
                == static_cast<int>(totals[6U].print_pixels),
            "manifest T count matches persisted TIFF bytes")
        && ExpectTrue(exclusive, "persisted T pixels clear RGBWSV")
        && ExpectTrue(
            manifest.at("productionAcceptance").as_string()
                == "rgbwsvt_candidate_unvalidated",
            "T-05B package remains an unvalidated candidate");
}

bool MissingTransferPreservesLegacyProjection(const std::filesystem::path& root)
{
    const std::filesystem::path legacyPackage = root / "legacy";
    const std::filesystem::path transferPackage = root / "no_transfer";
    const std::filesystem::path legacyConfig = WriteConfig(
        root, "legacy", MakeConfig(legacyPackage, std::nullopt));
    const std::filesystem::path transferConfig = WriteConfig(
        root, "no_transfer", MakeConfig(transferPackage, std::array<int, 3>{1, 2, 3}));
    (void)slicer_core::run_slicer(legacyConfig, CandidateOptions());
    (void)slicer_core::run_slicer(transferConfig, CandidateOptions());
    const slicer_core::Json legacyManifest = ReadJson(legacyPackage / "manifest.json");
    const slicer_core::Json transferManifest = ReadJson(transferPackage / "manifest.json");
    bool passed = ExpectTrue(
        legacyManifest.at("layers").size() == transferManifest.at("layers").size(),
        "no-transfer package keeps layer count");
    for (std::size_t layerIndex = 0U;
         layerIndex < legacyManifest.at("layers").size(); ++layerIndex)
    {
        const std::string legacyPath =
            legacyManifest.at("layers").at(layerIndex).at("path").as_string();
        const std::string transferPath =
            transferManifest.at("layers").at(layerIndex).at("path").as_string();
        const slicer_core::TiffReadResult legacy =
            slicer_core::read_rgbwsv_tiff(legacyPackage / legacyPath);
        const slicer_core::RgbwsvtTiffReadResult transfer =
            slicer_core::ReadRgbwsvtTiff(transferPackage / transferPath);
        for (std::size_t pixel = 0U; pixel < legacy.pixels.size() / 6U; ++pixel)
        {
            for (std::size_t channel = 0U; channel < 6U; ++channel)
            {
                passed = ExpectTrue(
                    legacy.pixels[pixel * 6U + channel]
                        == transfer.pixels[pixel * 7U + channel],
                    "no-transfer RGBWSV projection is byte exact") && passed;
            }
            passed = ExpectTrue(
                transfer.pixels[pixel * 7U + 6U] == 255U,
                "no-transfer T channel is empty") && passed;
        }
    }
    return passed;
}

bool DefaultPreviewCandidateUsesPersistedTransferSemantics(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "default_preview";
    const std::filesystem::path config = WriteConfig(
        root, "default_preview",
        MakeConfig(package, std::array<int, 3>{255, 220, 198}));
    (void)slicer_core::run_slicer(config);
    const slicer_core::Json manifest = ReadJson(package / "manifest.json");
    const slicer_core::Json preview = ReadJson(package / "reports/preview_report.json");
    const slicer_core::Json process = ReadJson(package / "reports/material_process_report.json");
    const slicer_core::Json slice = ReadJson(package / "reports/slice_report.json");
    const slicer_core::Json transfer = ReadJson(package / "reports/transfer_channel_report.json");
    const slicer_core::Json& generated = preview.at("generated");
    bool filesExist = generated.size() > 0U;
    bool hasTransferPixels = false;
    for (const slicer_core::Json& entry : generated.as_array())
    {
        filesExist = filesExist
            && entry.at("channel").as_string() == "transfer"
            && std::filesystem::exists(package / entry.at("path").as_string());
        hasTransferPixels = hasTransferPixels || entry.at("printPixels").as_int() > 0;
    }
    return ExpectTrue(
               preview.at("productionSource").as_string() == "rgbwsvt_tiff",
               "preview selects the manifest protocol")
        && ExpectTrue(
            filesExist && hasTransferPixels,
            "T diagnostic previews are generated from final RGBWSVT bytes")
        && ExpectTrue(
            process.at("statisticsSource").as_string() == "persisted_tiff_bytes",
            "material process report declares final-byte authority")
        && ExpectTrue(
            slice.at("statisticsSource").as_string() == "persisted_tiff_bytes"
                && slice.at("totals").at("rgbPrintPixels").as_int()
                    == process.at("rgb").at("printPixels").as_int()
                && slice.at("totals").at("transferPrintPixels").as_int()
                    == process.at("transfer").at("printPixels").as_int(),
            "slice report replaces pre-transfer RGB counts with final TIFF statistics")
        && ExpectTrue(
            process.at("transfer").at("printPixels").as_int()
                == transfer.at("totals").at("transferPrintPixels").as_int(),
            "material and transfer reports agree on T pixels")
        && ExpectTrue(
            transfer.at("totals").at("unexpectedOverlapPixels").as_int() == 0,
            "persisted transfer bytes remain exclusive")
        && ExpectTrue(
            manifest.at("reports").at("transferChannel").as_string()
                == "reports/transfer_channel_report.json",
            "manifest publishes the transfer report");
}

}  // namespace

int main()
{
    const std::filesystem::path root = TestRoot();
    std::filesystem::create_directories(root);
    int failures = 0;
    try
    {
        failures += Reality03ManifestMatchesPersistedBytes(root) ? 0 : 1;
        failures += MissingTransferPreservesLegacyProjection(root) ? 0 : 1;
        failures += DefaultPreviewCandidateUsesPersistedTransferSemantics(root) ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL LegacyRgbwsvtPackageTests exception: " << error.what() << '\n';
        ++failures;
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    if (failures != 0)
    {
        std::cerr << "FAIL LegacyRgbwsvtPackageTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS LegacyRgbwsvtPackageTests 3/3\n";
    return 0;
}
