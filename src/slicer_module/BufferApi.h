#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief 按打印模块 SPI 三态缓冲区协议写入 UTF-8 内容。
 * @param content 待写内容，不隐含尾部 NUL 字节。
 * @param output 调用方持有的输出缓冲区；nullptr 表示仅探测大小。
 * @param capacity 输出缓冲区字节容量；零同样表示仅探测大小。
 * @param outRequired 可选的所需字节数输出指针，不含 NUL。
 * @return 成功时返回写入字节数，失败时返回 PM_ERR_* 值。
 * @note 小于 content.size() + 1 的缓冲区绝不会被修改。
 */
[[nodiscard]] int WriteOut(
    std::string_view content,
    char* output,
    int capacity,
    int* outRequired) noexcept;

}  // namespace slicesoft::module
