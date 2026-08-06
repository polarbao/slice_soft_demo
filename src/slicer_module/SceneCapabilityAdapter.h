#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"

#include <memory>
#include <string>

namespace slicesoft::module
{

class ModelCapabilityAdapter;

/** @brief Wires scene, collision, and ViewData capabilities to facades. */
class SceneCapabilityAdapter final
{
public:
    /** @brief Binds scene routing to imported models. @param models Model adapter. */
    explicit SceneCapabilityAdapter(ModelCapabilityAdapter& models);

    /** @brief Releases scene sessions and ViewData blobs. */
    ~SceneCapabilityAdapter();

    /** @brief Executes one scene-side capability. @param capability Frozen ID. @param request DTO object. @return Terminal output. */
    [[nodiscard]] CapabilityOutput Execute(
        const std::string& capability,
        const slicer_core::Json& request);

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
