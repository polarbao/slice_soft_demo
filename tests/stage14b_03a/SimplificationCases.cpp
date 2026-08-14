// 本文件验证网格简化后的拓扑、边界和确定性；这些断言用于阻止按步长丢三角的
// 历史实现重新进入视图数据路径。
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace stage14b03a
{
namespace
{

using Edge = std::pair<std::uint32_t, std::uint32_t>;

Edge MakeEdge(const std::uint32_t first, const std::uint32_t second)
{
    return first < second
        ? Edge{first, second}
        : Edge{second, first};
}

std::size_t FindRoot(
    std::vector<std::size_t>& parents,
    const std::size_t index)
{
    if (parents.at(index) != index)
    {
        parents.at(index) = FindRoot(parents, parents.at(index));
    }
    return parents.at(index);
}

void Join(
    std::vector<std::size_t>& parents,
    const std::size_t left,
    const std::size_t right)
{
    const std::size_t leftRoot = FindRoot(parents, left);
    const std::size_t rightRoot = FindRoot(parents, right);
    if (leftRoot != rightRoot)
    {
        parents.at(rightRoot) = leftRoot;
    }
}

struct TopologyCheck
{
    std::size_t component_count{0U};
    bool has_isolated_triangle{false};
};

TopologyCheck CheckSubmeshTopology(
    const slicer_core::api::ViewMesh& mesh,
    const slicer_core::api::ViewSubmesh& submesh)
{
    const std::size_t triangleCount = submesh.index_count / 3U;
    const std::size_t first = submesh.first_index;
    std::vector<std::size_t> parents(triangleCount);
    std::iota(parents.begin(), parents.end(), 0U);
    std::vector<bool> hasNeighbor(triangleCount, false);
    std::map<Edge, std::vector<std::size_t>> edgeTriangles;
    for (std::size_t triangle{0U}; triangle < triangleCount; ++triangle)
    {
        const std::size_t offset = first + triangle * 3U;
        const std::array<std::uint32_t, 3> vertices{
            mesh.indices.at(offset),
            mesh.indices.at(offset + 1U),
            mesh.indices.at(offset + 2U)};
        for (std::size_t edge{0U}; edge < 3U; ++edge)
        {
            edgeTriangles[MakeEdge(
                vertices.at(edge),
                vertices.at((edge + 1U) % 3U))].push_back(triangle);
        }
    }
    for (const auto& [edge, triangles] : edgeTriangles)
    {
        static_cast<void>(edge);
        if (triangles.size() < 2U)
        {
            continue;
        }
        for (std::size_t index{1U}; index < triangles.size(); ++index)
        {
            Join(parents, triangles.front(), triangles.at(index));
        }
        for (const std::size_t triangle : triangles)
        {
            hasNeighbor.at(triangle) = true;
        }
    }
    std::set<std::size_t> roots;
    for (std::size_t triangle{0U}; triangle < triangleCount; ++triangle)
    {
        roots.emplace(FindRoot(parents, triangle));
    }
    return {
        roots.size(),
        std::find(hasNeighbor.begin(), hasNeighbor.end(), false)
            != hasNeighbor.end()};
}

void AddGridTriangle(
    slicer_core::SceneModel& model,
    const slicer_core::Triangle& triangle,
    const std::array<slicer_core::TexCoord, 3>& uv,
    const std::string& materialName)
{
    model.triangles.push_back(triangle);
    slicer_core::TriangleTextureInfo binding;
    binding.has_uv = true;
    binding.material_name = materialName;
    binding.uv = uv;
    model.triangle_textures.push_back(std::move(binding));
}

slicer_core::SceneModel MakeGridModel(
    const bool multiMaterial,
    const bool uvSeam)
{
    constexpr std::size_t columns{80U};
    constexpr std::size_t rows{100U};
    constexpr std::size_t materialBoundary{60U};
    constexpr std::size_t seamColumn{40U};
    constexpr std::size_t triangleCount{columns * rows * 2U};

    slicer_core::SceneModel model;
    model.model_path = uvSeam
        ? "uv-seam-grid.obj"
        : "multi-material-grid.obj";
    model.format = "obj";
    model.vertex_count = (columns + 1U) * (rows + 1U);
    model.face_count = triangleCount;
    model.triangle_count = triangleCount;
    model.bbox_mm = {{0.0, 0.0, 0.0}, {80.0, 100.0, 0.0}};

    slicer_core::MaterialInfo first;
    first.name = "material-a";
    first.has_diffuse = true;
    first.diffuse_rgb = {200U, 80U, 40U};
    model.material_infos.push_back(first);
    if (multiMaterial)
    {
        slicer_core::MaterialInfo second;
        second.name = "material-b";
        second.has_diffuse = true;
        second.diffuse_rgb = {40U, 120U, 220U};
        model.material_infos.push_back(second);
    }

    model.triangles.reserve(triangleCount);
    model.triangle_textures.reserve(triangleCount);
    for (std::size_t y{0U}; y < rows; ++y)
    {
        for (std::size_t x{0U}; x < columns; ++x)
        {
            const double left = static_cast<double>(x);
            const double right = static_cast<double>(x + 1U);
            const double bottom = static_cast<double>(y);
            const double top = static_cast<double>(y + 1U);
            const std::string& materialName = multiMaterial
                && x >= materialBoundary
                ? model.material_infos.at(1U).name
                : model.material_infos.front().name;
            const double uvLeft = uvSeam && x >= seamColumn
                ? static_cast<double>(x - seamColumn)
                    / static_cast<double>(columns - seamColumn)
                : static_cast<double>(x)
                    / static_cast<double>(uvSeam ? seamColumn : columns);
            const double uvRight = uvSeam && x >= seamColumn
                ? static_cast<double>(x + 1U - seamColumn)
                    / static_cast<double>(columns - seamColumn)
                : static_cast<double>(x + 1U)
                    / static_cast<double>(uvSeam ? seamColumn : columns);
            const double uvBottom = bottom / static_cast<double>(rows);
            const double uvTop = top / static_cast<double>(rows);
            AddGridTriangle(
                model,
                {{left, bottom, 0.0},
                 {right, bottom, 0.0},
                 {right, top, 0.0}},
                {{{uvLeft, uvBottom},
                  {uvRight, uvBottom},
                  {uvRight, uvTop}}},
                materialName);
            AddGridTriangle(
                model,
                {{left, bottom, 0.0},
                 {right, top, 0.0},
                 {left, top, 0.0}},
                {{{uvLeft, uvBottom},
                  {uvRight, uvTop},
                  {uvLeft, uvTop}}},
                materialName);
        }
    }
    return model;
}

slicer_core::SceneModel MakeDisconnectedModel()
{
    constexpr std::size_t triangleCount{10001U};
    slicer_core::SceneModel model;
    model.model_path = "disconnected-triangles.obj";
    model.format = "obj";
    model.vertex_count = triangleCount * 3U;
    model.face_count = triangleCount;
    model.triangle_count = triangleCount;
    model.bbox_mm = {{0.0, 0.0, 0.0}, {101.0, 100.0, 0.0}};
    slicer_core::MaterialInfo material;
    material.name = "material";
    material.has_diffuse = true;
    material.diffuse_rgb = {128U, 128U, 128U};
    model.material_infos.push_back(material);
    for (std::size_t index{0U}; index < triangleCount; ++index)
    {
        const double x = static_cast<double>(index % 101U);
        const double y = static_cast<double>(index / 101U);
        model.triangles.push_back({
            {x, y, 0.0},
            {x + 0.4, y, 0.0},
            {x, y + 0.4, 0.0}});
        slicer_core::TriangleTextureInfo binding;
        binding.material_name = material.name;
        model.triangle_textures.push_back(std::move(binding));
    }
    return model;
}

void MultiMaterialTriangleBudgetCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeGridModel(true, false));
    auto textures = std::make_shared<TestTextureSource>();
    const auto provider = MakeProvider({{430U, model}}, textures);
    const TestCancelToken active;
    auto request = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    request.lod = slicer_core::api::ViewLod::Lod2;
    const auto result = provider->GetViewData(
        request,
        MakeSnapshot({{"multi-material-budget", 430U}}),
        active);
    Require(
        result.IsOk(),
        result.IsOk()
            ? "multi-material simplification should close"
            : "multi-material simplification failed: "
                + result.Error()->code + " " + result.Error()->message
                + " " + result.Error()->detail);
    const auto& mesh = result.Value()->meshes.front();
    Require(mesh.indices.size() / 3U >= 9500U
                && mesh.indices.size() / 3U <= 10000U,
            "multi-material simplification should stay within 5% of budget");
    Require(mesh.submeshes.size() == 2U,
            "multi-material simplification should retain both groups");
    const std::size_t firstCount =
        mesh.submeshes.at(0U).index_count / 3U;
    const std::size_t secondCount =
        mesh.submeshes.at(1U).index_count / 3U;
    Require(firstCount >= 7125U && firstCount <= 7500U
                && secondCount >= 2375U && secondCount <= 2500U,
            "material groups should remain within proportional budgets");
    for (const auto& submesh : mesh.submeshes)
    {
        const TopologyCheck topology = CheckSubmeshTopology(mesh, submesh);
        Require(topology.component_count == 1U,
                "simplified material group should remain connected");
        Require(!topology.has_isolated_triangle,
                "simplification must not create isolated triangles");
    }
    float minimumX = mesh.positions.at(0U);
    float maximumX = mesh.positions.at(0U);
    float minimumY = mesh.positions.at(1U);
    float maximumY = mesh.positions.at(1U);
    for (std::size_t vertex{0U}; vertex < mesh.positions.size() / 3U;
         ++vertex)
    {
        minimumX = std::min(minimumX, mesh.positions.at(vertex * 3U));
        maximumX = std::max(maximumX, mesh.positions.at(vertex * 3U));
        minimumY = std::min(minimumY, mesh.positions.at(vertex * 3U + 1U));
        maximumY = std::max(maximumY, mesh.positions.at(vertex * 3U + 1U));
    }
    Require(std::abs(minimumX) <= 1.0e-5F
                && std::abs(maximumX - 80.0F) <= 1.0e-5F
                && std::abs(minimumY) <= 1.0e-5F
                && std::abs(maximumY - 100.0F) <= 1.0e-5F,
            "locked border should retain an exact zero-drift outline");
}

void UvSeamSimplificationCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeGridModel(false, true));
    auto textures = std::make_shared<TestTextureSource>();
    const auto provider = MakeProvider({{431U, model}}, textures);
    const TestCancelToken active;
    auto request = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    request.lod = slicer_core::api::ViewLod::Lod2;
    const auto result = provider->GetViewData(
        request,
        MakeSnapshot({{"uv-seam-budget", 431U}}),
        active);
    Require(
        result.IsOk(),
        result.IsOk()
            ? "UV seam simplification should close"
            : "UV seam simplification failed: "
                + result.Error()->code + " " + result.Error()->message
                + " " + result.Error()->detail);
    const auto& mesh = result.Value()->meshes.front();
    bool foundZero{false};
    bool foundOne{false};
    for (std::size_t vertex{0U}; vertex < mesh.positions.size() / 3U;
         ++vertex)
    {
        const float x = mesh.positions.at(vertex * 3U);
        if (std::abs(x - 40.0F) > 1.0e-5F)
        {
            continue;
        }
        const float u = mesh.texcoord0.at(vertex * 2U);
        foundZero = foundZero || std::abs(u) <= 1.0e-5F;
        foundOne = foundOne || std::abs(u - 1.0F) <= 1.0e-5F;
    }
    Require(foundZero && foundOne,
            "simplification must preserve both sides of a UV seam");
}

void AutoSimplificationReasonCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeGridModel(false, false));
    auto textures = std::make_shared<TestTextureSource>();
    const auto provider = MakeProvider({{433U, model}}, textures);
    const TestCancelToken active;
    auto request = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    request.lod = slicer_core::api::ViewLod::Auto;
    request.max_bytes = 384U * 1024U;
    const auto result = provider->GetViewData(
        request,
        MakeSnapshot({{"auto-simplification-reason", 433U}}),
        active);
    Require(
        result.IsOk(),
        result.IsOk()
            ? "auto simplification should close"
            : "auto simplification failed: " + result.Error()->code
                + " " + result.Error()->message
                + " " + result.Error()->detail);
    Require(result.Value()->truncated,
            "auto simplification must report truncation");
    Require(
        result.Value()->truncation_reason
            == "mesh_simplified_lod2_for_max_bytes",
        "safe simplification must not use the legacy decimation reason: "
            + result.Value()->truncation_reason);
    Require(result.Value()->meshes.front().lod
                == slicer_core::api::ViewLod::Lod2,
            "auto simplification should select lod2 for the bounded fixture");
}

void UnsafeDisconnectedMeshCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeDisconnectedModel());
    auto textures = std::make_shared<TestTextureSource>();
    const auto provider = MakeProvider({{432U, model}}, textures);
    const TestCancelToken active;
    auto request = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    request.lod = slicer_core::api::ViewLod::Lod2;
    const auto result = provider->GetViewData(
        request,
        MakeSnapshot({{"disconnected-budget", 432U}}),
        active);
    Require(!result.IsOk(),
            "unsafe disconnected mesh must fail instead of jump sampling");
    Require(
        result.Error()->code == "PM-SLICER-VIEWDATA-SIMPLIFICATION",
        "unsafe disconnected simplification returned "
            + result.Error()->code + " " + result.Error()->message
            + " " + result.Error()->detail);
}

}  // namespace

void RunSimplificationCases()
{
    MultiMaterialTriangleBudgetCase();
    UvSeamSimplificationCase();
    AutoSimplificationReasonCase();
    UnsafeDisconnectedMeshCase();
}

}  // namespace stage14b03a
