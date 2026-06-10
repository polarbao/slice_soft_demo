#include "slicer_core/importers/mtl/MtlImporter.h"

namespace slicer_core
{

MtlImportSummary MakeLegacyMtlSummary(const std::filesystem::path& sourcePath,
                                      const std::vector<MaterialInfo>& materials)
{
    MtlImportSummary summary;
    summary.source_path = sourcePath;
    summary.materials = materials;
    return summary;
}

}  // namespace slicer_core
