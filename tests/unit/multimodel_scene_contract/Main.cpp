#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <cmath>
#include <filesystem>
#include <fstream>
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

slicer_core::ModelInstance MakeInstance(
    const std::string& instanceId,
    const std::string& modelId)
{
    slicer_core::ModelInstance instance;
    instance.instanceid = instanceId;
    instance.modelid = modelId;
    instance.sourcetransformidentity = "source-transform-" + modelId;
    instance.sourcebboxmm.min = {0.0, 0.0, 0.0};
    instance.sourcebboxmm.max = {10.0, 20.0, 2.0};
    instance.effectivebboxmm = instance.sourcebboxmm;
    return instance;
}

slicer_core::MultiModelScene MakeDraftScene()
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-fixture";
    scene.scenerevision = 3U;
    scene.materialbindingmode =
        slicer_core::SceneMaterialBindingMode::SceneProfileOnly;
    scene.resolvedprofileid = "profile-a";

    slicer_core::ResourceScope objScope;
    objScope.resourcescopeid = "scope-obj";
    objScope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    objScope.rootpath = "models/a";
    scene.resourcescopes.push_back(objScope);

    slicer_core::ResourceScope threeMfScope;
    threeMfScope.resourcescopeid = "scope-3mf";
    threeMfScope.kind = slicer_core::ResourceScopeKind::ThreeMfPackage;
    threeMfScope.rootpath = "models/b/model.3mf";
    threeMfScope.packagepath = "models/b/model.3mf";
    threeMfScope.partidentity = "/3D/3dmodel.model";
    scene.resourcescopes.push_back(threeMfScope);

    slicer_core::ModelSource objSource;
    objSource.modelid = "model-a";
    objSource.sourcepath = "models/a/model.obj";
    objSource.format = "obj";
    objSource.resourcescopeid = "scope-obj";
    objSource.sourcehash = "source-hash-a";
    objSource.resourcehash = "resource-hash-a";
    objSource.displayname = "模型 A";
    scene.models.push_back(objSource);

    slicer_core::ModelSource threeMfSource;
    threeMfSource.modelid = "model-b";
    threeMfSource.sourcepath = "models/b/model.3mf";
    threeMfSource.format = "3mf";
    threeMfSource.resourcescopeid = "scope-3mf";
    threeMfSource.sourcehash = "source-hash-b";
    threeMfSource.resourcehash = "resource-hash-b";
    threeMfSource.displayname = "模型 B";
    scene.models.push_back(threeMfSource);

    slicer_core::SceneModelInstance first;
    first.instance = MakeInstance("instance-a", "model-a");
    first.requestedtransform.translatexmm = 1.5;
    first.effectivetransform = first.requestedtransform;
    first.instance.transform = first.effectivetransform;
    first.resolvedprofileid = "profile-a";
    scene.instances.push_back(first);

    slicer_core::SceneModelInstance second;
    second.instance = MakeInstance("instance-b", "model-b");
    second.derivedlayouttransform.translateymm = 25.0;
    second.effectivetransform = second.derivedlayouttransform;
    second.instance.transform = second.effectivetransform;
    second.resolvedprofileid = "profile-a";
    scene.instances.push_back(second);
    return scene;
}

bool StableErrorNames()
{
    using slicer_core::SceneValidationErrorCode;
    const struct
    {
        SceneValidationErrorCode code;
        std::string expected;
    } cases[]{
        {SceneValidationErrorCode::None, "NONE"},
        {SceneValidationErrorCode::SchemaUnsupported,
         "SCENE_SCHEMA_UNSUPPORTED"},
        {SceneValidationErrorCode::SceneIdEmpty, "SCENE_ID_EMPTY"},
        {SceneValidationErrorCode::SceneRevisionStale,
         "SCENE_REVISION_STALE"},
        {SceneValidationErrorCode::SceneRevisionInvalid,
         "SCENE_REVISION_INVALID"},
        {SceneValidationErrorCode::ModelIdDuplicate, "MODEL_ID_DUPLICATE"},
        {SceneValidationErrorCode::InstanceIdDuplicate,
         "INSTANCE_ID_DUPLICATE"},
        {SceneValidationErrorCode::InstanceModelReferenceMissing,
         "INSTANCE_MODEL_REFERENCE_MISSING"},
        {SceneValidationErrorCode::ResourceScopeEscape,
         "RESOURCE_SCOPE_ESCAPE"},
        {SceneValidationErrorCode::BuildVolumeUndefined,
         "BUILD_VOLUME_UNDEFINED"},
        {SceneValidationErrorCode::SceneProfileMismatch,
         "SCENE_PROFILE_MISMATCH"},
        {SceneValidationErrorCode::EffectiveConfigWriteFailed,
         "SCENE_EFFECTIVE_CONFIG_WRITE_FAILED"},
    };

    bool ok{true};
    for (const auto& item : cases)
    {
        ok = ExpectTrue(
                 slicer_core::SceneValidationErrorCodeName(item.code)
                     == item.expected,
                 "scene error name follows stable contract")
            && ok;
    }
    return ok;
}

