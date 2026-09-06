#include "slicer_core/support/BoundedSupportShapeScan.h"

#include "slicer_core/support/SupportShapePipeline.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace slicer_core;

std::atomic<bool> gTrackAllocations{false};
std::atomic<std::size_t> gAllocationCount{0U};

void RecordAllocation() noexcept
{
    if (gTrackAllocations.load(std::memory_order_relaxed))
    {
        gAllocationCount.fetch_add(1U, std::memory_order_relaxed);
    }
}

std::size_t Index(const int width, const int x, const int y) noexcept
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

template <typename Exception>
bool ExpectThrows(
    const std::function<void()>& operation,
    const std::string& message)
{
    try
    {
        operation();
    }
    catch (const Exception&)
    {
        return true;
    }
    catch (...)
    {
    }
    std::cerr << "FAIL " << message << '\n';
    return false;
}

/**
 * @param demandPixels 哪些列有支撑需求。默认只有 0 号 —— 那正是原用例的形状，
 *        而它导致 pre-shape support 只有一个孤立像素，随即被
 *        `min_component_area_px = 2` 剔除干净，于是形状优化的四项加法运算
 *        （膨胀、闭运算、水平/垂直桥接）**全部空转**，added 恒为 0。
 *        换句话说，这条「retained oracle 对拍」此前只验到了最小面积剔除一步。
 *        要激活加法，需求像素必须落在模型【外侧】且彼此连通到不被剔除
 *        —— 模型是个 5x5 方框，被它围住的空腔四面都是模型，膨胀无处可写。
 */
BoundedSupportDemandPlan BuildPlan(
    const int layerCount,
    const std::size_t pixelCount,
    const bool lowerEnabled = true,
    const std::vector<std::size_t>& demandPixels = {0U})
{
    std::vector<int> lower(pixelCount, -1);
    std::vector<int> last(pixelCount, -1);
    std::vector<int> upper(pixelCount, -1);
    std::vector<int> unsupported(pixelCount, 0);
    for (const std::size_t pixel : demandPixels)
    {
        if (pixel >= pixelCount)
        {
            continue;
        }
        lower[pixel] = layerCount - 1;
        last[pixel] = layerCount - 1;
        upper[pixel] = layerCount - 1;
    }
    BoundedSupportDemandRequest request;
    request.layerCount = layerCount;
    request.lowerSourceLayers = lower;
    request.modelLastLayers = last;
    request.upperBoundaryLastLayers = upper;
    request.unsupportedTopExclusiveLayers = unsupported;
    request.lowerEnabled = lowerEnabled;
    return BuildBoundedSupportDemandPlan(request);
}

void SetSupport(
    std::vector<std::uint8_t>& mask,
    std::vector<SupportType>& types,
    const std::size_t index,
    const SupportType type)
{
    mask[index] = 1U;
    if (SupportTypePriority(type) >= SupportTypePriority(types[index]))
    {
        types[index] = type;
    }
}

