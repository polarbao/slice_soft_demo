#pragma once

#include <QString>

struct ToolPaths
{
    QString repo_root;
    QString build_dir;
    QString slicer_cli;
    QString openvdb_slicer_cli;
    QString rip_reader;
    QString powershell;

    /**
     * @brief Resolve runtime tools for a repository or packaged application root.
     * @param repoRoot Repository root or packaged runtime root.
     * @return Resolved tool paths for the current build configuration.
     */
    static ToolPaths FromRepoRoot(const QString& repoRoot);
};
