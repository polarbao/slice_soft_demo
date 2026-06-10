#include "slicer_core/reports/ReportBase.h"

namespace slicer_core
{
namespace
{

Json::Object CopyObject(const Json& report)
{
    return report.as_object();
}

Json::Array CopyArrayField(const Json::Object& object, const std::string& key)
{
    const auto found = object.find(key);
    if (found == object.end())
    {
        return {};
    }
    return found->second.as_array();
}

Json::Object CopyObjectField(const Json::Object& object, const std::string& key)
{
    const auto found = object.find(key);
    if (found == object.end())
    {
        return {};
    }
    return found->second.as_object();
}

}  // namespace

Json MakeReportBase(const std::string& schema, const Json& source, const Json& configSnapshot)
{
    return Json::object({
        {"schema", schema},
        {"source", source},
        {"configSnapshot", configSnapshot},
        {"stats", Json::object({})},
        {"warnings", Json::array({})},
        {"errors", Json::array({})},
        {"timings", Json::object({})},
    });
}

Json AppendReportWarning(const Json& report, const std::string& warning)
{
    Json::Object object = CopyObject(report);
    Json::Array warnings = CopyArrayField(object, "warnings");
    warnings.emplace_back(warning);
    object["warnings"] = Json{std::move(warnings)};
    return Json{std::move(object)};
}

Json AppendReportError(const Json& report, const std::string& error)
{
    Json::Object object = CopyObject(report);
    Json::Array errors = CopyArrayField(object, "errors");
    errors.emplace_back(error);
    object["errors"] = Json{std::move(errors)};
    return Json{std::move(object)};
}

Json AppendReportTiming(const Json& report, const std::string& name, const double milliseconds)
{
    Json::Object object = CopyObject(report);
    Json::Object timings = CopyObjectField(object, "timings");
    timings[name] = Json{milliseconds};
    object["timings"] = Json{std::move(timings)};
    return Json{std::move(object)};
}

}  // namespace slicer_core
