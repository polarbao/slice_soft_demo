#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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

slicer_core::AdaptedTriangleMesh MakeAttributedBox()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 2.0, 0.5);
    mesh.triangle_attributes.resize(mesh.mesh.triangles.size());
    for (std::size_t index{0U}; index < mesh.triangle_attributes.size(); ++index)
    {
        slicer_core::SurfaceTriangleAttributes& attributes = mesh.triangle_attributes.at(index);
        attributes.source_triangle_index = index;
        attributes.has_uv = true;
        attributes.material_name = "material-a";
        attributes.uv = {
            slicer_core::TexCoord{0.0, 0.0},
            slicer_core::TexCoord{1.0, 0.0},
            slicer_core::TexCoord{0.0, 1.0}};
    }

    slicer_core::MaterialInfo material;
    material.name = "material-a";
    material.diffuse_rgb = {10U, 20U, 30U};
    material.has_diffuse = true;
    material.diffuse_texture_path = "textures/color.png";
    material.has_texture = true;
    material.texture_exists = true;
    material.texture_source = "filesystem";
    mesh.material_infos.push_back(material);
    return mesh;
}

slicer_core::MeshRepairOptions MakeOptions()
{
    slicer_core::MeshRepairOptions options;
    options.enabled = false;
    options.mode = "strict_closed";
    options.weldToleranceMm = 0.001;
    options.maxBoundaryLoopEdges = 32U;
    options.maxBoundaryLoopDiameterMm = 0.5;
    options.allowNewFaces = false;
    return options;
}

