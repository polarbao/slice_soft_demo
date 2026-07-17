#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/material/MaterialChannelComposer.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Input for converting exact Stage 12E masks into in-memory RGBWSV layers.
 */
struct TextureFillPartitionDiagnosticComposerRequest
{
    const GlobalTextureFillPartitionResult* partition{nullptr};
    const TextureFillPartitionTextureTransferResult* transfer{nullptr};
    std::string modelFillMaterial{"white"};
    std::uint8_t modelFillValue{0U};
    std::array<std::uint8_t, 3> modelFillRgb{0, 0, 0};
};

/**
 * @brief One true-Z diagnostic layer with exact semantic masks and RGBWSV memory.
 */
struct TextureFillPartitionDiagnosticLayer
{
    int layerIndex{0};
    double zMm{0.0};
    std::vector<std::uint8_t> textureSurfaceMask;
    std::vector<std::uint8_t> modelFillMask;
    MaterialChannelComposerResult composed;
};

/**
 * @brief Aggregate Stage 12E diagnostic-composer counters.
 */
struct TextureFillPartitionDiagnosticComposerStats
{
    std::uint64_t textureSurfaceVoxels{0U};
    std::uint64_t modelFillVoxels{0U};
    std::uint64_t modelFillWhiteVoxels{0U};
    std::uint64_t modelFillVarnishVoxels{0U};
    std::uint64_t modelFillRgbVoxels{0U};
    std::uint64_t supportPrintVoxels{0U};
    std::uint64_t emptyVoxels{0U};
};

/**
 * @brief In-memory diagnostic composition result without package output.
 */
struct TextureFillPartitionDiagnosticComposerResult
{
    bool available{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    int width{0};
    int height{0};
    int depth{0};
    std::array<std::string, 6> channelOrder{"R", "G", "B", "W", "S", "V"};
    std::vector<TextureFillPartitionDiagnosticLayer> layers;
    TextureFillPartitionDiagnosticComposerStats stats;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Compose exact texture/fill masks into diagnostic RGBWSV memory.
 * @param request Validated partition, transferred RGB, and model-fill material.
 * @return Per-layer diagnostic buffers; no TIFF or manifest is written.
 */
TextureFillPartitionDiagnosticComposerResult ComposeTextureFillPartitionDiagnostic(
    const TextureFillPartitionDiagnosticComposerRequest& request);

}  // namespace slicer_core
