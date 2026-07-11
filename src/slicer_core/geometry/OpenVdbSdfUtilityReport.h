#pragma once

#include "slicer_core/geometry/GeometryKernelService.h"
#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Stable inputs for the diagnostic-only OpenVDB SDF utility report.
 *
 * The referenced geometry and topology results must remain alive for the
 * duration of BuildOpenVdbSdfUtilityReport. The report never owns or writes
 * production RGBWSV output.
 */
struct OpenVdbSdfUtilityReportInput
{
    std::string build_type{"Unknown"};
    std::string build_dir;
    std::string command_line;
    std::string model_path;
    std::string input_format;
    std::string admission_mode{"strict_closed"};
    double requested_shell_thickness_mm{0.10};
    const GeometryKernelResult* geometry_result{nullptr};
    const MeshTopologyReport* topology_report{nullptr};
};

/**
 * @brief Build the Stage 12B-R2 OpenVDB SDF utility diagnostic report.
 * @param input Build, input, geometry, and topology evidence.
 * @return JSON report using schema slicesoft.openvdb_sdf_utility.12b_r2.1.
 */
Json BuildOpenVdbSdfUtilityReport(const OpenVdbSdfUtilityReportInput& input);

}  // namespace slicer_core
