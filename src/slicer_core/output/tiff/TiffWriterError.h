#pragma once

#include <stdexcept>
#include <string>

namespace slicer_core
{

/**
 * @brief Identifies stable failures raised by a TIFF writer backend.
 */
enum class TiffWriterErrorCode
{
    InvalidInput,
    OpenFailed,
    TagSetupFailed,
    StripWriteFailed,
    TileWriteFailed,
    CloseFailed,
    OutputValidationFailed,
    PublishFailed
};

/**
 * @brief Converts a writer error code to its stable protocol string.
 * @param code Writer error code.
 * @return Stable lowercase error identifier.
 */
std::string TiffWriterErrorCodeString(TiffWriterErrorCode code);

/**
 * @brief Carries a stable TIFF writer error code and diagnostic detail.
 */
class TiffWriterException final : public std::runtime_error
{
public:
    /**
     * @brief Constructs a writer exception.
     * @param code Stable writer error code.
     * @param detail Human-readable diagnostic detail.
     */
    TiffWriterException(
        TiffWriterErrorCode code,
        const std::string& detail);

    /**
     * @brief Returns the stable writer error code.
     * @return Error code supplied at construction.
     */
    TiffWriterErrorCode Code() const noexcept;

private:
    TiffWriterErrorCode m_code;
};

}  // namespace slicer_core
