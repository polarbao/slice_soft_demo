#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/output/tiff/TiffRowLayout.h"

namespace slicer_core
{
inline TiffRowOrder ReadManifestTiffRowOrder(const Json& tiff)
{
    return tiff.contains("rowOrder")
        ? ParseTiffRowOrder(tiff.at("rowOrder").as_string()) : TiffRowOrder::MinYFirst;
}

inline void ValidateTiffRowOrder(const Json& tiff, const TiffImageSpec& spec)
{
    if (ReadManifestTiffRowOrder(tiff) != spec.row_order)
        throw std::runtime_error("TIFF rowOrder does not match manifest.tiff.rowOrder");
}
} // namespace slicer_core
