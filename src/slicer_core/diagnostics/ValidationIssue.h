#pragma once

#include "slicer_core/json_value.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable validation severity used by experimental and production-readiness reports.
 */
enum class ValidationSeverity
{
    Info,
    Warning,
    Error,
};

/**
 * @brief Stable validation issue with machine-readable code and human-readable message.
 */
struct ValidationIssue
{
    std::string code;
    ValidationSeverity severity{ValidationSeverity::Info};
    std::string message;
    Json context;
};

/**
 * @brief Convert severity to report text.
 * @param severity Validation severity.
 * @return Stable severity text.
 */
std::string ValidationSeverityName(ValidationSeverity severity);

/**
 * @brief Create a validation issue without additional context.
 * @param code Stable issue code.
 * @param severity Issue severity.
 * @param message Human-readable message.
 * @return Validation issue.
 */
ValidationIssue MakeValidationIssue(
    const std::string& code,
    ValidationSeverity severity,
    const std::string& message);

/**
 * @brief Convert validation issues to JSON objects for reports.
 * @param issues Issues to serialize.
 * @return JSON array.
 */
Json ValidationIssuesToJson(const std::vector<ValidationIssue>& issues);

/**
 * @brief Extract stable codes by severity.
 * @param issues Issues to scan.
 * @param severity Severity to include.
 * @return JSON string array with stable codes.
 */
Json ValidationIssueCodesToJson(
    const std::vector<ValidationIssue>& issues,
    ValidationSeverity severity);

}  // namespace slicer_core
