#include "slicer_core/reports/MultiModelSceneMatrixReport.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace slicer_core
{
namespace
{

bool IsNonNegativeFinite(const double value)
{
    return std::isfinite(value) && value >= 0.0;
}

Json StringsToJson(const std::vector<std::string>& values)
{
    Json::Array output;
    output.reserve(values.size());
    for (const std::string& value : values)
    {
        output.emplace_back(value);
    }
    return Json{std::move(output)};
}

Json TimingToJson(const MultiModelSceneMatrixTiming& timing)
{
    return Json::object({
        {"importMs", timing.importms},
        {"layoutMs", timing.layoutms},
        {"preflightAdmissionMs", timing.preflightadmissionms},
        {"sliceMs", timing.slicems},
        {"composeMs", timing.composems},
        {"tiffAndReportWriteMs", timing.tiffandreportwritems},
        {"ripValidationMs", timing.ripvalidationms},
        {"totalMs", timing.totalms},
    });
}

Json CaseToJson(const MultiModelSceneMatrixCase& item)
{
    const double reuseRatio =
        item.instancecount > 0
        ? static_cast<double>(item.reusedinstancecount)
            / static_cast<double>(item.instancecount)
        : 0.0;
    return Json::object({
        {"caseId", item.caseid},
        {"category", item.category},
        {"status", item.status},
        {"expectedPass", item.expectedpass},
        {"passed", item.passed},
        {"instanceCount", item.instancecount},
        {"uniqueModelCount", item.uniquemodelcount},
        {"sliceProducerInvocationCount",
         item.sliceproducerinvocationcount},
        {"reusedInstanceCount", item.reusedinstancecount},
        {"resourceReuseRatio", reuseRatio},
        {"formats", StringsToJson(item.formats)},
        {"modelIds", StringsToJson(item.modelids)},
        {"grid",
         Json::object({
             {"widthPx", item.widthpx},
             {"heightPx", item.heightpx},
             {"layerCount", item.layercount},
         })},
        {"package",
         Json::object({
             {"written", item.packagewritten},
             {"ripStrictPass", item.ripstrictpass},
             {"bytes", item.packagebytes},
             {"path", item.packagedir.generic_string()},
         })},
        {"peakWorkingSetBytes", item.peakworkingsetbytes},
        {"errorCode", item.errorcode},
        {"message", item.message},
        {"timing", TimingToJson(item.timing)},
    });
}

bool HasValidTiming(const MultiModelSceneMatrixTiming& timing)
{
    return IsNonNegativeFinite(timing.importms)
        && IsNonNegativeFinite(timing.layoutms)
        && IsNonNegativeFinite(timing.preflightadmissionms)
        && IsNonNegativeFinite(timing.slicems)
        && IsNonNegativeFinite(timing.composems)
        && IsNonNegativeFinite(timing.tiffandreportwritems)
        && IsNonNegativeFinite(timing.ripvalidationms)
        && IsNonNegativeFinite(timing.totalms);
}

}  // namespace

bool ValidateMultiModelSceneMatrixReport(
    const MultiModelSceneMatrixReport& report)
{
    if (report.schema
            != "slicesoft.multimodel_scene_matrix.13b.1"
        || (report.status != "passed"
            && report.status != "blocked")
        || report.buildconfig.empty()
        || report.compiler.empty()
        || report.cases.empty()
        || report.productiongo
        || report.productionstatus != "INPUT_OPEN"
        || report.productionblockers.empty())
    {
        return false;
    }

    std::set<std::string> caseIds;
    bool allPassed{true};
    for (const MultiModelSceneMatrixCase& item : report.cases)
    {
        if (item.caseid.empty()
            || !caseIds.insert(item.caseid).second
            || (item.category != "positive"
                && item.category != "negative")
            || (item.status != "passed"
                && item.status != "blocked")
            || item.instancecount < 0
            || item.uniquemodelcount < 0
            || item.sliceproducerinvocationcount < 0
            || item.reusedinstancecount < 0
            || item.reusedinstancecount > item.instancecount
            || !HasValidTiming(item.timing))
        {
            return false;
        }
        const bool expectedOutcome =
            item.expectedpass ? item.passed : !item.passed;
        if (!expectedOutcome)
        {
            allPassed = false;
        }
        if (item.category == "positive"
            && item.passed
            && (!item.packagewritten
                || !item.ripstrictpass
                || item.packagedir.empty()
                || item.widthpx <= 0
                || item.heightpx <= 0
                || item.layercount <= 0))
        {
            return false;
        }
        if (item.category == "negative"
            && (!item.errorcode.empty() || !item.passed)
            && item.packagewritten)
        {
            return false;
        }
    }

    return report.functionalmatrixpass == allPassed
        && report.status
            == (report.functionalmatrixpass
                    ? "passed"
                    : "blocked");
}

Json SerializeMultiModelSceneMatrixReport(
    const MultiModelSceneMatrixReport& report)
{
    if (!ValidateMultiModelSceneMatrixReport(report))
    {
        throw std::invalid_argument(
            "multi-model scene matrix report is invalid");
    }

    Json::Array cases;
    cases.reserve(report.cases.size());
    for (const MultiModelSceneMatrixCase& item : report.cases)
    {
        cases.push_back(CaseToJson(item));
    }
    return Json::object({
        {"schema", report.schema},
        {"status", report.status},
        {"functionalMatrixPass", report.functionalmatrixpass},
        {"productionGo", report.productiongo},
        {"productionStatus", report.productionstatus},
        {"productionBlockers",
         StringsToJson(report.productionblockers)},
        {"knownCoverageGaps",
         StringsToJson(report.knowncoveragegaps)},
        {"environment",
         Json::object({
             {"buildConfig", report.buildconfig},
             {"compiler", report.compiler},
         })},
        {"fixedProtocol",
         Json::object({
             {"schema", "p0.rgbwsv.2"},
             {"channelOrder",
              Json::array({"R", "G", "B", "W", "S", "V"})},
             {"bitDepth", 8},
             {"polarity", "black_is_print"},
             {"printValue", 0},
             {"emptyValue", 255},
         })},
        {"cases", Json{std::move(cases)}},
    });
}

std::string RenderMultiModelSceneMatrixMarkdown(
    const MultiModelSceneMatrixReport& report)
{
    if (!ValidateMultiModelSceneMatrixReport(report))
    {
        throw std::invalid_argument(
            "multi-model scene matrix report is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "# Stage 13B-07 真实模型功能矩阵\n\n";
    output << "- 功能矩阵："
           << (report.functionalmatrixpass ? "PASS" : "FAIL")
           << "\n";
    output << "- Production：INPUT_OPEN\n";
    output << "- 构建：" << report.buildconfig
           << " / " << report.compiler << "\n\n";
    output << "| Case | 类型 | 实例 | 生产器 | 复用 | Grid | RIP | 总耗时 ms | 结果 |\n";
    output << "|---|---|---:|---:|---:|---|---|---:|---|\n";
    output << std::fixed << std::setprecision(2);
    for (const MultiModelSceneMatrixCase& item : report.cases)
    {
        output << "| " << item.caseid
               << " | " << item.category
               << " | " << item.instancecount
               << " | " << item.sliceproducerinvocationcount
               << " | " << item.reusedinstancecount
               << " | " << item.widthpx << "x" << item.heightpx
               << "x" << item.layercount
               << " | " << (item.ripstrictpass ? "PASS" : "-")
               << " | " << item.timing.totalms
               << " | " << item.status << " |\n";
    }
    output << "\n## Production Blockers\n\n";
    for (const std::string& blocker : report.productionblockers)
    {
        output << "- " << blocker << "\n";
    }
    output << "\n## 已知覆盖缺口\n\n";
    for (const std::string& gap : report.knowncoveragegaps)
    {
        output << "- " << gap << "\n";
    }
    return output.str();
}

}  // namespace slicer_core

