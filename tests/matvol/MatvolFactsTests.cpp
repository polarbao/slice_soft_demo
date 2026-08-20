// MATVOL MV-01：资产事实、合成 fixture、旧顶面投影 baseline 与独立逐层 owner oracle。
//
// 本文件【不引入任何生产 API】，只在测试内复刻既有几何规则并建立独立对照，
// 用于把一次性人工诊断转成可跨机器重复的机器事实。
//
// 复刻的既有规则（源：src/slicer_core/slicer.cpp，均为匿名命名空间内实现，测试 TU 不可达）：
//   XY 采样中心    (x + 0.5, y + 0.5) * pixelSize + origin      slicer.cpp:1327-1332
//   重心坐标容差    epsilon = -1.0e-9；分母退化阈值 1.0e-12       slicer.cpp:1179-1199
//   顶面平局规则    zMm >= zMax，Z 相等时【三角面索引大者胜】      slicer.cpp:1343
//   层中心公式      z = (layerIndex + 0.5) * layerThicknessMm     slicer.cpp:1159
//   层区间换算      ceil(z/t - 0.5) / floor(z/t - 0.5)            slicer.cpp:1202-1208
//   Kd 量化        round(clamp(v,0,1) * 255)                     model.cpp:1702-1705

#include "slicer_core/model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::Triangle;
using slicer_core::Vec3;

// ---------------------------------------------------------------------------
// 断言辅助（仓库约定：手写断言，无第三方框架，进程返回码即结果）
// ---------------------------------------------------------------------------

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

// ---------------------------------------------------------------------------
// 复刻既有几何规则
// ---------------------------------------------------------------------------

constexpr double kBarycentricEpsilon{-1.0e-9};
constexpr double kDegenerateDenominator{1.0e-12};
constexpr double kCoincidentHitEpsilonMm{1.0e-9};

/// @brief 复刻 slicer.cpp:1179-1199 的 XY 重心坐标判定，容差与退化阈值必须逐字一致。
bool PointInTriangleXy(
    const double px,
    const double py,
    const Triangle& triangle,
    double& w0,
    double& w1,
    double& w2)
{
    const double denominator =
        (triangle.b.y - triangle.c.y) * (triangle.a.x - triangle.c.x)
        + (triangle.c.x - triangle.b.x) * (triangle.a.y - triangle.c.y);
    if (std::abs(denominator) < kDegenerateDenominator)
    {
        return false;
    }
    w0 = ((triangle.b.y - triangle.c.y) * (px - triangle.c.x)
          + (triangle.c.x - triangle.b.x) * (py - triangle.c.y))
        / denominator;
    w1 = ((triangle.c.y - triangle.a.y) * (px - triangle.c.x)
          + (triangle.a.x - triangle.c.x) * (py - triangle.c.y))
        / denominator;
    w2 = 1.0 - w0 - w1;
    return w0 >= kBarycentricEpsilon && w1 >= kBarycentricEpsilon && w2 >= kBarycentricEpsilon;
}

/// @brief 复刻 slicer.cpp:1202-1208 的层换算，禁止另造 Z 量化公式。
int FirstLayerAtOrAboveZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::ceil(zMm / layerThicknessMm - 0.5));
}

int LastLayerAtOrBelowZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::floor(zMm / layerThicknessMm - 0.5));
}

/// @brief 复刻 model.cpp:1702-1705 的 Kd 量化。
std::uint8_t KdComponentToU8(const double value)
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    return static_cast<std::uint8_t>(std::llround(clamped * 255.0));
}

// ---------------------------------------------------------------------------
// Fixture 数据模型
// ---------------------------------------------------------------------------

constexpr std::uint32_t kNoOwner{0xFFFFFFFFU};

/// @brief 合成 fixture 中的一种材质；priority 为显式优先级，数值大者胜。
struct FixtureMaterial
{
    std::string name;
    std::array<double, 3> kd{0.0, 0.0, 0.0};
    int priority{0};
};

/// @brief 合成网格：三角面与逐三角面材质下标并行，镜像 ModelReport 的
///        triangles / triangle_textures 索引并行不变量。
struct FixtureMesh
{
    std::string name;
    std::vector<Triangle> triangles;
    std::vector<std::uint32_t> triangleMaterial;
    std::vector<FixtureMaterial> materials;
};

/// @brief 采样网格与层参数，取小尺寸以便手算复核。
struct FixtureGrid
{
    int widthPx{4};
    int heightPx{4};
    double pixelSizeMm{1.0};
    double originXMm{0.0};
    double originYMm{0.0};
    double layerThicknessMm{1.0};
    int layerCount{8};
};

double PixelCenterX(const FixtureGrid& grid, const int x)
{
    return grid.originXMm + (static_cast<double>(x) + 0.5) * grid.pixelSizeMm;
}

double PixelCenterY(const FixtureGrid& grid, const int y)
{
    return grid.originYMm + (static_cast<double>(y) + 0.5) * grid.pixelSizeMm;
}

std::size_t PixelIndex(const FixtureGrid& grid, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(grid.widthPx)
        + static_cast<std::size_t>(x);
}

std::size_t PixelCount(const FixtureGrid& grid)
{
    return static_cast<std::size_t>(grid.widthPx) * static_cast<std::size_t>(grid.heightPx);
}

// ---------------------------------------------------------------------------
// 网格构造：显式三角面，外法向朝外，便于手算
// ---------------------------------------------------------------------------

void AppendTriangle(
    FixtureMesh& mesh,
    const std::uint32_t materialIndex,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c)
{
    mesh.triangles.push_back(Triangle{a, b, c});
    mesh.triangleMaterial.push_back(materialIndex);
}

