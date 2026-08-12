#include "slicer_core/geometry/ContactPostureMetrics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr double kMinimumSpanMm{1.0e-9};

double Clamp(const double value, const double minimum, const double maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

double ClippedTriangleProjectedAreaBelowZ(
    const Triangle& triangle,
    const double maximumZMm)
{
    struct WeightedPoint
    {
        Vec3 point;
    };
    std::vector<WeightedPoint> polygon{
        {triangle.a},
        {triangle.b},
        {triangle.c}};
    std::vector<WeightedPoint> clipped;
    clipped.reserve(4U);
    WeightedPoint previous{polygon.back()};
    bool previousInside{previous.point.z <= maximumZMm};
    for (const WeightedPoint& current : polygon)
    {
        const bool currentInside{current.point.z <= maximumZMm};
        if (currentInside != previousInside)
        {
            const double denominator{current.point.z - previous.point.z};
            const double ratio{std::abs(denominator) <= kMinimumSpanMm
                ? 0.0
                : Clamp(
                    (maximumZMm - previous.point.z) / denominator,
                    0.0,
                    1.0)};
            clipped.push_back({
                {
                    previous.point.x + (current.point.x - previous.point.x) * ratio,
                    previous.point.y + (current.point.y - previous.point.y) * ratio,
                    maximumZMm}});
        }
        if (currentInside)
        {
            clipped.push_back(current);
        }
        previous = current;
        previousInside = currentInside;
    }
    if (clipped.size() < 3U)
    {
        return 0.0;
    }
    double signedArea{0.0};
    for (std::size_t index{0U}; index < clipped.size(); ++index)
    {
        const Vec3& current{clipped.at(index).point};
        const Vec3& next{clipped.at((index + 1U) % clipped.size()).point};
        signedArea += current.x * next.y - next.x * current.y;
    }
    return std::abs(signedArea) * 0.5;
}

double EndBandWidth(
    const ModelReport& model,
    const double minimumY,
    const double maximumY)
{
    double minimumX{std::numeric_limits<double>::max()};
    double maximumX{std::numeric_limits<double>::lowest()};
    for (const Triangle& triangle : model.triangles)
    {
        for (const Vec3& point : {triangle.a, triangle.b, triangle.c})
        {
            if (point.y < minimumY || point.y > maximumY)
            {
                continue;
            }
            minimumX = std::min(minimumX, point.x);
            maximumX = std::max(maximumX, point.x);
        }
    }
    return minimumX <= maximumX ? maximumX - minimumX : 0.0;
}

}  // namespace

ContactPostureMetrics MeasureContactPosture(
    const ModelReport& model,
    const ContactPostureMetricPolicy& policy)
{
    if (!std::isfinite(policy.sidebandfraction)
        || policy.sidebandfraction <= 0.0
        || policy.sidebandfraction >= 0.5)
    {
        throw std::invalid_argument("sideBandFraction must be in (0, 0.5)");
    }
    if (!std::isfinite(policy.firstslabfraction)
        || policy.firstslabfraction <= 0.0
        || policy.firstslabfraction > 1.0)
    {
        throw std::invalid_argument("firstSlabFraction must be in (0, 1]");
    }
    if (!std::isfinite(policy.layerthicknessmm)
        || policy.layerthicknessmm <= 0.0)
    {
        throw std::invalid_argument("layerThicknessMm must be positive");
    }

    ContactPostureMetrics result;
    if (model.triangles.empty())
    {
        result.rejectionreason = "model_has_no_triangles";
        return result;
    }

    result.longaxislengthmm = model.bbox_mm.max.y - model.bbox_mm.min.y;
    result.transversespanmm = model.bbox_mm.max.x - model.bbox_mm.min.x;
    if (result.longaxislengthmm <= result.transversespanmm
        || result.transversespanmm <= kMinimumSpanMm)
    {
        result.rejectionreason = "long_axis_is_not_positive_y";
        return result;
    }

    const double sideBandWidth{result.transversespanmm * policy.sidebandfraction};
    const double leftBandMaximumX{model.bbox_mm.min.x + sideBandWidth};
    const double rightBandMinimumX{model.bbox_mm.max.x - sideBandWidth};
    const double centerBandHalfWidth{sideBandWidth};
    const double centerX{0.5 * (model.bbox_mm.min.x + model.bbox_mm.max.x)};
    result.leftbandminimumzmm = std::numeric_limits<double>::max();
    result.rightbandminimumzmm = std::numeric_limits<double>::max();
    double centerBandMinimumZMm{std::numeric_limits<double>::max()};
    for (const Triangle& triangle : model.triangles)
    {
        for (const Vec3& point : {triangle.a, triangle.b, triangle.c})
        {
            if (point.x <= leftBandMaximumX)
            {
                result.leftbandminimumzmm = std::min(
                    result.leftbandminimumzmm,
                    point.z);
                ++result.leftbandvertexcount;
            }
            if (point.x >= rightBandMinimumX)
            {
                result.rightbandminimumzmm = std::min(
                    result.rightbandminimumzmm,
                    point.z);
                ++result.rightbandvertexcount;
            }
            if (std::abs(point.x - centerX) <= centerBandHalfWidth)
            {
                centerBandMinimumZMm = std::min(centerBandMinimumZMm, point.z);
                ++result.centerbandvertexcount;
            }
        }
        result.firsthalfslabareamm2 += ClippedTriangleProjectedAreaBelowZ(
            triangle,
            model.bbox_mm.min.z
                + policy.layerthicknessmm * policy.firstslabfraction);
        result.firstslabareamm2 += ClippedTriangleProjectedAreaBelowZ(
            triangle,
            model.bbox_mm.min.z + policy.layerthicknessmm);
        result.secondslabareamm2 += ClippedTriangleProjectedAreaBelowZ(
            triangle,
            model.bbox_mm.min.z + 2.0 * policy.layerthicknessmm);
    }

    if (result.leftbandvertexcount == 0U
        || result.rightbandvertexcount == 0U
        || result.centerbandvertexcount == 0U)
    {
        result.rejectionreason = "boundary_or_center_band_is_empty";
        return result;
    }
    result.sideenvelopedeltamm =
        result.rightbandminimumzmm - result.leftbandminimumzmm;
    result.centertosideenvelopedeltamm = centerBandMinimumZMm
        - std::min(result.leftbandminimumzmm, result.rightbandminimumzmm);
    result.candidateangledeg = std::atan2(
        result.sideenvelopedeltamm,
        result.transversespanmm)
        * 180.0 / 3.14159265358979323846;

    const double endBandLength{result.longaxislengthmm * policy.sidebandfraction};
    const double negativeYWidth{EndBandWidth(
        model,
        model.bbox_mm.min.y,
        model.bbox_mm.min.y + endBandLength)};
    const double positiveYWidth{EndBandWidth(
        model,
        model.bbox_mm.max.y - endBandLength,
        model.bbox_mm.max.y)};
    result.positiveytipwidthdeltamm = negativeYWidth - positiveYWidth;
    result.candidateanglewithinlimit = std::abs(result.candidateangledeg)
        <= policy.maximumabsolutecandidateangledeg;
    result.positivezconstraintsatisfied = result.centertosideenvelopedeltamm
        >= policy.requiredpositivezenvelopedeltamm;
    result.positiveyconstraintsatisfied = result.positiveytipwidthdeltamm
        >= policy.requiredpositiveytipdeltamm;
    result.valid = true;
    return result;
}

}  // namespace slicer_core
