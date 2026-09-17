// 切片进度回调与阶段计时的实现（F-09 第 6 步从 slicer.cpp 搬出）。
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。

#include "slicer_core/pipeline/SliceProgressNotifier.h"

#include <algorithm>
#include <stdexcept>

namespace slicer_core::progress {

double ElapsedMsSince(const SlicerClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(SlicerClock::now() - start).count();
}

void NotifyProgress(
    const SliceRunOptions& options,
    const SlicerClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent)
{
    if (options.cancellation_requested
        && options.cancellation_requested())
    {
        throw std::runtime_error("cooperative slice cancellation requested");
    }
    if (!options.progress_callback)
    {
        return;
    }

    options.progress_callback(SliceRunProgress{
        phase,
        current,
        total,
        std::clamp(percent, 0, 100),
        ElapsedMsSince(runStart)});
}

bool ShouldNotifyLayerProgress(const int completedLayers, const int layerCount)
{
    if (completedLayers <= 1 || completedLayers >= layerCount)
    {
        return true;
    }

    const int interval = std::max(1, (layerCount + 99) / 100);
    return completedLayers % interval == 0;
}

}  // namespace slicer_core::progress
