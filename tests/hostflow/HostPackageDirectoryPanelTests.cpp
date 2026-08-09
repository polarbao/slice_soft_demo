#include "apps/slicer_ui_host_sim/HostPackageReviewPanel.h"

#include <QApplication>
#include <QDir>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTextStream errors(stderr);
    QTemporaryDir root;
    if (!Check(root.isValid(), QStringLiteral("H-D-05 临时目录不可用。"), errors))
    {
        return 2;
    }

    HostPackageReviewPanel panel;
    auto* openButton = panel.findChild<QPushButton*>(
        QStringLiteral("hostOpenPackageDirectoryButton"));
    if (!Check(openButton != nullptr,
               QStringLiteral("结果页缺少打开包目录按钮。"), errors)
        || !Check(!openButton->isEnabled(),
                  QStringLiteral("未加载生产包时按钮必须禁用。"), errors))
    {
        return 3;
    }

    const QString packageDirectory = QDir(root.path()).filePath(
        QStringLiteral("job-returned-package"));
    if (!Check(QDir().mkpath(packageDirectory),
               QStringLiteral("无法创建生产包 fixture。"), errors))
    {
        return 4;
    }

    QString requestedDirectory;
    QObject::connect(
        &panel,
        &HostPackageReviewPanel::SigOpenPackageDirectoryRequested,
        &panel,
        [&](const QString& directory)
        {
            requestedDirectory = directory;
        });
    hostpackagereview review;
    review.valid = true;
    review.packagedirectory = packageDirectory;
    panel.SetPackage(review);
    if (!Check(openButton->isEnabled(),
               QStringLiteral("有效生产包目录未启用打开入口。"), errors))
    {
        return 5;
    }
    openButton->click();
    if (!Check(requestedDirectory == packageDirectory,
               QStringLiteral("打开入口未原样返回作业 packageDir。"), errors))
    {
        return 6;
    }

    hostpackagereview missingReview;
    missingReview.valid = true;
    missingReview.packagedirectory = QDir(root.path()).filePath(
        QStringLiteral("missing-package"));
    panel.SetPackage(missingReview);
    if (!Check(!openButton->isEnabled(),
               QStringLiteral("缺失生产包目录必须禁用入口。"), errors))
    {
        return 7;
    }

    hostpackagereview invalidReview;
    invalidReview.packagedirectory = packageDirectory;
    panel.SetPackage(invalidReview);
    if (!Check(!openButton->isEnabled(),
               QStringLiteral("严格校验失败时必须禁用入口。"), errors))
    {
        return 8;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HD05_PASS packageDir=job_returned_exact failClosed=2"
        << Qt::endl;
    return 0;
}
