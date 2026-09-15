// P0FIX / P0-05：宿主能力判定必须解析 provides 数组，不得做文本子串匹配。
//
// 关键用例是第 3、4 组：能力名出现在 JSON 的【其它位置】时必须判为不支持，
// 而 provides 里真有时必须判为支持。裸子串实现会在第 3 组失败。

#include "HostSliceProtocolRoute.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <iostream>
#include <string>

namespace {

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

QJsonObject OutputWith(const QString& protocol)
{
    QJsonObject output;
    output.insert(QStringLiteral("packageProtocol"), protocol);
    return output;
}

// 正常模块自述：provides 含 16 项，包含 slice.rgbwsvt。
QByteArray FullModuleInfo()
{
    return QByteArrayLiteral(
        R"({"schema":"slicesoft.module_info.1","id":"slicer",)"
        R"("provides":["model.import","slice.rgbwsv","slice.rgbwsvt",)"
        R"("package.read_report"]})");
}

// 只声明 rgbwsv 的模块：provides 里【没有】rgbwsvt，
// 但该字符串出现在别的字段里——裸子串实现会在这里误判为支持。
QByteArray DecoyModuleInfo()
{
    return QByteArrayLiteral(
        R"({"schema":"slicesoft.module_info.1","id":"slicer",)"
        R"("provides":["model.import","slice.rgbwsv","package.read_report"],)"
        R"("notes":"this build does not implement slice.rgbwsvt yet"})");
}

}  // namespace

int main()
{
    bool passed = true;
    QString protocol;
    QString capability;
    QString error;

    // 1. 默认协议路由不变。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.rgbwsv.2")),
            FullModuleInfo(), &protocol, &capability, &error);
        passed = ExpectTrue(ok && capability == QStringLiteral("slice.rgbwsv"),
                     "p0.rgbwsv.2 routes to slice.rgbwsv")
            && passed;
    }

    // 2. provides 里真有 rgbwsvt 时必须放行。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.rgbwsvt.1")),
            FullModuleInfo(), &protocol, &capability, &error);
        passed = ExpectTrue(ok && capability == QStringLiteral("slice.rgbwsvt"),
                     "declared rgbwsvt capability is accepted")
            && passed;
    }

    // 3. 【核心】能力名只出现在别的字段里时必须判为不支持。
    //    裸子串实现会在这里放行，从而让宿主向不支持的模块提交 rgbwsvt 作业。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.rgbwsvt.1")),
            DecoyModuleInfo(), &protocol, &capability, &error);
        passed = ExpectTrue(!ok && !error.isEmpty(),
                     "capability name outside provides must NOT count as support")
            && passed;
    }

    // 4. 模块自述不是合法 JSON 时必须失败关闭，而不是放行。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.rgbwsvt.1")),
            QByteArrayLiteral("{ this is not valid json slice.rgbwsvt"),
            &protocol, &capability, &error);
        passed = ExpectTrue(!ok && !error.isEmpty(),
                     "malformed module info fails closed")
            && passed;
    }

    // 5. provides 不是数组时同样失败关闭。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.rgbwsvt.1")),
            QByteArrayLiteral(R"({"provides":"slice.rgbwsvt"})"),
            &protocol, &capability, &error);
        passed = ExpectTrue(!ok && !error.isEmpty(),
                     "non-array provides fails closed")
            && passed;
    }

    // 6. 未知协议仍然报错。
    {
        protocol.clear();
        capability.clear();
        error.clear();
        const bool ok = HostSliceProtocolRoute::Resolve(
            OutputWith(QStringLiteral("p0.unknown.9")),
            FullModuleInfo(), &protocol, &capability, &error);
        passed = ExpectTrue(!ok && !error.isEmpty(),
                     "unknown package protocol is rejected")
            && passed;
    }

    if (!passed)
    {
        std::cerr << "host slice protocol route: FAIL\n";
        return 1;
    }
    std::cout << "host slice protocol route: PASS\n";
    return 0;
}
