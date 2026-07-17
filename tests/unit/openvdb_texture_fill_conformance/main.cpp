#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"
#include "slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

bool IsOpenVdbAvailable()
{
    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    return status.compiled_with_openvdb && status.runtime_available;
}

slicer_core::TextureFillPartitionGridSpec MakeGrid(
    const int width,
    const int height,
    const int depth,
    const double originX,
    const double originY,
    const double originZ,
    const double spacingX,
    const double spacingY,
    const double spacingZ)
{
    slicer_core::TextureFillPartitionGridSpec grid;
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.originXMm = originX;
    grid.originYMm = originY;
    grid.originZMm = originZ;
    grid.spacingXMm = spacingX;
    grid.spacingYMm = spacingY;
    grid.spacingZMm = spacingZ;
    return grid;
}

slicer_core::TextureFillPartitionGridSpec MakeUnitBoxGrid()
{
    return MakeGrid(12, 12, 12, -0.1, -0.1, -0.1, 0.1, 0.1, 0.1);
}

std::size_t GridIndex(
    const slicer_core::TextureFillPartitionGridSpec& grid,
    const int x,
    const int y,
    const int z)
{
    return
        (static_cast<std::size_t>(z) * static_cast<std::size_t>(grid.height)
         + static_cast<std::size_t>(y))
            * static_cast<std::size_t>(grid.width)
        + static_cast<std::size_t>(x);
}

void TranslateMesh(
    slicer_core::TriangleMeshData& mesh,
    const double x,
    const double y,
    const double z)
{
    for (slicer_core::Vec3& vertex : mesh.vertices)
    {
        vertex.x += x;
        vertex.y += y;
        vertex.z += z;
    }
    mesh.bbox_mm.min.x += x;
    mesh.bbox_mm.min.y += y;
    mesh.bbox_mm.min.z += z;
    mesh.bbox_mm.max.x += x;
    mesh.bbox_mm.max.y += y;
    mesh.bbox_mm.max.z += z;
}

void AppendMesh(
    slicer_core::TriangleMeshData& destination,
    const slicer_core::TriangleMeshData& source,
    const bool reverseOrientation)
{
    const int vertexOffset = static_cast<int>(destination.vertices.size());
    destination.vertices.insert(
        destination.vertices.end(),
        source.vertices.begin(),
        source.vertices.end());
    for (std::array<int, 3> triangle : source.triangles)
    {
        for (int& vertexIndex : triangle)
        {
            vertexIndex += vertexOffset;
        }
        if (reverseOrientation)
        {
            std::swap(triangle.at(1), triangle.at(2));
        }
        destination.triangles.push_back(triangle);
    }
}

slicer_core::TriangleMeshData MakeOctahedronMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.source_name = "generated-octahedron";
    mesh.vertices = {
        {0.5, 0.5, 1.0},
        {0.5, 0.5, 0.0},
        {1.0, 0.5, 0.5},
        {0.5, 1.0, 0.5},
        {0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5},
    };
    mesh.triangles = {
        {0, 2, 3},
        {0, 3, 4},
        {0, 4, 5},
        {0, 5, 2},
        {1, 3, 2},
        {1, 4, 3},
        {1, 5, 4},
        {1, 2, 5},
    };
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

slicer_core::TriangleMeshData MakeClosedCavityMesh()
{
    slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    mesh.source_name = "generated-closed-cavity";
    slicer_core::TriangleMeshData inner =
        slicer_core::MakeGeneratedBoxMesh(0.4, 0.4, 0.4);
    TranslateMesh(inner, 0.3, 0.3, 0.3);
    AppendMesh(mesh, inner, true);
    return mesh;
}

