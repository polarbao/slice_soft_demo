#pragma once

#include "slicer_core/scene/SceneViewGeometry.h"

#include <QString>

#include <cstdint>

/**
 * @brief Immutable request for one cached-source scene reprojection.
 */
struct SceneProjectionRequest
{
    QString cachekey;
    QString sceneid;
    std::uint64_t scenerevision{0U};
    slicer_core::ModelInstance instance;
    slicer_core::SceneViewAdmissionStatus admissionstatus{
        slicer_core::SceneViewAdmissionStatus::Unknown};
};
