#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include <filesystem>
#include <stdexcept>

namespace slicesoft::worker
{

/** @brief 建立可信作业标识前的稳定解析失败类别。 */
enum class WorkerRequestParseErrorCode
{
    InvalidPath,
    ReadFailure,
    InvalidEncoding,
    InvalidJson,
    ContractViolation
};

/** @brief 描述请求解析或校验触发的失败即拒绝结果。 */
class WorkerRequestParseError final : public std::runtime_error
{
public:
    /**
     * @brief 使用稳定类别创建解析错误。
     * @param code 稳定解析失败类别。
     * @param message 不含请求秘密的可读诊断信息。
     */
    WorkerRequestParseError(
        WorkerRequestParseErrorCode code,
        const std::string& message);

    /** @brief 返回稳定解析失败类别。 */
    [[nodiscard]] WorkerRequestParseErrorCode Code() const noexcept;

private:
    WorkerRequestParseErrorCode m_code;
};

/** @brief 严格的 file_contract_v1 请求解析器与语义校验器。 */
class WorkerRequestParser final
{
public:
    /**
     * @brief 将一个 request.json 绝对路径解析为不可变信封。
     * @param requestPath 已存在的普通文件绝对路径。
     * @return 带有规范化所持路径的已校验请求信封。
     * @throws WorkerRequestParseError 路径、编码、JSON 或合同校验失败时抛出。
     */
    [[nodiscard]] static WorkerRequestEnvelope Parse(
        const std::filesystem::path& requestPath);
};

}  // namespace slicesoft::worker
