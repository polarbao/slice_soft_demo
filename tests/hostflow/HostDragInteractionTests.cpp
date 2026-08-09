#include "HostModelImportWorkflow.h"
#include "ModuleClient.h"
#include "MoveOptimizationPolicy.h"
#include "SceneInteractionController.h"
#include "TopViewCanvasWidget.h"
#include "render/TopViewRenderPolicy.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QTextStream>

#include <algorithm>
#include <cstdlib>

namespace
{
void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "H-D-03 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

QPointF FindInstancePoint(
    const TopViewFrame& frame,
    const QSize& imageSize,
    const QString& instanceId)
{
    int minimumX = imageSize.width();
    int minimumY = imageSize.height();
    int maximumX = -1;
    int maximumY = -1;
    for (int y = 0; y < imageSize.height(); y += 4)
    {
        for (int x = 0; x < imageSize.width(); x += 4)
        {
            if (TopViewRenderPolicy::PickInstance(
                    frame, imageSize, QPointF(x, y)) == instanceId)
            {
                minimumX = (std::min)(minimumX, x);
                minimumY = (std::min)(minimumY, y);
                maximumX = (std::max)(maximumX, x);
                maximumY = (std::max)(maximumY, y);
            }
        }
    }
    if (maximumX < minimumX || maximumY < minimumY)
    {
        return {};
    }
    return QPointF(
        (minimumX + maximumX) * 0.5,
        (minimumY + maximumY) * 0.5);
}

void SendMouseEvent(
    QWidget* target,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons)
{
    QMouseEvent event(type, position, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString evidenceRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--evidence-root"));
    Require(!modulePath.isEmpty() && !repositoryRoot.isEmpty(),
            QStringLiteral("--module and --repo-root are required"));

    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "model/obj/小马物语/小马物语小指/MF_Xiao_ma_Xiaozhi_ty03.obj"));
    Require(QFileInfo::exists(modelPath),
            QStringLiteral("production textured nail model is missing"));

    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    HostModelImportWorkflow workflow(client);
    hostmodelimportresult importResult;
    Require(workflow.ImportModel(modelPath, &importResult, &error), error);

    TopViewRenderPolicy renderer(client);
    TopViewFrame frame;
    Require(renderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &frame, &error),
            error);
    const QSize imageSize(1000, 620);
    const QImage image = renderer.Render(frame, imageSize);
    Require(!image.isNull(), QStringLiteral("initial top image is null"));
    const QPointF dragStart = FindInstancePoint(
        frame, imageSize, importResult.instanceid);
    Require(!dragStart.isNull(),
            QStringLiteral("imported instance is not pickable"));

    SceneInteractionController controller(client);
    MoveOptimizationPolicy movePolicy;
    TopViewCanvasWidget canvas;
    canvas.resize(imageSize);
    canvas.SetImage(image);
    canvas.show();
    application.processEvents();

    QPointF dragStartWorld;
    quint64 callCountAtDragStart{0U};
    quint64 callCountBeforeCommit{0U};
    int localUpdateCount{0};
    bool committed{false};
    canvas.SetModelDragCallbacks(
        [&](const QPointF& imagePoint)
        {
            const QString picked = TopViewRenderPolicy::PickInstance(
                frame, imageSize, imagePoint);
            if (picked != importResult.instanceid
                || !TopViewRenderPolicy::ImageToWorld(
                    frame, imageSize, imagePoint, &dragStartWorld)
                || !controller.Attach(
                    workflow.SceneHandle(), workflow.SceneRevision())
                || !controller.BeginTransient(picked)
                || !movePolicy.Begin(frame, picked))
            {
                return false;
            }
            callCountAtDragStart = client.CallCount();
            return true;
        },
        [&](const QPointF& imagePoint)
        {
            QPointF worldPoint;
            Require(TopViewRenderPolicy::ImageToWorld(
                        frame, imageSize, imagePoint, &worldPoint),
                    QStringLiteral("pointer could not map to build XY"));
            const QPointF delta = worldPoint - dragStartWorld;
            Require(controller.UpdateTransientTranslation(
                        delta.x(), delta.y(), 0.0)
                        && movePolicy.UpdateTranslation(
                            delta.x(), delta.y(), 0.0),
                    QStringLiteral("local translation update failed"));
            const QImage preview = renderer.Render(
                movePolicy.Frame(), imageSize);
            Require(!preview.isNull(),
                    QStringLiteral("local preview render failed"));
            canvas.UpdateImage(preview);
            ++localUpdateCount;
            Require(client.CallCount() == callCountAtDragStart,
                    QStringLiteral("UI-M1 pointer move crossed DLL"));
        },
        [&]()
        {
            callCountBeforeCommit = client.CallCount();
            Require(controller.CommitTransient(&error)
                        == CommitOutcome::Committed,
                    error);
            Require(workflow.AdoptSceneState(
                        controller.SceneHandle(),
                        controller.SceneRevision(),
                        &error),
                    error);
            Require(movePolicy.AcceptCommit(
                        controller.SceneRevision(),
                        controller.ViewDataIdentity()),
                    QStringLiteral("local frame did not adopt Commit"));
            frame = movePolicy.Frame();
            canvas.UpdateImage(renderer.Render(frame, imageSize));
            committed = true;
        });

    const quint64 revisionBeforeDrag = workflow.SceneRevision();
    client.ResetCallCount();
    SendMouseEvent(
        &canvas, QEvent::MouseButtonPress, dragStart,
        Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 12; ++step)
    {
        SendMouseEvent(
            &canvas,
            QEvent::MouseMove,
            dragStart + QPointF(step * 1.5, step * -0.5),
            Qt::NoButton,
            Qt::LeftButton);
    }
    SendMouseEvent(
        &canvas,
        QEvent::MouseButtonRelease,
        dragStart + QPointF(18.0, -6.0),
        Qt::LeftButton,
        Qt::NoButton);
    application.processEvents();

    Require(localUpdateCount == 12,
            QStringLiteral("canvas did not deliver every local move"));
    Require(callCountBeforeCommit == 0U,
            QStringLiteral("transient lane called the module before release"));
    Require(committed,
            QStringLiteral("release did not perform an atomic Commit"));
    Require(workflow.SceneRevision() == revisionBeforeDrag + 1U,
            QStringLiteral("release did not advance revision exactly once"));
    Require(controller.SnapshotReadCount() == 0U,
            QStringLiteral("normal Commit appended a forbidden snapshot"));
    Require(!controller.ViewDataIdentity().isEmpty(),
            QStringLiteral("Commit did not return ViewData identity"));

    if (!evidenceRoot.isEmpty())
    {
        Require(QDir().mkpath(evidenceRoot),
                QStringLiteral("evidence directory creation failed"));
        Require(canvas.grab().save(QDir(evidenceRoot).filePath(
                    QStringLiteral("hostflow_hd03_drag_commit.png"))),
                QStringLiteral("drag evidence save failed"));
    }

    QTextStream(stdout)
        << "HOSTFLOW_H_D_03_DRAG_PASS updates=" << localUpdateCount
        << " transientCalls=" << callCountBeforeCommit
        << " revision=" << workflow.SceneRevision()
        << " commitCalls=" << client.CallCount()
        << Qt::endl;
    return 0;
}
