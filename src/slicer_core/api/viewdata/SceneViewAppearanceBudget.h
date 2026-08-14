#pragma once

#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"

#include <cstddef>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 将外观纹理降采样到最大边长。
 *
 * 返回的外观保持 RGBA8 纹理语义和材质绑定。纹理与外观标识根据结果内容
 * 重新计算；降采样后相同的纹理会去重，同时保证所有材质引用闭合。
 *
 * @param appearance 要复制并降采样的闭合已解析外观。
 * @param maxTextureEdgePx 以像素为单位的纹理最大宽度或高度，必须大于零。
 * @return 纹理引用闭合的独立外观副本。
 * @throws std::invalid_argument 源纹理或绑定无效时抛出。
 */
[[nodiscard]] ResolvedViewAppearance DownsampleAppearanceTextures(
    const ResolvedViewAppearance& appearance,
    std::size_t maxTextureEdgePx);

}  // namespace slicer_core::api::viewdata_detail
