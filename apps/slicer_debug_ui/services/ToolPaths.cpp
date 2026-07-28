#include "ToolPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace
{

QString FirstExistingPath(const QStringList& candidates)
{
    for (const QString& candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
        {
            return QDir::cleanPath(candidate);
        }
    }
    return candidates.isEmpty() ? QString{} : QDir::cleanPath(candidates.constFirst());
}

}  // namespace

ToolPaths ToolPaths::FromRepoRoot(const QString& repoRoot)
{
    const QDir root(repoRoot);
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    ToolPaths paths;
    paths.repo_root = root.absolutePath();
#if defined(Q_OS_WIN)
#if defined(_DEBUG)
    const QString config = QStringLiteral("Debug");
#else
    const QString config = QStringLiteral("Release");
#endif
    const QString unifiedBuildDir = root.filePath(QStringLiteral("build-slicesoft/%1").arg(config));
    const QString legacyBuildDir = root.filePath(QStringLiteral("build/%1").arg(config));
    paths.slicer_cli = FirstExistingPath({
        applicationDir.filePath("slicer_cli.exe"),
        QDir(legacyBuildDir).filePath("slicer_cli.exe"),
        QDir(unifiedBuildDir).filePath("slicer_cli.exe"),
    });
    paths.rip_reader = FirstExistingPath({
        applicationDir.filePath("rip_reader_test.exe"),
        QDir(legacyBuildDir).filePath("rip_reader_test.exe"),
        QDir(unifiedBuildDir).filePath("rip_reader_test.exe"),
    });
    paths.openvdb_slicer_cli = root.filePath(
        QStringLiteral("build-openvdb-09p/%1/slicer_cli.exe").arg(config));
    paths.build_dir = QFileInfo(paths.slicer_cli).absolutePath();
    paths.powershell = "powershell";
#else
    paths.build_dir = root.filePath("build");
    paths.slicer_cli = root.filePath("build/slicer_cli");
    paths.openvdb_slicer_cli = root.filePath("build-openvdb-09p/slicer_cli");
    paths.rip_reader = root.filePath("build/rip_reader_test");
    paths.powershell = "pwsh";
#endif
    return paths;
}
