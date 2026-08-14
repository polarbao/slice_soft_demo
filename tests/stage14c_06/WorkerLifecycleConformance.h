#pragma once

#include "SpiModuleApi.h"

#include <filesystem>

namespace slicesoft::tests
{

/**
 * @brief 通过冻结的公开 C ABI 验证 Worker 生命周期行为。
 * @param api 运行时加载的模块 API。
 * @param module 持有所提交任务的有效模块句柄。
 * @param repository 用于定位测试夹具的仓库绝对根目录。
 */
void TestWorkerLifecycleConformance(
    const SpiModuleApi& api,
    pm_module_t* module,
    const std::filesystem::path& repository);

}  // namespace slicesoft::tests
