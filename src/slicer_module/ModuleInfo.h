#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief Returns the frozen module identifier.
 * @return The constant identifier `slicer`.
 */
[[nodiscard]] std::string_view GetModuleId() noexcept;

/**
 * @brief Returns the frozen semantic module version.
 * @return The constant semantic version `0.1.0`.
 */
[[nodiscard]] std::string_view GetModuleVersion() noexcept;

/**
 * @brief Returns the MSVC runtime identity of the current binary.
 * @return `MSVC-x64-MD` for Release or `MSVC-x64-MDd` for Debug.
 */
[[nodiscard]] std::string_view GetModuleRuntime() noexcept;

/**
 * @brief Returns the supported build configuration of the current binary.
 * @return `Release` or `Debug`.
 */
[[nodiscard]] std::string_view GetModuleBuildConfig() noexcept;

/**
 * @brief Returns the immutable UTF-8 module-information JSON.
 * @return A process-lifetime view of the `slicesoft.module_info.1` object.
 * @note The function performs no allocation, persistence, or Worker startup.
 */
[[nodiscard]] std::string_view GetModuleInfoJson() noexcept;

}  // namespace slicesoft::module
