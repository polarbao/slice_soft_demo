#include "slicer_core/layout/GridLayoutPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr double kBoundsTolerance{1.0e-9};

GridLayoutResult Failure(
    const GridLayoutErrorCode code,
    const std::string& instanceId,
    const std::string& field,
    const std::string& message,
    const std::uint64_t sourceSceneRevision)
{
    GridLayoutResult result;
    result.sourcescenerevision = sourceSceneRevision;
    result.derivedscenerevision = sourceSceneRevision;
    result.error = GridLayoutError{
        code,
        instanceId,
        field,
        message};
    return result;
}

bool IsFiniteBounds(const BoundingBox& bounds)
{
    return std::isfinite(bounds.min.x)
        && std::isfinite(bounds.min.y)
        && std::isfinite(bounds.min.z)
        && std::isfinite(bounds.max.x)
        && std::isfinite(bounds.max.y)
        && std::isfinite(bounds.max.z)
        && bounds.max.x - bounds.min.x > kBoundsTolerance
        && bounds.max.y - bounds.min.y > kBoundsTolerance
        && bounds.max.z + kBoundsTolerance >= bounds.min.z;
}

bool IsTranslationOnly(const ModelTransform& transform)
{
    const ModelTransform normalized =
        NormalizeModelTransform(transform);
    return std::isfinite(normalized.translatexmm)
        && std::isfinite(normalized.translateymm)
        && std::abs(normalized.rotatexdeg) <= kBoundsTolerance
        && std::abs(normalized.rotateydeg) <= kBoundsTolerance
        && std::abs(normalized.rotatezdeg) <= kBoundsTolerance
        && std::abs(normalized.uniformscale - 1.0)
            <= kBoundsTolerance
        && !normalized.mirrorx
        && !normalized.mirrory
        && !normalized.landonbuildplate;
}

BoundingBox TranslateBounds(
    const BoundingBox& bounds,
    const double translateX,
    const double translateY)
{
    BoundingBox translated = bounds;
    translated.min.x += translateX;
    translated.max.x += translateX;
    translated.min.y += translateY;
    translated.max.y += translateY;
    return translated;
}

bool BoundsOverlap(
    const BoundingBox& left,
    const BoundingBox& right)
{
    const double overlapX =
        std::min(left.max.x, right.max.x)
        - std::max(left.min.x, right.min.x);
    const double overlapY =
        std::min(left.max.y, right.max.y)
        - std::max(left.min.y, right.min.y);
    return overlapX > kBoundsTolerance
        && overlapY > kBoundsTolerance;
}

void IncludeBounds(BoundingBox& target, const BoundingBox& bounds)
{
    target.min.x = std::min(target.min.x, bounds.min.x);
    target.min.y = std::min(target.min.y, bounds.min.y);
    target.min.z = std::min(target.min.z, bounds.min.z);
    target.max.x = std::max(target.max.x, bounds.max.x);
    target.max.y = std::max(target.max.y, bounds.max.y);
    target.max.z = std::max(target.max.z, bounds.max.z);
}

}  // namespace

bool GridLayoutResult::IsValid() const
{
    return !error.has_value();
}

std::string_view GridLayoutErrorCodeName(
    const GridLayoutErrorCode code)
{
    switch (code)
    {
    case GridLayoutErrorCode::None:
        return "NONE";
    case GridLayoutErrorCode::InstanceCapacityExceeded:
        return "LAYOUT_INSTANCE_CAPACITY_EXCEEDED";
    case GridLayoutErrorCode::ParameterOutOfRange:
        return "LAYOUT_PARAMETER_OUT_OF_RANGE";
    case GridLayoutErrorCode::SceneRevisionStale:
        return "LAYOUT_SCENE_REVISION_STALE";
    case GridLayoutErrorCode::InstanceBoundsInvalid:
        return "LAYOUT_INSTANCE_BOUNDS_INVALID";
    case GridLayoutErrorCode::LockedInstanceConflict:
        return "LAYOUT_LOCKED_INSTANCE_CONFLICT";
    case GridLayoutErrorCode::InstanceNotFound:
        return "LAYOUT_INSTANCE_NOT_FOUND";
    }
    return "LAYOUT_UNKNOWN";
}

