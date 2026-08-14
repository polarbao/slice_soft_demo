#pragma once

#include "slicer_core/model.h"

#include <cstddef>
#include <string>

namespace slicer_core
{

/** @brief Stage 16 接触姿态基线的冻结测量参数。 */
struct ContactPostureMetricPolicy
{
    double sidebandfraction{0.125};
    double firstslabfraction{0.5};
    double layerthicknessmm{0.038};
    double maximumabsolutecandidateangledeg{12.0};
    double requiredpositivezenvelopedeltamm{0.0};
    double requiredpositiveytipdeltamm{0.0};
};

/** @brief 一个已定向甲片模型的只读接触与方向测量。 */
struct ContactPostureMetrics
{
    bool valid{false};
    std::string rejectionreason;
    double longaxislengthmm{0.0};
    double transversespanmm{0.0};
    double leftbandminimumzmm{0.0};
    double rightbandminimumzmm{0.0};
    double sideenvelopedeltamm{0.0};
    double centertosideenvelopedeltamm{0.0};
    double firsthalfslabareamm2{0.0};
    double firstslabareamm2{0.0};
    double secondslabareamm2{0.0};
    double candidateangledeg{0.0};
    double positiveytipwidthdeltamm{0.0};
    std::size_t leftbandvertexcount{0U};
    std::size_t rightbandvertexcount{0U};
    std::size_t centerbandvertexcount{0U};
    bool candidateanglewithinlimit{false};
    bool positivezconstraintsatisfied{false};
    bool positiveyconstraintsatisfied{false};
};

/**
 * @brief 在不改变几何的前提下测量当前定向姿态。
 * @param model 已导入并自动定向的模型报告。
 * @param policy 冻结的 Stage 16 测量策略。
 * @return 接触、角度和方向指标，或明确拒绝原因。
 */
ContactPostureMetrics MeasureContactPosture(
    const ModelReport& model,
    const ContactPostureMetricPolicy& policy = {});

}  // namespace slicer_core
