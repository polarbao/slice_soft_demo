#include "slicer_core/output/rgbwsv/RgbwsvCapabilitySummary.h"

#include <stdexcept>

namespace slicer_core
{

void ValidateRgbwsvCapabilitySummary(
    const RgbwsvProductionPackageWriteRequest& request)
{
    if (request.perinstance.has_value()
        != request.profileecho.has_value())
    {
        throw std::invalid_argument(
            "RGBWSV package capability summary fields must be provided together");
    }
    if (!request.perinstance.has_value())
    {
        return;
    }
    const Json& perInstance = *request.perinstance;
    const Json& profileEcho = *request.profileecho;
    if (!perInstance.is_array()
        || !profileEcho.is_object()
        || !profileEcho.contains("profileVersion")
        || !profileEcho.at("profileVersion").is_string()
        || profileEcho.at("profileVersion").as_string().empty()
        || !profileEcho.contains("profileHash")
        || !profileEcho.at("profileHash").is_string()
        || profileEcho.at("profileHash").as_string().empty())
    {
        throw std::invalid_argument(
            "RGBWSV package capability summary is incomplete");
    }
}

void AppendRgbwsvCapabilitySummary(
    Json::Object& manifestObject,
    const RgbwsvProductionPackageWriteRequest& request)
{
    if (!request.perinstance.has_value())
    {
        return;
    }
    manifestObject["perInstance"] = *request.perinstance;
    manifestObject["profileEcho"] = *request.profileecho;
}

}  // namespace slicer_core