void AddInternalVoidReference(
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::uint8_t>& model,
    std::vector<std::uint8_t>& support,
    std::vector<SupportType>& types)
{
    if (!request.internalVoid.enabled)
    {
        return;
    }
    const std::size_t count{model.size()};
    std::vector<std::uint8_t> external(count, 0U);
    std::vector<std::uint8_t> visited(count, 0U);
    std::vector<int> stack;
    const auto pushExternal = [&](const int x, const int y)
    {
        if (x < 0 || x >= request.widthPx || y < 0 || y >= request.heightPx)
        {
            return;
        }
        const std::size_t index{Index(request.widthPx, x, y)};
        if (model[index] == 0U && external[index] == 0U)
        {
            external[index] = 1U;
            stack.push_back(static_cast<int>(index));
        }
    };
    for (int x{0}; x < request.widthPx; ++x)
    {
        pushExternal(x, 0);
        pushExternal(x, request.heightPx - 1);
    }
    for (int y{0}; y < request.heightPx; ++y)
    {
        pushExternal(0, y);
        pushExternal(request.widthPx - 1, y);
    }
    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    constexpr std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};
    const auto visitNeighbors = [&](const int current, const auto& visitor)
    {
        const int x{current % request.widthPx};
        const int y{current / request.widthPx};
        if (request.connectivity == 8)
        {
            for (const auto& delta : neighbors8)
            {
                visitor(x + delta[0], y + delta[1]);
            }
        }
        else
        {
            for (const auto& delta : neighbors4)
            {
                visitor(x + delta[0], y + delta[1]);
            }
        }
    };
    while (!stack.empty())
    {
        const int current{stack.back()};
        stack.pop_back();
        visitNeighbors(current, pushExternal);
    }
    for (std::size_t start{0U}; start < count; ++start)
    {
        if (model[start] != 0U || external[start] != 0U || visited[start] != 0U)
        {
            continue;
        }
        std::vector<int> component;
        stack.push_back(static_cast<int>(start));
        visited[start] = 1U;
        while (!stack.empty())
        {
            const int current{stack.back()};
            stack.pop_back();
            component.push_back(current);
            const auto pushComponent = [&](const int x, const int y)
            {
                if (x < 0 || x >= request.widthPx
                    || y < 0 || y >= request.heightPx)
                {
                    return;
                }
                const std::size_t next{Index(request.widthPx, x, y)};
                if (model[next] == 0U && external[next] == 0U
                    && visited[next] == 0U)
                {
                    visited[next] = 1U;
                    stack.push_back(static_cast<int>(next));
                }
            };
            visitNeighbors(current, pushComponent);
        }
        if (static_cast<int>(component.size()) < request.internalVoid.min_area_px)
        {
            continue;
        }
        for (const int pixel : component)
        {
            SetSupport(
                support,
                types,
                static_cast<std::size_t>(pixel),
                SupportType::InternalVoid);
        }
    }
}

void SynchronizeTypesReference(
    const std::vector<std::uint8_t>& original,
    const std::vector<std::uint8_t>& optimized,
    std::vector<SupportType>& types)
{
    for (std::size_t index{0U}; index < optimized.size(); ++index)
    {
        if (optimized[index] == 0U)
        {
            types[index] = SupportType::None;
        }
        else if (original[index] == 0U && types[index] == SupportType::None)
        {
            types[index] = SupportType::BottomProjection;
        }
    }
}

class CapturingSink final : public BoundedSupportShapeReportSink
{
public:
    void Consume(const BoundedSupportShapeLayerReport& report) override
    {
        reports.push_back(report);
    }
    std::vector<BoundedSupportShapeLayerReport> reports;
};

class ThrowingSink final : public BoundedSupportShapeReportSink
{
public:
    void Consume(const BoundedSupportShapeLayerReport&) override
    {
        throw std::runtime_error("sink failure");
    }
};

