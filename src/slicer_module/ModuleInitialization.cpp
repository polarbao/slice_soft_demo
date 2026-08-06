#include "slicer_module/ModuleInitialization.h"

namespace slicesoft::module
{
namespace
{

bool InitializeProcessInfrastructure() noexcept
{
    return true;
}

ModuleInitialization& GetProcessInitialization() noexcept
{
    static ModuleInitialization initialization{InitializeProcessInfrastructure};
    return initialization;
}

}  // namespace

ModuleInitialization::ModuleInitialization(
    const ModuleInitializationAction action) noexcept
    : m_action{action}
{
}

bool ModuleInitialization::EnsureInitialized() noexcept
{
    if (m_state.load(std::memory_order_acquire)
        == ModuleInitializationState::Failed)
    {
        return false;
    }

    try
    {
        std::call_once(m_once, [this]() noexcept
        {
            InitializeOnce();
        });
    }
    catch (...)
    {
        m_state.store(ModuleInitializationState::Failed, std::memory_order_release);
        return false;
    }

    return m_state.load(std::memory_order_acquire)
        == ModuleInitializationState::Initialized;
}

ModuleInitializationState ModuleInitialization::GetState() const noexcept
{
    return m_state.load(std::memory_order_acquire);
}

std::size_t ModuleInitialization::GetInvocationCount() const noexcept
{
    return m_invocationCount.load(std::memory_order_acquire);
}

void ModuleInitialization::InitializeOnce() noexcept
{
    m_invocationCount.fetch_add(1U, std::memory_order_relaxed);
    const bool initialized = m_action != nullptr && m_action();
    m_state.store(
        initialized
            ? ModuleInitializationState::Initialized
            : ModuleInitializationState::Failed,
        std::memory_order_release);
}

bool EnsureProcessModuleInitialized() noexcept
{
    return GetProcessInitialization().EnsureInitialized();
}

}  // namespace slicesoft::module
