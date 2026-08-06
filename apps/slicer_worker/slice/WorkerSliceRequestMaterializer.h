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

/** @brief Stable failure raised while materializing a slice Worker request. */
class WorkerSliceRequestMaterializationError final : public std::runtime_error
{
public:
    /**
     * @brief Creates a materialization failure with a public stable code.
     * @param code Stable PM-SLICER error code.
     * @param message Human-readable diagnostic without request secrets.
     */
    WorkerSliceRequestMaterializationError(
        std::string code,
        const std::string& message);

    /** @brief Returns the stable PM-SLICER error code. */
    [[nodiscard]] const std::string& Code() const noexcept;

private:
    std::string m_code;
};

/** @brief Immutable paths and identities produced for one validated slice request. */
class WorkerSliceMaterialization final
{
public:
    /**
     * @brief Creates an immutable materialization result.
     * @param sceneSnapshotPath Canonical committed scene snapshot.
     * @param profilePath Canonical effective Profile document.
     * @param sceneConfigPath Existing scene effective-config path.
     * @param packageDirectory Requested final package directory.
     * @param sceneHash Plain lowercase SHA-256 scene digest.
     * @param profileHash Prefixed lowercase SHA-256 Profile digest.
     * @param profileVersion Caller-declared Profile version included in the hash.
     * @param sceneRevision Committed scene revision verified during materialization.
     * @param targetMode Explicit production pipeline mode from the effective Profile.
     * @param dpiX Horizontal production resolution.
     * @param dpiY Vertical production resolution.
     * @param productionAdmissionCommitted True when every visible instance carries committed admission.
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

    /** @brief Returns the committed scene snapshot path. */
    [[nodiscard]] const std::filesystem::path& SceneSnapshotPath() const noexcept;

    /** @brief Returns the validated effective Profile path. */
    [[nodiscard]] const std::filesystem::path& ProfilePath() const noexcept;

    /** @brief Returns the generated scene effective-config path. */
    [[nodiscard]] const std::filesystem::path& SceneConfigPath() const noexcept;

    /** @brief Returns the requested final package directory. */
    [[nodiscard]] const std::filesystem::path& PackageDirectory() const noexcept;

    /** @brief Returns the plain lowercase scene SHA-256 digest. */
    [[nodiscard]] const std::string& SceneHash() const noexcept;

    /** @brief Returns the prefixed lowercase Profile SHA-256 digest. */
    [[nodiscard]] const std::string& ProfileHash() const noexcept;

    /** @brief Returns the caller-declared Profile version. */
    [[nodiscard]] const std::string& ProfileVersion() const noexcept;

    /** @brief Returns the committed scene revision. */
    [[nodiscard]] std::uint64_t SceneRevision() const noexcept;

    /** @brief Returns the explicit production pipeline mode. */
    [[nodiscard]] const std::string& TargetMode() const noexcept;

    /** @brief Returns the horizontal production DPI. */
    [[nodiscard]] int DpiX() const noexcept;

    /** @brief Returns the vertical production DPI. */
    [[nodiscard]] int DpiY() const noexcept;

    /** @brief Returns whether the committed scene carries production admission. */
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

/** @brief Validates and atomically materializes file-contract slice inputs. */
class WorkerSliceRequestMaterializer final
{
public:
    /**
     * @brief Computes the canonical Profile identity used by Worker requests.
     * @param profile Profile object containing all fields except or including profileHash.
     * @return `sha256:` followed by a lowercase 64-character digest.
     * @throws std::invalid_argument When profile is not an object.
     */
    [[nodiscard]] static std::string ComputeProfileHash(
        const slicer_core::Json& profile);

    /**
     * @brief Materializes one validated `slice.rgbwsv` request in its job directory.
     * @param request Immutable parsed Worker request.
     * @param cancelToken Cooperative cancellation token.
     * @return Validated materialized paths and identities.
     * @throws WorkerSliceRequestMaterializationError On any fail-closed validation or IO failure.
     */
    [[nodiscard]] static WorkerSliceMaterialization Materialize(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken);
};

}  // namespace slicesoft::worker
