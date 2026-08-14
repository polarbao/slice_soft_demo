#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

namespace slicesoft::module
{

/**
 * @brief 模块 DLL 内部使用的进程初始化状态。
 */
enum class ModuleInitializationState
{
    Uninitialized,
    Initialized,
    Failed
};

/**
 * @brief 由 ModuleInitialization 调用一次且不抛异常的动作。
 * @return 进程基础设施就绪时返回 true。
 */
using ModuleInitializationAction = bool (*)() noexcept;

/**
 * @brief 封装一个仅执行一次的初始化边界。
 *
 * 此类仅供 slicer_module 内部使用，不持有 Worker、引擎、线程、
 * 文件或模块句柄状态。
 */
class ModuleInitialization final
{
public:
    /**
     * @brief 围绕一个动作创建初始化边界。
     * @param action 不抛异常的进程初始化动作。
     */
    explicit ModuleInitialization(ModuleInitializationAction action) noexcept;

    /**
     * @brief 确保此边界内的动作只运行一次。
     * @return 仅当初始化成功完成时返回 true。
     */
    [[nodiscard]] bool EnsureInitialized() noexcept;

    /**
     * @brief 返回未初始化、成功或失败状态。
     * @return 当前初始化状态。
     */
    [[nodiscard]] ModuleInitializationState GetState() const noexcept;

    /**
     * @brief 返回受保护动作的调用次数。
     * @return 初始化前为零，首次尝试后为一。
     */
    [[nodiscard]] std::size_t GetInvocationCount() const noexcept;

private:
    void InitializeOnce() noexcept;

    std::once_flag m_once;
    ModuleInitializationAction m_action{nullptr};
    std::atomic<ModuleInitializationState> m_state{
        ModuleInitializationState::Uninitialized};
    std::atomic_size_t m_invocationCount{0U};
};

/**
 * @brief 确保 slicer_module 进程基础设施仅初始化一次。
 * @return 进程基础设施就绪时返回 true。
 */
[[nodiscard]] bool EnsureProcessModuleInitialized() noexcept;

}  // namespace slicesoft::module
