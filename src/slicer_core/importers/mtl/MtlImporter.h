#pragma once

#include "slicer_core/scene/SceneModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Summary for material data exposed by the MTL importer boundary.
 */
struct MtlImportSummary
{
    std::filesystem::path source_path;
    std::vector<MaterialInfo> materials;
    std::vector<std::string> warnings;
};

/**
 * @brief Build a legacy MTL summary from already parsed material data.
 * @param sourcePath Source MTL path.
 * @param materials Parsed material list.
 * @return Material import summary.
 */
MtlImportSummary MakeLegacyMtlSummary(const std::filesystem::path& sourcePath,
                                      const std::vector<MaterialInfo>& materials);

}  // namespace slicer_core
