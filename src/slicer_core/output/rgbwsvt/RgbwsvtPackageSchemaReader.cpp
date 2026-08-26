#include "slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h"

#include "slicer_core/json_value.h"
#include "slicer_core/rip_reader.h"

#include <exception>
#include <fstream>
#include <string>

namespace slicer_core
{

std::string ReadPackageManifestSchema(
    const std::filesystem::path& packageDirectory)
{
    const std::filesystem::path manifestPath = packageDirectory / "manifest.json";
    std::ifstream input{manifestPath, std::ios::binary};
    if (!input)
    {
        throw ValidationError(
            ValidationErrorCode::ManifestMissing,
            "manifest is missing: " + manifestPath.generic_string());
    }

    Json manifest;
    try
    {
        manifest = Json::parse(input);
    }
    catch (const std::exception& error)
    {
        throw ValidationError(
            ValidationErrorCode::ManifestParseFailed,
            "manifest parse failed: " + std::string{error.what()});
    }
    if (!manifest.is_object() || !manifest.contains("schema")
        || !manifest.at("schema").is_string())
    {
        throw ValidationError(
            ValidationErrorCode::SchemaUnsupported,
            "manifest schema is missing");
    }

    const std::string schema = manifest.at("schema").as_string();
    if (schema != "p0.rgbwsv.2" && schema != "p0.rgbwsvt.1")
    {
        throw ValidationError(
            ValidationErrorCode::SchemaUnsupported,
            "unsupported package schema: " + schema);
    }
    return schema;
}

}  // namespace slicer_core
