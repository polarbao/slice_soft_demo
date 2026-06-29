#include "slicer_core/geometry/TriangleIntersectionQuery.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace slicer_core
{
namespace
{

struct Aabb
{
    Vec3 min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    Vec3 max{-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()};
};

struct Vec2
{
    double x{0.0};
    double y{0.0};
};

Vec3 Subtract(const Vec3& left, const Vec3& right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 Add(const Vec3& left, const Vec3& right)
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 Multiply(const Vec3& value, const double scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

double Dot(const Vec3& left, const Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 Cross(const Vec3& left, const Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

double Length(const Vec3& value)
{
    return std::sqrt(Dot(value, value));
}

void Expand(Aabb& bounds, const Vec3& point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

Aabb MakeBounds(
    const TriangleMeshData& mesh,
    const std::array<int, 3>& triangle,
    const double padding)
{
    Aabb bounds;
    for (const int vertexIndex : triangle)
    {
        Expand(bounds, mesh.vertices.at(static_cast<std::size_t>(vertexIndex)));
    }
    bounds.min.x -= padding;
    bounds.min.y -= padding;
    bounds.min.z -= padding;
    bounds.max.x += padding;
    bounds.max.y += padding;
    bounds.max.z += padding;
    return bounds;
}

bool AabbOverlap(const Aabb& left, const Aabb& right)
{
    return left.min.x <= right.max.x && left.max.x >= right.min.x
        && left.min.y <= right.max.y && left.max.y >= right.min.y
        && left.min.z <= right.max.z && left.max.z >= right.min.z;
}

std::array<Vec3, 3> TrianglePoints(
    const TriangleMeshData& mesh,
    const std::array<int, 3>& triangle)
{
    return {
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(0))),
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(1))),
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(2))),
    };
}

Vec3 TriangleNormal(const std::array<Vec3, 3>& triangle)
{
    return Cross(Subtract(triangle.at(1), triangle.at(0)), Subtract(triangle.at(2), triangle.at(0)));
}

double SignedDistanceToPlane(
    const Vec3& normal,
    const Vec3& planePoint,
    const Vec3& point)
{
    const double normalLength = Length(normal);
    if (normalLength <= 0.0)
    {
        return 0.0;
    }
    return Dot(normal, Subtract(point, planePoint)) / normalLength;
}

bool AllNearPlane(
    const std::array<Vec3, 3>& triangle,
    const Vec3& normal,
    const Vec3& planePoint,
    const double epsilonMm)
{
    return std::abs(SignedDistanceToPlane(normal, planePoint, triangle.at(0))) <= epsilonMm
        && std::abs(SignedDistanceToPlane(normal, planePoint, triangle.at(1))) <= epsilonMm
        && std::abs(SignedDistanceToPlane(normal, planePoint, triangle.at(2))) <= epsilonMm;
}

int DominantProjectionAxis(const Vec3& normal)
{
    const double ax = std::abs(normal.x);
    const double ay = std::abs(normal.y);
    const double az = std::abs(normal.z);
    if (ax >= ay && ax >= az)
    {
        return 0;
    }
    if (ay >= ax && ay >= az)
    {
        return 1;
    }
    return 2;
}

Vec2 ProjectPoint(const Vec3& point, const int dropAxis)
{
    if (dropAxis == 0)
    {
        return {point.y, point.z};
    }
    if (dropAxis == 1)
    {
        return {point.x, point.z};
    }
    return {point.x, point.y};
}

double Cross2D(const Vec2& a, const Vec2& b, const Vec2& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool PointOnSegment2D(const Vec2& point, const Vec2& a, const Vec2& b, const double epsilon)
{
    if (std::abs(Cross2D(a, b, point)) > epsilon)
    {
        return false;
    }
    return point.x >= std::min(a.x, b.x) - epsilon
        && point.x <= std::max(a.x, b.x) + epsilon
        && point.y >= std::min(a.y, b.y) - epsilon
        && point.y <= std::max(a.y, b.y) + epsilon;
}

bool SegmentsIntersect2D(
    const Vec2& a0,
    const Vec2& a1,
    const Vec2& b0,
    const Vec2& b1,
    const double epsilon)
{
    const double d1 = Cross2D(a0, a1, b0);
    const double d2 = Cross2D(a0, a1, b1);
    const double d3 = Cross2D(b0, b1, a0);
    const double d4 = Cross2D(b0, b1, a1);
    if (((d1 > epsilon && d2 < -epsilon) || (d1 < -epsilon && d2 > epsilon))
        && ((d3 > epsilon && d4 < -epsilon) || (d3 < -epsilon && d4 > epsilon)))
    {
        return true;
    }
    return PointOnSegment2D(b0, a0, a1, epsilon)
        || PointOnSegment2D(b1, a0, a1, epsilon)
        || PointOnSegment2D(a0, b0, b1, epsilon)
        || PointOnSegment2D(a1, b0, b1, epsilon);
}

bool PointInTriangle2D(
    const Vec2& point,
    const std::array<Vec2, 3>& triangle,
    const double epsilon)
{
    const double c0 = Cross2D(triangle.at(0), triangle.at(1), point);
    const double c1 = Cross2D(triangle.at(1), triangle.at(2), point);
    const double c2 = Cross2D(triangle.at(2), triangle.at(0), point);
    const bool hasNegative = c0 < -epsilon || c1 < -epsilon || c2 < -epsilon;
    const bool hasPositive = c0 > epsilon || c1 > epsilon || c2 > epsilon;
    return !(hasNegative && hasPositive);
}

double PolygonArea2D(const std::vector<Vec2>& polygon)
{
    if (polygon.size() < 3U)
    {
        return 0.0;
    }
    double area{0.0};
    for (std::size_t index{0}; index < polygon.size(); ++index)
    {
        const Vec2& current = polygon.at(index);
        const Vec2& next = polygon.at((index + 1U) % polygon.size());
        area += current.x * next.y - next.x * current.y;
    }
    return std::abs(area) * 0.5;
}

Vec2 LineIntersection2D(const Vec2& p0, const Vec2& p1, const Vec2& q0, const Vec2& q1)
{
    const double a1 = p1.y - p0.y;
    const double b1 = p0.x - p1.x;
    const double c1 = a1 * p0.x + b1 * p0.y;
    const double a2 = q1.y - q0.y;
    const double b2 = q0.x - q1.x;
    const double c2 = a2 * q0.x + b2 * q0.y;
    const double determinant = a1 * b2 - a2 * b1;
    if (std::abs(determinant) <= 1.0e-18)
    {
        return p1;
    }
    return {
        (b2 * c1 - b1 * c2) / determinant,
        (a1 * c2 - a2 * c1) / determinant,
    };
}

std::vector<Vec2> ClipPolygon(
    const std::vector<Vec2>& input,
    const Vec2& edgeStart,
    const Vec2& edgeEnd,
    const double orientation,
    const double epsilon)
{
    std::vector<Vec2> output;
    if (input.empty())
    {
        return output;
    }
    auto inside = [&](const Vec2& point)
    {
        return orientation * Cross2D(edgeStart, edgeEnd, point) >= -epsilon;
    };

    Vec2 previous = input.back();
    bool previousInside = inside(previous);
    for (const Vec2& current : input)
    {
        const bool currentInside = inside(current);
        if (currentInside != previousInside)
        {
            output.push_back(LineIntersection2D(previous, current, edgeStart, edgeEnd));
        }
        if (currentInside)
        {
            output.push_back(current);
        }
        previous = current;
        previousInside = currentInside;
    }
    return output;
}

TriangleIntersectionKind CoplanarIntersectionKind(
    const std::array<Vec3, 3>& left,
    const std::array<Vec3, 3>& right,
    const Vec3& normal,
    const double epsilonMm)
{
    const int dropAxis = DominantProjectionAxis(normal);
    const std::array<Vec2, 3> left2{
        ProjectPoint(left.at(0), dropAxis),
        ProjectPoint(left.at(1), dropAxis),
        ProjectPoint(left.at(2), dropAxis),
    };
    const std::array<Vec2, 3> right2{
        ProjectPoint(right.at(0), dropAxis),
        ProjectPoint(right.at(1), dropAxis),
        ProjectPoint(right.at(2), dropAxis),
    };
    const double orientation = Cross2D(left2.at(0), left2.at(1), left2.at(2)) >= 0.0 ? 1.0 : -1.0;
    std::vector<Vec2> clipped{right2.at(0), right2.at(1), right2.at(2)};
    for (std::size_t edge{0}; edge < 3U; ++edge)
    {
        clipped = ClipPolygon(clipped, left2.at(edge), left2.at((edge + 1U) % 3U), orientation, epsilonMm);
    }
    if (PolygonArea2D(clipped) > epsilonMm * epsilonMm)
    {
        return TriangleIntersectionKind::CoplanarOverlap;
    }
    for (std::size_t leftEdge{0}; leftEdge < 3U; ++leftEdge)
    {
        for (std::size_t rightEdge{0}; rightEdge < 3U; ++rightEdge)
        {
            if (SegmentsIntersect2D(
                    left2.at(leftEdge),
                    left2.at((leftEdge + 1U) % 3U),
                    right2.at(rightEdge),
                    right2.at((rightEdge + 1U) % 3U),
                    epsilonMm))
            {
                return TriangleIntersectionKind::TouchingOnly;
            }
        }
    }
    for (const Vec2& point : left2)
    {
        if (PointInTriangle2D(point, right2, epsilonMm))
        {
            return TriangleIntersectionKind::TouchingOnly;
        }
    }
    for (const Vec2& point : right2)
    {
        if (PointInTriangle2D(point, left2, epsilonMm))
        {
            return TriangleIntersectionKind::TouchingOnly;
        }
    }
    return TriangleIntersectionKind::AabbOnly;
}

bool SegmentTriangleIntersection(
    const Vec3& segmentStart,
    const Vec3& segmentEnd,
    const std::array<Vec3, 3>& triangle,
    const double epsilonMm,
    bool& touchingOnly)
{
    const Vec3 direction = Subtract(segmentEnd, segmentStart);
    const Vec3 edge1 = Subtract(triangle.at(1), triangle.at(0));
    const Vec3 edge2 = Subtract(triangle.at(2), triangle.at(0));
    const Vec3 pvec = Cross(direction, edge2);
    const double determinant = Dot(edge1, pvec);
    if (std::abs(determinant) <= epsilonMm)
    {
        return false;
    }
    const double inverse = 1.0 / determinant;
    const Vec3 tvec = Subtract(segmentStart, triangle.at(0));
    const double u = Dot(tvec, pvec) * inverse;
    if (u < -epsilonMm || u > 1.0 + epsilonMm)
    {
        return false;
    }
    const Vec3 qvec = Cross(tvec, edge1);
    const double v = Dot(direction, qvec) * inverse;
    if (v < -epsilonMm || u + v > 1.0 + epsilonMm)
    {
        return false;
    }
    const double t = Dot(edge2, qvec) * inverse;
    if (t < -epsilonMm || t > 1.0 + epsilonMm)
    {
        return false;
    }

    const double w = 1.0 - u - v;
    touchingOnly = t <= epsilonMm || t >= 1.0 - epsilonMm
        || u <= epsilonMm || v <= epsilonMm || w <= epsilonMm;
    (void)Add(segmentStart, Multiply(direction, t));
    return true;
}

TriangleIntersectionKind NonCoplanarIntersectionKind(
    const std::array<Vec3, 3>& left,
    const std::array<Vec3, 3>& right,
    const double epsilonMm)
{
    bool foundTouch{false};
    for (std::size_t edge{0}; edge < 3U; ++edge)
    {
        bool touching{false};
        if (SegmentTriangleIntersection(left.at(edge), left.at((edge + 1U) % 3U), right, epsilonMm, touching))
        {
            if (!touching)
            {
                return TriangleIntersectionKind::ConfirmedIntersection;
            }
            foundTouch = true;
        }
        if (SegmentTriangleIntersection(right.at(edge), right.at((edge + 1U) % 3U), left, epsilonMm, touching))
        {
            if (!touching)
            {
                return TriangleIntersectionKind::ConfirmedIntersection;
            }
            foundTouch = true;
        }
    }
    return foundTouch ? TriangleIntersectionKind::TouchingOnly : TriangleIntersectionKind::AabbOnly;
}

}  // namespace

bool TrianglesShareVertexIndex(
    const std::array<int, 3>& left,
    const std::array<int, 3>& right)
{
    for (const int leftIndex : left)
    {
        for (const int rightIndex : right)
        {
            if (leftIndex == rightIndex)
            {
                return true;
            }
        }
    }
    return false;
}

TriangleIntersectionResult TestTriangleIntersection(
    const TriangleMeshData& mesh,
    const std::size_t leftIndex,
    const std::size_t rightIndex,
    const double epsilonMm)
{
    const std::array<int, 3>& leftIndices = mesh.triangles.at(leftIndex);
    const std::array<int, 3>& rightIndices = mesh.triangles.at(rightIndex);
    const Aabb leftBounds = MakeBounds(mesh, leftIndices, epsilonMm);
    const Aabb rightBounds = MakeBounds(mesh, rightIndices, epsilonMm);
    TriangleIntersectionResult result;
    result.aabb_candidate = AabbOverlap(leftBounds, rightBounds);
    if (!result.aabb_candidate)
    {
        return result;
    }

    const std::array<Vec3, 3> left = TrianglePoints(mesh, leftIndices);
    const std::array<Vec3, 3> right = TrianglePoints(mesh, rightIndices);
    const Vec3 leftNormal = TriangleNormal(left);
    const Vec3 rightNormal = TriangleNormal(right);
    if (Length(leftNormal) <= epsilonMm || Length(rightNormal) <= epsilonMm)
    {
        result.kind = TriangleIntersectionKind::TouchingOnly;
        return result;
    }

    if (AllNearPlane(left, rightNormal, right.at(0), epsilonMm)
        && AllNearPlane(right, leftNormal, left.at(0), epsilonMm))
    {
        result.kind = CoplanarIntersectionKind(left, right, leftNormal, epsilonMm);
        return result;
    }

    result.kind = NonCoplanarIntersectionKind(left, right, epsilonMm);
    return result;
}

}  // namespace slicer_core
