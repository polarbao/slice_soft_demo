#pragma once

#include "contracts/print_module_spi.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace slicesoft::module
{

/**
 * @brief Minimal lifecycle states shared by the module ABI shell.
 */
enum class JobLifecycleState
{
    Queued,
    Running,
    Cancelling,
    Succeeded,
    Failed,
    Cancelled
};

/**
 * @brief Validated state associated with one opaque module handle.
 */
class ModuleHandleState final
{
public:
    /**
     * @brief Creates state for a registry-assigned module identity.
     * @param id Registry-local module identity.
     */
    explicit ModuleHandleState(std::uint64_t id) noexcept;

    /**
     * @brief Returns the registry-local module identity.
     * @return Monotonically increasing module identity.
     */
    [[nodiscard]] std::uint64_t Id() const noexcept;

    /**
     * @brief Reports whether the handle remains registered.
     * @return True while the module handle is live.
     */
    [[nodiscard]] bool IsActive() const noexcept;

private:
    void Deactivate() noexcept;

    std::uint64_t m_id{0U};
    std::atomic_bool m_active{true};

    friend class HandleRegistry;
};

/**
 * @brief Validated state associated with one opaque job handle.
 */
class JobHandleState final
{
public:
    /**
     * @brief Creates state for a registry-assigned job and owner identity.
     * @param id Registry-local job identity.
     * @param ownerModuleId Registry-local owner module identity.
     */
    JobHandleState(std::uint64_t id, std::uint64_t ownerModuleId) noexcept;

    /**
     * @brief Returns the registry-local job identity.
     * @return Monotonically increasing job identity.
     */
    [[nodiscard]] std::uint64_t Id() const noexcept;

    /**
     * @brief Returns the identity of the owning module.
     * @return Registry-local module identity.
     */
    [[nodiscard]] std::uint64_t OwnerModuleId() const noexcept;

    /**
     * @brief Reports whether the job handle remains registered.
     * @return True while the job handle is live.
     */
    [[nodiscard]] bool IsActive() const noexcept;

    /**
     * @brief Returns the current minimal job lifecycle state.
     * @return Current job lifecycle state.
     */
    [[nodiscard]] JobLifecycleState LifecycleState() const noexcept;

    /**
     * @brief Reports whether cooperative cancellation was requested.
     * @return True after the first accepted cancellation request.
     */
    [[nodiscard]] bool IsCancellationRequested() const noexcept;

private:
    void Deactivate() noexcept;
    void ForceCancelled() noexcept;
    bool RequestCancel() noexcept;
    bool SetLifecycleState(JobLifecycleState state) noexcept;

    std::uint64_t m_id{0U};
    std::uint64_t m_ownerModuleId{0U};
    std::atomic_bool m_active{true};
    std::atomic_bool m_cancellationRequested{false};
    std::atomic<JobLifecycleState> m_lifecycleState{JobLifecycleState::Queued};

    friend class HandleRegistry;
};

/**
 * @brief Owns and validates opaque module and job handles.
 *
 * Handle tokens are never dereferenced before a live-registry lookup. Retired
 * tokens remain uniquely owned by the registry until its destruction so a
 * stale pointer cannot become valid again through allocator address reuse.
 */
class HandleRegistry final
{
public:
    /**
     * @brief Creates an empty registry.
     */
    HandleRegistry();

    /**
     * @brief Releases all live states and retained opaque tokens.
     */
    ~HandleRegistry();

    HandleRegistry(const HandleRegistry&) = delete;
    HandleRegistry& operator=(const HandleRegistry&) = delete;
    HandleRegistry(HandleRegistry&&) = delete;
    HandleRegistry& operator=(HandleRegistry&&) = delete;

    /**
     * @brief Returns the process-wide registry used by exported SPI functions.
     * @return Process-wide registry instance.
     */
    [[nodiscard]] static HandleRegistry& Instance();

    /**
     * @brief Creates and registers one opaque module handle.
     * @return Live module handle.
     * @throws std::bad_alloc if registry storage cannot be allocated.
     */
    [[nodiscard]] pm_module_t* CreateModule();

    /**
     * @brief Destroys a module and retires all jobs that it still owns.
     * @param module Opaque module handle; nullptr is a successful no-op.
     * @return True for a live handle or nullptr, false for a stale/foreign one.
     */
    [[nodiscard]] bool DestroyModule(pm_module_t* module);

    /**
     * @brief Creates a queued job owned by a live module.
     * @param module Live owner module handle.
     * @return Live job handle, or nullptr for an invalid owner.
     * @throws std::bad_alloc if registry storage cannot be allocated.
     */
    [[nodiscard]] pm_job_t* CreateJob(pm_module_t* module);

    /**
     * @brief Releases a job, forcing unfinished minimal state to cancelled.
     * @param job Opaque job handle; nullptr is a successful no-op.
     * @return True for a live handle or nullptr, false for a stale/foreign one.
     */
    [[nodiscard]] bool ReleaseJob(pm_job_t* job);

    /**
     * @brief Resolves a live module handle without dereferencing caller memory.
     * @param module Opaque module handle.
     * @return Shared state for a live handle, otherwise nullptr.
     */
    [[nodiscard]] std::shared_ptr<const ModuleHandleState> FindModule(
        pm_module_t* module) const;

    /**
     * @brief Resolves a live job handle without dereferencing caller memory.
     * @param job Opaque job handle.
     * @return Shared state for a live handle, otherwise nullptr.
     */
    [[nodiscard]] std::shared_ptr<const JobHandleState> FindJob(
        pm_job_t* job) const;

    /**
     * @brief Resolves a live job only when it belongs to the supplied module.
     * @param module Expected owner module handle.
     * @param job Opaque job handle.
     * @return Shared state for a matching live pair, otherwise nullptr.
     */
    [[nodiscard]] std::shared_ptr<const JobHandleState> FindJob(
        pm_module_t* module,
        pm_job_t* job) const;

    /**
     * @brief Requests idempotent cooperative cancellation for a live job.
     * @param job Opaque job handle.
     * @return True if the handle is live, including already-terminal jobs.
     */
    [[nodiscard]] bool RequestCancel(pm_job_t* job);

    /**
     * @brief Advances the minimal state model for a live job.
     * @param job Opaque job handle.
     * @param state Requested lifecycle state.
     * @return True when the transition is valid and accepted.
     */
    [[nodiscard]] bool SetJobLifecycleState(
        pm_job_t* job,
        JobLifecycleState state);

    /**
     * @brief Returns the number of currently live module handles.
     * @return Live module count.
     */
    [[nodiscard]] std::size_t ActiveModuleCount() const;

    /**
     * @brief Returns the number of currently live job handles.
     * @return Live job count.
     */
    [[nodiscard]] std::size_t ActiveJobCount() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
