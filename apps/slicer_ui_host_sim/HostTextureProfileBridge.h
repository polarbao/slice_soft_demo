#pragma once

#include "HostRequestBuilder.h"
#include "HostSliceSettings.h"

namespace HostTextureProfileBridge
{
/** @brief 在生成 Profile 前校验全部宿主纹理枚举值。 */
bool IsValid(const hosttexturesettings& texture);

/** @brief 将 Qt 宿主纹理应用模式转换为 C DTO。 */
enum hosttextureapplymode ToApplyMode(HostTextureApplyMode mode);

/** @brief 将 Qt 宿主纹理采样器转换为 C DTO。 */
enum hosttexturesampler ToSampler(HostTextureSampler sampler);

/** @brief 将 Qt 宿主 UV 寻址模式转换为 C DTO。 */
enum hosttextureuvaddressmode ToUvAddressMode(HostTextureUvAddressMode mode);

/** @brief 将 Qt 宿主纹理缺失策略转换为 C DTO。 */
enum hosttexturemissingpolicy ToMissingPolicy(HostTextureMissingPolicy policy);

/** @brief 将 Qt 宿主非表面策略转换为 C DTO。 */
enum hosttexturenonsurfacepolicy ToNonSurfacePolicy(
    HostTextureNonSurfacePolicy policy);

/** @brief 将 Qt 宿主白色载体策略转换为 C DTO。 */
enum hosttexturewhitepolicy ToWhitePolicy(HostTextureWhitePolicy policy);
}
