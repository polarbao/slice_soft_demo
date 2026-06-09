#pragma once

#include <QString>
#include <QStringList>

struct PackageSummary {
    QString package_dir;
    QString manifest_path;
    QStringList report_paths;
    QStringList preview_paths;
    QStringList warnings;
};

class PackageLoader {
public:
    PackageSummary load(const QString& package_dir) const;
};

