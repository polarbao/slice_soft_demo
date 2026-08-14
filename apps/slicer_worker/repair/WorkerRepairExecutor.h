#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicesoft::worker
{

/** @brief 冻结的 geometry.repair Worker 能力生产执行器。 */
class WorkerRepairExecutor final : public IWorkerCapabilityExecutor
{
public:
    /**
     * @brief 基于生产修复 Facade 接口创建执行器。
     * @param facade 独占持有的修复 Facade，不得为空。
     */
    explicit WorkerRepairExecutor(
        std::unique_ptr<slicer_core::api::RepairFacade> facade);

    /**
     * @brief 实体化、执行并发布一个由作业持有的修复资产。
     * @param request 不可变的 Worker 请求信封。
     * @param cancelToken 协作式取消令牌。
     * @return 结构化修复结果或稳定的失败即拒绝错误。
     */
    [[nodiscard]] WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override;

private:
    std::unique_ptr<slicer_core::api::RepairFacade> m_facade;
};

/**
 * @brief 创建 geometry.repair 的生产 Worker 执行器。
 * @return 已接入生产修复 Facade 的独占执行器。
 */
[[nodiscard]] std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerRepairExecutor();

}  // namespace slicesoft::worker
