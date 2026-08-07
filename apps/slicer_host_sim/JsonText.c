#include "JsonText.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* FindValue(const char* json, const char* key)
{
    char* pattern;
    const char* found;
    const char* value;
    if (json == NULL || key == NULL)
    {
        return NULL;
    }
    pattern = HostFormat("\"%s\"", key);
    if (pattern == NULL)
    {
        return NULL;
    }
    found = strstr(json, pattern);
    free(pattern);
    if (found == NULL)
    {
        return NULL;
    }
    value = strchr(found, ':');
    if (value == NULL)
    {
        return NULL;
    }
    ++value;
    while (isspace((unsigned char)*value) != 0)
    {
        ++value;
    }
    return value;
}

char* HostFormat(const char* format, ...)
{
    va_list arguments;
    va_list copy;
    int required;
    char* output;
    if (format == NULL)
    {
        return NULL;
    }
    va_start(arguments, format);
    va_copy(copy, arguments);
    required = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (required < 0)
    {
        va_end(arguments);
        return NULL;
    }
    output = (char*)malloc((size_t)required + 1U);
    if (output == NULL)
    {
        va_end(arguments);
        return NULL;
    }
    if (vsnprintf(output, (size_t)required + 1U, format, arguments) != required)
    {
        free(output);
        output = NULL;
    }
    va_end(arguments);
    return output;
}

char* HostJsonEscape(const char* value)
{
    size_t inputLength;
    size_t capacity;
    size_t readIndex;
    size_t writeIndex = 0U;
    char* output;
    if (value == NULL)
    {
        return NULL;
    }
    inputLength = strlen(value);
    capacity = inputLength * 2U + 1U;
    output = (char*)malloc(capacity);
    if (output == NULL)
    {
        return NULL;
    }
    for (readIndex = 0U; readIndex < inputLength; ++readIndex)
    {
        const unsigned char character = (unsigned char)value[readIndex];
        const char* escape = NULL;
        switch (character)
        {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default: break;
        }
        if (escape != NULL)
        {
            output[writeIndex++] = escape[0];
            output[writeIndex++] = escape[1];
        }
        else if (character < 0x20U)
        {
            free(output);
            return NULL;
        }
        else
        {
            output[writeIndex++] = (char)character;
        }
    }
    output[writeIndex] = '\0';
    return output;
}

int HostJsonReadString(const char* json, const char* key, char** output)
{
    const char* value = FindValue(json, key);
    const char* cursor;
    size_t capacity;
    size_t length = 0U;
    char* decoded;
    if (output == NULL || value == NULL || *value != '"')
    {
        return 0;
    }
    *output = NULL;
    capacity = strlen(value) + 1U;
    decoded = (char*)malloc(capacity);
    if (decoded == NULL)
    {
        return 0;
    }
    cursor = value + 1;
    while (*cursor != '\0' && *cursor != '"')
    {
        char character = *cursor++;
        if (character == '\\')
        {
            character = *cursor++;
            switch (character)
            {
            case '"': character = '"'; break;
            case '\\': character = '\\'; break;
            case '/': character = '/'; break;
            case 'b': character = '\b'; break;
            case 'f': character = '\f'; break;
            case 'n': character = '\n'; break;
            case 'r': character = '\r'; break;
            case 't': character = '\t'; break;
            default:
                free(decoded);
                return 0;
            }
        }
        decoded[length++] = character;
    }
    if (*cursor != '"')
    {
        free(decoded);
        return 0;
    }
    decoded[length] = '\0';
    *output = decoded;
    return 1;
}

int HostJsonReadBoolean(const char* json, const char* key, int* output)
{
    const char* value = FindValue(json, key);
    if (value == NULL || output == NULL)
    {
        return 0;
    }
    if (strncmp(value, "true", 4U) == 0)
    {
        *output = 1;
        return 1;
    }
    if (strncmp(value, "false", 5U) == 0)
    {
        *output = 0;
        return 1;
    }
    return 0;
}

int HostJsonReadUnsigned(
    const char* json,
    const char* key,
    unsigned long long* output)
{
    const char* value = FindValue(json, key);
    char* end = NULL;
    unsigned long long parsed;
    if (value == NULL || output == NULL || *value == '-')
    {
        return 0;
    }
    parsed = strtoull(value, &end, 10);
    if (end == value)
    {
        return 0;
    }
    *output = parsed;
    return 1;
}

int HostJsonReadObject(const char* json, const char* key, char** output)
{
    const char* value = FindValue(json, key);
    const char* cursor;
    int depth = 0;
    int inString = 0;
    int escaped = 0;
    size_t length;
    char* copy;
    if (value == NULL || output == NULL || *value != '{')
    {
        return 0;
    }
    *output = NULL;
    for (cursor = value; *cursor != '\0'; ++cursor)
    {
        const char character = *cursor;
        if (inString != 0)
        {
            if (escaped != 0)
            {
                escaped = 0;
            }
            else if (character == '\\')
            {
                escaped = 1;
            }
            else if (character == '"')
            {
                inString = 0;
            }
            continue;
        }
        if (character == '"')
        {
            inString = 1;
        }
        else if (character == '{')
        {
            ++depth;
        }
        else if (character == '}' && --depth == 0)
        {
            length = (size_t)(cursor - value) + 1U;
            copy = (char*)malloc(length + 1U);
            if (copy == NULL)
            {
                return 0;
            }
            memcpy(copy, value, length);
            copy[length] = '\0';
            *output = copy;
            return 1;
        }
    }
    return 0;
}

int HostJsonReadNumber3(const char* json, const char* key, double output[3])
{
    const char* cursor = FindValue(json, key);
    size_t index;
    if (cursor == NULL || output == NULL || *cursor++ != '[')
    {
        return 0;
    }
    for (index = 0U; index < 3U; ++index)
    {
        char* end = NULL;
        while (isspace((unsigned char)*cursor) != 0)
        {
            ++cursor;
        }
        output[index] = strtod(cursor, &end);
        if (end == cursor || isfinite(output[index]) == 0)
        {
            return 0;
        }
        cursor = end;
        while (isspace((unsigned char)*cursor) != 0)
        {
            ++cursor;
        }
        if (index < 2U)
        {
            if (*cursor++ != ',')
            {
                return 0;
            }
        }
    }
    while (isspace((unsigned char)*cursor) != 0)
    {
        ++cursor;
    }
    return *cursor == ']';
}
