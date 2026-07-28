#include "slicer_core/scene/SceneResourceIdentity.h"

#include "slicer_core/system/Sha256.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace slicer_core
{
namespace
{

std::string ReadResource(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read scene resource identity: "
            + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::string StableTextureIdentity(const MaterialInfo& material)
{
    if (material.diffuse_texture_path.empty())
    {
        return {};
    }
    if (material.texture_source == "3mf_internal")
    {
        return material.diffuse_texture_path.filename()
            .generic_string();
    }
    return material.diffuse_texture_path.lexically_normal()
        .generic_string();
}

}  // namespace

std::string ComputeSceneResourceHash(const SceneModel& model)
{
    std::string payload;
    payload.reserve(model.material_infos.size() * 128U + 128U);
    payload.append(model.model_path.lexically_normal().generic_string());
    payload.push_back('|');
    payload.append(model.format);
    payload.push_back('|');
    payload.append(std::to_string(model.material_infos.size()));
    for (const MaterialInfo& material : model.material_infos)
    {
        payload.push_back('|');
        payload.append(material.name);
        payload.push_back('|');
        payload.append(std::to_string(material.has_diffuse));
        payload.push_back(',');
        payload.append(std::to_string(material.has_texture));
        payload.push_back(',');
        payload.append(std::to_string(material.texture_exists));
        payload.push_back('|');
        payload.append(
            std::to_string(material.diffuse_rgb.at(0U)));
        payload.push_back(',');
        payload.append(
            std::to_string(material.diffuse_rgb.at(1U)));
        payload.push_back(',');
        payload.append(
            std::to_string(material.diffuse_rgb.at(2U)));
        payload.push_back('|');
        payload.append(material.texture_source);
        payload.push_back('|');
        payload.append(StableTextureIdentity(material));
        if (material.texture_exists
            && !material.diffuse_texture_path.empty())
        {
            payload.push_back('|');
            payload.append(ComputeSha256(
                ReadResource(material.diffuse_texture_path)));
        }
    }
    payload.push_back('|');
    payload.append(std::to_string(
        model.three_mf.texture_resource_count));
    payload.push_back(',');
    payload.append(std::to_string(
        model.three_mf.texture_loaded_count));
    return ComputeSha256(payload);
}

}  // namespace slicer_core
