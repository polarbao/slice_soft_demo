#include "slicer_core/config/ConfigSchema.h"

#include <stdexcept>

namespace slicer_core
{

std::string ReadConfigSchema(const Json& root)
{
    if (!root.contains("schema"))
    {
        return {};
    }
    return root.at("schema").as_string();
}

ConfigSchemaKind DetectConfigSchemaKind(const Json& root)
{
    const std::string schema = ReadConfigSchema(root);
    if (schema.empty())
    {
        return ConfigSchemaKind::Legacy;
    }
    if (schema == SlicerConfig1SchemaName())
    {
        return ConfigSchemaKind::SlicerConfig1;
    }
    throw std::runtime_error("unsupported slicer config schema: " + schema);
}

std::string SlicerConfig1SchemaName()
{
    return "slicer.config.1";
}

}  // namespace slicer_core
