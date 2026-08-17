#include "HostVersionInfo.h"

#include "SliceSoftBuildVersion.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>
#include <string_view>

namespace HostVersionInfo
{

QString ApplicationVersion()
{
    return QString::fromLatin1(SLICESOFT_APP_IMPLEMENTATION_VERSION);
}

QString ApplicationFullBuildVersion()
{
    return QString::fromLatin1(SLICESOFT_APP_FULL_BUILD_VERSION);
}

QString SlicerImplementationVersion()
{
    return QString::fromLatin1(SLICESOFT_SLICER_IMPLEMENTATION_VERSION);
}

QString ApplicationTitle()
{
    return QStringLiteral("SliceSoft %1 · 打印宿主参考实现")
        .arg(ApplicationVersion());
}

QString ApplicationDiagnosticText()
{
    return QStringLiteral(
               "软件版本\n"
               "  组件：%1\n"
               "  实现版本：%2\n"
               "  完整构建：%3\n"
               "  Source：%4 (%5)\n"
               "  构建：%6 · %7 · %8\n"
               "  变体：TIFF=%9 · OpenVDB=%10")
        .arg(
            QString::fromLatin1(SLICESOFT_APP_ID),
            ApplicationVersion(),
            ApplicationFullBuildVersion(),
            QString::fromLatin1(SLICESOFT_SOURCE_REVISION),
            QString::fromLatin1(SLICESOFT_SOURCE_STATE),
            QString::fromLatin1(SLICESOFT_BUILD_CONFIG),
            QString::fromLatin1(SLICESOFT_BUILD_RUNTIME),
            QString::fromLatin1(SLICESOFT_BUILD_TRIPLET),
            QString::fromLatin1(SLICESOFT_TIFF_BACKEND_VERSION_VARIANT))
        .arg(QString::fromLatin1(SLICESOFT_OPENVDB_VERSION_VARIANT));
}

QString SlicerVersionFromModuleInfo(const QByteArray& moduleInfo)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        moduleInfo, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return {};
    }
    return document.object().value(QStringLiteral("version")).toString();
}

void ApplyApplicationMetadata(QCoreApplication& application)
{
    application.setApplicationName(QStringLiteral("SliceSoft"));
    application.setApplicationVersion(ApplicationVersion());
    application.setOrganizationName(QStringLiteral("SliceSoft"));
}

bool TryRunVersionCommand(
    const int argc,
    char* argv[],
    int* exitCode)
{
    for (int index = 1; index < argc; ++index)
    {
        if (std::string_view{argv[index]} == "--version")
        {
            std::cout
                << SLICESOFT_APP_NAME << ' '
                << SLICESOFT_APP_IMPLEMENTATION_VERSION << '\n'
                << "build " << SLICESOFT_APP_FULL_BUILD_VERSION << '\n';
            if (exitCode != nullptr)
            {
                *exitCode = 0;
            }
            return true;
        }
    }
    return false;
}

}  // namespace HostVersionInfo
