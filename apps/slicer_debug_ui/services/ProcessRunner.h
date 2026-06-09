#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>

class ProcessRunner final : public QObject {
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);

    bool isRunning() const;
    void run(const QString& program, const QStringList& args, const QString& working_dir);
    void stop();

signals:
    void started(const QString& command);
    void output(const QString& text);
    void errorOutput(const QString& text);
    void finished(int exit_code, qint64 elapsed_ms);
    void failed(const QString& message);

private:
    QString formatCommand(const QString& program, const QStringList& args) const;

    QProcess process_;
    QElapsedTimer timer_;
};

