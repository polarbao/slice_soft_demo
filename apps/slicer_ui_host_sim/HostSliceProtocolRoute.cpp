#include "HostSliceProtocolRoute.h"

bool HostSliceProtocolRoute::Resolve(
    const QJsonObject& output,
    const QByteArray& moduleInfo,
    QString* packageProtocol,
    QString* sliceCapability,
    QString* error)
{
    if (packageProtocol == nullptr || sliceCapability == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Host 切片协议路由输出目标无效。");
        }
        return false;
    }
    *packageProtocol = output.value(QStringLiteral("packageProtocol"))
        .toString(QStringLiteral("p0.rgbwsv.2"));
    if (*packageProtocol == QStringLiteral("p0.rgbwsv.2"))
    {
        *sliceCapability = QStringLiteral("slice.rgbwsv");
        return true;
    }
    if (*packageProtocol == QStringLiteral("p0.rgbwsvt.1"))
    {
        if (!moduleInfo.contains("slice.rgbwsvt"))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral(
                    "当前模块不支持 Profile 请求的 RGBWSVT 切片能力。");
            }
            return false;
        }
        *sliceCapability = QStringLiteral("slice.rgbwsvt");
        return true;
    }
    if (error != nullptr)
    {
        *error = QStringLiteral("有效 Profile 的输出协议不受支持：%1")
            .arg(*packageProtocol);
    }
    return false;
}