/// @brief 追加一个轴对齐闭合盒体（12 个三角面）。
///        顶/底面沿【非对角线方向】切分，避免像素中心恰好落在共享边上。
void AppendClosedBox(
    FixtureMesh& mesh,
    const std::uint32_t materialIndex,
    const Vec3& lo,
    const Vec3& hi)
{
    const Vec3 v000{lo.x, lo.y, lo.z};
    const Vec3 v100{hi.x, lo.y, lo.z};
    const Vec3 v110{hi.x, hi.y, lo.z};
    const Vec3 v010{lo.x, hi.y, lo.z};
    const Vec3 v001{lo.x, lo.y, hi.z};
    const Vec3 v101{hi.x, lo.y, hi.z};
    const Vec3 v111{hi.x, hi.y, hi.z};
    const Vec3 v011{lo.x, hi.y, hi.z};

    // 底面（法向 -Z）
    AppendTriangle(mesh, materialIndex, v000, v110, v100);
    AppendTriangle(mesh, materialIndex, v000, v010, v110);
    // 顶面（法向 +Z）
    AppendTriangle(mesh, materialIndex, v001, v101, v111);
    AppendTriangle(mesh, materialIndex, v001, v111, v011);
    // 四个侧面：XY 退化，垂直射线不命中，符合既有 2.5D 采样跳过规则
    AppendTriangle(mesh, materialIndex, v000, v100, v101);
    AppendTriangle(mesh, materialIndex, v000, v101, v001);
    AppendTriangle(mesh, materialIndex, v100, v110, v111);
    AppendTriangle(mesh, materialIndex, v100, v111, v101);
    AppendTriangle(mesh, materialIndex, v110, v010, v011);
    AppendTriangle(mesh, materialIndex, v110, v011, v111);
    AppendTriangle(mesh, materialIndex, v010, v000, v001);
    AppendTriangle(mesh, materialIndex, v010, v001, v011);
}

/// @brief 追加一张【开放】水平表面（2 个三角面，无厚度、无体积）。
void AppendOpenSheet(
    FixtureMesh& mesh,
    const std::uint32_t materialIndex,
    const Vec3& lo,
    const double hiX,
    const double hiY)
{
    const Vec3 p00{lo.x, lo.y, lo.z};
    const Vec3 p10{hiX, lo.y, lo.z};
    const Vec3 p11{hiX, hiY, lo.z};
    const Vec3 p01{lo.x, hiY, lo.z};
    AppendTriangle(mesh, materialIndex, p00, p10, p11);
    AppendTriangle(mesh, materialIndex, p00, p11, p01);
}

// ---------------------------------------------------------------------------
// MV-F01..F06 合成 fixture（全部测试内构造，不依赖外部资产与 Golden）
// ---------------------------------------------------------------------------

const FixtureMaterial& GreenMaterial()
{
    // 与 03.mtl 的 usemtl 01 同值：Kd 0.2471 0.7451 0.4941 -> [63,190,126]
    static const FixtureMaterial material{"01", {0.2471, 0.7451, 0.4941}, 200};
    return material;
}

const FixtureMaterial& PeachMaterial()
{
    // 与 03.mtl 的 usemtl 02 同值：Kd 1.0000 0.8627 0.7765 -> [255,220,198]
    static const FixtureMaterial material{"02", {1.0000, 0.8627, 0.7765}, 100};
    return material;
}

/// @brief MV-F01：两个 Z 向不重叠的闭合盒体，同一 XY 列内随 Z 换材质。
FixtureMesh MakeFixtureF01()
{
    FixtureMesh mesh;
    mesh.name = "MV-F01";
    mesh.materials = {PeachMaterial(), GreenMaterial()};
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 2.0});
    AppendClosedBox(mesh, 1U, Vec3{0.0, 0.0, 3.0}, Vec3{4.0, 4.0, 5.0});
    return mesh;
}

/// @brief MV-F02：两个 Z 向重叠的闭合盒体，重叠层必须由显式优先级裁决。
FixtureMesh MakeFixtureF02()
{
    FixtureMesh mesh;
    mesh.name = "MV-F02";
    mesh.materials = {PeachMaterial(), GreenMaterial()};
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 3.0});
    AppendClosedBox(mesh, 1U, Vec3{0.0, 0.0, 2.0}, Vec3{4.0, 4.0, 5.0});
    return mesh;
}

/// @brief MV-F03：闭合主体 + 开放顶面，是 finger_suoguo/03.obj 的最小复现。
FixtureMesh MakeFixtureF03()
{
    FixtureMesh mesh;
    mesh.name = "MV-F03";
    mesh.materials = {PeachMaterial(), GreenMaterial()};
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 3.0});
    AppendOpenSheet(mesh, 1U, Vec3{0.0, 0.0, 3.0}, 4.0, 4.0);
    return mesh;
}

/// @brief MV-F04：分离的两段实体，单列产生 4 个交点与 2 个区间。
FixtureMesh MakeFixtureF04()
{
    FixtureMesh mesh;
    mesh.name = "MV-F04";
    mesh.materials = {PeachMaterial()};
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 2.0});
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 4.0}, Vec3{4.0, 4.0, 6.0});
    return mesh;
}

/// @brief MV-F05：纯白与近白材质，供 Stage 15 按需补白判据使用。
FixtureMesh MakeFixtureF05()
{
    FixtureMesh mesh;
    mesh.name = "MV-F05";
    mesh.materials = {
        FixtureMaterial{"pure_white", {1.0, 1.0, 1.0}, 100},
        FixtureMaterial{"near_white", {0.98, 0.98, 0.98}, 200},
    };
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 2.0});
    AppendClosedBox(mesh, 1U, Vec3{0.0, 0.0, 3.0}, Vec3{4.0, 4.0, 5.0});
    return mesh;
}

