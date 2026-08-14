#pragma once

#include "HostRequestBuilder.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 构建有效 Profile 的纹理根成员。
 * @param settings 有效的宿主侧纹理设置。
 * @param canonical 接收不带逗号的规范化缩进成员。
 * @param compact 接收不带逗号的紧凑成员。
 * @return 成功时返回非零值；两个输出字符串均由调用方持有。
 */
int HostBuildTextureProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    char** canonical,
    char** compact);

#ifdef __cplusplus
}
#endif
