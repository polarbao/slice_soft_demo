#pragma once

#include "slicer_core/json_value.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Severity for a diagnostic message.
 */
enum class DiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Fatal,
};

/**
 * @brief Single diagnostic message emitted by a pipeline boundary.
 */
struct DiagnosticMessage
{
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    std::string code;
    std::string message;
    std::string source;
    Json context;
};

using Diagnostic = DiagnosticMessage;

/**
 * @brief Diagnostic message collection for pipeline and wrapper APIs.
 */
struct Diagnostics
{
    std::vector<DiagnosticMessage> messages;

    /**
     * @brief Check whether the collection contains any error severity message.
     * @return True when at least one message has error severity.
     */
    bool HasErrors() const;
};

}  // namespace slicer_core