/// @brief MV-F06：三材质共存，冻结优先级裁决的顺序无关性。
FixtureMesh MakeFixtureF06()
{
    FixtureMesh mesh;
    mesh.name = "MV-F06";
    mesh.materials = {
        PeachMaterial(),
        GreenMaterial(),
        FixtureMaterial{"varnish", {0.5, 0.5, 0.5}, 300},
    };
    AppendClosedBox(mesh, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 4.0});
    AppendClosedBox(mesh, 1U, Vec3{0.0, 0.0, 1.0}, Vec3{4.0, 4.0, 3.0});
    AppendClosedBox(mesh, 2U, Vec3{0.0, 0.0, 2.0}, Vec3{4.0, 4.0, 6.0});
    return mesh;
}

// ---------------------------------------------------------------------------
// 独立 dense oracle：仅用于小网格 expected，生产实现禁止保留这种全层栈
// ---------------------------------------------------------------------------

struct ZInterval
{
    double loMm{0.0};
    double hiMm{0.0};
};

/// @brief 单材质在单像素列的求解结果；unpaired 表示交点数为奇数（开放面）。
struct ColumnMaterialSolution
{
    std::vector<ZInterval> intervals;
    bool unpaired{false};
    std::size_t mergedHitCount{0};
};

/// @brief 收集某材质在给定像素中心的垂直射线交点，升序并合并共面/共享边重复命中。
ColumnMaterialSolution SolveColumnMaterial(
    const FixtureMesh& mesh,
    const std::uint32_t materialIndex,
    const double px,
    const double py)
{
    std::vector<double> hits;
    for (std::size_t index{0}; index < mesh.triangles.size(); ++index)
    {
        if (mesh.triangleMaterial.at(index) != materialIndex)
        {
            continue;
        }
        const Triangle& triangle = mesh.triangles.at(index);
        double w0{0.0};
        double w1{0.0};
        double w2{0.0};
        if (!PointInTriangleXy(px, py, triangle, w0, w1, w2))
        {
            continue;
        }
        hits.push_back(w0 * triangle.a.z + w1 * triangle.b.z + w2 * triangle.c.z);
    }
    std::sort(hits.begin(), hits.end());

    std::vector<double> merged;
    for (const double z : hits)
    {
        if (!merged.empty() && std::abs(z - merged.back()) <= kCoincidentHitEpsilonMm)
        {
            continue;
        }
        merged.push_back(z);
    }

    ColumnMaterialSolution solution;
    solution.mergedHitCount = merged.size();
    if (merged.size() % 2U != 0U)
    {
        solution.unpaired = true;
        return solution;
    }
    for (std::size_t index{0}; index + 1U < merged.size(); index += 2U)
    {
        solution.intervals.push_back(ZInterval{merged.at(index), merged.at(index + 1U)});
    }
    return solution;
}

/// @brief 稠密 owner 体：owner[layer * pixelCount + pixel]。
struct DenseOwnerVolume
{
    FixtureGrid grid;
    std::vector<std::uint32_t> owner;
    std::vector<std::uint32_t> unpairedMaterials;
    std::uint64_t overlapResolvedPixels{0};
    std::uint64_t overlapBlockedPixels{0};
};

/// @brief 构造独立 dense owner 参考；同优先级重叠记为 blocked，不静默择一。
DenseOwnerVolume BuildDenseOwnerReference(const FixtureMesh& mesh, const FixtureGrid& grid)
{
    DenseOwnerVolume volume;
    volume.grid = grid;
    volume.owner.assign(static_cast<std::size_t>(grid.layerCount) * PixelCount(grid), kNoOwner);

    std::set<std::uint32_t> unpaired;
    for (int y{0}; y < grid.heightPx; ++y)
    {
        for (int x{0}; x < grid.widthPx; ++x)
        {
            const double px = PixelCenterX(grid, x);
            const double py = PixelCenterY(grid, y);
            const std::size_t pixel = PixelIndex(grid, x, y);

            for (std::size_t material{0}; material < mesh.materials.size(); ++material)
            {
                const std::uint32_t materialIndex = static_cast<std::uint32_t>(material);
                const ColumnMaterialSolution solution =
                    SolveColumnMaterial(mesh, materialIndex, px, py);
                if (solution.unpaired)
                {
                    unpaired.insert(materialIndex);
                    continue;
                }
                for (const ZInterval& interval : solution.intervals)
                {
                    const int firstLayer = FirstLayerAtOrAboveZ(interval.loMm, grid.layerThicknessMm);
                    const int lastLayer = LastLayerAtOrBelowZ(interval.hiMm, grid.layerThicknessMm);
                    const int beginLayer = std::max(0, firstLayer);
                    const int endLayer = std::min(grid.layerCount - 1, lastLayer);
                    for (int layer{beginLayer}; layer <= endLayer; ++layer)
                    {
                        const std::size_t slot =
                            static_cast<std::size_t>(layer) * PixelCount(grid) + pixel;
                        const std::uint32_t current = volume.owner.at(slot);
                        if (current == kNoOwner)
                        {
                            volume.owner.at(slot) = materialIndex;
                            continue;
                        }
                        if (current == materialIndex)
                        {
                            continue;
                        }
                        const int currentPriority =
                            mesh.materials.at(static_cast<std::size_t>(current)).priority;
                        const int candidatePriority = mesh.materials.at(material).priority;
                        if (candidatePriority > currentPriority)
                        {
                            volume.owner.at(slot) = materialIndex;
                            ++volume.overlapResolvedPixels;
                        }
                        else if (candidatePriority < currentPriority)
                        {
                            ++volume.overlapResolvedPixels;
                        }
                        else
                        {
                            ++volume.overlapBlockedPixels;
                        }
                    }
                }
            }
        }
    }
    volume.unpairedMaterials.assign(unpaired.begin(), unpaired.end());
    return volume;
}

