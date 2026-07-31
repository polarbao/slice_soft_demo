#include "slicer_core/output/tiff/TiffWriterFactory.h"

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
    const TiffStorageMode storageMode)
{
    const TiffWriterBackend configured =
        GetConfiguredTiffWriterBackend();
    if (configured == TiffWriterBackend::LibTiff
        && storageMode == TiffStorageMode::Tiled)
    {
        return TiffWriterBackend::Handwritten;
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
        ResolveTiffWriterBackend(spec.storage_mode);
    const std::unique_ptr<ITiffWriter> writer =
        CreateTiffWriter(backend);
    writer->Write(path, spec, pixels);
}

}  // namespace slicer_core
