#include "slicer_core/preflight/SceneFullPreflightService.h"

#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace slicer_core
{
namespace
{

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read model source: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool HasAbsoluteResourceIdentity(
    const MultiModelScene& scene,
    const ModelSource& source)
{
    const auto found = std::find_if(
        scene.resourcescopes.begin(),
        scene.resourcescopes.end(),
        [&source](const ResourceScope& scope)
        {
            return scope.resourcescopeid == source.resourcescopeid;
        });
    if (!source.sourcepath.is_absolute()
        || found == scene.resourcescopes.end()
        || !found->rootpath.is_absolute())
    {
        return false;
    }
    return found->kind != ResourceScopeKind::ThreeMfPackage
        || found->packagepath.is_absolute();
}

}  // namespace

SceneFullPreflightResolvedModel SceneFullPreflightService::ResolveModel(
    const SceneFullPreflightRequest& request,
    const ModelSource& source)
{
    try
    {
        if (!HasAbsoluteResourceIdentity(*request.scene, source)
            || !std::filesystem::is_regular_file(source.sourcepath)
            || ComputeSha256(ReadFile(source.sourcepath))
                != source.sourcehash)
        {
            return {{}, SceneFullPreflightResolutionErrorCode::ResourceMissing,
                "model source identity is unavailable or changed"};
        }
        SceneFullPreflightResolvedModel resolved =
            request.modelresolver(source);
        if (!resolved.IsValid() || resolved.model->triangles.empty())
        {
            if (resolved.errorcode
                == SceneFullPreflightResolutionErrorCode::None)
            {
                resolved.errorcode =
                    SceneFullPreflightResolutionErrorCode::ImportInvalid;
            }
            if (resolved.detail.empty())
            {
                resolved.detail = "model resolver returned no geometry";
            }
            return resolved;
        }
        if (ComputeSceneResourceHash(*resolved.model)
            != source.resourcehash)
        {
            return {{}, SceneFullPreflightResolutionErrorCode::ResourceMissing,
                "model adjacent-resource identity changed"};
        }
        return resolved;
    }
    catch (const std::exception& error)
    {
        return {{}, SceneFullPreflightResolutionErrorCode::ImportInvalid,
            error.what()};
    }
}

}  // namespace slicer_core
