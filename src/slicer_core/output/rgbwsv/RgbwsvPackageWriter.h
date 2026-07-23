#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Grid metadata written into an RGBWSV production package.
 */
struct RgbwsvProductionGridSpec
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    int dpiX{600};
    int dpiY{600};
    double pixelSizeXmm{25.4 / 600.0};
    double pixelSizeYmm{25.4 / 600.0};
    double layerThicknessMm{0.01};
    double originXmm{0.0};
    double originYmm{0.0};
    double originZmm{0.0};
};

/**
 * @brief TIFF storage settings shared by Legacy and Global Surface Shell output.
 */
struct RgbwsvProductionStorageSpec
{
    std::string storageMode{"stripped"};
    int rowsPerStrip{64};
    int tileWidth{256};
    int tileHeight{256};
};

/**
 * @brief Production package preview settings.
 */
struct RgbwsvProductionPreviewSpec
{
    bool enabled{true};
    std::string format{"ppm"};
    int interval{10};
};

/**
 * @brief Non-owning view used by the shared per-layer TIFF writer.
 */
struct RgbwsvProductionLayerView
{
    int widthPx{0};
    int heightPx{0};
    std::span<const std::uint8_t> channels;
};

/**
 * @brief Complete request for publishing an admitted RGBWSV production package.
 */
struct RgbwsvProductionPackageWriteRequest
{
    std::filesystem::path packageDir;
    std::filesystem::path sourceConfigPath;
    std::filesystem::path sourceModelPath;
    std::string sourceFormat;
    std::string requestedPipelineMode;
    std::string effectivePipelineMode;
    std::string productionAcceptance{"not_evaluated"};
    RgbwsvProductionGridSpec grid;
    RgbwsvProductionStorageSpec storage;
    RgbwsvProductionPreviewSpec preview;
    std::vector<RgbwsvProductionLayer> layers;
};

/**
 * @brief Result returned after an RGBWSV package is validated and published.
 */
struct RgbwsvProductionPackageWriteResult
{
    bool productionOutputWritten{false};
    bool fallbackApplied{false};
    int layerCount{0};
    std::filesystem::path packageDir;
    std::filesystem::path replacedPackageBackupDir;
};

/**
 * @brief Write one RGBWSV TIFF through the shared fixed-protocol writer.
 * @param path Destination TIFF path.
 * @param storage TIFF storage settings.
 * @param layer Final interleaved RGBWSV bytes and dimensions.
 */
void WriteRgbwsvProductionLayerTiff(
    const std::filesystem::path& path,
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvProductionLayerView& layer);

/**
 * @brief Validate, stage, RIP-check, and atomically publish a production package.
 * @param request Admitted final RGBWSV layers and package metadata.
 * @return Published package summary.
 */
RgbwsvProductionPackageWriteResult WriteRgbwsvProductionPackage(
    const RgbwsvProductionPackageWriteRequest& request);

}  // namespace slicer_core
