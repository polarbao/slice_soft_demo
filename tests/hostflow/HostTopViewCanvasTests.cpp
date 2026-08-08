#include "HostModelImportWorkflow.h"
#include "ModuleClient.h"
#include "TopViewCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "render/TopViewRenderPolicy.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cstdlib>

namespace
{
void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "H-D-01 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

int CountColorfulPixels(const QImage& image)
{
    int count{0};
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            const int minimum = (std::min)(
                {qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int maximum = (std::max)(
                {qRed(pixel), qGreen(pixel), qBlue(pixel)});
            count += maximum - minimum > 35 ? 1 : 0;
        }
    }
    return count;
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
    Require(importResult.trianglecount == 11680U
            && importResult.hasuv
            && workflow.SceneHandle() != 0U
            && workflow.SceneRevision() != 0U,
            QStringLiteral("production model import identity is incomplete"));

    TopViewRenderPolicy renderer(client);
    TopViewFrame frame;
    Require(renderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &frame, &error),
            error);
    Require(frame.instances.size() == 1
            && !frame.instances.front().surfacePreview.isNull(),
            QStringLiteral("top ViewData did not provide a surface preview"));
    const QImage image = renderer.Render(frame, QSize(1000, 620));
    Require(!image.isNull() && CountColorfulPixels(image) > 0,
            QStringLiteral("top ViewData did not render to QImage"));

    ViewWorkspaceWidget workspace;
    workspace.resize(1100, 700);
    workspace.SetMode(HostViewMode::Top);
    workspace.SetTopImage(image);
    workspace.show();
    application.processEvents();
    TopViewCanvasWidget* canvas = workspace.TopCanvas();
    Require(canvas != nullptr && canvas->HasImage(),
            QStringLiteral("visible host canvas did not retain the image"));

    client.ResetCallCount();
    canvas->ZoomAt(1.25, QPointF(500.0, 310.0));
    canvas->PanBy(QPointF(18.0, -9.0));
    application.processEvents();
    Require(client.CallCount() == 0U,
            QStringLiteral("local canvas navigation crossed the DLL boundary"));
    canvas->ResetView();
    application.processEvents();

    if (!evidenceRoot.isEmpty())
    {
        Require(QDir().mkpath(evidenceRoot),
                QStringLiteral("evidence directory creation failed"));
        const QString screenshotPath = QDir(evidenceRoot).filePath(
            QStringLiteral("hostflow_hd01_real_model_top_view.png"));
        Require(workspace.grab().save(screenshotPath),
                QStringLiteral("visible top-view screenshot save failed"));
    }

    QTextStream(stdout)
        << "HOSTFLOW_H_D_01_TOP_CANVAS_PASS triangles="
        << importResult.trianglecount
        << " previewCache=" << renderer.CachedPreviewCount()
        << " localNavigationCalls=" << client.CallCount()
        << Qt::endl;
    return 0;
}
