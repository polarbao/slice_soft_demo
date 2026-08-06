#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

namespace slicesoft::module
{

/**
 * @brief Process initialization state used inside the module DLL.
 */
enum class ModuleInitializationState
{
    Uninitialized,
    Initialized,
    Failed
};

/**
 * @brief No-throw action invoked once by ModuleInitialization.
 * @return True when the process infrastructure is ready.
 */
using ModuleInitializationAction = bool (*)() noexcept;

/**
 * @brief Owns one call-once initialization boundary.
 *
 * The class is internal to slicer_module. It deliberately owns no Worker,
 * engine, thread, file, or module-handle state.
 */
class ModuleInitialization final
{
public:
    /**
     * @brief Creates an initialization boundary around an action.
     * @param action No-throw process initialization action.
     */
    explicit ModuleInitialization(ModuleInitializationAction action) noexcept;

    /**
     * @brief Ensures the action has run once for this boundary.
     * @return True only when initialization completed successfully.
     */
    [[nodiscard]] bool EnsureInitialized() noexcept;

    /**
     * @brief Returns the terminal or pending initialization state.
     * @return Current initialization state.
     */
    [[nodiscard]] ModuleInitializationState GetState() const noexcept;

    /**
     * @brief Returns how many times the protected action was invoked.
     * @return Zero before initialization and one after its first attempt.
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
 * @brief Ensures slicer_module process infrastructure is initialized once.
 * @return True when the process infrastructure is ready.
 */
[[nodiscard]] bool EnsureProcessModuleInitialized() noexcept;

}  // namespace slicesoft::module
