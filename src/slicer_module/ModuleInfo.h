#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief 返回冻结的模块标识。
 * @return 常量标识 `slicer`。
 */
[[nodiscard]] std::string_view GetModuleId() noexcept;

/**
 * @brief 返回冻结的模块语义版本。
 * @return 常量语义版本 `0.1.0`。
 */
[[nodiscard]] std::string_view GetModuleVersion() noexcept;

/**
 * @brief 返回当前二进制的 MSVC 运行库标识。
 * @return Release 为 `MSVC-x64-MD`，Debug 为 `MSVC-x64-MDd`。
 */
[[nodiscard]] std::string_view GetModuleRuntime() noexcept;

/**
 * @brief 返回当前二进制支持的构建配置。
 * @return `Release` 或 `Debug`。
 */
[[nodiscard]] std::string_view GetModuleBuildConfig() noexcept;

/**
 * @brief 返回不可变的 UTF-8 模块信息 JSON。
 * @return 有效期与进程相同的 `slicesoft.module_info.1` 对象视图。
 * @note 此函数不执行分配、持久化或 Worker 启动。
 */
[[nodiscard]] std::string_view GetModuleInfoJson() noexcept;

}  // namespace slicesoft::module
