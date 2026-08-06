#include "slicer_core/geometry/repair/DeterministicObjAssetWriter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace slicer_core
{
namespace
{

struct MaterialAsset
{
    std::string name;
    const MaterialInfo* source{nullptr};
    std::filesystem::path relativeTexturePath;
};

void ThrowIfCancelled(const DeterministicObjAssetWriteRequest& request)
{
    if (request.cancellationRequested && request.cancellationRequested())
    {
        throw std::runtime_error("deterministic OBJ asset writing was cancelled");
    }
}

bool HasInvalidTokenCharacter(const std::string& value)
{
    return value.empty() || std::any_of(
        value.begin(), value.end(),
        [](const unsigned char character)
        {
            return std::isspace(character) != 0
                || std::iscntrl(character) != 0;
        });
}

std::string Lowercase(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string FourDigitIndex(const std::size_t index)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setw(4) << std::setfill('0') << index;
    return stream.str();
}

void ValidateRequest(const DeterministicObjAssetWriteRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw std::runtime_error("deterministic OBJ writer requires a mesh");
    }
    if (request.mesh->mesh.vertices.empty()
        || request.mesh->mesh.triangles.empty())
    {
        throw std::runtime_error("deterministic OBJ writer requires non-empty geometry");
    }
    if (request.mesh->triangle_attributes.size()
        != request.mesh->mesh.triangles.size())
    {
        throw std::runtime_error(
            "deterministic OBJ writer requires one attribute record per triangle");
    }
    if (Lowercase(request.outputObjPath.extension().string()) != ".obj")
    {
        throw std::runtime_error("repair output format must be OBJ");
    }
    if (request.outputObjPath.filename().empty())
    {
        throw std::runtime_error("repair OBJ output path is invalid");
    }
    if (std::filesystem::exists(request.outputObjPath))
    {
        throw std::runtime_error("repair OBJ output already exists");
    }
}

std::vector<MaterialAsset> BuildMaterialAssets(
    const AdaptedTriangleMesh& mesh)
{
    std::vector<MaterialAsset> assets;
    std::map<std::string, std::size_t> indices;
    for (const MaterialInfo& material : mesh.material_infos)
    {
        if (HasInvalidTokenCharacter(material.name))
        {
            throw std::runtime_error(
                "repair material name is empty or contains whitespace/control characters");
        }
        if (!indices.emplace(material.name, assets.size()).second)
        {
            throw std::runtime_error("repair material names must be unique");
        }
        assets.push_back({material.name, &material, {}});
    }
    for (const SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        if (attributes.material_name.empty())
        {
            continue;
        }
        if (HasInvalidTokenCharacter(attributes.material_name))
        {
            throw std::runtime_error(
                "triangle material name contains whitespace/control characters");
        }
        if (indices.emplace(attributes.material_name, assets.size()).second)
        {
            assets.push_back({attributes.material_name, nullptr, {}});
        }
    }
    return assets;
}

void CopyTextureResources(
    const DeterministicObjAssetWriteRequest& request,
    std::vector<MaterialAsset>& materials,
    DeterministicObjAssetWriteResult& result)
{
    std::size_t textureIndex{0U};
    const std::filesystem::path resourcesDirectory =
        request.outputObjPath.parent_path() / "resources";
    for (MaterialAsset& material : materials)
    {
        if (material.source == nullptr || !material.source->has_texture)
        {
            continue;
        }
        ThrowIfCancelled(request);
        const std::filesystem::path sourcePath =
            material.source->diffuse_texture_path;
        if (!material.source->texture_exists
            || sourcePath.empty()
            || !std::filesystem::is_regular_file(sourcePath))
        {
            throw std::runtime_error(
                "repair texture resource is missing: " + sourcePath.generic_string());
        }
        std::string extension = Lowercase(sourcePath.extension().string());
        if (extension.empty())
        {
            extension = ".bin";
        }
        material.relativeTexturePath = std::filesystem::path("resources")
            / ("texture_" + FourDigitIndex(textureIndex) + extension);
        const std::filesystem::path outputPath =
            request.outputObjPath.parent_path() / material.relativeTexturePath;
        std::filesystem::create_directories(resourcesDirectory);
        if (std::filesystem::exists(outputPath))
        {
            throw std::runtime_error("repair texture output already exists");
        }
        if (!std::filesystem::copy_file(sourcePath, outputPath))
        {
            throw std::runtime_error(
                "failed to copy repair texture resource: "
                + sourcePath.generic_string());
        }
        result.texturePaths.push_back(outputPath);
        ++textureIndex;
    }
    result.textureBytesPreserved = true;
}

void WriteMaterialLibrary(
    const std::filesystem::path& path,
    const std::vector<MaterialAsset>& materials)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output)
    {
        throw std::runtime_error(
            "failed to create repair MTL: " + path.generic_string());
    }
    output << "# SliceSoft deterministic repair material library v1\n";
    output << std::fixed << std::setprecision(9);
    for (const MaterialAsset& material : materials)
    {
        output << "newmtl " << material.name << '\n';
        if (material.source != nullptr && material.source->has_diffuse)
        {
            constexpr double denominator{255.0};
            output << "Kd "
                   << static_cast<double>(material.source->diffuse_rgb[0]) / denominator
                   << ' '
                   << static_cast<double>(material.source->diffuse_rgb[1]) / denominator
                   << ' '
                   << static_cast<double>(material.source->diffuse_rgb[2]) / denominator
                   << '\n';
        }
        if (!material.relativeTexturePath.empty())
        {
            output << "map_Kd "
                   << material.relativeTexturePath.generic_string() << '\n';
        }
        output << '\n';
    }
    output.flush();
    if (!output)
    {
        throw std::runtime_error(
            "failed to write repair MTL: " + path.generic_string());
    }
}

