#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"
#include "slicer_core/api/SceneViewDtos.h"

#include <memory>

namespace slicesoft::module
{

/** @brief Serializes the frozen ViewData v1.2 DTO and owns bounded blob storage. */
class SceneViewDataAdapter final
{
public:
    /** @brief Creates an empty bounded ViewData blob store. */
    SceneViewDataAdapter();

    /** @brief Releases all retained ViewData blobs. */
    ~SceneViewDataAdapter();

    /** @brief Serializes provider output without another DTO. @param data Provider result. @return v1.2 envelope. */
    [[nodiscard]] slicer_core::Json Serialize(
        const slicer_core::api::SceneViewData& data);

    /** @brief Reads one frozen ViewData blob chunk. @param request Read request. @return Binary output. */
    [[nodiscard]] CapabilityOutput ReadBlob(const slicer_core::Json& request);

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
