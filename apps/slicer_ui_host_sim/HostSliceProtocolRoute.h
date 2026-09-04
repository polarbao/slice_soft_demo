#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

/** @brief 将 Effective Profile 输出协议严格映射到 Host 切片能力。 */
class HostSliceProtocolRoute final
{
public:
    static bool Resolve(
        const QJsonObject& output,
        const QByteArray& moduleInfo,
        QString* packageProtocol,
        QString* sliceCapability,
        QString* error);
};