bool DraftAndProductionValidation()
{
    const slicer_core::MultiModelScene scene = MakeDraftScene();
    const auto draft = slicer_core::ValidateMultiModelScene(
        scene,
        slicer_core::SceneValidationPurpose::Draft);
    const auto production = slicer_core::ValidateMultiModelScene(
        scene,
        slicer_core::SceneValidationPurpose::Production);

    slicer_core::MultiModelScene fixture = scene;
    fixture.buildvolume.source =
        slicer_core::BuildVolumeSource::Fixture;
    fixture.buildvolume.widthmm = 300.0;
    fixture.buildvolume.heightmm = 100.0;
    fixture.buildvolume.origin =
        slicer_core::BuildVolumeOrigin::LowerLeft;
    fixture.buildvolume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    fixture.buildvolume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    fixture.buildvolume.isfixture = true;
    const auto fixtureFunctional = slicer_core::ValidateMultiModelScene(
        fixture,
        slicer_core::SceneValidationPurpose::FunctionalFixture);
    const auto fixtureProduction = slicer_core::ValidateMultiModelScene(
        fixture,
        slicer_core::SceneValidationPurpose::Production);
    slicer_core::MultiModelScene device = fixture;
    device.buildvolume.source =
        slicer_core::BuildVolumeSource::DeviceProfile;
    device.buildvolume.isfixture = false;
    const auto deviceProduction = slicer_core::ValidateMultiModelScene(
        device,
        slicer_core::SceneValidationPurpose::Production);

    return ExpectTrue(draft.IsValid(), "unresolved build volume permits draft")
        && ExpectTrue(
            !production.IsValid()
                && production.errors.front().code
                    == slicer_core::SceneValidationErrorCode::
                        BuildVolumeUndefined,
            "unresolved build volume blocks production")
        && ExpectTrue(
            fixtureFunctional.IsValid(),
            "explicit fixture build volume passes functional validation")
        && ExpectTrue(
            !fixtureProduction.IsValid(),
            "fixture build volume cannot claim device production readiness")
        && ExpectTrue(
            deviceProduction.IsValid(),
            "device Profile build volume passes production validation");
}

bool IdentityAndResourceValidation()
{
    bool ok{true};

    slicer_core::MultiModelScene duplicateModel = MakeDraftScene();
    duplicateModel.models.push_back(duplicateModel.models.front());
    ok = ExpectTrue(
             slicer_core::ValidateMultiModelScene(
                 duplicateModel,
                 slicer_core::SceneValidationPurpose::Draft)
                     .errors.front()
                     .code
                 == slicer_core::SceneValidationErrorCode::ModelIdDuplicate,
             "duplicate model id is rejected")
        && ok;

    slicer_core::MultiModelScene duplicateInstance = MakeDraftScene();
    duplicateInstance.instances.push_back(
        duplicateInstance.instances.front());
    ok = ExpectTrue(
             slicer_core::ValidateMultiModelScene(
                 duplicateInstance,
                 slicer_core::SceneValidationPurpose::Draft)
                     .errors.front()
                     .code
                 == slicer_core::SceneValidationErrorCode::
                     InstanceIdDuplicate,
             "duplicate instance id is rejected")
        && ok;

    slicer_core::MultiModelScene missingReference = MakeDraftScene();
    missingReference.instances.front().instance.modelid = "missing";
    ok = ExpectTrue(
             slicer_core::ValidateMultiModelScene(
                 missingReference,
                 slicer_core::SceneValidationPurpose::Draft)
                     .errors.front()
                     .code
                 == slicer_core::SceneValidationErrorCode::
                     InstanceModelReferenceMissing,
             "missing model reference is rejected")
        && ok;

    slicer_core::MultiModelScene escaped = MakeDraftScene();
    escaped.models.front().sourcepath = "models/other/model.obj";
    ok = ExpectTrue(
             slicer_core::ValidateMultiModelScene(
                 escaped,
                 slicer_core::SceneValidationPurpose::Draft)
                     .errors.front()
                     .code
                 == slicer_core::SceneValidationErrorCode::
                     ResourceScopeEscape,
             "OBJ source cannot escape resource scope")
        && ok;

    slicer_core::MultiModelScene mixedProfile = MakeDraftScene();
    mixedProfile.instances.back().resolvedprofileid = "profile-b";
    return ExpectTrue(
               slicer_core::ValidateMultiModelScene(
                   mixedProfile,
                   slicer_core::SceneValidationPurpose::Draft)
                       .errors.front()
                       .code
                   == slicer_core::SceneValidationErrorCode::
                       SceneProfileMismatch,
               "scene_profile_only rejects mixed profiles")
        && ok;
}