// ---------------------------------------------------------------------------
// Legacy 顶面投影 baseline：复刻 build_material_role_columns 的整列传播语义
// ---------------------------------------------------------------------------

/// @brief 复刻 slicer.cpp:1300-1349 的顶面选取（zMm >= zMax，Z 相等时索引大者胜），
///        再按 build_material_role_columns 的语义把顶面材质沿整列传播。
DenseOwnerVolume BuildLegacyTopProjectionBaseline(const FixtureMesh& mesh, const FixtureGrid& grid)
{
    DenseOwnerVolume volume;
    volume.grid = grid;
    volume.owner.assign(static_cast<std::size_t>(grid.layerCount) * PixelCount(grid), kNoOwner);

    for (int y{0}; y < grid.heightPx; ++y)
    {
        for (int x{0}; x < grid.widthPx; ++x)
        {
            const double px = PixelCenterX(grid, x);
            const double py = PixelCenterY(grid, y);
            bool hasModel{false};
            double zMax{-1.0e30};
            double zMin{1.0e30};
            std::uint32_t topMaterial{kNoOwner};

            for (std::size_t index{0}; index < mesh.triangles.size(); ++index)
            {
                const Triangle& triangle = mesh.triangles.at(index);
                double w0{0.0};
                double w1{0.0};
                double w2{0.0};
                if (!PointInTriangleXy(px, py, triangle, w0, w1, w2))
                {
                    continue;
                }
                const double zMm = w0 * triangle.a.z + w1 * triangle.b.z + w2 * triangle.c.z;
                hasModel = true;
                zMin = std::min(zMin, zMm);
                if (zMm >= zMax)
                {
                    zMax = zMm;
                    topMaterial = mesh.triangleMaterial.at(index);
                }
            }
            if (!hasModel)
            {
                continue;
            }
            const int beginLayer = std::max(0, FirstLayerAtOrAboveZ(zMin, grid.layerThicknessMm));
            const int endLayer =
                std::min(grid.layerCount - 1, LastLayerAtOrBelowZ(zMax, grid.layerThicknessMm));
            const std::size_t pixel = PixelIndex(grid, x, y);
            for (int layer{beginLayer}; layer <= endLayer; ++layer)
            {
                volume.owner.at(static_cast<std::size_t>(layer) * PixelCount(grid) + pixel) =
                    topMaterial;
            }
        }
    }
    return volume;
}

// ---------------------------------------------------------------------------
// owner diff schema（MV-01 冻结草案）：layer / x / y / expected / actual / materialKey
// ---------------------------------------------------------------------------

struct OwnerDiffEntry
{
    int layerIndex{0};
    int x{0};
    int y{0};
    std::uint32_t expected{kNoOwner};
    std::uint32_t actual{kNoOwner};
    std::string expectedMaterialKey;
    std::string actualMaterialKey;
};

std::string MaterialKeyOf(const FixtureMesh& mesh, const std::uint32_t owner)
{
    if (owner == kNoOwner)
    {
        return "<none>";
    }
    return mesh.name + "/" + mesh.materials.at(static_cast<std::size_t>(owner)).name;
}

/// @brief 逐 layer/x/y 比较两个 owner 体，产出机器可读差异条目。
std::vector<OwnerDiffEntry> DiffOwnerVolumes(
    const FixtureMesh& mesh,
    const DenseOwnerVolume& expected,
    const DenseOwnerVolume& actual)
{
    std::vector<OwnerDiffEntry> entries;
    const FixtureGrid& grid = expected.grid;
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        for (int y{0}; y < grid.heightPx; ++y)
        {
            for (int x{0}; x < grid.widthPx; ++x)
            {
                const std::size_t slot = static_cast<std::size_t>(layer) * PixelCount(grid)
                    + PixelIndex(grid, x, y);
                const std::uint32_t expectedOwner = expected.owner.at(slot);
                const std::uint32_t actualOwner = actual.owner.at(slot);
                if (expectedOwner == actualOwner)
                {
                    continue;
                }
                OwnerDiffEntry entry;
                entry.layerIndex = layer;
                entry.x = x;
                entry.y = y;
                entry.expected = expectedOwner;
                entry.actual = actualOwner;
                entry.expectedMaterialKey = MaterialKeyOf(mesh, expectedOwner);
                entry.actualMaterialKey = MaterialKeyOf(mesh, actualOwner);
                entries.push_back(entry);
            }
        }
    }
    return entries;
}

/// @brief 差异条目的稳定文本表达，供后续卡直接升级为 JSON 报告字段。
std::string FormatOwnerDiffEntry(const OwnerDiffEntry& entry)
{
    std::ostringstream stream;
    stream << "{\"layer\":" << entry.layerIndex << ",\"x\":" << entry.x << ",\"y\":" << entry.y
           << ",\"expected\":\"" << entry.expectedMaterialKey << "\""
           << ",\"actual\":\"" << entry.actualMaterialKey << "\"}";
    return stream.str();
}

