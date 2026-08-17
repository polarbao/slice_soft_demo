#include "rip_integration/RipSettings.h"

#include <utility>

namespace slicesoft::rip
{

RipStatus RipStatus::Success()
{
    return RipStatus{true, {}, {}};
}

RipStatus RipStatus::Failure(
    std::string code,
    std::string message)
{
    return RipStatus{false, std::move(code), std::move(message)};
}

RipStatus ValidateRipSettings(const RipSettings& settings)
{
    if (settings.intent < 0 || settings.intent > 3)
    {
        return RipStatus::Failure(
            "RIP_SETTINGS_INTENT_INVALID",
            "RIP rendering intent must be in the inclusive range 0..3");
    }
    if (settings.color_mode != 0)
    {
        return RipStatus::Failure(
            "RIP_SETTINGS_COLOR_MODE_UNSUPPORTED",
            "the current RIP runtime only has a documented color mode of 0");
    }
    if (settings.gray_bits != 1 && settings.gray_bits != 2)
    {
        return RipStatus::Failure(
            "RIP_SETTINGS_GRAY_BITS_INVALID",
            "RIP device gray bits must be 1 or 2");
    }
    if (settings.output_directory_name != "rip")
    {
        return RipStatus::Failure(
            "RIP_SETTINGS_OUTPUT_DIRECTORY_INVALID",
            "the RIP output directory name is fixed to 'rip'");
    }
    if (settings.input_icc_path.empty()
        || settings.output_icc_path.empty())
    {
        return RipStatus::Failure(
            "RIP_SETTINGS_ICC_PATH_MISSING",
            "both input and output ICC paths are required");
    }
    return RipStatus::Success();
}

}  // namespace slicesoft::rip
