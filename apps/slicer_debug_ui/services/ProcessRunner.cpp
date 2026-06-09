#include "ProcessRunner.h"

#include <QProcessEnvironment>

ProcessRunner::ProcessRunner(QObject* parent) : QObject(parent) {
    process_.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&process_, &QProcess::readyReadStandardOutput, this, [this]() {
        emit output(QString::fromLocal8Bit(process_.readAllStandardOutput()));
    });
    connect(&process_, &QProcess::readyReadStandardError, this, [this]() {
        emit errorOutput(QString::fromLocal8Bit(process_.readAllStandardError()));
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error);
        emit failed(process_.errorString());
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exit_code, QProcess::ExitStatus status) {
                Q_UNUSED(status);
                emit finished(exit_code, timer_.isValid() ? timer_.elapsed() : 0);
            });
}

bool ProcessRunner::isRunning() const {
    return process_.state() != QProcess::NotRunning;
}

void ProcessRunner::run(const QString& program, const QStringList& args, const QString& working_dir) {
    if (isRunning()) {
        emit failed("已有命令正在执行");
        return;
    }
    process_.setProgram(program);
    process_.setArguments(args);
    process_.setWorkingDirectory(working_dir);
    timer_.restart();
    emit started(formatCommand(program, args));
    process_.start();
}

void ProcessRunner::stop() {
    if (!isRunning()) {
        return;
    }
    process_.terminate();
}

QString ProcessRunner::formatCommand(const QString& program, const QStringList& args) const {
    QStringList parts;
    parts.push_back(program);
    for (const QString& arg : args) {
        if (arg.contains(' ')) {
            parts.push_back('"' + arg + '"');
        } else {
            parts.push_back(arg);
        }
    }
    return parts.join(' ');
}
