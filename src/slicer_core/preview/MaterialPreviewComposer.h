#pragma once

#include "slicer_core/preview/ProductionLayerRef.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Supported TIFF-native production material preview modes.
 */
enum class MaterialPreviewMode
{
    Red,
    Green,
    Blue,
    White,
    Support,
    Varnish,
    Rgb,
    RgbWhite,
    RgbSupport,
    RgbVarnish,
    RgbSupportWhiteVarnish,
    Occupancy,
    Empty,
};

/**
 * @brief One display-only straight-alpha RGBA color.
 */
struct PreviewColor
{
    std::uint8_t red{0U};
    std::uint8_t green{0U};
    std::uint8_t blue{0U};
    std::uint8_t alpha{255U};

    bool operator==(const PreviewColor& other) const = default;
};

/**
 * @brief Configurable colors used only for in-memory production preview.
 */
struct MaterialPreviewPalette
{
    PreviewColor empty{255U, 255U, 255U, 255U};
    PreviewColor red{255U, 0U, 0U, 255U};
    PreviewColor green{0U, 255U, 0U, 255U};
    PreviewColor blue{0U, 0U, 255U, 255U};
    PreviewColor white{0U, 170U, 255U, 180U};
    /// 支撑伪彩色取品红。此前为纯绿 (0,255,0)，与真实材质的绿（例如 03.obj 的
    /// 材质 01 = 63,190,126）在组合预览里难以分辨，曾迫使结果页默认退为 RGB-only、
    /// 从而让支撑在默认视图中完全不可见。品红不会与任何真实材质色相撞。
    PreviewColor support{255U, 0U, 255U, 180U};
    PreviewColor varnish{127U, 127U, 127U, 180U};
    PreviewColor occupancy{80U, 80U, 80U, 255U};
};

/**
 * @brief One deterministic material preview composition request.
 */
struct MaterialPreviewRequest
{
    MaterialPreviewMode mode{MaterialPreviewMode::Rgb};
    MaterialPreviewPalette palette;
};

/**
 * @brief Production-channel statistics computed independently from display colors.
 */
struct MaterialPreviewStats
{
    std::uint64_t pixelCount{0U};
    std::uint64_t rgbPixels{0U};
    std::uint64_t whitePixels{0U};
    std::uint64_t supportPixels{0U};
    std::uint64_t varnishPixels{0U};
    std::uint64_t occupiedPixels{0U};
    std::uint64_t emptyPixels{0U};
    std::uint64_t multiMaterialPixels{0U};
};

/**
 * @brief Display-ready RGBA pixels derived from one immutable RGBWSV layer.
 */
struct MaterialPreviewResult
{
    std::string sourceIdentity;
    int layerIndex{-1};
    double zMm{0.0};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    int dpiX{0};
    int dpiY{0};
    std::vector<std::uint8_t> rgba;
    MaterialPreviewStats stats;
};

/**
 * @brief Exact production values and print flags for one TIFF pixel.
 */
struct MaterialPixelProbe
{
    std::uint32_t x{0U};
    std::uint32_t y{0U};
    std::array<std::uint8_t, rgbwsv_channel_count> values{};
    bool hasRgb{false};
    bool hasWhite{false};
    bool hasSupport{false};
    bool hasVarnish{false};
    bool isEmpty{true};
    bool multipleMaterials{false};
};

/**
 * @brief Stable material preview validation error categories.
 */
enum class MaterialPreviewErrorCode
{
    BufferInvalid,
    DimensionInvalid,
    PixelOutOfRange,
    ModeInvalid,
};

/**
 * @brief Convert a material preview error to its stable protocol string.
 * @param code Error category.
 * @return Stable MATERIAL_PREVIEW_* string.
 */
std::string MaterialPreviewErrorCodeString(
    MaterialPreviewErrorCode code);

/**
 * @brief Structured failure raised by material preview composition.
 */
class MaterialPreviewError final : public std::runtime_error
{
public:
    /**
     * @brief Construct a stable material preview failure.
     * @param code Error category.
     * @param message Technical diagnostic message.
     */
    MaterialPreviewError(
        MaterialPreviewErrorCode code,
        std::string message);

    /**
     * @brief Return the stable error category.
     * @return Error category.
     */
    MaterialPreviewErrorCode Code() const noexcept;

private:
    MaterialPreviewErrorCode m_code;
};

/**
 * @brief Compose display-only RGBA views from one production RGBWSV buffer.
 */
class MaterialPreviewComposer final
{
public:
    /**
     * @brief Compose one deterministic material preview.
     * @param buffer Immutable production RGBWSV layer.
     * @param request Preview mode and display-only palette.
     * @return RGBA pixels, source metadata, and production statistics.
     */
    static MaterialPreviewResult Compose(
        const RgbwsvLayerBuffer& buffer,
        const MaterialPreviewRequest& request);

    /**
     * @brief Read exact six-channel production values at one raw TIFF pixel.
     * @param buffer Immutable production RGBWSV layer.
     * @param x Raw TIFF X coordinate.
     * @param y Raw TIFF Y coordinate.
     * @return Exact values and material print flags.
     */
    static MaterialPixelProbe Probe(
        const RgbwsvLayerBuffer& buffer,
        std::uint32_t x,
        std::uint32_t y);
};

}  // namespace slicer_core