slicer_core::TriangleMeshData MakeOverlappingClosedBoxes()
{
    slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(0.7, 0.7, 0.7);
    mesh.source_name = "generated-self-intersection";
    slicer_core::TriangleMeshData second =
        slicer_core::MakeGeneratedBoxMesh(0.7, 0.7, 0.7);
    TranslateMesh(second, 0.3, 0.3, 0.3);
    AppendMesh(mesh, second, false);
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

slicer_core::GlobalTextureFillPartitionRequest MakeRequest(
    const slicer_core::TriangleMeshData& mesh,
    const slicer_core::TextureFillPartitionGridSpec& grid,
    const double widthMm)
{
    slicer_core::GlobalTextureFillPartitionRequest request;
    request.mesh = &mesh;
    request.grid = grid;
    request.options.requestedWidthMm = widthMm;
    return request;
}

slicer_core::GlobalTextureFillPartitionResult EvaluateCpu(
    const slicer_core::GlobalTextureFillPartitionRequest& request)
{
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    return slicer_core::GlobalTextureFillPartitionService(&backend).Evaluate(
        request);
}

slicer_core::GlobalTextureFillPartitionResult EvaluateOpenVdb(
    const slicer_core::GlobalTextureFillPartitionRequest& request)
{
    slicer_core::OpenVdbTextureFillConformanceBackend backend;
    return slicer_core::GlobalTextureFillPartitionService(&backend).Evaluate(
        request);
}

bool BuildLaneContractIsStable()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult cpu =
        EvaluateCpu(request);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    const slicer_core::TextureFillPartitionConformanceResult conformance =
        slicer_core::CompareTextureFillPartitionResults(cpu, openVdb);

    if (!ExpectTrue(cpu.partitionPass, "CPU candidate remains independently available"))
    {
        return false;
    }
    if (!IsOpenVdbAvailable())
    {
        return ExpectTrue(!openVdb.available, "OpenVDB OFF candidate is unavailable")
            && ExpectTrue(
                HasIssueCode(
                    openVdb.issues,
                    "E_12E_OPENVDB_BACKEND_UNAVAILABLE"),
                "OpenVDB OFF candidate uses a stable issue")
            && ExpectTrue(
                conformance.conformanceStatus == "unavailable",
                "OpenVDB OFF conformance is unavailable")
            && ExpectTrue(
                conformance.productionAcceptance == "not_evaluated",
                "OpenVDB OFF conformance is not production accepted");
    }
    return ExpectTrue(openVdb.available, "OpenVDB ON candidate is available")
        && ExpectTrue(openVdb.partitionPass, "OpenVDB ON candidate invariant passes")
        && ExpectTrue(
            openVdb.backendRole == "conformance_candidate",
            "OpenVDB role remains conformance candidate")
        && ExpectTrue(
            openVdb.productionAcceptance == "not_evaluated",
            "OpenVDB candidate is not production accepted");
}

bool ClosedBoxRecordsSameGridConformance()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult cpu = EvaluateCpu(request);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    const slicer_core::TextureFillPartitionConformanceResult conformance =
        slicer_core::CompareTextureFillPartitionResults(cpu, openVdb);
    std::cout
        << "CONFORMANCE closed_box"
        << " cpuModel=" << cpu.stats.modelVoxels
        << " openVdbModel=" << openVdb.stats.modelVoxels
        << " modelOnlyCpu=" << conformance.modelOnlyCpuVoxels
        << " modelOnlyOpenVdb=" << conformance.modelOnlyOpenVdbVoxels
        << " maxDistanceDeltaMm=" << conformance.maxDistanceDeltaMm
        << " thresholdDeltaMm=" << conformance.allTextureThresholdDeltaMm
        << " cpuCoreMs=" << cpu.performance.totalCoreMs
        << " openVdbCoreMs=" << openVdb.performance.totalCoreMs
        << " openVdbGridBytes=" << openVdb.performance.openVdbGridBytes
        << '\n';

    return ExpectTrue(cpu.partitionPass, "closed box CPU partition passes")
        && ExpectTrue(openVdb.partitionPass, "closed box OpenVDB partition passes")
        && ExpectTrue(conformance.sameGrid, "closed box candidates share one grid")
        && ExpectTrue(
            conformance.conformanceStatus == "diagnostic",
            "closed box conformance remains diagnostic")
        && ExpectTrue(
            conformance.commonDistanceSamples > 0U,
            "closed box has common exact-distance samples")
        && ExpectTrue(
            conformance.maxDistanceDeltaMm < 1.0e-12,
            "common exact-distance samples use the same metric")
        && ExpectTrue(
            openVdb.queryStats.sdfSampleCount
                == openVdb.performance.gridVoxelCount,
            "OpenVDB samples the complete request grid")
        && ExpectTrue(
            openVdb.queryStats.sdfActiveSampleCount
                    + openVdb.queryStats.sdfBackgroundSampleCount
                == openVdb.queryStats.sdfSampleCount,
            "OpenVDB active and background sample evidence is complete");
}

