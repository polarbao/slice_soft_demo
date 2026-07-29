#pragma once

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"
#include "slicer_core/preview/ProductionLayerRef.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Input for mapping diagnostic texture/fill ownership onto one production TIFF layer.
 */
struct TextureFillPartitionSemanticPreviewRequest
{
    const GlobalTextureFillPartitionResult* partition{nullptr};
    const RgbwsvLayerBuffer* productionlayer{nullptr};
};

/**
 * @brief Same-layer diagnostic semantics aligned to one production TIFF buffer.
 */
struct TextureFillPartitionSemanticPreviewResult
{
    bool available{false};
    std::string status{"unavailable"};
    std::string errorcode;
    std::string message;
    std::string productionsourceidentity;
    int layerindex{-1};
    double zmm{0.0};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    bool alltexture{false};
    bool fullclosurelinkageevaluated{false};
    std::uint64_t modelpixels{0U};
    std::uint64_t texturesurfacepixels{0U};
    std::uint64_t modelfillpixels{0U};
    std::uint64_t supportpixels{0U};
    std::uint64_t varnishpixels{0U};
    double texturecoverage{0.0};
    double modelfillcoverage{0.0};
    std::vector<std::uint8_t> modelmask;
    std::vector<std::uint8_t> texturesurfacemask;
    std::vector<std::uint8_t> modelfillmask;
    std::vector<std::uint8_t> supportmask;
    std::vector<std::uint8_t> varnishmask;
};

/**
 * @brief Map one diagnostic three-dimensional partition to the exact production TIFF layer.
 * @param request Validated diagnostic partition and manifest-derived TIFF buffer.
 * @return Same-size semantic masks and counters; no production output is written.
 */
TextureFillPartitionSemanticPreviewResult
BuildTextureFillPartitionSemanticPreview(
    const TextureFillPartitionSemanticPreviewRequest& request);

}  // namespace slicer_core
