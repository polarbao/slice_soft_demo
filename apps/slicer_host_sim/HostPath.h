#pragma once

#include <wchar.h>

/**
 * @brief Creates an absolute Windows directory path recursively.
 * @param path Directory path to create.
 * @return Non-zero when the directory exists after the call.
 */
int HostEnsureDirectoryTree(const wchar_t* path);

/**
 * @brief Converts a UTF-16 Windows path to normalized UTF-8.
 * @param value UTF-16 input value.
 * @return Heap UTF-8 string owned by the caller, or NULL on failure.
 */
char* HostUtf8FromWide(const wchar_t* value);
