#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief 返回确定性的模块本地自检报告。
 * @return 有效期与进程相同、且无持久化副作用的 UTF-8 JSON。
 * @note Worker 生命周期检查明确推迟到 Worker Gate。
 */
[[nodiscard]] std::string_view GetModuleSelfTestJson() noexcept;

}  // namespace slicesoft::module
