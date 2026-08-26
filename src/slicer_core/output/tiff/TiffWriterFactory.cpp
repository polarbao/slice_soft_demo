#include "slicer_core/output/tiff/TiffWriterFactory.h"

#include "slicer_core/output/tiff/TiffWriterError.h"
#include "slicer_core/output/tiff/TiffWriterImplementations.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace slicer_core
{

std::string TiffWriterBackendString(const TiffWriterBackend backend)
{
    switch (backend)
    {
        case TiffWriterBackend::Handwritten:
            return "handwritten";
        case TiffWriterBackend::LibTiff:
            return "libtiff";
    }
    return "unknown";
}

TiffWriterBackend GetConfiguredTiffWriterBackend()
{
    const std::string configured{SLICESOFT_CONFIGURED_TIFF_BACKEND};
    if (configured == "handwritten")
    {
        return TiffWriterBackend::Handwritten;
    }
    if (configured == "libtiff")
    {
        return TiffWriterBackend::LibTiff;
    }
    throw std::runtime_error(
        "unsupported configured TIFF writer backend: " + configured);
}

TiffWriterBackend ResolveTiffWriterBackend(
    const TiffImageSpec& spec)
{
    const TiffWriterBackend configured =
        GetConfiguredTiffWriterBackend();
    if (configured == TiffWriterBackend::LibTiff
        && spec.storage_mode == TiffStorageMode::Tiled
        && spec.tile_width > 0U
        && spec.tile_height > 0U
        && (spec.tile_width < 16U
            || spec.tile_height < 16U
            || spec.tile_width % 16U != 0U
            || spec.tile_height % 16U != 0U))
    {
        throw TiffWriterException(
            TiffWriterErrorCode::InvalidInput,
            "LibTIFF tile width and height must be positive multiples of 16; "
            "the deprecated handwritten compatibility fallback is disabled");
    }
    return configured;
}

std::unique_ptr<ITiffWriter> CreateTiffWriter(
    const TiffWriterBackend backend)
{
    switch (backend)
    {
        case TiffWriterBackend::Handwritten:
            return detail::CreateHandwrittenTiffWriter();
        case TiffWriterBackend::LibTiff:
            return detail::CreateLibTiffWriter();
    }
    throw std::runtime_error("unsupported TIFF writer backend");
}

void WriteRgbwsvTiffWithConfiguredBackend(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    const TiffWriterBackend backend =
        ResolveTiffWriterBackend(spec);
    const std::unique_ptr<ITiffWriter> writer =
        CreateTiffWriter(backend);
    writer->Write(path, spec, pixels);
}

void write_rgbwsv_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    WriteRgbwsvTiffWithConfiguredBackend(path, spec, pixels);
}

void write_rgbwsvt_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    if (spec.samples_per_pixel != 7U)
    {
        throw TiffWriterException(
            TiffWriterErrorCode::InvalidInput,
            "RGBWSVT TIFF requires exactly seven samples per pixel");
    }
    if (GetConfiguredTiffWriterBackend() != TiffWriterBackend::LibTiff)
    {
        throw TiffWriterException(
            TiffWriterErrorCode::InvalidInput,
            "RGBWSVT TIFF requires the LibTIFF backend; handwritten output is not supported");
    }
    CreateTiffWriter(TiffWriterBackend::LibTiff)->Write(path, spec, pixels);
}

}  // namespace slicer_core
