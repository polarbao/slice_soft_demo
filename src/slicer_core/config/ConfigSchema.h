#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Supported slice configuration schema kinds.
 */
enum class ConfigSchemaKind
{
    Legacy,
    SlicerConfig1,
};

/**
 * @brief Read the config schema string from a JSON root object.
 * @param root Config JSON root.
 * @return Schema string, or an empty string for legacy config.
 */
std::string ReadConfigSchema(const Json& root);

/**
 * @brief Detect the supported schema kind for a config JSON root.
 * @param root Config JSON root.
 * @return Supported schema kind.
 */
ConfigSchemaKind DetectConfigSchemaKind(const Json& root);

/**
 * @brief Return the canonical R2 schema name.
 * @return The string slicer.config.1.
 */
std::string SlicerConfig1SchemaName();

}  // namespace slicer_core
