#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"
#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace slicesoft::worker
{

/** @brief 单作业精确能力注册表与失败即拒绝分派器。 */
class WorkerJobDispatcher final
{
public:
    /**
     * @brief 为一项冻结能力注册唯一执行器。
     * @param capability 精确能力名称。
     * @param executor 独占的生产或测试执行器实现。
     * @throws std::invalid_argument 能力未知、执行器为空或重复注册时抛出。
     */
    void Register(
        std::string capability,
        std::unique_ptr<IWorkerCapabilityExecutor> executor);

    /**
     * @brief 分派一个请求并创建标识闭合的结果。
     * @param request 不可变的已校验请求。
     * @return 精确执行器的成功结果或稳定的失败即拒绝结果。
     */
    [[nodiscard]] WorkerResultEnvelope Dispatch(
        const WorkerRequestEnvelope& request) const;

private:
    std::unordered_map<std::string, std::unique_ptr<IWorkerCapabilityExecutor>> m_executors;
};

}  // namespace slicesoft::worker
