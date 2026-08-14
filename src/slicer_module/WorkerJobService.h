#pragma once

#include "contracts/print_module_spi.h"
#include "slicer_module/CapabilityCarrierRouter.h"
#include "slicer_module/CapabilityJsonAdapter.h"

#include <cstdint>
#include <memory>
#include <string>

namespace slicesoft::module
{

/** @brief 将一个重型能力接入 Worker 载体的结果。 */
struct WorkerJobSubmission
{
    bool accepted{false};
    std::string errorCode;
    std::string errorMessage;
    std::string errorDetail;
};

/**
 * @brief 管理冻结公共 C SPI 背后的异步 Worker 作业。
 *
 * 本服务管理进程客户端、私有文件合同目录、缓存进度、终态结果、取消状态，
 * 并负责模块和作业清理；它绝不在进程内链接或调用切片引擎。
 */
class WorkerJobService final
{
public:
    /** @brief 返回进程级 Worker 作业服务。 @return 共享服务。 */
    [[nodiscard]] static WorkerJobService& Instance();

    /**
     * @brief 接受已验证的 Worker 路由并异步启动。
     * @param job 有效的公共 SPI 作业句柄。
     * @param module 持有该作业的有效模块句柄。
     * @param route 已验证的 Worker 载体路由。
     * @param moduleId 注册表内的模块标识。
     * @param jobId 注册表内的作业标识。
     * @return 接受结果；若失败，则尚未创建纳入服务管理的后台线程。
     */
    [[nodiscard]] WorkerJobSubmission Submit(
        pm_job_t* job,
        pm_module_t* module,
        CapabilityRoute route,
        std::uint64_t moduleId,
        std::uint64_t jobId);

    /** @brief 报告服务是否仍持有该 Worker 作业。 @param job 作业句柄。 @return 服务仍持有时返回 true。 */
    [[nodiscard]] bool HasJob(pm_job_t* job) const;

    /** @brief 返回缓存的进度 JSON。 @param job 作业句柄。 @return 进度或空字符串。 */
    [[nodiscard]] std::string Poll(pm_job_t* job) const;

    /** @brief 返回公共 SPI 终态结果字节。 @param job 作业句柄。 @return 结果；终态前为空。 */
    [[nodiscard]] std::shared_ptr<const CapabilityOutput> Result(
        pm_job_t* job) const;

    /** @brief 请求协作式取消。 @param job 作业句柄。 @return 作业已保留时返回 true。 */
    bool RequestCancel(pm_job_t* job) noexcept;

    /** @brief 取消、等待并移除一个 Worker 作业。 @param job 作业句柄。 */
    void ReleaseJob(pm_job_t* job) noexcept;

    /** @brief 取消、等待并移除模块持有的所有 Worker 作业。 @param module 模块句柄。 */
    void RemoveModule(pm_module_t* module) noexcept;

private:
    struct Implementation;

    WorkerJobService();
    ~WorkerJobService();

    WorkerJobService(const WorkerJobService&) = delete;
    WorkerJobService& operator=(const WorkerJobService&) = delete;

    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
