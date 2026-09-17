// P0FIX / F-34：模型导入文件尺寸上限。
//
// 这是 F-33（JSON 解析深度上限）的姊妹用例——同一个威胁模型的另一半。
// `model.import` 是 `in_process` 能力，解析与分配都发生在【宿主地址空间】；
// 在加这道闸门之前，一个 2 GB 的 ASCII STL 足以把宿主打印软件撑爆。
//
// 判据是「超限时抛出可捕获的 std::exception，且错误信息可操作」，
// 不是「返回某个值」——调用方能拿到诊断，宿主不会消失。
//
// 【本用例覆盖不到的边界，说在前面】它测的是判定函数本身，
// 不能证明 `load_model_report` 真的调用了它（那需要一个 >512 MiB 的样本，
// 不适合进仓库）。接线是靠人工证伪验证的：把常量临时调低到 1 MiB，
// 跑字节级基线（11 个真实模型），确认全部被拒；还原后恢复通过。

#include "slicer_core/model.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <exception>
#include <string>

namespace {

using slicer_core::EnsureModelFileWithinLimit;
using slicer_core::kMaxModelFileBytes;

constexpr std::uintmax_t kOneMiB{1024ULL * 1024ULL};

std::string CaptureThrow(const std::uintmax_t actual, const std::uintmax_t limit)
{
    try
    {
        EnsureModelFileWithinLimit("C:/models/huge.stl", actual, limit);
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return {};
}

}  // namespace

int main()
{
    return slicesoft_test::RunCases(
        "model import size limit (F-34)",
        {
            {"under limit passes",
             [] {
                 EnsureModelFileWithinLimit("m.obj", kOneMiB, 2U * kOneMiB);
             }},
            {"exactly at limit passes",
             [] {
                 // 边界取「小于等于」：恰好等于上限的文件不该被拒。
                 EnsureModelFileWithinLimit("m.obj", 2U * kOneMiB, 2U * kOneMiB);
             }},
            {"one byte over throws",
             [] {
                 const std::string what = CaptureThrow(2U * kOneMiB + 1U, 2U * kOneMiB);
                 SLICESOFT_EXPECT_FALSE(what.empty(), "超限必须抛出");
             }},
            {"message is actionable",
             [] {
                 const std::string what = CaptureThrow(3U * kOneMiB, kOneMiB);
                 // 三要素缺一不可：是哪个文件、实际多大、上限多少。
                 // 少了任何一个，收到报错的人都无法判断该怎么办。
                 SLICESOFT_EXPECT_TRUE(
                     what.find("huge.stl") != std::string::npos, "错误信息须点名文件");
                 SLICESOFT_EXPECT_TRUE(
                     what.find(std::to_string(3U * kOneMiB)) != std::string::npos,
                     "错误信息须给出实际字节数");
                 SLICESOFT_EXPECT_TRUE(
                     what.find(std::to_string(kOneMiB)) != std::string::npos,
                     "错误信息须给出上限");
             }},
            {"zero limit means unlimited",
             [] {
                 // 0 是「不限」，供测试构造用；产品路径永远传 kMaxModelFileBytes。
                 EnsureModelFileWithinLimit("m.obj", 8ULL * 1024ULL * kOneMiB, 0U);
             }},
            {"constant is 512 MiB",
             [] {
                 // 钉住常量：它是仓库内最大真实模型（39.96 MB）的约 13 倍。
                 // 若有人改小它，正常资产会被拒；改大则闸门失去意义。
                 // 任一方向的改动都应当先改这条断言，即先被迫解释理由。
                 SLICESOFT_EXPECT_EQ(
                     kMaxModelFileBytes, 512ULL * kOneMiB, "导入上限常量被改动");
             }},
        });
}
