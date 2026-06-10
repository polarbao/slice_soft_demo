#include "slicer_core/reports/ReportSchemaValidator.h"

namespace slicer_core
{

bool ValidateReportSchema(const Json& report, const std::string& expectedSchema, std::string& error)
{
    if (!report.contains("schema"))
    {
        error = "missing schema";
        return false;
    }
    if (report.at("schema").as_string() != expectedSchema)
    {
        error = "schema mismatch";
        return false;
    }
    return true;
}

bool ValidateReportBaseFields(const Json& report, std::string& error)
{
    for (const std::string field : {"schema", "source", "configSnapshot", "stats", "warnings", "errors", "timings"})
    {
        if (!report.contains(field))
        {
            error = "missing report base field: " + field;
            return false;
        }
    }
    return true;
}

}  // namespace slicer_core
