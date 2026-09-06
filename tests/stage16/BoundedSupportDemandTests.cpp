#include "slicer_core/support/BoundedSupportDemand.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using slicer_core::BoundedSupportDemandPlan;
using slicer_core::BoundedSupportDemandRequest;
using slicer_core::BuildBoundedSupportDemandPlan;
using slicer_core::GeometryOccupancyInputKind;
using slicer_core::MaterializePreShapeSupportLayer;
using slicer_core::SupportType;
using slicer_core::SupportTypePriority;

static_assert(!std::is_copy_constructible_v<BoundedSupportDemandPlan>);
static_assert(!std::is_copy_assignable_v<BoundedSupportDemandPlan>);
static_assert(std::is_nothrow_move_constructible_v<BoundedSupportDemandPlan>);

struct Facts
{
    int layerCount{0};
    std::vector<int> lower;
    std::vector<int> last;
    std::vector<int> upper;
    std::vector<int> unsupportedTop;
    bool lowerEnabled{false};
    bool fullEnabled{false};
    bool upperEnabled{false};
    bool unsupportedEnabled{false};
    GeometryOccupancyInputKind inputKind{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

bool ExpectThrowsInvalidArgument(
    const std::function<void()>& operation,
    const std::string& message)
{
    try
    {
        operation();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    catch (...)
    {
    }
    std::cerr << "FAIL " << message << '\n';
    return false;
}

BoundedSupportDemandPlan BuildPlan(const Facts& facts)
{
    BoundedSupportDemandRequest request;
    request.layerCount = facts.layerCount;
    request.inputKind = facts.inputKind;
    request.lowerSourceLayers = facts.lower;
    request.modelLastLayers = facts.last;
    request.upperBoundaryLastLayers = facts.upper;
    request.unsupportedTopExclusiveLayers = facts.unsupportedTop;
    request.lowerEnabled = facts.lowerEnabled;
    request.fullVerticalEnabled = facts.fullEnabled;
    request.upperEnabled = facts.upperEnabled;
    request.unsupportedEnabled = facts.unsupportedEnabled;
    return BuildBoundedSupportDemandPlan(request);
}

std::vector<SupportType> MaterializeTypes(
    const BoundedSupportDemandPlan& plan,
    const int layerIndex,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>* const supportMask = nullptr,
    const std::vector<std::uint8_t>* const upperBoundaryMask = nullptr)
{
    std::vector<std::uint8_t> localSupport(plan.ColumnCount(), 9U);
    std::vector<SupportType> types(
        plan.ColumnCount(),
        SupportType::InternalVoid);
    MaterializePreShapeSupportLayer(
        plan,
        layerIndex,
        modelMask,
        upperBoundaryMask != nullptr ? *upperBoundaryMask : modelMask,
        localSupport,
        types);
    if (supportMask != nullptr)
    {
        *supportMask = std::move(localSupport);
    }
    return types;
}

bool ExactHalfOpenBoundaries()
{
    const std::vector<std::uint8_t> emptyModel{0U};
    bool passed{true};

    Facts facts{5, {2}, {2}, {-1}, {0}};
    facts.lowerEnabled = true;
    auto plan = BuildPlan(facts);
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        const auto types = MaterializeTypes(plan, layerIndex, emptyModel);
        const SupportType expected = layerIndex < 2
            ? SupportType::BottomProjection
            : SupportType::None;
        passed = ExpectTrue(types.front() == expected, "bottom range is [0, lower)")
            && passed;
    }

    facts = Facts{5, {0}, {3}, {-1}, {0}};
    facts.fullEnabled = true;
    plan = BuildPlan(facts);
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        const auto types = MaterializeTypes(plan, layerIndex, emptyModel);
        const SupportType expected = layerIndex < 3
            ? SupportType::FullVerticalProjection
            : SupportType::None;
        passed = ExpectTrue(types.front() == expected, "full range is [0, last)")
            && passed;
    }

    facts = Facts{5, {-1}, {-1}, {2}, {0}};
    facts.upperEnabled = true;
    plan = BuildPlan(facts);
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        const auto types = MaterializeTypes(plan, layerIndex, emptyModel);
        const SupportType expected = layerIndex > 2
            ? SupportType::UpperProjection
            : SupportType::None;
        passed = ExpectTrue(types.front() == expected, "upper range is (upper, layerCount)")
            && passed;
    }

