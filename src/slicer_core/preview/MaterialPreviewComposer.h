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
    /// 缩裹材料 T 通道。仅 p0.rgbwsvt.1 包存在该平面。
    Transfer,
    Rgb,
    RgbWhite,
    RgbSupport,
    RgbVarnish,
    RgbSupportWhiteVarnish,
    /// 七通道组合判读视图：在 W/S/V 之上再叠加缩裹 T。
    RgbSupportWhiteVarnishTransfer,
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
    PreviewColor support{0U, 255U, 0U, 180U};
    PreviewColor varnish{127U, 127U, 127U, 180U};
    // 缩裹伪彩色取品红，与 config.h 的 preview.transfer_color 默认值一致。
    // 选品红是因为它不与既有三色相撞：白墨青蓝、支撑纯绿、光油中灰；
    // 也不与 03/08 系列缩裹材质的真实色（浅桃 255,220,198 与黄 255,255,0）混淆。
    PreviewColor transfer{255U, 0U, 255U, 180U};
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
    /// @param transferPlane 可选的逐像素 T 通道平面（长度须等于像素数）。
    ///        六通道包传 nullptr；七通道包传 T 平面，使缩裹可参与合成而非被丢弃。
    static MaterialPreviewResult Compose(
        const RgbwsvLayerBuffer& buffer,
        const MaterialPreviewRequest& request,
        const std::vector<std::uint8_t>* transferPlane = nullptr);

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
