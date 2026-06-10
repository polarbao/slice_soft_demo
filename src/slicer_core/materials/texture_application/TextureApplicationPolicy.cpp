#include "slicer_core/materials/texture_application/TextureApplicationPolicy.h"

namespace slicer_core
{

TextureApplicationPolicy MakeLegacyTextureApplicationPolicy(const TextureConfig&)
{
    return TextureApplicationPolicy{};
}

std::string TextureApplicationModeName(const TextureApplicationMode mode)
{
    switch (mode)
    {
    case TextureApplicationMode::FullVolume:
        return "full_volume";
    case TextureApplicationMode::SurfaceShell:
        return "surface_shell";
    case TextureApplicationMode::TopSurfaceOnly:
        return "top_surface_only";
    case TextureApplicationMode::OuterSurfaceShell:
        return "outer_surface_shell";
    }
    return "unknown";
}

}  // namespace slicer_core
