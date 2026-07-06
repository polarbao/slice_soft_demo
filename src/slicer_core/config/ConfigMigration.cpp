#include "slicer_core/config/ConfigMigration.h"

#include "slicer_core/config/ConfigSchema.h"

namespace slicer_core
{
namespace
{

void CopyIfPresent(Json::Object& target, const Json& source, const std::string& sourceKey, const std::string& targetKey)
{
    if (source.contains(sourceKey))
    {
        target.emplace(targetKey, source.at(sourceKey));
    }
}

Json NormalizeSlicerConfig1(const Json& root)
{
    Json::Object target;

    CopyIfPresent(target, root, "input", "input");
    CopyIfPresent(target, root, "output", "output");
    CopyIfPresent(target, root, "support", "support");
    CopyIfPresent(target, root, "preview", "preview");
    CopyIfPresent(target, root, "texture", "texture");
    CopyIfPresent(target, root, "outerVarnish", "outerVarnish");

    if (root.contains("pipeline"))
    {
        CopyIfPresent(target, root.at("pipeline"), "slicingMode", "slicingMode");
    }

    if (root.contains("geometry"))
    {
        const Json& geometry = root.at("geometry");
        CopyIfPresent(target, geometry, "modelTransform", "modelTransform");
        CopyIfPresent(target, geometry, "autoOrient", "autoOrient");
        CopyIfPresent(target, geometry, "relief", "relief");
    }

    if (root.contains("materials"))
    {
        const Json& materials = root.at("materials");
        CopyIfPresent(target, materials, "modelMaterial", "modelMaterial");
        CopyIfPresent(target, materials, "texture", "texture");
        CopyIfPresent(target, materials, "modelFill", "modelFill");
        CopyIfPresent(target, materials, "outerVarnish", "outerVarnish");
        CopyIfPresent(target, materials, "roleMapping", "materialRoleMapping");
        CopyIfPresent(target, materials, "materialPolicy", "materialPolicy");
        CopyIfPresent(target, materials, "materialProcessProfile", "materialProcessProfile");
    }

    return Json{std::move(target)};
}

}  // namespace

NormalizedConfig NormalizeConfigRoot(const Json& root)
{
    const ConfigSchemaKind schemaKind = DetectConfigSchemaKind(root);
    if (schemaKind == ConfigSchemaKind::Legacy)
    {
        return NormalizedConfig{{}, root};
    }

    return NormalizedConfig{SlicerConfig1SchemaName(), NormalizeSlicerConfig1(root)};
}

Json NormalizeConfigJson(const Json& root)
{
    return NormalizeConfigRoot(root).legacy_root;
}

}  // namespace slicer_core
