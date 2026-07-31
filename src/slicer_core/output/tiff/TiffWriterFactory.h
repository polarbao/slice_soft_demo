#pragma once

#include "slicer_core/output/tiff/ITiffWriter.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

namespace slicer_core
{

/**
 * @brief Converts a TIFF writer backend to its stable configuration name.
 * @param backend Writer backend.
 * @return `handwritten` or `libtiff`.
 */
std::string TiffWriterBackendString(TiffWriterBackend backend);

/**
 * @brief Returns the backend selected by the current build lane.
 * @return Configured writer backend.
 */
TiffWriterBackend GetConfiguredTiffWriterBackend();

/**
 * @brief Resolves the effective backend for a storage mode.
 * @param storageMode Requested TIFF storage mode.
 * @return Effective backend. In 03D-03 tiled output retains handwritten fallback.
 */
TiffWriterBackend ResolveTiffWriterBackend(TiffStorageMode storageMode);

/**
 * @brief Creates a writer for an explicitly selected backend.
 * @param backend Writer backend.
 * @return Owned writer instance.
 */
std::unique_ptr<ITiffWriter> CreateTiffWriter(TiffWriterBackend backend);

/**
 * @brief Writes with the effective backend selected for the current build.
 * @param path Destination TIFF path.
 * @param spec Fixed TIFF image and storage contract.
 * @param pixels Contiguous RGBWSV uint8 pixels.
 */
void WriteRgbwsvTiffWithConfiguredBackend(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

}  // namespace slicer_core
