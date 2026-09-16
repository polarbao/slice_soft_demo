// P0FIX / P0-05：宿主能力判定必须解析 provides 数组，不得做文本子串匹配。
//
// 关键用例是第 3、4 组：能力名出现在 JSON 的【其它位置】时必须判为不支持，
// 而 provides 里真有时必须判为支持。裸子串实现会在第 3 组失败。
//
// 2026-09-15：本文件是 tests/support/Expect.h 的试点之一（F-17 + F-44）。
// 原先自带的 ExpectTrue 不打印实参，且 main() 没有任何 catch——被测代码一旦抛出
// 未捕获异常，进程不是快速失败而是挂住等 ctest 超时。

#include "HostSliceProtocolRoute.h"
#include "tests/support/Expect.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <string>

namespace {

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

// 一次解析的结果打包，便于在断言里打印实际拿到的能力与错误文本。
struct Resolved
{
    bool ok{false};
    QString protocol;
    QString capability;
    QString error;
};

Resolved Resolve(const QString& packageProtocol, const QByteArray& moduleInfo)
{
    Resolved out;
    out.ok = HostSliceProtocolRoute::Resolve(
        OutputWith(packageProtocol), moduleInfo,
        &out.protocol, &out.capability, &out.error);
    return out;
}

}  // namespace

int main()
{
    using slicesoft_test::RunCases;

    return RunCases("host slice protocol route", {
        {"default_protocol_routes_to_rgbwsv",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.rgbwsv.2"), FullModuleInfo());
             SLICESOFT_EXPECT_TRUE(r.ok, "p0.rgbwsv.2 must resolve");
             SLICESOFT_EXPECT_EQ(r.capability.toStdString(), std::string{"slice.rgbwsv"},
                 "p0.rgbwsv.2 routes to slice.rgbwsv");
         }},

        {"declared_rgbwsvt_capability_is_accepted",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.rgbwsvt.1"), FullModuleInfo());
             SLICESOFT_EXPECT_TRUE(r.ok, "declared rgbwsvt capability must resolve");
             SLICESOFT_EXPECT_EQ(r.capability.toStdString(), std::string{"slice.rgbwsvt"},
                 "declared rgbwsvt capability is accepted");
         }},

        // 核心用例：能力名只出现在别的字段里时必须判为不支持。
        // 裸子串实现会在这里放行，从而让宿主向不支持的模块提交 rgbwsvt 作业。
        {"capability_name_outside_provides_must_not_count_as_support",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.rgbwsvt.1"), DecoyModuleInfo());
             SLICESOFT_EXPECT_FALSE(r.ok,
                 "capability name outside provides must NOT count as support");
             SLICESOFT_EXPECT_FALSE(r.error.isEmpty(),
                 "rejection must carry a diagnosis");
         }},

        {"malformed_module_info_fails_closed",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.rgbwsvt.1"),
                 QByteArrayLiteral("{ this is not valid json slice.rgbwsvt"));
             SLICESOFT_EXPECT_FALSE(r.ok, "malformed module info fails closed");
             SLICESOFT_EXPECT_FALSE(r.error.isEmpty(),
                 "rejection must carry a diagnosis");
         }},

        {"non_array_provides_fails_closed",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.rgbwsvt.1"),
                 QByteArrayLiteral(R"({"provides":"slice.rgbwsvt"})"));
             SLICESOFT_EXPECT_FALSE(r.ok, "non-array provides fails closed");
             SLICESOFT_EXPECT_FALSE(r.error.isEmpty(),
                 "rejection must carry a diagnosis");
         }},

        {"unknown_package_protocol_is_rejected",
         []
         {
             const Resolved r = Resolve(QStringLiteral("p0.unknown.9"), FullModuleInfo());
             SLICESOFT_EXPECT_FALSE(r.ok, "unknown package protocol is rejected");
             SLICESOFT_EXPECT_FALSE(r.error.isEmpty(),
                 "rejection must carry a diagnosis");
         }},
    });
}
