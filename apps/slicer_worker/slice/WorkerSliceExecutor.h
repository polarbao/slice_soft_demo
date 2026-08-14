#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>
#include <ostream>

namespace slicesoft::worker
{

/** @brief 冻结的 slice.rgbwsv 能力受控生产执行器。 */
class WorkerSliceExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief 基于权威预检与切片 Facade 接口创建执行器。
     * @param preflightFacade 独占持有的生产预检 Facade，不得为空。
     * @param sliceFacade 独占持有的生产切片 Facade，不得为空。
     * @param protocolOutput 接收保留进度与耗时行的输出流。
     */
    WorkerSliceExecutor(
        std::unique_ptr<slicer_core::api::PreflightFullFacade> preflightFacade,
        std::unique_ptr<slicer_core::api::SliceFacade> sliceFacade,
        std::ostream& protocolOutput);

    /**
     * @brief 实体化、预检并切片一个已提交场景。
     * @param request 不可变的 Worker 信封。
     * @param cancelToken 协作式取消令牌。
     * @return 基础生产 Package 证据或稳定的失败即拒绝错误。
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
 * @brief 创建受控生产切片执行器。
 * @param protocolOutput 接收保留协议行的输出流。
 * @return 已接入唯一生产 Facade 的独占执行器。
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerSliceExecutor(std::ostream& protocolOutput);

}  // namespace slicesoft::worker
