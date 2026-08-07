#include "ViewModeSwitch.h"

ViewModeSwitch::ViewModeSwitch(const HostViewMode initialMode)
    : m_mode(initialMode)
{
}

void ViewModeSwitch::SetMode(const HostViewMode mode)
{
    m_mode = mode;
}

HostViewMode ViewModeSwitch::Mode() const
{
    return m_mode;
}
