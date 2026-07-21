#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool GridCoversBoundingBoxWithPadding()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 2.0, 0.5);
    const slicer_core::TextureFillPartitionGridSpec grid =
        slicer_core::BuildTextureFillPartitionBenchmarkGrid(
            mesh.bbox_mm,
            0.10,
            1);

    return ExpectTrue(grid.width == 12, "grid width includes one-cell padding")
        && ExpectTrue(grid.height == 22, "grid height includes one-cell padding")
        && ExpectTrue(grid.depth == 7, "grid depth includes one-cell padding")
        && ExpectTrue(std::abs(grid.originXMm + 0.10) < 1.0e-12, "grid X origin")
        && ExpectTrue(std::abs(grid.originYMm + 0.10) < 1.0e-12, "grid Y origin")
        && ExpectTrue(std::abs(grid.originZMm + 0.10) < 1.0e-12, "grid Z origin");
}

bool ClosedFixtureProducesDiagnosticReleaseEvidence()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::AdaptedTriangleMesh adapted;
    adapted.mesh = mesh;
    adapted.triangle_attributes.resize(mesh.triangles.size());
    for (std::size_t index{0}; index < adapted.triangle_attributes.size(); ++index)
    {
        adapted.triangle_attributes[index].source_triangle_index = index;
    }

    slicer_core::TextureFillPartitionReleaseBenchmarkRequest request;
    request.mesh = &adapted.mesh;
    request.adaptedMesh = &adapted;
    request.caseName = "generated_closed_box";
    request.configPath = "generated://closed_box";
    request.modelPath = "generated://closed_box";
    request.buildType = "Release";
    request.voxelMm = 0.10;
    request.widthMm = 0.20;
    request.paddingVoxels = 1;

    const slicer_core::TextureFillPartitionReleaseBenchmarkResult result =
        slicer_core::RunTextureFillPartitionReleaseBenchmark(request);

    const slicer_core::Json& report = result.report;
    return ExpectTrue(result.partition.partitionPass, "closed fixture partition passes")
        && ExpectTrue(result.textureTransfer.available, "texture transfer is available")
        && ExpectTrue(result.rasterMapping.available, "raster mapping is available")
        && ExpectTrue(result.fullClosure.available, "full closure is available")
        && ExpectTrue(result.fullClosure.fullClosurePass, "full closure passes")
        && ExpectTrue(result.evidenceCollected, "release evidence is collected")
        && ExpectTrue(!result.productionAdmitted, "benchmark never admits production")
        && ExpectTrue(
               report.at("schema").as_string()
                   == "slicesoft.texture_fill_partition.release_evidence.12e_08c.1",
               "release evidence schema")
        && ExpectTrue(report.at("diagnosticOnly").as_bool(), "report is diagnostic-only")
        && ExpectTrue(
               !report.at("productionOutputWritten").as_bool(),
               "report records no production output")
        && ExpectTrue(
               report.at("timingsMs").at("outputWriteMs").as_double() == 0.0,
               "output time is excluded from core budget")
        && ExpectTrue(
               report.at("timingsMs").at("textureTransferMs").as_double() >= 0.0,
               "texture transfer time is measured")
        && ExpectTrue(
               report.at("timingsMs").at("rasterMappingMs").as_double() >= 0.0,
               "raster mapping time is measured")
        && ExpectTrue(
               report.at("timingsMs").at("fullClosureMs").as_double() >= 0.0,
               "full closure time is measured")
        && ExpectTrue(
               report.at("textureTransfer").at("available").as_bool(),
               "report contains texture transfer evidence")
        && ExpectTrue(
               report.at("rasterMapping").at("available").as_bool(),
               "report contains raster mapping evidence")
        && ExpectTrue(
               report.at("fullClosure").at("fullClosurePass").as_bool(),
               "report contains full-closure evidence")
        && ExpectTrue(
               report.at("partition").at("unassignedModelVoxels").as_double() == 0.0,
               "partition has no unassigned model voxels");
}

bool MismatchedAdaptedMeshIsRejected()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::AdaptedTriangleMesh adapted;
    adapted.mesh = slicer_core::MakeGeneratedBoxMesh(2.0, 1.0, 1.0);
    adapted.triangle_attributes.resize(adapted.mesh.triangles.size());

    slicer_core::TextureFillPartitionReleaseBenchmarkRequest request;
    request.mesh = &mesh;
    request.adaptedMesh = &adapted;
    try
    {
        static_cast<void>(
            slicer_core::RunTextureFillPartitionReleaseBenchmark(request));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return ExpectTrue(false, "mismatched adapted mesh must be rejected");
}

}  // namespace

int main()
{
    if (!GridCoversBoundingBoxWithPadding())
    {
        return 1;
    }
    if (!ClosedFixtureProducesDiagnosticReleaseEvidence())
    {
        return 1;
    }
    if (!MismatchedAdaptedMeshIsRejected())
    {
        return 1;
    }
    std::cout << "Texture fill partition release benchmark tests complete.\n";
    return 0;
}
