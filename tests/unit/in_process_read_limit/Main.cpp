// P0FIX / F-35 第一层：进程内整文件读取的尺寸上限。
//
// 与 model_import_size_limit（F-34）是姊妹用例。F-34 挡住了 load_model_report，
// 但 ModelFacadeImplementation 读【整个模型文件】算 SHA-256 的那条路径绕开了它——
// 等于闸门旁边留了一扇没锁的门。本闸门堵的就是那扇门。
//
// 其中最要紧的一条断言是「两处上限必须同值」：它们读的是同一批文件，
// 取值一旦分叉，就又回到「一扇锁了一扇没锁」的状态，而没有任何东西会报错。

#include "slicer_core/model.h"
#include "slicer_core/system/BoundedFileRead.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using slicer_core::EnsureReadWithinLimit;
using slicer_core::ReadOpenFileBounded;
using slicer_core::kMaxInProcessReadBytes;
using slicer_core::kMaxModelFileBytes;

constexpr std::uintmax_t kOneMiB{1024ULL * 1024ULL};

std::string CaptureThrow(const std::uintmax_t actual, const std::uintmax_t limit)
{
    try
    {
        EnsureReadWithinLimit("C:/pkg/manifest.json", actual, limit);
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return {};
}

/// 在临时目录写一个指定大小的文件，返回其路径；调用方负责删除。
std::filesystem::path WriteTempFile(const std::string& name, const std::size_t bytes)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::ofstream output{path, std::ios::binary};
    output << std::string(bytes, 'x');
    output.close();
    return path;
}

}  // namespace

int main()
{
    return slicesoft_test::RunCases(
        "in-process read limit (F-35)",
        {
            {"under limit passes",
             [] { EnsureReadWithinLimit("m.json", kOneMiB, 2U * kOneMiB); }},
            {"exactly at limit passes",
             [] { EnsureReadWithinLimit("m.json", 2U * kOneMiB, 2U * kOneMiB); }},
            {"one byte over throws",
             [] {
                 SLICESOFT_EXPECT_FALSE(
                     CaptureThrow(2U * kOneMiB + 1U, 2U * kOneMiB).empty(),
                     "超限必须抛出");
             }},
            {"message is actionable",
             [] {
                 const std::string what = CaptureThrow(3U * kOneMiB, kOneMiB);
                 SLICESOFT_EXPECT_TRUE(
                     what.find("manifest.json") != std::string::npos, "须点名文件");
                 SLICESOFT_EXPECT_TRUE(
                     what.find(std::to_string(3U * kOneMiB)) != std::string::npos,
                     "须给出实际字节数");
                 SLICESOFT_EXPECT_TRUE(
                     what.find(std::to_string(kOneMiB)) != std::string::npos,
                     "须给出上限");
             }},
            {"zero limit means unlimited",
             [] { EnsureReadWithinLimit("m.json", 8ULL * 1024ULL * kOneMiB, 0U); }},
            {"两处上限必须同值",
             [] {
                 // 模型导入闸门与进程内读取闸门读的是同一批文件。
                 // 取值一旦分叉就又回到「一扇锁了一扇没锁」，而没有任何东西会报错，
                 // 所以把它钉成断言：改任一处都要先撞红，即先被迫解释理由。
                 SLICESOFT_EXPECT_EQ(
                     kMaxInProcessReadBytes, kMaxModelFileBytes,
                     "进程内读取上限与模型导入上限已分叉");
             }},
            {"bounded read rejects an oversized stream",
             [] {
                 // 这条走【真实的流】，证明闸门确实长在读取路径上，
                 // 而不只是一个没人调用的纯函数。
                 const auto path = WriteTempFile("slicesoft_f35_probe.bin", 4096U);
                 std::ifstream input{path, std::ios::binary};
                 SLICESOFT_EXPECT_TRUE(input.is_open(), "探针文件应能打开");
                 std::string thrown;
                 try
                 {
                     ReadOpenFileBounded(input, path, 1024U);
                 }
                 catch (const std::exception& error)
                 {
                     thrown = error.what();
                 }
                 input.close();
                 std::filesystem::remove(path);
                 SLICESOFT_EXPECT_FALSE(thrown.empty(), "4096 字节超过 1024 上限，须抛出");
             }},
            {"bounded read returns full content within limit",
             [] {
                 const auto path = WriteTempFile("slicesoft_f35_ok.bin", 4096U);
                 std::ifstream input{path, std::ios::binary};
                 const std::string content =
                     ReadOpenFileBounded(input, path, kOneMiB);
                 input.close();
                 std::filesystem::remove(path);
                 // 加界不能改变正常路径的读取结果——这是本改动的「不改行为」判据。
                 SLICESOFT_EXPECT_EQ(content.size(), std::size_t{4096}, "内容须完整读回");
             }},
        });
}
