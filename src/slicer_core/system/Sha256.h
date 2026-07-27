#pragma once

#include <string>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Compute a lowercase SHA-256 digest for an arbitrary byte string.
 * @param payload Bytes to hash.
 * @return Lowercase 64-character SHA-256 hex string.
 */
std::string ComputeSha256(std::string_view payload);

}  // namespace slicer_core
