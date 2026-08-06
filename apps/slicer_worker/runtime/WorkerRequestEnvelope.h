#pragma once

#include "slicer_worker/runtime/WorkerJobIdentity.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief Validated file_contract_v1 request without algorithm-specific interpretation. */
class WorkerRequestEnvelope final
{
public:
    /**
     * @brief Creates a validated request envelope.
     * @param identity Immutable request identity and owned paths.
     * @param major File-contract major version.
     * @param minor File-contract minor version.
     * @param timeout Finite worker timeout.
     * @param sceneHash Optional validated scene hash.
     * @param scene Raw scene object or JSON null when absent.
     * @param profile Raw profile object or JSON null when absent.
     * @param input Raw input object or JSON null when absent.
     * @param output Raw output object or JSON null when absent.
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

    /** @brief Returns the immutable job identity. */
    [[nodiscard]] const WorkerJobIdentity& Identity() const noexcept;

    /** @brief Returns the file-contract major version. */
    [[nodiscard]] std::uint32_t Major() const noexcept;

    /** @brief Returns the file-contract minor version. */
    [[nodiscard]] std::uint32_t Minor() const noexcept;

    /** @brief Returns the finite execution timeout. */
    [[nodiscard]] std::chrono::milliseconds Timeout() const noexcept;

    /** @brief Returns the optional validated scene hash. */
    [[nodiscard]] const std::optional<std::string>& SceneHash() const noexcept;

    /** @brief Returns whether a raw scene object is present. */
    [[nodiscard]] bool HasScene() const noexcept;

    /** @brief Returns the raw scene object or JSON null. */
    [[nodiscard]] const slicer_core::Json& Scene() const noexcept;

    /** @brief Returns whether a raw profile object is present. */
    [[nodiscard]] bool HasProfile() const noexcept;

    /** @brief Returns the raw profile object or JSON null. */
    [[nodiscard]] const slicer_core::Json& Profile() const noexcept;

    /** @brief Returns whether a raw input object is present. */
    [[nodiscard]] bool HasInput() const noexcept;

    /** @brief Returns the raw input object or JSON null. */
    [[nodiscard]] const slicer_core::Json& Input() const noexcept;

    /** @brief Returns whether a raw output object is present. */
    [[nodiscard]] bool HasOutput() const noexcept;

    /** @brief Returns the raw output object or JSON null. */
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
