#include "HostMainWindow.h"

#include "MoveOptimizationPolicy.h"
#include "SceneInteractionController.h"
#include "ThreeDCanvasWidget.h"
#include "TopViewCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QImage>
#include <QLabel>

void HostMainWindow::InitializeViewWorkspace()
{
    m_workspace->SetMode(m_viewSettings->DefaultViewMode());
    m_workspace->SetThreeDProjection(m_viewSettings->ThreeDProjection());
    m_workspace->ThreeDCanvas()->SetCameraChangedCallback([this]()
    {
        RenderThreeDView();
    });
    m_workspace->TopCanvas()->SetModelDragCallbacks(
        [this](const QPointF& imagePoint)
        {
            return BeginTopViewDrag(imagePoint);
        },
        [this](const QPointF& imagePoint)
        {
            UpdateTopViewDrag(imagePoint);
        },
        [this]()
        {
            FinishTopViewDrag();
        });
}

void HostMainWindow::RefreshTopView()
{
    const quint64 sceneHandle = m_importWorkflow->SceneHandle();
    const quint64 sceneRevision = m_importWorkflow->SceneRevision();
    if (!m_client.IsOpen() || sceneHandle == 0 || sceneRevision == 0)
    {
        m_topViewFrame.reset();
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视刷新缺少有效场景身份。"));
        return;
    }
    if (!m_topViewPolicy)
    {
        m_topViewPolicy = std::make_unique<TopViewRenderPolicy>(m_client);
    }

    auto frame = std::make_unique<TopViewFrame>();
    QString error;
    if (!m_topViewPolicy->Refresh(
            sceneHandle, sceneRevision, frame.get(), &error))
    {
        m_topViewFrame.reset();
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视视图刷新失败：%1").arg(error));
        return;
    }
    const QImage image = m_topViewPolicy->Render(
        *frame, m_workspace->TopRenderSize());
    if (image.isNull())
    {
        m_workspace->ClearTopImage();
        m_workspace->ShowViewError(
            QStringLiteral("俯视视图渲染失败，未生成完整图像。"));
        return;
    }
    m_topViewFrame = std::move(frame);
    m_workspace->SetTopImage(image);
    m_workspace->ShowViewError({});
}

bool HostMainWindow::BeginTopViewDrag(const QPointF& imagePoint)
{
    if (!m_topViewFrame || !m_topViewPolicy)
    {
        return false;
    }
    const QSize renderSize = m_workspace->TopRenderSize();
    const QString instanceId = TopViewRenderPolicy::PickInstance(
        *m_topViewFrame, renderSize, imagePoint);
    QPointF worldPoint;
    if (instanceId.isEmpty()
        || !TopViewRenderPolicy::ImageToWorld(
            *m_topViewFrame, renderSize, imagePoint, &worldPoint))
    {
        return false;
    }
    if (!m_interactionController)
    {
        m_interactionController =
            std::make_unique<SceneInteractionController>(m_client);
        m_movePolicy = std::make_unique<MoveOptimizationPolicy>();
    }
    if (!m_interactionController->Attach(
            m_importWorkflow->SceneHandle(),
            m_importWorkflow->SceneRevision())
        || !m_interactionController->BeginTransient(instanceId)
        || !m_movePolicy->Begin(*m_topViewFrame, instanceId))
    {
        return false;
    }
    m_modelListPanel->SelectInstance(instanceId);
    m_dragStartWorld = worldPoint;
    m_dragCallCount = m_client.CallCount();
    return true;
}

void HostMainWindow::UpdateTopViewDrag(const QPointF& imagePoint)
{
    if (!m_interactionController || !m_movePolicy
        || !m_interactionController->HasTransient())
    {
        return;
    }
    QPointF worldPoint;
    if (!TopViewRenderPolicy::ImageToWorld(
            *m_topViewFrame,
            m_workspace->TopRenderSize(),
            imagePoint,
            &worldPoint))
    {
        return;
    }
    const QPointF delta = worldPoint - m_dragStartWorld;
    if (!m_interactionController->UpdateTransientTranslation(
            delta.x(), delta.y(), 0.0)
        || !m_movePolicy->UpdateTranslation(delta.x(), delta.y(), 0.0)
        || m_client.CallCount() != m_dragCallCount)
    {
        m_interactionController->DiscardTransient();
        m_movePolicy->Rollback();
        RenderTransientTopView();
        m_workspace->ShowViewError(QStringLiteral(
            "拖拽瞬态阶段越过模块边界，已回滚本地预览。"));
        return;
    }
    RenderTransientTopView();
}

void HostMainWindow::FinishTopViewDrag()
{
    if (!m_interactionController || !m_movePolicy
        || !m_interactionController->HasTransient())
    {
        return;
    }
    QString error;
    const CommitOutcome outcome =
        m_interactionController->CommitTransient(&error);
    if (outcome == CommitOutcome::Failed
        || !m_importWorkflow->AdoptSceneState(
            m_interactionController->SceneHandle(),
            m_interactionController->SceneRevision(),
            &error))
    {
        m_movePolicy->Rollback();
        RefreshTopView();
        m_workspace->ShowViewError(
            QStringLiteral("模型拖拽提交失败：%1").arg(error));
        return;
    }
    if (outcome == CommitOutcome::StaleRecovered)
    {
        m_movePolicy->Rollback();
        RefreshTopView();
        RefreshThreeDView();
        RefreshTextureWhitePreflight();
        m_workspace->ShowViewError(error);
        return;
    }
    if (!m_movePolicy->AcceptCommit(
            m_interactionController->SceneRevision(),
            m_interactionController->ViewDataIdentity()))
    {
        m_movePolicy->Rollback();
        RefreshTopView();
    }
    else
    {
        *m_topViewFrame = m_movePolicy->Frame();
        RenderTransientTopView();
    }
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(),
        m_importWorkflow->SceneRevision());
    m_statusLabel->setText(QStringLiteral(
        "模型拖拽已提交 · revision=%1 · 碰撞=%2 · 越界=%3 · ABI 调用 %4 次")
        .arg(m_importWorkflow->SceneRevision())
        .arg(m_interactionController->CollisionCount())
        .arg(m_interactionController->OutOfBoundsCount())
        .arg(m_client.CallCount()));
    RefreshThreeDView();
    RefreshTextureWhitePreflight();
}

void HostMainWindow::RenderTransientTopView()
{
    if (!m_topViewPolicy || !m_topViewFrame)
    {
        return;
    }
    const TopViewFrame& frame = m_movePolicy && m_movePolicy->IsActive()
        ? m_movePolicy->Frame() : *m_topViewFrame;
    const QImage image = m_topViewPolicy->Render(
        frame, m_workspace->TopRenderSize());
    if (!image.isNull())
    {
        m_workspace->UpdateTopImage(image);
    }
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

void HostMainWindow::RefreshSceneViews()
{
    RefreshTextureWhitePreflight();
    if (m_importWorkflow->InstanceCount() == 0)
    {
        m_topViewFrame.reset();
        m_threeDFrame.reset();
        m_workspace->ClearTopImage();
        m_workspace->ClearThreeDImage();
        m_workspace->ShowViewError({});
        return;
    }
    RefreshTopView();
    RefreshThreeDView();
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
