#include "ProductionSliceRunSession.h"

#include "ProductionModeCatalog.h"

QStringList ProductionSliceRunSession::Begin(
    const ProductionSliceRunRequest& request)
{
    Invalidate();
    QStringList errors;
    const ProductionModeCapability* mode =
        ProductionModeCatalog::FindMode(request.mode);
    if (mode == nullptr)
    {
        errors.push_back(QStringLiteral("请求的生产切片模式无效。"));
    }
    if (request.mode == slicer_core::SlicePipelineMode::GlobalSurfaceShell)
    {
        const ProductionProfileCapability* profile =
            ProductionModeCatalog::FindProfile(request.profileid.toStdString());
        if (profile == nullptr || profile->mode != request.mode)
        {
            errors.push_back(
                QStringLiteral("全局纹理壳层缺少获准的 Production Profile。"));
        }
    }
    if (request.sessionid.trimmed().isEmpty())
    {
        errors.push_back(QStringLiteral("生产切片 sessionId 不能为空。"));
    }
    if (request.configpath.trimmed().isEmpty())
    {
        errors.push_back(QStringLiteral("生产切片生效配置路径不能为空。"));
    }
    if (request.packagedir.trimmed().isEmpty())
    {
        errors.push_back(QStringLiteral("生产切片输出包路径不能为空。"));
    }
    if (errors.isEmpty())
    {
        m_activeRequest = request;
    }
    return errors;
}

ProductionSliceRunCompletion ProductionSliceRunSession::Complete(
    const int exitCode)
{
    ProductionSliceRunCompletion completion;
    completion.success = exitCode == 0 && m_activeRequest.has_value();
    completion.fallbackapplied = false;
    if (completion.success)
    {
        completion.packagedirtoload = m_activeRequest->packagedir;
    }
    Invalidate();
    return completion;
}

void ProductionSliceRunSession::Invalidate()
{
    m_activeRequest.reset();
}

bool ProductionSliceRunSession::IsActive() const
{
    return m_activeRequest.has_value();
}
