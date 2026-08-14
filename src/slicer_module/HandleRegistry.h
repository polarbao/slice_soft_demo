#pragma once

#include "contracts/print_module_spi.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace slicesoft::module
{

/**
 * @brief 模块 ABI 外壳共享的精简生命周期状态。
 */
enum class JobLifecycleState
{
    Queued,
    Running,
    Cancelling,
    Succeeded,
    Failed,
    Cancelled
};

/**
 * @brief 与一个不透明模块句柄关联的已验证状态。
 */
class ModuleHandleState final
{
public:
    /**
     * @brief 为注册表分配的模块标识创建状态。
     * @param id 注册表内的模块标识。
     */
    explicit ModuleHandleState(std::uint64_t id) noexcept;

    /**
     * @brief 返回注册表内的模块标识。
     * @return 单调递增的模块标识。
     */
    [[nodiscard]] std::uint64_t Id() const noexcept;

    /**
     * @brief 报告句柄是否仍在注册表中。
     * @return 模块句柄有效时返回 true。
     */
    [[nodiscard]] bool IsActive() const noexcept;

private:
    void Deactivate() noexcept;

    std::uint64_t m_id{0U};
    std::atomic_bool m_active{true};

    friend class HandleRegistry;
};

/**
 * @brief 与一个不透明作业句柄关联的已验证状态。
 */
class JobHandleState final
{
public:
    /**
     * @brief 为注册表分配的作业及其所有者标识创建状态。
     * @param id 注册表内的作业标识。
     * @param ownerModuleId 注册表内的所有者模块标识。
     */
    JobHandleState(std::uint64_t id, std::uint64_t ownerModuleId) noexcept;

    /**
     * @brief 返回注册表内的作业标识。
     * @return 单调递增的作业标识。
     */
    [[nodiscard]] std::uint64_t Id() const noexcept;

    /**
     * @brief 返回所属模块的标识。
     * @return 注册表内的模块标识。
     */
    [[nodiscard]] std::uint64_t OwnerModuleId() const noexcept;

    /**
     * @brief 报告作业句柄是否仍在注册表中。
     * @return 作业句柄有效时返回 true。
     */
    [[nodiscard]] bool IsActive() const noexcept;

    /**
     * @brief 返回当前精简作业生命周期状态。
     * @return 当前作业生命周期状态。
     */
    [[nodiscard]] JobLifecycleState LifecycleState() const noexcept;

    /**
     * @brief 报告是否已请求协作式取消。
     * @return 首次接受取消请求后返回 true。
     */
    [[nodiscard]] bool IsCancellationRequested() const noexcept;

private:
    void Deactivate() noexcept;
    void ForceCancelled() noexcept;
    bool RequestCancel() noexcept;
    bool SetLifecycleState(JobLifecycleState state) noexcept;

    std::uint64_t m_id{0U};
    std::uint64_t m_ownerModuleId{0U};
    std::atomic_bool m_active{true};
    std::atomic_bool m_cancellationRequested{false};
    std::atomic<JobLifecycleState> m_lifecycleState{JobLifecycleState::Queued};

    friend class HandleRegistry;
};

/**
 * @brief 持有并验证不透明模块句柄和作业句柄。
 *
 * 查找有效注册项之前绝不解引用句柄令牌。注册表销毁前始终独占已退役令牌，
 * 避免失效指针因分配器复用地址而再次变成有效句柄。
 */
class HandleRegistry final
{
public:
    /**
     * @brief 创建空注册表。
     */
    HandleRegistry();

    /**
     * @brief 释放所有有效状态和保留的不透明令牌。
     */
    ~HandleRegistry();

    HandleRegistry(const HandleRegistry&) = delete;
    HandleRegistry& operator=(const HandleRegistry&) = delete;
    HandleRegistry(HandleRegistry&&) = delete;
    HandleRegistry& operator=(HandleRegistry&&) = delete;

    /**
     * @brief 返回导出 SPI 函数使用的进程级注册表。
     * @return 进程级注册表实例。
     */
    [[nodiscard]] static HandleRegistry& Instance();

    /**
     * @brief 创建并注册一个不透明模块句柄。
     * @return 有效模块句柄。
     * @throws std::bad_alloc 注册表存储分配失败时抛出。
     */
    [[nodiscard]] pm_module_t* CreateModule();

    /**
     * @brief 销毁模块并退役其仍持有的所有作业。
     * @param module 不透明模块句柄；nullptr 视为成功的空操作。
     * @return 有效句柄或 nullptr 返回 true，失效句柄或非本注册表句柄返回 false。
     */
    [[nodiscard]] bool DestroyModule(pm_module_t* module);

    /**
     * @brief 为有效模块创建一个排队中的作业。
     * @param module 持有新作业的有效模块句柄。
     * @return 有效作业句柄；所属模块无效时返回 nullptr。
     * @throws std::bad_alloc 注册表存储分配失败时抛出。
     */
    [[nodiscard]] pm_job_t* CreateJob(pm_module_t* module);

    /**
     * @brief 释放作业，并将尚未结束的精简生命周期状态强制置为已取消。
     * @param job 不透明作业句柄；nullptr 视为成功的空操作。
     * @return 有效句柄或 nullptr 返回 true，失效句柄或非本注册表句柄返回 false。
     */
    [[nodiscard]] bool ReleaseJob(pm_job_t* job);

    /**
     * @brief 不解引用调用方内存，解析有效模块句柄。
     * @param module 不透明模块句柄。
     * @return 有效句柄的共享状态，否则返回 nullptr。
     */
    [[nodiscard]] std::shared_ptr<const ModuleHandleState> FindModule(
        pm_module_t* module) const;

    /**
     * @brief 不解引用调用方内存，解析有效作业句柄。
     * @param job 不透明作业句柄。
     * @return 有效句柄的共享状态，否则返回 nullptr。
     */
    [[nodiscard]] std::shared_ptr<const JobHandleState> FindJob(
        pm_job_t* job) const;

    /**
     * @brief 仅当作业属于指定模块时解析该有效作业。
     * @param module 预期的所有者模块句柄。
     * @param job 不透明作业句柄。
     * @return 匹配且有效的句柄对所对应的共享状态，否则返回 nullptr。
     */
    [[nodiscard]] std::shared_ptr<const JobHandleState> FindJob(
        pm_module_t* module,
        pm_job_t* job) const;

    /**
     * @brief 对有效作业请求幂等的协作式取消。
     * @param job 不透明作业句柄。
     * @return 句柄有效时返回 true，包括已经结束的作业。
     */
    [[nodiscard]] bool RequestCancel(pm_job_t* job);

    /**
     * @brief 推进有效作业的精简生命周期状态机。
     * @param job 不透明作业句柄。
     * @param state 请求进入的生命周期状态。
     * @return 状态转换有效并被接受时返回 true。
     */
    [[nodiscard]] bool SetJobLifecycleState(
        pm_job_t* job,
        JobLifecycleState state);

    /**
     * @brief 返回当前有效模块句柄数量。
     * @return 有效模块数量。
     */
    [[nodiscard]] std::size_t ActiveModuleCount() const;

    /**
     * @brief 返回当前有效作业句柄数量。
     * @return 有效作业数量。
     */
    [[nodiscard]] std::size_t ActiveJobCount() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
