#include "HostDiagnostics.h"
#include "HostMainWindow.h"
#include "HostVersionInfo.h"
#include "HostRipJobController.h"
#include "diagnostics/host/ProcessDiagnostics.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QDesktopServices>
#include <QMenuBar>
#include <QMessageBox>
#include <QStyle>
#include <QUrl>
#include <QTimer>
#include <QDebug>
#include <cstdio>

namespace
{
QtMessageHandler previousHandler = nullptr;
void QtLog(QtMsgType type, const QMessageLogContext& context, const QString& text)
{
    static thread_local bool inHandler = false;
    if (inHandler) return;
    inHandler = true;
    const int level = type == QtFatalMsg ? 5 : type == QtDebugMsg ? 1 : type == QtInfoMsg ? 2 : type == QtWarningMsg ? 3 : 4;
    const auto message = text.toUtf8();
    slicesoft::diagnostics::WriteApplicationLog("qt", "message",
        {message.constData(), static_cast<std::size_t>(message.size())}, level);
    if (previousHandler) previousHandler(type, context, text);
    else std::fprintf(stderr, "%s\n", message.constData());
    inHandler = false;
}
struct QtLogGuard
{
    QtLogGuard() { previousHandler = qInstallMessageHandler(QtLog); }
    ~QtLogGuard()
    {
        const auto current = qInstallMessageHandler(previousHandler);
        if (current != QtLog) qInstallMessageHandler(current);
    }
};
}

int RunDiagnosticHost(const QString& modulePath)
{
    using namespace slicesoft::diagnostics;
    ProcessDiagnostics diagnostics("host", HostVersionInfo::ApplicationVersion().toStdString());
    QtLogGuard qtLog;
    HostMainWindow window(modulePath);
    auto* menu = window.menuBar()->addMenu(QStringLiteral("诊断"));
    menu->setObjectName(QStringLiteral("diagnostics_menu"));
    auto* open = menu->addAction(window.style()->standardIcon(QStyle::SP_DirOpenIcon), QStringLiteral("打开日志目录"));
    open->setEnabled(diagnostics.Session() != nullptr);
    QObject::connect(open, &QAction::triggered, &window, [&diagnostics]
    {
        if (diagnostics.Session()) QDesktopServices::openUrl(QUrl::fromLocalFile(
            QString::fromStdWString(diagnostics.Session()->Directory().wstring())));
    });
    auto* status = menu->addAction(QStringLiteral("日志与转储状态"));
    QObject::connect(status, &QAction::triggered, &window, [&diagnostics, &window]
    {
        QString text = QString::fromStdString(diagnostics.StartupStatus());
        if (const auto session = diagnostics.Session())
        {
            const auto state = session->Status();
            text += QStringLiteral("\n已写入 %1；丢弃 %2；失败 %3\n%4")
                .arg(state.written).arg(state.dropped).arg(state.failed)
                .arg(QString::fromStdString(state.lastError));
        }
        QMessageBox::information(&window, QStringLiteral("诊断状态"), text);
    });
    for (auto* button : window.findChildren<QAbstractButton*>())
    {
        if (button->objectName().isEmpty()) continue;
        const auto name = button->objectName().toStdString();
        QObject::connect(button, &QAbstractButton::clicked, &window, [name]
        {
            WriteApplicationLog("ui_command", "requested", name);
        });
    }
    for (auto* rip : window.findChildren<HostRipJobController*>())
    {
        QObject::connect(rip, &HostRipJobController::SigCompleted, &window,
            [](bool success, bool cancelled, const QString& code, const QString& message,
                const QString& output, qint64 elapsedMs)
            {
                WriteApplicationLog("rip_job", success ? "completed" : cancelled ? "cancelled" : "failed",
                    (message + QStringLiteral("; output=%1; elapsedMs=%2").arg(output).arg(elapsedMs)).toStdString(),
                    success || cancelled ? 2 : 4, code.toStdString(), LogChannel::Rip);
            });
    }
    if (QApplication::arguments().contains(QStringLiteral("--diagnostics-self-test")))
    {
        if (!diagnostics.Session() || !diagnostics.DumpEnabled()) return 21;
        qWarning("LOGDUMP_QT_BRIDGE_SMOKE");
        QTimer::singleShot(200, &window, [] { QApplication::quit(); });
    }
    window.show();
    return QApplication::exec();
}
