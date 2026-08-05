#pragma once

#include <functional>
#include <string>

namespace slicer_core::api {

/** @brief Cooperative cancellation source required by long-running facades. */
class ICancelToken
{
public:
    virtual ~ICancelToken() = default;

    /** @brief Tests whether cancellation was requested. @return True when work should stop. */
    [[nodiscard]] virtual bool IsCancelRequested() const noexcept = 0;
};

/** @brief Progress event shared by Worker and in-process facade callers. */
struct ProgressEvent
{
    std::string stage;
    int percent{0};
    int layers_done{0};
    int layers_total{0};
    std::string current_instance_id;
};

/** @brief Callback receiving monotonic facade progress events. */
using ProgressSink = std::function<void(const ProgressEvent&)>;

}  // namespace slicer_core::api