bool SchemaRoundTripAndHash()
{
    const slicer_core::MultiModelScene original = MakeDraftScene();
    const slicer_core::Json document =
        slicer_core::SerializeMultiModelScene(original);
    const auto decoded = slicer_core::DeserializeMultiModelScene(document);
    if (!ExpectTrue(decoded.IsValid(), "scene schema round-trip decodes"))
    {
        return false;
    }

    const slicer_core::Json encodedAgain =
        slicer_core::SerializeMultiModelScene(decoded.scene);
    const std::string firstHash =
        slicer_core::ComputeMultiModelSceneHash(original);
    const std::string secondHash =
        slicer_core::ComputeMultiModelSceneHash(decoded.scene);

    slicer_core::Json::Object unsupported = document.as_object();
    unsupported["schema"] = "unsupported.scene.1";
    const auto unsupportedResult =
        slicer_core::DeserializeMultiModelScene(
            slicer_core::Json(std::move(unsupported)));

    return ExpectTrue(
               document.dump(0) == encodedAgain.dump(0),
               "scene serialization is canonical")
        && ExpectTrue(
            firstHash.size() == 64U && firstHash == secondHash,
            "scene hash is stable across round-trip")
        && ExpectTrue(
            !unsupportedResult.IsValid()
                && unsupportedResult.error->code
                    == slicer_core::SceneValidationErrorCode::
                        SchemaUnsupported,
            "unsupported schema fails closed");
}

bool BuildVolumeZLimitContract()
{
    const slicer_core::MultiModelScene legacyScene = MakeDraftScene();
    const slicer_core::Json legacyDocument =
        slicer_core::SerializeMultiModelScene(legacyScene);
    const std::string legacyCanonical = legacyDocument.dump(0);
    const auto legacyDecoded =
        slicer_core::DeserializeMultiModelScene(legacyDocument);

    const slicer_core::SceneBuildVolume defaultVolume =
        slicer_core::MakeDefaultDeviceBuildVolume();

    slicer_core::MultiModelScene limitedScene = MakeDraftScene();
    limitedScene.buildvolume = defaultVolume;
    limitedScene.buildvolume.zlimitmm = 1.5;
    const auto exceeded = slicer_core::ValidateMultiModelScene(
        limitedScene,
        slicer_core::SceneValidationPurpose::Production);
    const slicer_core::Json limitedDocument =
        slicer_core::SerializeMultiModelScene(limitedScene);
    const auto limitedDecoded =
        slicer_core::DeserializeMultiModelScene(limitedDocument);

    limitedScene.buildvolume.zlimitmm = 2.0;
    const auto atLimit = slicer_core::ValidateMultiModelScene(
        limitedScene,
        slicer_core::SceneValidationPurpose::Production);

    limitedScene.buildvolume.zlimitmm = 0.0;
    const auto invalid = slicer_core::ValidateMultiModelScene(
        limitedScene,
        slicer_core::SceneValidationPurpose::Production);

    return ExpectTrue(
               !legacyDocument.at("buildVolume").contains("zLimitMm")
                   && legacyDecoded.IsValid()
                   && !legacyDecoded.scene.buildvolume.zlimitmm.has_value()
                   && legacyCanonical
                       == slicer_core::SerializeMultiModelScene(
                              legacyDecoded.scene)
                              .dump(0),
               "omitted Z limit preserves legacy canonical scene bytes")
        && ExpectTrue(
            defaultVolume.source
                    == slicer_core::BuildVolumeSource::DeviceProfile
                && defaultVolume.widthmm
                    == slicer_core::kDefaultDeviceBuildVolumeWidthMm
                && defaultVolume.heightmm
                    == slicer_core::kDefaultDeviceBuildVolumeHeightMm
                && defaultVolume.zlimitmm
                    == slicer_core::kDefaultDeviceBuildVolumeZLimitMm
                && !defaultVolume.isfixture,
            "default device volume is explicitly 230 x 100 x 60 mm")
        && ExpectTrue(
            exceeded.IsValid()
                && exceeded.warnings.size() == limitedScene.instances.size(),
            "world bbox Z exceedance produces non-blocking warnings")
        && ExpectTrue(
            limitedDocument.at("buildVolume").at("zLimitMm").as_double()
                    == 1.5
                && limitedDecoded.IsValid()
                && limitedDecoded.scene.buildvolume.zlimitmm == 1.5,
            "explicit Z limit survives canonical scene round-trip")
        && ExpectTrue(
            atLimit.IsValid() && atLimit.warnings.empty(),
            "world bbox at the exact Z limit does not warn")
        && ExpectTrue(
            !invalid.IsValid()
                && invalid.errors.front().code
                    == slicer_core::SceneValidationErrorCode::
                        BuildVolumeUndefined,
            "non-positive Z limit fails closed");
}

