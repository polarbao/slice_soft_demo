#pragma once

#ifdef _WIN32
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{
// wmain avoids the CRT's lossy ACP conversion before UTF-8 request processing.
// Native u8string (not generic_u8string) preserves argument separators verbatim.
inline int RunUtf8Main(int argc, wchar_t** argv, int (*entry)(int, char**))
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        const auto bytes = std::filesystem::path(argv[i]).u8string();
        arguments.emplace_back(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    std::vector<char*> pointers;
    pointers.reserve(arguments.size() + 1);
    for (auto& argument : arguments) pointers.push_back(argument.data());
    pointers.push_back(nullptr);
    return entry(argc, pointers.data());
}
} // namespace slicer_core
#endif
