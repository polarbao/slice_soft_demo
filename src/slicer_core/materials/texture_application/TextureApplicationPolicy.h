#pragma once

#include "slicer_core/config.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Texture application modes reserved by the material strategy boundary.
 */
enum class TextureApplicationMode
{
    FullVolume,
    SurfaceShell,
    TopSurfaceOnly,
    OuterSurfaceShell,
};

/**
 * @brief Texture application strategy object.
 */
struct TextureApplicationPolicy
{
    TextureApplicationMode mode{TextureApplicationMode::FullVolume};
    int shell_thickness_px{0};
    double shell_thickness_mm{0.0};
    std::string fill_role{"base"};
    std::string shell_region{"outer_surface"};
};

/**
 * @brief Create a texture application policy matching current legacy behavior.
 * @param config Legacy texture config.
 * @return Texture application policy boundary.
 */
TextureApplicationPolicy MakeLegacyTextureApplicationPolicy(const TextureConfig& config);

/**
 * @brief Convert a texture application mode to a stable name.
 * @param mode Texture application mode.
 * @return Stable mode name.
 */
std::string TextureApplicationModeName(TextureApplicationMode mode);

}  // namespace slicer_core
