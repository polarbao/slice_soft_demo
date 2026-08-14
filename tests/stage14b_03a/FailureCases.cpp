// 本文件集中验证 ViewData 资源缺失、解码失败与预算越界等拒绝路径；
// 失败必须保持可诊断且不得退化为不完整的成功结果。
#include "TestSupport.h"

#include <algorithm>

namespace stage14b03a
{
namespace
{

slicer_core::api::ApiResult<slicer_core::api::SceneViewData> RunSingle(
    slicer_core::SceneModel model,
    const std::shared_ptr<TestTextureSource>& textures,
    slicer_core::api::SceneViewDataRequest request =
        MakeRequest(slicer_core::api::ViewMode::Top))
{
    const auto sharedModel = std::make_shared<const slicer_core::SceneModel>(
        std::move(model));
    const auto provider = MakeProvider({{401U, sharedModel}}, textures);
    const TestCancelToken active;
    return provider->GetViewData(
        request,
        MakeSnapshot({{"failure-instance", 401U}}),
        active);
}

void MissingAndDecodeFailures()
{
    auto missingTextures = std::make_shared<TestTextureSource>();
    auto missingModel = MakeTexturedQuad(
        "missing.obj", "used", "missing.png");
    const auto missing = RunSingle(std::move(missingModel), missingTextures);
    RequireError(missing.Error(), "PM-SLICER-INPUT-0001",
                 "missing used texture");

    auto decodeTextures = std::make_shared<TestTextureSource>();
    decodeTextures->AddError(
        "broken.png",
        {"PM-SLICER-INPUT-0002", "decode failed", "broken.png"});
    auto decodeModel = MakeTexturedQuad(
        "decode.obj", "used", "broken.png");
    const auto decode = RunSingle(std::move(decodeModel), decodeTextures);
    RequireError(decode.Error(), "PM-SLICER-INPUT-0002",
                 "undecodable used texture");
}

void UvAndMaterialFailures()
{
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "valid.png",
        MakeTexture({0U, 0U, 0U, 255U}, {255U, 255U, 255U, 255U}));
    auto noUv = MakeTexturedQuad("no_uv.obj", "used", "valid.png");
    noUv.triangle_textures.front().has_uv = false;
    const auto noUvResult = RunSingle(std::move(noUv), textures);
    RequireError(noUvResult.Error(), "PM-SLICER-INPUT-0002",
                 "textured triangle without UV");

    auto badMaterial = MakeTexturedQuad(
        "bad_material.obj", "used", "valid.png");
    badMaterial.triangle_textures.front().material_name = "unknown";
    const auto badMaterialResult = RunSingle(
        std::move(badMaterial), textures);
    RequireError(badMaterialResult.Error(), "PM-SLICER-INPUT-0002",
                 "unknown material binding");

    auto missingLibraryMaterial = MakeTexturedQuad(
        "missing-library-material.obj", "mtl0", "unused.png");
    missingLibraryMaterial.material_infos.clear();
    missingLibraryMaterial.material_libraries.push_back("missing.mtl");
    const auto missingLibraryResult = RunSingle(
        std::move(missingLibraryMaterial), textures);
    RequireError(missingLibraryResult.Error(), "PM-SLICER-INPUT-0002",
                 "declared library with unresolved material binding");
}

void BudgetAndRequestFailures()
{
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "valid.png",
        MakeTexture({0U, 0U, 0U, 255U}, {255U, 255U, 255U, 255U}));
    auto budgetRequest = MakeRequest(slicer_core::api::ViewMode::Top);
    budgetRequest.max_bytes = 64U;
    const auto budget = RunSingle(
        MakeTexturedQuad("budget.obj", "used", "valid.png"),
        textures,
        budgetRequest);
    RequireError(budget.Error(), "PM-SLICER-VIEWDATA-BUDGET",
                 "insufficient texture-preserving budget");

    auto outlineRequest = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    outlineRequest.lod = slicer_core::api::ViewLod::OutlineOnly;
    const auto outline = RunSingle(
        MakeTexturedQuad("outline.obj", "used", "valid.png"),
        textures,
        outlineRequest);
    RequireError(outline.Error(), "PM-SLICER-PROFILE-0031",
                 "three_d outline_only");

    auto missingContentRequest = MakeRequest(
        slicer_core::api::ViewMode::Top);
    missingContentRequest.content.erase(
        std::remove(
            missingContentRequest.content.begin(),
            missingContentRequest.content.end(),
            slicer_core::api::ViewContent::SurfacePreview),
        missingContentRequest.content.end());
    const auto missingContent = RunSingle(
        MakeTexturedQuad("content.obj", "used", "valid.png"),
        textures,
        missingContentRequest);
    RequireError(missingContent.Error(), "PM-SLICER-PROFILE-0031",
                 "top content without surface preview");

    auto duplicateContentRequest = MakeRequest(
        slicer_core::api::ViewMode::Top);
    duplicateContentRequest.content.push_back(
        slicer_core::api::ViewContent::Bbox);
    const auto duplicateContent = RunSingle(
        MakeTexturedQuad("duplicate.obj", "used", "valid.png"),
        textures,
        duplicateContentRequest);
    RequireError(duplicateContent.Error(), "PM-SLICER-PROFILE-0031",
                 "duplicate ViewData content");
}

void CancellationFailure()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad("cancel.obj", "used", "valid.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "valid.png",
        MakeTexture({0U, 0U, 0U, 255U}, {255U, 255U, 255U, 255U}));
    const auto provider = MakeProvider({{501U, model}}, textures);
    const TestCancelToken cancelled(true);
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        MakeSnapshot({{"cancel-instance", 501U}}),
        cancelled);
    RequireError(result.Error(), "PM-SLICER-CANCELLED-0070",
                 "cancelled ViewData request");
}

}  // namespace

void RunFailureCases()
{
    MissingAndDecodeFailures();
    UvAndMaterialFailures();
    BudgetAndRequestFailures();
    CancellationFailure();
}

}  // namespace stage14b03a
