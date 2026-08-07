#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "HostPath.h"

#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>

int HostEnsureDirectoryTree(const wchar_t* path)
{
    wchar_t* mutablePath;
    size_t index;
    size_t length;
    int success = 1;
    if (path == NULL || path[0] == L'\0')
    {
        return 0;
    }
    length = wcslen(path);
    mutablePath = (wchar_t*)malloc((length + 1U) * sizeof(wchar_t));
    if (mutablePath == NULL)
    {
        return 0;
    }
    (void)wcscpy_s(mutablePath, length + 1U, path);
    for (index = 0U; index < length; ++index)
    {
        if (mutablePath[index] == L'/')
        {
            mutablePath[index] = L'\\';
        }
    }
    for (index = 3U; index <= length; ++index)
    {
        const wchar_t saved = mutablePath[index];
        if (saved != L'\\' && saved != L'\0')
        {
            continue;
        }
        mutablePath[index] = L'\0';
        if (!CreateDirectoryW(mutablePath, NULL)
            && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            success = 0;
            mutablePath[index] = saved;
            break;
        }
        mutablePath[index] = saved;
    }
    free(mutablePath);
    return success;
}

char* HostUtf8FromWide(const wchar_t* value)
{
    int required;
    char* output;
    int index;
    if (value == NULL)
    {
        return NULL;
    }
    required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        NULL,
        0,
        NULL,
        NULL);
    if (required <= 0)
    {
        return NULL;
    }
    output = (char*)malloc((size_t)required);
    if (output == NULL
        || WideCharToMultiByte(
               CP_UTF8,
               WC_ERR_INVALID_CHARS,
               value,
               -1,
               output,
               required,
               NULL,
               NULL) <= 0)
    {
        free(output);
        return NULL;
    }
    for (index = 0; output[index] != '\0'; ++index)
    {
        if (output[index] == '\\')
        {
            output[index] = '/';
        }
    }
    return output;
}