/// @brief owner 体的稳定摘要（FNV-1a 64），用于重复运行一致性断言。
std::uint64_t OwnerVolumeDigest(const DenseOwnerVolume& volume)
{
    std::uint64_t hash{0xCBF29CE484222325ULL};
    for (const std::uint32_t owner : volume.owner)
    {
        for (int shift{0}; shift < 32; shift += 8)
        {
            const std::uint8_t byte = static_cast<std::uint8_t>((owner >> shift) & 0xFFU);
            hash ^= static_cast<std::uint64_t>(byte);
            hash *= 0x100000001B3ULL;
        }
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Reality 资产事实：finger_suoguo 目录当前【未纳入版本控制】，
// 因此本组用例在资产缺失时显式 SKIP，绝不伪造 PASS。
// ---------------------------------------------------------------------------

struct RealityMaterialFacts
{
    std::string materialName;
    std::size_t triangleCount{0};
    std::size_t boundaryEdgeCount{0};
    std::size_t nonManifoldEdgeCount{0};
};

/// @brief 极简 OBJ 解析：只统计 usemtl 分组的扇形三角化面数与边流形性。
///        故意不复用 model.cpp，以避免自动定向等无关变换干扰事实固化。
std::vector<RealityMaterialFacts> ParseRealityMaterialFacts(const std::filesystem::path& objPath)
{
    std::ifstream input(objPath, std::ios::binary);
    if (!input)
    {
        return {};
    }
    std::map<std::string, std::vector<std::array<int, 3>>> groups;
    std::vector<std::string> order;
    std::string active;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.rfind("usemtl", 0) == 0)
        {
            std::istringstream stream(line);
            std::string token;
            stream >> token >> active;
            if (groups.find(active) == groups.end())
            {
                groups.emplace(active, std::vector<std::array<int, 3>>{});
                order.push_back(active);
            }
        }
        else if (line.rfind("f ", 0) == 0)
        {
            std::istringstream stream(line);
            std::string token;
            stream >> token;
            std::vector<int> indices;
            while (stream >> token)
            {
                const std::size_t slash = token.find('/');
                const std::string first = slash == std::string::npos ? token : token.substr(0, slash);
                if (!first.empty())
                {
                    indices.push_back(std::stoi(first));
                }
            }
            for (std::size_t k{1}; k + 1U < indices.size(); ++k)
            {
                groups[active].push_back({indices.at(0), indices.at(k), indices.at(k + 1U)});
            }
        }
    }

    std::vector<RealityMaterialFacts> facts;
    for (const std::string& name : order)
    {
        const std::vector<std::array<int, 3>>& triangles = groups.at(name);
        std::map<std::pair<int, int>, int> edges;
        for (const std::array<int, 3>& triangle : triangles)
        {
            const std::array<std::pair<int, int>, 3> pairs{
                std::make_pair(triangle[0], triangle[1]),
                std::make_pair(triangle[1], triangle[2]),
                std::make_pair(triangle[2], triangle[0])};
            for (const std::pair<int, int>& pair : pairs)
            {
                const std::pair<int, int> key{
                    std::min(pair.first, pair.second), std::max(pair.first, pair.second)};
                ++edges[key];
            }
        }
        RealityMaterialFacts entry;
        entry.materialName = name;
        entry.triangleCount = triangles.size();
        for (const std::pair<const std::pair<int, int>, int>& edge : edges)
        {
            if (edge.second == 1)
            {
                ++entry.boundaryEdgeCount;
            }
            else if (edge.second > 2)
            {
                ++entry.nonManifoldEdgeCount;
            }
        }
        facts.push_back(entry);
    }
    return facts;
}

std::filesystem::path RealityAssetPath(const std::string& fileName)
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR} / "model" / "obj" / "reality"
        / "finger_suoguo" / fileName;
#else
    return std::filesystem::path{"model"} / "obj" / "reality" / "finger_suoguo" / fileName;
#endif
}

