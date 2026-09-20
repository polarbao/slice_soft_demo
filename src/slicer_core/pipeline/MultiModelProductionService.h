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
    OutputPublicationFailed,
    PackageTargetBusy,
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
    std::string jobid;
    std::string attemptid;
    SliceRunProgressCallback progresscallback;

    /**
     * @brief Optional synchronous, non-owning cancellation source.
     *
     * The caller must keep the token alive until the service returns.
     */
    const api::ICancelToken* canceltoken{nullptr};

    /**
     * @brief MW3-05：本次运行产出整版七通道包（`p0.rgbwsvt.1`）。
     *
     * 由 `RunTransferProductionEntry` 显式置位——**它本来就知道自己是
     * 缩裹入口**，比让服务去重解析 profile 里的 packageProtocol 更直接，
     * 也让「入口说七通道、实际却没有一个实例带缩裹」这种不一致可被察觉。
     *
     * 置位后服务会给合成请求挂上 `platemasksink`、把整版六通道层按整版
     * 掩膜装配成七通道层，再交给包会话的七通道 `AppendLayer`。
     * **为 false 时整条路径零分配、零分支**，六通道行为逐字节不变。
     */
    bool transferchannel{false};
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
