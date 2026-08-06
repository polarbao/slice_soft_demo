#pragma once

#include "contracts/print_module_spi.h"
#include "slicer_module/CapabilityCarrierRouter.h"
#include "slicer_module/CapabilityJsonAdapter.h"

#include <cstdint>
#include <memory>
#include <string>

namespace slicesoft::module
{

/** @brief Outcome of accepting one heavy capability into the Worker carrier. */
struct WorkerJobSubmission
{
    bool accepted{false};
    std::string errorCode;
    std::string errorMessage;
    std::string errorDetail;
};

/**
 * @brief Owns asynchronous Worker jobs behind the frozen public C SPI.
 *
 * The service owns process clients, private file-contract directories, cached
 * progress, terminal results, cancellation, and module/job cleanup. It never
 * links or calls the slicing engine in-process.
 */
class WorkerJobService final
{
public:
    /** @brief Returns the process-wide Worker job service. @return Shared service. */
    [[nodiscard]] static WorkerJobService& Instance();

    /**
     * @brief Accepts one validated Worker route and starts it asynchronously.
     * @param job Live public job handle.
     * @param module Live owner module handle.
     * @param route Validated Worker carrier route.
     * @param moduleId Registry-local module identity.
     * @param jobId Registry-local job identity.
     * @return Acceptance result; failures occur before a thread is retained.
     */
    [[nodiscard]] WorkerJobSubmission Submit(
        pm_job_t* job,
        pm_module_t* module,
        CapabilityRoute route,
        std::uint64_t moduleId,
        std::uint64_t jobId);

    /** @brief Reports whether the job is Worker-owned. @param job Job handle. @return True when retained. */
    [[nodiscard]] bool HasJob(pm_job_t* job) const;

    /** @brief Returns the cached progress JSON. @param job Job handle. @return Progress or empty. */
    [[nodiscard]] std::string Poll(pm_job_t* job) const;

    /** @brief Returns terminal public result bytes. @param job Job handle. @return Result or null before terminal. */
    [[nodiscard]] std::shared_ptr<const CapabilityOutput> Result(
        pm_job_t* job) const;

    /** @brief Requests cooperative cancellation. @param job Job handle. @return True for a retained job. */
    bool RequestCancel(pm_job_t* job) noexcept;

    /** @brief Cancels, joins, and removes one Worker job. @param job Job handle. */
    void ReleaseJob(pm_job_t* job) noexcept;

    /** @brief Cancels, joins, and removes all Worker jobs owned by a module. @param module Module handle. */
    void RemoveModule(pm_module_t* module) noexcept;

private:
    struct Implementation;

    WorkerJobService();
    ~WorkerJobService();

    WorkerJobService(const WorkerJobService&) = delete;
    WorkerJobService& operator=(const WorkerJobService&) = delete;

    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
