#include "HostSliceProtocolRoute.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

/**
 * @brief 按 pm_module_info 的 provides 数组精确判定能力。
 *
 * 此处必须解析结构而不是对整份 JSON 做子串匹配：能力名出现在 JSON 的任何
 * 位置（未来的描述字段、错误示例、或某个更长能力名的前缀）都会让子串匹配
 * 误判为"支持"；反过来模块若把能力名写成等价的 JSON 转义形式，子串匹配又
 * 会误判为"不支持"。
 */
bool ModuleProvidesCapability(
    const QByteArray& moduleInfo,
    const QString& capability)
{
    QJsonParseError parseError{};
    const QJsonDocument document =
        QJsonDocument::fromJson(moduleInfo, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return false;
    }
    const QJsonValue provides =
        document.object().value(QStringLiteral("provides"));
    if (!provides.isArray())
    {
        return false;
    }
    const QJsonArray entries = provides.toArray();
    for (const QJsonValue& entry : entries)
    {
        if (entry.isString() && entry.toString() == capability)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

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
        if (!ModuleProvidesCapability(
                moduleInfo, QStringLiteral("slice.rgbwsvt")))
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
