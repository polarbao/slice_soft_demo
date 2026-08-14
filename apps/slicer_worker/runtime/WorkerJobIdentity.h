#pragma once

#include <filesystem>
#include <string>

namespace slicesoft::worker
{

/** @brief 单个文件合同 Worker 作业的不可变标识与所持路径。 */
class WorkerJobIdentity final
{
public:
    /**
     * @brief 创建已校验的 Worker 作业标识。
     * @param jobId 来自 request.json 的稳定作业标识。
     * @param correlationId 调用方关联标识。
     * @param capability 精确 Worker 能力名称。
     * @param requestPath 规范化的请求绝对路径。
     */
    WorkerJobIdentity(
        std::string jobId,
        std::string correlationId,
        std::string capability,
        std::filesystem::path requestPath);

    /** @brief 返回稳定作业标识。 */
    [[nodiscard]] const std::string& JobId() const noexcept;

    /** @brief 返回调用方关联标识。 */
    [[nodiscard]] const std::string& CorrelationId() const noexcept;

    /** @brief 返回请求的精确能力。 */
    [[nodiscard]] const std::string& Capability() const noexcept;

    /** @brief 返回规范化的请求文件绝对路径。 */
    [[nodiscard]] const std::filesystem::path& RequestPath() const noexcept;

    /** @brief 返回由本作业持有的规范化目录。 */
    [[nodiscard]] const std::filesystem::path& JobDirectory() const noexcept;

    /** @brief 返回最终结果文档路径。 */
    [[nodiscard]] const std::filesystem::path& ResultPath() const noexcept;

    /** @brief 返回临时结果文档路径。 */
    [[nodiscard]] const std::filesystem::path& ResultTemporaryPath() const noexcept;

    /** @brief 返回协作式取消标记路径。 */
    [[nodiscard]] const std::filesystem::path& CancelPath() const noexcept;

private:
    std::string m_jobId;
    std::string m_correlationId;
    std::string m_capability;
    std::filesystem::path m_requestPath;
    std::filesystem::path m_jobDirectory;
    std::filesystem::path m_resultPath;
    std::filesystem::path m_resultTemporaryPath;
    std::filesystem::path m_cancelPath;
};

}  // namespace slicesoft::worker
