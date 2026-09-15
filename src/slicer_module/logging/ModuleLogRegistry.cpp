#include "ModuleLogRegistry.h"

#include "ModuleLogDispatcher.h"
#include "slicer_module/HandleRegistry.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>
#include <limits>

namespace slicesoft::module::logging
{
namespace
{
struct Slot
{
    std::uint64_t id;
    std::shared_ptr<ModuleLogDispatcher> dispatcher;
    bool closing{false};
};
struct Registry
{
    std::mutex mutex;
    std::unordered_map<pm_module_t*, Slot> slots;
};
Registry& State() { static Registry registry; return registry; }

std::shared_ptr<ModuleLogDispatcher> Find(pm_module_t* const module)
{
    auto& state = State();
    std::lock_guard lock{state.mutex};
    const auto slot = state.slots.find(module);
    return slot == state.slots.end() ? nullptr : slot->second.dispatcher;
}
}

void RegisterModule(pm_module_t* const module, const std::uint64_t id) noexcept
{
    try
    {
        auto& state = State();
        std::lock_guard lock{state.mutex};
        state.slots.emplace(module, Slot{id, std::make_shared<ModuleLogDispatcher>(id)});
    }
    catch (...) {}
}

int SetCallback(pm_module_t* const module, const slicer_log_callback_v1 callback,
    void* const context, const int minimumLevel) noexcept
{
    if (callback == nullptr || minimumLevel < -1 || minimumLevel > 5)
        return SLICER_LOG_INVALID_ARGUMENT;
    try
    {
        if (!HandleRegistry::Instance().FindModule(module)) return SLICER_LOG_INVALID_ARGUMENT;
        auto& state = State();
        std::lock_guard lock{state.mutex};
        const auto slot = state.slots.find(module);
        if (slot == state.slots.end() || slot->second.closing) return SLICER_LOG_INVALID_STATE;
        return slot->second.dispatcher->Set(callback, context, minimumLevel);
    }
    catch (...) { return SLICER_LOG_RESOURCE_ERROR; }
}

int ClearCallback(pm_module_t* const module, const int timeoutMs) noexcept
{
    if (timeoutMs < 0) return SLICER_LOG_INVALID_ARGUMENT;
    try
    {
        if (!HandleRegistry::Instance().FindModule(module)) return SLICER_LOG_INVALID_ARGUMENT;
        const auto slot = Find(module);
        return slot ? slot->Clear(timeoutMs) : SLICER_LOG_INVALID_STATE;
    }
    catch (...) { return SLICER_LOG_RESOURCE_ERROR; }
}

bool IsCallbackThread(pm_module_t* const module) noexcept
{
    try
    {
        const auto owner = HandleRegistry::Instance().FindModule(module);
        return owner && ModuleLogDispatcher::IsModuleCallbackThread(owner->Id());
    }
    catch (...) { return false; }
}

bool HasCallback(pm_module_t* const module) noexcept
{
    try { const auto slot = Find(module); return slot && slot->Enabled(); }
    catch (...) { return false; }
}

bool IsJobCallbackThread(pm_job_t* const job) noexcept
{
    try
    {
        const auto jobState = HandleRegistry::Instance().FindJob(job);
        if (!jobState) return false;
        return ModuleLogDispatcher::IsModuleCallbackThread(jobState->OwnerModuleId());
    }
    catch (...) {}
    return false;
}

void ShutdownModule(pm_module_t* const module) noexcept
{
    try
    {
        std::shared_ptr<ModuleLogDispatcher> slot;
        {
            auto& state = State();
            std::lock_guard lock{state.mutex};
            const auto entry = state.slots.find(module);
            if (entry == state.slots.end()) return;
            slot = entry->second.dispatcher;
            if (slot->IsCallbackThread()) return;
            entry->second.closing = true;
        }
        (void)slot->Clear(-1);
        auto& state = State();
        std::lock_guard lock{state.mutex};
        state.slots.erase(module);
    }
    catch (...) {}
}

void Emit(pm_module_t* const module, const int level, const std::string_view action,
    const std::string_view phase, const std::string_view code,
    const std::string_view message, const std::string_view jobId) noexcept
{
    try
    {
        const auto slot = Find(module);
        if (!slot || !slot->Enabled()) return;
        diagnostics::LogEvent event;
        event.level = level;
        event.module = "slicer_module";
        event.action = action.substr(0, 256);
        event.phase = phase.substr(0, 256);
        event.code = code.substr(0, 256);
        event.message = message.substr(0, 8192);
        event.jobId = jobId.substr(0, 256);
        slot->Emit(std::move(event));
    }
    catch (...) {}
}

void EmitForwarded(pm_module_t* const module, const std::uint32_t sourcePid,
    const std::string_view json, const std::string_view jobId) noexcept
{
    try
    {
        const auto slot = Find(module);
        if (!slot || sourcePid == 0 || json.size() > diagnostics::MaximumEventBytes) return;
        const auto value = nlohmann::json::parse(json, nullptr, false);
        if (!value.is_object() || !value.contains("sourcePid") || !value["sourcePid"].is_number_unsigned()
            || value["sourcePid"].get<std::uint64_t>() != sourcePid
            || value.value("sourceProcessRole", "") != "worker") return;
        const auto level = value.value("level", -1);
        if (!diagnostics::ValidEvent(json, level) || !value.contains("sourceTid")
            || !value["sourceTid"].is_number_unsigned()
            || value["sourceTid"].get<std::uint64_t>() == 0
            || value["sourceTid"].get<std::uint64_t>() > (std::numeric_limits<std::uint32_t>::max)()) return;
        if (!value.contains("timestampUtcMs") || !value["timestampUtcMs"].is_number_integer()
            || (value["timestampUtcMs"].is_number_unsigned()
                && value["timestampUtcMs"].get<std::uint64_t>() > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
            || value["timestampUtcMs"].get<std::int64_t>() <= 0) return;
        diagnostics::LogEvent event;
        event.level = level;
        event.module = "slicer_worker";
        event.action = value.value("action", "request");
        event.phase = value.value("phase", "");
        event.code = value.value("code", "");
        event.message = value.value("message", "");
        event.jobId = jobId;
        event.sourceProcessRole = "worker";
        event.sourcePid = sourcePid;
        event.sourceTid = value["sourceTid"].get<std::uint32_t>();
        event.timestampUtcMs = value.value("timestampUtcMs", std::int64_t{0});
        slot->Emit(std::move(event));
    }
    catch (...) {}
}
}
