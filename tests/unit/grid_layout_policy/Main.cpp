#include "slicer_core/layout/GridLayoutPolicy.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr double kTolerance{1.0e-9};

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool ApproximatelyEqual(const double left, const double right)
{
    return std::abs(left - right) <= kTolerance;
}

slicer_core::GridLayoutItem MakeItem(
    const int index,
    const double width,
    const double height,
    const double minX = 0.0,
    const double minY = 0.0)
{
    slicer_core::GridLayoutItem item;
    item.instance.instanceid = "instance-" + std::to_string(index);
    item.instance.modelid = "model-" + std::to_string(index);
    item.instance.sourcetransformidentity =
        "source-" + std::to_string(index);
    item.instance.sourcebboxmm = {
        {0.0, 0.0, 0.0},
        {width, height, 1.0}};
    item.instance.effectivebboxmm = {
        {minX, minY, 0.0},
        {minX + width, minY + height, 1.0}};
    return item;
}

slicer_core::GridLayoutRequest MakeRequest(const int count)
{
    slicer_core::GridLayoutRequest request;
    request.currentscenerevision = 7U;
    request.expectedscenerevision = 7U;
    for (int index = 0; index < count; ++index)
    {
        request.items.push_back(MakeItem(index, 10.0, 5.0));
    }
    return request;
}

void SupportsOneElevenTwelveAndTwentyTwo()
{
    for (const int count : {1, 11, 12, 22})
    {
        const slicer_core::GridLayoutResult result =
            slicer_core::ComputeGridLayout(MakeRequest(count));
        Require(result.IsValid(), "supported instance count should pass");
        Require(
            result.placements.size()
                == static_cast<std::size_t>(count),
            "layout should produce one placement per instance");
        Require(
            result.sourcescenerevision == 7U
                && result.derivedscenerevision == 8U,
            "layout revisions should advance exactly once");
    }

    const slicer_core::GridLayoutResult twelve =
        slicer_core::ComputeGridLayout(MakeRequest(12));
    Require(
        twelve.placements.at(10U).row == 0
            && twelve.placements.at(10U).column == 10,
        "the eleventh instance should end the first row");
    Require(
        twelve.placements.at(11U).row == 1
            && twelve.placements.at(11U).column == 0,
        "the twelfth instance should begin the second row");
    Require(
        ApproximatelyEqual(
            twelve.placements.at(10U).effectivebboxmm.min.x,
            200.0),
        "column gap should be edge-to-edge 10 mm");
    Require(
        ApproximatelyEqual(
            twelve.placements.at(11U).effectivebboxmm.min.y,
            15.0),
        "row gap should be edge-to-edge 10 mm");
}

void RejectsCapacityParametersAndStaleRevision()
{
    const slicer_core::GridLayoutResult empty =
        slicer_core::ComputeGridLayout(MakeRequest(0));
    Require(
        !empty.IsValid()
            && empty.error->code
                == slicer_core::GridLayoutErrorCode::InstanceNotFound,
        "empty layout should fail closed");

    const slicer_core::GridLayoutResult maximumExceeded =
        slicer_core::ComputeGridLayout(MakeRequest(23));
    Require(
        !maximumExceeded.IsValid()
            && maximumExceeded.error->code
                == slicer_core::GridLayoutErrorCode::
                    InstanceCapacityExceeded,
        "more than 22 instances should be rejected");

    slicer_core::GridLayoutRequest overflow = MakeRequest(12);
    overflow.layout.maxcolumns = 5;
    overflow.layout.maxrows = 2;
    const slicer_core::GridLayoutResult overflowResult =
        slicer_core::ComputeGridLayout(overflow);
    Require(
        !overflowResult.IsValid()
            && overflowResult.error->code
                == slicer_core::GridLayoutErrorCode::
                    InstanceCapacityExceeded,
        "capacity overflow must fail closed");
    Require(
        overflowResult.placements.empty(),
        "capacity failure must not return partial placements");

    slicer_core::GridLayoutRequest invalid = MakeRequest(1);
    invalid.layout.columngapmm = -0.01;
    Require(
        slicer_core::ComputeGridLayout(invalid).error->code
            == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
        "negative gap should be rejected");

    for (const int columns : {0, 12})
    {
        slicer_core::GridLayoutRequest invalidColumns = MakeRequest(1);
        invalidColumns.layout.maxcolumns = columns;
        Require(
            slicer_core::ComputeGridLayout(invalidColumns).error->code
                == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
            "column count outside 1..11 should be rejected");
    }
    for (const int rows : {0, 3})
    {
        slicer_core::GridLayoutRequest invalidRows = MakeRequest(1);
        invalidRows.layout.maxrows = rows;
        Require(
            slicer_core::ComputeGridLayout(invalidRows).error->code
                == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
            "row count outside 1..2 should be rejected");
    }
    slicer_core::GridLayoutRequest invalidPolicy = MakeRequest(1);
    invalidPolicy.layout.policy = "manual";
    Require(
        slicer_core::ComputeGridLayout(invalidPolicy).error->code
            == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
        "non-grid policy should be rejected");
    slicer_core::GridLayoutRequest invalidSpacing = MakeRequest(1);
    invalidSpacing.layout.spacingmode = "origin_distance";
    Require(
        slicer_core::ComputeGridLayout(invalidSpacing).error->code
            == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
        "non-edge spacing should be rejected");
    slicer_core::GridLayoutRequest invalidOrder = MakeRequest(1);
    invalidOrder.layout.order = "column_major";
    Require(
        slicer_core::ComputeGridLayout(invalidOrder).error->code
            == slicer_core::GridLayoutErrorCode::ParameterOutOfRange,
        "non-row-major order should be rejected");

    slicer_core::GridLayoutRequest stale = MakeRequest(1);
    stale.expectedscenerevision = 6U;
    Require(
        slicer_core::ComputeGridLayout(stale).error->code
            == slicer_core::GridLayoutErrorCode::SceneRevisionStale,
        "stale scene revision should be rejected");
}