slicer_core::Json LoadGolden()
{
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests" / "golden" / "expected" / "12e_mesh_repair_report_skeleton.json";
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json BuildStableProjection(const slicer_core::Json& report)
{
    const slicer_core::Json& hashes = report.at("hashes");
    return slicer_core::Json::object({
        {"schema", report.at("schema")},
        {"status", report.at("status")},
        {"mode", report.at("mode")},
        {"repairEnabled", report.at("repairEnabled")},
        {"repairAttempted", report.at("repairAttempted")},
        {"productionOutputWritten", report.at("productionOutputWritten")},
        {"input", report.at("input")},
        {"options", report.at("options")},
        {"hashContract", slicer_core::Json::object({
             {"algorithm", hashes.at("algorithm")},
             {"canonicalizationVersion", hashes.at("canonicalizationVersion")},
             {"geometryHashLength", static_cast<int>(hashes.at("preRepairGeometryHash").as_string().size())},
             {"attributeHashLength", static_cast<int>(hashes.at("preRepairAttributeHash").as_string().size())},
             {"operationHashLength", static_cast<int>(hashes.at("repairOperationHash").as_string().size())},
             {"optionsHashLength", static_cast<int>(hashes.at("optionsHash").as_string().size())},
         })},
        {"preRepair", report.at("preRepair")},
        {"eligibility", report.at("eligibility")},
        {"operations", report.at("operations")},
        {"attributePreservation", report.at("attributePreservation")},
        {"postRepair", report.at("postRepair")},
        {"admission", report.at("admission")},
        {"performance", report.at("performance")},
        {"issues", report.at("issues")},
    });
}

bool Sha256MatchesKnownVector()
{
    return ExpectTrue(
        slicer_core::ComputeMeshRepairSha256("abc")
            == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 implementation matches the abc known vector");
}

bool HashesAreStableAndIndependent()
{
    const slicer_core::AdaptedTriangleMesh original = MakeAttributedBox();
    const slicer_core::MeshRepairOptions options = MakeOptions();
    const slicer_core::MeshRepairHashes first =
        slicer_core::ComputeMeshRepairPreHashes(original, options);
    const slicer_core::MeshRepairHashes second =
        slicer_core::ComputeMeshRepairPreHashes(original, options);

    slicer_core::AdaptedTriangleMesh geometryChanged = original;
    geometryChanged.mesh.vertices.at(0U).x += 0.01;
    const slicer_core::MeshRepairHashes geometryHashes =
        slicer_core::ComputeMeshRepairPreHashes(geometryChanged, options);

    slicer_core::AdaptedTriangleMesh attributeChanged = original;
    attributeChanged.triangle_attributes.at(0U).material_name = "material-b";
    const slicer_core::MeshRepairHashes attributeHashes =
        slicer_core::ComputeMeshRepairPreHashes(attributeChanged, options);

    slicer_core::MeshRepairOptions optionsChanged = options;
    optionsChanged.weldToleranceMm = 0.002;
    const slicer_core::MeshRepairHashes changedOptionsHashes =
        slicer_core::ComputeMeshRepairPreHashes(original, optionsChanged);
    slicer_core::MeshRepairOptions windingOptions = options;
    windingOptions.allowWindingRepair = true;
    const slicer_core::MeshRepairHashes windingOptionHashes =
        slicer_core::ComputeMeshRepairPreHashes(original, windingOptions);
    slicer_core::MeshRepairOptions boundaryOptions = options;
    boundaryOptions.allowBoundaryFill = true;
    boundaryOptions.newFaceAttributePolicy = "inherit_uniform_material_no_uv";
    const slicer_core::MeshRepairHashes boundaryOptionHashes =
        slicer_core::ComputeMeshRepairPreHashes(original, boundaryOptions);

    return ExpectTrue(first.preRepairGeometryHash == second.preRepairGeometryHash, "geometry hash repeats")
        && ExpectTrue(first.preRepairAttributeHash == second.preRepairAttributeHash, "attribute hash repeats")
        && ExpectTrue(first.optionsHash == second.optionsHash, "options hash repeats")
        && ExpectTrue(first.repairOperationHash == second.repairOperationHash, "operation hash repeats")
        && ExpectTrue(first.preRepairGeometryHash != geometryHashes.preRepairGeometryHash, "geometry change changes geometry hash")
        && ExpectTrue(first.preRepairAttributeHash == geometryHashes.preRepairAttributeHash, "geometry change keeps attribute hash")
        && ExpectTrue(first.preRepairGeometryHash == attributeHashes.preRepairGeometryHash, "attribute change keeps geometry hash")
        && ExpectTrue(first.preRepairAttributeHash != attributeHashes.preRepairAttributeHash, "attribute change changes attribute hash")
        && ExpectTrue(first.optionsHash != changedOptionsHashes.optionsHash, "options change changes options hash")
        && ExpectTrue(first.optionsHash != windingOptionHashes.optionsHash, "winding option changes options hash")
        && ExpectTrue(first.optionsHash != boundaryOptionHashes.optionsHash, "boundary policy changes options hash");
}

bool OperationHashIsDeterministic()
{
    slicer_core::MeshRepairOperation operation;
    operation.operationId = 1U;
    operation.type = slicer_core::MeshRepairOperationType::RemoveExactDuplicateFace;
    operation.reasonCode = "MESH_DUPLICATE_FACES";
    operation.inputElementIds = {7U, 9U};
    operation.attributeDecision = slicer_core::MeshRepairAttributeDecision::Preserved;
    operation.affectedFaces = 1U;
    operation.durationMs = 1.0;

    const std::string first = slicer_core::ComputeMeshRepairOperationsHash({operation});
    operation.durationMs = 20.0;
    const std::string second = slicer_core::ComputeMeshRepairOperationsHash({operation});
    operation.inputElementIds.push_back(10U);
    const std::string changed = slicer_core::ComputeMeshRepairOperationsHash({operation});
    return ExpectTrue(first == second, "operation hash excludes nondeterministic timing")
        && ExpectTrue(first != changed, "operation content changes operation hash");
}

bool InvalidInputUsesStableError()
{
    slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    mesh.triangles.at(0U).at(0U) = -1;
    try
    {
        static_cast<void>(slicer_core::ComputeMeshRepairGeometryHash(mesh));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
                   error.Code() == slicer_core::MeshRepairErrorCode::InputInvalid,
                   "invalid mesh carries InputInvalid")
            && ExpectTrue(
                   slicer_core::MeshRepairErrorCodeName(error.Code())
                       == "E_12E_REPAIR_INPUT_INVALID",
                   "invalid mesh carries stable error text");
    }
    return ExpectTrue(false, "invalid mesh must throw MeshRepairError");
}

bool StableErrorNamesMatchFormalContract()
{
    return ExpectTrue(
               slicer_core::MeshRepairErrorCodeName(slicer_core::MeshRepairErrorCode::Ineligible)
                   == "E_12E_REPAIR_NOT_ELIGIBLE",
               "ineligible error matches formal contract")
        && ExpectTrue(
            slicer_core::MeshRepairErrorCodeName(slicer_core::MeshRepairErrorCode::AttributeMismatch)
                == "E_12E_REPAIR_ATTRIBUTE_CONFLICT",
            "attribute error matches formal contract");
}

bool ReportSkeletonMatchesGolden()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::MeshRepairOptions options = MakeOptions();
    const slicer_core::MeshRepairHashes hashes =
        slicer_core::ComputeMeshRepairPreHashes(mesh, options);

    slicer_core::MeshRepairInputSummary input;
    input.sourcePath = "model/obj/fixture/model.obj";
    input.inputFormat = "obj";
    input.vertexCount = static_cast<std::uint64_t>(mesh.mesh.vertices.size());
    input.triangleCount = static_cast<std::uint64_t>(mesh.mesh.triangles.size());
    input.componentCount = 1U;
    input.materialCount = static_cast<std::uint64_t>(mesh.material_infos.size());
    input.textureResourceCount = 1U;

    const slicer_core::Json report =
        slicer_core::BuildMeshRepairReportSkeleton(input, options, hashes);
    return ExpectTrue(
               BuildStableProjection(report).dump(2) == LoadGolden().dump(2),
               "report skeleton matches schema golden")
        && ExpectTrue(!report.at("repairAttempted").as_bool(), "skeleton does not attempt repair")
        && ExpectTrue(!report.at("productionOutputWritten").as_bool(), "skeleton does not write production output")
        && ExpectTrue(!report.at("admission").at("productionAllowed").as_bool(), "skeleton is non-production only");
}

}  // namespace

int main()
{
    try
    {
        const bool ok = Sha256MatchesKnownVector()
            && HashesAreStableAndIndependent()
            && OperationHashIsDeterministic()
            && InvalidInputUsesStableError()
            && StableErrorNamesMatchFormalContract()
            && ReportSkeletonMatchesGolden();
        if (!ok)
        {
            return 1;
        }
        std::cout << "mesh_repair_contract_unit_tests: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL unexpected exception: " << error.what() << '\n';
        return 1;
    }
}
