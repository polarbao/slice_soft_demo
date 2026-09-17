#include "slicer_core/system/BoundedFileRead.h"

#include "slicer_core/system/Utf8Path.h"

#include <ios>
#include <iterator>
#include <stdexcept>

namespace slicer_core
{

void EnsureReadWithinLimit(
    const std::filesystem::path& path,
    const std::uintmax_t actualBytes,
    const std::uintmax_t limitBytes)
{
    if (limitBytes == 0U || actualBytes <= limitBytes)
    {
        return;
    }
    throw std::runtime_error(
        "file is too large to read in process: " + PathToUtf8(path)
        + " is " + std::to_string(actualBytes) + " bytes, limit is "
        + std::to_string(limitBytes)
        + " bytes; this capability parses in the host address space");
}

std::string ReadOpenFileBounded(
    std::ifstream& input,
    const std::filesystem::path& path,
    const std::uintmax_t limitBytes)
{
    // 用流本身量尺寸而不是 filesystem::file_size：量的就是即将读的这个流，
    // 不存在「查的是路径、读的是另一个东西」的缝隙。
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size > 0)
    {
        EnsureReadWithinLimit(path, static_cast<std::uintmax_t>(size), limitBytes);
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

}  // namespace slicer_core
