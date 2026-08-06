#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicesoft::worker
{

/** @brief Production executor for the frozen geometry.preflight.full Worker capability. */
class WorkerPreflightExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief Create an executor over one authoritative facade.
     * @param facade Owning production or test facade; must not be null.
     */
    explicit WorkerPreflightExecutor(
        std::unique_ptr<slicer_core::api::PreflightFullFacade> facade);

    /**
     * @brief Materialize, validate, and execute one full-preflight request.
     * @param request Immutable Worker envelope.
     * @param cancelToken Cooperative cancellation token.
     * @return Structured business result or stable fail-closed error.
     */
    [[nodiscard]] WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override;

private:
    std::unique_ptr<slicer_core::api::PreflightFullFacade> m_facade;
};

/**
 * @brief Create the production geometry.preflight.full Worker executor.
 * @return Owning executor wired to the production preflight facade.
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerPreflightExecutor();

}  // namespace slicesoft::worker
