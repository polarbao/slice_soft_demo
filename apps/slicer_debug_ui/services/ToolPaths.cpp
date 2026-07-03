#include "ToolPaths.h"

#include <QDir>

ToolPaths ToolPaths::fromRepoRoot(const QString& repo_root) {
    const QDir root(repo_root);
    ToolPaths paths;
    paths.repo_root = root.absolutePath();
    paths.build_dir = root.filePath("build");
#if defined(Q_OS_WIN)
    paths.slicer_cli = root.filePath("build/Debug/slicer_cli.exe");
    paths.openvdb_slicer_cli = root.filePath("build-openvdb-09p/Debug/slicer_cli.exe");
    paths.rip_reader = root.filePath("build/Debug/rip_reader_test.exe");
    paths.powershell = "powershell";
#else
    paths.slicer_cli = root.filePath("build/slicer_cli");
    paths.openvdb_slicer_cli = root.filePath("build-openvdb-09p/slicer_cli");
    paths.rip_reader = root.filePath("build/rip_reader_test");
    paths.powershell = "pwsh";
#endif
    return paths;
}
