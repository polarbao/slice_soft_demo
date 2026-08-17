#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

/** @brief Immutable identity of one source Package file. */
struct hostripsourcefileidentity
{
    QString path;
    qint64 size{0};
    QByteArray sha256;
};

/** @brief Filesystem safety operations shared by the RIP host controller. */
class HostRipSafety final
{
public:
    [[nodiscard]] static bool CaptureSourceIdentity(
        const QString& packageDirectory,
        const QStringList& sourcePaths,
        QVector<hostripsourcefileidentity>* identity,
        QString* error = nullptr);

    [[nodiscard]] static bool VerifySourceIdentity(
        const QString& packageDirectory,
        const QVector<hostripsourcefileidentity>& expected,
        QString* error = nullptr);

    /** Remove only a real, direct `.rip.staging.*` Package child. */
    [[nodiscard]] static bool RemoveOwnedStaging(
        const QString& packageDirectory,
        const QString& stagingDirectory,
        QString* error = nullptr);
};