    facts = Facts{5, {0}, {3}, {-1}, {3}};
    facts.unsupportedEnabled = true;
    plan = BuildPlan(facts);
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        const auto types = MaterializeTypes(plan, layerIndex, emptyModel);
        const SupportType expected = layerIndex < 3
            ? SupportType::UnsupportedIsland
            : SupportType::None;
        passed = ExpectTrue(
                     types.front() == expected,
                     "unsupported range is [0, topExclusive)")
            && passed;
    }
    return passed;
}

bool ModelPriorityAndTypeCollisions()
{
    Facts facts{6, {1}, {4}, {4}, {3}};
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto plan = BuildPlan(facts);

    std::vector<std::uint8_t> support;
    auto types = MaterializeTypes(plan, 0, {0U}, &support);
    bool passed = ExpectTrue(
        types.front() == SupportType::UnsupportedIsland,
        "unsupported outranks full, upper and bottom");
    passed = ExpectTrue(support.front() == 1U, "winning candidate sets support mask")
        && passed;

    types = MaterializeTypes(plan, 0, {1U}, &support);
    passed = ExpectTrue(
                 types.front() == SupportType::None && support.front() == 0U,
                 "model pixels always retain priority")
        && passed;

    facts.unsupportedEnabled = false;
    const auto withoutUnsupported = BuildPlan(facts);
    types = MaterializeTypes(withoutUnsupported, 0, {0U});
    passed = ExpectTrue(
                 types.front() == SupportType::FullVerticalProjection,
                 "full outranks upper and bottom")
        && passed;

    facts.fullEnabled = false;
    const auto withoutFull = BuildPlan(facts);
    types = MaterializeTypes(withoutFull, 0, {0U});
    passed = ExpectTrue(
                 types.front() == SupportType::BottomProjection,
                 "bottom remains when unsupported and full are disabled below the model")
        && passed;

    types = MaterializeTypes(withoutFull, 5, {0U});
    passed = ExpectTrue(
                 types.front() == SupportType::UpperProjection,
                 "upper is selected above the upper boundary")
        && passed;

    facts.upperEnabled = false;
    const auto bottomOnly = BuildPlan(facts);
    types = MaterializeTypes(bottomOnly, 0, {0U});
    passed = ExpectTrue(
                 types.front() == SupportType::BottomProjection,
                 "bottom remains when higher-priority types are disabled")
        && passed;
    return ExpectTrue(
               SupportTypePriority(SupportType::InternalVoid)
                       > SupportTypePriority(SupportType::UnsupportedIsland)
                   && SupportTypePriority(SupportType::UnsupportedIsland)
                       > SupportTypePriority(SupportType::FullVerticalProjection)
                   && SupportTypePriority(SupportType::FullVerticalProjection)
                       > SupportTypePriority(SupportType::UpperProjection)
                   && SupportTypePriority(SupportType::UpperProjection)
                       > SupportTypePriority(SupportType::BottomProjection)
                   && SupportTypePriority(SupportType::BottomProjection)
                       > SupportTypePriority(SupportType::ProjectionBase),
               "priority is explicit and independent from enum values")
        && passed;
}

bool BoundaryExtremesAndInternalModelGaps()
{
    Facts facts{4, {0, 0}, {3, 3}, {3, 3}, {3, 0}};
    facts.lowerEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto plan = BuildPlan(facts);
    bool passed{true};
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        const std::vector<std::uint8_t> model{
            0U,
            layerIndex == 1 ? static_cast<std::uint8_t>(1U)
                            : static_cast<std::uint8_t>(0U)};
        const auto types = MaterializeTypes(plan, layerIndex, model);
        const SupportType expected = layerIndex < 3
            ? SupportType::UnsupportedIsland
            : SupportType::None;
        passed = ExpectTrue(
                     types.front() == expected,
                     "last legal unsupported source writes [0, N-1)")
            && passed;
        passed = ExpectTrue(
                     types[1] == SupportType::None,
                     "lower=0, upper=N-1 and empty unsupported emit nothing")
            && passed;
    }

    facts.fullEnabled = true;
    facts.unsupportedEnabled = false;
    const auto fullPlan = BuildPlan(facts);
    const auto gapTypes = MaterializeTypes(fullPlan, 1, {0U, 0U});
    const auto occupiedTypes = MaterializeTypes(fullPlan, 1, {1U, 0U});
    return ExpectTrue(
               gapTypes.front() == SupportType::FullVerticalProjection,
               "an exact current-layer model gap permits retained full support")
        && ExpectTrue(
            occupiedTypes.front() == SupportType::None,
            "an occupied current-layer model pixel suppresses retained support")
        && passed;
}