void UsesMaximumColumnAndRowBoundsDeterministically()
{
    slicer_core::GridLayoutRequest request;
    request.currentscenerevision = 3U;
    request.expectedscenerevision = 3U;
    request.layout.maxcolumns = 2;
    request.items.push_back(MakeItem(0, 10.0, 5.0, -2.0, -3.0));
    request.items.push_back(MakeItem(1, 4.0, 8.0, 12.0, -1.0));
    request.items.push_back(MakeItem(2, 20.0, 3.0, 6.0, 9.0));
    request.items.push_back(MakeItem(3, 6.0, 7.0, -4.0, 4.0));

    const slicer_core::GridLayoutResult first =
        slicer_core::ComputeGridLayout(request);
    const slicer_core::GridLayoutResult second =
        slicer_core::ComputeGridLayout(request);
    Require(first.IsValid() && second.IsValid(), "mixed bounds should pass");
    Require(
        ApproximatelyEqual(
            first.placements.at(1U).effectivebboxmm.min.x,
            30.0),
        "second column should follow maximum first-column width");
    Require(
        ApproximatelyEqual(
            first.placements.at(2U).effectivebboxmm.min.y,
            18.0),
        "second row should follow maximum first-row height");
    for (std::size_t index = 0U;
         index < first.placements.size();
         ++index)
    {
        Require(
            ApproximatelyEqual(
                first.placements[index].layoutoffsetxmm,
                second.placements[index].layoutoffsetxmm)
                && ApproximatelyEqual(
                    first.placements[index].layoutoffsetymm,
                    second.placements[index].layoutoffsetymm),
            "identical input must produce deterministic offsets");
    }
}

void PreservesHiddenOccupancyAndLockedPlacement()
{
    slicer_core::GridLayoutRequest hidden = MakeRequest(2);
    hidden.items.front().instance.visible = false;
    hidden.layout.columngapmm = 0.01;
    const slicer_core::GridLayoutResult hiddenResult =
        slicer_core::ComputeGridLayout(hidden);
    Require(hiddenResult.IsValid(), "hidden item should remain in layout");
    Require(
        ApproximatelyEqual(
            hiddenResult.placements.at(1U).effectivebboxmm.min.x,
            10.01),
        "hidden item should occupy its row-major slot");

    slicer_core::GridLayoutRequest locked = MakeRequest(2);
    locked.items.at(1U).instance.locked = true;
    locked.items.at(1U).instance.effectivebboxmm = {
        {0.0, 0.0, 0.0},
        {10.0, 5.0, 1.0}};
    const slicer_core::GridLayoutResult lockedResult =
        slicer_core::ComputeGridLayout(locked);
    Require(
        !lockedResult.IsValid()
            && lockedResult.error->code
                == slicer_core::GridLayoutErrorCode::
                    LockedInstanceConflict,
        "locked overlap should fail closed");
    Require(
        lockedResult.placements.empty(),
        "locked conflict must not expose partial placements");

    slicer_core::GridLayoutRequest lockedWithoutConflict = MakeRequest(2);
    lockedWithoutConflict.items.at(1U).instance.locked = true;
    lockedWithoutConflict.items.at(1U).instance.effectivebboxmm = {
        {40.0, 0.0, 0.0},
        {50.0, 5.0, 1.0}};
    lockedWithoutConflict.items.at(1U).instance.transform.translatexmm =
        40.0;
    lockedWithoutConflict.items.at(1U)
        .currentderivedlayouttransform.translatexmm = 40.0;
    const slicer_core::GridLayoutResult lockedWithoutConflictResult =
        slicer_core::ComputeGridLayout(lockedWithoutConflict);
    Require(
        lockedWithoutConflictResult.IsValid()
            && ApproximatelyEqual(
                lockedWithoutConflictResult.placements.at(1U)
                    .effectivebboxmm.min.x,
                40.0),
        "non-overlapping locked instance should retain its placement");
}

void RemovesPreviousDerivedOffsetBeforeRelayout()
{
    slicer_core::GridLayoutRequest request = MakeRequest(1);
    slicer_core::GridLayoutItem& item = request.items.front();
    item.currentderivedlayouttransform.translatexmm = 100.0;
    item.currentderivedlayouttransform.translateymm = 50.0;
    item.instance.transform =
        slicer_core::ComposeModelTransforms(
            item.currentderivedlayouttransform,
            item.requestedtransform);
    item.instance.effectivebboxmm = {
        {100.0, 50.0, 0.0},
        {110.0, 55.0, 1.0}};

    const slicer_core::GridLayoutResult result =
        slicer_core::ComputeGridLayout(request);
    Require(result.IsValid(), "relayout should pass");
    Require(
        ApproximatelyEqual(
            result.placements.front().effectivebboxmm.min.x,
            0.0)
            && ApproximatelyEqual(
                result.placements.front().effectivebboxmm.min.y,
                0.0),
        "relayout should not accumulate an old derived offset");
}

}  // namespace

int main()
{
    SupportsOneElevenTwelveAndTwentyTwo();
    RejectsCapacityParametersAndStaleRevision();
    UsesMaximumColumnAndRowBoundsDeterministically();
    PreservesHiddenOccupancyAndLockedPlacement();
    RemovesPreviousDerivedOffsetBeforeRelayout();
    std::cout << "grid_layout_policy_unit_tests passed\n";
    return 0;
}
