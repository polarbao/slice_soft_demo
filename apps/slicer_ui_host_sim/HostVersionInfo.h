#pragma once

#include <QByteArray>
#include <QString>

class QCoreApplication;

namespace HostVersionInfo
{

QString ApplicationVersion();
QString ApplicationFullBuildVersion();
QString SlicerImplementationVersion();
QString ApplicationTitle();
QString ApplicationDiagnosticText();
QString SlicerVersionFromModuleInfo(const QByteArray& moduleInfo);
void ApplyApplicationMetadata(QCoreApplication& application);
bool TryRunVersionCommand(int argc, char* argv[], int* exitCode);

}  // namespace HostVersionInfo