void WriteObj(
    const DeterministicObjAssetWriteRequest& request,
    const bool hasMaterials,
    const std::filesystem::path& mtlPath)
{
    const AdaptedTriangleMesh& adapted = *request.mesh;
    std::ofstream output(
        request.outputObjPath,
        std::ios::binary | std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output)
    {
        throw std::runtime_error(
            "failed to create repair OBJ: "
            + request.outputObjPath.generic_string());
    }
    output << "# SliceSoft deterministic repair asset v1\n";
    if (hasMaterials)
    {
        output << "mtllib " << mtlPath.filename().generic_string() << '\n';
    }
    output << std::fixed << std::setprecision(17);
    for (const Vec3& vertex : adapted.mesh.vertices)
    {
        ThrowIfCancelled(request);
        output << "v " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
    }

    std::vector<std::array<std::size_t, 3>> textureIndices(
        adapted.mesh.triangles.size());
    std::size_t nextTextureIndex{1U};
    for (std::size_t triangleIndex{0U};
         triangleIndex < adapted.triangle_attributes.size();
         ++triangleIndex)
    {
        const SurfaceTriangleAttributes& attributes =
            adapted.triangle_attributes.at(triangleIndex);
        if (!attributes.has_uv)
        {
            continue;
        }
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            textureIndices.at(triangleIndex).at(corner) = nextTextureIndex++;
            output << "vt " << attributes.uv.at(corner).u << ' '
                   << attributes.uv.at(corner).v << '\n';
        }
    }

    std::string activeMaterial;
    bool materialActive{false};
    for (std::size_t triangleIndex{0U};
         triangleIndex < adapted.mesh.triangles.size();
         ++triangleIndex)
    {
        ThrowIfCancelled(request);
        const SurfaceTriangleAttributes& attributes =
            adapted.triangle_attributes.at(triangleIndex);
        if (!attributes.material_name.empty()
            && (!materialActive || activeMaterial != attributes.material_name))
        {
            output << "usemtl " << attributes.material_name << '\n';
            activeMaterial = attributes.material_name;
            materialActive = true;
        }
        const std::array<int, 3>& triangle =
            adapted.mesh.triangles.at(triangleIndex);
        output << 'f';
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            const int vertexIndex = triangle.at(corner);
            if (vertexIndex < 0
                || static_cast<std::size_t>(vertexIndex)
                    >= adapted.mesh.vertices.size())
            {
                throw std::runtime_error(
                    "repair triangle references an invalid vertex");
            }
            output << ' ' << (vertexIndex + 1);
            if (attributes.has_uv)
            {
                output << '/' << textureIndices.at(triangleIndex).at(corner);
            }
        }
        output << '\n';
    }
    output.flush();
    if (!output)
    {
        throw std::runtime_error(
            "failed to write repair OBJ: "
            + request.outputObjPath.generic_string());
    }
}

}  // namespace

DeterministicObjAssetWriteResult WriteDeterministicObjAsset(
    const DeterministicObjAssetWriteRequest& request)
{
    ValidateRequest(request);
    ThrowIfCancelled(request);
    std::filesystem::create_directories(request.outputObjPath.parent_path());

    DeterministicObjAssetWriteResult result;
    result.objPath = request.outputObjPath;
    std::vector<MaterialAsset> materials = BuildMaterialAssets(*request.mesh);
    if (!materials.empty())
    {
        result.mtlPath = request.outputObjPath;
        result.mtlPath.replace_extension(".mtl");
        if (std::filesystem::exists(result.mtlPath))
        {
            throw std::runtime_error("repair MTL output already exists");
        }
    }

    try
    {
        CopyTextureResources(request, materials, result);
        if (!materials.empty())
        {
            WriteMaterialLibrary(result.mtlPath, materials);
        }
        WriteObj(request, !materials.empty(), result.mtlPath);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(result.objPath, ignored);
        if (!result.mtlPath.empty())
        {
            std::filesystem::remove(result.mtlPath, ignored);
        }
        for (const std::filesystem::path& texturePath : result.texturePaths)
        {
            std::filesystem::remove(texturePath, ignored);
        }
        throw;
    }

    result.uvPreserved = true;
    result.materialsPreserved = true;
    return result;
}

}  // namespace slicer_core
