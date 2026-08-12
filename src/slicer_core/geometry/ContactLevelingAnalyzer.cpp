#include "slicer_core/geometry/ContactLevelingAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr double kPi{3.14159265358979323846};
constexpr double kComparisonTolerance{1.0e-9};

struct EvaluatedCandidate
{
    double angledeg{0.0};
    ModelReport model;
    ContactPostureMetrics metrics;
};

ModelReport RollAndGround(
    const ModelReport& model,
    const double angleDeg)
{
    const double radians{angleDeg * kPi / 180.0};
    const double cosine{std::cos(radians)};
    const double sine{std::sin(radians)};
    const double centerX{0.5 * (model.bbox_mm.min.x + model.bbox_mm.max.x)};

    ModelReport result{model};
    result.bbox_mm.min = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()};
    result.bbox_mm.max = {
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest()};
    const auto rotatePoint =
        [centerX, cosine, sine](Vec3& point)
        {
            const double relativeX{point.x - centerX};
            const double z{point.z};
            point.x = centerX + relativeX * cosine - z * sine;
            point.z = relativeX * sine + z * cosine;
        };
    for (Triangle& triangle : result.triangles)
    {
        for (Vec3* point : {&triangle.a, &triangle.b, &triangle.c})
        {
            rotatePoint(*point);
            result.bbox_mm.min.x = std::min(result.bbox_mm.min.x, point->x);
            result.bbox_mm.min.y = std::min(result.bbox_mm.min.y, point->y);
            result.bbox_mm.min.z = std::min(result.bbox_mm.min.z, point->z);
            result.bbox_mm.max.x = std::max(result.bbox_mm.max.x, point->x);
            result.bbox_mm.max.y = std::max(result.bbox_mm.max.y, point->y);
            result.bbox_mm.max.z = std::max(result.bbox_mm.max.z, point->z);
        }
    }
    const double groundOffset{-result.bbox_mm.min.z};
    for (Triangle& triangle : result.triangles)
    {
        triangle.a.z += groundOffset;
        triangle.b.z += groundOffset;
        triangle.c.z += groundOffset;
    }
    result.bbox_mm.max.z += groundOffset;
    result.bbox_mm.min.z = 0.0;
    return result;
}

bool IsBetter(
    const EvaluatedCandidate& candidate,
    const EvaluatedCandidate& current)
{
    const double areaDelta{candidate.metrics.firsthalfslabareamm2
        - current.metrics.firsthalfslabareamm2};
    if (std::abs(areaDelta) > kComparisonTolerance)
    {
        return areaDelta > 0.0;
    }
    const double envelopeDelta{
        std::abs(current.metrics.sideenvelopedeltamm)
        - std::abs(candidate.metrics.sideenvelopedeltamm)};
    if (std::abs(envelopeDelta) > kComparisonTolerance)
    {
        return envelopeDelta > 0.0;
    }
    const double absoluteAngleDelta{
        std::abs(current.angledeg) - std::abs(candidate.angledeg)};
    if (std::abs(absoluteAngleDelta) > kComparisonTolerance)
    {
        return absoluteAngleDelta > 0.0;
    }
    return candidate.angledeg < current.angledeg;
}

bool SatisfiesConstraints(
    const EvaluatedCandidate& candidate,
    const ModelReport& baseline,
    const ContactPostureMetricPolicy& metricPolicy,
    const ContactLevelingPolicy& levelingPolicy)
{
    const double baselineHeight{baseline.bbox_mm.max.z - baseline.bbox_mm.min.z};
    const double candidateHeight{
        candidate.model.bbox_mm.max.z - candidate.model.bbox_mm.min.z};
    const double baselineFootprint{
        baseline.bbox_mm.max.x - baseline.bbox_mm.min.x};
    const double candidateFootprint{
        candidate.model.bbox_mm.max.x - candidate.model.bbox_mm.min.x};
    return candidate.metrics.valid
        && candidate.metrics.positivezconstraintsatisfied
        && candidate.metrics.positiveyconstraintsatisfied
        && std::abs(candidate.angledeg)
            <= metricPolicy.maximumabsolutecandidateangledeg
        && candidateHeight - baselineHeight
            <= levelingPolicy.maximumheightincreasemm
                + kComparisonTolerance
        && candidateFootprint - baselineFootprint
            <= levelingPolicy.maximumfootprintincreasemm
                + kComparisonTolerance;
}

}  // namespace

