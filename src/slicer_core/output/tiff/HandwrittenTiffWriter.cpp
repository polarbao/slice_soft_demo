#include "slicer_core/output/tiff/TiffWriterImplementations.h"

#include <memory>

namespace slicer_core
{
namespace
{

class HandwrittenTiffWriter final : public ITiffWriter
{
public:
    TiffWriterBackend Backend() const noexcept override
    {
        return TiffWriterBackend::Handwritten;
    }

    void Write(
        const std::filesystem::path& path,
        const TiffImageSpec& spec,
        const std::span<const std::uint8_t> pixels) const override
    {
        if (spec.storage_mode == TiffStorageMode::Tiled)
        {
            write_rgbwsv_tiled_tiff(path, spec, pixels);
            return;
        }
        write_rgbwsv_stripped_tiff(path, spec, pixels);
    }
};

}  // namespace

namespace detail
{

std::unique_ptr<ITiffWriter> CreateHandwrittenTiffWriter()
{
    return std::make_unique<HandwrittenTiffWriter>();
}

}  // namespace detail
}  // namespace slicer_core
