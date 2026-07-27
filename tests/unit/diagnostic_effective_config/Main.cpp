#include "slicer_core/pipeline/DiagnosticEffectiveConfig.h"

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

slicer_core::DiagnosticEffectiveConfigRequest MakeSingleModelRequest()
{
    slicer_core::DiagnosticEffectiveConfigRequest request;
    request.subjecttype =
        slicer_core::DiagnosticSubjectType::SingleModel;
    request.sessionid = "session-single";
    request.sourceprofileid = "profile-a";
    request.generatedatutc = "2026-07-27T10:00:00Z";
    request.sourceconfigpath = "samples/configs/source.json";
    request.singlemodel.modelpath = "models/nail.obj";
    request.singlemodel.modelhash = "model-hash-a";
    request.singlemodel.sourceconfighash = "source-config-hash-a";
    request.requested.texturesurfacewidthmm = 0.10;
    request.requested.modelfillmaterial = "white";
    request.requested.diagnosticbackendrequest = "legacy_cpu";
    request.derived.minimumwidthmm = 0.01;
    request.derived.maximumwidthmm = 0.50;
    request.derived.alltexturethresholdmm = std::nullopt;
    request.derived.backendavailability = "available";
    request.derived.derivationsource = "diagnostic-width-sweep-v1";
    request.effective.texturesurfacewidthmm = 0.10;
    request.effective.modelfillmaterial = "white";
    request.effective.diagnosticbackend = "legacy_cpu";
    request.effective.resolvedprofileid = "profile-a";
    return request;
}

slicer_core::MultiModelScene MakeScene()
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-fixture";
    scene.scenerevision = 4U;
    scene.resolvedprofileid = "profile-a";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-a";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = "models";
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource model;
    model.modelid = "model-a";
    model.sourcepath = "models/nail.obj";
    model.format = "obj";
    model.resourcescopeid = "scope-a";
    model.sourcehash = "source-hash-a";
    model.resourcehash = "resource-hash-a";
    model.displayname = "nail";
    scene.models.push_back(model);

    slicer_core::ModelSource secondModel = model;
    secondModel.modelid = "model-b";
    secondModel.sourcepath = "models/nail-b.obj";
    secondModel.sourcehash = "source-hash-b";
    secondModel.resourcehash = "resource-hash-b";
    secondModel.displayname = "nail-b";
    scene.models.push_back(secondModel);

    slicer_core::SceneModelInstance sceneInstance;
    sceneInstance.instance.instanceid = "instance-a";
    sceneInstance.instance.modelid = "model-a";
    sceneInstance.instance.sourcetransformidentity =
        "source-transform-a";
    sceneInstance.instance.transformrevision = 7U;
    sceneInstance.instance.sourcebboxmm.min = {0.0, 0.0, 0.0};
    sceneInstance.instance.sourcebboxmm.max = {10.0, 20.0, 2.0};
    sceneInstance.instance.effectivebboxmm =
        sceneInstance.instance.sourcebboxmm;
    sceneInstance.resolvedprofileid = "profile-a";
    scene.instances.push_back(sceneInstance);

    slicer_core::SceneModelInstance secondInstance =
        sceneInstance;
    secondInstance.instance.instanceid = "instance-b";
    secondInstance.instance.modelid = "model-b";
    secondInstance.instance.sourcetransformidentity =
        "source-transform-b";
    secondInstance.instance.transformrevision = 3U;
    scene.instances.push_back(secondInstance);
    return scene;
}

slicer_core::DiagnosticEffectiveConfigRequest MakeSceneRequest()
{
    slicer_core::DiagnosticEffectiveConfigRequest request =
        MakeSingleModelRequest();
    request.subjecttype = slicer_core::DiagnosticSubjectType::Scene;
    request.sessionid = "session-scene";
    request.singlemodel = {};
    slicer_core::DiagnosticSceneSelection selection;
    selection.scene = MakeScene();
    selection.currentmodelid = "model-a";
    selection.currentinstanceid = "instance-a";
    request.scene = std::move(selection);
    return request;
}

