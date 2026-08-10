#include "HostImportDirectoryPolicy.h"

#include <QDir>
#include <QStringList>

namespace
{
QString ExistingDirectory(const QString& path)
{
    if (path.trimmed().isEmpty())
    {
        return {};
    }
    const QDir directory(path);
    return directory.exists()
        ? QDir::cleanPath(directory.absolutePath())
        : QString{};
}
}

QString HostImportDirectoryPolicy::Resolve(
    const QString& applicationDirectory,
    const QString& workingDirectory,
    const QString& previousDirectory)
{
    const QStringList candidates{
        previousDirectory,
        QDir(applicationDirectory).filePath(QStringLiteral("model")),
        QDir(workingDirectory).filePath(QStringLiteral("model")),
        applicationDirectory,
        workingDirectory};
    for (const QString& candidate : candidates)
    {
        const QString resolved = ExistingDirectory(candidate);
        if (!resolved.isEmpty())
        {
            return resolved;
        }
    }
    return QDir::homePath();
}
