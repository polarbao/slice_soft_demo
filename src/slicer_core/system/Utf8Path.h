#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace slicer_core
{

// JSON/SPI paths are UTF-8, never the process ANSI code page.
inline std::filesystem::path PathFromUtf8(const std::string_view text)
{
    if (text.find('\0') != std::string_view::npos)
        throw std::invalid_argument("UTF-8 path contains an embedded NUL");
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

inline std::string PathToUtf8(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

// Append ASCII staging suffixes without narrowing an existing native path.
inline std::filesystem::path PathWithSuffix(
    std::filesystem::path path, const std::string_view suffix)
{
    path += PathFromUtf8(suffix);
    return path;
}

} // namespace slicer_core
