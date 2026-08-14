#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicesoft::worker
{

/** @brief 冻结的 geometry.preflight.full Worker 能力生产执行器。 */
class WorkerPreflightExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief 基于权威 Facade 接口创建执行器。
     * @param facade 独占持有的生产或测试 Facade，不得为空。
     */
    explicit WorkerPreflightExecutor(
        std::unique_ptr<slicer_core::api::PreflightFullFacade> facade);

    /**
     * @brief 实体化、校验并执行一次完整预检请求。
     * @param request 不可变的 Worker 信封。
     * @param cancelToken 协作式取消令牌。
     * @return 结构化业务结果或稳定的失败即拒绝错误。
     */
    [[nodiscard]] WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override;

private:
    std::unique_ptr<slicer_core::api::PreflightFullFacade> m_facade;
};

/**
 * @brief 创建 geometry.preflight.full 的生产 Worker 执行器。
 * @return 已接入生产预检 Facade 的独占执行器。
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerPreflightExecutor();

}  // namespace slicesoft::worker
