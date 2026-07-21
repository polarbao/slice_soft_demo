#include "slicer_core/preflight/ModelPreflightCacheIdentity.h"

#include "slicer_core/geometry/repair/MeshRepairHash.h"

#include <string>
#include <string_view>

namespace slicer_core
{
namespace
{

void AppendCanonicalField(
    std::string& payload,
    const std::string_view name,
    const std::string_view value)
{
    payload.append(name);
    payload.push_back(':');
    payload.append(std::to_string(value.size()));
    payload.push_back(':');
    payload.append(value);
    payload.push_back('\n');
}

}  // namespace

std::string ComputeModelPreflightCacheKey(
    const ModelPreflightCacheIdentity& identity)
{
    std::string payload{"slicesoft.model_preflight.cache.1\n"};
    AppendCanonicalField(payload, "sourceHash", identity.sourceHash);
    AppendCanonicalField(payload, "resourceHash", identity.resourceHash);
    AppendCanonicalField(payload, "transformHash", identity.transformHash);
    AppendCanonicalField(payload, "optionsHash", identity.optionsHash);
    AppendCanonicalField(payload, "algorithmVersion", identity.algorithmVersion);
    return ComputeMeshRepairSha256(payload);
}

}  // namespace slicer_core
