#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>
#include <ostream>

namespace slicesoft::worker
{

/** @brief Controlled production executor for the frozen slice.rgbwsv capability. */
class WorkerSliceExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief Creates an executor over the authoritative preflight and slice facades.
     * @param preflightFacade Owning production preflight facade; must not be null.
     * @param sliceFacade Owning production slice facade; must not be null.
     * @param protocolOutput Stream receiving reserved progress and timing lines.
     */
    WorkerSliceExecutor(
        std::unique_ptr<slicer_core::api::PreflightFullFacade> preflightFacade,
        std::unique_ptr<slicer_core::api::SliceFacade> sliceFacade,
        std::ostream& protocolOutput);

    /**
     * @brief Materializes, preflights, and slices one committed scene.
     * @param request Immutable Worker envelope.
     * @param cancelToken Cooperative cancellation token.
     * @return Basic production package evidence or a stable fail-closed error.
     */
    [[nodiscard]] WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override;

private:
    std::unique_ptr<slicer_core::api::PreflightFullFacade> m_preflightFacade;
    std::unique_ptr<slicer_core::api::SliceFacade> m_sliceFacade;
    std::ostream* m_protocolOutput{nullptr};
};

/**
 * @brief Creates the controlled production slice executor.
 * @param protocolOutput Stream receiving reserved protocol lines.
 * @return Owning executor wired to the only production facades.
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerSliceExecutor(std::ostream& protocolOutput);

}  // namespace slicesoft::worker
