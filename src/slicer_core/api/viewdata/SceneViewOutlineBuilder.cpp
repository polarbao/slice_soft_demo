#include "slicer_core/api/viewdata/SceneViewOutlineBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 文件职责：从表面预览提取确定性的闭合局部轮廓；
// 边界：轮廓仅供视图与诊断使用，不替代生产几何边界。
namespace slicer_core::api::viewdata_detail
{
namespace
{

struct GridVertex
{
    int x{0};
    int y{0};
};

struct GridVertexLess
{
    bool operator()(const GridVertex& left, const GridVertex& right) const noexcept
    {
        return left.x < right.x
            || (left.x == right.x && left.y < right.y);
    }
};

struct BoundaryEdge
{
    GridVertex start;
    GridVertex end;
    bool used{false};
};

struct OutlineRecord
{
    ViewOutline outline;
    long double absolute_area{0.0L};
    long double signed_area{0.0L};
};

template <class T>
ApiResult<T> Failure(
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure({
        "PM-SLICER-INPUT-0002",
        std::string(message),
        detail});
}

bool IsSame(const GridVertex& left, const GridVertex& right) noexcept
{
    return left.x == right.x && left.y == right.y;
}

bool IsOccupied(
    const SurfacePreview& preview,
    const int x,
    const int y) noexcept
{
    if (x < 0 || y < 0 || x >= preview.width_px || y >= preview.height_px)
    {
        return false;
    }

    const std::size_t pixel =
        static_cast<std::size_t>(y)
            * static_cast<std::size_t>(preview.width_px)
        + static_cast<std::size_t>(x);
    return preview.rgba8.at(pixel * 4U + 3U) != 0U;
}

void AddBoundaryEdges(
    const SurfacePreview& preview,
    std::vector<BoundaryEdge>& edges)
{
    for (int y{0}; y < preview.height_px; ++y)
    {
        for (int x{0}; x < preview.width_px; ++x)
        {
            if (!IsOccupied(preview, x, y))
            {
                continue;
            }

            if (!IsOccupied(preview, x, y - 1))
            {
                edges.push_back({{x + 1, y}, {x, y}, false});
            }
            if (!IsOccupied(preview, x + 1, y))
            {
                edges.push_back({{x + 1, y + 1}, {x + 1, y}, false});
            }
            if (!IsOccupied(preview, x, y + 1))
            {
                edges.push_back({{x, y + 1}, {x + 1, y + 1}, false});
            }
            if (!IsOccupied(preview, x - 1, y))
            {
                edges.push_back({{x, y}, {x, y + 1}, false});
            }
        }
    }
}

int DirectionIndex(const GridVertex& start, const GridVertex& end) noexcept
{
    const int dx = end.x - start.x;
    const int dy = end.y - start.y;
    if (dx > 0)
    {
        return 0;
    }
    if (dy > 0)
    {
        return 1;
    }
    if (dx < 0)
    {
        return 2;
    }
    return 3;
}

int TurnPriority(
    const BoundaryEdge& current,
    const BoundaryEdge& candidate) noexcept
{
    const int incoming = DirectionIndex(current.start, current.end);
    const int outgoing = DirectionIndex(candidate.start, candidate.end);
    const int delta = (outgoing - incoming + 4) % 4;
    if (delta == 3)
    {
        return 0;
    }
    if (delta == 0)
    {
        return 1;
    }
    if (delta == 1)
    {
        return 2;
    }
    return 3;
}

std::optional<std::size_t> SelectNextEdge(
    const BoundaryEdge& current,
    const std::vector<std::size_t>& candidates,
    const std::vector<BoundaryEdge>& edges)
{
    std::optional<std::size_t> selected;
    int selectedPriority{std::numeric_limits<int>::max()};
    GridVertexLess less;

    for (const std::size_t candidateIndex : candidates)
    {
        const BoundaryEdge& candidate = edges.at(candidateIndex);
        if (candidate.used)
        {
            continue;
        }

        const int priority = TurnPriority(current, candidate);
        if (!selected.has_value()
            || priority < selectedPriority
            || (priority == selectedPriority
                && less(candidate.end, edges.at(*selected).end)))
        {
            selected = candidateIndex;
            selectedPriority = priority;
        }
    }
    return selected;
}

bool IsRemovableCollinear(
    const GridVertex& previous,
    const GridVertex& current,
    const GridVertex& next) noexcept
{
    const std::int64_t ax =
        static_cast<std::int64_t>(current.x) - previous.x;
    const std::int64_t ay =
        static_cast<std::int64_t>(current.y) - previous.y;
    const std::int64_t bx =
        static_cast<std::int64_t>(next.x) - current.x;
    const std::int64_t by =
        static_cast<std::int64_t>(next.y) - current.y;
    const std::int64_t cross = ax * by - ay * bx;
    const std::int64_t dot = ax * bx + ay * by;
    return cross == 0 && dot >= 0;
}

void SimplifyLoop(std::vector<GridVertex>& points)
{
    if (points.size() > 1U && IsSame(points.front(), points.back()))
    {
        points.pop_back();
    }

    points.erase(
        std::unique(
            points.begin(),
            points.end(),
            [](const GridVertex& left, const GridVertex& right)
            {
                return IsSame(left, right);
            }),
        points.end());

    bool changed{true};
    while (changed && points.size() >= 3U)
    {
        changed = false;
        for (std::size_t index{0U}; index < points.size(); ++index)
        {
            const std::size_t previous =
                (index + points.size() - 1U) % points.size();
            const std::size_t next = (index + 1U) % points.size();
            if (IsRemovableCollinear(
                    points.at(previous),
                    points.at(index),
                    points.at(next)))
            {
                points.erase(
                    points.begin() + static_cast<std::ptrdiff_t>(index));
                changed = true;
                break;
            }
        }
    }
}

void CanonicalizeLoop(std::vector<GridVertex>& points)
{
    GridVertexLess less;
    const auto first = std::min_element(points.begin(), points.end(), less);
    std::rotate(points.begin(), first, points.end());
}

std::array<double, 2> ToLocalPoint(
    const GridVertex& point,
    const SurfacePreview& preview)
{
    const double widthMm =
        preview.local_bounds_mm.max_mm.at(0U)
        - preview.local_bounds_mm.min_mm.at(0U);
    const double heightMm =
        preview.local_bounds_mm.max_mm.at(1U)
        - preview.local_bounds_mm.min_mm.at(1U);
    return {
        preview.local_bounds_mm.min_mm.at(0U)
            + widthMm * static_cast<double>(point.x)
                / static_cast<double>(preview.width_px),
        preview.local_bounds_mm.max_mm.at(1U)
            - heightMm * static_cast<double>(point.y)
                / static_cast<double>(preview.height_px)};
}

long double SignedArea(const std::vector<GridVertex>& points) noexcept
{
    long double twiceArea{0.0L};
    for (std::size_t index{0U}; index < points.size(); ++index)
    {
        const GridVertex& current = points.at(index);
        const GridVertex& next = points.at((index + 1U) % points.size());
        twiceArea += static_cast<long double>(current.x) * next.y
            - static_cast<long double>(next.x) * current.y;
    }
    return -twiceArea * 0.5L;
}

OutlineRecord BuildOutlineRecord(
    std::vector<GridVertex> points,
    const SurfacePreview& preview)
{
    SimplifyLoop(points);
    CanonicalizeLoop(points);

    OutlineRecord record;
    record.signed_area = SignedArea(points);
    record.absolute_area = std::abs(record.signed_area);
    record.outline.points_mm.reserve(points.size() + 1U);
    for (const GridVertex& point : points)
    {
        record.outline.points_mm.push_back(ToLocalPoint(point, preview));
    }
    record.outline.points_mm.push_back(record.outline.points_mm.front());
    return record;
}

bool OutlineLess(const OutlineRecord& left, const OutlineRecord& right)
{
    if (left.absolute_area != right.absolute_area)
    {
        return left.absolute_area > right.absolute_area;
    }
    const auto& leftFirst = left.outline.points_mm.front();
    const auto& rightFirst = right.outline.points_mm.front();
    if (leftFirst != rightFirst)
    {
        return leftFirst < rightFirst;
    }
    if (left.signed_area != right.signed_area)
    {
        return left.signed_area > right.signed_area;
    }
    return left.outline.points_mm < right.outline.points_mm;
}

ApiResult<std::vector<ViewOutline>> TraceOutlines(
    const SurfacePreview& preview,
    std::vector<BoundaryEdge> edges)
{
    std::map<GridVertex, std::vector<std::size_t>, GridVertexLess> outgoing;
    for (std::size_t index{0U}; index < edges.size(); ++index)
    {
        outgoing[edges.at(index).start].push_back(index);
    }

    std::vector<OutlineRecord> records;
    for (std::size_t startEdge{0U}; startEdge < edges.size(); ++startEdge)
    {
        if (edges.at(startEdge).used)
        {
            continue;
        }

        const GridVertex first = edges.at(startEdge).start;
        std::vector<GridVertex> points{first};
        std::size_t currentIndex = startEdge;
        bool closed{false};

        for (std::size_t traversed{0U}; traversed <= edges.size(); ++traversed)
        {
            BoundaryEdge& current = edges.at(currentIndex);
            if (current.used)
            {
                break;
            }
            current.used = true;
            points.push_back(current.end);
            if (IsSame(current.end, first))
            {
                closed = true;
                break;
            }

            const auto found = outgoing.find(current.end);
            if (found == outgoing.end())
            {
                break;
            }
            const std::optional<std::size_t> next = SelectNextEdge(
                current,
                found->second,
                edges);
            if (!next.has_value())
            {
                break;
            }
            currentIndex = *next;
        }

        if (!closed)
        {
            return Failure<std::vector<ViewOutline>>(
                "Surface preview alpha boundary is not closed",
                preview.preview_identity);
        }

        SimplifyLoop(points);
        if (points.size() < 3U)
        {
            continue;
        }
        records.push_back(BuildOutlineRecord(std::move(points), preview));
    }

    std::sort(records.begin(), records.end(), OutlineLess);
    std::vector<ViewOutline> outlines;
    outlines.reserve(records.size());
    for (OutlineRecord& record : records)
    {
        outlines.push_back(std::move(record.outline));
    }
    return ApiResult<std::vector<ViewOutline>>::Success(std::move(outlines));
}

ApiResult<void> ValidatePreview(const SurfacePreview& preview)
{
    if (preview.width_px <= 0 || preview.height_px <= 0)
    {
        return Failure<void>(
            "Surface preview dimensions must be positive",
            preview.preview_identity);
    }

    const std::size_t width = static_cast<std::size_t>(preview.width_px);
    const std::size_t height = static_cast<std::size_t>(preview.height_px);
    if (width > std::numeric_limits<std::size_t>::max() / height
        || width * height
            > std::numeric_limits<std::size_t>::max() / 4U
        || preview.rgba8.size() != width * height * 4U)
    {
        return Failure<void>(
            "Surface preview RGBA8 payload size is invalid",
            preview.preview_identity);
    }

    const double minX = preview.local_bounds_mm.min_mm.at(0U);
    const double minY = preview.local_bounds_mm.min_mm.at(1U);
    const double maxX = preview.local_bounds_mm.max_mm.at(0U);
    const double maxY = preview.local_bounds_mm.max_mm.at(1U);
    if (!std::isfinite(minX) || !std::isfinite(minY)
        || !std::isfinite(maxX) || !std::isfinite(maxY)
        || maxX <= minX || maxY <= minY)
    {
        return Failure<void>(
            "Surface preview local XY bounds are invalid",
            preview.preview_identity);
    }
    return ApiResult<void>::Success();
}

}  // namespace

ApiResult<std::vector<ViewOutline>> BuildViewOutlines(
    const SurfacePreview& preview) noexcept
{
    try
    {
        const ApiResult<void> validation = ValidatePreview(preview);
        if (!validation.IsOk())
        {
            return ApiResult<std::vector<ViewOutline>>::Failure(
                *validation.Error());
        }

        std::vector<BoundaryEdge> edges;
        AddBoundaryEdges(preview, edges);
        if (edges.empty())
        {
            return ApiResult<std::vector<ViewOutline>>::Success({});
        }
        return TraceOutlines(preview, std::move(edges));
    }
    catch (const std::exception& error)
    {
        return Failure<std::vector<ViewOutline>>(
            "Failed to extract surface preview outlines",
            error.what());
    }
    catch (...)
    {
        return Failure<std::vector<ViewOutline>>(
            "Failed to extract surface preview outlines",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::viewdata_detail
