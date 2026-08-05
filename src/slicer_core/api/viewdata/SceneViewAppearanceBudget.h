#pragma once

#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"

#include <cstddef>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Downsamples appearance textures to a maximum edge length.
 *
 * The returned appearance preserves RGBA8 texture semantics and material
 * bindings. Texture and appearance identities are recomputed from the
 * resulting content. Textures that become identical after downsampling are
 * deduplicated while all material references remain closed.
 *
 * @param appearance Closed resolved appearance to copy and downsample.
 * @param maxTextureEdgePx Maximum texture width or height in pixels; must be
 * greater than zero.
 * @return Independent appearance copy with closed texture references.
 * @throws std::invalid_argument When a source texture or binding is invalid.
 */
[[nodiscard]] ResolvedViewAppearance DownsampleAppearanceTextures(
    const ResolvedViewAppearance& appearance,
    std::size_t maxTextureEdgePx);

}  // namespace slicer_core::api::viewdata_detail
