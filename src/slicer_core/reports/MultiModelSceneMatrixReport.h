#pragma once

#include "slicer_core/json_value.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Monotonic phase timings for one Stage 13B matrix case.
 */
struct MultiModelSceneMatrixTiming
{
    double importms{0.0};
    double layoutms{0.0};
    double preflightadmissionms{0.0};
    double slicems{0.0};
    double composems{0.0};
    double tiffandreportwritems{0.0};
    double ripvalidationms{0.0};
    double totalms{0.0};
};

/**
 * @brief One positive or negative Stage 13B matrix result.
 */
struct MultiModelSceneMatrixCase
{
    std::string caseid;
    std::string category;
    std::string status;
    bool expectedpass{true};
    bool passed{false};
    int instancecount{0};
    int uniquemodelcount{0};
    int sliceproducerinvocationcount{0};
    int reusedinstancecount{0};
    int widthpx{0};
    int heightpx{0};
    int layercount{0};
    bool packagewritten{false};
    bool ripstrictpass{false};
    std::uint64_t packagebytes{0U};
    std::uint64_t peakworkingsetbytes{0U};
    std::filesystem::path packagedir;
    std::vector<std::string> formats;
    std::vector<std::string> modelids;
    std::string errorcode;
    std::string message;
    MultiModelSceneMatrixTiming timing;
};

/**
 * @brief Complete machine-readable Stage 13B functional matrix report.
 */
struct MultiModelSceneMatrixReport
{
    std::string schema{
        "slicesoft.multimodel_scene_matrix.13b.1"};
    std::string status{"blocked"};
    std::string buildconfig;
    std::string compiler;
    bool functionalmatrixpass{false};
    bool productiongo{false};
    std::string productionstatus{"INPUT_OPEN"};
    std::vector<std::string> productionblockers;
    std::vector<std::string> knowncoveragegaps;
    std::vector<MultiModelSceneMatrixCase> cases;
};

/**
 * @brief Validate Stage 13B report identity, cases, and production safety.
 * @param report Matrix report to validate.
 * @return True when the report is complete and internally consistent.
 */
bool ValidateMultiModelSceneMatrixReport(
    const MultiModelSceneMatrixReport& report);

/**
 * @brief Serialize a Stage 13B matrix report to stable JSON.
 * @param report Validated report.
 * @return Canonically ordered JSON document.
 * @throws std::invalid_argument when the report is inconsistent.
 */
Json SerializeMultiModelSceneMatrixReport(
    const MultiModelSceneMatrixReport& report);

/**
 * @brief Render a concise human-readable Stage 13B matrix table.
 * @param report Validated report.
 * @return Markdown report.
 * @throws std::invalid_argument when the report is inconsistent.
 */
std::string RenderMultiModelSceneMatrixMarkdown(
    const MultiModelSceneMatrixReport& report);

}  // namespace slicer_core