bool UpperBoundaryOccupancyOnlySuppressesUpper()
{
    Facts facts{5, {-1, 1}, {-1, 4}, {1, 4}, {0, 3}};
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto plan = BuildPlan(facts);
    const std::vector<std::uint8_t> model{0U, 0U};
    const std::vector<std::uint8_t> upperBoundary{1U, 1U};

    const auto allTypes = MaterializeTypes(
        plan,
        2,
        model,
        nullptr,
        &upperBoundary);
    bool passed = ExpectTrue(
        allTypes[1] == SupportType::UnsupportedIsland,
        "outer boundary does not suppress non-upper demand");

    facts.lowerEnabled = false;
    facts.fullEnabled = false;
    facts.unsupportedEnabled = false;
    const auto upperOnlyPlan = BuildPlan(facts);
    std::vector<std::uint8_t> support;
    const auto upperOnlyTypes = MaterializeTypes(
        upperOnlyPlan,
        2,
        model,
        &support,
        &upperBoundary);
    return ExpectTrue(
               upperOnlyTypes.front() == SupportType::None
                   && support.front() == 0U,
               "outer boundary suppresses upper projection")
        && passed;
}

bool DisabledModesAndEmptyColumns()
{
    Facts facts{4, {-1, 2}, {-1, 3}, {-1, 3}, {0, 3}};
    const auto plan = BuildPlan(facts);
    bool passed{true};
    for (int layerIndex{0}; layerIndex < facts.layerCount; ++layerIndex)
    {
        std::vector<std::uint8_t> support;
        const auto types = MaterializeTypes(
            plan,
            layerIndex,
            {0U, 0U},
            &support);
        passed = ExpectTrue(
                     types == std::vector<SupportType>(2U, SupportType::None)
                         && support == std::vector<std::uint8_t>(2U, 0U),
                     "disabled modes fully clear reusable outputs")
            && passed;
    }

    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto enabledPlan = BuildPlan(facts);
    const auto types = MaterializeTypes(enabledPlan, 1, {0U, 0U});
    return ExpectTrue(
               types.front() == SupportType::None,
               "empty per-column facts remain empty")
        && passed;
}

