#pragma once

#include "HostRequestBuilder.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 构建「多材质纵深 RGB + 按需补白」候选的材料相关根成员。
 * @param settings 有效的宿主侧材料与支撑设置。
 * @param escapedProfileId 经 JSON 转义且不含引号的 Profile 标识。
 * @param canonical 接收规范化的缩进片段。
 * @param compact 接收紧凑响应片段。
 * @return 成功时返回非零值；两个输出字符串均由调用方持有。
 *
 * 本函数独立于既有的 HostBuildMaterialProfileFragments，因此启用 MATVOL 时
 * 既有工艺的片段构造代码路径完全不被触碰，旧 Profile 字节与哈希由此得到
 * 结构性保证。片段内 materialVolumePolicy 按字母序落在 materialRoleMapping
 * 与 modelFill 之间。
 */
int HostBuildVolumetricProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact);

#ifdef __cplusplus
}
#endif
