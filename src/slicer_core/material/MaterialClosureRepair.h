#pragma once

#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/**
 * @brief Model-fill material written by exact material-closure repair.
 */
enum class MaterialClosureModelFillMaterial
{
    Rgb,
    White,
    Varnish,
    None,
};

/**
 * @brief Channel values used when a repair plan is applied to an RGBWSV layer.
 */
struct MaterialClosureRepairValues
{
    MaterialClosureModelFillMaterial modelFillMaterial{MaterialClosureModelFillMaterial::White};
    std::array<std::uint8_t, 3> modelFillRgb{0U, 0U, 0U};
    std::uint8_t modelFillValue{0U};
    std::uint8_t supportValue{0U};
};

/**
 * @brief Per-pixel exact repair plan built before production TIFF writing.
 */
struct MaterialClosureRepairPlan
{
    int widthPx{0};
    int heightPx{0};
    std::vector<std::uint8_t> externalBackgroundMask;
    std::vector<std::uint8_t> expectedOccupiedDomainMask;
    std::vector<std::uint8_t> modelFillRepairMask;
    std::vector<std::uint8_t> supportRepairMask;
    std::vector<std::uint8_t> internalVoidSupportRepairMask;
    std::vector<std::uint8_t> colorFillRepairMask;
    std::vector<std::uint8_t> modelSupportRepairMask;
    std::vector<std::uint8_t> varnishSupportRepairMask;
    std::vector<std::uint8_t> rejectedTooWideMask;
    int modelFillRepairPixels{0};
    int supportRepairPixels{0};
    int rejectedTooWidePixels{0};
    int externalBackgroundProtectedPixels{0};
};

/** @brief Borrowed repair plan valid until its workspace is reused. */
struct MaterialClosureRepairPlanView
{
    int widthPx{0};
    int heightPx{0};
    std::span<const std::uint8_t> externalBackgroundMask;
    std::span<const std::uint8_t> expectedOccupiedDomainMask;
    std::span<const std::uint8_t> modelFillRepairMask;
    std::span<const std::uint8_t> supportRepairMask;
    std::span<const std::uint8_t> internalVoidSupportRepairMask;
    std::span<const std::uint8_t> colorFillRepairMask;
    std::span<const std::uint8_t> modelSupportRepairMask;
    std::span<const std::uint8_t> varnishSupportRepairMask;
    std::span<const std::uint8_t> rejectedTooWideMask;
    int modelFillRepairPixels{0};
    int supportRepairPixels{0};
    int rejectedTooWidePixels{0};
    int externalBackgroundProtectedPixels{0};
};

/** @brief Caller-owned storage reused while building repair plans. */
struct MaterialClosureRepairWorkspace
{
    void Prepare(std::size_t pixelCount);

    std::vector<std::uint8_t> externalBackgroundMask;
    std::vector<std::uint8_t> expectedOccupiedDomainMask;
    std::vector<std::uint8_t> modelFillRepairMask;
    std::vector<std::uint8_t> supportRepairMask;
    std::vector<std::uint8_t> internalVoidSupportRepairMask;
    std::vector<std::uint8_t> colorFillRepairMask;
    std::vector<std::uint8_t> modelSupportRepairMask;
    std::vector<std::uint8_t> varnishSupportRepairMask;
    std::vector<std::uint8_t> rejectedTooWideMask;
    std::vector<std::uint8_t> visited;
    std::vector<std::uint8_t> componentMask;
    std::vector<std::size_t> componentPixels;
};

/** @brief Mutable borrowed semantic masks updated by repair application. */
struct MaterialClosureSemanticLayerMutableInputView
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::span<std::uint8_t> textureSurfaceMask;
    std::span<std::uint8_t> modelFillMask;
    std::span<std::uint8_t> modelMaterialMask;
    std::span<std::uint8_t> supportFillMask;
    std::span<std::uint8_t> internalVoidSupportMask;
    std::span<std::uint8_t> surfaceVarnishMask;
    std::span<std::uint8_t> outerVarnishShellMask;
    std::span<std::uint8_t> modelEnvelopeMask;
    std::span<std::uint8_t> supportRequiredMask;
    std::span<std::uint8_t> expectedOccupiedDomainMask;
    std::span<std::uint8_t> layerEmptyMask;
};

/** @brief Create a mutable borrowed view over the existing owning semantic DTO. */
[[nodiscard]] MaterialClosureSemanticLayerMutableInputView
ViewMaterialClosureSemanticLayerInput(
    MaterialClosureSemanticLayerInput& input) noexcept;

/** @brief Build a repair plan using caller-prepared reusable storage. */
MaterialClosureRepairPlanView BuildMaterialClosureRepairPlan(
    const MaterialClosureSemanticLayerInputView& input,
    const MaterialClosureSemanticLayerAnalysisView& analysis,
    int connectivity,
    MaterialClosureRepairWorkspace& workspace);

/**
 * @brief Actual channel and semantic-mask changes made by a repair plan.
 */
struct MaterialClosureRepairApplicationResult
{
    int repairedPixels{0};
    int repairedModelFillPixels{0};
    int repairedSupportPixels{0};
    int repairedColorFillPixels{0};
    int repairedModelSupportPixels{0};
    int repairedInternalVoidPixels{0};
    int repairedVarnishSupportPixels{0};
    int blockedExternalBackgroundRepairPixels{0};
    int blockedOutsideExpectedDomainRepairPixels{0};
    int blockedRejectedTooWideRepairPixels{0};
};

/** @brief Apply a borrowed repair plan without allocating. */
MaterialClosureRepairApplicationResult ApplyMaterialClosureRepair(
    const MaterialClosureRepairPlanView& plan,
    const MaterialClosureRepairValues& values,
    std::span<std::uint8_t> layer,
    MaterialClosureSemanticLayerMutableInputView& input);

/**
 * @brief Builds a conservative one-pixel repair plan from exact semantic evidence.
 * @param input Original semantic evidence for contextual model/support decisions.
 * @param analysis Exact gap analysis produced from the same input.
 * @param connectivity Pixel connectivity, either 4 or 8.
 * @return Repair masks. Two-pixel-thick or unclassifiable components are not repaired.
 * @throws std::invalid_argument When dimensions, connectivity, or mask sizes are invalid.
 */
MaterialClosureRepairPlan BuildMaterialClosureRepairPlan(
    const MaterialClosureSemanticLayerInput& input,
    const MaterialClosureSemanticLayerAnalysis& analysis,
    int connectivity);

/**
 * @brief Applies a repair plan to an interleaved uint8 RGBWSV layer and semantic masks.
 * @param plan Exact repair plan for the layer.
 * @param values Material and support channel values.
 * @param layer Mutable interleaved R,G,B,W,S,V production buffer.
 * @param input Mutable semantic evidence updated to match the repaired layer.
 * @return Actual repair counts.
 * @throws std::invalid_argument When layer or mask dimensions do not match the plan.
 */
MaterialClosureRepairApplicationResult ApplyMaterialClosureRepair(
    const MaterialClosureRepairPlan& plan,
    const MaterialClosureRepairValues& values,
    std::vector<std::uint8_t>& layer,
    MaterialClosureSemanticLayerInput& input);

}  // namespace slicer_core
