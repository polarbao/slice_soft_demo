#pragma once

#include "slicer_core/geometry/OpenVdbSurfaceShell.h"
#include "slicer_core/material/MaterialChannelComposer.h"
#include "slicer_core/materials/texture_application/SurfaceTextureTransfer.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Options for mapping OpenVDB candidate masks into per-layer composer inputs.
 */
struct OpenVdbCandidateLayerBufferOptions
{
    std::array<std::uint8_t, 3> interior_rgb{128, 128, 128};
    std::vector<std::uint8_t> support_mask;
    std::vector<std::uint8_t> white_mask;
    std::vector<std::uint8_t> varnish_mask;
    std::uint8_t support_value{0};
    std::uint8_t white_value{0};
    std::uint8_t varnish_value{0};
    bool preserve_model_priority{true};
};

/**
 * @brief Per-layer counters produced before TIFF/package writing.
 */
struct OpenVdbCandidateLayerBufferStats
{
    int layer_index{0};
    int support_pixels{0};
    int model_pixels{0};
    int shell_pixels{0};
    int interior_pixels{0};
    int white_pixels{0};
    int varnish_pixels{0};
    int cleared_support_conflict_pixels{0};
};

/**
 * @brief One layer of OpenVDB candidate material composer input.
 */
struct OpenVdbCandidateLayerBuffer
{
    int layer_index{0};
    MaterialChannelComposerInput composer_input;
    OpenVdbCandidateLayerBufferStats stats;
};

/**
 * @brief Result of expanding OpenVDB 3D masks into per-layer composer inputs.
 */
struct OpenVdbCandidateLayerBufferBuildResult
{
    int width{0};
    int height{0};
    int depth{0};
    std::vector<OpenVdbCandidateLayerBuffer> layers;
    std::string error;
};

/**
 * @brief Build per-layer MaterialChannelComposerInput values from OpenVDB shell/interior masks.
 * @param shell OpenVDB shell/interior classification result.
 * @param transfer Surface RGB transfer result for shell voxels.
 * @param options Optional support/W/V masks and default material values.
 * @return Per-layer input buffers and diagnostics. No TIFF or package files are written.
 */
OpenVdbCandidateLayerBufferBuildResult BuildOpenVdbCandidateLayerBuffers(
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferResult& transfer,
    const OpenVdbCandidateLayerBufferOptions& options);

}  // namespace slicer_core