ContactLevelingCandidate AnalyzeContactLeveling(
    const ModelReport& model,
    const ContactPostureMetricPolicy& metricPolicy,
    const ContactLevelingPolicy& levelingPolicy)
{
    if (!std::isfinite(levelingPolicy.minimumangledeg)
        || !std::isfinite(levelingPolicy.maximumangledeg)
        || levelingPolicy.minimumangledeg > levelingPolicy.maximumangledeg
        || levelingPolicy.coarseangleincrementdeg <= 0.0
        || levelingPolicy.refineangleincrementdeg <= 0.0
        || levelingPolicy.refineneighborhooddeg < 0.0)
    {
        throw std::invalid_argument("contact leveling search policy is invalid");
    }

    ContactLevelingCandidate result;
    const ContactPostureMetrics baseline{
        MeasureContactPosture(model, metricPolicy)};
    if (!baseline.valid)
    {
        result.rejectionreason = "baseline_" + baseline.rejectionreason;
        return result;
    }
    result.baselinefirsthalfslabareamm2 = baseline.firsthalfslabareamm2;
    result.baselineheightmm = model.bbox_mm.max.z - model.bbox_mm.min.z;
    result.baselinefootprintxmm = model.bbox_mm.max.x - model.bbox_mm.min.x;

    EvaluatedCandidate best;
    bool hasBest{false};
    const auto evaluate =
        [&](const double angleDeg)
        {
            EvaluatedCandidate candidate;
            candidate.angledeg = angleDeg;
            candidate.model = RollAndGround(model, angleDeg);
            candidate.metrics = MeasureContactPosture(candidate.model, metricPolicy);
            ++result.evaluatedcandidatecount;
            if (!SatisfiesConstraints(
                    candidate,
                    model,
                    metricPolicy,
                    levelingPolicy))
            {
                return;
            }
            if (!hasBest || IsBetter(candidate, best))
            {
                best = std::move(candidate);
                hasBest = true;
            }
        };

    for (double angle{levelingPolicy.minimumangledeg};
         angle <= levelingPolicy.maximumangledeg + kComparisonTolerance;
         angle += levelingPolicy.coarseangleincrementdeg)
    {
        evaluate(std::min(angle, levelingPolicy.maximumangledeg));
    }
    if (!hasBest)
    {
        result.rejectionreason = "no_candidate_satisfies_constraints";
        return result;
    }

    const double refineMinimum{std::max(
        levelingPolicy.minimumangledeg,
        best.angledeg - levelingPolicy.refineneighborhooddeg)};
    const double refineMaximum{std::min(
        levelingPolicy.maximumangledeg,
        best.angledeg + levelingPolicy.refineneighborhooddeg)};
    for (double angle{refineMinimum};
         angle <= refineMaximum + kComparisonTolerance;
         angle += levelingPolicy.refineangleincrementdeg)
    {
        evaluate(std::min(angle, refineMaximum));
    }

    result.available = true;
    result.status = "diagnostic_only";
    result.candidateangledeg = best.angledeg;
    result.candidatefirsthalfslabareamm2 =
        best.metrics.firsthalfslabareamm2;
    result.contactareaimprovementmm2 =
        result.candidatefirsthalfslabareamm2
        - result.baselinefirsthalfslabareamm2;
    result.candidateheightmm =
        best.model.bbox_mm.max.z - best.model.bbox_mm.min.z;
    result.heightincreasemm =
        result.candidateheightmm - result.baselineheightmm;
    result.candidatefootprintxmm =
        best.model.bbox_mm.max.x - best.model.bbox_mm.min.x;
    result.footprintincreasemm =
        result.candidatefootprintxmm - result.baselinefootprintxmm;
    result.sideenvelopedeltamm = best.metrics.sideenvelopedeltamm;
    result.positivezconstraintsatisfied =
        best.metrics.positivezconstraintsatisfied;
    result.positiveyconstraintsatisfied =
        best.metrics.positiveyconstraintsatisfied;
    result.angleconstraintsatisfied =
        std::abs(best.angledeg)
        <= metricPolicy.maximumabsolutecandidateangledeg;
    result.heightconstraintsatisfied =
        result.heightincreasemm
        <= levelingPolicy.maximumheightincreasemm
            + kComparisonTolerance;
    result.footprintconstraintsatisfied =
        result.footprintincreasemm
        <= levelingPolicy.maximumfootprintincreasemm
            + kComparisonTolerance;
    return result;
}

}  // namespace slicer_core
