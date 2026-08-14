#include "slicer_core/output/tiff/TiffBackendBuildInfo.h"
#include "slicer_core/output/tiff/TiffWriterError.h"
#include "slicer_core/output/tiff/TiffWriterFactory.h"
#include "slicer_core/tiff_io.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

int Fail(const std::string& message)
{
    std::cerr << "tiff_writer_backend_unit_tests: " << message << '\n';
    return 1;
}

std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width)
            * height
            * slicer_core::rgbwsv_channel_count,
        255U);
    for (std::size_t index{0U}; index < pixels.size(); ++index)
    {
        pixels.at(index) =
            static_cast<std::uint8_t>((index * 37U + 11U) % 256U);
    }
    return pixels;
}

bool WriteAndDecode(
    const slicer_core::TiffWriterBackend backend,
    const std::filesystem::path& path,
    const slicer_core::TiffImageSpec& spec,
    const std::vector<std::uint8_t>& pixels)
{
    std::unique_ptr<slicer_core::ITiffWriter> writer =
        slicer_core::CreateTiffWriter(backend);
    if (writer->Backend() != backend)
    {
        return false;
    }
    writer->Write(path, spec, pixels);
    const slicer_core::TiffReadResult decoded =
        slicer_core::read_rgbwsv_tiff(path);
    if (decoded.pixels != pixels
        || decoded.spec.storage_mode != spec.storage_mode)
    {
        return false;
    }
    if (spec.storage_mode == slicer_core::TiffStorageMode::Tiled)
    {
        return decoded.spec.tile_width == spec.tile_width
            && decoded.spec.tile_height == spec.tile_height;
    }
    return decoded.spec.rows_per_strip == spec.rows_per_strip;
}

std::size_t CountTemporarySiblings(
    const std::filesystem::path& path)
{
    const std::string prefix = path.filename().string() + ".tmp.";
    std::size_t count{0U};
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(path.parent_path()))
    {
        if (entry.path().filename().string().starts_with(prefix))
        {
            ++count;
        }
    }
    return count;
}

std::uint16_t ReadU16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    return static_cast<std::uint16_t>(
        bytes.at(offset)
        | (static_cast<std::uint16_t>(bytes.at(offset + 1U)) << 8U));
}

std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes.at(offset))
        | (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 3U)) << 24U);
}

std::vector<std::uint8_t> ReadSingleTileBytes(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (bytes.size() < 8U
        || bytes.at(0) != 'I'
        || bytes.at(1) != 'I'
        || ReadU16(bytes, 2U) != 42U)
    {
        throw std::runtime_error("unexpected LibTIFF tiled header");
    }

    const std::size_t ifdOffset = ReadU32(bytes, 4U);
    const std::uint16_t entryCount = ReadU16(bytes, ifdOffset);
    std::uint32_t tileOffset{0U};
    std::uint32_t tileByteCount{0U};
    for (std::uint16_t index{0U}; index < entryCount; ++index)
    {
        const std::size_t entryOffset =
            ifdOffset + 2U + static_cast<std::size_t>(index) * 12U;
        const std::uint16_t tag = ReadU16(bytes, entryOffset);
        const std::uint32_t count = ReadU32(bytes, entryOffset + 4U);
        if (count != 1U)
        {
            continue;
        }
        if (tag == 324U)
        {
            tileOffset = ReadU32(bytes, entryOffset + 8U);
        }
        else if (tag == 325U)
        {
            tileByteCount = ReadU32(bytes, entryOffset + 8U);
        }
    }
    if (tileOffset == 0U
        || tileByteCount == 0U
        || static_cast<std::size_t>(tileOffset) + tileByteCount
            > bytes.size())
    {
        throw std::runtime_error("invalid single-tile offset or byte count");
    }
    return {
        bytes.begin() + static_cast<std::ptrdiff_t>(tileOffset),
        bytes.begin()
            + static_cast<std::ptrdiff_t>(tileOffset + tileByteCount)};
}

