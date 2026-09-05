#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/ModelInstance.h"

#include <filesystem>
#include <functional>

namespace slicer_core
{

/**
 * @brief Request for extracting one Legacy instance as in-memory RGBWSV layers.
 */
struct LegacySceneLayerAdapterRequest
{
    std::filesystem::path configpath;
    std::filesystem::path modelpathoverride;
    std::string modelformatoverride{"auto"};
    SceneRasterIdentity identity;
    ModelInstance instance;
    const ModelReport* modelreportoverride{nullptr};
    SliceRunProgressCallback progresscallback;

    /**
     * @brief MF-05 步骤 2：逐层出口。非空时 adapter【不再累积】raster.layers。
     *
     * 现状是每层 push_back 且不释放，单实例持有全部层（6 通道 + 4 张归属 mask
     * = 10 B/列/层）。用户 0.2+0.3 @10um 实测斜率 189 MB/层，1,429 层即 270 GB。
     *
     * 设置本回调后每层交出即释放。返回 false 表示消费方要求中止（取消或异常），
     * adapter 应停止产层 —— 这条是防死锁的关键：屏障失效时生产者必须能退出。
     */
    std::function<bool(SceneInstanceRasterLayer&&)> layersink;

    /** @brief Synchronous, non-owning cancellation source for this adapter run. */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief Run the existing Legacy producer without file output.
 * @param request Config path and immutable admitted instance identity.
 * @return Complete instance raster or one structured fail-closed error.
 */
SceneRasterAdapterResult AdaptLegacySceneLayers(
    const LegacySceneLayerAdapterRequest& request);

}  // namespace slicer_core
