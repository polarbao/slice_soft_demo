#pragma once

#include <QTextEdit>
#include <QWidget>

class LogPanel final : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);

public slots:
    void appendCommand(const QString& command);
    void appendOutput(const QString& text);
    void appendError(const QString& text);
    void appendResult(int exit_code, qint64 elapsed_ms);
    void clear();

private:
    void appendHtmlLine(const QString& text, const QString& color);
    QString highlightErrorCodes(const QString& text) const;

    QTextEdit* text_{nullptr};
};

