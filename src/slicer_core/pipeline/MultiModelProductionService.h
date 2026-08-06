#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/api/Cancellation.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Stable failures returned by the scene production service.
 */
enum class MultiModelProductionErrorCode
{
    None,
    EffectiveConfigInvalid,
    EffectiveConfigStale,
    ResourceUnresolved,
    ProfileMismatch,
    BuildVolumeUndefined,
    PipelineModeNotAdmitted,
    ProductionPackageInvalid,
    Cancelled,
};

/**
 * @brief Structured scene production failure.
 */
struct MultiModelProductionError
{
    MultiModelProductionErrorCode code{
        MultiModelProductionErrorCode::None};
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::string field;
    std::string message;
};

/**
 * @brief Immutable scene production request.
 */
struct MultiModelProductionRequest
{
    std::filesystem::path effectiveconfigpath;
    SliceRunProgressCallback progresscallback;

    /**
     * @brief Optional synchronous, non-owning cancellation source.
     *
     * The caller must keep the token alive until the service returns.
     */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief One scene production result and immutable package identity.
 */
struct MultiModelProductionResult
{
    bool packagewritten{false};
    std::filesystem::path packagedir;
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    std::string scenehash;
    std::string effectiveconfighash;
    std::size_t visibleinstancecount{0U};
    int layercount{0};
    SliceRunProfile profile;
    std::optional<MultiModelProductionError> error;

    /**
     * @brief Report whether one complete scene package was published.
     * @return True when publication succeeded without an error.
     */
    bool IsValid() const;
};

/**
 * @brief Return one stable scene production error name.
 * @param code Scene production error code.
 * @return Stable ASCII error name.
 */
std::string_view MultiModelProductionErrorCodeName(
    MultiModelProductionErrorCode code);

/**
 * @brief Produce one RGBWSV package from an immutable scene effective config.
 * @param request Effective-config path.
 * @return Published package identity or one fail-closed error.
 */
MultiModelProductionResult RunMultiModelProductionService(
    const MultiModelProductionRequest& request);

}  // namespace slicer_core
