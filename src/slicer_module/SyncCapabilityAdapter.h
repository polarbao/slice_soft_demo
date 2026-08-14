#pragma once

#include "contracts/print_module_spi.h"
#include "slicer_module/CapabilityJsonAdapter.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace slicesoft::module
{

/** @brief DEV_14 第 5 节冻结的进程内能力 ID。 */
inline constexpr std::array<std::string_view, 13> SyncCapabilities{
    "model.import",
    "model.get_metadata",
    "model.release",
    "scene.apply_operation",
    "scene.get_snapshot",
    "scene.get_viewdata",
    "geometry.preflight",
    "geometry.collision",
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report"};

/** @brief 验证并执行一个提交请求的结果。 */
struct CapabilitySubmission
{
    bool accepted{false};
    std::string capability;
    std::string errorcode;
    std::string errormessage;
    std::string errordetail;
    CapabilityOutput output;
};

/** @brief 同步轻量能力的进程内路由器和终态结果存储。 */
class SyncCapabilityAdapter final
{
public:
    /** @brief 返回进程级适配器。 @return 共享适配器实例。 */
    [[nodiscard]] static SyncCapabilityAdapter& Instance();

    /** @brief 注册一个有效模块。 @param module 不透明模块句柄。 @return 就绪时返回 true。 */
    [[nodiscard]] bool RegisterModule(pm_module_t* module);

    /** @brief 移除模块及其保留的所有终态输出。 @param module 不透明模块句柄。 */
    void RemoveModule(pm_module_t* module) noexcept;

    /** @brief 验证载体路由并执行同步请求。 @param module 所有者模块。 @param requestText UTF-8 请求。 @return 提交结果。 */
    [[nodiscard]] CapabilitySubmission Execute(
        pm_module_t* module,
        std::string_view requestText);

    /** @brief 存储已进入终态的作业输出。 @param job 作业句柄。 @param module 所有者。 @param submission 已接受输出。 @return 保留成功时返回 true。 */
    [[nodiscard]] bool StoreJob(
        pm_job_t* job,
        pm_module_t* module,
        CapabilitySubmission submission);

    /** @brief 获取不可变的终态进度快照。 @param job 作业句柄。 @return 进度 JSON 或空字符串。 */
    [[nodiscard]] std::string Poll(pm_job_t* job) const;

    /** @brief 获取不可变的终态结果字节。 @param job 作业句柄。 @return 输出或空指针。 */
    [[nodiscard]] std::shared_ptr<const CapabilityOutput> Result(
        pm_job_t* job) const;

    /** @brief 移除一个已释放作业的保留输出。 @param job 作业句柄。 */
    void ReleaseJob(pm_job_t* job) noexcept;

private:
    struct Implementation;

    SyncCapabilityAdapter();
    ~SyncCapabilityAdapter();

    SyncCapabilityAdapter(const SyncCapabilityAdapter&) = delete;
    SyncCapabilityAdapter& operator=(const SyncCapabilityAdapter&) = delete;

    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