bool CompactReportMatches(
    const BoundedSupportShapeLayerReport& actual,
    const SupportShapeLayerReport& expected,
    const int layerIndex)
{
    bool matches{actual.layerIndex == layerIndex
        && actual.pre.componentCount == expected.pre.component_count
        && actual.pre.largestComponentArea == expected.pre.largest_component_area
        && actual.pre.smallComponentCount == expected.pre.small_component_count
        && actual.pre.tinyComponentCount == expected.pre.tiny_component_count
        && actual.post.componentCount == expected.post.component_count
        && actual.post.largestComponentArea == expected.post.largest_component_area
        && actual.addedSupportPixels == expected.added_support_pixels
        && actual.removedSupportPixels == expected.removed_support_pixels
        && actual.pre.components.size() == expected.pre.components.size()
        && actual.post.components.size() == expected.post.components.size()
        && actual.filteredComponents.size() == expected.filtered_components.size()
        && actual.bridgedGaps.size() == expected.bridged_gaps.size()
        && actual.warnings == expected.warnings};
    for (std::size_t index{0U}; matches && index < actual.pre.components.size(); ++index)
    {
        const auto& left{actual.pre.components[index]};
        const auto& right{expected.pre.components[index]};
        matches = left.areaPx == right.area_px
            && left.minX == right.min_x && left.minY == right.min_y
            && left.maxX == right.max_x && left.maxY == right.max_y;
    }
    // 以下三处此前【只比 size 不比内容】—— 那意味着两份实现只要产出的条目数
    // 相同，内容与顺序全错也照样通过。桥接记录尤其危险：它的顺序由「水平全扫完
    // 再走垂直」这条同序约定决定，而那正是两份实现最容易分家的地方。
    for (std::size_t index{0U}; matches && index < actual.post.components.size(); ++index)
    {
        const auto& left{actual.post.components[index]};
        const auto& right{expected.post.components[index]};
        matches = left.areaPx == right.area_px
            && left.minX == right.min_x && left.minY == right.min_y
            && left.maxX == right.max_x && left.maxY == right.max_y;
    }
    if (!matches)
    {
        std::cout << "  [diag] post.components 内容不一致\n";
        return false;
    }
    for (std::size_t index{0U};
         matches && index < actual.filteredComponents.size();
         ++index)
    {
        const auto& left{actual.filteredComponents[index]};
        const auto& right{expected.filtered_components[index]};
        // ⚠ layer_index 不能直接对比：oracle `OptimizeSupportShapeForLayer` 是
        //   「假单层」—— 它把单层包成一元 vector 再走整栈实现，故其报告里的
        //   layer_index 【恒为 0】，而 scanner 报的是真实层号。补齐本比对时正是
        //   在这里先红的（scanner 报 1、oracle 报 0，面积与 bbox 完全一致）。
        //   故分别断言两侧各自的正确值，把这个 oracle 缺陷钉住：一旦哪天 oracle
        //   改成报真实层号，这里会立刻红，提醒把断言改回直接相等。
        matches = left.layerIndex == layerIndex && right.layer_index == 0
            && left.areaPx == right.area_px
            && left.minX == right.min_x && left.minY == right.min_y
            && left.maxX == right.max_x && left.maxY == right.max_y;
        if (!matches)
        {
            std::cout << "  [diag] filteredComponents[" << index
                      << "] layer " << left.layerIndex << " vs "
                      << right.layer_index << ", area " << left.areaPx
                      << " vs " << right.area_px << ", bbox ("
                      << left.minX << "," << left.minY << ")-(" << left.maxX
                      << "," << left.maxY << ") vs (" << right.min_x << ","
                      << right.min_y << ")-(" << right.max_x << ","
                      << right.max_y << ")\n";
        }
    }
    for (std::size_t index{0U}; matches && index < actual.bridgedGaps.size(); ++index)
    {
        const auto& left{actual.bridgedGaps[index]};
        const auto& right{expected.bridged_gaps[index]};
        // 同上：oracle 的 layer_index 恒为 0。
        matches = left.layerIndex == layerIndex && right.layer_index == 0
            && left.x0 == right.x0 && left.y0 == right.y0
            && left.x1 == right.x1 && left.y1 == right.y1
            && left.gapPx == right.gap_px
            && left.direction == right.direction;
        if (!matches)
        {
            std::cout << "  [diag] bridgedGaps[" << index << "] layer "
                      << left.layerIndex << " vs " << right.layer_index
                      << ", (" << left.x0 << "," << left.y0 << ")-("
                      << left.x1 << "," << left.y1 << ") gap " << left.gapPx
                      << " " << left.direction << "  vs  (" << right.x0 << ","
                      << right.y0 << ")-(" << right.x1 << "," << right.y1
                      << ") gap " << right.gap_px << " " << right.direction
                      << "\n";
        }
    }
    return matches;
}