bool SlopedBodyRecordsBoundaryDifferences()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh = MakeOctahedronMesh();
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult cpu = EvaluateCpu(request);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    const slicer_core::TextureFillPartitionConformanceResult conformance =
        slicer_core::CompareTextureFillPartitionResults(cpu, openVdb);
    return ExpectTrue(cpu.partitionPass, "sloped CPU partition passes")
        && ExpectTrue(openVdb.partitionPass, "sloped OpenVDB partition passes")
        && ExpectTrue(conformance.sameGrid, "sloped candidates share one grid")
        && ExpectTrue(
            conformance.conformanceStatus == "diagnostic",
            "sloped differences remain diagnostic evidence");
}

bool ThinWallAllowsAllTextureEndpoint()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.15);
    const slicer_core::TextureFillPartitionGridSpec grid = MakeGrid(
        12,
        12,
        5,
        -0.1,
        -0.1,
        -0.05,
        0.1,
        0.1,
        0.05);
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        grid,
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult cpu = EvaluateCpu(request);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    return ExpectTrue(cpu.partitionPass, "thin-wall CPU partition passes")
        && ExpectTrue(openVdb.partitionPass, "thin-wall OpenVDB partition passes")
        && ExpectTrue(cpu.widthMetrics.allTexture, "thin-wall CPU is all texture")
        && ExpectTrue(
            openVdb.widthMetrics.allTexture,
            "thin-wall OpenVDB is all texture")
        && ExpectTrue(
            openVdb.stats.modelFillVoxels == 0U,
            "thin-wall OpenVDB fill naturally disappears");
}

bool ClosedCavityRemainsOutsideModel()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh = MakeClosedCavityMesh();
    const slicer_core::TextureFillPartitionGridSpec grid = MakeUnitBoxGrid();
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        grid,
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    const std::size_t cavityIndex = GridIndex(grid, 5, 5, 5);
    const std::size_t shellIndex = GridIndex(grid, 3, 5, 5);
    return ExpectTrue(openVdb.partitionPass, "closed-cavity OpenVDB partition passes")
        && ExpectTrue(
            openVdb.modelMask.values.at(cavityIndex) == 0U,
            "closed cavity center remains outside OpenVDB model")
        && ExpectTrue(
            openVdb.modelMask.values.at(shellIndex) == 1U,
            "closed cavity material remains inside OpenVDB model");
}

bool StrictTopologyBlocksBeforeLevelSet()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    slicer_core::TriangleMeshData open =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    open.triangles.pop_back();
    slicer_core::TriangleMeshData nonManifold =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    nonManifold.triangles.push_back(nonManifold.triangles.front());
    const slicer_core::TriangleMeshData intersect = MakeOverlappingClosedBoxes();
    const slicer_core::GlobalTextureFillPartitionResult openResult =
        EvaluateOpenVdb(MakeRequest(open, MakeUnitBoxGrid(), 0.20));
    const slicer_core::GlobalTextureFillPartitionResult nonManifoldResult =
        EvaluateOpenVdb(MakeRequest(nonManifold, MakeUnitBoxGrid(), 0.20));
    const slicer_core::GlobalTextureFillPartitionResult intersectResult =
        EvaluateOpenVdb(MakeRequest(intersect, MakeUnitBoxGrid(), 0.20));
    return ExpectTrue(openResult.status == "blocked", "open mesh is blocked")
        && ExpectTrue(
            HasIssueCode(
                openResult.issues,
                "E_12E_OPENVDB_TOPOLOGY_BLOCKED"),
            "open mesh uses stable OpenVDB topology issue")
        && ExpectTrue(
            nonManifoldResult.status == "blocked",
            "non-manifold mesh is blocked")
        && ExpectTrue(
            intersectResult.status == "blocked",
            "self-intersection is blocked")
        && ExpectTrue(
            openResult.performance.levelSetMs == 0.0
                && nonManifoldResult.performance.levelSetMs == 0.0
                && intersectResult.performance.levelSetMs == 0.0,
            "topology blockers run before OpenVDB level-set generation");
}