bool InvalidInputsFailClosed()
{
    Facts facts{4, {1}, {2}, {2}, {0}};
    bool passed{true};

    Facts invalid = facts;
    invalid.layerCount = 0;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "non-positive layer count")
        && passed;

    invalid = facts;
    invalid.inputKind = GeometryOccupancyInputKind::GeneralMesh;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "GeneralMesh is not admitted to compact support demand")
        && passed;

    invalid = facts;
    invalid.inputKind = static_cast<GeometryOccupancyInputKind>(77);
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "unknown input kind")
        && passed;

    invalid = facts;
    invalid.upper.push_back(2);
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "inconsistent fact dimensions")
        && passed;

    invalid = facts;
    invalid.lower.front() = -2;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "invalid optional lower fact")
        && passed;

    invalid = facts;
    invalid.last.front() = 4;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "out-of-range model last fact")
        && passed;

    invalid = facts;
    invalid.unsupportedTop.front() = 5;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "out-of-range unsupported exclusive top")
        && passed;

    invalid = facts;
    invalid.upper.front() = 1;
    invalid.upperEnabled = true;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "upper boundary excludes part of the model")
        && passed;

    invalid = facts;
    invalid.unsupportedTop.front() = 1;
    invalid.lower.front() = 2;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "unsupported source is outside the model range")
        && passed;

    invalid = facts;
    invalid.lower.front() = 3;
    invalid.last.front() = 2;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "reversed model summary")
        && passed;

    invalid = facts;
    invalid.last.front() = -1;
    passed = ExpectThrowsInvalidArgument(
                 [&invalid]() { (void)BuildPlan(invalid); },
                 "partially empty model summary")
        && passed;

    const auto plan = BuildPlan(facts);
    std::vector<std::uint8_t> model{0U};
    std::vector<std::uint8_t> support{0U};
    std::vector<std::uint8_t> upperBoundary{0U};
    std::vector<SupportType> types{SupportType::None};
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, -1, model, upperBoundary, support, types);
                 },
                 "negative materialization layer")
        && passed;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 4, model, upperBoundary, support, types);
                 },
                 "out-of-range materialization layer")
        && passed;
    std::vector<std::uint8_t> emptyMask;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, emptyMask, upperBoundary, support, types);
                 },
                 "wrong model buffer size")
        && passed;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, model, emptyMask, support, types);
                 },
                 "wrong upper-boundary buffer size")
        && passed;
    std::vector<SupportType> emptyTypes;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, model, upperBoundary, support, emptyTypes);
                 },
                 "wrong type-map buffer size")
        && passed;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, model, upperBoundary, emptyMask, types);
                 },
                 "wrong output buffer size")
        && passed;
    model.front() = 2U;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, model, upperBoundary, support, types);
                 },
                 "non-binary model mask")
        && passed;
    model.front() = 0U;
    Facts upperFacts{4, {1}, {2}, {2}, {0}};
    upperFacts.upperEnabled = true;
    const auto upperPlan = BuildPlan(upperFacts);
    upperBoundary.front() = 2U;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         upperPlan, 3, model, upperBoundary, support, types);
                 },
                 "non-binary upper-boundary mask")
        && passed;
    upperBoundary.front() = 0U;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan, 0, model, upperBoundary, model, types);
                 },
                 "model/output alias")
        && passed;
    return passed;
}

bool OverlapAndExceptionGuaranteesFailClosed()
{
    Facts facts{4, {1, 1}, {3, 3}, {3, 3}, {2, 2}};
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto plan = BuildPlan(facts);
    std::vector<std::uint8_t> upperBoundary{0U, 0U};
    std::vector<SupportType> types(2U, SupportType::None);
    bool passed{true};

    std::vector<std::uint8_t> partialArena{0U, 0U, 0U};
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan,
                         0,
                         std::span<const std::uint8_t>{partialArena}.first(2U),
                         upperBoundary,
                         std::span<std::uint8_t>{partialArena}.subspan(1U, 2U),
                         types);
                 },
                 "partially overlapping input/output spans")
        && passed;

    std::vector<SupportType> outputArena(2U, SupportType::None);
    std::vector<std::uint8_t> model{0U, 0U};
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan,
                         0,
                         model,
                         upperBoundary,
                         std::span<std::uint8_t>{
                             reinterpret_cast<std::uint8_t*>(outputArena.data()),
                             outputArena.size()},
                         outputArena);
                 },
                 "overlapping support/type output spans")
        && passed;

    std::vector<std::uint8_t> support{9U, 9U};
    std::vector<SupportType> untouchedTypes{
        SupportType::InternalVoid,
        SupportType::InternalVoid};
    model[1] = 2U;
    passed = ExpectThrowsInvalidArgument(
                 [&]() {
                     MaterializePreShapeSupportLayer(
                         plan,
                         0,
                         model,
                         upperBoundary,
                         support,
                         untouchedTypes);
                 },
                 "binary validation happens before output mutation")
        && passed;
    return ExpectTrue(
               support == std::vector<std::uint8_t>({9U, 9U})
                   && untouchedTypes
                       == std::vector<SupportType>({
                           SupportType::InternalVoid,
                           SupportType::InternalVoid}),
               "invalid input leaves caller outputs unchanged")
        && passed;
}

int ReferencePriority(const SupportType type)
{
    switch (type)
    {
        case SupportType::UnsupportedIsland:
            return 4;
        case SupportType::FullVerticalProjection:
            return 3;
        case SupportType::UpperProjection:
            return 2;
        case SupportType::BottomProjection:
            return 1;
        default:
            return 0;
    }
}