bool TilePaddingIsWhite(
    const std::vector<std::uint8_t>& tile,
    const slicer_core::TiffImageSpec& spec,
    const std::vector<std::uint8_t>& pixels)
{
    for (std::uint32_t row{0U}; row < spec.tile_height; ++row)
    {
        for (std::uint32_t column{0U}; column < spec.tile_width; ++column)
        {
            for (std::uint16_t channel{0U};
                 channel < spec.samples_per_pixel;
                 ++channel)
            {
                const std::size_t tileOffset =
                    (static_cast<std::size_t>(row) * spec.tile_width
                     + column)
                    * spec.samples_per_pixel
                    + channel;
                const std::uint8_t expected =
                    row < spec.height && column < spec.width
                    ? pixels.at(
                          (static_cast<std::size_t>(row) * spec.width
                           + column)
                              * spec.samples_per_pixel
                          + channel)
                    : 255U;
                if (tile.at(tileOffset) != expected)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace

int main()
{
    constexpr std::uint32_t width{13U};
    constexpr std::uint32_t height{5U};
    slicer_core::TiffImageSpec spec;
    spec.width = width;
    spec.height = height;
    spec.rows_per_strip = 2U;
    spec.storage_mode = slicer_core::TiffStorageMode::Stripped;
    const std::vector<std::uint8_t> pixels = MakePixels(width, height);

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_03d_writer_backend_"
           + std::to_string(
               std::chrono::steady_clock::now()
                   .time_since_epoch()
                   .count()));
    std::filesystem::create_directories(directory);

    try
    {
        if (!WriteAndDecode(
                slicer_core::TiffWriterBackend::Handwritten,
                directory / "handwritten.tiff",
                spec,
                pixels))
        {
            return Fail("handwritten adapter did not preserve pixels");
        }

        const slicer_core::TiffBackendBuildInfo buildInfo =
            slicer_core::GetTiffBackendBuildInfo();
        const slicer_core::TiffWriterBackend configured =
            slicer_core::GetConfiguredTiffWriterBackend();
        const bool expectsLibTiff =
            buildInfo.libtiffdependencyavailable;
        if ((configured == slicer_core::TiffWriterBackend::LibTiff)
            != expectsLibTiff)
        {
            return Fail("configured backend does not match build capability");
        }
        if (slicer_core::ResolveTiffWriterBackend(
                spec)
            != configured)
        {
            return Fail("stripped storage did not select configured backend");
        }
        if (expectsLibTiff)
        {
            if (!WriteAndDecode(
                    slicer_core::TiffWriterBackend::LibTiff,
                    directory / "libtiff.tiff",
                    spec,
                    pixels))
            {
                return Fail("LibTIFF stripped writer did not preserve pixels");
            }
#ifdef _WIN32
            std::filesystem::path longPathDirectory = directory;
            constexpr std::size_t targetDirectoryLength{230U};
            const std::size_t currentLength =
                longPathDirectory.native().size();
            if (currentLength + 1U < targetDirectoryLength)
            {
                longPathDirectory /=
                    std::string(
                        targetDirectoryLength - currentLength - 1U,
                        'a');
            }
            const std::filesystem::path longPath =
                longPathDirectory / "layer_000000.tiff";
            if (!WriteAndDecode(
                    slicer_core::TiffWriterBackend::LibTiff,
                    longPath,
                    spec,
                    pixels))
            {
                return Fail(
                    "LibTIFF long-path writer did not preserve pixels");
            }
#endif
            slicer_core::TiffImageSpec tiledSpec = spec;
            tiledSpec.storage_mode = slicer_core::TiffStorageMode::Tiled;
            tiledSpec.tile_width = 16U;
            tiledSpec.tile_height = 16U;
            if (slicer_core::ResolveTiffWriterBackend(tiledSpec)
                != slicer_core::TiffWriterBackend::LibTiff)
            {
                return Fail("aligned tiled storage did not select LibTIFF");
            }
            if (!WriteAndDecode(
                    slicer_core::TiffWriterBackend::LibTiff,
                    directory / "libtiff_tiled.tiff",
                    tiledSpec,
                    pixels))
            {
                return Fail("LibTIFF tiled writer did not preserve pixels");
            }
            const std::vector<std::uint8_t> rawTile =
                ReadSingleTileBytes(directory / "libtiff_tiled.tiff");
            if (!TilePaddingIsWhite(rawTile, tiledSpec, pixels))
            {
                return Fail("LibTIFF tiled padding was not initialized to 255");
            }

            slicer_core::TiffImageSpec nonstandardTiledSpec = tiledSpec;
            nonstandardTiledSpec.tile_width = 8U;
            nonstandardTiledSpec.tile_height = 4U;
            const std::filesystem::path fallbackPath =
                directory / "deprecated_fallback.tiff";
            const std::string fallbackSentinel{"existing-fallback-output"};
            {
                std::ofstream output(fallbackPath, std::ios::binary);
                output << fallbackSentinel;
            }
            try
            {
                slicer_core::WriteRgbwsvTiffWithConfiguredBackend(
                    fallbackPath,
                    nonstandardTiledSpec,
                    pixels);
                return Fail(
                    "nonstandard tiled storage used the deprecated fallback");
            }
            catch (const slicer_core::TiffWriterException& error)
            {
                if (error.Code()
                    != slicer_core::TiffWriterErrorCode::InvalidInput)
                {
                    return Fail(
                        "nonstandard tiled storage returned the wrong error code");
                }
            }
            std::ifstream fallbackInput(fallbackPath, std::ios::binary);
            const std::string preservedFallback{
                std::istreambuf_iterator<char>{fallbackInput},
                std::istreambuf_iterator<char>{}};
            if (preservedFallback != fallbackSentinel)
            {
                return Fail(
                    "rejected nonstandard tiled storage replaced the existing output");
            }
            if (CountTemporarySiblings(fallbackPath) != 0U)
            {
                return Fail(
                    "rejected nonstandard tiled storage left a temporary sibling");
            }

            const std::filesystem::path atomicPath =
                directory / "atomic_failure.tiff";
            const std::string sentinel{"existing-output"};
            {
                std::ofstream output(atomicPath, std::ios::binary);
                output << sentinel;
            }
            slicer_core::TiffImageSpec invalidSpec = tiledSpec;
            invalidSpec.tile_width = 0U;
            try
            {
                const std::unique_ptr<slicer_core::ITiffWriter> writer =
                    slicer_core::CreateTiffWriter(
                        slicer_core::TiffWriterBackend::LibTiff);
                writer->Write(atomicPath, invalidSpec, pixels);
                return Fail("invalid LibTIFF tile dimensions were accepted");
            }
            catch (const slicer_core::TiffWriterException& error)
            {
                if (error.Code()
                    != slicer_core::TiffWriterErrorCode::InvalidInput)
                {
                    return Fail("invalid tiled input returned the wrong error code");
                }
            }
            std::ifstream atomicInput(atomicPath, std::ios::binary);
            const std::string preserved{
                std::istreambuf_iterator<char>{atomicInput},
                std::istreambuf_iterator<char>{}};
            if (preserved != sentinel)
            {
                return Fail("failed LibTIFF write replaced the existing output");
            }
            if (CountTemporarySiblings(atomicPath) != 0U)
            {
                return Fail("failed LibTIFF write left a temporary sibling");
            }

            const std::filesystem::path publishTarget =
                directory / "publish_target.tiff";
            std::filesystem::create_directory(publishTarget);
            try
            {
                const std::unique_ptr<slicer_core::ITiffWriter> writer =
                    slicer_core::CreateTiffWriter(
                        slicer_core::TiffWriterBackend::LibTiff);
                writer->Write(publishTarget, tiledSpec, pixels);
                return Fail("LibTIFF publish to a directory was accepted");
            }
            catch (const slicer_core::TiffWriterException& error)
            {
                if (error.Code()
                    != slicer_core::TiffWriterErrorCode::PublishFailed)
                {
                    return Fail("publish failure returned the wrong error code");
                }
            }
            if (!std::filesystem::is_directory(publishTarget)
                || CountTemporarySiblings(publishTarget) != 0U)
            {
                return Fail("publish failure did not preserve and clean outputs");
            }
        }
        else
        {
            try
            {
                (void)slicer_core::CreateTiffWriter(
                    slicer_core::TiffWriterBackend::LibTiff);
                return Fail("unavailable LibTIFF writer was created");
            }
            catch (const std::runtime_error&)
            {
            }
        }
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(directory);
        return Fail(error.what());
    }

    std::filesystem::remove_all(directory);
    std::cout << "tiff_writer_backend_unit_tests: PASS\n";
    return 0;
}
