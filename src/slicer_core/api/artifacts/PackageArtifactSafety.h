#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core::api::artifacts
{

/** @brief 一次生产包发布尝试所使用的作业专属路径。 */
struct PackageArtifactIdentity
{
    std::filesystem::path package_directory;
    std::filesystem::path staging_directory;
    std::filesystem::path backup_directory;
    std::filesystem::path lease_directory;
    std::string job_id;
    std::string attempt_id;
};

/** @brief 一次幂等清理或崩溃恢复的结果。 */
struct PackageArtifactRecoveryResult
{
    bool success{false};
    bool target_removed{false};
    bool target_restored{false};
    bool staging_removed{false};
    bool backup_removed{false};
    bool lease_removed{false};
    std::string error;
    std::vector<std::filesystem::path> residual_paths;
};

/** @brief 仅接受完整且已发布 RGBWSV 生产包的验证回调。 */
using PackageArtifactValidator =
    std::function<bool(const std::filesystem::path&)>;

/** @brief 获取或释放单目标文件系统租约的结果。 */
struct PackageArtifactLeaseResult
{
    bool success{false};
    bool conflict{false};
    std::string error;
};

/** @brief 作业专属发布处理引发的稳定输出失败。 */
class PackageArtifactOutputError final : public std::runtime_error
{
public:
    /**
     * @brief 构造一个稳定发布失败。
     * @param message 不受用户格式控制的诊断消息。
     */
    explicit PackageArtifactOutputError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/** @brief 生产包目标被其他作业持有时抛出的稳定冲突。 */
class PackageArtifactLeaseConflict final : public std::runtime_error
{
public:
    /**
     * @brief 构造一个目标租约冲突。
     * @param message 不受用户格式控制的诊断消息。
     */
    explicit PackageArtifactLeaseConflict(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/**
 * @brief 构造规范化的作业专属生产包产物路径。
 * @param packageDirectory 最终 Package 目录的绝对路径。
 * @param jobId 文件合同作业标识。
 * @param attemptId 本次发布尝试的唯一标识。
 * @return 已验证的目标、暂存、备份和租约路径。
 * @throws std::invalid_argument 路径或标识不安全时抛出。
 */
[[nodiscard]] PackageArtifactIdentity MakePackageArtifactIdentity(
    const std::filesystem::path& packageDirectory,
    const std::string& jobId,
    const std::string& attemptId);

/**
 * @brief 为请求派生确定性且可安全用作文件名的尝试 ID。
 * @param correlationId 非空的文件合同关联标识。
 * @return 基于小写 SHA-256 的稳定尝试令牌。
 */
[[nodiscard]] std::string MakePackageAttemptId(
    std::string_view correlationId);

/**
 * @brief 报告 Package 路径是否指向暂存、备份、租约、tmp 或 bak 数据。
 * @param path 候选 Package 路径。
 * @return 文件名包含保留临时产物标记时返回 true。
 */
[[nodiscard]] bool IsTemporaryPackagePath(
    const std::filesystem::path& path) noexcept;

/**
 * @brief 为指定所有者获取目标级发布租约。
 * @param identity 已验证的作业专属产物标识。
 * @return 租约结果；已被其他所有者持有时 conflict 为 true。
 */
[[nodiscard]] PackageArtifactLeaseResult AcquirePackageArtifactLease(
    const PackageArtifactIdentity& identity) noexcept;

/**
 * @brief 仅在租约所有者与产物标识匹配时释放目标级租约。
 * @param identity 已验证的作业专属产物标识。
 * @return 租约结果；所有者不匹配或租约记录格式错误时失败即拒绝。
 */
[[nodiscard]] PackageArtifactLeaseResult ReleasePackageArtifactLease(
    const PackageArtifactIdentity& identity) noexcept;

/**
 * @brief 仅恢复或清理某次尝试明确持有的产物。
 * @param identity 已验证的作业专属产物标识。
 * @param validator 删除或恢复备份前使用的严格生产包验证器。
 * @return 幂等恢复证据；失败时保留状态不确定的产物。
 */
[[nodiscard]] PackageArtifactRecoveryResult RecoverPackageArtifacts(
    const PackageArtifactIdentity& identity,
    const PackageArtifactValidator& validator) noexcept;

/**
 * @brief 删除本作业租约下新产生且经指定验证器确认的拒绝产物。
 * @param identity 已验证且当前租约仍由本作业持有的产物标识。
 * @param validator 只接受本次允许删除的拒绝产物的严格验证器。
 * @return 删除证据；所有权、路径或验证不闭合时保留目标并失败。
 */
[[nodiscard]] PackageArtifactRecoveryResult RemoveRejectedPublishedPackage(
    const PackageArtifactIdentity& identity,
    const PackageArtifactValidator& validator) noexcept;

}  // namespace slicer_core::api::artifacts
