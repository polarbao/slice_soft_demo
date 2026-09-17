#pragma once

// 切片进度回调与阶段计时（F-09 第 6 步从 slicer.cpp 搬出）。
//
// SlicerClock 放在 slicer_core 而非 progress 子命名空间：run_slicer 里有多处
// SlicerClock::now()，留在外层可使那些用法一字不改。
//
// 【已知的重复实现，本步刻意不合并】src/slicer_core/pipeline/OpenVdbCandidatePipeline.cpp
// 的匿名命名空间里有一套同名同形的 ElapsedMsSince / NotifyProgress /
// ShouldNotifyLayerProgress（时钟别名叫 PipelineClock），是这三个函数的第二份拷贝。
// 合并它们会改变该文件的可见行为面，不属于「只移位」的范围；本单元落在 pipeline/
// 下正是为了让那次合并有个现成的家。见 analysis/04 的 F-49。

#include "slicer_core/slicer.h"

#include <chrono>
#include <string>

namespace slicer_core {

using SlicerClock = std::chrono::steady_clock;

namespace progress {

double ElapsedMsSince(const SlicerClock::time_point& start);

void NotifyProgress(
    const SliceRunOptions& options,
    const SlicerClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent);

bool ShouldNotifyLayerProgress(const int completedLayers, const int layerCount);

}  // namespace progress

}  // namespace slicer_core
