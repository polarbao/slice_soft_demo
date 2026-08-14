#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core::api
{

/**
 * @brief 计算 Worker 请求使用的规范有效 Profile 标识。
 * @param profile 可包含或不包含自声明 profileHash 字段的 Profile JSON。
 * @return `sha256:` 后接小写的规范文档摘要。
 * @throws std::invalid_argument profile 不是 JSON 对象时抛出。
 */
[[nodiscard]] std::string ComputeProfileDocumentHash(
    const Json& profile);

}  // namespace slicer_core::api
