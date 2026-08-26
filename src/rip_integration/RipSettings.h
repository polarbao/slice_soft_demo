#pragma once

#include <filesystem>
#include <string>

namespace slicesoft::rip
{

/** @brief Stable non-throwing result used by the RIP integration boundary. */
struct RipStatus
{
    bool ok{false};
    std::string code;
    std::string message;

    [[nodiscard]] static RipStatus Success();
    [[nodiscard]] static RipStatus Failure(
        std::string code,
        std::string message);
};

/** @brief Host-owned settings for one external RIP invocation. */
struct RipSettings
{
    bool auto_run_after_slice{false};
    int intent{0};
    int transparent_mode{0};
    int color_mode{0};
    bool continue_on_layer_error{false};
    int gray_bits{2};
    std::filesystem::path input_icc_path;
    std::filesystem::path output_icc_path;
    std::string output_directory_name{"rip"};
};

/** @brief Validate settings without loading the external RIP runtime. */
[[nodiscard]] RipStatus ValidateRipSettings(const RipSettings& settings);

}  // namespace slicesoft::rip
