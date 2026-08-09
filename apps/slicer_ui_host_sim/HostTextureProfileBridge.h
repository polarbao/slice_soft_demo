#pragma once

#include "HostRequestBuilder.h"
#include "HostSliceSettings.h"

namespace HostTextureProfileBridge
{
/** @brief Validate all host texture enumerations before profile generation. */
bool IsValid(const hosttexturesettings& texture);

/** @brief Convert the Qt host texture application mode to the C DTO. */
enum hosttextureapplymode ToApplyMode(HostTextureApplyMode mode);

/** @brief Convert the Qt host texture sampler to the C DTO. */
enum hosttexturesampler ToSampler(HostTextureSampler sampler);

/** @brief Convert the Qt host UV address mode to the C DTO. */
enum hosttextureuvaddressmode ToUvAddressMode(HostTextureUvAddressMode mode);

/** @brief Convert the Qt host missing-texture policy to the C DTO. */
enum hosttexturemissingpolicy ToMissingPolicy(HostTextureMissingPolicy policy);

/** @brief Convert the Qt host non-surface policy to the C DTO. */
enum hosttexturenonsurfacepolicy ToNonSurfacePolicy(
    HostTextureNonSurfacePolicy policy);

/** @brief Convert the Qt host white-carrier policy to the C DTO. */
enum hosttexturewhitepolicy ToWhitePolicy(HostTextureWhitePolicy policy);
}
