#include "slicer_core/model/FrameGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{
Vec3 Rotate(const Vec3& point, const std::array<double, 3>& degrees)
{
    Vec3 result = point;
    const double rx = degrees[0] * std::numbers::pi_v<double> / 180.0;
    const double ry = degrees[1] * std::numbers::pi_v<double> / 180.0;
    const double rz = degrees[2] * std::numbers::pi_v<double> / 180.0;
    result = {result.x, result.y * std::cos(rx) - result.z * std::sin(rx),
              result.y * std::sin(rx) + result.z * std::cos(rx)};
    result = {result.x * std::cos(ry) + result.z * std::sin(ry), result.y,
              -result.x * std::sin(ry) + result.z * std::cos(ry)};
    return {result.x * std::cos(rz) - result.y * std::sin(rz),
            result.x * std::sin(rz) + result.y * std::cos(rz), result.z};
}
}

bool IsFrameMaterial(const std::string_view name) noexcept
{
    return name == "nail-Default";
}

void IncludeFramePointXY(BoundingBox& bounds, const Vec3& point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
    {
        throw std::runtime_error("E_FRAME_GEOMETRY_NONFINITE: frame coordinates must be finite");
    }
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
}

ImportedFrameGeometry ExtractFrameGeometry(
    std::vector<Vec3>& vertices,
    std::vector<std::array<std::size_t, 3>>& faces,
    std::vector<TriangleTextureInfo>& textures)
{
    ImportedFrameGeometry frame;
    if (std::none_of(textures.begin(), textures.end(), [](const auto& info)
        { return IsFrameMaterial(info.material_name); }))
    {
        return frame;
    }
    if (textures.size() != faces.size())
    {
        throw std::runtime_error("E_FRAME_BINDING_INVALID: frame face bindings are not aligned");
    }
    std::vector<unsigned char> printable(vertices.size(), 0U);
    frame.sourcevertexcount = vertices.size();
    std::vector<unsigned char> locator(vertices.size(), 0U);
    std::size_t write = 0U;
    for (std::size_t index = 0U; index < faces.size(); ++index)
    {
        const bool isFrame = IsFrameMaterial(textures[index].material_name);
        for (const std::size_t vertex : faces[index])
        {
            (isFrame ? locator : printable).at(vertex) = 1U;
        }
        if (isFrame)
        {
            ++frame.trianglecount;
            continue;
        }
        if (write != index)
        {
            faces[write] = faces[index];
            textures[write] = std::move(textures[index]);
        }
        ++write;
    }
    if (write == 0U)
    {
        throw std::runtime_error("E_FRAME_NO_PRINTABLE_GEOMETRY: frame-only assets cannot be sliced");
    }
    faces.resize(write);
    textures.resize(write);
    frame.printableanchor = vertices.at(faces.front()[0]);
    std::vector<std::size_t> mapping(vertices.size(), 0U);
    std::size_t vertexWrite = 0U;
    for (std::size_t index = 0U; index < vertices.size(); ++index)
    {
        if (locator[index] != 0U)
        {
            frame.vertices.push_back(vertices[index]);
        }
        if (printable[index] != 0U)
        {
            mapping[index] = vertexWrite;
            vertices[vertexWrite++] = vertices[index];
        }
    }
    vertices.resize(vertexWrite);
    for (auto& face : faces)
    {
        for (auto& vertex : face)
        {
            vertex = mapping[vertex];
        }
    }
    return frame;
}

ModelReport AttachFrameGeometry(ModelReport report, ImportedFrameGeometry frame)
{
    // Reserved declarations are metadata, even when no face references them.
    std::erase_if(report.materials, [](const auto& material)
        { return IsFrameMaterial(material.name); });
    std::erase_if(report.material_infos, [](const auto& material)
        { return IsFrameMaterial(material.name); });
    if (frame.vertices.empty())
    {
        return report;
    }
    report.frame_triangle_count = frame.trianglecount;
    // Vertex/face/UV counts remain source statistics; triangle_count is printable.
    report.vertex_count = frame.sourcevertexcount;
    const Vec3 anchor = Rotate(frame.printableanchor, report.auto_orient.rotation_deg);
    const Vec3 orientedAnchor = report.triangles.at(0).a;
    for (Vec3& point : frame.vertices)
    {
        point = Rotate(point, report.auto_orient.rotation_deg);
        point = {point.x + orientedAnchor.x - anchor.x,
                 point.y + orientedAnchor.y - anchor.y,
                 point.z + orientedAnchor.z - anchor.z};
        IncludeFramePointXY(report.bbox_mm, point);
    }
    report.frame_vertices = std::move(frame.vertices);
    return report;
}
}
