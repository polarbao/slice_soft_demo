#pragma once

/** @brief Host-local presentation modes allowed by the frozen UI contract. */
enum class HostViewMode
{
    Top,
    ThreeD
};

/**
 * @brief Switches presentation mode without owning or mutating scene state.
 */
class ViewModeSwitch final
{
public:
    /** @brief Creates a switch with the supplied initial mode. */
    explicit ViewModeSwitch(HostViewMode initialMode = HostViewMode::Top);

    /** @brief Selects the current local presentation mode. */
    void SetMode(HostViewMode mode);

    /** @brief Returns the current local presentation mode. */
    [[nodiscard]] HostViewMode Mode() const;

private:
    HostViewMode m_mode{HostViewMode::Top};
};
