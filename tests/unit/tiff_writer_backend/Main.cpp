#include "slicer_core/output/tiff/TiffBackendBuildInfo.h"
#include "slicer_core/output/tiff/TiffWriterFactory.h"
#include "slicer_core/tiff_io.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
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
    return decoded.pixels == pixels
        && decoded.spec.storage_mode
            == slicer_core::TiffStorageMode::Stripped
        && decoded.spec.rows_per_strip == spec.rows_per_strip;
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
                slicer_core::TiffStorageMode::Stripped)
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
            if (slicer_core::ResolveTiffWriterBackend(
                    slicer_core::TiffStorageMode::Tiled)
                != slicer_core::TiffWriterBackend::Handwritten)
            {
                return Fail("03D-03 tiled storage must retain handwritten fallback");
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
