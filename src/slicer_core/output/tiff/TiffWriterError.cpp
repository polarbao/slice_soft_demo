#include "slicer_core/output/tiff/TiffWriterError.h"

namespace slicer_core
{

std::string TiffWriterErrorCodeString(const TiffWriterErrorCode code)
{
    switch (code)
    {
        case TiffWriterErrorCode::InvalidInput:
            return "tiff_invalid_input";
        case TiffWriterErrorCode::OpenFailed:
            return "tiff_open_failed";
        case TiffWriterErrorCode::TagSetupFailed:
            return "tiff_tag_setup_failed";
        case TiffWriterErrorCode::StripWriteFailed:
            return "tiff_strip_write_failed";
        case TiffWriterErrorCode::TileWriteFailed:
            return "tiff_tile_write_failed";
        case TiffWriterErrorCode::CloseFailed:
            return "tiff_close_failed";
        case TiffWriterErrorCode::OutputValidationFailed:
            return "tiff_output_validation_failed";
        case TiffWriterErrorCode::PublishFailed:
            return "tiff_publish_failed";
    }
    return "tiff_unknown_error";
}

TiffWriterException::TiffWriterException(
    const TiffWriterErrorCode code,
    const std::string& detail)
    : std::runtime_error(
          TiffWriterErrorCodeString(code)
          + (detail.empty() ? std::string{} : ": " + detail)),
      m_code(code)
{
}

TiffWriterErrorCode TiffWriterException::Code() const noexcept
{
    return m_code;
}

}  // namespace slicer_core
