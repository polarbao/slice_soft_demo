#pragma once
#include "diagnostics/host/ProcessDiagnostics.h"
#include <QByteArray>
#include <QProcess>
#include <algorithm>

inline void RecordRipProcessStart(const QProcess& process)
{
    const auto detail = QStringLiteral("exe=%1; cwd=%2; arguments=%3")
        .arg(process.program(), process.workingDirectory(), process.arguments().join(QStringLiteral(" | ")));
    slicesoft::diagnostics::WriteApplicationLog("rip_process", "start", detail.toStdString(),
        2, {}, slicesoft::diagnostics::LogChannel::Rip);
}

inline void RecordRipProcessExit(int exitCode, QProcess::ExitStatus exitStatus)
{
    const auto detail = QStringLiteral("exitCode=%1; exitStatus=%2").arg(exitCode).arg(static_cast<int>(exitStatus));
    slicesoft::diagnostics::WriteApplicationLog("rip_process", "exit", detail.toStdString(),
        exitCode == 0 && exitStatus == QProcess::NormalExit ? 2 : 4, {}, slicesoft::diagnostics::LogChannel::Rip);
}

inline void AppendCapped(QByteArray* destination, const QByteArray& value,
    const char* source = "stdout")
{
    constexpr int maximumCapturedBytes = 1024 * 1024;
    for (int offset = 0; offset < value.size();)
    {
        int length = (std::min)(2048, value.size() - offset);
        if (offset + length < value.size())
            while (length > 0 && (static_cast<unsigned char>(value[offset + length]) & 0xc0) == 0x80) --length;
        if (length == 0) length = (std::min)(2048, value.size() - offset);
        slicesoft::diagnostics::WriteApplicationLog("rip_process", source,
            {value.constData() + offset, static_cast<std::size_t>(length)},
            std::string_view(source) == "stderr" ? 3 : 2, {}, slicesoft::diagnostics::LogChannel::Rip);
        offset += length;
    }
    if (destination->size() < maximumCapturedBytes)
        destination->append(value.left(maximumCapturedBytes - destination->size()));
}

inline void RecordRipFailure(const QString& code, const QString& message)
{
    slicesoft::diagnostics::WriteApplicationLog("rip_job", "failed",
        message.toStdString(), 4, code.toStdString(), slicesoft::diagnostics::LogChannel::Rip);
}
