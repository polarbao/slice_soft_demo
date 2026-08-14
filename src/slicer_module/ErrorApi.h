#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief 替换当前线程最近一次 SPI 失败 JSON。
 * @param code 稳定的 PM-SLICER 错误码。
 * @param message 人类可读的 UTF-8 错误摘要。
 * @param detail 人类可读的 UTF-8 诊断详情。
 *
 * 成功操作不会调用此函数，因此最近一次失败会一直保留，
 * 直到同一线程出现下一次失败。
 */
void SetThreadLastError(
    std::string_view code,
    std::string_view message,
    std::string_view detail) noexcept;

/**
 * @brief 以稳定 JSON 返回当前线程最近一次 SPI 失败。
 * @return 在当前线程下次调用 SetThreadLastError 前始终有效的视图。
 */
[[nodiscard]] std::string_view GetThreadLastErrorJson() noexcept;

}  // namespace slicesoft::module
