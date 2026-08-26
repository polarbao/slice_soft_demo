#pragma once

#include "HostSliceSettings.h"

#include <QString>

/** @brief 从部署在宿主旁的新版工艺 JSON 读取 T 通道策略。 */
class HostTransferProcessPresetLoader final
{
public:
    /** @brief 判断部署目录中是否至少存在一个可严格加载的 RGBWSVT 工艺。 */
    static bool HasAnyDeployedProfile(QString* error);

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
