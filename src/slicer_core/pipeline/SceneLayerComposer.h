#pragma once

#include "slicer_core/pipeline/SceneRasterTypes.h"

#include <span>

namespace slicer_core
{

struct MultiModelLayerComposeRequest;

/**
 * @brief Immutable evidence that the scene composer closed every output pixel.
 *
 * Only the composer/orchestrator can create this value. Downstream report and
 * package stages may therefore reuse the composer's validation evidence
 * instead of rescanning every RGBWSV byte before the independent persisted
 * package validation.
 */
class ValidatedSceneLayerComposeResult final
{
public:
    ValidatedSceneLayerComposeResult(
        ValidatedSceneLayerComposeResult&&) noexcept = default;
    ValidatedSceneLayerComposeResult& operator=(
        ValidatedSceneLayerComposeResult&&) noexcept = default;
    ValidatedSceneLayerComposeResult(
        const ValidatedSceneLayerComposeResult&) = delete;
    ValidatedSceneLayerComposeResult& operator=(
        const ValidatedSceneLayerComposeResult&) = delete;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const SceneLayerComposeResult& Value() const noexcept;
    [[nodiscard]] SceneLayerComposeResult Release() && noexcept;

private:
    explicit ValidatedSceneLayerComposeResult(
        SceneLayerComposeResult result) noexcept;

    SceneLayerComposeResult m_result;
    bool m_validated{false};

    friend ValidatedSceneLayerComposeResult ComposeValidatedSceneLayers(
        const SceneLayerComposeRequest& request);
    friend ValidatedSceneLayerComposeResult
    ComposeAdmittedSceneRastersValidated(
        const MultiModelLayerComposeRequest& request);
    friend ValidatedSceneLayerComposeResult
    ComposeAdmittedSceneRastersValidated(
        MultiModelLayerComposeRequest&& request);
};

/**
 * @brief Compose admitted per-instance RGBWSV rasters into one shared scene raster.
 * @param request Immutable scene grid, protocol, revision, and instance layers.
 * @return Complete writer-ready layers or one structured fail-closed error.
 */
SceneLayerComposeResult ComposeSceneLayers(
    const SceneLayerComposeRequest& request);

/**
 * @brief Compose and retain the composer's in-loop closure evidence.
 * @param request Immutable scene grid, protocol, revision, and instance layers.
 * @return Immutable validated result, or the original structured blocked result.
 */
ValidatedSceneLayerComposeResult ComposeValidatedSceneLayers(
    const SceneLayerComposeRequest& request);

namespace internal
{

/**
 * @brief Synchronously compose borrowed authoritative raster buffers.
 *
 * This internal entry avoids copying multi-gigabyte per-instance RGBWSV
 * buffers while keeping the public request owning and lifetime-safe.
 */
SceneLayerComposeResult ComposeSceneLayersBorrowed(
    const SceneLayerComposeRequest& request,
    std::span<const SceneInstanceRaster> instances);

/**
 * @brief Consume an owned raster list when an exact single-instance fast path is available.
 *
 * Multi-instance and offset scenes retain the established borrowed composer.
 */
SceneLayerComposeResult ComposeSceneLayersConsuming(
    const SceneLayerComposeRequest& request,
    std::vector<SceneInstanceRaster>&& instances);

}  // namespace internal

}  // namespace slicer_core
