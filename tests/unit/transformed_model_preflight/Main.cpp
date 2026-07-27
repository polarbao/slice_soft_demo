#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/preflight/TransformedModelPreflight.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

slicer_core::SceneModel LoadClosedCube()
{
    slicer_core::SliceConfig config;
    config.input.model_path =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/models/openvdb/surface_shell_cube.obj";
    config.input.format = "obj";
    config.auto_orient.enabled = false;
    return slicer_core::load_model_report(
        config,
        std::filesystem::path(SLICESOFT_SOURCE_DIR));
}

slicer_core::SceneModel MakeOpenTriangle()
{
    slicer_core::SceneModel source;
    source.model_path = "open-triangle.obj";
    source.format = "obj";
    source.triangles = {
        {
            {0.0, 0.0, 0.0},
            {10.0, 0.0, 0.0},
            {0.0, 10.0, 0.0},
        },
    };
    source.triangle_count = 1U;
    source.bbox_mm.min = {0.0, 0.0, 0.0};
    source.bbox_mm.max = {10.0, 10.0, 0.0};
    return source;
}

slicer_core::ModelInstance MakeInstance(
    const slicer_core::SceneModel& source)
{
    slicer_core::ModelInstance instance;
    instance.instanceid = "instance-1";
    instance.modelid = "model-1";
    instance.sourcetransformidentity = "source-transform-1";
    instance.sourcebboxmm = source.bbox_mm;
    instance.effectivebboxmm = source.bbox_mm;
    return instance;
}

slicer_core::TransformedModelPreflightRequest MakeRequest(
    const slicer_core::SceneModel& source,
    const slicer_core::ModelInstance& instance)
{
    slicer_core::TransformedModelPreflightRequest request;
    request.source = &source;
    request.instance = instance;
    request.sourcehash = "source-hash";
    request.resourcehash = "resource-hash";
    request.sceneid = "scene-1";
    request.scenerevision = 1U;
    request.expectedscenerevision = 1U;
    request.expectedtransformrevision =
        instance.transformrevision;
    request.generation = 7U;
    request.admissioncontext.global_backend_available = true;
    return request;
}

bool MirrorPreflightPreservesSourceAndBindsIdentity()
{
    const slicer_core::SceneModel source = LoadClosedCube();
    const slicer_core::Triangle sourceFirst =
        source.triangles.front();
    slicer_core::ModelInstance instance = MakeInstance(source);
    instance.transform.mirrorx = true;
    instance.transformrevision = 1U;

    slicer_core::TransformedModelPreflightService service;
    const slicer_core::TransformedModelPreflightExecution result =
        service.Run(MakeRequest(source, instance));

    return ExpectTrue(result.IsValid(), "mirror preflight completes")
        && ExpectTrue(
            result.generation == 7U
                && result.sceneid == "scene-1"
                && result.scenerevision == 1U
                && result.transformrevision == 1U,
            "execution keeps scene and transform identity")
        && ExpectTrue(
            result.source.result.status
                    == slicer_core::ModelPreflightStatus::Passed
                && result.transformed.result.status
                    == slicer_core::ModelPreflightStatus::Passed,
            "closed source and mirrored geometry pass diagnostics")
        && ExpectTrue(
            result.transformed.result.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Passed,
            "available global backend admits strict closed mirror")
        && ExpectTrue(
            result.source.result.identity.transformHash
                != result.transformed.result.identity.transformHash,
            "source and transformed identities remain distinct")
        && ExpectTrue(
            source.triangles.front().a.x == sourceFirst.a.x
                && source.triangles.front().b.y == sourceFirst.b.y
                && source.triangles.front().c.z == sourceFirst.c.z,
            "preflight does not mutate source geometry");
}

bool TopologyAdmissionRemainsModeSpecific()
{
    const slicer_core::SceneModel source = MakeOpenTriangle();
    const slicer_core::ModelInstance instance = MakeInstance(source);
    slicer_core::TransformedModelPreflightService service;
    const slicer_core::TransformedModelPreflightExecution result =
        service.Run(MakeRequest(source, instance));

    return ExpectTrue(result.IsValid(), "open mesh audit completes")
        && ExpectTrue(
            result.transformed.result.legacyAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Warning,
            "legacy preserves topology warning")
        && ExpectTrue(
            result.transformed.result.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "global blocks open topology")
        && ExpectTrue(
            !result.transformed.result.globalAdmission.blockerCodes.empty(),
            "global blocker remains machine readable");
}

bool CancelAndStaleFailClosed()
{
    const slicer_core::SceneModel source = LoadClosedCube();
    const slicer_core::ModelInstance instance = MakeInstance(source);
    slicer_core::TransformedModelPreflightService service;

    auto cancelledRequest = MakeRequest(source, instance);
    cancelledRequest.cancellationrequested = []()
    {
        return true;
    };
    const auto cancelled = service.Run(cancelledRequest);

    auto staleRequest = MakeRequest(source, instance);
    staleRequest.expectedscenerevision = 0U;
    const auto stale = service.Run(staleRequest);

    return ExpectTrue(
               cancelled.cancelled
                   && cancelled.transformed.result.status
                       == slicer_core::ModelPreflightStatus::Cancelled,
               "cancelled request is not admitted")
        && ExpectTrue(
            cancelled.transformed.result.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "cancelled global admission is blocked")
        && ExpectTrue(
            stale.stale
                && stale.transformed.result.status
                    == slicer_core::ModelPreflightStatus::Stale,
            "stale revisions fail closed");
}

bool MissingTextureResourceBlocksBothModes()
{
    slicer_core::SceneModel source = LoadClosedCube();
    slicer_core::MaterialInfo material;
    material.name = "missing-texture";
    material.has_texture = true;
    material.texture_exists = false;
    source.material_infos.push_back(material);
    const slicer_core::ModelInstance instance = MakeInstance(source);
    slicer_core::TransformedModelPreflightService service;
    const auto result = service.Run(MakeRequest(source, instance));

    return ExpectTrue(
               result.source.result.status
                   == slicer_core::ModelPreflightStatus::Blocked,
               "missing source texture blocks preflight")
        && ExpectTrue(
            result.transformed.result.legacyAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "missing texture blocks legacy")
        && ExpectTrue(
            result.transformed.result.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "missing texture blocks global");
}

}  // namespace

int main()
{
    bool ok = true;
    ok = MirrorPreflightPreservesSourceAndBindsIdentity() && ok;
    ok = TopologyAdmissionRemainsModeSpecific() && ok;
    ok = CancelAndStaleFailClosed() && ok;
    ok = MissingTextureResourceBlocksBothModes() && ok;
    if (!ok)
    {
        return 1;
    }
    std::cout << "transformed_model_preflight_unit_tests: PASS\n";
    return 0;
}