/**
 * @param maxAddedSupportRatio 超比例回滚的阈值。
 *
 * 原用例只跑 20.0 —— 那个值大到**回滚永不触发**，于是形状优化的第八步
 * （added 超限则把本层新增全部撤销）从来没有被对拍覆盖过。它恰恰是两份实现
 * 里判据最多的一步（分母、floor、守卫、回滚循环、两条告警字符串），
 * 也是唯一会写 warnings 的一步。故改为参数化，两个档各跑一遍。
 */
bool RetainedOracleAndFootprintWithRatio(
    const double maxAddedSupportRatio,
    const bool expectRollback,
    const bool expectAdditions,
    const std::vector<std::size_t>& demandPixels)
{
    constexpr int width{7};
    constexpr int height{7};
    constexpr int layers{3};
    const std::size_t count{static_cast<std::size_t>(width * height)};
    auto plan{BuildPlan(layers, count, true, demandPixels)};
    BoundedSupportShapeScanRequest request;
    request.widthPx = width;
    request.heightPx = height;
    request.layerCount = layers;
    request.connectivity = 4;
    request.internalVoid.enabled = true;
    request.internalVoid.fill_rule = "all_internal_voids";
    request.internalVoid.min_area_px = 9;
    request.shape.enabled = true;
    request.shape.min_component_area_px = 2;
    request.shape.xy_dilation_px = 1;
    request.shape.closing_radius_px = 1;
    request.shape.bridge_gap_px = 1;
    request.shape.max_added_support_ratio = maxAddedSupportRatio;

    std::vector<std::vector<std::uint8_t>> models(
        layers, std::vector<std::uint8_t>(count, 0U));
    for (int layer{0}; layer < layers; ++layer)
    {
        for (int x{1}; x <= 5; ++x)
        {
            models[layer][Index(width, x, 1)] = 1U;
            models[layer][Index(width, x, 5)] = 1U;
        }
        for (int y{1}; y <= 5; ++y)
        {
            models[layer][Index(width, 1, y)] = 1U;
            models[layer][Index(width, 5, y)] = 1U;
        }
    }
    models[2][Index(width, 3, 3)] = 1U;
    const auto upper{models};
    CapturingSink sink;
    BoundedSupportShapeScanner scanner{request, plan, &sink};
    std::vector<std::uint8_t> output(count, 0U);
    std::vector<SupportType> outputTypes(count, SupportType::None);
    std::vector<std::uint8_t> expectedFootprint(count, 0U);
    bool passed{true};
    for (int layer{0}; layer < layers; ++layer)
    {
        std::vector<std::uint8_t> expected(count, 0U);
        std::vector<SupportType> expectedTypes(count, SupportType::None);
        MaterializePreShapeSupportLayer(
            plan, layer, models[layer], upper[layer], expected, expectedTypes);
        AddInternalVoidReference(
            request, models[layer], expected, expectedTypes);
        const auto original{expected};
        const SupportShapeOptimizationResult expectedShape{
            OptimizeSupportShapeForLayer(
            request.shape,
            models[layer],
            expected,
            width,
            height,
            request.connectivity)};
        SynchronizeTypesReference(original, expected, expectedTypes);
        const std::size_t reportsBefore{sink.reports.size()};
        scanner.ConsumeLayer(
            layer, models[layer], upper[layer], output, outputTypes);
        passed = Expect(output == expected, "bounded mask matches retained layer") && passed;
        passed = Expect(outputTypes == expectedTypes, "bounded type matches retained layer") && passed;
        if (!expectedShape.layers.empty())
        {
            passed = Expect(
                sink.reports.size() == reportsBefore + 1U
                    && CompactReportMatches(
                        sink.reports.back(), expectedShape.layers.front(), layer),
                "compact report matches retained report fields") && passed;
        }
        for (std::size_t index{0U}; index < count; ++index)
        {
            expectedFootprint[index] = expectedFootprint[index] != 0U
                    || expected[index] != 0U
                ? 1U
                : 0U;
        }
    }
    auto result{std::move(scanner).Finish()};
    passed = Expect(
        std::vector<std::uint8_t>(
            result.SupportFootprint().begin(), result.SupportFootprint().end())
            == expectedFootprint,
        "post-shape footprint matches retained union") && passed;
    passed = Expect(result.ReplayDigests().size() == layers, "one digest per layer") && passed;
    passed = Expect(!sink.reports.empty(), "compact reports are handed to sink") && passed;
    if (!sink.reports.empty())
    {
        passed = Expect(
            !sink.reports.front().pre.components.empty(),
            "compact report retains component summaries") && passed;
    }
    // 证明这个参数不是摆设：小 ratio 档必须真的触发回滚，大 ratio 档必须不触发。
    // 少了这一条，把 ratio 改成 0.05 只是换了个数字 —— 第八步依旧没被覆盖，
    // 而测试照样绿。回滚是唯一会写 warnings 的一步，故用它作判据。
    const bool anyRollback = std::any_of(
        sink.reports.begin(),
        sink.reports.end(),
        [](const BoundedSupportShapeLayerReport& report)
        { return !report.warnings.empty(); });
    passed = Expect(
        anyRollback == expectRollback,
        expectRollback
            ? "small maxAddedSupportRatio actually triggers the rollback branch"
            : "large maxAddedSupportRatio leaves the rollback branch untaken")
        && passed;
    // 证明四项加法运算真的写入了像素。少了这一条，夹具只要几何稍变成
    // 「加法无处可写」，对拍就退化成只验最小面积剔除 —— 而它照样全绿。
    // 注意只能在【未回滚】的档上判：回滚会把 added 清零。
    const bool anyAdditions = std::any_of(
        sink.reports.begin(),
        sink.reports.end(),
        [](const BoundedSupportShapeLayerReport& report)
        { return report.addedSupportPixels > 0; });
    passed = Expect(
        anyAdditions == expectAdditions,
        expectAdditions
            ? "shape additions (dilation/closing/bridging) actually fire"
            : "enclosed-cavity fixture leaves shape additions untaken")
        && passed;
    return passed;
}