std::filesystem::path MakeTempDirectory(const std::string& name)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / ("slicesoft_09a_02_" + name);
    std::error_code error;
    std::filesystem::remove_all(path, error);
    std::filesystem::create_directories(path);
    return path;
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool StableErrorNames()
{
    using slicer_core::DiagnosticEffectiveConfigErrorCode;
    const struct
    {
        DiagnosticEffectiveConfigErrorCode code;
        std::string expected;
    } cases[]{
        {DiagnosticEffectiveConfigErrorCode::None, "NONE"},
        {DiagnosticEffectiveConfigErrorCode::SchemaUnsupported,
         "DIAGNOSTIC_CONFIG_SCHEMA_UNSUPPORTED"},
        {DiagnosticEffectiveConfigErrorCode::SubjectInvalid,
         "DIAGNOSTIC_SUBJECT_INVALID"},
        {DiagnosticEffectiveConfigErrorCode::SceneIdentityRequired,
         "DIAGNOSTIC_SCENE_IDENTITY_REQUIRED"},
        {DiagnosticEffectiveConfigErrorCode::InstanceReferenceMissing,
         "DIAGNOSTIC_INSTANCE_REFERENCE_MISSING"},
        {DiagnosticEffectiveConfigErrorCode::RevisionStale,
         "DIAGNOSTIC_REVISION_STALE"},
        {DiagnosticEffectiveConfigErrorCode::ProfileMismatch,
         "DIAGNOSTIC_PROFILE_MISMATCH"},
        {DiagnosticEffectiveConfigErrorCode::RequestInvalid,
         "DIAGNOSTIC_REQUEST_INVALID"},
        {DiagnosticEffectiveConfigErrorCode::DerivationUnavailable,
         "DIAGNOSTIC_DERIVATION_UNAVAILABLE"},
        {DiagnosticEffectiveConfigErrorCode::Cancelled,
         "DIAGNOSTIC_CONFIG_CANCELLED"},
        {DiagnosticEffectiveConfigErrorCode::IntegrityFailed,
         "DIAGNOSTIC_CONFIG_INTEGRITY_FAILED"},
        {DiagnosticEffectiveConfigErrorCode::WriteFailed,
         "DIAGNOSTIC_CONFIG_WRITE_FAILED"},
    };

    bool passed = true;
    for (const auto& item : cases)
    {
        passed = ExpectTrue(
                     slicer_core::DiagnosticEffectiveConfigErrorCodeName(
                         item.code)
                         == item.expected,
                     "stable error " + item.expected)
            && passed;
    }
    return passed;
}

bool SingleModelRoundTripPreservesNull()
{
    const auto result =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            MakeSingleModelRequest());
    if (!ExpectTrue(result.IsValid(), "single model generation"))
    {
        return false;
    }

    const slicer_core::Json& document = result.document;
    const slicer_core::Json& identity = document.at("identity");
    const slicer_core::Json& derived = document.at("derived");
    return ExpectTrue(
               document.at("schema").as_string()
                   == slicer_core::DiagnosticEffectiveConfigSchemaName(),
               "diagnostic schema")
        && ExpectTrue(
            document.at("subjectType").as_string() == "single_model",
            "single model subject")
        && ExpectTrue(
            identity.at("modelPath").as_string() == "models/nail.obj",
            "single model identity")
        && ExpectTrue(
            derived.at("allTextureThresholdMm").is_null(),
            "unevaluated threshold remains null")
        && ExpectTrue(
            result.confighash.size() == 64U,
            "single model hash");
}

bool SceneRoundTripBindsCurrentInstance()
{
    const auto request = MakeSceneRequest();
    const auto result =
        slicer_core::GenerateDiagnosticEffectiveConfig(request);
    if (!ExpectTrue(result.IsValid(), "scene generation"))
    {
        return false;
    }
    const slicer_core::Json& identity = result.document.at("identity");
    return ExpectTrue(
               result.document.at("subjectType").as_string() == "scene",
               "scene subject")
        && ExpectTrue(
            identity.at("sceneId").as_string() == "scene-fixture",
            "scene identity")
        && ExpectTrue(
            identity.at("sceneRevision").as_double() == 4.0,
            "scene revision")
        && ExpectTrue(
            identity.at("currentModelId").as_string() == "model-a",
            "current model")
        && ExpectTrue(
            identity.at("currentInstanceId").as_string()
                == "instance-a",
            "current instance")
        && ExpectTrue(
            identity.at("transformRevision").as_double() == 7.0,
            "transform revision");
}

