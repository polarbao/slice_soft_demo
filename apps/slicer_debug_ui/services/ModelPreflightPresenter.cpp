#include "ModelPreflightPresenter.h"

#include <QHash>
#include <QSet>

namespace
{

QString StatusText(const slicer_core::ModelPreflightStatus status)
{
    switch (status)
    {
    case slicer_core::ModelPreflightStatus::NotRun:
    case slicer_core::ModelPreflightStatus::Pending:
        return QStringLiteral("待检测");
    case slicer_core::ModelPreflightStatus::Running:
        return QStringLiteral("检测中");
    case slicer_core::ModelPreflightStatus::Passed:
        return QStringLiteral("检测通过");
    case slicer_core::ModelPreflightStatus::Warning:
        return QStringLiteral("检测有警告");
    case slicer_core::ModelPreflightStatus::Blocked:
        return QStringLiteral("检测阻断");
    case slicer_core::ModelPreflightStatus::Stale:
        return QStringLiteral("检测结果已过期");
    case slicer_core::ModelPreflightStatus::Cancelled:
        return QStringLiteral("检测已取消");
    }
    return QStringLiteral("待检测");
}

QString ModeText(const slicer_core::ModelPreflightPipelineMode mode)
{
    return mode == slicer_core::ModelPreflightPipelineMode::Legacy
        ? QStringLiteral("传统切片")
        : QStringLiteral("全局表面壳层（实验）");
}

QString AdmissionText(const slicer_core::ModelPreflightAdmissionStatus status)
{
    switch (status)
    {
    case slicer_core::ModelPreflightAdmissionStatus::Passed:
        return QStringLiteral("允许进入当前模式");
    case slicer_core::ModelPreflightAdmissionStatus::Warning:
        return QStringLiteral("需要确认风险");
    case slicer_core::ModelPreflightAdmissionStatus::Blocked:
        return QStringLiteral("禁止进入当前模式");
    }
    return QStringLiteral("禁止进入当前模式");
}

QString SeverityText(const slicer_core::ModelPreflightIssueSeverity severity)
{
    switch (severity)
    {
    case slicer_core::ModelPreflightIssueSeverity::Info:
        return QStringLiteral("信息");
    case slicer_core::ModelPreflightIssueSeverity::Warning:
        return QStringLiteral("警告");
    case slicer_core::ModelPreflightIssueSeverity::Error:
        return QStringLiteral("错误");
    }
    return QStringLiteral("信息");
}

const slicer_core::ModeAdmissionResult& AdmissionForMode(
    const slicer_core::ModelPreflightResult& result,
    const slicer_core::ModelPreflightPipelineMode mode)
{
    return mode == slicer_core::ModelPreflightPipelineMode::Legacy
        ? result.legacyAdmission
        : result.globalAdmission;
}

}  // namespace

ModelPreflightPresentation ModelPreflightPresenter::Present(
    const slicer_core::ModelPreflightExecutionResult& execution,
    const slicer_core::ModelPreflightPipelineMode mode)
{
    ModelPreflightPresentation presentation;
    presentation.state = StatusText(execution.result.status);
    presentation.mode = ModeText(mode);
    presentation.running = execution.result.status
        == slicer_core::ModelPreflightStatus::Running;
    presentation.cancancel = presentation.running;
    presentation.canrecheck = !presentation.running;

    const slicer_core::ModeAdmissionResult& admission =
        AdmissionForMode(execution.result, mode);
    presentation.admission = AdmissionText(admission.status);
    presentation.detail = QStringLiteral(
                              "generation=%1，cache=%2，fast=%3，full=%4")
                              .arg(execution.generation)
                              .arg(execution.cacheHit ? QStringLiteral("命中")
                                                      : QStringLiteral("未命中"))
                              .arg(execution.fastComplete ? QStringLiteral("完成")
                                                          : QStringLiteral("未完成"))
                              .arg(execution.fullComplete ? QStringLiteral("完成")
                                                          : QStringLiteral("未完成"));

    for (const slicer_core::ModelPreflightIssue& issue : execution.result.issues)
    {
        ModelPreflightIssuePresentation row;
        row.severity = SeverityText(issue.severity);
        row.summary = IssueSummary(issue.code);
        row.count = QString::number(issue.count);
        row.recommendation = IssueRecommendation(issue.code);
        row.code = QString::fromStdString(issue.code);
        presentation.issues.push_back(row);
    }

    if (execution.result.status == slicer_core::ModelPreflightStatus::Running
        || execution.result.status == slicer_core::ModelPreflightStatus::Pending)
    {
        return presentation;
    }

    QSet<QString> listedCodes;
    for (const ModelPreflightIssuePresentation& row : presentation.issues)
    {
        listedCodes.insert(row.code);
    }
    const auto appendAdmissionCode =
        [&](const std::string& code, const QString& severity)
    {
        const QString qcode = QString::fromStdString(code);
        if (listedCodes.contains(qcode))
        {
            return;
        }
        ModelPreflightIssuePresentation row;
        row.severity = severity;
        row.summary = IssueSummary(code);
        row.count = QStringLiteral("1");
        row.recommendation = IssueRecommendation(code);
        row.code = qcode;
        presentation.issues.push_back(row);
        listedCodes.insert(qcode);
    };
    for (const std::string& code : admission.blockerCodes)
    {
        appendAdmissionCode(code, QStringLiteral("错误"));
    }
    for (const std::string& code : admission.warningCodes)
    {
        appendAdmissionCode(code, QStringLiteral("警告"));
    }
    return presentation;
}

