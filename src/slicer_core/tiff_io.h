#pragma once

#include "slicer_core/TiffReadApi.h"

#include <filesystem>
#include <span>

namespace slicer_core
{

/**
 * @brief Writes one tiled RGBWSV uint8 TIFF with the handwritten backend.
 * @param path Destination TIFF path.
 * @param spec Image and tiled storage contract.
 * @param pixels Contiguous RGBWSV uint8 pixels.
 */
void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

/**
 * @brief Writes one stripped RGBWSV uint8 TIFF with the handwritten backend.
 * @param path Destination TIFF path.
 * @param spec Image and stripped storage contract.
 * @param pixels Contiguous RGBWSV uint8 pixels.
 */
void write_rgbwsv_stripped_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

/**
 * @brief Writes one RGBWSV uint8 TIFF with the configured engine backend.
 * @param path Destination TIFF path.
 * @param spec Image and storage contract.
 * @param pixels Contiguous RGBWSV uint8 pixels.
 */
void write_rgbwsv_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

/**
 * @brief Writes one seven-channel RGBWSVT TIFF through LibTIFF only.
 *
 * The handwritten compatibility backend intentionally rejects this protocol.
 */
void write_rgbwsvt_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

}  // namespace slicer_core