bool SceneMismatchFailsClosed()
{
    auto missingInstance = MakeSceneRequest();
    missingInstance.scene->currentinstanceid = "missing";
    const auto missingResult =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            missingInstance);

    auto profileMismatch = MakeSceneRequest();
    profileMismatch.sourceprofileid = "profile-b";
    const auto profileResult =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            profileMismatch);

    auto modelMismatch = MakeSceneRequest();
    modelMismatch.scene->currentmodelid = "other-model";
    const auto modelResult =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            modelMismatch);

    return ExpectTrue(
               missingResult.error.has_value()
                   && missingResult.error->code
                       == slicer_core::
                           DiagnosticEffectiveConfigErrorCode::
                               InstanceReferenceMissing,
               "missing instance rejected")
        && ExpectTrue(
            profileResult.error.has_value()
                && profileResult.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::
                            ProfileMismatch,
            "scene Profile mismatch rejected")
        && ExpectTrue(
            modelResult.error.has_value()
                && modelResult.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::
                            InstanceReferenceMissing,
            "current model mismatch rejected");
}

bool InvalidRequestAndDerivationFailClosed()
{
    auto invalidRequest = MakeSingleModelRequest();
    invalidRequest.requested.texturesurfacewidthmm = -0.01;
    const auto requestResult =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            invalidRequest);

    auto invalidDerivation = MakeSingleModelRequest();
    invalidDerivation.derived.maximumwidthmm = 0.05;
    const auto derivationResult =
        slicer_core::GenerateDiagnosticEffectiveConfig(
            invalidDerivation);

    return ExpectTrue(
               requestResult.error.has_value()
                   && requestResult.error->code
                       == slicer_core::
                           DiagnosticEffectiveConfigErrorCode::
                               RequestInvalid,
               "negative requested width rejected")
        && ExpectTrue(
            derivationResult.error.has_value()
                && derivationResult.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::
                            DerivationUnavailable,
            "out-of-range effective derivation rejected");
}

