#include "slicer_core/output/rgbwsv/RgbwsvSceneExtension.h"

#include <exception>
#include <string>

namespace slicer_core
{
namespace
{

constexpr std::string_view kReportSchema{
    "slicesoft.multimodel_scene_report.13b.1"};
constexpr std::string_view kSummarySchema{
    "slicesoft.multimodel_scene_summary.13b.1"};

bool HasMatchingString(
    const Json& first,
    const Json& second,
    const std::string& key)
{
    return first.contains(key)
        && second.contains(key)
        && first.at(key).is_string()
        && second.at(key).is_string()
        && first.at(key).as_string()
            == second.at(key).as_string();
}

bool HasMatchingNumber(
    const Json& first,
    const Json& second,
    const std::string& key)
{
    return first.contains(key)
        && second.contains(key)
        && first.at(key).is_number()
        && second.at(key).is_number()
        && first.at(key).as_double()
            == second.at(key).as_double();
}

}  // namespace

bool MultiModelSceneReportDocument::IsValid() const
{
    try
    {
        if (!manifestsummary.is_object()
            || !report.is_object()
            || manifestsummary.at("schema").as_string()
                != MultiModelSceneSummarySchemaName()
            || report.at("schema").as_string()
                != MultiModelSceneReportSchemaName()
            || !HasMatchingString(
                manifestsummary,
                report,
                "sceneId")
            || !HasMatchingNumber(
                manifestsummary,
                report,
                "sceneRevision")
            || !HasMatchingString(
                manifestsummary,
                report,
                "sceneHash")
            || manifestsummary.at("productionReady").as_bool()
                != report.at("productionReady").as_bool()
            || manifestsummary.at("sceneReport").as_string()
                != MultiModelSceneReportRelativePath().generic_string()
            || !report.at("productionOutputWritten").as_bool()
            || report.at("status").as_string()
                != (manifestsummary.at("productionReady").as_bool()
                        ? "production_format_written"
                        : "functional_fixture_format_written")
            || !report.at("models").is_array()
            || !report.at("instances").is_array()
            || manifestsummary.at("modelCount").as_int()
                != static_cast<int>(report.at("models").size())
            || manifestsummary.at("instanceCount").as_int()
                != static_cast<int>(report.at("instances").size())
            || report.at("modelCount").as_int()
                != manifestsummary.at("modelCount").as_int()
            || report.at("instanceCount").as_int()
                != manifestsummary.at("instanceCount").as_int()
            || report.at("visibleInstanceCount").as_int()
                != manifestsummary.at("visibleInstanceCount").as_int())
        {
            return false;
        }
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::string_view MultiModelSceneReportSchemaName()
{
    return kReportSchema;
}

std::string_view MultiModelSceneSummarySchemaName()
{
    return kSummarySchema;
}

std::filesystem::path MultiModelSceneReportRelativePath()
{
    return std::filesystem::path{
        "reports/multimodel_scene_report.json"};
}

}  // namespace slicer_core
