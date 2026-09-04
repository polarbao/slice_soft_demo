#pragma once

#include "HostSliceSettings.h"

/** @brief 将宿主 T 策略条件写入新版 Effective Profile。 */
class HostTransferProfileBridge final
{
public:
    /** @brief 校验 Profile、协议与 T 策略的显式一致性。 */
    static bool Validate(
        const hostslicesettings& settings,
        QString* error);

    /** @brief 仅对 RGBWSVT 分支添加协议字段并重算闭合 hash。 */
    static void Apply(
        const hostslicesettings& settings,
        QJsonObject* profile,
        QString* profileHash);
};