bool WidthThresholdUsesSharedUnitsAndStep()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        MakeUnitBoxGrid(),
        10.0);
    const slicer_core::GlobalTextureFillPartitionResult cpu = EvaluateCpu(request);
    const slicer_core::GlobalTextureFillPartitionResult openVdb =
        EvaluateOpenVdb(request);
    const double cpuSteps = cpu.widthMetrics.allTextureThresholdMm / 0.01;
    const double openVdbSteps =
        openVdb.widthMetrics.allTextureThresholdMm / 0.01;
    return ExpectTrue(cpu.widthMetrics.allTexture, "CPU reaches all-texture endpoint")
        && ExpectTrue(
            openVdb.widthMetrics.allTexture,
            "OpenVDB reaches all-texture endpoint")
        && ExpectTrue(
            std::abs(cpuSteps - std::round(cpuSteps)) < 1.0e-8,
            "CPU threshold uses 0.01 mm steps")
        && ExpectTrue(
            std::abs(openVdbSteps - std::round(openVdbSteps)) < 1.0e-8,
            "OpenVDB threshold uses 0.01 mm steps");
}

bool RepeatedOpenVdbEvaluationIsDeterministic()
{
    if (!IsOpenVdbAvailable())
    {
        return true;
    }
    const slicer_core::TriangleMeshData mesh = MakeOctahedronMesh();
    const slicer_core::GlobalTextureFillPartitionRequest request = MakeRequest(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult first =
        EvaluateOpenVdb(request);
    const slicer_core::GlobalTextureFillPartitionResult second =
        EvaluateOpenVdb(request);
    return ExpectTrue(first.partitionPass && second.partitionPass, "repeat runs pass")
        && ExpectTrue(
            first.modelMask.values == second.modelMask.values,
            "OpenVDB model occupancy is deterministic")
        && ExpectTrue(
            first.textureSurfaceMask.values
                == second.textureSurfaceMask.values,
            "OpenVDB texture mask is deterministic")
        && ExpectTrue(
            first.modelFillMask.values == second.modelFillMask.values,
            "OpenVDB fill mask is deterministic")
        && ExpectTrue(
            first.widthMetrics.allTextureThresholdMm
                == second.widthMetrics.allTextureThresholdMm,
            "OpenVDB dynamic threshold is deterministic");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"build_lane_contract_is_stable", BuildLaneContractIsStable},
        {"closed_box_records_same_grid_conformance", ClosedBoxRecordsSameGridConformance},
        {"sloped_body_records_boundary_differences", SlopedBodyRecordsBoundaryDifferences},
        {"thin_wall_allows_all_texture_endpoint", ThinWallAllowsAllTextureEndpoint},
        {"closed_cavity_remains_outside_model", ClosedCavityRemainsOutsideModel},
        {"strict_topology_blocks_before_level_set", StrictTopologyBlocksBeforeLevelSet},
        {"width_threshold_uses_shared_units_and_step", WidthThresholdUsesSharedUnitsAndStep},
        {"repeated_openvdb_evaluation_is_deterministic", RepeatedOpenVdbEvaluationIsDeterministic},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "OpenVDB texture/fill conformance unit tests complete.\n";
    return 0;
}
