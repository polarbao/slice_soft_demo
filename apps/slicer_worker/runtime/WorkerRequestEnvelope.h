#pragma once

#include "slicer_worker/runtime/WorkerJobIdentity.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief 不含算法专属解释且已校验的 file_contract_v1 请求。 */
class WorkerRequestEnvelope final
{
public:
    /**
     * @brief 创建已校验的请求信封。
     * @param identity 不可变请求标识与所持路径。
     * @param major 文件合同主版本号。
     * @param minor 文件合同次版本号。
     * @param timeout 有限的 Worker 超时时间。
     * @param sceneHash 可选的已校验场景哈希。
     * @param scene 原始场景对象；不存在时为 JSON null。
     * @param profile 原始 Profile 对象；不存在时为 JSON null。
     * @param input 原始输入对象；不存在时为 JSON null。
     * @param output 原始输出对象；不存在时为 JSON null。
     */
    WorkerRequestEnvelope(
        WorkerJobIdentity identity,
        std::uint32_t major,
        std::uint32_t minor,
        std::chrono::milliseconds timeout,
        std::optional<std::string> sceneHash,
        slicer_core::Json scene,
        slicer_core::Json profile,
        slicer_core::Json input,
        slicer_core::Json output);

    /** @brief 返回不可变作业标识。 */
    [[nodiscard]] const WorkerJobIdentity& Identity() const noexcept;

    /** @brief 返回文件合同主版本号。 */
    [[nodiscard]] std::uint32_t Major() const noexcept;

    /** @brief 返回文件合同次版本号。 */
    [[nodiscard]] std::uint32_t Minor() const noexcept;

    /** @brief 返回有限执行超时时间。 */
    [[nodiscard]] std::chrono::milliseconds Timeout() const noexcept;

    /** @brief 返回可选的已校验场景哈希。 */
    [[nodiscard]] const std::optional<std::string>& SceneHash() const noexcept;

    /** @brief 返回是否存在原始场景对象。 */
    [[nodiscard]] bool HasScene() const noexcept;

    /** @brief 返回原始场景对象；不存在时返回 JSON null。 */
    [[nodiscard]] const slicer_core::Json& Scene() const noexcept;

    /** @brief 返回是否存在原始 Profile 对象。 */
    [[nodiscard]] bool HasProfile() const noexcept;

    /** @brief 返回原始 Profile 对象；不存在时返回 JSON null。 */
    [[nodiscard]] const slicer_core::Json& Profile() const noexcept;

    /** @brief 返回是否存在原始输入对象。 */
    [[nodiscard]] bool HasInput() const noexcept;

    /** @brief 返回原始输入对象；不存在时返回 JSON null。 */
    [[nodiscard]] const slicer_core::Json& Input() const noexcept;

    /** @brief 返回是否存在原始输出对象。 */
    [[nodiscard]] bool HasOutput() const noexcept;

    /** @brief 返回原始输出对象；不存在时返回 JSON null。 */
    [[nodiscard]] const slicer_core::Json& Output() const noexcept;

private:
    WorkerJobIdentity m_identity;
    std::uint32_t m_major{0U};
    std::uint32_t m_minor{0U};
    std::chrono::milliseconds m_timeout{0};
    std::optional<std::string> m_sceneHash;
    slicer_core::Json m_scene;
    slicer_core::Json m_profile;
    slicer_core::Json m_input;
    slicer_core::Json m_output;
};

}  // namespace slicesoft::worker
