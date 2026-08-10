#pragma once

#include <QString>

/** @brief Resolves the initial model directory for the reference host. */
class HostImportDirectoryPolicy final
{
public:
    /**
     * @brief Chooses an existing model import directory in stable priority order.
     * @param applicationDirectory Directory containing the running executable.
     * @param workingDirectory Current process working directory.
     * @param previousDirectory Most recently used directory in this session.
     * @return Absolute existing directory suitable for a model file dialog.
     */
    [[nodiscard]] static QString Resolve(
        const QString& applicationDirectory,
        const QString& workingDirectory,
        const QString& previousDirectory = {});
};
