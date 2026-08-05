#include "slicer_core/model/ObjFaceParser.h"

#include <stdexcept>
#include <string>

namespace slicer_core::model_detail {
namespace {

std::size_t ParseRequiredIndex(
    const std::string& token,
    const std::size_t itemCount)
{
    if (token.empty())
    {
        throw std::runtime_error("OBJ face contains empty vertex index");
    }
    const int index = std::stoi(token);
    if (index == 0)
    {
        throw std::runtime_error("OBJ vertex indices are 1-based; zero is invalid");
    }
    if (index > 0)
    {
        const std::size_t zeroBased = static_cast<std::size_t>(index - 1);
        if (zeroBased >= itemCount)
        {
            throw std::runtime_error("OBJ face references vertex outside loaded range");
        }
        return zeroBased;
    }
    const int resolved = static_cast<int>(itemCount) + index;
    if (resolved < 0)
    {
        throw std::runtime_error("OBJ negative face index is outside loaded range");
    }
    return static_cast<std::size_t>(resolved);
}

int ParseOptionalIndex(const std::string& token, const std::size_t itemCount)
{
    if (token.empty())
    {
        return -1;
    }
    const int index = std::stoi(token);
    if (index == 0)
    {
        throw std::runtime_error("OBJ indices are 1-based; zero is invalid");
    }
    if (index > 0)
    {
        const int zeroBased = index - 1;
        if (zeroBased >= static_cast<int>(itemCount))
        {
            throw std::runtime_error("OBJ face references index outside loaded range");
        }
        return zeroBased;
    }
    const int resolved = static_cast<int>(itemCount) + index;
    if (resolved < 0)
    {
        throw std::runtime_error("OBJ negative face index is outside loaded range");
    }
    return resolved;
}

}  // namespace

ObjFaceVertex ParseObjFaceVertex(
    const std::string_view token,
    const std::size_t vertexCount,
    const std::size_t texcoordCount,
    const std::size_t normalCount)
{
    ObjFaceVertex result;
    const std::size_t firstSlash = token.find('/');
    if (firstSlash == std::string_view::npos)
    {
        result.position_index = ParseRequiredIndex(std::string(token), vertexCount);
        return result;
    }

    result.position_index = ParseRequiredIndex(
        std::string(token.substr(0, firstSlash)),
        vertexCount);

    const std::size_t secondSlash = token.find('/', firstSlash + 1U);
    const std::string texcoordText = secondSlash == std::string_view::npos
        ? std::string(token.substr(firstSlash + 1U))
        : std::string(token.substr(firstSlash + 1U, secondSlash - firstSlash - 1U));
    result.texcoord_index = ParseOptionalIndex(texcoordText, texcoordCount);

    if (secondSlash != std::string_view::npos)
    {
        result.normal_index = ParseOptionalIndex(
            std::string(token.substr(secondSlash + 1U)),
            normalCount);
    }
    return result;
}

}  // namespace slicer_core::model_detail
