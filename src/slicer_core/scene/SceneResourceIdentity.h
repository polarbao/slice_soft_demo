#pragma once

#include "slicer_core/scene/SceneModel.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Compute a stable adjacent-resource identity for a scene model.
 *
 * 3MF internal textures are identified by their logical cache file name and
 * content, not by the transient extraction directory used by one import run.
 *
 * @param model Imported scene model and resolved material resources.
 * @return SHA-256 identity for material and texture resources.
 */
std::string ComputeSceneResourceHash(const SceneModel& model);

}  // namespace slicer_core