bool DeterministicFixtureMatchesBruteForce()
{
    constexpr std::size_t columnCount{257U};
    constexpr int layerCount{32};
    Facts facts;
    facts.layerCount = layerCount;
    facts.lower.resize(columnCount);
    facts.last.resize(columnCount);
    facts.upper.resize(columnCount);
    facts.unsupportedTop.resize(columnCount);
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    for (std::size_t index{0U}; index < columnCount; ++index)
    {
        if (index % 11U == 0U)
        {
            facts.lower[index] = -1;
            facts.last[index] = -1;
            facts.upper[index] = -1;
        }
        else
        {
            facts.lower[index] = static_cast<int>((index * 3U) % 9U);
            facts.last[index] = std::min(
                layerCount - 1,
                facts.lower[index]
                    + static_cast<int>((index * 5U) % 17U));
            facts.upper[index] = std::min(
                layerCount - 1,
                facts.last[index]
                    + static_cast<int>((index * 7U) % 5U));
        }
        if (facts.last[index] >= 1 && index % 4U == 0U)
        {
            const int sourceSpan = facts.last[index]
                - std::max(1, facts.lower[index]) + 1;
            facts.unsupportedTop[index] = std::max(1, facts.lower[index])
                + static_cast<int>(index % static_cast<std::size_t>(sourceSpan));
        }
        else
        {
            facts.unsupportedTop[index] = 0;
        }
    }

    const auto plan = BuildPlan(facts);
    std::vector<std::vector<std::uint8_t>> modelVolume(
        static_cast<std::size_t>(layerCount),
        std::vector<std::uint8_t>(columnCount, 0U));
    std::vector<std::vector<std::uint8_t>> upperBoundaryVolume(
        static_cast<std::size_t>(layerCount),
        std::vector<std::uint8_t>(columnCount, 0U));
    for (int layerIndex{0}; layerIndex < layerCount; ++layerIndex)
    {
        for (std::size_t index{0U}; index < columnCount; ++index)
        {
            const bool inRange = facts.lower[index] >= 0
                && layerIndex >= facts.lower[index]
                && layerIndex <= facts.last[index];
            const bool keepEndpoint = layerIndex == facts.lower[index]
                || layerIndex == facts.last[index]
                || layerIndex == facts.unsupportedTop[index];
            const std::uint8_t occupied = inRange
                    && (keepEndpoint
                        || (index + static_cast<std::size_t>(layerIndex) * 3U)
                                % 7U
                            != 0U)
                ? 1U
                : 0U;
            modelVolume[static_cast<std::size_t>(layerIndex)][index] = occupied;
            upperBoundaryVolume[static_cast<std::size_t>(layerIndex)][index] =
                occupied != 0U
                    || (facts.upper[index] >= 0
                        && layerIndex > facts.last[index]
                        && layerIndex <= facts.upper[index])
                ? 1U
                : 0U;
        }
    }

    std::vector<std::vector<std::uint8_t>> retainedSupport(
        static_cast<std::size_t>(layerCount),
        std::vector<std::uint8_t>(columnCount, 0U));
    std::vector<std::vector<SupportType>> retainedTypes(
        static_cast<std::size_t>(layerCount),
        std::vector<SupportType>(columnCount, SupportType::None));
    const auto setRetained =
        [&retainedSupport, &retainedTypes](
            const int layerIndex,
            const std::size_t index,
            const SupportType candidate) {
            auto& type = retainedTypes[static_cast<std::size_t>(layerIndex)][index];
            retainedSupport[static_cast<std::size_t>(layerIndex)][index] = 1U;
            if (ReferencePriority(candidate) >= ReferencePriority(type))
            {
                type = candidate;
            }
        };
    for (std::size_t index{0U}; index < columnCount; ++index)
    {
        for (int layerIndex{0}; layerIndex < facts.lower[index]; ++layerIndex)
        {
            if (modelVolume[static_cast<std::size_t>(layerIndex)][index] == 0U)
            {
                setRetained(layerIndex, index, SupportType::BottomProjection);
            }
        }
        for (int layerIndex{0}; layerIndex < facts.last[index]; ++layerIndex)
        {
            if (modelVolume[static_cast<std::size_t>(layerIndex)][index] == 0U)
            {
                setRetained(
                    layerIndex,
                    index,
                    SupportType::FullVerticalProjection);
            }
        }
        for (int layerIndex{facts.upper[index] + 1};
             facts.upper[index] >= 0 && layerIndex < layerCount;
             ++layerIndex)
        {
            if (upperBoundaryVolume[static_cast<std::size_t>(layerIndex)][index]
                == 0U)
            {
                setRetained(layerIndex, index, SupportType::UpperProjection);
            }
        }
        for (int layerIndex{0};
             layerIndex < facts.unsupportedTop[index];
             ++layerIndex)
        {
            if (modelVolume[static_cast<std::size_t>(layerIndex)][index] == 0U)
            {
                setRetained(layerIndex, index, SupportType::UnsupportedIsland);
            }
        }
    }

    std::vector<std::uint8_t> support(columnCount, 0U);
    std::vector<SupportType> observed(columnCount, SupportType::None);
    bool passed{true};
    for (int layerIndex{0}; layerIndex < layerCount; ++layerIndex)
    {
        MaterializePreShapeSupportLayer(
            plan,
            layerIndex,
            modelVolume[static_cast<std::size_t>(layerIndex)],
            upperBoundaryVolume[static_cast<std::size_t>(layerIndex)],
            support,
            observed);
        passed = ExpectTrue(
                     observed
                             == retainedTypes[static_cast<std::size_t>(layerIndex)]
                         && support
                             == retainedSupport[static_cast<std::size_t>(layerIndex)],
                     "257x32 fixture matches retained full-stack oracle")
            && passed;
    }
    return passed;
}

