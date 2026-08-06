#pragma once

#include "SpiModuleApi.h"

#include <filesystem>

namespace slicesoft::tests
{

/**
 * @brief Verifies Worker lifecycle behavior through the frozen public C ABI.
 * @param api Runtime-loaded module API.
 * @param module Live module handle that owns the submitted jobs.
 * @param repository Absolute repository root used to locate the test fixture.
 */
void TestWorkerLifecycleConformance(
    const SpiModuleApi& api,
    pm_module_t* module,
    const std::filesystem::path& repository);

}  // namespace slicesoft::tests