const RealityMaterialFacts* FindMaterialFacts(
    const std::vector<RealityMaterialFacts>& facts,
    const std::string& name)
{
    for (const RealityMaterialFacts& entry : facts)
    {
        if (entry.materialName == name)
        {
            return &entry;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 用例
// ---------------------------------------------------------------------------

/// @brief Kd 量化必须与 model.cpp:1702-1705 逐位一致，冻结两个真实材质颜色。
bool KdQuantizationMatchesImporter()
{
    bool passed{true};
    const FixtureMaterial& green = GreenMaterial();
    passed = ExpectTrue(KdComponentToU8(green.kd[0]) == 63U, "green Kd r quantizes to 63") && passed;
    passed = ExpectTrue(KdComponentToU8(green.kd[1]) == 190U, "green Kd g quantizes to 190") && passed;
    passed = ExpectTrue(KdComponentToU8(green.kd[2]) == 126U, "green Kd b quantizes to 126") && passed;

    const FixtureMaterial& peach = PeachMaterial();
    passed = ExpectTrue(KdComponentToU8(peach.kd[0]) == 255U, "peach Kd r quantizes to 255") && passed;
    passed = ExpectTrue(KdComponentToU8(peach.kd[1]) == 220U, "peach Kd g quantizes to 220") && passed;
    passed = ExpectTrue(KdComponentToU8(peach.kd[2]) == 198U, "peach Kd b quantizes to 198") && passed;

    passed = ExpectTrue(KdComponentToU8(-1.0) == 0U, "negative Kd clamps to 0") && passed;
    passed = ExpectTrue(KdComponentToU8(2.0) == 255U, "over-range Kd clamps to 255") && passed;
    return passed;
}

/// @brief MV-F01：同一 XY 列内随 Z 换材质，owner 必须逐层可手算。
bool ClosedIntervalsProduceHandCheckableOwners()
{
    const FixtureMesh mesh = MakeFixtureF01();
    FixtureGrid grid;
    const DenseOwnerVolume reference = BuildDenseOwnerReference(mesh, grid);

    bool passed{true};
    passed = ExpectTrue(reference.unpairedMaterials.empty(), "MV-F01 has no unpaired material") && passed;
    passed = ExpectTrue(reference.overlapBlockedPixels == 0U, "MV-F01 has no blocked overlap") && passed;

    // 盒 A z=[0,2] -> 层 0..1；盒 B z=[3,5] -> 层 3..4；层 2 与 5..7 无 owner。
    const std::array<std::uint32_t, 8> expectedByLayer{0U, 0U, kNoOwner, 1U, 1U, kNoOwner, kNoOwner, kNoOwner};
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        for (std::size_t pixel{0}; pixel < PixelCount(grid); ++pixel)
        {
            const std::uint32_t owner =
                reference.owner.at(static_cast<std::size_t>(layer) * PixelCount(grid) + pixel);
            if (owner != expectedByLayer.at(static_cast<std::size_t>(layer)))
            {
                passed = ExpectTrue(false, "MV-F01 layer owner matches hand-computed expectation")
                    && passed;
                break;
            }
        }
    }
    return passed;
}

/// @brief MV-F01/F03：Legacy 顶面投影【结构性】无法表达纵深材质，差异必须被机器捕获。
bool LegacyTopProjectionCannotExpressDepthOwnership()
{
    bool passed{true};
    FixtureGrid grid;

    const FixtureMesh f01 = MakeFixtureF01();
    const DenseOwnerVolume reference = BuildDenseOwnerReference(f01, grid);
    const DenseOwnerVolume legacy = BuildLegacyTopProjectionBaseline(f01, grid);
    const std::vector<OwnerDiffEntry> diffs = DiffOwnerVolumes(f01, reference, legacy);
    passed = ExpectTrue(!diffs.empty(), "MV-F01 legacy top projection differs from depth ownership")
        && passed;

    // Legacy 把顶面材质（绿）铺满整列，因此层 0..1 本应为浅桃色却被判成绿色。
    bool foundLowerLayerMisattribution{false};
    for (const OwnerDiffEntry& entry : diffs)
    {
        if (entry.layerIndex <= 1 && entry.expected == 0U && entry.actual == 1U)
        {
            foundLowerLayerMisattribution = true;
            break;
        }
    }
    passed = ExpectTrue(
                 foundLowerLayerMisattribution,
                 "MV-F01 legacy assigns top material to lower-layer pixels")
        && passed;

    const FixtureMesh f03 = MakeFixtureF03();
    const DenseOwnerVolume legacyF03 = BuildLegacyTopProjectionBaseline(f03, grid);
    bool allGreen{true};
    bool anyOwned{false};
    for (const std::uint32_t owner : legacyF03.owner)
    {
        if (owner == kNoOwner)
        {
            continue;
        }
        anyOwned = true;
        if (owner != 1U)
        {
            allGreen = false;
        }
    }
    passed = ExpectTrue(anyOwned, "MV-F03 legacy baseline owns at least one pixel") && passed;
    passed = ExpectTrue(
                 allGreen,
                 "MV-F03 legacy baseline attributes every owned pixel to the open green surface")
        && passed;
    return passed;
}

/// @brief MV-F03：开放材质交点数为奇数，默认必须 fail closed，闭合材质不受影响。
bool OpenSurfaceMaterialFailsClosed()
{
    const FixtureMesh mesh = MakeFixtureF03();
    FixtureGrid grid;
    const DenseOwnerVolume reference = BuildDenseOwnerReference(mesh, grid);

    bool passed{true};
    passed = ExpectTrue(
                 reference.unpairedMaterials.size() == 1U
                     && reference.unpairedMaterials.front() == 1U,
                 "MV-F03 reports exactly the open green material as unpaired")
        && passed;

    const ColumnMaterialSolution open =
        SolveColumnMaterial(mesh, 1U, PixelCenterX(grid, 0), PixelCenterY(grid, 0));
    passed = ExpectTrue(open.unpaired, "MV-F03 open sheet column is unpaired") && passed;
    passed = ExpectTrue(open.mergedHitCount == 1U, "MV-F03 open sheet yields a single hit") && passed;

    const ColumnMaterialSolution closed =
        SolveColumnMaterial(mesh, 0U, PixelCenterX(grid, 0), PixelCenterY(grid, 0));
    passed = ExpectTrue(!closed.unpaired, "MV-F03 closed body column stays paired") && passed;
    passed = ExpectTrue(closed.intervals.size() == 1U, "MV-F03 closed body yields one interval")
        && passed;
    return passed;
}

/// @brief MV-F04：分离实体必须保留为两个区间，禁止被 first/last 包络填平。
bool SeparatedIntervalsAreNotEnvelopeFilled()
{
    const FixtureMesh mesh = MakeFixtureF04();
    FixtureGrid grid;
    const ColumnMaterialSolution solution =
        SolveColumnMaterial(mesh, 0U, PixelCenterX(grid, 1), PixelCenterY(grid, 1));

    bool passed{true};
    passed = ExpectTrue(solution.mergedHitCount == 4U, "MV-F04 column yields four merged hits")
        && passed;
    passed = ExpectTrue(solution.intervals.size() == 2U, "MV-F04 column yields two intervals")
        && passed;

    const DenseOwnerVolume reference = BuildDenseOwnerReference(mesh, grid);
    const std::size_t pixel = PixelIndex(grid, 1, 1);
    // z=[0,2] -> 层 0..1；空腔 z=[2,4] -> 层 2..3 必须无 owner；z=[4,6] -> 层 4..5。
    const std::array<std::uint32_t, 8> expected{0U, 0U, kNoOwner, kNoOwner, 0U, 0U, kNoOwner, kNoOwner};
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        const std::uint32_t owner =
            reference.owner.at(static_cast<std::size_t>(layer) * PixelCount(grid) + pixel);
        passed = ExpectTrue(
                     owner == expected.at(static_cast<std::size_t>(layer)),
                     "MV-F04 cavity layers stay unowned")
            && passed;
    }
    return passed;
}

/// @brief MV-F02/F06：重叠必须由显式优先级裁决，且与材质声明顺序无关；同级必须阻断。
bool OverlapIsResolvedByExplicitPriorityOnly()
{
    bool passed{true};
    FixtureGrid grid;

    const FixtureMesh mesh = MakeFixtureF02();
    const DenseOwnerVolume reference = BuildDenseOwnerReference(mesh, grid);
    // 盒 A（浅桃，priority 100）层 0..2；盒 B（绿，priority 200）层 2..4；层 2 重叠。
    const std::size_t pixel = PixelIndex(grid, 2, 2);
    passed = ExpectTrue(
                 reference.owner.at(0U * PixelCount(grid) + pixel) == 0U,
                 "MV-F02 layer 0 is owned by the lower box")
        && passed;
    passed = ExpectTrue(
                 reference.owner.at(2U * PixelCount(grid) + pixel) == 1U,
                 "MV-F02 overlapped layer is won by the higher priority material")
        && passed;
    passed = ExpectTrue(
                 reference.owner.at(4U * PixelCount(grid) + pixel) == 1U,
                 "MV-F02 layer 4 is owned by the upper box")
        && passed;
    passed = ExpectTrue(reference.overlapResolvedPixels > 0U, "MV-F02 records resolved overlap")
        && passed;
    passed = ExpectTrue(reference.overlapBlockedPixels == 0U, "MV-F02 blocks nothing when priorities differ")
        && passed;

    // 声明顺序反转后结论必须不变：优先级是唯一裁决依据。
    FixtureMesh reversed;
    reversed.name = "MV-F02-reversed";
    reversed.materials = {GreenMaterial(), PeachMaterial()};
    AppendClosedBox(reversed, 1U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 3.0});
    AppendClosedBox(reversed, 0U, Vec3{0.0, 0.0, 2.0}, Vec3{4.0, 4.0, 5.0});
    const DenseOwnerVolume reversedReference = BuildDenseOwnerReference(reversed, grid);
    passed = ExpectTrue(
                 reversedReference.owner.at(2U * PixelCount(grid) + pixel) == 0U,
                 "MV-F02 reversed declaration order still selects the green material")
        && passed;

    // 同级重叠必须阻断，不得静默择一。
    FixtureMesh tied;
    tied.name = "MV-F02-tied";
    tied.materials = {
        FixtureMaterial{"a", {0.1, 0.2, 0.3}, 100},
        FixtureMaterial{"b", {0.4, 0.5, 0.6}, 100},
    };
    AppendClosedBox(tied, 0U, Vec3{0.0, 0.0, 0.0}, Vec3{4.0, 4.0, 3.0});
    AppendClosedBox(tied, 1U, Vec3{0.0, 0.0, 2.0}, Vec3{4.0, 4.0, 5.0});
    const DenseOwnerVolume tiedReference = BuildDenseOwnerReference(tied, grid);
    passed = ExpectTrue(
                 tiedReference.overlapBlockedPixels > 0U,
                 "equal priority overlap is blocked instead of silently resolved")
        && passed;
    return passed;
}

