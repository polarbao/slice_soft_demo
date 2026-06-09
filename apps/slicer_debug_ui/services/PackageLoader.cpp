#include "PackageLoader.h"

#include <QDir>
#include <QFileInfo>

PackageSummary PackageLoader::load(const QString& package_dir) const {
    PackageSummary summary;
    summary.package_dir = QFileInfo(package_dir).absoluteFilePath();
    const QDir package(summary.package_dir);
    if (!package.exists()) {
        summary.warnings.push_back("package directory does not exist: " + summary.package_dir);
        return summary;
    }

    const QString manifest = package.filePath("manifest.json");
    if (QFileInfo::exists(manifest)) {
        summary.manifest_path = manifest;
    } else {
        summary.warnings.push_back("manifest.json is missing");
    }

    const QDir reports(package.filePath("reports"));
    if (reports.exists()) {
        const QStringList files = reports.entryList(QStringList{"*.json"}, QDir::Files, QDir::Name);
        for (const QString& file : files) {
            summary.report_paths.push_back(reports.filePath(file));
        }
    } else {
        summary.warnings.push_back("reports directory is missing");
    }

    const QDir preview(package.filePath("preview"));
    if (preview.exists()) {
        const QStringList files = preview.entryList(QStringList{"*.png", "*.ppm", "*.PNG", "*.PPM"}, QDir::Files, QDir::Name);
        for (const QString& file : files) {
            summary.preview_paths.push_back(preview.filePath(file));
        }
    } else {
        summary.warnings.push_back("preview directory is missing");
    }
    return summary;
}

