#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicesoft::worker
{

/** @brief Production executor for the frozen geometry.repair Worker capability. */
class WorkerRepairExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief Creates an executor over one production repair facade.
     * @param facade Owning repair facade; must not be null.
     */
    explicit WorkerRepairExecutor(
        std::unique_ptr<slicer_core::api::RepairFacade> facade);

    /**
     * @brief Materializes, executes, and publishes one job-owned repair asset.
     * @param request Immutable Worker request envelope.
     * @param cancelToken Cooperative cancellation token.
     * @return Structured repair result or stable fail-closed error.
     */
    [[nodiscard]] WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override;

private:
    std::unique_ptr<slicer_core::api::RepairFacade> m_facade;
};

/**
 * @brief Creates the production geometry.repair Worker executor.
 * @return Owning executor wired to the production repair facade.
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerRepairExecutor();

}  // namespace slicesoft::worker
