#pragma once

#include "slicer_core/geometry/ContactPostureMetrics.h"

#include <string>

namespace slicer_core
{

/** @brief Stable diagnostic-only policy for Stage 16 contact leveling. */
struct ContactLevelingPolicy
{
    double minimumangledeg{-12.0};
    double maximumangledeg{12.0};
    double coarseangleincrementdeg{0.5};
    double refineangleincrementdeg{0.1};
    double refineneighborhooddeg{0.5};
    double maximumheightincreasemm{0.5};
    double maximumfootprintincreasemm{0.5};
};

/** @brief One read-only contact-leveling candidate and its constraints. */
struct ContactLevelingCandidate
{
    bool available{false};
    std::string status{"rejected"};
    std::string rejectionreason;
    double candidateangledeg{0.0};
    double baselinefirsthalfslabareamm2{0.0};
    double candidatefirsthalfslabareamm2{0.0};
    double contactareaimprovementmm2{0.0};
    double baselineheightmm{0.0};
    double candidateheightmm{0.0};
    double heightincreasemm{0.0};
    double baselinefootprintxmm{0.0};
    double candidatefootprintxmm{0.0};
    double footprintincreasemm{0.0};
    double sideenvelopedeltamm{0.0};
    bool positivezconstraintsatisfied{false};
    bool positiveyconstraintsatisfied{false};
    bool angleconstraintsatisfied{false};
    bool heightconstraintsatisfied{false};
    bool footprintconstraintsatisfied{false};
    int evaluatedcandidatecount{0};
};

/**
 * @brief Analyze a bounded roll candidate without changing the input model.
 * @param model Imported, auto-oriented and grounded model report.
 * @param metricpolicy Frozen Stage 16 contact measurement policy.
 * @param levelingpolicy Deterministic bounded search policy.
 * @return Best diagnostic candidate or a stable rejection reason.
 */
ContactLevelingCandidate AnalyzeContactLeveling(
    const ModelReport& model,
    const ContactPostureMetricPolicy& metricpolicy = {},
    const ContactLevelingPolicy& levelingpolicy = {});

}  // namespace slicer_core