GridLayoutResult ComputeGridLayout(
    const GridLayoutRequest& request)
{
    if (request.expectedscenerevision
        != request.currentscenerevision)
    {
        return Failure(
            GridLayoutErrorCode::SceneRevisionStale,
            {},
            "sceneRevision",
            "scene revision is stale",
            request.currentscenerevision);
    }
    if (request.layout.policy != "grid"
        || request.layout.maxcolumns < 1
        || request.layout.maxcolumns > 11
        || request.layout.maxrows < 1
        || request.layout.maxrows > 2
        || !std::isfinite(request.layout.columngapmm)
        || !std::isfinite(request.layout.rowgapmm)
        || request.layout.columngapmm < 0.0
        || request.layout.rowgapmm < 0.0
        || request.layout.spacingmode != "edge_clearance"
        || request.layout.order != "row_major")
    {
        return Failure(
            GridLayoutErrorCode::ParameterOutOfRange,
            {},
            "layout",
            "layout parameters must satisfy the P0 11x2 grid contract",
            request.currentscenerevision);
    }
    if (request.items.empty())
    {
        return Failure(
            GridLayoutErrorCode::InstanceNotFound,
            {},
            "items",
            "layout requires at least one scene instance",
            request.currentscenerevision);
    }
    const std::size_t capacity =
        static_cast<std::size_t>(request.layout.maxcolumns)
        * static_cast<std::size_t>(request.layout.maxrows);
    if (request.items.size() > capacity
        || request.items.size() > 22U)
    {
        return Failure(
            GridLayoutErrorCode::InstanceCapacityExceeded,
            {},
            "items",
            "scene instance count exceeds the requested grid capacity",
            request.currentscenerevision);
    }

    std::vector<BoundingBox> requestedBounds;
    requestedBounds.reserve(request.items.size());
    std::vector<double> columnWidths(
        static_cast<std::size_t>(request.layout.maxcolumns),
        0.0);
    std::vector<double> rowHeights(
        static_cast<std::size_t>(request.layout.maxrows),
        0.0);

    for (std::size_t index = 0U;
         index < request.items.size();
         ++index)
    {
        const GridLayoutItem& item = request.items[index];
        const auto requestedValidation =
            ValidateModelTransform(
                item.requestedtransform,
                item.instance.instanceid,
                item.instance.modelid);
        const auto derivedValidation =
            ValidateModelTransform(
                item.currentderivedlayouttransform,
                item.instance.instanceid,
                item.instance.modelid);
        const ModelTransform expectedEffective =
            ComposeModelTransforms(
                item.currentderivedlayouttransform,
                item.requestedtransform);
        if (item.instance.instanceid.empty()
            || item.instance.modelid.empty()
            || !requestedValidation.IsValid()
            || !derivedValidation.IsValid()
            || !IsTranslationOnly(
                item.currentderivedlayouttransform)
            || !ModelTransformsEquivalent(
                expectedEffective,
                item.instance.transform)
            || !IsFiniteBounds(item.instance.effectivebboxmm))
        {
            return Failure(
                GridLayoutErrorCode::InstanceBoundsInvalid,
                item.instance.instanceid,
                "effectiveBboxMm",
                "instance transform or effective bounds are invalid",
                request.currentscenerevision);
        }

        const BoundingBox baseBounds = TranslateBounds(
            item.instance.effectivebboxmm,
            -item.currentderivedlayouttransform.translatexmm,
            -item.currentderivedlayouttransform.translateymm);
        if (!IsFiniteBounds(baseBounds))
        {
            return Failure(
                GridLayoutErrorCode::InstanceBoundsInvalid,
                item.instance.instanceid,
                "requestedBboxMm",
                "requested instance bounds are invalid",
                request.currentscenerevision);
        }
        requestedBounds.push_back(baseBounds);

        const int row =
            static_cast<int>(index)
            / request.layout.maxcolumns;
        const int column =
            static_cast<int>(index)
            % request.layout.maxcolumns;
        columnWidths[static_cast<std::size_t>(column)] =
            std::max(
                columnWidths[static_cast<std::size_t>(column)],
                baseBounds.max.x - baseBounds.min.x);
        rowHeights[static_cast<std::size_t>(row)] =
            std::max(
                rowHeights[static_cast<std::size_t>(row)],
                baseBounds.max.y - baseBounds.min.y);
    }

    std::vector<double> columnOrigins(
        columnWidths.size(),
        kDefaultSceneBoundaryMarginMm);
    for (std::size_t column = 1U;
         column < columnOrigins.size();
         ++column)
    {
        columnOrigins[column] =
            columnOrigins[column - 1U]
            + columnWidths[column - 1U]
            + request.layout.columngapmm;
    }
    std::vector<double> rowOrigins(
        rowHeights.size(),
        kDefaultSceneBoundaryMarginMm);
    for (std::size_t row = 1U; row < rowOrigins.size(); ++row)
    {
        rowOrigins[row] =
            rowOrigins[row - 1U]
            + rowHeights[row - 1U]
            + request.layout.rowgapmm;
    }

    GridLayoutResult result;
    result.sourcescenerevision = request.currentscenerevision;
    result.derivedscenerevision =
        request.currentscenerevision + 1U;
    result.placements.reserve(request.items.size());
    bool hasBounds{false};
    for (std::size_t index = 0U;
         index < request.items.size();
         ++index)
    {
        const GridLayoutItem& item = request.items[index];
        const int row =
            static_cast<int>(index)
            / request.layout.maxcolumns;
        const int column =
            static_cast<int>(index)
            % request.layout.maxcolumns;

        GridLayoutPlacement placement;
        placement.instanceid = item.instance.instanceid;
        placement.row = row;
        placement.column = column;
        placement.requestedtransform =
            NormalizeModelTransform(item.requestedtransform);
        if (item.instance.locked)
        {
            placement.derivedlayouttransform =
                NormalizeModelTransform(
                    item.currentderivedlayouttransform);
            placement.effectivetransform =
                NormalizeModelTransform(item.instance.transform);
            placement.effectivebboxmm =
                item.instance.effectivebboxmm;
        }
        else
        {
            placement.layoutoffsetxmm =
                columnOrigins[static_cast<std::size_t>(column)]
                - requestedBounds[index].min.x;
            placement.layoutoffsetymm =
                rowOrigins[static_cast<std::size_t>(row)]
                - requestedBounds[index].min.y;
            placement.derivedlayouttransform.translatexmm =
                placement.layoutoffsetxmm;
            placement.derivedlayouttransform.translateymm =
                placement.layoutoffsetymm;
            placement.effectivetransform =
                ComposeModelTransforms(
                    placement.derivedlayouttransform,
                    placement.requestedtransform);
            placement.effectivebboxmm = TranslateBounds(
                requestedBounds[index],
                placement.layoutoffsetxmm,
                placement.layoutoffsetymm);
        }
        placement.layoutoffsetxmm =
            placement.derivedlayouttransform.translatexmm;
        placement.layoutoffsetymm =
            placement.derivedlayouttransform.translateymm;
        result.changed =
            result.changed
            || !ModelTransformsEquivalent(
                placement.derivedlayouttransform,
                item.currentderivedlayouttransform);
        if (!hasBounds)
        {
            result.boundsmm = placement.effectivebboxmm;
            hasBounds = true;
        }
        else
        {
            IncludeBounds(
                result.boundsmm,
                placement.effectivebboxmm);
        }
        result.placements.push_back(std::move(placement));
    }

    for (std::size_t left = 0U;
         left < result.placements.size();
         ++left)
    {
        for (std::size_t right = left + 1U;
             right < result.placements.size();
             ++right)
        {
            if (!request.items[left].instance.locked
                && !request.items[right].instance.locked)
            {
                continue;
            }
            if (BoundsOverlap(
                    result.placements[left].effectivebboxmm,
                    result.placements[right].effectivebboxmm))
            {
                return Failure(
                    GridLayoutErrorCode::LockedInstanceConflict,
                    request.items[
                        request.items[right].instance.locked
                            ? right
                            : left]
                        .instance.instanceid,
                    "locked",
                    "locked instance overlaps a derived grid placement",
                    request.currentscenerevision);
            }
        }
    }
    return result;
}

}  // namespace slicer_core
