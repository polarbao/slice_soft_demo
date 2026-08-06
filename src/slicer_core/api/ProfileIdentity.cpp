#include "slicer_core/api/ProfileIdentity.h"

#include "slicer_core/system/Sha256.h"

#include <stdexcept>
#include <utility>

namespace slicer_core::api
{

std::string ComputeProfileDocumentHash(const Json& profile)
{
    if (!profile.is_object())
    {
        throw std::invalid_argument("Profile must be a JSON object");
    }
    Json::Object hashInput = profile.as_object();
    hashInput.erase("profileHash");
    return "sha256:" + ComputeSha256(Json(std::move(hashInput)).dump(0));
}

}  // namespace slicer_core::api