bool RetainedOracleAndFootprint()
{
    // 20.0：回滚不触发，覆盖正常路径。
    // 0.05：回滚必然触发（本夹具每层新增远超 pre 支撑像素数的 5%），
    //       覆盖第八步及其两条告警字符串 —— 告警内容也在 CompactReportMatches
    //       的 `actual.warnings == expected.warnings` 里逐字比对。
    // 7x7 网格。0 号是 (0,0)、7 号是 (0,1)：都在 5x5 模型方框【外侧】的左列，
    // 两者 4-连通故面积 2、不被 min_component_area_px=2 剔除，且其邻域
    // (0,2)/(1,0) 是空的 —— 膨胀有处可写。
    const std::vector<std::size_t> enclosedOnly{0U};
    const std::vector<std::size_t> outsideEdge{0U, 7U};

    // 档一：保留原夹具。加法空转（空腔四面是模型），只验最小面积剔除。
    bool passed{RetainedOracleAndFootprintWithRatio(
        20.0, false, false, enclosedOnly)};
    // 档二：加法真正生效，且 ratio 大到不回滚 —— 这一档才验到膨胀/闭运算/桥接。
    passed = RetainedOracleAndFootprintWithRatio(
        20.0, false, true, outsideEdge) && passed;
    // 档三：同一夹具配 ratio=0，maxAdded 恒为 0，回滚必然触发。
    passed = RetainedOracleAndFootprintWithRatio(
        0.0, true, false, outsideEdge) && passed;
    return passed;
}

