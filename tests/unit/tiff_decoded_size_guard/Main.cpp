// P0FIX / F-52：TIFF 声明尺寸与文件实际大小的对账。
//
// 这是 F-33 / F-34 / F-51 同族里最后一条，也是性质最坏的一条：
// 前三条挡的是「文件真的很大」，本条挡的是【文件很小却声称自己很大】——攻击成本几乎为零。
//
// 此前 tiff_io.cpp 按文件头的 宽×高×通道 直接 assign 并【填充】，
// 而数据量与声明尺寸的对账要到读条带时才做，那时内存已经分配完毕。
// 一个几 KB、头里声称十万见方的 TIFF 足以把宿主拖进交换。
//
// 判据是【比值】而非绝对上限：PackBits 的理论上限是 64:1，取 128 留一倍余量。
// 刻意不设绝对上限——那需要一个拿不出证据的数字，定小了会打断大幅面生产。

#include "slicer_core/TiffReadApi.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <string>

namespace {

using slicer_core::EnsureDecodedSizeIsPlausible;
using slicer_core::kMaxTiffDecodedExpansion;

std::string CaptureThrow(
    const std::uintmax_t decoded, const std::uintmax_t fileBytes)
{
    try
    {
        EnsureDecodedSizeIsPlausible(decoded, fileBytes, "C:/pkg/layer_000001.tiff");
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
        "tiff decoded size guard (F-52)",
        {
            {"real world ratio passes",
             [] {
                 // 实测真实产物的比值是 1.0–1.2:1，离 128 的上界有约百倍余量。
                 EnsureDecodedSizeIsPlausible(1198638U, 1027747U, "layer.tiff");
             }},
            {"exactly at ceiling passes",
             [] {
                 EnsureDecodedSizeIsPlausible(
                     1024U * kMaxTiffDecodedExpansion, 1024U, "layer.tiff");
             }},
            {"one byte over throws",
             [] {
                 SLICESOFT_EXPECT_FALSE(
                     CaptureThrow(1024U * kMaxTiffDecodedExpansion + 1U, 1024U).empty(),
                     "超出可信膨胀比必须抛出");
             }},
            {"the attack shape is rejected",
             [] {
                 // 4 KB 的文件声称解码出 100000x100000x6 ≈ 60 GB。
                 const std::uintmax_t decoded = 100000ULL * 100000ULL * 6ULL;
                 SLICESOFT_EXPECT_FALSE(
                     CaptureThrow(decoded, 4096U).empty(),
                     "几KB声称六十GB必须被拒");
             }},
            {"message is actionable",
             [] {
                 const std::string what = CaptureThrow(1048576U, 1024U);
                 // 三要素：哪个文件、声称多大、文件实际多大。
                 SLICESOFT_EXPECT_TRUE(
                     what.find("layer_000001.tiff") != std::string::npos, "须点名文件");
                 SLICESOFT_EXPECT_TRUE(
                     what.find("1048576") != std::string::npos, "须给出声明的解码字节数");
                 SLICESOFT_EXPECT_TRUE(
                     what.find("1024") != std::string::npos, "须给出文件实际字节数");
             }},
            {"unknown file size makes no judgment",
             [] {
                 // 0 表示未知。宁可不判，也不要在信息不足时误拒真实资产。
                 EnsureDecodedSizeIsPlausible(
                     8ULL * 1024ULL * 1024ULL * 1024ULL, 0U, "layer.tiff");
             }},
            {"huge file size does not overflow",
             [] {
                 // fileBytes * 128 会溢出的量级：必须安全退出而不是绕回小数导致误拒。
                 // 那种量级的文件另有「一次读入」那道检查兜着。
                 EnsureDecodedSizeIsPlausible(
                     1024U, std::numeric_limits<std::uintmax_t>::max(), "layer.tiff");
             }},
            {"expansion constant is 128",
             [] {
                 // 钉住取值：PackBits 理论上限 64:1，128 是留一倍余量。
                 // 改小会误拒高压缩的合法资产，改大则失去意义——任一方向都该先解释理由。
                 SLICESOFT_EXPECT_EQ(
                     kMaxTiffDecodedExpansion, std::uintmax_t{128}, "可信膨胀比被改动");
             }},
        });
}
