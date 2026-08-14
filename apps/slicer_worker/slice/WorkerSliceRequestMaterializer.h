#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include "slicer_core/api/Cancellation.h"
#include "slicer_core/json_value.h"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace slicesoft::worker
{

/** @brief 实体化切片 Worker 请求时抛出的稳定失败。 */
class WorkerSliceRequestMaterializationError final : public std::runtime_error
{
public:
    /**
     * @brief 使用公共稳定错误码创建实体化失败。
     * @param code 稳定的 PM-SLICER 错误码。
     * @param message 不含请求秘密的可读诊断信息。
     */
    WorkerSliceRequestMaterializationError(
        std::string code,
        const std::string& message);

    /** @brief 返回稳定的 PM-SLICER 错误码。 */
    [[nodiscard]] const std::string& Code() const noexcept;

private:
    std::string m_code;
};

/** @brief 为一个已校验切片请求生成的不可变路径与标识。 */
class WorkerSliceMaterialization final
{
public:
    /**
     * @brief 创建不可变实体化结果。
     * @param sceneSnapshotPath 规范化的已提交场景快照。
     * @param profilePath 规范化的有效 Profile 文档。
     * @param sceneConfigPath 已存在的场景有效配置路径。
     * @param packageDirectory 请求的最终 Package 目录。
     * @param sceneHash 不带前缀的小写 SHA-256 场景摘要。
     * @param profileHash 带前缀的小写 SHA-256 Profile 摘要。
     * @param profileVersion 调用方声明且纳入哈希的 Profile 版本。
     * @param sceneRevision 实体化期间校验的已提交场景修订号。
     * @param targetMode 来自有效 Profile 的显式生产流水线模式。
     * @param dpiX 水平生产分辨率。
     * @param dpiY 垂直生产分辨率。
     * @param productionAdmissionCommitted 所有可见实例均带已提交准入时为 true。
     */
    WorkerSliceMaterialization(
        std::filesystem::path sceneSnapshotPath,
        std::filesystem::path profilePath,
        std::filesystem::path sceneConfigPath,
        std::filesystem::path packageDirectory,
        std::string sceneHash,
        std::string profileHash,
        std::string profileVersion,
        std::uint64_t sceneRevision,
        std::string targetMode,
        int dpiX,
        int dpiY,
        bool productionAdmissionCommitted);

    /** @brief 返回已提交场景快照路径。 */
    [[nodiscard]] const std::filesystem::path& SceneSnapshotPath() const noexcept;

    /** @brief 返回已校验的有效 Profile 路径。 */
    [[nodiscard]] const std::filesystem::path& ProfilePath() const noexcept;

    /** @brief 返回生成的场景有效配置路径。 */
    [[nodiscard]] const std::filesystem::path& SceneConfigPath() const noexcept;

    /** @brief 返回请求的最终 Package 目录。 */
    [[nodiscard]] const std::filesystem::path& PackageDirectory() const noexcept;

    /** @brief 返回不带前缀的小写场景 SHA-256 摘要。 */
    [[nodiscard]] const std::string& SceneHash() const noexcept;

    /** @brief 返回带前缀的小写 Profile SHA-256 摘要。 */
    [[nodiscard]] const std::string& ProfileHash() const noexcept;

    /** @brief 返回调用方声明的 Profile 版本。 */
    [[nodiscard]] const std::string& ProfileVersion() const noexcept;

    /** @brief 返回已提交场景修订号。 */
    [[nodiscard]] std::uint64_t SceneRevision() const noexcept;

    /** @brief 返回显式生产流水线模式。 */
    [[nodiscard]] const std::string& TargetMode() const noexcept;

    /** @brief 返回水平生产 DPI。 */
    [[nodiscard]] int DpiX() const noexcept;

    /** @brief 返回垂直生产 DPI。 */
    [[nodiscard]] int DpiY() const noexcept;

    /** @brief 返回已提交场景是否带有生产准入。 */
    [[nodiscard]] bool ProductionAdmissionCommitted() const noexcept;

private:
    std::filesystem::path m_sceneSnapshotPath;
    std::filesystem::path m_profilePath;
    std::filesystem::path m_sceneConfigPath;
    std::filesystem::path m_packageDirectory;
    std::string m_sceneHash;
    std::string m_profileHash;
    std::string m_profileVersion;
    std::uint64_t m_sceneRevision{0U};
    std::string m_targetMode;
    int m_dpiX{0};
    int m_dpiY{0};
    bool m_productionAdmissionCommitted{false};
};

/** @brief 校验并原子实体化文件合同切片输入。 */
class WorkerSliceRequestMaterializer final
{
public:
    /**
     * @brief 计算 Worker 请求使用的规范化 Profile 标识。
     * @param profile 包含除 profileHash 外全部字段，或包含该字段的 Profile 对象。
     * @return `sha256:` 后接 64 个小写字符的摘要。
     * @throws std::invalid_argument profile 不是对象时抛出。
     */
    [[nodiscard]] static std::string ComputeProfileHash(
        const slicer_core::Json& profile);

    /**
     * @brief 在作业目录中实体化一个已校验的 `slice.rgbwsv` 请求。
     * @param request 不可变的已解析 Worker 请求。
     * @param cancelToken 协作式取消令牌。
     * @return 已校验的实体化路径与标识。
     * @throws WorkerSliceRequestMaterializationError 任何失败即拒绝校验或 IO 失败时抛出。
     */
    [[nodiscard]] static WorkerSliceMaterialization Materialize(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken);
};

}  // namespace slicesoft::worker
