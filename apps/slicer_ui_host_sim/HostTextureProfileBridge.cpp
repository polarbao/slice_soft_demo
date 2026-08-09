#include "HostTextureProfileBridge.h"

namespace HostTextureProfileBridge
{
bool IsValid(const hosttexturesettings& texture)
{
    return (texture.applymode
                == HostTextureApplyMode::SolidVolumeFromTopSurface
            || texture.applymode == HostTextureApplyMode::TopSurfaceOnly
            || texture.applymode == HostTextureApplyMode::TopSurfaceBand)
        && (texture.sampler == HostTextureSampler::Nearest
            || texture.sampler == HostTextureSampler::Bilinear)
        && (texture.uvaddressmode == HostTextureUvAddressMode::Clamp
            || texture.uvaddressmode == HostTextureUvAddressMode::Repeat)
        && (texture.missingpolicy
                == HostTextureMissingPolicy::WarnAndFallback
            || texture.missingpolicy == HostTextureMissingPolicy::FailFast)
        && (texture.nonsurfacepolicy
                == HostTextureNonSurfacePolicy::ModelMaterial
            || texture.nonsurfacepolicy == HostTextureNonSurfacePolicy::Empty)
        && (texture.whitepolicy == HostTextureWhitePolicy::FailClosed
            || texture.whitepolicy == HostTextureWhitePolicy::WhiteUnderbase);
}

enum hosttextureapplymode ToApplyMode(const HostTextureApplyMode mode)
{
    switch (mode)
    {
    case HostTextureApplyMode::SolidVolumeFromTopSurface:
        return HOST_TEXTURE_SOLID_VOLUME_FROM_TOP;
    case HostTextureApplyMode::TopSurfaceOnly:
        return HOST_TEXTURE_TOP_SURFACE_ONLY;
    case HostTextureApplyMode::TopSurfaceBand:
        return HOST_TEXTURE_TOP_SURFACE_BAND;
    }
    return static_cast<enum hosttextureapplymode>(-1);
}

enum hosttexturesampler ToSampler(const HostTextureSampler sampler)
{
    return sampler == HostTextureSampler::Nearest ? HOST_TEXTURE_NEAREST
                                                  : HOST_TEXTURE_BILINEAR;
}

enum hosttextureuvaddressmode ToUvAddressMode(
    const HostTextureUvAddressMode mode)
{
    return mode == HostTextureUvAddressMode::Repeat ? HOST_TEXTURE_UV_REPEAT
                                                    : HOST_TEXTURE_UV_CLAMP;
}

enum hosttexturemissingpolicy ToMissingPolicy(
    const HostTextureMissingPolicy policy)
{
    return policy == HostTextureMissingPolicy::FailFast
        ? HOST_TEXTURE_FAIL_FAST
        : HOST_TEXTURE_WARN_AND_FALLBACK;
}

enum hosttexturenonsurfacepolicy ToNonSurfacePolicy(
    const HostTextureNonSurfacePolicy policy)
{
    return policy == HostTextureNonSurfacePolicy::Empty
        ? HOST_TEXTURE_NON_SURFACE_EMPTY
        : HOST_TEXTURE_NON_SURFACE_MODEL_MATERIAL;
}

enum hosttexturewhitepolicy ToWhitePolicy(
    const HostTextureWhitePolicy policy)
{
    return policy == HostTextureWhitePolicy::WhiteUnderbase
        ? HOST_TEXTURE_WHITE_UNDERBASE
        : HOST_TEXTURE_WHITE_FAIL_CLOSED;
}
}
