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

/** @brief 运行时加载的冻结 SPI v1 导出表视图。 */
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
 * @brief 加载 DLL，并严格解析冻结的公共 SPI 符号。
 * @param api 目标函数表。
 * @param libraryPath DLL 的绝对或相对路径。
 * @param error 接收可读错误信息的缓冲区。
 * @param errorCapacity 错误缓冲区容量。
 * @return 成功时返回非零值。
 */
int HostModuleApiLoad(
    HostModuleApi* api,
    const wchar_t* libraryPath,
    char* error,
    size_t errorCapacity);

/**
 * @brief 卸载此前加载的模块 DLL。
 * @param api 卸载后需要清空的 API 表。
 * @return 本函数无返回值。
 */
void HostModuleApiUnload(HostModuleApi* api);

/**
 * @brief 按三态缓冲区合同读取 pm_module_info。
 * @param api 已加载的公共 SPI 表。
 * @param output 接收由调用方持有的堆分配 UTF-8 字符串。
 * @return 成功读取模块信息时返回非零值。
 */
int HostModuleApiReadInfo(const HostModuleApi* api, char** output);

/**
 * @brief 按三态缓冲区合同读取 pm_poll。
 * @param api 已加载的公共 SPI 表。
 * @param job 活动中的公共 SPI 作业句柄。
 * @param output 接收由调用方持有的堆分配 UTF-8 字符串。
 * @return 成功读取作业状态时返回非零值。
 */
int HostModuleApiReadPoll(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output);

/**
 * @brief 按三态缓冲区合同读取 pm_result。
 * @param api 已加载的公共 SPI 表。
 * @param job 已完成的公共 SPI 作业句柄。
 * @param output 接收由调用方持有的堆分配 UTF-8 字符串。
 * @return 成功读取作业结果时返回非零值。
 */
int HostModuleApiReadResult(
    const HostModuleApi* api,
    pm_job_t* job,
    char** output);

/**
 * @brief 按三态缓冲区合同读取 pm_self_test。
 * @param api 已加载的公共 SPI 表。
 * @param module 公共 SPI 模块实例。
 * @param output 接收由调用方持有的堆分配 UTF-8 字符串。
 * @return 成功读取自检响应时返回非零值。
 */
int HostModuleApiReadSelfTest(
    const HostModuleApi* api,
    pm_module_t* module,
    char** output);

/**
 * @brief 按三态缓冲区合同读取 pm_last_error。
 * @param api 已加载的公共 SPI 表。
 * @param output 接收由调用方持有的堆分配 UTF-8 字符串。
 * @return 成功读取最近错误响应时返回非零值。
 */
int HostModuleApiReadLastError(const HostModuleApi* api, char** output);
