#include "HostMainWindow.h"

#include "ThreeDCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QImage>

void HostMainWindow::InitializeViewWorkspace()
{
    m_workspace->SetMode(m_viewSettings->DefaultViewMode());
    m_workspace->SetThreeDProjection(m_viewSettings->ThreeDProjection());
    m_workspace->ThreeDCanvas()->SetCameraChangedCallback([this]()
    {
        RenderThreeDView();
    });
}

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

void HostMainWindow::RefreshThreeDView()
{
    const quint64 sceneHandle = m_importWorkflow->SceneHandle();
    const quint64 sceneRevision = m_importWorkflow->SceneRevision();
    if (!m_client.IsOpen() || sceneHandle == 0U || sceneRevision == 0U)
    {
        m_threeDFrame.reset();
        m_workspace->ClearThreeDImage();
        m_workspace->ShowViewError(
            QStringLiteral("3D 刷新缺少有效场景身份。"));
        return;
    }
    if (!m_threeDBackend)
    {
        m_threeDBackend = std::make_unique<CpuRasterBackend>();
        m_threeDPolicy = std::make_unique<SceneRenderPolicy>(
            m_client, *m_threeDBackend);
    }

    auto frame = std::make_unique<ThreeDFrame>();
    QString error;
    if (!m_threeDPolicy->Refresh(
            sceneHandle, sceneRevision, frame.get(), &error))
    {
        m_threeDFrame.reset();
        m_workspace->ClearThreeDImage();
        m_workspace->ShowViewError(
            QStringLiteral("3D 视图刷新失败：%1").arg(error));
        return;
    }
    m_threeDFrame = std::move(frame);
    m_workspace->SetThreeDSceneBounds(m_threeDFrame->worldBounds);
    RenderThreeDView();
}

void HostMainWindow::RenderThreeDView()
{
    if (!m_threeDFrame || !m_threeDPolicy)
    {
        return;
    }
    const quint64 callCountBefore = m_client.CallCount();
    const QSize size = m_workspace->ThreeDRenderSize();
    slicer::render::ImageOut output;
    const slicer::render::FrameResult result = m_threeDPolicy->Render(
        *m_threeDFrame,
        m_workspace->ThreeDCanvas()->Camera(),
        static_cast<std::uint32_t>(size.width()),
        static_cast<std::uint32_t>(size.height()),
        &output);
    if (!result.ok || output.widthPx == 0U || output.heightPx == 0U
        || output.rgba8.size() != static_cast<std::size_t>(
            output.widthPx) * output.heightPx * 4U)
    {
        m_workspace->ClearThreeDImage();
        m_workspace->ShowViewError(QStringLiteral(
            "3D 本地渲染失败：%1")
            .arg(QString::fromStdString(result.errorCode)));
        return;
    }
    if (m_client.CallCount() != callCountBefore)
    {
        m_workspace->ClearThreeDImage();
        m_workspace->ShowViewError(QStringLiteral(
            "3D 相机操作越过了模块边界，已停止显示。"));
        return;
    }
    const QImage image(
        output.rgba8.data(),
        static_cast<int>(output.widthPx),
        static_cast<int>(output.heightPx),
        static_cast<int>(output.widthPx * 4U),
        QImage::Format_RGBA8888);
    m_workspace->SetThreeDImage(image.copy());
    m_workspace->ShowViewError({});
}
