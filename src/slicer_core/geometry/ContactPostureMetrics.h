#pragma once

#include "slicer_core/model.h"

#include <cstddef>
#include <string>

namespace slicer_core
{

/** @brief Frozen measurement parameters for Stage 16 contact posture baselines. */
struct ContactPostureMetricPolicy
{
    double sidebandfraction{0.125};
    double firstslabfraction{0.5};
    double layerthicknessmm{0.038};
    double maximumabsolutecandidateangledeg{12.0};
    double requiredpositivezenvelopedeltamm{0.0};
    double requiredpositiveytipdeltamm{0.0};
};

/** @brief Read-only contact and direction measurements for one oriented nail model. */
struct ContactPostureMetrics
{
    bool valid{false};
    std::string rejectionreason;
    double longaxislengthmm{0.0};
    double transversespanmm{0.0};
    double leftbandminimumzmm{0.0};
    double rightbandminimumzmm{0.0};
    double sideenvelopedeltamm{0.0};
    double centertosideenvelopedeltamm{0.0};
    double firsthalfslabareamm2{0.0};
    double firstslabareamm2{0.0};
    double secondslabareamm2{0.0};
    double candidateangledeg{0.0};
    double positiveytipwidthdeltamm{0.0};
    std::size_t leftbandvertexcount{0U};
    std::size_t rightbandvertexcount{0U};
    std::size_t centerbandvertexcount{0U};
    bool candidateanglewithinlimit{false};
    bool positivezconstraintsatisfied{false};
    bool positiveyconstraintsatisfied{false};
};

/**
 * @brief Measure the current oriented pose without changing geometry.
 * @param model Imported and auto-oriented model report.
 * @param policy Frozen Stage 16 measurement policy.
 * @return Contact, angle and direction metrics or an explicit rejection reason.
 */
ContactPostureMetrics MeasureContactPosture(
    const ModelReport& model,
    const ContactPostureMetricPolicy& policy = {});

}  // namespace slicer_core