bool EffectiveTransformCompositionIsFrozen()
{
    slicer_core::ModelTransform requested;
    requested.translatexmm = 2.0;

    slicer_core::ModelTransform derived;
    derived.translateymm = 3.0;
    derived.rotatezdeg = 90.0;

    const slicer_core::ModelTransform effective =
        slicer_core::ComposeModelTransforms(derived, requested);
    if (!ExpectTrue(
            std::abs(effective.translatexmm) < 1.0e-9
                && std::abs(effective.translateymm - 5.0) < 1.0e-9
                && std::abs(effective.rotatezdeg - 90.0) < 1.0e-9,
            "effective transform uses derived * requested order"))
    {
        return false;
    }

    slicer_core::MultiModelScene scene = MakeDraftScene();
    scene.instances.front().requestedtransform = requested;
    scene.instances.front().derivedlayouttransform = derived;
    scene.instances.front().effectivetransform = effective;
    scene.instances.front().instance.transform = effective;
    if (!ExpectTrue(
            slicer_core::ValidateMultiModelScene(
                scene,
                slicer_core::SceneValidationPurpose::Draft)
                .IsValid(),
            "composed effective transform validates"))
    {
        return false;
    }

    scene.instances.front().effectivetransform.translatexmm += 1.0;
    scene.instances.front().instance.transform =
        scene.instances.front().effectivetransform;
    if (!ExpectTrue(
            !slicer_core::ValidateMultiModelScene(
                 scene,
                 slicer_core::SceneValidationPurpose::Draft)
                 .IsValid(),
            "effective transform drift fails closed"))
    {
        return false;
    }

    slicer_core::MultiModelScene serializedScene = MakeDraftScene();
    serializedScene.instances.front().requestedtransform = requested;
    serializedScene.instances.front().derivedlayouttransform = derived;
    serializedScene.instances.front().effectivetransform.translatexmm = 0.0;
    serializedScene.instances.front().effectivetransform.translateymm = 5.0;
    serializedScene.instances.front().effectivetransform.rotatezdeg = 90.0;
    serializedScene.instances.front().instance.transform =
        serializedScene.instances.front().effectivetransform;
    if (!ExpectTrue(
            slicer_core::ValidateMultiModelScene(
                serializedScene,
                slicer_core::SceneValidationPurpose::Draft)
                .IsValid(),
            "equivalent decimal JSON transform tolerates trig round-off"))
    {
        return false;
    }

    slicer_core::MultiModelScene mirroredScene = MakeDraftScene();
    mirroredScene.instances.front().requestedtransform.mirrory = true;
    mirroredScene.instances.front().effectivetransform.mirrory = true;
    mirroredScene.instances.front().instance.transform =
        mirroredScene.instances.front().effectivetransform;
    return ExpectTrue(
        slicer_core::ValidateMultiModelScene(
            mirroredScene,
            slicer_core::SceneValidationPurpose::Draft)
            .IsValid(),
        "equivalent mirror representation satisfies composition");
}

bool SceneRevisionIsOptimistic()
{
    const slicer_core::MultiModelScene scene = MakeDraftScene();
    const auto current = slicer_core::ValidateSceneRevision(
        scene,
        scene.scenerevision);
    const auto stale = slicer_core::ValidateSceneRevision(
        scene,
        scene.scenerevision - 1U);
    return ExpectTrue(
               !current.has_value(),
               "current scene revision is accepted")
        && ExpectTrue(
            stale.has_value()
                && stale->code
                    == slicer_core::SceneValidationErrorCode::
                        SceneRevisionStale,
            "stale scene revision returns stable error");
}

