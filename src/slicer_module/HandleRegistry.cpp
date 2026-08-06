#include "HandleRegistry.h"

#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

struct pm_module_s
{
    std::uint64_t identity{0U};
};

struct pm_job_s
{
    std::uint64_t identity{0U};
};

namespace slicesoft::module
{
namespace
{

bool IsTerminal(const JobLifecycleState state) noexcept
{
    return state == JobLifecycleState::Succeeded
        || state == JobLifecycleState::Failed
        || state == JobLifecycleState::Cancelled;
}

bool CanTransition(
    const JobLifecycleState current,
    const JobLifecycleState requested) noexcept
{
    if (current == requested)
    {
        return true;
    }
    if (IsTerminal(current))
    {
        return false;
    }
    if (current == JobLifecycleState::Cancelling)
    {
        return requested == JobLifecycleState::Cancelled;
    }
    if (current == JobLifecycleState::Queued)
    {
        return requested == JobLifecycleState::Running
            || requested == JobLifecycleState::Cancelling
            || IsTerminal(requested);
    }
    return requested == JobLifecycleState::Cancelling
        || IsTerminal(requested);
}

}  // namespace

ModuleHandleState::ModuleHandleState(const std::uint64_t id) noexcept
    : m_id(id)
{
}

std::uint64_t ModuleHandleState::Id() const noexcept
{
    return m_id;
}

bool ModuleHandleState::IsActive() const noexcept
{
    return m_active.load(std::memory_order_acquire);
}

void ModuleHandleState::Deactivate() noexcept
{
    m_active.store(false, std::memory_order_release);
}

JobHandleState::JobHandleState(
    const std::uint64_t id,
    const std::uint64_t ownerModuleId) noexcept
    : m_id(id),
      m_ownerModuleId(ownerModuleId)
{
}

std::uint64_t JobHandleState::Id() const noexcept
{
    return m_id;
}

std::uint64_t JobHandleState::OwnerModuleId() const noexcept
{
    return m_ownerModuleId;
}

bool JobHandleState::IsActive() const noexcept
{
    return m_active.load(std::memory_order_acquire);
}

JobLifecycleState JobHandleState::LifecycleState() const noexcept
{
    return m_lifecycleState.load(std::memory_order_acquire);
}

bool JobHandleState::IsCancellationRequested() const noexcept
{
    return m_cancellationRequested.load(std::memory_order_acquire);
}

void JobHandleState::Deactivate() noexcept
{
    m_active.store(false, std::memory_order_release);
}

void JobHandleState::ForceCancelled() noexcept
{
    if (!IsTerminal(LifecycleState()))
    {
        m_cancellationRequested.store(true, std::memory_order_release);
        m_lifecycleState.store(
            JobLifecycleState::Cancelled,
            std::memory_order_release);
    }
}

bool JobHandleState::RequestCancel() noexcept
{
    if (!IsActive())
    {
        return false;
    }
    JobLifecycleState current = LifecycleState();
    if (IsTerminal(current))
    {
        return true;
    }
    m_cancellationRequested.store(true, std::memory_order_release);
    while (!IsTerminal(current) && current != JobLifecycleState::Cancelling)
    {
        if (m_lifecycleState.compare_exchange_weak(
                current,
                JobLifecycleState::Cancelling,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            break;
        }
    }
    return true;
}

bool JobHandleState::SetLifecycleState(
    const JobLifecycleState state) noexcept
{
    if (!IsActive())
    {
        return false;
    }
    JobLifecycleState current = LifecycleState();
    while (CanTransition(current, state))
    {
        if (m_lifecycleState.compare_exchange_weak(
                current,
                state,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            return true;
        }
    }
    return false;
}

class HandleRegistry::Implementation final
{
public:
    mutable std::mutex m_mutex;
    std::uint64_t m_nextModuleId{1U};
    std::uint64_t m_nextJobId{1U};
    std::deque<pm_module_s> m_moduleTokens;
    std::deque<pm_job_s> m_jobTokens;
    std::unordered_map<pm_module_t*, std::shared_ptr<ModuleHandleState>>
        m_modules;
    std::unordered_map<pm_job_t*, std::shared_ptr<JobHandleState>> m_jobs;
    std::unordered_map<pm_module_t*, std::unordered_set<pm_job_t*>>
        m_moduleJobs;
    std::unordered_map<pm_job_t*, pm_module_t*> m_jobOwners;
};

HandleRegistry::HandleRegistry()
    : m_implementation(std::make_unique<Implementation>())
{
}

HandleRegistry::~HandleRegistry()
{
    try
    {
        std::scoped_lock lock{m_implementation->m_mutex};
        for (auto& [handle, state] : m_implementation->m_jobs)
        {
            (void)handle;
            state->ForceCancelled();
            state->Deactivate();
        }
        for (auto& [handle, state] : m_implementation->m_modules)
        {
            (void)handle;
            state->Deactivate();
        }
    }
    catch (...)
    {
    }
}

HandleRegistry& HandleRegistry::Instance()
{
    static HandleRegistry registry;
    return registry;
}

pm_module_t* HandleRegistry::CreateModule()
{
    std::scoped_lock lock{m_implementation->m_mutex};
    const std::uint64_t id = m_implementation->m_nextModuleId++;
    m_implementation->m_moduleTokens.push_back({id});
    pm_module_t* handle = &m_implementation->m_moduleTokens.back();
    const auto state = std::make_shared<ModuleHandleState>(id);
    m_implementation->m_moduleJobs.emplace(
        handle,
        std::unordered_set<pm_job_t*>{});
    try
    {
        m_implementation->m_modules.emplace(handle, state);
    }
    catch (...)
    {
        m_implementation->m_moduleJobs.erase(handle);
        throw;
    }
    return handle;
}

bool HandleRegistry::DestroyModule(pm_module_t* const module)
{
    if (module == nullptr)
    {
        return true;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto moduleEntry = m_implementation->m_modules.find(module);
    if (moduleEntry == m_implementation->m_modules.end())
    {
        return false;
    }

    const auto ownedJobs = m_implementation->m_moduleJobs.find(module);
    if (ownedJobs != m_implementation->m_moduleJobs.end())
    {
        for (pm_job_t* const job : ownedJobs->second)
        {
            const auto jobEntry = m_implementation->m_jobs.find(job);
            if (jobEntry != m_implementation->m_jobs.end())
            {
                jobEntry->second->ForceCancelled();
                jobEntry->second->Deactivate();
                m_implementation->m_jobs.erase(jobEntry);
            }
            m_implementation->m_jobOwners.erase(job);
        }
        m_implementation->m_moduleJobs.erase(ownedJobs);
    }
    moduleEntry->second->Deactivate();
    m_implementation->m_modules.erase(moduleEntry);
    return true;
}

pm_job_t* HandleRegistry::CreateJob(pm_module_t* const module)
{
    if (module == nullptr)
    {
        return nullptr;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto moduleEntry = m_implementation->m_modules.find(module);
    if (moduleEntry == m_implementation->m_modules.end())
    {
        return nullptr;
    }

    const std::uint64_t id = m_implementation->m_nextJobId++;
    m_implementation->m_jobTokens.push_back({id});
    pm_job_t* handle = &m_implementation->m_jobTokens.back();
    const auto state = std::make_shared<JobHandleState>(
        id,
        moduleEntry->second->Id());
    auto& ownedJobs = m_implementation->m_moduleJobs.at(module);
    ownedJobs.insert(handle);
    try
    {
        m_implementation->m_jobOwners.emplace(handle, module);
        try
        {
            m_implementation->m_jobs.emplace(handle, state);
        }
        catch (...)
        {
            m_implementation->m_jobOwners.erase(handle);
            throw;
        }
    }
    catch (...)
    {
        ownedJobs.erase(handle);
        throw;
    }
    return handle;
}

bool HandleRegistry::ReleaseJob(pm_job_t* const job)
{
    if (job == nullptr)
    {
        return true;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto jobEntry = m_implementation->m_jobs.find(job);
    if (jobEntry == m_implementation->m_jobs.end())
    {
        return false;
    }
    jobEntry->second->ForceCancelled();
    jobEntry->second->Deactivate();

    const auto ownerEntry = m_implementation->m_jobOwners.find(job);
    if (ownerEntry != m_implementation->m_jobOwners.end())
    {
        const auto ownedJobs = m_implementation->m_moduleJobs.find(ownerEntry->second);
        if (ownedJobs != m_implementation->m_moduleJobs.end())
        {
            ownedJobs->second.erase(job);
        }
        m_implementation->m_jobOwners.erase(ownerEntry);
    }
    m_implementation->m_jobs.erase(jobEntry);
    return true;
}

std::shared_ptr<const ModuleHandleState> HandleRegistry::FindModule(
    pm_module_t* const module) const
{
    if (module == nullptr)
    {
        return nullptr;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto entry = m_implementation->m_modules.find(module);
    return entry == m_implementation->m_modules.end()
        ? nullptr
        : entry->second;
}

std::shared_ptr<const JobHandleState> HandleRegistry::FindJob(
    pm_job_t* const job) const
{
    if (job == nullptr)
    {
        return nullptr;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto entry = m_implementation->m_jobs.find(job);
    return entry == m_implementation->m_jobs.end()
        ? nullptr
        : entry->second;
}

std::shared_ptr<const JobHandleState> HandleRegistry::FindJob(
    pm_module_t* const module,
    pm_job_t* const job) const
{
    if (module == nullptr || job == nullptr)
    {
        return nullptr;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    if (m_implementation->m_modules.find(module)
        == m_implementation->m_modules.end())
    {
        return nullptr;
    }
    const auto ownerEntry = m_implementation->m_jobOwners.find(job);
    if (ownerEntry == m_implementation->m_jobOwners.end()
        || ownerEntry->second != module)
    {
        return nullptr;
    }
    const auto jobEntry = m_implementation->m_jobs.find(job);
    return jobEntry == m_implementation->m_jobs.end()
        ? nullptr
        : jobEntry->second;
}

bool HandleRegistry::RequestCancel(pm_job_t* const job)
{
    if (job == nullptr)
    {
        return false;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto entry = m_implementation->m_jobs.find(job);
    return entry != m_implementation->m_jobs.end()
        && entry->second->RequestCancel();
}

bool HandleRegistry::SetJobLifecycleState(
    pm_job_t* const job,
    const JobLifecycleState state)
{
    if (job == nullptr)
    {
        return false;
    }
    std::scoped_lock lock{m_implementation->m_mutex};
    const auto entry = m_implementation->m_jobs.find(job);
    return entry != m_implementation->m_jobs.end()
        && entry->second->SetLifecycleState(state);
}

std::size_t HandleRegistry::ActiveModuleCount() const
{
    std::scoped_lock lock{m_implementation->m_mutex};
    return m_implementation->m_modules.size();
}

std::size_t HandleRegistry::ActiveJobCount() const
{
    std::scoped_lock lock{m_implementation->m_mutex};
    return m_implementation->m_jobs.size();
}

}  // namespace slicesoft::module
