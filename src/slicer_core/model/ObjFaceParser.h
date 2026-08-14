#pragma once

#include <cstddef>
#include <string_view>

namespace slicer_core::model_detail {

/** @brief 从一个 OBJ 面顶点令牌解析出的索引。 */
struct ObjFaceVertex
{
    std::size_t position_index{0};
    int texcoord_index{-1};
    int normal_index{-1};
};

/**
 * @brief 解析并验证一个 OBJ 面顶点令牌。
 * @param token 采用 v、v/vt、v//vn 或 v/vt/vn 形式的 OBJ 令牌。
 * @param vertexCount 当前可用的源位置数量。
 * @param texcoordCount 当前可用的源纹理坐标数量。
 * @param normalCount 当前可用的源法线数量。
 * @return 解析后从 0 开始的索引；缺失的可选索引为 -1。
 */
[[nodiscard]] ObjFaceVertex ParseObjFaceVertex(
    std::string_view token,
    std::size_t vertexCount,
    std::size_t texcoordCount,
    std::size_t normalCount);

}  // namespace slicer_core::model_detail
