#pragma once

#include "slicer_core/tiff_io.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace slicer_core
{

/**
 * @brief Identifies a TIFF production writer implementation.
 */
enum class TiffWriterBackend
{
    Handwritten,
    LibTiff
};

/**
 * @brief Abstracts an RGBWSV TIFF writer without changing the package protocol.
 */
class ITiffWriter
{
public:
    /**
     * @brief Destroys the writer.
     */
    virtual ~ITiffWriter() = default;

    /**
     * @brief Returns the implementation represented by this writer.
     * @return Writer backend identifier.
     */
    virtual TiffWriterBackend Backend() const noexcept = 0;

    /**
     * @brief Writes one RGBWSV TIFF image.
     * @param path Destination TIFF path.
     * @param spec Fixed TIFF image and storage contract.
     * @param pixels Contiguous RGBWSV uint8 pixels.
     */
    virtual void Write(
        const std::filesystem::path& path,
        const TiffImageSpec& spec,
        std::span<const std::uint8_t> pixels) const = 0;
};

}  // namespace slicer_core
