#pragma once

// 进程内能力的整文件读取上限（F-35 第一层 / F-51）。
//
// 16 个能力里 12 个是 in_process——解析与分配都发生在【宿主打印软件的地址空间】。
// 此前 ReadFileBytes 在三处各有一份且全部无界；其中模型门面那份读整个模型文件算
// SHA-256，【绕开了 F-34 给 load_model_report 加的闸门】，等于闸门旁边留了一扇没锁的门。
//
// 【刻意不合并那三个函数】它们「打不开」时的行为各不相同且各自都正确
// （抛 runtime_error / 返回空串 / 报 ManifestMissing），合并会改变可见行为。
// 本函数只接管【读取】这一段——那正是三者相同且都缺上限的部分。

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace slicer_core
{

/// 进程内读取的字节上限，与 model.h 的 kMaxModelFileBytes 同值：
/// 模型 SHA-256 路径读的就是同一批文件，两处取值不同只会制造「一扇门锁了一扇没锁」。
inline constexpr std::uintmax_t kMaxInProcessReadBytes{512ULL * 1024ULL * 1024ULL};

/**
 * @brief 读取已打开的文件流，超过上限即抛出。
 * @param input 已打开的流；本函数【不】处理打不开的情形，由调用方自行处置。
 * @param path 仅用于错误信息。
 * @param limitBytes 上限；0 表示不限（供测试构造用）。
 * @return 文件全部内容。
 * @throws std::runtime_error 超限时。
 */
[[nodiscard]] std::string ReadOpenFileBounded(
    std::ifstream& input,
    const std::filesystem::path& path,
    std::uintmax_t limitBytes = kMaxInProcessReadBytes);

/**
 * @brief 尺寸超限即抛出，错误信息给出实际值、上限与文件名。
 * @param path 仅用于错误信息。
 * @param actualBytes 实际字节数。
 * @param limitBytes 上限；0 表示不限。
 * @throws std::runtime_error 超限时。
 */
void EnsureReadWithinLimit(
    const std::filesystem::path& path,
    std::uintmax_t actualBytes,
    std::uintmax_t limitBytes);

}  // namespace slicer_core
