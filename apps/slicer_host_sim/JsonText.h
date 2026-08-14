#pragma once

#include <stddef.h>

/**
 * @brief 分配按 printf 格式化的 UTF-8 字符串。
 * @param format 兼容 printf 的格式字符串。
 * @return 由调用方持有的堆分配字符串；失败时返回 NULL。
 */
char* HostFormat(const char* format, ...);

/**
 * @brief 转义 UTF-8 字符串，使其可用作 JSON 字符串值。
 * @param value 未转义的 UTF-8 字节序列。
 * @return 不含外围引号的堆分配字符串；失败时返回 NULL。
 */
char* HostJsonEscape(const char* value);

/**
 * @brief 从可信模块响应中读取指定名称的 JSON 字符串。
 * @param json JSON 对象文本。
 * @param key 对象成员名称。
 * @param output 接收由调用方持有的堆分配字符串。
 * @return 字段存在且有效时返回非零值。
 */
int HostJsonReadString(const char* json, const char* key, char** output);

/**
 * @brief 读取指定名称的 JSON 布尔值。
 * @param json JSON 对象文本。
 * @param key 对象成员名称。
 * @param output 接收零或一。
 * @return 字段存在且有效时返回非零值。
 */
int HostJsonReadBoolean(const char* json, const char* key, int* output);

/**
 * @brief 读取指定名称的非负 JSON 整数。
 * @param json JSON 对象文本。
 * @param key 对象成员名称。
 * @param output 接收整数值。
 * @return 字段存在且有效时返回非零值。
 */
int HostJsonReadUnsigned(
    const char* json,
    const char* key,
    unsigned long long* output);

/**
 * @brief 复制指定名称的 JSON 对象及其花括号。
 * @param json JSON 对象文本。
 * @param key 对象成员名称。
 * @param output 接收由调用方持有的堆分配字符串。
 * @return 字段存在且结构闭合时返回非零值。
 */
int HostJsonReadObject(const char* json, const char* key, char** output);

/**
 * @brief 读取指定名称且恰含三个数值的 JSON 数组。
 * @param json JSON 对象文本。
 * @param key 对象成员名称。
 * @param output 接收三个双精度值。
 * @return 数组恰含三个有限数值时返回非零值。
 */
int HostJsonReadNumber3(const char* json, const char* key, double output[3]);
