#ifndef SLICESOFT_APPS_SLICER_HOST_SIM_HOST_M1_INTAKE_H
#define SLICESOFT_APPS_SLICER_HOST_SIM_HOST_M1_INTAKE_H

#include "HostModuleApi.h"

/**
 * @brief 校验 SPI v1 与冻结的十五项模块能力清单。
 * @param api 已加载的宿主侧模块函数表。
 * @return 模块信息满足 M1 合同时返回非零值。
 */
int HostM1IntakeCheckModuleInfo(const HostModuleApi* api);

/**
 * @brief 执行 M1 自检与未知能力的失败即拒绝探测。
 * @param api 已加载的宿主侧模块函数表。
 * @param module 通过 pm_create 创建的有效模块实例。
 * @param selfTest 接收已分配的 UTF-8 自检响应。
 * @return 两项 M1 检查均通过时返回非零值。
 */
int HostM1IntakeRun(
    const HostModuleApi* api,
    pm_module_t* module,
    char** selfTest);

#endif
