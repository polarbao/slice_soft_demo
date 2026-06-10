#include "slicer_core/diagnostics/Diagnostics.h"

#include <algorithm>

namespace slicer_core
{

bool Diagnostics::HasErrors() const
{
    return std::any_of(messages.begin(), messages.end(), [](const DiagnosticMessage& message)
    {
        return message.severity == DiagnosticSeverity::Error;
    });
}

}  // namespace slicer_core
