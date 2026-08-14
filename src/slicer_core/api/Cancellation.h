#pragma once

#include <functional>
#include <string>

namespace slicer_core::api {

/** @brief 长耗时 Facade 调用所需的协作式取消源。 */
class ICancelToken
{
public:
    virtual ~ICancelToken() = default;

    /** @brief 检查是否已请求取消。 @return 工作应停止时返回 true。 */
    [[nodiscard]] virtual bool IsCancelRequested() const noexcept = 0;
};

/** @brief Worker 与进程内 Facade 调用方共享的进度事件。 */
struct ProgressEvent
{
    std::string stage;
    int percent{0};
    int layers_done{0};
    int layers_total{0};
    std::string current_instance_id;
};

/** @brief 接收 Facade 单调递进进度事件的回调。 */
using ProgressSink = std::function<void(const ProgressEvent&)>;

}  // namespace slicer_core::api
