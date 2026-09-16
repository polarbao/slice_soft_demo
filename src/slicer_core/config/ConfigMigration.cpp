#include "slicer_core/config/ConfigMigration.h"

#include "slicer_core/config/ConfigSchema.h"

#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>

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

// F-45：本归一化按白名单逐键搬运，白名单之外的键此前被【静默丢弃】——不报错、不告警。
// 后果有两层：
//   1) 拼错的键等于没写，而作者拿不到任何信号；
//   2) 有些设置经 v1 根本无法表达。background（config.cpp:248）与 geometrySampling
//      （config.cpp:676）都从 legacy root 读取，却都不在白名单里——同一功能
//      legacy 能配、v1 配了等于没配。
// 仓库自己的 v1 样例就踩过：它曾声明 diagnostics、materials.textureApplication、
// materials.varnishGeometry 三块，三块全被丢弃。
//
// 这里把静默丢弃改成报错。**不做**的是把那三个键补进映射——它们当前什么都不做，
// 补进映射会让既有配置突然生效，那是行为变更，须单独裁定，不在本条范围内。
void RejectUnknownKeys(
    const Json& source,
    const std::string& where,
    std::initializer_list<const char*> allowed)
{
    if (!source.is_object())
    {
        return;
    }
    const std::set<std::string> known{allowed.begin(), allowed.end()};
    for (const auto& entry : source.as_object())
    {
        if (known.count(entry.first) == 0U)
        {
            throw std::runtime_error(
                "unsupported key in " + where + ": " + entry.first
                + " (slicer.config.1 silently discarded it before; declare it under a "
                  "supported key or remove it)");
        }
    }
}

Json NormalizeSlicerConfig1(const Json& root)
{
    Json::Object target;

    // 顶层放行集 = 下方逐条搬运的 11 个键 + 判别键 schema（DetectConfigSchemaKind 在
    // 归一化【之前】读它）+ 三个容器键（本身不搬运，各自递归校验）。
    RejectUnknownKeys(root, "slicer.config.1 root", {
        "schema",
        "input", "output", "support", "preview", "texture",
        "outerVarnish", "surfaceVarnish", "materialClosure",
        "materialVolumePolicy", "transferChannelPolicy", "slicePipeline",
        "pipeline", "geometry", "materials"});

    CopyIfPresent(target, root, "input", "input");
    CopyIfPresent(target, root, "output", "output");
    CopyIfPresent(target, root, "support", "support");
    CopyIfPresent(target, root, "preview", "preview");
    CopyIfPresent(target, root, "texture", "texture");
    CopyIfPresent(target, root, "outerVarnish", "outerVarnish");
    CopyIfPresent(target, root, "surfaceVarnish", "surfaceVarnish");
    CopyIfPresent(target, root, "materialClosure", "materialClosure");
    CopyIfPresent(target, root, "materialVolumePolicy", "materialVolumePolicy");
    CopyIfPresent(target, root, "transferChannelPolicy", "transferChannelPolicy");
    CopyIfPresent(target, root, "slicePipeline", "slicePipeline");

    if (root.contains("pipeline"))
    {
        RejectUnknownKeys(
            root.at("pipeline"), "slicer.config.1 pipeline", {"slicingMode"});
        CopyIfPresent(target, root.at("pipeline"), "slicingMode", "slicingMode");
    }

    if (root.contains("geometry"))
    {
        const Json& geometry = root.at("geometry");
        RejectUnknownKeys(geometry, "slicer.config.1 geometry", {
            "modelTransform", "autoOrient", "relief"});
        CopyIfPresent(target, geometry, "modelTransform", "modelTransform");
        CopyIfPresent(target, geometry, "autoOrient", "autoOrient");
        CopyIfPresent(target, geometry, "relief", "relief");
    }

    if (root.contains("materials"))
    {
        const Json& materials = root.at("materials");
        RejectUnknownKeys(materials, "slicer.config.1 materials", {
            "modelMaterial", "texture", "modelFill", "outerVarnish",
            "surfaceVarnish", "roleMapping", "materialPolicy",
            "materialProcessProfile"});
        CopyIfPresent(target, materials, "modelMaterial", "modelMaterial");
        CopyIfPresent(target, materials, "texture", "texture");
        CopyIfPresent(target, materials, "modelFill", "modelFill");
        CopyIfPresent(target, materials, "outerVarnish", "outerVarnish");
        CopyIfPresent(target, materials, "surfaceVarnish", "surfaceVarnish");
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