bool SceneRevisionsRemainJsonExact()
{
    constexpr std::uint64_t kFirstInexactJsonInteger{
        9007199254740992ULL};
    slicer_core::MultiModelScene scene = MakeDraftScene();
    scene.scenerevision = kFirstInexactJsonInteger;
    const auto sceneResult = slicer_core::ValidateMultiModelScene(
        scene,
        slicer_core::SceneValidationPurpose::Draft);

    scene.scenerevision = 3U;
    scene.instances.front().instance.transformrevision =
        kFirstInexactJsonInteger;
    const auto instanceResult = slicer_core::ValidateMultiModelScene(
        scene,
        slicer_core::SceneValidationPurpose::Draft);

    return ExpectTrue(
               !sceneResult.IsValid()
                   && sceneResult.errors.front().code
                       == slicer_core::SceneValidationErrorCode::
                           SceneRevisionInvalid,
               "scene revision must remain exactly representable in JSON")
        && ExpectTrue(
            !instanceResult.IsValid()
                && instanceResult.errors.front().code
                    == slicer_core::SceneValidationErrorCode::
                        SceneRevisionInvalid,
            "transform revision must remain exactly representable in JSON");
}

bool SingleModelProjectionIsCompatible()
{
    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "single-scope";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = "models/single";

    slicer_core::ModelSource source;
    source.modelid = "single-model";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcepath = "models/single/model.obj";
    source.format = "obj";
    source.sourcehash = "single-source-hash";
    source.resourcehash = "single-resource-hash";

    const slicer_core::ModelInstance instance =
        MakeInstance("single-instance", source.modelid);
    const slicer_core::MultiModelScene scene =
        slicer_core::ProjectSingleModelScene(
            "single-scene",
            source,
            scope,
            instance,
            "profile-a");

    return ExpectTrue(
               scene.models.size() == 1U && scene.instances.size() == 1U,
               "single model projects to one model and instance")
        && ExpectTrue(
            scene.instances.front().instance.transformrevision
                    == instance.transformrevision
                && slicer_core::ModelTransformsEquivalent(
                    scene.instances.front().effectivetransform,
                    instance.transform),
            "single model projection preserves identity transform")
        && ExpectTrue(
            slicer_core::ValidateMultiModelScene(
                scene,
                slicer_core::SceneValidationPurpose::Draft)
                .IsValid(),
            "single model projection is a valid scene draft");
}

