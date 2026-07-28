#pragma once

#include "slicer_core/tiff_io.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Immutable identity and manifest metadata for one production TIFF layer.
 */
struct ProductionLayerRef
{
    std::string packageIdentity;
    std::string manifestHash;
    std::string sourceIdentity;
    int layerIndex{-1};
    double zMm{0.0};
    std::filesystem::path path;
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    TiffStorageMode storage{TiffStorageMode::Stripped};
    std::string checksum;
    int dpiX{0};
    int dpiY{0};
};

/**
 * @brief Manifest-derived index for one RGBWSV production package.
 */
struct ProductionPackageIndex
{
    std::filesystem::path packageDirectory;
    std::filesystem::path manifestPath;
    std::string packageIdentity;
    std::string manifestHash;
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    int dpiX{0};
    int dpiY{0};
    TiffStorageMode storage{TiffStorageMode::Stripped};
    std::vector<ProductionLayerRef> layers;
};

/**
 * @brief Decoded immutable six-channel production layer shared by preview consumers.
 */
struct RgbwsvLayerBuffer
{
    std::string sourceIdentity;
    int layerIndex{-1};
    double zMm{0.0};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    int dpiX{0};
    int dpiY{0};
    std::vector<std::uint8_t> pixels;
    std::array<TiffChannelStats, rgbwsv_channel_count> channelStats{};
    std::array<std::uint64_t, rgbwsv_channel_count> channelChecksums{};
    std::size_t decodedBytes{0U};
};

}  // namespace slicer_core