bool CallerBuffersAreReusableAndDeterministic()
{
    Facts facts{5, {2, -1, 1}, {4, -1, 3}, {4, -1, 3}, {0, 0, 2}};
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.upperEnabled = true;
    facts.unsupportedEnabled = true;
    const auto plan = BuildPlan(facts);
    const std::vector<std::uint8_t> model{0U, 0U, 0U};
    std::vector<std::uint8_t> support(plan.ColumnCount(), 0U);
    std::vector<SupportType> types(plan.ColumnCount(), SupportType::None);
    const auto* const supportAddress = support.data();
    const auto* const typeAddress = types.data();
    MaterializePreShapeSupportLayer(plan, 1, model, model, support, types);
    const auto firstSupport = support;
    const auto firstTypes = types;
    std::fill(
        support.begin(),
        support.end(),
        static_cast<std::uint8_t>(7U));
    std::fill(types.begin(), types.end(), SupportType::InternalVoid);
    MaterializePreShapeSupportLayer(plan, 1, model, model, support, types);
    return ExpectTrue(
               support.data() == supportAddress && types.data() == typeAddress,
               "caller-owned buffers retain their addresses")
        && ExpectTrue(
            support == firstSupport && types == firstTypes,
            "repeated materialization is deterministic and overwrites outputs");
}

bool PlanOwnsFactsAndSurvivesMove()
{
    Facts facts{5, {2}, {4}, {4}, {3}};
    facts.lowerEnabled = true;
    facts.fullEnabled = true;
    facts.unsupportedEnabled = true;
    auto plan = BuildPlan(facts);
    facts.lower.front() = 0;
    facts.last.front() = 0;
    facts.unsupportedTop.front() = 0;

    BoundedSupportDemandPlan movedPlan{std::move(plan)};
    const auto types = MaterializeTypes(movedPlan, 1, {0U});
    return ExpectTrue(
        types.front() == SupportType::UnsupportedIsland,
        "plan owns copied request facts and remains usable after move");
}

}  // namespace

int main()
{
    bool passed{true};
    passed = ExactHalfOpenBoundaries() && passed;
    passed = ModelPriorityAndTypeCollisions() && passed;
    passed = BoundaryExtremesAndInternalModelGaps() && passed;
    passed = UpperBoundaryOccupancyOnlySuppressesUpper() && passed;
    passed = DisabledModesAndEmptyColumns() && passed;
    passed = InvalidInputsFailClosed() && passed;
    passed = OverlapAndExceptionGuaranteesFailClosed() && passed;
    passed = DeterministicFixtureMatchesBruteForce() && passed;
    passed = CallerBuffersAreReusableAndDeterministic() && passed;
    passed = PlanOwnsFactsAndSurvivesMove() && passed;
    if (passed)
    {
        std::cout << "PASS MF-03B1 bounded support demand tests\n";
        return 0;
    }
    return 1;
}
