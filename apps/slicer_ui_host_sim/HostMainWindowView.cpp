#include "HostMainWindow.h"

#include "ViewWorkspaceWidget.h"
#include "render/TopViewRenderPolicy.h"

#include <QImage>

void HostMainWindow::RefreshTopView()
{
    const quint64 sceneHandle = m_importWorkflow->SceneHandle();
    const quint64 sceneRevision = m_importWorkflow->SceneRevision();
    if (!m_client.IsOpen() || sceneHandle == 0 || sceneRevision == 0)
    {
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视刷新缺少有效场景身份。"));
        return;
    }
    if (!m_topViewPolicy)
    {
        m_topViewPolicy = std::make_unique<TopViewRenderPolicy>(m_client);
    }

    TopViewFrame frame;
    QString error;
    if (!m_topViewPolicy->Refresh(
            sceneHandle, sceneRevision, &frame, &error))
    {
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视视图刷新失败：%1").arg(error));
        return;
    }
    const QImage image = m_topViewPolicy->Render(
        frame, m_workspace->TopRenderSize());
    if (image.isNull())
    {
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视视图渲染失败，未生成完整图像。"));
        return;
    }
    m_workspace->SetTopImage(image);
    m_workspace->ShowViewError({});
}
