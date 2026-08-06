#include "slicer_core/geometry/repair/DeterministicObjAssetWriter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using slicer_core::AdaptedTriangleMesh;
using slicer_core::DeterministicObjAssetWriteRequest;
using slicer_core::DeterministicObjAssetWriteResult;
using slicer_core::MaterialInfo;
using slicer_core::SurfaceTriangleAttributes;
using slicer_core::TexCoord;
using slicer_core::Vec3;

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read test output");
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void WriteBytes(
    const std::filesystem::path& path,
    const std::vector<unsigned char>& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

AdaptedTriangleMesh MakeTexturedMesh(const std::filesystem::path& texturePath)
{
    AdaptedTriangleMesh mesh;
    mesh.mesh.vertices = {
        Vec3{0.0, 0.0, 0.0},
        Vec3{1.0, 0.0, 0.0},
        Vec3{1.0, 1.0, 0.0},
        Vec3{0.0, 1.0, 0.0}};
    mesh.mesh.triangles = {{0, 1, 2}, {0, 2, 3}};

    SurfaceTriangleAttributes first;
    first.source_triangle_index = 0U;
    first.has_uv = true;
    first.uv = {TexCoord{0.0, 0.0}, TexCoord{1.0, 0.0}, TexCoord{1.0, 1.0}};
    first.material_name = "fixture";
    SurfaceTriangleAttributes second;
    second.source_triangle_index = 1U;
    second.has_uv = true;
    second.uv = {TexCoord{0.0, 0.0}, TexCoord{1.0, 1.0}, TexCoord{0.0, 1.0}};
    second.material_name = "fixture";
    mesh.triangle_attributes = {first, second};

    MaterialInfo material;
    material.name = "fixture";
    material.diffuse_rgb = {12U, 34U, 56U};
    material.has_diffuse = true;
    material.diffuse_texture_path = texturePath;
    material.has_texture = true;
    material.texture_exists = true;
    mesh.material_infos.push_back(material);
    return mesh;
}

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool WritesDeterministicAttributedAsset()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_obj_writer";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const std::filesystem::path texturePath = root / "source.png";
    const std::vector<unsigned char> textureBytes{0x89U, 0x50U, 0x4eU, 0x47U, 0x01U};
    WriteBytes(texturePath, textureBytes);
    const AdaptedTriangleMesh mesh = MakeTexturedMesh(texturePath);

    const std::filesystem::path firstRoot = root / "first";
    const std::filesystem::path secondRoot = root / "second";
    DeterministicObjAssetWriteRequest firstRequest;
    firstRequest.mesh = &mesh;
    firstRequest.outputObjPath = firstRoot / "repaired.obj";
    const DeterministicObjAssetWriteResult first =
        slicer_core::WriteDeterministicObjAsset(firstRequest);

    DeterministicObjAssetWriteRequest secondRequest;
    secondRequest.mesh = &mesh;
    secondRequest.outputObjPath = secondRoot / "repaired.obj";
    const DeterministicObjAssetWriteResult second =
        slicer_core::WriteDeterministicObjAsset(secondRequest);

    const std::string obj = ReadText(first.objPath);
    const std::string mtl = ReadText(first.mtlPath);
    bool pass = true;
    pass = Expect(obj == ReadText(second.objPath), "OBJ bytes must be deterministic") && pass;
    pass = Expect(mtl == ReadText(second.mtlPath), "MTL bytes must be deterministic") && pass;
    pass = Expect(obj.find("mtllib repaired.mtl") != std::string::npos,
                  "OBJ must bind the sibling MTL") && pass;
    pass = Expect(obj.find("usemtl fixture") != std::string::npos,
                  "OBJ must retain triangle material assignment") && pass;
    pass = Expect(obj.find("f 1/1 2/2 3/3") != std::string::npos,
                  "OBJ must retain indexed UV assignment") && pass;
    pass = Expect(mtl.find("map_Kd resources/texture_0000.png") != std::string::npos,
                  "MTL must reference a job-owned texture") && pass;
    pass = Expect(ReadText(first.texturePaths.at(0U))
                      == ReadText(second.texturePaths.at(0U)),
                  "copied texture bytes must be deterministic") && pass;
    pass = Expect(first.uvPreserved && first.materialsPreserved
                      && first.textureBytesPreserved,
                  "writer must report complete attribute evidence") && pass;
    std::filesystem::remove_all(root, ignored);
    return pass;
}

bool RejectsMissingTextureAndCleansPartialOutput()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_obj_writer_missing";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    AdaptedTriangleMesh mesh = MakeTexturedMesh(root / "missing.png");
    mesh.material_infos.at(0U).texture_exists = false;
    DeterministicObjAssetWriteRequest request;
    request.mesh = &mesh;
    request.outputObjPath = root / "staging" / "repaired.obj";
    bool rejected{false};
    try
    {
        static_cast<void>(slicer_core::WriteDeterministicObjAsset(request));
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    const bool pass = Expect(rejected, "missing texture must fail closed")
        && Expect(!std::filesystem::exists(request.outputObjPath),
                  "failed writer must not leave an OBJ")
        && Expect(!std::filesystem::exists(
                      request.outputObjPath.parent_path() / "repaired.mtl"),
                  "failed writer must not leave an MTL");
    std::filesystem::remove_all(root, ignored);
    return pass;
}

}  // namespace

int main()
{
    const bool pass = WritesDeterministicAttributedAsset()
        && RejectsMissingTextureAndCleansPartialOutput();
    if (!pass)
    {
        return 1;
    }
    std::cout << "stage14d08_r3_obj_writer_tests: PASS\n";
    return 0;
}
