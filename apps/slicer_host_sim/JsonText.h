#pragma once

#include <stddef.h>

/**
 * @brief Allocates a printf-formatted UTF-8 string.
 * @param format printf-compatible format string.
 * @return Heap string owned by the caller, or NULL on failure.
 */
char* HostFormat(const char* format, ...);

/**
 * @brief Escapes one UTF-8 string for use as a JSON string value.
 * @param value Unescaped UTF-8 bytes.
 * @return Heap string without surrounding quotes, or NULL on failure.
 */
char* HostJsonEscape(const char* value);

/**
 * @brief Reads a named JSON string from a trusted module response.
 * @param json JSON object text.
 * @param key Object member name.
 * @param output Receives a heap string owned by the caller.
 * @return Non-zero when the field is present and valid.
 */
int HostJsonReadString(const char* json, const char* key, char** output);

/**
 * @brief Reads a named JSON boolean.
 * @param json JSON object text.
 * @param key Object member name.
 * @param output Receives zero or one.
 * @return Non-zero when the field is present and valid.
 */
int HostJsonReadBoolean(const char* json, const char* key, int* output);

/**
 * @brief Reads a named non-negative JSON integer.
 * @param json JSON object text.
 * @param key Object member name.
 * @param output Receives the integer value.
 * @return Non-zero when the field is present and valid.
 */
int HostJsonReadUnsigned(
    const char* json,
    const char* key,
    unsigned long long* output);

/**
 * @brief Copies a named JSON object including its braces.
 * @param json JSON object text.
 * @param key Object member name.
 * @param output Receives a heap string owned by the caller.
 * @return Non-zero when the field is present and structurally closed.
 */
int HostJsonReadObject(const char* json, const char* key, char** output);

/**
 * @brief Reads a named three-number JSON array.
 * @param json JSON object text.
 * @param key Object member name.
 * @param output Receives three doubles.
 * @return Non-zero when exactly three finite numbers are present.
 */
int HostJsonReadNumber3(const char* json, const char* key, double output[3]);
