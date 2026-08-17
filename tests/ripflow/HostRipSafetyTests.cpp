#include "HostRipSafety.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <filesystem>
#include <system_error>

namespace
{
bool WriteFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(content) == content.size();
}

bool Expect(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

std::filesystem::path FsPath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    (void)application;
    QTemporaryDir temporary;
    if (!Expect(temporary.isValid(), QStringLiteral("temporary root")))
    {
        return 1;
    }
    const QString package = QDir(temporary.path()).filePath(
        QStringLiteral("package"));
    const QString layers = QDir(package).filePath(QStringLiteral("layers"));
    QDir().mkpath(layers);
    const QString manifest = QDir(package).filePath(
        QStringLiteral("manifest.json"));
    const QString layer = QDir(layers).filePath(
        QStringLiteral("layer_000000.tiff"));
    bool pass = Expect(WriteFile(manifest, "manifest"), QStringLiteral("manifest fixture"))
        && Expect(WriteFile(layer, "layer"), QStringLiteral("layer fixture"));

    QVector<hostripsourcefileidentity> identity;
    QString error;
    pass = Expect(
        HostRipSafety::CaptureSourceIdentity(
            package, QStringList{manifest, layer}, &identity, &error),
        QStringLiteral("capture source identity: %1").arg(error)) && pass;
    pass = Expect(
        HostRipSafety::VerifySourceIdentity(package, identity, &error),
        QStringLiteral("unchanged source identity")) && pass;
    pass = Expect(WriteFile(layer, "changed"), QStringLiteral("mutate layer")) && pass;
    pass = Expect(
        !HostRipSafety::VerifySourceIdentity(package, identity, &error),
        QStringLiteral("changed layer is rejected")) && pass;

    const QString stage = QDir(package).filePath(
        QStringLiteral(".rip.staging.regular"));
    QDir().mkpath(stage);
    pass = Expect(
        WriteFile(QDir(stage).filePath(QStringLiteral("partial.bin")), "partial"),
        QStringLiteral("staging fixture")) && pass;
    pass = Expect(
        HostRipSafety::RemoveOwnedStaging(package, stage, &error),
        QStringLiteral("owned staging removal: %1").arg(error)) && pass;
    pass = Expect(!QFile::exists(stage), QStringLiteral("staging removed")) && pass;

    const QString outside = QDir(temporary.path()).filePath(
        QStringLiteral("outside"));
    QDir().mkpath(outside);
    WriteFile(QDir(outside).filePath(QStringLiteral("keep.txt")), "keep");
    const QString linkedStage = QDir(package).filePath(
        QStringLiteral(".rip.staging.link"));
    std::error_code linkError;
    std::filesystem::create_directory_symlink(
        FsPath(outside), FsPath(linkedStage), linkError);
    if (!linkError)
    {
        pass = Expect(
            !HostRipSafety::RemoveOwnedStaging(package, linkedStage, &error),
            QStringLiteral("linked staging is rejected")) && pass;
        pass = Expect(
            QFile::exists(QDir(outside).filePath(QStringLiteral("keep.txt"))),
            QStringLiteral("linked target remains")) && pass;
        std::filesystem::remove(FsPath(linkedStage), linkError);
    }

    QTextStream(stdout)
        << (pass ? "RIPFLOW_SAFETY_TEST_PASS" : "RIPFLOW_SAFETY_TEST_FAILED")
        << Qt::endl;
    return pass ? 0 : 1;
}
