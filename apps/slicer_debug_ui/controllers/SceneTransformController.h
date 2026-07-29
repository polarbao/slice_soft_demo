#pragma once

#include "../models/SceneDocument.h"
#include "../models/SceneProjectionRequest.h"
#include "../models/SceneSelectionModel.h"
#include "../services/SceneModelRepository.h"

#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Stable scene transform command failures.
 */
enum class SceneTransformErrorCode
{
    None,
    NoSelection,
    InstanceLocked,
    SceneRevisionStale,
    TransformRevisionStale,
    NonFinite,
    ScaleNonPositive,
    SourceCacheMissing,
    ProjectionUnavailable,
    EffectiveConfigStale,
    SaveCancelled,
    SaveFailed,
};

/**
 * @brief One scene transform command failure.
 */
struct SceneTransformError
{
    SceneTransformErrorCode code{SceneTransformErrorCode::None};
    QString field;
    QString message;
};

/**
 * @brief Result of one precise transform command.
 */
struct SceneTransformCommandResult
{
    bool changed{false};
    std::optional<SceneTransformError> error;

    /**
     * @brief Report whether the command passed validation.
     * @return True when no error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Session-scoped scene/effective-config save request.
 */
struct SceneTransformSaveRequest
{
    std::filesystem::path sessiondirectory;
    std::filesystem::path sourceprofileconfigpath;
    std::filesystem::path outputpackagedir;
    std::string sourceprofileid;
    std::string generatedatutc;
    slicer_core::SceneBuildVolume buildvolume;
    int dpix{slicer_core::kDefaultOutputDpiX};
    int dpiy{slicer_core::kDefaultOutputDpiY};
    double layerheightmm{
        slicer_core::kDefaultLayerThicknessMm};
    std::string slicepipelinemode{"legacy"};
    std::uint64_t expectedscenerevision{0U};
    std::uint64_t expectedtransformrevision{0U};
    bool production{false};
    bool cancelled{false};
};

/**
 * @brief Saved single-instance scene identity and paths.
 */
struct SceneTransformSaveResult
{
    slicer_core::MultiModelScene scene;
    std::filesystem::path scenepath;
    std::filesystem::path effectiveconfigpath;
    std::string confighash;
    std::optional<SceneTransformError> error;

    /**
     * @brief Report whether save and readback completed.
     * @return True when no error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Immutable in-memory scene snapshot used by diagnostic and save flows.
 */
struct SceneTransformSnapshotResult
{
    slicer_core::MultiModelScene scene;
    std::optional<SceneTransformError> error;

    /**
     * @brief Report whether the current scene was assembled successfully.
     * @return True when no source, identity, or validation error exists.
     */
    bool IsValid() const;
};

/**
 * @brief Convert a transform command error to stable ASCII.
 * @param code Error code.
 * @return Stable machine-readable name.
 */
std::string_view SceneTransformErrorCodeName(
    SceneTransformErrorCode code);

/**
 * @brief Apply precise instance transforms and save current scene state.
 */
class SceneTransformController final : public QObject
{
    Q_OBJECT

public:
    using ProjectionRequester =
        std::function<void(const SceneProjectionRequest&)>;

    /**
     * @brief Create the precise transform controller.
     * @param document Editable single-instance scene document.
     * @param selectionModel Shared instance selection.
     * @param repository Immutable source model repository.
     * @param parent QObject owner.
     */
    explicit SceneTransformController(
        SceneDocument* document,
        SceneSelectionModel* selectionModel,
        SceneModelRepository* repository,
        QObject* parent = nullptr);

    /**
     * @brief Install the asynchronous projection request callback.
     * @param requester Callback owned by the UI orchestration layer.
     */
    void SetProjectionRequester(ProjectionRequester requester);

    /**
     * @brief Apply one complete X/Y, rotate-Z, uniform-scale transform.
     * @param transform Requested transform.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @param expectedTransformRevision Caller-observed transform revision.
     * @return Structured command result.
     */
    SceneTransformCommandResult SetTransform(
        const slicer_core::ModelTransform& transform,
        std::uint64_t expectedSceneRevision,
        std::uint64_t expectedTransformRevision);

    /**
     * @brief Move current effective XY bounds to the software scene origin.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @param expectedTransformRevision Caller-observed transform revision.
     * @return Structured command result.
     */
    SceneTransformCommandResult CenterAtSceneOrigin(
        std::uint64_t expectedSceneRevision,
        std::uint64_t expectedTransformRevision);

    /**
     * @brief Restore identity instance transform.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @param expectedTransformRevision Caller-observed transform revision.
     * @return Structured command result.
     */
    SceneTransformCommandResult ResetTransform(
        std::uint64_t expectedSceneRevision,
        std::uint64_t expectedTransformRevision);

    /**
     * @brief Save and read back a current-scene effective config.
     * @param request Session path, Profile, slice contract, and revisions.
     * @return Saved scene and structured error.
     */
    SceneTransformSaveResult SaveSceneEffectiveConfig(
        const SceneTransformSaveRequest& request);

    /**
     * @brief Assemble the current scene without writing files or marking it saved.
     * @param sourceProfileId Scene-wide material process Profile identity.
     * @param buildVolume Explicit diagnostic or production build volume.
     * @return Immutable scene snapshot or a structured failure.
     */
    SceneTransformSnapshotResult BuildCurrentScene(
        const std::string& sourceProfileId,
        const slicer_core::SceneBuildVolume& buildVolume) const;

signals:
    void SigCommandFailed(const QString& code, const QString& message);
    void SigTransformChanged();
    void SigSceneSaved(const QString& effectiveConfigPath);

private:
    SceneTransformCommandResult ValidateCommand(
        std::uint64_t expectedSceneRevision,
        std::uint64_t expectedTransformRevision);
    SceneTransformCommandResult ApplyTransform(
        const slicer_core::ModelTransform& transform,
        std::uint64_t expectedSceneRevision,
        std::uint64_t expectedTransformRevision);
    SceneTransformCommandResult Failure(
        SceneTransformErrorCode code,
        const QString& field,
        const QString& message);

    SceneDocument* m_document{nullptr};
    SceneSelectionModel* m_selectionModel{nullptr};
    SceneModelRepository* m_repository{nullptr};
    ProjectionRequester m_projectionRequester;
};
