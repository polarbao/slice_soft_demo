#include "HostModuleApi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*HostBufferCall)(void*, char*, int, int*);

typedef struct HostJobCall
{
    const HostModuleApi* api;
    pm_job_t* job;
} HostJobCall;

typedef struct HostModuleCall
{
    const HostModuleApi* api;
    pm_module_t* module;
} HostModuleCall;

static void SetError(char* error, size_t capacity, const char* message)
{
    if (error != NULL && capacity > 0U)
    {
        (void)snprintf(error, capacity, "%s", message);
    }
}

static int ReadBuffer(HostBufferCall call, void* context, char** output)
{
    int required = 0;
    int attempt;
    if (call == NULL || output == NULL)
    {
        return 0;
    }
    *output = NULL;
    if (call(context, NULL, 0, &required) != PM_ERR_BUFFER_SMALL
        || required <= 0)
    {
        return 0;
    }
    for (attempt = 0; attempt < 4; ++attempt)
    {
        char* buffer = (char*)calloc((size_t)required + 1U, 1U);
        int written;
        if (buffer == NULL)
        {
            return 0;
        }
        written = call(context, buffer, required + 1, &required);
        if (written >= 0)
        {
            if (buffer[written] != '\0')
            {
                free(buffer);
                return 0;
            }
            *output = buffer;
            return 1;
        }
        free(buffer);
        if (written != PM_ERR_BUFFER_SMALL || required <= 0)
        {
            return 0;
        }
    }
    return 0;
}

static int ReadInfo(void* context, char* output, int capacity, int* required)
{
    const HostModuleApi* api = (const HostModuleApi*)context;
    return api->m_moduleInfo(output, capacity, required);
}

static int ReadPoll(void* context, char* output, int capacity, int* required)
{
    const HostJobCall* call = (const HostJobCall*)context;
    return call->api->m_poll(call->job, output, capacity, required);
}

static int ReadResult(void* context, char* output, int capacity, int* required)
{
    const HostJobCall* call = (const HostJobCall*)context;
    return call->api->m_result(call->job, output, capacity, required);
}

static int ReadSelfTest(void* context, char* output, int capacity, int* required)
{
    const HostModuleCall* call = (const HostModuleCall*)context;
    return call->api->m_selfTest(
        call->module,
        output,
        capacity,
        required);
}

static int ReadLastError(void* context, char* output, int capacity, int* required)
{
    const HostModuleApi* api = (const HostModuleApi*)context;
    return api->m_lastError(output, capacity, required);
}

#define HOST_LOAD_SYMBOL(api, member, name, error, capacity)                 \
    do                                                                       \
    {                                                                        \
        FARPROC address = GetProcAddress((api)->m_library, (name));          \
        if (address == NULL)                                                  \
        {                                                                    \
            SetError((error), (capacity), "missing SPI export: " name);     \
            HostModuleApiUnload((api));                                      \
            return 0;                                                        \
        }                                                                    \
        memcpy(&(api)->member, &address, sizeof((api)->member));             \
    } while (0)

int HostModuleApiLoad(
    HostModuleApi* api,
    const wchar_t* libraryPath,
    char* error,
    size_t errorCapacity)
{
    if (api == NULL || libraryPath == NULL)
    {
        SetError(error, errorCapacity, "module path is required");
        return 0;
    }
    memset(api, 0, sizeof(*api));
    api->m_library = LoadLibraryW(libraryPath);
    if (api->m_library == NULL)
    {
        SetError(error, errorCapacity, "could not load slicer module DLL");
        return 0;
    }
    HOST_LOAD_SYMBOL(api, m_spiVersion, "pm_spi_version", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_moduleInfo, "pm_module_info", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_create, "pm_create", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_destroy, "pm_destroy", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_submit, "pm_submit", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_poll, "pm_poll", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_cancel, "pm_cancel", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_result, "pm_result", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_release, "pm_release", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_selfTest, "pm_self_test", error, errorCapacity);
    HOST_LOAD_SYMBOL(api, m_lastError, "pm_last_error", error, errorCapacity);
    return 1;
}

void HostModuleApiUnload(HostModuleApi* api)
{
    if (api != NULL && api->m_library != NULL)
    {
        FreeLibrary(api->m_library);
        memset(api, 0, sizeof(*api));
    }
}

int HostModuleApiReadInfo(const HostModuleApi* api, char** output)
{
    return ReadBuffer(ReadInfo, (void*)api, output);
}

int HostModuleApiReadPoll(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output)
{
    HostJobCall call = {api, job};
    return ReadBuffer(ReadPoll, &call, output);
}

int HostModuleApiReadResult(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output)
{
    HostJobCall call = {api, job};
    return ReadBuffer(ReadResult, &call, output);
}

int HostModuleApiReadSelfTest(
    const HostModuleApi* api,
    pm_module_t* module,
    char** output)
{
    HostModuleCall call = {api, module};
    return ReadBuffer(ReadSelfTest, &call, output);
}

int HostModuleApiReadLastError(const HostModuleApi* api, char** output)
{
    return ReadBuffer(ReadLastError, (void*)api, output);
}