QString ModelPreflightPresenter::IssueSummary(const std::string& code)
{
    static const QHash<QString, QString> summaries{
        {QStringLiteral("E_12E_PREFLIGHT_NOT_RUN"), QStringLiteral("尚未完成模型预检")},
        {QStringLiteral("E_12E_PREFLIGHT_STALE"), QStringLiteral("模型预检结果已过期")},
        {QStringLiteral("E_12E_PREFLIGHT_CANCELLED"), QStringLiteral("模型预检已取消")},
        {QStringLiteral("E_12E_PREFLIGHT_IMPORT_INVALID"), QStringLiteral("模型或配置无法导入")},
        {QStringLiteral("E_12E_PREFLIGHT_RESOURCE_MISSING"), QStringLiteral("模型引用的材质或纹理资源缺失")},
        {QStringLiteral("E_12E_PREFLIGHT_NON_FINITE_GEOMETRY"), QStringLiteral("模型包含非有限坐标")},
        {QStringLiteral("E_12E_PREFLIGHT_AUDIT_INCOMPLETE"), QStringLiteral("完整拓扑审计未完成")},
        {QStringLiteral("E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED"), QStringLiteral("拓扑不满足全局表面壳层模式")},
        {QStringLiteral("E_12E_PREFLIGHT_BACKEND_UNAVAILABLE"), QStringLiteral("OpenVDB 候选运行环境不可用")},
        {QStringLiteral("MESH_SELF_INTERSECTION_CONFIRMED"), QStringLiteral("检测到模型自相交")},
        {QStringLiteral("MESH_BOUNDARY_EDGES"), QStringLiteral("检测到开放边界")},
        {QStringLiteral("MESH_NON_MANIFOLD_EDGES"), QStringLiteral("检测到非流形边")},
        {QStringLiteral("MESH_DEGENERATE_TRIANGLES"), QStringLiteral("检测到退化三角形")},
        {QStringLiteral("MESH_DUPLICATE_FACES"), QStringLiteral("检测到重复面")},
        {QStringLiteral("MESH_OPPOSITE_DUPLICATE_FACES"), QStringLiteral("检测到反向重复面")},
        {QStringLiteral("MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT"), QStringLiteral("重复面存在材质或纹理属性冲突")},
        {QStringLiteral("MESH_LOCAL_WINDING_INCONSISTENCY"), QStringLiteral("检测到局部绕序不一致")},
    };
    const QString key = QString::fromStdString(code);
    return summaries.value(
        key,
        QStringLiteral("检测到未识别问题，已按安全策略处理"));
}

QString ModelPreflightPresenter::IssueRecommendation(const std::string& code)
{
    static const QHash<QString, QString> recommendations{
        {QStringLiteral("E_12E_PREFLIGHT_NOT_RUN"), QStringLiteral("请先运行模型预检。")},
        {QStringLiteral("E_12E_PREFLIGHT_STALE"), QStringLiteral("模型、资源或配置已变化，请重新检测。")},
        {QStringLiteral("E_12E_PREFLIGHT_CANCELLED"), QStringLiteral("确认输入未变化后重新检测。")},
        {QStringLiteral("E_12E_PREFLIGHT_IMPORT_INVALID"), QStringLiteral("检查模型格式、配置路径和文件读取权限。")},
        {QStringLiteral("E_12E_PREFLIGHT_RESOURCE_MISSING"), QStringLiteral("补齐同目录 MTL/贴图，或明确配置资源缺失策略。")},
        {QStringLiteral("E_12E_PREFLIGHT_NON_FINITE_GEOMETRY"), QStringLiteral("在建模软件中清理 NaN/Inf 顶点后重新导出。")},
        {QStringLiteral("E_12E_PREFLIGHT_AUDIT_INCOMPLETE"), QStringLiteral("提高审计预算或降低模型复杂度，未完整审计前禁止放行。")},
        {QStringLiteral("E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED"), QStringLiteral("全局模式要求 strict closed；请先完成可审计修复。")},
        {QStringLiteral("E_12E_PREFLIGHT_BACKEND_UNAVAILABLE"), QStringLiteral("构建并部署 OpenVDB ON 候选工具及其运行时依赖。")},
        {QStringLiteral("MESH_SELF_INTERSECTION_CONFIRMED"), QStringLiteral("该问题需要外部重建或可审计修复，不能自动忽略。")},
        {QStringLiteral("MESH_BOUNDARY_EDGES"), QStringLiteral("闭合开放边界，并复核修复后的 UV 与材料属性。")},
        {QStringLiteral("MESH_NON_MANIFOLD_EDGES"), QStringLiteral("拆分或重建非流形连接后重新执行完整审计。")},
        {QStringLiteral("MESH_DEGENERATE_TRIANGLES"), QStringLiteral("删除或重建零面积面，并保留属性映射证据。")},
        {QStringLiteral("MESH_DUPLICATE_FACES"), QStringLiteral("去除重复面并复核纹理、材质归属。")},
        {QStringLiteral("MESH_OPPOSITE_DUPLICATE_FACES"), QStringLiteral("去除反向重复面并统一局部法线。")},
        {QStringLiteral("MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT"), QStringLiteral("先人工确认冲突面的正确材质和 UV，禁止无依据合并。")},
        {QStringLiteral("MESH_LOCAL_WINDING_INCONSISTENCY"), QStringLiteral("统一面绕序和法线后重新执行 strict 检测。")},
    };
    const QString key = QString::fromStdString(code);
    return recommendations.value(
        key,
        QStringLiteral("请保留稳定错误码并联系开发人员复核；未知错误不会静默放行。"));
}