bool WriteReadTamperCancelAndStale()
{
    const std::filesystem::path directory =
        MakeTempDirectory("transaction");
    const std::filesystem::path outputPath =
        directory / "slice_config.diagnostic.effective.json";
    const std::filesystem::path sourcePath =
        directory / "source.json";
    {
        std::ofstream source(sourcePath);
        source << "{\"source\":true}\n";
    }
    const std::string sourceBefore = ReadText(sourcePath);

    auto request = MakeSceneRequest();
    request.sourceconfigpath = sourcePath;
    request.generatedconfigpath = outputPath;
    const auto written =
        slicer_core::WriteDiagnosticEffectiveConfig(request);
    const auto read =
        slicer_core::ReadDiagnosticEffectiveConfig(outputPath);
    if (!ExpectTrue(
            written.IsValid() && read.IsValid(),
            "write and readback"))
    {
        std::filesystem::remove_all(directory);
        return false;
    }

    const std::string validBytes = ReadText(outputPath);
    auto cancelled = request;
    cancelled.cancelled = true;
    const auto cancelResult =
        slicer_core::WriteDiagnosticEffectiveConfig(cancelled);
    const bool cancelPreservedOutput =
        ReadText(outputPath) == validBytes;

    auto overwrite = request;
    overwrite.generatedconfigpath = sourcePath;
    const auto overwriteResult =
        slicer_core::WriteDiagnosticEffectiveConfig(overwrite);

    auto staleScene = request;
    ++staleScene.scene->scene.scenerevision;
    auto staleTransform = request;
    ++staleTransform.scene->scene.instances.front()
          .instance.transformrevision;
    auto staleRequested = request;
    staleRequested.requested.texturesurfacewidthmm = 0.20;
    staleRequested.effective.texturesurfacewidthmm = 0.20;
    auto staleInstance = request;
    staleInstance.scene->currentmodelid = "model-b";
    staleInstance.scene->currentinstanceid = "instance-b";

    slicer_core::Json::Object hashTamperedObject =
        written.document.as_object();
    hashTamperedObject["configHash"] = "bad-hash";
    const slicer_core::Json hashTamperedDocument(
        std::move(hashTamperedObject));

    {
        std::fstream tamper(
            outputPath,
            std::ios::in | std::ios::out | std::ios::binary);
        std::string bytes{
            std::istreambuf_iterator<char>(tamper),
            std::istreambuf_iterator<char>()};
        const std::size_t position = bytes.find("profile-a");
        if (position != std::string::npos)
        {
            bytes.replace(position, 9U, "profile-z");
        }
        tamper.clear();
        tamper.seekp(0);
        tamper.write(
            bytes.data(),
            static_cast<std::streamsize>(bytes.size()));
        tamper.close();
    }
    const auto tampered =
        slicer_core::ReadDiagnosticEffectiveConfig(outputPath);

    const bool passed =
        ExpectTrue(
            cancelResult.error.has_value()
                && cancelResult.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::Cancelled,
            "cancelled write rejected")
        && ExpectTrue(
            ReadText(sourcePath) == sourceBefore,
            "source config preserved")
        && ExpectTrue(
            cancelPreservedOutput,
            "cancelled write preserves previous successful config")
        && ExpectTrue(
            overwriteResult.error.has_value()
                && overwriteResult.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "source overwrite rejected")
        && ExpectTrue(
            validBytes.find(
                slicer_core::DiagnosticEffectiveConfigSchemaName())
                != std::string::npos,
            "valid file published")
        && ExpectTrue(
            !slicer_core::IsDiagnosticEffectiveConfigStale(
                written.document,
                request),
            "current request is not stale")
        && ExpectTrue(
            slicer_core::IsDiagnosticEffectiveConfigStale(
                written.document,
                staleScene),
            "scene revision stale")
        && ExpectTrue(
            slicer_core::IsDiagnosticEffectiveConfigStale(
                written.document,
                staleTransform),
            "transform revision stale")
        && ExpectTrue(
            slicer_core::IsDiagnosticEffectiveConfigStale(
                written.document,
                staleRequested),
            "requested settings stale")
        && ExpectTrue(
            slicer_core::IsDiagnosticEffectiveConfigStale(
                written.document,
                staleInstance),
            "current instance stale")
        && ExpectTrue(
            slicer_core::IsDiagnosticEffectiveConfigStale(
                hashTamperedDocument,
                request),
            "hash-only tamper is stale")
        && ExpectTrue(
            tampered.error.has_value()
                && tampered.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::
                            IntegrityFailed,
            "tampered file rejected")
        && ExpectTrue(
            !std::filesystem::exists(outputPath.string() + ".tmp"),
            "staging file absent");

    std::filesystem::remove_all(directory);
    return passed;
}

bool FilenameAndFixtureContracts()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path fixture =
        sourceRoot / "samples/configs/diagnostic_effective/"
        "single_model_request.json";
    const std::filesystem::path sceneFixture =
        sourceRoot / "samples/configs/diagnostic_effective/"
        "scene_instance_a_request.json";
    const std::filesystem::path badFixture =
        sourceRoot / "samples/configs/diagnostic_effective/bad/"
        "missing_instance.json";
    auto request = MakeSingleModelRequest();
    request.generatedconfigpath =
        MakeTempDirectory("filename") / "slice_config.effective.json";
    const auto result =
        slicer_core::WriteDiagnosticEffectiveConfig(request);
    const bool passed =
        ExpectTrue(
            std::filesystem::exists(fixture)
                && std::filesystem::exists(sceneFixture)
                && std::filesystem::exists(badFixture),
            "diagnostic fixtures exist")
        && ExpectTrue(
            result.error.has_value()
                && result.error->code
                    == slicer_core::
                        DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "legacy filename rejected");
    std::filesystem::remove_all(
        request.generatedconfigpath.parent_path());
    return passed;
}

}  // namespace

int main()
{
    const bool passed = StableErrorNames()
        && SingleModelRoundTripPreservesNull()
        && SceneRoundTripBindsCurrentInstance()
        && SceneMismatchFailsClosed()
        && InvalidRequestAndDerivationFailClosed()
        && WriteReadTamperCancelAndStale()
        && FilenameAndFixtureContracts();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS diagnostic_effective_config_unit_tests\n";
    return 0;
}
