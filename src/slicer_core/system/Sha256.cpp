#include "slicer_core/system/Sha256.h"

#include "slicer_core/system/Sha256Internal.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace slicer_core
{

std::string ComputeSha256(const std::string_view payload)
{
    detail::Sha256Hasher hasher;
    hasher.Update(
        reinterpret_cast<const std::uint8_t*>(payload.data()),
        payload.size());
    const std::array<std::uint8_t, 32> digest = hasher.Finalize();

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest)
    {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

}  // namespace slicer_core
