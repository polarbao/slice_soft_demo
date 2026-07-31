#include "slicer_core/output/tiff/TiffBackendBuildInfo.h"
#include "slicer_core/output/tiff/TiffWriterFactory.h"
#include "slicer_core/tiff_io.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr std::array<std::uint16_t, 11U> commonRequiredTags{
    256U,
    257U,
    258U,
    259U,
    262U,
    270U,
    277U,
    284U,
    305U,
    338U,
    339U};

int Fail(const std::string& message)
{
    std::cerr << "tiff_writer_equivalence_unit_tests: "
              << message << '\n';
    return 1;
}

std::uint16_t ReadU16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    if (offset + 2U > bytes.size())
    {
        throw std::runtime_error("TIFF uint16 read outside file");
    }
    return static_cast<std::uint16_t>(
        bytes.at(offset)
        | (static_cast<std::uint16_t>(bytes.at(offset + 1U)) << 8U));
}

std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    if (offset + 4U > bytes.size())
    {
        throw std::runtime_error("TIFF uint32 read outside file");
    }
    return static_cast<std::uint32_t>(bytes.at(offset))
        | (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 3U)) << 24U);
}

std::set<std::uint16_t> ReadSingleIfdTags(
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
        throw std::runtime_error("expected little-endian Classic TIFF");
    }

    const std::size_t ifdOffset = ReadU32(bytes, 4U);
    const std::uint16_t entryCount = ReadU16(bytes, ifdOffset);
    std::set<std::uint16_t> tags;
    for (std::uint16_t index{0U}; index < entryCount; ++index)
    {
        const std::size_t entryOffset =
            ifdOffset + 2U + static_cast<std::size_t>(index) * 12U;
        tags.insert(ReadU16(bytes, entryOffset));
    }
    const std::size_t nextIfdOffset =
        ifdOffset + 2U + static_cast<std::size_t>(entryCount) * 12U;
    if (ReadU32(bytes, nextIfdOffset) != 0U)
    {
        throw std::runtime_error("multiple TIFF IFDs are not allowed");
    }
    return tags;
}

