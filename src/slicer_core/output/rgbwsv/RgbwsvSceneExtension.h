#pragma once

#include "slicer_core/json_value.h"

#include <filesystem>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Typed optional scene metadata published with one RGBWSV package.
 */
struct MultiModelSceneReportDocument
{
    Json manifestsummary;
    Json report;

    /**
     * @brief Validate schemas, identities, counts, and fixed report path.
     * @return True when the summary and report form one coherent document.
     */
    bool IsValid() const;
};

/**
 * @brief Return the fixed multi-model scene report schema.
 * @return Stable scene report schema name.
 */
std::string_view MultiModelSceneReportSchemaName();

/**
 * @brief Return the fixed manifest scene-summary schema.
 * @return Stable scene-summary schema name.
 */
std::string_view MultiModelSceneSummarySchemaName();

/**
 * @brief Return the fixed scene report path inside a package.
 * @return reports/multimodel_scene_report.json.
 */
std::filesystem::path MultiModelSceneReportRelativePath();

}  // namespace slicer_core
