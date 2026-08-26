#pragma once

#include "HostSliceSettings.h"

class QSettings;

/** @brief Workspace v8 的 RGBWSVT 协议与 T 通道策略持久化边界。 */
class HostWorkspaceTransferState final
{
public:
    /** @brief 保存协议选择与完整外部 T 策略镜像。 */
    static void Save(
        QSettings& settings,
        HostPackageProtocol protocol,
        const hosttransferchannelsettings& transfer);

    /**
     * @brief 恢复 v8 状态，或将 v7 显式迁移为旧协议且禁用 T。
     * @return 状态字段完整且彼此一致时返回 true。
     */
    static bool Restore(
        QSettings& settings,
        int schemaVersion,
        const QString& profileId,
        const QString& processPresetId,
        HostPackageProtocol* protocol,
        hosttransferchannelsettings* transfer);
};
