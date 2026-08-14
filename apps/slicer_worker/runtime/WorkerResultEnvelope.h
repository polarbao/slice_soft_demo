#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief Worker 取消结果使用的冻结清理证据。 */
class WorkerResultCleanup final
{
public:
    /**
     * @brief 创建清理证据。
     * @param stagingRemoved 是否已移除当前暂存区。
     * @param published 是否已发布新 Package。
     */
    WorkerResultCleanup(bool stagingRemoved, bool published) noexcept;

    /** @brief 返回是否已移除当前暂存区。 */
    [[nodiscard]] bool StagingRemoved() const noexcept;

    /** @brief 返回是否已发布新 Package。 */
    [[nodiscard]] bool Published() const noexcept;

private:
    bool m_stagingRemoved{false};
    bool m_published{false};
};

/** @brief 标识闭合的 file_contract_v1 结果文档。 */
class WorkerResultEnvelope final
{
public:
    /**
     * @brief 根据可信请求标识创建成功结果。
     * @param request 已校验的不可变请求信封。
     * @param output 来自真实执行器的非空输出对象。
     * @param engineVersion 非空引擎版本。
     * @param elapsed 非负耗时。
     * @return 带有 PM-SLICER-OK-0000 的有效成功结果。
     */
    [[nodiscard]] static WorkerResultEnvelope Success(
        const WorkerRequestEnvelope& request,
        slicer_core::Json output,
        std::string engineVersion,
        std::chrono::duration<double, std::milli> elapsed);

    /**
     * @brief 根据可信请求标识创建已处理失败结果。
     * @param request 已校验的不可变请求信封。
     * @param code 冻结的非成功 PM-SLICER 错误码。
     * @param message 非空公共失败消息。
     * @param detail 可选诊断详情。
     * @param engineVersion 非空引擎版本。
     * @param elapsed 非负耗时。
     * @param cleanup 可选清理证据；取消结果必须提供。
     * @return 有效且标识闭合的失败结果。
     */
    [[nodiscard]] static WorkerResultEnvelope Failure(
        const WorkerRequestEnvelope& request,
        std::string code,
        std::string message,
        std::optional<std::string> detail,
        std::string engineVersion,
        std::chrono::duration<double, std::milli> elapsed,
        std::optional<WorkerResultCleanup> cleanup = std::nullopt);

    /** @brief 返回复制到结果中的不可变请求标识。 */
    [[nodiscard]] const WorkerJobIdentity& Identity() const noexcept;

    /** @brief 返回结果是否成功。 */
    [[nodiscard]] bool Ok() const noexcept;

    /** @brief 返回稳定的 PM-SLICER 结果码。 */
    [[nodiscard]] const std::string& Code() const noexcept;

    /** @brief 将稳定错误码映射到冻结的 Worker 进程退出类别。 */
    [[nodiscard]] int ProcessExitCode() const noexcept;

    /** @brief 将结果序列化为 file_contract_v1 JSON 对象。 */
    [[nodiscard]] slicer_core::Json ToJson() const;

private:
    WorkerResultEnvelope(
        WorkerJobIdentity identity,
        bool ok,
        std::string code,
        slicer_core::Json output,
        std::string message,
        std::optional<std::string> detail,
        std::string engineVersion,
        double elapsedMs,
        std::optional<WorkerResultCleanup> cleanup);

    WorkerJobIdentity m_identity;
    bool m_ok{false};
    std::string m_code;
    slicer_core::Json m_output;
    std::string m_message;
    std::optional<std::string> m_detail;
    std::string m_engineVersion;
    double m_elapsedMs{0.0};
    std::optional<WorkerResultCleanup> m_cleanup;
};

}  // namespace slicesoft::worker
