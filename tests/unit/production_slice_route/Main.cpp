#include "ProcessRunner.h"
#include "ProductionProfileSourceResolver.h"
#include "ProductionSliceRunSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

struct processresult
{
    bool started{false};
    bool finished{false};
    bool failedtostart{false};
    int exitcode{-999};
};

processresult RunExitProcess(const int exitCode)
{
    ProcessRunner runner;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(10000);
    processresult result;
    QObject::connect(
        &runner,
        &ProcessRunner::SigStarted,
        &loop,
        [&result](const QString&)
        {
            result.started = true;
        });
    QObject::connect(
        &runner,
        &ProcessRunner::SigFinished,
        &loop,
        [&result, &loop](const int processExitCode, const qint64)
        {
            result.finished = true;
            result.exitcode = processExitCode;
            loop.quit();
        });
    QObject::connect(
        &runner,
        &ProcessRunner::SigFailed,
        &loop,
        [&result, &loop](const QString&)
        {
            result.failedtostart = true;
            loop.quit();
        });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    runner.Run(
        QStringLiteral("cmd.exe"),
        QStringList{
            QStringLiteral("/d"),
            QStringLiteral("/c"),
            QStringLiteral("exit"),
            QStringLiteral("/b"),
            QString::number(exitCode)},
        QDir::currentPath());
    loop.exec();
    return result;
}

bool TestStopTerminatesConsoleProcess()
{
    ProcessRunner runner;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    bool finished{false};
    bool failed{false};
    qint64 elapsedMs{0};
    QObject::connect(
        &runner,
        &ProcessRunner::SigStarted,
        &loop,
        [&runner](const QString&)
        {
            QTimer::singleShot(
                100,
                &runner,
                [&runner]()
                {
                    runner.Stop();
                });
        });
    QObject::connect(
        &runner,
        &ProcessRunner::SigFinished,
        &loop,
        [&finished, &elapsedMs, &loop](
            const int,
            const qint64 measuredElapsedMs)
        {
            finished = true;
            elapsedMs = measuredElapsedMs;
            loop.quit();
        });
    QObject::connect(
        &runner,
        &ProcessRunner::SigFailed,
        &loop,
        [&failed](const QString&)
        {
            failed = true;
        });
    QObject::connect(
        &timeout,
        &QTimer::timeout,
        &loop,
        &QEventLoop::quit);

    timeout.start();
    runner.Run(
        QStringLiteral("powershell.exe"),
        QStringList{
            QStringLiteral("-NoProfile"),
            QStringLiteral("-Command"),
            QStringLiteral("Start-Sleep -Seconds 30")},
        QDir::currentPath());
    loop.exec();

    return ExpectTrue(finished, "cancelled console process exits")
        && ExpectTrue(!failed, "intentional stop is not a process failure")
        && ExpectTrue(
            elapsedMs < timeout.interval(),
            "cancelled console process exits before timeout")
        && ExpectTrue(!runner.IsRunning(), "runner is reusable after stop");
}

QJsonDocument LegacyDocument()
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("input"),
         QJsonObject{
             {QStringLiteral("modelPath"), QStringLiteral("legacy.obj")},
             {QStringLiteral("format"), QStringLiteral("obj")}}},
        {QStringLiteral("output"),
         QJsonObject{
             {QStringLiteral("dpiX"), 635},
             {QStringLiteral("dpiY"), 600},
             {QStringLiteral("layerThicknessMm"), 0.01},
             {QStringLiteral("storageMode"), QStringLiteral("stripped")}}},
        {QStringLiteral("preview"),
         QJsonObject{{QStringLiteral("enabled"), true},
                     {QStringLiteral("interval"), 10}}},
    });
}

bool TestProfileSourceResolution()
{
    ProductionProfileSourceRequest request;
    request.reporoot = QString::fromUtf8(SLICESOFT_SOURCE_DIR);
    request.mode = slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    request.requestedprofileid =
        QStringLiteral("global_surface_shell_material_parity_candidate");
    request.legacytemplatepath = QStringLiteral("legacy.json");
    request.legacyoriginaldocument = LegacyDocument();
    request.legacyoverridedocument = LegacyDocument();

    const ProductionProfileSourceResult resolved =
        ProductionProfileSourceResolver().Resolve(request);
    const QJsonObject root = resolved.overridedocument.object();
    const QJsonObject output = root.value(QStringLiteral("output")).toObject();
    return ExpectTrue(resolved.IsValid(), "Global source resolves")
        && ExpectTrue(
            resolved.profileid
                == QStringLiteral(
                    "global_surface_shell_material_parity_candidate"),
            "Global source Profile identity")
        && ExpectTrue(
            root.value(QStringLiteral("slicePipeline"))
                    .toObject()
                    .value(QStringLiteral("mode"))
                    .toString()
                == QStringLiteral("global_surface_shell"),
            "Global source mode identity")
        && ExpectTrue(
            output.value(QStringLiteral("dpiX")).toInt() == 635
                && output.value(QStringLiteral("dpiY")).toInt() == 600,
            "allowed output overrides copied")
        && ExpectTrue(
            root.value(QStringLiteral("support"))
                .toObject()
                .value(QStringLiteral("enabled"))
                .toBool(false),
            "Profile-owned support preserved");
}

