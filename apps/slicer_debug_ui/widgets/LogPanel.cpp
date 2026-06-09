#include "LogPanel.h"

#include <QRegularExpression>
#include <QTextDocument>
#include <QVBoxLayout>

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    text_ = new QTextEdit(this);
    text_->setReadOnly(true);
    text_->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(text_);
}

void LogPanel::appendCommand(const QString& command) {
    appendHtmlLine("> " + command, "#1f5f99");
}

void LogPanel::appendOutput(const QString& text) {
    appendHtmlLine(text, "#222222");
}

void LogPanel::appendError(const QString& text) {
    appendHtmlLine(text, "#a00000");
}

void LogPanel::appendResult(const int exit_code, const qint64 elapsed_ms) {
    appendHtmlLine(QString("exitCode=%1 elapsedMs=%2").arg(exit_code).arg(elapsed_ms),
                   exit_code == 0 ? "#1f7a1f" : "#a00000");
}

void LogPanel::clear() {
    text_->clear();
}

void LogPanel::appendHtmlLine(const QString& text, const QString& color) {
    QString escaped = text.toHtmlEscaped();
    escaped = highlightErrorCodes(escaped);
    text_->append(QString("<pre style=\"color:%1; margin:0;\">%2</pre>").arg(color, escaped));
}

QString LogPanel::highlightErrorCodes(const QString& text) const {
    QString result = text;
    const QRegularExpression error_code(R"(E_[A-Z0-9_]+)");
    QRegularExpressionMatchIterator matches = error_code.globalMatch(result);
    int offset = 0;
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString replacement =
            "<span style=\"background:#ffe0e0; color:#a00000; font-weight:bold;\">" + match.captured(0) + "</span>";
        result.replace(match.capturedStart(0) + offset, match.capturedLength(0), replacement);
        offset += replacement.size() - match.capturedLength(0);
    }
    return result;
}