std::vector<BoundedSupportReplayDigest> RunDigestCase(
    const bool addUpperBoundaryPixel,
    const int minimumComponentArea)
{
    constexpr int width{4};
    constexpr int layers{2};
    const std::size_t count{16U};
    auto plan{BuildPlan(layers, count)};
    BoundedSupportShapeScanRequest request;
    request.widthPx = width;
    request.heightPx = width;
    request.layerCount = layers;
    request.shape.enabled = true;
    request.shape.min_component_area_px = minimumComponentArea;
    request.shape.max_added_support_ratio = 2.0;
    BoundedSupportShapeScanner scanner{request, plan};
    std::vector<std::uint8_t> model(count, 0U);
    std::vector<std::uint8_t> upper(count, 0U);
    std::vector<std::uint8_t> output(count, 0U);
    std::vector<SupportType> types(count, SupportType::None);
    if (addUpperBoundaryPixel)
    {
        upper[3] = 1U;
    }
    for (int layer{0}; layer < layers; ++layer)
    {
        scanner.ConsumeLayer(layer, model, upper, output, types);
    }
    auto result{std::move(scanner).Finish()};
    return {result.ReplayDigests().begin(), result.ReplayDigests().end()};
}

bool DigestIsDeterministicAndCoversInputs()
{
    const auto first{RunDigestCase(false, 0)};
    const auto repeated{RunDigestCase(false, 0)};
    const auto upperChanged{RunDigestCase(true, 0)};
    const auto policyChanged{RunDigestCase(false, 2)};
    return Expect(first == repeated, "digest is deterministic")
        && Expect(first != upperChanged, "digest covers upper-boundary input")
        && Expect(first != policyChanged, "digest covers policy/report domain");
}

std::string DigestHex(const BoundedSupportReplayDigest& digest)
{
    static constexpr char digits[]{"0123456789abcdef"};
    std::string result;
    result.reserve(digest.size() * 2U);
    for (const std::uint8_t byte : digest)
    {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

bool CanonicalDigestGolden()
{
    const auto digests{RunDigestCase(false, 0)};
    const std::string actual{DigestHex(digests.front())};
    constexpr std::string_view expected{
        "7425ddc31356405e69a2811b666c3fcb6527ab22c2ebb6f5762fa48650719af7"};
    if (actual != expected)
    {
        std::cerr << "DIGEST " << actual << '\n';
        return false;
    }
    return true;
}

bool ScratchBuffersAvoidMaskAllocations()
{
    auto plan{BuildPlan(2, 16U)};
    BoundedSupportShapeScanRequest request;
    request.widthPx = 4;
    request.heightPx = 4;
    request.layerCount = 2;
    request.internalVoid.enabled = false;
    request.shape.enabled = false;
    BoundedSupportShapeScanner scanner{request, plan};
    std::vector<std::uint8_t> model(16U, 0U);
    std::vector<std::uint8_t> upper(16U, 0U);
    std::vector<std::uint8_t> output(16U, 0U);
    std::vector<SupportType> types(16U, SupportType::None);
    const auto* const outputAddress{output.data()};
    const auto* const typeAddress{types.data()};
    gAllocationCount.store(0U, std::memory_order_relaxed);
    gTrackAllocations.store(true, std::memory_order_relaxed);
    scanner.ConsumeLayer(0, model, upper, output, types);
    scanner.ConsumeLayer(1, model, upper, output, types);
    gTrackAllocations.store(false, std::memory_order_relaxed);
    return Expect(
               gAllocationCount.load(std::memory_order_relaxed) == 0U,
               "mask-only ConsumeLayer path performs no allocation")
        && Expect(output.data() == outputAddress && types.data() == typeAddress,
                  "caller buffers retain their addresses");
}

bool ValidationIsRetryable()
{
    constexpr int layers{2};
    auto plan{BuildPlan(layers, 4U)};
    BoundedSupportShapeScanRequest request;
    request.widthPx = 2;
    request.heightPx = 2;
    request.layerCount = layers;
    BoundedSupportShapeScanner scanner{request, plan};
    std::vector<std::uint8_t> model(4U, 0U);
    std::vector<std::uint8_t> upper(4U, 0U);
    std::vector<std::uint8_t> output(4U, 9U);
    std::vector<SupportType> types(4U, SupportType::ProjectionBase);
    model[0] = 2U;
    bool passed{ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, model, upper, output, types); },
        "non-binary input fails")};
    passed = Expect(scanner.ExpectedLayerIndex() == 0, "invalid input does not advance") && passed;
    passed = Expect(output[0] == 9U, "invalid input preserves caller output") && passed;
    model[0] = 0U;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(1, model, upper, output, types); },
        "out-of-order layer fails") && passed;
    std::span<std::uint8_t> aliased{model};
    passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, model, upper, aliased, types); },
        "input/output alias fails") && passed;
    scanner.ConsumeLayer(0, model, upper, output, types);
    scanner.ConsumeLayer(1, model, upper, output, types);
    static_cast<void>(std::move(scanner).Finish());
    passed = ExpectThrows<std::logic_error>(
        [&] { static_cast<void>(std::move(scanner).Finish()); },
        "second Finish fails closed") && passed;
    passed = ExpectThrows<std::logic_error>(
        [&] { scanner.ConsumeLayer(1, model, upper, output, types); },
        "Consume after Finish fails closed") && passed;

    auto incompletePlan{BuildPlan(layers, 4U)};
    BoundedSupportShapeScanner incomplete{request, incompletePlan};
    passed = ExpectThrows<std::logic_error>(
        [&] { static_cast<void>(std::move(incomplete).Finish()); },
        "early Finish fails") && passed;
    request.inputKind = GeometryOccupancyInputKind::GeneralMesh;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { BoundedSupportShapeScanner rejected{request, incompletePlan}; },
        "GeneralMesh fails closed") && passed;
    return passed;
}

