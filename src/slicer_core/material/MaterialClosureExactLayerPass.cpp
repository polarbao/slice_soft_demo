#include "slicer_core/material/MaterialClosureExactLayerPass.h"

#include <utility>

namespace slicer_core
{

MaterialClosureExactLayerOutcome RunMaterialClosureExactLayerPass(
    MaterialClosureSemanticLayerInput& input,
    const MaterialClosureExactLayerRequest& request,
    const std::span<std::uint8_t> layer,
    MaterialClosureExactLayerWorkspace& workspace)
{
    // View 版本不自行 Prepare（只有 owning 便利版本做），故由调用方负责。
    workspace.semantic.Prepare(input.layerEmptyMask.size());
    const MaterialClosureSemanticLayerInputView view =
        ViewMaterialClosureSemanticLayerInput(std::as_const(input));
    // 只用到 summary，故不需要 owning 版本把 7 个 mask 再拷一遍。
    const MaterialClosureSemanticLayerAnalysisView analysis =
        AnalyzeMaterialClosureSemanticLayer(
            view, request.connectivity, request.maxGapPx, workspace.semantic);

    MaterialClosureExactLayerOutcome outcome;
    outcome.result = analysis.summary;
    if (!request.repair)
    {
        return outcome;
    }

    workspace.repair.Prepare(analysis.candidateGapMask.size());
    const MaterialClosureRepairPlanView plan = BuildMaterialClosureRepairPlan(
        view, analysis, request.connectivity, workspace.repair);
    MaterialClosureSemanticLayerMutableInputView mutableView =
        ViewMaterialClosureSemanticLayerInput(input);
    const MaterialClosureRepairApplicationResult applied =
        ApplyMaterialClosureRepair(plan, request.repairValues, layer, mutableView);
    // 复检可以复用同一个语义 workspace：analysis 借出的 mask 到此不再被读
    // （summary 是值拷贝），plan 的计数也是值成员。
    const MaterialClosureSemanticLayerResult remaining =
        DetectMaterialClosureSemanticLayer(
            view, request.connectivity, request.maxGapPx, workspace.semantic);

    MaterialClosureSemanticLayerResult& result = outcome.result;
    result.repairAttempted = true;
    result.repairedPixels = applied.repairedPixels;
    result.repairedColorFillPixels = applied.repairedColorFillPixels;
    result.repairedModelSupportPixels = applied.repairedModelSupportPixels;
    result.repairedInternalVoidPixels = applied.repairedInternalVoidPixels;
    result.repairedVarnishSupportPixels = applied.repairedVarnishSupportPixels;
    result.remainingGapPixels = remaining.gapPixels;
    result.remainingColorFillGapPixels = remaining.colorFillGapPixels;
    result.remainingModelSupportGapPixels = remaining.modelSupportGapPixels;
    result.remainingColorSupportGapPixels = remaining.colorSupportGapPixels;
    result.remainingInternalVoidGapPixels = remaining.internalVoidGapPixels;
    result.remainingVarnishSupportGapPixels = remaining.varnishSupportGapPixels;
    result.repairRejectedTooWidePixels = plan.rejectedTooWidePixels;

    outcome.repairedModelFillPixels = applied.repairedModelFillPixels;
    outcome.repairedSupportPixels = applied.repairedSupportPixels;
    outcome.repairedInternalVoidPixels = applied.repairedInternalVoidPixels;
    return outcome;
}

}  // namespace slicer_core
