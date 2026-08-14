#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"
#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include "slicer_core/api/Cancellation.h"
#include "slicer_core/json_value.h"

#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief 无权改写 Worker 作业标识的算法结果。 */
class WorkerCapabilityExecutionResult final
{
public:
    /** @brief 使用非空业务输出对象创建执行成功结果。 */
    [[nodiscard]] static WorkerCapabilityExecutionResult Success(
        slicer_core::Json output);

    /** @brief 使用稳定错误码与消息创建执行失败结果。 */
    [[nodiscard]] static WorkerCapabilityExecutionResult Failure(
        std::string code,
        std::string message,
        std::optional<std::string> detail = std::nullopt,
        std::optional<WorkerResultCleanup> cleanup = std::nullopt);

    /** @brief 返回执行是否成功。 */
    [[nodiscard]] bool Ok() const noexcept;

    /** @brief 返回不含标识信封的业务输出。 */
    [[nodiscard]] const slicer_core::Json& Output() const noexcept;

    /** @brief 返回稳定失败码。 */
    [[nodiscard]] const std::string& Code() const noexcept;

    /** @brief 返回失败消息。 */
    [[nodiscard]] const std::string& Message() const noexcept;

    /** @brief 返回可选诊断详情。 */
    [[nodiscard]] const std::optional<std::string>& Detail() const noexcept;

    /** @brief 返回可选清理证据。 */
    [[nodiscard]] const std::optional<WorkerResultCleanup>& Cleanup() const noexcept;

private:
    WorkerCapabilityExecutionResult(
        bool ok,
        slicer_core::Json output,
        std::string code,
        std::string message,
        std::optional<std::string> detail,
        std::optional<WorkerResultCleanup> cleanup);

    bool m_ok{false};
    slicer_core::Json m_output;
    std::string m_code;
    std::string m_message;
    std::optional<std::string> m_detail;
    std::optional<WorkerResultCleanup> m_cleanup;
};

/** @brief 单项重型能力的 Worker 私有执行端口。 */
class IWorkerCapabilityExecutor
{
public:
    virtual ~IWorkerCapabilityExecutor() = default;

    /**
     * @brief 执行已校验请求，但不持有结果标识。
     * @param request 不可变的已校验请求。
     * @param cancelToken 由分派器持有的协作式取消令牌。
     * @return 供分派器构建结果信封的业务结果。
     */
    [[nodiscard]] virtual WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) = 0;
};

}  // namespace slicesoft::worker