/// @brief owner diff schema 必须携带 layer/x/y/expected/actual/materialKey 六要素。
bool OwnerDiffSchemaCarriesRequiredFields()
{
    const FixtureMesh mesh = MakeFixtureF01();
    FixtureGrid grid;
    const DenseOwnerVolume reference = BuildDenseOwnerReference(mesh, grid);
    const DenseOwnerVolume legacy = BuildLegacyTopProjectionBaseline(mesh, grid);
    const std::vector<OwnerDiffEntry> diffs = DiffOwnerVolumes(mesh, reference, legacy);

    bool passed{true};
    passed = ExpectTrue(!diffs.empty(), "diff schema sample is non-empty") && passed;
    if (diffs.empty())
    {
        return passed;
    }
    const OwnerDiffEntry& entry = diffs.front();
    passed = ExpectTrue(entry.x >= 0 && entry.x < grid.widthPx, "diff entry carries in-range x")
        && passed;
    passed = ExpectTrue(entry.y >= 0 && entry.y < grid.heightPx, "diff entry carries in-range y")
        && passed;
    passed = ExpectTrue(
                 entry.layerIndex >= 0 && entry.layerIndex < grid.layerCount,
                 "diff entry carries in-range layer")
        && passed;
    passed = ExpectTrue(entry.expected != entry.actual, "diff entry only records real differences")
        && passed;
    passed = ExpectTrue(
                 !entry.expectedMaterialKey.empty() && !entry.actualMaterialKey.empty(),
                 "diff entry carries both material keys")
        && passed;

    const std::string formatted = FormatOwnerDiffEntry(entry);
    passed = ExpectTrue(
                 formatted.find("\"layer\"") != std::string::npos
                     && formatted.find("\"x\"") != std::string::npos
                     && formatted.find("\"y\"") != std::string::npos
                     && formatted.find("\"expected\"") != std::string::npos
                     && formatted.find("\"actual\"") != std::string::npos,
                 "formatted diff entry exposes all schema fields")
        && passed;
    return passed;
}

/// @brief 全部 fixture 必须可重复生成：两次独立构造的 owner 摘要一致。
bool FixtureGenerationIsDeterministic()
{
    bool passed{true};
    FixtureGrid grid;
    const std::array<FixtureMesh (*)(), 6> builders{
        &MakeFixtureF01, &MakeFixtureF02, &MakeFixtureF03,
        &MakeFixtureF04, &MakeFixtureF05, &MakeFixtureF06};

    for (const auto& builder : builders)
    {
        const FixtureMesh first = builder();
        const FixtureMesh second = builder();
        passed = ExpectTrue(
                     first.triangles.size() == second.triangles.size()
                         && first.triangleMaterial == second.triangleMaterial,
                     "fixture mesh construction is reproducible")
            && passed;

        const DenseOwnerVolume firstVolume = BuildDenseOwnerReference(first, grid);
        const DenseOwnerVolume secondVolume = BuildDenseOwnerReference(second, grid);
        passed = ExpectTrue(
                     OwnerVolumeDigest(firstVolume) == OwnerVolumeDigest(secondVolume),
                     "owner volume digest is stable across repeated runs")
            && passed;
        passed = ExpectTrue(firstVolume.owner == secondVolume.owner, "owner volume is byte-identical")
            && passed;
    }
    return passed;
}

