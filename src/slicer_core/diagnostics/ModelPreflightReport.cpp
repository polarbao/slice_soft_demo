#include "slicer_core/diagnostics/ModelPreflightReport.h"

#include "slicer_core/preflight/ModelPreflightCacheIdentity.h"

#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

Json StringsToJson(const std::vector<std::string>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.emplace_back(value);
    }
    return Json{std::move(array)};
}

Json BuildIdentity(const ModelPreflightCacheIdentity& identity)
{
    return Json::object({
        {"sourceHash", identity.sourceHash},
        {"resourceHash", identity.resourceHash},
        {"transformHash", identity.transformHash},
        {"optionsHash", identity.optionsHash},
        {"algorithmVersion", identity.algorithmVersion},
    });
}

Json BuildAdmission(const ModeAdmissionResult& admission)
{
    return Json::object({
        {"mode", ModelPreflightPipelineModeName(admission.mode)},
        {"status", ModelPreflightAdmissionStatusName(admission.status)},
        {"blockerCodes", StringsToJson(admission.blockerCodes)},
        {"warningCodes", StringsToJson(admission.warningCodes)},
    });
}

Json BuildIssues(const std::vector<ModelPreflightIssue>& issues)
{
    Json::Array array;
    for (const ModelPreflightIssue& issue : issues)
    {
        array.push_back(Json::object({
            {"code", issue.code},
            {"category", issue.category},
            {"severity", ModelPreflightIssueSeverityName(issue.severity)},
            {"count", issue.count},
            {"summaryKey", issue.summaryKey},
            {"recommendationKey", issue.recommendationKey},
            {"context", issue.context},
        }));
    }
    return Json{std::move(array)};
}

}  // namespace

Json BuildModelPreflightReportSkeleton(
    const ModelPreflightCacheIdentity& identity)
{
    ModelPreflightResult result;
    result.identity = identity;
    result.cacheKey = ComputeModelPreflightCacheKey(identity);
    const std::string notRunCode =
        ModelPreflightErrorCodeName(ModelPreflightErrorCode::NotRun);
    result.legacyAdmission.blockerCodes = {notRunCode};
    result.globalAdmission.blockerCodes = {notRunCode};
    return BuildModelPreflightReport(result);
}

Json BuildModelPreflightReport(const ModelPreflightResult& result)
{
    return Json::object({
        {"schema", "slicesoft.model_preflight.12e_08c_r4.1"},
        {"status", ModelPreflightStatusName(result.status)},
        {"cacheKey", result.cacheKey},
        {"identity", BuildIdentity(result.identity)},
        {"legacyAdmission", BuildAdmission(result.legacyAdmission)},
        {"globalAdmission", BuildAdmission(result.globalAdmission)},
        {"productionOutputWritten", result.productionOutputWritten},
        {"issues", BuildIssues(result.issues)},
    });
}

}  // namespace slicer_core
