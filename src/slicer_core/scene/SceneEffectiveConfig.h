#pragma once

#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <optional>
#include <string>

namespace slicer_core
{

/**
 * @brief Request used to derive one immutable scene effective config.
 */
struct SceneEffectiveConfigRequest
{
    MultiModelScene scene;
    std::filesystem::path sourcescenepath;
    std::filesystem::path generatedconfigpath;
    std::string sourceprofileid;
    std::filesystem::path sourceprofileconfigpath;
    std::filesystem::path outputpackagedir;
    std::string generatedatutc;
    int dpix{635};
    int dpiy{600};
    double layerheightmm{0.01};
    std::string slicepipelinemode{"legacy"};
    bool production{false};
    bool cancelled{false};
};

/**
 * @brief Generated scene effective config or one stable failure.
 */
struct SceneEffectiveConfigResult
{
    Json document;
    std::string confighash;
    std::optional<SceneValidationError> error;

    /**
     * @brief Report whether effective config generation succeeded.
     * @return True when a document exists without a blocking error.
     */
    bool IsValid() const;
};

/**
 * @brief Return the canonical scene effective-config schema.
 * @return Stable schema string.
 */
std::string_view SceneEffectiveConfigSchemaName();

/**
 * @brief Generate an in-memory immutable scene effective config.
 * @param request Scene, source identity, Profile, and audit timestamp.
 * @return Effective document and stable config hash.
 */
SceneEffectiveConfigResult GenerateSceneEffectiveConfig(
    const SceneEffectiveConfigRequest& request);

/**
 * @brief Generate and atomically publish a session-scoped effective config.
 * @param request Generation request including the output path.
 * @return Written document or a stable write/cancel error.
 */
SceneEffectiveConfigResult WriteSceneEffectiveConfig(
    const SceneEffectiveConfigRequest& request);

/**
 * @brief Read one scene effective config from disk.
 * @param path Effective config path.
 * @return Parsed document or a stable read error.
 */
SceneEffectiveConfigResult ReadSceneEffectiveConfig(
    const std::filesystem::path& path);

/**
 * @brief Check scene and transform revisions against an effective config.
 * @param document Previously generated effective config.
 * @param scene Current scene draft.
 * @return True when identity or revision data no longer match.
 */
bool IsSceneEffectiveConfigStale(
    const Json& document,
    const MultiModelScene& scene);

}  // namespace slicer_core