/// @brief Reality 03.obj 的逐材质拓扑事实；资产缺失时 SKIP 而非伪造 PASS。
bool RealityAssetTopologyFactsAreFrozen()
{
    const std::filesystem::path objPath = RealityAssetPath("03.obj");
    if (!std::filesystem::exists(objPath))
    {
        std::cout << "SKIP reality_03_topology_facts asset not present: " << objPath.string()
                  << '\n';
        return true;
    }
    const std::vector<RealityMaterialFacts> facts = ParseRealityMaterialFacts(objPath);

    bool passed{true};
    passed = ExpectTrue(facts.size() == 2U, "reality 03 declares exactly two usemtl groups") && passed;

    const RealityMaterialFacts* green = FindMaterialFacts(facts, "01");
    passed = ExpectTrue(green != nullptr, "reality 03 contains usemtl 01") && passed;
    if (green != nullptr)
    {
        passed = ExpectTrue(green->triangleCount == 14966U, "reality 03 material 01 has 14966 triangles")
            && passed;
        passed = ExpectTrue(
                     green->boundaryEdgeCount == 1382U,
                     "reality 03 material 01 has 1382 boundary edges and is therefore OPEN")
            && passed;
        passed = ExpectTrue(
                     green->nonManifoldEdgeCount == 0U,
                     "reality 03 material 01 has no non-manifold edge")
            && passed;
    }

    const RealityMaterialFacts* peach = FindMaterialFacts(facts, "02");
    passed = ExpectTrue(peach != nullptr, "reality 03 contains usemtl 02") && passed;
    if (peach != nullptr)
    {
        passed = ExpectTrue(peach->triangleCount == 12126U, "reality 03 material 02 has 12126 triangles")
            && passed;
        passed = ExpectTrue(
                     peach->boundaryEdgeCount == 0U,
                     "reality 03 material 02 is a CLOSED orientable subgroup")
            && passed;
        passed = ExpectTrue(
                     peach->nonManifoldEdgeCount == 0U,
                     "reality 03 material 02 has no non-manifold edge")
            && passed;
    }
    return passed;
}

/// @brief 同目录 08/09 变体的【两个材质都开放】，不能替代 03 作为闭合体基线。
bool RealitySiblingVariantsAreOpenOnBothMaterials()
{
    bool passed{true};
    bool evaluatedAny{false};
    for (const std::string& fileName : {std::string{"08.obj"}, std::string{"09.obj"}})
    {
        const std::filesystem::path objPath = RealityAssetPath(fileName);
        if (!std::filesystem::exists(objPath))
        {
            std::cout << "SKIP reality_sibling_variants asset not present: " << objPath.string()
                      << '\n';
            continue;
        }
        evaluatedAny = true;
        const std::vector<RealityMaterialFacts> facts = ParseRealityMaterialFacts(objPath);
        const RealityMaterialFacts* peach = FindMaterialFacts(facts, "02");
        passed = ExpectTrue(peach != nullptr, "reality sibling contains usemtl 02") && passed;
        if (peach != nullptr)
        {
            passed = ExpectTrue(
                         peach->boundaryEdgeCount > 0U,
                         "reality sibling material 02 is OPEN and cannot serve as the closed baseline")
                && passed;
        }
    }
    if (!evaluatedAny)
    {
        std::cout << "SKIP reality_sibling_variants no sibling asset present\n";
    }
    return passed;
}

/// @brief SKIP 路径本身必须可验证：缺失资产时解析器返回空而不是崩溃或伪造事实。
bool MissingAssetPathYieldsNoFacts()
{
    const std::filesystem::path missing =
        RealityAssetPath("__matvol_missing_asset_should_not_exist__.obj");
    bool passed{true};
    passed = ExpectTrue(!std::filesystem::exists(missing), "missing asset path really is absent")
        && passed;
    passed = ExpectTrue(
                 ParseRealityMaterialFacts(missing).empty(),
                 "missing asset yields no fabricated facts")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 10> tests{{
        {"kd_quantization_matches_importer", KdQuantizationMatchesImporter},
        {"closed_intervals_hand_checkable", ClosedIntervalsProduceHandCheckableOwners},
        {"legacy_top_projection_cannot_express_depth", LegacyTopProjectionCannotExpressDepthOwnership},
        {"open_surface_material_fails_closed", OpenSurfaceMaterialFailsClosed},
        {"separated_intervals_not_envelope_filled", SeparatedIntervalsAreNotEnvelopeFilled},
        {"overlap_resolved_by_explicit_priority", OverlapIsResolvedByExplicitPriorityOnly},
        {"owner_diff_schema_fields", OwnerDiffSchemaCarriesRequiredFields},
        {"fixture_generation_deterministic", FixtureGenerationIsDeterministic},
        {"reality_03_topology_facts", RealityAssetTopologyFactsAreFrozen},
        {"reality_sibling_variants_open", RealitySiblingVariantsAreOpenOnBothMaterials},
    }};

    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (!test())
            {
                std::cerr << "FAIL " << name << '\n';
                ++failed;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    if (!MissingAssetPathYieldsNoFacts())
    {
        std::cerr << "FAIL missing_asset_path_yields_no_facts\n";
        ++failed;
    }
    if (failed == 0)
    {
        std::cout << "PASS MatvolFactsTests " << (tests.size() + 1U) << "/" << (tests.size() + 1U)
                  << '\n';
        return 0;
    }
    std::cerr << "FAIL MatvolFactsTests " << failed << " failed\n";
    return 1;
}
