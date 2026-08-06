#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief Returns the deterministic module-local self-test report.
 * @return Process-lifetime UTF-8 JSON with no persistent side effects.
 * @note Worker lifecycle checks are explicitly deferred to the Worker gate.
 */
[[nodiscard]] std::string_view GetModuleSelfTestJson() noexcept;

}  // namespace slicesoft::module