bool EffectiveConfigWriteReadbackAndStale()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13b_scene_effective_config";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path sourcePath = root / "scene.json";
    const std::filesystem::path generatedPath =
        root / "session" / "scene_config.effective.json";

    slicer_core::SceneEffectiveConfigRequest request;
    request.scene = MakeDraftScene();
    request.sourcescenepath = sourcePath;
    request.generatedconfigpath = generatedPath;
    request.sourceprofileid = "profile-a";
    request.generatedatutc = "2026-07-27T10:00:00.000Z";
    const std::string sourceSceneHash =
        slicer_core::ComputeMultiModelSceneHash(request.scene);

    const auto generated =
        slicer_core::GenerateSceneEffectiveConfig(request);
    const auto written =
        slicer_core::WriteSceneEffectiveConfig(request);
    const auto readback =
        slicer_core::ReadSceneEffectiveConfig(generatedPath);
    const auto revertedScene = readback.IsValid()
        ? slicer_core::DeserializeMultiModelScene(
              readback.document.at("sceneConfig"))
        : slicer_core::MultiModelSceneDecodeResult{};

    slicer_core::MultiModelScene changed = request.scene;
    ++changed.scenerevision;
    slicer_core::MultiModelScene transformed = request.scene;
    ++transformed.instances.front().instance.transformrevision;

    slicer_core::SceneEffectiveConfigRequest cancelled = request;
    cancelled.generatedconfigpath = root / "cancelled.json";
    cancelled.cancelled = true;
    const auto cancelledResult =
        slicer_core::WriteSceneEffectiveConfig(cancelled);

    slicer_core::SceneEffectiveConfigRequest overwrite = request;
    overwrite.sourcescenepath = generatedPath;
    overwrite.generatedconfigpath = generatedPath;
    const auto overwriteResult =
        slicer_core::WriteSceneEffectiveConfig(overwrite);

    const bool result =
        ExpectTrue(generated.IsValid(), "effective config generation succeeds")
        && ExpectTrue(written.IsValid(), "effective config writes atomically")
        && ExpectTrue(
            readback.IsValid()
                && readback.document.dump(0) == written.document.dump(0),
            "effective config readback matches generated document")
        && ExpectTrue(
            revertedScene.IsValid()
                && revertedScene.scene.sceneid
                    == request.scene.sceneid
                && revertedScene.scene.scenerevision
                    == request.scene.scenerevision
                && slicer_core::ComputeMultiModelSceneHash(
                       revertedScene.scene)
                    == sourceSceneHash,
            "effective config scene snapshot supports identity-preserving revert")
        && ExpectTrue(
            slicer_core::ComputeMultiModelSceneHash(request.scene)
                == sourceSceneHash,
            "effective config transaction never mutates the scene draft")
        && ExpectTrue(
            !slicer_core::IsSceneEffectiveConfigStale(
                readback.document,
                request.scene),
            "current scene revision is not stale")
        && ExpectTrue(
            slicer_core::IsSceneEffectiveConfigStale(
                readback.document,
                changed),
            "scene revision change makes effective config stale")
        && ExpectTrue(
            slicer_core::IsSceneEffectiveConfigStale(
                readback.document,
                transformed),
            "transform revision change makes effective config stale")
        && ExpectTrue(
            !cancelledResult.IsValid()
                && !std::filesystem::exists(
                    cancelled.generatedconfigpath),
            "cancelled generation leaves no partial output")
        && ExpectTrue(
            !overwriteResult.IsValid()
                && overwriteResult.error->code
                    == slicer_core::SceneValidationErrorCode::
                        EffectiveConfigWriteFailed,
            "effective config cannot overwrite scene draft");

    std::filesystem::remove_all(root, cleanupError);
    return result;
}

bool RepositoryFixturesAreUsable()
{
    const std::filesystem::path root{SLICESOFT_SOURCE_DIR};
    std::ifstream positiveInput(
        root / "samples/configs/scene/fixture_two_model_scene.json");
    std::ifstream badInput(
        root / "samples/configs/scene/bad/duplicate_model_id.json");
    if (!ExpectTrue(
            positiveInput.good() && badInput.good(),
            "scene repository fixtures are readable"))
    {
        return false;
    }

    const auto positive = slicer_core::DeserializeMultiModelScene(
        slicer_core::Json::parse(positiveInput));
    const auto bad = slicer_core::DeserializeMultiModelScene(
        slicer_core::Json::parse(badInput));
    return ExpectTrue(
               std::abs(
                   slicer_core::MultiModelScene{}
                       .layout.columngapmm
                   - 10.0)
                       < 1.0e-9
                   && std::abs(
                          slicer_core::MultiModelScene{}
                              .layout.rowgapmm
                          - 10.0)
                       < 1.0e-9,
               "new scenes default to 10 mm row and column gaps")
        && ExpectTrue(
               positive.IsValid()
                   && std::abs(
                          positive.scene.layout
                              .columngapmm
                          - 10.0)
                       < 1.0e-9
                   && std::abs(
                          positive.scene.layout
                              .rowgapmm
                          - 10.0)
                       < 1.0e-9
                   && slicer_core::ValidateMultiModelScene(
                       positive.scene,
                       slicer_core::SceneValidationPurpose::Draft)
                       .IsValid(),
               "two-model repository fixture validates")
        && ExpectTrue(
            bad.IsValid()
                && !slicer_core::ValidateMultiModelScene(
                     bad.scene,
                     slicer_core::SceneValidationPurpose::Draft)
                     .IsValid(),
            "bad repository fixture is rejected by validation");
}

}  // namespace

int main()
{
    const bool ok = StableErrorNames()
        && DraftAndProductionValidation()
        && IdentityAndResourceValidation()
        && SchemaRoundTripAndHash()
        && BuildVolumeZLimitContract()
        && EffectiveTransformCompositionIsFrozen()
        && SceneRevisionIsOptimistic()
        && SceneRevisionsRemainJsonExact()
        && SingleModelProjectionIsCompatible()
        && EffectiveConfigWriteReadbackAndStale()
        && RepositoryFixturesAreUsable();
    if (!ok)
    {
        return 1;
    }

    std::cout << "multimodel_scene_contract_unit_tests: PASS\n";
    return 0;
}