bool HasRequiredTags(
    const std::filesystem::path& path,
    const slicer_core::TiffStorageMode storageMode)
{
    const std::set<std::uint16_t> tags = ReadSingleIfdTags(path);
    for (const std::uint16_t tag : commonRequiredTags)
    {
        if (!tags.contains(tag))
        {
            return false;
        }
    }
    const std::array<std::uint16_t, 3U> strippedTags{
        273U,
        278U,
        279U};
    const std::array<std::uint16_t, 4U> tiledTags{
        322U,
        323U,
        324U,
        325U};
    if (storageMode == slicer_core::TiffStorageMode::Tiled)
    {
        for (const std::uint16_t tag : tiledTags)
        {
            if (!tags.contains(tag))
            {
                return false;
            }
        }
        return true;
    }
    for (const std::uint16_t tag : strippedTags)
    {
        if (!tags.contains(tag))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> MakePixels(
    const slicer_core::TiffImageSpec& spec,
    const std::size_t caseIndex)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(spec.width)
            * spec.height
            * spec.samples_per_pixel,
        255U);
    if (caseIndex == 0U)
    {
        return pixels;
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(spec.width) * spec.height;
    for (std::size_t pixelIndex{0U};
         pixelIndex < pixelCount;
         ++pixelIndex)
    {
        const std::size_t offset =
            pixelIndex * spec.samples_per_pixel;
        switch (caseIndex)
        {
            case 1U:
                pixels.at(offset) =
                    static_cast<std::uint8_t>((pixelIndex * 17U) % 255U);
                pixels.at(offset + 1U) =
                    static_cast<std::uint8_t>((pixelIndex * 31U) % 255U);
                pixels.at(offset + 2U) =
                    static_cast<std::uint8_t>((pixelIndex * 47U) % 255U);
                break;
            case 2U:
                pixels.at(offset + 3U) = 0U;
                break;
            case 3U:
                pixels.at(offset + 4U) = 0U;
                break;
            case 4U:
                pixels.at(offset + 5U) = 0U;
                break;
            default:
                for (std::size_t channel{0U};
                     channel < spec.samples_per_pixel;
                     ++channel)
                {
                    pixels.at(offset + channel) =
                        static_cast<std::uint8_t>(
                            (pixelIndex * 29U + channel * 43U) % 256U);
                }
                break;
        }
    }
    return pixels;
}

bool StatsAreEqual(
    const slicer_core::TiffChannelStats& first,
    const slicer_core::TiffChannelStats& second)
{
    return first.print_pixels == second.print_pixels
        && first.full_print_pixels == second.full_print_pixels
        && first.partial_print_pixels == second.partial_print_pixels
        && first.empty_pixels == second.empty_pixels
        && first.min_value == second.min_value
        && first.max_value == second.max_value;
}

bool ResultsAreEquivalent(
    const slicer_core::TiffReadResult& handwritten,
    const slicer_core::TiffReadResult& libTiff,
    const slicer_core::TiffImageSpec& expectedSpec)
{
    if (handwritten.pixels != libTiff.pixels
        || handwritten.pixels.empty()
        || handwritten.spec.width != expectedSpec.width
        || handwritten.spec.height != expectedSpec.height
        || handwritten.spec.storage_mode != expectedSpec.storage_mode
        || libTiff.spec.width != expectedSpec.width
        || libTiff.spec.height != expectedSpec.height
        || libTiff.spec.storage_mode != expectedSpec.storage_mode
        || handwritten.channel_checksums != libTiff.channel_checksums)
    {
        return false;
    }
    if (expectedSpec.storage_mode == slicer_core::TiffStorageMode::Tiled)
    {
        if (handwritten.spec.tile_width != expectedSpec.tile_width
            || handwritten.spec.tile_height != expectedSpec.tile_height
            || libTiff.spec.tile_width != expectedSpec.tile_width
            || libTiff.spec.tile_height != expectedSpec.tile_height)
        {
            return false;
        }
    }
    else if (handwritten.spec.rows_per_strip != expectedSpec.rows_per_strip
             || libTiff.spec.rows_per_strip != expectedSpec.rows_per_strip)
    {
        return false;
    }
    for (std::size_t channel{0U};
         channel < slicer_core::rgbwsv_channel_count;
         ++channel)
    {
        if (!StatsAreEqual(
                handwritten.channel_stats.at(channel),
                libTiff.channel_stats.at(channel)))
        {
            return false;
        }
    }
    return true;
}

bool RunStorageMatrix(
    const std::filesystem::path& directory,
    const slicer_core::TiffStorageMode storageMode)
{
    slicer_core::TiffImageSpec spec;
    spec.width = storageMode == slicer_core::TiffStorageMode::Tiled
        ? 19U
        : 17U;
    spec.height = 7U;
    spec.rows_per_strip = 3U;
    spec.tile_width = 16U;
    spec.tile_height = 16U;
    spec.storage_mode = storageMode;

    const std::unique_ptr<slicer_core::ITiffWriter> handwrittenWriter =
        slicer_core::CreateTiffWriter(
            slicer_core::TiffWriterBackend::Handwritten);
    const std::unique_ptr<slicer_core::ITiffWriter> libTiffWriter =
        slicer_core::CreateTiffWriter(
            slicer_core::TiffWriterBackend::LibTiff);
    const std::string storageName =
        slicer_core::tiff_storage_mode_string(storageMode);

    for (std::size_t caseIndex{0U}; caseIndex < 6U; ++caseIndex)
    {
        const std::vector<std::uint8_t> pixels =
            MakePixels(spec, caseIndex);
        const std::filesystem::path handwrittenPath =
            directory
            / (storageName + "_handwritten_"
               + std::to_string(caseIndex) + ".tiff");
        const std::filesystem::path libTiffPath =
            directory
            / (storageName + "_libtiff_"
               + std::to_string(caseIndex) + ".tiff");
        handwrittenWriter->Write(handwrittenPath, spec, pixels);
        libTiffWriter->Write(libTiffPath, spec, pixels);

        slicer_core::TiffReadResult handwritten;
        slicer_core::TiffReadResult libTiff;
        try
        {
            handwritten = slicer_core::read_rgbwsv_tiff(handwrittenPath);
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(
                handwrittenPath.string() + ": " + error.what());
        }
        try
        {
            libTiff = slicer_core::read_rgbwsv_tiff(libTiffPath);
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(
                libTiffPath.string() + ": " + error.what());
        }
        if (handwritten.pixels != pixels
            || libTiff.pixels != pixels
            || !ResultsAreEquivalent(handwritten, libTiff, spec)
            || !HasRequiredTags(handwrittenPath, storageMode)
            || !HasRequiredTags(libTiffPath, storageMode))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

int main()
{
    const slicer_core::TiffBackendBuildInfo buildInfo =
        slicer_core::GetTiffBackendBuildInfo();
    if (!buildInfo.libtiffdependencyavailable)
    {
        std::cout
            << "tiff_writer_equivalence_unit_tests: PASS (LibTIFF unavailable)\n";
        return 0;
    }

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_03d_writer_equivalence_"
           + std::to_string(
               std::chrono::steady_clock::now()
                   .time_since_epoch()
                   .count()));
    std::filesystem::create_directories(directory);
    try
    {
        const bool passed =
            RunStorageMatrix(
                directory,
                slicer_core::TiffStorageMode::Stripped)
            && RunStorageMatrix(
                directory,
                slicer_core::TiffStorageMode::Tiled);
        std::filesystem::remove_all(directory);
        if (!passed)
        {
            return Fail("handwritten and LibTIFF outputs were not equivalent");
        }
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(directory);
        return Fail(error.what());
    }

    std::cout << "tiff_writer_equivalence_unit_tests: PASS\n";
    return 0;
}
