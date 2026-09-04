#pragma once

#include "HostSliceSettings.h"

#include <QString>
#include <QStringList>

/** @brief 从部署在宿主旁的新版工艺 JSON 读取 T 通道策略。 */
class HostTransferProcessPresetLoader final
{
public:
    /** @brief 判断部署目录中是否至少存在一个可严格加载的 RGBWSVT 工艺。 */
    static bool HasAnyDeployedProfile(QString* error);

    /**
     * @brief 按文件名顺序列出部署目录中的全部 RGBWSVT 工艺文件名。
     * @return 未过滤可加载性的原始文件名列表；目录不存在时为空。
     */
    static QStringList DeployedProfileFileNames();

    /**
     * @brief 读取部署目录共享的那一条 T 通道策略。
     *
     * 部署目录里每个 *_rgbwsvt.json 都整体拷贝了一份旧工艺，但本类只读其中的
     * transferChannelPolicy 块，而这些块在各文件间是同一条策略的副本。因此这里
     * 取按文件名顺序第一个可严格加载的文件即可，无需把文件名写死在调用方。
     * 副本之间是否已漂移由 matvol_t_host_profile_tests 守。
     *
     * @param transferSettings 接收 T 识别策略。
     * @param error 接收失败即拒绝原因。
     * @return 至少有一个文件可严格加载时返回 true。
     */
    static bool LoadDeployedPolicy(
        hosttransferchannelsettings* transferSettings,
        QString* error);

    /**
     * @brief 读取并严格校验一个 RGBWSVT 工艺文件。
     * @param fileName 工艺目录内的文件名。
     * @param transferSettings 接收外部配置中的 T 识别策略。
     * @param error 接收失败即拒绝原因。
     * @return 协议、通道顺序与策略完整有效时返回 true。
     */
    static bool Load(
        const QString& fileName,
        hosttransferchannelsettings* transferSettings,
        QString* error);
};
