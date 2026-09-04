#pragma once

#include "slicer_core/TiffReadApi.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

struct RgbwsvtPackageLayer
{
    int index{0};
    double zMm{0.0};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::filesystem::path path;
    std::string fileIdentity;
    TiffStorageMode storage{TiffStorageMode::Stripped};
    TiffCompressionMode compression{TiffCompressionMode::None};
    std::vector<std::uint64_t> checksums;
    std::vector<TiffChannelStats> channelStats;
};

struct RgbwsvtPackageValidation
{
    std::filesystem::path packageDirectory;
    std::filesystem::path manifestPath;
    std::string schema;
    std::string productionAcceptance;
    std::string manifestHash;
    std::string packageIdentity;
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    int dpiX{0};
    int dpiY{0};
    int bitDepth{8};
    std::string polarity{"black_is_print"};
    std::string storageMode;
    std::string compression;
    std::vector<std::string> channelOrder;
    std::vector<TiffChannelStats> totalChannelStats;
    std::vector<RgbwsvtPackageLayer> layers;
};

struct RgbwsvtDecodedPackageLayer
{
    RgbwsvtPackageLayer descriptor;
    std::vector<std::uint8_t> pixels;
};

[[nodiscard]] std::string ReadPackageManifestSchema(
    const std::filesystem::path& packageDirectory);

[[nodiscard]] RgbwsvtPackageValidation ValidateRgbwsvtPackage(
    const std::filesystem::path& packageDirectory);

[[nodiscard]] RgbwsvtDecodedPackageLayer ReadRgbwsvtPackageLayer(
    const RgbwsvtPackageLayer& layer);

[[nodiscard]] bool IsRgbwsvtPackageSnapshotCurrent(
    const RgbwsvtPackageValidation& package);

}  // namespace slicer_core
