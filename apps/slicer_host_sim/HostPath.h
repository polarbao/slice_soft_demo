#pragma once

#include <wchar.h>

/**
 * @brief 递归创建 Windows 绝对目录路径。
 * @param path 待创建的目录路径。
 * @return 调用后目录存在时返回非零值。
 */
int HostEnsureDirectoryTree(const wchar_t* path);

/**
 * @brief 将 UTF-16 Windows 路径转换为规范化的 UTF-8 路径。
 * @param value UTF-16 输入值。
 * @return 由调用方持有的堆分配 UTF-8 字符串；失败时返回 NULL。
 */
char* HostUtf8FromWide(const wchar_t* value);
