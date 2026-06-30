#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Production admission mode for OpenVDB experimental pipeline decisions.
 */
enum class AdmissionMode
{
    StrictClosed,
    WarnAndAttempt,
    DiagnosticOnly,
    RepairThenStrict,
};

/**
 * @brief Production admission result status.
 */
enum class AdmissionStatus
{
    ProductionAllowed,
    NonProductionOnly,
    DiagnosticOnly,
    FailFast,
};

/**
 * @brief Machine-readable production admission decision.
 */
struct ProductionAdmissionDecision
{
    AdmissionStatus status{AdmissionStatus::DiagnosticOnly};
    bool productionAllowed{false};
    bool nonProduction{true};
    std::vector<std::string> blockerCodes;
    std::vector<std::string> warningCodes;
    std::vector<std::string> suggestedActions;
};

/**
 * @brief Convert an admission mode to stable report text.
 * @param mode Admission mode.
 * @return Stable snake_case text.
 */
std::string AdmissionModeName(AdmissionMode mode);

/**
 * @brief Convert an admission status to stable report text.
 * @param status Admission status.
 * @return Stable snake_case text.
 */
std::string AdmissionStatusName(AdmissionStatus status);

/**
 * @brief Evaluate production admission from validation issues.
 * @param issues Stable validation issues from diagnostics.
 * @param mode Requested admission mode.
 * @return Admission decision for the experimental OpenVDB path.
 */
ProductionAdmissionDecision EvaluateProductionAdmission(
    const std::vector<ValidationIssue>& issues,
    AdmissionMode mode);

}  // namespace slicer_core
