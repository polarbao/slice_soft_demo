#include "slicer_core/diagnostics/ValidationIssue.h"

namespace slicer_core
{

std::string ValidationSeverityName(const ValidationSeverity severity)
{
    switch (severity)
    {
    case ValidationSeverity::Info:
        return "info";
    case ValidationSeverity::Warning:
        return "warning";
    case ValidationSeverity::Error:
        return "error";
    }
    return "info";
}

ValidationIssue MakeValidationIssue(
    const std::string& code,
    const ValidationSeverity severity,
    const std::string& message)
{
    ValidationIssue issue;
    issue.code = code;
    issue.severity = severity;
    issue.message = message;
    issue.context = Json::object({});
    return issue;
}

Json ValidationIssuesToJson(const std::vector<ValidationIssue>& issues)
{
    Json::Array array;
    for (const ValidationIssue& issue : issues)
    {
        array.push_back(Json::object({
            {"code", issue.code},
            {"severity", ValidationSeverityName(issue.severity)},
            {"message", issue.message},
            {"context", issue.context},
        }));
    }
    return Json{array};
}

Json ValidationIssueCodesToJson(
    const std::vector<ValidationIssue>& issues,
    const ValidationSeverity severity)
{
    Json::Array array;
    for (const ValidationIssue& issue : issues)
    {
        if (issue.severity == severity)
        {
            array.push_back(issue.code);
        }
    }
    return Json{array};
}

}  // namespace slicer_core
