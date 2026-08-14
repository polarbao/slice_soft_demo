#pragma once

#include "slicer_core/geometry/ContactPostureMetrics.h"

#include <string>

namespace slicer_core
{

/** @brief Stage 16 接触调平的稳定仅诊断策略。 */
struct ContactLevelingPolicy
{
    double minimumangledeg{-12.0};
    double maximumangledeg{12.0};
    double coarseangleincrementdeg{0.5};
    double refineangleincrementdeg{0.1};
    double refineneighborhooddeg{0.5};
    double maximumheightincreasemm{0.5};
    double maximumfootprintincreasemm{0.5};
};

/** @brief 一个只读接触调平候选项及其约束。 */
struct ContactLevelingCandidate
{
    bool available{false};
    std::string status{"rejected"};
    std::string rejectionreason;
    double candidateangledeg{0.0};
    double baselinefirsthalfslabareamm2{0.0};
    double candidatefirsthalfslabareamm2{0.0};
    double contactareaimprovementmm2{0.0};
    double baselineheightmm{0.0};
    double candidateheightmm{0.0};
    double heightincreasemm{0.0};
    double baselinefootprintxmm{0.0};
    double candidatefootprintxmm{0.0};
    double footprintincreasemm{0.0};
    double sideenvelopedeltamm{0.0};
    bool positivezconstraintsatisfied{false};
    bool positiveyconstraintsatisfied{false};
    bool angleconstraintsatisfied{false};
    bool heightconstraintsatisfied{false};
    bool footprintconstraintsatisfied{false};
    int evaluatedcandidatecount{0};
};

/**
 * @brief 在不改变输入模型的前提下分析有界横滚候选项。
 * @param model 已导入、自动定向并落地的模型报告。
 * @param metricpolicy 冻结的 Stage 16 接触测量策略。
 * @param levelingpolicy 确定性有界搜索策略。
 * @return 最佳诊断候选项或稳定拒绝原因。
 */
ContactLevelingCandidate AnalyzeContactLeveling(
    const ModelReport& model,
    const ContactPostureMetricPolicy& metricpolicy = {},
    const ContactLevelingPolicy& levelingpolicy = {});

/**
 * @brief 应用诊断横滚角，并将复制模型落地到 Z=0。
 * @param model 已导入、自动定向并落地的源模型。
 * @param angledeg 绕 Y 正向长轴旋转的角度，单位为度。
 * @return 变换后的副本；源模型不被修改。
 */
ModelReport ApplyContactLevelingAngle(
    const ModelReport& model,
    double angledeg);

}  // namespace slicer_core
