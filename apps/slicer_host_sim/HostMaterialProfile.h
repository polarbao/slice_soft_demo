#pragma once

#include "HostRequestBuilder.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 构建有效 Profile 中与材料相关的根成员。
 * @param settings 有效的宿主侧材料与支撑设置。
 * @param escapedProfileId 经 JSON 转义且不含引号的 Profile 标识。
 * @param canonical 接收规范化的缩进片段。
 * @param compact 接收紧凑响应片段。
 * @return 成功时返回非零值；两个输出字符串均由调用方持有。
 */
int HostBuildMaterialProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact);

#ifdef __cplusplus
}
#endif
