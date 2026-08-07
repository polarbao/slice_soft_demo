#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PM_MODULE_STATIC
#include "contracts/print_module_spi.h"

#include <stddef.h>

typedef int (PM_CALL *HostSpiVersionFunction)(void);
typedef int (PM_CALL *HostModuleInfoFunction)(char*, int, int*);
typedef pm_module_t* (PM_CALL *HostCreateFunction)(const char*);
typedef void (PM_CALL *HostDestroyFunction)(pm_module_t*);
typedef pm_job_t* (PM_CALL *HostSubmitFunction)(pm_module_t*, const char*);
typedef int (PM_CALL *HostPollFunction)(pm_job_t*, char*, int, int*);
typedef int (PM_CALL *HostCancelFunction)(pm_job_t*);
typedef int (PM_CALL *HostResultFunction)(pm_job_t*, char*, int, int*);
typedef void (PM_CALL *HostReleaseFunction)(pm_job_t*);
typedef int (PM_CALL *HostSelfTestFunction)(pm_module_t*, char*, int, int*);
typedef int (PM_CALL *HostLastErrorFunction)(char*, int, int*);

/** @brief Runtime-loaded view of the frozen SPI v1 export table. */
typedef struct HostModuleApi
{
    HMODULE m_library;
    HostSpiVersionFunction m_spiVersion;
    HostModuleInfoFunction m_moduleInfo;
    HostCreateFunction m_create;
    HostDestroyFunction m_destroy;
    HostSubmitFunction m_submit;
    HostPollFunction m_poll;
    HostCancelFunction m_cancel;
    HostResultFunction m_result;
    HostReleaseFunction m_release;
    HostSelfTestFunction m_selfTest;
    HostLastErrorFunction m_lastError;
} HostModuleApi;

/**
 * @brief Loads a DLL and resolves exactly the frozen public SPI symbols.
 * @param api Destination table.
 * @param libraryPath Absolute or relative DLL path.
 * @param error Buffer receiving a readable error.
 * @param errorCapacity Error buffer size.
 * @return Non-zero on success.
 */
int HostModuleApiLoad(
    HostModuleApi* api,
    const wchar_t* libraryPath,
    char* error,
    size_t errorCapacity);

/**
 * @brief Unloads a previously loaded module DLL.
 * @param api API table to clear after unloading.
 * @return This function does not return a value.
 */
void HostModuleApiUnload(HostModuleApi* api);

/**
 * @brief Reads pm_module_info using the three-state buffer contract.
 * @param api Loaded public SPI table.
 * @param output Receives a heap UTF-8 string owned by the caller.
 * @return Non-zero when the module information was read successfully.
 */
int HostModuleApiReadInfo(const HostModuleApi* api, char** output);

/**
 * @brief Reads pm_poll using the three-state buffer contract.
 * @param api Loaded public SPI table.
 * @param job Active public SPI job handle.
 * @param output Receives a heap UTF-8 string owned by the caller.
 * @return Non-zero when the job status was read successfully.
 */
int HostModuleApiReadPoll(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output);

/**
 * @brief Reads pm_result using the three-state buffer contract.
 * @param api Loaded public SPI table.
 * @param job Completed public SPI job handle.
 * @param output Receives a heap UTF-8 string owned by the caller.
 * @return Non-zero when the job result was read successfully.
 */
int HostModuleApiReadResult(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output);

/**
 * @brief Reads pm_self_test using the three-state buffer contract.
 * @param api Loaded public SPI table.
 * @param module Public SPI module instance.
 * @param output Receives a heap UTF-8 string owned by the caller.
 * @return Non-zero when the self-test response was read successfully.
 */
int HostModuleApiReadSelfTest(
    const HostModuleApi* api,
    pm_module_t* module,
    char** output);

/**
 * @brief Reads pm_last_error using the three-state buffer contract.
 * @param api Loaded public SPI table.
 * @param output Receives a heap UTF-8 string owned by the caller.
 * @return Non-zero when the last-error response was read successfully.
 */
int HostModuleApiReadLastError(const HostModuleApi* api, char** output);