bool TestInvalidProfileDoesNotStartSession()
{
    ProductionSliceRunSession session;
    ProductionSliceRunRequest request;
    request.mode = slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    request.profileid = QStringLiteral("unknown");
    request.sessionid = QStringLiteral("invalid");
    request.configpath = QStringLiteral("invalid.json");
    request.packagedir = QStringLiteral("old-package");
    const QStringList errors = session.Begin(request);
    return ExpectTrue(!errors.isEmpty(), "invalid Profile blocked")
        && ExpectTrue(!session.IsActive(), "invalid Profile has no active session")
        && ExpectTrue(
            session.Complete(0).packagedirtoload.isEmpty(),
            "invalid Profile cannot load old package");
}

bool TestLegacyFailureThenRetry()
{
    ProductionSliceRunSession session;
    ProductionSliceRunRequest request;
    request.mode = slicer_core::SlicePipelineMode::Legacy;
    request.sessionid = QStringLiteral("legacy-failure");
    request.configpath = QStringLiteral("failure.json");
    request.packagedir = QStringLiteral("stale-package");
    if (!session.Begin(request).isEmpty())
    {
        return ExpectTrue(false, "legacy failure session begins");
    }

    const processresult failedProcess = RunExitProcess(7);
    const ProductionSliceRunCompletion failedCompletion =
        session.Complete(failedProcess.exitcode);
    if (!ExpectTrue(
            failedProcess.started && failedProcess.finished
                && !failedProcess.failedtostart
                && failedProcess.exitcode == 7,
            "failure process executed")
        || !ExpectTrue(!failedCompletion.success, "failure is not success")
        || !ExpectTrue(
            failedCompletion.packagedirtoload.isEmpty(),
            "failure does not load stale package")
        || !ExpectTrue(
            !failedCompletion.fallbackapplied,
            "failure does not apply fallback"))
    {
        return false;
    }

    request.sessionid = QStringLiteral("legacy-retry");
    request.configpath = QStringLiteral("retry.json");
    request.packagedir = QStringLiteral("retry-package");
    if (!session.Begin(request).isEmpty())
    {
        return ExpectTrue(false, "legacy retry session begins");
    }
    const processresult retryProcess = RunExitProcess(0);
    const ProductionSliceRunCompletion retryCompletion =
        session.Complete(retryProcess.exitcode);
    return ExpectTrue(
               retryProcess.started && retryProcess.finished
                   && retryProcess.exitcode == 0,
               "retry process executed")
        && ExpectTrue(retryCompletion.success, "retry succeeds")
        && ExpectTrue(
            retryCompletion.packagedirtoload
                == QStringLiteral("retry-package"),
            "retry loads only current package")
        && ExpectTrue(
            !retryCompletion.fallbackapplied,
            "retry has no fallback");
}

bool TestGlobalProcessHasNoLegacyFallback()
{
    ProductionSliceRunSession session;
    ProductionSliceRunRequest request;
    request.mode = slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    request.profileid =
        QStringLiteral("global_surface_shell_restricted_candidate");
    request.sessionid = QStringLiteral("global");
    request.configpath = QStringLiteral("global.json");
    request.packagedir = QStringLiteral("global-package");
    if (!session.Begin(request).isEmpty())
    {
        return ExpectTrue(false, "Global session begins");
    }
    const processresult process = RunExitProcess(0);
    const ProductionSliceRunCompletion completion =
        session.Complete(process.exitcode);
    return ExpectTrue(process.started && process.finished, "Global process executed")
        && ExpectTrue(completion.success, "Global process succeeds")
        && ExpectTrue(
            completion.packagedirtoload
                == QStringLiteral("global-package"),
            "Global loads exact package")
        && ExpectTrue(!completion.fallbackapplied, "Global never falls back");
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    const bool passed = TestProfileSourceResolution()
        && TestInvalidProfileDoesNotStartSession()
        && TestLegacyFailureThenRetry()
        && TestGlobalProcessHasNoLegacyFallback()
        && TestStopTerminatesConsoleProcess();
    if (!passed)
    {
        return 1;
    }
    std::cout
        << "PASS production_slice_route_process_tests "
           "legacy/global/invalid/retry/no-fallback\n";
    return 0;
}
