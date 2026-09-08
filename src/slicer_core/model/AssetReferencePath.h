#pragma once

#include "slicer_core/system/Utf8Path.h"

namespace slicer_core::model_detail
{

// OBJ/MTL predates UTF-8. Preserve native ANSI references only when the bytes
// are invalid UTF-8; valid UTF-8 must never silently select a different asset.
// This compatibility rule is NOT permitted at the SPI/JSON boundary.
inline std::filesystem::path AssetReferencePath(const std::string_view text)
{
#ifdef _WIN32
    try
    {
        return PathFromUtf8(text);
    }
    catch (const std::system_error&)
    {
        return std::filesystem::path(std::string{text});
    }
#else
    return PathFromUtf8(text);
#endif
}

} // namespace slicer_core::model_detail
