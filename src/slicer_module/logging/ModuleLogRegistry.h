#pragma once

#include "contracts/slicer_logging.h"

#include <cstdint>
#include <string_view>

namespace slicesoft::module::logging
{
// Best effort: optional diagnostics allocation never fails module creation.
void RegisterModule(pm_module_t* module, std::uint64_t id) noexcept;
int SetCallback(pm_module_t* module, slicer_log_callback_v1 callback,
    void* context, int minimumLevel) noexcept;
int ClearCallback(pm_module_t* module, int timeoutMs) noexcept;
bool IsCallbackThread(pm_module_t* module) noexcept;
bool IsJobCallbackThread(pm_job_t* job) noexcept;
bool HasCallback(pm_module_t* module) noexcept;
// Called before retiring handles; prevents a concurrent setter from reviving a slot.
void ShutdownModule(pm_module_t* module) noexcept;
void Emit(pm_module_t* module, int level, std::string_view action,
    std::string_view phase, std::string_view code, std::string_view message,
    std::string_view jobId = {}) noexcept;
void EmitForwarded(pm_module_t* module, std::uint32_t sourcePid,
    std::string_view json, std::string_view jobId) noexcept;
}
