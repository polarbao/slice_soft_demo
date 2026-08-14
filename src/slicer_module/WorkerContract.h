#pragma once

#include "WorkerClient.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicesoft::module
{

/** @brief file_contract_v1 发现及兼容性检查的稳定结果分类。 */
enum class WorkerContractDecision
{
    Compatible,
    TransportFailure,
    InvalidDocument,
    MajorMismatch,
    MinorTooOld,
    MissingProductionContract,
    MissingCapability
};

/** @brief --contract-info 返回的已验证 Worker 标识。 */
struct WorkerContractInfo
{
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::string engineVersion;
    std::vector<std::string> produces;
    std::vector<std::string> capabilities;
};

/** @brief 启动 Worker 作业前必须满足的模块要求。 */
struct WorkerContractRequirement
{
    std::uint32_t major{1};
    std::uint32_t minor{0};
    std::vector<std::string> requiredProduces{"p0.rgbwsv.2"};
    std::vector<std::string> requiredCapabilities;
};

/** @brief 完整的发现握手传输与兼容性结果。 */
struct WorkerContractResult
{
    bool compatible{false};
    WorkerContractDecision decision{WorkerContractDecision::TransportFailure};
    std::string errorCode;
    std::string errorMessage;
    WorkerContractInfo info;
    WorkerRunResult transport;
};

/** @brief 执行并验证私有 file_contract_v1 发现握手。 */
class WorkerContractNegotiator final
{
public:
    /**
     * @brief 将协商绑定到负责进程生命周期的现有 WorkerClient。
     * @param client 保持 Stage 14D-02 超时和进程树语义的客户端。
     */
    explicit WorkerContractNegotiator(WorkerClient& client) noexcept;

    /**
     * @brief 运行 --contract-info 并应用失败即拒绝的兼容规则。
     * @param workerExecutable slicer_worker.exe 或测试 Worker 的绝对路径。
     * @param requirement 必需的主/次版本、生产合同和能力。
     * @return 已验证 Worker 信息或稳定拒绝诊断。
     */
    [[nodiscard]] WorkerContractResult Negotiate(
        const std::filesystem::path& workerExecutable,
        const WorkerContractRequirement& requirement) const;

private:
    WorkerClient& m_client;
};

}  // namespace slicesoft::module
