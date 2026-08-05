#pragma once

#include <cstddef>
#include <string_view>

namespace slicer_core::model_detail {

/** @brief Resolved indices from one OBJ face vertex token. */
struct ObjFaceVertex
{
    std::size_t position_index{0};
    int texcoord_index{-1};
    int normal_index{-1};
};

/**
 * @brief Parses and validates one OBJ face vertex token.
 * @param token OBJ token in v, v/vt, v//vn, or v/vt/vn form.
 * @param vertexCount Number of source positions currently available.
 * @param texcoordCount Number of source texture coordinates currently available.
 * @param normalCount Number of source normals currently available.
 * @return Resolved zero-based indices, with -1 for absent optional indices.
 */
[[nodiscard]] ObjFaceVertex ParseObjFaceVertex(
    std::string_view token,
    std::size_t vertexCount,
    std::size_t texcoordCount,
    std::size_t normalCount);

}  // namespace slicer_core::model_detail
