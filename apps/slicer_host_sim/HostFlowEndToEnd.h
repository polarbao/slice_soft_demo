#pragma once

#include "HostModuleApi.h"

/**
 * @brief 执行 HOSTFLOW H-A-03 空场景生产闭环。
 * @param api 已加载的公共 SPI 函数表。
 * @param module 有效的模块实例。
 * @param repository 包含参考模型的 UTF-8 仓库根目录。
 * @param outputRoot 由宿主管理的 UTF-8 证据目录。
 * @return 导入、场景编辑、切片与验证全部成功时返回非零值。
 */
int HostFlowRunEndToEnd(
    const HostModuleApi* api,
    pm_module_t* module,
    const char* repository,
    const char* outputRoot);
