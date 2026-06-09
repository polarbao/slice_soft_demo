#pragma once

#include <QString>

struct ToolPaths {
    QString repo_root;
    QString build_dir;
    QString slicer_cli;
    QString rip_reader;
    QString powershell;

    static ToolPaths fromRepoRoot(const QString& repo_root);
};