bool SinkFailureTerminatesScanner()
{
    auto plan{BuildPlan(2, 9U)};
    BoundedSupportShapeScanRequest request;
    request.widthPx = 3;
    request.heightPx = 3;
    request.layerCount = 2;
    request.shape.enabled = true;
    request.shape.min_component_area_px = 2;
    std::vector<std::uint8_t> model(9U, 0U);
    std::vector<std::uint8_t> upper(9U, 0U);
    std::vector<std::uint8_t> output(9U, 7U);
    std::vector<SupportType> types(9U, SupportType::ProjectionBase);
    ThrowingSink sink;
    BoundedSupportShapeScanner scanner{request, plan, &sink};
    bool passed{ExpectThrows<std::runtime_error>(
        [&] { scanner.ConsumeLayer(0, model, upper, output, types); },
        "sink exception propagates")};
    passed = Expect(output[0] == 7U, "sink failure preserves caller output") && passed;
    passed = ExpectThrows<std::logic_error>(
        [&] { scanner.ConsumeLayer(0, model, upper, output, types); },
        "failed scanner rejects retry") && passed;
    passed = ExpectThrows<std::logic_error>(
        [&] { static_cast<void>(std::move(scanner).Finish()); },
        "failed scanner cannot Finish") && passed;
    return passed;
}

}  // namespace

void* operator new(const std::size_t size)
{
    RecordAllocation();
    if (void* const memory{std::malloc(size)})
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* const memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* const memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void* const memory, const std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* const memory, const std::size_t) noexcept
{
    ::operator delete(memory);
}

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 6> tests{{
        {"retained_oracle_and_footprint", RetainedOracleAndFootprint},
        {"digest_domain", DigestIsDeterministicAndCoversInputs},
        {"digest_golden", CanonicalDigestGolden},
        {"scratch_reuse", ScratchBuffersAvoidMaskAllocations},
        {"validation_retry", ValidationIsRetryable},
        {"sink_failure", SinkFailureTerminatesScanner},
    }};
    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (!test())
            {
                ++failed;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    if (failed == 0)
    {
        std::cout << "PASS BoundedSupportShapeScanTests " << tests.size()
                  << "/" << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL BoundedSupportShapeScanTests " << failed << " failed\n";
    return 1;
}
